< envPaths

errlogInit(20000)

dbLoadDatabase("$(TOP)/dbd/pimegaApp.dbd")
pimegaApp_registerRecordDeviceDriver(pdbbase) 

# Prefix for all records
epicsEnvSet("PREFIX", "SOL7:")
# The port name for the detector
epicsEnvSet("PORT",   "PIMEGA")
# The queue size for all plugins
epicsEnvSet("QSIZE",  "20")
# The maximim image width; used for row profiles in the NDPluginStats plugin
epicsEnvSet("XSIZE",  "512")
# The maximim image height; used for column profiles in the NDPluginStats plugin
epicsEnvSet("YSIZE",  "512")
# The maximum number of time seried points in the NDPluginStats plugin
epicsEnvSet("NCHANS", "2048")
# The maximum number of frames buffered in the NDPluginCircularBuff plugin
epicsEnvSet("CBUFFS", "500")
# The IP address of the Pimega system
epicsEnvSet("PIMEGA_IP", "127.0.0.1")
#epicsEnvSet("PIMEGA_IP", "10.0.27.32")
# The IP port for the command socket
epicsEnvSet("PIMEGA_PORT", "60000")
# The search path for database files
epicsEnvSet("EPICS_DB_INCLUDE_PATH", "$(ADCORE)/db")

# pimegaDetectorConfig(
#              portName,           # The name of the asyn port to be created
#              address, 	       # The ip address of the pimega detector
#              port,               # the number port of pimega detector
#              maxSizeX,           # The size of the pimega detector in the X direction.
#              maxSizeY,           # The size of the pimega detector in the Y direction.
#              maxBuffers,         # The maximum number of NDArray buffers that the NDArrayPool for this driver is
#                                    allowed to allocate. Set this to 0 to allow an unlimited number of buffers.
#              maxMemory,          # The maximum amount of memory that the NDArrayPool for this driver is
#                                    allowed to allocate. Set this to 0 to allow an unlimited amount of memory.
#              priority,           # The thread priority for the asyn port driver thread if ASYN_CANBLOCK is set in asynFlags.
#              stackSize,          # The stack size for the asyn port driver thread if ASYN_CANBLOCK is set in asynFlags.
pimegaDetectorConfig("$(PORT)",$(PIMEGA_IP),$(PIMEGA_PORT), $(XSIZE), $(YSIZE), 0, 0, 0, 0)


dbLoadRecords("$(ADPIMEGA)/db/pimega.template","P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1")

# Create a standard arrays plugin, set it to get data from pimega driver.
#NDStdArraysConfigure("Image1", 5, 0, "$(PORT)", 0, 0)

#dbLoadRecords("$(ADCORE)/db/NDStdArrays.template", "P=$(PREFIX),R=image1:,PORT=Image1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT),TYPE=Int16,FTVL=SHORT,NELEMENTS=262144")

# Load all other plugins using commonPlugins.cmd
< $(ADCORE)/iocBoot/commonPlugins.cmd
set_requestfile_path("$(ADPIMEGA)/pimegaApp/Db")


iocInit()

# save things every thirty seconds
#create_monitor_set("auto_settings.req", 30,"P=$(PREFIX)")
