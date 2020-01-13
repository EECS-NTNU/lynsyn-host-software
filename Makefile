###############################################################################
#
# Lynsyn Makefile
#
# Compiles host software for all Lynsyn boards
#
# Usage:
#    make            Builds all host software
#    make clean      Cleans everything
#
# Make sure to adjust settings at the top of this file for your system
#
###############################################################################

###############################################################################
# Settings for the host tools

export QMAKE = qmake CONFIG+=release

###############################################################################
###############################################################################
# Everything below this should not need any changes

export HOSTDIR = $(shell pwd)

ifeq ($(OS),Windows_NT)
export CFLAGS = -I$(HOSTDIR)/../argp-standalone-1.3/ -I/mingw64/include/libusb-1.0/
export LDFLAGS = -largp -L$(HOSTDIR)/../argp-standalone-1.3/
HOST_EXECUTABLES=bin/lynsyn_tester bin/lynsyn_sampler bin/lynsyn_xsvf bin/lynsyn_viewer
else
HOST_EXECUTABLES=bin/lynsyn_tester bin/lynsyn_sampler bin/lynsyn_xvc bin/lynsyn_xsvf bin/lynsyn_viewer
endif

export CFLAGS += -g -O2 -Wall -I/usr/include/libusb-1.0/ -I$(HOSTDIR)/common/ -I$(HOSTDIR)/liblynsyn/ 
export LDFLAGS += -lusb-1.0 
export CXXFLAGS = -std=gnu++11 $(CFLAGS)

export CC = gcc
export CPP = g++
export LD = g++
export AR = ar
export RANLIB = ranlib

###############################################################################

.PHONY: host_software
host_software: $(HOST_EXECUTABLES)
	@echo
	@echo "Host software compilation successful"
	@echo

.PHONY: bin/lynsyn_tester
bin/lynsyn_tester:
	mkdir -p bin
	cd lynsyn_tester && $(MAKE)
	cp lynsyn_tester/lynsyn_tester bin

.PHONY: bin/lynsyn_sampler
bin/lynsyn_sampler:
	cd lynsyn_sampler && $(MAKE)
	cp lynsyn_sampler/lynsyn_sampler bin

.PHONY: bin/lynsyn_xvc
bin/lynsyn_xvc:
	cd lynsyn_xvc && $(MAKE)
	cp lynsyn_xvc/lynsyn_xvc bin

.PHONY: bin/lynsyn_xsvf
bin/lynsyn_xsvf:
	cd libxsvf && $(MAKE) lynsyn_xsvf
	cp libxsvf/lynsyn_xsvf bin

.PHONY: bin/lynsyn_viewer
bin/lynsyn_viewer:
	mkdir -p lynsyn_viewer/build
	cd lynsyn_viewer/build && $(QMAKE) ..
	cd lynsyn_viewer/build && $(MAKE)
ifeq ($(OS),Windows_NT)
	cp lynsyn_viewer/build/release/lynsyn_viewer bin
else
	cp lynsyn_viewer/build/lynsyn_viewer bin
endif

###############################################################################

.PHONY: install
install: host_software install_hw
	cp bin/* /usr/bin/
	cp .jtagdevices /etc/jtagdevices
	@echo
	@echo "Software and hardware installed"
	@echo

###############################################################################

.PHONY: install_hw
install_hw:
	cp udev/48-lynsyn.rules /etc/udev/rules.d
	udevadm control --reload-rules

###############################################################################

.PHONY : clean
clean:
	cd lynsyn_tester && $(MAKE) clean
	cd lynsyn_sampler && $(MAKE) clean
	cd libxsvf && $(MAKE) clean
	cd lynsyn_xvc && $(MAKE) clean
	rm -rf lynsyn_viewer/build
	rm -rf liblynsyn/*.o bin fwbin
