#include "neigh_list.h"

extern "C" {
#include <errno.h>
}

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "conn_manager.h"
#include "gps_manager.h"
#include "list.h"
#include "log.h"
#include "packet.h"
#include "timer.h"
#include "utils.h"

struct neigh_list nei_list;
struct GeoSVRTimer *list_cleaner_timer;
FILE *neigh_file;

static int neigh_entry_expire_timeout = DEFAULT_NEIGH_LIST_EXPIRE;
static int neigh_list_cleaner_timeout = DEFAULT_LIST_CLEANER_TIMER;

static int neigh_list_expire(void *arg);
static int list_cleaner_cb(void *arg);
static int rebroadcast_expire(void *arg);

ConnManager *conn_manager = NULL;
GpsManager  *gps_manager = NULL;

void
neigh_list_init(Config *config)
{
    nei_list.num = 0;
    INIT_LIST_HEAD(&nei_list.list);

    if (config->entry_expire_period != 0)
        neigh_entry_expire_timeout = config->entry_expire_period;
    if (config->list_cleaner_period != 0)
        neigh_list_cleaner_timeout = config->list_cleaner_period;

    list_cleaner_timer = geosvr_timer_new(list_cleaner_cb,
            &nei_list, neigh_list_cleaner_timeout);

    /* open neighbour log file */
    neigh_file = fopen(config->neigh_file, "a+");
    if (neigh_file == NULL)
        error("cannot open neighbour log");
    fprintf(neigh_file, "====\n");
}

void
neigh_list_fini()
{
    list_t *tmp, *pos;

    list_foreach_safe(pos, tmp, &nei_list.list) {
        struct neighbor *n = (struct neighbor *)pos;
        neigh_list_delete(n);
    }

    geosvr_timer_free(&list_cleaner_timer);

    if (neigh_file != NULL)
        fclose(neigh_file);
}

struct neighbor *
neigh_list_insert(struct geosvr_node_packet *pkt, int roadid,
                  u_int64_t wait_time, ConnManager *conn_mgr)
{
    struct neighbor *n;

    n = (struct neighbor *)malloc(sizeof(struct neighbor));
    if (n == NULL)
        error("Out of memory");

    memset(n, 0, sizeof(struct neighbor));

    n->ip_addr = pkt->saddr;
    n->latitude = pkt->latitude;
    n->longitude = pkt->longitude;
    n->track = pkt->track;
    n->speed = pkt->speed;
    n->seqno = pkt->seqno;
    n->state = 0;
    n->state |= (1 << pkt->direct);
    n->state &= ~NEIGH_STATE_INVALID;
    n->roadid = roadid;
    n->timeout_timer = geosvr_timer_new(neigh_list_expire,
                                        n, neigh_entry_expire_timeout);
    n->rebroadcast_timer =
        geosvr_timer_new(rebroadcast_expire, n, wait_time);
    n->conn_mgr = conn_mgr;

    nei_list.num++;

    list_add(&nei_list.list, &n->l);

    debug("[Neigh List] Insert a new neighbor %s\n",
          ip_to_str(pkt->saddr));

    return n;
}

struct neighbor *
neigh_list_update(struct neighbor *n, struct geosvr_node_packet *pkt,
                  int roadid, u_int64_t wait_time, ConnManager *conn_mgr)
{
    n->latitude = pkt->latitude;
    n->longitude = pkt->longitude;
    n->track = pkt->track;
    n->speed = pkt->speed;
    n->seqno = pkt->seqno;
    n->state &= ~NEIGH_STATE_INDIRECT;
    n->state &= ~NEIGH_STATE_DIRECT;
    n->state |= (1 << pkt->direct);
    n->roadid = roadid;
    if (n->state & NEIGH_STATE_INVALID) {
        n->state &= ~NEIGH_STATE_INVALID;
        n->timeout_timer =
            geosvr_timer_new(neigh_list_expire,
                             n, neigh_entry_expire_timeout);
    } else
        geosvr_timer_reset(n->timeout_timer,
                           neigh_entry_expire_timeout);
    if (n->state & NEIGH_STATE_REBROADCAST) {
        n->state &= ~NEIGH_STATE_REBROADCAST;
        n->rebroadcast_timer =
            geosvr_timer_new(rebroadcast_expire, n, wait_time);
    }
    n->conn_mgr = conn_mgr;

    debug("[Neigh List] Update a neighbor %s\n",
          ip_to_str(pkt->saddr));

    return n;
}

void
neigh_list_delete(struct neighbor *n)
{
    g_return_if_fail(n != NULL);

    list_detach(&n->l);
    if (n->rebroadcast_timer != NULL &&
        !(n->state & NEIGH_STATE_REBROADCAST)) {
        geosvr_timer_free(&n->rebroadcast_timer);
        n->rebroadcast_timer = NULL;
    }
    nei_list.num--;
    free(n);
    n = NULL;
}

struct neighbor *
neigh_list_find(u_int32_t ip_addr)
{
    list_t *pos;

    if (nei_list.num == 0)
        return NULL;

    list_foreach(pos, &nei_list.list) {
        struct neighbor *n = (struct neighbor *)pos;

        if (ip_addr == n->ip_addr)
            return n;
    }

    return NULL;
}

static int
neigh_list_expire(void *arg)
{
    struct neighbor *n;

    n = (struct neighbor *)arg;
    if (n == NULL)
        return 0;

    n->state |= NEIGH_STATE_INVALID;

    debug("[Neigh List] Neighbor entry %s is timeout\n",
          ip_to_str(n->ip_addr));

    return 0;
}

static int
list_cleaner_cb(void *arg)
{
    list_t *tmp, *pos;

    neigh_list_dump();

    debug("[Neigh List] start cleaner...\n");
    list_foreach_safe(pos, tmp, &nei_list.list) {
        struct neighbor *n = (struct neighbor *)pos;

        if (n->state & NEIGH_STATE_INVALID)
            neigh_list_delete(n);
    }

    return 1;
}

struct neigh_list *
neigh_list_get_list()
{
    return &nei_list;
}

static int
rebroadcast_expire(void *arg)
{
    struct neighbor *nei;
    struct geosvr_node_packet pkt;

    nei = (struct neighbor *)arg;
    nei->state |= NEIGH_STATE_REBROADCAST;

    memset(&pkt, 0, sizeof(pkt));
    /*pkt.taddr = nei->conn_mgr->get_local_addr().s_addr;*/
    pkt.taddr = conn_manager->get_local_addr().s_addr;
    pkt.saddr = nei->ip_addr;
    pkt.latitude = nei->latitude;
    pkt.longitude = nei->longitude;
    pkt.track = nei->track;
    pkt.speed = nei->speed;
    pkt.direct = 0;
    pkt.seqno = nei->seqno;
    
    /*if (nei->conn_mgr->send_data(&pkt, sizeof(pkt)) < 0)*/
    if (conn_manager->send_data(&pkt, sizeof(pkt)) < 0)
        error("send rebroadcast packet error: %s\n", strerror(errno));

    debug("[neigh mgr] rebroadcast a node packet. (%s latitude %.6f longitude %.6f speed %.6f seqno %ld)\n",
          ip_to_str(pkt.saddr), pkt.latitude, pkt.longitude, pkt.speed, pkt.seqno);

    return 0;
}

void
neigh_list_dump()
{
    list_t *tmp, *pos;
    char buf[200];

    fprintf(neigh_file, "----------\n");
    now(buf, 200);
    list_foreach_safe(pos, tmp, &nei_list.list) {
        struct neighbor *n = (struct neighbor *)pos;

        fprintf(neigh_file, "[%s] addr %s latitude %.6f longitude %.6f track %.6f speed %.6f state %d seqno %d roadid %d distance %f\n",
              buf, ip_to_str(ntohl(n->ip_addr)), n->latitude, n->longitude, n->track, n->speed, n->state, n->seqno, n->roadid,
              distance(n->longitude, n->latitude, gps_manager->get_longitude(), gps_manager->get_latitude()));
    }
    fprintf(neigh_file, "----------\n");
    fflush(neigh_file);
}
