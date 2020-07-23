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

#include "pimegaDetector.h"

static void acquisitionTaskC(void *drvPvt)
{
    pimegaDetector *pPvt = (pimegaDetector *) drvPvt;
    pPvt->acqTask();
}

void pimegaDetector::generateImage(void)
{
    const char *functionName = "generateImage";

    // simulate a image
    for(int i=0; i <= p_imageSize ;i++) {
        pimega_image[i] = rand()%UINT16_MAX;
        }
    //*****************************************************

    getIntegerParam(NDArrayCallbacks, &arrayCallbacks);
    // Get an image buffer from the pool
    getIntegerParam(ADMaxSizeX, &itemp); dims[0] = itemp;
    getIntegerParam(ADMaxSizeY, &itemp); dims[1] = itemp;

    this->pArrays[0] = this->pNDArrayPool->alloc(2, dims, NDUInt32, p_imageSize * sizeof(uint32_t), pimega_image);

    setIntegerParam(NDArraySizeX, dims[0]);
    setIntegerParam(NDArraySizeY, dims[1]);

    if (arrayCallbacks) {
        // Call the NDArray callback
        asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
                "%s:%s: calling imageData callback\n", driverName, functionName);
        doCallbacksGenericPointer(this->pArrays[0], NDArrayData, 0);
        this->pArrays[0]->release();
    }
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
    int numImages;
    int imageMode, numImagesCounter;
    int acquire=0;
    NDArray *pImage;
    double acquireTime, acquirePeriod;
    int acquireStatus = 0;
    bool bufferOverflow=0;
    int newImage=0;
    epicsTimeStamp startTime, endTime;

    const char *functionName = "pimegaDetector::acqTask";

    this->lock();
    /* Loop forever */
    while (1) {

        if (!acquire)  {
            // Release the lock while we wait for an event that says acquire has started, then lock again
            asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
                "%s:%s: waiting for acquire to start\n", driverName, functionName);
            this->unlock();
            status = epicsEventWait(startEventId_);
            this->lock();

            setStringParam(ADStatusMessage, "Acquiring data");
            //setIntegerParam(ADNumImagesCounter, 0); 

            /* We are acquiring. */
            /* Get the current time */
            epicsTimeGetCurrent(&startTime);

            getIntegerParam(ADImageMode, &imageMode);
            /* Get the exposure parameters */
            getDoubleParam(ADAcquireTime, &acquireTime);
            getDoubleParam(ADAcquirePeriod, &acquirePeriod);

            getIntegerParam(ADNumImages, &numImages);
            getIntegerParam(ADNumImagesCounter, &numImagesCounter);

            /* Open the shutter */
            setShutter(ADShutterOpen);
            setStringParam(ADStatusMessage, "Acquiring data");
            setIntegerParam(ADStatus, ADStatusAcquire); 

            callParamCallbacks();
            bufferOverflow =0;

            startAcquire();
            acquire = 1;

        }

        if (acquire) {
            // Read detector state
            acquireStatus = status_acquire(pimega);
            numImagesCounter = pimega->acquireParam.numExposuresCounter;
            if (newImage != numImagesCounter)
                {
                    generateImage();
                    newImage = numImagesCounter;
                }          
        }

        this->unlock();
        eventStatus = epicsEventWaitWithTimeout(this->stopEventId_, 0);
        this->lock();

        if (eventStatus == epicsEventWaitOK) {
            US_Acquire(pimega,0);
            //send_stopAcquire_toBackend(pimega);
            setShutter(0);
            setIntegerParam(ADAcquire, 0);
            acquire=0;

            setIntegerParam(ADNumImagesCounter, numImagesCounter);
            if (bufferOverflow) setStringParam(ADStatusMessage,
                    "Acquisition aborted by buffer overflow");

            if (imageMode == ADImageContinuous) {
                setIntegerParam(ADStatus, ADStatusIdle);
                setStringParam(ADStatusMessage, "Acquisition finished");
            }
            else {
                setIntegerParam(ADStatus, ADStatusAborted);
                setStringParam(ADStatusMessage, "Acquisition aborted by user");
            }
            callParamCallbacks();
        }

        if (acquireStatus == DONE_ACQ && acquire) {
            generateImage();
            if (imageMode == ADImageSingle) {
                acquire=0;
                newImage = 0;
                setIntegerParam(ADAcquire, 0);
                setIntegerParam(ADStatus, ADStatusIdle);
                setStringParam(ADStatusMessage, "Acquisition finished");
            }

            else if (imageMode == ADImageMultiple) {
                acquire=0;
                setIntegerParam(ADAcquire, 0);
                setIntegerParam(ADStatus, ADStatusIdle);
                setStringParam(ADStatusMessage, "Acquisition finished");
                setIntegerParam(ADNumImagesCounter, numImagesCounter+1);
                newImage = 0;
            }

            else if (imageMode == ADImageContinuous) {
                acquire=0;
                newImage = 0;
            }
        }
        /* Call the callbacks to update any changes */
        callParamCallbacks();
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

    /* Ensure that ADStatus is set correctly before we set ADAcquire.*/
    getIntegerParam(ADStatus, &adstatus);

    /* Set the parameter and readback in the parameter library.  This may be overwritten when we read back the
     * status at the end, but that's OK */
    status |= setIntegerParam(function, value);

    if (function == ADAcquire) {
        if (value && (adstatus == ADStatusIdle || adstatus == ADStatusError || adstatus == ADStatusAborted)) {
            /* Send an event to wake up the acq task.  */
            epicsEventSignal(this->startEventId_);
        }
        if (!value && (adstatus == ADStatusAcquire)) {
          /* This was a command to stop acquisition */
            epicsEventSignal(this->stopEventId_);
            epicsThreadSleep(.1);
        }
    }

    else if (function == ADImageMode)
        status |= imageMode(value);
    else if (function == PimegaOmrOPMode)
        status |= omrOpMode(value);
    else if (function == ADNumExposures)
        status |=  numExposures(value);
    else if (function == PimegaReset)
        status |=  reset(value);
    else if (function == PimegaMedipixMode)
        status |= medipixMode(value);
    else if (function == PimegaModule)
        status |= selectModule(value);
    //else if (function == ADTriggerMode)
    //    status |=  triggerMode(value);
    //else if (function == PimegaMedipixBoard)
    //    status |= medipixBoard(value);
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
    else if (function == PimegaReadCounter)
        status |= readCounter(value);
    else if (function == PimegaSenseDacSel)
        status |= senseDacSel(value);

    //DACS functions
    else if (function == PimegaCas)
        status |= setDACValue(DAC_CAS, value, function);
    else if (function == PimegaDelay)
        status |=  setDACValue(DAC_Delay, value, function);
    else if (function == PimegaDisc)
        status |=  setDACValue(DAC_Disc, value, function);
    else if (function == PimegaDiscH)
        status |=  setDACValue(DAC_DiscH, value, function);
    else if (function == PimegaDiscL)
        status |=  setDACValue(DAC_DiscL, value, function);
    else if (function == PimegaDiscLS)
        status |=  setDACValue(DAC_DiscLS, value, function);
    else if (function == PimegaFbk)
        status |=  setDACValue(DAC_FBK, value, function);
    else if (function == PimegaGnd)
        status |=  setDACValue(DAC_GND, value, function);
    else if (function == PimegaIkrum)
        status |=  setDACValue(DAC_IKrum, value, function);
    else if (function == PimegaPreamp)
        status |=  setDACValue(DAC_Preamp, value, function);
    else if (function == PimegaRpz)
        status |=  setDACValue(DAC_RPZ, value, function);
    else if (function == PimegaShaper)
        status |=  setDACValue(DAC_Shaper, value, function);
    else if (function == PimegaThreshold0)
        status |=  setDACValue(DAC_ThresholdEnergy0, value, function);
    else if (function == PimegaThreshold1)
        status |=  setDACValue(DAC_ThresholdEnergy1, value, function);
    else if (function == PimegaTpBufferIn)
        status |=  setDACValue(DAC_TPBufferIn, value, function);
    else if (function == PimegaTpBufferOut)
        status |=  setDACValue(DAC_TPBufferOut, value, function);
    else if (function == PimegaTpRef)
        status |=  setDACValue(DAC_TPRef, value, function);
    else if (function == PimegaTpRefA)
        status |=  setDACValue(DAC_TPRefA, value, function);
    else if (function == PimegaTpRefB)
        status |=  setDACValue(DAC_TPRefB, value, function);
    else
    {
        if (function < FIRST_PIMEGA_PARAM)
                status = ADDriver::writeInt32(pasynUser, value);
    }

    if (status){
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
              "%s:%s: error, status=%d function=%d, value=%d\n",
              driverName, functionName, status, function, value);}
    else {
         /* Update any changed parameters */
        callParamCallbacks();
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
              "%s:%s: function=%d, value=%d\n",
              driverName, functionName, function, value);
    }
    return (asynStatus)status;

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

    status |= setDoubleParam(function, value);

    if (function == ADAcquireTime)
        status |= acqTime(value);

    else if (function == PimegaSensorBias)
        status |= sensorBias(value);

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

asynStatus pimegaDetector::readFloat32Array(asynUser *pasynUser, epicsFloat32 *value, size_t nElements, size_t *nIn)
{
    int function = pasynUser->reason;
    int addr;
    int numPoints = N_DACS_OUTS;
    epicsFloat32 *inPtr;
    const char *paramName;
    static const char *functionName = "pimegaDetector::readFloat32Array";

    this->getAddress(pasynUser, &addr);
 
    if(function == PimegaDacsOutSense)
    {
        inPtr = PimegaDacsOutSense_;
    }
    else {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
            "%s:%s: ERROR: unknown function=%d\n",
            driverName, functionName, function);
        return asynError;
    }

    *nIn = nElements;
    if (*nIn > (size_t) numPoints) *nIn = (size_t) numPoints;
        memcpy(value, inPtr, *nIn*sizeof(epicsFloat32)); 

    return asynSuccess;
}

asynStatus pimegaDetector::readFloat64(asynUser *pasynUser, epicsFloat64 *value)
{

    int function = pasynUser->reason;
    int status=0;
    static const char *functionName = "readFloat64";
    int scanStatus;

    getParameter(ADStatus,&scanStatus);

    //if (function == ADTemperatureActual) {
    //    status = US_TemperatureActual(pimega);
    //    setParameter(ADTemperatureActual, pimega->cached_result.actual_temperature);
    //}

    if ((function == ADTimeRemaining) && (scanStatus == ADStatusAcquire)) {
        //status = US_TimeRemaining_RBV(pimega);
        //status = US_TemperatureActual(pimega);
        *value = pimega->acquireParam.timeRemaining;
    }

    else if (function == PimegaSensorBias){
        status = US_SensorBias_RBV(pimega);
        *value = pimega->pimegaParam.bias_voltage;
        setParameter(PimegaSensorBias, *value);
    }

    else if (function == PimegaDacOutSense){
        status = US_ImgChipDACOUTSense_RBV(pimega);
        *value = pimega->pimegaParam.dacOutput;
    }

    //Other functions we call the base class method
    else {
        status = asynPortDriver::readFloat64(pasynUser, value);
    }
    return (status==0) ? asynSuccess : asynError;
}

asynStatus pimegaDetector::readInt32(asynUser *pasynUser, epicsInt32 *value)
{
    int function = pasynUser->reason;
    int status=0;
    static const char *functionName = "readInt32";
    int scanStatus;

    getParameter(ADStatus,&scanStatus);

    if ((function == PimegaBackBuffer) && (scanStatus == ADStatusAcquire)) {
        //*value = static_cast<int>(pimega->acq_status_return.bufferUsed);
    }

    if ((function == ADNumImagesCounter) && (scanStatus == ADStatusAcquire)) {
        //US_NumExposuresCounter_RBV(pimega);
        *value = pimega->acquireParam.numExposuresCounter;
    }

    //Other functions we call the base class method
    else {
        status = asynPortDriver::readInt32(pasynUser, value);
    }
    return (status==0) ? asynSuccess : asynError;

}

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
                                    const char *address_module01,
                                    const char *address_module02,
                                    const char *address_module03,
                                    const char *address_module04,
                                    int port, int maxSizeX, int maxSizeY,
                                    int detectorModel, int maxBuffers,
                                    size_t maxMemory, int priority, int stackSize)
{
    new pimegaDetector(portName,
                       address_module01,
                       address_module02,
                       address_module03,
                       address_module04,
                       port, maxSizeX, maxSizeY,
                       detectorModel, maxBuffers,
                       maxMemory, priority, stackSize);
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
                   const char *address_module01, const char *address_module02,
                   const char *address_module03, const char *address_module04,
                   int port, int maxSizeX, int maxSizeY,
                   int detectorModel, int maxBuffers, size_t maxMemory, int priority, int stackSize)

       : ADDriver(portName, 1, 0, maxBuffers, maxMemory,
                asynInt32ArrayMask | asynFloat64ArrayMask | asynFloat32ArrayMask
                    | asynGenericPointerMask | asynInt16ArrayMask,
                asynInt32ArrayMask | asynFloat64ArrayMask | asynFloat32ArrayMask
                    | asynGenericPointerMask | asynInt16ArrayMask,
                ASYN_CANBLOCK, 1, /* ASYN_CANBLOCK=1, ASYN_MULTIDEVICE=0, autoConnect=1 */
                priority, stackSize),

                pollTime_(DEFAULT_POLL_TIME),
                forceCallback_(1)

{
    int status = asynSuccess;
    const char *functionName = "pimegaDetector::pimegaDetector";
    const char* ips[] = {address_module01,
                         address_module02,
                         address_module03,
                         address_module04};

    pimega_image = (uint32_t*)malloc(p_imageSize * sizeof(uint32_t));
    // initialize random seed:
    srand (time(NULL));

    //Alocate memory for PimegaDacsOutSense_
    PimegaDacsOutSense_ = (epicsFloat32 *)calloc(N_DACS_OUTS, sizeof(epicsFloat32));

    // Initialise the debugger
    initDebugger(1);
    debugLevel("all",1);

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
    if (pimega) debug(functionName, "Pimega Object created!");

    connect(ips, port);
    //pimega->debug_out = fopen("log.txt", "w+");
    //report(pimega->debug_out, 1);
    //fflush(pimega->debug_out);

    createParameters();
    setDefaults();
    define_master_module(pimega, 1, false);

    /* Create the thread that runs acquisition */
    status = (epicsThreadCreate("pimegaDetTask", 
                                epicsThreadPriorityMedium,
                                epicsThreadGetStackSize(epicsThreadStackMedium),
                                (EPICSTHREADFUNC)acquisitionTaskC,
                                this) == NULL);
    if (status) {
        debug(functionName, "epicsTheadCreate failure for image task");
    }
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

void pimegaDetector::connect(const char *address[4], unsigned short port)
{
    unsigned i;
    int rc = 0;

    //Serial Test
    //rc = open_serialPort(pimega, "/dev/ttyUSB0");
    
    // Connect to backend
    pimega_connect_backend(pimega, "127.0.0.1", 5412);
    // Ethernet test
    for(int _module = 0; _module < 4; _module++) {
        if (strcmp(address[_module],"0")) {
            for (i = 0; i < 5; i++) {
                rc |= pimega_connect(pimega, _module, address[_module], port);
                if (rc == PIMEGA_SUCCESS) break;
                epicsThreadSleep(1);
            }
            if (rc != PIMEGA_SUCCESS)
                panic("Unable to connect. Aborting...");
        } 
    }
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
    createParam(pimegaMedipixModeString,    asynParamInt32,     &PimegaMedipixMode);
    createParam(pimegaModuleString,         asynParamInt32,     &PimegaModule);
    createParam(pimegaefuseIDString,        asynParamOctet,     &PimegaefuseID);
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
    createParam(pimegaDacBiasString,        asynParamInt32,     &PimegaDacBias);
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
    createParam(pimegaReadCounterString,    asynParamInt32,     &PimegaReadCounter);
    createParam(pimegaSenseDacSelString,    asynParamInt32,     &PimegaSenseDacSel);
    createParam(pimegaDacOutSenseString,    asynParamFloat64,   &PimegaDacOutSense);
    createParam(pimegaBackendBufferString,  asynParamInt32,     &PimegaBackBuffer);
    createParam(pimegaResetRDMABufferString,asynParamInt32,     &PimegaResetRDMABuffer);
    createParam(pimegaSensorBiasString,     asynParamFloat64,   &PimegaSensorBias);
    createParam(pimegaDacsOutSenseString,   asynParamFloat32Array, &PimegaDacsOutSense);

    /* Do callbacks so higher layers see any changes */
    callParamCallbacks();

}

void pimegaDetector::setDefaults(void)
{

    //TODO remove this variables after test
    int maxSizeX = 16;
    int maxSizeY = 16;

    setParameter(ADMaxSizeX, maxSizeX);
    setParameter(ADMaxSizeY, maxSizeY);
    setParameter(ADSizeX, maxSizeX);
    setParameter(ADSizeX, maxSizeX);
    setParameter(ADSizeY, maxSizeY);
    setParameter(NDArraySizeX, maxSizeX);
    setParameter(NDArraySizeY, maxSizeY);
    setParameter(NDArraySize, 0);
    setParameter(NDDataType,  NDUInt32);
    setParameter(NDArrayCallbacks, 1);
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
    setParameter(PimegaBackBuffer, 0);
    setParameter(ADImageMode, ADImageSingle);

    setParameter(PimegaMedipixChip, 1);
    setParameter(PimegaMedipixBoard, 2);

    //getDacsValues();
}

void pimegaDetector::getDacsValues(void)
{
    int chip_id;
    int mfb;
    getParameter(PimegaMedipixChip, &chip_id);
    getParameter(PimegaMedipixBoard, &mfb);

    printf("\n\nChip ID: %i\n\n", chip_id);

    get_dacs_values(pimega, chip_id);
    setParameter(PimegaThreshold0, (int)pimega->digital_dac_values[DAC_ThresholdEnergy0]);
    setParameter(PimegaThreshold1, (int)pimega->digital_dac_values[DAC_ThresholdEnergy1]);
    setParameter(PimegaPreamp, (int)pimega->digital_dac_values[DAC_Preamp]);
    setParameter(PimegaIkrum, (int)pimega->digital_dac_values[DAC_IKrum]);
    setParameter(PimegaShaper, (int)pimega->digital_dac_values[DAC_Shaper]);
    setParameter(PimegaDisc, (int)pimega->digital_dac_values[DAC_Disc]);
    setParameter(PimegaDiscLS, (int)pimega->digital_dac_values[DAC_DiscLS]);
    //setParameter(PimegaShaperTest, pimega->digital_dac_values[DAC_ShaperTest])
    setParameter(PimegaDiscL, (int)pimega->digital_dac_values[DAC_DiscL]);
    setParameter(PimegaDelay, (int)pimega->digital_dac_values[DAC_Delay]);
    setParameter(PimegaTpBufferIn, (int)pimega->digital_dac_values[DAC_TPBufferIn]);
    setParameter(PimegaTpBufferOut, (int)pimega->digital_dac_values[DAC_TPBufferOut]);
    setParameter(PimegaRpz, (int)pimega->digital_dac_values[DAC_RPZ]);
    setParameter(PimegaGnd, (int)pimega->digital_dac_values[DAC_GND]);
    setParameter(PimegaTpRef, (int)pimega->digital_dac_values[DAC_TPRef]);
    setParameter(PimegaFbk, (int)pimega->digital_dac_values[DAC_FBK]);
    setParameter(PimegaCas, (int)pimega->digital_dac_values[DAC_CAS]);
    setParameter(PimegaTpRefA, (int)pimega->digital_dac_values[DAC_TPRefA]);
    setParameter(PimegaTpRefB, (int)pimega->digital_dac_values[DAC_TPRefB]);
    //setParameter(PimegaShaperTest, pimega->digital_dac_values[DAC_Test]);
    setParameter(PimegaDiscH,(int)pimega->digital_dac_values[DAC_DiscH]);

    getDacsOutSense();
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

asynStatus pimegaDetector::startAcquire()
{
    int rc;
    int autoSave;
    int resetRDMA;
    char fullFileName[MAX_FILENAME_LEN];
    
    /* Create the full filename */
    createFileName(sizeof(fullFileName), fullFileName);
    rc = set_file_name_template(pimega, fullFileName);

    getParameter(NDAutoSave,&autoSave);
    getParameter(PimegaResetRDMABuffer, &resetRDMA);
    update_backend_acqArgs(pimega, false, autoSave, resetRDMA, 1, 5);
    execute_acquire(pimega);
}

asynStatus pimegaDetector::selectModule(uint8_t module)
{
    int rc;
    rc = select_module(pimega, module);
    if (rc != PIMEGA_SUCCESS) {
        error("Invalid module number: %s\n", pimega_error_string(rc));
        return asynError;
    }
    setParameter(PimegaModule, module);
    return asynSuccess;    
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

    rc = select_board(pimega, board_id);
    if (rc != PIMEGA_SUCCESS) {
        error("Invalid number of boards: %s\n", pimega_error_string(rc));
        return asynError;
    }

    setParameter(PimegaMedipixBoard, board_id);
    return asynSuccess;
}

asynStatus pimegaDetector::medipixMode(uint8_t mode)
{
    int rc;
    rc = set_medipix_mode(pimega, (pimega_medipix_mode_t)mode);
    if (rc != PIMEGA_SUCCESS) {
        error("Invalid Medipix Mode: %s\n", pimega_error_string(rc));
        return asynError;
    }
    setParameter(PimegaMedipixMode, mode);

    return asynSuccess;   
}

asynStatus pimegaDetector::imgChipID(uint8_t chip_id)
{
    int rc;
    int OmrOp;
    char *_efuseID;

    rc = select_chipNumber(pimega, chip_id);
    if (rc != PIMEGA_SUCCESS) {
        error("Invalid number of medipix chip ID: %s\n", pimega_error_string(rc));
        return asynError;
    }
    setParameter(PimegaMedipixChip, chip_id);
    setParameter(PimegaMedipixBoard, pimega->chip_pos.mfb);

    /* Get e-fuseID from selected chip_id */ 
    rc = US_efuseID_RBV(pimega);
    if (rc != PIMEGA_SUCCESS) return asynError;
    _efuseID = pimega->pimegaParam.efuseID;
    setParameter(PimegaefuseID, _efuseID);

    getDacsValues();
    return asynSuccess;   

}

asynStatus pimegaDetector::numExposures(unsigned number)
{
    int rc;

    rc = set_numberExposures(pimega, number);
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

    rc = set_acquireTime(pimega, acquire_time_s);
    if (rc != PIMEGA_SUCCESS){
        error("Invalid acquire time: %s\n", pimega_error_string(rc));
        return asynError;
    }
    setParameter(ADAcquireTime, acquire_time_s);
    return asynSuccess;
}

asynStatus pimegaDetector::sensorBias(float voltage)
{
    int rc;
    rc = set_sensorBias(pimega, voltage);
    if (rc != PIMEGA_SUCCESS) {
        error("Invalid voltage value: %s\n", pimega_error_string(rc));
        return asynError;
    }
    setParameter(PimegaSensorBias, voltage);
    return asynSuccess;
}

asynStatus pimegaDetector::readCounter(int counter)
{
    int rc;
    rc = US_ReadCounter(pimega, (pimega_read_counter_t)counter);
    if (rc != PIMEGA_SUCCESS){ return asynError;
    }

    setParameter(PimegaReadCounter, counter);
    return asynSuccess;
}

asynStatus pimegaDetector::senseDacSel(u_int8_t dac)
{
    int rc;
    rc = US_SenseDacSel(pimega, dac);
    if (rc != PIMEGA_SUCCESS){ return asynError;
    }

    setParameter(PimegaSenseDacSel, dac);
    return asynSuccess;
}

asynStatus pimegaDetector::getDacsOutSense(void)
{
    for (int i=0; i<N_DACS_OUTS; i++) {
        PimegaDacsOutSense_[i] = (epicsFloat32)(pimega->analog_dac_values[i+1]);
    }
    doCallbacksFloat32Array(PimegaDacsOutSense_, N_DACS_OUTS, PimegaDacsOutSense, 0);

    return asynSuccess;
}

asynStatus pimegaDetector::imageMode(u_int8_t mode)
{
    //int rc;
    //rc = US_ImageMode(pimega, (pimega_image_mode_t)mode);
    //if (rc != PIMEGA_SUCCESS) return asynError;
    
    setParameter(ADImageMode, mode);
    return asynSuccess;
}

asynStatus pimegaDetector::initDebugger(int initDebug)
{
  // Set all debugging levels to initialised value
  debugMap_["pimegaDetector::acqTask"]                  = initDebug;
  debugMap_["pimegaDetector::pimegaDetector"]           = initDebug;
  debugMap_["pimegaDetector::readEnum"]                 = initDebug;
  debugMap_["pimegaDetector::writeInt32"]               = initDebug;
  debugMap_["pimegaDetector::writeFloat64"]             = initDebug;
  return asynSuccess;
}

asynStatus pimegaDetector::debugLevel(const std::string& method, int onOff)
{
  if (method == "all"){
    debugMap_["pimegaDetector::acqTask"]                = onOff;
    debugMap_["pimegaDetector::pimegaDetector"]         = onOff;
    debugMap_["pimegaDetector::readEnum"]               = onOff;
    debugMap_["pimegaDetector::writeInt32"]             = onOff;
    debugMap_["pimegaDetector::writeFloat64"]           = onOff;
  } else {
    debugMap_[method] = onOff;
  }
  return asynSuccess;
}


asynStatus pimegaDetector::debug(const std::string& method, const std::string& msg)
{
  // First check for the debug entry in the debug map
  if (debugMap_.count(method) == 1){
    // Now check if debug is turned on
    if (debugMap_[method] == 1){
      // Print out the debug message
      std::cout << method << ": " << msg << std::endl;
    }
  }
  return asynSuccess;
}

asynStatus pimegaDetector::debug(const std::string& method, const std::string& msg, int value)
{
  // First check for the debug entry in the debug map
  if (debugMap_.count(method) == 1){
    // Now check if debug is turned on
    if (debugMap_[method] == 1){
      // Print out the debug message
      std::cout << method << ": " << msg << " [" << value << "]" << std::endl;
    }
  }
  return asynSuccess;
}

asynStatus pimegaDetector::debug(const std::string& method, const std::string& msg, double value)
{
  // First check for the debug entry in the debug map
  if (debugMap_.count(method) == 1){
    // Now check if debug is turned on
    if (debugMap_[method] == 1){
      // Print out the debug message
      std::cout << method << ": " << msg << " [" << value << "]" << std::endl;
    }
  }
  return asynSuccess;
}

asynStatus pimegaDetector::debug(const std::string& method, const std::string& msg, const std::string& value)
{
  // First check for the debug entry in the debug map
  if (debugMap_.count(method) == 1){
    // Now check if debug is turned on
    if (debugMap_[method] == 1){
      // Copy the string
      std::string val = value;
      // Trim the output
      val.erase(val.find_last_not_of("\n")+1);
      // Print out the debug message
      std::cout << method << ": " << msg << " [" << val << "]" << std::endl;
    }
  }
  return asynSuccess;
}


/* Code for iocsh registration */
static const iocshArg pimegaDetectorConfigArg0 = { "Port name", iocshArgString };
static const iocshArg pimegaDetectorConfigArg1 = { "pimega module 1 address", iocshArgString };
static const iocshArg pimegaDetectorConfigArg2 = { "pimega module 2 address", iocshArgString };
static const iocshArg pimegaDetectorConfigArg3 = { "pimega module 3 address", iocshArgString };
static const iocshArg pimegaDetectorConfigArg4 = { "pimega module 4 address", iocshArgString };
static const iocshArg pimegaDetectorConfigArg5 = { "pimega port", iocshArgInt };
static const iocshArg pimegaDetectorConfigArg6 = { "maxSizeX", iocshArgInt };
static const iocshArg pimegaDetectorConfigArg7 = { "maxSizeY", iocshArgInt };
static const iocshArg pimegaDetectorConfigArg8 = { "detectorModel", iocshArgInt};
static const iocshArg pimegaDetectorConfigArg9 = { "maxBuffers", iocshArgInt };
static const iocshArg pimegaDetectorConfigArg10 = { "maxMemory", iocshArgInt };
static const iocshArg pimegaDetectorConfigArg11 = { "priority", iocshArgInt };
static const iocshArg pimegaDetectorConfigArg12 = { "stackSize", iocshArgInt };
static const iocshArg * const pimegaDetectorConfigArgs[] =  {&pimegaDetectorConfigArg0,
                                                            &pimegaDetectorConfigArg1,
                                                            &pimegaDetectorConfigArg2,
                                                            &pimegaDetectorConfigArg3,
                                                            &pimegaDetectorConfigArg4,
                                                            &pimegaDetectorConfigArg5,
                                                            &pimegaDetectorConfigArg6,
                                                            &pimegaDetectorConfigArg7,
                                                            &pimegaDetectorConfigArg8,
                                                            &pimegaDetectorConfigArg9,
                                                            &pimegaDetectorConfigArg10,
                                                            &pimegaDetectorConfigArg11,
                                                            &pimegaDetectorConfigArg12};
static const iocshFuncDef configpimegaDetector =
{ "pimegaDetectorConfig", 12, pimegaDetectorConfigArgs };

static void configpimegaDetectorCallFunc(const iocshArgBuf *args)
{
    pimegaDetectorConfig(args[0].sval, args[1].sval, args[2].sval, args[3].sval, args[4].sval,
                        args[5].ival, args[6].ival, args[7].ival, args[8].ival, args[9].ival,
                        args[10].ival, args[11].ival, args[12].ival);
}

static void pimegaDetectorRegister(void)
{

    iocshRegister(&configpimegaDetector, configpimegaDetectorCallFunc);
}

extern "C"
{
epicsExportRegistrar(pimegaDetectorRegister);
}
