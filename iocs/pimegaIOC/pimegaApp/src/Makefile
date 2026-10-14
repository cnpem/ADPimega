TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

PROD_NAME = pimegaApp
PROD_IOC_WIN32  += $(PROD_NAME)
PROD_IOC_Linux  += $(PROD_NAME)

# <name>.dbd will be created from <name>Include.dbd
DBD +=  $(PROD_NAME).dbd
$(PROD_NAME)_DBD += pimega.dbd
#$(PROD_NAME)_DBD += drvAsynIPPort.dbd

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
$(PROD_NAME)_SRCS += $(PROD_NAME)_registerRecordDeviceDriver.cpp $(PROD_NAME)Main.cpp

# Add locally compiled object code
PROD_LIBS += pimegaDetector
PROD_LIBS += pimega


include $(ADCORE)/ADApp/commonDriverMakefile

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

