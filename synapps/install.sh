#!/bin/bash

cd /tmp
wget https://epics.anl.gov/bcda/synApps/tar/synApps_6_1.tar.gz
tar -xvf synApps_6_1.tar.gz

mv synApps_6_1 /usr/local/epics/synApps


