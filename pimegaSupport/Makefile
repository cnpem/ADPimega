TOP=..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

USR_INCLUDES += -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include

INC += pimega.h
INC += decode.h
INC += utils.h
INC += lib/acquisition.h
INC += lib/backend_config.h
INC += lib/backend_interface.h
INC += lib/config_file.h
INC += lib/config.h
INC += lib/dac.h
INC += lib/debug.h
INC += lib/generic.h
INC += lib/load.h
INC += lib/monitoring.h
INC += lib/omr.h
INC += lib/scan.h
INC += lib/sd_card.h
INC += lib/system.h
INC += lib/test_pulse.h
INC += lib/trigger.h
INC += driver/US_acquisition.h
INC += driver/US_dac.h
INC += driver/US_debug.h
INC += driver/US_ethernet.h
INC += driver/US_generic.h
INC += driver/US_interface.h
INC += driver/US_load.h
INC += driver/US_monitoring.h
INC += driver/US_omr.h
INC += driver/US_scan.h
INC += driver/US_sd_card.h
INC += driver/US_serial.h
INC += driver/US_system.h
INC += driver/US_test_pulse.h
INC += driver/US_trigger.h
INC += compatibility/compatibility.h
INC += default/default/trigger.h

ifeq (linux-x86_64, $(findstring linux-x86_64, $(T_A)))
LIB_INSTALLS_Linux += ../os/linux-x86_64/libpimega.so
endif

ifeq (linux-ppc64, $(findstring linux-ppc64, $(T_A)))
LIB_INSTALLS_Linux += ../os/linux-ppc64/libpimega.so
endif

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
