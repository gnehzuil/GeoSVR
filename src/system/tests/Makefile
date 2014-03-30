CPP = g++
VPATH = ../src ../include ../tests
CPPFLAGS += -DDEBUG_ENABLED `mysql_config --cflags` `pkg-config glib-2.0 --cflags` -g -O -Wall -I../include -I../lib/gpsd-2.35 -I../kernel_mod
LDLIBS += -levent `mysql_config --libs` `pkg-config glib-2.0 --libs` ../lib/gpsd-2.35/.libs/libgps.a

all: test_conn_mgr test_db_mgr test_map_mgr test_timer test_neigh_mgr

test_conn_mgr: conn_manager.o utils.o
test_db_mgr: database_manager.o
test_map_mgr: map_manager.o database_manager.o gps_manager.o utils.o
test_timer: timer.o
test_neigh_mgr: conn_manager.o neigh_manager.o timer.o gps_manager.o neigh_list.o list.o map_manager.o database_manager.o utils.o

clean:
	rm -rf *.o test_db_mgr test_map_mgr test_timer test_neigh_mgr

.PHONY: clean
