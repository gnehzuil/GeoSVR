#ifndef GEOSVR_UTILS_H_
#define GEOSVR_UTILS_H_

extern "C" {
#include <arpa/inet.h>
#include <sys/types.h>
}

#include <cmath>
#include <cstring>

#define PI 3.1415926

struct Config {
    const char* local_addr;
    const char* bcast_addr;
    u_int32_t listen_port;
    u_int32_t beacon_period;
    u_int32_t entry_expire_period;
    u_int32_t list_cleaner_period;
    int enable_device;

    int enable_track;
    const char* track_file;
    const char* bdcast_file;
    const char* recv_file;
    const char* node_file;
    const char* neigh_file;
    const char* forward_file;
    int enable_record;
    const char* record_file;
};

/* This function lets you print more than one IP address at the same time */
inline char *ip_to_str(struct in_addr addr)
{
    static char buf[16 * 4];
    static int index = 0;
    char *str;
    addr.s_addr = htonl(addr.s_addr);
    strcpy(&buf[index], inet_ntoa(addr));
    str = &buf[index];
    index += 16;
    index %= 64;

    return str;
}

inline char *ip_to_str(u_int32_t addr)
{
    static char buf[16 * 4];
    static int index = 0;
    char *str;
    struct in_addr tmpaddr;

    tmpaddr.s_addr = htonl(addr);
    strcpy(&buf[index], inet_ntoa(tmpaddr));
    str = &buf[index];
    index += 16;
    index %= 64;

    return str;
}

#if 0
inline double distance(double x1, double y1, double x2, double y2)
{
    return sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2));
}
#endif

inline double distance(double xa, double ya, double xb, double yb)
{
    double r = 6371.0 * 1000;

    double x1 = xa / 180 * PI;
    double y1 = ya / 180 * PI;
    double x2 = xb / 180 * PI;
    double y2 = yb / 180 * PI;

    return acos(sin(y1) * sin(y2) + cos(y1) * cos(y2) * cos(x2 - x1)) * r;
}

inline double angle(double x1, double y1, double x2, double y2)
{
    if (x1 == x2) {
        if (y1 < y2)
            return 0.0;
        else
            return 180.0;
    } else if (y1 == y2) {
        if (x1 < x2)
            return 90.0;
        else
            return 270.0;
    } else if (x1 == x2 && y1 == y2) {
        return 0;
    } else {
        double alpha = atan2(y2 - y1, x2 - x1);
        alpha = alpha * 180.0 / PI;

        if (0.0 < alpha && alpha < 90.0)
            return -(90.0 - alpha);
        else if (90.0 < alpha && alpha < 180.0)
            return alpha - 90.0;
        else if (-180.0 < alpha && alpha < -90.0)
            return -alpha;
        else
            return alpha - 90.0;
    }
}

inline double dist_node2line(double lat, double lon,
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

    double p = (a + b + c) / 2.0;
    return 2 * sqrt(fabs(p * (p - a) * (p - b) * (p - c))) / c;
}

int load_config(const char* file, struct Config* config);
void output_config(struct Config* config);

void now(char *buf, size_t n);

#endif // GEOSVR_UTILS_H_
