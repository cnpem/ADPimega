/* pimegaDetector.cpp
 *
 * This is a driver for the Pimega detector (Quad chip version but supports other chip counts)
 *
 * The driver is designed to communicate with the chip via the matching Labview controller over TCP/IP
 *
 * Author: Douglas Araujo
 *         Brazilian Synchrotron Light Laboratory.
 *
 * Created:  Jan 09 2019
 *
 * Original Source from pilatusDetector by Mark Rivers and from merlinDetector by Giles Knap.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdint.h>

// #include <epicsTime.h>
#include <epicsThread.h>
#include <epicsEvent.h>
#include <epicsMutex.h>
#include <epicsString.h>
#include <epicsStdio.h>
#include <epicsMutex.h>
#include <cantProceed.h>
#include <iocsh.h>
#include <epicsExport.h>
#include <epicsExit.h>

#include <asynOctetSyncIO.h>

#include <pimega.h>
#include "ADDriver.h"

#include "pimegaDetector.h"

static void pollerThreadC(void * drvPvt)
{
    pimegaDetector *pPvt = (pimegaDetector *)drvPvt;
    pPvt->pollerThread();
}

static void acquisitionTaskC(void *drvPvt)
{
    pimegaDetector *pPvt = (pimegaDetector *) drvPvt;

    pPvt->acqTask();
}

/** This thread controls acquisition, reads image files to get the image data, and
 * does the callbacks to send it to higher layers
 * It is totally decoupled from the command thread and simply waits for data
 * frames to be sent on the data channel (TCP) regardless of the state in the command
 * thread and TCP channel */
void pimegaDetector::acqTask()
{
    int status = asynSuccess;
    int eventStatus=0;
    int imageMode;
    int acquire=0;
    double acquireTime, acquirePeriod;

    int statusParam = 0;

    const char *functionName = "acqTask";

    this->lock();

    /* Loop forever */
    while (1) { 

        if (!acquire)  {
            // Release the lock while we wait for an event that says acquire has started, then lock again
            this->unlock();
            asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
                "%s:%s: waiting for acquire to start\n", driverName, functionName);
            status = epicsEventWait(startEventId_);
            setStringParam(ADStatusMessage, "Acquiring data");   
            this->lock();
            acquire = 1;  
        }

        /* We are acquiring. */
        getIntegerParam(ADImageMode, &imageMode);

        /* Get the exposure parameters */
        getDoubleParam(ADAcquireTime, &acquireTime);
        getDoubleParam(ADAcquirePeriod, &acquirePeriod);

        /* Open the shutter */
        setShutter(ADShutterOpen);

        // TODO: test acquire
        for (int j = 0; j < 2; j++) 
            {
                medipixBoard(j);
                imgChipID(0);          // all chips
                US_AcquireTime(pimega, acquireTime);    // set acquire time
                US_NumExposures(pimega, 1);             // set number of exposures
                US_Acquire(pimega, 1); // informs the detector that it are ready to recive a trigger signal
            }
    
        Set_Trigger(pimega, 1);
        Set_Trigger(pimega, 0);

        setStringParam(ADStatusMessage, "Acquiring data");
        setIntegerParam(ADStatus, ADStatusAcquire);
        callParamCallbacks();

        while (acquire) {
            US_DetectorState_RBV(pimega);
            this->unlock();
            eventStatus = epicsEventWaitWithTimeout(this->stopEventId_, 0);
            this->lock();

            if (eventStatus == epicsEventWaitOK) {
                US_Acquire(pimega,0);
                acquire=0;
                setStringParam(ADStatusMessage, "Acquisition aborted");
                setIntegerParam(ADStatus, ADStatusAborted);
                callParamCallbacks();
                break;
            }
            if (!strcmp(pimega->cached_result.detector_state,"Done")) 
            {
                acquire=0;
                setIntegerParam(ADStatus, ADStatusIdle);
                setStringParam(ADStatusMessage, "Acquisition finished");
                continue;
            }

        }
        setShutter(0);
        setIntegerParam(ADAcquire, 0);

        /* Call the callbacks to update any changes */
        callParamCallbacks();        
    }

}

void pimegaDetector::pollerThread()
{
    /* This function runs in a separate thread. It waits for the poll time */
    static const char *functionName = "pollerThread";
    epicsFloat64 actualtemp;
    epicsInt32 _i=0;

    while(1) 
    {
        
        lock();
        // Read the digital inputs
        US_TemperatureActual(pimega);

        actualtemp = pimega->cached_result.actual_temperature;

        printf("Valor da temperature atual: %.2f e valor de i: %d\n", actualtemp, _i);

        forceCallback_ = 0;

        _i++;
        setParameter(ADTemperatureActual, actualtemp);

        callParamCallbacks();
        unlock();
        epicsThreadSleep(pollTime_);
        
    }
}


asynStatus pimegaDetector::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
    int function = pasynUser->reason;
    int status = asynSuccess;
    static const char *functionName = "writeInt32";
    const char *paramName;

    int adstatus;
    int acquiring;

    getParamName(function, &paramName);
    printf("Valor de function: %d\n", function);
    printf("Nome da funcao: %s\n", paramName);
    printf("Valor de value: %d\n", value);
       

    /* Ensure that ADStatus is set correctly before we set ADAcquire.*/
    getIntegerParam(ADStatus, &adstatus);

    /*
    if (function == ADAcquire) {

        printf("valor de adstatus: %d\n", adstatus);

      if (value && ((adstatus == ADStatusIdle) || adstatus == ADStatusError || adstatus == ADStatusAborted)) {
        setStringParam(ADStatusMessage, "Acquiring data");
        setIntegerParam(ADStatus, ADStatusAcquire);
      }
      if ((!value)  && (adstatus == ADStatusAcquire)) { 
        setStringParam(ADStatusMessage, "Acquisition aborted");
        setIntegerParam(ADStatus, ADStatusAborted);
      }
    }

    */
 
    /* Set the parameter and readback in the parameter library.  This may be overwritten when we read back the
     * status at the end, but that's OK */
    status |= setIntegerParam(function, value);

    if (function == ADAcquire) {
        if (value && (adstatus == ADStatusIdle || adstatus == ADStatusError || adstatus == ADStatusAborted)) {
            /* Send an event to wake up the Pilatus task.  */
            epicsEventSignal(this->startEventId_);
        } 
        if (!value && (adstatus == ADStatusAcquire)) {
          /* This was a command to stop acquisition */
            epicsEventSignal(this->stopEventId_);
            epicsThreadSleep(.1);
        }
    }

    else if (function == PimegaOmrOPMode)
        status |= omrOpMode(value);

    else if (function == ADNumExposures)
        status |=  numExposures(value);

    else if (function == PimegaReset)
        status |=  reset(value);
    
    else if (function == ADTriggerMode)
        status |=  triggerMode(value);
    
    else if (function == PimegaMedipixBoard)
        status |= medipixBoard(value);
    else if (function == PimegaMedipixChip)
        status |= imgChipID(value);

    else if (function == PimegaPixelMode)
        status |= pixelMode(value);
    else if (function == PimegaContinuosRW)
        status |= continuosRW(value);
    else if (function == PimegaPolarity)
        status |= polarity(value);
    else if (function == PimegaDiscriminator)
        status |= discriminator(value);
    else if (function == PimegaTestPulse)
        status |= enableTP(value);
    else if (function == PimegaCounterDepth)
        status |= counterDepth(value);
    else if (function == PimegaEqualization)
        status |= equalization(value);
    else if (function == PimegaGain)
        status |= gainMode(value);

    //DACS functions
    else if (function == PimegaCas)
        status |= setDACValue(HS_CAS, value, function);
    else if (function == PimegaDelay) 
        status |=  setDACValue(HS_Delay, value, function);
    else if (function == PimegaDisc) 
        status |=  setDACValue(HS_Disc, value, function);
    else if (function == PimegaDiscH) 
        status |=  setDACValue(HS_DiscH, value, function);
    else if (function == PimegaDiscL) 
        status |=  setDACValue(HS_DiscL, value, function);
    else if (function == PimegaDiscLS) 
        status |=  setDACValue(HS_DiscLS, value, function);
    else if (function == PimegaFbk) 
        status |=  setDACValue(HS_FBK, value, function);
    else if (function == PimegaGnd) 
        status |=  setDACValue(HS_GND, value, function);
    else if (function == PimegaIkrum) 
        status |=  setDACValue(HS_IKrum, value, function);
    else if (function == PimegaPreamp) 
        status |=  setDACValue(HS_Preamp, value, function);
    else if (function == PimegaRpz) 
        status |=  setDACValue(HS_RPZ, value, function);
    else if (function == PimegaShaper) 
        status |=  setDACValue(HS_Shaper, value, function);
    else if (function == PimegaThreshold0) 
        status |=  setDACValue(HS_ThresholdEnergy0, value, function);
    else if (function == PimegaThreshold1) 
        status |=  setDACValue(HS_ThresholdEnergy1, value, function);
    else if (function == PimegaTpBufferIn) 
        status |=  setDACValue(HS_TPBufferIn, value, function);
    else if (function == PimegaTpBufferOut) 
        status |=  setDACValue(HS_TPBufferOut, value, function);
    else if (function == PimegaTpRef) 
        status |=  setDACValue(HS_TPRef, value, function);
    else if (function == PimegaTpRefA) 
        status |=  setDACValue(HS_TPRefA, value, function);
    else if (function == PimegaTpRefB) 
        status |=  setDACValue(HS_TPRefB, value, function);
    else 
    {
        if (function < FIRST_PIMEGA_PARAM) 
                status = ADDriver::writeInt32(pasynUser, value);    
    }

    if (status) 
        asynPrint(pasynUser, ASYN_TRACE_ERROR, 
              "%s:%s: error, status=%d function=%d, value=%d\n", 
              driverName, functionName, status, function, value);
    else {     
         /* Update any changed parameters */
        callParamCallbacks();   
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
              "%s:%s: function=%d, value=%d\n", 
              driverName, functionName, function, value);
    }

    return((asynStatus)status); 

}

asynStatus pimegaDetector::writeFloat64(asynUser *pasynUser, epicsFloat64 value)
{
    int function = pasynUser->reason;
    int status = asynSuccess;
    const char *paramName;
    const char *functionName = "writeFloat64";

    getParamName(function, &paramName);
    printf("Valor de function: %d\n", function);
    printf("Nome da funcao: %s\n", paramName);
    printf("Valor de value: %f\n", value);

    callParamCallbacks();
    status |= setDoubleParam(function, value);

    if (function == ADAcquireTime)
        status |= acqTime(value);

    else {
    /* If this parameter belongs to a base class call its method */
        if (function < FIRST_PIMEGA_PARAM) status = ADDriver::writeFloat64(pasynUser, value);
    }   

    if (status)
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
              "%s:writeFloat64 error, status=%d function=%d, value=%f\n",
              driverName, status, function, value);
    else{
        /* Do callbacks so higher layers see any changes */
        callParamCallbacks();
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
              "%s:writeFloat64: function=%d, value=%f\n",
              driverName, function, value);
    }

    return((asynStatus)status);
}


/*
asynStatus pimegaDetector::readFloat64(asynUser *pasynUser, epicsFloat64 *value)
{

    int function = pasynUser->reason;
    int status=0;
    static const char *functionName = "readFloat64";
    
    if (function == PimegaActualTemp) {
        US_TemperatureActual(pimega);
        *value = pimega->cached_result.actual_temperature;
    }
    //Other functions we call the base class method
    else {
        status = asynPortDriver::readFloat64(pasynUser, value);
    }
    callParamCallbacks();
    return (status==0) ? asynSuccess : asynError;      
}
*/

/** Configuration command for Pimega driver; creates a new Pimega object.
  * \param[in] portName The name of the asyn port driver to be created.
  * \param[in] CommandPort The asyn network port connection to the Pimega
  * \param[in] maxBuffers The maximum number of NDArray buffers that the NDArrayPool for this driver is 
  *            allowed to allocate. Set this to -1 to allow an unlimited number of buffers.
  * \param[in] maxMemory The maximum amount of memory that the NDArrayPool for this driver is 
  *            allowed to allocate. Set this to -1 to allow an unlimited amount of memory.
  * \param[in] priority The thread priority for the asyn port driver thread if ASYN_CANBLOCK is set in asynFlags.
  * \param[in] stackSize The stack size for the asyn port driver thread if ASYN_CANBLOCK is set in asynFlags.
  */
extern "C" int pimegaDetectorConfig(const char *portName,
        const char *address, int port, int maxSizeX, int maxSizeY, int detectorModel, int maxBuffers,
        size_t maxMemory, int priority, int stackSize)
{
    new pimegaDetector(portName, address, port, maxSizeX,
            maxSizeY, detectorModel, maxBuffers, maxMemory, priority, stackSize);
    return (asynSuccess);
}

/** Constructor for pimega driver; most parameters are simply passed to ADDriver::ADDriver.
 * After calling the base class constructor this method creates a thread to collect the detector data,
 * and sets reasonable default values for the parameters defined in this class, asynNDArrayDriver, and ADDriver.
 * \param[in] portName The name of the asyn port driver to be created.
 * \param[in] CommandIP The asyn network port connection to the Pimega
 * \param[in] maxSizeX The size of the pimega detector in the X direction.
 * \param[in] maxSizeY The size of the pimega detector in the Y direction.
 * \param[in] portName The name of the asyn port driver to be created.
 * \param[in] maxBuffers The maximum number of NDArray buffers that the NDArrayPool for this driver is
 *            allowed to allocate. Set this to -1 to allow an unlimited number of buffers.
 * \param[in] maxMemory The maximum amount of memory that the NDArrayPool for this driver is
 *            allowed to allocate. Set this to -1 to allow an unlimited amount of memory.
 * \param[in] priority The thread priority for the asyn port driver thread if ASYN_CANBLOCK is set in asynFlags.
 * \param[in] stackSize The stack size for the asyn port driver thread if ASYN_CANBLOCK is set in asynFlags.
 */
pimegaDetector::pimegaDetector(const char *portName,
        const char *address, int port, int maxSizeX, int maxSizeY, int detectorModel, int maxBuffers,
        size_t maxMemory, int priority, int stackSize)


       : ADDriver(portName, 1, 0, maxBuffers, maxMemory,
                asynInt32ArrayMask | asynFloat64ArrayMask
                        | asynGenericPointerMask | asynInt16ArrayMask,
                asynInt32ArrayMask | asynFloat64ArrayMask
                        | asynGenericPointerMask | asynInt16ArrayMask,
                ASYN_CANBLOCK, 1, /* ASYN_CANBLOCK=1, ASYN_MULTIDEVICE=0, autoConnect=1 */
                priority, stackSize),

                pollTime_(DEFAULT_POLL_TIME),
                forceCallback_(1)

{
    int status = asynSuccess;
    const char *functionName = "pimegaDetector";

    /* Create the epicsEvents for signaling to the simulate task when acquisition starts and stops */
    startEventId_ = epicsEventCreate(epicsEventEmpty);
    if (!startEventId_) {
        printf("%s:%s epicsEventCreate failure for start event\n",
            driverName, functionName);
        return;
    }
    stopEventId_ = epicsEventCreate(epicsEventEmpty);
    if (!stopEventId_) {
        printf("%s:%s epicsEventCreate failure for stop event\n",
            driverName, functionName);
        return;
    }
    
    detModel = (pimega_detector_model_t) detectorModel;
    pimega = pimega_new(detModel);
    connect(address, port);

    //pimega->debug_out = fopen("log.txt", "w+");
    //report(pimega->debug_out, 1);
    //fflush(pimega->debug_out);

    createParameters();
    setDefaults();
    
    // send image pattern to test
    //Send_Image(pimega, 3);
    
    //reset(1);    

    
    /*
    epicsThreadCreate("pimega_test_poller",epicsThreadPriorityMedium,
                        epicsThreadGetStackSize(epicsThreadStackMedium),
                        (EPICSTHREADFUNC)pollerThreadC,
                        this); 

    /* Create the thread that runs acquisition */
    status = (epicsThreadCreate("pimegaDetTask", epicsThreadPriorityMedium,
            epicsThreadGetStackSize(epicsThreadStackMedium),
            (EPICSTHREADFUNC) acquisitionTaskC, this) == NULL); 
    if (status)
    {
        printf("%s:%s epicsThreadCreate failure for acquisition task\n", driverName,
                functionName);
        return;
    }

}

/* Code for iocsh registration */
static const iocshArg pimegaDetectorConfigArg0 = { "Port name", iocshArgString };
static const iocshArg pimegaDetectorConfigArg1 = { "pimega address", iocshArgString };
static const iocshArg pimegaDetectorConfigArg2 = { "pimega port", iocshArgInt };
static const iocshArg pimegaDetectorConfigArg3 = { "maxSizeX", iocshArgInt };
static const iocshArg pimegaDetectorConfigArg4 = { "maxSizeY", iocshArgInt };
static const iocshArg pimegaDetectorConfigArg5 = { "detectorModel", iocshArgInt};
static const iocshArg pimegaDetectorConfigArg6 = { "maxBuffers", iocshArgInt };
static const iocshArg pimegaDetectorConfigArg7 = { "maxMemory", iocshArgInt };
static const iocshArg pimegaDetectorConfigArg8 = { "priority", iocshArgInt };
static const iocshArg pimegaDetectorConfigArg9 = { "stackSize", iocshArgInt };
static const iocshArg * const pimegaDetectorConfigArgs[] =  {&pimegaDetectorConfigArg0,
                                                          &pimegaDetectorConfigArg1,
                                                          &pimegaDetectorConfigArg2,
                                                          &pimegaDetectorConfigArg3,
                                                          &pimegaDetectorConfigArg4,
                                                          &pimegaDetectorConfigArg5,
                                                          &pimegaDetectorConfigArg6,
                                                          &pimegaDetectorConfigArg7,
                                                          &pimegaDetectorConfigArg8,
                                                          &pimegaDetectorConfigArg9};
static const iocshFuncDef configpimegaDetector =
{ "pimegaDetectorConfig", 9, pimegaDetectorConfigArgs };

static void configpimegaDetectorCallFunc(const iocshArgBuf *args)
{
    pimegaDetectorConfig(args[0].sval, args[1].sval, args[2].ival, args[3].ival, 
                        args[4].ival, args[5].ival, args[6].ival, args[7].ival, args[8].ival, args[9].ival);
}

static void pimegaDetectorRegister(void)
{

    iocshRegister(&configpimegaDetector, configpimegaDetectorCallFunc);
}

extern "C"
{
epicsExportRegistrar(pimegaDetectorRegister);
}



/************************************************************************************
 * Validation functions
 ***********************************************************************************/
static bool validate_bool(int v)
{
    if (v == 0 || v == 1) return true;
    return false;
}

static bool validate_counter_depth(int v)
{
    return (v >= 0 && v < PIMEGA_COUNTERDEPTH_ENUM_END);
}

static bool validate_gain_mode(int v)
{
    return (v >= 0 && v < PIMEGA_GAIN_MODE_ENUM_END);
}

static bool validate_operation_mode(int v)
{
    return (v >= 0 && v < PIMEGA_OPERATION_MODE_ENUM_END);
}

/************************************************************************************/

void pimegaDetector::panic(const char *msg)
{
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s\n", msg);
    epicsExit(0);
}

void pimegaDetector::connect(const char *address, unsigned short port)
{
    unsigned i;
    int rc;

    for (i = 0; i < 5; i++) {
        rc = pimega_connect(pimega, address, port);
        if (rc == PIMEGA_SUCCESS) return;
        epicsThreadSleep(1);
    }
    panic("Unable to connect. Aborting...");
}

void pimegaDetector::setParameter(int index, const char *value)
{
    asynStatus status;

    status = setStringParam(index, value);
    if (status != asynSuccess)
        panic("setParameter failed.");
}

void pimegaDetector::setParameter(int index, int value)
{
    asynStatus status;

    status = setIntegerParam(index, value);
    if (status != asynSuccess)
        panic("setParameter failed.");
}

void pimegaDetector::setParameter(int index, double value)
{
    asynStatus status;

    status = setDoubleParam(index, value);
    if (status != asynSuccess)
        panic("setParameter failed.");
}

void pimegaDetector::getParameter(int index, int maxChars, char *value)
{
    asynStatus status;

    status = getStringParam(index, maxChars, value);
    if (status != asynSuccess)
        panic("getStringParam failed.");
}

void pimegaDetector::getParameter(int index, int *value)
{
    asynStatus status;

    status = getIntegerParam(index, value);
    if (status != asynSuccess)
        panic("getIntegerParam failed.");
}

void pimegaDetector::getParameter(int index, double *value)
{
    asynStatus status;
status = getDoubleParam(index, value);

    if (status != asynSuccess)
        panic("getDoubleParam failed.");
}

void pimegaDetector::createParameters(void)
{
    createParam(pimegaefuseIDString,        asynParamFloat64,   &PimegaefuseID);
    createParam(pimegaOmrOPModeString,      asynParamInt32,     &PimegaOmrOPMode);
    createParam(pimegaMedipixBoardString,   asynParamInt32,     &PimegaMedipixBoard);
    createParam(pimegaMedipixChipString,    asynParamInt32,     &PimegaMedipixChip);
    createParam(pimegaPixeModeString,       asynParamInt32,     &PimegaPixelMode);
    createParam(pimegaContinuosRWString,    asynParamInt32,     &PimegaContinuosRW);
    createParam(pimegaPolarityString,       asynParamInt32,     &PimegaPolarity);
    createParam(pimegaDiscriminatorString,  asynParamInt32,     &PimegaDiscriminator);
    createParam(pimegaTestPulseString,      asynParamInt32,     &PimegaTestPulse);
    createParam(pimegaCounterDepthString,   asynParamInt32,     &PimegaCounterDepth);
    createParam(pimegaEqualizationString,   asynParamInt32,     &PimegaEqualization);
    createParam(pimegaGainString,           asynParamInt32,     &PimegaGain);
    createParam(pimegaResetString,          asynParamInt32,     &PimegaReset);
    createParam(pimegaThreshold0String,     asynParamInt32,     &PimegaThreshold0);
    createParam(pimegaThreshold1String,     asynParamInt32,     &PimegaThreshold1);
    createParam(pimegaDacPreampString,      asynParamInt32,     &PimegaPreamp);
    createParam(pimegaDacIKrumString,       asynParamInt32,     &PimegaIkrum);
    createParam(pimegaDacShaperString,      asynParamInt32,     &PimegaShaper);
    createParam(pimegaDacDiscString,        asynParamInt32,     &PimegaDisc);
    createParam(pimegaDacDiscLSString,      asynParamInt32,     &PimegaDiscLS);
    createParam(pimegaDacDiscLString,       asynParamInt32,     &PimegaDiscL);
    createParam(pimegaDacDelayString,       asynParamInt32,     &PimegaDelay);
    createParam(pimegaDacTPBufferInString,  asynParamInt32,     &PimegaTpBufferIn);
    createParam(pimegaDacTPBufferOutString, asynParamInt32,     &PimegaTpBufferOut);
    createParam(pimegaDacRpzString,         asynParamInt32,     &PimegaRpz);
    createParam(pimegaDacGndString,         asynParamInt32,     &PimegaGnd);
    createParam(pimegaDacTPRefString,       asynParamInt32,     &PimegaTpRef);
    createParam(pimegaDacFbkString,         asynParamInt32,     &PimegaFbk);
    createParam(pimegaDacCasString,         asynParamInt32,     &PimegaCas);
    createParam(pimegaDacTPRefAString,      asynParamInt32,     &PimegaTpRefA);
    createParam(pimegaDacTPRefBString,      asynParamInt32,     &PimegaTpRefB);
    createParam(pimegaDacDiscHString,       asynParamInt32,     &PimegaDiscH);

    /* Do callbacks so higher layers see any changes */
    callParamCallbacks();

}

void pimegaDetector::setDefaults(void)
{
    setParameter(NDArrayCallbacks, 0);
    setParameter(NDDataType, NDUInt32);
    setParameter(NDColorMode, NDColorModeMono);
    setParameter(NDBayerPattern, NDBayerRGGB);
    setParameter(NDNDimensions, 0);
    setParameter(NDArraySizeX, 0);
    setParameter(NDArraySizeY, 0);
    setParameter(NDArraySizeZ, 0);
    setParameter(NDArraySize, 0);
    setParameter(NDArrayCallbacks, 0);
    setParameter(NDArrayCounter, 0);
    setParameter(NDPoolMaxMemory, 0.1);
    setParameter(NDPoolUsedMemory, 0.0);
    setParameter(NDPoolMaxBuffers, 1);
    setParameter(NDPoolAllocBuffers, 0);
    setParameter(NDPoolFreeBuffers, 0);
    setParameter(NDFilePathExists, 0);
    setParameter(NDFileNumber, 0);
    setParameter(NDAutoIncrement, 1);
    setParameter(NDAutoSave, 1);
    setParameter(NDFileFormat, 0);
    setParameter(NDWriteFile, 0);
    setParameter(NDReadFile, 0);
    setParameter(NDFileWriteMode, NDFileModeSingle);
    setParameter(NDFileWriteStatus, 0);
    setParameter(NDFileCapture, 0);
    setParameter(NDFileNumCapture, 0);
    setParameter(NDFileNumCaptured, 0);
    setParameter(NDFileDeleteDriverFile, 0);
    setParameter(ADTemperature, 30.0);
    setParameter(ADTemperatureActual, 30.0);
    setParameter(ADBinX, 1);
    setParameter(ADBinY, 1);
    setParameter(ADMinX, 0);
    setParameter(ADMinY, 0);
    setParameter(ADReverseX, 0);
    setParameter(ADReverseY, 0);
    setParameter(ADTriggerMode, PIMEGA_TRIGGER_MODE_INTERNAL);
    setParameter(ADFrameType, ADFrameNormal);
    setParameter(ADNumExposures, 1);
    /* String parameters are initialized in st.cmd file because of a bug in asyn
     * that makes string records behave differently from integer records.  */
    setParameter(NDAttributesFile, "");
    setParameter(NDFilePath, "");
    setParameter(NDFileName, "");
    setParameter(NDFileTemplate, "");
    setParameter(NDFullFileName, "");
    setParameter(NDFileWriteMessage, "");
}

void pimegaDetector::prepareScan(unsigned board)
{
    medipixBoard(board);
    //imgChipID(pimega, 0);          // all chips
    //US_AcquireTime(pimega, acquireTime);    // set acquire time
    US_NumExposures(pimega, 1);             // set number of exposures
    US_Acquire(pimega, 1);
}

void pimegaDetector::report(FILE *fp, int details)
{

    fprintf(fp, " Pimega detector: %s\n", this->portName);
    
    if (details > 0) {
        int  dataType;
        getIntegerParam(NDDataType, &dataType);
        fprintf(fp, "  Data type:         %d\n", dataType);
    }

    ADDriver::report(fp, details);
}

asynStatus pimegaDetector::triggerMode(int trigger)
{
    int rc;
    rc = US_TriggerMode(pimega, (pimega_trigger_mode_t)trigger);
    if (rc != PIMEGA_SUCCESS) {
        error("TriggerMode out the range: %s\n", pimega_error_string(rc));
        return asynError;
    }
}

asynStatus pimegaDetector::setDACValue(pimega_dac_t dac, int value, int parameter)
{
    int rc;

    rc = US_Set_DAC_Variable(pimega, dac, (unsigned)value);
    if (rc != PIMEGA_SUCCESS) {
        error("Unable to change DAC value: %s\n", pimega_error_string(rc));
        return asynError;
    }
    setParameter(parameter, value);

    return asynSuccess;
}

asynStatus pimegaDetector::reset(short action)
{
    int rc;
    if (action < 0 || action > 1) {
        error("Invalid boolean value: %d\n", action);
        return asynError;
    }

    rc = US_Reset(pimega, action);
    if (rc != PIMEGA_SUCCESS) {
        return asynError; }

    return asynSuccess;
}

asynStatus  pimegaDetector::medipixBoard(uint8_t board_id)
{
    int rc;

    rc = Select_Board(pimega, board_id);
    if (rc != PIMEGA_SUCCESS) {
        error("Invalid number of boards: %s\n", pimega_error_string(rc));
        return asynError;
    }

    setParameter(PimegaMedipixBoard, board_id);
    return asynSuccess;


}

asynStatus pimegaDetector::imgChipID(uint8_t chip_id)
{
    int rc;
    int OmrOp;
    epicsFloat64 _efuseID;

    rc = US_ImgChipNumberID(pimega, chip_id);
    if (rc != PIMEGA_SUCCESS) {
        error("Invalid number of medipix chip ID: %s\n", pimega_error_string(rc));
        return asynError;
    }
    setParameter(PimegaMedipixChip, chip_id);

    /*Get OMR operation mode */
    getParameter(PimegaOmrOPMode,&OmrOp);

    /* Get e-fuseID from selected chip_id */
    rc = US_efuseID_RBV(pimega);
    if (rc != PIMEGA_SUCCESS) return asynError;
    _efuseID = pimega->cached_result.efuseID;
    setParameter(PimegaefuseID, _efuseID);

    return asynSuccess;

}

asynStatus pimegaDetector::numExposures(unsigned number)
{
    int rc;

    rc = US_NumExposures(pimega, number);
    if (rc != PIMEGA_SUCCESS){
        error("Invalid number of exposures: %s\n", pimega_error_string(rc));
        return asynError;
    }
    setParameter(ADNumExposures, (int)number);
    return asynSuccess;
}

asynStatus pimegaDetector::omrOpMode(int mode)
{
    int rc;
    
    if(!validate_operation_mode(mode)){
        error("Invalid OMR operation mode value: %d\n", mode); return asynError; 
    }
    
    rc = US_OmrOMSelec(pimega, (pimega_operation_mode_t)mode);
    if (rc != PIMEGA_SUCCESS){ return asynError;
    }

    setParameter(PimegaOmrOPMode, mode);
    return asynSuccess;   
}

asynStatus pimegaDetector::pixelMode(int mode)
{
    int rc;

    if(!validate_bool(mode)){
        error("Invalid boolean value: %d\n", mode); return asynError; 
    }
    
    rc = US_PixelMode(pimega, (pimega_pixel_mode_t)mode);
    if (rc != PIMEGA_SUCCESS){ return asynError;
    }

    setParameter(PimegaPixelMode, mode);
    return asynSuccess;
}

asynStatus pimegaDetector::continuosRW(int mode)
{
    int rc;

    if(!validate_bool(mode)){
        error("Invalid boolean value: %d\n", mode); return asynError; 
    }
    
    rc = US_ContinuousRW(pimega, (pimega_crw_srw_t)mode);
    if (rc != PIMEGA_SUCCESS){ return asynError;
    }

    setParameter(PimegaContinuosRW, mode);
    return asynSuccess;
}

asynStatus pimegaDetector::polarity(int mode)
{
    int rc;

    if(!validate_bool(mode)){
        error("Invalid boolean value: %d\n", mode); return asynError; 
    }
    
    rc = US_Polarity(pimega, (pimega_polarity_t)mode);
    if (rc != PIMEGA_SUCCESS){ return asynError;
    }

    setParameter(PimegaPolarity, mode);
    return asynSuccess;
}

asynStatus pimegaDetector::discriminator(int mode)
{
    int rc;

    if(!validate_bool(mode)){
        error("Invalid boolean value: %d\n", mode); return asynError; 
    }
    
    rc = US_Discriminator(pimega, (pimega_discriminator_t)mode);
    if (rc != PIMEGA_SUCCESS){ return asynError;
    }

    setParameter(PimegaDiscriminator, mode);
    return asynSuccess;
}

asynStatus pimegaDetector::enableTP(int mode)
{
    int rc;

    if(!validate_bool(mode)){
        error("Invalid boolean value: %d\n", mode); return asynError; 
    }
    
    rc = US_TestPulse(pimega, mode);
    if (rc != PIMEGA_SUCCESS){ return asynError;
    }

    setParameter(PimegaTestPulse, mode);
    return asynSuccess;   
}

asynStatus pimegaDetector::counterDepth(int mode)
{
    int rc;
    
    printf("Valor de mode do counterDepth: %d\n", mode);

    if(!validate_counter_depth(mode)){
        error("Invalid counterDepth value: %d\n", mode); return asynError; 
    }
    
    rc = US_CounterDepth(pimega, (pimega_counterDepth_t)mode);
    if (rc != PIMEGA_SUCCESS){ return asynError;
    }

    setParameter(PimegaCounterDepth, mode);
    return asynSuccess;   
}

asynStatus pimegaDetector::equalization(int mode)
{
    int rc;

    if(!validate_bool(mode)){
        error("Invalid boolean value: %d\n", mode); return asynError; 
    }
    
    rc = US_Equalization(pimega, mode);
    if (rc != PIMEGA_SUCCESS){ return asynError;
    }

    setParameter(PimegaEqualization, mode);
    return asynSuccess;   
}

asynStatus pimegaDetector::gainMode(int mode)
{
    int rc;
    
    if(!validate_gain_mode(mode)){
        error("Invalid gain mode value: %d\n", mode); return asynError; 
    }
    
    rc = US_Gain(pimega, (pimega_gain_mode_t)mode);
    if (rc != PIMEGA_SUCCESS){ return asynError;
    }

    setParameter(PimegaGain, mode);
    return asynSuccess;   
}

asynStatus pimegaDetector::acqTime(float acquire_time_s)
{
    int rc;
    rc = US_AcquireTime(pimega, acquire_time_s);
    if (rc != PIMEGA_SUCCESS){
        error("Invalid acquire time: %s\n", pimega_error_string(rc));
        return asynError;
    }
    setParameter(ADAcquireTime, acquire_time_s);
    return asynSuccess;
}