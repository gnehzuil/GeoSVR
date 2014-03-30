#ifndef GEOSVR_MAP_MANAGER_H_
#define GEOSVR_MAP_MANAGER_H_

#include <vector>
#include <boost/config.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>

using namespace boost;

struct Node {
    int id;
    double x;
    double y;

    Node() : id(-1), x(0.0), y(0.0) {}
    Node(int id, double x, double y) : id(id), x(x), y(y) {}
    Node(const Node& p) : id(p.id), x(p.x), y(p.y) {}
};

struct Road {
    int id;
    int type;   // store road's type

    Road() : id(-1), type(0) {}
    Road(const Road& r) : id(r.id), type(r.type) {}
    Road(int id, int t) : id(id), type(t) {}
};

struct Route {
    int roadid;
    int startid;
    int endid;
    int type;
};

typedef adjacency_list<listS, vecS, undirectedS, Node, Road> Map;
typedef graph_traits<Map>::vertex_descriptor vertex_descriptor;
typedef graph_traits<Map>::edge_descriptor edge_descriptor;
typedef std::pair<int, int> Edge;

struct DedupedData;
class DatabaseManager;
class GpsManager;

class MapManager {

    public:
        MapManager(GpsManager* gps);
        virtual ~MapManager();

        int on_road(double lat, double lon, double track);
        Route get_route(int id);
        struct DedupedData* get_road_info(int roadid);

        std::pair<int, int> get_rect(double x1, double y1, double t1,
                                     double x2, double y2, double t2);
        std::vector<std::vector<Route> > get_routes(std::pair<int, int> rect);
        std::vector<Route> get_optimal_route(double x1, double y1, double t1,
                                             double x2, double y2, double t2);

        double map_matching(double lat, double lon,
                double sx, double sy, double ex, double ey);

        // just for test
        void print_data();

    private:
        void import_map_data();
        void import_data();
        std::vector<Route> get_route(std::pair<int, int> rect);
        int more_route(std::pair<int, int> rect);
        void restore_routes(std::vector<std::vector<Route> > routes);
        std::vector<Route> cal_optimal_route(std::vector<std::vector<Route> > routes);
        void route_attribute(std::vector<Route> route, int& type, double &dev);

        Map* map_;
        DatabaseManager* database_mgr_;
        GpsManager* gps_mgr_;
        std::vector<Node> nodes_;
        std::vector<Road> roads_;
        std::vector<Edge> edges_; // XXX: a edge describes a road between two nodes.
};

#endif // GEOSVR_MAP_MANAGER_H_
