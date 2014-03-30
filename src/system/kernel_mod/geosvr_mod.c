#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netlink.h>
#include <linux/net.h>
#include <linux/skbuff.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/stat.h>
#include <linux/inet.h>

/* need linux kernel version > 2.6 */
#include <linux/moduleparam.h>

#include <net/sock.h>

#include "geosvr_mod.h"
#include "geosvr_netlink.h"
#include "geosvr_queue.h"
#include "geosvr_ipenc.h"

struct if_info *ifip;

/* module parameter definition */
static int routing_port;
module_param(routing_port, int, S_IRUGO);

static __be32 bcast_addr = 0;
static char *bcast_addr_str = NULL;
module_param(bcast_addr_str, charp, S_IRUGO);

static unsigned int
geosvr_hook(unsigned int hooknum, struct sk_buff *skb,
		 const struct net_device *in,
		 const struct net_device *out,
		 int (*okfn)(struct sk_buff *))
{
	struct iphdr *iph = (struct iphdr *)((skb)->network_header);
	struct in_addr ifaddr, bcaddr;
    struct geosvr_hdr *geo_hdr;

	if (iph == NULL)
		return NF_ACCEPT;

    if (iph != NULL) {
        if (iph->protocol == IPPROTO_UDP) {
            struct udphdr *udp;

            udp = (struct udphdr *)((char *)iph + (iph->ihl << 2));
            if (ntohs(udp->dest) == 2947 ||
                ntohs(udp->source) == 2947)
                return NF_ACCEPT;
        } else if (iph->protocol == IPPROTO_TCP) {
            struct tcphdr *tcp;

            tcp = (struct tcphdr *)((char *)iph + (iph->ihl << 2));
            if (ntohs(tcp->dest) == 2947 ||
                ntohs(tcp->source) == 2947)
                return NF_ACCEPT;
        }
    }

	memset(&ifaddr, 0, sizeof(struct in_addr));
	memset(&bcaddr, 0, sizeof(struct in_addr));

    if (hooknum == NF_INET_PRE_ROUTING) {
        if (ifip->dev->ifindex == in->ifindex) {
            ifaddr = ifip->if_addr;
            bcaddr = ifip->bc_addr;
        }
    } else {
        if (ifip->dev->ifindex == out->ifindex) {
            ifaddr = ifip->if_addr;
            bcaddr = ifip->bc_addr;
        }
    }

    /* we let broadcast and multicast packets through directly */
    if (iph->daddr == INADDR_BROADCAST ||
        IN_MULTICAST(ntohl(iph->daddr)) ||
        iph->daddr == bcaddr.s_addr) {

        if (bcast_addr_str == NULL)
            return NF_ACCEPT;
        else {
            if (iph->saddr == bcast_addr ||
                iph->saddr == ifaddr.s_addr)
                return NF_ACCEPT;
            else
                return NF_DROP;
        }
    }

	switch (hooknum) {
		case NF_INET_PRE_ROUTING:
            if (geosvr_ip_has_encapsulated(skb)) {
                geo_hdr = geosvr_ip_decapsulate(skb);
                iph = ip_hdr(skb);
                if (geo_hdr == NULL)
                    return NF_ACCEPT;
                else if (iph->daddr == ifaddr.s_addr) {
                    kfree(geo_hdr);
                    return NF_ACCEPT;
                } else {
                    /* request routing */
                    geosvr_queue_enqueue_packet(skb, okfn);
                    geosvr_netlink_req_normal(GEOSVR_NL_MSG_TYPE_FORWARD,
                                              geo_hdr->src, geo_hdr->dst, geo_hdr->route_len,
                                              geo_hdr->route);

                    return NF_STOLEN;
                }
            } else
                return NF_ACCEPT;
		case NF_INET_LOCAL_OUT:
            /* request routing */
            geosvr_queue_enqueue_packet(skb, okfn);
            geosvr_netlink_req_normal(GEOSVR_NL_MSG_TYPE_SEND,
                                      iph->saddr, iph->daddr, 0, NULL);
            return NF_STOLEN;
		default:
			break;
	}

	return NF_ACCEPT;
}

static struct nf_hook_ops geosvr_ops[] = {
	{
		/* packet come in network layer from datalink layer */
		.hook = geosvr_hook,
#ifdef KERNEL26
		.owner = THIS_MODULE,
#endif
		.pf = PF_INET,
		.hooknum = NF_INET_PRE_ROUTING,
		.priority = NF_IP_PRI_FIRST,
	},
	{
		/* packet come in network layer from transport layer */
		.hook = geosvr_hook,
#ifdef KERNEL26
		.owner = THIS_MODULE,
#endif
		.pf = PF_INET,
		.hooknum = NF_INET_LOCAL_OUT,
		.priority = NF_IP_PRI_FILTER,
	},
};

static int __init
geosvr_linux_init(void)
{
	struct net_device *dev = NULL;
	int ret = 0;

    ret = geosvr_queue_init();
    if (ret < 0)
        return ret;

    ret = geosvr_netlink_init();
    if (ret < 0)
        goto queue;

	ret = nf_register_hook(&geosvr_ops[0]);
	if (ret < 0)
		goto netlink;

	ret = nf_register_hook(&geosvr_ops[1]);
	if (ret < 0)
		goto hook0;

    /* XXX: we assume interface's name is 'eth0' */
    dev = dev_get_by_name(&init_net, "eth0");
    if (dev == NULL)
        printk(KERN_INFO "No device available\n");
    else {
        if_info_add(dev);
        dev_put(dev);
    }

	printk(KERN_ALERT "GeoSVR: GeoSVR linux kernel module init...\n");
	printk(KERN_ALERT "GeoSVR: routing port: %d\n", routing_port);

    if (bcast_addr_str != NULL) {
        printk(KERN_ALERT "GeoSVR: broadcast addr: %s\n", bcast_addr_str);
        bcast_addr = in_aton(bcast_addr_str);
    }

	return ret;

hook0:
	nf_unregister_hook(&geosvr_ops[0]);
netlink:
    geosvr_netlink_fini();
queue:
    geosvr_queue_fini();

	return ret;
}

static void __exit
geosvr_linux_exit(void)
{
	int i;

    if_info_purge();

	for (i = 0; i < sizeof(geosvr_ops) / sizeof(struct nf_hook_ops); i++)
		nf_unregister_hook(&geosvr_ops[i]);

    geosvr_queue_fini();
    geosvr_netlink_fini();

	printk(KERN_ALERT "GeoSVR: GeoSVR linux kernel module exit...\n");
}

module_init(geosvr_linux_init);
module_exit(geosvr_linux_exit);

MODULE_DESCRIPTION ("GeoSVR linux kernel support");
MODULE_AUTHOR("gnehzuil");
