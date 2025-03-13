/* pimegaDetector.cpp
 *
 * This is a EPICS driver for the Pimega detector
 */

#include "pimegaDetector.h"

#include <bits/stdint-uintn.h>

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <lib/zmq_message_broker.hpp>

static pimega_t *pimega_global;

static void alarmTaskC(void *drvPvt) {
  pimegaDetector *pPvt = (pimegaDetector *)drvPvt;
  pPvt->alarmTask();
}

void pimegaDetector::alarmTask() {
  /* Loop forever */
  while (true) {
    if (GetTemperatureMonitorEnableStatus(pimega)) {
      pimegaDetector::getTemperatureHighest();
      pimegaDetector::getTemperatureStatus();
    }
    epicsThreadSleep(1.0);
  }
}

static void acquisitionTaskC(void *drvPvt) {
  pimegaDetector *pPvt = (pimegaDetector *)drvPvt;
  pPvt->acqTask();
}

void pimegaDetector::updateEpicsFrame(void *data, NDDataType_t ndarray_dtype) {
  int sizex, sizey;
  getIntegerParam(ADMaxSizeX, &sizex);
  getIntegerParam(ADMaxSizeY, &sizey);

  PIMEGA_PRINT(pimega, TRACE_MASK_FLOW, "updateEpicsFrame\n");

  size_t array_dims[2] = {sizex, sizey};
  NDArray *PimegaNDArray =
      this->pNDArrayPool->alloc(2, array_dims, ndarray_dtype, 0, NULL);
  memcpy(PimegaNDArray->pData, data, PimegaNDArray->dataSize);
  updateTimeStamp(&PimegaNDArray->epicsTS);
  this->getAttributes(PimegaNDArray->pAttributeList);
  doCallbacksGenericPointer(PimegaNDArray, NDArrayData, 0);
  PimegaNDArray->release();
}

/** This thread controls acquisition, reads image files to get the image data,
 * and does the callbacks to send it to higher layers It is totally decoupled
 * from the command thread and simply waits for data frames to be sent on the
 * data channel (TCP) regardless of the state in the command thread and TCP
 * channel */
void pimegaDetector::acqTask() {
  int status = asynSuccess;
  int eventStatus = 0;
  int numImages, numExposuresVar;
  uint64_t alignmentImagesCounter;
  int acquire = 0, i;
  int autoSave;
  int triggerMode;

  double acquireTime, acquirePeriod, remainingTime, elapsedTime;
  int acquireStatus = 0;
  epicsTimeStamp startTime, endTime;
  int indexEnable, backendStatus;
  bool indexEnableBool;
  const char *functionName = "acqTask";
  int64_t acquireImageCount = 0, acquireImageSavedCount = 0;
  int acquireStatusError = 0;

  this->lock();
  /* Loop forever */
  while (true) {
    /* No acquisition in place */
    if (!acquire) {
      /* reset acquireStatus */
      acquireStatus = 0;
      alignmentImagesCounter = 1;
      // Release the lock while we wait for an event that says acquire has
      // started, then lock again
      PIMEGA_PRINT(pimega, TRACE_MASK_FLOW,
                   "%s: Waiting for acquire to start\n", functionName);
      this->unlock();
      status = epicsEventWait(startAcquireEventId_);
      PIMEGA_PRINT(pimega, TRACE_MASK_FLOW, "%s: Acquire request received\n",
                   functionName);

      /* We are acquiring. */
      acquireStatusError = 0;

      /* Get the exposure parameters */
      getDoubleParam(ADAcquireTime, &acquireTime);
      getDoubleParam(ADAcquirePeriod, &acquirePeriod);

      getIntegerParam(ADNumExposures, &numExposuresVar);
      getIntegerParam(ADNumImages, &numImages);
      getIntegerParam(ADTriggerMode, &triggerMode);

      this->lock();
      /* Open the shutter */
      setShutter(ADShutterOpen);
      UPDATEIOCSTATUS("Acquiring");
      setIntegerParam(ADStatus, ADStatusAcquire);
      /* Backend status */
      getParameter(NDFileCapture, &backendStatus);
      status = startAcquire();

      if (status != asynSuccess) {
        this->unlock();
        PIMEGA_PRINT(pimega, TRACE_MASK_ERROR,
                     "%s: startAcquire() failed. Stop event sent\n",
                     functionName);
        epicsEventSignal(this->stopAcquireEventId_);
        acquireStatusError = 1;
        epicsThreadSleep(.1);
        this->lock();
      } else {
        acquire = 1;
        PIMEGA_PRINT(pimega, TRACE_MASK_FLOW, "%s: Acquire started\n",
                     functionName);
        /* Get the current time */
        epicsTimeGetCurrent(&startTime);
      }
    }
    /* Decoupled this from the next loop. Only needs to update acquireStatus
       when this condition is true (acquire && (acquireStatus != DONE_ACQ) */
    if (acquire && (acquireStatus != DONE_ACQ)) {
      acquireStatus = status_acquire(pimega);
    }
    /* will enter here when the detector did not finish acquisition
      (acquireStatus != DONE_ACQ) or when Elapsed time is chosen
      (!pimega->trigger_in_enum.PIMEGA_TRIGGER_IN_INTERNAL) */
    if (acquireStatus == PERMISSION_DENIED) {
      UPDATEIOCSTATUS("Permission Denied, press stop");
    }
    if (acquire &&
        (acquireStatus != DONE_ACQ || acquireStatus != PERMISSION_DENIED ||
         triggerMode != this->trigger_input_cfg->PIMEGA_TRIGGER_IN_INTERNAL)) {
      epicsTimeGetCurrent(&endTime);
      elapsedTime = epicsTimeDiffInSeconds(&endTime, &startTime);
      if (acquirePeriod != 0) {
        remainingTime = (acquirePeriod * numExposuresVar) - elapsedTime -
                        acquirePeriod + acquireTime;
      } else {
        remainingTime = (acquireTime * numExposuresVar) - elapsedTime;
      }
      if (remainingTime < 0) {
        remainingTime = 0;
      }
      if (triggerMode == this->trigger_input_cfg->PIMEGA_TRIGGER_IN_INTERNAL) {
        setDoubleParam(ADTimeRemaining, remainingTime);
      } else {
        setDoubleParam(ADTimeRemaining, elapsedTime);
      }
    }
    eventStatus = epicsEventTryWait(this->stopAcquireEventId_);

    /* Stop event detected */
    if (eventStatus == epicsEventWaitOK) {
      PIMEGA_PRINT(pimega, TRACE_MASK_FLOW,
                   "%s: Stop acquire request received in thread\n",
                   functionName);

      setShutter(0);
      setIntegerParam(ADAcquire, 0);
      acquire = 0;
      setIntegerParam(ADStatus, ADStatusAborted);
      if (acquireStatusError == 1) {
        acquireStatusError = 0;
        UPDATEIOCSTATUS(this->error_str);
        this->error_str[0] = '\0';
      } else {
        abort_save(pimega);
        UPDATEIOCSTATUS("Stop send to the backend");
      }
      callParamCallbacks();
      continue;
    }

    /* Added this delay for the thread not to hog the processor. No need to run
     * on full speed. */
    this->unlock();
    usleep(1000);
    this->lock();

    // printf("Index error = %d\n", GetAcqStatusIndexError(pimega));
    /* Will enter here only one time when the acqusition time is over. The
      current configuration assumes that when time is up, the thread goes to
      sleep, but perhaps we should consider changing this to only after
      when the frames are ready, acquire should become 0*/
    if (acquireStatus == DONE_ACQ && acquire) {
      /* Identify if Module error occured or received frames in all, or some
       * modules is 0 */
      bool moduleError = false;
      uint64_t recievedBackendCount = UINT64_MAX, processedBackendCount;
      recievedBackendCount = 0;
      processedBackendCount = GetAcqStatusProcessedImageNum(pimega);
      /* For several Acquires with one backend Capture call, the number of
         images sent to backend X is a multiple of the number of images sent to
         the detector Y ( X = K x Y ). So the offset to establish the end of a
         single acquire needs to be tracked */
      // acquireImageCount = recievedBackendCount - recievedBackendCountOffset;
      acquireImageCount =
          GetAcqStatusFramesReceived(pimega, get_selected_module(pimega) - 1);
      acquireImageSavedCount =
          GetAcqStatusSavedFramesCount(pimega) - recievedBackendCountOffset;

      /* Index enable */
      getIntegerParam(PimegaIndexEnable, &indexEnable);
      indexEnableBool = (bool)indexEnable;

      /* If save is enabled */
      getParameter(NDAutoSave, &autoSave);

      /* Acquire logic */
      switch (triggerMode) {
        /* Internal Trigger : Acquire should go down after the number of images
         * configured to the detector is received  */
        case IOC_TRIGGER_MODE_INTERNAL:

          /* Acquire and IOC status message management. Acquire still will wait
             for the images to be saved (if necessary) to go to 0 or will wait
             for index to receive the images or both */
          if (GetAcqStatusDone(pimega) != DONE_ACQ) {
            UPDATEIOCSTATUS("Not all images received. Waiting");
          } else if (indexEnableBool == true &&
                     GetAcqStatusSentAcquisitionsCount(pimega) <
                         (unsigned int)GetAcqParamCameraNumCapture(pimega)) {
            UPDATEIOCSTATUS("Sending frames to Index");
          } else if (processedBackendCount <
                     (unsigned int)GetAcqParamCameraNumCapture(pimega)) {
            UPDATEIOCSTATUS("Images received, processing");
          } else if (acquireStatus == DONE_ACQ) {
            finishAcq(triggerMode, &acquire, &acquireStatus,
                      &recievedBackendCountOffset, numExposuresVar);
          } else {
            UPDATEIOCSTATUS("Waiting Acquire Period");
          }

          break;

        case IOC_TRIGGER_MODE_EXTERNAL:

          /* In any case, when external trigger is enabled, what decides if
             Acquire becomes 0 or not is the total number of received images in
             the backend. Notice that the following snippet of code is identical
             to that of the Capture and server status message management block
           */
          if (GetAcqParamCameraNumCapture(pimega) != 0) {
            if (GetAcqStatusProcessedImageNum(pimega) <
                (unsigned int)GetAcqParamCameraNumCapture(pimega)) {
              UPDATEIOCSTATUS("Waiting for trigger");
            } else if (autoSave == 1 &&
                       processedBackendCount <
                           GetAcqStatusSavedFramesCount(pimega)) {
              UPDATEIOCSTATUS("Saving images..");
            } else if (indexEnableBool == true &&
                       GetAcqStatusSentAcquisitionsCount(pimega) <
                           (unsigned int)GetAcqParamCameraNumCapture(pimega)) {
              UPDATEIOCSTATUS("Sending frames to Index");
            } else if (acquireStatus == DONE_ACQ) {
              finishAcq(triggerMode, &acquire, &acquireStatus,
                        &recievedBackendCountOffset, numExposuresVar);
            } else {
              UPDATEIOCSTATUS("Waiting Acquire Period");
            }
          } else {
            UPDATEIOCSTATUS("Receiving images");
          }
          break;

        case IOC_TRIGGER_MODE_ALIGNMENT:

          if (acquireStatus == DONE_ACQ) {
            finishAcq(triggerMode, &acquire, &acquireStatus,
                      &recievedBackendCountOffset, numExposuresVar);
          }
          break;
      }

      /* Errors reported by backend override previous messages. */
      if (moduleError != false) {
        UPDATEIOCSTATUS("Detector error");
        setIntegerParam(ADStatus, ADStatusError);
      } else if (GetAcqStatusIndexError(pimega) != false) {
        UPDATEIOCSTATUS("Index error");
        setIntegerParam(ADStatus, ADStatusError);
      }
    }
    /* Call the callbacks to update any changes */
    callParamCallbacks();
  }
}

void pimegaDetector::finishAcq(int trigger, int *acquire, int *acquireStatus,
                               uint64_t *recievedBackendCountOffset,
                               int numExposuresVar) {
  switch (trigger) {
    case IOC_TRIGGER_MODE_ALIGNMENT:
      configureAlignment(false);
      PIMEGA_PRINT(pimega, TRACE_MASK_FLOW, "%s: Alignment stopped\n",
                   __func__);
      UPDATEIOCSTATUS("Alignment stopped");
      break;

    default:
      PIMEGA_PRINT(pimega, TRACE_MASK_FLOW, "%s: Acquisition finished\n",
                   __func__);
      UPDATEIOCSTATUS("Acquisition finished");
      *recievedBackendCountOffset += numExposuresVar;

      *acquire = 0;
      setIntegerParam(ADAcquire, 0);
      *acquireStatus = 0;
      setIntegerParam(ADStatus, ADStatusIdle);
      break;
  }
}

static void captureTaskC(void *drvPvt) {
  pimegaDetector *pPvt = (pimegaDetector *)drvPvt;
  pPvt->captureTask();
}

void pimegaDetector::captureTask() {
  int i, status, adstatus, received_acq, autoSave, indexEnable;
  bool indexEnableBool, moduleError;
  int capture = 0;
  int eventStatus = 0;
  uint64_t prevAcquisitionCount = 0;
  uint64_t previousReceivedCount = 0;
  uint64_t recievedBackendCount, processedBackendCount;

  this->lock();
  /* Loop forever */
  while (true) {
    if (!capture) {
      // Release the lock while we wait for an event that says acquire has
      // started, then lock again
      PIMEGA_PRINT(pimega, TRACE_MASK_FLOW,
                   "%s: Waiting for capture to start\n", __func__);
      this->unlock();
      status = epicsEventWait(startCaptureEventId_);
      this->lock();
      PIMEGA_PRINT(pimega, TRACE_MASK_FLOW, "%s: Capture started\n", __func__);

      prevAcquisitionCount = 0;
      recievedBackendCountOffset = 0;
      previousReceivedCount = 0;
      capture = 1;
    }

    eventStatus = epicsEventTryWait(this->stopCaptureEventId_);

    /* Stop event detected */
    if (eventStatus == epicsEventWaitOK) {
      PIMEGA_PRINT(pimega, TRACE_MASK_FLOW,
                   "%s: Capture Stop request received in thread\n", __func__);
      stop_acquire(pimega);
      status = send_stopAcquire_to_backend(pimega);
      status |= abort_save(pimega);
      int counter = -1;

      this->unlock();
      while (counter != 0) {
        get_acqStatus_from_backend(pimega);
        counter = (int)GetAcqStatusSavedFramesCount(pimega);
        usleep(1000);
      }
      this->lock();

      if (status != 0) {
        PIMEGA_PRINT(pimega, TRACE_MASK_ERROR, "%s: Failed - %s\n",
                     "send_stopAcquire_to_backend", this->error_str);
        UPDATESERVERSTATUS(this->error_str);
        this->error_str[0] = '\0';
      } else {
        capture = 0;
        UPDATESERVERSTATUS("Backend stopped");
        continue;
      }
    }

    /* Added this delay for the thread not to hog the processor. */
    this->unlock();
    usleep(1000);

    if (capture) {
      get_acqStatus_from_backend(pimega);
      moduleError = false;
      recievedBackendCount = UINT64_MAX;
      moduleError |= GetAcqStatusModuleError(pimega, 0);
      recievedBackendCount = 0;
      processedBackendCount = GetAcqStatusProcessedImageNum(pimega);
      /*Anamoly detection. Upon incorrect configuration the detector, a number
        of images larger that what has been requested may arrive. In that case,
        to establish the end of the capture, an upper bound
        GetAcqParamCameraNumCapture(pimega) is set for recievedBackendCount*/
      if (GetAcqParamCameraNumCapture(pimega) != 0 &&
          recievedBackendCount >
              (unsigned int)GetAcqParamCameraNumCapture(pimega)) {
        recievedBackendCount =
            (unsigned int)GetAcqParamCameraNumCapture(pimega);
      }
    }
    getParameter(NDAutoSave, &autoSave);
    getIntegerParam(PimegaIndexEnable, &indexEnable);
    indexEnableBool = (bool)indexEnable;
    /* Capture and server status message management ( UPDATESERVERSTATUS &&
       NDFileCapture handling )
        - The number of the frontend images may or may not be the same as the
       number configured to the backend.
        - Capture should go down only after the number of captures of the
       backend arrived and if save is enabled, saved too.
        - if Backend capture number is configured to 0, the capture will never
       go to 0 unless forced to 0 by the user

        backendStatus != 0 permits that the thread executes this snippet the
       last time when the NDFileCapture is set to 0 */

    received_acq = 0;
    for (int module = 1; module <= GetTotalModules(pimega); module++) {
      if (received_acq == 0 || (int)GetAcqStatusAcquisitionCount(
                                   pimega, module - 1) > received_acq) {
        received_acq = (int)GetAcqStatusAcquisitionCount(pimega, module - 1);
        if (received_acq < GetAcqStatusProcessedImageNum(pimega)) {
          received_acq = GetAcqStatusProcessedImageNum(pimega);
        }
      }
    }

    this->lock();
    if (GetAcqParamCameraNumCapture(pimega) != 0 && capture) {
      /* Timer finished and data should have arrived already ( but not
       * necessarily saved ) */
      getIntegerParam(ADStatus, &adstatus);
      if (adstatus == ADStatusAborted) {
        UPDATESERVERSTATUS("Aborted");
      } else if (received_acq < (int)GetAcqParamCameraNumCapture(pimega)) {
        UPDATESERVERSTATUS("Waiting for images");
      } else if (autoSave == 1 && GetAcqStatusDone(pimega) != DONE_ACQ) {
        UPDATESERVERSTATUS("Saving");
      } else if (GetAcqStatusProcessedImageNum(pimega) <
                 (int)GetAcqParamCameraNumCapture(pimega)) {
        UPDATESERVERSTATUS("Processing images");
      } else {
        setParameter(NDFileCapture, 0);
        capture = 0;
        setDoubleParam(ADTimeRemaining, 0);
        PIMEGA_PRINT(pimega, TRACE_MASK_FLOW, "%s: Backend finished\n",
                     __func__);
        UPDATEIOCSTATUS("Acquisition finished");
        UPDATESERVERSTATUS("Backend done");
        callParamCallbacks();
      }
    } else {
      UPDATESERVERSTATUS("Receiving images");
    }
    /* Errors reported by backend override previous messages. */
    if (moduleError != false) {
      UPDATESERVERSTATUS("Detector dropped frames");
    } else if (GetAcqStatusIndexError(pimega) != false) {
      UPDATESERVERSTATUS("Index not responding");
    }
  }
}

void pimegaDetector::updateIOCStatus(const char *message, int size) {
  epicsInt8 *array = (epicsInt8 *)message;
  doCallbacksInt8Array(array, size, PimegaIOCStatusMessage, 0);
}

void pimegaDetector::updateServerStatus(const char *message, int size) {
  epicsInt8 *array = (epicsInt8 *)message;
  doCallbacksInt8Array(array, size, PimegaServerStatusMessage, 0);
}

asynStatus pimegaDetector::writeInt32(asynUser *pasynUser, epicsInt32 value) {
  int function = pasynUser->reason;
  int status = asynSuccess;
  static const char *functionName = "writeInt32";
  const char *paramName;

  char ok_str[100] = "";
  int adstatus, backendStatus, acquireRunning;
  // int acquiring;
  getParamName(function, &paramName);
  PIMEGA_PRINT(pimega, TRACE_MASK_FLOW, "%s: %s(%d) requested value %d\n",
               functionName, paramName, function, value);

  /* Ensure that ADStatus is set correctly before we set ADAcquire.*/
  getIntegerParam(ADStatus, &adstatus);
  getParameter(NDFileCapture, &backendStatus);
  getParameter(ADAcquire, &acquireRunning);

  createParam(pimegaTraceMaskWarningString, asynParamInt32,
              &PimegaTraceMaskWarning);
  createParam(pimegaTraceMaskErrorString, asynParamInt32,
              &PimegaTraceMaskError);
  createParam(pimegaTraceMaskDriverIOString, asynParamInt32,
              &PimegaTraceMaskDriverIO);
  createParam(pimegaTraceMaskFlowString, asynParamInt32, &PimegaTraceMaskFlow);
  createParam(pimegaTraceMaskString, asynParamInt32, &PimegaTraceMask);

  if (function == PimegaTraceMaskWarning) {
    set_individual_trace_mask(pimega, TRACE_MASK_WARNING, value);
  } else if (function == PimegaTraceMaskError) {
    set_individual_trace_mask(pimega, TRACE_MASK_ERROR, value);
  } else if (function == PimegaTraceMaskDriverIO) {
    set_individual_trace_mask(pimega, TRACE_MASK_DRIVERIO, value);
  } else if (function == PimegaTraceMaskFlow) {
    set_individual_trace_mask(pimega, TRACE_MASK_FLOW, value);
  } else if (function == PimegaTraceMask) {
    set_trace_mask(pimega, value);
  } else if (function == ADAcquire) {
    if (value && backendStatus &&
        (adstatus == ADStatusIdle || adstatus == ADStatusAborted)) {
      /* Send an event to wake up the acq task.  */
      PIMEGA_PRINT(pimega, TRACE_MASK_FLOW,
                   "%s: Requested acquire start event. Sending acquire start "
                   "event signal to thread\n",
                   functionName);
      epicsEventSignal(this->startAcquireEventId_);
      strcat(ok_str, "Acquiring");
    } else if (!value &&
               (adstatus == ADStatusAcquire || adstatus == ADStatusError)) {
      /* This was a command to stop acquisition */
      PIMEGA_PRINT(pimega, TRACE_MASK_FLOW,
                   "%s: Requested acquire stop event. Sending acquire stop "
                   "event signal to thread\n",
                   functionName);
      epicsEventSignal(this->stopAcquireEventId_);
      epicsThreadSleep(.1);
      strcat(ok_str, "Stopping acquisition");
    } else {
      PIMEGA_PRINT(pimega, TRACE_MASK_ERROR,
                   "%s: value=%d, adstatus=%s(%d), backendStatus=%d\n",
                   functionName, value,
                   adstatus == ADStatusIdle      ? "ADStatusIdle"
                   : adstatus == ADStatusError   ? "ADStatusError"
                   : adstatus == ADStatusAborted ? "ADStatusAborted"
                   : adstatus == ADStatusAcquire ? "ADStatusAcquire"
                                                 : "adstatus not known",
                   adstatus, backendStatus);
      status = asynError;
      if (value) {
        strncpy(this->error_str, "Cannot start", strlen(this->error_str));
      } else {
        strncpy(this->error_str, "Already stopped", strlen(this->error_str));
      }
    }
  }

  else if (function == NDFileCapture) {
    if (value) {
      if (acquireRunning == 0 && backendStatus == 0) {
        PIMEGA_PRINT(pimega, TRACE_MASK_FLOW,
                     "%s: Requested capture start event. Sending capture start "
                     "event signal to thread\n",
                     functionName);
        status = startCaptureBackend();

        if (status == PIMEGA_SUCCESS) {
          epicsEventSignal(this->startCaptureEventId_);
          strcat(ok_str, "Started backend");
        } else {
          PIMEGA_PRINT(pimega, TRACE_MASK_ERROR,
                       "%s: startCaptureBackend failed. Sending asynError\n",
                       functionName);
          status = asynError;
        }
      } else {
        if (acquireRunning == 1) {
          PIMEGA_PRINT(pimega, TRACE_MASK_ERROR,
                       "%s: Detector acquisition running. Will not start a new "
                       "backend capture. Sending asynError\n",
                       functionName);
          strncpy(this->error_str, "Stop current acquisition first",
                  strlen(this->error_str));
          status = asynError;
        } else {
          PIMEGA_PRINT(pimega, TRACE_MASK_ERROR,
                       "%s: Backend already running. Will not start a new "
                       "backend capture. Sending asynError\n",
                       functionName);
          strncpy(this->error_str, "Stop current acquisition first",
                  strlen(this->error_str));
          status = asynError;
        }
      }
    }
    if (!value) {
      if (backendStatus == 1) {
        PIMEGA_PRINT(pimega, TRACE_MASK_FLOW,
                     "%s: Requested capture stop event. Sending capture stop "
                     "event signal to thread\n",
                     functionName);
        epicsEventSignal(this->stopCaptureEventId_);
        epicsThreadSleep(.1);
        setDoubleParam(ADTimeRemaining, 0);
        strcat(ok_str, "Acquisition stopped");
      } else {
        PIMEGA_PRINT(pimega, TRACE_MASK_ERROR,
                     "%s: Backend already stopped. Sending asynError\n",
                     functionName);
        strncpy(this->error_str, "Backend already stopped",
                strlen(this->error_str));
        strcat(ok_str, "Backend already stopped");
      }
    }
  } else if (acquireRunning == 1) {
    strncpy(this->error_str, "Stop current acquisition first",
            strlen(this->error_str));
    status = asynError;
  } else if (function == PimegaSendImage) {
    UPDATEIOCSTATUS("Sending Images");
    if (value) status |= sendImage();
    strcat(ok_str, "Sending image done");
  } else if (function == PimegaLoadEqStart) {
    UPDATEIOCSTATUS("Equalizing. Please Wait");
    if (value) status |= loadEqualization();
    strcat(ok_str, "Equalization Finished");
  } else if (function == PimegaCheckSensors) {
    UPDATEIOCSTATUS("Checking sensors. Please Wait");
    if (value) status |= checkSensors();
    strcat(ok_str, "Sensors checked");
  } else if (function == PimegaOmrOPMode) {
    status |= setOMRValue(OMR_M, value, function);
    strcat(ok_str, "OMR value set");
  } else if (function == ADNumExposures) {
    status |= numExposures(value);
    strcat(ok_str, "Exposures # set");
  } else if (function == PimegaReset) {
    UPDATEIOCSTATUS("Reseting. Please wait");
    status |= reset(value);
    strcat(ok_str, "Reset done");
  } else if (function == PimegaMedipixMode) {
    status |= medipixMode(value);
    strcat(ok_str, "Medipix mode set");
  } else if (function == PimegaModule) {
    status |= selectModule(value);
    strcat(ok_str, "Module selected");
  } else if (function == ADTriggerMode) {
    status |= triggerMode((enum ioc_trigger_mode_t)value);
    strcat(ok_str, "Trigger mode set");
  } else if (function == PimegaConfigDiscL) {
    UPDATEIOCSTATUS("Setting ConfigDiscL value");
    status |= configDiscL(value);
    strcat(ok_str, "ConfigDiscL set");
  } else if (function == PimegaMedipixBoard) {
    status |= medipixBoard(value);
    strcat(ok_str, "ConfigDiscL set");
  } else if (function == PimegaMedipixChip) {
    if (value < 1) {
      error("Invalid value: %d\n", value);
      return asynError;
    }
    status |= imgChipID(value);
    strcat(ok_str, "Chip selected");
  } else if (function == PimegaPixelMode) {
    status |= setOMRValue(OMR_CSM_SPM, value, function);
    strcat(ok_str, "Pixel mode set");
  } else if (function == PimegaContinuosRW) {
    status |= setOMRValue(OMR_CRW_SRW, value, function);
    strcat(ok_str, "read/write set");
  } else if (function == PimegaPolarity) {
    status |= setOMRValue(OMR_Polarity, value, function);
    strcat(ok_str, "Polarity set");
  } else if (function == PimegaDiscriminator) {
    status |= setOMRValue(OMR_Disc_CSM_SPM, value, function);
    strcat(ok_str, "Discriminator set");
  } else if (function == PimegaTestPulse) {
    status |= setOMRValue(OMR_EnableTP, value, function);
    strcat(ok_str, "Test pulse set");
  } else if (function == PimegaCounterDepth) {
    status |= setOMRValue(OMR_CountL, value, function);
    strcat(ok_str, "Counter depth set");
  } else if (function == PimegaEqualization) {
    status |= setOMRValue(OMR_Equalization, value, function);
    strcat(ok_str, "Equalization set");
  } else if (function == PimegaGain) {
    status |= setOMRValue(OMR_Gain_Mode, value, function);
    strcat(ok_str, "Gain set");
  } else if (function == PimegaAllModules) {
    if ((value < PIMEGA_SEND_ONE_CHIP_ONE_MODULE) ||
        (value > PIMEGA_SEND_ALL_CHIPS_ALL_MODULES)) {
      error("Invalid value: %d\n", value);
      return asynError;
    }
  } else if (function == PimegaMBSendMode) {
    if ((value < 0) || (value > 4)) {
      error("Invalid value: %d\n", value);
      return asynError;
    }
  } else if (function == PimegaExtBgSel) {
    status |= setOMRValue(OMR_Ext_BG_Sel, value, function);
    strcat(ok_str, "BG select set");
  } else if (function == PimegaReadCounter) {
    status |= readCounter(value);
    strcat(ok_str, "Read counter set");
  } else if (function == PimegaSenseDacSel) {
    status |= senseDacSel(value);
    strcat(ok_str, "Sense DAC set");
  }
  // DACS functions
  else if (function == PimegaCas) {
    status |= setDACValue(DAC_CAS, value, function);
    strcat(ok_str, "DAC CAS set");
  } else if (function == PimegaDelay) {
    status |= setDACValue(DAC_Delay, value, function);
    strcat(ok_str, "DAC Delay set");
  } else if (function == PimegaDisc) {
    status |= setDACValue(DAC_Disc, value, function);
    strcat(ok_str, "DAC Disc set");
  } else if (function == PimegaDiscH) {
    status |= setDACValue(DAC_DiscH, value, function);
    strcat(ok_str, "DAC DiscH set");
  } else if (function == PimegaDiscL) {
    status |= setDACValue(DAC_DiscL, value, function);
    strcat(ok_str, "DAC DiscL set");
  } else if (function == PimegaDiscLS) {
    status |= setDACValue(DAC_DiscLS, value, function);
    strcat(ok_str, "DAC DiscLS set");
  } else if (function == PimegaFbk) {
    status |= setDACValue(DAC_FBK, value, function);
    strcat(ok_str, "DAC FBK set");
  } else if (function == PimegaGnd) {
    status |= setDACValue(DAC_GND, value, function);
    strcat(ok_str, "DAC GND set");
  } else if (function == PimegaIkrum) {
    status |= setDACValue(DAC_IKrum, value, function);
    strcat(ok_str, "DAC IKrum set");
  } else if (function == PimegaPreamp) {
    status |= setDACValue(DAC_Preamp, value, function);
    strcat(ok_str, "DAC Preamp set");
  } else if (function == PimegaRpz) {
    status |= setDACValue(DAC_RPZ, value, function);
    strcat(ok_str, "DAC RPZ set");
  } else if (function == PimegaShaper) {
    status |= setDACValue(DAC_Shaper, value, function);
    strcat(ok_str, "DAC Shaper set");
  } else if (function == PimegaThreshold0) {
    status |= setDACValue(DAC_ThresholdEnergy0, value, function);
    strcat(ok_str, "DAC TH0 set");
  } else if (function == PimegaThreshold1) {
    status |= setDACValue(DAC_ThresholdEnergy1, value, function);
    strcat(ok_str, "DAC TH1 set");
  } else if (function == PimegaTpBufferIn) {
    status |= setDACValue(DAC_TPBufferIn, value, function);
    strcat(ok_str, "DAC TPBufferIn set");
  } else if (function == PimegaTpBufferOut) {
    status |= setDACValue(DAC_TPBufferOut, value, function);
    strcat(ok_str, "DAC TPBufferOut set");
  } else if (function == PimegaTpRef) {
    status |= setDACValue(DAC_TPRef, value, function);
    strcat(ok_str, "DAC TPRef set");
  } else if (function == PimegaTpRefA) {
    status |= setDACValue(DAC_TPRefA, value, function);
    strcat(ok_str, "DAC TPRefA set");
  } else if (function == PimegaTpRefB) {
    status |= setDACValue(DAC_TPRefB, value, function);
    strcat(ok_str, "DAC TPRefB set");
  } else if (function == PimegaReadMBTemperature) {
    if (!value) {
      UPDATEIOCSTATUS("Reading MB temperatures");
      status |= getMbTemperature();
      strcat(ok_str, "MB temperatures fetched");
    }
  } else if (function == PimegaTempMonitorEnable) {
    status |= setTempMonitor(value);
    strcat(ok_str, "Temperature Monitor enable set");
  } else if (function == PimegaReadSensorTemperature) {
    if (!value) {
      UPDATEIOCSTATUS("Reading sensors temperatures");
      status |= getMedipixTemperatures();
      strcat(ok_str, "Sensor temperatures fetched");
    }
  } else if (function == PimegaMetadataOM) {
    status |= metadataHandler(value);
    strcat(ok_str, "Metadata OP mode performed");
  } else if (function == PimegaFrameProcessMode) {
    setParameter(function, value);
    strcat(ok_str, "Frame process mode set");
  } else if (function == PimegaDacCountScan) {
    UPDATEIOCSTATUS("Running DAC Count Scan");
    status |= dacCountScan();
    strcat(ok_str, "DAC Count Scan done");
  } else if (function == PimegaDiagnostic) {
    UPDATEIOCSTATUS("Performing diagnostic");
    status |= diagnostic();
    strcat(ok_str, "Diagnostic performed");
  } else {
    if (function < FIRST_PIMEGA_PARAM) {
      status = ADDriver::writeInt32(pasynUser, value);
      strcat(ok_str, paramName);
      strcat(ok_str, " OK");
    }
  }

  if (status) {
    PIMEGA_PRINT(pimega, TRACE_MASK_ERROR,
                 "%s: Failed - status=%d function=%s(%d), value=%d - %s\n",
                 __func__, status, paramName, function, value, this->error_str);
    UPDATEIOCSTATUS(this->error_str);
    this->error_str[0] = '\0';
  } else {
    /* Set the parameter and readback in the parameter library.  This may be
     * overwritten when we read back the status at the end, but that's OK */
    setIntegerParam(function, value);
    callParamCallbacks();
    PIMEGA_PRINT(pimega, TRACE_MASK_FLOW,
                 "%s: Success - status=%d function=%s(%d), value=%d\n",
                 functionName, status, paramName, function, value);
    UPDATEIOCSTATUS(ok_str);
  }
  return (asynStatus)status;
}

asynStatus pimegaDetector::writeInt32Array(asynUser *pasynUser,
                                           epicsInt32 *value,
                                           size_t nElements) {
  int function = pasynUser->reason;
  size_t i;
  int status = asynSuccess;
  const char *paramName;
  char ok_str[100] = "";

  getParamName(function, &paramName);
  PIMEGA_PRINT(pimega, TRACE_MASK_FLOW,
               "writeInt32Array: %s(%d) nElements=%d, requested value [ ",
               paramName, function, nElements, value);
  for (i = 0; i < nElements; i++) printf("%d ", value[i]);
  printf("]\n");

  if (function == PimegaLoadEqualization) {
    status = set_eq_cfg(pimega, (uint32_t *)value, nElements);
    strcat(ok_str, "Equalization string set");
  } else if (function == PimegaDacCountScanChips) {
    if (nElements > MAX_NUM_CHIPS) {
      PIMEGA_PRINT(pimega, TRACE_MASK_ERROR,
                   "writeInt32Array: %s(%d) Array larger than expected. "
                   "Expected nElements < %d but got %d\n",
                   paramName, function, MAX_NUM_CHIPS, nElements);
      return asynError;
    }

    for (int index = 0; index < nElements && value[index] > 0; index++) {
      PimegaDacCountScanSelectedChips_.push_back(value[index]);
    }
  } else if (function < FIRST_PIMEGA_PARAM) {
    status = ADDriver::writeInt32Array(pasynUser, value, nElements);
    strcat(ok_str, paramName);
    strcat(ok_str, " OK");
  }

  if (status) {
    char err[100] = "Error setting ";
    strcat(err, paramName);
    UPDATEIOCSTATUS(err);
    PIMEGA_PRINT(pimega, TRACE_MASK_ERROR,
                 "%s: Failed - status=%d function=%s(%d), nElements=%d, value=",
                 "writeInt32Array", status, paramName, function, nElements);
    for (i = 0; i < nElements; i++) printf("%d ", value[i]);
    printf("]\n");
  } else {
    doCallbacksInt32Array(value, nElements, function, 0);
    UPDATEIOCSTATUS(ok_str);
    PIMEGA_PRINT(
        pimega, TRACE_MASK_FLOW,
        "%s: Success - status=%d function=%s(%d), nElements=%d, value=[ ",
        "writeInt32Array", status, paramName, function, nElements);
    for (i = 0; i < nElements; i++) printf("%d ", value[i]);
    printf("]\n");
  }
  return ((asynStatus)status);
}

asynStatus pimegaDetector::writeOctet(asynUser *pasynUser, const char *value,
                                      size_t maxChars, size_t *nActual) {
  int function = pasynUser->reason;
  int status = asynSuccess, acquireRunning;
  const char *paramName;
  char ok_str[100] = "";
  getParamName(function, &paramName);

  PIMEGA_PRINT(pimega, TRACE_MASK_FLOW,
               "writeOctet: %s(%d) requested value %s\n", paramName, function,
               value);

  getParameter(ADAcquire, &acquireRunning);
  if (acquireRunning == 1) {
    strncpy(this->error_str, "Stop current acquisition first",
            strlen(this->error_str));
    status = asynError;
  } else if (function == pimegaDacDefaults) {
    *nActual = maxChars;
    UPDATEIOCSTATUS("Setting DACs");
    status = dacDefaults(value);
    strcat(ok_str, "Setting DACs done");
  } else if (function == PimegaIndexID) {
    *nActual = maxChars;
    setParameter(function, value);
    strcat(ok_str, "Index ID set");
  } else if (function == PimegaMetadataField) {
    *nActual = maxChars;
    setParameter(function, value);
    strcat(ok_str, "Metadata Field set");
  } else if (function == PimegaMetadataValue) {
    *nActual = maxChars;
    setParameter(function, value);
    strcat(ok_str, "Metadata Value set");
  } else if (function == PimegaDiagnosticDir) {
    *nActual = maxChars;
    setParameter(function, value);
    strcat(ok_str, "Diagnostic directory set");
  } else if (function == PimegaDiagnosticSysInfoID) {
    *nActual = maxChars;
    setParameter(function, value);
    strcat(ok_str, "Diagnostic sysinfoID set");
  } else {
    /* If this parameter belongs to a base class call its method */
    if (function < FIRST_PIMEGA_PARAM) {
      status = ADDriver::writeOctet(pasynUser, value, maxChars, nActual);
      strcat(ok_str, paramName);
      strcat(ok_str, " OK");
    }
  }

  if (status) {
    PIMEGA_PRINT(pimega, TRACE_MASK_ERROR,
                 "%s: Failed - status=%d function=%s(%d), value=%s - %s\n",
                 __func__, status, paramName, function, value, this->error_str);
    UPDATEIOCSTATUS(this->error_str);
    this->error_str[0] = '\0';
  } else {
    /* Do callbacks so higher layers see any changes */
    callParamCallbacks();
    PIMEGA_PRINT(pimega, TRACE_MASK_FLOW,
                 "%s: Success - status=%d function=%s(%d), value=%s\n",
                 "writeOctet", status, paramName, function, value);

    UPDATEIOCSTATUS(ok_str);
  }

  return ((asynStatus)status);
}

asynStatus pimegaDetector::dacDefaults(const char *file) {
  int rc = 0;

  rc = configure_module_dacs_with_file(pimega, file);
  if (rc != PIMEGA_SUCCESS) {
    error("Invalid value: %s\n", pimega_error_string(rc));
    return asynError;
  }
  setParameter(pimegaDacDefaults, file);
  return asynSuccess;
}

asynStatus pimegaDetector::writeFloat64(asynUser *pasynUser,
                                        epicsFloat64 value) {
  int function = pasynUser->reason;
  int status = asynSuccess, acquireRunning;
  const char *paramName;
  char ok_str[100] = "";
  getParamName(function, &paramName);
  static const char *functionName = "writeFloat64";
  PIMEGA_PRINT(pimega, TRACE_MASK_FLOW, "%s: %s(%d) requested value %f\n",
               functionName, paramName, function, value);

  getParameter(ADAcquire, &acquireRunning);
  if (acquireRunning == 1) {
    strncpy(this->error_str, "Stop current acquisition first",
            strlen(this->error_str));
    status = asynError;
  } else if (function == ADAcquireTime) {
    status |= acqTime(value);
    strcat(ok_str, "Exposure time set");
  } else if (function == PimegaDistance) {
    UPDATEIOCSTATUS("Adjusting sample distance");
    setParameter(PimegaDistance, value);
    strcat(ok_str, "Distance set");
  } else if (function == ADAcquirePeriod) {
    status |= acqPeriod(value);
    strcat(ok_str, "Acquire period set");
  }

  else if (function == PimegaSensorBias) {
    UPDATEIOCSTATUS("Adjusting sensor bias");
    status |= sensorBias(value);
    strcat(ok_str, "Sensor bias set");
  } else if (function == PimegaExtBgIn) {
    UPDATEIOCSTATUS("Adjusting bandgap");
    status |= setExtBgIn(value);
    strcat(ok_str, "Bandgap set");
  } else if (function == PimegaEnergy) {
    UPDATEIOCSTATUS("Setting Energy");
    status |= setThresholdEnergy(value);
    strcat(ok_str, "Energy set");
  } else {
    /* If this parameter belongs to a base class call its method */
    if (function < FIRST_PIMEGA_PARAM) {
      status = ADDriver::writeFloat64(pasynUser, value);
      strcat(ok_str, paramName);
      strcat(ok_str, " OK");
    }
  }

  if (status) {
    PIMEGA_PRINT(pimega, TRACE_MASK_ERROR,
                 "%s: Failed - status=%d function=%s(%d), value=%f - %s\n",
                 functionName, status, paramName, function, value,
                 this->error_str);
    UPDATEIOCSTATUS(this->error_str);
    this->error_str[0] = '\0';
  } else {
    /* Do callbacks so higher layers see any changes */
    callParamCallbacks();
    PIMEGA_PRINT(pimega, TRACE_MASK_FLOW,
                 "%s: Success - status=%d function=%s(%d), value=%f\n",
                 functionName, status, paramName, function, value);

    UPDATEIOCSTATUS(ok_str);
  }

  return ((asynStatus)status);
}

asynStatus pimegaDetector::readFloat32Array(asynUser *pasynUser,
                                            epicsFloat32 *value,
                                            size_t nElements, size_t *nIn) {
  int function = pasynUser->reason;
  int addr, status;
  const char *paramName;
  int numPoints = 0, acquireRunning;
  epicsFloat32 *inPtr;
  // const char *paramName;
  static const char *functionName = "readFloat32Array";
  getParamName(function, &paramName);
  this->getAddress(pasynUser, &addr);

  getParameter(ADAcquire, &acquireRunning);
  /*if (acquireRunning == 1)
  {
      strncpy(this->error_str, "Stop current acquisition first", sizeof("Stop
  current acquisition first")); UPDATEIOCSTATUS(this->error_str);
  this->error_str[0] = '\0'; return asynError;
  }
  else */
  if (function == PimegaDacsOutSense) {
    inPtr = PimegaDacsOutSense_;
    numPoints = N_DACS_OUTS;
  }

  // Other functions we call the base class method
  else {
    status = asynPortDriver::readFloat32Array(pasynUser, value, nElements, nIn);
  }

  if (status == 0) {
    *nIn = nElements;
    if (*nIn > (size_t)numPoints) *nIn = (size_t)numPoints;
    memcpy(value, inPtr, *nIn * sizeof(epicsFloat32));
    return asynSuccess;
  } else {
    PIMEGA_PRINT(pimega, TRACE_MASK_ERROR,
                 "%s: Failed - status=%d function=%s(%d), value=%f\n",
                 functionName, status, paramName, function, value);
    UPDATEIOCSTATUS(this->error_str);
    this->error_str[0] = '\0';
    return asynError;
  }

  return asynSuccess;
}

asynStatus pimegaDetector::readFloat64(asynUser *pasynUser,
                                       epicsFloat64 *value) {
  int function = pasynUser->reason;
  int status = 0;
  const char *paramName;
  // static const char *functionName = "readFloat64";
  double temp = 0;
  int scanStatus, i, acquireRunning;
  getParamName(function, &paramName);
  static const char *functionName = "readFloat64";
  getParameter(ADStatus, &scanStatus);

  // if (function == ADTemperatureActual) {
  //    status = US_TemperatureActual(pimega);
  //    setParameter(ADTemperatureActual,
  //    pimega->cached_result.actual_temperature);
  //}
  getParameter(ADAcquire, &acquireRunning);

  if (function == PimegaBackBuffer) {
    *value = GetAcqStatusBufferUsage(pimega, 0) * 100;
  }

  else if (function == PimegaDacOutSense) {
    if (acquireRunning == 1) {
      strncpy(this->error_str, "Stop current acquisition first",
              strlen(this->error_str));
      status = asynError;
    } else {
      status = get_dac_out_sense(pimega);
      *value = GetCurrentDACOutSense(pimega);
    }
  }

  // Other functions we call the base class method
  else {
    status = asynPortDriver::readFloat64(pasynUser, value);
  }
  if (status == 0) {
    return asynSuccess;
  } else {
    PIMEGA_PRINT(pimega, TRACE_MASK_ERROR,
                 "%s: Failed - status=%d function=%s(%d), value=%f - %s\n",
                 functionName, status, paramName, function, value,
                 this->error_str);
    return asynError;
  }
}

asynStatus pimegaDetector::readInt32(asynUser *pasynUser, epicsInt32 *value) {
  int function = pasynUser->reason;
  int status = 0;
  int scanStatus, i, acquireRunning, autoSave, received_acq;
  uint64_t temp = ULLONG_MAX;
  uint64_t temp_proc = ULLONG_MAX;
  uint64_t temp_saved = ULLONG_MAX;
  int backendStatus;
  const char *paramName;
  int error;
  getParamName(function, &paramName);

  getParameter(ADStatus, &scanStatus);
  getParameter(NDFileCapture, &backendStatus);
  getParameter(ADAcquire, &acquireRunning);
  getParameter(NDAutoSave, &autoSave);

  if (function == PimegaBackendStats) {
    if (GetAcqStatusModuleError(pimega, 0) == 1 ||
        GetAcqStatusModuleError(pimega, 1) == 1 ||
        GetAcqStatusModuleError(pimega, 2) == 1 ||
        GetAcqStatusModuleError(pimega, 3) == 1)
      error = 1;
    else
      error = 0;

    received_acq = 0;
    for (int module = 1; module <= GetTotalModules(pimega); module++) {
      if (received_acq == 0 || (int)GetAcqStatusAcquisitionCount(
                                   pimega, module - 1) > received_acq) {
        received_acq = (int)GetAcqStatusAcquisitionCount(pimega, module - 1);
        if (received_acq < GetAcqStatusProcessedImageNum(pimega)) {
          received_acq = GetAcqStatusProcessedImageNum(pimega);
        }
      }
    }

    setParameter(PimegaReceiveError, error);
    setParameter(PimegaM1ReceiveError, (int)GetAcqStatusModuleError(pimega, 0));
    setParameter(PimegaM2ReceiveError, (int)GetAcqStatusModuleError(pimega, 1));
    setParameter(PimegaM3ReceiveError, (int)GetAcqStatusModuleError(pimega, 2));
    setParameter(PimegaM4ReceiveError, (int)GetAcqStatusModuleError(pimega, 3));
    setParameter(PimegaM1LostFrameCount,
                 (int)GetAcqStatusLostFrameCount(pimega, 0));
    setParameter(PimegaM2LostFrameCount,
                 (int)GetAcqStatusLostFrameCount(pimega, 1));
    setParameter(PimegaM3LostFrameCount,
                 (int)GetAcqStatusLostFrameCount(pimega, 2));
    setParameter(PimegaM4LostFrameCount,
                 (int)GetAcqStatusLostFrameCount(pimega, 3));
    setParameter(PimegaM1RxFrameCount,
                 (int)GetAcqStatusFramesReceived(pimega, 0));
    setParameter(PimegaM2RxFrameCount,
                 (int)GetAcqStatusFramesReceived(pimega, 1));
    setParameter(PimegaM3RxFrameCount,
                 (int)GetAcqStatusFramesReceived(pimega, 2));
    setParameter(PimegaM4RxFrameCount,
                 (int)GetAcqStatusFramesReceived(pimega, 3));
    setParameter(PimegaM1AquisitionCount,
                 (int)GetAcqStatusAcquisitionCount(pimega, 0));
    setParameter(PimegaM2AquisitionCount,
                 (int)GetAcqStatusAcquisitionCount(pimega, 1));
    setParameter(PimegaM3AquisitionCount,
                 (int)GetAcqStatusAcquisitionCount(pimega, 2));
    setParameter(PimegaM4AquisitionCount,
                 (int)GetAcqStatusAcquisitionCount(pimega, 3));
    setParameter(PimegaM1RdmaBufferUsage,
                 (double)GetAcqStatusBufferUsage(pimega, 0) * 100);
    setParameter(PimegaM2RdmaBufferUsage,
                 (double)GetAcqStatusBufferUsage(pimega, 1) * 100);
    setParameter(PimegaM3RdmaBufferUsage,
                 (double)GetAcqStatusBufferUsage(pimega, 2) * 100);
    setParameter(PimegaM4RdmaBufferUsage,
                 (double)GetAcqStatusBufferUsage(pimega, 3) * 100);
    setParameter(PimegaIndexError, (int)GetAcqStatusIndexError(pimega));
    setParameter(PimegaIndexCounter,
                 (int)GetAcqStatusSentAcquisitionsCount(pimega));
    setParameter(ADNumImagesCounter, received_acq);
    setParameter(PimegaProcessedImageCounter,
                 GetAcqStatusProcessedImageNum(pimega));
    setParameter(NDFileNumCaptured, (int)GetAcqStatusSavedFramesCount(pimega));
    callParamCallbacks();
  } else if (function == PimegaModule) {
    *value = get_selected_module(pimega);
  }
  // Other functions we call the base class method
  else {
    status = asynPortDriver::readInt32(pasynUser, value);
  }
  return (status == 0) ? asynSuccess : asynError;
}

/** Configuration command for Pimega driver; creates a new Pimega object.
 * \param[in] portName The name of the asyn port driver to be created.
 * \param[in] CommandPort The asyn network port connection to the Pimega
 * \param[in] maxBuffers The maximum number of NDArray buffers that the
 * NDArrayPool for this driver is allowed to allocate. Set this to -1 to allow
 * an unlimited number of buffers. \param[in] maxMemory The maximum amount of
 * memory that the NDArrayPool for this driver is allowed to allocate. Set this
 * to -1 to allow an unlimited amount of memory. \param[in] priority The thread
 * priority for the asyn port driver thread if ASYN_CANBLOCK is set in
 * asynFlags. \param[in] stackSize The stack size for the asyn port driver
 * thread if ASYN_CANBLOCK is set in asynFlags.
 */
extern "C" int pimegaDetectorConfig(
    const char *portName, const char *address_module01,
    const char *address_module02, const char *address_module03,
    const char *address_module04, const char *address_module05,
    const char *address_module06, const char *address_module07,
    const char *address_module08, const char *address_module09,
    const char *address_module10, int port, int maxSizeX, int maxSizeY,
    int detectorModel, int maxBuffers, size_t maxMemory, int priority,
    int stackSize, int simulate, int backendOn, int log,
    unsigned short backend_port, unsigned short vis_frame_port,
    int IntAcqResetRDMA, int numModulesX, int numModulesY) {
  new pimegaDetector(portName, address_module01, address_module02,
                     address_module03, address_module04, address_module05,
                     address_module06, address_module07, address_module08,
                     address_module09, address_module10, port, maxSizeX,
                     maxSizeY, detectorModel, maxBuffers, maxMemory, priority,
                     stackSize, simulate, backendOn, log, backend_port,
                     vis_frame_port, IntAcqResetRDMA, numModulesX, numModulesY);

  return (asynSuccess);
}

/** Constructor for pimega driver; most parameters are simply passed to
 * ADDriver::ADDriver. After calling the base class constructor this method
 * creates a thread to collect the detector data, and sets reasonable default
 * values for the parameters defined in this class, asynNDArrayDriver, and
 * ADDriver. \param[in] portName The name of the asyn port driver to be created.
 * \param[in] CommandIP The asyn network port connection to the Pimega
 * \param[in] maxSizeX The size of the pimega detector in the X direction.
 * \param[in] maxSizeY The size of the pimega detector in the Y direction.
 * \param[in] portName The name of the asyn port driver to be created.
 * \param[in] maxBuffers The maximum number of NDArray buffers that the
 * NDArrayPool for this driver is allowed to allocate. Set this to -1 to allow
 * an unlimited number of buffers. \param[in] maxMemory The maximum amount of
 * memory that the NDArrayPool for this driver is allowed to allocate. Set this
 * to -1 to allow an unlimited amount of memory. \param[in] priority The thread
 * priority for the asyn port driver thread if ASYN_CANBLOCK is set in
 * asynFlags. \param[in] stackSize The stack size for the asyn port driver
 * thread if ASYN_CANBLOCK is set in asynFlags.
 */
pimegaDetector::pimegaDetector(
    const char *portName, const char *address_module01,
    const char *address_module02, const char *address_module03,
    const char *address_module04, const char *address_module05,
    const char *address_module06, const char *address_module07,
    const char *address_module08, const char *address_module09,
    const char *address_module10, int port, int SizeX, int SizeY,
    int detectorModel, int maxBuffers, size_t maxMemory, int priority,
    int stackSize, int simulate, int backendOn, int log,
    unsigned short backend_port, unsigned short vis_frame_port,
    int IntAcqResetRDMA, int numModulesX, int numModulesY)

    : ADDriver(
          portName, 1, 0, maxBuffers, maxMemory,
          asynInt32ArrayMask | asynFloat64ArrayMask | asynFloat32ArrayMask |
              asynGenericPointerMask | asynInt16ArrayMask | asynInt8ArrayMask,
          asynInt32ArrayMask | asynFloat64ArrayMask | asynFloat32ArrayMask |
              asynGenericPointerMask | asynInt16ArrayMask | asynInt8ArrayMask,
          ASYN_CANBLOCK,
          1, /* ASYN_CANBLOCK=1, ASYN_MULTIDEVICE=0, autoConnect=1 */
          priority, stackSize),

      pollTime_(DEFAULT_POLL_TIME),
      forceCallback_(1)

{
  BoolAcqResetRDMA = (bool)IntAcqResetRDMA;
  int status = asynSuccess;
  const char *functionName = "pimegaDetector::pimegaDetector";
  const char *ips[] = {address_module01, address_module02, address_module03,
                       address_module04, address_module05, address_module06,
                       address_module07, address_module08, address_module09,
                       address_module10};

  numImageSaved = 0;
  // initialize random seed:
  srand(time(NULL));

  // Alocate memory for PimegaDacsOutSense_
  PimegaDacsOutSense_ =
      (epicsFloat32 *)calloc(N_DACS_OUTS, sizeof(epicsFloat32));

  // Alocate memory for PimegaDisabledSensors_
  PimegaDisabledSensors_ = (epicsInt32 *)calloc(36, sizeof(epicsInt32));

  if (simulate == 1)
    printf("Simulation mode activated.\n");
  else
    printf("Simulation mode inactivate.\n");

  // Initialise the debugger
  initDebugger(1);
  debugLevel("all", 1);

  /* Create the epicsEvents for signaling to the simulate task when acquisition
   * starts and stops */
  startAcquireEventId_ = epicsEventCreate(epicsEventEmpty);
  if (!startAcquireEventId_) {
    printf("%s:%s epicsEventCreate failure for acquire start event\n",
           driverName, functionName);
    return;
  }
  stopAcquireEventId_ = epicsEventCreate(epicsEventEmpty);
  if (!stopAcquireEventId_) {
    printf("%s:%s epicsEventCreate failure for acquire stop event\n",
           driverName, functionName);
    return;
  }
  startCaptureEventId_ = epicsEventCreate(epicsEventEmpty);
  if (!startCaptureEventId_) {
    printf("%s:%s epicsEventCreate failure for start capture event\n",
           driverName, functionName);
    return;
  }
  stopCaptureEventId_ = epicsEventCreate(epicsEventEmpty);
  if (!stopCaptureEventId_) {
    printf("%s:%s epicsEventCreate failure for capture stop event\n",
           driverName, functionName);
    return;
  }

  pimega = pimega_new((pimega_detector_model_t)detectorModel, true);
  pimega_global = pimega;
  EnableLog(pimega, log);
  UseBackendOnStart(pimega, (backend_status_t)backendOn);
  trigger_input_cfg = GetTriggerInputConfigPtr(pimega);
  if (log == 1) {
    if (initLog(pimega) == false) {
      PIMEGA_PRINT(pimega, TRACE_MASK_WARNING,
                   "pimegaDetector: Disabling logging\n");
      exit(0);
      EnableLog(pimega, 0);
    } else {
      log_file_path = (char *)malloc(sizeof(char) * 50);
      memset(log_file_path, ' ', 50);
      GetLogFilePath(pimega, log_file_path);
    }
  }
  error_str = ErrorStringPointer(pimega);
  maxSizeX = SizeX;
  maxSizeY = SizeY;
  numModulesX_ = numModulesX;
  numModulesY_ = numModulesY;

  if (pimega)
    PIMEGA_PRINT(pimega, TRACE_MASK_FLOW,
                 "pimegaDetector: Pimega struct created\n");

  SetSimulationFlag(pimega, simulate);
  connect(ips, port, backend_port, vis_frame_port);
  status = prepare_pimega(pimega);
  if (status != PIMEGA_SUCCESS) panic("Unable to prepare pimega. Aborting");

  createParameters();

  setDefaults();

  /* get the MB Hardware version and store it */
  get_MbHwVersion(pimega);

  // Alocate memory for PimegaMBTemperature_
  PimegaMBTemperature_ = (epicsFloat32 *)calloc(
      GetModuleTemperatureSensorCount(pimega), sizeof(epicsFloat32));

  /* Create the thread that runs acquisition */
  status = (epicsThreadCreate("pimegaDetTask", epicsThreadPriorityMedium,
                              epicsThreadGetStackSize(epicsThreadStackMedium),
                              (EPICSTHREADFUNC)acquisitionTaskC, this) == NULL);

  status = (epicsThreadCreate("pimegacaptureTask", epicsThreadPriorityMedium,
                              epicsThreadGetStackSize(epicsThreadStackMedium),
                              (EPICSTHREADFUNC)captureTaskC, this) == NULL);

  status = (epicsThreadCreate("pimegaAlarmTask", epicsThreadPriorityMedium,
                              epicsThreadGetStackSize(epicsThreadStackMedium),
                              (EPICSTHREADFUNC)alarmTaskC, this) == NULL);

  if (status) {
    debug(functionName, "epicsTheadCreate failure for image task");
  }

  define_master_module(
      pimega, GetMasterModule(pimega), false,
      this->trigger_input_cfg->PIMEGA_TRIGGER_IN_EXTERNAL_POS_EDGE);

  /* Reset RDMA logic in the FPGA at initialization */
  send_allinitArgs_allModules(pimega);
}

void pimegaDetector::panic(const char *msg) {
  asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s\n", msg);
  epicsExit(0);
}

void pimegaDetector::connect(const char *address[10], unsigned short port,
                             unsigned short backend_port,
                             unsigned short vis_frame_port) {
  int rc = 0;
  unsigned short ports[10] = {10000, 10001, 10002, 10003, 10004,
                              10005, 10006, 10007, 10008, 10010};

  if (GetSimulationFlag(pimega) == 0)
    ports[0] = ports[1] = ports[2] = ports[3] = ports[4] = ports[5] = ports[6] =
        ports[7] = ports[8] = ports[9] = port;

  char connection_address[1024];
  sprintf(connection_address, "tcp://127.0.0.1:%d", vis_frame_port);
  const std::string visualizer_topic = "pimega_high_perf";
  const size_t max_frame_size = maxSizeX * maxSizeY * sizeof(uint32_t);
  message_consumer = new ZmqMessageConsumer(connection_address,
                                            visualizer_topic, max_frame_size);

  message_consumer->subscribe(
      "ioc_frame_visualizer_callback", [this](void *data) {
        int counter_depth;
        getParameter(PimegaCounterDepth, &counter_depth);
        this->updateEpicsFrame(data, counter_depth == 3 ? NDUInt32 : NDUInt16);
      });

  rc =
      pimega_connect_backend(pimega, "127.0.0.1", backend_port, vis_frame_port);
  if (rc != PIMEGA_SUCCESS) panic("Unable to connect with Backend. Aborting");
  receive_initArgs_from_backend(pimega);

  // Connect to detector
  rc |= pimega_connect(pimega, address, ports);
  if (rc != PIMEGA_SUCCESS) panic("Unable to connect with detector. Aborting");
}

void pimegaDetector::setParameter(int index, const char *value) {
  asynStatus status;

  status = setStringParam(index, value);
  if (status != asynSuccess) panic("setParameter failed.");
}

void pimegaDetector::setParameter(int index, int value) {
  asynStatus status;

  status = setIntegerParam(index, value);
  if (status != asynSuccess) panic("setParameter failed.");
}

void pimegaDetector::setParameter(int index, double value) {
  asynStatus status;

  status = setDoubleParam(index, value);
  if (status != asynSuccess) panic("setParameter failed.");
}

void pimegaDetector::getParameter(int index, int maxChars, char *value) {
  asynStatus status;

  status = getStringParam(index, maxChars, value);
  if (status != asynSuccess) panic("getStringParam failed.");
}

void pimegaDetector::getParameter(int index, int *value) {
  asynStatus status;

  status = getIntegerParam(index, value);
  if (status != asynSuccess) panic("getIntegerParam failed.");
}

void pimegaDetector::getParameter(int index, double *value) {
  asynStatus status;
  status = getDoubleParam(index, value);

  if (status != asynSuccess) panic("getDoubleParam failed.");
}

void pimegaDetector::createParameters(void) {
  createParam(pimegaMedipixModeString, asynParamInt32, &PimegaMedipixMode);
  createParam(pimegaModuleString, asynParamInt32, &PimegaModule);
  createParam(pimegaefuseIDString, asynParamOctet, &PimegaefuseID);
  createParam(pimegaLogFileString, asynParamOctet, &PimegaLogFile);
  createParam(pimegaDacDefaultsString, asynParamOctet, &pimegaDacDefaults);
  createParam(pimegaOmrOPModeString, asynParamInt32, &PimegaOmrOPMode);
  createParam(pimegaMedipixBoardString, asynParamInt32, &PimegaMedipixBoard);
  createParam(pimegaMedipixChipString, asynParamInt32, &PimegaMedipixChip);
  createParam(pimegaPixeModeString, asynParamInt32, &PimegaPixelMode);
  createParam(pimegaContinuosRWString, asynParamInt32, &PimegaContinuosRW);
  createParam(pimegaPolarityString, asynParamInt32, &PimegaPolarity);
  createParam(pimegaDiscriminatorString, asynParamInt32, &PimegaDiscriminator);
  createParam(pimegaTestPulseString, asynParamInt32, &PimegaTestPulse);
  createParam(pimegaCounterDepthString, asynParamInt32, &PimegaCounterDepth);
  createParam(pimegaEqualizationString, asynParamInt32, &PimegaEqualization);
  createParam(pimegaGainString, asynParamInt32, &PimegaGain);
  createParam(pimegaResetString, asynParamInt32, &PimegaReset);
  createParam(pimegaDacBiasString, asynParamInt32, &PimegaDacBias);
  createParam(pimegaThreshold0String, asynParamInt32, &PimegaThreshold0);
  createParam(pimegaThreshold1String, asynParamInt32, &PimegaThreshold1);
  createParam(pimegaDacPreampString, asynParamInt32, &PimegaPreamp);
  createParam(pimegaDacIKrumString, asynParamInt32, &PimegaIkrum);
  createParam(pimegaDacShaperString, asynParamInt32, &PimegaShaper);
  createParam(pimegaDacDiscString, asynParamInt32, &PimegaDisc);
  createParam(pimegaDacDiscLSString, asynParamInt32, &PimegaDiscLS);
  createParam(pimegaDacDiscLString, asynParamInt32, &PimegaDiscL);
  createParam(pimegaDacDelayString, asynParamInt32, &PimegaDelay);
  createParam(pimegaDacTPBufferInString, asynParamInt32, &PimegaTpBufferIn);
  createParam(pimegaDacTPBufferOutString, asynParamInt32, &PimegaTpBufferOut);
  createParam(pimegaDacRpzString, asynParamInt32, &PimegaRpz);
  createParam(pimegaDacGndString, asynParamInt32, &PimegaGnd);
  createParam(pimegaDacTPRefString, asynParamInt32, &PimegaTpRef);
  createParam(pimegaDacFbkString, asynParamInt32, &PimegaFbk);
  createParam(pimegaDacCasString, asynParamInt32, &PimegaCas);
  createParam(pimegaDacTPRefAString, asynParamInt32, &PimegaTpRefA);
  createParam(pimegaDacTPRefBString, asynParamInt32, &PimegaTpRefB);
  createParam(pimegaDacDiscHString, asynParamInt32, &PimegaDiscH);
  createParam(pimegaReadCounterString, asynParamInt32, &PimegaReadCounter);
  createParam(pimegaSenseDacSelString, asynParamInt32, &PimegaSenseDacSel);
  createParam(pimegaDacOutSenseString, asynParamFloat64, &PimegaDacOutSense);
  createParam(pimegaBackendBufferString, asynParamFloat64, &PimegaBackBuffer);
  createParam(pimegaResetRDMABufferString, asynParamInt32,
              &PimegaResetRDMABuffer);
  createParam(pimegaBackendLFSRString, asynParamInt32, &PimegaBackLFSR);
  createParam(pimegaSensorBiasString, asynParamFloat64, &PimegaSensorBias);
  createParam(pimegaEnergyString, asynParamFloat64, &PimegaEnergy);
  createParam(pimegaAllModulesString, asynParamInt32, &PimegaAllModules);
  createParam(pimegaDacsOutSenseString, asynParamFloat32Array,
              &PimegaDacsOutSense);
  createParam(pimegaSendImageString, asynParamInt32, &PimegaSendImage);
  createParam(pimegaSelSendImageString, asynParamInt32, &PimegaSelSendImage);
  createParam(pimegaSendDacDoneString, asynParamInt32, &PimegaSendDacDone);
  createParam(pimegaConfigDiscLString, asynParamInt32, &PimegaConfigDiscL);
  createParam(pimegaLoadEqString, asynParamInt32Array, &PimegaLoadEqualization);
  createParam(pimegaExtBgInString, asynParamFloat64, &PimegaExtBgIn);
  createParam(pimegaExtBgSelString, asynParamInt32, &PimegaExtBgSel);
  createParam(pimegaMbM1TempString, asynParamFloat32Array,
              &PimegaMBTemperatureM1);
  createParam(pimegaMbM2TempString, asynParamFloat32Array,
              &PimegaMBTemperatureM2);
  createParam(pimegaMbM3TempString, asynParamFloat32Array,
              &PimegaMBTemperatureM3);
  createParam(pimegaMbM4TempString, asynParamFloat32Array,
              &PimegaMBTemperatureM4);
  createParam(pimegaSensorM1TempString, asynParamFloat32Array,
              &PimegaSensorTemperatureM1);
  createParam(pimegaSensorM2TempString, asynParamFloat32Array,
              &PimegaSensorTemperatureM2);
  createParam(pimegaSensorM3TempString, asynParamFloat32Array,
              &PimegaSensorTemperatureM3);
  createParam(pimegaSensorM4TempString, asynParamFloat32Array,
              &PimegaSensorTemperatureM4);
  createParam(pimegaMBAvgM1String, asynParamFloat64, &PimegaMBAvgTSensorM1);
  createParam(pimegaMBAvgM2String, asynParamFloat64, &PimegaMBAvgTSensorM2);
  createParam(pimegaMBAvgM3String, asynParamFloat64, &PimegaMBAvgTSensorM3);
  createParam(pimegaMBAvgM4String, asynParamFloat64, &PimegaMBAvgTSensorM4);
  createParam(pimegaLoadEqStartString, asynParamInt32, &PimegaLoadEqStart);
  createParam(pimegaReadSensorTemperatureString, asynParamInt32,
              &PimegaReadSensorTemperature);
  createParam(pimegaMPAvgM1String, asynParamFloat64, &PimegaMPAvgTSensorM1);
  createParam(pimegaMPAvgM2String, asynParamFloat64, &PimegaMPAvgTSensorM2);
  createParam(pimegaMPAvgM3String, asynParamFloat64, &PimegaMPAvgTSensorM3);
  createParam(pimegaMPAvgM4String, asynParamFloat64, &PimegaMPAvgTSensorM4);
  createParam(pimegaCheckSensorsString, asynParamInt32, &PimegaCheckSensors);
  createParam(pimegaReadMBTemperatureString, asynParamInt32,
              &PimegaReadMBTemperature);
  createParam(pimegaTempMonitorEnableString, asynParamInt32,
              &PimegaTempMonitorEnable);
  createParam(pimegaDisabledSensorsM1String, asynParamInt32Array,
              &PimegaDisabledSensorsM1);
  createParam(pimegaDisabledSensorsM2String, asynParamInt32Array,
              &PimegaDisabledSensorsM2);
  createParam(pimegaDisabledSensorsM3String, asynParamInt32Array,
              &PimegaDisabledSensorsM3);
  createParam(pimegaDisabledSensorsM4String, asynParamInt32Array,
              &PimegaDisabledSensorsM4);
  createParam(pimegaM1TempStatusString, asynParamInt32,
              &PimegaTemperatureStatusM1);
  createParam(pimegaM2TempStatusString, asynParamInt32,
              &PimegaTemperatureStatusM2);
  createParam(pimegaM3TempStatusString, asynParamInt32,
              &PimegaTemperatureStatusM3);
  createParam(pimegaM4TempStatusString, asynParamInt32,
              &PimegaTemperatureStatusM4);
  createParam(pimegaM1TempHighestString, asynParamFloat64,
              &PimegaTemperatureHighestM1);
  createParam(pimegaM2TempHighestString, asynParamFloat64,
              &PimegaTemperatureHighestM2);
  createParam(pimegaM3TempHighestString, asynParamFloat64,
              &PimegaTemperatureHighestM3);
  createParam(pimegaM4TempHighestString, asynParamFloat64,
              &PimegaTemperatureHighestM4);
  createParam(pimegaEnableBulkProcessingString, asynParamInt32,
              &PimegaEnableBulkProcessing);
  createParam(pimegaAbortSaveString, asynParamInt32, &PimegaAbortSave);
  createParam(pimegaIndexEnableString, asynParamInt32, &PimegaIndexEnable);
  createParam(pimegaAcquireShmemEnableString, asynParamInt32,
              &PimegaAcqShmemEnable);

  createParam(pimegaIndexSendModeString, asynParamInt32, &PimegaIndexSendMode);
  createParam(pimegaIndexIDString, asynParamOctet, &PimegaIndexID);
  createParam(pimegaIndexCounterString, asynParamInt32, &PimegaIndexCounter);
  createParam(pimegaProcessedCounterString, asynParamInt32,
              &PimegaProcessedImageCounter);

  createParam(pimegaMBSendModeString, asynParamInt32, &PimegaMBSendMode);
  createParam(pimegaDistanceString, asynParamFloat64, &PimegaDistance);
  createParam(pimegaIOCStatusMsgString, asynParamInt8Array,
              &PimegaIOCStatusMessage);
  createParam(pimegaServerStatusMsgString, asynParamInt8Array,
              &PimegaServerStatusMessage);
  createParam(pimegaTraceMaskWarningString, asynParamInt32,
              &PimegaTraceMaskWarning);
  createParam(pimegaTraceMaskErrorString, asynParamInt32,
              &PimegaTraceMaskError);
  createParam(pimegaTraceMaskDriverIOString, asynParamInt32,
              &PimegaTraceMaskDriverIO);
  createParam(pimegaTraceMaskFlowString, asynParamInt32, &PimegaTraceMaskFlow);
  createParam(pimegaTraceMaskString, asynParamInt32, &PimegaTraceMask);

  createParam(pimegaReceiveErrorString, asynParamInt32, &PimegaReceiveError);
  createParam(pimegaM1ReceiveErrorString, asynParamInt32,
              &PimegaM1ReceiveError);
  createParam(pimegaM2ReceiveErrorString, asynParamInt32,
              &PimegaM2ReceiveError);
  createParam(pimegaM3ReceiveErrorString, asynParamInt32,
              &PimegaM3ReceiveError);
  createParam(pimegaM4ReceiveErrorString, asynParamInt32,
              &PimegaM4ReceiveError);
  createParam(pimegaM1LostFrameCountString, asynParamInt32,
              &PimegaM1LostFrameCount);
  createParam(pimegaM2LostFrameCountString, asynParamInt32,
              &PimegaM2LostFrameCount);
  createParam(pimegaM3LostFrameCountString, asynParamInt32,
              &PimegaM3LostFrameCount);
  createParam(pimegaM4LostFrameCountString, asynParamInt32,
              &PimegaM4LostFrameCount);
  createParam(pimegaM1RxFrameCountString, asynParamInt32,
              &PimegaM1RxFrameCount);
  createParam(pimegaM2RxFrameCountString, asynParamInt32,
              &PimegaM2RxFrameCount);
  createParam(pimegaM3RxFrameCountString, asynParamInt32,
              &PimegaM3RxFrameCount);
  createParam(pimegaM4RxFrameCountString, asynParamInt32,
              &PimegaM4RxFrameCount);
  createParam(pimegaM1AquisitionCountString, asynParamInt32,
              &PimegaM1AquisitionCount);
  createParam(pimegaM2AquisitionCountString, asynParamInt32,
              &PimegaM2AquisitionCount);
  createParam(pimegaM3AquisitionCountString, asynParamInt32,
              &PimegaM3AquisitionCount);
  createParam(pimegaM4AquisitionCountString, asynParamInt32,
              &PimegaM4AquisitionCount);
  createParam(pimegaM1RdmaBufferUsageString, asynParamFloat64,
              &PimegaM1RdmaBufferUsage);
  createParam(pimegaM2RdmaBufferUsageString, asynParamFloat64,
              &PimegaM2RdmaBufferUsage);
  createParam(pimegaM3RdmaBufferUsageString, asynParamFloat64,
              &PimegaM3RdmaBufferUsage);
  createParam(pimegaM4RdmaBufferUsageString, asynParamFloat64,
              &PimegaM4RdmaBufferUsage);
  createParam(pimegaBackendStatsString, asynParamInt32, &PimegaBackendStats);
  createParam(pimegaIndexErrorString, asynParamInt32, &PimegaIndexError);
  createParam(pimegaMetadataFieldString, asynParamOctet, &PimegaMetadataField);
  createParam(pimegaMetadataValueString, asynParamOctet, &PimegaMetadataValue);
  createParam(pimegaMetadataOMString, asynParamOctet, &PimegaMetadataOM);
  createParam(pimegaFrameProcessModeString, asynParamInt32,
              &PimegaFrameProcessMode);

  createParam(pimegaDacCountScanString, asynParamInt32, &PimegaDacCountScan);
  createParam(pimegaDacCountScanDacsString, asynParamInt32,
              &PimegaDacCountScanDac);
  createParam(pimegaDacCountScanStartString, asynParamInt32,
              &PimegaDacCountScanStart);
  createParam(pimegaDacCountScanStopString, asynParamInt32,
              &PimegaDacCountScanStop);
  createParam(pimegaDacCountScanStepString, asynParamInt32,
              &PimegaDacCountScanStep);
  createParam(pimegaDacCountScanChipsString, asynParamInt8Array,
              &PimegaDacCountScanChips);
  createParam(pimegaDacCountScanData, asynParamGenericPointer,
              &PimegaDacCountScanData);

  createParam(pimegaDiagnosticString, asynParamInt32, &PimegaDiagnostic);
  createParam(pimegaDiagnosticDirString, asynParamOctet, &PimegaDiagnosticDir);
  createParam(pimegaDiagnosticSysInfoIDString, asynParamOctet,
              &PimegaDiagnosticSysInfoID);

  /* Do callbacks so higher layers see any changes */
  callParamCallbacks();
}

asynStatus pimegaDetector::setDefaults(void) {
  int rc = 0;
  setParameter(ADMaxSizeX, maxSizeX);
  setParameter(ADMaxSizeY, maxSizeY);
  setParameter(ADSizeX, maxSizeX);
  setParameter(ADSizeX, maxSizeX);
  setParameter(ADSizeY, maxSizeY);
  setParameter(NDArraySizeX, maxSizeX);
  setParameter(NDArraySizeY, maxSizeY);
  setParameter(NDArraySize, 0);
  setParameter(NDDataType, NDUInt32);
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
  setParameter(NDFileNumCapture, 1);
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
  setParameter(PimegaDistance, 22000.0);
  setParameter(ADFrameType, ADFrameNormal);
  setParameter(PimegaDacCountScanDac, DAC_ThresholdEnergy0);
  setParameter(PimegaDacCountScanStart, 0);
  setParameter(PimegaDacCountScanStop, 100);
  setParameter(PimegaDacCountScanStep, 1);
  rc = acqPeriod(0.0);
  if (rc != PIMEGA_SUCCESS) return asynError;
  rc = acqTime(1.0);
  if (rc != PIMEGA_SUCCESS) return asynError;
  rc = numExposures(1);
  if (rc != PIMEGA_SUCCESS) return asynError;
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
  setParameter(PimegaReceiveError, 0);
  setParameter(PimegaM1ReceiveError, 0);
  setParameter(PimegaM2ReceiveError, 0);
  setParameter(PimegaM3ReceiveError, 0);
  setParameter(PimegaM4ReceiveError, 0);
  setParameter(PimegaM1LostFrameCount, 0);
  setParameter(PimegaM2LostFrameCount, 0);
  setParameter(PimegaM3LostFrameCount, 0);
  setParameter(PimegaM4LostFrameCount, 0);
  setParameter(PimegaM1RxFrameCount, 0);
  setParameter(PimegaM2RxFrameCount, 0);
  setParameter(PimegaM3RxFrameCount, 0);
  setParameter(PimegaM4RxFrameCount, 0);
  setParameter(PimegaM1AquisitionCount, 0);
  setParameter(PimegaM2AquisitionCount, 0);
  setParameter(PimegaM3AquisitionCount, 0);
  setParameter(PimegaM4AquisitionCount, 0);
  setParameter(PimegaM1RdmaBufferUsage, 0.0);
  setParameter(PimegaM2RdmaBufferUsage, 0.0);
  setParameter(PimegaM3RdmaBufferUsage, 0.0);
  setParameter(PimegaM4RdmaBufferUsage, 0.0);
  setParameter(PimegaIndexError, 0);
  setParameter(PimegaIndexCounter, 0);
  setParameter(PimegaProcessedImageCounter, 0);
  setParameter(PimegaTemperatureHighestM1, 0.0);
  setParameter(PimegaTemperatureHighestM2, 0.0);
  setParameter(PimegaTemperatureHighestM3, 0.0);
  setParameter(PimegaTemperatureHighestM4, 0.0);
  setParameter(PimegaMPAvgTSensorM1, 0.0);
  setParameter(PimegaMPAvgTSensorM2, 0.0);
  setParameter(PimegaMPAvgTSensorM3, 0.0);
  setParameter(PimegaMPAvgTSensorM4, 0.0);
  setParameter(NDFileNumCaptured, 0);
  setParameter(PimegaFrameProcessMode, 0);

  setParameter(PimegaDiagnosticDir, "EPICSDiagnostic");
  setParameter(PimegaDiagnosticSysInfoID, "EPICS-Diagnostic");
  setParameter(ADManufacturer, "Pitec");

  setParameter(NDDriverVersion, ADPIMEGA_VERSION);
  setParameter(ADModel, GetModelName(pimega));

  char version[PIMEGA_SMALL_STRING];
  GetFirmwareVersion(pimega, version, sizeof(version));
  setParameter(ADFirmwareVersion, version);

  setParameter(PimegaModule, 10);
  setParameter(PimegaMedipixBoard, 2);
  rc = select_board(pimega, 2);
  if (rc != PIMEGA_SUCCESS) return asynError;

  rc = set_medipix_mode(pimega, MODE_B12);
  if (rc != PIMEGA_SUCCESS) return asynError;
  setParameter(PimegaMedipixMode, MODE_B12);

  rc = getSensorBias(pimega, PIMEGA_ONE_MB_LOW_FLEX_ONE_MODULE);
  if (rc != PIMEGA_SUCCESS) return asynError;
  setParameter(PimegaSensorBias,
               GetModuleBiasVoltage(pimega, PIMEGA_THREAD_MAIN));

  setParameter(PimegaLogFile, this->log_file_path);

  SetAcqParamCameraNumCapture(pimega, 1);

  callParamCallbacks();
  return asynSuccess;
}

asynStatus pimegaDetector::getDacsValues(void) {
  int sensor, rc;
  getParameter(PimegaMedipixChip, &sensor);
  sensor -= 1;

  rc = get_dac(pimega, DIGITAL_READ_ALL_DACS, DAC_ThresholdEnergy0);
  if (rc != PIMEGA_SUCCESS) return asynError;
  setParameter(PimegaThreshold0,
               GetDigitalDacValue(pimega, sensor, DAC_ThresholdEnergy0));
  setParameter(PimegaThreshold1,
               GetDigitalDacValue(pimega, sensor, DAC_ThresholdEnergy1));
  setParameter(PimegaPreamp, GetDigitalDacValue(pimega, sensor, DAC_Preamp));
  setParameter(PimegaIkrum, GetDigitalDacValue(pimega, sensor, DAC_IKrum));
  setParameter(PimegaShaper, GetDigitalDacValue(pimega, sensor, DAC_Shaper));
  setParameter(PimegaDisc, GetDigitalDacValue(pimega, sensor, DAC_Disc));
  setParameter(PimegaDiscLS, GetDigitalDacValue(pimega, sensor, DAC_DiscLS));
  // setParameter(PimegaShaperTest, pimega->digital_dac_values[DAC_ShaperTest])
  setParameter(PimegaDiscL, GetDigitalDacValue(pimega, sensor, DAC_DiscL));
  setParameter(PimegaDelay, GetDigitalDacValue(pimega, sensor, DAC_Delay));
  setParameter(PimegaTpBufferIn,
               GetDigitalDacValue(pimega, sensor, DAC_TPBufferIn));
  setParameter(PimegaTpBufferOut,
               GetDigitalDacValue(pimega, sensor, DAC_TPBufferOut));
  setParameter(PimegaRpz, GetDigitalDacValue(pimega, sensor, DAC_RPZ));
  setParameter(PimegaGnd, GetDigitalDacValue(pimega, sensor, DAC_GND));
  setParameter(PimegaTpRef, GetDigitalDacValue(pimega, sensor, DAC_TPRef));
  setParameter(PimegaFbk, GetDigitalDacValue(pimega, sensor, DAC_FBK));
  setParameter(PimegaCas, GetDigitalDacValue(pimega, sensor, DAC_CAS));
  setParameter(PimegaTpRefA, GetDigitalDacValue(pimega, sensor, DAC_TPRefA));
  setParameter(PimegaTpRefB, GetDigitalDacValue(pimega, sensor, DAC_TPRefB));
  // setParameter(PimegaShaperTest, pimega->digital_dac_values[DAC_Test]);
  setParameter(PimegaDiscH, GetDigitalDacValue(pimega, sensor, DAC_DiscH));
  // getDacsOutSense();
  return asynSuccess;
}

asynStatus pimegaDetector::getOmrValues(void) {
  int rc = 0;
  rc = get_omr(pimega);
  if (rc != PIMEGA_SUCCESS) return asynError;
  setParameter(PimegaOmrOPMode, GetOMRValue(pimega, OMR_M));
  setParameter(PimegaContinuosRW, GetOMRValue(pimega, OMR_CRW_SRW));
  setParameter(PimegaPolarity, GetOMRValue(pimega, OMR_Polarity));
  setParameter(PimegaDiscriminator, GetOMRValue(pimega, OMR_Disc_CSM_SPM));
  setParameter(PimegaTestPulse, GetOMRValue(pimega, OMR_EnableTP));
  setParameter(PimegaCounterDepth, GetOMRValue(pimega, OMR_CountL));
  setParameter(PimegaEqualization, GetOMRValue(pimega, OMR_Equalization));
  setParameter(PimegaPixelMode, GetOMRValue(pimega, OMR_CSM_SPM));
  setParameter(PimegaGain, GetOMRValue(pimega, OMR_Gain_Mode));
  setParameter(PimegaExtBgSel, GetOMRValue(pimega, OMR_Ext_BG_Sel));
  return asynSuccess;
}

asynStatus pimegaDetector::getExtBgIn(void) {
  int rc;
  rc = get_ImgChip_ExtBgIn(pimega);
  if (rc != PIMEGA_SUCCESS) return asynError;
  setParameter(PimegaExtBgIn, GetCurrentExtBGIn(pimega));
  return asynSuccess;
}

void pimegaDetector::report(FILE *fp, int details) {
  fprintf(fp, " Pimega detector: %s\n", this->portName);

  if (details > 0) {
    int dataType;
    getIntegerParam(NDDataType, &dataType);
    fprintf(fp, "  Data type:         %d\n", dataType);
  }

  ADDriver::report(fp, details);
}

asynStatus pimegaDetector::startAcquire(void) {
  int rc = 0;
  SetSoftwareTrigger(pimega, false);
  if (BoolAcqResetRDMA) {
    send_allinitArgs_allModules(pimega);
  }
  rc = execute_acquire(pimega);
  // send_stopAcquire_to_backend(pimega);
  if (rc != PIMEGA_SUCCESS) return asynError;
  return asynSuccess;
}

asynStatus pimegaDetector::startCaptureBackend(void) {
  int rc = 0;
  int acqMode, autoSave, lfsr, bulkProcessingEnum;
  int frameProcessMode;
  char fullFileName[PIMEGA_MAX_FILENAME_LEN];
  bool bulkProcessingBool;
  double acquirePeriod, acquireTime;
  int triggerMode;
  char IndexID[30] = "";
  int indexEnable, ShmemEnable;
  int indexSendMode;  // enum IndexSendMode
  UPDATEIOCSTATUS("Starting acquisition");
  UPDATESERVERSTATUS("Configuring");

  /* Clean up */
  reset_acq_status_return(pimega);

  /* Create the full filename */
  createFileName(sizeof(fullFileName), fullFileName);
  setParameter(NDFullFileName, fullFileName);
  rc = (asynStatus)set_file_name_template(pimega, fullFileName);
  if (rc != PIMEGA_SUCCESS) return asynError;
  getParameter(PimegaMedipixMode, &acqMode);
  getParameter(NDAutoSave, &autoSave);
  getParameter(PimegaBackLFSR, &lfsr);
  getParameter(PimegaEnableBulkProcessing, &bulkProcessingEnum);
  getParameter(ADAcquirePeriod, &acquirePeriod);
  getParameter(ADAcquireTime, &acquireTime);
  getParameter(ADTriggerMode, &triggerMode);
  getParameter(PimegaFrameProcessMode, &frameProcessMode);

  getStringParam(PimegaIndexID, sizeof(IndexID), IndexID);
  getParameter(PimegaIndexEnable, &indexEnable);
  getParameter(PimegaAcqShmemEnable, &ShmemEnable);
  getParameter(PimegaIndexSendMode, &indexSendMode);

  configureAlignment(triggerMode == IOC_TRIGGER_MODE_ALIGNMENT);

  rc = (asynStatus)update_backend_acqArgs(
      pimega, lfsr, autoSave, BoolAcqResetRDMA,
      GetAcqParamCameraNumCapture(pimega), frameProcessMode);
  if (rc != PIMEGA_SUCCESS) return asynError;

  rc = (asynStatus)send_acqArgs_to_backend(pimega);
  get_acquire_period(pimega);
  setParameter(ADAcquirePeriod, GetAcquirePeriod(pimega));
  if (rc != PIMEGA_SUCCESS) {
    char error[100];
    decode_backend_error(pimega->ack.error, error);
    UPDATESERVERSTATUS(error);
    strncpy(this->error_str, "Error configuring backend",
            strlen(this->error_str));
    return asynError;
  }

  UPDATESERVERSTATUS("Backend Ready");

  return asynSuccess;
}

asynStatus pimegaDetector::dac_scan_tmp(pimega_dac_t dac) {
  int rc = 0;
  printf("DAC: %d\n", dac);
  if (dac == DAC_GND) {
    rc = dac_scan(pimega, DAC_GND, 90, 150, 1, 0.65, 75,
                  PIMEGA_SEND_ALL_CHIPS_ALL_MODULES);
    if (rc != PIMEGA_SUCCESS) return asynError;
    rc = select_module(pimega, 4);
    if (rc != PIMEGA_SUCCESS) return asynError;
    rc = select_chipNumber(pimega, 36);
    if (rc != PIMEGA_SUCCESS) return asynError;
    rc = dac_scan(pimega, DAC_GND, 50, 100, 1, 0.65, 75,
                  PIMEGA_SEND_ONE_CHIP_ONE_MODULE);
    if (rc != PIMEGA_SUCCESS) return asynError;
  }

  else if (dac == DAC_FBK) {
    rc = dac_scan(pimega, DAC_FBK, 140, 200, 1, 0.9, 75,
                  PIMEGA_SEND_ALL_CHIPS_ALL_MODULES);
    if (rc != PIMEGA_SUCCESS) return asynError;
    rc = select_module(pimega, 4);
    if (rc != PIMEGA_SUCCESS) return asynError;
    rc = select_chipNumber(pimega, 36);
    if (rc != PIMEGA_SUCCESS) return asynError;
    rc = dac_scan(pimega, DAC_FBK, 80, 130, 1, 0.9, 75,
                  PIMEGA_SEND_ONE_CHIP_ONE_MODULE);
    if (rc != PIMEGA_SUCCESS) return asynError;
  }

  else if (dac == DAC_CAS) {
    rc = dac_scan(pimega, DAC_CAS, 140, 200, 1, 0.85, 75,
                  PIMEGA_SEND_ALL_CHIPS_ALL_MODULES);
    if (rc != PIMEGA_SUCCESS) return asynError;
    rc = select_module(pimega, 4);
    if (rc != PIMEGA_SUCCESS) return asynError;
    rc = select_chipNumber(pimega, 36);
    if (rc != PIMEGA_SUCCESS) return asynError;
    rc = dac_scan(pimega, DAC_CAS, 80, 130, 1, 0.85, 75,
                  PIMEGA_SEND_ONE_CHIP_ONE_MODULE);
    if (rc != PIMEGA_SUCCESS) return asynError;
  }

  return asynError;
}

asynStatus pimegaDetector::dacCountScan() {
  int rc = PIMEGA_SUCCESS;
  int start = 0, stop = 100, step = 1;
  uint8_t current_module = 1;
  pimega_dac_t dac;

  int sizex, sizey;
  getIntegerParam(ADMaxSizeX, &sizex);
  getIntegerParam(ADMaxSizeY, &sizey);

  getParameter(PimegaDacCountScanStart, &start);
  getParameter(PimegaDacCountScanStop, &stop);
  getParameter(PimegaDacCountScanStep, &step);
  getParameter(PimegaDacCountScanDac, (int *)&dac);
  getParameter(PimegaModule, (int *)&current_module);

  char fullFileName[PIMEGA_MAX_FILENAME_LEN];
  createFileName(sizeof(fullFileName), fullFileName);
  setParameter(NDFullFileName, fullFileName);

  uint8_t *sensors = NULL;
  size_t slen = PimegaDacCountScanSelectedChips_.size();
  if (slen > 0) {
    sensors = PimegaDacCountScanSelectedChips_.data();
  }

  uint32_t *counts =
      DACCountScan(pimega, dac, sensors, slen, start, stop, step, sizex, sizey,
                   numModulesX_, numModulesY_, current_module, fullFileName);
  if (counts == NULL) {
    return asynError;
  }

  int maxChips = GetMaxChipsPerModule(pimega);
  int steps = (stop - start) / step + 1;

  size_t shape[2] = {steps, maxChips};

  PimegaDacCountScanResult =
      this->pNDArrayPool->alloc(2, shape, NDUInt32, 0, NULL);
  memcpy(PimegaDacCountScanResult->pData, counts,
         PimegaDacCountScanResult->dataSize);

  doCallbacksGenericPointer(PimegaDacCountScanResult, PimegaDacCountScanData,
                            0);

  DACCountScanFree(counts);
  PimegaDacCountScanResult->release();

  return asynSuccess;
}

asynStatus pimegaDetector::selectModule(uint8_t module) {
  int rc = 0;
  int mfb, send_mode;
  getParameter(PimegaMedipixBoard, &mfb);
  getParameter(PimegaMBSendMode, &send_mode);
  rc = select_module(pimega, module);
  if (rc != PIMEGA_SUCCESS) {
    return asynError;
  }
  rc = select_board(pimega, mfb);
  if (rc != PIMEGA_SUCCESS) return asynError;
  rc = getSensorBias(pimega, (pimega_send_mb_flex_t)send_mode);
  if (rc != PIMEGA_SUCCESS) return asynError;
  setParameter(PimegaSensorBias,
               GetModuleBiasVoltage(pimega, PIMEGA_THREAD_MAIN));

  setParameter(PimegaModule, module);
  return asynSuccess;
}

asynStatus pimegaDetector::triggerMode(ioc_trigger_mode_t trigger) {
  int rc = 0;
  if ((trigger < IOC_TRIGGER_MODE_INTERNAL) ||
      (trigger > IOC_TRIGGER_MODE_ALIGNMENT)) {
    error("Invalid value: %d\n", trigger);
    return asynError;
  }

  switch (trigger) {
    case IOC_TRIGGER_MODE_INTERNAL:
      rc = configure_trigger(pimega, TRIGGER_MODE_IN_INTERNAL_OUT_ACQ);
      break;
    case IOC_TRIGGER_MODE_EXTERNAL:
      rc = configure_trigger(pimega, TRIGGER_MODE_IN_EXTERNAL_OUT_ACQ);
      break;
    case IOC_TRIGGER_MODE_ALIGNMENT:
      rc = configure_trigger(pimega, TRIGGER_MODE_IN_INTERNAL_OUT_SHUTTER);
      break;
  }

  if (rc != PIMEGA_SUCCESS) {
    error("TriggerMode out the range: %s\n", pimega_error_string(rc));
    return asynError;
  }
  return asynSuccess;
}

asynStatus pimegaDetector::configDiscL(int value) {
  int rc = 0;
  int all_modules;
  getParameter(PimegaAllModules, &all_modules);
  rc = config_discl_all(pimega, value);
  if (rc != PIMEGA_SUCCESS) {
    error("Value out the range: %s\n", pimega_error_string(rc));
    return asynError;
  }
  return asynSuccess;
}

asynStatus pimegaDetector::setDACValue(pimega_dac_t dac, int value,
                                       int parameter) {
  int rc = 0;
  int all_modules;

  /* TODO: Is this necessary? callParamCallbacks is setting the PV.
   * PimegaSendDacDone is not used anywhere. */
  setParameter(PimegaSendDacDone, 0);
  callParamCallbacks();

  getParameter(PimegaAllModules, &all_modules);
  rc = set_dac(pimega, dac, (unsigned)value, (pimega_send_to_all_t)all_modules);
  if (rc != PIMEGA_SUCCESS) {
    error("Unable to change DAC value: %s\n", pimega_error_string(rc));
    return asynError;
  }

  setParameter(PimegaSendDacDone, 1);
  // rc = US_ImgChipDACOUTSense_RBV(pimega);
  // setParameter(PimegaDacOutSense, pimega->pimegaParam.dacOutput);
  setParameter(parameter, value);
  return asynSuccess;
}

asynStatus pimegaDetector::setOMRValue(pimega_omr_t omr, int value,
                                       int parameter) {
  int rc = 0;
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

asynStatus pimegaDetector::loadEqualization() {
  int rc = 0, send_form, sensor;

  getParameter(PimegaAllModules, &send_form);
  getParameter(PimegaMedipixChip, &sensor);

  rc |= load_equalization(pimega, pimega->loadEqCFG, sensor,
                          (pimega_send_to_all_t)send_form);

  if (rc != PIMEGA_SUCCESS) return asynError;
  return asynSuccess;
}

asynStatus pimegaDetector::sendImage(void) {
  int rc = 0, send_to_all, pattern;

  getParameter(PimegaSelSendImage, &pattern);
  getParameter(PimegaAllModules, &send_to_all);
  send_image(pimega, send_to_all, pattern);

  if (rc != PIMEGA_SUCCESS) return asynError;
  return asynSuccess;
}

asynStatus pimegaDetector::checkSensors(void) {
  int rc = 0, idxParam;

  idxParam = PimegaDisabledSensorsM1;
  rc = check_and_disable_sensors(pimega);
  for (int module = 1; module <= GetTotalModules(pimega); module++) {
    for (int sensor = 0; sensor < GetChipsPerModule(pimega); sensor++) {
      PimegaDisabledSensors_[sensor] =
          (epicsInt32)(CheckSensorIsDisabled(pimega, module - 1, sensor));
    }
    doCallbacksInt32Array(PimegaDisabledSensors_, GetChipsPerModule(pimega),
                          idxParam, 0);
    idxParam++;
  }

  if (rc != PIMEGA_SUCCESS) return asynError;
  return asynSuccess;
}

asynStatus pimegaDetector::reset(short action) {
  int rc = PIMEGA_SUCCESS;
  int rc_aux = PIMEGA_SUCCESS;
  int send_mode;
  getParameter(PimegaMBSendMode, &send_mode);

  if (action < 0 || action > 1) {
    error("Invalid boolean value: %d\n", action);
    return asynError;
  }

  if (action == 0) {
    rc = pimega_reset(pimega);
  } else {
    char _file[256] = "";
    getStringParam(pimegaDacDefaults, sizeof(_file), _file);
    printf("reading file %s\n", _file);
    rc |= pimega_reset_and_init(pimega, _file);
  }
  if (rc != PIMEGA_SUCCESS) rc_aux = rc;
  /* Set some default parameters */
  rc = acqPeriod(0.0);
  if (rc != PIMEGA_SUCCESS) rc_aux = rc;
  rc = acqTime(1.0);
  if (rc != PIMEGA_SUCCESS) rc_aux = rc;
  rc = numExposures(1);
  if (rc != PIMEGA_SUCCESS) rc_aux = rc;
  setParameter(ADTriggerMode,
               this->trigger_input_cfg->PIMEGA_TRIGGER_IN_INTERNAL);
  rc = medipixMode(MODE_B12);
  if (rc != PIMEGA_SUCCESS) rc_aux = rc;

  /* Get some parameters */
  rc = getDacsValues();
  if (rc != PIMEGA_SUCCESS) rc_aux = rc;
  setParameter(PimegaSensorBias,
               GetModuleBiasVoltage(pimega, PIMEGA_THREAD_MAIN));

  if (rc_aux != PIMEGA_SUCCESS) {
    return asynError;
  }

  return asynSuccess;
}

asynStatus pimegaDetector::medipixBoard(uint8_t board_id) {
  int rc = 0, send_mode;

  rc = select_board(pimega, board_id);

  getParameter(PimegaMBSendMode, &send_mode);
  if (rc != PIMEGA_SUCCESS) {
    return asynError;
  }

  rc = getSensorBias(pimega, (pimega_send_mb_flex_t)send_mode);
  if (rc != PIMEGA_SUCCESS) return asynError;

  setParameter(PimegaSensorBias,
               GetModuleBiasVoltage(pimega, PIMEGA_THREAD_MAIN));

  // getMfbTemperature();
  setParameter(PimegaMedipixBoard, board_id);
  return asynSuccess;
}

asynStatus pimegaDetector::medipixMode(uint8_t mode) {
  int rc = 0;
  rc = set_medipix_mode(pimega, (acquisition_mode_t)mode);
  if (rc != PIMEGA_SUCCESS) {
    error("Invalid Medipix Mode: %s\n", pimega_error_string(rc));
    return asynError;
  }
  setParameter(PimegaMedipixMode, mode);

  return asynSuccess;
}

asynStatus pimegaDetector::imgChipID(uint8_t chip_id) {
  int rc = 0;
  char *_efuseID;

  rc = select_chipNumber(pimega, chip_id);
  if (rc != PIMEGA_SUCCESS) {
    error("Invalid number of medipix chip ID: %s\n", pimega_error_string(rc));
    return asynError;
  }
  setParameter(PimegaMedipixChip, chip_id);
  setParameter(PimegaMedipixBoard, get_currrent_mb(pimega));

  /* Get e-fuseID from selected chip_id */
  rc = efuseid_rbv(pimega);
  if (rc != PIMEGA_SUCCESS) return asynError;
  _efuseID = GetCurrentEfuseID(pimega);
  setParameter(PimegaefuseID, _efuseID);

  rc = getDacsValues();
  if (rc != PIMEGA_SUCCESS) return asynError;
  rc = getOmrValues();
  if (rc != PIMEGA_SUCCESS) return asynError;
  rc = getExtBgIn();
  if (rc != PIMEGA_SUCCESS) return asynError;
  return asynSuccess;
}

asynStatus pimegaDetector::numExposures(unsigned number) {
  int rc = 0;
  rc = set_numberExposures(pimega, number);
  if (rc != PIMEGA_SUCCESS) {
    error("Invalid number of exposures: %s\n", pimega_error_string(rc));
    return asynError;
  }
  setParameter(ADNumExposures, (int)number);
  SetAcqParamCameraNumCapture(pimega, number);
  return asynSuccess;
}

asynStatus pimegaDetector::acqTime(double acquire_time_s) {
  int rc = 0;
  double acquire_time_min = 1e-6;
  double acquire_time_max = 18446744073709551615e-6;
  // TODO (Lumentum): Check why the pimega.template is not blocking invalid
  // values
  if ((acquire_time_s < acquire_time_min) ||
      (acquire_time_s > acquire_time_max)) {
    error("Invalid acquire time: %lf\n", acquire_time_s);
    return asynError;
  }
  uint64_t acquire_time_us = (uint64_t)(acquire_time_s * 1e6);
  rc = set_acquireTime(pimega, acquire_time_us);
  if (rc != PIMEGA_SUCCESS) {
    error("Invalid acquire time: %s\n", pimega_error_string(rc));
    return asynError;
  }
  setParameter(ADAcquireTime, acquire_time_s);
  get_acquire_period(pimega);
  double acq_period_s_rbv = GetAcquirePeriod(pimega);
  setParameter(ADAcquirePeriod, acq_period_s_rbv);
  return asynSuccess;
}

asynStatus pimegaDetector::acqPeriod(double period_time_s) {
  int rc = 0;
  double acquire_period_min = 1e-6;
  double acquire_period_max = 18446744073709551615e-6;
  // TODO (Lumentum): Check why the pimega.template is not blocking invalid
  // values
  if (((period_time_s < acquire_period_min) ||
       (period_time_s > acquire_period_max)) &&
      period_time_s != 0) {
    error("Invalid acquire period: %lf\n", period_time_s);
    return asynError;
  }
  uint64_t period_time_us = (uint64_t)(period_time_s * 1e6);
  rc = set_periodTime(pimega, period_time_us);
  if (rc != PIMEGA_SUCCESS) {
    error("Invalid period time: %s\n", pimega_error_string(rc));
    return asynError;
  } else {
    get_acquire_period(pimega);
    double acq_period_s_rbv = GetAcquirePeriod(pimega);
    setParameter(ADAcquirePeriod, acq_period_s_rbv);
    return asynSuccess;
  }
}

asynStatus pimegaDetector::metadataHandler(int op_mode) {
  int rc = asynSuccess;
  char field[MAX_METADATA_LENGTH] = "";
  char value[MAX_METADATA_LENGTH] = "";
  char result[PIMEGA_SIZE_RESULT] = "";
  getParameter(PimegaMetadataField, sizeof(field), field);
  getParameter(PimegaMetadataValue, sizeof(value), value);
  switch (op_mode) {
    case (kSetMethod):
      rc = set_collection_metadata(pimega, field, value);
      break;
    case (kGetMethod):
      rc = get_collection_metadata(pimega, field);
      if (rc == PIMEGA_SUCCESS) {
        sscanf(get_module_result(pimega), "%s", result);
      }
      break;
    case (kDelMethod):
      rc = del_collection_metadata(pimega, field);
      break;
    case (kClearMethod):
      rc = clear_collection_metadata(pimega);
      break;
    default:
      error("Invalid metadata operation: %d\n", op_mode);
  }
  if (rc != PIMEGA_SUCCESS) {
    error("Invalid value: %s\n", pimega_error_string(rc));
    return asynError;
  }
  if (op_mode != kSetMethod) {
    setParameter(PimegaMetadataValue, result);
  }
  return asynSuccess;
}

asynStatus pimegaDetector::setExtBgIn(float voltage) {
  int rc = 0;

  rc = set_ImgChip_ExtBgIn(pimega, voltage);
  if (rc != PIMEGA_SUCCESS) {
    error("Invalid value: %s\n", pimega_error_string(rc));
    return asynError;
  }
  setParameter(PimegaExtBgIn, voltage);
  return asynSuccess;
}

asynStatus pimegaDetector::sensorBias(float voltage) {
  int rc = 0;
  int send_mode;

  getParameter(PimegaMBSendMode, &send_mode);
  rc = setSensorBias(pimega, voltage, (pimega_send_mb_flex_t)send_mode);
  if (rc != PIMEGA_SUCCESS) {
    error("Invalid value: %s\n", pimega_error_string(rc));
    return asynError;
  }

  getSensorBias(pimega, (pimega_send_mb_flex_t)send_mode);
  setParameter(PimegaSensorBias,
               GetModuleBiasVoltage(pimega, PIMEGA_THREAD_MAIN));

  return asynSuccess;
}

asynStatus pimegaDetector::setThresholdEnergy(float energy) {
  int rc = PIMEGA_SUCCESS;
  rc = set_energy(pimega, energy);
  if (rc != PIMEGA_SUCCESS) {
    error("Error while trying to set energy\n%s\n", pimega_error_string(rc));
    return asynError;
  }
  setParameter(PimegaEnergy, GetCalibrationEnergy(pimega));
  return asynSuccess;
}

asynStatus pimegaDetector::getThresholdEnergy(void) {
  int rc = get_energy(pimega);
  if (rc != PIMEGA_SUCCESS) return asynError;
  return asynSuccess;
}

asynStatus pimegaDetector::readCounter(int counter) {
  int rc = 0;
  rc = read_counter(pimega, (pimega_read_counter_t)counter);
  if (rc != PIMEGA_SUCCESS) {
    return asynError;
  }

  setParameter(PimegaReadCounter, counter);
  return asynSuccess;
}

asynStatus pimegaDetector::senseDacSel(u_int8_t dac) {
  int rc = 0;
  rc = setOMRValue(OMR_Sense_DAC, dac, PimegaSenseDacSel);
  if (rc != PIMEGA_SUCCESS) return asynError;
  rc = get_dac_out_sense(pimega);
  if (rc != PIMEGA_SUCCESS) return asynError;
  SenseDacSel_RBV(pimega);
  setParameter(PimegaDacOutSense, GetCurrentDACOutSense(pimega));
  setParameter(PimegaSenseDacSel, SenseDacSelValue(pimega));
  return asynSuccess;
}

asynStatus pimegaDetector::getDacsOutSense(void) {
  int chip_id;
  getParameter(PimegaMedipixChip, &chip_id);

  // TODO: Check if this iteration starting at 0 makes sense. The pimega_dac_t
  // enum starts at 1.
  for (int i = 0; i < N_DACS_OUTS; i++) {
    PimegaDacsOutSense_[i] = (epicsFloat32)GetLastAnalogDacValue(
        this->pimega, chip_id - 1, (pimega_dac_t)i);
  }
  doCallbacksFloat32Array(PimegaDacsOutSense_, N_DACS_OUTS, PimegaDacsOutSense,
                          0);

  return asynSuccess;
}

asynStatus pimegaDetector::getMbTemperature(void) {
  int idxWaveform, idxAvg, rc;
  int total_sensors;
  float sum = 0.00, average;

  total_sensors = GetModuleTemperatureSensorCount(pimega);
  idxWaveform = PimegaMBTemperatureM1;
  idxAvg = PimegaMBAvgTSensorM1;

  rc = getMB_Temperatures(pimega);
  if (rc != PIMEGA_SUCCESS) return asynError;

  for (int module = 1; module <= GetTotalModules(pimega); module++) {
    for (int i = 0; i < total_sensors; i++) {
      PimegaMBTemperature_[i] =
          (epicsFloat32)GetBoardTemperatureFromModule(pimega, module - 1, i);
      sum += PimegaMBTemperature_[i];
    }
    average = sum / total_sensors;
    sum = 0;
    setParameter(idxAvg, average);
    doCallbacksFloat32Array(PimegaMBTemperature_, total_sensors, idxWaveform,
                            0);
    idxWaveform++;
    idxAvg++;
  }

  return asynSuccess;
}

asynStatus pimegaDetector::setTempMonitor(int enable) {
  int rc;
  rc = set_temp_monitor_enable(pimega, enable,
                               PIMEGA_SEND_ALL_CHIPS_ALL_MODULES);
  if (rc != PIMEGA_SUCCESS) {
    UPDATEIOCSTATUS("Temperature Monitor enable failed");
    return asynError;
  }
  setParameter(PimegaTempMonitorEnable, enable);
  return asynSuccess;
}

asynStatus pimegaDetector::getTemperatureStatus(void) {
  int idxTempStatus[] = {PimegaTemperatureStatusM1, PimegaTemperatureStatusM2,
                         PimegaTemperatureStatusM3, PimegaTemperatureStatusM4};

  for (int module = 0; module < GetTotalModules(pimega); module++) {
    setIntegerParam(idxTempStatus[module],
                    GetTemperatureStatusFromModule(pimega, module));
  }
  return asynSuccess;
}

asynStatus pimegaDetector::getTemperatureHighest(void) {
  int idxTempHighest[] = {
      PimegaTemperatureHighestM1, PimegaTemperatureHighestM2,
      PimegaTemperatureHighestM3, PimegaTemperatureHighestM4};

  for (int module = 0; module < GetTotalModules(pimega); module++) {
    setParameter(idxTempHighest[module],
                 GetTemperatureHighestFromModule(pimega, module));
  }
  return asynSuccess;
}

asynStatus pimegaDetector::getMedipixTemperatures(void) {
  int rc = 0;
  int idxTemp[] = {PimegaSensorTemperatureM1, PimegaSensorTemperatureM2,
                   PimegaSensorTemperatureM3, PimegaSensorTemperatureM4};
  int idxAvg[] = {PimegaMPAvgTSensorM1, PimegaMPAvgTSensorM2,
                  PimegaMPAvgTSensorM3, PimegaMPAvgTSensorM4};
  rc = getMedipixSensor_Temperatures_Adjusted(pimega);
  if (rc != PIMEGA_SUCCESS) return asynError;
  for (int module = 1; module <= GetTotalModules(pimega); module++) {
    doCallbacksFloat32Array(pimega->pimegaParam.allchip_temperature[module - 1],
                            GetChipsPerModule(pimega), idxTemp[module - 1], 0);
    setParameter(idxAvg[module - 1],
                 GetChipAverageTemperatureFromModule(pimega, module - 1));
  }
  return asynSuccess;
}

asynStatus pimegaDetector::getMedipixAvgTemperature(void) {
  int idxAvg = PimegaMPAvgTSensorM1;
  int rc = get_TemperatureSensorAvg(pimega);
  if (rc != PIMEGA_SUCCESS) return asynError;
  for (int module = 1; module <= GetTotalModules(pimega); module++) {
    setParameter(idxAvg,
                 GetChipAverageTemperatureFromModule(pimega, module - 1));
    idxAvg++;
  }
  return asynSuccess;
}

asynStatus pimegaDetector::initDebugger(int initDebug) {
  // Set all debugging levels to initialised value
  debugMap_["pimegaDetector::acqTask"] = initDebug;
  debugMap_["pimegaDetector::pimegaDetector"] = initDebug;
  debugMap_["pimegaDetector::readEnum"] = initDebug;
  debugMap_["pimegaDetector::writeInt32"] = initDebug;
  debugMap_["pimegaDetector::writeFloat64"] = initDebug;
  return asynSuccess;
}

asynStatus pimegaDetector::debugLevel(const std::string &method, int onOff) {
  if (method == "all") {
    debugMap_["pimegaDetector::acqTask"] = onOff;
    debugMap_["pimegaDetector::pimegaDetector"] = onOff;
    debugMap_["pimegaDetector::readEnum"] = onOff;
    debugMap_["pimegaDetector::writeInt32"] = onOff;
    debugMap_["pimegaDetector::writeFloat64"] = onOff;
  } else {
    debugMap_[method] = onOff;
  }
  return asynSuccess;
}

asynStatus pimegaDetector::debug(const std::string &method,
                                 const std::string &msg) {
  // First check for the debug entry in the debug map
  if (debugMap_.count(method) == 1) {
    // Now check if debug is turned on
    if (debugMap_[method] == 1) {
      // Print out the debug message
      std::cout << method << ": " << msg << std::endl;
    }
  }
  return asynSuccess;
}

asynStatus pimegaDetector::debug(const std::string &method,
                                 const std::string &msg, int value) {
  // First check for the debug entry in the debug map
  if (debugMap_.count(method) == 1) {
    // Now check if debug is turned on
    if (debugMap_[method] == 1) {
      // Print out the debug message
      std::cout << method << ": " << msg << " [" << value << "]" << std::endl;
    }
  }
  return asynSuccess;
}

asynStatus pimegaDetector::debug(const std::string &method,
                                 const std::string &msg, double value) {
  // First check for the debug entry in the debug map
  if (debugMap_.count(method) == 1) {
    // Now check if debug is turned on
    if (debugMap_[method] == 1) {
      // Print out the debug message
      std::cout << method << ": " << msg << " [" << value << "]" << std::endl;
    }
  }
  return asynSuccess;
}

asynStatus pimegaDetector::debug(const std::string &method,
                                 const std::string &msg,
                                 const std::string &value) {
  // First check for the debug entry in the debug map
  if (debugMap_.count(method) == 1) {
    // Now check if debug is turned on
    if (debugMap_[method] == 1) {
      // Copy the string
      std::string val = value;
      // Trim the output
      val.erase(val.find_last_not_of("\n") + 1);
      // Print out the debug message
      std::cout << method << ": " << msg << " [" << val << "]" << std::endl;
    }
  }
  return asynSuccess;
}

asynStatus pimegaDetector::configureAlignment(bool alignment_mode) {
  int numExposuresVar;
  int numCaptureVar;
  int max_num_capture = 2147483647;
  if (alignment_mode) {
    set_numberExposures(pimega, max_num_capture);
    SetAcqParamCameraNumCapture(pimega, max_num_capture);
  } else {
    numCaptureVar = GetAcqParamCameraNumCapture(pimega);
    getIntegerParam(ADNumExposures, &numExposuresVar);
    set_numberExposures(pimega, numExposuresVar);
    getParameter(NDFileNumCapture, &numCaptureVar);
  }
  return asynSuccess;
}

asynStatus pimegaDetector::diagnostic() {
  char diagnosticDir[PIMEGA_MAX_FILE_NAME];
  char diagnosticSysInfoID[PIMEGA_SMALL_STRING];

  getParameter(PimegaDiagnosticDir, sizeof(diagnosticDir), diagnosticDir);
  getParameter(PimegaDiagnosticSysInfoID, sizeof(diagnosticSysInfoID),
               diagnosticSysInfoID);

  PIMEGA_PRINT(pimega, TRACE_MASK_FLOW,
               "Running diagnostic (Dir='%s' SysInfoID='%s')\n", diagnosticDir,
               diagnosticSysInfoID);

  int ret = Diagnostic(pimega, diagnosticSysInfoID, diagnosticDir);

  return ret == PIMEGA_SUCCESS ? asynSuccess : asynError;
}

/* Code for iocsh registration */
static const iocshArg pimegaDetectorConfigArg0 = {"Port name", iocshArgString};
static const iocshArg pimegaDetectorConfigArg1 = {"pimega module 1 address",
                                                  iocshArgString};
static const iocshArg pimegaDetectorConfigArg2 = {"pimega module 2 address",
                                                  iocshArgString};
static const iocshArg pimegaDetectorConfigArg3 = {"pimega module 3 address",
                                                  iocshArgString};
static const iocshArg pimegaDetectorConfigArg4 = {"pimega module 4 address",
                                                  iocshArgString};
static const iocshArg pimegaDetectorConfigArg5 = {"pimega module 5 address",
                                                  iocshArgString};
static const iocshArg pimegaDetectorConfigArg6 = {"pimega module 6 address",
                                                  iocshArgString};
static const iocshArg pimegaDetectorConfigArg7 = {"pimega module 7 address",
                                                  iocshArgString};
static const iocshArg pimegaDetectorConfigArg8 = {"pimega module 8 address",
                                                  iocshArgString};
static const iocshArg pimegaDetectorConfigArg9 = {"pimega module 9 address",
                                                  iocshArgString};
static const iocshArg pimegaDetectorConfigArg10 = {"pimega module 10 address",
                                                   iocshArgString};
static const iocshArg pimegaDetectorConfigArg11 = {"pimega port", iocshArgInt};
static const iocshArg pimegaDetectorConfigArg12 = {"maxSizeX", iocshArgInt};
static const iocshArg pimegaDetectorConfigArg13 = {"maxSizeY", iocshArgInt};
static const iocshArg pimegaDetectorConfigArg14 = {"detectorModel",
                                                   iocshArgInt};
static const iocshArg pimegaDetectorConfigArg15 = {"maxBuffers", iocshArgInt};
static const iocshArg pimegaDetectorConfigArg16 = {"maxMemory", iocshArgInt};
static const iocshArg pimegaDetectorConfigArg17 = {"priority", iocshArgInt};
static const iocshArg pimegaDetectorConfigArg18 = {"stackSize", iocshArgInt};
static const iocshArg pimegaDetectorConfigArg19 = {"simulate", iocshArgInt};
static const iocshArg pimegaDetectorConfigArg20 = {"backendOn", iocshArgInt};
static const iocshArg pimegaDetectorConfigArg21 = {"log", iocshArgInt};
static const iocshArg pimegaDetectorConfigArg22 = {"backend_port", iocshArgInt};
static const iocshArg pimegaDetectorConfigArg23 = {"vis_frame_port",
                                                   iocshArgInt};
static const iocshArg pimegaDetectorConfigArg24 = {"IntAcqResetRDMA",
                                                   iocshArgInt};
static const iocshArg *const pimegaDetectorConfigArgs[] = {
    &pimegaDetectorConfigArg0,  &pimegaDetectorConfigArg1,
    &pimegaDetectorConfigArg2,  &pimegaDetectorConfigArg3,
    &pimegaDetectorConfigArg4,  &pimegaDetectorConfigArg5,
    &pimegaDetectorConfigArg6,  &pimegaDetectorConfigArg7,
    &pimegaDetectorConfigArg8,  &pimegaDetectorConfigArg9,
    &pimegaDetectorConfigArg10, &pimegaDetectorConfigArg11,
    &pimegaDetectorConfigArg12, &pimegaDetectorConfigArg13,
    &pimegaDetectorConfigArg14, &pimegaDetectorConfigArg15,
    &pimegaDetectorConfigArg16, &pimegaDetectorConfigArg17,
    &pimegaDetectorConfigArg18, &pimegaDetectorConfigArg19,
    &pimegaDetectorConfigArg20, &pimegaDetectorConfigArg21,
    &pimegaDetectorConfigArg22, &pimegaDetectorConfigArg23,
    &pimegaDetectorConfigArg24};
static const iocshFuncDef configpimegaDetector = {"pimegaDetectorConfig", 25,
                                                  pimegaDetectorConfigArgs};

static void configpimegaDetectorCallFunc(const iocshArgBuf *args) {
  pimegaDetectorConfig(
      args[0].sval, args[1].sval, args[2].sval, args[3].sval, args[4].sval,
      args[5].sval, args[6].sval, args[7].sval, args[8].sval, args[9].sval,
      args[10].sval, args[11].ival, args[12].ival, args[13].ival, args[14].ival,
      args[15].ival, args[16].ival, args[17].ival, args[18].ival, args[19].ival,
      args[20].ival, args[21].ival, args[22].ival, args[23].ival, args[24].ival,
      args[25].ival, args[26].ival);
}

static void pimegaDetectorRegister(void) {
  iocshRegister(&configpimegaDetector, configpimegaDetectorCallFunc);
}

extern "C" {
epicsExportRegistrar(pimegaDetectorRegister);
}

static const iocshArg pimegaPrintMaskArg0 = {
    "pimegaPrintMask 0x{maskDriverIO, maskError, maskWarning, maskFlow}",
    iocshArgInt};
static const iocshArg *const pimegaPrintMaskArgs[] = {&pimegaPrintMaskArg0};
static const iocshFuncDef pimegaPrintMaskFuncIocsh = {"pimegaPrintMask", 1,
                                                      pimegaPrintMaskArgs};

void pimegaPrintMaskFunc(const iocshArgBuf *args) {
  set_trace_mask(pimega_global, args[0].ival);
}

static void pimegaPrintMaskRegister(void) {
  iocshRegister(&pimegaPrintMaskFuncIocsh, pimegaPrintMaskFunc);
}

extern "C" {
epicsExportRegistrar(pimegaPrintMaskRegister);
}
