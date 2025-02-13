TOP=../..

include $(TOP)/configure/CONFIG

# -------------------------------
# Build an Support Module
# -------------------------------

USR_INCLUDES +=-I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include
USR_INCLUDES +=-I$(LIBPIMEGA_INCLUDE_PATH)

LIBRARY_IOC_Linux += pimegaDetector

LIB_SRCS += pimegaDetector.cpp

pimegaDetector_CXXFLAGS += '-DADPIMEGA_VERSION="$(shell git describe --tags)"'

# ------------------------
# Build the Area Detector Derived Library
# ------------------------
DBD += pimega.dbd

USR_CXXFLAGS += -O0 -g
USR_CFLAGS += -O0 -g

include $(ADCORE)/ADApp/commonLibraryMakefile

include $(TOP)/configure/RULES
