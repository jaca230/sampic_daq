#ifndef __SAMPIC256_LIB_H
#define __SAMPIC256_LIB_H
 
/****************************************************************
 *	File	: 	SAMPIC_256Ch_lib.h								*
 *																*
 *	Date	:	April 2020  									*
 *																*
 *	Authors	:   J.Maalmi / D.Breton	                            *
 *	            								                *
 *								 								*
 ***************************************************************/


/* =============================================================================================== */
/* =============================================================================================== */
#include "lpDevC.h" 
#include "SAMPIC_256Ch_Type.h"

#define SAMPIC_256CH_LIBRARY_VERSION   "3.2"

typedef enum
{
	
  SAMPIC256CH_Success = 0,
  SAMPIC256CH_CommError = -1, 
  SAMPIC256CH_GenericError = -2 ,
  SAMPIC256CH_CrateNotFound = -3,
  SAMPIC256CH_ConnectionAlreadyOpen = -4, 
  SAMPIC256CH_NoCrateConnected = -5,
  SAMPIC256CH_NoFeBoardInCrate = -6,
  SAMPIC256CH_OpenDeviceError = - 7,

  SAMPIC256CH_InvalidParam =  -8,
  SAMPIC256CH_NoEvent = -9,
  SAMPIC256CH_AcquisitionRunning = -10,
  SAMPIC256CH_OutOfRange = -11,
  SAMPIC256CH_OutOfMemory = -12,
  SAMPIC256CH_AcquisitionError = -13,
  SAMPIC256CH_ErrInvalidEvent = -14,
  SAMPIC256CH_EventNotAllocated = -15,
  SAMPIC256CH_SaveDataFileNotOpened  = -16,
  SAMPIC256CH_NoFileFound = -17,
  SAMPIC256CH_ErrorWhileOpeningFile = -18,
  SAMPIC256CH_PurgeBufferIncomplete = -19,
  SAMPIC256CH_ReachedEndOfFile = -20,
  SAMPIC256CH_ReceivedInterruptFrame = -21,
  SAMPIC256CH_InconsistencyInBoardsType = -22,
  SAMPIC256CH_Timeout = -23,
  SAMPIC256CH_InvalidConnectionHandle = -24,
  SAMPIC256CH_InvalidChannelNumber = -25,
  SAMPIC256CH_FunctionNotAllowed = -26,
  SAMPIC256CH_CommunicationWriteError = -27,
  SAMPIC256CH_CommunicationReadError = -28,
  SAMPIC256CH_CommunicationReadRequestError = -29,
  SAMPIC256CH_CommunicationReadExtendedError = -30, 
  SAMPIC256CH_SAMPICSlowControlAccessError = -31,
  SAMPIC256CH_ErrNonAllocatedMemory = -32,
  SAMPIC256CH_AtLeastOneCalibFileNotFound = -33,
  SAMPIC256CH_ADCLinearityCalibValuesNotLoaded = -34, 
  SAMPIC256CH_InitDeviceError = - 35,  
  SAMPIC256CH_ErrInvalidTriggerDataEvent = -36,  
  
  
  SAMPIC256CH_InvalidHandle = -40,
  SAMPIC256CH_NoFrameRead = -50,
  
  SAMPIC256CH_NotYetImplemented = -99	
	
}SAMPIC256CH_ErrCode;


#pragma pack(1) 





#pragma pack() 



/* =============================================================================================== */
/* ===========    Open functions : init ========================================================== */
/* =============================================================================================== */


SAMPIC256CH_ErrCode SAMPIC256CH_ReadCrateConnectionParamsFromFile(char fileName[],  CrateConnectionParamStruct *crateConnectionParams);

SAMPIC256CH_ErrCode SAMPIC256CH_OpenCrateConnection(CrateConnectionParamStruct crateConnectionParams, CrateInfoStruct *crateInfoParams);

SAMPIC256CH_ErrCode SAMPIC256CH_CloseCrateConnection(CrateInfoStruct *crateInfoParams);

SAMPIC256CH_ErrCode SAMPIC256CH_LoadAllCalibValuesFromFiles(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, char directory[MAX_PATHNAME_LENGTH]);

SAMPIC256CH_ErrCode SAMPIC256CH_SetDefaultParameters(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams);

SAMPIC256CH_ErrCode SAMPIC256CH_ResetCrate(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams);   

SAMPIC256CH_ErrCode SAMPIC256CH_ReLoadAllParameters(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams); 

SAMPIC256CH_ErrCode SAMPIC256CH_CheckCrateFirmwareVersions(CrateInfoStruct *crateInfoParams);

// specific reload calib values functions 
SAMPIC256CH_ErrCode  	SAMPIC256CH_ReloadInternalTriggerThresholdOffsetsFromFiles	(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, char directory[MAX_PATHNAME_LENGTH], Boolean *calibFilesLoadedFromSpecificBaselineFolder);   

// Correction Levels
SAMPIC256CH_ErrCode SAMPIC256CH_SetCrateCorrectionLevels(CrateInfoStruct *crateInfoParams,CrateParamStruct *crateParams, Boolean adcLinearityCorection, Boolean timeINLCorrection, Boolean residualPedestalCorrection);
SAMPIC256CH_ErrCode SAMPIC256CH_GetCrateCorrectionLevels(CrateInfoStruct *crateInfoParams,CrateParamStruct *crateParams, Boolean *adcLinearityCorection, Boolean *timeINLCorrection, Boolean *residualPedestalCorrection);



// Low hardware access function

SAMPIC256CH_ErrCode 	SAMPIC256CH_BusWriteWords		(CrateInfoStruct *crateInfoParams,AccessType_t access_type, FpgaType_t fpga_type, int fe_board_target,int fe_fpga_target, char sub_address, void* buffer,int word_count);
SAMPIC256CH_ErrCode 	SAMPIC256CH_BusCommandReadWords	(CrateInfoStruct *crateInfoParams,AccessType_t access_type, FpgaType_t fpga_type, int fe_board_target,int fe_fpga_target, char sub_address, int word_count);
SAMPIC256CH_ErrCode 	SAMPIC256CH_BusReadWords		(CrateInfoStruct *crateInfoParams,AccessType_t access_type, FpgaType_t fpga_type, int fe_board_target,int fe_fpga_target, char sub_address, uchar* buffer,int word_count);

SAMPIC256CH_ErrCode     SAMPIC256CH_BusReadExtended		(int deviceHandle, void *buffer, ML_Frame *mf_array, int max_num_bytes, int *nframes); 
SAMPIC256CH_ErrCode     SAMPIC256CH_DAQBusReadExtended	(int deviceHandle, void *buffer, ML_Frame *mf_array, int max_num_bytes, int *nframes);
SAMPIC256CH_ErrCode		SAMPIC256CH_Read_EEPROM(CrateInfoStruct *crateInfoParams,EepromSourceType_t eepromSrce, int fe_board_target,int fe_fpga_target, char sub_address, unsigned char* buffer,int nbOfBytes);    
SAMPIC256CH_ErrCode 	SAMPIC256CH_Write_EEPROM(CrateInfoStruct *crateInfoParams,EepromSourceType_t eepromSrce,int fe_board_target,int fe_fpga_target, char sub_address, unsigned char* buffer,int nbOfBytes);

SAMPIC256CH_ErrCode 	SAMPIC256CH_Read_ControlBoard_EPROM_Info 					(CrateInfoStruct *crateInfoParams);
SAMPIC256CH_ErrCode 	SAMPIC256CH_Read_FeBoard_CTRL_EEPROM_Info 					(CrateInfoStruct *crateInfoParams,int feBoardIndex);
SAMPIC256CH_ErrCode 	SAMPIC256CH_Read_FrontEndBlock_EEPROM_Info 					(CrateInfoStruct *crateInfoParams,int feBoardIndex, int feBlockIndex); 



// High level Configuration function

SAMPIC256CH_ErrCode     Set_SystemADCNbOfBits(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int adcNbOfBits);
SAMPIC256CH_ErrCode 	Get_SystemADCNbOfBits(CrateParamStruct *crateParams, int *adcNbOfBits);

SAMPIC256CH_ErrCode 	SAMPIC256CH_SetChannelMode(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int channel ,Boolean mode);  /* 1 = channel enabled, 0 = channel disabled */
SAMPIC256CH_ErrCode 	SAMPIC256CH_GetChannelMode(CrateParamStruct *crateParams, int feBoard, int channel, Boolean *mode);  /* ON = channel enabled, OFF = channel disabled */

SAMPIC256CH_ErrCode 	SAMPIC256CH_SetBaselineReference(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, float vRefBaseline /* in Volts between 0 and 1.6V */);	  // this function sets the baseline and updates the thresholds for all channels
SAMPIC256CH_ErrCode 	SAMPIC256CH_GetBaselineReference(CrateParamStruct *crateParams, int feBoard, int sampicIndex, float *vRefBaseline /* in Volts */);

SAMPIC256CH_ErrCode 	SAMPIC256CH_SetBaselineForCalib(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, float vRefBaseline /* in Volts between 0 and 1.6V */); // without updating channel thresholds                       

/* SetSampling Frequency function */
/* if useExternalClock is FALSE, then samplingFreq must be one of the values enumarated in SAMPIC_InternalSamplingFreqType_t, otherwise samplingFreq is externalClock x 64 */
 
SAMPIC256CH_ErrCode     SAMPIC256CH_SetSamplingFrequency(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int  samplingFreq /* in MS/s */, Boolean useExternalClock);   /* this function changes the sampling frequency and also re-load all calibration values from files, external clock frequency must be between 20MHz and 133 MHz */


SAMPIC256CH_ErrCode     SAMPIC256CH_GetSamplingFrequency(CrateParamStruct *crateParams, int  *samplingFreq, Boolean *useExternalClock);

SAMPIC256CH_ErrCode     SAMPIC256CH_SetSmartReadMode(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, Boolean mode, int nbOfSamplesToRead, int offsetForStartOfRead);  
SAMPIC256CH_ErrCode     SAMPIC256CH_GetSmartReadMode(CrateParamStruct *crateParams, Boolean *mode, int *nbOfSamplesToRead, int *offsetForStartOfRead);


SAMPIC256CH_ErrCode     SAMPIC256CH_SetTOTMeasurementMode(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, Boolean mode);  
SAMPIC256CH_ErrCode     SAMPIC256CH_GetTOTMeasurementMode(CrateParamStruct *crateParams, Boolean *mode);

SAMPIC256CH_ErrCode     SAMPIC256CH_SetSampicTOTRange(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, SAMPIC_TOTRange_t totRange);  
SAMPIC256CH_ErrCode     SAMPIC256CH_GetSampicTOTRange(CrateParamStruct *crateParams, int feBoard, int sampicIndex, SAMPIC_TOTRange_t *totRange);

SAMPIC256CH_ErrCode     SAMPIC256CH_SetSampicTOTFilterParams(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, Boolean enTotFilter, Boolean enWideCap, float pulseWidth  /* reject pulses with tot values less than .. in ns*/);  
SAMPIC256CH_ErrCode     SAMPIC256CH_GetSampicTOTFilterParams(CrateParamStruct *crateParams, int feBoard, int sampicIndex, Boolean *enTotFilter, Boolean *enWideCap, float *pulseWidth);


SAMPIC256CH_ErrCode     SAMPIC256CH_SetSampicPostTrigParams(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, Boolean enablePostTrig, int postTrigVal /* value between 0 and 7 */);  
SAMPIC256CH_ErrCode     SAMPIC256CH_GetSampicPostTrigParams(CrateParamStruct *crateParams, int feBoard, int sampicIndex, Boolean *enablePostTrig, int *postTrigVal);

SAMPIC256CH_ErrCode     SAMPIC256CH_SetExternalTriggerType(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, ExternalTriggerType_t  extTrigType);
SAMPIC256CH_ErrCode     SAMPIC256CH_GetExternalTriggerType(CrateParamStruct *crateParams,ExternalTriggerType_t  *extTrigType);

SAMPIC256CH_ErrCode     SAMPIC256CH_SetExternalTriggerSigLevel(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, SignalLevel_t  extSigLevel);
SAMPIC256CH_ErrCode     SAMPIC256CH_GetExternalTriggerSigLevel(CrateParamStruct *crateParams,SignalLevel_t  *extSigLevel);

SAMPIC256CH_ErrCode     SAMPIC256CH_SetExternalTriggerEdge(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, EdgeType_t  extTrigEdge); 
SAMPIC256CH_ErrCode     SAMPIC256CH_GetExternalTriggerEdge(CrateParamStruct *crateParams,EdgeType_t  *extTrigEdge);

SAMPIC256CH_ErrCode     SAMPIC256CH_SetExternalSyncEdge(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, EdgeType_t  extTrigEdge); 
SAMPIC256CH_ErrCode     SAMPIC256CH_GetExternalSyncEdge(CrateParamStruct *crateParams,EdgeType_t  *extTrigEdge); 



SAMPIC256CH_ErrCode     SAMPIC256CH_SetExternalSyncSigLevel(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, SignalLevel_t  extSigLevel);
SAMPIC256CH_ErrCode     SAMPIC256CH_GetExternalSyncSigLevel(CrateParamStruct *crateParams,SignalLevel_t  *extSigLevel);

SAMPIC256CH_ErrCode     SAMPIC256CH_SetSampicChannelTriggerMode(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, int channelIndex /* between 0 and 15 */, SAMPIC_ChannelTriggerMode_t  channelTriggerMode);
SAMPIC256CH_ErrCode     SAMPIC256CH_GetSampicChannelTriggerMode(CrateParamStruct *crateParams,int feBoard, int sampicIndex, int channelIndex, SAMPIC_ChannelTriggerMode_t  *channelTriggerMode);


SAMPIC256CH_ErrCode     SAMPIC256CH_SetSampicChannelInternalThreshold(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, int channelIndex /* between 0 and 15 */, float  relativeThreshold /* relative threshold + Vbaseline must be between 0 and 1.8V */); 
SAMPIC256CH_ErrCode     SAMPIC256CH_GetSampicChannelInternalThreshold(CrateParamStruct *crateParams,int feBoard, int sampicIndex, int channelIndex, float  *relativeThreshold /* in Volts between 0 and 1.8V */); 

SAMPIC256CH_ErrCode     SAMPIC256CH_SetChannelSelflTriggerEdge(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, int channelIndex, EdgeType_t  triggerEdge);
SAMPIC256CH_ErrCode     SAMPIC256CH_GetChannelSelfTriggerEdge(CrateParamStruct *crateParams,int feBoard, int sampicIndex, int channelIndex,  EdgeType_t  *triggerEdge); 

SAMPIC256CH_ErrCode     SAMPIC256CH_SetSampicCentralTriggerMode(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, SampicCentralTriggerMode_t centralTriggerMode);
SAMPIC256CH_ErrCode     SAMPIC256CH_GetSampicCentralTriggerMode(CrateParamStruct *crateParams,int feBoard, int sampicIndex, SampicCentralTriggerMode_t *centralTriggerMode); 

SAMPIC256CH_ErrCode     SAMPIC256CH_SetSampicExternalThresholdMode(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, Boolean mode /* 1 = External, 0 = Internal */);
SAMPIC256CH_ErrCode     SAMPIC256CH_GetSampicExternalThresholdMode(CrateParamStruct *crateParams,int feBoard, int sampicIndex, Boolean *mode); 

SAMPIC256CH_ErrCode     SAMPIC256CH_SetSampicCentralTriggerEffect(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex,SampicCentralTriggerEffect_t centralTriggerEffect);
SAMPIC256CH_ErrCode     SAMPIC256CH_GetSampicCentralTriggerEffect(CrateParamStruct *crateParams,int feBoard, int sampicIndex, SampicCentralTriggerEffect_t *centralTriggerEffect); 

SAMPIC256CH_ErrCode     SAMPIC256CH_SetSampicCentralTriggerPrimitivesOptions(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, SAMPIC_CT_PrimitivesMode_t primitivesMode, int primitivesGateLength /* between 0 and 7 x 1/8 of the clock period */);
SAMPIC256CH_ErrCode     SAMPIC256CH_GetSampicCentralTriggerPrimitivesOptions(CrateParamStruct *crateParams,int feBoard, int sampicIndex, SAMPIC_CT_PrimitivesMode_t *primitivesMode, int *primitivesGateLength ); 

SAMPIC256CH_ErrCode     SAMPIC256CH_SetSampicChannelSourceForCT(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, int channelIndex, Boolean mode);
SAMPIC256CH_ErrCode     SAMPIC256CH_GetSampicChannelSourceForCT(CrateParamStruct *crateParams,int feBoard, int sampicIndex, int channelIndex, Boolean *mode); 


SAMPIC256CH_ErrCode     SAMPIC256CH_SetPulserMode(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, Boolean  mode, PulserSourceType_t pulserSource, Boolean synchronousPulses);
SAMPIC256CH_ErrCode     SAMPIC256CH_GetPulserMode(CrateParamStruct *crateParams,Boolean  *mode, PulserSourceType_t *pulserSource, Boolean *synchronousPulses);

SAMPIC256CH_ErrCode     SAMPIC256CH_SetAutoPulserPeriod(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int pulsePeriod);  /*  in clock period of 1MHz minimum 2 max 65535 */
SAMPIC256CH_ErrCode     SAMPIC256CH_GetAutoPulserPeriod(CrateParamStruct *crateParams,int *pulsePeriod);
 
SAMPIC256CH_ErrCode     SAMPIC256CH_SetSampicChannelPulseMode(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, int channelIndex, Boolean mode /* 1 = Enable, 0 = Disable */);
SAMPIC256CH_ErrCode     SAMPIC256CH_GetSampicChannelPulseMode(CrateParamStruct *crateParams,int feBoard, int sampicIndex, int channelIndex, Boolean *mode);     

SAMPIC256CH_ErrCode     SAMPIC256CH_SetSampicPulserWidth(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex,unsigned char pulseWidth /* multiple of 10 ns */);
SAMPIC256CH_ErrCode     SAMPIC256CH_GetSampicPulserWidth(CrateParamStruct *crateParams,int feBoard, int sampicIndex, unsigned char *pulseWidth /* multiple of 10 ns */);     

SAMPIC256CH_ErrCode     SAMPIC256CH_SetSampicExternalThreshold(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex,float externalThreshold);
SAMPIC256CH_ErrCode     SAMPIC256CH_GetSampicExternalThreshold(CrateParamStruct *crateParams,int feBoard, int sampicIndex, float *externalThreshold );     

SAMPIC256CH_ErrCode 	SAMPIC256CH_SetLevel3TriggerLogic(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, Boolean enableBuildL3, TriggerLogicParamStruct triggerLogicParamsForL3);
SAMPIC256CH_ErrCode 	SAMPIC256CH_GetLevel3TriggerLogic(CrateParamStruct *crateParams, Boolean *enableBuildL3, TriggerLogicParamStruct *triggerLogicParams);

SAMPIC256CH_ErrCode 	SAMPIC256CH_SetLevel3CoincidenceModeWithExtTrigGate(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, Boolean enableCoincidence);
SAMPIC256CH_ErrCode 	SAMPIC256CH_GetLevel3CoincidenceModeWithExtTrigGate(CrateParamStruct *crateParams, Boolean *enableCoincidence);

SAMPIC256CH_ErrCode 	SAMPIC256CH_SetLevel3ExtTrigGate(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, unsigned char extTrigGate);
SAMPIC256CH_ErrCode 	SAMPIC256CH_GetLevel3ExtTrigGate(CrateParamStruct *crateParams, unsigned char *extTrigGate);

SAMPIC256CH_ErrCode 	SAMPIC256CH_SetLevel2TriggerBuildOption(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, Boolean enableBuildL2);
SAMPIC256CH_ErrCode 	SAMPIC256CH_GetLevel2TriggerBuildOption(CrateParamStruct *crateParams, Boolean *enableBuildL2); 

SAMPIC256CH_ErrCode 	SAMPIC256CH_SetLevel2TriggerLogic(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, TriggerLogicParamStruct triggerLogicParamsForL2);
SAMPIC256CH_ErrCode 	SAMPIC256CH_GetLevel2TriggerLogic(CrateParamStruct *crateParams, int feBoard, TriggerLogicParamStruct *triggerLogicParamsForL2);


SAMPIC256CH_ErrCode 	SAMPIC256CH_SetLevel2CoincidenceModeWithExtTrigGate(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, Boolean enableCoincidence);
SAMPIC256CH_ErrCode 	SAMPIC256CH_GetLevel2CoincidenceModeWithExtTrigGate(CrateParamStruct *crateParams, int feBoard, Boolean *enableCoincidence);

SAMPIC256CH_ErrCode 	SAMPIC256CH_SetLevel2ExtTrigGate(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, unsigned char extTrigGate);
SAMPIC256CH_ErrCode 	SAMPIC256CH_GetLevel2ExtTrigGate(CrateParamStruct *crateParams, int feBoard, unsigned char *extTrigGate);


SAMPIC256CH_ErrCode 	SAMPIC256CH_SetPrimitivesGateLength(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, unsigned char primitivesGateLength); /* clock periods of 10 ns */
SAMPIC256CH_ErrCode 	SAMPIC256CH_GetPrimitivesGateLength(CrateParamStruct *crateParams, unsigned char *primitivesGateLength);

SAMPIC256CH_ErrCode 	SAMPIC256CH_SetLevel2LatencyGateLength(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, unsigned char latencyGateLength); /* clock periods of 10 ns */
SAMPIC256CH_ErrCode 	SAMPIC256CH_GetLevel2LatencyGateLength(CrateParamStruct *crateParams, unsigned char *latencyGateLength);

SAMPIC256CH_ErrCode 	SAMPIC256CH_SetFrontEndBoardGlobalTriggerOption(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, FebGlobalTrigger_t febGlobalTrigger);
SAMPIC256CH_ErrCode 	SAMPIC256CH_GetFrontEndBoardGlobalTriggerOption(CrateParamStruct *crateParams, int feBoard, FebGlobalTrigger_t *febGlobalTrigger);

SAMPIC256CH_ErrCode     SAMPIC256CH_SetSampicTriggerOption(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, SampicTriggerOption_t sampicTriggerOption);
SAMPIC256CH_ErrCode     SAMPIC256CH_GetSampicTriggerOption(CrateParamStruct *crateParams,int feBoard, int sampicIndex, SampicTriggerOption_t *sampicTriggerOption);     
																																			 
SAMPIC256CH_ErrCode     SAMPIC256CH_SetSampicEnableTriggerMode(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, Boolean  useExtTrigAsEnableTrig, Boolean openGateOnExtTrig, unsigned char extTrigGate);
SAMPIC256CH_ErrCode     SAMPIC256CH_GetSampicEnableTriggerMode(CrateParamStruct *crateParams,int feBoard, int sampicIndex, Boolean  *useExtTrigAsEnableTrig, Boolean *openGateOnExtTrig, unsigned char *extTrigGate);


SAMPIC256CH_ErrCode     SAMPIC256CH_SetSampicCommonDeadTimeMode(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, Boolean  enCommonDeadTime);
SAMPIC256CH_ErrCode     SAMPIC256CH_GetSampicCommonDeadTimeMode(CrateParamStruct *crateParams,int feBoard, int sampicIndex, Boolean  *enCommonDeadTime);

// Acquisition   functions

SAMPIC256CH_ErrCode  SAMPIC256CH_AllocateEventMemory(void **eventBuffer, ML_Frame **mf_array);   // to call once at the beginning of the Soft and for each daqlink (1 to max 4 per crate).

SAMPIC256CH_ErrCode  SAMPIC256CH_InitEvent(EventStruct *event);   

SAMPIC256CH_ErrCode  SAMPIC256CH_StartRun(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, Boolean resync);

SAMPIC256CH_ErrCode  SAMPIC256CH_PrepareEvent(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams);		// first function to be called  at each event

SAMPIC256CH_ErrCode  SAMPIC256CH_AwakeAcquisition(CrateInfoStruct *crateInfoParams);		// function to be called every second ( < 1 Hz)if in UDP connection and very low events acquisition rate.

SAMPIC256CH_ErrCode  SAMPIC256CH_ReadEventBuffer(CrateInfoStruct *crateInfoParams,int feBoard /* feboard direct link index */,  void *eventBuffer, ML_Frame *mf_array, int *nframes);	// 2nd function to be called at each event

SAMPIC256CH_ErrCode  SAMPIC256CH_DecodeEvent(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, ML_Frame *mf_array, EventStruct *my_event, int nframes, int *numberOfHits); // 3rd function to be called at each event

SAMPIC256CH_ErrCode  SAMPIC256CH_StopRun(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams);	   // to be called to stop the run.
		
SAMPIC256CH_ErrCode  SAMPIC256CH_FreeEventMemory(void **eventBuffer, ML_Frame **mf_array);   // to call once at the beginning of the Soft and for each daqlink (1 to max 4 per crate).

SAMPIC256CH_ErrCode SAMPIC256CH_Purge_AllDaqBuffersChain (CrateInfoStruct *crateInfoParams , int maxloops); 

// Advanced Features



SAMPIC256CH_ErrCode     SAMPIC256CH_SetAutoConversionMode(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, Boolean  autoConv);
SAMPIC256CH_ErrCode     SAMPIC256CH_GetAutoConversionMode(CrateParamStruct *crateParams, Boolean  *autoConv);

SAMPIC256CH_ErrCode     SAMPIC256CH_SetConversionLength(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, unsigned char convertLength);
SAMPIC256CH_ErrCode     SAMPIC256CH_GetConversionLength(CrateParamStruct *crateParams, unsigned char *convertLength);

SAMPIC256CH_ErrCode 	SAMPIC256CH_SetSampicADCRampValue(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, float adcRampValue);  
SAMPIC256CH_ErrCode 	SAMPIC256CH_GetSampicADCRampValue(CrateParamStruct *crateParams, int feBoard, int sampicIndex, float *adcRampValue);

SAMPIC256CH_ErrCode 	SAMPIC256CH_SetSampicVdacDLLValue(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, float vdacDllValue);  
SAMPIC256CH_ErrCode 	SAMPIC256CH_GetSampicVdacDLLValue(CrateParamStruct *crateParams, int feBoard, int sampicIndex, float *vdacDllValue);

SAMPIC256CH_ErrCode 	SAMPIC256CH_SetSampicVdacDLLContinuity(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, float vdacDllContinuityValue);  
SAMPIC256CH_ErrCode 	SAMPIC256CH_GetSampicVdacDLLContinuity(CrateParamStruct *crateParams, int feBoard, int sampicIndex, float *vdacDllContinuityValue);

SAMPIC256CH_ErrCode 	SAMPIC256CH_SetSampicVdacRosc(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, float vdacRosc);  
SAMPIC256CH_ErrCode 	SAMPIC256CH_GetSampicVdacRosc(CrateParamStruct *crateParams, int feBoard, int sampicIndex, float *vdacRosc);

SAMPIC256CH_ErrCode 	SAMPIC256CH_SetSampicDLLSpeedMode(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, SampicDLLModeType_t dllMode);  
SAMPIC256CH_ErrCode 	SAMPIC256CH_GetSampicDLLSpeedMode(CrateParamStruct *crateParams, int feBoard, int sampicIndex, SampicDLLModeType_t *dllMode);

SAMPIC256CH_ErrCode 	SAMPIC256CH_SetSampicOverflowDacValue(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, float overflowDacValue);  
SAMPIC256CH_ErrCode 	SAMPIC256CH_GetSampicOverflowDacValue(CrateParamStruct *crateParams, int feBoard, int sampicIndex, float *overflowDacValue);

SAMPIC256CH_ErrCode     SAMPIC256CH_SetSampicLvdsLowCurrentMode(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard,int sampicIndex, Boolean mode);  
SAMPIC256CH_ErrCode     SAMPIC256CH_GetSampicLvdsLowCurrentMode(CrateParamStruct *crateParams, int feBoard, int sampicIndex, Boolean *mode);  

SAMPIC256CH_ErrCode 	SAMPIC256CH_UpdateADCRampValuesFromCalibValues(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams);  
SAMPIC256CH_ErrCode 	SAMPIC256CH_UpdateChannelsInternalThresholdsFromCalibValues(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams);   

SAMPIC256CH_ErrCode     SAMPIC256CH_SetNbOfFramesPerBlock(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int nbOfFramesPerBlock);
SAMPIC256CH_ErrCode     SAMPIC256CH_GetNbOfFramesPerBlock(CrateParamStruct *crateParams, int *nbOfFramesPerBlock);


SAMPIC256CH_ErrCode     SAMPIC256CH_SetCrateSycnhronisationMode(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, Boolean  syncMode /* '1' Crate is in Synchronisation Mode, '0' Crate is in standalione */, Boolean masterMode /* '1' Crate is in Master Mode, '0' Crate is in Slave Mode */, Boolean coincidenceMode);
SAMPIC256CH_ErrCode     SAMPIC256CH_GetCrateSycnhronisationMode(CrateParamStruct *crateParams, Boolean  *syncMode, Boolean *masterMode, Boolean *coincidenceMode);


//External Trigger Counter and TimeStamping

SAMPIC256CH_ErrCode     SAMPIC256CH_SetExternalTriggerCounterMode(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, Boolean  enableExtTriggerCounter /* '1' Crate is Enable, '0' Disable */, Boolean enableDetectExtTriggerID /* '1' enable detect external trigger ID from ExtTrig */);
SAMPIC256CH_ErrCode     SAMPIC256CH_GetExternalTriggerCounterMode(CrateParamStruct *crateParams, Boolean  *enableExtTriggerCounter /* '1' Crate is Enable, '0' Disable */, Boolean *enableDetectExtTriggerID /* '1' enable detect external trigger ID from ExtTrig */);

SAMPIC256CH_ErrCode     SAMPIC256CH_SetMinNbOfTriggersPerEvent(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, unsigned char nbOfTriggersPerEvent /* between 1 and 127 */);
SAMPIC256CH_ErrCode     SAMPIC256CH_GetMinNbOfTriggersPerEvent(CrateParamStruct *crateParams, unsigned char *nbOfTriggersPerEvent);

/* =============================================================================================== */
/* ===========    Reset Functions       ========================================================== */
/* =============================================================================================== */

void  	SAMPIC256CH_ResetResidualPedestalCalibValues					(CrateInfoStruct *crateInfoParams, int feBoardIndex);
void    SAMPIC256CH_ResetADCLinearityCalibValues						(CrateInfoStruct *crateInfoParams, int feBoardIndex); 
void  	SAMPIC256CH_ResetInternalTriggerThresholdOffsetCalibValues		(CrateInfoStruct *crateInfoParams, int feBoardIndex);   // Update Internal Threshold values after resetting Calib Values
void	SAMPIC256CH_ResetADCRampCalibValues								(CrateInfoStruct *crateInfoParams, int feBoardIndex);
void	SAMPIC256CH_ResetTOTCalibValues									(CrateInfoStruct *crateInfoParams, int feBoardIndex);
void  	SAMPIC256CH_ResetTOTDACOffsetsValues							(CrateInfoStruct *crateInfoParams, int feBoardIndex);      
void 	SAMPIC256CH_ResetTimeINLCalibValues								(CrateInfoStruct *crateInfoParams, int feBoardIndex, int channel);  


/* =============================================================================================== */
/* =============================================================================================== */

/* =============================================================================================== */
/* ===========    For Calib Functions   ========================================================= */
/* =============================================================================================== */

SAMPIC256CH_ErrCode  	SAMPIC256CH_SetAutoINLCalibMode					(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, Boolean mode);
SAMPIC256CH_ErrCode  	SAMPIC256CH_GetAutoINLCalibMode					(CrateParamStruct *crateParams, Boolean *mode);   

SAMPIC256CH_ErrCode  	SAMPIC256CH_SetSampicAutoINLCalibParams			(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, Boolean enDigitalLevel, int freq, int risingEdgeSlope, int fallingEdgeSlope, int highLevel, int lowLevel);
SAMPIC256CH_ErrCode  	SAMPIC256CH_GetSampicAutoINLCalibParams			(CrateParamStruct *crateParams, int feBoard, int sampicIndex, Boolean *enDigitalLevel, int *freq, int *risingEdgeSlope, int *fallingEdgeSlope,int *highLevel, int *lowLevel);   


SAMPIC256CH_ErrCode  	SAMPIC256CH_SetControlBoardEnableTrigger	(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, Boolean mode);

#endif
