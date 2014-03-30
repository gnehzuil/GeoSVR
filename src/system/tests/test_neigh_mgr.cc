#include "neigh_manager.h"

extern "C" {
#include <event.h>
}

#include "conn_manager.h"
#include "gps_manager.h"
#include "map_manager.h"
#include "timer.h"
#include "utils.h"

int main(void)
{
    Config config;
    const char* config_file = "geosvr.conf";

    if (load_config(config_file, &config) == -1)
        fprintf(stderr, "read config file error");

    output_config(&config);

    event_init();
    GpsManager gps_mgr(&config);
    MapManager map_mgr(&gps_mgr);
    ConnManager conn_mgr(&config);
    NeighManager neigh_mgr(&conn_mgr, &gps_mgr, &map_mgr, &config);
    event_dispatch();
    return 0;
}
