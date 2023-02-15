#!/bin/bash

sudo rm -rf /usr/local/epics/synApps/support/areaDetector-R3-7/ADPimega
sudo cp -r ../../epics /usr/local/epics/synApps/support/areaDetector-R3-7/ADPimega
cd $PIMEGA_PSS/api
sudo bash build.sh -e
cd /usr/local/epics/synApps/support/areaDetector-R3-7/ADPimega
sudo make
cd iocs
sudo make
cd pimegaIOC
sudo make
cd iocBoot
sudo make

