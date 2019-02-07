#ifndef _PIMEGA_H_INCLUDED_
#define _PIMEGA_H_INCLUDED_

#if __GNUC__ >= 4
#pragma GCC visibility push(default)
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define PIMEGA_SUCCESS 0
#define PIMEGA_NAME_ERROR -1
#define PIMEGA_PARSE_ERROR -2
#define PIMEGA_COMMAND_FAILED -3
#define PIMEGA_DAC_FAILED -4
#define PIMEGA_DAC_VALUE_FAILED -5
#define PIMEGA_DATA_SERVER_DISCONNECTED -6
#define PIMEGA_INVALID_ERROR_ID -7
#define PIMEGA_DATA_SERVER_PARSE_ERROR -8
#define PIMEGA_INVALID_BIT_RATE -9
#define PIMEGA_INVALID_IMAGE_COUNT -10
#define	PIMEGA_OMR_FAILED -11
#define	PIMEGA_OMR_VALUE_FAILED -12

#define PIMEGA_TIMEOUT 3000000
#define DATA_SERVER_TIMEOUT 5000000
#define PIMEGA_MAX_FILE_NAME 250
#define PIMEGA_NETWORK_RATE_BPS 100e6f
#define PIMEGA_MAXIMUM_IMAGE_COUNT 16383

#define PIMEGA_MIN_GAP 2e-5f

#define PIMEGA_MAX_TEMPERATURE 120
#define PIMEGA_MIN_TEMPERATURE 20

#define PIMEGA_MAX_BIASVOLTAGE 200
#define PIMEGA_MIN_BIASVOLTAGE -200

#define PIMEGA_MAX_RETURN_TEXT 512

#define PIMEGA_MAX_BOARDS 2
#define PIMEGA_MAX_IMGCHIPS 6


typedef enum pimega_operation_mode_t {
	PIMEGA_READ_COUNTER_L = 0,
	PIMEGA_LOAD_DACS,
	PIMEGA_LOAD_COUNTER_L,
	PIMEGA_READ_DACS,
	PIMEGA_READ_COUNTER_H,
	PIMEGA_LOAD_CTPR,
	PIMEGA_LOAD_COUNTER_H,
	PIMEGA_READ_OMR,
	PIMEGA_OPERATION_MODE_ENUM_END,
} pimega_operation_mode_t;

typedef enum pimega_crw_srw_t {
	PIMEGA_CRW_SRW_MODE_SEQUENTIAL = 0,
	PIMEGA_CRW_SRW_MODE_CONTINUOUS,
	PIMEGA_CRW_SRW_MODE_ENUM_END,
} pimega_crw_srw_t;

typedef enum pimega_polarity_t {
	PIMEGA_POLARITY_ELECTRON = 0,
	PIMEGA_POLARITY_HOLES,
	PIMEGA_POLARITY_ENUM_END,
} pimega_polarity_t;

typedef enum pimega_dataout_t {
	PIMEGA_DATA_OUT_0 = 0,
	PIMEGA_DATA_OUT_2,
	PIMEGA_DATA_OUT_4,
	PIMEGA_DATA_OUT_8,
	PIMEGA_DATA_OUT_ENUM_END,
} pimega_dataout_t;

typedef enum pimega_discriminator_t {
	PIMEGA_DISCRIMINATOR_LOW = 0,
	PIMEGA_DISCRIMINATOR_HIGH,
	PIMEGA_DISCRIMINATOR_ENUM_END,
} pimega_discriminator_t;

typedef enum pimega_counterDepth_t {
	PIMEGA_COUNTERDEPTH_1BIT   = 0,
	PIMEGA_COUNTERDEPTH_12BITS,
	PIMEGA_COUNTERDEPTH_6BITS,
	PIMEGA_COUNTERDEPTH_24BITS,
	PIMEGA_COUNTERDEPTH_ENUM_END,
} pimega_counterDepth_t;

typedef enum pimega_spectroscopic_mode_t {
	PIMEGA_COLOUR_MODE_FINE_PITCH = 0,
	PIMEGA_COLOUR_MODE_SPECTROSCOPIC,
	PIMEGA_COLOUR_MODE_ENUM_END,
} pimega_spectroscopic_mode_t;

typedef enum pimega_pixel_mode_t {
	PIMEGA_PIXEL_MODE_SINGLE_PIXEL = 0,
	PIMEGA_PIXEL_MODE_CHARGE_SUMMING,
	PIMEGA_PIXEL_MODE_ENUM_END,
} pimega_pixel_mode_t;

typedef enum pimega_gain_mode_t {
	PIMEGA_GAIN_MODE_SUPER_HIGH = 0,
	PIMEGA_GAIN_MODE_LOW,
	PIMEGA_GAIN_MODE_HIGH,
	PIMEGA_GAIN_MODE_SUPER_LOW,
	PIMEGA_GAIN_MODE_ENUM_END,
} pimega_gain_mode_t;

typedef enum pimega_omr_t {
	OMR_M=0,
	OMR_CRW_SRW,
	OMR_Polarity,
	OMR_PS,
	OMR_Disc_CSM_SPM,
	OMR_EnableTP,
	OMR_CountL,
	OMR_CollumBLock,
	OMR_CollumBLock_Sel,
	OMR_RowBlock,
	OMR_RowBlock_Sel,
	OMR_Equalization,
	OMR_Colour_Mode,
	OMR_CSM_SPM,
	OMR_Info_Header,
	OMR_Fuse_Sel,
	OMR_Fuse_Pulse_Width,
	OMR_Gain_Mode,
	OMR_Sense_DAC,
	OMR_Ext_DAC,
	OMR_Ext_BG_Sel,
	OMR_ENUM_END,
} pimega_omr_t;


typedef enum pimega_dac_t {
	HS_CAS=0, 				//0
	HS_Delay, 				//1
	HS_Disc,				//2
	HS_DiscH,				//3
	HS_DiscL,				//4
	HS_DiscLS,				//5
	HS_FBK,					//6
	HS_GND,					//7
	HS_IKrum,				//8
	HS_Preamp,				//9
	HS_RPZ,					//10
	HS_Shaper,				//11
	HS_ThresholdEnergy0,	//12
	HS_ThresholdEnergy1,	//13
	HS_TPBufferIn, 			//14
	HS_TPBufferOut,			//15
	HS_TPRef,				//16
	HS_TPRefA,				//17
	HS_TPRefB,				//18
	DAC_ENUM_END,
} pimega_dac_t;

typedef struct pimega_dac_values_t {
	unsigned HS_CAS;				
	unsigned HS_Delay; 				
	unsigned HS_Disc;				
	unsigned HS_DiscH;				
	unsigned HS_DiscL;				
	unsigned HS_DiscLS;				
	unsigned HS_FBK;					
	unsigned HS_GND;					
	unsigned HS_IKrum;				
	unsigned HS_Preamp;			
	unsigned HS_RPZ;					
	unsigned HS_Shaper;				
	unsigned HS_ThresholdEnergy0;	
	unsigned HS_ThresholdEnergy1;	
	unsigned HS_TPBufferIn; 			
	unsigned HS_TPBufferOut;			
	unsigned HS_TPRef;				
	unsigned HS_TPRefA;				
	unsigned HS_TPRefB;				
} pimega_dac_values_t;


typedef enum pimega_trigger_mode_t {
	PIMEGA_TRIGGER_MODE_INTERNAL = 0,
	PIMEGA_TRIGGER_MODE_EXTERNAL,
	PIMEGA_TRIGGER_MODE_ENUM_END,
}pimega_trigger_mode_t;


typedef enum pimega_read_counter_t {
	PIMEGA_COUNTER_LOW = 0,
	PIMEGA_COUNTER_HIGH,
	PIMEGA_COUNTER_BOTH,
	PIMEGA_READ_COUNTER_ENUM_END,
} pimega_read_counter_t;


typedef enum pimega_column_t {
	PIMEGA_COLUMN_0 = 0,
	PIMEGA_COLUMN_4,
	PIMEGA_COLUMN_2,
	PIMEGA_COLUMN_6,
	PIMEGA_COLUMN_1,
	PIMEGA_COLUMN_5,
	PIMEGA_COLUMN_3,
	PIMEGA_COLUMN_7,
	PIMEGA_COLUMN_ENUM_END,
} pimega_column_t;

typedef enum pimega_row_count_t {
	PIMEGA_1_ROW = 0,
	PIMEGA_16_ROWS,
	PIMEGA_4_ROWS,
	PIMEGA_64_ROWS,
	PIMEGA_2_ROWS,
	PIMEGA_32_ROWS,
	PIMEGA_8_ROWS,
	PIMEGA_128_ROWS,
	PIMEGA_ROW_COUNT_ENUM_END,
} pimega_row_count_t;


typedef enum pimega_image_mode_t
{
	PIMEGA_IMAGE_MODE_BACK = 0,
	PIMEGA_IMAGE_MODE_SPM_CSM,
	PIMEGA_IMAGE_MODE_DENERGY,
	PIMEGA_IMAGE_MODE_SMFRAMES,
	PIMEGA_IMAGE_MODE_TRIGGER,
	PIMEGA_IMAGE_MODE_SEQCONT,
	PIMEGA_IMAGE_MODE_THRESHOLD,
	PIMEGA_IMAGE_MODE_TESTPULSE,
	PIMEGA_IMAGE_MODE_24BITS,
	PIMEGA_IMAGE_MODE_ENUM_END,
} pimega_image_mode_t;

typedef enum pimega_test_pulse_pattern_t {
	PIMEGA_TEST_PULSE_CLEAR = 0,
	PIMEGA_TEST_PULSE_SET,
	PIMEGA_TEST_PULSE_EVEN_COLUMNS,
	PIMEGA_TEST_PULSE_ODD_COLUMNS,
} pimega_test_pulse_pattern_t;

typedef struct pimega_operation_register_t {
	pimega_operation_mode_t operation_mode; 	//US_OmrOMSelec
	pimega_crw_srw_t crw_srw_mode;			    //US_ContinuousRW
	pimega_polarity_t polarity;					//US_Polarity
	pimega_dataout_t dataout; 					//US_OmrPSSelect
	pimega_discriminator_t discriminator;		//US_Discriminator
	bool test_pulse;							//US_TestPulse
	pimega_counterDepth_t counterDepth_mode; 	//US_CounterDepth (CountL - Medipix)
	bool equalization;							//US_Equalization
	pimega_spectroscopic_mode_t colour_mode; 	//US_SpectroscopicMode - ColourMode medipix (bit 20 OMR)
	pimega_pixel_mode_t pixel_mode;				//US_PixelMode
	pimega_gain_mode_t gain_mode;				//US_Gain
	pimega_dac_t dac;							//US_Set/Get DAC
	pimega_trigger_mode_t trigger_mode;			//US_TriggerMode
	bool discard_data;							//US_DiscardData			
	float time_remaining;						//US_TimeRemaining_RBV
	float acquire_time_s;						//US_AcquireTime
	float bias_voltage;
	unsigned num_images;						//US_NumImages
	uint32_t num_images_counter;				//US_NumImagesCounter_RBV
	float temperature;
	float actual_temperature;
	float acq_period_time_s;
	pimega_read_counter_t read_counter;
	pimega_image_mode_t image_mode;
	bool acquire_state;
	char detector_state[512];
	bool software_trigger;						//US_SotwareTrigger
	bool external_band_gap;
	uint32_t num_exposures;						//US_NumExposures
} pimega_operation_register_t;



// *************** backend struct args ****************

typedef struct __attribute__((__packed__)){
	uint64_t noOfFrames;
	char fileName[100];
} requestToBackendArgs;

typedef struct __attribute__((__packed__)) {
	uint8_t  SGID[16];
	uint8_t  DGID[16];
	uint8_t  DMAC[8];
	uint64_t VADDR;
	uint32_t RKEY;
	uint32_t QPN;
} backendInitArgs;

// ******************************************************


typedef struct pimega_t {
	int pimega_socket;
	FILE *data_server_in;
	FILE *data_server_out;
	FILE *debug_out;
	pimega_operation_register_t cached_result;
	pimega_dac_values_t dac_values;
	char file_template[PIMEGA_MAX_FILE_NAME];
} pimega_t;



pimega_t *pimega_new(void);


int US_DetectorState_RBV(pimega_t *pimega);

int US_TimeRemaining_RBV(pimega_t *pimega);
int US_Reset(pimega_t *pimega, short action);
int US_Acquire(pimega_t *pimega, short  action);
int pimega_acquire_with_callback(pimega_t *pimega, bool action,
		bool (*callback)(const char *file_name, void *data), void *data);
int US_Acquire_RBV(pimega_t *pimega);

// --------------- K60 functions exclusive -----------------------------------------

int Send_Image(pimega_t *pimega, unsigned pattern);


int Set_Trigger(pimega_t *pimega, bool set_trigger);
int Set_Trigger_RBV(pimega_t *pimega);

int Select_Board(pimega_t *pimega, int board_id);
int Select_Board_RBV(pimega_t *pimega);




// ---------------------------------------------------------------------------------

// -------------- OMR Prototypes ---------------------------------------------------
int US_OmrOMSelec(pimega_t *pimega, pimega_operation_mode_t operation_mode);
int US_OmrOMSelec_RBV(pimega_t *pimega);
int US_ContinuousRW(pimega_t *pimega, pimega_crw_srw_t crw_srw_mode);
int US_ContinuousRW_RBV(pimega_t *pimega);
int US_Polarity(pimega_t *pimega, pimega_polarity_t polarity);
int US_Polarity_RBV(pimega_t *pimega);
int US_OmrPSSelec(pimega_t *pimega, pimega_dataout_t dataout);
int US_OmrPSSelec_RBV(pimega_t *pimega);
int US_Discriminator(pimega_t *pimega, pimega_discriminator_t discriminator);
int US_Discriminator_RBV(pimega_t *pimega);
int US_TestPulse(pimega_t *pimega, bool test_pulse);
int US_TestPulse_RBV(pimega_t *pimega);
int US_CounterDepth(pimega_t *pimega, pimega_counterDepth_t mode);
int US_CounterDepth_RBV(pimega_t *pimega);
int US_Equalization(pimega_t *pimega, bool equalization);
int US_Equalization_RBV(pimega_t *pimega);
int US_Equalization_Default(pimega_t *pimega);
int US_Equalization_Region(pimega_t *pimega);
int US_SpectroscopicMode(pimega_t *pimega, pimega_spectroscopic_mode_t colour_mode);
int US_PixelMode(pimega_t *pimega, pimega_pixel_mode_t mode);
int US_PixelMode_RBV(pimega_t *pimega);
int US_Gain(pimega_t *pimega, pimega_gain_mode_t gain_mode);
int US_Gain_RBV(pimega_t *pimega);
// ----------------------------------------------------------------------------------


int US_Set_OMR(pimega_t *pimega, pimega_omr_t omr, int value);


// ---------------- DAC Prototypes -------------------------------------------
int US_Set_DAC_Variable(pimega_t *pimega, pimega_dac_t dac, int value);
int US_Get_DAC_Variable(pimega_t *pimega, pimega_dac_t dac);
// --------------------------------------------------------------------------


int US_NumImages(pimega_t *pimega, unsigned num_images);
int US_NumImages_RBV(pimega_t *pimega);
int US_NumExposures(pimega_t *pimega, int num_exposures);
int US_NumExposures_RBV(pimega_t *pimega);


int US_SensorBias(pimega_t *pimega, float bias_voltage);
int US_SensorBias_RBV(pimega_t *pimega);

int US_AcquireTime(pimega_t *pimega, float acquire_time_s);
int US_AcquireTime_RBV(pimega_t *pimega);
int US_AcquirePeriod(pimega_t *pimega, float acq_period_time_s);
int US_AcquirePeriod_RBV(pimega_t *pimega);


int US_TriggerMode(pimega_t *pimega, pimega_trigger_mode_t trigger_mode);
int US_TriggerMode_RBV(pimega_t *pimega);
int US_SoftawareTrigger(pimega_t *pimega, bool software_trigger);
int US_SoftawareTrigger_RBV(pimega_t *pimega);

int US_ImgChipNumberID(pimega_t *pimega, uint8_t sensor_position);
int US_ImgChipNumberID_RBV(pimega_t *pimega);

int US_ReadCounter(pimega_t *pimega, pimega_read_counter_t counter);
int US_ReadCounter_RBV(pimega_t *pimega);
int US_ImageMode(pimega_t *pimega, pimega_image_mode_t imagemode);
int US_ImageMode_RBV(pimega_t *pimega);
int US_Temperature(pimega_t *pimega, float temperature);
int US_Temperature_RBV(pimega_t *pimega);
int US_TemperatureActual(pimega_t *pimega);
int US_DiscardData(pimega_t *pimega, bool discard_data);
int US_DiscardData_RBV(pimega_t *pimega);




void pimega_delete(pimega_t *pimega);

int pimega_connect(pimega_t *pimega, const char *address, unsigned short port);
void pimega_disconnect(pimega_t *pimega);

int pimega_set_data_server_address(pimega_t *pimega, const char *address);
int pimega_set_data_server_port(pimega_t *pimega, unsigned short port);

int pimega_set_row_select(pimega_t *pimega, bool enable);
int pimega_set_row_count(pimega_t *pimega, pimega_row_count_t count);
int pimega_set_column_select(pimega_t *pimega, bool enable);
int pimega_set_column_number(pimega_t *pimega, pimega_column_t column);

int pimega_set_test_pulse_count(pimega_t *pimega, unsigned count);
int pimega_set_test_pulse_period(pimega_t *pimega, unsigned period);
int pimega_set_test_pulse_pattern(pimega_t *pimega,
		pimega_test_pulse_pattern_t pattern);


int pimega_connect_data_server(pimega_t *pimega, const char *address,
		unsigned short port);
void pimega_disconnect_data_server(pimega_t *pimega);
int pimega_set_file_name_template(pimega_t *pimega, const char *name);
const char *pimega_error_string(int error);
void pimega_set_debug_stream(pimega_t *pimega, FILE *stream);


#ifdef __cplusplus
} /* extern "C" */
#endif

#if __GNUC__ >= 4
#pragma GCC visibility pop
#endif

#endif /* _PIMEGA_H_INCLUDED_ */