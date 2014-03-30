#!/bin/bash

sudo rmmod geosvr
sudo insmod kernel_mod/geosvr.ko
gnome-terminal -e "sudo src/geosvrd"
