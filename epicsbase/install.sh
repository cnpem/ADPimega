#!/bin/bash

cp epics.sh /etc/profile.d/
chmod +x /etc/profile.d/epics.sh
echo ". /etc/profile.d/epics.sh" >> /etc/bash.bashrc
arch | xargs -i@ echo "/usr/local/epics/base/lib/linux-@" > /etc/ld.so.conf.d/epics.conf
cd /tmp
wget https://epics.anl.gov/download/base/base-3.15.6.tar.gz
tar -xzf base-3.15.6.tar.gz
mkdir /usr/local/epics
mv base-3.15.6 /usr/local/epics/base
cd /usr/local/epics/base
make -j
ln -s /usr/local/epics/base /usr/local/epics/base
rm /tmp/base-3.15.6.tar.gz
