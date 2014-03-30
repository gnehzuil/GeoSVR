/* $Id: gpsd_dbus.h 4371 2007-06-01 21:17:33Z esr $ */
#ifdef DBUS_ENABLE

#ifndef _gpsd_dbus_h_
#define _gpsd_dbus_h_

#include <dbus/dbus.h>

int initialize_dbus_connection (void);
void send_dbus_fix (struct gps_device_t* channel);

#endif /* _gpsd_dbus_h_ */

#endif
