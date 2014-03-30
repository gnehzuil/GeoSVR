#include "conn_manager.h"

extern "C" {
#include <geosvr_netlink.h>
}

#include <cstring>
#include <cmath>
#include <iostream>

#include "utils.h"

int main(void)
{
    Config config;
    const char* config_file = "geosvr.conf";

    if (load_config(config_file, &config) == -1)
        fprintf(stderr, "read config file error");

    output_config(&config);

    event_init();
    struct geosvr_nlmsg msg;

    memset(&msg, 0, sizeof(msg));
    ConnManager conn_mgr(&config);
    conn_mgr.nl_send_msg((void *)&msg, sizeof(msg), GEOSVR_RSP_SEND);

    event_dispatch();

    return 0;
}
