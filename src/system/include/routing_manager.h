#ifndef GEOSVR_ROUTING_MANAGER_H_
#define GEOSVR_ROUTING_MANAGER_H_

extern "C" {
#include <netinet/in.h>
#include <linux/netfilter.h>
#include <event.h>
}

#include <cstdio>
#include <string>
#include <vector>

struct geosvr_nlmsg;

struct Config;
struct Route;
class GpsManager;
class ConnManager;
class MapManager;
class NeighManager;

class RoutingManager {
    public:
        RoutingManager(Config* config);
        ~RoutingManager();

        void handle_req_route_normal(struct geosvr_nlmsg* msg);

    private:
        GpsManager* gps_mgr_;
        ConnManager* conn_mgr_;
        MapManager* map_mgr_;
        NeighManager* neigh_mgr_;

        void handle_req_route_send_normal(struct geosvr_nlmsg* msg);
        void handle_req_route_forward_normal(struct geosvr_nlmsg* msg);

        std::string ofr_tostring(std::vector<Route>& ofr);
        std::vector<Route> extract_ofr(struct geosvr_nlmsg* msg);
        void drop_msg(struct geosvr_nlmsg* msg, const char* error);
        void send_msg_without_handle(struct geosvr_nlmsg* msg, int req_type);

        static void req_route_cb(char* data, size_t len, void* arg);

        FILE* forward_file_;
};

#endif // GEOSVR_ROUTING_MANAGER_H_
