#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/init.h>
#include <linux/ip.h>
#include <linux/netdevice.h>
#include <net/sock.h>
#include <net/route.h>
#include <net/ip.h>

#include "geosvr_mod.h"
#include "geosvr_ipenc.h"
#include "geosvr_netlink.h"

int
geosvr_ip_encapsulate(struct sk_buff *skb, struct geosvr_nlmsg *msg)
{
    struct iphdr *ohdr, *nhdr;
    struct geosvr_hdr *geo_hdr;
    struct rtable *rt;
    struct net *net = dev_net(ifip->dev);
    struct flowi fl;

    if (skb_shared(skb))
        return -1;

    if (skb_cow(skb, sizeof(struct geosvr_hdr) + msg->route_len) < 0)
        return -1;

    ohdr = ip_hdr(skb);
    skb_push(skb, sizeof(struct geosvr_hdr) + msg->route_len);
    skb_reset_network_header(skb);
    nhdr = ip_hdr(skb);
    memmove(nhdr, ohdr, ohdr->ihl << 2);

    geo_hdr = (struct geosvr_hdr *)((char *)nhdr + (nhdr->ihl << 2));
    memset(geo_hdr, 0, sizeof(struct geosvr_hdr) + msg->route_len);
    geo_hdr->prefix[0] = htons(GEOSVR_HDR_PREFIX);
    geo_hdr->prefix[1] = htons(GEOSVR_HDR_PREFIX);
    geo_hdr->src = msg->src;
    geo_hdr->dst = msg->dst;
    geo_hdr->route_len = msg->route_len;
    memcpy(geo_hdr->route, msg->route, msg->route_len);

    nhdr->tot_len = htons(ntohs(nhdr->tot_len) + sizeof(struct geosvr_hdr) + msg->route_len);

    memset(&fl, 0, sizeof(struct flowi));
    fl.fl4_dst = msg->nhop;
    if (ip_route_output_key(net, &rt, &fl) < 0) {
        printk(KERN_ALERT "GeoSVR: route lookup error\n");
        return -1;
    }

    skb_dst_drop(skb);
    skb_dst_set(skb, &rt->u.dst);

    nhdr = ip_hdr(skb);
    skb->ip_summed = CHECKSUM_NONE;
    ip_send_check(nhdr);

    return 0;
}

struct geosvr_hdr *
geosvr_ip_decapsulate(struct sk_buff *skb)
{
    struct sk_buff *nskb;
    struct iphdr *iph;
    struct geosvr_hdr *geo_hdr, *tmp_hdr;
    int len;

    if (skb_shared(skb) || skb_cloned(skb)) {
        nskb = pskb_copy(skb, GFP_ATOMIC);
        kfree_skb(skb);
        if (nskb == NULL)
            return NULL;
    } else
        nskb = skb;

    iph = ip_hdr(nskb);
    tmp_hdr = (struct geosvr_hdr *)((char *)iph + (iph->ihl << 2));
    len = sizeof(struct geosvr_hdr) + tmp_hdr->route_len;

    if (!pskb_may_pull(nskb, len))
        return NULL;

    iph = ip_hdr(nskb);
    tmp_hdr = (struct geosvr_hdr *)((char *)iph + (iph->ihl << 2));
    geo_hdr = kmalloc(len, GFP_ATOMIC);
    if (geo_hdr == NULL)
        return NULL;

    memset(geo_hdr, 0, len);
    memcpy(geo_hdr, tmp_hdr, len);

    memmove(nskb->data + (iph->ihl << 2),
            nskb->data + (iph->ihl << 2) + len,
            nskb->len + (iph->ihl << 2) - len);
    skb_trim(nskb, nskb->len - len);
    skb_set_network_header(nskb, 0);
    iph = ip_hdr(nskb);

    iph->tot_len = htons(ntohs(iph->tot_len) - len);
    ip_send_check(iph);

    return geo_hdr;
}

int
geosvr_ip_has_encapsulated(struct sk_buff *skb)
{
    struct sk_buff *nskb;
    struct iphdr *iph;
    struct geosvr_hdr *geo_hdr;

    if (skb_shared(skb) || skb_cloned(skb)) {
        nskb = pskb_copy(skb, GFP_ATOMIC);
        kfree_skb(skb);
        if (nskb == NULL)
            return 0;
    } else
        nskb = skb;

    iph = ip_hdr(nskb);
    geo_hdr = (struct geosvr_hdr *)((char *)iph + (iph->ihl << 2));
    if (ntohs(geo_hdr->prefix[0]) == GEOSVR_HDR_PREFIX &&
        ntohs(geo_hdr->prefix[1]) == GEOSVR_HDR_PREFIX)
        return 1;
    else
        return 0;
}

int
geosvr_ip_forward(struct sk_buff *skb, struct geosvr_nlmsg *msg)
{
    struct iphdr *iph, *nhdr;
    struct rtable *rt;
    struct flowi fl;
    struct net *net = dev_net(ifip->dev);

    memset(&fl, 0, sizeof(struct flowi));
    fl.oif = 0;
    fl.fl4_dst = msg->nhop;
    fl.fl4_tos = 0;
    if (ip_route_output_key(net, &rt, &fl) < 0) {
        printk(KERN_ALERT "GeoSVR: route lookup error\n");
        return -1;
    }

    iph = ip_hdr(skb);
    ip_select_ident(iph, &rt->u.dst, NULL);

    skb_dst_drop(skb);
    skb_dst_set(skb, &rt->u.dst);

    nf_reset(skb);

    iph = ip_hdr(skb);
    skb->ip_summed = CHECKSUM_NONE;
    ip_send_check(iph);

    NF_HOOK(PF_INET, NF_INET_FORWARD, skb, skb->dev, \
            rt->u.dst.dev, dst_output);

    return 0;
}
