# Standard configuration for PIMEGA detectors
#
# In the following list, when parameters are forwarded to the
# pimegaDetectorConfig iocsh function, the corresponding argument name is
# documented right before the environment variable name and followed by an
# equal sign (=).
#
# Configuration parameters:
#
# $(PREFIX)
# Prefix for all records
#
# detectorModel=$(MODEL_NAME)
# Detector model name, according to the following list.
#
#  - pimega45d
#  - pimega135dl
#  - pimega135d
#  - pimega540d
#  - pimega450d
#  - pimega450ds
#
# address_module{01..10}=$(PIMEGA_MODULE{01..10}_IP)
# IP address for each detector module. Addresses which refer to a non-existent
# module for a given detector model are ignored (and contain a default value),
# therefore can be left undefined.
#
# port=$(PIMEGA_PORT)
# The detector TCP connection port number
#
# SizeX=$(XSIZE)
# The size of the sensor in the X direction.
#
# SizeY=$(YSIZE)
# The size of the sensor in the Y direction.
#
# backend_port=$(COMMAND_PORT)
# Backend port for commands and status
#
# vis_frame_port=$(FRAME_PORT)
# Backend port for frame receiving
#
# $(CALIBRATION_FILE)
# Calibration file path containing default DAC values
#
# Optional parameters:
#
# backendOn=$(BACKEND_ON), default 1 (true)
# Run the IOC without connecting to the backend. Obviously, there will be no
# images received.
#
# log=$(ENABLE_LOG), default 0 (false)
# Enable pimega library logging to file (according to logFile PV)
#
# IntAcqResetRDMA=$(RESET_RDMA), default 1 (true)
# Reset the RDMA buffer before the acquisition
#
# simulate=$(SIMULATION), default 0 (false)
# Activate simulation mode.
#
# maxBuffers=$(ARRAYPOOL_MAX_BUFFERS), default 0 (unlimited)
# The maximum number of NDArray buffers that the NDArrayPool for this driver is
# allowed to allocate.
#
# maxMemory=$(ARRAYPOOL_MAX_MEMORY), default 0 (unlimited)
# The maximum amount of memory that the NDArrayPool for this driver is allowed
# to allocate. Defaults to an unlimited amount of memory.
#
# priority=$(ASYN_THREAD_PRIORITY), default 0
# The thread priority for the asyn port driver thread if ASYN_CANBLOCK is set
# in asynFlags.
#
# stackSize=$(ASYN_STACK_SIZE), default 0
# The stack size for the asyn port driver thread if ASYN_CANBLOCK is set in asynFlags.

on error halt

< envPaths

epicsEnvSet(MODEL_NUM_pimega45d, 1)
epicsEnvSet(MODEL_NUM_pimega135dl, 2)
epicsEnvSet(MODEL_NUM_pimega135d, 3)
epicsEnvSet(MODEL_NUM_pimega540d, 4)
epicsEnvSet(MODEL_NUM_pimega450d, 5)
epicsEnvSet(MODEL_NUM_pimega450ds, 6)

errlogInit(20000)

dbLoadDatabase("$(TOP)/dbd/pimegaApp.dbd")
pimegaApp_registerRecordDeviceDriver(pdbbase) 

# The port name for the detector
epicsEnvSet("PORT",   "PIMEGA")

# The search path for database files
epicsEnvSet("EPICS_DB_INCLUDE_PATH", "$(ADCORE)/db")

pimegaDetectorConfig("$(PORT)",$(PIMEGA_MODULE01_IP="127.0.0.1"), $(PIMEGA_MODULE02_IP="127.0.0.1"),$(PIMEGA_MODULE03_IP="127.0.0.1"),$(PIMEGA_MODULE04_IP="127.0.0.1"),$(PIMEGA_MODULE05_IP="127.0.0.1"),$(PIMEGA_MODULE06_IP="127.0.0.1"),$(PIMEGA_MODULE07_IP="127.0.0.1"),$(PIMEGA_MODULE08_IP="127.0.0.1"),$(PIMEGA_MODULE09_IP="127.0.0.1"),$(PIMEGA_MODULE10_IP="127.0.0.1"),$(PIMEGA_PORT), $(XSIZE), $(YSIZE), $(MODEL_NUM_$(MODEL_NAME)), $(ARRAYPOOL_MAX_BUFFERS="0"), $(ARRAYPOOL_MAX_MEMORY="0"), $(ASYN_THREAD_PRIORITY="0"), $(ASYN_STACK_SIZE="0"), $(SIMULATION="0"), $(BACKEND_ON="1"),$(ENABLE_LOG="0"), $(COMMAND_PORT), $(FRAME_PORT), $(RESET_RDMA="1"))

dbLoadRecords("$(ADPIMEGA)/db/$(MODEL_NAME).db", "P=$(PREFIX), R=cam1:, PORT=$(PORT), ADDR=0, TIMEOUT=1")

# Load asynRecord record
dbLoadRecords("$(ASYN)/db/asynRecord.db", "P=${PREFIX}, R=asyn1,PORT=$(PORT),ADDR=0,OMAX=256,IMAX=256")
asynSetTraceMask($(PORT), 0, 0x00)

# Use larger callback queue to account for the high number of PVs
callbackSetQueueSize(5000)
