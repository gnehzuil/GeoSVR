#ifndef NEIGH_MANAGER_H_
#define NEIGH_MANAGER_H_

extern "C" {
#include <netinet/in.h>
}

#include <cstdio>
#include <vector>

class ConnManager;
class GpsManager;
class MapManager;
struct Config;
struct DedupedData;
struct GeoSVRTimer;
struct Route;
struct geosvr_node_packet;
struct neighbor;

extern ConnManager *conn_manager;
extern GpsManager  *gps_manager;

class NeighManager {
    public:
        NeighManager(ConnManager* conn, GpsManager* gps, MapManager* map, Config* config);
        ~NeighManager();

        void send_node();
        void recv_node(char* data, size_t len);

        int dst_is_direct(struct in_addr dst_addr);
        struct in_addr next_hop(std::vector<Route>& ofr,
                double dstx, double dsty);
        struct neighbor* get_neighbor(struct in_addr addr);

    private:
        // private functions
        struct in_addr restricted_forwarding_algo(std::vector<Route>& ofr,
                                                  double dstx, double dsty);
        u_int64_t wait_time(struct geosvr_node_packet *pkt);
        int in_circle(struct geosvr_node_packet *pkt);
        int on_same_road(struct neighbor *n, struct DedupedData *curr_road);

        // private members
        GeoSVRTimer* timer_;
        ConnManager* conn_;
        GpsManager* gps_;
        MapManager* map_;

        static int send_node_info(void* arg);
        static void recv_node_info(char* data, size_t len, void* arg);

        static u_int32_t seqno;

        FILE* node_file_;
        FILE* recv_file_;
};

#endif // NEIGH_MANAGER_H_
