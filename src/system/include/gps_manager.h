#ifndef GEOSVR_GPS_MANAGER_H_
#define GEOSVR_GPS_MANAGER_H_

extern "C" {
#include "gps.h"
}

#include <cstdio>
#include <utility>

struct Config;

class GpsManager {
    public:
        GpsManager(Config* config);
        virtual ~GpsManager();

        int read_gps_data();

        double get_latitude() {
            if (data_buf_ == NULL)
                return 0.0;
            return transform_fmt(data_buf_->latitude);
        }

        double get_longitude() { 
            if (data_buf_ == NULL)
                return 0.0;
            return transform_fmt(data_buf_->longitude);
        }

        double get_timestamp() {
            if (data_buf_ == NULL)
                return 0.0;
            return data_buf_->time;
        }

        double get_track() {
            if (data_buf_ == NULL)
                return 0.0;
            return data_buf_->track;
        }

        double get_speed() {
            if (data_buf_ == NULL)
                return 0.0;
            return data_buf_->speed;
        }

        void log_track();

    private:
        struct gps_fix_t* data_buf_;
        struct gps_data_t* gps_data_;

        int enable_device_;
        FILE* track_file_;
        FILE* record_file_;

        void read_gps_data_from_file(void);

        /*
         * XX: we can refer rz600 test program
         * e.g. nlat = (int)lat - (lat - (int)lat) * 5.0 / 3.0
         */
        inline double transform_fmt(double in) {
            /* XXX: need to transform latitude and longitude in windows */
            /*return (int)in + (in - (int)in) * 100.0 / 60.0;*/

            /* XXX: we don not need to transform in linux */
            return in;
        }
};

#endif // GEOSVR_GPS_MANAGER_H_
