#!/bin/bash

cd /tmp
wget https://github.com/EPICS-synApps/support/archive/R6-0.tar.gz
tar -xzf R6-0.tar.gz

cd support-R6-0

sed -i 's:EPICS_BASE=/APSshare/epics/base-3.15.5:EPICS_BASE=/usr/local/epics/base:g' assemble_synApps.sh

./assemble_synApps.sh

mv synApps /usr/local/epics

sed -i 's:SUPPORT=/tmp/support-R6-0/synApps/support:SUPPORT=/usr/local/epics/synApps/support:g' /usr/local/epics/synApps/support/configure/RELEASE

cd /usr/local/epics/synApps/support
make release
make

