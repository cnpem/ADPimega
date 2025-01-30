#!/bin/bash
ROOT_DIR=$PWD
EPICS_BASE_VERSION='7.0.8.1'
SYNAPPPS_VERSION_1='6_3'
SYNAPPPS_VERSION='R6-3'
EPICS_INSTALL_PATH="/home/$USER/.local/share/epics"
DOWNLOADS_PATH="$EPICS_INSTALL_PATH/../tmp_download"
MAX_CPUS=$(($(nproc)-1))
INFO='\033[0;32m'
WARN='\033[0;33m'
NC='\033[0m'

unused_modules=(
    "CAMAC"
    "CAPUTRECORDER"
    "DAC128V"
    "DELAYGEN"
    "DXP"
    "DXPSITORO"
    "ETHERIP"
    "GALIL"
    "IP"
    "IP330"
    "IPUNIDIG"
    "LABJACK"
    "LUA"
    "LOVE"
    "MCA"
    "MEASCOMP"
    "MOTOR"
    "MODBUS"
    "OPTICS"
    "QUADEM"
    "SCALER"
    "SOFTGLUE"
    "SOFTGLUEZYNQ"
    "STD"
    "STREAM"
    "VAC"
    "VME"
    "XSPRESS3"
    "YOKOGAWA_DAS"
    "XXX"
    "ALLEN_BRADLEY"
    "ULDAQ"
)

CleanBeforeInstall(){
    echo -e "${INFO}==> Removing existing EPICS installation folder ${NC}"
    rm -rf $EPICS_INSTALL_PATH
}

SetupInstaller() {
    echo -e "${INFO}==> Creating EPICS install directory ${NC}"
    set +e
    mkdir -p $EPICS_INSTALL_PATH
}

DownloadRequirements() {
    echo -e "${INFO}==> Downloading requirements ${NC}"
    cd $EPICS_INSTALL_PATH
    EPICS_DL_FILE=base-$EPICS_BASE_VERSION.tar.gz
    SYNAPPS_DL_FILE=synApps_$SYNAPPPS_VERSION_1.tar.gz

    mkdir $DOWNLOADS_PATH
    cd $DOWNLOADS_PATH

    if [ -f "$EPICS_DL_FILE" ]; then
        echo -e "${WARN}==> $EPICS_DL_FILE already exists ${NC}"
    else
        echo -e "${INFO}==> Downloading $EPICS_DL_FILE ${NC}"
        wget https://epics.anl.gov/download/base/$EPICS_DL_FILE
    fi
    tar -xzf $EPICS_DL_FILE

    if [ -f "$SYNAPPS_DL_FILE" ]; then
        echo -e "${WARN}==> $SYNAPPS_DL_FILE already exists ${NC}"
    else
        echo -e "${INFO}==> Downloading $SYNAPPS_DL_FILE ${NC}"
        wget https://github.com/EPICS-synApps/support/releases/download/$SYNAPPPS_VERSION/synApps_$SYNAPPPS_VERSION_1.tar.gz
    fi

    tar -xzf $SYNAPPS_DL_FILE
    cd -

}

CompileEPICSBase() {
    echo -e "${INFO}==> Setting EPICS Base in $EPICS_INSTALL_PATH ${NC}"
    cd $EPICS_INSTALL_PATH
    mv $DOWNLOADS_PATH/base-$EPICS_BASE_VERSION $EPICS_INSTALL_PATH/base
    cd $EPICS_INSTALL_PATH/base
    # Set the install location on EPICS CONFIG_SITE to be used by all dependencies
    sed -i "/^#INSTALL_LOCATION=/c\INSTALL_LOCATION=$EPICS_INSTALL_PATH/base" configure/CONFIG_SITE

    echo -e "${INFO}==> Build EPICS BASE ${NC}"
    make -j$MAX_CPUS -s -Wfatal-errors
    if [ $? -eq 0 ];then
        echo -e "${INFO}==> EPICS Base installed ${NC}"
    else
        echo -e "${WARN}==> EPICS Base failed to install ${NC}"
        exit 1
    fi
}

CompileSynapps() {
    # See: https://areadetector.github.io/areaDetector/install_guide.html
    echo -e "${INFO}==> Preparing SynApps ${NC}"
    cd $EPICS_INSTALL_PATH
    # Move new support modules
    mv $DOWNLOADS_PATH/synApps_$SYNAPPPS_VERSION_1 $EPICS_INSTALL_PATH/synApps

    # Update release files
    SYNAPPS_RELEASE_FILE=$EPICS_INSTALL_PATH/synApps/support/configure/RELEASE
    # Update SUPPORT path
    sed -i "/^SUPPORT=/c\SUPPORT=$EPICS_INSTALL_PATH/synApps/support" $SYNAPPS_RELEASE_FILE
    # Update EPICS_BASE install path
    sed -i "/^EPICS_BASE=/c\EPICS_BASE=$EPICS_INSTALL_PATH/base" $SYNAPPS_RELEASE_FILE

    for module in "${unused_modules[@]}"; do
        sed -i "s/^$module=/#$module=/" $SYNAPPS_RELEASE_FILE
    done

    . /etc/os-release
    if [[ $NAME =~ "Ubuntu" && $VERSION_ID =~ "22." ]]; then
        # Fix for Ubuntu 22
        # Enable TIRPC for ASYN
        sed -i "/^# TIRPC/c\TIRPC = YES" $EPICS_INSTALL_PATH/synApps/support/asyn-$ASYN_VERSION/configure/CONFIG_SITE
    fi

    echo -e "${INFO}==> Setup release files ${NC}"
    # This step updates all .local files with default areaDetector values
    cd $EPICS_INSTALL_PATH/synApps/support/areaDetector-$AREA_DETECTOR_VERSION/configure
    bash copyFromExample

    # Clear the default list of drivers that should be built(they are not imported during download)
    echo '' > RELEASE.local.linux-x86_64
    cd $EPICS_INSTALL_PATH/synApps/support

    # Update release files
    make release -s

    echo -e "${INFO}==> Build synApps Support ${NC}"
    make -j$MAX_CPUS -s -Wfatal-errors
    if [ $? -eq 0 ];then
        echo -e "${INFO}==> synApps installed ${NC}"
    else
        echo -e "${WARN}==> synApps failed to install ${NC}"
        exit 1
    fi
}

echo -e "${INFO}==> Starting EPICS installation ${NC}"
CleanBeforeInstall
SetupInstaller
DownloadRequirements
CompileEPICSBase
CompileSynapps
echo -e "${INFO}==> Script done! ${NC}"
