#include "routing_manager.h"

extern "C" {
#include <errno.h>
#include <event.h>
#include <getopt.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <netinet/in.h>
#include <linux/netfilter.h>

#include <geosvr_netlink.h>
}

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

#include <boost/lexical_cast.hpp>

#include "conn_manager.h"
#include "gps_manager.h"
#include "log.h"
#include "map_manager.h"
#include "neigh_list.h"
#include "neigh_manager.h"
#include "packet.h"
#include "timer.h"
#include "utils.h"

RoutingManager::RoutingManager(Config* config)
{
    gps_mgr_ = new GpsManager(config);
    conn_mgr_ = new ConnManager(config);
    map_mgr_ = new MapManager(gps_mgr_);
    neigh_mgr_ = new NeighManager(conn_mgr_, gps_mgr_,
                                  map_mgr_, config);

    conn_mgr_->set_req_route_normal_callback(req_route_cb, this);

    /* open forward log file */
    forward_file_ = fopen(config->forward_file, "a+");
    if (forward_file_ == NULL)
        error("cannot open forward log");
    fprintf(forward_file_, "====\n");
    setvbuf(forward_file_, NULL, _IOLBF, 0);
}

RoutingManager::~RoutingManager()
{
    delete gps_mgr_;
    delete conn_mgr_;
    delete map_mgr_;
    delete neigh_mgr_;

    if (forward_file_ != NULL)
        fclose(forward_file_);
}

void
RoutingManager::handle_req_route_normal(struct geosvr_nlmsg* msg)
{
    switch (msg->req_type) {
    case GEOSVR_NL_MSG_TYPE_SEND:
        handle_req_route_send_normal(msg);
        break;
    case GEOSVR_NL_MSG_TYPE_FORWARD:
        handle_req_route_forward_normal(msg);
        break;
    default:
        drop_msg(msg, "recv an unknown route request msg");
        break;
    }
}

void
RoutingManager::handle_req_route_send_normal(struct geosvr_nlmsg* msg)
{
    struct in_addr dst_addr;
    struct neighbor *nei;
    std::vector<Route> ofr;
    char tmbuf[200];

    now(tmbuf, 200);
    fprintf(forward_file_, "----\n");
    fprintf(forward_file_, "[%s] %s request a route to %s\n", tmbuf,
            ip_to_str(ntohl(msg->src)), ip_to_str(ntohl(msg->dst)));

    memset(&dst_addr, 0, sizeof(dst_addr));
    dst_addr.s_addr = msg->dst;
    if (msg->dst == conn_mgr_->get_local_addr().s_addr) {
        send_msg_without_handle(msg, GEOSVR_NL_MSG_REQ_SEND);
        return;
    }
    if (neigh_mgr_->dst_is_direct(dst_addr)) {
        send_msg_without_handle(msg, GEOSVR_NL_MSG_REQ_SEND);
        fprintf(forward_file_, "[%s] %s --> %s (send to neighbour)\n", tmbuf,
                ip_to_str(ntohl(conn_mgr_->get_local_addr().s_addr)),
                ip_to_str(ntohl(msg->dst)));
        return;
    }

    nei = neigh_mgr_->get_neighbor(dst_addr);
    if (nei == NULL) {
        drop_msg(msg, "can't find neighbor");
        fprintf(forward_file_, "[%s] can not find %s in neighbour list\n",
                tmbuf, ip_to_str(ntohl(msg->dst)));
        return;
    }
    ofr = map_mgr_->get_optimal_route(gps_mgr_->get_longitude(),
            gps_mgr_->get_latitude(), gps_mgr_->get_track(),
            nei->longitude, nei->latitude, nei->track);
    if (ofr.size() == 0) {
        drop_msg(msg, "can't get an ofr");
        fprintf(forward_file_, "[%s] can not get ofr to %s\n",
                tmbuf, ip_to_str(ntohl(msg->dst)));
        return;
    } else {
        std::vector<Route>::iterator iter = ofr.begin();
        fprintf(forward_file_, "[%s] ofr: %d", tmbuf, iter->roadid);
        for ( ++iter; iter != ofr.end(); ++iter)
            fprintf(forward_file_, " --> %d", iter->roadid);
        fprintf(forward_file_, "\n");
    }

    struct in_addr nhop = neigh_mgr_->next_hop(ofr,
                        nei->longitude, nei->latitude);
    if (nhop.s_addr == 0) {
        drop_msg(msg, "can't find next hop");
        fprintf(forward_file_, "[%s] can not find next hop to %s\n",
                tmbuf, ip_to_str(ntohl(msg->dst)));
        return;
    }

    std::string ofr_str = ofr_tostring(ofr);
    struct geosvr_nlmsg* rmsg;

    char buf[BUFSIZ];
    memset(buf, 0, BUFSIZ);
    rmsg = (struct geosvr_nlmsg*)buf;

    rmsg->priority = GEOSVR_RSP_SEND;
    rmsg->req_type = GEOSVR_NL_MSG_REQ_SEND;
    rmsg->src = msg->src;
    rmsg->dst = msg->dst;
    rmsg->nhop = nhop.s_addr;
    rmsg->route_len = ofr_str.size();
    memcpy(rmsg->route, ofr_str.c_str(), rmsg->route_len);

    fprintf(forward_file_, "[%s] %s --> %s (forward)\n", tmbuf,
            ip_to_str(ntohl(conn_mgr_->get_local_addr().s_addr)),
            ip_to_str(ntohl(nhop.s_addr)));

    conn_mgr_->nl_send_msg((void *)rmsg,
            sizeof(geosvr_nlmsg) + rmsg->route_len, rmsg->priority);
}

void
RoutingManager::handle_req_route_forward_normal(struct geosvr_nlmsg* msg)
{
    struct in_addr dst_addr;
    struct neighbor *nei;
    std::vector<Route> ofr;

    memset(&dst_addr, 0, sizeof(dst_addr));
    dst_addr.s_addr = msg->dst;
    if (msg->dst == conn_mgr_->get_local_addr().s_addr) {
        send_msg_without_handle(msg, GEOSVR_NL_MSG_REQ_FORWARD);
        return;
    }
    if (neigh_mgr_->dst_is_direct(dst_addr)) {
        send_msg_without_handle(msg,
                GEOSVR_NL_MSG_REQ_FORWARD);
        return;
    }

    nei = neigh_mgr_->get_neighbor(dst_addr);
    if (nei == NULL) {
        drop_msg(msg, "can't find neighbor");
        return;
    }
    
    ofr = extract_ofr(msg);
    if (ofr.size() == 0) {
        drop_msg(msg, "can't get an ofr");
        return;
    }

    struct in_addr nhop = neigh_mgr_->next_hop(ofr,
                        nei->longitude, nei->latitude);
    if (nhop.s_addr == 0) {
        drop_msg(msg, "can't find next hop");
        return;
    }

    std::string ofr_str = ofr_tostring(ofr);
    struct geosvr_nlmsg* rmsg;

    char buf[BUFSIZ];
    memset(buf, 0, BUFSIZ);
    rmsg = (struct geosvr_nlmsg*)buf;

    rmsg->priority = GEOSVR_RSP_SEND;
    rmsg->req_type = GEOSVR_NL_MSG_REQ_FORWARD;
    rmsg->src = msg->src;
    rmsg->dst = msg->dst;
    rmsg->nhop = nhop.s_addr;
    rmsg->route_len = ofr.size();
    memcpy(rmsg->route, ofr_str.c_str(), rmsg->route_len);

    conn_mgr_->nl_send_msg((void *)rmsg,
            sizeof(geosvr_nlmsg) + rmsg->route_len, rmsg->priority);
}

std::string
RoutingManager::ofr_tostring(std::vector<Route>& ofr)
{
    std::string ofr_str;

    for (std::vector<Route>::iterator iter = ofr.begin();
         iter != ofr.end();
         ++iter) {

        std::string s = boost::lexical_cast<std::string>((*iter).roadid);
        ofr_str += s + ",";
    }

    return ofr_str.substr(0, ofr_str.size() - 1);
}

std::vector<Route>
RoutingManager::extract_ofr(struct geosvr_nlmsg* msg)
{
    std::vector<Route> ofr;
    Route r;
    char *start, *end;
    int id;

    for (start = msg->route; *start != '\0'; ) {
        for (end = start; *end != ',' && *end != '\0'; end++)
            ;
        sscanf(start, "%d,", &id);

        r = map_mgr_->get_route(id);
        ofr.push_back(r);

        if (*end == '\0')
            break;
        else
            start = ++end;
    }

    return ofr;
}

void
RoutingManager::send_msg_without_handle(struct geosvr_nlmsg* msg, int req_type)
{
    struct geosvr_nlmsg *rmsg;

    rmsg = (struct geosvr_nlmsg *)malloc(sizeof(struct geosvr_nlmsg));
    if (rmsg == NULL)
        error("Out of memory");

    rmsg->priority = GEOSVR_RSP_SEND;
    rmsg->req_type = req_type;
    rmsg->dst = msg->dst;
    rmsg->nhop = msg->dst;
    rmsg->route_len = 0;

    conn_mgr_->nl_send_msg((void *)rmsg,
            sizeof(geosvr_nlmsg) + rmsg->route_len, rmsg->priority);

    free(rmsg);
}

void
RoutingManager::drop_msg(struct geosvr_nlmsg* msg, const char* error)
{
    msg->priority = GEOSVR_RSP_DROP;
    conn_mgr_->nl_send_msg((void *)msg,
            sizeof(geosvr_nlmsg) + msg->route_len, msg->priority);

    struct in_addr addr;
    memset(&addr, 0, sizeof(addr));
    addr.s_addr = msg->dst;
    warning("drop msg %s: %s", ip_to_str(ntohl(addr.s_addr)), error);
}

// static functions
void
RoutingManager::req_route_cb(char* data, size_t len, void* arg)
{
    geosvr_nlmsg* msg = (geosvr_nlmsg*)data;
    RoutingManager* routing_mgr = (RoutingManager*)arg;

    switch (msg->priority) {
    case GEOSVR_PRI_URGENT:
    case GEOSVR_PRI_NORMAL:
        debug("[routing mgr] handle normal route request\n");
        routing_mgr->handle_req_route_normal(msg);
        break;
    default:
        break;
    }
}
