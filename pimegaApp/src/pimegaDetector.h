/*
 * pimegaDetector.h
 *
 *  Created on: 11 Dec 2018
 *      Author: Douglas Araujo
 */

#include <epicsEvent.h>

/** Messages to/from Labview command channel */
#define MAX_MESSAGE_SIZE 256
#define MAX_FILENAME_LEN 256
#define MAX_BAD_PIXELS 100
/** Time to poll when reading from Labview */
#define ASYN_POLL_TIME .01

/** Time between checking to see if image file is complete */
#define FILE_READ_DELAY .01

#define DIMS 2
#define DEFAULT_POLL_TIME 2

static const char *driverName = "pimegaDetector";

#define error(fmt, ...) asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, \
        "%s:%d " fmt, __FILE__, __LINE__, __VA_ARGS__)
                                  

#define pimegaefuseIDString             "EFUSE_ID"
#define pimegaOmrOPModeString           "OMR_OP_MODE"
#define pimegaMedipixBoardString        "MEDIPIX_BOARD"
#define pimegaMedipixChipString         "MEDIPIX_CHIP"
#define pimegaPixeModeString            "PIXEL_MODE"
#define pimegaContinuosRWString         "CONTINUOUSRW"
#define pimegaPolarityString            "POLARITY"
#define pimegaDiscriminatorString       "DISCRIMINATOR"
#define pimegaTestPulseString           "TEST_PULSE"
#define pimegaCounterDepthString        "COUNTER_DEPTH"
#define pimegaEqualizationString        "EQUALIZATION"
#define pimegaGainString                "GAIN_MODE"
#define pimegaDacCasString              "CAS"
#define pimegaDacDelayString            "DELAY"
#define pimegaDacDiscString             "DISC"
#define pimegaDacDiscHString            "DISC_H"
#define pimegaDacDiscLString            "DISC_L"
#define pimegaDacDiscLSString           "DISC_LS"
#define pimegaDacFbkString              "FBK"
#define pimegaDacGndString              "GND"
#define pimegaDacIKrumString            "IKRUM"
#define pimegaDacPreampString           "PREAMP"
#define pimegaDacRpzString              "RPZ"
#define pimegaDacShaperString           "SHAPER"
#define pimegaThreshold0String          "THRESHOLD0"
#define pimegaThreshold1String          "THRESHOLD1"
#define pimegaDacTPBufferInString       "TP_BUFFER_IN"
#define pimegaDacTPBufferOutString      "TP_BUFFER_OUT"
#define pimegaDacTPRefString            "TP_REF"
#define pimegaDacTPRefAString           "TP_REF_A"
#define pimegaDacTPRefBString           "TP_REF_B"
#define pimegaResetString               "RESET"

class pimegaDetector: public ADDriver
{
public:
    pimegaDetector(const char *portName, const char *address, int port, int maxSizeX, int maxSizeY,
                int detectorModel, int maxBuffers, size_t maxMemory, int priority, int stackSize);

    virtual asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value);
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    //virtual asynStatus readFloat64(asynUser *pasynUser, epicsFloat64 *value);

    virtual void report(FILE *fp, int details);
    virtual void pollerThread(void);
    virtual void acqTask(void);


protected:
    int PimegaReset;
    #define FIRST_PIMEGA_PARAM PimegaReset
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
    #define LAST_PIMEGA_PARAM PimegaTpRefB

private:

    // ***** poller control variables ****
    double pollTime_;
    int forceCallback_;
    // ***********************************

    epicsEventId startEventId_;
    epicsEventId stopEventId_;

    pimega_t *pimega;
    pimega_detector_model_t detModel;

    void panic(const char *msg);
    void connect(const char *address, unsigned short port);
    void createParameters(void);
    void setParameter(int index, const char *value);
    void setParameter(int index, int value);
    void setParameter(int index, double value);
    void getParameter(int index, int maxChars, char *value);
    void getParameter(int index, int *value);
    void getParameter(int index, double *value);

    void setDefaults(void);
    void prepareScan(unsigned board);

    asynStatus triggerMode(int trigger);
    asynStatus reset(short action);
    asynStatus setDACValue(pimega_dac_t dac, int value, int parameter);
    asynStatus imgChipID(uint8_t chip_id);
    asynStatus medipixBoard(uint8_t board_id);
    asynStatus numExposures(unsigned number);
    asynStatus pixelMode(int mode);
    asynStatus continuosRW(int mode);
    asynStatus polarity(int mode);
    asynStatus discriminator(int mode);
    asynStatus enableTP(int mode);
    asynStatus counterDepth(int mode);
    asynStatus equalization(int mode);
    asynStatus gainMode(int mode);
    asynStatus acqTime(float acquire_time_s);
    asynStatus omrOpMode(int mode);
};

#define NUM_pimega_PARAMS (&LAST_pimega_PARAM - &FIRST_pimega_PARAM + 1)



