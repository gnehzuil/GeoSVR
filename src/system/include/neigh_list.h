#ifndef GEOSVR_NEIGH_LIST_H_
#define GEOSVR_NEIGH_LIST_H_

extern "C" {
#include <netinet/in.h>
}

#include "list.h"

#define DEFAULT_NEIGH_LIST_EXPIRE 3000 // 3s
#define DEFAULT_LIST_CLEANER_TIMER 10000 // 10s

enum {
    NEIGH_STATE_INDIRECT     = (1 << 0),
    NEIGH_STATE_DIRECT       = (1 << 1),
    NEIGH_STATE_REBROADCAST  = (1 << 2),
    NEIGH_STATE_INVALID      = (1 << 3),
};

class ConnManager;
class GpsManager;
struct Config;
struct GeoSVRTimer;
struct geosvr_node_packet;

struct neighbor {
    list_t l;
    u_int32_t ip_addr;
    double latitude;
    double longitude;
    double track; // heading
    double speed;
    u_int32_t seqno;
    u_int8_t state;
    u_int32_t roadid;

    struct GeoSVRTimer *timeout_timer;
    struct GeoSVRTimer *rebroadcast_timer;
    ConnManager *conn_mgr;
};

struct neigh_list {
    unsigned int num;
    list_t list;
};

void neigh_list_init(Config *config);
void neigh_list_fini();

struct neighbor *neigh_list_insert(struct geosvr_node_packet *pkt, int roadid,
                                   u_int64_t wait_time, ConnManager *conn_mgr);
struct neighbor *neigh_list_update(struct neighbor *n,
        struct geosvr_node_packet *pkt, int roadid,
        u_int64_t wait_time, ConnManager *conn_mgr);
void neigh_list_delete(struct neighbor *n);
struct neighbor *neigh_list_find(u_int32_t ip_addr);

struct neigh_list *neigh_list_get_list();

void neigh_list_dump();

#endif // GEOSVR_NEIGH_LIST_H_
