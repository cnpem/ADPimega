#!/bin/bash

rm -rf /usr/local/epics
cp data/epics.sh /etc/profile.d/
cp data/RELEASE /tmp/
cp data/RELEASE_motor-r7-1 /tmp/
cp data/RELEASE_ipac /tmp/
cp -r ../../epics /tmp/ADPimega
chmod +x /etc/profile.d/epics.sh
echo ". /etc/profile.d/epics.sh" >> /etc/bash.bashrc
arch | xargs -i@ echo "/usr/local/epics/base/lib/linux-@" > /etc/ld.so.conf.d/epics.conf
cd /tmp
wget https://epics.anl.gov/download/base/base-3.15.6.tar.gz
tar -xzf base-3.15.6.tar.gz
wget https://epics.anl.gov/bcda/synApps/tar/synApps_6_1.tar.gz
tar -xvf synApps_6_1.tar.gz
mkdir /usr/local/epics
mv base-3.15.6 /usr/local/epics/base
mv synApps_6_1 /usr/local/epics/synApps
mv RELEASE /usr/local/epics/synApps/support/configure/RELEASE
mv RELEASE_motor-r7-1 /usr/local/epics/synApps/support/motor-R7-1/configure/RELEASE
mv RELEASE_ipac /usr/local/epics/synApps/support/ipac-2-15/configure/RELEASE
cd /usr/local/epics/base
make -j
ln -s /usr/local/epics/base /usr/local/epics/base
cd /usr/local/epics/synApps/support
make release
make
mv /tmp/ADPimega /usr/local/epics/synApps/support/areaDetector-R3-7/

