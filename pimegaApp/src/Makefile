TOP=../..

include $(TOP)/configure/CONFIG

# -------------------------------
# Build an Support Module
# -------------------------------

USR_INCLUDES += $(addprefix -I, $(GLIB_INCLUDE))
USR_INCLUDES += $(addprefix -I, $(PIMEGA_INCLUDE))

LIBRARY_IOC_Linux += pimegaDetector

LIB_SRCS += pimegaDetector.cpp

LIB_SYS_LIBS_Linux += pimega
# ------------------------
# Build the Area Detector Derived Library
# ------------------------
DBD += pimega.dbd

USR_CXXFLAGS += -O0 -g
USR_CFLAGS += -O0 -g

include $(ADCORE)/ADApp/commonLibraryMakefile

include $(TOP)/configure/RULES
