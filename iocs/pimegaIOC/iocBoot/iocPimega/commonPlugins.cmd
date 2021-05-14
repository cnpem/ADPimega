# This is an example file for creating plugins
# It uses the following environment variable macros
# Many of the parameters defined in this file are also in commonPlugins_settings.req so if autosave is being
# use the autosave value will replace the value passed to this file.

# $(PREFIX)      Prefix for all records
# $(PORT)        The port name for the detector.  In autosave.
# $(QSIZE)       The queue size for all plugins.  In autosave.
# $(XSIZE)       The maximum image width; used to set the maximum size for row profiles in the NDPluginStats plugin and 1-D FFT
#                   profiles in NDPluginFFT.
# $(YSIZE)       The maximum image height; used to set the maximum size for column profiles in the NDPluginStats plugin
# $(NCHANS)      The maximum number of time series points in the NDPluginStats, NDPluginROIStats, and NDPluginAttribute plugins
# $(CBUFFS)      The maximum number of frames buffered in the NDPluginCircularBuff plugin
# $(MAX_THREADS) The maximum number of threads for plugins which can run in multiple threads. Defaults to 5.

# Create an HDF5 file saving plugin
NDFileHDF5Configure("FileHDF1", $(QSIZE), 0, "$(PORT)", 0)
dbLoadRecords("NDFileHDF5.template",  "P=$(PREFIX),R=HDF1:,PORT=FileHDF1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT)")

# Create a Magick file saving plugin
#NDFileMagickConfigure("FileMagick1", $(QSIZE), 0, "$(PORT)", 0)
#dbLoadRecords("NDFileMagick.template","P=$(PREFIX),R=Magick1:,PORT=FileMagick1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT)")

# Create 4 ROI plugins
NDROIConfigure("ROI1", $(QSIZE), 0, "$(PORT)", 0, 0, 0, 0, 0, $(MAX_THREADS=5))
dbLoadRecords("NDROI.template",       "P=$(PREFIX),R=ROI1:,  PORT=ROI1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT)")
NDROIConfigure("ROI2", $(QSIZE), 0, "$(PORT)", 0, 0, 0, 0, 0, $(MAX_THREADS=5))
dbLoadRecords("NDROI.template",       "P=$(PREFIX),R=ROI2:,  PORT=ROI2,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT)")
NDROIConfigure("ROI3", $(QSIZE), 0, "$(PORT)", 0, 0, 0, 0, 0, $(MAX_THREADS=5))
dbLoadRecords("NDROI.template",       "P=$(PREFIX),R=ROI3:,  PORT=ROI3,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT)")
NDROIConfigure("ROI4", $(QSIZE), 0, "$(PORT)", 0, 0, 0, 0, 0, $(MAX_THREADS=5))
dbLoadRecords("NDROI.template",       "P=$(PREFIX),R=ROI4:,  PORT=ROI4,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT)")
NDROIConfigure("ROI5", $(QSIZE), 0, "$(PORT)", 0, 0, 0, 0, 0, $(MAX_THREADS=5))
dbLoadRecords("NDROI.template",       "P=$(PREFIX),R=ROI5:,  PORT=ROI5,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT)")
NDROIConfigure("ROI6", $(QSIZE), 0, "$(PORT)", 0, 0, 0, 0, 0, $(MAX_THREADS=5))
dbLoadRecords("NDROI.template",       "P=$(PREFIX),R=ROI6:,  PORT=ROI6,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT)")
NDROIConfigure("ROI7", $(QSIZE), 0, "$(PORT)", 0, 0, 0, 0, 0, $(MAX_THREADS=5))
dbLoadRecords("NDROI.template",       "P=$(PREFIX),R=ROI7:,  PORT=ROI7,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT)")
NDROIConfigure("ROI8", $(QSIZE), 0, "$(PORT)", 0, 0, 0, 0, 0, $(MAX_THREADS=5))
dbLoadRecords("NDROI.template",       "P=$(PREFIX),R=ROI8:,  PORT=ROI8,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT)")
NDROIConfigure("ROI9", $(QSIZE), 0, "$(PORT)", 0, 0, 0, 0, 0, $(MAX_THREADS=5))
dbLoadRecords("NDROI.template",       "P=$(PREFIX),R=ROI9:,  PORT=ROI9,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT)")
NDROIConfigure("ROI10", $(QSIZE), 0, "$(PORT)", 0, 0, 0, 0, 0, $(MAX_THREADS=5))
dbLoadRecords("NDROI.template",       "P=$(PREFIX),R=ROI10:,  PORT=ROI10,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT)")


# Create 8 ROIStat plugins
NDROIStatConfigure("ROISTAT1", $(QSIZE), 0, "$(PORT)", 0, 8, 0, 0, 0, 0, $(MAX_THREADS=5))
dbLoadRecords("NDROIStat.template",   "P=$(PREFIX),R=ROIStat1:  ,PORT=ROISTAT1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT),NCHANS=$(NCHANS)")
dbLoadRecords("NDROIStatN.template",  "P=$(PREFIX),R=ROIStat1:1:,PORT=ROISTAT1,ADDR=0,TIMEOUT=1,NCHANS=$(NCHANS)")
dbLoadRecords("NDROIStatN.template",  "P=$(PREFIX),R=ROIStat1:2:,PORT=ROISTAT1,ADDR=1,TIMEOUT=1,NCHANS=$(NCHANS)")
dbLoadRecords("NDROIStatN.template",  "P=$(PREFIX),R=ROIStat1:3:,PORT=ROISTAT1,ADDR=2,TIMEOUT=1,NCHANS=$(NCHANS)")
dbLoadRecords("NDROIStatN.template",  "P=$(PREFIX),R=ROIStat1:4:,PORT=ROISTAT1,ADDR=3,TIMEOUT=1,NCHANS=$(NCHANS)")
dbLoadRecords("NDROIStatN.template",  "P=$(PREFIX),R=ROIStat1:5:,PORT=ROISTAT1,ADDR=4,TIMEOUT=1,NCHANS=$(NCHANS)")
dbLoadRecords("NDROIStatN.template",  "P=$(PREFIX),R=ROIStat1:6:,PORT=ROISTAT1,ADDR=5,TIMEOUT=1,NCHANS=$(NCHANS)")
dbLoadRecords("NDROIStatN.template",  "P=$(PREFIX),R=ROIStat1:7:,PORT=ROISTAT1,ADDR=6,TIMEOUT=1,NCHANS=$(NCHANS)")
dbLoadRecords("NDROIStatN.template",  "P=$(PREFIX),R=ROIStat1:8:,PORT=ROISTAT1,ADDR=7,TIMEOUT=1,NCHANS=$(NCHANS)")

# Create a processing plugin
NDProcessConfigure("PROC1", $(QSIZE), 0, "$(PORT)", 0, 0, 0)
dbLoadRecords("NDProcess.template",   "P=$(PREFIX),R=Proc1:,  PORT=PROC1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT)")


# Create 5 statistics plugins
NDStatsConfigure("STATS1", $(QSIZE), 0, "$(PORT)", 0, 0, 0, 0, 0, $(MAX_THREADS=5))
dbLoadRecords("NDStats.template",     "P=$(PREFIX),R=Stats1:,  PORT=STATS1,ADDR=0,TIMEOUT=1,HIST_SIZE=256,XSIZE=$(XSIZE),YSIZE=$(YSIZE),NCHANS=$(NCHANS),NDARRAY_PORT=$(PORT)")
NDTimeSeriesConfigure("STATS1_TS", $(QSIZE), 0, "STATS1", 1, 23)
dbLoadRecords("$(ADCORE)/db/NDTimeSeries.template",  "P=$(PREFIX),R=Stats1:TS:, PORT=STATS1_TS,ADDR=0,TIMEOUT=1,NDARRAY_PORT=STATS1,NDARRAY_ADDR=1,NCHANS=$(NCHANS),ENABLED=1")

NDStatsConfigure("STATS2", $(QSIZE), 0, "ROI1",    0, 0, 0, 0, 0, $(MAX_THREADS=5))
dbLoadRecords("NDStats.template",     "P=$(PREFIX),R=Stats2:,  PORT=STATS2,ADDR=0,TIMEOUT=1,HIST_SIZE=256,XSIZE=$(XSIZE),YSIZE=$(YSIZE),NCHANS=$(NCHANS),NDARRAY_PORT=ROI1")
NDTimeSeriesConfigure("STATS2_TS", $(QSIZE), 0, "STATS2", 1, 23)
dbLoadRecords("$(ADCORE)/db/NDTimeSeries.template",  "P=$(PREFIX),R=Stats2:TS:, PORT=STATS2_TS,ADDR=0,TIMEOUT=1,NDARRAY_PORT=STATS2,NDARRAY_ADDR=1,NCHANS=$(NCHANS),ENABLED=1")

NDStatsConfigure("STATS3", $(QSIZE), 0, "ROI2",    0, 0, 0, 0, 0, $(MAX_THREADS=5))
dbLoadRecords("NDStats.template",     "P=$(PREFIX),R=Stats3:,  PORT=STATS3,ADDR=0,TIMEOUT=1,HIST_SIZE=256,XSIZE=$(XSIZE),YSIZE=$(YSIZE),NCHANS=$(NCHANS),NDARRAY_PORT=ROI2")
NDTimeSeriesConfigure("STATS3_TS", $(QSIZE), 0, "STATS3", 1, 23)
dbLoadRecords("$(ADCORE)/db/NDTimeSeries.template",  "P=$(PREFIX),R=Stats3:TS:, PORT=STATS3_TS,ADDR=0,TIMEOUT=1,NDARRAY_PORT=STATS3,NDARRAY_ADDR=1,NCHANS=$(NCHANS),ENABLED=1")

NDStatsConfigure("STATS4", $(QSIZE), 0, "ROI3",    0, 0, 0, 0, 0, $(MAX_THREADS=5))
dbLoadRecords("NDStats.template",     "P=$(PREFIX),R=Stats4:,  PORT=STATS4,ADDR=0,TIMEOUT=1,HIST_SIZE=256,XSIZE=$(XSIZE),YSIZE=$(YSIZE),NCHANS=$(NCHANS),NDARRAY_PORT=ROI3")
NDTimeSeriesConfigure("STATS4_TS", $(QSIZE), 0, "STATS4", 1, 23)
dbLoadRecords("$(ADCORE)/db/NDTimeSeries.template",  "P=$(PREFIX),R=Stats4:TS:, PORT=STATS4_TS,ADDR=0,TIMEOUT=1,NDARRAY_PORT=STATS4,NDARRAY_ADDR=1,NCHANS=$(NCHANS),ENABLED=1")

NDStatsConfigure("STATS5", $(QSIZE), 0, "ROI4",    0, 0, 0, 0, 0, $(MAX_THREADS=5))
dbLoadRecords("NDStats.template",     "P=$(PREFIX),R=Stats5:,  PORT=STATS5,ADDR=0,TIMEOUT=1,HIST_SIZE=256,XSIZE=$(XSIZE),YSIZE=$(YSIZE),NCHANS=$(NCHANS),NDARRAY_PORT=ROI4")
NDTimeSeriesConfigure("STATS5_TS", $(QSIZE), 0, "STATS5", 1, 23)
dbLoadRecords("$(ADCORE)/db/NDTimeSeries.template",  "P=$(PREFIX),R=Stats5:TS:, PORT=STATS5_TS,ADDR=0,TIMEOUT=1,NDARRAY_PORT=STATS5,NDARRAY_ADDR=1,NCHANS=$(NCHANS),ENABLED=1")

NDStatsConfigure("STATS6", $(QSIZE), 0, "ROI5",    0, 0, 0, 0, 0, $(MAX_THREADS=5))
dbLoadRecords("NDStats.template",     "P=$(PREFIX),R=Stats6:,  PORT=STATS6,ADDR=0,TIMEOUT=1,HIST_SIZE=256,XSIZE=$(XSIZE),YSIZE=$(YSIZE),NCHANS=$(NCHANS),NDARRAY_PORT=ROI5")
NDTimeSeriesConfigure("STATS6_TS", $(QSIZE), 0, "STATS6", 1, 23)
dbLoadRecords("$(ADCORE)/db/NDTimeSeries.template",  "P=$(PREFIX),R=Stats6:TS:, PORT=STATS6_TS,ADDR=0,TIMEOUT=1,NDARRAY_PORT=STATS6,NDARRAY_ADDR=1,NCHANS=$(NCHANS),ENABLED=1")

NDStatsConfigure("STATS7", $(QSIZE), 0, "ROI6",    0, 0, 0, 0, 0, $(MAX_THREADS=5))
dbLoadRecords("NDStats.template",     "P=$(PREFIX),R=Stats7:,  PORT=STATS7,ADDR=0,TIMEOUT=1,HIST_SIZE=256,XSIZE=$(XSIZE),YSIZE=$(YSIZE),NCHANS=$(NCHANS),NDARRAY_PORT=ROI6")
NDTimeSeriesConfigure("STATS7_TS", $(QSIZE), 0, "STATS7", 1, 23)
dbLoadRecords("$(ADCORE)/db/NDTimeSeries.template",  "P=$(PREFIX),R=Stats7:TS:, PORT=STATS7_TS,ADDR=0,TIMEOUT=1,NDARRAY_PORT=STATS7,NDARRAY_ADDR=1,NCHANS=$(NCHANS),ENABLED=1")


NDStatsConfigure("STATS8", $(QSIZE), 0, "ROI7",    0, 0, 0, 0, 0, $(MAX_THREADS=5))
dbLoadRecords("NDStats.template",     "P=$(PREFIX),R=Stats8:,  PORT=STATS5,ADDR=0,TIMEOUT=1,HIST_SIZE=256,XSIZE=$(XSIZE),YSIZE=$(YSIZE),NCHANS=$(NCHANS),NDARRAY_PORT=ROI7")
NDTimeSeriesConfigure("STATS8_TS", $(QSIZE), 0, "STATS8", 1, 23)
dbLoadRecords("$(ADCORE)/db/NDTimeSeries.template",  "P=$(PREFIX),R=Stats8:TS:, PORT=STATS8_TS,ADDR=0,TIMEOUT=1,NDARRAY_PORT=STATS8,NDARRAY_ADDR=1,NCHANS=$(NCHANS),ENABLED=1")


NDStatsConfigure("STATS9", $(QSIZE), 0, "ROI8",    0, 0, 0, 0, 0, $(MAX_THREADS=5))
dbLoadRecords("NDStats.template",     "P=$(PREFIX),R=Stats9:,  PORT=STATS9,ADDR=0,TIMEOUT=1,HIST_SIZE=256,XSIZE=$(XSIZE),YSIZE=$(YSIZE),NCHANS=$(NCHANS),NDARRAY_PORT=ROI8")
NDTimeSeriesConfigure("STATS9_TS", $(QSIZE), 0, "STATS9", 1, 23)
dbLoadRecords("$(ADCORE)/db/NDTimeSeries.template",  "P=$(PREFIX),R=Stats9:TS:, PORT=STATS9_TS,ADDR=0,TIMEOUT=1,NDARRAY_PORT=STATS9,NDARRAY_ADDR=1,NCHANS=$(NCHANS),ENABLED=1")


NDStatsConfigure("STATS10", $(QSIZE), 0, "ROI9",    0, 0, 0, 0, 0, $(MAX_THREADS=5))
dbLoadRecords("NDStats.template",     "P=$(PREFIX),R=Stats10:,  PORT=STATS10,ADDR=0,TIMEOUT=1,HIST_SIZE=256,XSIZE=$(XSIZE),YSIZE=$(YSIZE),NCHANS=$(NCHANS),NDARRAY_PORT=ROI9")
NDTimeSeriesConfigure("STATS10_TS", $(QSIZE), 0, "STATS10", 1, 23)
dbLoadRecords("$(ADCORE)/db/NDTimeSeries.template",  "P=$(PREFIX),R=Stats10:TS:, PORT=STATS10_TS,ADDR=0,TIMEOUT=1,NDARRAY_PORT=STATS10,NDARRAY_ADDR=1,NCHANS=$(NCHANS),ENABLED=1")

NDStatsConfigure("STATS11", $(QSIZE), 0, "ROI10",    0, 0, 0, 0, 0, $(MAX_THREADS=5))
dbLoadRecords("NDStats.template",     "P=$(PREFIX),R=Stats11:,  PORT=STATS11,ADDR=0,TIMEOUT=1,HIST_SIZE=256,XSIZE=$(XSIZE),YSIZE=$(YSIZE),NCHANS=$(NCHANS),NDARRAY_PORT=ROI10")
NDTimeSeriesConfigure("STATS11_TS", $(QSIZE), 0, "STATS11", 1, 23)
dbLoadRecords("$(ADCORE)/db/NDTimeSeries.template",  "P=$(PREFIX),R=Stats11:TS:, PORT=STATS11_TS,ADDR=0,TIMEOUT=1,NDARRAY_PORT=STATS11,NDARRAY_ADDR=1,NCHANS=$(NCHANS),ENABLED=1")


# Create a transform plugin
NDTransformConfigure("TRANS1", $(QSIZE), 0, "$(PORT)", 0, 0, 0, 0, 0, $(MAX_THREADS=5))
dbLoadRecords("NDTransform.template", "P=$(PREFIX),R=Trans1:,  PORT=TRANS1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT)")

# Create an overlay plugin with 8 overlays
NDOverlayConfigure("OVER1", $(QSIZE), 0, "$(PORT)", 0, 8, 0, 0, 0, 0, $(MAX_THREADS=5))
dbLoadRecords("NDOverlay.template", "P=$(PREFIX),R=Over1:, PORT=OVER1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT)")
dbLoadRecords("NDOverlayN.template","P=$(PREFIX),R=Over1:1:,NAME=ROI1,   SHAPE=1,O=Over1:,XPOS=$(PREFIX)ROI1:MinX_RBV,YPOS=$(PREFIX)ROI1:MinY_RBV,XSIZE=$(PREFIX)ROI1:SizeX_RBV,YSIZE=$(PREFIX)ROI1:SizeY_RBV,PORT=OVER1,ADDR=0,TIMEOUT=1")
dbLoadRecords("NDOverlayN.template","P=$(PREFIX),R=Over1:2:,NAME=ROI2,   SHAPE=1,O=Over1:,XPOS=$(PREFIX)ROI2:MinX_RBV,YPOS=$(PREFIX)ROI2:MinY_RBV,XSIZE=$(PREFIX)ROI2:SizeX_RBV,YSIZE=$(PREFIX)ROI2:SizeY_RBV,PORT=OVER1,ADDR=1,TIMEOUT=1")
dbLoadRecords("NDOverlayN.template","P=$(PREFIX),R=Over1:3:,NAME=ROI3,   SHAPE=1,O=Over1:,XPOS=$(PREFIX)ROI3:MinX_RBV,YPOS=$(PREFIX)ROI3:MinY_RBV,XSIZE=$(PREFIX)ROI3:SizeX_RBV,YSIZE=$(PREFIX)ROI3:SizeY_RBV,PORT=OVER1,ADDR=2,TIMEOUT=1")
dbLoadRecords("NDOverlayN.template","P=$(PREFIX),R=Over1:4:,NAME=ROI4,   SHAPE=1,O=Over1:,XPOS=$(PREFIX)ROI4:MinX_RBV,YPOS=$(PREFIX)ROI4:MinY_RBV,XSIZE=$(PREFIX)ROI4:SizeX_RBV,YSIZE=$(PREFIX)ROI4:SizeY_RBV,PORT=OVER1,ADDR=3,TIMEOUT=1")
dbLoadRecords("NDOverlayN.template","P=$(PREFIX),R=Over1:5:,NAME=Cursor1,SHAPE=1,O=Over1:,XPOS=junk,                  YPOS=junk,                  XSIZE=junk,                   YSIZE=junk,                   PORT=OVER1,ADDR=4,TIMEOUT=1")
dbLoadRecords("NDOverlayN.template","P=$(PREFIX),R=Over1:6:,NAME=Cursor2,SHAPE=1,O=Over1:,XPOS=junk,                  YPOS=junk,                  XSIZE=junk,                   YSIZE=junk,                   PORT=OVER1,ADDR=5,TIMEOUT=1")
dbLoadRecords("NDOverlayN.template","P=$(PREFIX),R=Over1:7:,NAME=Box1,   SHAPE=1,O=Over1:,XPOS=junk,                  YPOS=junk,                  XSIZE=junk,                   YSIZE=junk,                   PORT=OVER1,ADDR=6,TIMEOUT=1")
dbLoadRecords("NDOverlayN.template","P=$(PREFIX),R=Over1:8:,NAME=Box2,   SHAPE=1,O=Over1:,XPOS=junk,                  YPOS=junk,                  XSIZE=junk,                   YSIZE=junk,                   PORT=OVER1,ADDR=7,TIMEOUT=1")

# Create 2 color conversion plugins
NDColorConvertConfigure("CC1", $(QSIZE), 0, "$(PORT)", 0, 0, 0, 0, 0, $(MAX_THREADS=5))
dbLoadRecords("NDColorConvert.template", "P=$(PREFIX),R=CC1:,  PORT=CC1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT)")
NDColorConvertConfigure("CC2", $(QSIZE), 0, "$(PORT)", 0, 0, 0, 0, 0, $(MAX_THREADS=5))
dbLoadRecords("NDColorConvert.template", "P=$(PREFIX),R=CC2:,  PORT=CC2,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT)")

# Create a circular buffer plugin
NDCircularBuffConfigure("CB1", $(QSIZE), 0, "$(PORT)", 0, $(CBUFFS), 0)
dbLoadRecords("NDCircularBuff.template", "P=$(PREFIX),R=CB1:,  PORT=CB1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT)")

# Create an NDAttribute plugin with 8 attributes
NDAttrConfigure("ATTR1", $(QSIZE), 0, "$(PORT)", 0, 8, 0, 0, 0)
dbLoadRecords("NDAttribute.template",  "P=$(PREFIX),R=Attr1:,    PORT=ATTR1,ADDR=0,TIMEOUT=1,NCHANS=$(NCHANS),NDARRAY_PORT=$(PORT)")
dbLoadRecords("NDAttributeN.template", "P=$(PREFIX),R=Attr1:1:,  PORT=ATTR1,ADDR=0,TIMEOUT=1,NCHANS=$(NCHANS)")
dbLoadRecords("NDAttributeN.template", "P=$(PREFIX),R=Attr1:2:,  PORT=ATTR1,ADDR=1,TIMEOUT=1,NCHANS=$(NCHANS)")
dbLoadRecords("NDAttributeN.template", "P=$(PREFIX),R=Attr1:3:,  PORT=ATTR1,ADDR=2,TIMEOUT=1,NCHANS=$(NCHANS)")
dbLoadRecords("NDAttributeN.template", "P=$(PREFIX),R=Attr1:4:,  PORT=ATTR1,ADDR=3,TIMEOUT=1,NCHANS=$(NCHANS)")
dbLoadRecords("NDAttributeN.template", "P=$(PREFIX),R=Attr1:5:,  PORT=ATTR1,ADDR=4,TIMEOUT=1,NCHANS=$(NCHANS)")
dbLoadRecords("NDAttributeN.template", "P=$(PREFIX),R=Attr1:6:,  PORT=ATTR1,ADDR=5,TIMEOUT=1,NCHANS=$(NCHANS)")
dbLoadRecords("NDAttributeN.template", "P=$(PREFIX),R=Attr1:7:,  PORT=ATTR1,ADDR=6,TIMEOUT=1,NCHANS=$(NCHANS)")
dbLoadRecords("NDAttributeN.template", "P=$(PREFIX),R=Attr1:8:,  PORT=ATTR1,ADDR=7,TIMEOUT=1,NCHANS=$(NCHANS)")

# Create an FFT plugin
NDFFTConfigure("FFT1", 3, 0, "$(PORT)", 0, 0, 0, 0, 0, 5)
dbLoadRecords("NDFFT.template", "P=$(PREFIX), R=FFT1:, PORT=FFT1, ADDR=0, TIMEOUT=1, NDARRAY_PORT=$(PORT), NAME=FFT1, NCHANS=$(XSIZE)")

#set_requestfile_path("./")
#set_requestfile_path("$(ADCORE)/ADApp/Db")
#set_requestfile_path("$(ADCORE)/iocBoot")
#set_savefile_path("./autosave")
#set_pass0_restoreFile("auto_settings.sav")
#set_pass1_restoreFile("auto_settings.sav")
#save_restoreSet_status_prefix("$(PREFIX)")
#dbLoadRecords("$(AUTOSAVE)/asApp/Db/save_restoreStatus.db", "P=$(PREFIX)")
