/***************************************************************
 *	File	: 	SAMPIC_256Ch_hardware_core.c				 	*
 *																*
 *	Date	:	Juillet 2017   									*
 *																*
 *	Author	:	Jihane    Maalmi IJCLAB					        *
 *	            				                                *
 *								                                *
 ***************************************************************/

#ifdef _WINDOWS
 #include "windows.h"
#else
 #include <malloc.h>  
#endif

#include "lpDevC.h"

#ifdef __LINUX_BUILD
	#include <math.h>
	#include <string.h>
	#include <stdlib.h>
#else
	#include <userint.h>
	#include <ansi_c.h>
#endif

#include "SAMPIC_256Ch_Type.h"
#include "SAMPIC_256Ch_lib.h"
#include "SAMPIC_256Ch_hardware_core.h" 



/* ============================================================================ */
/* ========================== Fonctions Statiques ============================= */


#pragma pack(1) 


/* ============================================================================ */
/* ========================== Variables Globales ============================== */
/* ============================================================================ */

  static Si5332_I2CRegAccessStruct const SI5332E_DEFAULT_CONFIG[SI5332_GM2_REVD_REG_CONFIG_NUM_REGS] =   // Oscillateur interne à 50 MHz, sorties actives (0, 1, 2 et 7) à 200 MHz.  
{

	/* Start configuration preamble */
	/*    Set device in Ready mode */
	{ 0x0006, 0x01 },
	/* End configuration preamble */

	/* Start configuration registers */
	{ 0x0017, 0x53 },
	{ 0x0018, 0x50 },
	{ 0x0019, 0x56 },
	{ 0x001A, 0x32 },
	{ 0x001B, 0x5F },
	{ 0x001C, 0x31 },
	{ 0x0021, 0x6A },
	{ 0x0024, 0x01 },   
	{ 0x0025, 0x00 },
	{ 0x0026, 0x00 },
	{ 0x0027, 0x00 },
	{ 0x0028, 0x00 },
	{ 0x0029, 0x00 }, 
	{ 0x002A, 0x00 },
	{ 0x002B, 0x18 },
	{ 0x003C, 0x00 },
	{ 0x0048, 0x00 },
	{ 0x0054, 0x00 },
	{ 0x0060, 0x00 },
	{ 0x0067, 0x18 },
	{ 0x0068, 0x00 },
	{ 0x0069, 0x00 },
	{ 0x006A, 0x00 },
	{ 0x006B, 0x00 },
	{ 0x006C, 0x01 },
	{ 0x0073, 0x01 },
	{ 0x0074, 0x03 },
	{ 0x0075, 0x01 },
	{ 0x007A, 0x06 },
	{ 0x007B, 0x01 },
	{ 0x007C, 0x00 },
	{ 0x007D, 0x00 },
	{ 0x007F, 0x06 },
	{ 0x0080, 0x01 },
	{ 0x0081, 0x00 },
	{ 0x0082, 0x00 },
	{ 0x0089, 0x06 },
	{ 0x008A, 0x01 },
	{ 0x008B, 0x00 },
	{ 0x008C, 0x00 },
	{ 0x008E, 0x06 },
	{ 0x0090, 0x00 },
	{ 0x0091, 0x00 },
	{ 0x0098, 0x06 },
	{ 0x0099, 0x01 },  /* ajouté pour bloc de sortie N°4 */ 
	{ 0x009A, 0x00 },
	{ 0x009B, 0x00 },
	{ 0x009D, 0x06 },
	{ 0x009F, 0x00 },
	{ 0x00A0, 0x00 },
	{ 0x00A7, 0x06 },
	{ 0x00A8, 0x01 }, /* ajouté pour bloc de sortie N°6 */    
	{ 0x00A9, 0x00 },  
	{ 0x00AA, 0x00 },
	{ 0x00AC, 0x01 },
	{ 0x00AD, 0x01 },
	{ 0x00AE, 0x00 },
	{ 0x00AF, 0x00 },
	{ 0x00B0, 0x00 }, 
	{ 0x00B6, 0x4B },
	{ 0x00B7, 0x06 },
	{ 0x00B9, 0x00 },
	{ 0x00BA, 0x7E },
	{ 0x00BB, 0x00 },
	{ 0x00BC, 0x90 },
	{ 0x00BD, 0x00 },
	{ 0x00BE, 0x20 },
	/* End configuration registers */

	/* Start configuration postamble */
	/*    Set device in Active mode */
	{ 0x0006, 0x02 }
	/* End configuration postamble */

};

  
 static Si5332_I2CRegAccessStruct const SI5332E_DEFAULT_FE_CONFIG[SI5332_FE_GM2_REVD_REG_CONFIG_NUM_REGS] =
{

	/* Start configuration preamble */
	/*    Set device in Ready mode */
	{ 0x0006, 0x01 },
	/* End configuration preamble */

	/* Start configuration registers */
	{ 0x0017, 0x53 },
	{ 0x0018, 0x50 },
	{ 0x0019, 0x56 },
	{ 0x001A, 0x32 },
	{ 0x001B, 0x5F },
	{ 0x001C, 0x31 },
	{ 0x0024, 0x01 },
	{ 0x0025, 0x72 },
	{ 0x0026, 0x72 },
	{ 0x0027, 0x72 },
	{ 0x0028, 0x72 },
	{ 0x0029, 0x72 },
	{ 0x003C, 0x00 },
	{ 0x0048, 0x00 },
	{ 0x0054, 0x00 },
	{ 0x0060, 0x00 },
	{ 0x0073, 0x01 },
	{ 0x0074, 0x03 },
	{ 0x007A, 0x06 },
	{ 0x007B, 0x01 },
	{ 0x007C, 0x00 },
	{ 0x007D, 0x00 },
	{ 0x007F, 0x06 },
	{ 0x0080, 0x01 },
	{ 0x0081, 0x00 },
	{ 0x0082, 0x00 },
	{ 0x0089, 0x06 },
	{ 0x008A, 0x01 },
	{ 0x008B, 0x00 },
	{ 0x008C, 0x00 },
	{ 0x008E, 0x07 },
	{ 0x0090, 0x00 },
	{ 0x0091, 0x00 },
	{ 0x0098, 0x06 },
	{ 0x0099, 0x01 },
	{ 0x009A, 0x00 },
	{ 0x009B, 0x00 },
	{ 0x009D, 0x07 },
	{ 0x009F, 0x00 },
	{ 0x00A0, 0x00 },
	{ 0x00A7, 0x06 },
	{ 0x00A8, 0x01 },
	{ 0x00A9, 0x00 },
	{ 0x00AA, 0x00 },
	{ 0x00AC, 0x01 },
	{ 0x00AE, 0x00 },
	{ 0x00AF, 0x00 },
	{ 0x00B0, 0x00 },
	{ 0x00B6, 0x4B },
	{ 0x00B7, 0x02 },
	{ 0x00B9, 0x20 },
	{ 0x00BB, 0x20 },
	{ 0x00BC, 0x90 },
	{ 0x00BD, 0x04 },
	/* End configuration registers */

	/* Start configuration postamble */
	/*    Set device in Active mode */
	{ 0x0006, 0x02 },
	/* End configuration postamble */

};
/* ============================================================================ */
/* ========================== Variables Externes ============================== */
/* ============================================================================ */



#pragma pack()  

/* ============================================================================ */
/* ================= Fonctions de Delai ====================== */
/* ============================================================================ */

 void dsleep(int msec) 
 {
#ifdef _WINDOWS
  Sleep(msec);
#else
  usleep(msec* 1000);
 #endif
	
 }

/* ============================================================================ */
/* ================= Low level functions                 ====================== */
/* ============================================================================ */

 
 /* ============================================================================ */
SAMPIC256CH_ErrCode Purge_ControlBufferChain 		(int deviceHandle)
/* ============================================================================ */
 {
	SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success; 
	
	Boolean purgeOk = TRUE;
	
//	while(purgeOk == TRUE) // à verifier
//	{
		
//		dsleep(10);
		purgeOk = LPD_PurgeBuffers(deviceHandle);	   
			
//	}
	 
	return errCode;
}

/* ============================================================================ */
SAMPIC256CH_ErrCode Purge_DaqBufferChain 	(int deviceHandle,void *buffer, ML_Frame *mf_array, int maxloop)
/* ============================================================================ */
 {
	SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success; 
	
	Boolean purgeOk = TRUE;
	int nframes = 1;
	int n = 0;
	
    if(deviceHandle < 0)
		return SAMPIC256CH_InvalidHandle;
	
	
	while(nframes != 0)
	{
		if((maxloop > 0) && (n > maxloop))
			break;
	
		dsleep(10);
		errCode = SAMPIC256CH_BusReadExtended (deviceHandle, buffer, mf_array, MAX_BYTES_TO_READ, &nframes);      	   
		
		n++;
		
	}
	 
	if(nframes != 0)
		 errCode =  SAMPIC256CH_PurgeBufferIncomplete;
	else
		errCode = SAMPIC256CH_Success;
	
	return errCode;
}

 /* ============================================================================ */
SAMPIC256CH_ErrCode Purge_AllDaqBuffersChain 	(CrateInfoStruct *crateInfoParams , int maxloops)
/* ============================================================================ */
{
	
void *tmpEventBuf;
ML_Frame *tmpMF_array;
int feBoard;

SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success;  
	
 errCode = SAMPIC256CH_AllocateEventMemory(&tmpEventBuf,&tmpMF_array);
	
  if(crateInfoParams->ConnectionInfo.ControlBoardControlType == CTRL_AND_DAQ)
  {
	  
	if(errCode == SAMPIC256CH_Success) errCode = Purge_DaqBufferChain(crateInfoParams->ConnectionInfo.CtrlDeviceHandle,tmpEventBuf,tmpMF_array, maxloops);  	  
  }
  else // control Only 
  {
	 for(feBoard = 0; feBoard < crateInfoParams->NbOfFeBoards; feBoard++)
   	 {  
	  
		if(errCode == SAMPIC256CH_Success) errCode  = Purge_DaqBufferChain(crateInfoParams->ConnectionInfo.DaqHandle[feBoard],tmpEventBuf,tmpMF_array, maxloops); 
	 }
  }
   					   
 
 if(errCode == SAMPIC256CH_Success) errCode  = SAMPIC256CH_FreeEventMemory(&tmpEventBuf, &tmpMF_array);
 
 return errCode;
 

}

  
/* ============================================================================ */
/* ================= Init functions                      ====================== */
/* ============================================================================ */
 
 
/* ============================================================================ */
/* ================= Fonctions d'initialisation  ====================== */
/* ============================================================================ */

/* ============================================================================ */
void 		Init_CrateInfoStruct 			(CrateInfoStruct *crateInfoParams)
/* ============================================================================ */
{
	int i , feFpgaIndex;
//
	crateInfoParams->ConnectionInfo.ConnectionType = NO_CONNECTION;
	crateInfoParams->ConnectionInfo.ControlBoardControlType = CTRL_AND_DAQ;	
	strcpy(crateInfoParams->ConnectionInfo.UsbDeviceDescr,"");;
	strcpy(crateInfoParams->ConnectionInfo.UsbSerNum, "");
	crateInfoParams->ConnectionInfo.CtrlDeviceHandle= -1;
	crateInfoParams->ConnectionInfo.NbOfDAQConnections = 0;
	
	strcpy(crateInfoParams->ConnectionInfo.CtrlIpAddress, "");
	
	crateInfoParams->ConnectionInfo.CtrlPort = 0;
	
	for(i = 0; i <MAX_NB_OF_FE_BOARDS; i++)
	{
		crateInfoParams->ConnectionInfo.DaqHandle[i]= -1;
		strcpy(crateInfoParams->ConnectionInfo.DaqIpAddress[i], "");
		crateInfoParams->ConnectionInfo.DaqPort[i] = 0;
	}


	for(i= 0;  i <MAX_NB_OF_FE_BOARDS; i++)     
		crateInfoParams->FrontEndBoardsPathIndex[i]=i;
	
	crateInfoParams->SystemType = SAMPIC_BYPASS;
	crateInfoParams->SampicVersion = DEFAULT_SAMPIC_VERSION;
	
	crateInfoParams->NbOfFeBoards = 0;
	
	crateInfoParams->CrateBoardsInfo.ControlBoardInfo.BoardVersion = 1;
	crateInfoParams->CrateBoardsInfo.ControlBoardInfo.BoardSerNum = 1; 
	
	crateInfoParams->CrateBoardsInfo.ControlBoardInfo.Si5332IsPresent = FALSE;
	
	strcpy(crateInfoParams->CrateBoardsInfo.ControlBoardInfo.IpAddressFromEEPROM, "");
	crateInfoParams->CrateBoardsInfo.ControlBoardInfo.CtrlPortFromEEPROM =  0;
	crateInfoParams->CrateBoardsInfo.ControlBoardInfo.DaqPortFromEEPROM = 0;
	
	crateInfoParams->CrateBoardsInfo.ControlBoardInfo.FirmwareVersion.BoardVersion= 0;
	crateInfoParams->CrateBoardsInfo.ControlBoardInfo.FirmwareVersion.FPGAVersion= 0; 
	crateInfoParams->CrateBoardsInfo.ControlBoardInfo.FirmwareVersion.FPGAEvolution= 0; 
	crateInfoParams->CrateBoardsInfo.ControlBoardInfo.FeBoardsPresence = 0x00;
	
	
	for(i = 0; i < MAX_NB_OF_FE_BOARDS; i++)
	{
		
		
		crateInfoParams->CrateBoardsInfo.FeBoardInfo[i].BoardVersion = 1;
		crateInfoParams->CrateBoardsInfo.FeBoardInfo[i].BoardSerNum =  1;
		crateInfoParams->CrateBoardsInfo.FeBoardInfo[i].Si5332IsPresent =  FALSE; 
		// FEB CONTROL FPGA 
		crateInfoParams->CrateBoardsInfo.FeBoardInfo[i].ControlFpgaFirmwareVersion.BoardVersion = 0;
		crateInfoParams->CrateBoardsInfo.FeBoardInfo[i].ControlFpgaFirmwareVersion.FPGAVersion = 0;  
		crateInfoParams->CrateBoardsInfo.FeBoardInfo[i].ControlFpgaFirmwareVersion.FPGAEvolution = 0;
		
		strcpy(crateInfoParams->CrateBoardsInfo.FeBoardInfo[i].IpAddressFromEEPROM, "");
		crateInfoParams->CrateBoardsInfo.FeBoardInfo[i].CtrlPortFromEEPROM =  0;
  	    crateInfoParams->CrateBoardsInfo.FeBoardInfo[i].DaqPortFromEEPROM = 0;
 
	 
		// FEB FE FPGA
		for(feFpgaIndex = 0; feFpgaIndex <NB_OF_FE_FPGAS_IN_FE_BOARD; feFpgaIndex++) 
		{
			crateInfoParams->CrateBoardsInfo.FeBoardInfo[i].FeFpgaFirmwareVersion[feFpgaIndex].BoardVersion = 0;
			crateInfoParams->CrateBoardsInfo.FeBoardInfo[i].FeFpgaFirmwareVersion[feFpgaIndex].FPGAVersion = 0; 
			crateInfoParams->CrateBoardsInfo.FeBoardInfo[i].FeFpgaFirmwareVersion[feFpgaIndex].FPGAEvolution = 0; 
			
			
			crateInfoParams->CrateBoardsInfo.FeBoardInfo[i].FeBoardType[feFpgaIndex]  =  SAMPIC_BYPASS_BOARD;
			crateInfoParams->CrateBoardsInfo.FeBoardInfo[i].FeBlockVersion[feFpgaIndex] = DEFAULT_FE_BLOCK_VERSION;
			crateInfoParams->CrateBoardsInfo.FeBoardInfo[i].FeBlockSerNum[feFpgaIndex] = 0;
			crateInfoParams->CrateBoardsInfo.FeBoardInfo[i].SampicVersion[feFpgaIndex]  =  DEFAULT_SAMPIC_VERSION; 
			crateInfoParams->CrateBoardsInfo.FeBoardInfo[i].SampicEvolution[feFpgaIndex]  =  DEFAULT_SAMPIC_EVOLUTION; 
	
		}
			 
	
	

	}
		
}


/* ============================================================================ */
void		Init_CrateParamStruct			(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams)
/* ============================================================================ */
{
	
		
	int channel, feBoardIndex, feFpgaIndex;	
	
	
	crateParams->CommonParams.UseExternalClock = FALSE;
	crateParams->CommonParams.FreqEch = DEFAULT_FREQ_ECH;
	crateParams->CommonParams.EnPulseMode = FALSE;
	crateParams->CommonParams.EnPulseReSync = FALSE;

	crateParams->CommonParams.EnableAutoConversionMode = TRUE ;
	crateParams->CommonParams.EnableTOTMeasurement = FALSE;
	crateParams->CommonParams.EnablePingPongMode = FALSE;
	crateParams->CommonParams.ADCNbOfBits = DEFAULT_ADC_NB_OF_BITS;
	crateParams->CommonParams.EnableBuildL2 = FALSE;
	
	if(crateInfoParams->SystemType != SAMPET_SYSTEM)
	 crateParams->CommonParams.NbOfSamplesToRead = MAX_NB_OF_SAMPLES;
	else
	 crateParams->CommonParams.NbOfSamplesToRead = DEFAULT_SAMPET_NB_OF_SAMPLES;

	if(crateInfoParams->SystemType != SAMPET_SYSTEM)
	{
		crateParams->CommonParams.SmartRead = FALSE;
		crateParams->CommonParams.OffsetForStartOfRead= 0;  // ND: 6-Bits 

	}
	else
	{
		crateParams->CommonParams.SmartRead = TRUE; 
		crateParams->CommonParams.OffsetForStartOfRead= DEFAULT_SAMPET_OFFSET_FOR_START_OF_READ;  // ND: 6-Bits 

	}


	crateParams->CommonParams.ConvertDelay = 0; 

	crateParams->CommonParams.NbOfFramesPerBlock = 1; 
	crateParams->CommonParams.NbOfTriggersPerEvent = DEFAULT_NB_OF_TRIGGERS_PER_TRIGGER_EVENT;
	crateParams->CommonParams.L2CoincidenceLatencyLength = DEFAULT_LEVEL_2_LATENCY_GATE;
	crateParams->CommonParams.L2SAMPICPrimitivesGateLength = DEFAULT_PRIMITIVES_GATE_LENGTH;
	
	  	
	// Control Board FPGA
	crateParams->ControlBoardParams.EnableFlowControl = FALSE;
	
	crateParams->ControlBoardParams.EnableTrigger = FALSE;
	
	crateParams->ControlBoardParams.EnableExtTrigCounter  = FALSE;
	crateParams->ControlBoardParams.EnableDetectTriggerIDfromExtTrig  = FALSE;
	
	crateParams->ControlBoardParams.NbOfTriggersPerEvent = DEFAULT_NB_OF_TRIGGERS_PER_TRIGGER_EVENT;
		
		 // Trigger reg
	crateParams->ControlBoardParams.ExternalTriggerType = SOFTWARE; 
	crateParams->ControlBoardParams.ExternalTriggerEdge = RISING_EDGE;
	crateParams->ControlBoardParams.ExternalTriggerSigLevel = TTL_SIG;  
	
	crateParams->ControlBoardParams.ExternalSyncEdge = RISING_EDGE;    
	crateParams->ControlBoardParams.ExternalSyncSigLevel = TTL_SIG; 
	
	//Pulse Reg
	crateParams->ControlBoardParams.PulseReg  = DEFAULT_AUTO_PULSE_PERIOD -1;
	
		// Ext Trig Gate

	crateParams->ControlBoardParams.ExternalTrigGateForL3  = DEFAULT_EXT_TRIG_GATE;
		
	crateParams->ControlBoardParams.EnableCoincidenceWithExtTrigGateForL3 = FALSE;   // bit 0
	
	crateParams->ControlBoardParams.CrateIsInStandaloneMode = TRUE;
	crateParams->ControlBoardParams.CrateIsInMasterSyncMode = TRUE; 
	crateParams->ControlBoardParams.EnableMasterSlaveCoincidence  = FALSE;


	crateParams->ControlBoardParams.EnableBuildL3 = FALSE;   // bit 26

	crateParams->ControlBoardParams.SelFeb0ForL3 = 0; 	//bits 0-1
	crateParams->ControlBoardParams.SelFeb1ForL3 = 1; 	//bits 2-3
	crateParams->ControlBoardParams.SelFeb2ForL3 = 2; 	//bits 4-5
	crateParams->ControlBoardParams.SelFeb3ForL3 = 3; 	//bits 6-7
	crateParams->ControlBoardParams.Layer1TriggerLogic0 = LOGIC_AND; // bits 8-10
	crateParams->ControlBoardParams.Layer1TriggerLogic1 = LOGIC_AND; // bits 11-13
	crateParams->ControlBoardParams.Layer1TriggerLogic2 = LOGIC_AND; // bits 14-16
	crateParams->ControlBoardParams.Layer2TriggerLogic0 = LOGIC_AND; // bits 17-19
	crateParams->ControlBoardParams.Layer2TriggerLogic1 = LOGIC_AND; // bits 20-22
	crateParams->ControlBoardParams.Layer3TriggerLogic	= LOGIC_AND; // bits 23-25

	Build_ControlBoardControlReg2(crateParams);	
	Build_ControlBoardClockReg(crateParams);
	Build_ControlBoardTriggerReg(crateParams);	
	Build_ControlBoardControlReg(crateParams);
	Build_ControlBoardTriggerCombiLogicForL3Reg(crateParams);
	 
	//Front End Boards
	 
   		//  FE Boards Control FPGA Params
  	for(feBoardIndex = 0; feBoardIndex < MAX_NB_OF_FE_BOARDS; feBoardIndex++)
  	{
	   	
		crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.FeBoardsPresence = 0x0F;    // 64ch boards all 'front end blocks' are present
		
		crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.EnableTrigger  = FALSE;	
		crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.EnablePulserMode  = FALSE;
		crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.EnableClockOnSync = FALSE;
		crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.Sel_Async_Ext_Trig_Source = EXT_SIGNAL;		
																		  
		crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.EnableCoincidenceWithExtTrigGateForL2 = FALSE;
		crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.ExternalTriggerType = EXT_SIG; 
		crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.ExternalTriggerEdge = RISING_EDGE;
		crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.ReaqReqDelay = 10;  
	
	
		crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.ExternalTrigGateForL2 = DEFAULT_EXT_TRIG_GATE;
		
		crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.ClockRegister = 0x7000;    //External clock 
		
		crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.Si5332OMux0_Cell  = 0x72; //External clock   
	

		crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.FEBGlobalTriggerIsL3 = FALSE; //bit 1, if '0' => FEB Global Trigger is L2 else  FEB_GT is L3 
		
	  	crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.SelSAMPIC0ForL2 = 0; //bits 0-1
		crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.SelSAMPIC1ForL2 = 1; //bits 2-3
		crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.SelSAMPIC2ForL2 = 2; //bits 4-5
		crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.SelSAMPIC3ForL2 = 3; //bits 6-7
		crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.Layer1TriggerLogic0 = LOGIC_AND; // bits 8-10
		crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.Layer1TriggerLogic1 = LOGIC_AND; // bits 11-13
		crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.Layer1TriggerLogic2 = LOGIC_AND; // bits 14-16
		crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.Layer2TriggerLogic0 = LOGIC_AND; // bits 17-19
		crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.Layer2TriggerLogic1 = LOGIC_AND; // bits 20-22
		crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.Layer3TriggerLogic  = LOGIC_AND; 	   // bits 23-25

			
		
	// FE Boards FE FPGA Params 

	    for(feFpgaIndex = 0; feFpgaIndex < NB_OF_FE_FPGAS_IN_FE_BOARD; feFpgaIndex++)
		{	
			  crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].EnableTrigFPGA = FALSE;
			  crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].EnableTrigSampic = FALSE;
			  crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].EnableExtTrigGate = FALSE;
		  	  crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].EnExtTrisAsEnableTrig  = FALSE;
			  crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].UdpOk = FALSE;  // Attention! vérifier à quoi sert le bit pour les horloges!
			  crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].EnableExtTrigCounter = FALSE;

			  crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].RstDLL = FALSE;   //TEMP
			  crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].EnableAsyncExtTrig = TRUE;
		 
			  for(channel = 0; channel < NB_OF_CHANNELS_IN_SAMPIC; channel++)
				 crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].EnablePulseChannel[channel] = FALSE;
		  
		 	  crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].PulserSourceIsSync  = TRUE; // pulse source is Control Board
		  
			  crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].PulserWidth =  DEFAULT_PULSER_WIDTH; 
		   
			  crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].ClkEdgeForDataSampling = FALLING_EDGE;
		  
			  crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].EnableOrTriggerMode = FALSE;
			  crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].EnableL2Coincidence = FALSE;
			  crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].EnableDetectTriggerIDfromExtTrig = FALSE;
	 
		  	  crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].ExternalTrigGate = DEFAULT_EXT_TRIG_GATE ;   
  
		  }
	
   
		// SAMPIC INDIVIDUAL PARAMs

	    for(feFpgaIndex = 0; feFpgaIndex < NB_OF_FE_FPGAS_IN_FE_BOARD; feFpgaIndex++)
	    {	
	   
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.ExternalThreshold = 0.1; //Volts
			if(	crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoardIndex].SampicVersion[feFpgaIndex] == 5)
				crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.Vdac_Rosc = 1.0; // 0.95V pour avoir une fréquence de 1.33 GHz     	
			else
		    	crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.Vdac_Rosc = 0.95;
				
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.Vdac_StartRamp = 0.0;
		
			  // sampic V3
			//crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.Vdac_DLL = 1.1;    	
			if(	(crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoardIndex].SampicVersion[feFpgaIndex] == 3) && (crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoardIndex].SampicEvolution[feFpgaIndex] == 'C'))
				crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.Vdac_DLL = DEFAULT_SAMPIC_V3_C_VDAC_DLL;
			else if((crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoardIndex].SampicVersion[feFpgaIndex] == 3) && (crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoardIndex].SampicEvolution[feFpgaIndex] == 'D'))
				 crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.Vdac_DLL = DEFAULT_SAMPIC_V3_D_VDAC_DLL;
			else // > 3
			{
				if(crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoardIndex].FeBoardType[feFpgaIndex] == SAMPIC_SLOW_BOARD)
					 crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.Vdac_DLL = DEFAULT_SAMPIC_V5_SLOW_VDAC_DLL;
				else
				     crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.Vdac_DLL = DEFAULT_SAMPIC_V3_D_VDAC_DLL; 	
				
			}
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.Vdac_Dacm = 0.0;
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.Vdac_Dacp = 1.8;
		
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.Vdac_ramp = DEFAULT_VDAC_RAMP_SAMPICV3_11BITS;
		
			if(crateInfoParams->SystemType != SAMPET_SYSTEM)
			{
				crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.VBaseline =  DEFAULT_SAMPIC_BASELINE;
				crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.Vdac_reset = Convert_VBaseline_to_VDac_Reset(DEFAULT_SAMPIC_BASELINE);//0.545; //ATTENTION:  à mettre égal Vbias
		
			}
			else
			{  
				crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.VBaseline = DEFAULT_SAMPET_BASELINE;
				crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.Vdac_reset = Convert_VBaseline_to_VDac_Reset(DEFAULT_SAMPET_BASELINE);
			}
		
			// Parameters for Channels Registers
 	
		    for(channel = 0; channel < NB_OF_CHANNELS_IN_SAMPIC; channel ++)
		    {
		  
			  crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.RelativeInternalThreshold[channel]  = 0.1;  	  
		  
		   
			  crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.ExtDiscriThresholdSource[channel]= FALSE;
		      crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableTriggerChannel[channel]= TRUE;
		  
		      crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.SelfTriggerOn[channel]= FALSE;
			  crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.TriggerEdge[channel] = RISING_EDGE;
		  		
		 
			  crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.ChannelTriggerMode[channel] = SAMPIC_CHANNEL_EXT_TRIGGER_MODE; 
			  crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.DisableChannelForCentralTrigger[channel]= FALSE;
		 
		
			}
	   

	//CONFIG Reg 1  (Register 17) 
	   
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.TriggerResetSource = END_OF_CONV; 
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.SelfTriggerMode = NORMAL; 
		
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.DisableDelayDLL = FALSE;
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.DLLFastMode = TRUE; 		//	1 = FAST, 	 0 = SLOW 
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.DLLOut = FALSE; 			//	1 = Enabled, 0 = Disabled
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.DisableDelayClk = TRUE ; 	// 	1 = Disabled, 0 = Enabled
	
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.ThresholdDACsPowerDown = FALSE;
		
				// Modification for SAMPIC V2 
			
		 	crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnCoincidenceModeForCentralTrigger = FALSE;
		
			//Modification for SAMPIC V3
		
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.ResetChargePumps = FALSE; // permanent reset of both charge pumps  (main DLL and Delay Servo Control)
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.ResetAllSequencers = FALSE; // Reset of all FlipFlops and Sequencers inmplied in the event conversion and readout
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.DLLMode = DLL_FAST; 
	
	
	//CONFIG Reg 2  (Register 18) 
			
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.ForceRingOscAlwaysON = FALSE;
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.ADCClockSource = RING_OSC;
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.TestOutMode = ADC_COUNTER;  // FOR TEST
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.InhibRstADC = FALSE; // !! FALSE sinon on ne comprend rien
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.InhibReadoutData = FALSE;	 
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.InhibTokenExtReset = FALSE ;
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.SelHighLVDSCurrent = FALSE ;  
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.SCDataResync = FALSE;
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.SCEdgeDataResync = FALSE;
		
			  // Modification for SAMPIC V2 
			
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EndOfADCGrayCounter = ADC_COUNTER_IS_11BITS;  // A faire : une fonction qui le calcule en fonction du nombre de bits de l'ADC
			//crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableAutoConversionMode = crateParams->CommonParams.EnableAutoConversionMode;
		
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnablePostTrigger = FALSE;
					
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.CentralTriggerEffect = TRIG_CHANNEL_ONLY_IF_PARTICIPATING_TO_CT;	
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableTripleCoincidenceMode = FALSE;
	
	
	//CONFIG Reg 3  (Register 19)
			
		    if(crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoardIndex].SampicVersion[feFpgaIndex] == 3)
			{
				crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.Vdac_DLLContinuity = 1.0; //Volts
			}
			else
			{
				if(crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoardIndex].FeBoardType[feFpgaIndex] == SAMPIC_SLOW_BOARD)
				{
					crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.Vdac_DLLContinuity = DEFAULT_SAMPIC_V5_SLOW_VDAC_CONTINUITY;	  	
					
				}
				else
				{
					crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.Vdac_DLLContinuity = DEFAULT_SAMPIC_V5_VDAC_CONTINUITY;	
				}

				
			}
				
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.DLLContinuityPowerDown = FALSE;
			
				   // Modification for SAMPIC V2 
		
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableADCCompOverflow = FALSE;
			
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableInhibComp = FALSE;  		// '1' Enables automatic inhibition of the cell comparators out of the conversion phase 
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.DisablePrimitiveDeglitcher = FALSE; // = NoShapeChannel:if '0' => raw discri used for the self trigger, '1': shaped discri used instead
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableTOTFilterWideCap = FALSE; // '1' the TOT filter ramp uses the 100fF capacitor, '0' 1pF is used instead.
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.SelGatedDiscriForCTPrimitives = TRUE;  // or EnShaperTrig	  : '1' gated primitives used for Central Trigger, '0' raw shaped discri used instead
	
		
			// Modification for SAMPIC V3
			
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableDigitalInput = FALSE; // or TRUE?  
	
	// CONFIG REG4 	(Register 20)    
			
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.InternalTOTFilterWidth = 10; // ns
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableExternalTOTFilterDAC = FALSE;
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableTOTFilter = FALSE;
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.AutoConversionDelayThird = 2; //value between 0 and 7
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnablePingPong = crateParams->CommonParams.EnablePingPongMode;
		
	
	// CONFIG REG5 	(Register 21) 
	
		    crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.TOTRange = SAMPIC_TOT_RANGE_MAX_100_NS;
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.InternalTOTRampCurrentDAC = (float)DEFAULT_SAMPIC_V3_TOT_RAMP_DAC2; // Value DAC voltage applied to an external 10k resistor to produce the current. Ramp current range is 3.5 to 50µA
	
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableExternalTOTRampCurrentDAC = FALSE;
		//	crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableTOTMeasurement = crateParams->CommonParams.EnableTOTMeasurement;
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.TourDePisteDelay= 0; //value between 0 and 7 : it is the delay between enable_write in the analog Memory and trigger enabling, units are 1/8 of the clock period
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.DisableTOTOverflow = FALSE; // '1' disables using the TOT voltage overflow to stop the TOT ramp.
	
	
	// CONFIG REG6 	(Register 22)
	
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableExternalADCRampDAC = FALSE;
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableSyncConversion = TRUE; //'1' enable synchronising the start of the ADC ramp with ADC Clock/16
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.PostTrig = 7; // between 0 and 7; Units are 1/8 of Clock Period; 7 is minium 0 is maximum
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableCommonDeadTime = FALSE;
	
	
	// CONFIG REG7 	(Register 23) 
	
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.InternalOverflowDAC = 1.0 ;// Volts
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableExternalOverflowDAC = FALSE;
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.PrimitivesGateLength = 2; // Units are 1/8 of clock Period
		
				//Modification Sampic V3
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableDataResynchro = TRUE; // Test Jiji
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.InhibReloadRampData = TRUE; // inhibits the reload of the channel Cell Latches in case of conversion overflow
	
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.ReloadRampSrce = RAMP_OVERFLOW; // if '0' the srce of the reload of the channel cell latches is the overflow of the counter, '1': the srce is the overflow of the ramp
	
	
	// CONFIG REG8 	(Register 24) 
	
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.InternalRoscDAC = crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.Vdac_Rosc;
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableExternalRoscDAC = FALSE;
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.TimeINLCalibClockFreq = NO_CLOCK;
		
			//Modification Sampic V3
	
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.TimeINLCalibClockFreqV3 = V3_NO_CLOCK;
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableExternalVreset = TRUE;
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableTranslator = FALSE;  

	
	// CONFIG REG9 	(Register 25) 
	
			if(crateInfoParams->SystemType != SAMPET_SYSTEM)
				crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.InternalADCCalibDAC = DEFAULT_SAMPIC_BASELINE; //Min Value//  DAC for setting the voltage used by the ADC Calibration
			else
				crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.InternalADCCalibDAC = DEFAULT_SAMPET_BASELINE; //Min Value//  DAC for setting the voltage used by the ADC Calibration
		
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.InternalADCCalibDACIntValue = Calculate_10BitsDACIntValues(crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.InternalADCCalibDAC);
	
			if((crateInfoParams->SystemType == SAMPET_SYSTEM) || (crateInfoParams->SystemType == SAMPIC_BYPASS))
			{
				crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableDiffN = TRUE;
				crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableDiffP  = TRUE;
				crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableReturnBuffer = FALSE; 
		
			}
			else
			{
			 	crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableReturnBuffer = FALSE;  
				crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableDiffN = FALSE;
				crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableDiffP = FALSE;
			}
		
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.LVDSPolarityIsNegative = FALSE;
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableInternalVReset = FALSE;
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableADCCalibDAC = FALSE;
	
	// CONFIG REG10 (Register 26) 
	
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.TimeINLCalibRisingEdgeSlope = 15;       //?? // value between 0 and 15
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.TimeINLCalibFallingEdgeSlope = 15;	  //?? // value between 0 and 15
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.TimeINLCalibVLow = 7; 				  // value between 0 and 7
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.TimeINLCalibVHigh = 0;                  // value between 0 and 7
		
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableTimeINLCalib = FALSE;
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableReturnGnd = FALSE;
			
	
			Compute_SAMPICIndividualInternalThresholds(crateInfoParams, crateParams);
	
   
		}
	
	}
	
	Compute_NbOfExtraWords(crateParams); 
	Compute_ConvertLength (crateParams);  
	
			
	Build_FeBoardsCtrlFpgaControlReg(crateParams);
	Build_FeBoardsCtrlFpgaControlReg2(crateParams);  
	Build_FeBoardsCtrlFpgaTriggerReg(crateParams);
	Build_FeBoardCtrlFpgaTriggerCombiLogicForL2Reg(crateParams);

	Build_FeBoardsFeFpgasControlReg(crateParams);
	Build_FeBoardsFeFpgasPulserReg(crateParams);
	Build_FeBoardsFeFpgasL2CoincidenceReg(crateParams);


  	Build_FeBoardsSampicsChannelRegs(crateParams);
	Build_FeBoardsSampicsConfigReg1(crateParams);
	Build_FeBoardsSampicsConfigReg2(crateParams);   
	Build_FeBoardsSampicsConfigReg3(crateParams);
	Build_FeBoardsSampicsConfigReg4(crateParams); 
	Build_FeBoardsSampicsConfigReg5(crateParams); 
	Build_FeBoardsSampicsConfigReg6(crateParams); 
	Build_FeBoardsSampicsConfigReg7(crateParams); 
	Build_FeBoardsSampicsConfigReg8(crateParams); 
	Build_FeBoardsSampicsConfigReg9(crateParams); 
	Build_FeBoardsSampicsConfigReg10(crateParams); 
	


}


/* ============================================================================ */
void	Init_CrateCalibParams			(CrateInfoStruct *crateInfoParams)
/* ============================================================================ */
{
	strcpy(crateInfoParams->CrateCalibInfo.LastCalibDirectory, ".");
	
	SAMPIC256CH_ResetResidualPedestalCalibValues (crateInfoParams, ALL_FE_BOARDs);
	SAMPIC256CH_ResetADCLinearityCalibValues(crateInfoParams, ALL_FE_BOARDs);  
	SAMPIC256CH_ResetInternalTriggerThresholdOffsetCalibValues(crateInfoParams, ALL_FE_BOARDs);  
	SAMPIC256CH_ResetADCRampCalibValues(crateInfoParams, ALL_FE_BOARDs);  
	SAMPIC256CH_ResetTOTCalibValues(crateInfoParams, ALL_FE_BOARDs);  
	SAMPIC256CH_ResetTOTDACOffsetsValues(crateInfoParams, ALL_FE_BOARDs);   
	SAMPIC256CH_ResetTimeINLCalibValues (crateInfoParams, ALL_FE_BOARDs, ALL_CHANNELs);  
	

 	 
} 



/* ============================================================================ */
/* =================  Build des registres 					  ================= */
/* ============================================================================ */
/* ======================================================================== */
void Build_ControlBoardClockReg    (CrateParamStruct *crateParams)
/* ======================================================================== */
// FreqEch va être codée dans le registre clock_register du FPGA controller
// Il est composé de 2 groupes de 4 bits destinés aux 3 diviseurs de sortie utilisés dans le MAX3639 (A et B identiques, et C)
// Le MAX3639 est commandé en logique 3 états. Pour la simuler, on utilise les bits de poids pairs pour les valeurs des bits
// et les poids impairs pour les enables des tristates de sortie du FPGA qui sont au top level du firmware
{
	
	//default values for Si5332 @ 100 MHz ( 6.4 GS/s)
	
	crateParams->ControlBoardParams.Si5332RegO0 	 = 0x18;    	// add 0x2B
	crateParams->ControlBoardParams.Si5332Idpa 		 = 0x0018;  	// add 0x68 (MSB) 0x67 (LSB)
	crateParams->ControlBoardParams.Si5332PDivider 	 = 0x01;  	// add 0x75
	crateParams->ControlBoardParams.Si5332OutDivider = 0x01;   	// add 0x7B, 0x80, 0x8A, 0x99, 0xA8
	crateParams->ControlBoardParams.Si5332PllMode 	 = 0x20;		// add 0xBE

	if(crateParams->CommonParams.UseExternalClock == TRUE)
	{
		//external clock
		
		crateParams->ControlBoardParams.ClockRegister 		= 0x3000; 
		crateParams->ControlBoardParams.Si5332OMux0_Cell  	= 0x73;   		// external clock in 2
	}
	else
	{
		crateParams->ControlBoardParams.Si5332OMux0_Cell = 0x00;
		
		
		if(crateParams->CommonParams.FreqEch == 10240)   
		{
			crateParams->ControlBoardParams.ClockRegister 	= 0x0FFF;
			crateParams->ControlBoardParams.Si5332RegO0 	= 15;
		}
		else if(crateParams->CommonParams.FreqEch == 8512)
		{
			crateParams->ControlBoardParams.ClockRegister 	= 0x0FFC;
			crateParams->ControlBoardParams.Si5332RegO0 	= 18; 
		}
		else if(crateParams->CommonParams.FreqEch == 6400) 
		{
			crateParams->ControlBoardParams.ClockRegister 	= 0x0FF7;
			crateParams->ControlBoardParams.Si5332RegO0 	= 24; 
		}
		else if(crateParams->CommonParams.FreqEch == 4252)
		{
			crateParams->ControlBoardParams.ClockRegister 	= 0x0FF8;
			crateParams->ControlBoardParams.Si5332RegO0 	= 36; 
		}
		else if(crateParams->CommonParams.FreqEch == 3200)
		{
		
			crateParams->ControlBoardParams.ClockRegister 	= 0x0FF2;
			crateParams->ControlBoardParams.Si5332RegO0 	= 48; 
		}
		else if(crateParams->CommonParams.FreqEch == 2133)
		{
			crateParams->ControlBoardParams.ClockRegister 	= 0x0E38;
			crateParams->ControlBoardParams.Si5332RegO0 	= 72; 
		}
		else if(crateParams->CommonParams.FreqEch == 2000) // for Sampic Slow and board V3 with Si5332
		{
			crateParams->ControlBoardParams.Si5332RegO0 		= 0x4C;    	// add 0x2B
			crateParams->ControlBoardParams.Si5332Idpa 			= 0x802F;  	
			crateParams->ControlBoardParams.Si5332PDivider 		= 0x02;  	// add 0x75
			crateParams->ControlBoardParams.Si5332OutDivider 	= 0x01;   	// add 0x7B, 0x80, 0x8A, 0x99, 0xA8
			crateParams->ControlBoardParams.Si5332PllMode 		= 0x10;		// add 0xBE
		}
		else if(crateParams->CommonParams.FreqEch == 1600) // for SAMPIC FAST	?? à vérifier
		{
			crateParams->ControlBoardParams.ClockRegister 	= 0x0E32;
			crateParams->ControlBoardParams.Si5332RegO0 	= 96; 
		}
		else if(crateParams->CommonParams.FreqEch == 1280) // for Sampic Slow and board V3 with Si5332
		{
			crateParams->ControlBoardParams.Si5332RegO0 		= 0x77;    	// add 0x2B
			crateParams->ControlBoardParams.Si5332Idpa 			= 0x0077;  	
			crateParams->ControlBoardParams.Si5332PDivider 		= 0x05;  	// add 0x75
			crateParams->ControlBoardParams.Si5332OutDivider 	= 0x01;   	// add 0x7B, 0x80, 0x8A, 0x99, 0xA8
			crateParams->ControlBoardParams.Si5332PllMode 		= 0x04;		// add 0xBE
		}
	
		else if(crateParams->CommonParams.FreqEch == 1000) // for Sampic Slow and board V3 with Si5332
		{
			crateParams->ControlBoardParams.Si5332RegO0 		= 0x98;    	// add 0x2B
			crateParams->ControlBoardParams.Si5332Idpa 			= 0x802F;  	
			crateParams->ControlBoardParams.Si5332PDivider 		= 0x02;  	// add 0x75
			crateParams->ControlBoardParams.Si5332OutDivider 	= 0x01;   	// add 0x7B, 0x80, 0x8A, 0x99, 0xA8
			crateParams->ControlBoardParams.Si5332PllMode 		= 0x10;		// add 0xBE
		}
		else if(crateParams->CommonParams.FreqEch == 800) // for Sampic Slow and board V3 with Si5332
		{
			crateParams->ControlBoardParams.Si5332RegO0 		= 0xBE;    	// add 0x2B
			crateParams->ControlBoardParams.Si5332Idpa 			= 0x802F;  	
			crateParams->ControlBoardParams.Si5332PDivider 		= 0x02;  	// add 0x75
			crateParams->ControlBoardParams.Si5332OutDivider 	= 0x01;   	// add 0x7B, 0x80, 0x8A, 0x99, 0xA8
			crateParams->ControlBoardParams.Si5332PllMode 		= 0x10;		// add 0xBE
		}

		else if(crateParams->CommonParams.FreqEch == 640) // for Sampic Slow and board V3 with Si5332
		{
			crateParams->ControlBoardParams.Si5332RegO0 		= 0xEE;    	// add 0x2B
			crateParams->ControlBoardParams.Si5332Idpa 			= 0x0077;  
			crateParams->ControlBoardParams.Si5332PDivider 		= 0x05;  	// add 0x75
			crateParams->ControlBoardParams.Si5332OutDivider 	= 0x01;   	// add 0x7B, 0x80, 0x8A, 0x99, 0xA8
			crateParams->ControlBoardParams.Si5332PllMode 		= 0x04;		// add 0xBE
		}
		else if(crateParams->CommonParams.FreqEch == 500) // for Sampic Slow and board V3 with Si5332
		{
			crateParams->ControlBoardParams.Si5332RegO0 		= 0x98;    	// add 0x2B
			crateParams->ControlBoardParams.Si5332Idpa 			= 0x802F;  
			crateParams->ControlBoardParams.Si5332PDivider 		= 0x02;  	// add 0x75
			crateParams->ControlBoardParams.Si5332OutDivider 	= 0x02;   	// add 0x7B, 0x80, 0x8A, 0x99, 0xA8
			crateParams->ControlBoardParams.Si5332PllMode 		= 0x04;		// add 0xBE
		}
		else if(crateParams->CommonParams.FreqEch == 320) // for Sampic Slow and board V3 with Si5332
		{
			crateParams->ControlBoardParams.Si5332RegO0 		= 0xEE;    	// add 0x2B
			crateParams->ControlBoardParams.Si5332Idpa 			= 0x0077;  
			crateParams->ControlBoardParams.Si5332PDivider 		= 0x05;  	// add 0x75
			crateParams->ControlBoardParams.Si5332OutDivider 	= 0x02;   	// add 0x7B, 0x80, 0x8A, 0x99, 0xA8
			crateParams->ControlBoardParams.Si5332PllMode 		= 0x04;		// add 0xBE
		}

	}


	
}


/* ======================================================================== */
void Build_ControlBoardTriggerReg	(CrateParamStruct *crateParams)
/* ======================================================================== */
{

	unsigned char trigTypeVal;
	
	if( crateParams->ControlBoardParams.ExternalTriggerType == SOFTWARE)
		 trigTypeVal = 0x0;
	else if(crateParams->ControlBoardParams.ExternalTriggerType == INTERNAL_OSC) 
		 trigTypeVal = 0x2;
	else // EXT_TRIG
		 trigTypeVal = 0x1;
	
	crateParams->ControlBoardParams.TriggerRegister =   trigTypeVal + 
														(crateParams->ControlBoardParams.ExternalTriggerEdge << 6) +
														(crateParams->ControlBoardParams.ExternalSyncEdge << 7);

	
	
	
}

/* ======================================================================== */
void Build_ControlBoardControlReg	(CrateParamStruct *crateParams)
/* ======================================================================== */
{
	Boolean isTTL = TRUE;
	
	if(crateParams->ControlBoardParams.ExternalTriggerSigLevel == TTL_SIG)
	  isTTL = TRUE;
	else
	   isTTL = FALSE;
	
 	crateParams->ControlBoardParams.ControlRegister = (crateParams->ControlBoardParams.EnableTrigger << 1) + 
													  (crateParams->ControlBoardParams.EnableExtTrigCounter << 2) +   
													  (crateParams->ControlBoardParams.EnableDetectTriggerIDfromExtTrig << 3) + 
													  (crateParams->ControlBoardParams.EnableAutoPulse << 4) +
														(isTTL << 7) /*  TTL for output signals */;    
													    
	
}

/* ======================================================================== */
void Build_ControlBoardControlReg2	(CrateParamStruct *crateParams)
/* ======================================================================== */
{
	
 	crateParams->ControlBoardParams.ControlRegister2 =   crateParams->ControlBoardParams.EnableCoincidenceWithExtTrigGateForL3 + 
														(crateParams->ControlBoardParams.CrateIsInStandaloneMode << 1)+
														(crateParams->ControlBoardParams.CrateIsInMasterSyncMode << 2) + 
														(crateParams->ControlBoardParams.EnableMasterSlaveCoincidence << 3)+
														(crateParams->CommonParams.UseExternalClock << 4);
	
													

	
}

 /* ======================================================================== */
void Build_FeBoardsCtrlFpgaTriggerReg (CrateParamStruct *crateParams)
/* ======================================================================== */
{  
	int feBoardIndex;
   
	for(feBoardIndex = 0; feBoardIndex < MAX_NB_OF_FE_BOARDS; feBoardIndex++)
    {
	 	crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.TriggerRegister  = 	
										 crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.ExternalTriggerType +
										(crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.ExternalTriggerEdge<<6);
	}
}
 

/* ======================================================================== */
void	Build_ControlBoardTriggerCombiLogicForL3Reg  (CrateParamStruct *crateParams)  
/* ======================================================================== */
{
	
	crateParams->ControlBoardParams.TriggerCombiLogicForL3	=   (crateParams->ControlBoardParams.SelFeb0ForL3 &0x3) +
										   ((crateParams->ControlBoardParams.SelFeb1ForL3 &0x3)<<2) + 
										   ((crateParams->ControlBoardParams.SelFeb2ForL3 &0x3)<<4) +   
										   ((crateParams->ControlBoardParams.SelFeb3ForL3 &0x3)<<6) +   
										   ((crateParams->ControlBoardParams.Layer1TriggerLogic0 &0x7)<<8)+
										   ((crateParams->ControlBoardParams.Layer1TriggerLogic1 &0x7)<<11)+   
										   ((crateParams->ControlBoardParams.Layer1TriggerLogic2 &0x7)<<14)+  
										   ((crateParams->ControlBoardParams.Layer2TriggerLogic0 &0x7)<<17)+ 
										   ((crateParams->ControlBoardParams.Layer2TriggerLogic1 &0x7)<<20)+ 
										   ((crateParams->ControlBoardParams.Layer3TriggerLogic &0x7)<<23)+ 
										   ((crateParams->ControlBoardParams.EnableBuildL3)<<26); 
										   
}


/* ============================================================================ */ 
void Build_FeBoardsCtrlFpgaControlReg (CrateParamStruct *crateParams)
/* ============================================================================ */ 
{
	
	int feBoardIndex;
	
	for(feBoardIndex = 0; feBoardIndex < MAX_NB_OF_FE_BOARDS; feBoardIndex++)
	{
		
		crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.ControlRegister  = ((crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.EnableTrigger)<<1)
										+ ((crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.EnablePulserMode)<<3)
										+ ((crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.EnableClockOnSync)<<4) 
										+ ((crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.Sel_Async_Ext_Trig_Source)<<5);
	}
}

/* ============================================================================ */ 
void Build_FeBoardsCtrlFpgaControlReg2 (CrateParamStruct *crateParams)
/* ============================================================================ */ 
{  
	int feBoardIndex;
	
	for(feBoardIndex = 0; feBoardIndex < MAX_NB_OF_FE_BOARDS; feBoardIndex++)
	{
	
		crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.ControlRegister2  =  
										crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.EnableCoincidenceWithExtTrigGateForL2 +
		 								(crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.FEBGlobalTriggerIsL3 << 1);
	}
	
}


/* ============================================================================ */ 
void		Build_FeBoardCtrlFpgaTriggerCombiLogicForL2Reg (CrateParamStruct *crateParams)
/* ============================================================================ */ 
{
	
	int feBoardIndex;
	
	for(feBoardIndex = 0; feBoardIndex < MAX_NB_OF_FE_BOARDS; feBoardIndex++)
	{
	
		crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.TriggerCombiLogicForL2 =  
											(crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.SelSAMPIC0ForL2 &0x3) +
										   ((crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.SelSAMPIC1ForL2 &0x3)<<2) + 
										   ((crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.SelSAMPIC2ForL2 &0x3)<<4) +   
										   ((crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.SelSAMPIC3ForL2 &0x3)<<6) +   
										   ((crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.Layer1TriggerLogic0 &0x7)<<8)+
										   ((crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.Layer1TriggerLogic1 &0x7)<<11)+   
										   ((crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.Layer1TriggerLogic2 &0x7)<<14)+  
										   ((crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.Layer2TriggerLogic0 &0x7)<<17)+ 
										   ((crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.Layer2TriggerLogic1 &0x7)<<20)+ 
										   ((crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.Layer3TriggerLogic  &0x7)<<23)+ 
										   ((crateParams->CommonParams.EnableBuildL2 )<<26); 
			
			
			
	}
	
}

/* ============================================================================ */ 
void Build_FeBoardsFeFpgasControlReg (CrateParamStruct *crateParams)
/* ============================================================================ */ 
{
	 int feBoardIndex;  
	 int feFpgaIndex;
	
	 for(feBoardIndex = 0; feBoardIndex < MAX_NB_OF_FE_BOARDS; feBoardIndex++)
	 {
		  for(feFpgaIndex = 0; feFpgaIndex < NB_OF_FE_FPGAS_IN_FE_BOARD; feFpgaIndex++)
		  {
	
		  	if(crateParams->CommonParams.FreqEch < 3200) 
		  		crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SlowSamplingFrequency = TRUE;
		  	else
				crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SlowSamplingFrequency = FALSE;        

	 	 	 crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].ControlRegister =  crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].EnableTrigFPGA  +
			  											 ((crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].RstDLL )<<1) +
														 ((crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].EnableAsyncExtTrig)<<4) +
														 ((crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].EnableTrigSampic)<<5)+
														  ((crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].EnExtTrisAsEnableTrig)<<6) +
														  ((crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].UdpOk)<<7)+
														  (((unsigned short)(crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].EnableExtTrigCounter))<<8)+
														  (((unsigned short)(crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].ClkEdgeForDataSampling)) << 9) +
														  (((unsigned short)(crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].EnableExtTrigGate))<< 10) +
														   (((unsigned short)(crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].EnableOrTriggerMode))<< 11) +
														   (((unsigned short)(crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SlowSamplingFrequency))<< 12) + 
														   (((unsigned short)(crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].EnableL2Coincidence))<< 13) +
														   (((unsigned short)(crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].EnableDetectTriggerIDfromExtTrig))<< 14);
													  
	  										
		  }
	 }
	  
}


/* ============================================================================ */ 
void Build_FeBoardsFeFpgasL2CoincidenceReg(CrateParamStruct *crateParams)
/* ============================================================================ */ 
{
	 int feBoardIndex, feFpgaIndex;
	
	 for(feBoardIndex = 0; feBoardIndex < MAX_NB_OF_FE_BOARDS; feBoardIndex++)
	 {
		  for(feFpgaIndex = 0; feFpgaIndex < NB_OF_FE_FPGAS_IN_FE_BOARD; feFpgaIndex++)
		  {

		  	 crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].L2CoincidenceReg = 
				 (unsigned short)crateParams->CommonParams.L2SAMPICPrimitivesGateLength +  
				((unsigned short)crateParams->CommonParams.L2CoincidenceLatencyLength << 8);
		 
		  }
	 }																
	
	
}
/* ============================================================================ */ 
/* ============================================================================ */ 

/* ============================================================================ */ 
void Build_FeBoardsFeFpgasPulserReg(CrateParamStruct *crateParams)
/* ============================================================================ */ 
{
	

 int channel;
 int feBoardIndex, feFpgaIndex;
	
 for(feBoardIndex = 0; feBoardIndex < MAX_NB_OF_FE_BOARDS; feBoardIndex++)
 {
	  for(feFpgaIndex = 0; feFpgaIndex < NB_OF_FE_FPGAS_IN_FE_BOARD; feFpgaIndex++)
	  {	
		  crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].PulserRegister = 0;
	
		  for( channel = 0; channel < NB_OF_CHANNELS_IN_SAMPIC; channel++)
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].PulserRegister +=
		     ((unsigned int)crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].EnablePulseChannel[channel] << channel);
	
		  crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].PulserRegister +=  	
			((unsigned int)crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].PulserSourceIsSync << 16) +
													((unsigned int)crateParams->CommonParams.EnPulseMode << 17) +
													((unsigned int)crateParams->CommonParams.EnPulseReSync << 18) ; 
	
	  }
		 
 }
	
	
}

/* ============================================================================ */ 
void Build_FeBoardsSampicsChannelRegs (CrateParamStruct *crateParams)	 // Registers 1 to 16
/* ============================================================================ */ 
{
	

int dacIntValue;
int triggerSource;	

int channel;
int feBoardIndex, feFpgaIndex;

 for(feBoardIndex = 0; feBoardIndex < MAX_NB_OF_FE_BOARDS; feBoardIndex++)
 {
	  for(feFpgaIndex = 0; feFpgaIndex < NB_OF_FE_FPGAS_IN_FE_BOARD; feFpgaIndex++)
	  {		 
		  for(channel = 0; channel < NB_OF_CHANNELS_IN_SAMPIC; channel++)
		  {
			 
			   dacIntValue = Calculate_10BitsDACIntValues(crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.InternalThreshold[channel]); //10-Bit value;
		  
			   if(crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableTriggerChannel[channel] == FALSE)
				   triggerSource = 0x0; //?? 
			   else
				   triggerSource = 	 crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.ChannelTriggerMode[channel];
		   
		  	     
			   crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.ChannelRegister[channel]=
			   
				   (dacIntValue & 0x3FF) +
				   
				   ((1-crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.TriggerEdge[channel])<<10) +
			   
				   ((crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.ExtDiscriThresholdSource[channel])<<11) +
			   
				   ((crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableTriggerChannel[channel])<< 12) +
				   
				   (triggerSource << 13) +
				   
				   (crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.DisableChannelForCentralTrigger[channel] << 15); 
	       	   
			 
		  }
		 
	  }
 }
	 

}

/* ============================================================================ */ 
void Build_FeBoardsSampicsConfigReg1(CrateParamStruct *crateParams)		//Register 17
/* ============================================================================ */ 
{
 
 int feBoardIndex, feFpgaIndex;

	 for(feBoardIndex = 0; feBoardIndex < MAX_NB_OF_FE_BOARDS; feBoardIndex++)
	 {
		  for(feFpgaIndex = 0; feFpgaIndex < NB_OF_FE_FPGAS_IN_FE_BOARD; feFpgaIndex++)
		  {		 

				crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.ConfigReg1  =
		
				(	((crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.TriggerResetSource)&0x1) +
					(crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.ResetChargePumps << 1) +
					(crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.ResetAllSequencers << 2) +
					(crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnCoincidenceModeForCentralTrigger << 3) +	// nouveau champ SAMPIC V2
					(crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.DisableDelayDLL << 4)    +
					(crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.DLLMode << 5) + 
					(crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.DisableDelayClk <<7 )    +

					(((crateParams->CommonParams.OffsetForStartOfRead)& 0x3F) << 8) + // ND: 6-Bits 
	    
					(crateParams->CommonParams.SmartRead << 14) +
		
					(crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.ThresholdDACsPowerDown << 15) );
		 
		  }
	 }

}


/* ============================================================================ */ 
void Build_FeBoardsSampicsConfigReg2(CrateParamStruct *crateParams)		 //Register 18  
/* ============================================================================ */ 
{


 int feBoardIndex, feFpgaIndex;

	 for(feBoardIndex = 0; feBoardIndex < MAX_NB_OF_FE_BOARDS; feBoardIndex++)
	 {
		  for(feFpgaIndex = 0; feFpgaIndex < NB_OF_FE_FPGAS_IN_FE_BOARD; feFpgaIndex++)
		  {		 
		    	 	
			  crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.ConfigReg2  = 		
		   
			    crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.ForceRingOscAlwaysON +
	
			   (crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.ADCClockSource << 1) +
	   
			   (crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.TestOutMode << 3)+
	   
			   (crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.InhibRstADC << 6) +
	   
			   (crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.InhibReadoutData << 7)+
			   (crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.InhibTokenExtReset << 8)+
			   ((crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EndOfADCGrayCounter & 0x3) << 9)+ 
			   (crateParams->CommonParams.EnableAutoConversionMode << 11)+
			   (crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnablePostTrigger << 12)+   
			   (crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.SelHighLVDSCurrent << 13)+
			   (crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.CentralTriggerEffect << 14)+
			   (crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableTripleCoincidenceMode << 15);
			}
		
	 }

}

/* ============================================================================ */ 
void Build_FeBoardsSampicsConfigReg3(CrateParamStruct *crateParams)		//Register 19 
/* ============================================================================ */ 
{

 int dacIntValue;


  int feBoardIndex, feFpgaIndex;

  for(feBoardIndex = 0; feBoardIndex < MAX_NB_OF_FE_BOARDS; feBoardIndex++)
  {
	  for(feFpgaIndex = 0; feFpgaIndex < NB_OF_FE_FPGAS_IN_FE_BOARD; feFpgaIndex++)
	  {		  
		  dacIntValue = Calculate_10BitsDACIntValues(crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.Vdac_DLLContinuity); //Volts 

		  crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.ConfigReg3 = dacIntValue +
			  (crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.DLLContinuityPowerDown  << 10) +
			  (crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableADCCompOverflow  << 11) + 
			  (crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableDigitalInput  << 12) + 
			  (crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.DisablePrimitiveDeglitcher  << 13) + 
			  (crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableTOTFilterWideCap  << 14) + 
			  (crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.SelGatedDiscriForCTPrimitives  << 15);
	  }
  }
 

}


/* ============================================================================ */ 
void Build_FeBoardsSampicsConfigReg4(CrateParamStruct *crateParams)	//Register 20
/* ============================================================================ */ 
{
 int  i;
 int dacIntValue;
 unsigned short saved_reg, new_reg;

 float floatdacValue;
 int feBoardIndex, feFpgaIndex;
 Boolean enWideCap;
 float pulseWidth;

  for(feBoardIndex = 0; feBoardIndex < MAX_NB_OF_FE_BOARDS; feBoardIndex++)
  {
	  for(feFpgaIndex = 0; feFpgaIndex < NB_OF_FE_FPGAS_IN_FE_BOARD; feFpgaIndex++)
	  {	
	  
		  enWideCap = crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableTOTFilterWideCap;
		  pulseWidth = crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.InternalTOTFilterWidth;
		  
		  crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.InternalTOTFilterDAC = Convert_TOTFilterWidthToTOTFilterDAC(enWideCap ,pulseWidth); 

		  floatdacValue = crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.InternalTOTFilterDAC;
	 
		  if(floatdacValue < 0) floatdacValue = 0;  
		  	dacIntValue = Calculate_10BitsDACIntValues(floatdacValue); //Volts 

		  crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.ConfigReg4 = dacIntValue +
			  (crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableExternalTOTFilterDAC  << 10) +
			  (crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableTOTFilter  << 11) +  
			  ((crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.AutoConversionDelayThird & 0x7)  << 12) +    
			  (crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnablePingPong << 15);
	  
	 	  saved_reg = crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.ConfigReg4;
  
		  new_reg = 0;
 
		  for(i = 0; i < 16; i++)
		   new_reg += ((saved_reg & (int)pow (2, i))>>i)<<(15-i);
  
		  crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.ConfigReg4 = new_reg;
	  
	  }
  }

}


/* ============================================================================ */ 
void Build_FeBoardsSampicsConfigReg5(CrateParamStruct *crateParams)	//Register 21
/* ============================================================================ */ 
{
 int  i;
 int dacIntValue;
 unsigned short saved_reg, new_reg;
 float  floatdacValue;

 
 int feBoardIndex, feFpgaIndex;

  for(feBoardIndex = 0; feBoardIndex < MAX_NB_OF_FE_BOARDS; feBoardIndex++)
  {
	  for(feFpgaIndex = 0; feFpgaIndex < NB_OF_FE_FPGAS_IN_FE_BOARD; feFpgaIndex++)
	  {	
		  
		  floatdacValue =  crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.InternalTOTRampCurrentDAC;
	 	  if(floatdacValue < 0) floatdacValue = 0;
	  
	  	  dacIntValue = Calculate_10BitsDACIntValues(floatdacValue); //Volts 

		 crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.ConfigReg5 = dacIntValue +
			  (crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableExternalTOTRampCurrentDAC  << 10) +
			  (crateParams->CommonParams.EnableTOTMeasurement << 11) +
			  ((crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.TourDePisteDelay & 0x7) << 12) +
			  (crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.DisableTOTOverflow << 15);
	  
		  saved_reg = crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.ConfigReg5;
	  
		  new_reg = 0;
	  
		  for(i = 0; i < 16; i++)
		   new_reg += ((saved_reg & (int)pow (2, i))>>i)<<(15-i);
	  
		  crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.ConfigReg5 = new_reg;
	  }
  }

}


/* ============================================================================ */ 
void Build_FeBoardsSampicsConfigReg6(CrateParamStruct *crateParams)	//Register 22
/* ============================================================================ */ 
{
 int  i;
 int dacIntValue;
 unsigned short saved_reg, new_reg;

 int feBoardIndex, feFpgaIndex;

  for(feBoardIndex = 0; feBoardIndex < MAX_NB_OF_FE_BOARDS; feBoardIndex++)
  {
	  for(feFpgaIndex = 0; feFpgaIndex < NB_OF_FE_FPGAS_IN_FE_BOARD; feFpgaIndex++)
	  {	
	  
		  dacIntValue = Calculate_10BitsDACIntValues(crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.Vdac_ramp); //Volts 

		  crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.ConfigReg6 = dacIntValue +
			  (crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableExternalADCRampDAC  << 10) +
			  (crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableSyncConversion  << 11) +
			  ((crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.PostTrig & 0x7)  << 12) +
			  (crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableCommonDeadTime  << 15);
	  
		  saved_reg = crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.ConfigReg6;
	  
		  new_reg = 0;
	  
		  for(i = 0; i < 16; i++)
		   new_reg += ((saved_reg & (int)pow (2, i))>>i)<<(15-i);
	  
		  crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.ConfigReg6 = new_reg;
	  }
  }

}


/* ============================================================================ */ 
void Build_FeBoardsSampicsConfigReg7(CrateParamStruct *crateParams)	// Register 23
/* ============================================================================ */ 
{
 int  i;
 int dacIntValue;
 unsigned short saved_reg, new_reg;
 
 int feBoardIndex, feFpgaIndex;
 
  for(feBoardIndex = 0; feBoardIndex < MAX_NB_OF_FE_BOARDS; feBoardIndex++)
  {
	  for(feFpgaIndex = 0; feFpgaIndex < NB_OF_FE_FPGAS_IN_FE_BOARD; feFpgaIndex++)
	  {		  
	  
	
	   dacIntValue = Calculate_10BitsDACIntValues(crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.InternalOverflowDAC); //Volts 

	 
			
	   crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.ConfigReg7 = dacIntValue +
			  (crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableDataResynchro  << 10)+
			  (crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.InhibReloadRampData  << 11)+  
			  ((crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.PrimitivesGateLength & 0x7)  << 12)+
			  (crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.ReloadRampSrce  << 15);
	
			
	
		
	  saved_reg = crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.ConfigReg7;
	  
	  new_reg = 0;
	  
	  for(i = 0; i < 16; i++)
	   new_reg += ((saved_reg & (int)pow (2, i))>>i)<<(15-i);
	  
	  crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.ConfigReg7 = new_reg;
	  
	  }
	  
  }

}


/* ============================================================================ */ 
void Build_FeBoardsSampicsConfigReg8(CrateParamStruct *crateParams)	//Register  24
/* ============================================================================ */ 
{

 int dacIntValue;

	
  int feBoardIndex, feFpgaIndex;

  for(feBoardIndex = 0; feBoardIndex < MAX_NB_OF_FE_BOARDS; feBoardIndex++)
  {
	  for(feFpgaIndex = 0; feFpgaIndex < NB_OF_FE_FPGAS_IN_FE_BOARD; feFpgaIndex++)
	  {	  
		  
		  
		  dacIntValue = Calculate_10BitsDACIntValues(crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.InternalRoscDAC); //Volts 

	  

			
		  crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.ConfigReg8 = dacIntValue +
			  (crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableExternalRoscDAC  << 10)+
			  ((crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.TimeINLCalibClockFreqV3) << 11) +
			  (crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableExternalVreset  << 14) +
			  (crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableTranslator  << 15);
	
			
		
	 
	  }
  }

}


/* ============================================================================ */ 
void Build_FeBoardsSampicsConfigReg9(CrateParamStruct *crateParams)	// Register 25
/* ============================================================================ */ 
{
 int  i;

 unsigned short saved_reg, new_reg;

 int feBoardIndex, feFpgaIndex;

  for(feBoardIndex = 0; feBoardIndex < MAX_NB_OF_FE_BOARDS; feBoardIndex++)
  {
	  for(feFpgaIndex = 0; feFpgaIndex < NB_OF_FE_FPGAS_IN_FE_BOARD; feFpgaIndex++)
	  {	  	  
	  //dacIntValue = Calculate_10BitsDACIntValues(FeFpgaParams[feIndex].SAMPICIndividualParams.InternalADCCalibDAC); //Volts 

		  crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.ConfigReg9 = 
			   crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.InternalADCCalibDACIntValue +
			  (crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableReturnBuffer  << 10) +
			  (crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableDiffN  << 11) +
			  (crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableDiffP  << 12) +
			  (crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.LVDSPolarityIsNegative << 13) +
			  (crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableInternalVReset  << 14) +
		  	  (crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableADCCalibDAC  << 15);
	  
		  saved_reg = crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.ConfigReg9;
	  
		  new_reg = 0;
	  
		  for(i = 0; i < 16; i++)
		   new_reg += ((saved_reg & (int)pow (2, i))>>i)<<(15-i);
	  
		  crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.ConfigReg9 = new_reg;
	  }
  }

}


/* ============================================================================ */ 
void Build_FeBoardsSampicsConfigReg10(CrateParamStruct *crateParams)	//Register 26
/* ============================================================================ */ 
{

 int feBoardIndex, feFpgaIndex;

  for(feBoardIndex = 0; feBoardIndex < MAX_NB_OF_FE_BOARDS; feBoardIndex++)
  {
	  for(feFpgaIndex = 0; feFpgaIndex < NB_OF_FE_FPGAS_IN_FE_BOARD; feFpgaIndex++)
	  {	 	  

		    crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.ConfigReg10 = 
			  ((unsigned short)crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.TimeINLCalibRisingEdgeSlope & 0xF) +
			  (((unsigned short)crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.TimeINLCalibFallingEdgeSlope & 0xF)  << 4) + 
			  (((unsigned short)crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.TimeINLCalibVLow & 0x7)  << 8)+
			  (((unsigned short)crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.TimeINLCalibVHigh & 0x7) << 11) +
			  (crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableTimeINLCalib << 14) +
			  (crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableReturnGnd << 15);
	  																							    
	 
	  }
  }

}


/* ============================================================================ */ 
 void Compute_NbOfExtraWords (CrateParamStruct *crateParams)
/* ============================================================================ */ 
 {
    
   // uniquement pour SAMPIC Version >= 3
	 
	if( crateParams->CommonParams.EnableTOTMeasurement  == FALSE)
	{
		if(crateParams->CommonParams.EnableAutoConversionMode == FALSE)
			crateParams->CommonParams.NbOfExtraWords = 5; // Mode STANDARD 
		else
			crateParams->CommonParams.NbOfExtraWords = 6; // Mode STANDARD 
	}
	else 
	{
		if(crateParams->CommonParams.EnableAutoConversionMode == FALSE) 
		  crateParams->CommonParams.NbOfExtraWords = 6;
		else
		  crateParams->CommonParams.NbOfExtraWords = 7;  	
	}
   
  }
 
/* ============================================================================ */ 
/* ============================================================================ */ 

/* ============================================================================ */ 
void Compute_ConvertLength (CrateParamStruct *crateParams) 
/* ============================================================================ */ 
{
 
int adcNbOfBits;


	adcNbOfBits =  crateParams->CommonParams.ADCNbOfBits;

	if(	adcNbOfBits  == 11)
		crateParams->CommonParams.ConvertLength = ADC_11BITS_MAX_CONVERT_LENGTH; //* x 10 ns */
	else if(adcNbOfBits  == 10)
		crateParams->CommonParams.ConvertLength = ADC_11BITS_MAX_CONVERT_LENGTH/2;
	else if(adcNbOfBits  == 9)
		crateParams->CommonParams.ConvertLength = ADC_11BITS_MAX_CONVERT_LENGTH/4;
	else //if(FeFpgaParams[feIndex].SAMPICIndividualParams.ADC_Nb_Of_Bits  == 9)
		crateParams->CommonParams.ConvertLength = ADC_11BITS_MAX_CONVERT_LENGTH/8;

		
	 
	
}


/* ============================================================================ */ 
void Compute_SAMPICIndividualInternalThresholds(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams)
/* ============================================================================ */ 
{
	
	int feBoardIndex, feFpgaIndex, channel;
	float vBaseline;
	float offset, slope, relativeThreshold;
	

   for(feBoardIndex = 0; feBoardIndex < MAX_NB_OF_FE_BOARDS; feBoardIndex++)
   {
	  for(feFpgaIndex = 0; feFpgaIndex < NB_OF_FE_FPGAS_IN_FE_BOARD; feFpgaIndex++)
	  {			
 	
		   vBaseline = Convert_VDac_Reset_To_VBaseline( crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.Vdac_reset);
		   for(channel = 0; channel < NB_OF_CHANNELS_IN_SAMPIC; channel ++)
		   {
			  offset = crateInfoParams->CrateCalibInfo.CalibValues[feBoardIndex].InternalTriggerThresholdOffset[feFpgaIndex*NB_OF_CHANNELS_IN_SAMPIC + channel];
			  slope = crateInfoParams->CrateCalibInfo.CalibValues[feBoardIndex].InternalTriggerThresholdSlope[feFpgaIndex*NB_OF_CHANNELS_IN_SAMPIC + channel];
			  relativeThreshold = crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.RelativeInternalThreshold[channel];
			  
			  crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.InternalThreshold[channel]= 
				 relativeThreshold + vBaseline + offset + vBaseline*slope;
	
			 if(crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.InternalThreshold[channel] < 0)
				 crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.InternalThreshold[channel] = 0.0;
		 
			 if(crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.InternalThreshold[channel] > MAX_DAC_RAW_VALUE)
				 crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.InternalThreshold[channel] = MAX_DAC_RAW_VALUE;
			 
			 
		   }
	  }
  }
	

   Build_FeBoardsSampicsChannelRegs(crateParams); 
}



/* ============================================================================ */ 
/* ============================================================================ */ 

/* ============================================================================ */ 
/* ============================================================================ */ 


/* ============================================================================ */
/* =================  Checking System      ====================== */
/* ============================================================================ */





/* ============================================================================ */ 
SAMPIC256CH_ErrCode Check_FeBoardsInSystem(CrateInfoStruct *crateInfoParams)  
/* ============================================================================ */ 
{
int n, feBoardIndex;
int nframes = 0;
char sub_address;
int dummy = 0;
unsigned char data;
Boolean feBoardIsPresent[MAX_NB_OF_FE_BOARDS];

char  tempBuff[1024];
int deviceHandle;

ML_Frame   my_MLFrames[10];

SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success;

	deviceHandle = crateInfoParams->ConnectionInfo.CtrlDeviceHandle;
	
	if(deviceHandle < 0)
		return SAMPIC256CH_NoCrateConnected;
	
	sub_address = ad_control_board_FeBoardPresence;
	data = 0xFF;
	
	if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_BusWriteWords(crateInfoParams,CTRL_ACCESS, CB_CTRL_FPGA,dummy ,dummy, sub_address, &data,1);
	dsleep(100);
	
	Purge_ControlBufferChain(deviceHandle);
	
	for(feBoardIndex = 0; feBoardIndex <MAX_NB_OF_FE_BOARDS; feBoardIndex++)
	{
		  feBoardIsPresent[feBoardIndex]= FALSE;
		
	}
 
	crateInfoParams->CrateBoardsInfo.ControlBoardInfo.FeBoardsPresence =  0x00; //ALL ABSENT;
	
	crateInfoParams->NbOfFeBoards = 0;
	

	if(errCode == SAMPIC256CH_Success)
		errCode =  SAMPIC256CH_BusCommandReadWords (crateInfoParams,CTRL_ACCESS, FEB_CTRL_FPGA, ALL_FE_BOARDs, dummy, 0 /* sub_address*/, 1 /* word_count*/); 
	
	dsleep(100);
	
	if(errCode == SAMPIC256CH_Success) 
		errCode =  SAMPIC256CH_BusReadExtended(deviceHandle, tempBuff, my_MLFrames, MAX_BYTES_TO_READ, &nframes);   

	crateInfoParams->NbOfFeBoards = 0;
	
	for(n= 0; n< nframes; n++)
	{	
	 	feBoardIndex = my_MLFrames[n].path[0];
			
		if(feBoardIndex >= 0)
		{
			crateInfoParams->NbOfFeBoards++;
			
			feBoardIsPresent[feBoardIndex]= TRUE; 
		}
	}
	
	n=0;

	for(feBoardIndex = 0; feBoardIndex <MAX_NB_OF_FE_BOARDS; feBoardIndex++)
	{
		
	   if(feBoardIsPresent[feBoardIndex]==TRUE)  
	   {
		   
	   	  crateInfoParams->FrontEndBoardsPathIndex[n] = feBoardIndex;
	  	  n++;
	   }
		
	}
	


	for(feBoardIndex = 0; feBoardIndex < MAX_NB_OF_FE_BOARDS; feBoardIndex++)
  	{
	
	
		crateInfoParams->CrateBoardsInfo.ControlBoardInfo.FeBoardsPresence += (feBoardIsPresent[feBoardIndex]<<feBoardIndex);          
		
		
	}
	   
	sub_address = ad_control_board_FeBoardPresence;
	data = crateInfoParams->CrateBoardsInfo.ControlBoardInfo.FeBoardsPresence;

	if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_BusWriteWords (crateInfoParams,CTRL_ACCESS, CB_CTRL_FPGA,dummy ,dummy, sub_address, &data,1);
	
    return errCode;
}

/* ============================================================================ */ 
SAMPIC256CH_ErrCode Check_SystemType (CrateInfoStruct *crateInfoParams)
/* ============================================================================ */ 
{
int feBoard, feBlock;

FEBoardType_t feBoardType;

SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success; 

 crateInfoParams->SystemType  = SAMPIC_BYPASS_BOARD; //default Type
   
 if(errCode == SAMPIC256CH_Success) errCode =  Read_ControlBoard_EPROM_Info (crateInfoParams);
 
 for(feBoard = 0; feBoard < crateInfoParams->NbOfFeBoards; feBoard++)
 {
	 
 	if(errCode == SAMPIC256CH_Success) errCode =  Read_FeBoard_CTRL_EEPROM_Info (crateInfoParams, feBoard); 
	
	for(feBlock = 0; feBlock < NB_OF_FE_FPGAS_IN_FE_BOARD; feBlock++)
	{
	 	if(errCode == SAMPIC256CH_Success) errCode =  Read_FrontEndBlock_EEPROM_Info (crateInfoParams, feBoard, feBlock);    	
	}
	 
 }

 
 feBoardType = crateInfoParams->CrateBoardsInfo.FeBoardInfo[0].FeBoardType[0];
 
 if(errCode == SAMPIC256CH_Success)
 {
	 for(feBoard = 0; feBoard < crateInfoParams->NbOfFeBoards; feBoard++)
	 {
		for(feBlock = 0; feBlock < NB_OF_FE_FPGAS_IN_FE_BOARD; feBlock++)
		{
		
			if( crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoard].FeBoardType[feBlock] != feBoardType)
				return  SAMPIC256CH_InconsistencyInBoardsType;
		
		}
	 }
	 
	 crateInfoParams->SystemType = feBoardType;
 }
 
 
 return errCode;

}
/* ============================================================================ */ 
/* ============================================================================ */ 





//
///* ============================================================================ */
//void	Update_PulserMode (void)
///* ============================================================================ */
//{
//
//	int channel, feIndex;
//	
//	Boolean pulserMode = FALSE;
//	
//	for(feIndex = 0; feIndex < Nb_Of_Fe_FPGAs_In_System; feIndex ++)
//	{
//
//		if(pulserMode == TRUE)
//			break;
//		
//		for(channel = 0; channel < NB_OF_CHANNELS_IN_SAMPIC; channel++)
//		{
//			
//			if(	FeFpgaParams[feIndex].EnablePulseChannel[channel] == TRUE) 
//			{
//				pulserMode = TRUE;
//				break;
//				
//			}
//		}
//		
//	}
//	
//	SystemParams.EnPulseMode = pulserMode;
//	
//	
//		
//					
//	
//}


/* ============================================================================================== */
/* ============================  EEPROM INFO ================================================= */


/* ============================================================================================== */
SAMPIC256CH_ErrCode Read_ControlBoard_EPROM_Info (CrateInfoStruct *crateInfoParams) 
/* ============================================================================================== */
{
SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success;   
int dummy = 0;
char boardNumberChar[8]="";
int boardVersion,  boardSerialNumber;
unsigned char code;

char ipAddressChar[16]; 
char portChar[2];
int udpCtrlPort, udpDaqPort;
char stringVal[16];

	
	// Control Board Serial Number

	if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_Read_EEPROM(crateInfoParams,CB_CTRL_EEPROM,dummy,dummy, EEPROMBoardSerNumAdd, boardNumberChar,EEPROMBoardSerNumNbOfBytes);  
 	
	if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_Read_EEPROM(crateInfoParams,CB_CTRL_EEPROM,dummy,dummy, EEPROMBoardSerNumCodeAdd, &code,EEPROM_NB_OF_BYTES_FOR_CALIB_CODES);  
	
	if(errCode == SAMPIC256CH_Success && (code == (EEPROM_PAGE_SIZE - (EEPROMBoardSerNumCodeAdd + 1))))
	{

		sscanf(boardNumberChar , "%d.%d", &boardVersion, &boardSerialNumber);
		crateInfoParams->CrateBoardsInfo.ControlBoardInfo.BoardVersion = boardVersion;
		crateInfoParams->CrateBoardsInfo.ControlBoardInfo.BoardSerNum =  boardSerialNumber;
		
	/*	if(boardVersion >= 3)
		   crateInfoParams->CrateBoardsInfo.ControlBoardInfo.Si5332IsPresent = TRUE;
		else
		   crateInfoParams->CrateBoardsInfo.ControlBoardInfo.Si5332IsPresent = FALSE; */	
	}
	
	// Control Board IP Address
	
	if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_Read_EEPROM(crateInfoParams,CB_CTRL_EEPROM,dummy,dummy, EEPROMIpAddressUDPAdd, ipAddressChar,EEPROMIpAddressUDPNbOfBytes);  
 	
	if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_Read_EEPROM(crateInfoParams,CB_CTRL_EEPROM,dummy,dummy, EEPROMIpAddressCodeAdd, &code,EEPROM_NB_OF_BYTES_FOR_CALIB_CODES);  
	
	if(errCode == SAMPIC256CH_Success && (code == (EEPROM_PAGE_SIZE - (EEPROMIpAddressCodeAdd + 1))))
	{   
		sprintf(stringVal, "%d.%d.%d.%d", (unsigned char)ipAddressChar[3], (unsigned char)ipAddressChar[2],(unsigned char)ipAddressChar[1],(unsigned char)ipAddressChar[0]);

	 	strcpy(crateInfoParams->CrateBoardsInfo.ControlBoardInfo.IpAddressFromEEPROM, stringVal);
	
	}
	
	// Control Board Ctrl Port
	
	if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_Read_EEPROM(crateInfoParams,CB_CTRL_EEPROM,dummy,dummy, EEPROMUDPCtrlPortAdd, portChar,EEPROMUDPCtrlPortNbOfBytes);  
 	
	if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_Read_EEPROM(crateInfoParams,CB_CTRL_EEPROM,dummy,dummy, EEPROMUDPCtrlPortCodeAdd, &code,EEPROM_NB_OF_BYTES_FOR_CALIB_CODES);  
	
	if(errCode == SAMPIC256CH_Success && (code == (EEPROM_PAGE_SIZE - (EEPROMUDPCtrlPortCodeAdd + 1))))
	{
		udpCtrlPort = (unsigned char)portChar[0] + (((unsigned char)portChar[1])<<8); 
		crateInfoParams->CrateBoardsInfo.ControlBoardInfo.CtrlPortFromEEPROM = udpCtrlPort;
		
	}

	
	//Control Board DAQ Port
	
	if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_Read_EEPROM(crateInfoParams,CB_CTRL_EEPROM,dummy,dummy, EEPROMUDPDaqPortAdd, portChar,EEPROMUDPDaqPortNbOfBytes);  
 	
	if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_Read_EEPROM(crateInfoParams,CB_CTRL_EEPROM,dummy,dummy, EEPROMUDPDaqPortCodeAdd, &code,EEPROM_NB_OF_BYTES_FOR_CALIB_CODES);  
	
	if(errCode == SAMPIC256CH_Success && (code == (EEPROM_PAGE_SIZE - (EEPROMUDPDaqPortCodeAdd + 1))))
	{
		udpDaqPort = (unsigned char)portChar[0] + (((unsigned char)portChar[1])<<8); 
		crateInfoParams->CrateBoardsInfo.ControlBoardInfo.DaqPortFromEEPROM = udpDaqPort;
		
	}

	
	
	
 
 return errCode;	
	
}

/* ============================================================================================== */
SAMPIC256CH_ErrCode Read_FeBoard_CTRL_EEPROM_Info (CrateInfoStruct *crateInfoParams, int feBoardIndex) 
/* ============================================================================================== */
{
	
SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success;   
int dummy = 0;
char boardNumberChar[8]="";
int boardVersion,  boardSerialNumber;
unsigned char code;
char boardTypeChar;


char ipAddressChar[16]; 
char portChar[2];
int udpCtrlPort, udpDaqPort;

char stringVal[16]; 

if(feBoardIndex < 0)
 return SAMPIC256CH_InvalidParam;

		// FE Board CTRL EEPROM Serial Number

	if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_Read_EEPROM(crateInfoParams,FEB_CTRL_EEPROM,feBoardIndex,dummy, EEPROMBoardSerNumAdd, boardNumberChar,EEPROMBoardSerNumNbOfBytes);  
 	
	if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_Read_EEPROM(crateInfoParams,FEB_CTRL_EEPROM,feBoardIndex,dummy, EEPROMBoardSerNumCodeAdd, &code,EEPROM_NB_OF_BYTES_FOR_CALIB_CODES);  
	
	if(errCode == SAMPIC256CH_Success && (code == (EEPROM_PAGE_SIZE - (EEPROMBoardSerNumCodeAdd + 1))))
	{

		sscanf(boardNumberChar , "%d.%d", &boardVersion, &boardSerialNumber);
		crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoardIndex].BoardVersion = boardVersion;
		crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoardIndex].BoardSerNum =  boardSerialNumber;
		
		if(boardVersion >= 2)
		 crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoardIndex].Si5332IsPresent = TRUE;
		else
		 crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoardIndex].Si5332IsPresent = FALSE; 	
	
	}
	
	// Fe Board Type
	
	 if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_Read_EEPROM(crateInfoParams,FEB_CTRL_EEPROM,feBoardIndex, dummy, EEPROMBoardTypeAdd,  &boardTypeChar, EEPROMBoardTypeNbOfBytes);
	
	 if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_Read_EEPROM(crateInfoParams,FEB_CTRL_EEPROM,feBoardIndex, dummy, EEPROMBoardTypeCodeAdd, &code, EEPROM_NB_OF_BYTES_FOR_CALIB_CODES);

	 if(code == (EEPROM_PAGE_SIZE - (EEPROMBoardTypeCodeAdd + 1)))
	 {
	
		crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoardIndex].FeBoardTypeCharFromEEPROM = boardTypeChar;
		
	 }
	 else
	 {
		crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoardIndex].FeBoardTypeCharFromEEPROM = 'Z';
		
	 }
 

		 
	// Fe Board CTRL EEPROM Board IP Address
	
	if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_Read_EEPROM(crateInfoParams,FEB_CTRL_EEPROM,feBoardIndex,dummy, EEPROMIpAddressUDPAdd, ipAddressChar,EEPROMIpAddressUDPNbOfBytes);  
 	
	if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_Read_EEPROM(crateInfoParams,FEB_CTRL_EEPROM,feBoardIndex,dummy, EEPROMIpAddressCodeAdd, &code,EEPROM_NB_OF_BYTES_FOR_CALIB_CODES);  
	
	if(errCode == SAMPIC256CH_Success && (code == (EEPROM_PAGE_SIZE - (EEPROMIpAddressCodeAdd + 1))))
	{
	    sprintf(stringVal, "%d.%d.%d.%d", (unsigned char)ipAddressChar[3], (unsigned char)ipAddressChar[2],(unsigned char)ipAddressChar[1],(unsigned char)ipAddressChar[0]);

	 	strcpy(crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoardIndex].IpAddressFromEEPROM, stringVal);
	
	}
	
	// FE Board CTRL EEPROM Ctrl Port
	
	if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_Read_EEPROM(crateInfoParams,FEB_CTRL_EEPROM,feBoardIndex,dummy, EEPROMUDPCtrlPortAdd, portChar,EEPROMUDPCtrlPortNbOfBytes);  
 	
	if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_Read_EEPROM(crateInfoParams,FEB_CTRL_EEPROM,feBoardIndex,dummy, EEPROMUDPCtrlPortCodeAdd, &code,EEPROM_NB_OF_BYTES_FOR_CALIB_CODES);  
	
	if(errCode == SAMPIC256CH_Success && (code == (EEPROM_PAGE_SIZE - (EEPROMUDPCtrlPortCodeAdd + 1))))
	{
		udpCtrlPort = (unsigned char)portChar[0] + (((unsigned char)portChar[1])<<8); 
		crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoardIndex].CtrlPortFromEEPROM = udpCtrlPort;
		
	}

	
	//FE Board CTRL EEPROM DAQ Port
	
	if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_Read_EEPROM(crateInfoParams,FEB_CTRL_EEPROM,feBoardIndex,dummy, EEPROMUDPDaqPortAdd, portChar,EEPROMUDPDaqPortNbOfBytes);  
 	
	if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_Read_EEPROM(crateInfoParams,FEB_CTRL_EEPROM,feBoardIndex,dummy, EEPROMUDPDaqPortCodeAdd, &code,EEPROM_NB_OF_BYTES_FOR_CALIB_CODES);  
	
	if(errCode == SAMPIC256CH_Success && (code == (EEPROM_PAGE_SIZE - (EEPROMUDPDaqPortCodeAdd + 1))))
	{
		udpDaqPort = (unsigned char)portChar[0] + (((unsigned char)portChar[1])<<8); 
		crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoardIndex].DaqPortFromEEPROM = udpDaqPort;
		
	} 
		 
  return errCode;	
	
}
/* ============================================================================ */
SAMPIC256CH_ErrCode Read_FrontEndBlock_EEPROM_Info (CrateInfoStruct *crateInfoParams,int feBoardIndex, int feBlockIndex) 
/* ============================================================================ */
{
	
SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success;
char boardNumberChar[8]="";
int boardVersion,  boardSerialNumber;
unsigned char code;
unsigned char daughterBoardType;
char charVal;

int sampicVersion;
char sampicEvolutionChar[2];
char sampicVersionChar[2]; 

  	if((feBoardIndex < 0) || (feBlockIndex < 0)) 
 		return SAMPIC256CH_InvalidParam;
	  
	if( crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoardIndex].BoardVersion == 1)
	{
		if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_Read_EEPROM(crateInfoParams,FEB_FE_EEPROM,feBoardIndex,feBlockIndex, EEPROMBoardSerNumAdd, boardNumberChar,EEPROMBoardSerNumNbOfBytes);  
 	
	   	if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_Read_EEPROM(crateInfoParams,FEB_FE_EEPROM,feBoardIndex,feBlockIndex, EEPROMBoardSerNumCodeAdd, &code,EEPROM_NB_OF_BYTES_FOR_CALIB_CODES);  
 

		if((errCode == SAMPIC256CH_Success) && (code == (EEPROM_PAGE_SIZE - (EEPROMBoardSerNumCodeAdd + 1))))
		{

			sscanf(boardNumberChar , "%d.%d", &boardVersion, &boardSerialNumber);
			crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoardIndex].FeBlockVersion[feBlockIndex]= boardVersion;
			crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoardIndex].FeBlockSerNum[feBlockIndex]= boardSerialNumber; 
	
		}
	
	
		//Front End Block Type


		if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_Read_EEPROM(crateInfoParams,FEB_FE_EEPROM,feBoardIndex,feBlockIndex, EEPROMDaughterBoardTypeAdd, &daughterBoardType, EEPROMDaughterBoardTypeNbOfBytes); 
		if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_Read_EEPROM(crateInfoParams,FEB_FE_EEPROM,feBoardIndex,feBlockIndex, EEPROMDaughterBoardTypeCodeAdd, &code, EEPROM_NB_OF_BYTES_FOR_CALIB_CODES); 
	
	   	if((errCode == SAMPIC256CH_Success) && (code == (EEPROM_PAGE_SIZE - (EEPROMDaughterBoardTypeCodeAdd + 1))))
		{
		   crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoardIndex].FeBoardType[feBlockIndex]=  (FEBoardType_t)daughterBoardType;
	  
		}
	
	
	  // Front End Block Version

	   	if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_Read_EEPROM(crateInfoParams,FEB_FE_EEPROM,feBoardIndex,feBlockIndex, EEPROMDaughterBoardSampicVersionAdd, sampicVersionChar, EEPROMDaughterBoardSampicVersionNbOfBytes); 

		if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_Read_EEPROM(crateInfoParams,FEB_FE_EEPROM,feBoardIndex,feBlockIndex, EEPROMDaughterBoardSampicVersionCodeAdd, &code, EEPROM_NB_OF_BYTES_FOR_CALIB_CODES);  

		if((errCode == SAMPIC256CH_Success) && (code == (EEPROM_PAGE_SIZE - (EEPROMDaughterBoardSampicVersionCodeAdd + 1))))
		{
		   	sampicVersionChar[1]= '\0';    
			sscanf(sampicVersionChar , "%d", &sampicVersion);
		
			crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoardIndex].SampicVersion[feBlockIndex]= sampicVersion;

		}

		 // Front End Block Evolution  

	
	   	if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_Read_EEPROM(crateInfoParams,FEB_FE_EEPROM,feBoardIndex,feBlockIndex, EEPROMDaughterBoardSampicEvolutionAdd, sampicEvolutionChar, EEPROMDaughterBoardSampicEvolutionNbOfBytes); 
	
		if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_Read_EEPROM(crateInfoParams,FEB_FE_EEPROM,feBoardIndex,feBlockIndex, EEPROMDaughterBoardSampicEvolutionCodeAdd, &code, EEPROM_NB_OF_BYTES_FOR_CALIB_CODES);  

		if((errCode == SAMPIC256CH_Success) && (code == (EEPROM_PAGE_SIZE - (EEPROMDaughterBoardSampicEvolutionCodeAdd + 1))))
		{
			sampicEvolutionChar[1] = '\0'; 
			crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoardIndex].SampicEvolution[feBlockIndex] = sampicEvolutionChar[0];
	
		}
	

		// Daughter Board TOT Dac Offset

		if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_Read_EEPROM(crateInfoParams,FEB_FE_EEPROM,feBoardIndex,feBlockIndex, EEPROMTOTDacOffsetAdd, &charVal, EEPROMTOTDacOffsetNbOfBytes); 
		if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_Read_EEPROM(crateInfoParams,FEB_FE_EEPROM,feBoardIndex,feBlockIndex, EEPROMTOTDacOffsetCodeAdd, &code, EEPROM_NB_OF_BYTES_FOR_CALIB_CODES);  
	   	if(code == (EEPROM_PAGE_SIZE - (EEPROMTOTDacOffsetCodeAdd + 1)))
		{
		    crateInfoParams->CrateCalibInfo.CalibValues[feBoardIndex].TOTDacOffset[feBlockIndex]=  ((float)charVal/1000.0); // en Volts 
		}

		//Daugther Board TOT Filter Dac Offset

	
		if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_Read_EEPROM(crateInfoParams,FEB_FE_EEPROM,feBoardIndex,feBlockIndex, EEPROMTOTFilterDacOffsetAdd, &charVal, EEPROMTOTFilterDacOffsetNbOfBytes); 
		if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_Read_EEPROM(crateInfoParams,FEB_FE_EEPROM,feBoardIndex,feBlockIndex, EEPROMTOTFilterDacOffsetCodeAdd, &code, EEPROM_NB_OF_BYTES_FOR_CALIB_CODES);  
   	
		if((errCode == SAMPIC256CH_Success) && (code == (EEPROM_PAGE_SIZE - (EEPROMTOTFilterDacOffsetCodeAdd + 1))))
		{
		   crateInfoParams->CrateCalibInfo.CalibValues[feBoardIndex].TOTFilterDacOffset[feBlockIndex]= ((float)charVal/1000.0); // en Volts
		}

	}
	else // no EEPROM on FE Blocks from board version >= 2
	{
		//temporary to read from FE BOARD CTRL EEPROM!!!
		
		  crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoardIndex].FeBoardType[feBlockIndex]   =  SAMPIC_BYPASS_BOARD;
		  crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoardIndex].SampicVersion[feBlockIndex] =   5;
		  crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoardIndex].SampicEvolution[feBlockIndex] = 'A';
		  crateInfoParams->CrateCalibInfo.CalibValues[feBoardIndex].TOTDacOffset[feBlockIndex]   =  0;
		  crateInfoParams->CrateCalibInfo.CalibValues[feBoardIndex].TOTFilterDacOffset[feBlockIndex] = 0;
	}

	return errCode;
}


///* ============================================================================ */
///* ============================================================================ */
//
///* ============================================================================ */
//ErrorType Write_EEPROM_Info (void) 
///* ============================================================================ */
//{
//char boardNumber[8];
//char mezzaBoardNumber[8];
//int boardVersion, serNum;
//int feIndex;
//char stringVal[16];
//int int1, int2, int3, int4;
//char bufData[16];
//
//int dummy = 0;
//ErrorType errCode = Success;
//unsigned char  data;
//unsigned short udpPort;
//int daughterBoardType;
//
//
//
//    sprintf(boardNumber, "%x.%d", SystemParams.BoardVersion, SystemParams.BoardSerialNumber);  
//
//	ResetTextBox(EEPROMPanelHandle, PANEL_EEP_MOTHERBOARDSERNUM, boardNumber);
//
//    //Mother Board Serial Number
//	
//	if(errCode == Success) errCode = Write_EEPROM(CTRL_EEPROM, dummy, EEPROMBoardSerNumAdd, strlen(boardNumber)+1, boardNumber); 
//
//	data = (unsigned char)(EEPROM_PAGE_SIZE - (EEPROMBoardSerNumCodeAdd + 1));
//	
//	if(errCode == Success) errCode = Write_EEPROM(CTRL_EEPROM, dummy, EEPROMBoardSerNumCodeAdd, EEPROM_NB_OF_BYTES_FOR_CALIB_CODES, &data);  
//
//	sprintf(Message,"Board Number %s written to MOTHER BOARD EEPROM\n", boardNumber);
//	
//	Write_InfoMessage(Message, INFO, NO_POPUP);
//	
//	//Mother Board UDP IP ADDRESS
//	
//	 GetCtrlVal(EEPROMPanelHandle,PANEL_EEP_EEPROM_UDP_IP_ADDRESS, stringVal);
//	 if(sscanf(stringVal, "%d.%d.%d.%d", &int1, &int2, &int3, &int4) != 4) 
//	 {
//						
//			MessagePopup ("WARNING!", "Invalid IP Address! format should be: 'xxx.xxx.xxx.xxx");
//			ResetTextBox(EEPROMPanelHandle,PANEL_EEP_EEPROM_UDP_IP_ADDRESS, ""); 	
//	 }
//	else
//	{
//		
//		 bufData[0]= (unsigned char)int4;
//		 bufData[1]= (unsigned char)int3; 
//		 bufData[2]= (unsigned char)int2; 
//		 bufData[3]= (unsigned char)int1; 
//		  
//		 if(errCode == Success) errCode = Write_EEPROM(CTRL_EEPROM, dummy, EEPROMIpAddressUDPAdd, EEPROMIpAddressUDPNbOfBytes, bufData); 
//
//	 	 data = (unsigned char)(EEPROM_PAGE_SIZE - (EEPROMIpAddressCodeAdd + 1));
//		 if(errCode == Success) errCode = Write_EEPROM(CTRL_EEPROM, dummy, EEPROMIpAddressCodeAdd, EEPROM_NB_OF_BYTES_FOR_CALIB_CODES, &data);  
//
//	}
//	 
//	//Mother Board UDP PORT
//	 
//	 GetCtrlVal(EEPROMPanelHandle,PANEL_EEP_EEPROM_UDP_PORT, &udpPort); 
//	 if(errCode == Success) errCode = Write_EEPROM(CTRL_EEPROM, dummy, EEPROMUDPPortAdd, EEPROMUDPPortNbOfBytes, &udpPort);
//	 
//	 data = (unsigned char)(EEPROM_PAGE_SIZE - (EEPROMUDPPortCodeAdd + 1));
//	
//	 if(errCode == Success) errCode = Write_EEPROM(CTRL_EEPROM, dummy, EEPROMUDPPortCodeAdd, EEPROM_NB_OF_BYTES_FOR_CALIB_CODES, &data);  
//
//	
//	//Mezzanine Board Serial Number  
//	 
//	 
//	GetCtrlVal(EEPROMPanelHandle, PANEL_EEP_MEZZANINESERNUM, mezzaBoardNumber);
//	GetCtrlVal(EEPROMPanelHandle, PANEL_EEP_SELSAMPIC, &feIndex);
//	
//	if(sscanf(mezzaBoardNumber, "%d.%d", &boardVersion, &serNum) != 2)
//	{
//		sprintf(Message,  "Invalid Serial Number for Daughter Board index %d : format should be: 'x.x'", feIndex);
//		MessagePopup ("WARNING!", Message);
//		ResetTextBox(EEPROMPanelHandle,PANEL_EEP_MEZZANINESERNUM, ""); 
//
//	}
//	else
//	{
//		
//		
//		SystemParams.DaugtherBoardVersion[feIndex] = boardVersion;
//		SystemParams.DaugtherBoardSerialNumber[feIndex] =  serNum;
//		
//		if(errCode == Success) errCode = Write_EEPROM(FE_EEPROM, feIndex, EEPROMBoardSerNumAdd, strlen(mezzaBoardNumber)+1, mezzaBoardNumber); 
//		data = (unsigned char)(EEPROM_PAGE_SIZE - (EEPROMBoardSerNumCodeAdd + 1));
//	
//		if(errCode == Success) errCode = Write_EEPROM(FE_EEPROM, feIndex, EEPROMBoardSerNumCodeAdd, EEPROM_NB_OF_BYTES_FOR_CALIB_CODES, &data);  
//
//		
//	}
//	
//	
//	//Mezzanine Type
//	
//	GetCtrlVal(EEPROMPanelHandle, PANEL_EEP_DAUGHTER_BOARD_TYPE,&daughterBoardType);
//	
//	if(daughterBoardType == -1) 
//	{
//		sprintf(Message,  "Invalid Daughter Board Type for Daughter Board index %d : format should be: 'x.x'", feIndex);
//		MessagePopup ("WARNING!", Message);
//	}
//	else
//	{
//	
//		data =   (unsigned char) daughterBoardType;
//		if(errCode == Success) errCode = Write_EEPROM(FE_EEPROM, feIndex, EEPROMDaughterBoardTypeAdd, EEPROMDaughterBoardTypeNbOfBytes,&data); 
//		data =  (unsigned char) (EEPROM_PAGE_SIZE - (EEPROMDaughterBoardTypeCodeAdd + 1));
//	
//		if(errCode == Success) errCode = Write_EEPROM(FE_EEPROM, feIndex, EEPROMDaughterBoardTypeCodeAdd, EEPROM_NB_OF_BYTES_FOR_CALIB_CODES, &data);  
//	   
//	}
//
//	
//	
//	return errCode;
//	
//	
//
//	
//}
//
///* ============================================================================ */
///* ============================================================================ */
//



// /* ============================================================================ */
//ErrorType Write_TOTDacOffsetToEEPROM (int feIndex) 
///* ============================================================================ */
//{
//	
//	char charVal;
//	unsigned char code;
//	
//		
//	ErrorType errCode = Success;
//	GetCtrlVal(EEPROMPanelHandle, PANEL_EEP_TOT_DAC_OFFSET, &charVal);
//
//	
//	if(errCode == Success) errCode = Write_EEPROM(FE_EEPROM, feIndex, EEPROMTOTDacOffsetAdd, EEPROMTOTDacOffsetNbOfBytes, &charVal); 
//	code =  (unsigned char)(EEPROM_PAGE_SIZE - (EEPROMTOTDacOffsetCodeAdd + 1));
//	
//	if(errCode == Success) errCode = Write_EEPROM(FE_EEPROM, feIndex, EEPROMTOTDacOffsetCodeAdd, EEPROM_NB_OF_BYTES_FOR_CALIB_CODES, &code);  
//   
//	
//
//	if(errCode == Success)
//	{	
//		sprintf(Message, "Daugher Board index %d TOT DAC Offset successfully written to EEPROM!\n" ,feIndex); 
//		Write_InfoMessage(Message,INFO, NO_POPUP);
//	
//		
//	}
//	return errCode;
//	
//	
//}
//
// /* ============================================================================ */
//ErrorType Write_TOTFilterDacOffsetToEEPROM (int feIndex) 
///* ============================================================================ */
//{
//	
//	char charVal;
//	unsigned char code;
//	
//		
//	ErrorType errCode = Success;
//	GetCtrlVal(EEPROMPanelHandle, PANEL_EEP_TOT_FILTER_DAC_OFFSET, &charVal);
//
//	if(errCode == Success) errCode = Write_EEPROM(FE_EEPROM, feIndex, EEPROMTOTFilterDacOffsetAdd, EEPROMTOTFilterDacOffsetNbOfBytes, &charVal); 
//	code =  (unsigned char)(EEPROM_PAGE_SIZE - (EEPROMTOTFilterDacOffsetCodeAdd + 1));
//	
//	if(errCode == Success) errCode = Write_EEPROM(FE_EEPROM, feIndex, EEPROMTOTFilterDacOffsetCodeAdd, EEPROM_NB_OF_BYTES_FOR_CALIB_CODES, &code);  
//   
//	
//
//	if(errCode == Success)
//	{	
//		sprintf(Message, "Daugher Board index %d TOT Filter DAC Offset successfully written to EEPROM!\n" ,feIndex); 
//		Write_InfoMessage(Message,INFO, NO_POPUP);
//	
//		
//	}
//	return errCode;
//	
//	
//}
//
//
//
/* ============================================================================ */
/* =================  Chargement des paramètres  dans la carte ================ */
/* ============================================================================ */




/* =========================================================================== */							
SAMPIC256CH_ErrCode	Load_ControlBoardSi5332DefaultConfig (CrateInfoStruct *crateInfoParams) 
/* =========================================================================== */							
{
	
 int n;
 int dummy = 0;
 unsigned char regToLoadAdd, dataToLoad;
 unsigned short data;

 SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success;

 for(n = 0; n < SI5332_GM2_REVD_REG_CONFIG_NUM_REGS; n++)
 {
	data =  (unsigned short)(SI5332E_DEFAULT_CONFIG[n].address & 0xFF) + (unsigned short)(SI5332E_DEFAULT_CONFIG[n].value << 8);

	if(errCode == SAMPIC256CH_Success)  errCode = SAMPIC256CH_BusWriteWords(crateInfoParams, CTRL_ACCESS, CB_CTRL_FPGA, dummy, dummy, ad_control_board_si5332_access, &data,2);  
	  
	
 }

 dsleep(100);

 return errCode;	

}


/* =========================================================================== */							
SAMPIC256CH_ErrCode Load_ControlBoard_Si5332Reg	(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams)
/* =========================================================================== */							
{
 int n;
 int dummy = 0;
 unsigned char regToLoadAdd, dataToLoad;
 unsigned short dataToLoadShort;
 unsigned int data;
 Si5332_I2CRegAccessStruct si5332_I2CRegAccessValues[SI5332_GM2_REVD_REG_CONFIG_SINGLE_REG];
 SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success;	
	

	// Update of the O0 register

	data =  0x0106; // Set device in Ready mode
	if(errCode == SAMPIC256CH_Success)  errCode = SAMPIC256CH_BusWriteWords(crateInfoParams,  CTRL_ACCESS, CB_CTRL_FPGA, dummy, dummy, ad_control_board_si5332_access, &data,2); 	
	
	//0x2B
	regToLoadAdd = SI5332_REG_O0_ADD;
	dataToLoad = crateParams->ControlBoardParams.Si5332RegO0 & 0xFF;
	data =  (unsigned short)(regToLoadAdd & 0xFF) + (unsigned short)(dataToLoad << 8);  // L'interface I2C envoie les bytes en LSB first
	
	if(errCode == SAMPIC256CH_Success)  errCode =  SAMPIC256CH_BusWriteWords(crateInfoParams,  CTRL_ACCESS, CB_CTRL_FPGA, dummy, dummy, ad_control_board_si5332_access, &data,2); 	
	
	// 0x25			MUX0
	regToLoadAdd = SI5332_REG_MUX0_CELL_ADD;
	dataToLoad = crateParams->ControlBoardParams.Si5332OMux0_Cell & 0xFF;
	data =  (unsigned short)(regToLoadAdd & 0xFF) + (unsigned short)(dataToLoad << 8);  // L'interface I2C envoie les bytes en LSB first
	if(errCode == SAMPIC256CH_Success)  errCode =  SAMPIC256CH_BusWriteWords(crateInfoParams,  CTRL_ACCESS, CB_CTRL_FPGA, dummy, dummy, ad_control_board_si5332_access, &data,2); 
	//0x26	  	MUX1 
	regToLoadAdd = SI5332_REG_MUX0_CELL_ADD +1;
	dataToLoad = crateParams->ControlBoardParams.Si5332OMux0_Cell & 0xFF;
	data =  (unsigned short)(regToLoadAdd & 0xFF) + (unsigned short)(dataToLoad << 8);  // L'interface I2C envoie les bytes en LSB first
	if(errCode == SAMPIC256CH_Success)  errCode =  SAMPIC256CH_BusWriteWords(crateInfoParams,  CTRL_ACCESS, CB_CTRL_FPGA, dummy, dummy, ad_control_board_si5332_access, &data,2); 
	//0x27   	MUX2 
	regToLoadAdd = SI5332_REG_MUX0_CELL_ADD +2;
	dataToLoad = crateParams->ControlBoardParams.Si5332OMux0_Cell & 0xFF;
	data =  (unsigned short)(regToLoadAdd & 0xFF) + (unsigned short)(dataToLoad << 8);  // L'interface I2C envoie les bytes en LSB first
	if(errCode == SAMPIC256CH_Success)  errCode =  SAMPIC256CH_BusWriteWords(crateInfoParams,  CTRL_ACCESS, CB_CTRL_FPGA, dummy, dummy, ad_control_board_si5332_access, &data,2); 	
	//0x28    	MUX3 
	regToLoadAdd = SI5332_REG_MUX0_CELL_ADD +3;
	dataToLoad = crateParams->ControlBoardParams.Si5332OMux0_Cell & 0xFF;
	data =  (unsigned short)(regToLoadAdd & 0xFF) + (unsigned short)(dataToLoad << 8);  // L'interface I2C envoie les bytes en LSB first
	if(errCode == SAMPIC256CH_Success)  errCode =  SAMPIC256CH_BusWriteWords(crateInfoParams,  CTRL_ACCESS, CB_CTRL_FPGA, dummy, dummy, ad_control_board_si5332_access, &data,2); 	
	//0x29    	MUX4 
	regToLoadAdd = SI5332_REG_MUX0_CELL_ADD +4;
	dataToLoad = crateParams->ControlBoardParams.Si5332OMux0_Cell & 0xFF;
	data =  (unsigned short)(regToLoadAdd & 0xFF) + (unsigned short)(dataToLoad << 8);  // L'interface I2C envoie les bytes en LSB first
	if(errCode == SAMPIC256CH_Success)  errCode =  SAMPIC256CH_BusWriteWords(crateInfoParams,  CTRL_ACCESS, CB_CTRL_FPGA, dummy, dummy, ad_control_board_si5332_access, &data,2); 	
	
	
	//0x67  - 2 bytes
	
	regToLoadAdd = SI5332_REG_IDPA_ADD;
	dataToLoadShort = crateParams->ControlBoardParams.Si5332Idpa;
	data =  (unsigned int)(regToLoadAdd & 0xFF) + (unsigned int)((unsigned int)dataToLoadShort << 8);  // L'interface I2C envoie les bytes en LSB first
	if(errCode == SAMPIC256CH_Success)  errCode =   SAMPIC256CH_BusWriteWords(crateInfoParams,  CTRL_ACCESS, CB_CTRL_FPGA, dummy, dummy, ad_control_board_si5332_access, &data,2); 

	
	//0x75
	regToLoadAdd = SI5332_REG_P_VALUE_ADD;
	dataToLoad = crateParams->ControlBoardParams.Si5332PDivider & 0xFF;
	data =  (unsigned short)(regToLoadAdd & 0xFF) + (unsigned short)(dataToLoad << 8);  // L'interface I2C envoie les bytes en LSB first
	if(errCode == SAMPIC256CH_Success)  errCode = SAMPIC256CH_BusWriteWords(crateInfoParams,  CTRL_ACCESS, CB_CTRL_FPGA, dummy, dummy, ad_control_board_si5332_access, &data,2); 	
	
	
	//0x7B
	regToLoadAdd = SI5332_REG_OUT0_DIV_ADD;
	dataToLoad = crateParams->ControlBoardParams.Si5332OutDivider & 0xFF;
	data =  (unsigned short)(regToLoadAdd & 0xFF) + (unsigned short)(dataToLoad << 8);  // L'interface I2C envoie les bytes en LSB first
	if(errCode == SAMPIC256CH_Success)  errCode =  SAMPIC256CH_BusWriteWords(crateInfoParams,  CTRL_ACCESS, CB_CTRL_FPGA, dummy, dummy, ad_control_board_si5332_access, &data,2); 
	
	
	//0x80
	regToLoadAdd = SI5332_REG_OUT1_DIV_ADD;
	dataToLoad = crateParams->ControlBoardParams.Si5332OutDivider & 0xFF;
	data =  (unsigned short)(regToLoadAdd & 0xFF) + (unsigned short)(dataToLoad << 8);  // L'interface I2C envoie les bytes en LSB first
	if(errCode == SAMPIC256CH_Success)  errCode =  SAMPIC256CH_BusWriteWords(crateInfoParams,  CTRL_ACCESS, CB_CTRL_FPGA, dummy, dummy, ad_control_board_si5332_access, &data,2); 
	
	//0x8A
	
	regToLoadAdd = SI5332_REG_OUT2_DIV_ADD;
	dataToLoad = crateParams->ControlBoardParams.Si5332OutDivider & 0xFF;
	data =  (unsigned short)(regToLoadAdd & 0xFF) + (unsigned short)(dataToLoad << 8);  // L'interface I2C envoie les bytes en LSB first
	if(errCode == SAMPIC256CH_Success)  errCode =  SAMPIC256CH_BusWriteWords(crateInfoParams,  CTRL_ACCESS, CB_CTRL_FPGA, dummy, dummy, ad_control_board_si5332_access, &data,2); 
	
	//0x99
	regToLoadAdd = SI5332_REG_OUT4_DIV_ADD;
	dataToLoad = crateParams->ControlBoardParams.Si5332OutDivider & 0xFF;
	data =  (unsigned short)(regToLoadAdd & 0xFF) + (unsigned short)(dataToLoad << 8);  // L'interface I2C envoie les bytes en LSB first
	if(errCode == SAMPIC256CH_Success)  errCode =  SAMPIC256CH_BusWriteWords(crateInfoParams,  CTRL_ACCESS, CB_CTRL_FPGA, dummy, dummy, ad_control_board_si5332_access, &data,2); ;	
	
	
	//0xA8
	regToLoadAdd = SI5332_REG_OUT6_DIV_ADD;
	dataToLoad = crateParams->ControlBoardParams.Si5332OutDivider & 0xFF;
	data =  (unsigned short)(regToLoadAdd & 0xFF) + (unsigned short)(dataToLoad << 8);  // L'interface I2C envoie les bytes en LSB first
	if(errCode == SAMPIC256CH_Success)  errCode =  SAMPIC256CH_BusWriteWords(crateInfoParams,  CTRL_ACCESS, CB_CTRL_FPGA, dummy, dummy, ad_control_board_si5332_access, &data,2); 
	
	//0xAD
	regToLoadAdd = SI5332_REG_OUT7_DIV_ADD;
	dataToLoad = crateParams->ControlBoardParams.Si5332OutDivider & 0xFF;
	data =  (unsigned short)(regToLoadAdd & 0xFF) + (unsigned short)(dataToLoad << 8);  // L'interface I2C envoie les bytes en LSB first
	if(errCode == SAMPIC256CH_Success)  errCode =  SAMPIC256CH_BusWriteWords(crateInfoParams,  CTRL_ACCESS, CB_CTRL_FPGA, dummy, dummy, ad_control_board_si5332_access, &data,2); 

	//0xBE
	
	regToLoadAdd = SI5332_REG_PLL_MODE_ADD;
	dataToLoad = crateParams->ControlBoardParams.Si5332PllMode & 0xFF;
	data =  (unsigned short)(regToLoadAdd & 0xFF) + (unsigned short)(dataToLoad << 8);  // L'interface I2C envoie les bytes en LSB first
	if(errCode == SAMPIC256CH_Success)  errCode = SAMPIC256CH_BusWriteWords(crateInfoParams, CTRL_ACCESS, CB_CTRL_FPGA, dummy, dummy, ad_control_board_si5332_access, &data,2); 


	// Active Mode
	
	data =  0x0206; // Set device back in Active mode
	if(errCode == SAMPIC256CH_Success)  errCode =  SAMPIC256CH_BusWriteWords(crateInfoParams, CTRL_ACCESS, CB_CTRL_FPGA, dummy, dummy, ad_control_board_si5332_access, &data,2); 
	
	
	
    dsleep(100); 
	return errCode;
	
	
	
}



/* =========================================================================== */							
SAMPIC256CH_ErrCode	Load_FeBoardSi5332DefaultConfig (CrateInfoStruct *crateInfoParams, int feBoardIndex)
/* =========================================================================== */							
{
 int n;
 int dummy = 0;
 unsigned char regToLoadAdd, dataToLoad;
 unsigned short data;

 
 SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success;
 
 // on fait un accès en aveugle et on ne vérifie pas que le Si5332 est présent??
 
 for(n = 0; n < SI5332_FE_GM2_REVD_REG_CONFIG_NUM_REGS; n++)
 {
	data =  (unsigned short)(SI5332E_DEFAULT_FE_CONFIG[n].address & 0xFF) + (unsigned short)(SI5332E_DEFAULT_FE_CONFIG[n].value << 8);

	if(errCode == SAMPIC256CH_Success)  errCode = SAMPIC256CH_BusWriteWords(crateInfoParams, CTRL_ACCESS, FEB_CTRL_FPGA,feBoardIndex, dummy, ad_fe_board_ctrl_si5332_access, &data,2);  
  

 }

dsleep(100);
 

 return errCode;	

	
}
/* =========================================================================== */							
SAMPIC256CH_ErrCode Load_FeBoard_Si5332Reg	(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoardIndex) 
/* =========================================================================== */							
{
	
 int n;
 int dummy = 0;
 unsigned char regToLoadAdd, dataToLoad;
 unsigned short dataToLoadShort;
 unsigned int data;
 Si5332_I2CRegAccessStruct si5332_I2CRegAccessValues[SI5332_GM2_REVD_REG_CONFIG_SINGLE_REG];
 
 SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success;
 
 
    if((feBoardIndex < 0)|| (feBoardIndex >= crateInfoParams->NbOfFeBoards))  
		return SAMPIC256CH_InvalidParam;

    if( crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoardIndex].Si5332IsPresent == FALSE)
		return SAMPIC256CH_FunctionNotAllowed;
	
  	// Update of the O0 register

	data =  0x0106; // Set device in Ready mode
	
	if(errCode == SAMPIC256CH_Success)  errCode = SAMPIC256CH_BusWriteWords(crateInfoParams, CTRL_ACCESS, FEB_CTRL_FPGA,feBoardIndex, dummy, ad_fe_board_ctrl_si5332_access, &data,2);  

	// 0x25
	regToLoadAdd = SI5332_REG_MUX0_CELL_ADD;
	dataToLoad = crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.Si5332OMux0_Cell & 0xFF;
	data =  (unsigned short)(regToLoadAdd & 0xFF) + (unsigned short)(dataToLoad << 8);  // L'interface I2C envoie les bytes en LSB first
	if(errCode == SAMPIC256CH_Success)  errCode = SAMPIC256CH_BusWriteWords(crateInfoParams, CTRL_ACCESS, FEB_CTRL_FPGA,feBoardIndex, dummy, ad_fe_board_ctrl_si5332_access, &data,2);  
    
	// 0x26     
 	regToLoadAdd = SI5332_REG_MUX0_CELL_ADD + 1;
	dataToLoad = crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.Si5332OMux0_Cell & 0xFF;
	data =  (unsigned short)(regToLoadAdd & 0xFF) + (unsigned short)(dataToLoad << 8);  // L'interface I2C envoie les bytes en LSB first
	if(errCode == SAMPIC256CH_Success)  errCode = SAMPIC256CH_BusWriteWords(crateInfoParams, CTRL_ACCESS, FEB_CTRL_FPGA,feBoardIndex, dummy, ad_fe_board_ctrl_si5332_access, &data,2);  

	// 0x27     
 	regToLoadAdd = SI5332_REG_MUX0_CELL_ADD + 2;
	dataToLoad = crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.Si5332OMux0_Cell & 0xFF;
	data =  (unsigned short)(regToLoadAdd & 0xFF) + (unsigned short)(dataToLoad << 8);  // L'interface I2C envoie les bytes en LSB first
	if(errCode == SAMPIC256CH_Success)  errCode = SAMPIC256CH_BusWriteWords(crateInfoParams, CTRL_ACCESS, FEB_CTRL_FPGA,feBoardIndex, dummy, ad_fe_board_ctrl_si5332_access, &data,2);  
	
	
	// 0x28     
 	regToLoadAdd = SI5332_REG_MUX0_CELL_ADD + 3;
	dataToLoad = crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.Si5332OMux0_Cell & 0xFF;
	data =  (unsigned short)(regToLoadAdd & 0xFF) + (unsigned short)(dataToLoad << 8);  // L'interface I2C envoie les bytes en LSB first
	if(errCode == SAMPIC256CH_Success)  errCode = SAMPIC256CH_BusWriteWords(crateInfoParams, CTRL_ACCESS, FEB_CTRL_FPGA,feBoardIndex, dummy, ad_fe_board_ctrl_si5332_access, &data,2);  
	
	// 0x29     
 	regToLoadAdd = SI5332_REG_MUX0_CELL_ADD + 4;
	dataToLoad = crateParams->FeBoardParams[feBoardIndex].ControlFpgaParams.Si5332OMux0_Cell & 0xFF;
	data =  (unsigned short)(regToLoadAdd & 0xFF) + (unsigned short)(dataToLoad << 8);  // L'interface I2C envoie les bytes en LSB first
	if(errCode == SAMPIC256CH_Success)  errCode = SAMPIC256CH_BusWriteWords(crateInfoParams, CTRL_ACCESS, FEB_CTRL_FPGA,feBoardIndex, dummy, ad_fe_board_ctrl_si5332_access, &data,2);  
	
	// Active Mode
	
	data =  0x0206; // Set device back in Active mode
	if(errCode == SAMPIC256CH_Success)  errCode = SAMPIC256CH_BusWriteWords(crateInfoParams, CTRL_ACCESS, FEB_CTRL_FPGA,feBoardIndex, dummy, ad_fe_board_ctrl_si5332_access, &data,2);  
	
    dsleep(100); 
	return errCode;
	
}

/* =========================================================================== */							
SAMPIC256CH_ErrCode Load_SampicSCReg(CrateInfoStruct *crateInfoParams, int feBoardIndex,int feFpgaIndex, unsigned int sampicSCRegAdd, unsigned int sampicSCRegValue)
/* =========================================================================== */							

{
unsigned int data ;
unsigned char readData[3] ; 
unsigned long ulongData;
unsigned char sub_address; 
int nframes;
SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success;      

    if((feBoardIndex < 0)|| (feFpgaIndex < 0))  
		return SAMPIC256CH_InvalidParam;
	
	data = sampicSCRegAdd; // reg add in sampic
	sub_address = ad_fe_board_fe_sampic_slow_reg_add; 
	
	errCode = SAMPIC256CH_BusWriteWords (crateInfoParams,CTRL_ACCESS, FEB_FE_FPGA,feBoardIndex, feFpgaIndex, sub_address, &data,1);
	if(errCode != SAMPIC256CH_Success)
		return errCode;
	
	data = sampicSCRegValue; 
 	sub_address = ad_fe_board_fe_sampic_slow_control_access;
   
	errCode = SAMPIC256CH_BusWriteWords (crateInfoParams,CTRL_ACCESS, FEB_FE_FPGA,feBoardIndex, feFpgaIndex, sub_address, &data,2); 

	if(errCode != SAMPIC256CH_Success) 
		return errCode;
	
 	dsleep(2);    
	
	errCode = SAMPIC256CH_BusReadWords (crateInfoParams,CTRL_ACCESS, FEB_FE_FPGA,feBoardIndex, feFpgaIndex, sub_address, readData,2); 

	if(errCode != SAMPIC256CH_Success) 
	   return errCode;  
	
	
	
	if(((unsigned short)readData[0] + (((unsigned short)readData[1])<<8)) != (unsigned short)data)  
	{

		errCode = SAMPIC256CH_BusWriteWords (crateInfoParams,CTRL_ACCESS, FEB_FE_FPGA,feBoardIndex, feFpgaIndex, sub_address, &data,2); 

		if(errCode != SAMPIC256CH_Success) 
			return errCode;
	
 		dsleep(2);    
	
		errCode = SAMPIC256CH_BusReadWords (crateInfoParams,CTRL_ACCESS, FEB_FE_FPGA,feBoardIndex, feFpgaIndex, sub_address, readData,2); 

		if(errCode != SAMPIC256CH_Success) 
	   		return errCode;  
	
			
		if(((unsigned short)readData[0] + (((unsigned short)readData[1])<<8)) != (unsigned short)data)
		{
			return SAMPIC256CH_SAMPICSlowControlAccessError;	
		}
	}
	else 
		errCode = SAMPIC256CH_Success;
	
	return errCode;

}

/* =========================================================================== */							
SAMPIC256CH_ErrCode Load_HardwareSetup(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, BoardRegType_t toBeLoaded,  int feBoardIndex, int feFPGAIndex, int channelIndex)
/* =========================================================================== */							
// Si on s'adresse à un registre du controller_FPGA, feFpgaIndex et channelIndex sont ignorés.
// Si feFpgaIndex vaut -1, on est en broadcast si c'est un paramètre commun, ou en boucle si c'est un paramètre individuel.
{
unsigned int data ;
unsigned char readData[3] ; 
unsigned long ulongData;
unsigned char sub_address; 
int i, j;
unsigned int sampicConfigRegAdd;
unsigned int sampicConfigRegValue;
int handle;
int feboardIdx, feFPGAIdx, channelIdx;
int startFeIndex, endFeIndex;
int startFeFPGAIndex, endFeFPGAIndex;
int startChannelIndex, endChannelIndex;
int dummy = 0;

SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success;

	 if( crateInfoParams->ConnectionInfo.CtrlDeviceHandle < 0)
			return SAMPIC256CH_NoCrateConnected;
	 
	if(feBoardIndex < 0)
	{
		
		startFeIndex = 0;
		endFeIndex = crateInfoParams->NbOfFeBoards -1;
	}
	else
	{
		
		startFeIndex = feBoardIndex;
		endFeIndex = feBoardIndex;
	}
  	
	
	if(feFPGAIndex < 0)
	{
		startFeFPGAIndex = 0;	
		endFeFPGAIndex =  NB_OF_SAMPICS_IN_FE_BOARD -1;
	}
	else
	{
		
		startFeFPGAIndex = feFPGAIndex;	
		endFeFPGAIndex = feFPGAIndex;
	
	}
	
	if(channelIndex < 0)
	{
	
		startChannelIndex = 0;
		endChannelIndex = NB_OF_CHANNELS_IN_SAMPIC -1;
	
		
	}
	else
	{
	    startChannelIndex = channelIndex;
		endChannelIndex = channelIndex;
		
	}
/*#ifndef SAMPIC_DLL_EXPORTS	
	while(BoardAccessReq == TRUE);  // pas d'accès au hardware si un autre accès en cours (ex : DACs)		 comment fait-on pour la librairie??
	BoardAccessReq = TRUE;
#endif	   */
   
	
    /*========== START CONTROL BOARD FPGA REGISTERS ========*/ 
	
	if((toBeLoaded == CTRLB_CONTROL_REG) || (toBeLoaded == CTRLB_ALL_REGS)|| (toBeLoaded == ALL_REGS)) 
	{
		
		data = crateParams->ControlBoardParams.ControlRegister;
		sub_address = ad_control_board_control_reg;
		
		if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_BusWriteWords (crateInfoParams, CTRL_ACCESS, CB_CTRL_FPGA, dummy ,dummy, sub_address, &data,1);     
		 
		 
	}
	
		
	if((toBeLoaded == CTRLB_CONTROL_REG2) || (toBeLoaded == CTRLB_ALL_REGS)|| (toBeLoaded == ALL_REGS)) 
	{
		
		data = crateParams->ControlBoardParams.ControlRegister2;
		sub_address = ad_control_board_control_reg2;
		
		if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_BusWriteWords (crateInfoParams, CTRL_ACCESS, CB_CTRL_FPGA, dummy ,dummy, sub_address, &data,1);     
		 
		 
	}
	
	if((toBeLoaded == CTRLB_EXT_TRIG_GATE_FOR_L3) || (toBeLoaded == CTRLB_ALL_REGS)|| (toBeLoaded == ALL_REGS)) 
	{
		
		data = crateParams->ControlBoardParams.ExternalTrigGateForL3;
		sub_address = ad_control_board_ext_trig_gate_for_l3;
		
		if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_BusWriteWords (crateInfoParams, CTRL_ACCESS, CB_CTRL_FPGA, dummy ,dummy, sub_address, &data,1);     
		 
	}	
	
	if((toBeLoaded == CTRLB_TRIGGER_COMBI_LOGIC_FOR_L3) || (toBeLoaded == CTRLB_ALL_REGS)|| (toBeLoaded == ALL_REGS)) 
	{
		
		data = crateParams->ControlBoardParams.TriggerCombiLogicForL3;
		sub_address = ad_control_board_trigger_combi_for_l3;
		
		if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_BusWriteWords (crateInfoParams, CTRL_ACCESS, CB_CTRL_FPGA, dummy ,dummy, sub_address, &data,4);     
		 
		 
	}
		
	if((toBeLoaded == CTRLB_TRIGGER_REG) || (toBeLoaded == CTRLB_ALL_REGS)|| (toBeLoaded == ALL_REGS)) 
	{
		 
		data = crateParams->ControlBoardParams.TriggerRegister;
		sub_address = ad_control_board_trigger_reg;
		
		if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_BusWriteWords (crateInfoParams, CTRL_ACCESS, CB_CTRL_FPGA, dummy ,dummy, sub_address, &data,1);     
		 
		 
	}
	
	if((toBeLoaded == CTRLB_CLOCK_REG) || (toBeLoaded == CTRLB_ALL_REGS)|| (toBeLoaded == ALL_REGS)) 
	{
		
		if(crateInfoParams->CrateBoardsInfo.ControlBoardInfo.Si5332IsPresent == TRUE)
		{
			
			if(errCode == SAMPIC256CH_Success) errCode = Load_ControlBoard_Si5332Reg(crateInfoParams, crateParams);  
			
		}
		else
		{
			
			data = crateParams->ControlBoardParams.ClockRegister & 0xFFFF;
			sub_address = ad_control_board_clock_reg;
		
			if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_BusWriteWords (crateInfoParams, CTRL_ACCESS, CB_CTRL_FPGA, dummy ,dummy, sub_address, &data,2); 
		}
	 
		 
	}
	
	if((toBeLoaded == CTRLB_PULSE_REG) || (toBeLoaded == CTRLB_ALL_REGS)|| (toBeLoaded == ALL_REGS)) 
	{
		 
		data = crateParams->ControlBoardParams.PulseReg;
		sub_address = ad_control_board_pulse_reg;
		
		if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_BusWriteWords (crateInfoParams, CTRL_ACCESS, CB_CTRL_FPGA, dummy ,dummy, sub_address, &data,2);     
	 
		 
	}
	
	
	if((toBeLoaded == CTRLB_NB_OF_TRIGGER_PER_EVENT) || (toBeLoaded == CTRLB_ALL_REGS)|| (toBeLoaded == ALL_REGS))
	{
		data = crateParams->ControlBoardParams.NbOfTriggersPerEvent & 0xFF; 
	 	sub_address = ad_control_board_trigtag_size;
	    if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_BusWriteWords (crateInfoParams,CTRL_ACCESS, CB_CTRL_FPGA,dummy ,dummy, sub_address, &data,1); 
		
	}
	
	
	/*========== END CONTROL BOARD FPGA REGISTERS ========*/
	/*======================================================*/
	
	/*========== START FRONT END BOARD CONTROL FPGA REGISTERS ========*/ 
	
	if((toBeLoaded == FEB_CTRL_CONTROL_REG) || (toBeLoaded == FEB_CTRL_ALL_REGS)|| (toBeLoaded == ALL_REGS))
	{
		
		for(feboardIdx = startFeIndex; feboardIdx <= endFeIndex; feboardIdx++)
		{
			
			
			data = crateParams->FeBoardParams[feboardIdx].ControlFpgaParams.ControlRegister & 0xFF; 
			sub_address = ad_fe_board_ctrl_control_register;
		
			if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_BusWriteWords (crateInfoParams,CTRL_ACCESS, FEB_CTRL_FPGA,feboardIdx ,dummy, sub_address, &data,1);
		}
	
	}
	
	if((toBeLoaded == FEB_CTRL_CONTROL_REG2) || (toBeLoaded == FEB_CTRL_ALL_REGS)|| (toBeLoaded == ALL_REGS))
	{
		for(feboardIdx = startFeIndex; feboardIdx <= endFeIndex; feboardIdx++)
		{
			data = crateParams->FeBoardParams[feboardIdx].ControlFpgaParams.ControlRegister2 & 0xFF; 
			sub_address = ad_fe_board_ctrl_control_register2;
		
			if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_BusWriteWords (crateInfoParams,CTRL_ACCESS, FEB_CTRL_FPGA,feboardIdx ,dummy, sub_address, &data,1);   
		}
	}

	if((toBeLoaded == FEB_CTRL_CLOCK_REG) || (toBeLoaded == FEB_CTRL_ALL_REGS)|| (toBeLoaded == ALL_REGS))
	{
		for(feboardIdx = startFeIndex; feboardIdx <= endFeIndex; feboardIdx++)
		{
			
			if(crateInfoParams->CrateBoardsInfo.FeBoardInfo[feboardIdx].Si5332IsPresent == TRUE)
			{
				Load_FeBoardSi5332DefaultConfig(crateInfoParams,feboardIdx); 	
			}
			else
			{
				
				data = crateParams->FeBoardParams[feboardIdx].ControlFpgaParams.ClockRegister& 0xFFFF; 
				sub_address = ad_fe_board_ctrl_clock_register;
				if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_BusWriteWords (crateInfoParams,CTRL_ACCESS, FEB_CTRL_FPGA,feboardIdx ,dummy, sub_address, &data,2);  
			}
		}
		
	}
	
	if((toBeLoaded == FEB_CTRL_TRIGGER_REG) || (toBeLoaded == FEB_CTRL_ALL_REGS)|| (toBeLoaded == ALL_REGS))
	{
		for(feboardIdx = startFeIndex; feboardIdx <= endFeIndex; feboardIdx++)
		{
			data = crateParams->FeBoardParams[feboardIdx].ControlFpgaParams.TriggerRegister & 0xFF; 
			sub_address = ad_fe_board_ctrl_trigger_register;
			if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_BusWriteWords (crateInfoParams,CTRL_ACCESS, FEB_CTRL_FPGA,feboardIdx ,dummy, sub_address, &data,1); 
		}
		
	}
   /*if((toBeLoaded == FEB_CTRL_FE_BOARD_PRESENCE) || (toBeLoaded == FEB_CTRL_ALL_REGS)|| (toBeLoaded == ALL_REGS))
	{
	
		for(feboardIdx = startFeIndex; feboardIdx <= endFeIndex; feboardIdx++)
		{
			data = crateParams->FeBoardParams[feboardIdx].ControlFpgaParams.FeBoardsPresence & 0xFF; 
			sub_address = ad_fe_board_ctrl_FeBoardPresence;
			if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_BusWriteWords (crateInfoParams,CTRL_ACCESS, FEB_CTRL_FPGA,feboardIdx ,dummy, sub_address, &data,1); 
		}
	} */ 
	
	if((toBeLoaded == FEB_CTRL_READ_REQ_DELAY) || (toBeLoaded == FEB_CTRL_ALL_REGS)|| (toBeLoaded == ALL_REGS))
	{
		for(feboardIdx = startFeIndex; feboardIdx <= endFeIndex; feboardIdx++)
		{
			data = crateParams->FeBoardParams[feboardIdx].ControlFpgaParams.ReaqReqDelay; 
			sub_address = ad_fe_board_ctrl_read_req_delay;
			if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_BusWriteWords (crateInfoParams,CTRL_ACCESS, FEB_CTRL_FPGA,feboardIdx ,dummy, sub_address, &data,4); 
	
		}
	}
	 
	if((toBeLoaded == FEB_CTRL_EXT_TRIG_GATE_FOR_L2) || (toBeLoaded == FEB_CTRL_ALL_REGS)|| (toBeLoaded == ALL_REGS))
	{
		for(feboardIdx = startFeIndex; feboardIdx <= endFeIndex; feboardIdx++)
		{
			data = crateParams->FeBoardParams[feboardIdx].ControlFpgaParams.ExternalTrigGateForL2; 
			sub_address = ad_fe_board_ctrl_ext_trigger_gate_for_l2;
			if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_BusWriteWords (crateInfoParams,CTRL_ACCESS, FEB_CTRL_FPGA,feboardIdx ,dummy, sub_address, &data,1);  
		}
	}
	
	if((toBeLoaded == FEB_CTRL_TRIGGER_COMBI_LOGIC_FOR_L2) || (toBeLoaded == FEB_CTRL_ALL_REGS)|| (toBeLoaded == ALL_REGS))
	{
		for(feboardIdx = startFeIndex; feboardIdx <= endFeIndex; feboardIdx++)
		{
			data = crateParams->FeBoardParams[feboardIdx].ControlFpgaParams.TriggerCombiLogicForL2; 
			sub_address = ad_fe_board_ctrl_trigger_combi_for_l2;
			
			if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_BusWriteWords (crateInfoParams,CTRL_ACCESS, FEB_CTRL_FPGA,feboardIdx ,dummy, sub_address, &data,4);  
		}
	}
	 
 	
	/*========== END FRONT END BOARD CONTROL FPGA REGISTERS ========*/ 

	/*========== START FRONT END BOARD FRONT END FPGA REGISTERS ========*/ 
	
	
	// COMMON PARAMS
	if((toBeLoaded == FEB_FE_CONVERT_LENGTH_REG) || (toBeLoaded == FEB_FE_ALL_REGS)|| (toBeLoaded == ALL_REGS))
	{
		data = crateParams->CommonParams.ConvertLength & 0xFF; 
		sub_address = ad_fe_board_fe_convert_length;
		// in broadcast
		if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_BusWriteWords (crateInfoParams,CTRL_ACCESS, FEB_FE_FPGA,ALL_FE_BOARDs, ALL_FE_BOARDs_FE_FPGAs, sub_address, &data,1); 
			
	
	}
	
	if((toBeLoaded == FEB_FE_NB_OF_FRAMES_PER_BLOCK) || (toBeLoaded == FEB_FE_ALL_REGS)|| (toBeLoaded == ALL_REGS))
	{
		data = crateParams->CommonParams.NbOfFramesPerBlock & 0xFF; 
		sub_address = ad_fe_board_fe_nb_of_frames_per_block;
		// in broadcast
		if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_BusWriteWords (crateInfoParams,CTRL_ACCESS, FEB_FE_FPGA,ALL_FE_BOARDs, ALL_FE_BOARDs_FE_FPGAs, sub_address, &data,1); 

	}
		


	
	if((toBeLoaded == FEB_FE_NB_OF_TRIGGERS_PER_EVT) || (toBeLoaded == FEB_FE_ALL_REGS)|| (toBeLoaded == ALL_REGS))
	{
		data = crateParams->CommonParams.NbOfTriggersPerEvent & 0xFF; 
	 	sub_address = ad_fe_board_fe_nb_of_triggers_per_block;
		// in broadcast
	    if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_BusWriteWords (crateInfoParams,CTRL_ACCESS, FEB_FE_FPGA,ALL_FE_BOARDs, ALL_FE_BOARDs_FE_FPGAs, sub_address, &data,1); 

		
	}
	
	if((toBeLoaded == FEB_FE_CONVERT_DELAY_REG) || (toBeLoaded == FEB_FE_ALL_REGS)|| (toBeLoaded == ALL_REGS))
	{
	  
		data = crateParams->CommonParams.ConvertDelay & 0xFFFF; 
	 	sub_address = ad_fe_board_fe_convert_delay;
		// in broadcast 
	    if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_BusWriteWords (crateInfoParams,CTRL_ACCESS, FEB_FE_FPGA,ALL_FE_BOARDs, ALL_FE_BOARDs_FE_FPGAs, sub_address, &data,2); 

	}
	
	if((toBeLoaded == FEB_FE_SAMPIC_READ_LENGTH) || (toBeLoaded == FEB_FE_ALL_REGS)|| (toBeLoaded == ALL_REGS))
	{
		data = (crateParams->CommonParams.NbOfSamplesToRead + crateParams->CommonParams.NbOfExtraWords) & 0xFF; 
	 	sub_address = ad_fe_board_fe_sampic_read_length;
		// in broadcast 
	    if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_BusWriteWords (crateInfoParams,CTRL_ACCESS, FEB_FE_FPGA,ALL_FE_BOARDs, ALL_FE_BOARDs_FE_FPGAs, sub_address, &data,1); 
	}


	// INDIVIDUAL FE_BOARD, FE_FPGA REGISTERS

	if((toBeLoaded == FEB_FE_EXT_TRIG_GATE_LENGTH) || (toBeLoaded == FEB_FE_ALL_REGS)|| (toBeLoaded == ALL_REGS))
	{
	  for(feboardIdx = startFeIndex; feboardIdx <= endFeIndex; feboardIdx++)
	  {	
		  for(feFPGAIdx = startFeFPGAIndex; feFPGAIdx <= endFeFPGAIndex; feFPGAIdx++)
		  {
	
			data = crateParams->FeBoardParams[feboardIdx].FeFpgaParams[feFPGAIdx].ExternalTrigGate & 0xFF; 
	 		sub_address = ad_fe_board_fe_ext_trig_gate_length;
		
			if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_BusWriteWords (crateInfoParams,CTRL_ACCESS, FEB_FE_FPGA,feboardIdx, feFPGAIdx, sub_address, &data,1); 
	   	 }
	  }

	}
	
	if((toBeLoaded == FEB_FE_CONTROL_REG) || (toBeLoaded == FEB_FE_ALL_REGS)|| (toBeLoaded == ALL_REGS))
	{
	  for(feboardIdx = startFeIndex; feboardIdx <= endFeIndex; feboardIdx++)
	  {	
		  for(feFPGAIdx = startFeFPGAIndex; feFPGAIdx <= endFeFPGAIndex; feFPGAIdx++)
		  {
			data = crateParams->FeBoardParams[feboardIdx].FeFpgaParams[feFPGAIdx].ControlRegister & 0xFFFF; 
		 	sub_address = ad_fe_board_fe_control_reg;
			if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_BusWriteWords (crateInfoParams,CTRL_ACCESS, FEB_FE_FPGA,feboardIdx ,feFPGAIdx, sub_address, &data,2);  
		  }
	  }
	
	}
	
	if((toBeLoaded == FEB_FE_PULSER_REG) || (toBeLoaded == FEB_FE_ALL_REGS)|| (toBeLoaded == ALL_REGS))
	{
	  for(feboardIdx = startFeIndex; feboardIdx <= endFeIndex; feboardIdx++)
	  {	
		  for(feFPGAIdx = startFeFPGAIndex; feFPGAIdx <= endFeFPGAIndex; feFPGAIdx++)
		  {		
			data = crateParams->FeBoardParams[feboardIdx].FeFpgaParams[feFPGAIdx].PulserRegister; 
		 	sub_address = ad_fe_board_fe_pulser_reg;
			if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_BusWriteWords (crateInfoParams,CTRL_ACCESS, FEB_FE_FPGA,feboardIdx ,feFPGAIdx, sub_address, &data,3);   
	
		  }
	  }
	
	}
	
	if((toBeLoaded == FEB_FE_PULSER_WIDTH) || (toBeLoaded == FEB_FE_ALL_REGS)|| (toBeLoaded == ALL_REGS))
	{
	  for(feboardIdx = startFeIndex; feboardIdx <= endFeIndex; feboardIdx++)
	  {	
		  for(feFPGAIdx = startFeFPGAIndex; feFPGAIdx <= endFeFPGAIndex; feFPGAIdx++)
		  {		
			data = (unsigned short)crateParams->FeBoardParams[feboardIdx].FeFpgaParams[feFPGAIdx].PulserWidth; 
		 	sub_address = ad_fe_board_fe_pulser_width_reg;
			if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_BusWriteWords (crateInfoParams,CTRL_ACCESS, FEB_FE_FPGA,feboardIdx ,feFPGAIdx, sub_address, &data,2);   
	
			}
		}
	
	}
		
		
	if((toBeLoaded == FEB_FE_L2_COINCIDENCE_REG) || (toBeLoaded == FEB_FE_ALL_REGS)|| (toBeLoaded == ALL_REGS))
	{
	  for(feboardIdx = startFeIndex; feboardIdx <= endFeIndex; feboardIdx++)
	  {	
		  for(feFPGAIdx = startFeFPGAIndex; feFPGAIdx <= endFeFPGAIndex; feFPGAIdx++)
		  {		
			 data = (unsigned int)crateParams->FeBoardParams[feboardIdx].FeFpgaParams[feFPGAIdx].L2CoincidenceReg; 
			 sub_address = ad_fe_board_fe_L2_coincidence_reg;
			 if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_BusWriteWords (crateInfoParams,CTRL_ACCESS, FEB_FE_FPGA,feboardIdx ,feFPGAIdx, sub_address, &data,2);   

		  }
	  }
	
	}


	/*========== END FRONT-END BOARD FRONT END FPGA REGISTERS ========*/ 

	/*========== START FRONT-END BOARD SAMPIC INTERNAL REGISTERS ========*/ 

    if((toBeLoaded == FEB_SAMPIC_SC_CHANNEL_REG) || (toBeLoaded == FEB_SAMPIC_SC_ALL_REGS)|| (toBeLoaded == ALL_REGS))
	{	
	   for(feboardIdx = startFeIndex; feboardIdx <= endFeIndex; feboardIdx++)
	   {	
		  for(feFPGAIdx = startFeFPGAIndex; feFPGAIdx <= endFeFPGAIndex; feFPGAIdx++)
		  {
			  for (channelIdx= startChannelIndex; channelIdx <= endChannelIndex; channelIdx++)
			  {
				  
				 if(errCode == SAMPIC256CH_Success) errCode = Load_SampicSCReg(crateInfoParams, feboardIdx, feFPGAIdx, channelIdx + SampicChannelRegOffsetAdd, crateParams->FeBoardParams[feboardIdx].FeFpgaParams[feFPGAIdx].SAMPICIndividualParams.ChannelRegister[channelIdx]); 	  
			  }
		  }
	   }
	}
   
    if((toBeLoaded == FEB_SAMPIC_SC_REG1) || (toBeLoaded == FEB_SAMPIC_SC_ALL_REGS)|| (toBeLoaded == ALL_REGS))
	{	
	   for(feboardIdx = startFeIndex; feboardIdx <= endFeIndex; feboardIdx++)
	   {	
		  for(feFPGAIdx = startFeFPGAIndex; feFPGAIdx <= endFeFPGAIndex; feFPGAIdx++)
		  {
				  
			 if(errCode == SAMPIC256CH_Success) errCode = Load_SampicSCReg(crateInfoParams, feboardIdx, feFPGAIdx, SampicConfigReg1Add, crateParams->FeBoardParams[feboardIdx].FeFpgaParams[feFPGAIdx].SAMPICIndividualParams.ConfigReg1); 	  
			  
		  }
	   }	
	 	

	}
	
	if((toBeLoaded == FEB_SAMPIC_SC_REG2) || (toBeLoaded == FEB_SAMPIC_SC_ALL_REGS)|| (toBeLoaded == ALL_REGS))
	{		
	 
	   for(feboardIdx = startFeIndex; feboardIdx <= endFeIndex; feboardIdx++)
	   {	
		  for(feFPGAIdx = startFeFPGAIndex; feFPGAIdx <= endFeFPGAIndex; feFPGAIdx++)
		  {
				  
			 if(errCode == SAMPIC256CH_Success) errCode = Load_SampicSCReg(crateInfoParams, feboardIdx, feFPGAIdx, SampicConfigReg2Add, crateParams->FeBoardParams[feboardIdx].FeFpgaParams[feFPGAIdx].SAMPICIndividualParams.ConfigReg2); 	  
			  
		  }
	   }	
	
	}
		
	if((toBeLoaded == FEB_SAMPIC_SC_REG3) || (toBeLoaded == FEB_SAMPIC_SC_ALL_REGS)|| (toBeLoaded == ALL_REGS))
	{		
	 
	   for(feboardIdx = startFeIndex; feboardIdx <= endFeIndex; feboardIdx++)
	   {	
		  for(feFPGAIdx = startFeFPGAIndex; feFPGAIdx <= endFeFPGAIndex; feFPGAIdx++)
		  {
				  
			 if(errCode == SAMPIC256CH_Success) errCode = Load_SampicSCReg(crateInfoParams, feboardIdx, feFPGAIdx, SampicConfigReg3Add, crateParams->FeBoardParams[feboardIdx].FeFpgaParams[feFPGAIdx].SAMPICIndividualParams.ConfigReg3); 	  
			  
		  }
	   }	
	
	}
 
	
	if((crateInfoParams->SampicVersion >= 2) && ((toBeLoaded == FEB_SAMPIC_SC_REG4) || (toBeLoaded == FEB_SAMPIC_SC_ALL_REGS)|| (toBeLoaded == ALL_REGS)))
	{		
	   
	   for(feboardIdx = startFeIndex; feboardIdx <= endFeIndex; feboardIdx++)
	   {	
		  for(feFPGAIdx = startFeFPGAIndex; feFPGAIdx <= endFeFPGAIndex; feFPGAIdx++)
		  {
				  
			 if(errCode == SAMPIC256CH_Success) errCode = Load_SampicSCReg(crateInfoParams, feboardIdx, feFPGAIdx, SampicConfigReg4Add, crateParams->FeBoardParams[feboardIdx].FeFpgaParams[feFPGAIdx].SAMPICIndividualParams.ConfigReg4); 	  
			  
		  }
	   }	
	
	}
	
	if((crateInfoParams->SampicVersion >= 2) && ((toBeLoaded == FEB_SAMPIC_SC_REG5) || (toBeLoaded == FEB_SAMPIC_SC_ALL_REGS)|| (toBeLoaded == ALL_REGS)))
	{		
	   for(feboardIdx = startFeIndex; feboardIdx <= endFeIndex; feboardIdx++)
	   {	
		  for(feFPGAIdx = startFeFPGAIndex; feFPGAIdx <= endFeFPGAIndex; feFPGAIdx++)
		  {
				  
			 if(errCode == SAMPIC256CH_Success) errCode = Load_SampicSCReg(crateInfoParams, feboardIdx, feFPGAIdx, SampicConfigReg5Add, crateParams->FeBoardParams[feboardIdx].FeFpgaParams[feFPGAIdx].SAMPICIndividualParams.ConfigReg5); 	  
			  
		  }
	   }	
		
	}
	
	if((crateInfoParams->SampicVersion >= 2) && ((toBeLoaded == FEB_SAMPIC_SC_REG6) || (toBeLoaded == FEB_SAMPIC_SC_ALL_REGS)|| (toBeLoaded == ALL_REGS)))
	{		
	   for(feboardIdx = startFeIndex; feboardIdx <= endFeIndex; feboardIdx++)
	   {	
		  for(feFPGAIdx = startFeFPGAIndex; feFPGAIdx <= endFeFPGAIndex; feFPGAIdx++)
		  {
				  
			 if(errCode == SAMPIC256CH_Success) errCode = Load_SampicSCReg(crateInfoParams, feboardIdx, feFPGAIdx, SampicConfigReg6Add, crateParams->FeBoardParams[feboardIdx].FeFpgaParams[feFPGAIdx].SAMPICIndividualParams.ConfigReg6); 	  
			  
		  }
	   }	
		
	}
	
 	if((crateInfoParams->SampicVersion >= 2) && ((toBeLoaded == FEB_SAMPIC_SC_REG7) || (toBeLoaded == FEB_SAMPIC_SC_ALL_REGS)|| (toBeLoaded == ALL_REGS)))
	{		
	   for(feboardIdx = startFeIndex; feboardIdx <= endFeIndex; feboardIdx++)
	   {	
		  for(feFPGAIdx = startFeFPGAIndex; feFPGAIdx <= endFeFPGAIndex; feFPGAIdx++)
		  {
				  
			 if(errCode == SAMPIC256CH_Success) errCode = Load_SampicSCReg(crateInfoParams, feboardIdx, feFPGAIdx, SampicConfigReg7Add, crateParams->FeBoardParams[feboardIdx].FeFpgaParams[feFPGAIdx].SAMPICIndividualParams.ConfigReg7); 	  
			  
		  }
	   }	
		 
	}
	

	if((crateInfoParams->SampicVersion >= 2) && ((toBeLoaded == FEB_SAMPIC_SC_REG8) || (toBeLoaded == FEB_SAMPIC_SC_ALL_REGS)|| (toBeLoaded == ALL_REGS)))
	{		
	   for(feboardIdx = startFeIndex; feboardIdx <= endFeIndex; feboardIdx++)
	   {	
		  for(feFPGAIdx = startFeFPGAIndex; feFPGAIdx <= endFeFPGAIndex; feFPGAIdx++)
		  {
				  
			 if(errCode == SAMPIC256CH_Success) errCode = Load_SampicSCReg(crateInfoParams, feboardIdx, feFPGAIdx, SampicConfigReg8Add, crateParams->FeBoardParams[feboardIdx].FeFpgaParams[feFPGAIdx].SAMPICIndividualParams.ConfigReg8); 	  
			  
		  }
	   }
			
	}	
	
	
	if((crateInfoParams->SampicVersion >= 2) && ((toBeLoaded == FEB_SAMPIC_SC_REG9) || (toBeLoaded == FEB_SAMPIC_SC_ALL_REGS)|| (toBeLoaded == ALL_REGS)))
	{		

	   for(feboardIdx = startFeIndex; feboardIdx <= endFeIndex; feboardIdx++)
	   {	
		  for(feFPGAIdx = startFeFPGAIndex; feFPGAIdx <= endFeFPGAIndex; feFPGAIdx++)
		  {
				  
			 if(errCode == SAMPIC256CH_Success) errCode = Load_SampicSCReg(crateInfoParams, feboardIdx, feFPGAIdx, SampicConfigReg9Add, crateParams->FeBoardParams[feboardIdx].FeFpgaParams[feFPGAIdx].SAMPICIndividualParams.ConfigReg9); 	  
			  
		  }
	   }
		
	}	
		
	if((crateInfoParams->SampicVersion >= 2) && ((toBeLoaded == FEB_SAMPIC_SC_REG10) || (toBeLoaded == FEB_SAMPIC_SC_ALL_REGS)|| (toBeLoaded == ALL_REGS)))
	{		
	   for(feboardIdx = startFeIndex; feboardIdx <= endFeIndex; feboardIdx++)
	   {	
		  for(feFPGAIdx = startFeFPGAIndex; feFPGAIdx <= endFeFPGAIndex; feFPGAIdx++)
		  {
				  
			 if(errCode == SAMPIC256CH_Success) errCode = Load_SampicSCReg(crateInfoParams, feboardIdx, feFPGAIdx, SampicConfigReg10Add, crateParams->FeBoardParams[feboardIdx].FeFpgaParams[feFPGAIdx].SAMPICIndividualParams.ConfigReg10); 	  
			  
		  }
	   }
			
	}	
  
	 /*========== END FRONT-END BOARD SAMPIC INTERNAL REGISTERS ========*/  
	
	return errCode;
	
}
	
/* =========================================================================== */							
SAMPIC256CH_ErrCode Load_DACs(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, DacType_t dacToLoad,  int feBoardIndex, int feFPGAIndex) 
/* =========================================================================== */	
// Simule le bus SPI vers le DAC
{
	
SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success;

int dac, dacValue = 0, dataToWrite, n, spiWord, dummy = 0;
Boolean n_cs_dac, dataBit;
unsigned char spiData[1 + 2*NB_OF_EXT_DAC_BITS + 1], sub_address;	
int i, j;
int feboardIdx, feFPGAIdx, channelIdx;
int startFeIndex, endFeIndex;
int startFeFPGAIndex, endFeFPGAIndex;
int startChannelIndex, endChannelIndex;
float dacFloatValue;

	
    if( crateInfoParams->ConnectionInfo.CtrlDeviceHandle < 0)
			return SAMPIC256CH_NoCrateConnected;
	
    if(crateInfoParams->NbOfFeBoards == 0)
		return SAMPIC256CH_NoFeBoardInCrate;

   	if(feBoardIndex < 0)
	{
		
		startFeIndex = 0;
		endFeIndex = crateInfoParams->NbOfFeBoards -1;
	}
	else
	{
		
		startFeIndex = feBoardIndex;
		endFeIndex = feBoardIndex;
	}
  	
	
	if(feFPGAIndex < 0)
	{
		startFeFPGAIndex = 0;	
		endFeFPGAIndex =  NB_OF_SAMPICS_IN_FE_BOARD -1;
	}
	else
	{
		
		startFeFPGAIndex = feFPGAIndex;	
		endFeFPGAIndex = feFPGAIndex;
	
	}
	
	
	// CONTROL BOARD DACs
	
	if((dacToLoad == CB_EXT_TRIG_THRESHOLD) || (dacToLoad == CB_EXT_SYNC_THRESHOLD) || (dacToLoad == ALL_VDACS))   
	{
		
		for(dac = CB_EXT_TRIG_THRESHOLD; dac <= CB_EXT_SYNC_THRESHOLD; dac ++)
		{
			if((dac == dacToLoad) || (dacToLoad == ALL_VDACS))
			{
		
				spiWord = 0;
			 	n_cs_dac = 0;
				sub_address = ad_control_board_dac_reg;

				dataBit = 0;
				spiData[spiWord++] = CLOCK_LOW + (dataBit << 1) + n_cs_dac;	

				if(dac == CB_EXT_TRIG_THRESHOLD)
				{
						if(crateParams->ControlBoardParams.ExternalTriggerSigLevel == TTL_SIG)
							dacFloatValue = 0.9;
						else // NIM
							dacFloatValue = 2.5;
	
					dacValue = Calculate_16BitsDACIntValues(dacFloatValue, EXT_SIG_DAC_TYPE);
				}
				else if(dac == CB_EXT_SYNC_THRESHOLD) 
				{
					if(crateParams->ControlBoardParams.ExternalSyncSigLevel == TTL_SIG)
						dacFloatValue = 0.9;
					else // NIM
						dacFloatValue = 2.5;
	
					dacValue = Calculate_16BitsDACIntValues(dacFloatValue, EXT_SIG_DAC_TYPE);
				}


				dataToWrite = (0x3 << 20) + ((dac - CB_EXT_TRIG_THRESHOLD) << 16) + dacValue;

				for(n = NB_OF_EXT_DAC_BITS - 1; n>=0; n--) // MSB first
				{
					dataBit = (dataToWrite >> n) & 0x1;
					spiData[spiWord++] = CLOCK_LOW + (dataBit << 1) + n_cs_dac;	
					spiData[spiWord++] = CLOCK_HIGH + (dataBit << 1) + n_cs_dac;	
				}
			 	n_cs_dac = 1;
				dataBit = 0;
				spiData[spiWord] = CLOCK_LOW + (dataBit << 1) + n_cs_dac;
				if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_BusWriteWords (crateInfoParams,CTRL_ACCESS, CB_CTRL_FPGA,dummy ,dummy, sub_address, spiData, 1 + 2*NB_OF_EXT_DAC_BITS + 1/*nbOfBytes*/);  
			}
		}
	}
	
	
	for(feboardIdx = startFeIndex; feboardIdx <= endFeIndex; feboardIdx++)
	{	
		for(feFPGAIdx = startFeFPGAIndex; feFPGAIdx <= endFeFPGAIndex; feFPGAIdx++)
		{	
			if((dacToLoad != CB_EXT_TRIG_THRESHOLD) && (dacToLoad != CB_EXT_SYNC_THRESHOLD))
			{
			
				for(dac = FEB_VDAC_RESET; dac <= FEB_VDAC_DLL; dac ++)
				{
					if((dac == dacToLoad) || (dacToLoad == ALL_VDACS))
					{
						spiWord = 0;
					 	n_cs_dac = 0;
						sub_address = ad_fe_board_fe_dac_a_access;
						dataBit = 0;
						spiData[spiWord++] = CLOCK_LOW + (dataBit << 1) + n_cs_dac;	

						if(dac == FEB_VDAC_RESET) dacValue = Calculate_16BitsDACIntValues(crateParams->FeBoardParams[feboardIdx].FeFpgaParams[feFPGAIdx].SAMPICIndividualParams.Vdac_reset, SAMPIC_DAC_TYPE);
						//currently VDAC start RAMP concerns the ADC ramp shifted by - 100 mV
						else if(dac == FEB_VDAC_START_RAMP) dacValue = Calculate_16BitsDACIntValues(crateParams->FeBoardParams[feboardIdx].FeFpgaParams[feFPGAIdx].SAMPICIndividualParams.Vdac_StartRamp, SAMPIC_DAC_TYPE);
						else if(dac == FEB_VDAC_EXT_THRESHOLD) dacValue = Calculate_16BitsDACIntValues(crateParams->FeBoardParams[feboardIdx].FeFpgaParams[feFPGAIdx].SAMPICIndividualParams.ExternalThreshold, SAMPIC_DAC_TYPE);
						else if(dac == FEB_VDAC_DLL) dacValue = Calculate_16BitsDACIntValues(crateParams->FeBoardParams[feboardIdx].FeFpgaParams[feFPGAIdx].SAMPICIndividualParams.Vdac_DLL, SAMPIC_DAC_TYPE);

						dataToWrite = (0x3 << 20) + (dac << 16) + dacValue;

						for(n = NB_OF_EXT_DAC_BITS - 1; n>=0; n--) // MSB first
						{
							dataBit = (dataToWrite >> n) & 0x1;
							spiData[spiWord++] = CLOCK_LOW + (dataBit << 1) + n_cs_dac;	
							spiData[spiWord++] = CLOCK_HIGH + (dataBit << 1) + n_cs_dac;	
						}
					 	n_cs_dac = 1;
						dataBit = 0;
						spiData[spiWord] = CLOCK_LOW + (dataBit << 1) + n_cs_dac;
					    if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_BusWriteWords (crateInfoParams,CTRL_ACCESS, FEB_FE_FPGA,feboardIdx ,feFPGAIdx, sub_address, spiData, 1 + 2*NB_OF_EXT_DAC_BITS + 1/*nbOfBytes*/);  

					}
				}
		
			}

		}
	}
		
	return errCode;
}


/* =================================================================================== */
/* =================================================================================== */


///* ======================================================================= */
///* =================    Commandes envoyées à la carte ==================== */
///* ======================================================================= */

/* =========================================================================== */							
SAMPIC256CH_ErrCode Run_Reset (CrateInfoStruct *crateInfoParams, ResetType_t resetType)
/* =========================================================================== */							
{

 unsigned char sub_address;
 unsigned char data;
 int dummy = 0;
 SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success;    
   
/* 	CB_CTRL_ALL_FPGA,
	FEB_CTRL_ALL_FPGA,
	FEB_FE_ALL_FPGA,
	
	ALL_FPGAS
*/
 if((resetType == FEB_FE_ALL_FPGA) || (resetType == ALL_FPGAS))
 {
   sub_address = ad_fe_board_fe_global_reset;
   data = 0x1;
   
  if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_BusWriteWords (crateInfoParams,CTRL_ACCESS, FEB_FE_FPGA,ALL_FE_BOARDs, ALL_FE_BOARDs_FE_FPGAs, sub_address, &data,1); 

	 
 }
 
 if((resetType == FEB_CTRL_ALL_FPGA) || (resetType == ALL_FPGAS))
 {
   sub_address = ad_fe_board_ctrl_global_reset;
   data = 0x1;
   
  if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_BusWriteWords (crateInfoParams,CTRL_ACCESS, FEB_CTRL_FPGA, ALL_FE_BOARDs, dummy, sub_address, &data,1); 

	 
 } 

 return errCode;
 
}


///* =========================================================================== */							
SAMPIC256CH_ErrCode Reset_SAMPICSlowControl (CrateInfoStruct *crateInfoParams)
///* =========================================================================== */							
{

  int feIndex;
  unsigned short data;   
  unsigned char readData[2];
  unsigned char sub_address;
  SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success;            
  
 
 data = 1; 
 sub_address = ad_fe_board_fe_sampic_slow_reg_add;  
 
 if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_BusWriteWords (crateInfoParams,CTRL_ACCESS, FEB_FE_FPGA,ALL_FE_BOARDs, ALL_FE_BOARDs_FE_FPGAs, sub_address, &data,1); 
				
 data =0xAA55; 
 sub_address = ad_fe_board_fe_sampic_slow_control_access;
 
 if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_BusWriteWords (crateInfoParams,CTRL_ACCESS, FEB_FE_FPGA,ALL_FE_BOARDs, ALL_FE_BOARDs_FE_FPGAs, sub_address, &data,1); 
	
  return errCode;
  
}


///* =========================================================================== */							
SAMPIC256CH_ErrCode Reset_ExtTrig (CrateInfoStruct *crateInfoParams)
///* =========================================================================== */							
{
	unsigned short data ;
	unsigned char sub_address; 
	SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success;            
	
		
	data = 0x1;
	sub_address = ad_fe_board_fe_sampic_rst_ext_trig;
	
	errCode = SAMPIC256CH_BusWriteWords (crateInfoParams,CTRL_ACCESS, FEB_FE_FPGA,ALL_FE_BOARDs, ALL_FE_BOARDs_FE_FPGAs, sub_address, &data,1); 
		
	
	return errCode; 
	
}

/* =========================================================================== */							
SAMPIC256CH_ErrCode Reset_SAMPICDLL(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams)
/* =========================================================================== */							
{
 
 int feBoardIndex, sampicIndex;
 int dummy = 0;
 
 SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success; 
	
  for(feBoardIndex = 0; feBoardIndex < crateInfoParams->NbOfFeBoards; feBoardIndex++)
  {
	  for(sampicIndex = 0; sampicIndex < NB_OF_SAMPICS_IN_FE_BOARD; sampicIndex++)
	  {
		  
	  	crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[sampicIndex].RstDLL = TRUE;
		
	  }
	  
  }
   
  Build_FeBoardsFeFpgasControlReg(crateParams);
  
  errCode = Load_HardwareSetup(crateInfoParams, crateParams, FEB_FE_CONTROL_REG, ALL_FE_BOARDs, ALL_FE_BOARDs_FE_FPGAs, dummy); 

  dsleep(500);
	
  for(feBoardIndex = 0; feBoardIndex < crateInfoParams->NbOfFeBoards; feBoardIndex++)
  {
	  for(sampicIndex = 0; sampicIndex < NB_OF_SAMPICS_IN_FE_BOARD; sampicIndex++)
	  {
		  
	  	crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[sampicIndex].RstDLL = FALSE;  
		
	  }
	  
  } 
  
  Build_FeBoardsFeFpgasControlReg(crateParams);
  
  if (errCode == SAMPIC256CH_Success) errCode = Load_HardwareSetup(crateInfoParams, crateParams, FEB_FE_CONTROL_REG, ALL_FE_BOARDs, ALL_FE_BOARDs_FE_FPGAs, dummy); 
  
  return errCode;
	
}



/* =========================================================================== */							
SAMPIC256CH_ErrCode Resync_System (CrateInfoStruct *crateInfoParams)
/* =========================================================================== */							
{
    unsigned short data ;
	unsigned char sub_address; 
	int dummy = 0;

	SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success;
	
	
	data = 0x1;
	sub_address = ad_control_board_resync;
	
	errCode = SAMPIC256CH_BusWriteWords (crateInfoParams,CTRL_ACCESS, CB_CTRL_FPGA, dummy, dummy, sub_address, &data,1); 
	
	return errCode;
	
}

///* =========================================================================== */							
SAMPIC256CH_ErrCode Send_SoftTrig (CrateInfoStruct *crateInfoParams )
///* =========================================================================== */							
{
	unsigned short data ;
	unsigned char sub_address; 
	int dummy = 0;
	SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success;
	
	
		
	data = 0x1;
	sub_address = ad_control_board_send_soft_trig;
    
	errCode = SAMPIC256CH_BusWriteWords (crateInfoParams,CTRL_ACCESS, CB_CTRL_FPGA, dummy, dummy, sub_address, &data,3);  

	
    return errCode;
	
}


///* =========================================================================== */							
SAMPIC256CH_ErrCode Send_PulseCmd( CrateInfoStruct *crateInfoParams)
///* =========================================================================== */							
{
	unsigned data; 

	unsigned char sub_address;
	int dummy = 0; 
	
	SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success;
	
	data = 0x1;
	sub_address = ad_control_board_send_pulse;
  	
	errCode =  SAMPIC256CH_BusWriteWords (crateInfoParams,CTRL_ACCESS, CB_CTRL_FPGA, dummy, dummy, sub_address, &data,1);
	
	
	return errCode;     
	
}





/* ======================================================================= */
/* =================   Fonctions d'Acquisition			================== */
/* ======================================================================= */


	

///* ================================================================================== */
//void Decode_TriggerData(void)
///* ================================================================================== */
//{
//int n;
//float period;
//int feIndex;
//
//
//period = (MAX_NB_OF_SAMPLES* 1000.0)/(float)SystemParams.FreqEch;  // en ns
//
//	
//	  TriggerData.NbOfTriggers = TriggerData.RawDataSize/8;
//	  feIndex = TriggerData.FeIndex;
//	  
//	  for (n = 0; n< TriggerData.NbOfTriggers; n++)
//	  {
//		  if(FeFpgaParams[feIndex].EnableDetectTriggerIDfromExtTrig == FALSE)
//		  {
//			  
//		  		TriggerData.TriggerID[n] = (int)TriggerData.RawData[n*8] +
//			   						 (((int)TriggerData.RawData[n*8+1])<<8) +
//									 (((int)TriggerData.RawData[n*8+2])<<16);
//		  }
//		  else
//		  {
//			    TriggerData.TriggerID[n] = (int)TriggerData.RawData[n*8];
//				
//				TriggerData.TriggerIDfromExtTrig[n]=  ((int)TriggerData.RawData[n*8+1] + (((int)TriggerData.RawData[n*8+2])<<8));
//		
//		  }
//
//		   TriggerData.TriggerTimeStamp[n] =  (unsigned long long)TriggerData.RawData[n*8 + 3] +
//			   								(((unsigned long long)TriggerData.RawData[n*8 + 4])<<8) +
//											(((unsigned long long)TriggerData.RawData[n*8 + 5])<<16) + 
//											(((unsigned long long)TriggerData.RawData[n*8 + 6])<<24) + 
//											(((unsigned long long)TriggerData.RawData[n*8 + 7])<<32);
//		   TriggerData.TriggerTimeStamp[n] = 	  (unsigned long long)((double)TriggerData.TriggerTimeStamp[n]*period);
//		   
//	  }
//}


///* ================================================================================== */
//
///* ======================================================================= */
///* =================    Calibrations  ==================================== */
///* ======================================================================= */
//
///* ======================================================================= */
//void  Reset_PedestalValues(void)
///* ======================================================================= */
//{
//  int  channel, sample;
//  
//  
// 
//	for(channel = 0; channel < NB_OF_CHANNELS_IN_FE_BOARD; channel++)
//	{
//		for(sample = 0; sample < MAX_NB_OF_SAMPLES; sample++)	
//		 CalibValues.PedestalValues[channel][sample] = 0.0;
//		
//	}
//	
//	  
//	
//}
//
///* ======================================================================= */
//void  Reset_InternalTriggerThresholdOffsetValues(void)
///* ======================================================================= */
//{
//	int channel;
//	
//	for(channel = 0; channel < NB_OF_CHANNELS_IN_FE_BOARD; channel++)
//	{
//
//		CalibValues.InternalTriggerThresholdOffset[channel]= 0.0;
//		CalibValues.InternalTriggerThresholdSlope[channel]= 0.0;
//	}
//		
//}
//
//
//
// /* ======================================================================= */
//void  Reset_DACOffsetsValues(void)
///* ======================================================================= */
//{
// int feIndex;
// 
//	for(feIndex = 0; feIndex < NB_OF_SAMPICS_IN_FE_BOARD; feIndex++)
//	{
//		
//		CalibValues.TOTDacOffset[feIndex] = 0.0;
//		CalibValues.TOTFilterDacOffset[feIndex] = 0.0;
//		
//	}
//		
//}
//
//
// /* ======================================================================= */
//void  Reset_ADCRampCalibValues(void)
///* ======================================================================= */
//{
// int feIndex, i;
// 
// 
// for (feIndex = 0; feIndex < Nb_Of_Fe_FPGAs_In_System; feIndex++)
// {
//	if(FeFpgaParams[feIndex].BoardVersion == 1)
//	{
//		CalibValues.VDAC_Ramp[feIndex][0] = DEFAULT_VDAC_RAMP_SAMPICV0_11BITS;
//		CalibValues.VDAC_Ramp[feIndex][1] = DEFAULT_VDAC_RAMP_SAMPICV0_10BITS; 
//		for(i= 2; i < MAX_NB_OF_ADC_MODES; i++)
//		  CalibValues.VDAC_Ramp[feIndex][i] = DEFAULT_VDAC_RAMP_SAMPICV0_9BITS; 
//	}
//	else
//	{
//		if(SystemParams.SampicVersion <= 2)
//		{
//			CalibValues.VDAC_Ramp[feIndex][0] = DEFAULT_VDAC_RAMP_SAMPICV1_11BITS;
//			CalibValues.VDAC_Ramp[feIndex][1] = DEFAULT_VDAC_RAMP_SAMPICV1_10BITS; 
//			CalibValues.VDAC_Ramp[feIndex][2] = DEFAULT_VDAC_RAMP_SAMPICV1_9BITS;   
//			for(i= 3; i < MAX_NB_OF_ADC_MODES; i++)
//			  CalibValues.VDAC_Ramp[feIndex][i] = DEFAULT_VDAC_RAMP_SAMPICV1_8BITS; 
//		}
//		else
//		{
//		
//			if(SystemParams.SystemType == SAMPET_SYSTEM)
//				CalibValues.VDAC_Ramp[feIndex][0] = DEFAULT_VDAC_RAMP_SAMPETV3_11BITS;
//			else
//				CalibValues.VDAC_Ramp[feIndex][0] = DEFAULT_VDAC_RAMP_SAMPICV3_11BITS;//DEFAULT_VDAC_RAMP_SAMPICV3_11BITS;
//			
//			CalibValues.VDAC_Ramp[feIndex][1] = DEFAULT_VDAC_RAMP_SAMPICV3_10BITS; 
//			CalibValues.VDAC_Ramp[feIndex][2] = DEFAULT_VDAC_RAMP_SAMPICV3_9BITS;   
//			for(i= 3; i < MAX_NB_OF_ADC_MODES; i++)
//			  CalibValues.VDAC_Ramp[feIndex][i] = DEFAULT_VDAC_RAMP_SAMPICV3_8BITS; 
//	
//			
//			
//		}
//	}
// }
//}
//
////==============================================================================//
//void Reset_INL_Values(int channel)
////==============================================================================//
//{
//	
//int n,ch;
//
//	
// if( channel >= 0)
// {
//	 
//    for(n= 0; n < MAX_NB_OF_SAMPLES; n++)
//		CalibValues.INLValues[channel][n] = 0.0;
//		 
//	CalibValues.INL_RMS[channel]=0.0; 
//	 
// }
// else
// {
// 
//	for(  ch = 0; ch < Nb_Of_Channels_In_System; ch++)
//	{
//		  for(n= 0; n < MAX_NB_OF_SAMPLES; n++)
//			CalibValues.INLValues[ch][n] = 0.0;
//
// 	   CalibValues.INL_RMS[ch]=0.0; 
//	}
// }
//#ifndef SAMPIC_DLL_EXPORTS
//   Plot_INL_Histo();
//#endif
//   
//}
//
//
//
///* ======================================================================= */
//void Reset_TOTCalibValues (void)
///* ======================================================================= */
//{
//	int step, feIndex, channel;
//	
//	
//			
//	for(channel = 0; channel < NB_OF_CHANNELS_IN_FE_BOARD; channel++)
//	{
//	  for(step = 0; step < MAX_NB_OF_TOT_RAMP_CURRENT_VALUES; step++)
//	  {	  
//		
//	  	  CalibValues.TOTCalibSlope[step][channel] = 1; 
//	  	  CalibValues.TOTCalibIntercept[step][channel] = 0;
//	  }
//	}
//}
//
///* ======================================================================= */
///* =================   Calibration Corrections			 ================= */
///* ======================================================================= */
//
///* ======================================================================= */
//void Check_ADCCalibStatus(void)
///* ======================================================================= */
//{
//int channel;
//
//Boolean resultOK = TRUE;
//CalibStatus_t calibStatus;
//
//calibStatus =   SystemParams.ADCLinearityCalibStatus[0].CalibStatus; 
//
//for (channel = 0; channel < Nb_Of_Channels_In_System; channel++)
//{
//	
// 	if(SystemParams.ADCLinearityCalibStatus[channel].CalibStatus != calibStatus)
//	{
//		resultOK = FALSE;
//		break;
//	}
//	
//}
//	
//if(resultOK == TRUE && calibStatus != CALIB_VALUES_NOT_LOADED)
//	SystemParams.ADCLinearityGlobalCalibStatus.CalibStatus =  calibStatus;
//else
//	SystemParams.ADCLinearityGlobalCalibStatus.CalibStatus = CALIB_VALUES_NOT_LOADED;
//
//}
//

/* ======================================================================= */
void		Correct_AdcLinearity	(CrateInfoStruct *crateInfoParams, HitStruct *hit, Boolean smartRead) 
/* ======================================================================= */
{
	int feBoard, sampicIndex, n, channel, physicalIndex, orderedIndex;
	double delta, xroot1, xroot2;
 
 	int maxADCValue , xIndex;

	double  step , xValue , x1, x2, y1, y2;
   	
	float factor;
	float cellLinearity_a2;
	float cellLinearity_a1;
	float cellLinearity_a0;
	float floatRawDataSample;


	sampicIndex = hit->SampicIndex;
	channel = hit->Channel;
	feBoard = hit->FeBoardIndex;
	
	if(crateInfoParams->CrateCalibInfo.CalibStatus.ADCLinearityCrateCalibStatus.CalibStatus == CALIB_VALUES_NOT_LOADED)
	{
		hit->ADCCorrected = FALSE;
		return;
	}
	else
	  hit->ADCCorrected = TRUE;
	
	for(n=0;n<hit->DataSize;n++)  
	{ 
		physicalIndex = (n + hit->CellInfo)%MAX_NB_OF_SAMPLES;
		
		if(smartRead == FALSE)
			orderedIndex = (MAX_NB_OF_SAMPLES+ n- hit->FirstCellPhysicalIndex)%MAX_NB_OF_SAMPLES;
		else
			orderedIndex = n;
	
		if(hit->RawDataSamples[n] != 0 ) // si valeur reçue = 0, on garde zéro
		{
				
				cellLinearity_a2 = crateInfoParams->CrateCalibInfo.CalibValues[feBoard].CellLinearity_a2[channel][physicalIndex];
				cellLinearity_a1 = crateInfoParams->CrateCalibInfo.CalibValues[feBoard].CellLinearity_a1[channel][physicalIndex];
				cellLinearity_a0 = crateInfoParams->CrateCalibInfo.CalibValues[feBoard].CellLinearity_a0[channel][physicalIndex];
				floatRawDataSample = hit->RawDataSamples[n];
				
				if(cellLinearity_a2  != 0) //polyfit
				{
				    delta = pow(cellLinearity_a1,2)-4*cellLinearity_a2 *(cellLinearity_a0 -floatRawDataSample);

				//	if(delta >= 0 && crateInfoParams->CrateCalibInfo.CalibValues[feBoard].CellLinearity_a2[channel][physicalIndex]  > 0)
					if(delta >= 0)
					{	
						xroot1 = (-cellLinearity_a1 + sqrt(delta))/(2*cellLinearity_a2 ); 
						xroot2 = (-cellLinearity_a1 - sqrt(delta))/(2*cellLinearity_a2 );


						if(	(xroot1 >=  MIN_ADC_VOLT_VALUE) && (xroot1  <=  MAX_ADC_VOLT_VALUE))
						{
							 
							hit->CorrectedDataSamples[orderedIndex] = xroot1;   /* en volts*/
		
						}
						else if ( (xroot2 >=  MIN_ADC_VOLT_VALUE) && (xroot2  <=  MAX_ADC_VOLT_VALUE))
						{
							hit->CorrectedDataSamples[orderedIndex] = xroot2;   /* en volts*/
			

						}
		
			
					}
	
				}
				else //correction lineaire  si a2 = 0
				{
		
					hit->CorrectedDataSamples[orderedIndex] = 	(floatRawDataSample- cellLinearity_a0)/cellLinearity_a1; //en Volts	
	
				}	
		}
		else
		{
			hit->CorrectedDataSamples[orderedIndex] = 0;  	
			
		}
	}

}


/* ======================================================================= */
void		Correct_TimeLinearity	(CrateInfoStruct *crateInfoParams, HitStruct *hit)
/* ======================================================================= */
{
	
	int feBoard, sampicIndex, n, channel, physicalIndex, orderedIndex;

	int sample, i, prev_i, next_i, current_i, prev_n, current_n, next_n;
 	float prevCoef, currentCoef, nextCoef;
	float x,x1,x2,x3,y1,y2,y3;
	float samplingPeriod;
	
	float timeCorrectedDataSamples[MAX_NB_OF_SAMPLES];
	
	
	sampicIndex = hit->SampicIndex;
	channel = hit->Channel;
	feBoard = hit->FeBoardIndex;
	
	if((crateInfoParams->CrateCalibInfo.CalibStatus.TimeINLCalibStatus[feBoard][channel].CalibStatus != CALIB_VALUES_LOADED_FROM_FILE) &&
	   (crateInfoParams->CrateCalibInfo.CalibStatus.TimeINLCalibStatus[feBoard][channel].CalibStatus != CALIB_VALUES_LOADED_FROM_SOFTWARE))
	{
		hit->INLCorrected = FALSE;
		return;
	}
	else
	   hit->INLCorrected = TRUE;
	
	samplingPeriod = 1000000.0/(float)crateInfoParams->CrateCalibInfo.CalibStatus.TimeINLCalibStatus[feBoard][channel].CalibFreqEch; // en ps
	 
 				 
	for(n=0;n<hit->DataSize;n++)  
	{
		
		// Interpolation polynomiale de Lagrange

		i = (n + hit->FirstCellPhysicalIndex) % MAX_NB_OF_SAMPLES -1; //  On se recale dans le tableau d'INL qui est décalé de 1

		if(i < 0) i += MAX_NB_OF_SAMPLES;

		if((n != 0 ) && (n!= hit->DataSize -1))
		{
			prev_i = (i-1 +MAX_NB_OF_SAMPLES)%MAX_NB_OF_SAMPLES ;
			current_i = i;
			next_i = (i+1) % MAX_NB_OF_SAMPLES;

			prev_n = (n -1+MAX_NB_OF_SAMPLES)%MAX_NB_OF_SAMPLES ; 
			current_n = n;
		    next_n = (n+1) % MAX_NB_OF_SAMPLES; 
	
			x = current_n;
	
	

		}
		else if (n == 0)
		{
			prev_i = i;
			current_i = (i+1)% MAX_NB_OF_SAMPLES;
			next_i = (i+2)% MAX_NB_OF_SAMPLES;

			prev_n = n;
			current_n = (n+1)% MAX_NB_OF_SAMPLES;
			next_n = (n+2)% MAX_NB_OF_SAMPLES;
	
			x = prev_n;
	

		}
		else //n = dataSize -1
		{
			prev_i = (i-2 + MAX_NB_OF_SAMPLES)%MAX_NB_OF_SAMPLES ;
			current_i = (i-1 + MAX_NB_OF_SAMPLES)% MAX_NB_OF_SAMPLES;
			next_i = i;

			prev_n = (n-2 + MAX_NB_OF_SAMPLES)%MAX_NB_OF_SAMPLES ;
			current_n = (n-1 + MAX_NB_OF_SAMPLES)% MAX_NB_OF_SAMPLES;
			next_n = n;
	
			x = next_n;

		}

	   prevCoef = crateInfoParams->CrateCalibInfo.CalibValues[feBoard].TimeINLValues[channel][prev_i] / samplingPeriod;
	   currentCoef = crateInfoParams->CrateCalibInfo.CalibValues[feBoard].TimeINLValues[channel][current_i] / samplingPeriod;
	   nextCoef = crateInfoParams->CrateCalibInfo.CalibValues[feBoard].TimeINLValues[channel][next_i] / samplingPeriod;

	   x1 = prev_n + prevCoef;
	   x2 = current_n + currentCoef;
	   x3 = next_n + nextCoef; 

	   y1 = hit->CorrectedDataSamples[prev_n];
	   y2 = hit->CorrectedDataSamples[current_n];
	   y3 = hit->CorrectedDataSamples[next_n];

	   timeCorrectedDataSamples[n] = (y1*(x-x2)*(x-x3)/((x1-x2)*(x1-x3)))+
		   											  (y2*(x-x1)*(x-x3)/((x2-x1)*(x2-x3))) +
													  (y3*(x-x1)*(x-x2)/((x3-x1)*(x3-x2)));

	}   
		
	
	memcpy(hit->CorrectedDataSamples, timeCorrectedDataSamples, sizeof(float)* hit->DataSize);
	
	
}
/* ======================================================================= */
/* ======================================================================= */

/* ======================================================================= */
void Correct_ResiudalPedestals (CrateInfoStruct *crateInfoParams, HitStruct *hit)
/* ======================================================================= */
{
	
int n;
int feBoard, channel;
int physicalIndex;

	//Correction in physical Order ...

	channel = hit->Channel;
	feBoard = hit->FeBoardIndex;
	
	if(crateInfoParams->CrateCalibInfo.CalibStatus.ResidualPedestalCalibStatus[feBoard][channel].CalibStatus == CALIB_VALUES_NOT_LOADED)
	{
		hit->ResidualPedestalCorrected = FALSE;
		return;
	}
	else
	  hit->ResidualPedestalCorrected = TRUE;
	
	for(n=0;n<hit->DataSize;n++)  
	{ 
		physicalIndex = (n + hit->FirstCellPhysicalIndex)%MAX_NB_OF_SAMPLES;
		
	    hit->CorrectedDataSamples[n] -= crateInfoParams->CrateCalibInfo.CalibValues[feBoard].ResidualPedestalValues[channel][physicalIndex];
		
	}
 	
}

/* ======================================================================= */
//
///* ======================================================================= */
//void Correct_TimeLinearity						(int hitNumber)
///* ======================================================================= */
//{
//	int sample, i, prev_i, next_i, current_i, prev_n, current_n, next_n,n, frame, channel;
// 	float prevCoef, currentCoef, nextCoef;
//	float x,x1,x2,x3,y1,y2,y3;
//	float samplingPeriod;
//	
//	  	samplingPeriod = 1000000.0/(float)SystemParams.FreqEch;  // en ps
//
// 
//		for(n=0;n<Event.Hit[hitNumber].DataSize;n++)  
//		{
//
//			channel =Event.Hit[hitNumber].Channel;
//			
//			// Interpolation polynomiale de Lagrange
//
//			i = (n + Event.Hit[hitNumber].CellInfoForOrderedData) % MAX_NB_OF_SAMPLES -1; //  On se recale dans le tableau d'INL qui est décalé de 1
//
//			if(i < 0) i += MAX_NB_OF_SAMPLES;
//
//			if((n != 0 ) && (n!= Event.Hit[hitNumber].DataSize -1))
//			{
//				prev_i = (i-1 +MAX_NB_OF_SAMPLES)%MAX_NB_OF_SAMPLES ;
//				current_i = i;
//				next_i = (i+1) % MAX_NB_OF_SAMPLES;
//
//				prev_n = (n -1+MAX_NB_OF_SAMPLES)%MAX_NB_OF_SAMPLES ; 
//				current_n = n;
//			    next_n = (n+1) % MAX_NB_OF_SAMPLES; 
//		
//				x = current_n;
//		
//		
//
//			}
//			else if (n == 0)
//			{
//				prev_i = i;
//				current_i = (i+1)% MAX_NB_OF_SAMPLES;
//				next_i = (i+2)% MAX_NB_OF_SAMPLES;
//
//				prev_n = n;
//				current_n = (n+1)% MAX_NB_OF_SAMPLES;
//				next_n = (n+2)% MAX_NB_OF_SAMPLES;
//		
//				x = prev_n;
//		
//
//			}
//			else //n = dataSize -1
//			{
//				prev_i = (i-2 + MAX_NB_OF_SAMPLES)%MAX_NB_OF_SAMPLES ;
//				current_i = (i-1 + MAX_NB_OF_SAMPLES)% MAX_NB_OF_SAMPLES;
//				next_i = i;
//
//				prev_n = (n-2 + MAX_NB_OF_SAMPLES)%MAX_NB_OF_SAMPLES ;
//				current_n = (n-1 + MAX_NB_OF_SAMPLES)% MAX_NB_OF_SAMPLES;
//				next_n = n;
//		
//				x = next_n;
//
//			}
//
//		   prevCoef = CalibValues.SavedINLValues[channel][prev_i] / samplingPeriod;
//		   currentCoef = CalibValues.SavedINLValues[channel][current_i] / samplingPeriod;
//		   nextCoef = CalibValues.SavedINLValues[channel][next_i] / samplingPeriod;
//
//		   x1 = prev_n + prevCoef;
//		   x2 = current_n + currentCoef;
//		   x3 = next_n + nextCoef; 
//   
//		   y1 = Event.Hit[hitNumber].OrderedDataSamples[prev_n];
//		   y2 = Event.Hit[hitNumber].OrderedDataSamples[current_n];
//		   y3 = Event.Hit[hitNumber].OrderedDataSamples[next_n];
//   
//		   Event.Hit[hitNumber].TimeCorrectedDataSamples[n] = (y1*(x-x2)*(x-x3)/((x1-x2)*(x1-x3)))+
//			   											  (y2*(x-x1)*(x-x3)/((x2-x1)*(x2-x3))) +
//														  (y3*(x-x1)*(x-x2)/((x3-x1)*(x3-x2)));
//   
//
//   
//		}   
//	
//	
//	
//}
//
///* ======================================================================= */
//void Reset_DataSamples (void)
///* ======================================================================= */
//{
//	int hit, n;
//	
//	for (hit = 0; hit < Event.NbOfHitsInEvent; hit++)
//	{
//		for(n=0;n<Event.Hit[hit].DataSize;n++)  
//		{ 
//		
//			Event.Hit[hit].DataSamples[n]=  Event.Hit[hit].RawDataSamples[n];
//			
//		}
//		
//		// on recalcule Ordered Data Samples
//	
//   	    for(n= 0; n <Event.Hit[hit].DataSize; n++)
//			Event.Hit[hit].OrderedDataSamples[n]= Event.Hit[hit].DataSamples[(n+Event.Hit[hit].CellInfoForOrderedData)%MAX_NB_OF_SAMPLES];
//	
//		
//		
//	}
//}
//
//
//
///* ======================================================================= */
//void Apply_CorrectionsOnData (void)
///* ======================================================================= */
//{
//
//int hit;
//	
//	
// if(
//	((AcqParams.RunFromFile == FALSE ) && ((SystemParams.CorrectADCLinearity == TRUE) || (SystemParams.CorrectTimeINL == TRUE)))
//	||
//	((AcqParams.RunFromFile == TRUE) && (
//									((SystemParams.CorrectADCLinearity == TRUE) && (AcqParams.DataInFileADCCorrected == FALSE))
//									|| 
//									((SystemParams.CorrectTimeINL == TRUE) && (AcqParams.DataInFileINLCorrected == FALSE))
//									)
//	))
//	{
//	
//		for (hit = 0; hit < Event.NbOfHitsInEvent; hit++)
//		{
//		    if(
//				((AcqParams.RunFromFile == FALSE ) && (SystemParams.CorrectADCLinearity == TRUE)) ||
//			  	((AcqParams.RunFromFile == TRUE) && ((SystemParams.CorrectADCLinearity == TRUE) && (AcqParams.DataInFileADCCorrected == FALSE)))
//			  )
//			   Correct_AdcLinearity(hit);
//		
//			if(SystemParams.PedestalCorrection == TRUE)
//				 Correct_ExtraPedestals(hit);
//
//
//			if(
//				((AcqParams.RunFromFile == FALSE ) && (SystemParams.CorrectTimeINL == TRUE)) ||
//			  	((AcqParams.RunFromFile == TRUE) && ((SystemParams.CorrectTimeINL == TRUE) && (AcqParams.DataInFileINLCorrected == FALSE)))
//			  )
//			Correct_TimeLinearity(hit);
//		
//			/*if(AcqParams.En_ComputeIterativeLongDistanceINL == TRUE)
//			{
//	
//				Compute_IterativeGlobalINL(hit);
//		
//			}*/
//		
//		}
//	}
//	
//}
//
//


/* ======================================================================= */
/* ======================================================================= */




/* ======================================================================= */
/* =================    Lecture de status  =============================== */
/* ======================================================================= */

///* =========================================================================== */							
//void Read_CtrlFpgaVersion(void)
///* =========================================================================== */							
//{
//	unsigned char data = 0x0; ;
//	unsigned char sub_address; 
//	int dummy = 0;
//	
//	
//	if((UsbId < 0) && (UdpId < 0))
//		return;
//
//	sub_address = ad_fe_board_ctrl_FPGA_version;
//	
//	
//	BusReadWords( CTRL_FPGA, dummy, sub_address, &data, 1); 
//
//	CtrlFPGAParams.BoardVersion = (data&0xF0)>>4;
//	
//	CtrlFPGAParams.FpgaVersion = (data&0xF);
//	
//	sub_address =  ad_fe_board_ctrl_FPGA_evolution;
//	
//	BusReadWords( CTRL_FPGA, dummy, sub_address, &data, 1); 
//	
//	CtrlFPGAParams.FpgaEvolution = data;
//	
//	if(SystemParams.MotherBoardFirmwareVersion  == NULL)
//		SystemParams.MotherBoardFirmwareVersion = malloc(sizeof(char)*20);
//
//	sprintf(SystemParams.MotherBoardFirmwareVersion,"V%d.%d.%d",CtrlFPGAParams.BoardVersion,CtrlFPGAParams.FpgaVersion, CtrlFPGAParams.FpgaEvolution);  
//
//	sprintf(Message, "Controller FPGA Version: V%d.%d.%d \n", 	CtrlFPGAParams.BoardVersion,CtrlFPGAParams.FpgaVersion, 	CtrlFPGAParams.FpgaEvolution);
//
//	Write_InfoMessage(Message, INFO, NO_POPUP);
//	
//		
//}
//
//
///* =========================================================================== */							
//void Read_FeFpgaVersion(int feFPGAIndex)
///* =========================================================================== */							
//{
//
//	
//	unsigned char data = 0;
//	unsigned char sub_address; 
//	ErrorType errCode = Success;  
//	
//	if((feFPGAIndex < 0) || (feFPGAIndex > Nb_Of_Fe_FPGAs_In_System))
//		return;
//		
//	sub_address =  ad_fe_board_fe_fpga_version;   
//		
//	
//	errCode = BusReadWords( FE_FPGA, feFPGAIndex, sub_address, &data, 1); 
//
//	FeFpgaParams[feFPGAIndex].BoardVersion = (data&0xF0)>>4;
//	FeFpgaParams[feFPGAIndex].FpgaVersion = (data&0xF); 
//	
//	sub_address =  ad_fe_board_fe_fpga_evolution;
//	
//	errCode = BusReadWords( FE_FPGA, feFPGAIndex, sub_address, &data, 1); 
//	
//	FeFpgaParams[feFPGAIndex].FpgaEvolution = data;
//	
//	
//	sprintf(Message, "Front-End FPGA[%d] Version: V%d.%d.%d \n",feFPGAIndex,FeFpgaParams[feFPGAIndex].BoardVersion,FeFpgaParams[feFPGAIndex].FpgaVersion, FeFpgaParams[feFPGAIndex].FpgaEvolution);
//
//	Write_InfoMessage(Message, INFO, NO_POPUP);
//	
//
//	
//}
//
//
/* ======================================================================= */
/* ============================ Utilitaires ============================== */
/* ======================================================================= */
/* ============================================================================ */ 

/* ============================================================================ */ 
int Get_FeBoardIndexFromFramePathIndex(CrateInfoStruct *crateInfoParams, int framePathIndex)  
/* ============================================================================ */ 
{
   int feBoardIndex;
   int result;
   
     for (feBoardIndex = 0; feBoardIndex < MAX_NB_OF_FE_BOARDS; feBoardIndex++)
	 {
		 
			if(crateInfoParams->FrontEndBoardsPathIndex[feBoardIndex] == framePathIndex)
			{
				result = feBoardIndex;
				break;
			}
	 }
   
   return result;
	
}

/* ======================================================================= */
int	Convert_FromGrayToBinary (int grayValue, int nbOfBits)
/* ======================================================================= */
{
// converts a Gray integer of nbOfBits bits to a decimal integer.
int binary, grayBit, binBit;
	
	binary = 0;

// mask the MSB.
	
	grayBit = 1 << ( nbOfBits - 1 );
	
// copy the MSB.
	
	binary = grayValue & grayBit;
	
// store the bit we just set.
	
	binBit = binary;
	
// traverse remaining Gray bits.
	
	while( grayBit >>= 1 )
	{
		// shift the current binary bit to align with the Gray bit.
		binBit >>= 1;
		// XOR the two bits.
		binary |= binBit ^ ( grayValue & grayBit );
		// store the current binary bit
		binBit = binary & grayBit;
	}

	return( binary );
}

/* ======================================================================= */
int	Calculate_10BitsDACIntValues (float rawValue)
/* ======================================================================= */
{
int result;
 
	result =  (int)	(rawValue * 1023/ MAX_DAC_RAW_VALUE);	
	
	return result;
}
/* ======================================================================= */
float Calculate_10BitsDACFloatValue	(int intValue)																	
/* ======================================================================= */
{
float result;
			 
 result =   (float)((float)intValue * MAX_DAC_RAW_VALUE)/1023;

 return result;
	
}
/* ======================================================================= */
int Calculate_16BitsDACIntValues (float rawValue, float dacType)
/* ======================================================================= */
{
int result;
float maxDacValue;
 
 	if(dacType == EXT_SIG_DAC_TYPE) maxDacValue = MAX_EXT_SIG_DAC_VALUE;
	else 
	{
		maxDacValue = MAX_DAC_RAW_VALUE;
		maxDacValue *= MAX_DAC_REAL_VALUE_V4 / MAX_DAC_REAL_VALUE_V1_TO_V3; 
	}
	
	result =  (int)	(rawValue * 65535/ maxDacValue);	
	
	return result;
	
}
/* ======================================================================= */
float Calculate_16BitsDACFloatValue	(int intValue)
/* ======================================================================= */
{
float result;
			 
 result =   (float)((float)intValue * MAX_DAC_RAW_VALUE)/65535;

 return result;
	
}
/* ======================================================================= */
float Calculate_12BitsDACFloatValue (int intValue)
/* ======================================================================= */
{
	
float result;
			 
 result =   (float)((float)intValue * MAX_DAC_RAW_VALUE)/4095;
 
 result *= MAX_DAC_REAL_VALUE_V4 / MAX_DAC_REAL_VALUE_V1_TO_V3;

 return result;
	
}

/* ======================================================================= */
int						Calculate_12BitsDACIntValues	(float rawValue)
/* ======================================================================= */
{
 int result;
 
	result =  (int)	(rawValue * 4095/ MAX_DAC_RAW_VALUE);	
	
	return result;
	
	
}

 /* =========================================================================== */							

	

///* =========================================================================== */							
// void Save_Params(void)
///* =========================================================================== */							
//{
//int i;
//
//#ifndef SAMPIC_DLL_EXPORTS
//	SavedAcqParams = AcqParams;
//	SavedGuiParams = GuiParams;
//#endif
//	SavedCtrlFPGAParams = CtrlFPGAParams;
//	SavedSystemParams = SystemParams; 
//	
//	for(i=0; i<Nb_Of_Fe_FPGAs_In_System; i++)
//	{
//		SavedFeFpgaParams[i] =	FeFpgaParams[i];
//		SavedDebugParams[i] = DebugParams[i];
//	}
//}
//	
///* =========================================================================== */							
// void Restore_Params(void)
///* =========================================================================== */							
//{
//int i;
//#ifndef SAMPIC_DLL_EXPORTS
//	AcqParams = SavedAcqParams;
//	GuiParams = SavedGuiParams;
//#endif
//	CtrlFPGAParams = SavedCtrlFPGAParams;
//	
//	SystemParams= SavedSystemParams;
//	
//	Nb_Of_Channels_In_System = SystemParams.NbOfChannelsInSystem;
//	Nb_Of_Fe_FPGAs_In_System = SystemParams.NbOfFeBoardsInSystem;
//	
//	for(i=0; i<Nb_Of_Fe_FPGAs_In_System; i++)
//	{
//		FeFpgaParams[i] = SavedFeFpgaParams[i];
//		DebugParams[i] =  SavedDebugParams[i];
//	}
//}
//
///* =========================================================================== */							
///* =========================================================================== */							
//
// 
///* =================================================================================== */
///* =========================   Writing/reading Files ================================= */
///* =================================================================================== */
// /* =========================================================================== */ 
int Read_InternalTriggerThresholdOffset_From_Files(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, char directory[MAX_PATHNAME_LENGTH])
/* =========================================================================== */ 
{
FILE 	*fp;
char 	line[1024];
int		result, n, line_nb;
float	fval;
char	file_name[MAX_PATHNAME_LENGTH];
int init_channel, end_channel, channel;
float offset, slope; 

int resultOK = TRUE;
int resultBaselineFileOK = TRUE;
int globalResultOK = TRUE;
int feBoard, sampicIndex, sample;
int freqEch;
int  adcNbOfBits;
int boardVersion, boardSerNum;
char boardTypeCharFromEEPROM;
float baseline;

  freqEch = crateParams->CommonParams.FreqEch;
  adcNbOfBits = crateParams->CommonParams.ADCNbOfBits;

  if(crateInfoParams->SystemType == SAMPET_SYSTEM)
 	 return 1;
	
   for(feBoard = 0; feBoard < crateInfoParams->NbOfFeBoards; feBoard++)
   {
	   boardVersion = crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoard].BoardVersion;      
	   boardSerNum = crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoard].BoardSerNum;
	   
	   boardTypeCharFromEEPROM =   crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoard].FeBoardTypeCharFromEEPROM; 
		
	   resultOK = TRUE;
	
	   // first we read the default files
	   
	   
 		if(boardTypeCharFromEEPROM != 'Z')
		{
	    	sprintf(file_name,"%s/InternalTriggerThresholdOffsets_Files/IntTriggerThreshold_Calib_Board_T%c_V%d.%d.txt",directory, boardTypeCharFromEEPROM, boardVersion, boardSerNum); 
		}
		else
		{
			sprintf(file_name,"%s/InternalTriggerThresholdOffsets_Files/IntTriggerThreshold_Calib_Board_V%d.%d.txt",directory,boardVersion, boardSerNum);	
		}
		
	    fp = fopen(file_name, "r");

		if(fp==NULL)
		{
		
			resultOK = FALSE;
			
		}
		else
		{
	
			if(fgets(line, 1024,fp)!=NULL)
			{
			  if(sscanf(line, "Internal Trigger Threshold Offsets for channels %d to %d : ",&init_channel, &end_channel)!= 2)
			  {
		  
				resultOK = FALSE;  
				
			  }
			}
			else
			{
				resultOK = FALSE;
			}

			 
			if(resultOK == TRUE)
			{
				fgets(line, 1024,fp);
				for(channel=init_channel; channel <= end_channel; channel++)
				{
		
			
					fgets(line, 1024,fp);
					result = sscanf(line, "%f %f", &offset, &slope);
					if(result != 1  && result != 2)
					{
			
						resultOK = FALSE;
						break;
					}
					else
					{
						if(result == 1)
						 crateInfoParams->CrateCalibInfo.CalibValues[feBoard].InternalTriggerThresholdOffset[channel]= offset; 
						else if(result == 2)
						{
							crateInfoParams->CrateCalibInfo.CalibValues[feBoard].InternalTriggerThresholdOffset[channel]= offset; 
							crateInfoParams->CrateCalibInfo.CalibValues[feBoard].InternalTriggerThresholdSlope[channel]= slope; 
						}
					}
		  
				 }
			}
		}
		
	    if(fp != NULL)
			fclose(fp);
	   
	   //then we look for each sampic if files exists for the specific baseline of the sampic
		
		for(sampicIndex = 0; sampicIndex < NB_OF_SAMPICS_IN_FE_BOARD; sampicIndex++)
		{
			
			baseline = 	crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.VBaseline;
			
		  
			 if(boardTypeCharFromEEPROM != 'Z')
			 {
		    	sprintf(file_name,"%s/InternalTriggerThresholdOffsets_Files/Baseline_%4.3f/IntTriggerThreshold_Calib_Board_T%c_V%d.%d_sampic_%d.txt",directory,baseline, boardTypeCharFromEEPROM, boardVersion, boardSerNum, sampicIndex); 
			 }
			 else
			 {
				sprintf(file_name,"%s/InternalTriggerThresholdOffsets_Files/Baseline_%4.3f/IntTriggerThreshold_Calib_Board_V%d.%d_sampic_%d.txt",directory,baseline,boardVersion, boardSerNum, sampicIndex);	
			 }
	
			  fp = fopen(file_name, "r"); 
		  
			  if(fp == NULL)
				  resultBaselineFileOK = FALSE;
			  
			 if(fp != NULL)   
			  {
				if(fgets(line, 1024,fp)!=NULL)
				{
				  if(sscanf(line, "Internal Trigger Threshold Offsets for channels %d to %d : ",&init_channel, &end_channel)!= 2)
				  {
		  
					resultBaselineFileOK = FALSE;  
				
				  }
				}
				else
				{
					resultBaselineFileOK = FALSE;
				}

			 
				if(resultBaselineFileOK == TRUE)
				{
					fgets(line, 1024,fp);
					for(channel=init_channel; channel <= end_channel; channel++)
					{
		
			
						fgets(line, 1024,fp);
						result = sscanf(line, "%f %f", &offset, &slope);
						if(result != 1  && result != 2)
						{
			
							resultBaselineFileOK = FALSE;
							break;
						}
						else
						{
							if(result == 1)
							{
								crateInfoParams->CrateCalibInfo.CalibValues[feBoard].InternalTriggerThresholdOffset[channel]= offset; 
								crateInfoParams->CrateCalibInfo.CalibValues[feBoard].InternalTriggerThresholdSlope[channel]= 0.0;
							}
							else if(result == 2)
							{
								crateInfoParams->CrateCalibInfo.CalibValues[feBoard].InternalTriggerThresholdOffset[channel]= offset; 
								crateInfoParams->CrateCalibInfo.CalibValues[feBoard].InternalTriggerThresholdSlope[channel]= slope; 
							}
						}
		  
					 }
				}
	  
			  
			  }
			
			 if(fp!=NULL)
				fclose(fp);
		}

   
	 if(resultOK == FALSE && resultBaselineFileOK == FALSE)
	 {
		globalResultOK = FALSE;	
		SAMPIC256CH_ResetInternalTriggerThresholdOffsetCalibValues(crateInfoParams, feBoard);   
		
	 }
	 else
	 {
	  	crateInfoParams->CrateCalibInfo.CalibStatus.InternalTriggerThresholdCalibStatus[feBoard].CalibStatus = CALIB_VALUES_LOADED_FROM_FILE;
		crateInfoParams->CrateCalibInfo.CalibStatus.InternalTriggerThresholdCalibStatus[feBoard].CalibFreqEch = freqEch;
		crateInfoParams->CrateCalibInfo.CalibStatus.InternalTriggerThresholdCalibStatus[feBoard].CalibADCNbOfBits = adcNbOfBits;
		
		
	 }

   }

	 
  SAMPIC256CH_UpdateChannelsInternalThresholdsFromCalibValues (crateInfoParams, crateParams);
  
  if(resultBaselineFileOK == TRUE)
	  return 2;
  else 
    return globalResultOK;	
	
}



/* =================================================================================== */
int Read_ADCLinearityCalibValues_From_Files(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, char directory[MAX_PATHNAME_LENGTH])
/* =================================================================================== */
{

FILE 	*fp;
char 	line[1024];
int		n, line_nb;
float	fval;
char	file_name[MAX_PATHNAME_LENGTH];

int resultOK = TRUE;
int globalResultOK = TRUE;
int feBoard, channel, sample;
int freqEch;
int  adcNbOfBits;
int boardVersion, boardSerNum;
char boardTypeCharFromEEPROM;


   freqEch = crateParams->CommonParams.FreqEch;
   adcNbOfBits = crateParams->CommonParams.ADCNbOfBits;

 	
   for(feBoard = 0; feBoard < crateInfoParams->NbOfFeBoards; feBoard++)
   {
	   boardVersion = crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoard].BoardVersion;      
	   boardSerNum = crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoard].BoardSerNum;
	   boardTypeCharFromEEPROM =   crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoard].FeBoardTypeCharFromEEPROM; 
	   
 	   resultOK = TRUE;
		
		for(channel = 0; channel< NB_OF_CHANNELS_IN_FE_BOARD; channel++)
		{
	
			if(resultOK == FALSE)
				break;
			
			if(boardTypeCharFromEEPROM != 'Z')
			{
			  sprintf(file_name,"%s/ADC_Linearity_Files/ADC_Linearity_Calib_Board_T%c_V%d.%d_FreqEch_%d_MS_s_ADCNbOfBits%d_Ch%d.txt",directory,boardTypeCharFromEEPROM, boardVersion, boardSerNum,freqEch,adcNbOfBits, channel);	

			}
			else
			{
				
				sprintf(file_name,"%s/ADC_Linearity_FilesADC_Linearity_Calib_Board_V%d.%d_FreqEch_%d_MS_s_ADCNbOfBits%d_Ch%d.txt",directory,boardVersion, boardSerNum,freqEch,adcNbOfBits, channel);	
			}
			
			fp = fopen(file_name, "r");
		
		
			if(fp==NULL)
			{
				resultOK = FALSE;
				
				break;
			
			}
		
			if(fgets(line, 1024,fp)==NULL)
			{
				resultOK = FALSE; 
				break;
			
			}
	
			fgets(line, 1024,fp); /* ligne de commentaire */
		
			fgets(line, 1024,fp); /* ligne des slope*/


			for(n=0;n<MAX_NB_OF_SAMPLES;n++) /* il y a 64 en une seule ligne */
			{
				if ((sscanf(line+10*n, "%f", &fval)) != 1)
				{
					//Debug_Print("Probleme... \n");
					resultOK = FALSE;
					break;
				}
				else
				{
					crateInfoParams->CrateCalibInfo.CalibValues[feBoard].CellLinearity_a0[channel][n] = fval; 
				}
			}
		
		
			if(resultOK == TRUE)
			{
				fgets(line, 1024,fp); /* ligne des intercept */


				for(n=0;n<MAX_NB_OF_SAMPLES;n++) /* il y a 64 en une seule ligne */
				{
					if ((sscanf(line+10*n, "%f", &fval)) != 1)
					{
						//Debug_Print("Probleme... \n");
						resultOK = FALSE;
						break;
					}
					else
					{
						crateInfoParams->CrateCalibInfo.CalibValues[feBoard].CellLinearity_a1[channel][n] = fval; 
					}
				}
			
				fgets(line, 1024,fp); /* ligne des intercept */


				for(n=0;n<MAX_NB_OF_SAMPLES;n++) /* il y a 64 en une seule ligne */
				{
					if ((sscanf(line+10*n, "%f", &fval)) != 1)
					{
						//Debug_Print("Probleme... \n");
						resultOK = FALSE;
						break;
					}
					else
					{
						crateInfoParams->CrateCalibInfo.CalibValues[feBoard].CellLinearity_a2[channel][n] = fval; 
					}
				}

			}
			
		
		  
		  fclose(fp);
		}
	
		if(resultOK == FALSE)
		{
			SAMPIC256CH_ResetADCLinearityCalibValues(crateInfoParams, feBoard);  
			 
		    globalResultOK = FALSE;
		}
		else
		{
			crateInfoParams->CrateCalibInfo.CalibStatus.ADCLinearityCalibStatus[feBoard].CalibStatus = CALIB_VALUES_LOADED_FROM_FILE;
			crateInfoParams->CrateCalibInfo.CalibStatus.ADCLinearityCalibStatus[feBoard].CalibFreqEch = freqEch;  
			crateInfoParams->CrateCalibInfo.CalibStatus.ADCLinearityCalibStatus[feBoard].CalibADCNbOfBits = adcNbOfBits; 

		}
			
	 
   }

	if(globalResultOK == FALSE)  // at least error with one file
	{
		crateInfoParams->CrateCalibInfo.CalibStatus.ADCLinearityCrateCalibStatus.CalibStatus = CALIB_VALUES_NOT_LOADED;
		crateInfoParams->CrateCalibInfo.CalibStatus.ADCLinearityCrateCalibStatus.CalibFreqEch = freqEch; 
		crateInfoParams->CrateCalibInfo.CalibStatus.ADCLinearityCrateCalibStatus.CalibADCNbOfBits = adcNbOfBits; 

	}
	else
	{
		crateInfoParams->CrateCalibInfo.CalibStatus.ADCLinearityCrateCalibStatus.CalibStatus = CALIB_VALUES_LOADED_FROM_FILE; 
		crateInfoParams->CrateCalibInfo.CalibStatus.ADCLinearityCrateCalibStatus.CalibFreqEch = freqEch; 
		crateInfoParams->CrateCalibInfo.CalibStatus.ADCLinearityCrateCalibStatus.CalibADCNbOfBits = adcNbOfBits; 

	}

   return globalResultOK;
	
}


/* =================================================================================== */
int Read_ADCRampCalibValues_From_Files(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, char directory[MAX_PATHNAME_LENGTH])
/* =================================================================================== */
{
int feBoard, sampicIndex;

FILE 	*fp;
char 	line[1024];
int		n, line_nb;
float	fval;
char	file_name[MAX_PATHNAME_LENGTH];
int freqEch;      
int  adcNbOfBits;
int boardVersion, boardSerNum;
int resultOK = TRUE;
int globalResultOK = TRUE;
char boardTypeCharFromEEPROM; 
int dummy = 0;
int dllMode;
float dllContinuity, vdacDll;
float vdacRosc;
 
   freqEch = crateParams->CommonParams.FreqEch;
   adcNbOfBits = crateParams->CommonParams.ADCNbOfBits;

 	
   for(feBoard = 0; feBoard < crateInfoParams->NbOfFeBoards; feBoard++)
   {
	   	boardVersion = crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoard].BoardVersion;      
	   	boardSerNum = crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoard].BoardSerNum;
	    boardTypeCharFromEEPROM =   crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoard].FeBoardTypeCharFromEEPROM; 
 		
		resultOK = TRUE;    
		 
		for(sampicIndex = 0; sampicIndex< NB_OF_SAMPICS_IN_FE_BOARD; sampicIndex++)
		{				   
		
			if(resultOK == FALSE)
				break;
			
			if(boardTypeCharFromEEPROM != 'Z')
			{
				sprintf(file_name,"%s/ADC_IRamp_Files/ADC_IRamp_Calib_Board_T%c_V%d.%d_feIndex_%d_FreqEch_%d_MS_s.txt",directory,boardTypeCharFromEEPROM, boardVersion, boardSerNum,sampicIndex, freqEch);	
			}
			else
			{
				sprintf(file_name,"%s/ADC_IRamp_Files/ADC_IRamp_Calib_Board_V%d.%d_feIndex_%d_FreqEch_%d_MS_s.txt",directory,boardVersion, boardSerNum,sampicIndex, freqEch);	
			}
			
			fp = fopen(file_name, "r");
		
			// pour être compatible avec les anciens fichiers même pour toutes les fréquences
			if(fp==NULL)
			{
			
				sprintf(file_name,"%s/ADC_IRamp_Files/ADC_IRamp_Calib_Board_V%d.%d_feIndex_%d.txt",directory,boardVersion, boardSerNum,sampicIndex);	
	
				fp = fopen(file_name, "r");
			
			}
		
			if(fp==NULL)
			{
				
				resultOK = FALSE;
				break;
			
			}
		
			if(fgets(line, 1024,fp)==NULL)
			{
			
				resultOK = FALSE; 
				break;
			
			}
	
			fgets(line, 1024,fp); /* ligne de commentaire */
			fgets(line, 1024,fp); /* ligne des valeurs de calibration */


			for(n=0;n<MAX_NB_OF_ADC_MODES;n++) 
			{
				if ((sscanf(line+10*n, "%f", &fval)) != 1)
				{
					//Debug_Print("Probleme... \n");
					resultOK = FALSE;
					break;
				}
				else
				{
					crateInfoParams->CrateCalibInfo.CalibValues[feBoard].VDAC_Ramp[sampicIndex][n] = fval; 
				}
			}
			
			fgets(line, 1024,fp); /* ligne vide */ 
		
			if(fgets(line, 1024,fp)!= NULL)  //ligne DLL SPEED 	
			{
			
		    	if(sscanf(line, "SAMPIC DLL SPEED MODE : %d ", &dllMode) == 1)
		    	{
					crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.DLLMode = dllMode;	
				
		
				}
			}
	
			
			if(fgets(line, 1024,fp)!= NULL)  //ligne DLL CONTINUITY 	
			{
			
		    	if(sscanf(line, "SAMPIC VDAC_DLL_CONTINUITY: %f [Volts] VDAC_DLL: %f [Volts]", &dllContinuity, &vdacDll) == 2)
		    	{
					//crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.DLLMode = dllMode;	
				   crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.Vdac_DLLContinuity = dllContinuity;
				   crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.Vdac_DLL = vdacDll;
		
				}
			}
			
			if(fgets(line, 1024,fp)!= NULL)  //ligne VDAC_ROSC 	
			{
		    	if(sscanf(line, "SAMPIC VDAC_ROSC: %f [Volts]", &vdacRosc) == 1)
		    	{
					crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.InternalRoscDAC=vdacRosc; 	
				}
			}
			
			
			fclose(fp);

		}
	
	 if(resultOK == FALSE)
	 {
		SAMPIC256CH_ResetADCRampCalibValues(crateInfoParams,feBoard); 	
		globalResultOK = FALSE;	 
	 }
	 else
	 {
		 
		crateInfoParams->CrateCalibInfo.CalibStatus.ADCRampCalibStatus[feBoard].CalibStatus = CALIB_VALUES_LOADED_FROM_FILE;
		crateInfoParams->CrateCalibInfo.CalibStatus.ADCRampCalibStatus[feBoard].CalibFreqEch = freqEch;
		crateInfoParams->CrateCalibInfo.CalibStatus.ADCRampCalibStatus[feBoard].CalibADCNbOfBits = adcNbOfBits;
		
		Build_FeBoardsSampicsConfigReg3(crateParams);
		Build_FeBoardsSampicsConfigReg1(crateParams);
		Build_FeBoardsSampicsConfigReg8(crateParams);
		
		
		Load_DACs(crateInfoParams, crateParams,  FEB_VDAC_DLL,  ALL_FE_BOARDs, ALL_FE_BOARDs_FE_FPGAs);  
		Load_HardwareSetup(crateInfoParams, crateParams, FEB_SAMPIC_SC_REG1, ALL_FE_BOARDs, ALL_FE_BOARDs_FE_FPGAs, dummy); 
		Load_HardwareSetup(crateInfoParams, crateParams, FEB_SAMPIC_SC_REG3, ALL_FE_BOARDs, ALL_FE_BOARDs_FE_FPGAs, dummy);  
		Load_HardwareSetup(crateInfoParams, crateParams, FEB_SAMPIC_SC_REG8, ALL_FE_BOARDs, ALL_FE_BOARDs_FE_FPGAs, dummy); 
		
		Reset_SAMPICDLL (crateInfoParams,crateParams);	 
	 }
	
   }
	
   SAMPIC256CH_UpdateADCRampValuesFromCalibValues(crateInfoParams, crateParams);
	
	return globalResultOK;
}
/* =================================================================================== */

/* =================================================================================== */
int Read_INLValues_From_Files(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, char directory[MAX_PATHNAME_LENGTH])
/* =================================================================================== */
{
FILE 	*fp;
char 	line[1024];
int		n, line_nb;
float	fval;
char	file_name[MAX_PATHNAME_LENGTH];

int resultOK = TRUE;
int globalResultOK = TRUE;
int feBoard, channel, sample;
int freqEch;
int  adcNbOfBits;
int boardVersion, boardSerNum;
char boardTypeCharFromEEPROM;
char 	*lineTemp;   

   freqEch = crateParams->CommonParams.FreqEch;
   adcNbOfBits = crateParams->CommonParams.ADCNbOfBits;

   for(feBoard = 0; feBoard < crateInfoParams->NbOfFeBoards; feBoard++)
   {
	    boardVersion = crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoard].BoardVersion;      
	    boardSerNum = crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoard].BoardSerNum;
	    boardTypeCharFromEEPROM =   crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoard].FeBoardTypeCharFromEEPROM;
		
		for(channel = 0; channel< NB_OF_CHANNELS_IN_FE_BOARD; channel++)
		{
		
			resultOK = TRUE;
		 
			if(boardTypeCharFromEEPROM != 'Z')
			{
				sprintf(file_name,"%s/INL_Files/INL_Calib_Board_T%c_V%d.%d_FreqEch_%d_MS_s_Ch%d.txt",directory,boardTypeCharFromEEPROM, boardVersion,boardSerNum,freqEch,channel);	
	
			}
			else
			{
			    sprintf(file_name,"%s/INL_Files/INL_Calib_Board_V%d.%d_FreqEch_%d_MS_s_Ch%d.txt",directory,boardVersion,boardSerNum,freqEch,channel);	
	
			}
	
			fp = fopen(file_name, "r");
		
		
			if(fp==NULL)
			{
			
				resultOK = FALSE;

		
			}
			else
			{
			
				if(fgets(line, 1024,fp)==NULL)
				{
					resultOK = FALSE;
				
			
				}
				else
				{
	
					fgets(line, 1024,fp); /* ligne de commentaire */
					fgets(line, 1024,fp); /* ligne des piédestaux */
					
				    lineTemp =line;
		

					for(n=0;n<MAX_NB_OF_SAMPLES;n++) /* il y a 64 en une seule ligne */
					{
						//if ((sscanf(line+10*n, "%f", &fval)) != 1)
						if ((sscanf(lineTemp, "%f", &fval)) != 1)     
						{
							resultOK = FALSE;
						
							break;
						}
						else
						{
							crateInfoParams->CrateCalibInfo.CalibValues[feBoard].TimeINLValues[channel][n] = fval; /* 8 valeurs par lignes */
							lineTemp = strchr (lineTemp, '	');
							if(lineTemp != NULL)
								lineTemp++;
	
						}
					}

				   fclose(fp);
				}
			}
	
			if(resultOK == FALSE)
			{
				SAMPIC256CH_ResetTimeINLCalibValues	(crateInfoParams, feBoard, channel);
				globalResultOK = FALSE;  
			}
			else
			{
				crateInfoParams->CrateCalibInfo.CalibStatus.TimeINLCalibStatus[feBoard][channel].CalibStatus = CALIB_VALUES_LOADED_FROM_FILE;
				crateInfoParams->CrateCalibInfo.CalibStatus.TimeINLCalibStatus[feBoard][channel].CalibFreqEch = freqEch;
				crateInfoParams->CrateCalibInfo.CalibStatus.TimeINLCalibStatus[feBoard][channel].CalibADCNbOfBits = adcNbOfBits;
	
			}
			
			
		}
	
   }
   
  
  return globalResultOK;
	
}
/* =================================================================================== */


 /* =================================================================================== */
int Read_TOTCalibValues_From_Files(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, char directory[MAX_PATHNAME_LENGTH])
/* =================================================================================== */
{
int channel;

FILE 	*fp;
char 	line[1024];
int		n, line_nb, step;
float	fval;
char	file_name[MAX_PATHNAME_LENGTH];
int dacStep;
float dacVal, totIntercep, totSlope;
int resultOK = TRUE;
int globalResultOK = TRUE;
int feBoard, sample;
int  freqEch;
int  adcNbOfBits;
int boardVersion, boardSerNum;
char boardTypeCharFromEEPROM;  


  freqEch = crateParams->CommonParams.FreqEch;
  adcNbOfBits = crateParams->CommonParams.ADCNbOfBits;
  

	
   for(feBoard = 0; feBoard < crateInfoParams->NbOfFeBoards; feBoard++)
   {
	   	boardVersion = crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoard].BoardVersion;      
	   	boardSerNum = crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoard].BoardSerNum;
  		boardTypeCharFromEEPROM =   crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoard].FeBoardTypeCharFromEEPROM; 
		
		for(channel = 0; channel< NB_OF_CHANNELS_IN_FE_BOARD; channel++)
	    {
			resultOK = TRUE;
			
			if(boardTypeCharFromEEPROM != 'Z')
			{
			  sprintf(file_name,"%s/TOT_CalibFiles/TOT_IRamp_Calib_Board_T%c_V%d.%d_FreqEch_%d_MS_s_ADCNbOfBits%d_Ch%d.txt",directory, boardTypeCharFromEEPROM, boardVersion, boardSerNum,freqEch,adcNbOfBits, channel);	
			}
			else
			{
			   sprintf(file_name,"%s/TOT_CalibFiles/TOT_IRamp_Calib_Board_V%d.%d_FreqEch_%d_MS_s_ADCNbOfBits%d_Ch%d.txt",directory,boardVersion, boardSerNum,freqEch,adcNbOfBits, channel);	
			}
			
			fp = fopen(file_name, "r");
		
			if(fp==NULL)
			{
				resultOK = FALSE;
				break;
			}
		
			if(fgets(line, 1024,fp)==NULL) //ligne commentaire
			{
				resultOK = FALSE; 
				break;
			}
		
			fgets(line, 1024,fp); //ligne vide
		
			for(step = 0; step < MAX_NB_OF_TOT_RAMP_CURRENT_VALUES; step++)
			{
		
				 if(fgets(line, 1024,fp) == NULL)
				 {
					resultOK = FALSE; 
					break;  
			 
			 
				 } 
		   		
				if(sscanf(line,"dac step= %d dac Value= %f intercept= %f slope= %f  ", &dacStep, &dacVal, &totIntercep, &totSlope) != 4)
				{
					resultOK = FALSE; 
					break;  
			 
				
				}
				else
				{
				
					crateInfoParams->CrateCalibInfo.CalibValues[feBoard].TOTCalibIntercept[dacStep][channel]= totIntercep;
					crateInfoParams->CrateCalibInfo.CalibValues[feBoard].TOTCalibSlope[dacStep][channel]= totSlope;
				
				}
			}
		
			fclose(fp);
			
			if(resultOK == FALSE)
			{
			  globalResultOK = FALSE;
			  
			}

	}

		
	  if(resultOK == FALSE)
	  {
		
		globalResultOK = FALSE;
		SAMPIC256CH_ResetTOTDACOffsetsValues(crateInfoParams,feBoard);
		  
	  }
	  else
	  {
		crateInfoParams->CrateCalibInfo.CalibStatus.TOTCalibStatus[feBoard].CalibStatus = CALIB_VALUES_LOADED_FROM_FILE;
		crateInfoParams->CrateCalibInfo.CalibStatus.TOTCalibStatus[feBoard].CalibADCNbOfBits = adcNbOfBits;
		crateInfoParams->CrateCalibInfo.CalibStatus.TOTCalibStatus[feBoard].CalibFreqEch = freqEch;
		  
	  }
   }



   if(globalResultOK == TRUE)
   {
	   
		crateInfoParams->CrateCalibInfo.CalibStatus.TOTCrateCalibStatus.CalibStatus = CALIB_VALUES_LOADED_FROM_FILE;
		crateInfoParams->CrateCalibInfo.CalibStatus.TOTCrateCalibStatus.CalibADCNbOfBits = adcNbOfBits;
		crateInfoParams->CrateCalibInfo.CalibStatus.TOTCrateCalibStatus.CalibFreqEch = freqEch;
		
	   
   }
	return globalResultOK;	
	
	
}
/* =================================================================================== */


/* =================================================================================== */
void Read_ResidualPedestals_From_Files(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, char directory[MAX_PATHNAME_LENGTH])
/* =================================================================================== */
{
int feBoard, channel, sampicIndex;

FILE 	*fp;
char 	line[1024];
int		n, line_nb;
float	fval;
char	file_name[MAX_PATHNAME_LENGTH];
int freqEch;      
int  adcNbOfBits;
int boardVersion, boardSerNum;
int resultOK = TRUE;
int globalResultOK = TRUE;
char boardTypeCharFromEEPROM; 
int dummy = 0;
float baseline;

  freqEch = crateParams->CommonParams.FreqEch;
  adcNbOfBits = crateParams->CommonParams.ADCNbOfBits;
  
   for(feBoard = 0; feBoard < crateInfoParams->NbOfFeBoards; feBoard++)
   {
	   	boardVersion = crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoard].BoardVersion;      
	   	boardSerNum = crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoard].BoardSerNum;
  		boardTypeCharFromEEPROM =   crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoard].FeBoardTypeCharFromEEPROM; 
		
		
		for(channel = 0; channel< NB_OF_CHANNELS_IN_FE_BOARD; channel++)
	    {
  
			 sampicIndex =  (int)(channel/ NB_OF_CHANNELS_IN_SAMPIC);
		 
			 baseline = 	crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.VBaseline;
	
			 resultOK = TRUE;
  
			 if(boardTypeCharFromEEPROM != 'Z')
			 {
		    	sprintf(file_name,"%s/ResidualPedestal_Files/Baseline_%4.3f/ResidualPedestals_Calib_Board_T%c_V%d.%d_Ch%d.txt",directory,baseline, boardTypeCharFromEEPROM, boardVersion, boardSerNum, channel); 
			 }
			 else
			 {
				sprintf(file_name,"%s/ResidualPedestal_Files/Baseline_%4.3f/ResidualPedestals_Calib_Board_V%d.%d_Ch%d.txt",directory,baseline,boardVersion, boardSerNum, sampicIndex);	
			 }

			 fp = fopen(file_name, "r"); 
  
			 if(fp != NULL)   
			 {
		
				fgets(line, 1024,fp); /* ligne commenaire */  
				fgets(line, 1024,fp); /* ligne vide */
				fgets(line, 1024,fp); /* ligne des piédestaux */


				for(n=0;n<MAX_NB_OF_SAMPLES;n++) /* il y a 64 en une seule ligne */
				{
					if ((sscanf(line+10*n, "%f", &fval)) != 1)
					{
						resultOK = FALSE;
						break;
					}
					else
					{
						crateInfoParams->CrateCalibInfo.CalibValues[feBoard].ResidualPedestalValues[channel][n] = fval; 
					}
				}
		
				fclose(fp);
	  
			 }
			 else
				 resultOK = FALSE;
	
			if(resultOK == TRUE)
			{
   
			  	crateInfoParams->CrateCalibInfo.CalibStatus.ResidualPedestalCalibStatus[feBoard][channel].CalibStatus = CALIB_VALUES_LOADED_FROM_FILE;
				crateInfoParams->CrateCalibInfo.CalibStatus.ResidualPedestalCalibStatus[feBoard][channel].CalibFreqEch = freqEch;
				crateInfoParams->CrateCalibInfo.CalibStatus.ResidualPedestalCalibStatus[feBoard][channel].CalibADCNbOfBits = adcNbOfBits;
			}
			else
			{
			
			  	crateInfoParams->CrateCalibInfo.CalibStatus.ResidualPedestalCalibStatus[feBoard][channel].CalibStatus = CALIB_VALUES_NOT_LOADED;
				crateInfoParams->CrateCalibInfo.CalibStatus.ResidualPedestalCalibStatus[feBoard][channel].CalibFreqEch = freqEch;
				crateInfoParams->CrateCalibInfo.CalibStatus.ResidualPedestalCalibStatus[feBoard][channel].CalibADCNbOfBits = adcNbOfBits;
			
			}
		
		
		}		
	
   }
   
}






/* =================================================================================== */
/* =================================================================================== */

///* ======================================================================= */
//int Convert_VdacRamp_to_Nb_Of_Bits(int feIndex, float vdacRamp)    
///* ======================================================================= */
//{
//
// int nbOfBits;
// 
// if(FeFpgaParams[feIndex].BoardVersion == 1)
// {
//  	if ( vdacRamp < CalibValues.VDAC_Ramp[feIndex][1]) nbOfBits = 11;
// 	else if( vdacRamp >= CalibValues.VDAC_Ramp[feIndex][1] && vdacRamp < CalibValues.VDAC_Ramp[feIndex][2]) nbOfBits = 10;
// 	else nbOfBits = 9;	 
// }
// else
// {
//	 if ( vdacRamp < CalibValues.VDAC_Ramp[feIndex][1]) nbOfBits = 11;
//	 else if( vdacRamp >= CalibValues.VDAC_Ramp[feIndex][1] && vdacRamp < CalibValues.VDAC_Ramp[feIndex][2]) nbOfBits = 10;
//	 else if( vdacRamp >= CalibValues.VDAC_Ramp[feIndex][2] && vdacRamp < CalibValues.VDAC_Ramp[feIndex][3]) nbOfBits = 9;
//	 else nbOfBits = 8;      
// }
// 
// return nbOfBits;
// 
//} 
//	
/* ======================================================================= */
/* ======================================================================= */
/* ======================================================================= */
void Reset_HitInEvent(EventStruct *event,int hitNumber) 
/* ======================================================================= */
{
	
	if(event != NULL)
	{
		
			
		event->Hit[hitNumber].FeBoardIndex = -1;			
		event->Hit[hitNumber].Channel = -1;  
		event->Hit[hitNumber].SampicIndex = -1;  
		event->Hit[hitNumber].ChannelIndex = -1;
		event->Hit[hitNumber].DataSize = 0;
		event->Hit[hitNumber].INLCorrected = FALSE;
		event->Hit[hitNumber].ADCCorrected = FALSE;
		event->Hit[hitNumber].ResidualPedestalCorrected = FALSE;
		event->Hit[hitNumber].Amplitude = -1;
		event->Hit[hitNumber].Baseline = -1;
		event->Hit[hitNumber].Peak = -1;
		event->Hit[hitNumber].TimeIndex = -1;
		event->Hit[hitNumber].TimeInstant = -1;  				//ns 			//ex CFDTimeIndex
		event->Hit[hitNumber].TimeAmplitude = -1; 
		event->Hit[hitNumber].FirstCellTimeStamp = -1; 		// ns		   // Cell0TimeStamp
		
		
	}
	
	
}


/* ======================================================================= */
float Convert_ADCNbOfBitstoVdacRamp (CrateInfoStruct *crateInfoParams, int feBoardIndex, int feFPGAIndex, int nbOfBits)
/* ======================================================================= */
{
   float vdacRamp;
   
	if ( nbOfBits == 11) vdacRamp = crateInfoParams->CrateCalibInfo.CalibValues[feBoardIndex].VDAC_Ramp[feFPGAIndex][0];
 	else if (nbOfBits == 10) vdacRamp = crateInfoParams->CrateCalibInfo.CalibValues[feBoardIndex].VDAC_Ramp[feFPGAIndex][1];    
 	else  if(nbOfBits == 9) vdacRamp = crateInfoParams->CrateCalibInfo.CalibValues[feBoardIndex].VDAC_Ramp[feFPGAIndex][2]; 
 	else vdacRamp = crateInfoParams->CrateCalibInfo.CalibValues[feBoardIndex].VDAC_Ramp[feFPGAIndex][3];
	 

 
 return vdacRamp;
 
	 
}

/* ======================================================================= */
/* ======================================================================= */
 

/* ======================================================================= */
 float Convert_VDac_Reset_To_VBaseline(float vdac) 
 /* ======================================================================= */

 {
	 
	 float result;
	 
	 
	 //result = (vdac -  VBASELINE_TO_VDAC_OFFSET)/ VBASELINE_TO_VDAC_SLOPE;
	 
	 result = VBASELINE_TO_VDAC_OFFSET -  vdac / (MAX_DAC_RAW_VALUE / MAX_DAC_REAL_VALUE_V1_TO_V3);

	 
	  return result;
	 
 }
 
 
 /* ======================================================================= */
 float Convert_TOTFilterWidthToTOTFilterDAC(Boolean wideCap, float filterWidth /* ns*/) 
 /* ======================================================================= */
 { 
 
	 float result;
	 
	 if(wideCap == FALSE)
	 {
		 
		 result = 2.358*pow(filterWidth, -1.119);
		 
		 
	 }
	 else
	 {
		 
		 result = 10.84*pow(filterWidth, -1.004);	 
		 
	 }
	 
	  return result; 
	 
 }		
 
  /* ======================================================================= */
 float Convert_TOTFilterDACtotTOTFilterWidth(Boolean wideCap, float filterDAC /* Volts */) 
 /* ======================================================================= */
 { 
 
	 float result; /* Volts */
	 
	 if(wideCap == FALSE)
	 {
	
		 result = 2.1562*pow(filterDAC, -0.893);	   
		 
	 }
	 else
	 {		
		result = 10.729*pow(filterDAC, -0.996);
		
		 
	 }
	 
	 return result;
	 
	 
 }		
/* ======================================================================= */
/* ======================================================================= */
 
/* ======================================================================= */
/* ======================================================================= */
 float Convert_VBaseline_to_VDac_Reset(float vbaseline)
 /* ======================================================================= */
 {

	 float result;
	 
	 
	 //result = VBASELINE_TO_VDAC_SLOPE*vbaseline + VBASELINE_TO_VDAC_OFFSET; 
	 result = (MAX_DAC_RAW_VALUE / MAX_DAC_REAL_VALUE_V1_TO_V3) * (VBASELINE_TO_VDAC_OFFSET - vbaseline);  
	 
	 return result;
	 
 }
 
/* ======================================================================= */
/* ======================================================================= */ 
 
 
 /* =========================================================================== */							
void Wait_Milliseconds(int nbOfMilliseconds)
/* =========================================================================== */							
{
#if (defined(_WINDOWS) || defined(_WINDOWS_)) 
	Sleep(nbOfMilliseconds);
#else
	usleep(1000 * nbOfMilliseconds);
#endif
}


/* ======================================================================= */
/* ======================================================================= */
 int Get_TOTCurrentRampDacIndex(float totRampCurrentDac)
/* ======================================================================= */
 {

 int dacIndex;

 float default_tot_ramp_dac_0,default_tot_ramp_dac_1,default_tot_ramp_dac_2,default_tot_ramp_dac_3,default_tot_ramp_dac_4 ;

	
 default_tot_ramp_dac_0= DEFAULT_SAMPIC_V3_TOT_RAMP_DAC0; /* Volts pulses < 25 ns */
 default_tot_ramp_dac_1= DEFAULT_SAMPIC_V3_TOT_RAMP_DAC1; /* < 50 ns */
 default_tot_ramp_dac_2= DEFAULT_SAMPIC_V3_TOT_RAMP_DAC2; /* < 100 ns */
 default_tot_ramp_dac_3= DEFAULT_SAMPIC_V3_TOT_RAMP_DAC3; /* < 300 ns */
 default_tot_ramp_dac_4= DEFAULT_SAMPIC_V3_TOT_RAMP_DAC4; /* < 700 ns */
	
	
 if(totRampCurrentDac>  (float)default_tot_ramp_dac_1)
	 dacIndex = 0;
 else if( (totRampCurrentDac > (float)default_tot_ramp_dac_2) && (totRampCurrentDac <= (float)default_tot_ramp_dac_1))
	 dacIndex = 1;
 else if( (totRampCurrentDac > (float)default_tot_ramp_dac_3) && (totRampCurrentDac <= (float)default_tot_ramp_dac_2))
	 dacIndex = 2;
 else if( (totRampCurrentDac > (float)default_tot_ramp_dac_4) && (totRampCurrentDac <= (float)default_tot_ramp_dac_3))
	 dacIndex = 3;
 else if(totRampCurrentDac <= (float)default_tot_ramp_dac_4)
    dacIndex = 4;
	 
	 
return dacIndex;
	 
 }
 
/* ======================================================================= */
/* ======================================================================= */






