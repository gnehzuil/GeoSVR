#ifndef GEOSVR_NETLINK_H
#define GEOSVR_NETLINK_H

#include <linux/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#define NETLINK_GEOSVR (MAX_LINKS - 1)

enum {
    GEOSVRGRP_NOTIFY = 1,
#define GEOSVRGRP_NOTIFY GEOSVRGRP_NOTIFY
    __GEOSVRGRP_MAX,
};

#define GEOSVRGRP_MAX (__GEOSVRGRP_MAX - 1)

enum {
    GEOSVR_BASE = 100,
#define GEOSVR_BASE GEOSVR_BASE
    GEOSVR_PRI_NORMAL,
#define GEOSVR_PRI_NORMAL GEOSVR_PRI_NORMAL
    GEOSVR_PRI_URGENT,
#define GEOSVR_PRI_URGENT GEOSVR_PRI_URGENT
    GEOSVR_RSP_SEND,
#define GEOSVR_RSP_SEND GEOSVR_RSP_SEND
    GEOSVR_RSP_DROP,
#define GEOSVR_RSP_DROP GEOSVR_RSP_DROP
};

#define GEOSVR_NL_MSG_TYPE_SEND    1
#define GEOSVR_NL_MSG_TYPE_FORWARD 2

#define GEOSVR_NL_MSG_REQ_SEND    1
#define GEOSVR_NL_MSG_REQ_FORWARD 2

#define GEOSVR_NL_MSG_MASK_IPADDR     0x01
#define GEOSVR_NL_MSG_MASK_ROADID     0x02
#define GEOSVR_NL_MSG_MASK_COORDINATE 0x04

/* XXX: request/response a route */
struct geosvr_nlmsg {
    u_int8_t priority;
    u_int8_t req_type;
    u_int16_t info_mask;
    u_int32_t src;
    u_int32_t dst;
    u_int32_t nhop;
    u_int32_t roadid;
    double lat;
    double lon;
    u_int32_t route_len;
    char route[0];
};

#ifdef __KERNEL__

int geosvr_netlink_init(void);
void geosvr_netlink_fini(void);

void geosvr_netlink_req_normal(u_int8_t req_type, u_int32_t src,
        u_int32_t dst, u_int32_t route_len, char *route);

#endif /* __KERNEL__ */

#endif /* GEOSVR_NETLINK_H */
