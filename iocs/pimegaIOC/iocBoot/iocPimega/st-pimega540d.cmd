#!../../bin/linux-x86_64/pimegaApp

epicsEnvSet("PREFIX", "DET:PIMEGA540D:")
epicsEnvSet("MODEL_NAME", "pimega540d")
epicsEnvSet("XSIZE",  "3072")
epicsEnvSet("YSIZE",  "3072")

epicsEnvSet("PIMEGA_MODULE01_IP", "10.255.255.2")
epicsEnvSet("PIMEGA_MODULE02_IP", "10.255.255.6")
epicsEnvSet("PIMEGA_MODULE03_IP", "10.255.255.10")
epicsEnvSet("PIMEGA_MODULE04_IP", "10.255.255.14")

epicsEnvSet("PIMEGA_PORT", "60000")
epicsEnvSet("FRAME_PORT", "6464")
epicsEnvSet("COMMAND_PORT", "5412")

epicsEnvSet("CALIBRATION_FILE", "/usr/local/etc/adpimega/pimega540d.ini")

< pimega-template.cmd

epicsEnvSet("NELEMENTS", "9437184")

< plugins.cmd

< autosave.cmd

iocInit()

< post-init.cmd
