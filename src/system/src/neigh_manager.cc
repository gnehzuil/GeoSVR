#include "neigh_manager.h"

extern "C" {
#include <event.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
}

#include <cstring>

#include "conn_manager.h"
#include "database_manager.h"
#include "gps_manager.h"
#include "list.h"
#include "log.h"
#include "map_manager.h"
#include "neigh_list.h"
#include "packet.h"
#include "timer.h"
#include "utils.h"

u_int32_t NeighManager::seqno = 0;

NeighManager::NeighManager(ConnManager* conn, GpsManager* gps,
                           MapManager* map, Config* config)
{
    conn_ = conn;
    conn_->set_recv_callback(recv_node_info, this);

    gps_ = gps;
    map_ = map;

    timer_ = geosvr_timer_new(send_node_info, this, config->beacon_period);

    neigh_list_init(config);

    ::conn_manager = conn;
    ::gps_manager = gps;

    /* open node log file */
    node_file_ = fopen(config->node_file, "a+");
    if (node_file_ == NULL)
        error("cannot open node log");
    fprintf(node_file_, "====\n");

    /* open recv log file */
    recv_file_ = fopen(config->recv_file, "a+");
    if (recv_file_ == NULL)
        error("cannot open recv log");
    fprintf(recv_file_, "====\n");
}

NeighManager::~NeighManager()
{
    geosvr_timer_free(&timer_);
    neigh_list_fini();

    if (node_file_ != NULL)
        fclose(node_file_);

    if (recv_file_ != NULL)
        fclose(recv_file_);
}

struct neighbor*
NeighManager::get_neighbor(struct in_addr addr)
{
    return neigh_list_find(addr.s_addr);
}

int
NeighManager::dst_is_direct(struct in_addr dst_addr)
{
    struct neighbor *n;

    n = neigh_list_find(dst_addr.s_addr);
    if (n == NULL || (n->state & NEIGH_STATE_INDIRECT))
        return 0;
    else
        return 1;
}

struct in_addr
NeighManager::next_hop(std::vector<Route>& ofr,
        double dstx, double dsty)
{
    return restricted_forwarding_algo(ofr, dstx, dsty);
}

int
NeighManager::on_same_road(struct neighbor *n, struct DedupedData *curr_road)
{
    double d;

    d = dist_node2line(n->latitude, n->longitude,
                       curr_road->startx, curr_road->starty,
                       curr_road->endx, curr_road->endy);

    if (d < 100.0)
        return 1;

    return 0;
}

struct in_addr
NeighManager::restricted_forwarding_algo(std::vector<Route>& ofr, double dstx, double dsty)
{
    struct DedupedData *curr_road, *next_road;
    const double RESTRICTED_RANGE = 100.0;
    struct in_addr next;
    struct neigh_list *list;
    list_t *pos;
    double currx, curry;
    double min_dist = std::numeric_limits<double>::max();

    memset(&next, 0, sizeof(next));

    list = neigh_list_get_list();
    if (list->num == 0)
        return next;

    currx = gps_->get_longitude();
    curry = gps_->get_latitude();

    if (ofr.size() == 0) {
        return next;
    } else {
        for (std::vector<Route>::reverse_iterator iter = ofr.rbegin();
             iter != ofr.rend(); ++iter) {
            curr_road = map_->get_road_info(iter->roadid);
            list_foreach(pos, &list->list) {
                struct neighbor *n = (struct neighbor *)pos;

                if (n->state & NEIGH_STATE_DIRECT) {
                    if (n->roadid == curr_road->mainid || on_same_road(n, curr_road)) {
                        double dist_c2n = distance(currx, curry, n->longitude,
                                n->latitude);
                        double dist_abs = dist_c2n - RESTRICTED_RANGE;
                        if (abs(dist_abs) < min_dist) {
                            min_dist = dist_abs;
                            next.s_addr = n->ip_addr;
                        }
                    }
                }
            }

            if (next.s_addr != 0)
                return next;
        }
    }

    return next;
}

void
NeighManager::send_node()
{
    struct geosvr_node_packet pkt;
    char buf[200];

    if (gps_->read_gps_data() < 0) {
        warning("read gps data error");
    }

    memset(&pkt, 0, sizeof(pkt));
    pkt.taddr = conn_->get_local_addr().s_addr;
    pkt.saddr = pkt.taddr;
    pkt.latitude = gps_->get_latitude();
    pkt.longitude = gps_->get_longitude();
    pkt.track = gps_->get_track();
    pkt.speed = gps_->get_speed();
    pkt.direct = 1;
    pkt.seqno = seqno++;

    if (conn_->send_data(&pkt, sizeof(pkt)) < 0)
        error("send broadcast packet error\n");

    debug("[neigh mgr] broadcast a node packet. (latitude %.6f longitude %.6f track %.6f speed %.6f seqno %ld)\n",
          pkt.latitude, pkt.longitude, pkt.track, pkt.speed, pkt.seqno);
    now(buf, 200);
    fprintf(node_file_, "[%s] lat %.6f lon %.6f track %.6f speed %.6f seqno %ld roadid %d\n",
          buf, pkt.latitude, pkt.longitude, pkt.track, pkt.speed, pkt.seqno,
          map_->on_road(pkt.latitude, pkt.longitude, pkt.track));
    fflush(node_file_);
    fprintf(stderr, "[%s] --> lat %.6f lon %.6f track %.6f speed %.6f seqno %ld roadid %d\n",
          buf, pkt.latitude, pkt.longitude, pkt.track, pkt.speed, pkt.seqno,
          map_->on_road(pkt.latitude, pkt.longitude, pkt.track));
}

void
NeighManager::recv_node(char* data, size_t len)
{
    struct geosvr_node_packet* pkt;
    struct neighbor *nei;
    int roadid;
    char buf[200];

    pkt = (struct geosvr_node_packet*)data;

    if (pkt->taddr == conn_->get_local_addr().s_addr ||
        pkt->saddr == conn_->get_local_addr().s_addr)
        return;

    roadid = map_->on_road(pkt->latitude, pkt->longitude, pkt->track);

    nei = neigh_list_find(pkt->saddr);

    if (nei == NULL)
        neigh_list_insert(pkt, roadid, wait_time(pkt), conn_);
    else {
        /* we ignore obsolete packet using seqno */
        if (nei->seqno > pkt->seqno)
            return;

        if (nei->seqno == pkt->seqno &&
            (in_circle(pkt) || nei->state & NEIGH_STATE_DIRECT))
            return;

        neigh_list_update(nei, pkt, roadid, wait_time(pkt), conn_);
    }

    debug("[neigh mgr] tip %s sip %s latitude %.6f longitude %.6f track %.6f speed %.6f direct %d seqno %d\n",
          ip_to_str(pkt->taddr), ip_to_str(pkt->saddr), pkt->latitude,
          pkt->longitude, pkt->track, pkt->speed, pkt->direct, pkt->seqno);
    now(buf, 200);
    fprintf(recv_file_, "[%s] ip %s lat %.6f lon %.6f track %.6f speed %.6f seqno %ld roadid %d distance %f\n",
          buf, ip_to_str(ntohl(pkt->saddr)), pkt->latitude, pkt->longitude, pkt->track, pkt->speed, pkt->seqno, roadid,
          distance(gps_->get_longitude(), gps_->get_latitude(), pkt->longitude, pkt->latitude));
    fflush(recv_file_);
    fprintf(stderr, "[%s] <-- ip %s lat %.6f lon %.6f track %.6f speed %.6f seqno %ld roadid %d distance %f\n",
          buf, ip_to_str(ntohl(pkt->saddr)), pkt->latitude, pkt->longitude, pkt->track, pkt->speed, pkt->seqno, roadid,
          distance(gps_->get_longitude(), gps_->get_latitude(), pkt->longitude, pkt->latitude));
}

u_int64_t
NeighManager::wait_time(struct geosvr_node_packet *pkt)
{
    struct neighbor *nei;
    double dist = 0.0;
    u_int64_t time;

    nei = neigh_list_find(pkt->saddr);
    if (nei == NULL) {
        dist = distance(gps_->get_longitude(), gps_->get_latitude(),
                        pkt->longitude, pkt->latitude);
        time = (u_int64_t)(100 / dist);
        if (time == 0.0)
            time = 1000.0;
    } else {
        dist = distance(nei->longitude, nei->latitude,
                        pkt->longitude, pkt->latitude);
        time = (u_int64_t)(100 / dist);
        if (time == 0.0)
            time = 1000.0;
    }

    return time;
}

int
NeighManager::in_circle(struct geosvr_node_packet *pkt)
{
    double cpx, cpy, radius;
    struct neighbor *tnei;

    tnei = neigh_list_find(pkt->taddr);
    if (tnei == NULL)
        return 0;

    radius = distance(pkt->longitude, pkt->latitude,
                      tnei->longitude, tnei->latitude) / 2.0;

    cpx = tnei->longitude + (pkt->longitude - tnei->longitude) / 2.0;
    cpy = tnei->latitude + (pkt->latitude - tnei->latitude) / 2.0;

    if ((gps_->get_longitude() - cpx) * (gps_->get_longitude() - cpx) +
        (gps_->get_latitude() - cpy) * (gps_->get_latitude() - cpy) < radius)
        return 1;

    return 0;
}

// static function
int
NeighManager::send_node_info(void* arg)
{
    NeighManager* mgr = (NeighManager*) arg;

    mgr->send_node();

    return 1;
}

void
NeighManager::recv_node_info(char* data, size_t len, void* arg)
{
    NeighManager* neigh_mgr = (NeighManager*) arg;

    neigh_mgr->recv_node(data, len);
}
