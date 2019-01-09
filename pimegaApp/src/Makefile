TOP=../..

include $(TOP)/configure/CONFIG

# -------------------------------
# Build an Support Module
# -------------------------------

#INC += pimegaDetector.h

LIBRARY_IOC_Linux += pimegaDetector

LIB_SRCS += pimegaDetector.cpp

LIB_SYS_LIBS_Linux += pimega
# ------------------------
# Build the Area Detector Derived Library
# ------------------------
DBD += pimega.dbd


include $(ADCORE)/ADApp/commonLibraryMakefile

include $(TOP)/configure/RULES
