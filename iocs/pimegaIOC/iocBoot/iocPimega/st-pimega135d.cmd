#!../../bin/linux-x86_64/pimegaApp

epicsEnvSet("PREFIX", "DET:PIMEGA135D:")
epicsEnvSet("MODEL_NAME", "pimega135d")
epicsEnvSet("XSIZE",  "1536")
epicsEnvSet("YSIZE",  "1536")

epicsEnvSet("PIMEGA_MODULE01_IP", "10.255.255.2")

epicsEnvSet("PIMEGA_PORT", "60000")
epicsEnvSet("FRAME_PORT", "6464")
epicsEnvSet("COMMAND_PORT", "5412")

epicsEnvSet("CALIBRATION_FILE", "/usr/local/etc/adpimega/pimega135d.ini")

< pimega-template.cmd

epicsEnvSet("NELEMENTS", "2359296")

< plugins.cmd

< autosave.cmd

iocInit()

< post-init.cmd
