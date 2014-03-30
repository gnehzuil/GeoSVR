#ifndef GEOSVR_TIMER_H_
#define GEOSVR_TIMER_H_

extern "C" {
#include <sys/types.h>
}

typedef int (*TimerCallback)(void* data);

struct GeoSVRTimer;

GeoSVRTimer *geosvr_timer_new(TimerCallback func, void* data,
                              u_int64_t milliseconds);
void geosvr_timer_reset(GeoSVRTimer* timer, u_int64_t milliseconds);
void geosvr_timer_free(GeoSVRTimer** timer);

#endif // GEOSVR_TIMER_H_
