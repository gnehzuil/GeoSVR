#include "conn_manager.h"

extern "C" {
#include <arpa/inet.h>
#include <asm/types.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <linux/netlink.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <geosvr_netlink.h>
}

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "log.h"
#include "utils.h"

ConnManager::ConnManager(Config *config)
{
    struct sockaddr_in bcast_addr, listen_addr;
    int on = 1;

    if (config->local_addr == NULL ||
        config->bcast_addr == NULL)
        error("You must specify address.\n");

    if (inet_pton(AF_INET, config->local_addr, &local_addr_.sin_addr) == 0)
        error("invalid local address.\n");

    if (inet_pton(AF_INET, config->bcast_addr, &bcast_addr.sin_addr) == 0)
        error("invalid boradcast address.\n");

    socket_out_ = socket(AF_INET, SOCK_DGRAM, 0);

    if (setsockopt(socket_out_, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on)) < 0)
        error("setsockopt: %s\n", strerror(errno));

    local_addr_.sin_family = AF_INET;
    local_addr_.sin_port = 0;

    bcast_addr.sin_family = AF_INET;
    bcast_addr.sin_port = htons((unsigned short) config->listen_port);

    if (bind(socket_out_, (struct sockaddr *)&local_addr_, sizeof(local_addr_)) == -1)
        error("cannot bind local address.\n");

    if (connect(socket_out_, (struct sockaddr *)&bcast_addr, sizeof(bcast_addr)) == -1)
        error("cannot connect broadcast address.\n");

    socket_in_ = socket(AF_INET, SOCK_DGRAM, 0);

    memset(&listen_addr, 0, sizeof(listen_addr));
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    listen_addr.sin_port = htons((unsigned short) config->listen_port);

    setsockopt(socket_in_, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    if (bind(socket_in_, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) == -1)
        error("cannot bind listen addr\n");

    event_set(&ev_recv_, socket_in_, EV_READ | EV_PERSIST, receive, this);
    event_add(&ev_recv_, NULL);

    /*
     * XXX: set default callback function
     * This function will be setup in neigh manager
     */
    set_recv_callback(NULL, NULL);

    /*
     * XXX: set call function to NULL
     * This function will be setup in routing manager
     */
    set_req_route_normal_callback(NULL, NULL);

    memset(&peer_, 0, sizeof(struct sockaddr_nl));
    peer_.nl_family = AF_NETLINK;
    peer_.nl_pid = 0;
    peer_.nl_groups = 0;

    memset(&geosvrnl_, 0, sizeof(struct nlsock));
    geosvrnl_.seq = 0;
    geosvrnl_.local.nl_family = AF_NETLINK;
    geosvrnl_.local.nl_groups = GEOSVRGRP_NOTIFY;
    geosvrnl_.local.nl_pid = getpid();

    geosvrnl_.sock = socket(PF_NETLINK, SOCK_RAW, NETLINK_GEOSVR);
    if (geosvrnl_.sock < 0)
        error("unable to create GeoSVR netlink socket");

    if ((bind(geosvrnl_.sock, (struct sockaddr *)&geosvrnl_.local,
                    sizeof(geosvrnl_.local))) == -1)
        error("bind for GeoSVR netlink socket failed");

    unsigned int addrlen = sizeof(geosvrnl_.local);
    if (getsockname(geosvrnl_.sock,
                (struct sockaddr *)&geosvrnl_.local, &addrlen) < 0)
        error("getsockname failed");

    event_set(&ev_nl_recv_, geosvrnl_.sock,
            EV_READ | EV_PERSIST, recv_nl_msg, this);
    event_add(&ev_nl_recv_, NULL);

    /* open bdcast log file */
    bdcast_file_ = fopen(config->bdcast_file, "a+");
    if (bdcast_file_ == NULL)
        error("cannot open broadcast log");
    fprintf(bdcast_file_, "====\n");
}

ConnManager::~ConnManager()
{
    if (bdcast_file_ != NULL)
        fclose(bdcast_file_);
}

int
ConnManager::send_data(void* data, size_t len)
{
    ssize_t sent;
    char buf[200];

    sent = send(socket_out_, data, len, 0);
    if (sent < (ssize_t) len)
        return -1;

    debug("[conn mgr] send a packet, size = %d\n", sent);
    now(buf, 200);
    fprintf(bdcast_file_, "[%s] broadcast a packet, size = %d\n", buf, sent);
    fflush(bdcast_file_);

    return 0;
}

void
ConnManager::set_recv_callback(RecvCallback func, void* arg)
{
    recv_cb_ = func;
    recv_data_ = arg;
}

void
ConnManager::set_req_route_normal_callback(RecvCallback func, void* arg)
{
    req_route_normal_cb_ = func;
    req_route_normal_data_ = arg;
}

void
ConnManager::default_recv_cb(char* data, size_t len, void* arg)
{
    message("received a packet, size = %d", len);
}

int 
ConnManager::nl_send_msg(void* data, size_t len, u_int16_t type)
{
    struct msghdr {
        struct nlmsghdr n;
        char msg[0];
    } *msg = NULL;
    int res;

    msg = (struct msghdr *)malloc(sizeof(struct msghdr) + len);
    memset(msg, 0, sizeof(msghdr) + len);
    memcpy(msg->msg, data, len);

    debug("[conn mgr] send a netlink response msg\n");

    msg->n.nlmsg_len = NLMSG_LENGTH(len);
    msg->n.nlmsg_type = type;
    msg->n.nlmsg_flags = NLM_F_REQUEST;

    res = nl_send(&geosvrnl_, &msg->n);
    free(msg);
    msg = NULL;
    return res;
}

int
ConnManager::nl_send(struct nlsock* nl, struct nlmsghdr* n)
{
	int res;
	struct iovec iov = { (void *) n, n->nlmsg_len };
	struct msghdr msg =
	    { (void *) &peer_, sizeof(peer_), &iov, 1, NULL, 0, 0 };

	if (!nl)
		return -1;

	n->nlmsg_seq = ++nl->seq;
	n->nlmsg_pid = nl->local.nl_pid;

	/* Request an acknowledgement by setting NLM_F_ACK */
	/*n->nlmsg_flags |= NLM_F_ACK;*/

	/* Send message to netlink interface. */
	res = sendmsg(nl->sock, &msg, 0);

	if (res < 0) {
		fprintf(stderr, "error: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

void
ConnManager::nl_recv_msg(int fd, short ev, void* arg)
{
    const int buflen = 256;
    int len;
    socklen_t addrlen;
    struct nlmsghdr *nlm;
    struct nlmsgerr *nlmerr;
    struct geosvr_nlmsg *req_msg;
    struct in_addr saddr, daddr;
    char buf[buflen];

    memset(buf, 0, buflen);
    addrlen = sizeof(peer_);
    len = recvfrom(fd, buf, buflen, 0, (struct sockaddr *) &peer_,
            &addrlen);
    if (len <= 0) {
        warning("[conn mgr] recv netlink socket error: %s\n", strerror(errno));
        return;
    }

    nlm = (struct nlmsghdr *)buf;
    switch (nlm->nlmsg_type) {
    case NLMSG_ERROR:
        nlmerr = (struct nlmsgerr *)NLMSG_DATA(nlm);
        if (nlmerr->error == 0)
            /* ACK */
            ;
        else
            warning("[conn mgr] error %d type %d", nlmerr->error,
                    nlmerr->msg.nlmsg_type);
        break;
    case GEOSVR_PRI_URGENT:
    case GEOSVR_PRI_NORMAL:
        req_msg = (struct geosvr_nlmsg *)NLMSG_DATA(nlm);

        saddr.s_addr = ntohl(req_msg->src);
        daddr.s_addr = ntohl(req_msg->dst);
        debug("[conn mgr] received a normal route request [%d %d (src: %s dst: %s)\n",
              req_msg->priority, req_msg->req_type, ip_to_str(saddr), ip_to_str(daddr));

        req_route_normal_cb_((char*)req_msg, sizeof(*req_msg),
                req_route_normal_data_);

        break;
    default:
        debug("[conn mgr] received a netlink packet\n");
        break;
    }
}

// static function
void
ConnManager::receive(int fd, short ev, void* arg)
{
    ConnManager* conn_mgr = (ConnManager*) arg;
    struct sockaddr_in src_addr;
    socklen_t addrlen = sizeof(src_addr);
    const size_t buflen = 1500;
    char buf[buflen];
    char *data;
    ssize_t len;

    len = recvfrom(fd, buf, buflen, 0, (sockaddr*)&src_addr, &addrlen);
    if (len < 0)
        return;
    if (!memcmp(&src_addr.sin_addr, &conn_mgr->local_addr_.sin_addr,
                sizeof(struct in_addr)))
        return;

    debug("[conn mgr] received a packet, size = %d\n", len);

    data = (char*) malloc(len);
    memcpy(data, buf, len);

    conn_mgr->recv_cb_(data, len, conn_mgr->recv_data_);
    free(data);
}

void
ConnManager::recv_nl_msg(int fd, short ev, void* arg)
{
    ConnManager* conn_mgr = (ConnManager*) arg;

    conn_mgr->nl_recv_msg(fd, ev, arg);
}
