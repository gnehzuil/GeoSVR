CPPOBJS = main.o routing_manager.o database_manager.o map_manager.o gps_manager.o conn_manager.o timer.o neigh_manager.o list.o neigh_list.o utils.o

CPP = g++
CC = gcc
VPATH = src ../include ../tests ../kernel_mod
CFLAGS += $(DEBUG) `mysql_config --cflags` `pkg-config glib-2.0 --cflags` -g -O -Wall -I../include -I../lib/gpsd-2.35 -I../kernel_mod
CPPFLAGS = $(CFLAGS) -std=c++0x
LDLIBS += -levent `mysql_config --libs` `pkg-config glib-2.0 --libs` ../lib/gpsd-2.35/.libs/libgps.a

all: geosvrd

geosvrd: $(CPPOBJS)
	$(CPP) $(CPPFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.cc
	$(CPP) $(CPPFLAGS) -c $< -o $@

clean:
	rm -rf geosvrd *.o

.PHONY: clean
