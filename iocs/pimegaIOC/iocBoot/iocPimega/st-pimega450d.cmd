#!../../bin/linux-x86_64/pimegaApp

epicsEnvSet("PREFIX", "DET:PIMEGA450D:")
epicsEnvSet("MODEL_NAME", "pimega450d")
epicsEnvSet("XSIZE",  "15360")
epicsEnvSet("YSIZE",  "512")

epicsEnvSet("PIMEGA_MODULE01_IP", "10.255.255.2")
epicsEnvSet("PIMEGA_MODULE02_IP", "10.255.255.3")
epicsEnvSet("PIMEGA_MODULE03_IP", "10.255.255.4")
epicsEnvSet("PIMEGA_MODULE04_IP", "10.255.255.5")
epicsEnvSet("PIMEGA_MODULE05_IP", "10.255.255.6")
epicsEnvSet("PIMEGA_MODULE06_IP", "10.255.255.7")
epicsEnvSet("PIMEGA_MODULE07_IP", "10.255.255.8")
epicsEnvSet("PIMEGA_MODULE08_IP", "10.255.255.9")
epicsEnvSet("PIMEGA_MODULE09_IP", "10.255.255.10")
epicsEnvSet("PIMEGA_MODULE10_IP", "10.255.255.11")

epicsEnvSet("PIMEGA_PORT", "60000")
epicsEnvSet("FRAME_PORT", "6464")
epicsEnvSet("COMMAND_PORT", "5412")

epicsEnvSet("CALIBRATION_FILE", "/usr/local/etc/adpimega/pimega450d.ini")

< pimega-template.cmd

epicsEnvSet("NELEMENTS", "7864320")

< plugins.cmd

< autosave.cmd

iocInit()

< post-init.cmd
