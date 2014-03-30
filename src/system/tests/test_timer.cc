#include "timer.h"

extern "C" {
#include <event.h>
}

#include <iostream>

int test(void* data)
{
    std::cout << "haha" << std::endl;
    return 1;
}

int main(void)
{
    GeoSVRTimer *timer;

    event_init();
    timer = geosvr_timer_new(test, NULL, 1000);
    event_dispatch();

    return 0;
}
