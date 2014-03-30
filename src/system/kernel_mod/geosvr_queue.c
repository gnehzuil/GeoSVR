#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/init.h>
#include <linux/ip.h>
#include <linux/netdevice.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/netfilter_ipv4.h>
#include <net/sock.h>
#include <net/route.h>
#include <net/icmp.h>
#include <linux/udp.h>
#include <net/route.h>

#include "geosvr_queue.h"
#include "geosvr_netlink.h"
#include "geosvr_ipenc.h"

#define GEOSVR_QUEUE_MAX_DEFAULT 1024
#define NET_GEOSVR_QUEUE_QMAX 2088

extern int routing_port;

struct queue_entry {
	struct list_head l;
	struct sk_buff *skb;
	int (*okfn)(struct sk_buff *);
};

typedef int (*geosvr_queue_cmpfn)(struct queue_entry *, unsigned long);

static unsigned int queue_maxlen = GEOSVR_QUEUE_MAX_DEFAULT;
static rwlock_t queue_lock = RW_LOCK_UNLOCKED;
static unsigned int queue_total;
static LIST_HEAD(queue_list);

static inline int
__geosvr_queue_enqueue_entry(struct queue_entry *e)
{
	if (queue_total >= queue_maxlen) {
		if (net_ratelimit())
			printk(KERN_WARNING "GeoSVR: full at %d entries, "
					"dropping packet(s).\n", queue_total);
		return -ENOSPC;
	}

	list_add(&e->l, &queue_list);
	queue_total++;

	return 0;
}

static inline struct queue_entry *
__geosvr_queue_find_entry(geosvr_queue_cmpfn cmpfn, unsigned long data)
{
	struct list_head *p;

	list_for_each_prev(p, &queue_list) {
		struct queue_entry *e = (struct queue_entry *)p;

		if (!cmpfn || cmpfn(e, data))
			return e;
	}
	return NULL;
}

static inline struct queue_entry *
__geosvr_queue_find_dequeue_entry(geosvr_queue_cmpfn cmpfn, unsigned long data)
{
	struct queue_entry *e;

	e = __geosvr_queue_find_entry(cmpfn, data);
	if (e == NULL)
		return NULL;

	list_del(&e->l);
	queue_total--;

	return e;
}

static inline void
__geosvr_queue_flush(void)
{
	struct queue_entry *e;

	while ((e = __geosvr_queue_find_dequeue_entry(NULL, 0))) {
		kfree_skb(e->skb);
		kfree(e);
	}
}

static inline void
__geosvr_queue_reset(void)
{
	__geosvr_queue_flush();
}

static struct queue_entry *
geosvr_queue_find_dequeue_entry(geosvr_queue_cmpfn cmpfn, unsigned long data)
{
	struct queue_entry *e;

	write_lock_bh(&queue_lock);
	e = __geosvr_queue_find_dequeue_entry(cmpfn, data);
	write_unlock_bh(&queue_lock);

	return e;
}

void
geosvr_queue_flush(void)
{
	write_lock_bh(&queue_lock);
	__geosvr_queue_flush();
	write_unlock_bh(&queue_lock);
}

int
geosvr_queue_enqueue_packet(struct sk_buff *skb, int (*okfn)(struct sk_buff *))
{
	int status = -EINVAL;
	struct queue_entry *e;

	e = kmalloc(sizeof(struct queue_entry), GFP_ATOMIC);
	if (e == NULL) {
		printk(KERN_ERR "GEOSVR: Out of memory in geosvr_queue_enqueue_packet()\n");
		return -ENOMEM;
	}

	e->okfn = okfn;
	e->skb = skb;

	write_lock_bh(&queue_lock);

	status = __geosvr_queue_enqueue_entry(e);
	if (status < 0)
		goto unlock;

	write_unlock_bh(&queue_lock);
	return status;

unlock:
	write_unlock_bh(&queue_lock);
	kfree(e);

	return status;
}

static inline int
dest_cmp(struct queue_entry *e, unsigned long daddr)
{
    struct iphdr *iph = (struct iphdr *)(e->skb->network_header);
	return (daddr == iph->daddr);
}

int
geosvr_queue_find(__u32 daddr)
{
	struct queue_entry *e;
	int res = 0;

	read_lock_bh(&queue_lock);
	
	e = __geosvr_queue_find_entry(dest_cmp, daddr);
	if (e != NULL)
		res = 1;

	read_unlock_bh(&queue_lock);

	return res;
}

int
geosvr_queue_send_pkt(struct geosvr_nlmsg *msg)
{
	struct queue_entry *e;
	int pkts = 0;
    int res;

    while (1) {
        e = geosvr_queue_find_dequeue_entry(dest_cmp, msg->dst);
        if (e == NULL)
            return pkts;

        if (msg->route_len == 0) {
            if (msg->req_type == GEOSVR_NL_MSG_REQ_SEND) {
                pkts++;
                e->okfn(e->skb);
                kfree(e);
                continue;
            } else {
                pkts++;
                geosvr_ip_forward(e->skb, msg);
                kfree(e);
                continue;
            }
        }

        res = geosvr_ip_encapsulate(e->skb, msg);
        if (res < 0) {
            kfree_skb(e->skb);
            kfree(e);
            pkts++;
            continue;
        }

        pkts++;

        e->okfn(e->skb);
        kfree(e);
    }

	return 0;
}

int
geosvr_queue_drop_pkt(__u32 daddr)
{
    struct queue_entry *e;
    int pkts = 0;

    while (1) {
        e = geosvr_queue_find_dequeue_entry(dest_cmp, daddr);
        if (e == NULL)
            return pkts;

        icmp_send(e->skb, ICMP_DEST_UNREACH,
                ICMP_HOST_UNREACH, 0);

        kfree_skb(e->skb);
        kfree(e);
        pkts++;
    }

    return 0;
}

int
geosvr_queue_init(void)
{
	queue_total = 0;
	return 1;
}

void
geosvr_queue_fini(void)
{
	synchronize_net();
	geosvr_queue_flush();
}
