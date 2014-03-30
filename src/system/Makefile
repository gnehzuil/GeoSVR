app := src
tests := tests
kernel := kernel_mod
gps := lib/gpsd-2.35

all: gps kernel $(app) $(tests)

test: $(tests)

kernel: $(kernel)
	$(MAKE) --directory=$<

gps: $(gps)
	$(MAKE) --directory=$<

$(app) $(tests):
	$(MAKE) --directory=$@

clean:
	$(MAKE) --directory=src clean
	$(MAKE) --directory=tests clean
	$(MAKE) --directory=$(kernel) clean
	rm -f tags

clean_gps:
	$(MAKE) --directory=$(gps) clean

testload:
	sudo insmod kernel_mod/geosvr.ko bcast_addr_str="192.168.1.249"

load:
	sudo insmod kernel_mod/geosvr.ko

unload:
	sudo rmmod geosvr

run:
	sudo src/geosvrd

tags:
	ctags -R .

.PHONY: clean clean_gps $(app) $(tests) testload load unload run tags
