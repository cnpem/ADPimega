#!/bin/bash


CORES=$(lscpu | grep "CPU(s):      " | awk '{ print $2; }')
cd /tmp
wget https://epics.anl.gov/download/base/base-3.15.6.tar.gz
tar -xzf base-3.15.6.tar.gz
mkdir /usr/local/epics
mv base-3.15.6 /usr/local/epics
cd /usr/local/epics/base-3.15.6
make -j$CORES
ln -s /usr/local/epics/base-3.15.6 /usr/local/epics/base
rm /tmp/base-3.15.6.tar.gz
