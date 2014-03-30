#include "map_manager.h"

#include <iostream>

#include "gps_manager.h"
#include "utils.h"

int main(void)
{
    Config config;
    const char* config_file = "geosvr.conf";

    if (load_config(config_file, &config) == -1)
        fprintf(stderr, "read config file error");

    output_config(&config);

    GpsManager gps(&config);
    MapManager map(&gps);
    // test import road data
//    map.print_data();

    // test on_road function
    std::cout << map.on_road(39.986905, 116.309915, 179.370000);

    // test get_rect function
    /*std::pair<int, int> rect = map.get_rect(116.273706, 39.999547, 0.0, 116.273857, 39.999578, 0.0);*/
    /*std::cout << "first: " << rect.first << " second: " << rect.second << std::endl;*/
    
    return 0;
}
