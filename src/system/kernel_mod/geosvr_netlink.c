#include <linux/if.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/netlink.h>
#include <linux/version.h>

#include <linux/security.h>

#include <net/sock.h>

#include "geosvr_netlink.h"
#include "geosvr_queue.h"

static int peer_pid;
static struct sock *geosvrnl;
static DECLARE_MUTEX(geosvrnl_sem);

static struct sk_buff *
geosvr_netlink_build_msg(int type, void *data, int len)
{
    unsigned char *old_tail;
    size_t size = 0;
    struct sk_buff *skb;
    struct nlmsghdr *nlh;
    void *m;

    size = NLMSG_SPACE(len);

    skb = alloc_skb(size, GFP_ATOMIC);
    if (skb == NULL)
        goto nlmsg_failure;

    old_tail = skb_tail_pointer(skb);
    nlh = NLMSG_PUT(skb, 0, 0, type, size - sizeof(*nlh));

    m = NLMSG_DATA(nlh);
    memcpy(m, data, len);

    nlh->nlmsg_len = skb_tail_pointer(skb) - old_tail;
    NETLINK_CB(skb).pid = 0;

    return skb;

nlmsg_failure:
    if (skb != NULL)
        kfree_skb(skb);

    return NULL;
}

static int
geosvr_netlink_recv_peer(int type, void *msg, int len)
{
    int ret = 0;
    struct geosvr_nlmsg *m;

    switch (type) {
    case GEOSVR_RSP_SEND:
        if (len < sizeof(struct geosvr_nlmsg))
            return -EINVAL;

        m = (struct geosvr_nlmsg *)msg;

        geosvr_queue_send_pkt(msg);

        break;
    case GEOSVR_RSP_DROP:
        if (len < sizeof(struct geosvr_nlmsg))
            return -EINVAL;

        m = (struct geosvr_nlmsg *)msg;

        geosvr_queue_drop_pkt(m->dst);

        break;
    default:
        break;
    }

    return ret;
}

static int
geosvr_netlink_rcv_nl_event(struct notifier_block *this,
        unsigned long event, void *ptr)
{
    return NOTIFY_DONE;
}

static struct notifier_block geosvr_nl_notifier = {
    .notifier_call = geosvr_netlink_rcv_nl_event,
};

#define RCV_SKB_FAIL(err) do { netlink_ack(skb, nlh, (err)); return; } while (0)

static void
geosvr_netlink_rcv_skb(struct sk_buff *skb)
{
    int flags, pid, type, status, nlmsglen, skblen;
    struct nlmsghdr *nlh;

    skblen = skb->len;
    if (skblen < sizeof(struct nlmsghdr)) {
        printk("skblen to small\n");
        return;
    }

    nlh = (struct nlmsghdr *)skb->data;
    nlmsglen = nlh->nlmsg_len;

    if (nlmsglen < sizeof(struct nlmsghdr) || skblen < nlmsglen) {
        printk("nlmsg=%d skblen=%d to small\n", nlmsglen, skblen);
        return;
    }

    pid = nlh->nlmsg_pid;
    flags = nlh->nlmsg_flags;

    if (pid <= 0 || !(flags & NLM_F_REQUEST) || flags & NLM_F_MULTI)
        RCV_SKB_FAIL(-EINVAL);

    if (flags & MSG_TRUNC)
        RCV_SKB_FAIL(-ECOMM);

    type = nlh->nlmsg_type;

    if (security_netlink_recv(skb, CAP_NET_ADMIN))
        RCV_SKB_FAIL(-EPERM);

    if (peer_pid) {
        if (peer_pid != pid)
            RCV_SKB_FAIL(-EBUSY);
    } else
        peer_pid = pid;

    status = geosvr_netlink_recv_peer(type, NLMSG_DATA(nlh),
            skblen - NLMSG_LENGTH(0));
    if (status < 0)
        RCV_SKB_FAIL(status);

    if (flags & NLM_F_ACK)
        netlink_ack(skb, nlh, 0);
}

int
geosvr_netlink_init(void)
{
    netlink_register_notifier(&geosvr_nl_notifier);

    geosvrnl = netlink_kernel_create(&init_net, NETLINK_GEOSVR,
            GEOSVRGRP_MAX, geosvr_netlink_rcv_skb, NULL, THIS_MODULE);
    if (geosvrnl == NULL) {
        printk(KERN_ERR "geosvr_netlink: failed to create netlink socket\n");
        netlink_unregister_notifier(&geosvr_nl_notifier);
        return -1;
    }

    printk(KERN_DEBUG "geosvr_netlink: init\n");

    return 0;
}

void
geosvr_netlink_fini(void)
{
    sock_release(geosvrnl->sk_socket);
    down(&geosvrnl_sem);
    up(&geosvrnl_sem);

    netlink_unregister_notifier(&geosvr_nl_notifier);

    printk(KERN_DEBUG "geosvr_netlink: fini\n");
}

void
geosvr_netlink_req_normal(u_int8_t req_type, u_int32_t src,
        u_int32_t dst, u_int32_t route_len, char *route)
{
    struct sk_buff *skb = NULL;
    struct geosvr_nlmsg *m;
    size_t len;

    len = sizeof(struct geosvr_nlmsg) + route_len;
    m = kmalloc(len, GFP_ATOMIC);
    if (m == NULL) {
        printk(KERN_ALERT "GeoSVR: out of memory\n");
        return;
    }
    memset(m, 0, len);

    m->priority = GEOSVR_PRI_NORMAL;
    m->req_type = req_type;
    m->info_mask = GEOSVR_NL_MSG_MASK_IPADDR; 
    m->src = src;
    m->dst = dst;

    if (req_type == GEOSVR_NL_MSG_TYPE_FORWARD) {
        m->route_len = route_len;
        memcpy(m->route, route, route_len);
    }

    skb = geosvr_netlink_build_msg(m->priority, m, len);
    if (skb == NULL) {
        printk(KERN_ALERT "GeoSVR: skb=NULL\n");
        return;
    }

    netlink_broadcast(geosvrnl, skb, 0, GEOSVRGRP_NOTIFY, GFP_USER);
}
