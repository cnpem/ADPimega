/*
 * pimegaDetector.h
 */

// Standard includes
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <iostream>
#include <map>

// EPICS includes
#include <cantProceed.h>
#include <epicsEvent.h>
#include <epicsExit.h>
#include <epicsExport.h>
#include <epicsMutex.h>
#include <epicsStdio.h>
#include <epicsString.h>
#include <epicsThread.h>
#include <iocsh.h>

// Asyn driver includes
#include <asynOctetSyncIO.h>

// areaDetector includes
#include "ADDriver.h"

// pimega lib includes
#include <lib/acquisition.h>
#include <lib/backend_config.h>
#include <lib/backend_interface.h>
#include <lib/config.h>
#include <lib/config_file.h>
#include <lib/dac.h>
#include <lib/debug.h>
#include <lib/generic.h>
#include <lib/load.h>
#include <lib/pimega_thread.h>
#include <lib/monitoring.h>
#include <lib/omr.h>
#include <lib/scan.h>
#include <lib/sd_card.h>
#include <lib/system.h>
#include <lib/test_pulse.h>
#include <lib/trigger.h>
#include <lib/zmq_message_broker.hpp>
#include <pimega.h>

#define PIMEGA_MAX_FILENAME_LEN 300
#define MAX_BAD_PIXELS 100
/** Time to poll when reading from Labview */
#define ASYN_POLL_TIME .01

/** Time between checking to see if image file is complete */
#define FILE_READ_DELAY .01

#define DIMS 2
#define DEFAULT_POLL_TIME 2

#define N_DACS_OUTS 31
static const char *driverName = "pimegaDetector";

using vis_dtype = uint32_t;

#define error(fmt, ...) \
  asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s:%d " fmt, __FILE__, __LINE__, __VA_ARGS__)

#define UPDATEIOCSTATUS(x)         \
  do {                             \
    updateIOCStatus(x, sizeof(x)); \
  } while (0)

#define UPDATESERVERSTATUS(x)         \
  do {                                \
    updateServerStatus(x, sizeof(x)); \
  } while (0)

typedef enum ioc_trigger_mode_t {
  IOC_TRIGGER_MODE_INTERNAL = 0,
  IOC_TRIGGER_MODE_EXTERNAL = 1,
  IOC_TRIGGER_MODE_ALIGNMENT = 2
} ioc_trigger_mode_t;

#define pimegaMedipixModeString "MEDIPIX_MODE"
#define pimegaefuseIDString "EFUSE_ID"
#define pimegaOmrOPModeString "OMR_OP_MODE"
#define pimegaMedipixBoardString "MEDIPIX_BOARD"
#define pimegaMedipixChipString "MEDIPIX_CHIP"
#define pimegaPixeModeString "PIXEL_MODE"
#define pimegaContinuosRWString "CONTINUOUSRW"
#define pimegaPolarityString "POLARITY"
#define pimegaDiscriminatorString "DISCRIMINATOR"
#define pimegaTestPulseString "TEST_PULSE"
#define pimegaCounterDepthString "COUNTER_DEPTH"
#define pimegaEqualizationString "EQUALIZATION"
#define pimegaGainString "GAIN_MODE"
#define pimegaDacBiasString "DAC_BIAS"
#define pimegaDacCasString "CAS"
#define pimegaDacDelayString "DELAY"
#define pimegaDacDiscString "DISC"
#define pimegaDacDiscHString "DISC_H"
#define pimegaDacDiscLString "DISC_L"
#define pimegaDacDiscLSString "DISC_LS"
#define pimegaDacFbkString "FBK"
#define pimegaDacGndString "GND"
#define pimegaDacIKrumString "IKRUM"
#define pimegaDacPreampString "PREAMP"
#define pimegaDacRpzString "RPZ"
#define pimegaDacShaperString "SHAPER"
#define pimegaThreshold0String "THRESHOLD0"
#define pimegaThreshold1String "THRESHOLD1"
#define pimegaDacTPBufferInString "TP_BUFFER_IN"
#define pimegaDacTPBufferOutString "TP_BUFFER_OUT"
#define pimegaDacTPRefString "TP_REF"
#define pimegaDacTPRefAString "TP_REF_A"
#define pimegaDacTPRefBString "TP_REF_B"
#define pimegaResetString "RESET"
#define pimegaReadCounterString "READ_COUNTER"
#define pimegaSenseDacSelString "SENSE_DAC_SEL"
#define pimegaDacOutSenseString "DAC_OUT_SENSE"
#define pimegaDacsOutSenseString "DACS_OUT_SENSE"
#define pimegaBackendBufferString "BACK_BUFFER"
#define pimegaResetRDMABufferString "RESET_RDMA_BUFFER"
#define pimegaSensorBiasString "SENSOR_BIAS"
#define pimegaModuleString "PIMEGA_MODULE"
#define pimegaAllModulesString "ALL_MODULES"
#define pimegaBackendLFSRString "BACK_LFSR"
#define pimegaSendImageString "SEND_IMAGE"
#define pimegaEnergyString "THRESHOLD_ENERGY"
#define pimegaLoadEqStartString "LOAD_EQUALIZATION_START"
#define pimegaSelSendImageString "SEL_SEND_IMAGE"
#define pimegaSendDacDoneString "SEND_DAC_DONE"
#define pimegaConfigDiscLString "CONFIG_DISCL"
#define pimegaLoadEqString "LOAD_EQUALIZATION"
#define pimegaExtBgInString "EXT_BGIN"
#define pimegaExtBgSelString "EXT_BGSEL"
#define pimegaReadMBTemperatureString "READ_MB_TEMPERATURE"
#define pimegaTempMonitorEnableString "TEMP_MONITOR_ENABLE"
#define pimegaModuleTempStatusString "MODULE_TEMP_STATUS"
#define pimegaModuleTempHighestString "MODULE_TEMP_HIGHEST"
#define pimegaModuleMbTempString "MODULE_MB_TEMPERATURE"
#define pimegaModuleSensorTempString "MODULE_SENSOR_TEMPERATURE"
#define pimegaModuleMBAvgString "MODULE_MB_AVG_TSENSOR"
#define pimegaReadSensorTemperatureString "READ_SENSOR_TEMPERATURE"
#define pimegaModuleMPAvgString "MODULE_MP_AVG_TSENSOR"
#define pimegaDacDefaultsString "DAC_DEFAULTS"
#define pimegaCheckSensorsString "CHECK_SENSORS"
#define pimegaModuleDisabledSensorsString "MODULE_DISABLED_SENSORS"
#define pimegaMBSendModeString "MB_SEND_MODE"
#define pimegaEnableBulkProcessingString "ENABLE_BULK_PROCESSING"
#define pimegaAbortSaveString "ABORT_SAVE"
#define pimegaIndexIDString "INDEX_ID"
#define pimegaIndexEnableString "INDEX_ENABLE"
#define pimegaAcquireShmemEnableString "ACQ_TO_SHMEM_ENABLE"
#define pimegaIndexSendModeString "INDEX_SEND_MODE"
#define pimegaProcessedCounterString "PROCESSED_COUNTER"
#define pimegaDistanceString "DISTANCE"
#define pimegaLogFileString "LOGFILE"
#define pimegaIOCStatusMsgString "IOC_STATUS_MESSAGE"
#define pimegaServerStatusMsgString "SERVER_STATUS_MESSAGE"
#define pimegaTraceMaskWarningString "TRACE_MASK_WARNING"
#define pimegaTraceMaskErrorString "TRACE_MASK_ERROR"
#define pimegaTraceMaskDriverIOString "TRACE_MASK_DRIVERIO"
#define pimegaTraceMaskFlowString "TRACE_MASK_FLOW"
#define pimegaTraceMaskString "TRACE_MASK"
#define pimegaModuleLostFrameCountString "MODULE_LOST_FRAME_COUNT"
#define pimegaModuleRxFrameCountString "MODULE_RECEIVED_FRAME_COUNT"
#define pimegaModuleAcquisitionCountString "MODULE_RECEIVED_ACQUISITION_COUNT"
#define pimegaBackendStatsString "BACKEND_STATS"
#define pimegaMetadataFieldString "METADATA_FIELD"
#define pimegaMetadataValueString "METADATA_VALUE"
#define pimegaMetadataOMString "METADATA_OM"
#define pimegaFrameProcessModeString "FRAME_PROCESS_MODE"

class pimegaDetector : public ADDriver {
 public:
  pimegaDetector(const char *portName, const char *address_module01, const char *address_module02,
                 const char *address_module03, const char *address_module04,
                 const char *address_module05, const char *address_module06,
                 const char *address_module07, const char *address_module08,
                 const char *address_module09, const char *address_module10, int port, int maxSizeX,
                 int maxSizeY, int detectorModel, int maxBuffers, size_t maxMemory, int priority,
                 int stackSize, int simulate, int backendOn, int log, unsigned short backend_port,
                 unsigned short vis_frame_port, int IntAcqResetRDMA);

  virtual asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value);
  virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
  virtual asynStatus readInt32(asynUser *pasynUser, epicsInt32 *value);
  virtual asynStatus readFloat64(asynUser *pasynUser, epicsFloat64 *value);
  virtual asynStatus readFloat32Array(asynUser *pasynUser, epicsFloat32 *value, size_t nElements,
                                      size_t *nIn);
  virtual asynStatus writeOctet(asynUser *pasynUser, const char *value, size_t maxChars,
                                size_t *nActual);
  virtual asynStatus writeInt32Array(asynUser *pasynUser, epicsInt32 *value, size_t nElements);
  virtual void report(FILE *fp, int details);
  virtual void alarmTask(void);
  virtual void acqTask(void);
  virtual void captureTask(void);
  virtual void updateEpicsFrame(vis_dtype* data);
  void updateIOCStatus(const char *message, int size);
  void updateServerStatus(const char *message, int size);
  void newImageTask();
  void finishAcq(int trigger, int &acquire, int &acquireStatus,
                 uint64_t &receivedBackendCountOffset, int numExposuresVar);

  // Debugging routines
  asynStatus initDebugger(int initDebug);
  asynStatus debugLevel(const std::string &method, int onOff);
  asynStatus debug(const std::string &method, const std::string &msg);
  asynStatus debug(const std::string &method, const std::string &msg, int value);
  asynStatus debug(const std::string &method, const std::string &msg, double value);
  asynStatus debug(const std::string &method, const std::string &msg, const std::string &value);

 protected:
  int PimegaReset;
#define FIRST_PIMEGA_PARAM PimegaReset
  int PimegaMedipixMode;
  int PimegaefuseID;
  int PimegaOmrOPMode;
  int PimegaMedipixBoard;
  int PimegaMedipixChip;
  int PimegaContinuosRW;
  int PimegaPolarity;
  int PimegaDiscriminator;
  int PimegaPixelMode;
  int PimegaTestPulse;
  int PimegaCounterDepth;
  int PimegaEqualization;
  int PimegaGain;
  int PimegaDacBias;
  int PimegaCas;
  int PimegaDelay;
  int PimegaDisc;
  int PimegaDiscH;
  int PimegaDiscL;
  int PimegaDiscLS;
  int PimegaFbk;
  int PimegaGnd;
  int PimegaIkrum;
  int PimegaPreamp;
  int PimegaRpz;
  int PimegaShaper;
  int PimegaThreshold0;
  int PimegaThreshold1;
  int PimegaTpBufferIn;
  int PimegaTpBufferOut;
  int PimegaTpRef;
  int PimegaTpRefA;
  int PimegaTpRefB;
  int PimegaReadCounter;
  int PimegaSenseDacSel;
  int PimegaDacOutSense;
  int PimegaDacsOutSense;
  int PimegaBackBuffer;
  int PimegaResetRDMABuffer;
  int PimegaBackLFSR;
  int PimegaModule;
  int PimegaAllModules;
  int PimegaSendImage;
  int PimegaSelSendImage;
  int PimegaSendDacDone;
  int PimegaConfigDiscL;
  int PimegaLoadEqualization;
  int PimegaExtBgIn;
  int PimegaExtBgSel;
  int PimegaReadMBTemperature;
  int PimegaTempMonitorEnable;
  int PimegaModuleTemperatureStatus;
  int PimegaModuleTemperatureHighest;
  int PimegaModuleMBTemperature;
  int PimegaModuleMBAvgTSensor;
  int PimegaMBSelTSensor;
  int PimegaMBTSensor;
  int PimegaAcqShmemEnable;
  int PimegaReadSensorTemperature;
  int PimegaModuleSensorTemperature;
  int PimegaModuleMPAvgTSensor;
  int pimegaDacDefaults;
  int PimegaCheckSensors;
  int PimegaModuleDisabledSensors;
  int PimegaMBSendMode;
  int PimegaSensorBias;
  int PimegaEnergy;
  int PimegaEnableBulkProcessing;
  int PimegaAbortSave;
  int PimegaIndexID;
  int PimegaIndexEnable;
  int PimegaIndexSendMode;
  int PimegaProcessedImageCounter;
  int PimegaDistance;
  int PimegaLoadEqStart;
  int PimegaIOCStatusMessage;
  int PimegaServerStatusMessage;
  int PimegaTraceMaskWarning;
  int PimegaTraceMaskError;
  int PimegaTraceMaskDriverIO;
  int PimegaTraceMask;
  int PimegaTraceMaskFlow;
  int PimegaModuleLostFrameCount;
  int PimegaModuleRxFrameCount;
  int PimegaModuleAcquisitionCount;
  int PimegaBackendStats;
  int PimegaMetadataField;
  int PimegaMetadataValue;
  int PimegaMetadataOM;
  int PimegaFrameProcessMode;
  NDArray *PimegaNDArray = NULL;
  int PimegaLogFile;
  bool BoolAcqResetRDMA = false;
  IMessageConsumer* message_consumer = nullptr;
#define LAST_PIMEGA_PARAM PimegaLogFile

 private:
  // debug map
  std::map<std::string, int> debugMap_;

  // ***** poller control variables ****
  double pollTime_;
  int forceCallback_;
  // ***********************************

  epicsEventId startAcquireEventId_;
  epicsEventId stopAcquireEventId_;
  epicsEventId startCaptureEventId_;
  epicsEventId stopCaptureEventId_;
  epicsEvent stopCaptureCompletedEvent;

  pimega_t *pimega;
  int maxSizeX;
  int maxSizeY;

  int arrayCallbacks;
  size_t dims[2];
  int itemp;

  epicsInt32 *PimegaDisabledSensors_;
  epicsFloat32 *PimegaDacsOutSense_;
  epicsFloat32 *PimegaMBTemperature_;

  int numImageSaved;
  uint64_t recievedBackendCountOffset;

  void panic(const char *msg);
  void connect(const char *address[4], unsigned short port,
          unsigned short backend_port, unsigned short vis_frame_port);
  void createParameters(void);
  void setParameter(int index, const char *value, int address = 0);
  void setParameter(int index, int value, int address = 0);
  void setParameter(int index, double value, int address = 0);
  void getParameter(int index, int maxChars, char *value);
  void getParameter(int index, int *value);
  void getParameter(int index, double *value);
  asynStatus getDacsValues(void);
  asynStatus getOmrValues(void);

  asynStatus setDefaults(void);
  asynStatus getDacsOutSense(void);
  asynStatus getMbTemperature(void);
  asynStatus getMedipixTemperatures(void);
  asynStatus getMedipixAvgTemperature(void);
  asynStatus startAcquire(void);
  asynStatus stopAcquire();
  asynStatus startCaptureBackend(void);
  void signalAndWaitCaptureToStop();
  asynStatus waitForBackendStatus(int);

  asynStatus dac_scan_tmp(pimega_dac_t dac);
  asynStatus selectModule(uint8_t module);
  asynStatus medipixMode(uint8_t mode);
  asynStatus configDiscL(int value);
  asynStatus triggerMode(ioc_trigger_mode_t trigger);
  asynStatus reset(short action);
  asynStatus setDACValue(pimega_dac_t dac, int value, int parameter);
  asynStatus setOMRValue(pimega_omr_t dac, int value, int parameter);
  asynStatus imgChipID(uint8_t chip_id);
  asynStatus medipixBoard(uint8_t board_id);
  asynStatus numExposures(unsigned number);
  asynStatus acqPeriod(double period_time_s);
  asynStatus acqTime(double acquire_time_s);
  asynStatus sensorBias(float voltage);
  asynStatus readCounter(int counter);
  asynStatus senseDacSel(u_int8_t dac);
  asynStatus setPixelMode(pimega_pixel_mode_t mode);
  // asynStatus imageMode(u_int8_t mode);
  asynStatus sendImage(void);
  asynStatus checkSensors(void);
  asynStatus loadEqualization(uint32_t *cfg);
  asynStatus setExtBgIn(float voltage);
  asynStatus dacDefaults(const char *file);
  asynStatus getExtBgIn(void);
  asynStatus setThresholdEnergy(float energy);
  asynStatus getThresholdEnergy(void);
  asynStatus metadataHandler(int op_mode);
  asynStatus setTempMonitor(int enable);
  asynStatus getTemperatureStatus(void);
  asynStatus getTemperatureHighest(void);
  asynStatus configureNumImages(ioc_trigger_mode_t trigger_mode);
};

#define NUM_pimega_PARAMS (&LAST_pimega_PARAM - &FIRST_pimega_PARAM + 1)
