#include "timer.h"

#include <cstdlib>

extern "C" {
#include <sys/types.h>
#include <sys/time.h>

#include <event.h>
}

#include "log.h"

struct GeoSVRTimer {
    struct event ev_;
    struct timeval tv_;
    TimerCallback func_;
    void* data_;
    bool in_cb_;
};

static void
timer_callback(int fd, short event, void *vtimer)
{
    int more;
    GeoSVRTimer* timer = (GeoSVRTimer*) vtimer;

    timer->in_cb_ = true;
    more = (*timer->func_)(timer->data_);
    timer->in_cb_ = false;

    if (more)
        evtimer_add(&timer->ev_, &timer->tv_);
    else
        geosvr_timer_free(&timer);
}

void
geosvr_timer_free(GeoSVRTimer** ptimer)
{
    GeoSVRTimer* timer;

    if (ptimer == NULL)
        return;

    timer = *ptimer;
    *ptimer = NULL;

    if (timer != NULL && !timer->in_cb_) {
        event_del(&timer->ev_);
        free(timer);
        timer = NULL;
    }
}

struct timeval
timeval_from_msec(u_int64_t milliseconds)
{
    struct timeval ret;
    const u_int64_t microseconds = milliseconds * 1000;

    ret.tv_sec = microseconds / 1000000;
    ret.tv_usec = microseconds % 1000000;

    return ret;
}

void
geosvr_timer_reset(GeoSVRTimer* timer, u_int64_t milliseconds)
{
    timer->tv_ = timeval_from_msec(milliseconds);
    timer->in_cb_ = false;

    evtimer_del(&timer->ev_);
    evtimer_add(&timer->ev_, &timer->tv_);
}

GeoSVRTimer*
geosvr_timer_new(TimerCallback func, void* data, u_int64_t milliseconds)
{
    GeoSVRTimer* timer;

    timer = (struct GeoSVRTimer *)malloc(sizeof(struct GeoSVRTimer));
    if (timer == NULL)
        error("out of memory\n");

    timer->tv_ = timeval_from_msec(milliseconds);
    timer->func_ = func;
    timer->data_ = data;

    evtimer_set(&timer->ev_, timer_callback, timer);
    evtimer_add(&timer->ev_, &timer->tv_);

    return timer;
}
