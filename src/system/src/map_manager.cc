#include "map_manager.h"

#include <cmath>
#include <cstring>

#include <algorithm>
#include <iostream>
#include <boost/config.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/dijkstra_shortest_paths.hpp>
#include <boost/graph/graph_traits.hpp>

#include "database_manager.h"
#include "gps_manager.h"
#include "log.h"
#include "utils.h"

#define PI 3.1415926

MapManager::MapManager(GpsManager* gps)
{
    database_mgr_ = new DatabaseManager();
    gps_mgr_ = gps;

    import_map_data();
}

MapManager::~MapManager()
{
    if (database_mgr_ != NULL)
        delete database_mgr_;

    if (map_ != NULL)
        delete map_;
}

void
MapManager::import_map_data()
{
    database_mgr_->connect("localhost", "root", "");
    database_mgr_->get_map_data();

    import_data();

    map_ = new Map(edges_.begin(), edges_.end(), roads_.begin(), nodes_.size());
    if (map_ == NULL) {
        error("Out of memory\n");
        return;
    }

    graph_traits<Map>::vertex_iterator vi, vend;
    for (tie(vi, vend) = vertices(*map_); vi != vend; ++vi) {
        std::vector<Node>::iterator iter = nodes_.begin();
        for ( ; iter != nodes_.end(); ++iter)
            if (iter->id == *vi)
                break;
        (*map_)[*vi].id = iter->id;
        (*map_)[*vi].x = iter->x;
        (*map_)[*vi].y = iter->y;
    }
}

void
MapManager::import_data()
{
    int max = 0;
    int* bitmap;
    std::vector<Node> tmp;

    std::vector<DedupedData*>& deduped_data = database_mgr_->get_deduped_data();
    std::vector<DedupedData*>::iterator iter = deduped_data.begin();

    for ( ; iter != deduped_data.end(); ++iter) {
        // start node
        Node n1((*iter)->startid, (*iter)->startx, (*iter)->starty);
        if (n1.id > max)
            max = n1.id;
        tmp.push_back(n1);

        // end node
        Node n2((*iter)->endid, (*iter)->endx, (*iter)->endy);
        if (n2.id > max)
            max = n2.id;
        tmp.push_back(n2);

        // road
        Road r((*iter)->mainid, (*iter)->type);

        // we transform type value from 12 to 0
        if (r.type == 12)
            r.type = 0;
        roads_.push_back(r);

        // edge
        Edge e((*iter)->startid, (*iter)->endid);
        edges_.push_back(e);
    }

    // remove duplicated nodes
    bitmap = new int[max + 1];
    if (bitmap == NULL) {
        error("Out of memory\n");
        return;
    }
    std::fill(bitmap, bitmap + max + 1, 0);

    for (std::vector<Node>::iterator iter = tmp.begin();
         iter != tmp.end(); ++iter) {
        if (bitmap[iter->id] == 0) {
            bitmap[iter->id] = 1;
            nodes_.push_back(*iter);
        }
    }

    delete[] bitmap;
}

std::vector<Route>
MapManager::get_optimal_route(double x1, double y1, double t1,
                              double x2, double y2, double t2)
{
    std::pair<int, int> rect = get_rect(x1, y1, t1, x2, y2, t2);
    if (rect.first == -1 || rect.second == -1)
        return std::vector<Route>();

    std::vector<std::vector<Route> > routes = get_routes(rect);
    restore_routes(routes);
    return cal_optimal_route(routes);
}

std::vector<Route>
MapManager::cal_optimal_route(std::vector<std::vector<Route> > routes)
{
    std::vector<std::vector<Route> >::iterator optimal_iter, iter;
    int min_type = std::numeric_limits<int>::max();
    double min_dev = std::numeric_limits<double>::max();

    if (routes.size() == 0) {
        warning("[routing mgr] no optimal forwarding route\n");
        return std::vector<Route>();
    } else
        optimal_iter = routes.begin();
    route_attribute(*optimal_iter, min_type, min_dev);

    for (iter = optimal_iter + 1; iter != routes.end(); ++iter) {
        int type = 0;
        double dev = 0.0;
        route_attribute(*iter, type, dev);

        if (min_type > type) {
            optimal_iter = iter;
            min_type = type;
            min_dev = dev;
        } else if (min_type == type) {
            if (min_dev > dev) {
                optimal_iter = iter;
                min_type = type;
                min_dev = dev;
            } else
                continue;
        } else
            continue;
    }

    return *optimal_iter;
}

void
MapManager::route_attribute(std::vector<Route> route, int& type, double& dev)
{
    std::vector<Route>::iterator iter = route.begin();
    for ( ; iter != route.end(); ++iter)
        type += iter->type;

    double mean = type / route.size();
    double sum = 0.0;
    for (iter = route.begin(); iter != route.end(); ++iter) {
        double tmp1 = iter->type - mean;
        double tmp2 = tmp1 * tmp1;
        sum += tmp2;
    }

    dev = sqrt(sum / route.size());
}

void
MapManager::restore_routes(std::vector<std::vector<Route> > routes)
{
    std::vector<std::vector<Route> >::iterator route_iter = routes.begin();
    for ( ; route_iter != routes.end(); ++route_iter) {
        std::vector<Route>::iterator r_iter = route_iter->begin();
        for ( ; r_iter != route_iter->end(); ++r_iter) {
            graph_traits<Map>::edge_iterator ei, eend;
            for (tie(ei, eend) = edges(*map_); ei != eend; ++ei) {
                if ((*map_)[*ei].id == r_iter->roadid) {
                    std::vector<DedupedData*>::iterator data_iter;
                    data_iter = database_mgr_->get_deduped_data().begin();
                    for ( ; data_iter != database_mgr_->get_deduped_data().end();
                            ++data_iter) {
                        if ((*data_iter)->mainid == r_iter->roadid) {
                            (*map_)[*ei].type = (*data_iter)->type;
                            break;
                        }
                    }

                    break;
                }
            }
        }
    }
}

std::pair<int, int>
MapManager::get_rect(double x1, double y1, double t1, double x2, double y2, double t2)
{
    std::pair<int, int> rect;

    rect.first = -1;
    rect.second = -1;

    int road_src = on_road(y1, x1, t1);
    int road_dst = on_road(y2, x2, t2);

    if (road_src == -1 || road_dst == -1)
        return std::pair<int, int>(-1, -1);

    // src and dst are in the same road
    if (road_src == road_dst) {
        std::vector<DedupedData*>::iterator iter =
            database_mgr_->get_deduped_data().begin();
        for ( ; iter != database_mgr_->get_deduped_data().end(); ++iter) {
            if ((*iter)->mainid == road_src &&
                (*iter)->mainid == road_dst) {
                rect.first = (*iter)->startid;
                rect.second = (*iter)->endid;
                return rect;
            }
        }
    }

    // src and dst are not in the same road
    std::vector<DedupedData*>::iterator iter =
        database_mgr_->get_deduped_data().begin();
    for ( ; iter != database_mgr_->get_deduped_data().end(); ++iter) {
        if ((*iter)->mainid == road_src) {
            double len1 = distance(x1, y1, (*iter)->startx, (*iter)->starty);
            double len2 = distance(x1, y1, (*iter)->endx, (*iter)->endy);

            if (len1 <= len2)
                rect.first = (*iter)->startid;
            else
                rect.first = (*iter)->endid;
        } else if ((*iter)->mainid == road_dst) {
            double len1 = distance(x2, y2, (*iter)->startx, (*iter)->starty);
            double len2 = distance(x2, y2, (*iter)->endx, (*iter)->endy);

            if (len1 <= len2)
                rect.second = (*iter)->startid;
            else
                rect.second = (*iter)->endid;
        }
    }

    return rect;
}

std::vector<std::vector<Route> >
MapManager::get_routes(std::pair<int, int> rect)
{
    std::vector<std::vector<Route> > routes;

    // check src and dst are on the same road
    if (rect.first == rect.second) {
        graph_traits<Map>::edge_iterator ei, eend;
        for (tie(ei, eend) = edges(*map_); ei != eend; ++ei) {
            int found = 0;
            if (source(*ei, *map_) == rect.first &&
                target(*ei, *map_) == rect.second)
                found = 1;
            else if (source(*ei, *map_) == rect.second &&
                     target(*ei, *map_) == rect.first)
                found = 1;

            if (found == 1) {
                std::vector<Route> route;
                Route r;
                r.roadid = (*map_)[*ei].id;
                r.startid = rect.first;
                r.endid = rect.second;
                r.type = (*map_)[*ei].type;
                route.push_back(r);

                debug("src and dst are on the same road %d\n", (*map_)[*ei].id);

                routes.push_back(route);
                return routes;
            }
        }
    }

    while (more_route(rect)) {
        std::vector<Route> route = get_route(rect);
        if (route.size() == 0)
            break;

        routes.push_back(route);
    }

    return routes;
}

int
MapManager::more_route(std::pair<int, int> rect)
{
    double dist_src_dst = distance((*map_)[rect.first].x,
            (*map_)[rect.first].y, (*map_)[rect.second].x,
            (*map_)[rect.second].y);

    graph_traits<Map>::edge_iterator ei, eend;
    for (tie(ei, eend) = edges(*map_); ei != eend; ++ei) {
        if ((*map_)[*ei].type == std::numeric_limits<int>::max())
            continue;

        if ((*map_)[source(*ei, *map_)].id == (*map_)[rect.second].id ||
            (*map_)[target(*ei, *map_)].id == (*map_)[rect.second].id) {
            double dist = std::numeric_limits<double>::max();

            if ((*map_)[source(*ei, *map_)].id == (*map_)[rect.second].id)
                dist = distance((*map_)[rect.first].x,
                        (*map_)[rect.first].y, (*map_)[target(*ei, *map_)].x,
                        (*map_)[target(*ei, *map_)].y);
            else
                dist = distance((*map_)[rect.first].x,
                        (*map_)[rect.first].y, (*map_)[source(*ei, *map_)].x,
                        (*map_)[source(*ei, *map_)].y);

            if (dist_src_dst > dist)
                return 1;
        }
    }

    return 0;
}

std::vector<Route>
MapManager::get_route(std::pair<int, int> rect)
{
    vertex_descriptor src = vertex(rect.first, *map_);
    vertex_descriptor dst = vertex(rect.second, *map_);

    std::vector<vertex_descriptor> p(num_vertices(*map_));
    std::vector<int> d(num_vertices(*map_));
    std::vector<Route> route;

    dijkstra_shortest_paths(*map_, src, predecessor_map(&p[0]).
            weight_map(get(&Road::type, *map_)).
            distance_map(&d[0]));

    graph_traits<Map>::vertex_iterator src_iter, dst_iter;
    graph_traits<Map>::vertex_iterator vi, vend;
    for (tie(vi, vend) = vertices(*map_); vi != vend; ++vi) {
        if ((*map_)[*vi].id == rect.first)
            src_iter = vi;
        if ((*map_)[*vi].id == rect.second)
            dst_iter = vi;
    }

    graph_traits<Map>::edge_iterator ei, eend;
    for (vi = dst_iter; vi != src_iter; vi = p[*vi]) {
        /* FIXME: tricky method to break a loop */
        if (*vi == p[*vi])
            break;

        for (tie(ei, eend) = edges(*map_); ei != eend; ++ei) {
            int found = 0;
            if (source(*ei, *map_) == *vi &&
                target(*ei, *map_) == p[*vi])
                found = 1;
            else if (source(*ei, *map_) == p[*vi] &&
                     target(*ei, *map_) == *vi)
                found = 1;

            if (found == 1) {
                Route r;
                r.roadid = (*map_)[*ei].id;
                r.startid = *vi;
                r.endid = p[*vi];
                r.type = (*map_)[*ei].type;
                route.push_back(r);

                // temporaryly set type to max
                (*map_)[*ei].type = std::numeric_limits<int>::max();

                break;
            }
        }
    }

    std::reverse(route.begin(), route.end());
    return route;
}

int
MapManager::on_road(double lat, double lon, double track)
{
    double min = 10000.0;
    std::vector<RawData*>::iterator road_iter =
        database_mgr_->get_raw_data().end();
    std::vector<RawData*>::iterator road_iter2 =
        database_mgr_->get_raw_data().end();

    std::vector<RawData*>::iterator iter =
        database_mgr_->get_raw_data().begin();
    for ( ; iter != database_mgr_->get_raw_data().end(); ++iter) {
        double dist = map_matching(lat, lon, (*iter)->startx, (*iter)->starty,
                                    (*iter)->endx, (*iter)->endy);

        if (dist < min) {
            road_iter = iter;
            min = dist;
        }
    }

    if (road_iter != database_mgr_->get_raw_data().end() &&
        (*road_iter)->traffic_flow == 1) {
        double line_angle =
            angle((*road_iter)->startx, (*road_iter)->starty,
                  (*road_iter)->endx, (*road_iter)->endx);
        double track_abs;

        if (180.0 < track && track < 360.0)
            track_abs = abs(track - 360.0);
        else
            track_abs = track;

        if (line_angle < 0.0)
            line_angle = -line_angle;

        // find another road
        if (abs(track_abs - line_angle) > 90.0) {
            double tmpmin = 10000.0;
            for (iter = database_mgr_->get_raw_data().begin();
                 iter != database_mgr_->get_raw_data().end(); ++iter) {
                double dist = map_matching(lat, lon, (*iter)->startx, (*iter)->starty,
                                            (*iter)->endx, (*iter)->endy);
                line_angle =
                    angle((*iter)->startx, (*iter)->starty,
                          (*iter)->endx, (*iter)->endx);
                if (line_angle < 0.0)
                    line_angle = -line_angle;

                if (dist < tmpmin &&
                    (*iter)->mainid != (*road_iter)->mainid &&
                    abs(line_angle - track_abs) < 90.0) {
                    road_iter2 = iter;
                    tmpmin = dist;
                }
            }

            if (tmpmin < 100.0 &&
                road_iter2 != database_mgr_->get_raw_data().end())
                return (*road_iter2)->mainid;
        }
    }

    /*if (min < 25.0)*/
    if (min < 100.0 &&
        road_iter != database_mgr_->get_raw_data().end())
        return (*road_iter)->mainid;
    else
        return -1;
}

struct DedupedData*
MapManager::get_road_info(int roadid)
{
    std::vector<DedupedData*>::iterator iter;
    iter = database_mgr_->get_deduped_data().begin();
    for (; iter != database_mgr_->get_deduped_data().end(); ++iter) {
        if ((*iter)->mainid == roadid) {
            return *iter;
        }
    }

    return NULL;
}

double
MapManager::map_matching(double lat, double lon,
        double sx, double sy, double ex, double ey)
{
    double lat_radian = lat * PI / 180.0;
    double lat_unit = 110940;
    double lon_unit = (40075360 * cos(lat_radian)) / 360.0;

    double a = sqrt((lon - sx) * lon_unit * (lon - sx) * lon_unit +
            (lat - sy) * lat_unit * (lat - ey) * lat_unit);
    double b = sqrt((lon - ex) * lon_unit * (lon - ex) * lon_unit +
            (lat - ey) * lat_unit * (lat - ey) * lat_unit);
    double c = sqrt((ex - sx) * lon_unit * (ex - sx) * lon_unit +
            (ey - sy) * lat_unit * (ey - sy) * lat_unit);

    if (a * a + c * c - b * b < 0 || b * b + c * c - a * a < 0)
        return 100000.0;

    double p = (a + b + c) / 2.0;
    return 2 * sqrt(abs(p * (p - a) * (p - b) * (p - c))) / c;
}

Route
MapManager::get_route(int id)
{
    Route r;

    r.roadid = -1;
    graph_traits<Map>::edge_iterator ei, eend;
    for (tie(ei, eend) = edges(*map_); ei != eend; ++ei) {
        if ((*map_)[*ei].id == id) {
            r.roadid = (*map_)[*ei].id;
            r.startid = source(*ei, *map_);
            r.endid = target(*ei, *map_);
            r.type = (*map_)[*ei].type;

            return r;
        }
    }

    return r;
}

void
MapManager::print_data()
{
    graph_traits<Map>::edge_iterator ei, eend;
    for (tie(ei, eend) = edges(*map_); ei != eend; ++ei) {
        vertex_descriptor u = boost::source(*ei, *map_);
        vertex_descriptor v = boost::target(*ei, *map_);

        std::cout << "source: " << (*map_)[u].id << " " << (*map_)[u].x << " " << (*map_)[u].y << std::endl;
        std::cout << "target: " << (*map_)[v].id << " " << (*map_)[v].x << " " << (*map_)[v].y << std::endl;
    }
    graph_traits<Map>::vertex_iterator vi, vend;
    for (tie(vi, vend) = vertices(*map_); vi != vend; ++vi)
        std::cout << (*map_)[*vi].id << " " << (*map_)[*vi].x << " " << (*map_)[*vi].y << std::endl;

    for (std::vector<Node>::iterator iter = nodes_.begin();
         iter != nodes_.end(); ++iter)
        std::cout << "id: " << iter->id
                  << "x: " << iter->x
                  << "y: " << iter->y << std::endl;

    for (std::vector<Road>::iterator iter = roads_.begin();
         iter != roads_.end(); ++iter)
        std::cout << "id: " << iter->id
                  << "type: " << iter->type << std::endl;

    for (std::vector<Edge>::iterator iter = edges_.begin();
         iter != edges_.end(); ++iter)
        std::cout << "start: " << iter->first
                  << "end: " << iter->second << std::endl;
}
