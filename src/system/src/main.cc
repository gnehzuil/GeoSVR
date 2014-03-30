extern "C" {
#include <errno.h>
#include <event.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
}

#include <cstdio>
#include <cstdlib>
#include <iostream>

#include "conn_manager.h"
#include "gps_manager.h"
#include "log.h"
#include "map_manager.h"
#include "neigh_manager.h"
#include "routing_manager.h"
#include "packet.h"
#include "timer.h"
#include "utils.h"

void
create_log_dir()
{
    if (access(LOGDIR_PATH, F_OK) == 0)
        return;

    if (mkdir(LOGDIR_PATH, 0755) < 0)
        error("failed to create log dir");
}


int
main(int argc, char* argv[])
{
    Config config;
    const char* config_file = "geosvr.conf";
    int c;

    while ((c = getopt(argc, argv, "c:")) != -1) {
        switch (c) {
        case 'c':
            config_file = strdup(optarg);
            break;
        default:
            error("unknown parameters!");
        }
    }

    if (load_config(config_file, &config) == -1)
        error("read config file error");

    output_config(&config);

    create_log_dir();

    event_init();
    RoutingManager routing_mgr(&config);
    event_dispatch();

    return 0;
}
