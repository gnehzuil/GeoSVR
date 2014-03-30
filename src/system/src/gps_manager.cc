#include "gps_manager.h"

extern "C" {
#include "gps.h"
}

#include <cstdlib>
#include <cstring>

#include "log.h"
#include "timer.h"
#include "utils.h"

GpsManager::GpsManager(Config* config)
{
    data_buf_ = NULL;

    enable_device_ = config->enable_device;
    if (enable_device_) {
        gps_data_ = gps_open(NULL, DEFAULT_GPSD_PORT);
        if (gps_data_ == NULL)
            error("cannot open gps device file.\n");
    } else {
        data_buf_ = (struct gps_fix_t *)malloc(sizeof(struct gps_fix_t));
        if (data_buf_ == NULL)
            error("out of memory");
        memset(data_buf_, 0, sizeof(struct gps_fix_t));
    }

    if (config->enable_track) {
        track_file_ = fopen(config->track_file, "a+");
        if (track_file_ == NULL)
            error("cannot open gps track file");
        fprintf(track_file_, "====\n");
    }

    if (config->enable_record) {
        record_file_ = fopen(config->record_file, "r");
        if (record_file_ == NULL)
            error("cannot open gps record file");
    }
}

GpsManager::~GpsManager()
{
    if (record_file_ != NULL)
        fclose(record_file_);

    if (track_file_ != NULL)
        fclose(track_file_);

    if (enable_device_)
        gps_close(gps_data_);
    else
        if (data_buf_ != NULL)
            free(data_buf_);
}

int
GpsManager::read_gps_data()
{
    if (enable_device_) {
        if (gps_query(gps_data_, "o") == -1)
            return -1;
        if (gps_data_->online == 0 || gps_data_->status < STATUS_FIX)
            return -1;

        data_buf_ = &gps_data_->fix;
    } else if (record_file_ != NULL) {
        read_gps_data_from_file();
    } else {
        /* XXX: we use a constant value for debug */
        data_buf_->time = 1234567890.0;
        data_buf_->latitude = 39.123456;
        data_buf_->longitude = 116.123456;
        data_buf_->track = 0.0;
        data_buf_->speed = 40.00;
    }

    log_track();

    return 0;
}

void
GpsManager::log_track()
{
    char buf[200];

    debug("[gps mgr] [%.6f] %.6f %.6f %f %f\n", data_buf_->time,
            data_buf_->latitude, data_buf_->longitude,
            data_buf_->track, data_buf_->speed);

    if (track_file_ == NULL)
        return;

    now(buf, 200);
    fprintf(track_file_, "[%s] %.6f %.6f %.6f %f %f\n",
            buf, data_buf_->time,
            data_buf_->latitude, data_buf_->longitude,
            data_buf_->track, data_buf_->speed);
    fflush(track_file_);
}

void
GpsManager::read_gps_data_from_file(void)
{
    double time, lat, lon, track, speed;

    fscanf(record_file_, "%lf %lf %lf %lf %lf", &time, &lat, &lon, &speed, &track);
    debug("[gps mgr] %.6f %.6f %.6f %.6f %.6f\n", time, lat, lon, speed, track);

    data_buf_->time = time;
    data_buf_->latitude = lat;
    data_buf_->longitude = lon;
    data_buf_->speed = speed;
    data_buf_->track = track;
}
