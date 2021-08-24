cd /usr/local/epics/ && \
wget http://www.aps.anl.gov/epics/download/extensions/extensionsTop_20120904.tar.gz && \
tar -xzf extensionsTop_20120904.tar.gz && \
rm extensionsTop_20120904.tar.gz

cd /usr/local/epics/extensions/src && \
 wget http://downloads.sourceforge.net/project/procserv/2.6.1/procServ-2.6.1.tar.gz && \
tar -xzf procServ-2.6.1.tar.gz

rm /usr/local/epics/extensions/src/procServ-2.6.1.tar.gz

cd /usr/local/epics/extensions/src/procServ-2.6.1 && ./configure && make && make install 
