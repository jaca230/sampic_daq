#ifndef __SAMPIC256_HARD_H
#define __SAMPIC256_HARD_H

 
#pragma pack(1)





/* ===================================================================================================== */
/* ============                   Déclaration des fonctions partagees        =========================== */
/* ===================================================================================================== */

void 						Init_CrateInfoStruct 			(CrateInfoStruct *crateInfoParams); 
void						Init_CrateParamStruct			(CrateInfoStruct *crateInfoParams,CrateParamStruct *crateParams);

void 						Init_CrateCalibParams			(CrateInfoStruct *crateInfoParams);   

void 						dsleep							(int msec); 

int 						Get_FeBoardIndexFromFramePathIndex	(CrateInfoStruct *crateInfoParams, int framePathIndex);


SAMPIC256CH_ErrCode			Purge_ControlBufferChain 		(int deviceHandle);   // raw purging of the buffers
SAMPIC256CH_ErrCode			Purge_DAQBufferChain 			(int deviceHandle,void *buffer, ML_Frame *mf_array, int maxloop);   // reading remaining frames
SAMPIC256CH_ErrCode			Purge_AllDaqBuffersChain 		(CrateInfoStruct *crateInfoParams, int maxloop);

SAMPIC256CH_ErrCode			Check_FeBoardsInSystem			(CrateInfoStruct *crateInfoParams);
SAMPIC256CH_ErrCode			Check_SystemType 				(CrateInfoStruct *crateInfoParams);  

SAMPIC256CH_ErrCode 		Load_DACs						(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, DacType_t dacToLoad, int feBoardIndex, int feFPGAIndex);
SAMPIC256CH_ErrCode 		Load_HardwareSetup				(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, BoardRegType_t toBeLoaded,  int feBoardIndex, int feFPGAIndex, int channelIndex);
SAMPIC256CH_ErrCode 		Load_SampicSCReg				(CrateInfoStruct *crateInfoParams, int feBoardIndex,int feFpgaIndex, unsigned int sampicSCRegAdd, unsigned int sampicSCRegValue); 

SAMPIC256CH_ErrCode			Load_FeBoardSi5332DefaultConfig     	(CrateInfoStruct *crateInfoParams, int feBoardIndex);
SAMPIC256CH_ErrCode			Load_ControlBoardSi5332DefaultConfig    (CrateInfoStruct *crateInfoParams); 

SAMPIC256CH_ErrCode 		Load_ControlBoard_Si5332Reg				(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams);
SAMPIC256CH_ErrCode 		Load_FeBoard_Si5332Reg					(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoardIndex); 

void 						Build_ControlBoardClockReg				(CrateParamStruct *crateParams);
void						Build_ControlBoardTriggerReg			(CrateParamStruct *crateParams);
void						Build_ControlBoardControlReg			(CrateParamStruct *crateParams); 
void 						Build_ControlBoardControlReg2			(CrateParamStruct *crateParams);     
void						Build_ControlBoardTriggerCombiLogicForL3Reg  (CrateParamStruct *crateParams);  


void 						Build_FeBoardsCtrlFpgaControlReg 				(CrateParamStruct *crateParams);
void 						Build_FeBoardsCtrlFpgaTriggerReg 				(CrateParamStruct *crateParams);
void						Build_FeBoardsCtrlFpgaControlReg2				(CrateParamStruct *crateParams);
void						Build_FeBoardCtrlFpgaTriggerCombiLogicForL2Reg  (CrateParamStruct *crateParams); 

void 						Build_FeBoardsFeFpgasConvertLength 		(CrateParamStruct *crateParams);  
void 						Build_FeBoardsFeFpgasControlReg 		(CrateParamStruct *crateParams);  
void 						Build_FeBoardsFeFpgasPulserReg			(CrateParamStruct *crateParams); 
void						Build_FeBoardsFeFpgasL2CoincidenceReg	(CrateParamStruct *crateParams);


void						Build_FeBoardsSampicsConfigReg3			(CrateParamStruct *crateParams); 
void 						Build_FeBoardsSampicsConfigReg2			(CrateParamStruct *crateParams);
void 						Build_FeBoardsSampicsConfigReg1			(CrateParamStruct *crateParams);
void 						Build_FeBoardsSampicsConfigReg4			(CrateParamStruct *crateParams);  
void 						Build_FeBoardsSampicsConfigReg5			(CrateParamStruct *crateParams); 
void 						Build_FeBoardsSampicsConfigReg6			(CrateParamStruct *crateParams); 
void 						Build_FeBoardsSampicsConfigReg7			(CrateParamStruct *crateParams); 
void 						Build_FeBoardsSampicsConfigReg8			(CrateParamStruct *crateParams);
void 						Build_FeBoardsSampicsConfigReg9			(CrateParamStruct *crateParams); 
void 						Build_FeBoardsSampicsConfigReg10		(CrateParamStruct *crateParams); 
void 						Build_FeBoardsSampicsChannelRegs 		(CrateParamStruct *crateParams); 


SAMPIC256CH_ErrCode			Send_SoftTrig					(CrateInfoStruct *crateInfoParams);  
SAMPIC256CH_ErrCode 		Send_PulseCmd					(CrateInfoStruct *crateInfoParams); 

SAMPIC256CH_ErrCode			Reset_ExtTrig 					(CrateInfoStruct *crateInfoParams);
SAMPIC256CH_ErrCode 		Reset_SAMPICSlowControl 		(CrateInfoStruct *crateInfoParams); 
SAMPIC256CH_ErrCode			Reset_SAMPICDLL					(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams);
SAMPIC256CH_ErrCode 		Run_Reset 						(CrateInfoStruct *crateInfoParams, ResetType_t resetType);  

SAMPIC256CH_ErrCode 		Resync_System 					(CrateInfoStruct *crateInfoParams);   


void 						Compute_SAMPICIndividualInternalThresholds	(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams);

void 						Compute_NbOfExtraWords 					  	(CrateParamStruct *crateParams);
void 						Compute_ConvertLength 						(CrateParamStruct *crateParams);

int							Convert_FromGrayToBinary 			(int grayValue, int nbOfBits);
//int 						Convert_VdacRamp_to_Nb_Of_Bits	    (int feIndex, float vdacRamp);
float 						Convert_ADCNbOfBitstoVdacRamp 		(CrateInfoStruct *crateInfoParams, int feBoardIndex, int feFPGAIndex, int nbOfBits);


void 						Reset_HitInEvent							(EventStruct *event,int hitNumber);


int							Calculate_10BitsDACIntValues				(float rawValue);
int 						Calculate_16BitsDACIntValues				(float rawValue, float SAMPIC_DAC_TYPE); 
int 						Calculate_12BitsDACIntValues				(float rawValue); 

float 						Calculate_10BitsDACFloatValue				(int intValue);
float 						Calculate_16BitsDACFloatValue				(int intValue);
float						Calculate_12BitsDACFloatValue   			(int intValue);  

float 						Convert_VDac_Reset_To_VBaseline				(float vdac);
float 						Convert_VBaseline_to_VDac_Reset 			(float vbaseline); 
float 						Convert_TOTFilterWidthToTOTFilterDAC		(Boolean wideCap,float filterWidth /* ns*/);  
float 						Convert_TOTFilterDACtotTOTFilterWidth		(Boolean wideCap, float filterDAC /* Volts */);   



void 						Wait_Milliseconds							(int nbOfMilliseconds); 
int 						Get_TOTCurrentRampDacIndex					(float totRampCurrentDac);    

void						Correct_AdcLinearity						(CrateInfoStruct *crateInfoParams, HitStruct *hit, Boolean smartRead);
void						Correct_TimeLinearity						(CrateInfoStruct *crateInfoParams, HitStruct *hit);
void 						Correct_ResiudalPedestals 					(CrateInfoStruct *crateInfoParams, HitStruct *hit);
void 						Remap_ChannelsForPingPong					(CrateInfoStruct *crateInfoParams, HitStruct *hit);



void 						Save_INLValues								(void); 

//int 						Read_ResidualPedestals_From_Files				(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, char directory[MAX_PATHNAME_LENGTH]);
int 						Read_INLValues_From_Files						(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, char directory[MAX_PATHNAME_LENGTH]);
int 						Read_InternalTriggerThresholdOffset_From_Files	(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, char directory[MAX_PATHNAME_LENGTH]);
int							Read_ADCLinearityCalibValues_From_Files 		(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, char directory[MAX_PATHNAME_LENGTH]);
int							Read_ADCRampCalibValues_From_Files			    (CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, char directory[MAX_PATHNAME_LENGTH]); 
int 						Read_TOTCalibValues_From_Files					(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, char directory[MAX_PATHNAME_LENGTH]);
void 						Read_AllCalibValuesFromFiles					(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, char directory[MAX_PATHNAME_LENGTH]);
 
																					

// EEPROM

SAMPIC256CH_ErrCode 		Read_ControlBoard_EPROM_Info 					(CrateInfoStruct *crateInfoParams);
SAMPIC256CH_ErrCode 		Read_FeBoard_CTRL_EEPROM_Info 					(CrateInfoStruct *crateInfoParams,int feBoardIndex);
SAMPIC256CH_ErrCode 		Read_FrontEndBlock_EEPROM_Info 					(CrateInfoStruct *crateInfoParams,int feBoardIndex, int feBlockIndex); 

//ErrorType 				Write_TOTDacOffsetToEEPROM (int feIndex);
//ErrorType 				Write_TOTFilterDacOffsetToEEPROM (int feIndex);
//
//
//void 						Read_Calib_Values_From_EEPROM			(int sampicIndex);
//void 						Save_Calib_Files_From_EEPROM			(int sampicIndex); 
//void 						Read_All_Calib_Values_From_EEPROM		(void);
//
//																		  


/* =============================================================================================== */
/* =============================================================================================== */
#pragma pack()  

#endif
