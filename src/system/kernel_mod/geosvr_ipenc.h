#ifndef GEOSVR_IPENC_H
#define GEOSVR_IPENC_H

#define GEOSVR_HDR_PREFIX 0xFE

struct geosvr_nlmsg;

struct geosvr_hdr {
    int prefix[2];
    u_int32_t src;
    u_int32_t dst;
    u_int32_t route_len;
    char route[0];
};

int geosvr_ip_encapsulate(struct sk_buff *skb, struct geosvr_nlmsg *msg);
struct geosvr_hdr *geosvr_ip_decapsulate(struct sk_buff *skb);
int geosvr_ip_has_encapsulated(struct sk_buff *skb);
int geosvr_ip_forward(struct sk_buff *skb, struct geosvr_nlmsg *msg);

#endif /* GEOSVR_IPENC_H */
