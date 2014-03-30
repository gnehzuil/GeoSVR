#ifndef GEOSVR_LOG_H_
#define GEOSVR_LOG_H_

#define LOGDIR_PATH "log"

#include <cstdio>
extern "C" {
#include <glib.h>
}

#define warning(fmt, ...) g_warning("%s: " fmt, __func__, ##__VA_ARGS__)
#define error(fmt, ...) g_error("%s: " fmt, __func__, ##__VA_ARGS__)
#define message(fmt, ...) g_message(fmt, ##__VA_ARGS__)

#ifdef DEBUG_ENABLED
#define debug(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
#else
#define debug(fmt, ...)
#endif // DEBUG_ENABLED

#endif // GEOSVR_LOG_H_
