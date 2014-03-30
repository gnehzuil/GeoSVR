#!/bin/bash

sudo rmmod geosvr
sudo insmod kernel_mod/geosvr.ko bcast_addr_str="192.168.1.249"
gnome-terminal -e "sudo src/geosvrd"
