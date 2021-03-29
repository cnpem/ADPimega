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
    get_array_data(pimega);


    getIntegerParam(NDArrayCallbacks, &arrayCallbacks);
    // Get an image buffer from the pool
    getIntegerParam(ADMaxSizeX, &itemp); dims[0] = itemp;
    getIntegerParam(ADMaxSizeY, &itemp); dims[1] = itemp;

    if (this->pArrays[0]) this->pArrays[0]->release();
    this->pArrays[0] = this->pNDArrayPool->alloc(2, dims, NDUInt32, 0, 0);
    memcpy(this->pArrays[0]->pData, pimega->sample_frame, this->pArrays[0]->dataSize);

    setIntegerParam(NDArraySizeX, dims[0]);
    setIntegerParam(NDArraySizeY, dims[1]);

    if (arrayCallbacks) {
        // Call the NDArray callback
        asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
                "%s:%s: calling imageData callback\n", driverName, functionName);
        doCallbacksGenericPointer(this->pArrays[0], NDArrayData, 0);
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
    int numImages, numExposuresVar;
    int imageMode; 
    uint64_t numImagesCounter;
    int acquire=0, i;
    int autoSave;
    int triggerMode;
    //NDArray *pImage;
    double acquireTime, acquirePeriod, remainingTime, elapsedTime;
    int acquireStatus = 0;
    bool bufferOverflow=0;
    epicsTimeStamp startTime, endTime;
    int indexEnable, backendStatus;
    const char *functionName = "pimegaDetector::acqTask";

    this->lock();
    /* Loop forever */
    while (1) {
        /* No acquisition in place */
        if (!acquire)  {

            /* reset acquireStatus */
            acquireStatus = 0;
            numImagesCounter = 1;
            // Release the lock while we wait for an event that says acquire has started, then lock again
            asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
                "%s:%s: waiting for acquire to start\n", driverName, functionName);
            this->unlock();
            status = epicsEventWait(startEventId_);
            this->lock();
            asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
                "%s:%s: Acquire started\n", driverName, functionName);


            /* We are acquiring. */

            getIntegerParam(ADImageMode, &imageMode);
            /* Get the exposure parameters */
            getDoubleParam(ADAcquireTime, &acquireTime);
            getDoubleParam(ADAcquirePeriod, &acquirePeriod);

            getIntegerParam(ADNumExposures, &numExposuresVar);
            getIntegerParam(ADNumImages, &numImages);
            getIntegerParam(ADTriggerMode, &triggerMode);

            /* Open the shutter */
            setShutter(ADShutterOpen);
            setStringParam(ADStatusMessage, "Acquiring data");
            setIntegerParam(ADStatus, ADStatusAcquire); 

            callParamCallbacks();
            bufferOverflow =0;

            /* Backend status */
            getParameter(NDFileCapture,&backendStatus);

            /* if continous mode is chosen! */
            if (imageMode == ADImageContinuous || imageMode == ADImageSingle) {
                setParameter (ADNumExposures, 1);   
                numExposuresVar = 1;             
                numExposures(1);
            }
            
            status = startAcquire();
            if (status != asynSuccess) {
                err_print("startAcquire() failed. Stop event sent.\n");
                epicsEventSignal(this->stopEventId_);
                epicsThreadSleep(.1);
            }
            else {
                acquire = 1;
                /* Get the current time */
                epicsTimeGetCurrent(&startTime);
            }
        }


        /* Decoupled this from the next loop. Only needs to update acquireStatus
           when this condition is true (acquire && (acquireStatus != DONE_ACQ) */
        if (acquire && (acquireStatus != DONE_ACQ)) {
                acquireStatus = status_acquire(pimega);
        }

        /* will enter here when the detector did not finish acquisition (acquireStatus != DONE_ACQ)
           or when continous mode is selected (imageMode == ADImageContinuous)
           This loop has the function of updating the timer of the experiment. 
           Count up or down depending on whether continous or not */
        if (acquire && (acquireStatus != DONE_ACQ || imageMode == ADImageContinuous)) {
            
            epicsTimeGetCurrent(&endTime);
            elapsedTime = epicsTimeDiffInSeconds(&endTime, &startTime);
            if (acquirePeriod != 0) {
                remainingTime = (acquirePeriod * numExposuresVar) - elapsedTime;
            }
            else {
                remainingTime = (acquireTime * numExposuresVar)  - elapsedTime;
            }

            if (remainingTime < 0) remainingTime = 0;

            if (imageMode == ADImageContinuous || triggerMode != PIMEGA_TRIGGER_MODE_INTERNAL)
                setDoubleParam(ADTimeRemaining, elapsedTime);
            else
                setDoubleParam(ADTimeRemaining, remainingTime);

        }

        this->unlock();
        eventStatus = epicsEventWaitWithTimeout(this->stopEventId_, 0);
        this->lock();

        /* Stop event detected */
        if (eventStatus == epicsEventWaitOK) {
            v_print("epicsEventWaitWithTimeout returned epicsEventWaitOK (Stop signal detected).\n");
            //US_Acquire(pimega,0);
            stop_acquire(pimega);
            send_stopAcquire_toBackend(pimega);
            setShutter(0);
            setIntegerParam(ADAcquire, 0);
            acquire=0;

            /* TODO: This condition needs checking if needed. Always returns false. */
            if (bufferOverflow) setStringParam(ADStatusMessage,
                    "Acquisition aborted by buffer overflow");

            if (imageMode == ADImageContinuous) {
                setIntegerParam(ADStatus, ADStatusIdle);
                setStringParam(ADStatusMessage, "Acquisition finished");
                setParameter(ADStringFromServer, "Done"); //¯\_(⊙︿⊙)_/¯
            }
            else {
                setIntegerParam(ADStatus, ADStatusAborted);
                setStringParam(ADStatusMessage, "Acquisition aborted by user");
                setParameter(ADStringFromServer, "Backend stopped"); 
            }
            callParamCallbacks();
        }
        //printf("Index error = %d\n", pimega->acq_status_return.indexError);      
        /* Will enter here only one time when the acqusition time is over. The current configuration assumes that
          when time is up, the thread goes to sleep, but perhaps we should consider changing this to only after 
          when the frames are ready, acquire should become 0*/
        if (acquireStatus == DONE_ACQ && acquire) {

            /* Added this delay to guarantee that the scan of NDFileNumCaptured was performed at least once after acquireStatus turned DONE_ACQ */
            //usleep(200000);

            /* Identify if Module error occured or received frames in all, or some modules is 0 */
            bool moduleError = false;
            uint64_t minumumAcquisitionCount = UINT64_MAX;
            for (i = 0;  i < pimega->max_num_modules; i++)
            {
                moduleError |= pimega->acq_status_return.moduleError[i];
                if (minumumAcquisitionCount > pimega->acq_status_return.noOfAquisitions[i])
                    minumumAcquisitionCount = pimega->acq_status_return.noOfAquisitions[i];
            }
            /* Index enable */
            getIntegerParam(PimegaIndexEnable, &indexEnable);

            /* If save is enabled */
            getParameter(NDAutoSave, &autoSave);


            if (imageMode == ADImageSingle || imageMode == ADImageMultiple) {
            

                //printf("indexError=%x\n", pimega->acq_status_return.indexError);
                 /* Saving is enabled and the saved images is less than requested */
                 if (pimega->acq_status_return.savedAquisitionNum < (unsigned int)pimega->acquireParam.numCapture && 
                     autoSave == 1) {
                        /* Check if there are still images to save by comparing the received with the saved.
                           This is due to the slower saving rate. */
                        if (minumumAcquisitionCount > pimega->acq_status_return.savedAquisitionNum)
                        {
                            setStringParam(ADStatusMessage, "Saving acquired frames"); 
                        }
                        else{
                        /* The number of received images is equal or less than saved. Problem may exist. 
                            Check if external trigger is enabled. If not, detector dropped frames. */ 
                            setIntegerParam(ADStatus, ADStatusError);  
                            if (minumumAcquisitionCount == 0)
                                setParameter(ADStringFromServer, "No images received. Waiting...");
                            else    
                                setParameter(ADStringFromServer, "Not all images received. Waiting..."); 

                            if (triggerMode == PIMEGA_TRIGGER_MODE_INTERNAL) {
                                setStringParam(ADStatusMessage, "Detector not responding");
                            }
                            else {
                                 setStringParam(ADStatusMessage, "Trigger not received/Detector failure");
                            }
                        }
                }
                /* if index is enabled and the number of requested acquisitions is larger than the number of acquisitions
                   sent to index */
                else if (pimega->acq_status_return.indexSentAquisitionNum < (unsigned int)pimega->acquireParam.numCapture && 
                    (bool)indexEnable == true)  
                {
                    setStringParam(ADStatusMessage, "Sending frames to Index");
                }  
                /* Saving is not enabled, or saving is enabled and all images arrived */   
                else {
                    setStringParam(ADStatusMessage, "Acquisition finished");
                    setParameter(ADStringFromServer, "Done"); //¯\_(⊙︿⊙)_/¯
                    acquire=0;
                    setIntegerParam(ADAcquire, 0); 
                    acquireStatus = 0;
                    setIntegerParam(ADStatus, ADStatusIdle);   
                }  
                /* Errors reported by backend override previous messages. */                
                if (moduleError != false)
                {
                    setParameter(ADStatusMessage, "Detector error");
                    setParameter(ADStringFromServer, "Detector dropped frames");
                    setIntegerParam(ADStatus, ADStatusError);
                }
                else if (pimega->acq_status_return.indexError != false)
                {
                    setParameter(ADStatusMessage, "Index error");
                    setParameter(ADStringFromServer, "Index not responding");
                    setIntegerParam(ADStatus, ADStatusError);
                }                              
            }
           
            else if (imageMode == ADImageContinuous) {
                if (minumumAcquisitionCount >= numImagesCounter)
                {
                    status = startAcquire();
                    acquireStatus = 0;
                    numImagesCounter++;
                    setStringParam(ADStatusMessage, "Acquiring");
                    setParameter(ADStringFromServer, "Receiving images");                    
                } else {
                    setStringParam(ADStatusMessage, "Detector not responding");
                    setParameter(ADStringFromServer, "No images received. Waiting..."); 
                }
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


    int adstatus, backendStatus, acquireRunning;
    //int acquiring;

    getParamName(function, &paramName);
    printf("Int Function Idx: %d\n", function);
    printf("Int Function Name: %s\n", paramName);
    printf("Int Function Value: %d\n", value);

    /* Ensure that ADStatus is set correctly before we set ADAcquire.*/
    getIntegerParam(ADStatus, &adstatus);
    getParameter(NDFileCapture,&backendStatus);
    getParameter(ADAcquire,&acquireRunning);


    if (function == ADAcquire) {
        /* if d */
        if (value && backendStatus && 
            (adstatus == ADStatusIdle || adstatus == ADStatusError || adstatus == ADStatusAborted)) {
            /* Send an event to wake up the acq task.  */
            v_print("Start event detected. Sending start event signal.\n");
            epicsEventSignal(this->startEventId_);
            
        }
        else if (!value && (adstatus == ADStatusAcquire || adstatus == ADStatusError)) {
          /* This was a command to stop acquisition */
            v_print("Stop event detected. Sending stop event signal.\n");
            epicsEventSignal(this->stopEventId_);
            epicsThreadSleep(.1);
        }
        else {
            err_print("Neither value nor !value condition was chosen. value=%d, adstatus=%s(%d), backendStatus=%d. returning asynError\n", value, 
                        adstatus == ADStatusIdle? "ADStatusIdle" :
                        adstatus == ADStatusError? "ADStatusError" :
                        adstatus == ADStatusAborted? "ADStatusAborted" :
                        adstatus == ADStatusAcquire? "ADStatusAcquire" : "adstatus not known", adstatus, backendStatus);
            return asynError;
        }
    }

    else if (function == NDFileCapture) {
        if (value) {
            if (acquireRunning == 0)
            {
                status |= startCaptureBackend();
            } else
            {
                err_print("Detector acquisition running. Will not start a new backend capture. Sending asynError.\n");
                return asynError;
            }
        }
        if (!value) { 
            status |= send_stopAcquire_toBackend(pimega);
            setParameter(ADStringFromServer, "Backend Stopped");
        }
    }
    else if (function == PimegaAbortSave){
        if (backendStatus)
            setParameter(ADStringFromServer, "Cannot Abort - Must stop acquisition first");
        else if (value) 
        { 
            status |=  abort_save(pimega);
            setParameter(ADStringFromServer, "Save Aborted");
        }
    }
    else if (function == PimegaSendImage) {
        if (value) status |= sendImage();
    }
    else if (function == PimegaLoadEqStart) {
        
        if (value) status |= loadEqualization(pimega->loadEqCFG);
    }    
    
    else if (function == PimegaCheckSensors) {
        if (value) status |= checkSensors();
    }
    else if (function == PimegaOmrOPMode)
        status |= setOMRValue(OMR_M, value, function);
    else if (function == ADNumExposures)
        status |=  numExposures(value);
    else if (function == PimegaReset)
        status |=  reset(value);
    else if (function == PimegaMedipixMode)
        status |= medipixMode(value);
    else if (function == PimegaModule)
        status |= selectModule(value);
    else if (function == ADTriggerMode)
        status |=  triggerMode(value);
    else if (function == PimegaConfigDiscL)
        status |= configDiscL(value);
    else if (function == PimegaMedipixBoard)
        status |= medipixBoard(value);
    else if (function == PimegaMedipixChip)
        status |= imgChipID(value);
    else if (function == PimegaPixelMode)
        status |= setOMRValue(OMR_CSM_SPM, value, function);
    else if (function == PimegaContinuosRW)
        status |= setOMRValue(OMR_CRW_SRW, value, function);
    else if (function == PimegaPolarity)
        status |= setOMRValue(OMR_Polarity, value, function);
    else if (function == PimegaDiscriminator)
        status |= setOMRValue(OMR_Disc_CSM_SPM, value, function);
    else if (function == PimegaTestPulse)
        status |= setOMRValue(OMR_EnableTP, value, function);
    else if (function == PimegaCounterDepth)
        status |= setOMRValue(OMR_CountL, value, function);
    else if (function == PimegaEqualization)
        status |= setOMRValue(OMR_Equalization, value, function);
    else if (function == PimegaGain)
        status |= setOMRValue(OMR_Gain_Mode, value, function);
    else if (function == PimegaExtBgSel)
        status |= setOMRValue(OMR_Ext_BG_Sel, value, function);
    else if (function == PimegaReadCounter)
        status |= readCounter(value);
    else if (function == PimegaSenseDacSel)
        status |= senseDacSel(value);
    //DACS functions
    else if (function == PimegaCas)
        status |=  setDACValue(DAC_CAS, value, function);
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
    else if (function == PimegaMBSelTSensor) {
        setParameter(PimegaMBTSensor, pimega->pimegaParam.mb_temperature[0][value-1]);
    }
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
        /* Set the parameter and readback in the parameter library.  This may be overwritten when we read back the
        * status at the end, but that's OK */
        setIntegerParam(function, value);
         /* Update any changed parameters */
        callParamCallbacks();
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
              "%s:%s: function=%d, value=%d\n",
              driverName, functionName, function, value);
    }
    return (asynStatus)status;

}


asynStatus pimegaDetector::writeInt32Array(asynUser * 	pasynUser, epicsInt32 * 	value, size_t 	nElements )
{
    int function = pasynUser->reason;
    size_t i;
    int status = asynSuccess;
    const char *paramName;

    getParamName(function, &paramName);
    printf("String Function Idx: %d\n", function);
    printf("String Function Name: %s\n", paramName);
    printf("writeInt32Array Function nElements: %lu\n", nElements);
    printf("writeInt32Array Function Value: ");
    for (i = 0; i < nElements; i++)
        printf("%d ", value[i]);
    printf("\n");
  
    printf("String Function Idx: %d\n", function);
    if (function == PimegaLoadEqualization)
    {
        int idxParam = PimegaLoadEqualization;
        status = set_eq_cfg(pimega, (uint32_t *)value, nElements);
        doCallbacksInt32Array(value, nElements, idxParam, 0);
        printf("executed set_eq_cfg\n");
    }
    /* If this parameter belongs to a base class call its method */
    else if (function < FIRST_PIMEGA_PARAM) 
        status = ADDriver::writeInt32Array(pasynUser, value, nElements);


    if (status)
    {
        printf("Status not ok\n");
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
              "%s:writeInt32Array error, status=%d function=%d, value=",
              driverName, status, function);
              for ( i = 0; i < nElements; i++)
                printf("%d ", value[i]);
                printf("\n");
    }else{
        /* Do callbacks so higher layers see any changes */
        printf("Status ok\n");
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
              "%s:writeInt32Array: function=%d, value=",
              driverName, function);
              for ( i = 0; i < nElements; i++)
                printf("%d ", value[i]);
                printf("\n");              
    }

    return((asynStatus)status);
}


asynStatus pimegaDetector::writeOctet(asynUser *pasynUser, const char *value, size_t maxChars, size_t *nActual)
{
    int function = pasynUser->reason;
    int status = asynSuccess;
    const char *paramName;

    getParamName(function, &paramName);
    printf("String Function Idx: %d\n", function);
    printf("String Function Name: %s\n", paramName);
    printf("String Function Value: %s\n", value);

    if (function == pimegaDacDefaults)
    {
        *nActual = maxChars;
        status = dacDefaults(value);
    }
    else if (function == PimegaIndexID)
    {
        *nActual = maxChars;
        setParameter(function, value);
    }
    else {
    /* If this parameter belongs to a base class call its method */
        if (function < FIRST_PIMEGA_PARAM) status = ADDriver::writeOctet(pasynUser, value, maxChars, nActual);
    }

    if (status)
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
              "%s:writeOctet error, status=%d function=%d, value=%s\n",
              driverName, status, function, value);
    else{
        /* Do callbacks so higher layers see any changes */
        callParamCallbacks();
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
              "%s:writeOctet: function=%d, value=%s\n",
              driverName, function, value);
    }

    return((asynStatus)status);
}

asynStatus pimegaDetector::dacDefaults(const char * file)
{
    int rc;

    rc = configure_module_dacs_with_file(pimega, file);
    if (rc != PIMEGA_SUCCESS) {
        error("Invalid value: %s\n", pimega_error_string(rc));
        return asynError;
    }
    setParameter(pimegaDacDefaults, file);
    return asynSuccess;
}

asynStatus pimegaDetector::writeFloat64(asynUser *pasynUser, epicsFloat64 value)
{
    int function = pasynUser->reason;
    int status = asynSuccess;
    const char *paramName;
    //const char *functionName = "writeFloat64";

    getParamName(function, &paramName);
    printf("Float Function Idx: %d\n", function);
    printf("Float Function Name: %s\n", paramName);
    printf("Float Function Value: %f\n", value);

    status |= setDoubleParam(function, value);

    if (function == ADAcquireTime)
        status |= acqTime(value);

    else if (function == ADAcquirePeriod)
        status |= acqPeriod(value);

    else if (function == PimegaSensorBias)
        status |= sensorBias(value);

    else if (function == PimegaExtBgIn)
        status |= setExtBgIn(value);

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
    int numPoints = 0;
    epicsFloat32 *inPtr;
    //const char *paramName;
    static const char *functionName = "pimegaDetector::readFloat32Array";

    this->getAddress(pasynUser, &addr);
 
    if(function == PimegaDacsOutSense) {
        inPtr = PimegaDacsOutSense_;
        numPoints = N_DACS_OUTS;
    }
    else if (function == PimegaMBTemperatureM1){
        getMbTemperature();
        inPtr = PimegaMBTemperature_;
        numPoints = pimega->num_mb_tsensors;
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
    //static const char *functionName = "readFloat64";
    double temp = 0;
    int scanStatus, i;

    getParameter(ADStatus,&scanStatus);

    //if (function == ADTemperatureActual) {
    //    status = US_TemperatureActual(pimega);
    //    setParameter(ADTemperatureActual, pimega->cached_result.actual_temperature);
    //}

    if (function == PimegaBackBuffer) {
        for (i = 0;  i < pimega->max_num_modules; i++)
            if (temp < pimega->acq_status_return.bufferUsed[i])
                temp = pimega->acq_status_return.bufferUsed[i];
        *value = temp;        
    }

    else if (function == PimegaDacOutSense){
        status = US_ImgChipDACOUTSense_RBV(pimega);
        *value = pimega->pimegaParam.dacOutput;
    }

    else if (function == PimegaMPAvgTSensorM1){
        status = getMedipixTemperature();
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
    //static const char *functionName = "readInt32";
    int scanStatus, i;
    uint64_t temp = ULLONG_MAX;
    int backendStatus;

    getParameter(ADStatus, &scanStatus);
    getParameter(NDFileCapture, &backendStatus);



    if (function == ADNumImagesCounter) {
            for (i = 0;  i < pimega->max_num_modules; i++)
                if (temp > pimega->acq_status_return.noOfAquisitions[i])
                    temp = pimega->acq_status_return.noOfAquisitions[i];
            *value = temp;
    }

    else if (function == PimegaModule) {
        *value = pimega->pimega_module;
    }

    else if (function == NDFileNumCaptured) {
        if (backendStatus) {
            int num_capture = (unsigned int)pimega->acquireParam.numCapture;
            status = get_acqStatus_fromBackend(pimega);
            generateImage();
            int backend_saved = pimega->acq_status_return.savedAquisitionNum;

            if ((num_capture != 0) && (backend_saved == num_capture)) {
                send_stopAcquire_toBackend(pimega);

                setParameter(ADStatusMessage, "Finished acquisition");
                setParameter(ADStringToServer, "");
                setParameter(ADStringFromServer, "HDF5 file saved");
                setParameter(NDFileCapture, 0);
            }
        
            callParamCallbacks();
        }

        *value = pimega->acq_status_return.savedAquisitionNum;
    }
    else if (function == PimegaIndexCounter) {
        if (backendStatus) {
            *value = pimega->acq_status_return.indexSentAquisitionNum; 
        }      
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
                                    size_t maxMemory, int priority, int stackSize, int simulate, int backendOn, int log)
{
    new pimegaDetector(portName,
                       address_module01,
                       address_module02,
                       address_module03,
                       address_module04,
                       port, maxSizeX, maxSizeY,
                       detectorModel, maxBuffers,
                       maxMemory, priority, stackSize, simulate, backendOn, log);
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
                   int port, int SizeX, int SizeY,
                   int detectorModel, int maxBuffers, size_t maxMemory, int priority, int stackSize, int simulate, int backendOn, int log)

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

    numImageSaved = 0;
    // initialize random seed:
    srand (time(NULL));

    //Alocate memory for PimegaDacsOutSense_
    PimegaDacsOutSense_ = (epicsFloat32 *)calloc(N_DACS_OUTS, sizeof(epicsFloat32));

    //Alocate memory for PimegaDisabledSensors_
    PimegaDisabledSensors_ = (epicsInt32 *)calloc(36, sizeof(epicsInt32));

    if (simulate == 1)
        printf("Simulation mode activated.\n");
    else
        printf("Simulation mode inactivate.\n");


    
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


    pimega = pimega_new((pimega_detector_model_t)  detectorModel);
    pimega->log = log;
    pimega->detModel = (pimega_detector_model_t) detectorModel;
    pimega->backendOn = backendOn;
    if (log == 1)
    {
        if (initLog(pimega) == false)
        {
            warn_print("Disabling logging.\n");
            exit(0);
            pimega->log = 0;
        }
    }
    maxSizeX = SizeX;
    maxSizeY = SizeY;
    //pimega_set_debug_stream(pimega, pimega->debug_out);
    if (pimega) debug(functionName, "Pimega Object created!");
    pimega->simulate = simulate;
    connect(ips, port);
    status = prepare_pimega(pimega);
    if (status != PIMEGA_SUCCESS)
        panic("Unable to prepare pimega. Aborting...");
    //pimega->debug_out = fopen("log.txt", "w+");
    //report(pimega->debug_out, 1);
    //fflush(pimega->debug_out);

    createParameters();
    //check_and_disable_sensors(pimega);

    setDefaults();
    define_master_module(pimega, 1, false, PIMEGA_TRIGGER_MODE_EXTERNAL_POS_EDGE);

    //Alocate memory for PimegaMBTemperature_
    PimegaMBTemperature_ = (epicsFloat32 *)calloc(pimega->num_mb_tsensors,
                                                   sizeof(epicsFloat32));

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

void pimegaDetector::panic(const char *msg)
{
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s\n", msg);
    epicsExit(0);
}

bool pimegaDetector::initLog(pimega_t *pimega)
{
    
    struct tm *timenow;

    time_t now = time(NULL);
    timenow = gmtime(&now);

    strftime(pimega->logFileName, 40, "/tmp/ioclog_%Y%m%d_%H%M%S.log", timenow);

    pimega->logfp = fopen(pimega->logFileName,"w");
    if (pimega->logfp == NULL)
    {
        err_print("Failed to create log file %s.\n", pimega->logFileName);
        return false;
    }
    else
        return true;
}

void pimegaDetector::connect(const char *address[4], unsigned short port)
{
    int rc = 0;
    unsigned short ports[4] = {10000, 20000, 30000, 40000};
    
    if (pimega->simulate == 0) 
        ports[0] = ports[1] = ports[2] = ports[3] = port;
       
    //Serial Test
    //rc = open_serialPort(pimega, "/dev/ttyUSB0");
    
    // Connect to backend
    if (pimega->simulate == 1)
    {
        rc = pimega_connect_backend(pimega, "127.0.0.1", 5413);
        puts("simulated Backend");
    }
    else    
        rc = pimega_connect_backend(pimega, "127.0.0.1", 5412);

    if (rc != PIMEGA_SUCCESS) panic("Unable to connect with Backend. Aborting...");

    // Connect to detector
    rc |= pimega_connect(pimega, address, ports);
    if (rc != PIMEGA_SUCCESS) panic("Unable to connect with detector. Aborting...");
        
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
    createParam(pimegaLogFileString,        asynParamOctet,     &PimegaLogFile);
    createParam(pimegaDacDefaultsString,    asynParamOctet,     &pimegaDacDefaults);
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
    createParam(pimegaBackendBufferString,  asynParamFloat64,     &PimegaBackBuffer);
    createParam(pimegaResetRDMABufferString,asynParamInt32,     &PimegaResetRDMABuffer);
    createParam(pimegaBackendLFSRString,    asynParamInt32,     &PimegaBackLFSR);
    createParam(pimegaSensorBiasString,     asynParamFloat64,   &PimegaSensorBias);
    createParam(pimegaAllModulesString,     asynParamInt32,     &PimegaAllModules);
    createParam(pimegaDacsOutSenseString,   asynParamFloat32Array, &PimegaDacsOutSense);
    createParam(pimegaSendImageString,      asynParamInt32,     &PimegaSendImage);
    createParam(pimegaSelSendImageString,   asynParamInt32,     &PimegaSelSendImage);
    createParam(pimegaSendDacDoneString,    asynParamInt32,     &PimegaSendDacDone);
    createParam(pimegaConfigDiscLString,    asynParamInt32,     &PimegaConfigDiscL);
    createParam(pimegaLoadEqString,         asynParamInt32Array, &PimegaLoadEqualization);
    createParam(pimegaExtBgInString,        asynParamFloat64,   &PimegaExtBgIn);
    createParam(pimegaExtBgSelString,       asynParamInt32,     &PimegaExtBgSel);
    createParam(pimegaMbM1TempString,       asynParamFloat32Array, &PimegaMBTemperatureM1);
    createParam(pimegaMbM2TempString,       asynParamFloat32Array, &PimegaMBTemperatureM2);
    createParam(pimegaMbM3TempString,       asynParamFloat32Array, &PimegaMBTemperatureM3);
    createParam(pimegaMbM4TempString,       asynParamFloat32Array, &PimegaMBTemperatureM4);
    createParam(pimegaMBAvgM1String,        asynParamFloat64,   &PimegaMBAvgTSensorM1);
    createParam(pimegaMBAvgM2String,        asynParamFloat64,   &PimegaMBAvgTSensorM2);
    createParam(pimegaMBAvgM3String,        asynParamFloat64,   &PimegaMBAvgTSensorM3);
    createParam(pimegaMBAvgM4String,        asynParamFloat64,   &PimegaMBAvgTSensorM4);
    createParam(pimegaMbSelTSensorString,   asynParamInt32,     &PimegaMBSelTSensor);
    createParam(pimegaLoadEqStartString,    asynParamInt32,     &PimegaLoadEqStart);
    
    createParam(pimegaMbTSensorString,      asynParamFloat64,   &PimegaMBTSensor);
    createParam(pimegaMPAvgM1String,        asynParamFloat64,   &PimegaMPAvgTSensorM1);
    createParam(pimegaMPAvgM2String,        asynParamFloat64,   &PimegaMPAvgTSensorM2);
    createParam(pimegaMPAvgM3String,        asynParamFloat64,   &PimegaMPAvgTSensorM3);
    createParam(pimegaMPAvgM4String,        asynParamFloat64,   &PimegaMPAvgTSensorM4);
    createParam(pimegaCheckSensorsString,   asynParamInt32,     &PimegaCheckSensors);
    createParam(pimegaDisabledSensorsM1String,asynParamInt32Array, &PimegaDisabledSensorsM1);
    createParam(pimegaDisabledSensorsM2String,asynParamInt32Array, &PimegaDisabledSensorsM2);
    createParam(pimegaDisabledSensorsM3String,asynParamInt32Array, &PimegaDisabledSensorsM3);
    createParam(pimegaDisabledSensorsM4String,asynParamInt32Array, &PimegaDisabledSensorsM4);
    createParam(pimegaEnableBulkProcessingString, asynParamInt32, &PimegaEnableBulkProcessing);
    createParam(pimegaAbortSaveString,      asynParamInt32,     &PimegaAbortSave);
    createParam(pimegaIndexEnableString,    asynParamInt32,     &PimegaIndexEnable);
    createParam(pimegaIndexSendModeString,  asynParamInt32,     &PimegaIndexSendMode);
    createParam(pimegaIndexIDString,        asynParamOctet,     &PimegaIndexID);
    createParam(pimegaIndexCounterString,   asynParamInt32,     &PimegaIndexCounter);
    createParam(pimegaMBSendModeString,     asynParamInt32,     &PimegaMBSendMode);
    createParam(pimegaDistanceString,       asynParamInt32,     &PimegaDistance);

    /* Do callbacks so higher layers see any changes */
    callParamCallbacks();

}

void pimegaDetector::setDefaults(void)
{
    
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
    //setParameter(NDFileNumCapture, 1);
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
    setParameter(PimegaBackBuffer, 0.0);
    setParameter(ADImageMode, ADImageSingle);
    setParameter(PimegaModule, 1);
    //Set_DAC_Defaults(pimega);

    imgChipID(1);
    US_ImageMode_RBV(pimega);
    setParameter(PimegaMedipixMode, pimega->pimegaParam.image_mode);
    setParameter(PimegaLogFile, pimega->logFileName);
    callParamCallbacks();
}

void pimegaDetector::getDacsValues(void)
{
    int sensor;
    getParameter(PimegaMedipixChip, &sensor);
    sensor -= 1;

    get_dac(pimega, DIGITAL_READ_ALL_DACS, DAC_ThresholdEnergy0);
    setParameter(PimegaThreshold0, (int)pimega->digital_dac_values[sensor][DAC_ThresholdEnergy0-1]);
    setParameter(PimegaThreshold1, (int)pimega->digital_dac_values[sensor][DAC_ThresholdEnergy1-1]);
    setParameter(PimegaPreamp, (int)pimega->digital_dac_values[sensor][DAC_Preamp-1]);
    setParameter(PimegaIkrum, (int)pimega->digital_dac_values[sensor][DAC_IKrum-1]);
    setParameter(PimegaShaper, (int)pimega->digital_dac_values[sensor][DAC_Shaper-1]);
    setParameter(PimegaDisc, (int)pimega->digital_dac_values[sensor][DAC_Disc-1]);
    setParameter(PimegaDiscLS, (int)pimega->digital_dac_values[sensor][DAC_DiscLS-1]);
    //setParameter(PimegaShaperTest, pimega->digital_dac_values[DAC_ShaperTest])
    setParameter(PimegaDiscL, (int)pimega->digital_dac_values[sensor][DAC_DiscL-1]);
    setParameter(PimegaDelay, (int)pimega->digital_dac_values[sensor][DAC_Delay-1]);
    setParameter(PimegaTpBufferIn, (int)pimega->digital_dac_values[sensor][DAC_TPBufferIn-1]);
    setParameter(PimegaTpBufferOut, (int)pimega->digital_dac_values[sensor][DAC_TPBufferOut-1]);
    setParameter(PimegaRpz, (int)pimega->digital_dac_values[sensor][DAC_RPZ-1]);
    setParameter(PimegaGnd, (int)pimega->digital_dac_values[sensor][DAC_GND-1]);
    setParameter(PimegaTpRef, (int)pimega->digital_dac_values[sensor][DAC_TPRef-1]);
    setParameter(PimegaFbk, (int)pimega->digital_dac_values[sensor][DAC_FBK-1]);
    setParameter(PimegaCas, (int)pimega->digital_dac_values[sensor][DAC_CAS-1]);
    setParameter(PimegaTpRefA, (int)pimega->digital_dac_values[sensor][DAC_TPRefA-1]);
    setParameter(PimegaTpRefB, (int)pimega->digital_dac_values[sensor][DAC_TPRefB-1]);
    //setParameter(PimegaShaperTest, pimega->digital_dac_values[DAC_Test]);
    setParameter(PimegaDiscH,(int)pimega->digital_dac_values[sensor][DAC_DiscH-1]);
    //getDacsOutSense();
}

void pimegaDetector::getOmrValues(void)
{
    get_omr(pimega);
    setParameter(PimegaOmrOPMode, pimega->omr_values[OMR_M]);
    setParameter(PimegaContinuosRW, pimega->omr_values[OMR_CRW_SRW]);
    setParameter(PimegaPolarity, pimega->omr_values[OMR_Polarity]);
    setParameter(PimegaDiscriminator, pimega->omr_values[OMR_Disc_CSM_SPM]);
    setParameter(PimegaTestPulse, pimega->omr_values[OMR_EnableTP]);
    setParameter(PimegaCounterDepth, pimega->omr_values[OMR_CountL]);
    setParameter(PimegaEqualization, pimega->omr_values[OMR_Equalization]);
    setParameter(PimegaPixelMode, pimega->omr_values[OMR_CSM_SPM]);
    setParameter(PimegaGain, pimega->omr_values[OMR_Gain_Mode]);
    setParameter(PimegaExtBgSel, pimega->omr_values[OMR_Ext_BG_Sel]);
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

int pimegaDetector::startAcquire(void)
{
    int rc = 0;
    pimega->pimegaParam.software_trigger = false;
    rc |= execute_acquire(pimega);
    return rc;
}

int pimegaDetector::startCaptureBackend(void)
{
    int rc = 0;
    int acqMode, autoSave, lfsr, bulkProcessingEnum;
    char fullFileName[PIMEGA_MAX_FILENAME_LEN];
    bool bulkProcessingBool;
    double acquirePeriod, acquireTime;
    int triggerMode;
    bool externalTrigger;
    char IndexID[30] = "";
    int indexEnable;
    int indexSendMode;//enum IndexSendMode
    setStringParam(ADStatusMessage, "Starting acquisition");
    setParameter(ADStringFromServer, "Configuring");
    callParamCallbacks();
    
    /* Create the full filename */
    createFileName(sizeof(fullFileName), fullFileName);
    rc = set_file_name_template(pimega, fullFileName);

    getParameter(PimegaMedipixMode, &acqMode);
    getParameter(NDAutoSave,&autoSave);
    getParameter(PimegaBackLFSR, &lfsr);
    getParameter(PimegaEnableBulkProcessing, &bulkProcessingEnum);
    getParameter(ADAcquirePeriod, &acquirePeriod);
    getParameter(ADAcquireTime, &acquireTime);
    getParameter(ADTriggerMode, &triggerMode);


    getStringParam(PimegaIndexID, sizeof(IndexID), IndexID);
    getParameter(PimegaIndexEnable, &indexEnable);
    getParameter(PimegaIndexSendMode, &indexSendMode);

    /* Evaluate trigger if external or internal */
    if (triggerMode == PIMEGA_TRIGGER_MODE_INTERNAL)
        externalTrigger = false;
    else
        externalTrigger = true;
    getParameter(NDFileNumCapture, &pimega->acquireParam.numCapture);

    /* Evaluate if bulk processing is necessary*/
    bulkProcessingBool = evaluateBulkProcessing((enum bulkProcessingEnum)bulkProcessingEnum, acquirePeriod, 
                                                 acquireTime, externalTrigger, pimega->acquireParam.numCapture);

    /* Always reset RDMA logic in the FPGA at new capture */
    send_allinitArgs_allModules(pimega);

    /* Always reset backend RDMA buffers */
    rc = update_backend_acqArgs(pimega, acqMode, lfsr, autoSave, true, (bool)bulkProcessingBool,
                               (enum IndexSendMode)indexSendMode, IndexID, (bool) indexEnable);


    rc = send_acqArgs_toBackend(pimega);
    if (rc != PIMEGA_SUCCESS) {
        char error[100];
        decode_backend_error(pimega->ack.error, error);
        setParameter(ADStringFromServer, error);
        callParamCallbacks();
        return rc;
    }

    if (pimega->detModel == pimega540D){
        rc |= select_module(pimega, 2);
        rc |= US_Acquire(pimega, 1);
        rc |= select_module(pimega, 3);
        rc |= US_Acquire(pimega, 1);
        rc |= select_module(pimega, 4);
        rc |= US_Acquire(pimega, 1);
    }

    setParameter(ADStringFromServer, "Backend Ready");

    return rc;
}


int pimegaDetector::dac_scan_tmp(pimega_dac_t dac)
{
    int rc = 0;
    printf("DAC: %d\n", dac);
    if(dac == DAC_GND) {
       	rc = US_DAC_Scan(pimega, DAC_GND, 90, 150, 1, PIMEGA_SEND_ALL_CHIPS_ALL_MODULES);
        select_module(pimega, 4);
        select_chipNumber(pimega, 36);
        rc = US_DAC_Scan(pimega, DAC_GND, 50, 100, 1, PIMEGA_SEND_ONE_CHIP_ONE_MODULE);
    }

    else if(dac == DAC_FBK) {
       	rc = US_DAC_Scan(pimega, DAC_FBK, 140, 200, 1, PIMEGA_SEND_ALL_CHIPS_ALL_MODULES);;
        select_module(pimega, 4);
        select_chipNumber(pimega, 36);
        rc = US_DAC_Scan(pimega, DAC_FBK, 80, 130, 1, PIMEGA_SEND_ONE_CHIP_ONE_MODULE);
    }

    else if(dac == DAC_CAS) {
       	rc = US_DAC_Scan(pimega, DAC_CAS, 140, 200, 1, PIMEGA_SEND_ALL_CHIPS_ALL_MODULES);
        select_module(pimega, 4);
        select_chipNumber(pimega, 36);
        rc = US_DAC_Scan(pimega, DAC_CAS, 80, 130, 1, PIMEGA_SEND_ONE_CHIP_ONE_MODULE);
    }

	return rc;
}



asynStatus pimegaDetector::selectModule(uint8_t module)
{
    int rc;
    int mfb, send_mode;
    getParameter(PimegaMedipixBoard, &mfb);
    getParameter(PimegaMBSendMode, &send_mode);
    rc = select_module(pimega, module);
    if (rc != PIMEGA_SUCCESS) {
        error("Invalid module number: %s\n", pimega_error_string(rc));
        return asynError;
    }
    rc |= select_board(pimega, mfb);
    rc |= getSensorBias(pimega, (pimega_send_mb_flex_t) send_mode);
    setParameter(PimegaSensorBias, 
                 pimega->pimegaParam.bias_voltage);

    setParameter(PimegaModule, module);
    return asynSuccess;    
}

asynStatus pimegaDetector::triggerMode(int trigger)
{
    int rc;
    rc = configure_trigger(pimega, (pimega_trigger_mode_t)trigger);
    if (rc != PIMEGA_SUCCESS) {
        error("TriggerMode out the range: %s\n", pimega_error_string(rc));
        return asynError;
    }
    return asynSuccess;
}

asynStatus pimegaDetector::configDiscL(int value)
{
    int rc;
    int all_modules;
    getParameter(PimegaAllModules, &all_modules);
    rc = US_ConfigDiscL(pimega, value, (pimega_send_to_all_t)all_modules);
    if (rc != PIMEGA_SUCCESS) {
        error("Value out the range: %s\n", pimega_error_string(rc));
        return asynError;
    }
    return asynSuccess;
}

asynStatus pimegaDetector::setDACValue(pimega_dac_t dac, int value, int parameter)
{
    int rc;
    int all_modules;

    setParameter(PimegaSendDacDone, 0);
    callParamCallbacks();

    getParameter(PimegaAllModules, &all_modules);
    rc = set_dac(pimega, dac, (unsigned)value, (pimega_send_to_all_t)all_modules);
    if (rc != PIMEGA_SUCCESS) {
        error("Unable to change DAC value: %s\n", pimega_error_string(rc));
        return asynError;
    }

    setParameter(PimegaSendDacDone, 1);
    //rc = US_ImgChipDACOUTSense_RBV(pimega);
    //setParameter(PimegaDacOutSense, pimega->pimegaParam.dacOutput);
    setParameter(parameter, value);
    return asynSuccess;
}

asynStatus pimegaDetector::setOMRValue(pimega_omr_t omr, int value, int parameter)
{
    int rc;
    int all_modules;

    getParameter(PimegaAllModules, &all_modules);
    rc = set_omr(pimega, omr, (unsigned)value, (pimega_send_to_all_t)all_modules);
    if (rc != PIMEGA_SUCCESS) {
        error("Unable to change OMR value: %s\n", pimega_error_string(rc));
        return asynError;
    }

    setParameter(parameter, value);
    return asynSuccess;
}

asynStatus pimegaDetector::loadEqualization(uint32_t *cfg)
{
    int rc = 0, send_form, sensor;

    getParameter(PimegaAllModules, &send_form);
    getParameter(PimegaMedipixChip, &sensor);

    rc |= load_equalization(pimega, cfg, sensor, (pimega_send_to_all_t)send_form);

    if (rc != PIMEGA_SUCCESS) return asynError;
    return asynSuccess;
}

asynStatus pimegaDetector::sendImage(void)
{
    int rc = 0, send_to_all, pattern;

    getParameter(PimegaSelSendImage, &pattern);
    getParameter(PimegaAllModules, &send_to_all);
    send_image(pimega, send_to_all, pattern);

    if (rc != PIMEGA_SUCCESS) return asynError;
    return asynSuccess;
}

asynStatus pimegaDetector::checkSensors(void)
{
    int rc = 0, idxParam;

    idxParam = PimegaDisabledSensorsM1;
    rc = check_and_disable_sensors(pimega);
    for (int module = 1; module <= pimega->max_num_modules; module++) {
        for (int sensor=0; sensor<pimega->num_all_chips; sensor++) {
            PimegaDisabledSensors_[sensor] = (epicsInt32)(pimega->sensor_disabled[module-1][sensor]);
        }
        doCallbacksInt32Array(PimegaDisabledSensors_, pimega->num_all_chips, idxParam, 0);
        idxParam++;
        callParamCallbacks();
    }
    
    if (rc != PIMEGA_SUCCESS) return asynError;
    return asynSuccess;
}

asynStatus pimegaDetector::reset(short action)
{
    int rc = 0;
    if (action < 0 || action > 1) {
        error("Invalid boolean value: %d\n", action);
        return asynError;
    }

    if (action == 0) {
        rc = pimega_reset(pimega);
    }

    else {
        char _file[256] = "";
        getStringParam(pimegaDacDefaults, sizeof(_file), _file);
        printf("reading file %s\n", _file);
        rc |= pimega_reset_and_init(pimega, _file);
    }
    /* Set some default parameters */
    acqPeriod(0.0);
    acqTime(1.0);
    numExposures(1);
    setParameter(ADTriggerMode, PIMEGA_TRIGGER_MODE_INTERNAL);
    medipixMode(PIMEGA_MEDIPIX_MODE_DEFAULT);

    if (rc != PIMEGA_SUCCESS) {
        return asynError; }

    return asynSuccess;
}

asynStatus  pimegaDetector::medipixBoard(uint8_t board_id)
{
    int rc = 0, send_mode;

    rc = select_board(pimega, board_id);
    getParameter(PimegaMBSendMode, &send_mode);
    if (rc != PIMEGA_SUCCESS) {
        error("Invalid number of boards: %s\n", pimega_error_string(rc));
        return asynError;
    }

    rc |= getSensorBias(pimega, (pimega_send_mb_flex_t) send_mode);
    setParameter(PimegaSensorBias, 
                 pimega->pimegaParam.bias_voltage);


    //getMfbTemperature();
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
    char *_efuseID;

    rc = select_chipNumber(pimega, chip_id);
    if (rc != PIMEGA_SUCCESS) {
        error("Invalid number of medipix chip ID: %s\n", pimega_error_string(rc));
        return asynError;
    }
    setParameter(PimegaMedipixChip, chip_id);
    setParameter(PimegaMedipixBoard, pimega->sensor_pos.mb);

    /* Get e-fuseID from selected chip_id */ 
    rc = efuseid_rbv(pimega);
    if (rc != PIMEGA_SUCCESS) return asynError;
    _efuseID = pimega->pimegaParam.efuseID;
    setParameter(PimegaefuseID, _efuseID);

    getDacsValues();
    getOmrValues();
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

asynStatus pimegaDetector::acqPeriod(float period_time_s)
{
    int rc;

    rc = set_periodTime(pimega, period_time_s);
    if (rc != PIMEGA_SUCCESS){
        error("Invalid period time: %s\n", pimega_error_string(rc));
        return asynError;
    }

    else {
        setParameter(ADAcquirePeriod, period_time_s);
        return asynSuccess;
    }
}

asynStatus pimegaDetector::setExtBgIn(float voltage)
{
    int rc;

    rc = set_ImgChip_ExtBgIn(pimega, voltage);
    if (rc != PIMEGA_SUCCESS) {
        error("Invalid value: %s\n", pimega_error_string(rc));
        return asynError;
    }
    setParameter(PimegaExtBgIn, voltage);
    return asynSuccess;
}

asynStatus pimegaDetector::sensorBias(float voltage)
{
    int rc;
    int send_mode;

    getParameter(PimegaMBSendMode, &send_mode);
    rc = setSensorBias(pimega, voltage, (pimega_send_mb_flex_t)send_mode);
    if (rc != PIMEGA_SUCCESS) {
        error("Invalid value: %s\n", pimega_error_string(rc));
        return asynError;
    }
    setParameter(PimegaSensorBias,
                 pimega->pimegaParam.bias_voltage);

    
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
    int rc = 0;
    rc = setOMRValue(OMR_Sense_DAC, dac, PimegaSenseDacSel);
    if (rc != PIMEGA_SUCCESS) return asynError;
    rc = US_ImgChipDACOUTSense_RBV(pimega);
    setParameter(PimegaDacOutSense, pimega->pimegaParam.dacOutput);
    setParameter(PimegaSenseDacSel, dac);
    return asynSuccess;
}

asynStatus pimegaDetector::getDacsOutSense(void)
{
    int chip_id;
    getParameter(PimegaMedipixChip, &chip_id);

    for (int i=0; i<N_DACS_OUTS; i++) {
        PimegaDacsOutSense_[i] = (epicsFloat32)(pimega->analog_dac_values[chip_id-1][i]);
    }
    doCallbacksFloat32Array(PimegaDacsOutSense_, N_DACS_OUTS, PimegaDacsOutSense, 0);

    return asynSuccess;
}

asynStatus pimegaDetector::getMbTemperature(void)
{
    int idxWaveform, idxAvg;
    float sum=0.00, average;

    printf("Function: %d\n", PimegaMBTemperatureM1);
    idxWaveform = PimegaMBTemperatureM1;
    idxAvg = PimegaMBAvgTSensorM1;

    getMB_Temperatures(pimega);
    for (int module = 1; module <= pimega->max_num_modules; module++) {
        for (int i=0; i<pimega->num_mb_tsensors; i++) {
            PimegaMBTemperature_[i] = 
                (epicsFloat32)(pimega->pimegaParam.mb_temperature[module-1][i]);
            sum += PimegaMBTemperature_[i];
        }
        average = sum / pimega->num_mb_tsensors;
        sum = 0;
        setParameter(idxAvg, average);
        doCallbacksFloat32Array(PimegaMBTemperature_,
                                pimega->num_mb_tsensors,
                                idxWaveform,
                                0);
        idxWaveform++;
        idxAvg++;
    }
    
    callParamCallbacks();
    return asynSuccess;
}


asynStatus pimegaDetector::getMedipixTemperature(void)
{
    int idxAvg = PimegaMPAvgTSensorM1;
    get_TemperatureSensorAvg(pimega);
    for (int module = 1; module <= pimega->max_num_modules; module++)
        setParameter(idxAvg, pimega->pimegaParam.avg_chip_temperature[module-1]);
    callParamCallbacks();
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
static const iocshArg pimegaDetectorConfigArg13 = { "simulate", iocshArgInt };
static const iocshArg pimegaDetectorConfigArg14 = { "backendOn", iocshArgInt };
static const iocshArg pimegaDetectorConfigArg15 = { "log", iocshArgInt };
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
                                                            &pimegaDetectorConfigArg12,
                                                            &pimegaDetectorConfigArg13,
                                                            &pimegaDetectorConfigArg14,
                                                            &pimegaDetectorConfigArg15};
static const iocshFuncDef configpimegaDetector =
{ "pimegaDetectorConfig", 16, pimegaDetectorConfigArgs };

static void configpimegaDetectorCallFunc(const iocshArgBuf *args)
{
    pimegaDetectorConfig(args[0].sval, args[1].sval, args[2].sval, args[3].sval, args[4].sval,
                        args[5].ival, args[6].ival, args[7].ival, args[8].ival, args[9].ival,
                        args[10].ival, args[11].ival, args[12].ival, args[13].ival, args[14].ival, 
                        args[15].ival);
}

static void pimegaDetectorRegister(void)
{

    iocshRegister(&configpimegaDetector, configpimegaDetectorCallFunc);
}

extern "C"
{
epicsExportRegistrar(pimegaDetectorRegister);
}
