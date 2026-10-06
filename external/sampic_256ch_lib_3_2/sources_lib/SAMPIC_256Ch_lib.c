/***************************************************************
 *	File	: 	SAMPIC_256Ch_Lib.c								*
 *																*
 *	Date	:	April 2020   									*
 *																*
 *	Author	:	Dominique Breton	LAL Orsay					*
 *	            Jihane    Maalmi								*
 *																 *
 ***************************************************************/
 //==============================================================================
// Include files
 

#ifdef _WINDOWS
 #include "windows.h"
#else
 #include <malloc.h> 
 #include <inttypes.h>   
#endif

#include <time.h>
#include "lpDevC.h"

#ifdef __LINUX_BUILD
	#include <string.h>
	#include <stdlib.h>
#else
	#include <ansi_c.h>  
#endif


#include "SAMPIC_256Ch_Type.h"
#include "SAMPIC_256Ch_lib.h" 
#include "SAMPIC_256Ch_hardware_core.h"


//==============================================================================
// Constants					  

//==============================================================================
// Types

//==============================================================================
// Static global variables

//==============================================================================
// Static functions

//==============================================================================
// Global variables

//==============================================================================
// Global functions


//============================================================================== //
//============================================================================== //


//============================================================================== //
SAMPIC256CH_ErrCode SAMPIC256CH_ReadCrateConnectionParamsFromFile(char fileName[],  CrateConnectionParamStruct *CrateConnectionParams)
//============================================================================== //
{
																													     
	SAMPIC256CH_ErrCode error; 
	 
	error = SAMPIC256CH_NoFileFound;
	
	return error;
	
	
}


//============================================================================== //
//============================================================================== //

//============================================================================== //
SAMPIC256CH_ErrCode SAMPIC256CH_OpenCrateConnection(CrateConnectionParamStruct crateConnectionParams, CrateInfoStruct *crateInfoParams)
//============================================================================== //
{
	
  int i, nbOfDevices;
  char deviceDescriptor[MAX_DEVICE_DESCR_LENGTH];
  char usbSerNumchar[MAX_SERNUM_LENGTH];
  int  deviceHandle;
 // int  daqHandle[MAX_NB_OF_FE_BOARDS];
  int  boardVersionFromUsbEeprom;
  int  serNum;
  int boardVersionFromUser,serNumFromUser;
  Boolean crateFoundOk = FALSE;
  
  SAMPIC256CH_ErrCode error = SAMPIC256CH_NoCrateConnected;   	
	
  SAMPIC256CH_CloseCrateConnection(crateInfoParams);
	
  if(crateConnectionParams.ConnectionType == USB_CONNECTION)
  {
	   
		nbOfDevices = LPD_GetNumberOfDevs();
		if(nbOfDevices <= 0)
			return SAMPIC256CH_NoCrateConnected; 

		
		for (i = 0; i<nbOfDevices; i++)
		{
			
			LPD_GetDeviceDesc(deviceDescriptor, i);
			
			if(  (strcmp(deviceDescriptor,"CONTROLLER_4FE A")== 0) || (strcmp(deviceDescriptor,"CONTROLLER_4FE")== 0)) 
			{  
			
				LPD_GetDeviceSerNum	(usbSerNumchar, i);
				sscanf(usbSerNumchar,"%x.%d",&boardVersionFromUsbEeprom,&serNum); 
				sscanf(crateConnectionParams.CrateSerNum, "%d.%d", &boardVersionFromUser, &serNumFromUser);
					
			
				if( (strcmp(crateConnectionParams.CrateSerNum, "") == 0) || ( (boardVersionFromUser == boardVersionFromUsbEeprom) && (serNumFromUser == serNum))) 
					crateFoundOk = TRUE;
				
				
				if(crateFoundOk == TRUE)
				{
					
					deviceHandle = LPD_OpenUsb2Device(usbSerNumchar);
			
					if(deviceHandle < 0)
						return  SAMPIC256CH_OpenDeviceError;
				
					if(deviceHandle > 0)						    // success
					{
				
					    if( LPD_Init (deviceHandle, TRUE) == 0)
						{
							LPD_CloseDevice(deviceHandle);
							return SAMPIC256CH_InitDeviceError;
						}
					
						LPD_Init (deviceHandle, TRUE);
					
						error = SAMPIC256CH_Success; 
					
					    LPD_SetTimeouts(deviceHandle,1000,1000);
				
						LPD_SetXferSize(deviceHandle,MAX_BYTES_TO_READ,MAX_BYTES_TO_READ);
						LPD_SetLatencyTimer(deviceHandle,2);				// Attention avec la valeur 1 on est aux limites mettre 2 ms sinon. 
				
						lpdSetMaxLayer(deviceHandle,NB_OF_LAYERS_IN_SYSTEM); 
						LPD_ResetDevice(deviceHandle);
	
						LPD_SetSynchronousMode(deviceHandle, 100);
						LPD_SetFlowControlToHardware(deviceHandle);   		// OBLIGATOIRE en mode synchrone !!! Sinon ça marche pas !!!
						LPD_PurgeBuffers(deviceHandle);
						LPD_EnableInterrupts(deviceHandle, FALSE);
				   
						dsleep(1000); 
					
						crateInfoParams->ConnectionInfo.ConnectionType = USB_CONNECTION;
						crateInfoParams->ConnectionInfo.ControlBoardControlType = CTRL_AND_DAQ;  
					
						strcpy(crateInfoParams->ConnectionInfo.UsbDeviceDescr,deviceDescriptor);
						strcpy(crateInfoParams->ConnectionInfo.UsbSerNum,usbSerNumchar);
						crateInfoParams->CrateBoardsInfo.ControlBoardInfo.BoardSerNum = serNum;
					
						crateInfoParams->CrateBoardsInfo.ControlBoardInfo.BoardVersion = boardVersionFromUsbEeprom; 
						crateInfoParams->ConnectionInfo.CtrlDeviceHandle	= deviceHandle;
				
					
				
					break; 
					
					}
				}
			}
		}
		
		if(crateFoundOk == FALSE)
			return  SAMPIC256CH_CrateNotFound;
  }
  else // UDP
  {
 	if(crateConnectionParams.ControlBoardControlType == CTRL_AND_DAQ)
 	{
	 	  deviceHandle = LPD_OpenUdpDevice	(crateConnectionParams.CtrlIpAddress, crateConnectionParams.CtrlPort);
		  if(deviceHandle < 0)
			 return  SAMPIC256CH_OpenDeviceError;
				
		 if(LPD_Init(deviceHandle, TRUE) == 0)
			 return SAMPIC256CH_OpenDeviceError; 
		 
		 
		 LPD_SetXferSize(deviceHandle,MAX_BYTES_TO_READ,MAX_BYTES_TO_READ);
		 LPD_SetTimeouts(deviceHandle, 1000, 1000);
		 lpdSetMaxLayer(deviceHandle,NB_OF_LAYERS_IN_SYSTEM);
		 
		 lpdSetDAQMaxFrameSize(deviceHandle, MAX_SINGLE_FRAME_SIZE*2);  
		 lpdSetDAQFlowControl(/*DeviceId*/ deviceHandle, /*enableHandCheck*/ FALSE, /*(double)timeoutForRetransmi*/ 20);   
		 
		 error = SAMPIC256CH_Success;
		 
		 crateInfoParams->ConnectionInfo.CtrlDeviceHandle	=  deviceHandle;
		 crateInfoParams->ConnectionInfo.ConnectionType = UDP_CONNECTION;  
		 strcpy(crateInfoParams->ConnectionInfo.CtrlIpAddress, crateConnectionParams.CtrlIpAddress);
		 
		 crateInfoParams->ConnectionInfo.CtrlPort = crateConnectionParams.CtrlPort;
		 
		 lpdSetDAQFlowControl(deviceHandle, /*enableHandshake*/ FALSE, /*timeoutForRetransmit*/ 20 /* ms */);
		 dsleep(100);  
		 LPD_PurgeBuffers(deviceHandle);   
	

		
 	}
	else   // CTRL ONLY, needs to open also DAQ connections
	{
		
		
		
		
	}

	
  }

	if(error == SAMPIC256CH_Success)  error = Check_FeBoardsInSystem (crateInfoParams);
	
	if(error == SAMPIC256CH_Success)  error = Check_SystemType 	  (crateInfoParams);  

	Init_CrateCalibParams(crateInfoParams);     
	
	return error;  	
}

//============================================================================== //
//============================================================================== //


//============================================================================== //
SAMPIC256CH_ErrCode SAMPIC256CH_CloseCrateConnection(CrateInfoStruct *crateInfoParams)
//============================================================================== //
{
	 int i;
	 
	
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;   	  
	
	if(crateInfoParams->ConnectionInfo.CtrlDeviceHandle >= 0)
	{	
		LPD_CloseDevice(crateInfoParams->ConnectionInfo.CtrlDeviceHandle);
		crateInfoParams->ConnectionInfo.CtrlDeviceHandle = -1;
	}
	
	for(i=0; i < crateInfoParams->ConnectionInfo.NbOfDAQConnections; i++)
	{
		LPD_CloseDevice(crateInfoParams->ConnectionInfo.DaqHandle[i]);
		crateInfoParams->ConnectionInfo.DaqHandle[i] = -1;
	}
	
	
	Init_CrateInfoStruct(crateInfoParams);
	
	
	return error;
		

}

//============================================================================== //
SAMPIC256CH_ErrCode SAMPIC256CH_ResetCrate(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams)
//============================================================================== //
{
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;   		
	
	if(error == SAMPIC256CH_Success) error = Run_Reset(crateInfoParams, ALL_FPGAS);
	
	if(error == SAMPIC256CH_Success) error = Reset_ExtTrig(crateInfoParams);   
	
	if(error == SAMPIC256CH_Success) error =  Reset_SAMPICSlowControl(crateInfoParams); 
		
	return error;
}

// Low hardware access function

//============================================================================== //
SAMPIC256CH_ErrCode 	SAMPIC256CH_BusWriteWords		(CrateInfoStruct *crateInfoParams,AccessType_t access_type, FpgaType_t fpga_type, int fe_board_target,int fe_fpga_target, char sub_address, void* buffer,int word_count)
//============================================================================== //
{
	
int bytecount = 0;
int target_path_array[NB_OF_LAYERS_IN_SYSTEM];
 
	if(access_type == CTRL_ACCESS)
	{
		if( fpga_type == CB_CTRL_FPGA)
			target_path_array[0]= -1;
		else if(fpga_type == FEB_CTRL_FPGA)
		{

			if (fe_board_target == -1) // broadcast
			 target_path_array[0]= 0x20;
			else 
			 target_path_array[0]= crateInfoParams->FrontEndBoardsPathIndex[fe_board_target] ;

			target_path_array[1]= -1;
		}
		else   // FEB_FE_FPGA
		{
			if (fe_board_target == -1) // broadcast
			 target_path_array[0]= 0x20;
			else 
			 target_path_array[0]= crateInfoParams->FrontEndBoardsPathIndex[fe_board_target];	
	
			if(fe_fpga_target == -1) // broadcast 
			  target_path_array[1] = 0x20;
			else
			 target_path_array[1] =	fe_fpga_target;
	
			target_path_array[2] = -1; 
	
		}
	
		if(crateInfoParams->ConnectionInfo.CtrlDeviceHandle < 0)
			return SAMPIC256CH_InvalidConnectionHandle;

		bytecount = lpdWrt	(crateInfoParams->ConnectionInfo.CtrlDeviceHandle, target_path_array, sub_address, buffer, word_count);
	
		if(bytecount != word_count)
		{
		
			crateInfoParams->LastCommErrorInfo.AccessType = access_type;
			crateInfoParams->LastCommErrorInfo.FpgaType = fpga_type;  
			crateInfoParams->LastCommErrorInfo.FeBoardTarget = fe_board_target;  
			crateInfoParams->LastCommErrorInfo.FeFpgaTarget = fe_fpga_target;
			crateInfoParams->LastCommErrorInfo.SubAddress = sub_address;  
			crateInfoParams->LastCommErrorInfo.WordCountReq = word_count;
			crateInfoParams->LastCommErrorInfo.WordCountAcq = bytecount;
	
			return SAMPIC256CH_CommunicationWriteError;
		}


		}
		else
		{
			if(fe_board_target < 0)
				return SAMPIC256CH_InvalidParam;

			if(fpga_type != FEB_CTRL_FPGA)
				return SAMPIC256CH_InvalidParam;

			target_path_array[0]= -1;

			if(crateInfoParams->ConnectionInfo.DaqHandle[fe_board_target] < 0)
				return SAMPIC256CH_InvalidConnectionHandle;

			bytecount = lpdWrt	(crateInfoParams->ConnectionInfo.DaqHandle[fe_board_target], target_path_array, sub_address, buffer, word_count);

			if(bytecount != word_count)
			{
			   	crateInfoParams->LastCommErrorInfo.AccessType = access_type;
				crateInfoParams->LastCommErrorInfo.FpgaType = fpga_type;  
				crateInfoParams->LastCommErrorInfo.FeBoardTarget = fe_board_target;  
				crateInfoParams->LastCommErrorInfo.FeFpgaTarget = fe_fpga_target;
				crateInfoParams->LastCommErrorInfo.SubAddress = sub_address;  
				crateInfoParams->LastCommErrorInfo.WordCountReq = word_count;
				crateInfoParams->LastCommErrorInfo.WordCountAcq = bytecount;

				return SAMPIC256CH_CommunicationWriteError;
			}

	}

	return SAMPIC256CH_Success;	
	
	
}

//============================================================================== //
//============================================================================== //


//============================================================================== //
SAMPIC256CH_ErrCode 	SAMPIC256CH_BusCommandReadWords	(CrateInfoStruct *crateInfoParams,AccessType_t access_type, FpgaType_t fpga_type, int fe_board_target,int fe_fpga_target, char sub_address, int word_count)
//============================================================================== //
{
	
	
int bytecount = 0;
int target_path_array[NB_OF_LAYERS_IN_SYSTEM];
unsigned char dummyBuffer[256];
 

	if(access_type == CTRL_ACCESS)
	{
		if( fpga_type == CB_CTRL_FPGA)
			target_path_array[0]= -1;
		else if(fpga_type == FEB_CTRL_FPGA)
		{

			if (fe_board_target == -1) // broadcast
			 target_path_array[0]= 0x20;
			else 
			 target_path_array[0]= crateInfoParams->FrontEndBoardsPathIndex[fe_board_target] ;

			target_path_array[1]= -1;
		}
		else   // FEB_FE_FPGA
		{
			if (fe_board_target == -1) // broadcast
			 target_path_array[0]= 0x20;
			else 
			 target_path_array[0]= crateInfoParams->FrontEndBoardsPathIndex[fe_board_target];	
	
			if(fe_fpga_target == -1) // broadcast 
			  target_path_array[1] = 0x20;
			else
			 target_path_array[1] =	fe_fpga_target;
	
			target_path_array[2] = -1; 
	
		}
	
		if(crateInfoParams->ConnectionInfo.CtrlDeviceHandle < 0)
			return SAMPIC256CH_InvalidConnectionHandle;

		bytecount = lpdRequestForRd	(crateInfoParams->ConnectionInfo.CtrlDeviceHandle, target_path_array, sub_address, dummyBuffer, word_count);


		if(bytecount != word_count)
		{
			crateInfoParams->LastCommErrorInfo.AccessType = access_type;
			crateInfoParams->LastCommErrorInfo.FpgaType = fpga_type;  
			crateInfoParams->LastCommErrorInfo.FeBoardTarget = fe_board_target;  
			crateInfoParams->LastCommErrorInfo.FeFpgaTarget = fe_fpga_target;
			crateInfoParams->LastCommErrorInfo.SubAddress = sub_address;  
			crateInfoParams->LastCommErrorInfo.WordCountReq = word_count;
			crateInfoParams->LastCommErrorInfo.WordCountAcq = bytecount;
	
			
			
			return SAMPIC256CH_CommunicationReadRequestError;
		}

	}
	else
	{
		if(fe_board_target < 0)
			return SAMPIC256CH_InvalidParam;

		if(fpga_type != FEB_CTRL_FPGA)
			return SAMPIC256CH_InvalidParam;

		target_path_array[0]= -1;

		if(crateInfoParams->ConnectionInfo.DaqHandle[fe_board_target] < 0)
			return SAMPIC256CH_InvalidConnectionHandle;

		bytecount = lpdRequestForRd	(crateInfoParams->ConnectionInfo.DaqHandle[fe_board_target], target_path_array, sub_address, dummyBuffer, word_count);

		if( bytecount != word_count)
		{
			crateInfoParams->LastCommErrorInfo.AccessType = access_type;
			crateInfoParams->LastCommErrorInfo.FpgaType = fpga_type;  
			crateInfoParams->LastCommErrorInfo.FeBoardTarget = fe_board_target;  
			crateInfoParams->LastCommErrorInfo.FeFpgaTarget = fe_fpga_target;
			crateInfoParams->LastCommErrorInfo.SubAddress = sub_address;  
			crateInfoParams->LastCommErrorInfo.WordCountReq = word_count;
			crateInfoParams->LastCommErrorInfo.WordCountAcq = bytecount;

			return SAMPIC256CH_CommunicationReadRequestError;
		}

	}

	return SAMPIC256CH_Success;		
	
	
}


//============================================================================== //
SAMPIC256CH_ErrCode 	SAMPIC256CH_BusReadWords		(CrateInfoStruct *crateInfoParams,AccessType_t access_type, FpgaType_t fpga_type, int fe_board_target,int fe_fpga_target, char sub_address, uchar* buffer,int word_count)
//============================================================================== //
{
	
int bytecount = 0;
int target_path_array[NB_OF_LAYERS_IN_SYSTEM];
 
SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;   


	if(access_type == CTRL_ACCESS)
	{
		if( fpga_type == CB_CTRL_FPGA)
			target_path_array[0]= -1;	   // last layer  
		else if(fpga_type == FEB_CTRL_FPGA)
		{

			if (fe_board_target == -1) // braodcast
			 	return SAMPIC256CH_InvalidParam; // pas de boardcast en lecture

	
			target_path_array[0]= crateInfoParams->FrontEndBoardsPathIndex[fe_board_target] ;
			target_path_array[1]= -1;   // last layer  
		}
		else   // FEB_FE_FPGA
		{
			if (fe_board_target == -1) // broadcast
			 return SAMPIC256CH_InvalidParam; // pas de boardcast en lecture  
	
 
			target_path_array[0]= crateInfoParams->FrontEndBoardsPathIndex[fe_board_target];	

			if(fe_fpga_target == -1) // broadcast 
			  return SAMPIC256CH_InvalidParam; // pas de boardcast en lecture 
	
	
			target_path_array[1] =	fe_fpga_target;

			target_path_array[2] = -1;    // last layer

		}

		if(crateInfoParams->ConnectionInfo.CtrlDeviceHandle < 0)
			return SAMPIC256CH_InvalidConnectionHandle;

		bytecount = lpdRd	(crateInfoParams->ConnectionInfo.CtrlDeviceHandle, target_path_array, sub_address, buffer, word_count);


		if(bytecount != word_count) //2nd access trial
		{	
			bytecount = lpdRd	(crateInfoParams->ConnectionInfo.CtrlDeviceHandle, target_path_array, sub_address, buffer, word_count);  
		}

		if(bytecount != word_count)
		{
			crateInfoParams->LastCommErrorInfo.AccessType = access_type;
			crateInfoParams->LastCommErrorInfo.FpgaType = fpga_type;  
			crateInfoParams->LastCommErrorInfo.FeBoardTarget = fe_board_target;  
			crateInfoParams->LastCommErrorInfo.FeFpgaTarget = fe_fpga_target;
			crateInfoParams->LastCommErrorInfo.SubAddress = sub_address;  
			crateInfoParams->LastCommErrorInfo.WordCountReq = word_count;
			crateInfoParams->LastCommErrorInfo.WordCountAcq = bytecount;

			
			
			return SAMPIC256CH_CommunicationReadError; 
		}
	}
	else // DAQ ACCESS
	{
		if(fe_board_target < 0)
			return SAMPIC256CH_InvalidParam;

		if(fpga_type != FEB_CTRL_FPGA)

			return SAMPIC256CH_InvalidParam;

		target_path_array[0]= -1;  // last layer  

		if(crateInfoParams->ConnectionInfo.DaqHandle[fe_board_target] < 0)
			return SAMPIC256CH_InvalidConnectionHandle;

		bytecount = lpdRd (crateInfoParams->ConnectionInfo.DaqHandle[fe_board_target], target_path_array, sub_address, buffer, word_count);

		if(bytecount != word_count)    // 2nd trial access
			bytecount = lpdRd	(crateInfoParams->ConnectionInfo.DaqHandle[fe_board_target], target_path_array, sub_address, buffer, word_count);  
			
		if(bytecount != word_count)
		{
			crateInfoParams->LastCommErrorInfo.AccessType = access_type;
			crateInfoParams->LastCommErrorInfo.FpgaType = fpga_type;  
			crateInfoParams->LastCommErrorInfo.FeBoardTarget = fe_board_target;  
			crateInfoParams->LastCommErrorInfo.FeFpgaTarget = fe_fpga_target;
			crateInfoParams->LastCommErrorInfo.SubAddress = sub_address;  
			crateInfoParams->LastCommErrorInfo.WordCountReq = word_count;
			crateInfoParams->LastCommErrorInfo.WordCountAcq = bytecount;

			
			return SAMPIC256CH_CommunicationReadError;
		}


	}
	
	
	return error;		
}


//============================================================================== //
SAMPIC256CH_ErrCode     SAMPIC256CH_BusReadExtended(int handle, void *buffer, ML_Frame *mf_array, int max_num_bytes, int *nframes)
//============================================================================== //
{

	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;
	
    int bytecount;
  
 	if(handle>0)
	{   
	    bytecount = lpdRdEx(handle, buffer, mf_array, max_num_bytes, nframes); 
		
	}
	else
		return SAMPIC256CH_InvalidHandle;
		

	if(bytecount == 0)
	{   
		*nframes = 0;
		error = SAMPIC256CH_NoFrameRead;
	}
	else if( bytecount >0)
		error = SAMPIC256CH_Success;
	else // bytecount < 0
	{
		*nframes = 0; 
		error =  SAMPIC256CH_CommunicationReadExtendedError;
	}
	
	
	return error;

}

//============================================================================== //
SAMPIC256CH_ErrCode     SAMPIC256CH_DAQBusReadExtended(int handle, void *buffer, ML_Frame *mf_array, int max_num_bytes, int *nframes)
//============================================================================== //
{

	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;
	
    int bytecount;
  
 	if(handle>0)
	{   
	    bytecount = lpdReadDAQStream(handle, buffer, mf_array, max_num_bytes, nframes); 
		
	}
	else
		return SAMPIC256CH_InvalidHandle;
		

	if(bytecount == 0)
	{   
		*nframes = 0;
		error = SAMPIC256CH_NoFrameRead;
	}
	else if( bytecount >0)
		error = SAMPIC256CH_Success;
	else // bytecount < 0
	{
		*nframes = 0; 
		error =  SAMPIC256CH_CommunicationReadExtendedError;
	}
	
	
	return error;

}


//=========================  EEPROM ACCESS Functions  ================================ //
SAMPIC256CH_ErrCode		SAMPIC256CH_Read_EEPROM(CrateInfoStruct *crateInfoParams,EepromSourceType_t eepromSrce, int fe_board_target,int fe_fpga_target, char sub_address, unsigned char* buffer,int nbOfBytes)    
//======================================================================================= // 
{
	
SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;

unsigned char charData[2], eepromReg = 0x00;
char address, eepromRegAddress;
Boolean eepromPage;
unsigned short eepromAddress;
FpgaType_t fpga_type;

  
    if(sub_address > 0x01FFFF)
	   return  SAMPIC256CH_InvalidParam;
		   
    eepromPage = ((sub_address & 0x10000) !=0);
  
	// Fabrication de l'adresse à laquelle on veut écrire
	if(eepromSrce == CB_CTRL_EEPROM)
	{
		fpga_type = CB_CTRL_FPGA;
		eepromRegAddress = ad_control_board_ctrl_eeprom_reg;
		address = ad_control_board_ctrl_eeprom_access;
			
	}
	else if(eepromSrce == FEB_CTRL_EEPROM)
	{
		fpga_type = FEB_CTRL_FPGA;
		eepromRegAddress = ad_fe_board_ctrl_eeprom_reg;
		address = ad_fe_board_ctrl_eeprom_access;
	}
	else if(eepromSrce == FEB_FE_EEPROM)
	{
		fpga_type = FEB_FE_FPGA;
		eepromRegAddress = ad_fe_board_fe_eeprom_reg;
		address = ad_fe_board_fe_eeprom_access;
	}
		


	eepromAddress =  (unsigned short)(sub_address & 0xFFFF);
	
	
	// Préparation du tableau des bytes à écrire
	
    charData[0] = (unsigned char)((eepromAddress & 0xFF00)>> 8);
    charData[1] = (unsigned char)(eepromAddress & 0x00FF); 

	eepromReg = 0x01 + (eepromPage << 1);   // Wp non valide
	
	if(error == SAMPIC256CH_Success) error = SAMPIC256CH_BusWriteWords	(crateInfoParams,CTRL_ACCESS, fpga_type, fe_board_target,fe_fpga_target, eepromRegAddress, &eepromReg,1);
	
	if(error == SAMPIC256CH_Success) error = SAMPIC256CH_BusWriteWords	(crateInfoParams,CTRL_ACCESS, fpga_type, fe_board_target,fe_fpga_target, address, &charData,2);// 2 bytes pour préparer l'adresse interne de l'EEPROM 

	//Wait_Delay(0.01); //peut on le commenter??
	
	// demande de lecture d'un bloc
	if(error == SAMPIC256CH_Success) error = SAMPIC256CH_BusReadWords (crateInfoParams,CTRL_ACCESS, fpga_type,fe_board_target,fe_fpga_target, address, buffer, nbOfBytes); 

	return error;

	
}


//======================================================================================= // 
SAMPIC256CH_ErrCode 	SAMPIC256CH_Read_ControlBoard_EPROM_Info 					(CrateInfoStruct *crateInfoParams)
//======================================================================================= // 
{
SAMPIC256CH_ErrCode error;
	error	= Read_ControlBoard_EPROM_Info (crateInfoParams);	
return error;
}

//======================================================================================= // 
SAMPIC256CH_ErrCode 	SAMPIC256CH_Read_FeBoard_CTRL_EEPROM_Info 					(CrateInfoStruct *crateInfoParams,int feBoardIndex)
//======================================================================================= // 
{
SAMPIC256CH_ErrCode error;
	error	= Read_FeBoard_CTRL_EEPROM_Info (crateInfoParams,feBoardIndex);	
return error;
	
}

//======================================================================================= // 
SAMPIC256CH_ErrCode 	SAMPIC256CH_Read_FrontEndBlock_EEPROM_Info 					(CrateInfoStruct *crateInfoParams,int feBoardIndex, int feBlockIndex)
//======================================================================================= // 
{
SAMPIC256CH_ErrCode error;
	error	= Read_FrontEndBlock_EEPROM_Info (crateInfoParams,feBoardIndex,feBlockIndex);	
return error;	
	
}



//=========================  CALIB FILES Functions  ================================ //

//============================================================================== // 
SAMPIC256CH_ErrCode 	SAMPIC256CH_Write_EEPROM(CrateInfoStruct *crateInfoParams,EepromSourceType_t eepromSrce,int fe_board_target,int fe_fpga_target, char sub_address, unsigned char* buffer,int nbOfBytes)
//============================================================================== // 
{
	
unsigned char bufToWrite[EEPROM_PAGE_SIZE + 2];
int i; 
char address, eepromRegAddress;
unsigned char eepromReg = 0x00;
unsigned char *tmpbuf;
Boolean eepromPage;
FpgaType_t fpga_type;  
unsigned short eepromAddress;

SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;   


    if(sub_address > 0x01FFFF)
	   return  SAMPIC256CH_InvalidParam;
		   
    eepromPage = ((sub_address & 0x10000) !=0); 
  
    tmpbuf = (unsigned char*)buffer;
  
	// Fabrication de l'adresse à laquelle on veut écrire
	if(eepromSrce == CB_CTRL_EEPROM)
	{
		fpga_type = CB_CTRL_FPGA;
		eepromRegAddress = ad_control_board_ctrl_eeprom_reg;
		address = ad_control_board_ctrl_eeprom_access;
			
	}
	else if(eepromSrce == FEB_CTRL_EEPROM)
	{
		fpga_type = FEB_CTRL_FPGA;
		eepromRegAddress = ad_fe_board_ctrl_eeprom_reg;
		address = ad_fe_board_ctrl_eeprom_access;
	}
	else if(eepromSrce == FEB_FE_EEPROM)
	{
		fpga_type = FEB_FE_FPGA;
		eepromRegAddress = ad_fe_board_fe_eeprom_reg;
		address = ad_fe_board_fe_eeprom_access;
	}
		
   eepromAddress =  (unsigned short)(sub_address & 0xFFFF);

	// Préparation du tableau des bytes à écrire
	
   bufToWrite[0] = (unsigned char)((eepromAddress & 0xFF00)>> 8);
   bufToWrite[1] = (unsigned char)(eepromAddress & 0x00FF); 
   
   for ( i = 0; i< nbOfBytes; i++)
   { 	
	   bufToWrite[i+2]=  tmpbuf[i];
	
   }		
	
	//	Octets qu'on veut écrire
	
	eepromReg = 0x00 + (eepromPage << 1);   // Wp valide 
	
	if(error == SAMPIC256CH_Success) error = SAMPIC256CH_BusWriteWords	(crateInfoParams,CTRL_ACCESS, fpga_type, fe_board_target,fe_fpga_target, eepromRegAddress, &eepromReg,1);
	if(error == SAMPIC256CH_Success) error = SAMPIC256CH_BusWriteWords	(crateInfoParams,CTRL_ACCESS, fpga_type, fe_board_target,fe_fpga_target, address, bufToWrite,nbOfBytes + 2); 

		
	dsleep(200);
	
	eepromReg = 0x01;
	
	if(error == SAMPIC256CH_Success) error = SAMPIC256CH_BusWriteWords	(crateInfoParams,CTRL_ACCESS, fpga_type, fe_board_target,fe_fpga_target, eepromRegAddress, &eepromReg,1);

	
	return error;
	
	
}

//============================================================================== // 
//============================================================================== //


//============================================================================== //
SAMPIC256CH_ErrCode SAMPIC256CH_LoadAllCalibValuesFromFiles(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, char directory[MAX_PATHNAME_LENGTH])
//============================================================================== //			 
{
	
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;
	Boolean allCalibFilesLoaded = TRUE;
	int feBoard, channel;
	Boolean allResidualPedestalsFilesLoaded = TRUE;
	Boolean allTimeINLFilesLoaded = TRUE;
	
	

	Init_CrateCalibParams(crateInfoParams);
	
	strcpy(crateInfoParams->CrateCalibInfo.LastCalibDirectory, directory); 
	
	// Read calib values from files ....
	
	if(crateInfoParams->SystemType == SAMPET_SYSTEM)
	 Read_ADCRampCalibValues_From_Files (crateInfoParams,crateParams, directory); 
	else
		allCalibFilesLoaded &= Read_ADCRampCalibValues_From_Files 				(crateInfoParams,crateParams, directory); 
	
	allCalibFilesLoaded &= Read_ADCLinearityCalibValues_From_Files			(crateInfoParams,crateParams, directory); 
	allCalibFilesLoaded &= Read_INLValues_From_Files						(crateInfoParams,crateParams, directory);
	
 	if(crateInfoParams->SystemType == SAMPET_SYSTEM)
	 Read_InternalTriggerThresholdOffset_From_Files	(crateInfoParams,crateParams, directory);
	else
	 allCalibFilesLoaded &= Read_InternalTriggerThresholdOffset_From_Files	(crateInfoParams,crateParams, directory);  
	
	
	allCalibFilesLoaded &= Read_TOTCalibValues_From_Files	(crateInfoParams,crateParams, directory);

		
	if( allCalibFilesLoaded == FALSE)
		error = SAMPIC256CH_AtLeastOneCalibFileNotFound;   	

	for(feBoard = 0; feBoard < crateInfoParams->NbOfFeBoards; feBoard++)
	{
		for(channel = 0; channel < NB_OF_CHANNELS_IN_FE_BOARD; channel++)
		{
			if(crateInfoParams->CrateCalibInfo.CalibStatus.ResidualPedestalCalibStatus[feBoard][channel].CalibStatus == CALIB_VALUES_NOT_LOADED)
				allResidualPedestalsFilesLoaded = FALSE;
		}

		for(channel = 0; channel < NB_OF_CHANNELS_IN_FE_BOARD; channel++)
		{
			if(crateInfoParams->CrateCalibInfo.CalibStatus.TimeINLCalibStatus[feBoard][channel].CalibStatus == CALIB_VALUES_NOT_LOADED)
			allTimeINLFilesLoaded = FALSE;
	
		}
		
	}
	
	if(crateInfoParams->CrateCalibInfo.CalibStatus.ADCLinearityCrateCalibStatus.CalibStatus == CALIB_VALUES_LOADED_FROM_FILE)
	{
		crateParams->CommonParams.ADCLinearityCorrection = TRUE;
	}
	else
	{
		crateParams->CommonParams.ADCLinearityCorrection = FALSE;
	
	}
	
	if(allResidualPedestalsFilesLoaded == TRUE)
	  crateParams->CommonParams.ResidualPedestalCorrection = TRUE;
	else
	  crateParams->CommonParams.ResidualPedestalCorrection = FALSE;     	
	
	if(allTimeINLFilesLoaded == TRUE)
	  crateParams->CommonParams.INLCorrection = TRUE;
	else
	  crateParams->CommonParams.INLCorrection = FALSE;     	
	
	return error;
}


//============================================================================== //
SAMPIC256CH_ErrCode  	SAMPIC256CH_ReloadInternalTriggerThresholdOffsetsFromFiles	(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, char directory[MAX_PATHNAME_LENGTH], Boolean *calibFilesLoadedFromSpecificBaselineFolder)
//============================================================================== //
{
 int result;
 
	result = Read_InternalTriggerThresholdOffset_From_Files	(crateInfoParams,crateParams, directory);  	
	
	*calibFilesLoadedFromSpecificBaselineFolder = FALSE;
	
	if(result == 2)
			*calibFilesLoadedFromSpecificBaselineFolder = TRUE; 
	if(result > 0)
		return SAMPIC256CH_Success;
	else
		return SAMPIC256CH_AtLeastOneCalibFileNotFound;
	
}
//=========================  CONTROL Functions  ================================ //
//============================================================================== //
SAMPIC256CH_ErrCode SAMPIC256CH_SetDefaultParameters(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams)
//============================================================================== //
{
	SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success;         
	
	
	Init_CrateParamStruct (crateInfoParams, crateParams);
	
	
	//SI5332
	if(crateInfoParams->CrateBoardsInfo.ControlBoardInfo.Si5332IsPresent == TRUE)
	{
		errCode =	Load_ControlBoardSi5332DefaultConfig(crateInfoParams);
	}
	
//	if(errCode == SAMPIC256CH_Success) errCode = Load_FeBoardSi5332DefaultConfig (crateInfoParams, ALL_FE_BOARDs);  
		
	if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_ResetCrate(crateInfoParams, crateParams); 

	if(errCode == SAMPIC256CH_Success) errCode = Load_HardwareSetup(crateInfoParams, crateParams, ALL_REGS, ALL_FE_BOARDs, ALL_FE_BOARDs_FE_FPGAs, ALL_CHANNELs);  
			  
	if(errCode == SAMPIC256CH_Success) errCode = Load_DACs(crateInfoParams, crateParams, ALL_VDACS, ALL_FE_BOARDs, ALL_FE_BOARDs_FE_FPGAs);
	
	if(errCode == SAMPIC256CH_Success) errCode = Reset_SAMPICDLL (crateInfoParams,crateParams);
	
	
	return errCode;
	
}

//============================================================================== //
SAMPIC256CH_ErrCode SAMPIC256CH_ReLoadAllParameters(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams)
//============================================================================== //
{

	SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success;           
	
	if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_ResetCrate(crateInfoParams, crateParams); 
	
	
	/*Stop_Run();
	
	Run_Reset(ALL_FPGAS);
	
	Reset_SAMPICSlowControl();
	

		
		
	//reset sampic ??     
	Reset_SAMPICDLL();
		*/  
 	if(errCode == SAMPIC256CH_Success) errCode = Load_HardwareSetup(crateInfoParams, crateParams, ALL_REGS, ALL_FE_BOARDs, ALL_FE_BOARDs_FE_FPGAs, ALL_CHANNELs);  
			  
	if(errCode == SAMPIC256CH_Success) errCode = Load_DACs(crateInfoParams, crateParams, ALL_VDACS, ALL_FE_BOARDs, ALL_FE_BOARDs_FE_FPGAs);
	
	if(errCode == SAMPIC256CH_Success) errCode = Reset_SAMPICDLL (crateInfoParams,crateParams);
	
	return errCode;
	
}


//============================================================================== //
SAMPIC256CH_ErrCode SAMPIC256CH_CheckCrateFirmwareVersions(CrateInfoStruct *crateInfoParams)
//============================================================================== //
{
	
	SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success; 
	int dummy = 0;
	unsigned char data = 0x0;
	unsigned char sub_address; 
	int feBoardIndex, feFpgaIndex;
	
	// CONTROL BOARD FPGA FIRMWARE

	sub_address = ad_control_fpga_version;			  
	
	if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_BusReadWords (crateInfoParams,CTRL_ACCESS, CB_CTRL_FPGA, dummy,dummy, sub_address, &data,1);
	
	if(errCode == SAMPIC256CH_Success)  
	{
		crateInfoParams->CrateBoardsInfo.ControlBoardInfo.FirmwareVersion.BoardVersion =  (data&0xF0)>>4;
		crateInfoParams->CrateBoardsInfo.ControlBoardInfo.FirmwareVersion.FPGAVersion = (data&0xF);  
		
		if(crateInfoParams->CrateBoardsInfo.ControlBoardInfo.FirmwareVersion.BoardVersion >= 3)
		   crateInfoParams->CrateBoardsInfo.ControlBoardInfo.Si5332IsPresent = TRUE;
		else
		   crateInfoParams->CrateBoardsInfo.ControlBoardInfo.Si5332IsPresent = FALSE; 	

		
	}
	
    sub_address = ad_control_fpga_evolution;		  

	if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_BusReadWords (crateInfoParams ,CTRL_ACCESS, CB_CTRL_FPGA, dummy,dummy, sub_address, &data,1);

	if(errCode == SAMPIC256CH_Success)  
	{	
		crateInfoParams->CrateBoardsInfo.ControlBoardInfo.FirmwareVersion.FPGAEvolution  = data;
		
		
	}
	
	for(feBoardIndex = 0; feBoardIndex < crateInfoParams->NbOfFeBoards; feBoardIndex++)
	{
		// FE BOARD CONTROL FPGA FIRMWARE
		sub_address = ad_fe_board_ctrl_FPGA_version;
		
		if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_BusReadWords (crateInfoParams ,CTRL_ACCESS, FEB_CTRL_FPGA, feBoardIndex,dummy, sub_address, &data,1);
		
		if(errCode == SAMPIC256CH_Success)  
		{
			
			crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoardIndex].ControlFpgaFirmwareVersion.BoardVersion = (data&0xF0)>>4;   
			crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoardIndex].ControlFpgaFirmwareVersion.FPGAVersion = (data&0xF);  
		}
		
		 sub_address = ad_fe_board_ctrl_FPGA_evolution;		  
		
		if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_BusReadWords (crateInfoParams ,CTRL_ACCESS, FEB_CTRL_FPGA, feBoardIndex,dummy, sub_address, &data,1);
		
		if(errCode == SAMPIC256CH_Success)  
		{
		   crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoardIndex].ControlFpgaFirmwareVersion.FPGAEvolution = data;     
		}
		
		// FE BOARD FE FPGA FIRMWARE

		for( feFpgaIndex = 0; feFpgaIndex < NB_OF_FE_FPGAS_IN_FE_BOARD; feFpgaIndex++)
		{
			
			sub_address = ad_fe_board_fe_fpga_version; 
			if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_BusReadWords (crateInfoParams, CTRL_ACCESS, FEB_FE_FPGA, feBoardIndex,feFpgaIndex, sub_address, &data,1);
		
			if(errCode == SAMPIC256CH_Success)
			{
			
					crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoardIndex].FeFpgaFirmwareVersion[feFpgaIndex].BoardVersion = (data&0xF0)>>4;  
					crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoardIndex].FeFpgaFirmwareVersion[feFpgaIndex].FPGAVersion = (data&0xF);  
 		
			}
			
		
			// FEB FE FPGA
			sub_address = ad_fe_board_fe_fpga_evolution; 
			if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_BusReadWords (crateInfoParams, CTRL_ACCESS, FEB_FE_FPGA, feBoardIndex,feFpgaIndex, sub_address, &data,1);
		
			if(errCode == SAMPIC256CH_Success)  
		    {
				crateInfoParams->CrateBoardsInfo.FeBoardInfo[feBoardIndex].FeFpgaFirmwareVersion[feFpgaIndex].FPGAEvolution = data; 
			}
		
		}

	}


	return errCode;	
	
	
}


//============================================================================== //
SAMPIC256CH_ErrCode SAMPIC256CH_SetCrateCorrectionLevels(CrateInfoStruct *crateInfoParams,CrateParamStruct *crateParams, Boolean adcLinearityCorection, Boolean timeINLCorrection, Boolean residualPedestalCorrection)
//============================================================================== //
{
 SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success;

  if(adcLinearityCorection == TRUE)
  {
	  if(crateInfoParams->CrateCalibInfo.CalibStatus.ADCLinearityCrateCalibStatus.CalibStatus != CALIB_VALUES_NOT_LOADED)
	  	crateParams->CommonParams.ADCLinearityCorrection = TRUE;
	  else
	  {
		  crateParams->CommonParams.ADCLinearityCorrection = FALSE; 
		  errCode = SAMPIC256CH_ADCLinearityCalibValuesNotLoaded;
		  
	  }
  }
  else
	crateParams->CommonParams.ADCLinearityCorrection = FALSE;
  
  crateParams->CommonParams.INLCorrection = timeINLCorrection;
	
  crateParams->CommonParams.ResidualPedestalCorrection = residualPedestalCorrection;
	
  return errCode;
	  
   
}

//============================================================================== //
SAMPIC256CH_ErrCode SAMPIC256CH_GetCrateCorrectionLevels(CrateInfoStruct *crateInfoParams,CrateParamStruct *crateParams, Boolean *adcLinearityCorection, Boolean *timeINLCorrection, Boolean *residualPedestalCorrection)
//============================================================================== //
{
 
	SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success; 
	
	*adcLinearityCorection = crateParams->CommonParams.ADCLinearityCorrection;
	*timeINLCorrection = crateParams->CommonParams.INLCorrection;
	*residualPedestalCorrection = crateParams->CommonParams.ResidualPedestalCorrection;
	
	
	return errCode; 	
}


/* =================================================================================== */
SAMPIC256CH_ErrCode Get_SystemADCNbOfBits(CrateParamStruct *crateParams, int *adcNbOfBits)
/* =================================================================================== */
{
	
	
	SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success;
	
	*adcNbOfBits = crateParams->CommonParams.ADCNbOfBits;
	
	return errCode;
	
}
/* =================================================================================== */
SAMPIC256CH_ErrCode Set_SystemADCNbOfBits(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int adcNbOfBits)
/* =================================================================================== */
{
	SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success;           

	int dummy = 0;
	int feBoardIndex, feFPGAIndex;
	
	crateParams->CommonParams.ADCNbOfBits = adcNbOfBits;
	
	for(feBoardIndex = 0; feBoardIndex < crateInfoParams->NbOfFeBoards; feBoardIndex++)
	{
		
		for (feFPGAIndex = 0; feFPGAIndex < NB_OF_FE_FPGAS_IN_FE_BOARD; feFPGAIndex++)
		{
	
			crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[feFPGAIndex].SAMPICIndividualParams.Vdac_ramp = Convert_ADCNbOfBitstoVdacRamp (crateInfoParams, feBoardIndex, feFPGAIndex,adcNbOfBits);      
	
		}
	 }

	Build_FeBoardsSampicsConfigReg6(crateParams);
	
    errCode = Load_HardwareSetup(crateInfoParams, crateParams, FEB_SAMPIC_SC_REG6, ALL_FE_BOARDs, ALL_FE_BOARDs_FE_FPGAs, dummy);  
			  
	
	return errCode;
	
}


/* =================================================================================== */
SAMPIC256CH_ErrCode 	SAMPIC256CH_SetChannelMode(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int channel, Boolean mode  /* 1 = channel enabled, 0 = channel disabled */ ) 
/* =================================================================================== */
{
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;   
	int startChannelIndex, endChannelIndex, channelIdx;
	int feboardIdx, startFeIndex,endFeIndex;  
	int feFpgaIndex, sampicChannelIndex;

	
	if(feBoard >=  crateInfoParams->NbOfFeBoards)
		return SAMPIC256CH_InvalidParam;
	
	if(channel >= NB_OF_CHANNELS_IN_FE_BOARD)
		return SAMPIC256CH_InvalidParam; 
															   
	if(feBoard < 0)
	{
		
		startFeIndex = 0;
		endFeIndex = crateInfoParams->NbOfFeBoards -1;
	}
	else
	{
		
		startFeIndex = feBoard;
		endFeIndex = feBoard;
	}
  	

	if(channel < 0)
	{
	
		startChannelIndex = 0;
		endChannelIndex = NB_OF_CHANNELS_IN_FE_BOARD -1;
	
		
	}
	else
	{
	    startChannelIndex = channel;
		endChannelIndex = channel;
		
	}
	
	for(feboardIdx = startFeIndex; feboardIdx <= endFeIndex; feboardIdx++)
	{	
	  for(channelIdx = startChannelIndex; channelIdx <= endChannelIndex; channelIdx++)
	  {
			feFpgaIndex = (int)(channelIdx/NB_OF_CHANNELS_IN_SAMPIC);
			sampicChannelIndex =   channelIdx % NB_OF_CHANNELS_IN_SAMPIC;
  
			crateParams->FeBoardParams[feboardIdx].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableTriggerChannel[sampicChannelIndex] = mode;

			
	  }
	}
	
	Build_FeBoardsSampicsChannelRegs(crateParams);
	
	if(channel < 0)
	{
		feFpgaIndex = ALL_FE_BOARDs_FE_FPGAs;
		sampicChannelIndex = ALL_CHANNELs;
	}
	else
	{
		feFpgaIndex = (int)(channel/NB_OF_CHANNELS_IN_SAMPIC);
		sampicChannelIndex =   channel % NB_OF_CHANNELS_IN_SAMPIC;
  	 		
	}
		
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_SAMPIC_SC_CHANNEL_REG, feBoard, feFpgaIndex, sampicChannelIndex);


	
	return error;      	   
}

/* =================================================================================== */
SAMPIC256CH_ErrCode 	SAMPIC256CH_GetChannelMode(CrateParamStruct *crateParams, int feBoard, int channel, Boolean *mode)  
/* =================================================================================== */
{
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;   
	int feFpgaIndex, channelIndex;
	
	if(feBoard >= MAX_NB_OF_FE_BOARDS)
		return SAMPIC256CH_InvalidParam;

	if(channel >= NB_OF_CHANNELS_IN_FE_BOARD)
		return SAMPIC256CH_InvalidParam; 

	if(feBoard < 0 || channel < 0)
		return SAMPIC256CH_InvalidParam;
	
	feFpgaIndex = (int)(channel/NB_OF_CHANNELS_IN_SAMPIC);
	channelIndex =   channel % NB_OF_CHANNELS_IN_SAMPIC;
	
	*mode = crateParams->FeBoardParams[feBoard].FeFpgaParams[feFpgaIndex].SAMPICIndividualParams.EnableTriggerChannel[channelIndex];
	
	
	
	return error;      	   
}


/* =================================================================================== */
SAMPIC256CH_ErrCode 	SAMPIC256CH_SetBaselineForCalib(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, float vRefBaseline /* in Volts between 0 and 1.6V */)                       
/* =================================================================================== */
{
	
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;               	
	
	int startSampicIndex, endSampicIndex, sampicIdx;
	int feboardIdx, startFeIndex,endFeIndex;  
	
  
	if( (vRefBaseline < 0) || (vRefBaseline > 1.6)) 
	{
		error = SAMPIC256CH_OutOfRange;
		return error;
	}
	
	if(feBoard >=  crateInfoParams->NbOfFeBoards)
		return SAMPIC256CH_InvalidParam;
	

	if(feBoard < 0)
	{
		startFeIndex = 0;
		endFeIndex = crateInfoParams->NbOfFeBoards -1;
	}
	else
	{
		
		startFeIndex = feBoard;
		endFeIndex = feBoard;
	}
  	

	if(sampicIndex < 0)
	{
	
		startSampicIndex = 0;
		endSampicIndex = NB_OF_SAMPICS_IN_FE_BOARD -1;
	
		
	}
	else
	{
	    startSampicIndex = sampicIndex;
		endSampicIndex = sampicIndex;
		
	}
	
	
	for(feboardIdx = startFeIndex; feboardIdx <=  endFeIndex;  feboardIdx++)
	{
		
	  for(sampicIdx = startSampicIndex; sampicIdx <= endSampicIndex; sampicIdx++)
	  {
		  
		  	crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.VBaseline = vRefBaseline; 
			crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.Vdac_reset = Convert_VBaseline_to_VDac_Reset(vRefBaseline); 
		
		    if(error == SAMPIC256CH_Success) error = Load_DACs(crateInfoParams, crateParams, FEB_VDAC_RESET, feboardIdx, sampicIdx); 
			
		  
	  }
		
	}
	
	return error;  	
	
}
/* =================================================================================== */
/* =================================================================================== */


/* =================================================================================== */
SAMPIC256CH_ErrCode 	SAMPIC256CH_SetBaselineReference(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, float vRefBaseline /* in Volts between 0 and 1.6V */)                       
/* =================================================================================== */
{
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;               	
	
	int startSampicIndex, endSampicIndex, sampicIdx;
	int feboardIdx, startFeIndex,endFeIndex;  
	
  
	if( (vRefBaseline < 0) || (vRefBaseline > 1.6)) 
	{
		error = SAMPIC256CH_OutOfRange;
		return error;
	}
	
	if(feBoard >=  crateInfoParams->NbOfFeBoards)
		return SAMPIC256CH_InvalidParam;
	

	if(feBoard < 0)
	{
		startFeIndex = 0;
		endFeIndex = crateInfoParams->NbOfFeBoards -1;
	}
	else
	{
		
		startFeIndex = feBoard;
		endFeIndex = feBoard;
	}
  	

	if(sampicIndex < 0)
	{
	
		startSampicIndex = 0;
		endSampicIndex = NB_OF_SAMPICS_IN_FE_BOARD -1;
	
		
	}
	else
	{
	    startSampicIndex = sampicIndex;
		endSampicIndex = sampicIndex;
		
	}
	
	
	for(feboardIdx = startFeIndex; feboardIdx <=  endFeIndex;  feboardIdx++)
	{
		
	  for(sampicIdx = startSampicIndex; sampicIdx <= endSampicIndex; sampicIdx++)
	  {
		  
		  	crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.VBaseline =  vRefBaseline;
			crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.Vdac_reset = Convert_VBaseline_to_VDac_Reset(vRefBaseline);
		
		    if(error == SAMPIC256CH_Success) error = Load_DACs(crateInfoParams, crateParams, FEB_VDAC_RESET, feboardIdx, sampicIdx); 
			
		  
	  }
		
	}
	
	

// Updating Individual Thresholds 
	
	Compute_SAMPICIndividualInternalThresholds(crateInfoParams, crateParams);
	
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_SAMPIC_SC_CHANNEL_REG, feBoard, sampicIndex, ALL_CHANNELs); 
	
	
	return error;      	   
}


/* =================================================================================== */
SAMPIC256CH_ErrCode 	SAMPIC256CH_GetBaselineReference(CrateParamStruct *crateParams, int feBoard, int sampicIndex, float *vRefBaseline /* in Volts */)
/* =================================================================================== */
{
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;               	
		
	if(feBoard >= MAX_NB_OF_FE_BOARDS)
		return SAMPIC256CH_InvalidParam;
	
	if(sampicIndex >= NB_OF_SAMPICS_IN_FE_BOARD)
		return SAMPIC256CH_InvalidParam; 

	if(feBoard < 0 || sampicIndex < 0)
		return SAMPIC256CH_InvalidParam;

	*vRefBaseline =	crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.VBaseline;
	
	return error;      	   
}


/* =================================================================================== */
SAMPIC256CH_ErrCode     SAMPIC256CH_SetSamplingFrequency(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int  samplingFreq, Boolean useExternalClock)   /* this function changes the sampling frequency and also re-load all calibration values from files */
/* =================================================================================== */
{
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;
	
	int dummy = 0;
	int feBoard, sampicIndex;
	SampicDLLModeType_t dllMode;
	
	float externalClockFrequency;
	
	externalClockFrequency =  (float)(samplingFreq/MAX_NB_OF_SAMPLES);
	
	if( useExternalClock == TRUE && (externalClockFrequency < 20.0 && externalClockFrequency > 133.0))
			return SAMPIC256CH_InvalidParam;
	
	if(useExternalClock == FALSE && ( samplingFreq != SAMPIC_1_6_GS 
									&& samplingFreq != SAMPIC_2_133_GS
									&& samplingFreq != SAMPIC_3_2_GS 
									&& samplingFreq != SAMPIC_4_252_GS 
									&& samplingFreq != SAMPIC_6_4_GS 
									&& samplingFreq != SAMPIC_8_512_GS)
									)
			 return SAMPIC256CH_InvalidParam; 
	
	crateParams->CommonParams.UseExternalClock = useExternalClock;
	
	crateParams->CommonParams.FreqEch = samplingFreq;

	Build_ControlBoardClockReg(crateParams);

	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, CTRLB_CLOCK_REG, dummy, dummy, dummy); 

	Build_ControlBoardControlReg2(crateParams);
   	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, CTRLB_CONTROL_REG2, dummy, dummy, dummy); 

	Build_FeBoardsFeFpgasControlReg(crateParams); 
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams,crateParams, FEB_FE_CONTROL_REG, ALL_FE_BOARDs, ALL_FE_BOARDs_FE_FPGAs, dummy); 
	
	if( samplingFreq >= 6400)  
		dllMode = DLL_FAST;
	else if( samplingFreq >= 4252)
		dllMode = DLL_MEDIUM; 
	else if(samplingFreq >= 1500)     
		dllMode = DLL_SLOW;
	else
		dllMode = DLL_ULTRA_SLOW; 
	
	for(feBoard = 0; feBoard < MAX_NB_OF_FE_BOARDS; feBoard++)
	{
		for(sampicIndex = 0; sampicIndex < NB_OF_SAMPICS_IN_FE_BOARD; sampicIndex++)
		{
			
			crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.DLLMode = dllMode; 
			
			if(dllMode != DLL_FAST)
			 crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.Vdac_DLL = 1.1;	 // sinon certaines puces peuvent ne pas fonctionner!
			else
			 crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.Vdac_DLL = 1.0; 	 
			
		}
		
	}
   				   
	
	Build_FeBoardsSampicsConfigReg1(crateParams);

	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_SAMPIC_SC_REG1, ALL_FE_BOARDs, ALL_FE_BOARDs_FE_FPGAs, dummy); 
	
	if(error == SAMPIC256CH_Success) error =  Load_DACs(crateInfoParams, crateParams,  FEB_VDAC_DLL,  ALL_FE_BOARDs, ALL_FE_BOARDs_FE_FPGAs); 
	    				    
	if(error == SAMPIC256CH_Success) error = Reset_SAMPICDLL (crateInfoParams,crateParams);

	// PV say
  /* Change
    if(error == SAMPIC256CH_Success) error =
    SAMPIC256CH_LoadAllCalibValuesFromFiles(crateInfoParams, crateParams,
    crateInfoParams->CrateCalibInfo.LastCalibDirectory);        
  */
  // by
  char thedirectory[MAX_PATHNAME_LENGTH];
  strcpy(thedirectory, crateInfoParams->CrateCalibInfo.LastCalibDirectory);

  /*if(error == SAMPIC256CH_Success) error =*/
  SAMPIC256CH_LoadAllCalibValuesFromFiles(crateInfoParams, crateParams, thedirectory);  
  // PV has said

	
	return error;      	   
}

/* =================================================================================== */
SAMPIC256CH_ErrCode     SAMPIC256CH_GetSamplingFrequency(CrateParamStruct *crateParams, int  *samplingFreq, Boolean *useExternalClock)
/* =================================================================================== */
{
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;  
	
	*samplingFreq = crateParams->CommonParams.FreqEch;
	*useExternalClock = crateParams->CommonParams.UseExternalClock;
	
	
	return error;      	   
	
	
}

/* =================================================================================== */
SAMPIC256CH_ErrCode     SAMPIC256CH_SetSmartReadMode(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, Boolean mode, int nbOfSamplesToRead, int offsetForStartOfRead)
/* =================================================================================== */
{
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;  
	int dummy = 0;
	
	crateParams->CommonParams.SmartRead = mode&0x1;

	if( mode &0x1 == TRUE)
	{
		crateParams->CommonParams.OffsetForStartOfRead = offsetForStartOfRead;
		crateParams->CommonParams.NbOfSamplesToRead = nbOfSamplesToRead;
	}
	else
	{
		crateParams->CommonParams.OffsetForStartOfRead= 0;  
		crateParams->CommonParams.NbOfSamplesToRead = MAX_NB_OF_SAMPLES;
	}
	
   
	Build_FeBoardsSampicsConfigReg1(crateParams); 
	
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_SAMPIC_SC_REG1, ALL_FE_BOARDs, ALL_FE_BOARDs_FE_FPGAs, dummy); 
	
    if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_FE_SAMPIC_READ_LENGTH, ALL_FE_BOARDs, ALL_FE_BOARDs_FE_FPGAs, dummy); 

    if(error == SAMPIC256CH_Success) error = Reset_SAMPICDLL (crateInfoParams,crateParams); 
			
   return error;      	   
	
}
/* =================================================================================== */


/* =================================================================================== */
/* =================================================================================== */
SAMPIC256CH_ErrCode   SAMPIC256CH_GetSmartReadMode(CrateParamStruct *crateParams, Boolean *mode, int *nbOfSamplesToRead, int *offsetForStartOfRead)
/* =================================================================================== */
{

	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;  
	
	*mode = crateParams->CommonParams.SmartRead;
	*nbOfSamplesToRead = crateParams->CommonParams.NbOfSamplesToRead;
	*offsetForStartOfRead = crateParams->CommonParams.OffsetForStartOfRead;
	
	return error;      	   
	
}
/* =================================================================================== */


/* =================================================================================== */
SAMPIC256CH_ErrCode     SAMPIC256CH_SetTOTMeasurementMode(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, Boolean mode)
/* =================================================================================== */
{
	
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;  
	int dummy = 0;
	
	crateParams->CommonParams.EnableTOTMeasurement = mode&0x1;

	Build_FeBoardsSampicsConfigReg5(crateParams); 
	
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_SAMPIC_SC_REG5, ALL_FE_BOARDs, ALL_FE_BOARDs_FE_FPGAs, dummy); 
    
	Compute_NbOfExtraWords(crateParams);
	
	
    if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_FE_SAMPIC_READ_LENGTH, ALL_FE_BOARDs, ALL_FE_BOARDs_FE_FPGAs, dummy); 
 
	return error;
}
/* =================================================================================== */


/* =================================================================================== */
SAMPIC256CH_ErrCode     SAMPIC256CH_GetTOTMeasurementMode(CrateParamStruct *crateParams, Boolean *mode)
/* =================================================================================== */
{
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;  
	
	*mode = crateParams->CommonParams.EnableTOTMeasurement;
	
	return error;
	
	
}
/* =================================================================================== */

/* =================================================================================== */
SAMPIC256CH_ErrCode     SAMPIC256CH_SetSampicTOTRange(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, SAMPIC_TOTRange_t totRange)
/* =================================================================================== */
{
	
SAMPIC256CH_ErrCode error = SAMPIC256CH_Success; 
	
int startSampicIndex, endSampicIndex, sampicIdx;
int feboardIdx, startFeIndex,endFeIndex;  
int dummy = 0;

	if(feBoard >=  crateInfoParams->NbOfFeBoards)
		return SAMPIC256CH_InvalidParam;
	

	if(feBoard < 0)
	{
		startFeIndex = 0;
		endFeIndex = crateInfoParams->NbOfFeBoards -1;
	}
	else
	{
		startFeIndex = feBoard;
		endFeIndex = feBoard;
	}
  	

	if(sampicIndex < 0)
	{
		startSampicIndex = 0;
		endSampicIndex = NB_OF_SAMPICS_IN_FE_BOARD -1;
		
	}
	else
	{
	    startSampicIndex = sampicIndex;
		endSampicIndex = sampicIndex;
		
	}
	
	
	for(feboardIdx = startFeIndex; feboardIdx <=  endFeIndex;  feboardIdx++)
	{
	  for(sampicIdx = startSampicIndex; sampicIdx <= endSampicIndex; sampicIdx++)
	  {
		 crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.TOTRange = totRange;
		 
		 if(totRange == SAMPIC_TOT_RANGE_MAX_25_NS)
		 	crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.InternalTOTRampCurrentDAC  = DEFAULT_SAMPIC_V3_TOT_RAMP_DAC0;
		 else if(totRange == SAMPIC_TOT_RANGE_MAX_50_NS)
		 	crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.InternalTOTRampCurrentDAC  = DEFAULT_SAMPIC_V3_TOT_RAMP_DAC1;
		 else if(totRange == SAMPIC_TOT_RANGE_MAX_100_NS)
		 	crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.InternalTOTRampCurrentDAC  = DEFAULT_SAMPIC_V3_TOT_RAMP_DAC2;
		 else if(totRange == SAMPIC_TOT_RANGE_MAX_200_NS)
		 	crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.InternalTOTRampCurrentDAC  = DEFAULT_SAMPIC_V3_TOT_RAMP_DAC3;
		 else if(totRange == SAMPIC_TOT_RANGE_MAX_400_NS)
		 	crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.InternalTOTRampCurrentDAC  = DEFAULT_SAMPIC_V3_TOT_RAMP_DAC4;

		 
	  }
	}
	
 Build_FeBoardsSampicsConfigReg5(crateParams);

 if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_SAMPIC_SC_REG5, feBoard, sampicIndex, dummy); 

 return error;  
	
}

/* =================================================================================== */
SAMPIC256CH_ErrCode     SAMPIC256CH_GetSampicTOTRange(CrateParamStruct *crateParams, int feBoard, int sampicIndex, SAMPIC_TOTRange_t *totRange)
/* =================================================================================== */
{

SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;  

if(feBoard >= MAX_NB_OF_FE_BOARDS)
	return SAMPIC256CH_InvalidParam;

if(sampicIndex >= NB_OF_SAMPICS_IN_FE_BOARD)
	return SAMPIC256CH_InvalidParam; 

if(feBoard < 0 || sampicIndex < 0)
	return SAMPIC256CH_InvalidParam;

	  
 *totRange =	crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.TOTRange;
		  
return error;  	

}

/* =================================================================================== */
SAMPIC256CH_ErrCode     SAMPIC256CH_SetSampicTOTFilterParams(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, Boolean enTotFilter, Boolean enWideCap, float pulseWidth)  
/* =================================================================================== */
{
SAMPIC256CH_ErrCode error = SAMPIC256CH_Success; 
	
int startSampicIndex, endSampicIndex, sampicIdx;
int feboardIdx, startFeIndex,endFeIndex;  
int dummy = 0;

	if(feBoard >=  crateInfoParams->NbOfFeBoards)
		return SAMPIC256CH_InvalidParam;
	
	/*if(enWideCap == TRUE)
	{
		if( (pulseWidth <MIN_TOT_FILTER_WIDTH_FOR_WIDE_CAP) || (pulseWidth > MAX_TOT_FILTER_WIDTH_FOR_WIDE_CAP))
			return SAMPIC256CH_InvalidParam;
	}
	else
	{
		
		if( (pulseWidth <MIN_TOT_FILTER_WIDTH_FOR_SMALL_CAP) || (pulseWidth > MAX_TOT_FILTER_WIDTH_FOR_SMALL_CAP))
			return SAMPIC256CH_InvalidParam;
	
	}
	*/
	if(feBoard < 0)
	{
		startFeIndex = 0;
		endFeIndex = crateInfoParams->NbOfFeBoards -1;
	}
	else
	{
		startFeIndex = feBoard;
		endFeIndex = feBoard;
	}
  	

	if(sampicIndex < 0)
	{
		startSampicIndex = 0;
		endSampicIndex = NB_OF_SAMPICS_IN_FE_BOARD -1;
		
	}
	else
	{
	    startSampicIndex = sampicIndex;
		endSampicIndex = sampicIndex;
		
	}
	
	
	for(feboardIdx = startFeIndex; feboardIdx <=  endFeIndex;  feboardIdx++)
	{
	  for(sampicIdx = startSampicIndex; sampicIdx <= endSampicIndex; sampicIdx++)
	  {	
		  crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.EnableTOTFilter =   enTotFilter &0x1;
		  crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.EnableTOTFilterWideCap = enWideCap &0x1;
		  crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.InternalTOTFilterWidth  =  pulseWidth;
		  
	  }
	}
	
 
	Build_FeBoardsSampicsConfigReg4(crateParams);
 	Build_FeBoardsSampicsConfigReg3(crateParams);

 	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_SAMPIC_SC_REG4, feBoard, sampicIndex, dummy); 
    if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_SAMPIC_SC_REG3, feBoard, sampicIndex, dummy);
	
	
 return error;  
	
	
}

/* =================================================================================== */
SAMPIC256CH_ErrCode     SAMPIC256CH_GetSampicTOTFilterParams(CrateParamStruct *crateParams, int feBoard, int sampicIndex, Boolean *enTotFilter, Boolean *enWideCap, float *pulseWidth)
/* =================================================================================== */
 {
	 
SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;  

if(feBoard >= MAX_NB_OF_FE_BOARDS)
	return SAMPIC256CH_InvalidParam;

if(sampicIndex >= NB_OF_SAMPICS_IN_FE_BOARD)
	return SAMPIC256CH_InvalidParam; 

if(feBoard < 0 || sampicIndex < 0)
	return SAMPIC256CH_InvalidParam;
	 
		   *enTotFilter = crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.EnableTOTFilter;
		   *enWideCap = crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.EnableTOTFilterWideCap;
		   *pulseWidth =  crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.InternalTOTFilterWidth;
		   
	return error;     
 }


/* =================================================================================== */
SAMPIC256CH_ErrCode   SAMPIC256CH_SetSampicPostTrigParams(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, Boolean enablePostTrig, int postTrigVal /* value between 0 and 7 */)  
/* =================================================================================== */
{
	
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success; 
	
	int startSampicIndex, endSampicIndex, sampicIdx;
	int feboardIdx, startFeIndex,endFeIndex;  
	int dummy = 0;
  
	if( (postTrigVal < 0) || (postTrigVal > 7)) 
	{
		error = SAMPIC256CH_OutOfRange;
		return error;
	}
	
	if(feBoard >=  crateInfoParams->NbOfFeBoards)
		return SAMPIC256CH_InvalidParam;
	

	if(feBoard < 0)
	{
		startFeIndex = 0;
		endFeIndex = crateInfoParams->NbOfFeBoards -1;
	}
	else
	{
		startFeIndex = feBoard;
		endFeIndex = feBoard;
	}
  	

	if(sampicIndex < 0)
	{
		startSampicIndex = 0;
		endSampicIndex = NB_OF_SAMPICS_IN_FE_BOARD -1;
		
	}
	else
	{
	    startSampicIndex = sampicIndex;
		endSampicIndex = sampicIndex;
		
	}
	
	
	for(feboardIdx = startFeIndex; feboardIdx <=  endFeIndex;  feboardIdx++)
	{
	  for(sampicIdx = startSampicIndex; sampicIdx <= endSampicIndex; sampicIdx++)
	  {
		 crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.EnablePostTrigger  = enablePostTrig &0x1; 
		 crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.PostTrig = 7 - postTrigVal;
	  }
	}
	
	Build_FeBoardsSampicsConfigReg6(crateParams);
	Build_FeBoardsSampicsConfigReg2(crateParams);
	
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_SAMPIC_SC_REG2, feBoard, sampicIndex, dummy); 
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_SAMPIC_SC_REG6, feBoard, sampicIndex, dummy); 
				
			
    return error;
	 
	
	
}

/* =================================================================================== */
SAMPIC256CH_ErrCode   SAMPIC256CH_GetSampicPostTrigParams(CrateParamStruct *crateParams, int feBoard, int sampicIndex, Boolean *enablePostTrig, int *postTrigVal)
/* =================================================================================== */
{
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;  
	
	if(feBoard >= MAX_NB_OF_FE_BOARDS)
		return SAMPIC256CH_InvalidParam;
	
	if(sampicIndex >= NB_OF_SAMPICS_IN_FE_BOARD)
		return SAMPIC256CH_InvalidParam; 

	if(feBoard < 0 || sampicIndex < 0)
		return SAMPIC256CH_InvalidParam;
	
	*enablePostTrig =  crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.EnablePostTrigger;
	*postTrigVal =  7- crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.PostTrig;

	
    return error;   	
}

/* =================================================================================== */
SAMPIC256CH_ErrCode     SAMPIC256CH_SetExternalTriggerType(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, ExternalTriggerType_t  extTrigType)
/* =================================================================================== */
{
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;
	int dummy  =0;
	
	crateParams->ControlBoardParams.ExternalTriggerType = extTrigType; 
	
	Build_ControlBoardTriggerReg(crateParams);
	
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams,CTRLB_TRIGGER_REG , dummy, dummy, dummy); 
	
	return error;  
	
}

/* =================================================================================== */
SAMPIC256CH_ErrCode     SAMPIC256CH_GetExternalTriggerType(CrateParamStruct *crateParams,ExternalTriggerType_t  *extTrigType)
/* =================================================================================== */
{
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;
	
	*extTrigType =  crateParams->ControlBoardParams.ExternalTriggerType;
	
	
	 return error;  
	
}


/* =================================================================================== */
SAMPIC256CH_ErrCode     SAMPIC256CH_SetExternalTriggerEdge(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, EdgeType_t  extTrigEdge) 
/* =================================================================================== */
{
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;
	int dummy  =0;
	
	crateParams->ControlBoardParams.ExternalTriggerEdge = extTrigEdge;
	Build_ControlBoardTriggerReg(crateParams);
	
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams,CTRLB_TRIGGER_REG , dummy, dummy, dummy); 
	

	 return error;  
	
}

/* =================================================================================== */
SAMPIC256CH_ErrCode     SAMPIC256CH_GetExternalTriggerEdge(CrateParamStruct *crateParams,EdgeType_t  *extTrigEdge) 
/* =================================================================================== */
{
	
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;

	*extTrigEdge =  crateParams->ControlBoardParams.ExternalTriggerEdge;
	
	return error;  
	
}

/* =================================================================================== */
SAMPIC256CH_ErrCode     SAMPIC256CH_SetExternalTriggerSigLevel(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, SignalLevel_t  extSigLevel)
/* =================================================================================== */
{
	

	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;
	int dummy  =0;
	
	crateParams->ControlBoardParams.ExternalTriggerSigLevel = extSigLevel;
	
	Build_ControlBoardControlReg(crateParams);  

	if(error == SAMPIC256CH_Success) error = Load_DACs(crateInfoParams, crateParams, CB_EXT_TRIG_THRESHOLD, dummy, dummy);   	
	
	
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, CTRLB_CONTROL_REG, dummy, dummy, dummy);
		  
	return error;      	
}

/* =================================================================================== */
SAMPIC256CH_ErrCode     SAMPIC256CH_GetExternalTriggerSigLevel(CrateParamStruct *crateParams,SignalLevel_t  *extSigLevel)
/* =============================extSigLevel====================================================== */
{
	
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;
	
	*extSigLevel =  crateParams->ControlBoardParams.ExternalTriggerSigLevel;
	
	return error;  	
}

/* =================================================================================== */
SAMPIC256CH_ErrCode     SAMPIC256CH_SetExternalSyncSigLevel(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, SignalLevel_t  extSigLevel)
/* =================================================================================== */
{
	

	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;
	int dummy  =0;
	
	crateParams->ControlBoardParams.ExternalSyncSigLevel = extSigLevel;

	if(error == SAMPIC256CH_Success) error = Load_DACs(crateInfoParams, crateParams, CB_EXT_SYNC_THRESHOLD, dummy, dummy);   	
		  
	return error;      	
}

/* =================================================================================== */
SAMPIC256CH_ErrCode     SAMPIC256CH_GetExternalSyncSigLevel(CrateParamStruct *crateParams,SignalLevel_t  *extSigLevel)
/* =================================================================================== */
{
	
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;
	
	*extSigLevel =  crateParams->ControlBoardParams.ExternalSyncSigLevel;
	
	return error;  	
}


/* =================================================================================== */
SAMPIC256CH_ErrCode     SAMPIC256CH_SetExternalSyncEdge(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, EdgeType_t  extSyncEdge)
/* =================================================================================== */
{
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;
	int dummy  =0;
	
	crateParams->ControlBoardParams.ExternalSyncEdge = extSyncEdge;
	
	Build_ControlBoardTriggerReg(crateParams);
	
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams,CTRLB_TRIGGER_REG , dummy, dummy, dummy); 
	

	 return error;  
	
}

/* =================================================================================== */
SAMPIC256CH_ErrCode     SAMPIC256CH_GetExternalSyncEdge(CrateParamStruct *crateParams,EdgeType_t  *extSyncEdge) 
/* =================================================================================== */
{
	
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;

	*extSyncEdge =  crateParams->ControlBoardParams.ExternalSyncEdge;
	
	return error;  
	
}


/* =================================================================================== */
/* =================================================================================== */


/* =================================================================================== */
SAMPIC256CH_ErrCode     SAMPIC256CH_SetSampicChannelTriggerMode(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, int channelIndex, SAMPIC_ChannelTriggerMode_t  channelTriggerMode)
/* =================================================================================== */
{
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;   
	int  startChannelIndex, endChannelIndex, channelIdx;
	int  startFeIndex,endFeIndex, feboardIdx; 
	int  startSampicIndex,endSampicIndex, sampicIdx;


	
	if(feBoard >=  crateInfoParams->NbOfFeBoards)
		return SAMPIC256CH_InvalidParam;
	
	if(sampicIndex >= NB_OF_SAMPICS_IN_FE_BOARD)
		return SAMPIC256CH_InvalidParam; 
	
	if(channelIndex >= NB_OF_CHANNELS_IN_SAMPIC)
		return SAMPIC256CH_InvalidParam; 
	
	if(feBoard < 0)
	{
		
		startFeIndex = 0;
		endFeIndex = crateInfoParams->NbOfFeBoards -1;
	}
	else
	{
		
		startFeIndex = feBoard;
		endFeIndex = feBoard;
	}
  	

	if(sampicIndex < 0)
	{
		startSampicIndex = 0;
		endSampicIndex = NB_OF_SAMPICS_IN_FE_BOARD -1;
		
	}
	else
	{
	    startSampicIndex = sampicIndex;
		endSampicIndex = sampicIndex;
		
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
	
	for(feboardIdx = startFeIndex; feboardIdx <= endFeIndex; feboardIdx++)
	{
	 for(sampicIdx = startSampicIndex;  sampicIdx <= endSampicIndex ; sampicIdx++)
	 {
		  for(channelIdx = startChannelIndex; channelIdx <= endChannelIndex; channelIdx++)
		  {
				crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.ChannelTriggerMode[channelIdx] = channelTriggerMode;

		  }
	 	}
	}
	 
	Build_FeBoardsSampicsChannelRegs(crateParams);
	
		
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_SAMPIC_SC_CHANNEL_REG, feBoard, sampicIndex, channelIndex);

	return error;      	   
}

/* =================================================================================== */
SAMPIC256CH_ErrCode     SAMPIC256CH_GetSampicChannelTriggerMode(CrateParamStruct *crateParams,int feBoard, int sampicIndex, int channelIndex, SAMPIC_ChannelTriggerMode_t  *channelTriggerMode)
/* =================================================================================== */
 {
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;   

	
	if(feBoard >= MAX_NB_OF_FE_BOARDS)
		return SAMPIC256CH_InvalidParam;
	
	if(sampicIndex >= NB_OF_SAMPICS_IN_FE_BOARD)
		return SAMPIC256CH_InvalidParam;
	
	if(channelIndex >= NB_OF_CHANNELS_IN_SAMPIC)
		return SAMPIC256CH_InvalidParam; 

	if(feBoard < 0 || channelIndex < 0 || sampicIndex < 0)
		return SAMPIC256CH_InvalidParam;
	
	
	*channelTriggerMode =  crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.ChannelTriggerMode[channelIndex];
	
	return error;
	
	
}
//
/* =================================================================================== */
SAMPIC256CH_ErrCode     SAMPIC256CH_SetSampicChannelInternalThreshold(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, int channelIndex /* between 0 and 15 */, float  relativeThreshold)  
/* =================================================================================== */
{
	
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;   
	int  startChannelIndex, endChannelIndex, channelIdx;
	int  startFeIndex,endFeIndex, feboardIdx; 
	int  startSampicIndex,endSampicIndex, sampicIdx;
	float vBaseline;
	float appliedRelativeThreshold;

	
	if(feBoard >=  crateInfoParams->NbOfFeBoards)
		return SAMPIC256CH_InvalidParam;
	
	if(sampicIndex >= NB_OF_SAMPICS_IN_FE_BOARD)
		return SAMPIC256CH_InvalidParam; 
	
	if(channelIndex >= NB_OF_CHANNELS_IN_SAMPIC)
		return SAMPIC256CH_InvalidParam; 
	

	
	if(feBoard < 0)
	{
		
		startFeIndex = 0;
		endFeIndex = crateInfoParams->NbOfFeBoards -1;
	}
	else
	{
		
		startFeIndex = feBoard;
		endFeIndex = feBoard;
	}
  	

	if(sampicIndex < 0)
	{
		startSampicIndex = 0;
		endSampicIndex = NB_OF_SAMPICS_IN_FE_BOARD -1;
		
	}
	else
	{
	    startSampicIndex = sampicIndex;
		endSampicIndex = sampicIndex;
		
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
	
	for(feboardIdx = startFeIndex; feboardIdx <= endFeIndex; feboardIdx++)
	{
	 for(sampicIdx = startSampicIndex;  sampicIdx <= endSampicIndex ; sampicIdx++)
	 {
		  for(channelIdx = startChannelIndex; channelIdx <= endChannelIndex; channelIdx++)
		  {
			  
			  	SAMPIC256CH_GetBaselineReference(crateParams, feboardIdx, sampicIdx, &vBaseline); 
				 
				appliedRelativeThreshold =  relativeThreshold;
				
				if(vBaseline + relativeThreshold < 0)
					appliedRelativeThreshold = (-vBaseline);
				
				 if((vBaseline + relativeThreshold) > MAX_DAC_RAW_VALUE)
					appliedRelativeThreshold = MAX_DAC_RAW_VALUE - vBaseline;
				
					 
				crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.RelativeInternalThreshold[channelIdx] = appliedRelativeThreshold;

		  }
	 	}
	}
	 
	Compute_SAMPICIndividualInternalThresholds(crateInfoParams, crateParams);
	
		
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_SAMPIC_SC_CHANNEL_REG, feBoard, sampicIndex, channelIndex);

	return error;      	   
	
	
}

/* =================================================================================== */
SAMPIC256CH_ErrCode     SAMPIC256CH_GetSampicChannelInternalThreshold(CrateParamStruct *crateParams,int feBoard, int sampicIndex, int channelIndex, float  *relativeThreshold /* in Volts between 0 and 1.8V */) 
/* =================================================================================== */
{
	
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;   

	
	if(feBoard >= MAX_NB_OF_FE_BOARDS)
		return SAMPIC256CH_InvalidParam;
	
	if(sampicIndex >= NB_OF_SAMPICS_IN_FE_BOARD)
		return SAMPIC256CH_InvalidParam;
	
	if(channelIndex >= NB_OF_CHANNELS_IN_SAMPIC)
		return SAMPIC256CH_InvalidParam; 

	if(feBoard < 0 || channelIndex < 0 || sampicIndex < 0)
		return SAMPIC256CH_InvalidParam;
	
	
	*relativeThreshold =  crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.RelativeInternalThreshold[channelIndex];
	
	return error;
	
}

/* =================================================================================== */
SAMPIC256CH_ErrCode     SAMPIC256CH_SetChannelSelflTriggerEdge(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, int channelIndex, EdgeType_t  triggerEdge)
/* =================================================================================== */
{

SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;    	
	int  startChannelIndex, endChannelIndex, channelIdx;
	int  startFeIndex,endFeIndex, feboardIdx; 
	int  startSampicIndex,endSampicIndex, sampicIdx;

	if(feBoard >=  crateInfoParams->NbOfFeBoards)
		return SAMPIC256CH_InvalidParam;
	
	if(sampicIndex >= NB_OF_SAMPICS_IN_FE_BOARD)
		return SAMPIC256CH_InvalidParam; 
	
	if(channelIndex >= NB_OF_CHANNELS_IN_SAMPIC)
		return SAMPIC256CH_InvalidParam; 
	

	
	if(feBoard < 0)
	{
		
		startFeIndex = 0;
		endFeIndex = crateInfoParams->NbOfFeBoards -1;
	}
	else
	{
		
		startFeIndex = feBoard;
		endFeIndex = feBoard;
	}
  	

	if(sampicIndex < 0)
	{
		startSampicIndex = 0;
		endSampicIndex = NB_OF_SAMPICS_IN_FE_BOARD -1;
		
	}
	else
	{
	    startSampicIndex = sampicIndex;
		endSampicIndex = sampicIndex;
		
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
	
	for(feboardIdx = startFeIndex; feboardIdx <= endFeIndex; feboardIdx++)
	{
		 for(sampicIdx = startSampicIndex;  sampicIdx <= endSampicIndex ; sampicIdx++)
		 {
		  	for(channelIdx = startChannelIndex; channelIdx <= endChannelIndex; channelIdx++)
		  	{
		 
				crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.TriggerEdge[channelIdx] = triggerEdge;	  
		  
		  	}
		 }
	
	}
	
	Build_FeBoardsSampicsChannelRegs(crateParams);
	
		
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_SAMPIC_SC_CHANNEL_REG, feBoard, sampicIndex, channelIndex);

	return error;  	
}

/* =================================================================================== */
SAMPIC256CH_ErrCode     SAMPIC256CH_GetChannelSelfTriggerEdge(CrateParamStruct *crateParams,int feBoard, int sampicIndex, int channelIndex,  EdgeType_t  *triggerEdge) 
/* =================================================================================== */
{

SAMPIC256CH_ErrCode error = SAMPIC256CH_Success; 
	
	
	if(feBoard >= MAX_NB_OF_FE_BOARDS)
		return SAMPIC256CH_InvalidParam;
	
	if(sampicIndex >= NB_OF_SAMPICS_IN_FE_BOARD)
		return SAMPIC256CH_InvalidParam;
	
	if(channelIndex >= NB_OF_CHANNELS_IN_SAMPIC)
		return SAMPIC256CH_InvalidParam; 

	if(feBoard < 0 || channelIndex < 0 || sampicIndex < 0)
		return SAMPIC256CH_InvalidParam;
	
	   *triggerEdge = crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.TriggerEdge[channelIndex];
	   
	return error;  	
	
	
	
}

/* =================================================================================== */
SAMPIC256CH_ErrCode     SAMPIC256CH_SetSampicCentralTriggerMode(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, SampicCentralTriggerMode_t centralTriggerMode)
/* =================================================================================== */
{
	
SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;   
int startSampicIndex, endSampicIndex, sampicIdx;
int feboardIdx, startFeIndex,endFeIndex;  
int dummy = 0;

	
	if(feBoard >= MAX_NB_OF_FE_BOARDS)
		return SAMPIC256CH_InvalidParam;
	
	if(sampicIndex >= NB_OF_SAMPICS_IN_FE_BOARD)
		return SAMPIC256CH_InvalidParam;
	
	if(feBoard < 0)
	{
		startFeIndex = 0;
		endFeIndex = crateInfoParams->NbOfFeBoards -1;
	}
	else
	{
		startFeIndex = feBoard;
		endFeIndex = feBoard;
	}
  	

	if(sampicIndex < 0)
	{
		startSampicIndex = 0;
		endSampicIndex = NB_OF_SAMPICS_IN_FE_BOARD -1;
		
	}
	else
	{
	    startSampicIndex = sampicIndex;
		endSampicIndex = sampicIndex;
		
	}
	
	
	for(feboardIdx = startFeIndex; feboardIdx <=  endFeIndex;  feboardIdx++)
	{
	
	  for(sampicIdx = startSampicIndex; sampicIdx <= endSampicIndex; sampicIdx++)
	  {
		  
		if( centralTriggerMode == CENTRAL_OR)
		{
	
			crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.EnCoincidenceModeForCentralTrigger = FALSE;
			crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.EnableTripleCoincidenceMode = FALSE; 
			
		}
		else if( centralTriggerMode == CENTRAL_MULTIPLICITY_2)  
		{
			crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.EnCoincidenceModeForCentralTrigger = TRUE;
			crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.EnableTripleCoincidenceMode = FALSE; 
	
		}
		else  // multiplicity 3
		{
			crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.EnCoincidenceModeForCentralTrigger = TRUE;
			crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.EnableTripleCoincidenceMode = TRUE; 
			
		}
	 
	  }
	}
	
	Build_FeBoardsSampicsConfigReg1(crateParams);
	Build_FeBoardsSampicsConfigReg2(crateParams); 
	
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_SAMPIC_SC_REG2, feBoard, sampicIndex, dummy); 
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_SAMPIC_SC_REG1, feBoard, sampicIndex, dummy); 

   return error;
   
}

//============================================================================================================================================== //
SAMPIC256CH_ErrCode     SAMPIC256CH_GetSampicCentralTriggerMode(CrateParamStruct *crateParams,int feBoard, int sampicIndex, SampicCentralTriggerMode_t *centralTriggerMode)
//============================================================================================================================================== //
{
	

	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;  
	
	if(feBoard >= MAX_NB_OF_FE_BOARDS)
		return SAMPIC256CH_InvalidParam;
	
	if(sampicIndex >= NB_OF_SAMPICS_IN_FE_BOARD)
		return SAMPIC256CH_InvalidParam; 

	if(feBoard < 0 || sampicIndex < 0)
		return SAMPIC256CH_InvalidParam;
	
	if( crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.EnCoincidenceModeForCentralTrigger == FALSE)
		*centralTriggerMode =  CENTRAL_OR;
	else  //enCoincidence = TRUE
	{
		if(crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.EnableTripleCoincidenceMode == FALSE)
		 *centralTriggerMode =  CENTRAL_MULTIPLICITY_2; 
		else  //tripleCoincidence
		 *centralTriggerMode =  CENTRAL_MULTIPLICITY_3; 
	}
		
	return error;	
}



/* =================================================================================== */
SAMPIC256CH_ErrCode     SAMPIC256CH_SetSampicExternalThresholdMode(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, Boolean mode /* 1 = External, 0 = Internal */)
/* =================================================================================== */
{
	
SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;   
int startSampicIndex, endSampicIndex, sampicIdx;
int feboardIdx, startFeIndex,endFeIndex;
int channel;
int dummy = 0;

	
	if(feBoard >= MAX_NB_OF_FE_BOARDS)
		return SAMPIC256CH_InvalidParam;
	
	if(sampicIndex >= NB_OF_SAMPICS_IN_FE_BOARD)
		return SAMPIC256CH_InvalidParam;
	
	if(feBoard < 0)
	{
		startFeIndex = 0;
		endFeIndex = crateInfoParams->NbOfFeBoards -1;
	}
	else
	{
		startFeIndex = feBoard;
		endFeIndex = feBoard;
	}
  	

	if(sampicIndex < 0)
	{
		startSampicIndex = 0;
		endSampicIndex = NB_OF_SAMPICS_IN_FE_BOARD -1;
		
	}
	else
	{
	    startSampicIndex = sampicIndex;
		endSampicIndex = sampicIndex;
		
	}
	
	
	for(feboardIdx = startFeIndex; feboardIdx <=  endFeIndex;  feboardIdx++)
	{
	
	  for(sampicIdx = startSampicIndex; sampicIdx <= endSampicIndex; sampicIdx++)
	  {
		   for(channel = 0; channel < NB_OF_CHANNELS_IN_SAMPIC; channel ++)
		   {
	
				crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.ExtDiscriThresholdSource[channel] = mode;
			
		   }
	  }
	}
	
	Build_FeBoardsSampicsChannelRegs(crateParams);
	

	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_SAMPIC_SC_CHANNEL_REG, feBoard, sampicIndex, ALL_CHANNELs); 

   return error;
	
	
}

/* =================================================================================== */
SAMPIC256CH_ErrCode     SAMPIC256CH_GetSampicExternalThresholdMode(CrateParamStruct *crateParams,int feBoard, int sampicIndex, Boolean *mode) 
/* =================================================================================== */
{
	
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;  
	
	if(feBoard >= MAX_NB_OF_FE_BOARDS)
		return SAMPIC256CH_InvalidParam;
	
	if(sampicIndex >= NB_OF_SAMPICS_IN_FE_BOARD)
		return SAMPIC256CH_InvalidParam; 

	if(feBoard < 0 || sampicIndex < 0)
		return SAMPIC256CH_InvalidParam;
	
	*mode = crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.ExtDiscriThresholdSource[0];
	
		
	return error;	
	
}

//============================================================================================================================================== //
SAMPIC256CH_ErrCode     SAMPIC256CH_SetSampicCentralTriggerEffect(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex,SampicCentralTriggerEffect_t centralTriggerEffect)
//============================================================================================================================================== //
{

SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;   
int startSampicIndex, endSampicIndex, sampicIdx;
int feboardIdx, startFeIndex,endFeIndex;  
int dummy = 0;

	
	if(feBoard >= MAX_NB_OF_FE_BOARDS)
		return SAMPIC256CH_InvalidParam;
	
	if(sampicIndex >= NB_OF_SAMPICS_IN_FE_BOARD)
		return SAMPIC256CH_InvalidParam;
	
	if(feBoard < 0)
	{
		startFeIndex = 0;
		endFeIndex = crateInfoParams->NbOfFeBoards -1;
	}
	else
	{
		startFeIndex = feBoard;
		endFeIndex = feBoard;
	}
  	

	if(sampicIndex < 0)
	{
		startSampicIndex = 0;
		endSampicIndex = NB_OF_SAMPICS_IN_FE_BOARD -1;
		
	}
	else
	{
	    startSampicIndex = sampicIndex;
		endSampicIndex = sampicIndex;
		
	}
	
	
	for(feboardIdx = startFeIndex; feboardIdx <=  endFeIndex;  feboardIdx++)
	{
	
	  for(sampicIdx = startSampicIndex; sampicIdx <= endSampicIndex; sampicIdx++)
	  {	
	 		crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.CentralTriggerEffect =  centralTriggerEffect;   
			
	  }
	}
	
	Build_FeBoardsSampicsConfigReg2(crateParams);   
	
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_SAMPIC_SC_REG2, feBoard, sampicIndex, dummy); 
	
	return error;
	
	
}

//============================================================================================================================================== //
SAMPIC256CH_ErrCode     SAMPIC256CH_GetSampicCentralTriggerEffect(CrateParamStruct *crateParams,int feBoard, int sampicIndex, SampicCentralTriggerEffect_t *centralTriggerEffect)
//============================================================================================================================================== //
{

	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;  
	
	if(feBoard >= MAX_NB_OF_FE_BOARDS)
		return SAMPIC256CH_InvalidParam;
	
	if(sampicIndex >= NB_OF_SAMPICS_IN_FE_BOARD)
		return SAMPIC256CH_InvalidParam; 

	if(feBoard < 0 || sampicIndex < 0)
		return SAMPIC256CH_InvalidParam;

		 *centralTriggerEffect = crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.CentralTriggerEffect;
	
	return error;
	
}

//============================================================================================================================================== //
SAMPIC256CH_ErrCode     SAMPIC256CH_SetSampicCentralTriggerPrimitivesOptions(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, SAMPIC_CT_PrimitivesMode_t primitivesMode, int primitivesGateLength /* between 0 and 7 x 1/8 of the clock period */)
//============================================================================================================================================== //
{
SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;   
int startSampicIndex, endSampicIndex, sampicIdx;
int feboardIdx, startFeIndex,endFeIndex;  
int dummy = 0;

	
	if(feBoard >= MAX_NB_OF_FE_BOARDS)
		return SAMPIC256CH_InvalidParam;
	
	if(sampicIndex >= NB_OF_SAMPICS_IN_FE_BOARD)
		return SAMPIC256CH_InvalidParam;
	if(primitivesGateLength < 0 || primitivesGateLength > 7)
		return SAMPIC256CH_InvalidParam; 
	
	if(feBoard < 0)
	{
		startFeIndex = 0;
		endFeIndex = crateInfoParams->NbOfFeBoards -1;
	}
	else
	{
		startFeIndex = feBoard;
		endFeIndex = feBoard;
	}
  	

	if(sampicIndex < 0)
	{
		startSampicIndex = 0;
		endSampicIndex = NB_OF_SAMPICS_IN_FE_BOARD -1;
		
	}
	else
	{
	    startSampicIndex = sampicIndex;
		endSampicIndex = sampicIndex;
		
	}	
	
	 for(feboardIdx = startFeIndex; feboardIdx <=  endFeIndex;  feboardIdx++)
	{
	
	  for(sampicIdx = startSampicIndex; sampicIdx <= endSampicIndex; sampicIdx++)
	  {	
	 		crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.PrimitivesGateLength =  7-primitivesGateLength; 
			crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.SelGatedDiscriForCTPrimitives = primitivesMode;
			
	  }
	}
	 
	Build_FeBoardsSampicsConfigReg7(crateParams);   
	Build_FeBoardsSampicsConfigReg3(crateParams);
	
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_SAMPIC_SC_REG7, feBoard, sampicIndex, dummy);  	
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_SAMPIC_SC_REG3, feBoard, sampicIndex, dummy); 
	return error;
}

//============================================================================================================================================== //
SAMPIC256CH_ErrCode     SAMPIC256CH_GetSampicCentralTriggerPrimitivesOptions(CrateParamStruct *crateParams,int feBoard, int sampicIndex, SAMPIC_CT_PrimitivesMode_t *primitivesMode, int *primitivesGateLength)
//============================================================================================================================================== //
{
	
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;  
	
	if(feBoard >= MAX_NB_OF_FE_BOARDS)
		return SAMPIC256CH_InvalidParam;
	
	if(sampicIndex >= NB_OF_SAMPICS_IN_FE_BOARD)
		return SAMPIC256CH_InvalidParam; 

	if(feBoard < 0 || sampicIndex < 0)
		return SAMPIC256CH_InvalidParam;

	*primitivesGateLength = 7-crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.PrimitivesGateLength;
	
	*primitivesMode =  crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.SelGatedDiscriForCTPrimitives;
	
	return error;
	
	
}


//============================================================================================================================================== //
SAMPIC256CH_ErrCode     SAMPIC256CH_SetSampicChannelSourceForCT(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, int channelIndex, Boolean mode)
//============================================================================================================================================== //
{
	
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;   
	
	int  startChannelIndex, endChannelIndex, channelIdx;
	int  startFeIndex,endFeIndex, feboardIdx; 
	int  startSampicIndex,endSampicIndex, sampicIdx;

	if(feBoard >=  crateInfoParams->NbOfFeBoards)
		return SAMPIC256CH_InvalidParam;
	
	if(sampicIndex >= NB_OF_SAMPICS_IN_FE_BOARD)
		return SAMPIC256CH_InvalidParam; 
	
	if(channelIndex >= NB_OF_CHANNELS_IN_SAMPIC)
		return SAMPIC256CH_InvalidParam; 
	

	
	if(feBoard < 0)
	{
		
		startFeIndex = 0;
		endFeIndex = crateInfoParams->NbOfFeBoards -1;
	}
	else
	{
		
		startFeIndex = feBoard;
		endFeIndex = feBoard;
	}
  	

	if(sampicIndex < 0)
	{
		startSampicIndex = 0;
		endSampicIndex = NB_OF_SAMPICS_IN_FE_BOARD -1;
		
	}
	else
	{
	    startSampicIndex = sampicIndex;
		endSampicIndex = sampicIndex;
		
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
	
	for(feboardIdx = startFeIndex; feboardIdx <= endFeIndex; feboardIdx++)
	{
	 for(sampicIdx = startSampicIndex;  sampicIdx <= endSampicIndex ; sampicIdx++)
	 {
		  for(channelIdx = startChannelIndex; channelIdx <= endChannelIndex; channelIdx++)
		  {
			  
				crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.DisableChannelForCentralTrigger[channelIdx] = 1-mode;

		  }
	 	}
	}
	 
	Build_FeBoardsSampicsChannelRegs(crateParams);
	
		
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_SAMPIC_SC_CHANNEL_REG, feBoard, sampicIndex, channelIndex);

	return error;    

	
}

//============================================================================================================================================== //
SAMPIC256CH_ErrCode     SAMPIC256CH_GetSampicChannelSourceForCT(CrateParamStruct *crateParams,int feBoard, int sampicIndex, int channelIndex, Boolean *mode)
//============================================================================================================================================== //
{
	
SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;   

	
	if(feBoard >= MAX_NB_OF_FE_BOARDS)
		return SAMPIC256CH_InvalidParam;
	
	if(sampicIndex >= NB_OF_SAMPICS_IN_FE_BOARD)
		return SAMPIC256CH_InvalidParam;
	
	if(channelIndex >= NB_OF_CHANNELS_IN_SAMPIC)
		return SAMPIC256CH_InvalidParam; 

	if(feBoard < 0 || channelIndex < 0 || sampicIndex < 0)
		return SAMPIC256CH_InvalidParam;
	
	
	*mode =  1- crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.DisableChannelForCentralTrigger[channelIndex];
	
	return error;	
}



//============================================================================================================================================== //
SAMPIC256CH_ErrCode     SAMPIC256CH_SetPulserMode(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, Boolean  mode, PulserSourceType_t pulserSource, Boolean synchronousPulses)
//============================================================================================================================================== //
{
	
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;  
	int dummy = 0;
	
	crateParams->CommonParams.EnPulseMode = mode&0x1;
	crateParams->CommonParams.EnPulseReSync = (1- (synchronousPulses & 0x1)); 
	
	crateParams->ControlBoardParams.EnableAutoPulse = (Boolean)pulserSource;
   						  
    Build_ControlBoardControlReg(crateParams);
	Build_FeBoardsFeFpgasPulserReg(crateParams);
	
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, CTRLB_CONTROL_REG, dummy, dummy, dummy); 
	
    if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_FE_PULSER_REG, ALL_FE_BOARDs, ALL_FE_BOARDs_FE_FPGAs, dummy); 
	
   return error;      	
}

//============================================================================================================================================== //
SAMPIC256CH_ErrCode     SAMPIC256CH_GetPulserMode(CrateParamStruct *crateParams,Boolean  *mode, PulserSourceType_t *pulserSource, Boolean *synchronousPulses)
//============================================================================================================================================== //
{
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success; 
	
	*mode = crateParams->CommonParams.EnPulseMode;
	*pulserSource =  (PulserSourceType_t)crateParams->ControlBoardParams.EnableAutoPulse;
	*synchronousPulses =   1-	crateParams->CommonParams.EnPulseReSync;
	
	 return error; 
	
}


//============================================================================================================================================== //
SAMPIC256CH_ErrCode     SAMPIC256CH_SetAutoPulserPeriod(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int pulsePeriod) 
//============================================================================================================================================== //
{
	
SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;  
	int dummy = 0;
	
	if(pulsePeriod < 2|| pulsePeriod > 65535)
		 return SAMPIC256CH_InvalidParam;
		
	crateParams->ControlBoardParams.PulseReg =  (unsigned short)(pulsePeriod -1);
   						  
  	
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, CTRLB_PULSE_REG, dummy, dummy, dummy); 
	
  	
   return error;      	
	
	
}


//============================================================================================================================================== //
SAMPIC256CH_ErrCode     SAMPIC256CH_GetAutoPulserPeriod(CrateParamStruct *crateParams,int *pulsePeriod)
//============================================================================================================================================== //
{
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;   
	
	*pulsePeriod = (int) (crateParams->ControlBoardParams.PulseReg +1);
	
	return error;
}

//============================================================================================================================================== //
SAMPIC256CH_ErrCode     SAMPIC256CH_SetSampicChannelPulseMode(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, int channelIndex, Boolean mode)
//============================================================================================================================================== //
{
	
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;   
	
	int  startChannelIndex, endChannelIndex, channelIdx;
	int  startFeIndex,endFeIndex, feboardIdx; 
	int  startSampicIndex,endSampicIndex, sampicIdx;
	
	if(feBoard >=  crateInfoParams->NbOfFeBoards)
		return SAMPIC256CH_InvalidParam;
	
	if(sampicIndex >= NB_OF_SAMPICS_IN_FE_BOARD)
		return SAMPIC256CH_InvalidParam; 
	
	if(channelIndex >= NB_OF_CHANNELS_IN_SAMPIC)
		return SAMPIC256CH_InvalidParam; 
	

	
	if(feBoard < 0)
	{
		
		startFeIndex = 0;
		endFeIndex = crateInfoParams->NbOfFeBoards -1;
	}
	else
	{
		
		startFeIndex = feBoard;
		endFeIndex = feBoard;
	}
  	

	if(sampicIndex < 0)
	{
		startSampicIndex = 0;
		endSampicIndex = NB_OF_SAMPICS_IN_FE_BOARD -1;
		
	}
	else
	{
	    startSampicIndex = sampicIndex;
		endSampicIndex = sampicIndex;
		
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
	
	for(feboardIdx = startFeIndex; feboardIdx <= endFeIndex; feboardIdx++)
	{
	 for(sampicIdx = startSampicIndex;  sampicIdx <= endSampicIndex ; sampicIdx++)
	 {
		  for(channelIdx = startChannelIndex; channelIdx <= endChannelIndex; channelIdx++)
		  {
			 crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].EnablePulseChannel[channelIdx]= mode&0x1;

		  }
	 	}
	}
	 
	Build_FeBoardsFeFpgasPulserReg(crateParams);
	
		
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_FE_PULSER_REG, feBoard, sampicIndex, channelIndex);
	
	
	return error;    
	
}

//============================================================================================================================================== //
SAMPIC256CH_ErrCode     SAMPIC256CH_GetSampicChannelPulseMode(CrateParamStruct *crateParams,int feBoard,int sampicIndex, int channelIndex, Boolean *mode)     
//============================================================================================================================================== //
{
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;   

	
	if(feBoard >= MAX_NB_OF_FE_BOARDS)
		return SAMPIC256CH_InvalidParam;
	
	if(sampicIndex >= NB_OF_SAMPICS_IN_FE_BOARD)
		return SAMPIC256CH_InvalidParam;
	
	if(channelIndex >= NB_OF_CHANNELS_IN_SAMPIC)
		return SAMPIC256CH_InvalidParam; 

	if(feBoard < 0 || channelIndex < 0 || sampicIndex < 0)
		return SAMPIC256CH_InvalidParam;
	
	
	*mode =  crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].EnablePulseChannel[channelIndex];
	
	return error;
	
	
}


//============================================================================================================================================== //
SAMPIC256CH_ErrCode     SAMPIC256CH_SetSampicPulserWidth(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, unsigned char pulseWidth /* multiple of 10 ns */)
//============================================================================================================================================== //
{
	
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;   
	
	int  startFeIndex,endFeIndex, feboardIdx; 
	int  startSampicIndex,endSampicIndex, sampicIdx;
	int  dummy = 0;

	
	if(feBoard >=  crateInfoParams->NbOfFeBoards)
		return SAMPIC256CH_InvalidParam;
	
	if(sampicIndex >= NB_OF_SAMPICS_IN_FE_BOARD)
		return SAMPIC256CH_InvalidParam; 
	

	
	if(feBoard < 0)
	{
		
		startFeIndex = 0;
		endFeIndex = crateInfoParams->NbOfFeBoards -1;
	}
	else
	{
		
		startFeIndex = feBoard;
		endFeIndex = feBoard;
	}
  	

	if(sampicIndex < 0)
	{
		startSampicIndex = 0;
		endSampicIndex = NB_OF_SAMPICS_IN_FE_BOARD -1;
		
	}
	else
	{
	    startSampicIndex = sampicIndex;
		endSampicIndex = sampicIndex;
		
	}
	
	
	for(feboardIdx = startFeIndex; feboardIdx <= endFeIndex; feboardIdx++)
	{
		 for(sampicIdx = startSampicIndex;  sampicIdx <= endSampicIndex ; sampicIdx++)
	 	{
		 	crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].PulserWidth =  pulseWidth;
		  
	 	}	 
	}
	 
		
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_FE_PULSER_WIDTH, feBoard, sampicIndex, dummy);
	
	return error;    
	
}

//============================================================================================================================================== //
SAMPIC256CH_ErrCode     SAMPIC256CH_GetSampicPulserWidth(CrateParamStruct *crateParams,int feBoard,int sampicIndex, unsigned char *pulseWidth /* multiple of 10 ns */)     
//============================================================================================================================================== //
{
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;   

	
	if(feBoard >= MAX_NB_OF_FE_BOARDS)
		return SAMPIC256CH_InvalidParam;
	
	if(sampicIndex >= NB_OF_SAMPICS_IN_FE_BOARD)
		return SAMPIC256CH_InvalidParam;
	

	if(feBoard < 0 || sampicIndex < 0)
		return SAMPIC256CH_InvalidParam;
	
	
	*pulseWidth = crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].PulserWidth;
	
																 
	return error;
	
	
}



//============================================================================================================================================== //
SAMPIC256CH_ErrCode     SAMPIC256CH_SetSampicExternalThreshold(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex,float externalThreshold)
//============================================================================================================================================== //
{
	
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;   
	
	int  startFeIndex,endFeIndex, feboardIdx; 
	int  startSampicIndex,endSampicIndex, sampicIdx;
	int  dummy = 0;

	
	if(feBoard >=  crateInfoParams->NbOfFeBoards)
		return SAMPIC256CH_InvalidParam;
	
	if(sampicIndex >= NB_OF_SAMPICS_IN_FE_BOARD)
		return SAMPIC256CH_InvalidParam; 
	

	if(externalThreshold < 0 || externalThreshold > MAX_DAC_RAW_VALUE)
		return SAMPIC256CH_InvalidParam;
	
	if(feBoard < 0)
	{
		
		startFeIndex = 0;
		endFeIndex = crateInfoParams->NbOfFeBoards -1;
	}
	else
	{
		
		startFeIndex = feBoard;
		endFeIndex = feBoard;
	}
  	

	if(sampicIndex < 0)
	{
		startSampicIndex = 0;
		endSampicIndex = NB_OF_SAMPICS_IN_FE_BOARD -1;
		
	}
	else
	{
	    startSampicIndex = sampicIndex;
		endSampicIndex = sampicIndex;
		
	}
	
	
	for(feboardIdx = startFeIndex; feboardIdx <= endFeIndex; feboardIdx++)
	{
		for(sampicIdx = startSampicIndex;  sampicIdx <= endSampicIndex ; sampicIdx++)
	 	{
		 	crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.ExternalThreshold =  externalThreshold;

		  
	 	}
		
		
	}
	 
				
	error = Load_DACs(crateInfoParams, crateParams, FEB_VDAC_EXT_THRESHOLD, feBoard, sampicIndex); 
	
	return error;    
	
	
	
}


//============================================================================================================================================== //
SAMPIC256CH_ErrCode     SAMPIC256CH_GetSampicExternalThreshold(CrateParamStruct *crateParams,int feBoard, int sampicIndex, float *externalThreshold )     
//============================================================================================================================================== //
{
	
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;   

	
	if(feBoard >= MAX_NB_OF_FE_BOARDS)
		return SAMPIC256CH_InvalidParam;
	
	if(sampicIndex >= NB_OF_SAMPICS_IN_FE_BOARD)
		return SAMPIC256CH_InvalidParam;
	

	if(feBoard < 0 || sampicIndex < 0)
		return SAMPIC256CH_InvalidParam;
	
	
	*externalThreshold = crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.ExternalThreshold;
	
																 
	return error;	
	
}

//============================================================================================================================================== //
SAMPIC256CH_ErrCode 	SAMPIC256CH_SetLevel3TriggerLogic(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, Boolean enableBuildL3, TriggerLogicParamStruct triggerLogicParamsForL3)
//============================================================================================================================================== //
{
	
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;
	int dummy  =0;
	Boolean enableBuildL2;
	
	 
	SAMPIC256CH_GetLevel2TriggerBuildOption(crateParams, &enableBuildL2);
	
	if(enableBuildL3 == TRUE && enableBuildL2 == FALSE)
		return SAMPIC256CH_FunctionNotAllowed; // enableBuildL2 must ON
	
	crateParams->ControlBoardParams.SelFeb0ForL3 = triggerLogicParamsForL3.SelInput0; 
	crateParams->ControlBoardParams.SelFeb1ForL3 = triggerLogicParamsForL3.SelInput1; 
	crateParams->ControlBoardParams.SelFeb2ForL3 = triggerLogicParamsForL3.SelInput2; 
	crateParams->ControlBoardParams.SelFeb3ForL3 = triggerLogicParamsForL3.SelInput3; 
	
	crateParams->ControlBoardParams.Layer1TriggerLogic0 = triggerLogicParamsForL3.Layer1TriggerLogic0; 
	crateParams->ControlBoardParams.Layer1TriggerLogic1 = triggerLogicParamsForL3.Layer1TriggerLogic1; 
	crateParams->ControlBoardParams.Layer1TriggerLogic2 = triggerLogicParamsForL3.Layer1TriggerLogic2; 
	crateParams->ControlBoardParams.Layer2TriggerLogic0 = triggerLogicParamsForL3.Layer2TriggerLogic0; 
	crateParams->ControlBoardParams.Layer2TriggerLogic1 = triggerLogicParamsForL3.Layer2TriggerLogic1; 
	crateParams->ControlBoardParams.Layer3TriggerLogic	= triggerLogicParamsForL3.Layer3TriggerLogic; 
	
	crateParams->ControlBoardParams.EnableBuildL3 = enableBuildL3;
	
	Build_ControlBoardTriggerCombiLogicForL3Reg(crateParams); 
	
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams,CTRLB_TRIGGER_COMBI_LOGIC_FOR_L3 , dummy, dummy, dummy); 
	

	return error;  	
	
}

//============================================================================================================================================== //
SAMPIC256CH_ErrCode 	SAMPIC256CH_GetLevel3TriggerLogic(CrateParamStruct *crateParams, Boolean *enableBuildL3, TriggerLogicParamStruct *triggerLogicParams)
//============================================================================================================================================== //
{
	
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success; 
	
	
	*enableBuildL3 =crateParams->ControlBoardParams.EnableBuildL3;
	
	triggerLogicParams->SelInput0 =  	crateParams->ControlBoardParams.SelFeb0ForL3;
	triggerLogicParams->SelInput1 =  	crateParams->ControlBoardParams.SelFeb1ForL3;
	triggerLogicParams->SelInput2 =  	crateParams->ControlBoardParams.SelFeb2ForL3;
	triggerLogicParams->SelInput3 =  	crateParams->ControlBoardParams.SelFeb3ForL3;  
	
	triggerLogicParams->Layer1TriggerLogic0 = 	crateParams->ControlBoardParams.Layer1TriggerLogic0;
	triggerLogicParams->Layer1TriggerLogic1 = 	crateParams->ControlBoardParams.Layer1TriggerLogic1;
	triggerLogicParams->Layer1TriggerLogic2 = 	crateParams->ControlBoardParams.Layer1TriggerLogic2;
	
	triggerLogicParams->Layer2TriggerLogic0 = 	crateParams->ControlBoardParams.Layer2TriggerLogic0;
	triggerLogicParams->Layer2TriggerLogic1 = 	crateParams->ControlBoardParams.Layer2TriggerLogic1;
		
	triggerLogicParams->Layer3TriggerLogic = 	crateParams->ControlBoardParams.Layer3TriggerLogic;

	
	return error; 
	
}

//============================================================================================================================================== //
SAMPIC256CH_ErrCode 	SAMPIC256CH_SetLevel3CoincidenceModeWithExtTrigGate(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, Boolean enableCoincidence)
//============================================================================================================================================== //
{

	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success; 
	int dummy = 0;
	
	crateParams->ControlBoardParams.EnableCoincidenceWithExtTrigGateForL3  =  enableCoincidence;
			
	Build_ControlBoardControlReg2(crateParams); 
	
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams,CTRLB_CONTROL_REG2 , dummy, dummy, dummy); 
	
	return error;

		
}

//============================================================================================================================================== //
SAMPIC256CH_ErrCode 	SAMPIC256CH_GetLevel3CoincidenceModeWithExtTrigGate(CrateParamStruct *crateParams, Boolean *enableCoincidence)
//============================================================================================================================================== //
{
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success; 
	
	*enableCoincidence =  	crateParams->ControlBoardParams.EnableCoincidenceWithExtTrigGateForL3;
	
	return error;  
	
}

//============================================================================================================================================== //
SAMPIC256CH_ErrCode 	SAMPIC256CH_SetLevel3ExtTrigGate(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, unsigned char extTrigGate)
//============================================================================================================================================== //
{
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success; 
	int dummy = 0;
	
	if(extTrigGate < 3)
		extTrigGate = 3;  // min value is 3	
	crateParams->ControlBoardParams.ExternalTrigGateForL3 = extTrigGate - 3;
	
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, CTRLB_EXT_TRIG_GATE_FOR_L3 , dummy, dummy, dummy); 

	return error;  
	
}

//============================================================================================================================================== //
SAMPIC256CH_ErrCode 	SAMPIC256CH_GetLevel3ExtTrigGate(CrateParamStruct *crateParams, unsigned char *extTrigGate)
//============================================================================================================================================== //
{
	 
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;
	
	*extTrigGate = 	crateParams->ControlBoardParams.ExternalTrigGateForL3 + 3;
	
	return error; 
}

//============================================================================================================================================== //
SAMPIC256CH_ErrCode 	SAMPIC256CH_SetLevel2TriggerBuildOption(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, Boolean enableBuildL2)
//============================================================================================================================================== //
{
	int dummy = 0;
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success; 

   	if(enableBuildL2 == TRUE )
	 	SAMPIC256CH_SetAutoConversionMode(crateInfoParams, crateParams, FALSE);
	else
		SAMPIC256CH_SetAutoConversionMode(crateInfoParams, crateParams, TRUE); 
	
	crateParams->CommonParams.EnableBuildL2 = enableBuildL2;

	Build_FeBoardCtrlFpgaTriggerCombiLogicForL2Reg(crateParams);

	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_CTRL_TRIGGER_COMBI_LOGIC_FOR_L2, ALL_FE_BOARDs, dummy, dummy);  
	
	return error;
	
	
}


//============================================================================================================================================== //
SAMPIC256CH_ErrCode 	SAMPIC256CH_GetLevel2TriggerBuildOption(CrateParamStruct *crateParams, Boolean *enableBuildL2)
//============================================================================================================================================== //
{
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success; 
	
	*enableBuildL2	 =  crateParams->CommonParams.EnableBuildL2 ;
	
	return error;
	
}
//============================================================================================================================================== //


//============================================================================================================================================== //
SAMPIC256CH_ErrCode 	SAMPIC256CH_SetLevel2TriggerLogic(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard,TriggerLogicParamStruct triggerLogicParamsForL2)
//============================================================================================================================================== //
{
	
	int dummy = 0;
	int startFeIndex,endFeIndex,feboardIdx ; 
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success; 
	
	
	if(feBoard >=  crateInfoParams->NbOfFeBoards)
		return SAMPIC256CH_InvalidParam;
	
	if(feBoard < 0)
	{
		
		startFeIndex = 0;
		endFeIndex = crateInfoParams->NbOfFeBoards -1;
	}
	else
	{
		
		startFeIndex = feBoard;
		endFeIndex = feBoard;
	}

		   
	for(feboardIdx = startFeIndex; feboardIdx <= endFeIndex; feboardIdx++)
	{
		
	  	crateParams->FeBoardParams[feboardIdx].ControlFpgaParams.SelSAMPIC0ForL2 = triggerLogicParamsForL2.SelInput0; 
		crateParams->FeBoardParams[feboardIdx].ControlFpgaParams.SelSAMPIC1ForL2 = triggerLogicParamsForL2.SelInput1; 
		crateParams->FeBoardParams[feboardIdx].ControlFpgaParams.SelSAMPIC2ForL2 = triggerLogicParamsForL2.SelInput2; 
		crateParams->FeBoardParams[feboardIdx].ControlFpgaParams.SelSAMPIC3ForL2 = triggerLogicParamsForL2.SelInput3; 
		
		crateParams->FeBoardParams[feboardIdx].ControlFpgaParams.Layer1TriggerLogic0 = triggerLogicParamsForL2.Layer1TriggerLogic0; 
		crateParams->FeBoardParams[feboardIdx].ControlFpgaParams.Layer1TriggerLogic1 = triggerLogicParamsForL2.Layer1TriggerLogic1; 
		crateParams->FeBoardParams[feboardIdx].ControlFpgaParams.Layer1TriggerLogic2 = triggerLogicParamsForL2.Layer1TriggerLogic2; 
		crateParams->FeBoardParams[feboardIdx].ControlFpgaParams.Layer2TriggerLogic0 = triggerLogicParamsForL2.Layer2TriggerLogic0; 
		crateParams->FeBoardParams[feboardIdx].ControlFpgaParams.Layer2TriggerLogic1 = triggerLogicParamsForL2.Layer2TriggerLogic1; 
		crateParams->FeBoardParams[feboardIdx].ControlFpgaParams.Layer3TriggerLogic  =  triggerLogicParamsForL2.Layer3TriggerLogic; 	  

	
		
	}

	Build_FeBoardCtrlFpgaTriggerCombiLogicForL2Reg(crateParams);

	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_CTRL_TRIGGER_COMBI_LOGIC_FOR_L2, feBoard, dummy, dummy);  
	
	return error;
	
	
}

//============================================================================================================================================== //
SAMPIC256CH_ErrCode 	SAMPIC256CH_GetLevel2TriggerLogic(CrateParamStruct *crateParams, int feBoard, TriggerLogicParamStruct *triggerLogicParamsForL2)
//============================================================================================================================================== //
{
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success; 
	 
	if(feBoard < 0  || feBoard >= MAX_NB_OF_FE_BOARDS)
	    return SAMPIC256CH_InvalidParam; 

	
	triggerLogicParamsForL2->SelInput0 =  	crateParams->FeBoardParams[feBoard].ControlFpgaParams.SelSAMPIC0ForL2;
	triggerLogicParamsForL2->SelInput1 =  	crateParams->FeBoardParams[feBoard].ControlFpgaParams.SelSAMPIC1ForL2;
	triggerLogicParamsForL2->SelInput2 =  	crateParams->FeBoardParams[feBoard].ControlFpgaParams.SelSAMPIC2ForL2;
	triggerLogicParamsForL2->SelInput3 =  	crateParams->FeBoardParams[feBoard].ControlFpgaParams.SelSAMPIC3ForL2;  
	
	triggerLogicParamsForL2->Layer1TriggerLogic0 = 	crateParams->FeBoardParams[feBoard].ControlFpgaParams.Layer1TriggerLogic0;
	triggerLogicParamsForL2->Layer1TriggerLogic1 = 	crateParams->FeBoardParams[feBoard].ControlFpgaParams.Layer1TriggerLogic1;
	triggerLogicParamsForL2->Layer1TriggerLogic2 = 	crateParams->FeBoardParams[feBoard].ControlFpgaParams.Layer1TriggerLogic2;
	
	triggerLogicParamsForL2->Layer2TriggerLogic0 = 	crateParams->FeBoardParams[feBoard].ControlFpgaParams.Layer2TriggerLogic0;
	triggerLogicParamsForL2->Layer2TriggerLogic1 = 	crateParams->FeBoardParams[feBoard].ControlFpgaParams.Layer2TriggerLogic1;
		
	triggerLogicParamsForL2->Layer3TriggerLogic = 	crateParams->FeBoardParams[feBoard].ControlFpgaParams.Layer3TriggerLogic;
	 
	return error;
		 
	 
}


//============================================================================================================================================== //
SAMPIC256CH_ErrCode 	SAMPIC256CH_SetLevel2CoincidenceModeWithExtTrigGate(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, Boolean enableCoincidence)
//============================================================================================================================================== //
{
	
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;
	int dummy = 0;
	int startFeIndex,endFeIndex,feboardIdx ;
	
	if(feBoard >=  crateInfoParams->NbOfFeBoards)
		return SAMPIC256CH_InvalidParam;
	
	if(feBoard < 0)
	{
		
		startFeIndex = 0;
		endFeIndex = crateInfoParams->NbOfFeBoards -1;
	}
	else
	{
		
		startFeIndex = feBoard;
		endFeIndex = feBoard;
	}
  	
	for(feboardIdx = startFeIndex; feboardIdx <=  endFeIndex;  feboardIdx++)
	{
		crateParams->FeBoardParams[feboardIdx].ControlFpgaParams.EnableCoincidenceWithExtTrigGateForL2 = enableCoincidence;
		
	}
	
	Build_FeBoardsCtrlFpgaControlReg2(crateParams);  
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_CTRL_CONTROL_REG2, feBoard, dummy, dummy);  

	return error;	
}


//============================================================================================================================================== //
SAMPIC256CH_ErrCode 	SAMPIC256CH_GetLevel2CoincidenceModeWithExtTrigGate(CrateParamStruct *crateParams, int feBoard, Boolean *enableCoincidence)
//============================================================================================================================================== //
{
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;
	
	if(feBoard < 0  || feBoard >= MAX_NB_OF_FE_BOARDS)
	    	return SAMPIC256CH_InvalidParam; 
	
	*enableCoincidence   =   crateParams->FeBoardParams[feBoard].ControlFpgaParams.EnableCoincidenceWithExtTrigGateForL2;
	
	return error;	
	
}

//============================================================================================================================================== //
SAMPIC256CH_ErrCode 	SAMPIC256CH_SetLevel2ExtTrigGate(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, unsigned char extTrigGate)
//============================================================================================================================================== //
{
	
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;
	int dummy = 0;
	int startFeIndex,endFeIndex,feboardIdx;
	
	if(feBoard >=  crateInfoParams->NbOfFeBoards)
		return SAMPIC256CH_InvalidParam;

	if(extTrigGate < 3)
		extTrigGate	= 3;
	
	if(feBoard < 0)
	{
		
		startFeIndex = 0;
		endFeIndex = crateInfoParams->NbOfFeBoards -1;
	}
	else
	{
		
		startFeIndex = feBoard;
		endFeIndex = feBoard;
	}
		
	for(feboardIdx = startFeIndex; feboardIdx <=  endFeIndex;  feboardIdx++)
	{
		crateParams->FeBoardParams[feboardIdx].ControlFpgaParams.ExternalTrigGateForL2 = (extTrigGate -3);
	
	}
	
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_CTRL_EXT_TRIG_GATE_FOR_L2, feBoard, dummy, dummy);  
	
	
	return error;	
}


//============================================================================================================================================== //
SAMPIC256CH_ErrCode 	SAMPIC256CH_GetLevel2ExtTrigGate(CrateParamStruct *crateParams, int feBoard, unsigned char *extTrigGate)
//============================================================================================================================================== //
{
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;
	 
	if(feBoard < 0  || feBoard >= MAX_NB_OF_FE_BOARDS)
	    	return SAMPIC256CH_InvalidParam; 

	
	*extTrigGate = 	(crateParams->FeBoardParams[feBoard].ControlFpgaParams.ExternalTrigGateForL2 + 3);
	
	return error;	
	
	
}


//============================================================================================================================================== //
SAMPIC256CH_ErrCode SAMPIC256CH_SetPrimitivesGateLength(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, unsigned char primitivesGateLength  /* clock periods of 10 ns */)
//============================================================================================================================================== //
{
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;
	int dummy = 0;
	
	crateParams->CommonParams.L2SAMPICPrimitivesGateLength  =  primitivesGateLength;
	
	Build_FeBoardsFeFpgasL2CoincidenceReg(crateParams);
	
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_FE_L2_COINCIDENCE_REG, ALL_FE_BOARDs, ALL_FE_BOARDs_FE_FPGAs, dummy);  

		
	return error;		
		
		
	
}



//============================================================================================================================================== //
SAMPIC256CH_ErrCode 	SAMPIC256CH_GetPrimitivesGateLength(CrateParamStruct *crateParams, unsigned char *primitivesGateLength)
//============================================================================================================================================== //
{
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;

	*primitivesGateLength = crateParams->CommonParams.L2SAMPICPrimitivesGateLength;
	return error;
	
	
}


//============================================================================================================================================== //
SAMPIC256CH_ErrCode 	SAMPIC256CH_SetLevel2LatencyGateLength(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, unsigned char latencyGateLength /* clock periods of 10 ns */) 
//============================================================================================================================================== //
{
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;
	int dummy = 0;
	
	crateParams->CommonParams.L2CoincidenceLatencyLength  =  latencyGateLength;
	
	Build_FeBoardsFeFpgasL2CoincidenceReg(crateParams);
	
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_FE_L2_COINCIDENCE_REG, ALL_FE_BOARDs, ALL_FE_BOARDs_FE_FPGAs, dummy);  

		
	return error;		
		
		
	
}


//============================================================================================================================================== //
SAMPIC256CH_ErrCode 	SAMPIC256CH_GetLevel2LatencyGateLength(CrateParamStruct *crateParams, unsigned char *latencyGateLength)
//============================================================================================================================================== //
{
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;

	*latencyGateLength = crateParams->CommonParams.L2CoincidenceLatencyLength;
	
	return error;
	
	
}

//============================================================================================================================================== //
SAMPIC256CH_ErrCode 	SAMPIC256CH_SetFrontEndBoardGlobalTriggerOption(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, FebGlobalTrigger_t febGlobalTrigger)
//============================================================================================================================================== //
{
	
	
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;
	int dummy = 0;
	int startFeIndex,endFeIndex,feboardIdx;
	
	if(feBoard >=  crateInfoParams->NbOfFeBoards)
		return SAMPIC256CH_InvalidParam;
		
	if(feBoard < 0)
	{
		
		startFeIndex = 0;
		endFeIndex = crateInfoParams->NbOfFeBoards -1;
	}
	else
	{
		
		startFeIndex = feBoard;
		endFeIndex = feBoard;
	}
		
	for(feboardIdx = startFeIndex; feboardIdx <=  endFeIndex;  feboardIdx++)
	{
		crateParams->FeBoardParams[feboardIdx].ControlFpgaParams.FEBGlobalTriggerIsL3 = (Boolean)febGlobalTrigger;
	
	}
	
	Build_FeBoardsCtrlFpgaControlReg2(crateParams);
	
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_CTRL_CONTROL_REG2, feBoard, dummy, dummy);  
	
	
	return error;	
	
	
}


//============================================================================================================================================== //
SAMPIC256CH_ErrCode 	SAMPIC256CH_GetFrontEndBoardGlobalTriggerOption(CrateParamStruct *crateParams, int feBoard, FebGlobalTrigger_t *febGlobalTrigger)
//============================================================================================================================================== //
{
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;
	 
	if(feBoard < 0  || feBoard >= MAX_NB_OF_FE_BOARDS)
	    	return SAMPIC256CH_InvalidParam; 

	
	*febGlobalTrigger = (FebGlobalTrigger_t)(crateParams->FeBoardParams[feBoard].ControlFpgaParams.FEBGlobalTriggerIsL3);
	
	return error;	
	
	
	
}

//============================================================================================================================================== //
SAMPIC256CH_ErrCode     SAMPIC256CH_SetSampicTriggerOption(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, SampicTriggerOption_t sampicTriggerOption)
//============================================================================================================================================== //
{
	int  startFeIndex,endFeIndex, feboardIdx; 
	int  startSampicIndex,endSampicIndex, sampicIdx;
	int  dummy = 0;
	
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;  
		
	if(feBoard >= MAX_NB_OF_FE_BOARDS)
		return SAMPIC256CH_InvalidParam;
	
	if(sampicIndex >= NB_OF_SAMPICS_IN_FE_BOARD)
		return SAMPIC256CH_InvalidParam;
	
		
	if(feBoard < 0)
	{
		startFeIndex = 0;
		endFeIndex = crateInfoParams->NbOfFeBoards -1;
	}
	else
	{
		startFeIndex = feBoard;
		endFeIndex = feBoard;
	}
  	

	if(sampicIndex < 0)
	{
		startSampicIndex = 0;
		endSampicIndex = NB_OF_SAMPICS_IN_FE_BOARD -1;
		
	}
	else
	{
	    startSampicIndex = sampicIndex;
		endSampicIndex = sampicIndex;
		
	}	
	
	for(feboardIdx = startFeIndex; feboardIdx <=  endFeIndex;  feboardIdx++)
	{
	
	  for(sampicIdx = startSampicIndex; sampicIdx <= endSampicIndex; sampicIdx++)
	  {	
	 		crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].EnableL2Coincidence =  (Boolean)sampicTriggerOption; 
			
	  }
	}
	
	 
	Build_FeBoardsFeFpgasControlReg(crateParams);
	  
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_FE_CONTROL_REG, feBoard, sampicIndex, dummy);
	
	
	return error;	
		
	
}

//============================================================================================================================================== //
SAMPIC256CH_ErrCode     SAMPIC256CH_GetSampicTriggerOption(CrateParamStruct *crateParams,int feBoard, int sampicIndex, SampicTriggerOption_t *sampicTriggerOption)     
//============================================================================================================================================== //
{
	
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;  
	
	if(feBoard >= MAX_NB_OF_FE_BOARDS)
		return SAMPIC256CH_InvalidParam;
	
	if(sampicIndex >= NB_OF_SAMPICS_IN_FE_BOARD)
		return SAMPIC256CH_InvalidParam; 

	if(feBoard < 0 || sampicIndex < 0)
		return SAMPIC256CH_InvalidParam;

	
	*sampicTriggerOption =  (SampicTriggerOption_t)crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].EnableL2Coincidence;
	
	return error;
	
	
}


//============================================================================================================================================== //
SAMPIC256CH_ErrCode     SAMPIC256CH_SetSampicEnableTriggerMode(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, Boolean  useExtTrigAsEnableTrig, Boolean openGateOnExtTrig, unsigned char extTrigGate)
//============================================================================================================================================== //
{
	int dummy = 0;
	int startFeIndex,endFeIndex,feboardIdx;
	int  startSampicIndex,endSampicIndex, sampicIdx;    
		
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success; 	
	
	if(feBoard >=  crateInfoParams->NbOfFeBoards)
		return SAMPIC256CH_InvalidParam;
		
	
	if(extTrigGate < MIN_TRIG_GATE_VALUE)
			return SAMPIC256CH_InvalidParam;  
	
	if(feBoard < 0)
	{
		
		startFeIndex = 0;
		endFeIndex = crateInfoParams->NbOfFeBoards -1;
	}
	else
	{
		
		startFeIndex = feBoard;
		endFeIndex = feBoard;
	}
	
	
	if(sampicIndex < 0)
	{
		startSampicIndex = 0;
		endSampicIndex = NB_OF_SAMPICS_IN_FE_BOARD -1;
		
	}
	else
	{
	    startSampicIndex = sampicIndex;
		endSampicIndex = sampicIndex;
		
	}	
	
	for(feboardIdx = startFeIndex; feboardIdx <=  endFeIndex;  feboardIdx++)
	{
	  for(sampicIdx = startSampicIndex; sampicIdx <= endSampicIndex; sampicIdx++)
	  {	
		    crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].EnExtTrisAsEnableTrig =  (Boolean)useExtTrigAsEnableTrig;       
	 		crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].EnableExtTrigGate =  (Boolean)openGateOnExtTrig; 
			crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].ExternalTrigGate =  (extTrigGate-MIN_TRIG_GATE_VALUE);
			
	  }
	}
	
		 
	Build_FeBoardsFeFpgasControlReg(crateParams);
	  
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_FE_CONTROL_REG, feBoard, sampicIndex, dummy);
	
	
	return error;	


	
}

//============================================================================================================================================== //
SAMPIC256CH_ErrCode     SAMPIC256CH_GetSampicEnableTriggerMode(CrateParamStruct *crateParams, int feBoard, int sampicIndex, Boolean  *useExtTrigAsEnableTrig, Boolean *openGateOnExtTrig, unsigned char *extTrigGate)
//============================================================================================================================================== //
{
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;    
	
	if(feBoard >= MAX_NB_OF_FE_BOARDS)
		return SAMPIC256CH_InvalidParam;
	
	if(sampicIndex >= NB_OF_SAMPICS_IN_FE_BOARD)
		return SAMPIC256CH_InvalidParam; 

	if(feBoard < 0 || sampicIndex < 0)
		return SAMPIC256CH_InvalidParam;

	
	
	*useExtTrigAsEnableTrig = crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].EnExtTrisAsEnableTrig;
	*openGateOnExtTrig = crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].EnableExtTrigGate;
	*extTrigGate =  (crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].ExternalTrigGate) + MIN_TRIG_GATE_VALUE;
	
	return error;
	
	
}
//============================================================================================================================================== //


//============================================================================================================================================== //
SAMPIC256CH_ErrCode     SAMPIC256CH_SetSampicCommonDeadTimeMode(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, Boolean  enCommonDeadTime)
//============================================================================================================================================== //
{
	int dummy = 0;
	int startFeIndex,endFeIndex,feboardIdx;
	int  startSampicIndex,endSampicIndex, sampicIdx;    
		
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success; 	
	
	if(feBoard >=  crateInfoParams->NbOfFeBoards)
		return SAMPIC256CH_InvalidParam;
		
	
	if(feBoard < 0)
	{
		
		startFeIndex = 0;
		endFeIndex = crateInfoParams->NbOfFeBoards -1;
	}
	else
	{
		
		startFeIndex = feBoard;
		endFeIndex = feBoard;
	}
	
	
	if(sampicIndex < 0)
	{
		startSampicIndex = 0;
		endSampicIndex = NB_OF_SAMPICS_IN_FE_BOARD -1;
		
	}
	else
	{
	    startSampicIndex = sampicIndex;
		endSampicIndex = sampicIndex;
		
	}	
	
	for(feboardIdx = startFeIndex; feboardIdx <=  endFeIndex;  feboardIdx++)
	{
	  for(sampicIdx = startSampicIndex; sampicIdx <= endSampicIndex; sampicIdx++)
	  {	
	
  			crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.EnableCommonDeadTime = enCommonDeadTime;
	  }
	}
	
	Build_FeBoardsSampicsConfigReg6(crateParams);
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_SAMPIC_SC_REG6, feBoard, sampicIndex, dummy); 
	
	return error;
	
	
}


//============================================================================================================================================== //
SAMPIC256CH_ErrCode     SAMPIC256CH_GetSampicCommonDeadTimeMode(CrateParamStruct *crateParams,int feBoard, int sampicIndex, Boolean  *enCommonDeadTime)
//============================================================================================================================================== //
{
	
		
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;    
	
	if(feBoard >= MAX_NB_OF_FE_BOARDS)
		return SAMPIC256CH_InvalidParam;
	
	if(sampicIndex >= NB_OF_SAMPICS_IN_FE_BOARD)
		return SAMPIC256CH_InvalidParam; 

	if(feBoard < 0 || sampicIndex < 0)
		return SAMPIC256CH_InvalidParam;

	*enCommonDeadTime = crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.EnableCommonDeadTime;
	
	return error; 
}


//=========================  END Control Functions ============================= //
//============================================================================== //




// ACQUISITION
//============================================================================== //


//============================================================================== //
SAMPIC256CH_ErrCode  SAMPIC256CH_AllocateEventMemory(void **eventBuffer, ML_Frame **mf_array)   // to call once at the beginning of the Soft and for each daqlink (1 to max 4 per crate).
//============================================================================== //
{

  SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success; 
  
  *eventBuffer = malloc(SIZE_OF_ML_BUFFER * sizeof(char));
  *mf_array = malloc(MAX_EXPECTED_FRAMES * sizeof(ML_Frame));
  


  return errCode; 
}


//============================================================================== //
 SAMPIC256CH_ErrCode  SAMPIC256CH_InitEvent(EventStruct *event) 
//============================================================================== //
 {
	
	int frame;
	
	event->NbOfHitsInEvent = 0;	
	event->TriggerData.RawDataSize = 0;
	event->TriggerData.NbOfTriggers = 0;
	
   for (frame = 0; frame <MAX_EXPECTED_FRAMES; frame++)
   {
	   
	  	Reset_HitInEvent (event,frame); 
	   
   }

 	return SAMPIC256CH_Success; 
 }
 
//============================================================================== //
SAMPIC256CH_ErrCode SAMPIC256CH_StartRun(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, Boolean resync)
//============================================================================== //
{

  SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success;           
  
  int dummy= 0; 
  int feBoard, feIndex;
  int deviceHandle;
  int max_data_frame_size;
  int max_trigger_frame_size;
  int max_frame_size;
  int nbOfFramesPerBlock;
  Boolean standardFirmware, enableExtTriggerCounter, enableDetectExtTriggerID;
  unsigned char nbOfTriggersPerEvent;
  
  deviceHandle = crateInfoParams->ConnectionInfo.CtrlDeviceHandle;
  
 
  if(crateInfoParams->ConnectionInfo.ConnectionType == UDP_CONNECTION)
  {
	  // nbOfFramesPerBlock = 31;
	  // nbOfFramesPerBlock = 1;
	   
	   SAMPIC256CH_GetNbOfFramesPerBlock(crateParams, &nbOfFramesPerBlock);
			
	   max_data_frame_size = (nbOfFramesPerBlock+1)*MAX_SINGLE_FRAME_SIZE;
	   
	   SAMPIC256CH_GetExternalTriggerCounterMode(crateParams, &enableExtTriggerCounter, &enableDetectExtTriggerID);
 	   SAMPIC256CH_GetMinNbOfTriggersPerEvent(crateParams, &nbOfTriggersPerEvent);

	   if(((crateInfoParams->CrateBoardsInfo.ControlBoardInfo.FirmwareVersion.FPGAEvolution & 0x40) >> 5) == 0)
	  	standardFirmware = TRUE;
   	   else
	    standardFirmware = FALSE; // T2K Firmware
	   
	   if(enableExtTriggerCounter == TRUE)
	   {
			if( standardFirmware == TRUE)
				   max_trigger_frame_size = ((int)nbOfTriggersPerEvent+1)*SINGLE_TRIGGER_DATA_SIZE;
			else
			 	   max_trigger_frame_size = ((int)nbOfTriggersPerEvent+1)*SINGLE_T2K_TRIGGER_DATA_SIZE;  	
	   }
	   else
		  max_trigger_frame_size = 0; 
		   

	   if(max_data_frame_size >= max_trigger_frame_size)
		   max_frame_size = max_data_frame_size;
	   else
		  max_frame_size = max_trigger_frame_size;
	   
	   lpdSetDAQMaxFrameSize(deviceHandle,1);
	   lpdSetDAQMaxFrameSize(deviceHandle,max_frame_size); 
	  
	   crateParams->ControlBoardParams.EnableFlowControl = TRUE;
	  
	   lpdSetDAQFlowControl(deviceHandle,  crateParams->ControlBoardParams.EnableFlowControl, 20 /* ms*/);
	  
  }
  
  if(resync == TRUE) 
  {
	  if(errCode == SAMPIC256CH_Success) errCode =	Resync_System(crateInfoParams);
  }
  
  
  for(feBoard = 0; feBoard < crateInfoParams->NbOfFeBoards; feBoard++)
  {
	  for(feIndex = 0; feIndex < NB_OF_FE_FPGAS_IN_FE_BOARD; feIndex++)
	  {
		  
	  	crateParams->FeBoardParams[feBoard].FeFpgaParams[feIndex].EnableTrigFPGA = TRUE;
	  	crateParams->FeBoardParams[feBoard].FeFpgaParams[feIndex].EnableTrigSampic = TRUE; 
	  }
	 	  
  }
 
 
  
 	 
  Build_FeBoardsFeFpgasControlReg(crateParams);
  
  if(errCode == SAMPIC256CH_Success) errCode = Load_HardwareSetup(crateInfoParams, crateParams, FEB_FE_CONTROL_REG, ALL_FE_BOARDs, ALL_FE_BOARDs_FE_FPGAs, dummy);   
 
  for(feBoard = 0; feBoard < crateInfoParams->NbOfFeBoards; feBoard++)
  {
  	crateParams->FeBoardParams[feBoard].ControlFpgaParams.EnableTrigger = TRUE;
  }
  
 
  Build_FeBoardsCtrlFpgaControlReg (crateParams);
  
  if(errCode == SAMPIC256CH_Success) errCode = Load_HardwareSetup(crateInfoParams, crateParams, FEB_CTRL_CONTROL_REG, ALL_FE_BOARDs, dummy, dummy);   
  
  
  // Enable Trigger in the Controller Board
  
  crateParams->ControlBoardParams.EnableTrigger = TRUE;
  
  Build_ControlBoardControlReg(crateParams);
  
  if(errCode == SAMPIC256CH_Success) errCode = Load_HardwareSetup(crateInfoParams, crateParams, CTRLB_CONTROL_REG, dummy, dummy, dummy);   
  

  return errCode; 
}

//============================================================================== //

//============================================================================== //
SAMPIC256CH_ErrCode SAMPIC256CH_PrepareEvent(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams)		// first function to be called  at each event
//============================================================================== //
{

  SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success;  
  int feBoard, sampicIndex, channel;
  int n, nbOfFramesPerBlock;
  

  Boolean  sendSoftTrig, sendSoftPulse;

  sendSoftTrig = FALSE;
  sendSoftPulse = FALSE;
  
  if( crateParams->ControlBoardParams.ExternalTriggerType  == SOFTWARE)
  {
	 
	  for(feBoard = 0; feBoard < crateInfoParams->NbOfFeBoards; feBoard++)
	  {
		  
		for(sampicIndex = 0; sampicIndex < NB_OF_SAMPICS_IN_FE_BOARD; sampicIndex++)
		{
			
			for(channel = 0; channel < NB_OF_CHANNELS_IN_SAMPIC; channel++)
			{
				
				
				if( 
					(crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.ChannelTriggerMode[channel] == SAMPIC_CHANNEL_EXT_TRIGGER_MODE) &&
					(crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.EnableTriggerChannel[channel] == TRUE))
				{
					sendSoftTrig = TRUE;
					break;
					
				}
					
				
			}
			
		}
			  
		  
	  }
	  
  }
  
  
 if( (crateParams->CommonParams.EnPulseMode == TRUE) && (crateParams->ControlBoardParams.EnableAutoPulse == FALSE))
 {
	 
	sendSoftPulse = TRUE;	 
	 
 }
 
 if(sendSoftPulse == TRUE)
	 errCode = Send_PulseCmd(crateInfoParams);
 
 
 if(sendSoftTrig == TRUE)
 {
	 
	 
	nbOfFramesPerBlock= crateParams->CommonParams.NbOfFramesPerBlock;
	
	for(n=0; n< nbOfFramesPerBlock; n++)
	{
    	if(errCode == SAMPIC256CH_Success) errCode = Send_SoftTrig (crateInfoParams); 
	}
   
 }
	
  return errCode; 
}

//============================================================================== //


//============================================================================== //
SAMPIC256CH_ErrCode  SAMPIC256CH_AwakeAcquisition(CrateInfoStruct *crateInfoParams)		// function to be called every second ( < 1 Hz)if in UDP connection and very low events acquisition rate.
//============================================================================== //
{
	
	SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success;
	int dummy = 0;
	char sub_address = 0x0;
	unsigned char data;

	 if(crateInfoParams->ConnectionInfo.ConnectionType == UDP_CONNECTION)	
		errCode = SAMPIC256CH_BusReadWords (crateInfoParams,CTRL_ACCESS /* a changer en DAQ ACCESS */, CB_CTRL_FPGA, dummy, dummy, sub_address, &data,1);
	
	return errCode;

}

//============================================================================== //
SAMPIC256CH_ErrCode SAMPIC256CH_ReadEventBuffer(CrateInfoStruct *crateInfoParams,int feBoard /* feboard direct link index */,  void *eventBuffer, ML_Frame *mf_array, int *nframes)	// 2nd function to be called at each event
//============================================================================== //
{

int deviceHandle;
SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success;

  
  if( (crateInfoParams->ConnectionInfo.ControlBoardControlType == CTRL_ONLY)  &&  (crateInfoParams->ConnectionInfo.NbOfDAQConnections >0) )
  {
	  
	if(feBoard >= crateInfoParams->ConnectionInfo.NbOfDAQConnections)	  
		return SAMPIC256CH_InvalidParam;
	else
	 deviceHandle = crateInfoParams->ConnectionInfo.DaqHandle[feBoard];	
	  
  }
  
  if( crateInfoParams->ConnectionInfo.ControlBoardControlType == CTRL_AND_DAQ)
	  deviceHandle = crateInfoParams->ConnectionInfo.CtrlDeviceHandle;
  
  if(crateInfoParams->ConnectionInfo.ConnectionType == USB_CONNECTION)
  {
	  errCode = SAMPIC256CH_BusReadExtended(deviceHandle, eventBuffer,mf_array, MAX_BYTES_TO_READ, nframes); 
  }
  else if(crateInfoParams->ConnectionInfo.ConnectionType == UDP_CONNECTION) 
  {
	 //dsleep(1);   /* ms*/
	 errCode = SAMPIC256CH_DAQBusReadExtended(deviceHandle, eventBuffer,mf_array, MAX_BYTES_TO_READ, nframes);       
	  
  }
  
  // UDP connection plus tard

// if(UdpId > 0)
//	{
//		 //dsleep(0.1);
//		 //bytecount = UdpReadExML_New(UdpId, TempBuf, My_MLFrames, MAX_BYTES_TO_READ, &nframes);     
//		 bytecount = UdpReadDAQStream(UdpId, TempBuf, My_MLFrames, MAX_BYTES_TO_READ, &nframes);  
//	}
//	
  return errCode; 
}

//============================================================================== //

//============================================================================== //
SAMPIC256CH_ErrCode SAMPIC256CH_DecodeEvent(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, ML_Frame *mf_array, EventStruct *event, int nframes, int *numberOfHits) // 3rd function to be called at each event
//============================================================================== //
{

  SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success;    
  
 int sample,n, frame, channel;
 int adcLatchedIndex;
 int cellInfoIndex;
 int fpgaTimeStampIndex;

 int timeStampAIndex;
 int timeStampBIndex;
 int samplesInitIndex;
 int rawGrayData;
 int feBoardIndex, sampicIndex;
 int headerIndex;
 int physicalIndex;
 int offsetIndex;
 int totIndex;
 int startOfADCRampIndex;
 int sampicHeaderOffset;
 float totRampCurrentDac;
 int 	rawTOTValue;
 double cell0TimeInstant;
 Boolean standardFirmware = TRUE;
 int expectedTriggerDataSize;
 int nbOfTriggerPerEvent = 1;


 int singleTriggerDataSize = 8;
 int triggerDataOffset;

 Boolean lastTriggerPosition, currentTriggerPosition;

 
 float samplingPeriod;
 int hitNumber;
 int hitSize;
 int nframe;
 int maxADCValue;	
 int totdacIndex;
 
 Boolean enableAutoConv, enableToTMeasurement, smartRead;
 Boolean enableDetectTriggerIdDromExtTrig = FALSE;
 int nbOfFramesPerBlock, nbOfSamplesToRead;
 

   samplingPeriod = 1000000.0/(float)(crateParams->CommonParams.FreqEch);  // en ps
	
   if(crateParams->CommonParams.ADCNbOfBits ==  11)
		maxADCValue = ADC_11BITS_MAX_VALUE;
	else if(crateParams->CommonParams.ADCNbOfBits == 10) 
		maxADCValue = ADC_10BITS_MAX_VALUE;
	else if(crateParams->CommonParams.ADCNbOfBits == 9) 
		maxADCValue = ADC_9BITS_MAX_VALUE;
	else
		maxADCValue = ADC_8BITS_MAX_VALUE; 
 
 	   
	   
   headerIndex = 0;
   adcLatchedIndex = 1;
   fpgaTimeStampIndex = 3;

   sampicHeaderOffset = 2;
   
   cellInfoIndex = 8 + sampicHeaderOffset;
   
     

   enableAutoConv  = crateParams->CommonParams.EnableAutoConversionMode;
   enableToTMeasurement = crateParams->CommonParams.EnableTOTMeasurement; 
   smartRead =  crateParams->CommonParams.SmartRead;
   nbOfSamplesToRead =  crateParams->CommonParams.NbOfSamplesToRead;
   nbOfFramesPerBlock = crateParams->CommonParams.NbOfFramesPerBlock;
   enableDetectTriggerIdDromExtTrig = crateParams->ControlBoardParams.EnableDetectTriggerIDfromExtTrig;
   nbOfTriggerPerEvent = crateParams->ControlBoardParams.NbOfTriggersPerEvent;
   
      if(((crateInfoParams->CrateBoardsInfo.ControlBoardInfo.FirmwareVersion.FPGAEvolution & 0x40) >> 5) == 0)
   {
	   standardFirmware = TRUE;
	   expectedTriggerDataSize =   nbOfTriggerPerEvent * SINGLE_TRIGGER_DATA_SIZE;
	   
   }
   else
   {
	   standardFirmware = FALSE; // T2K Firmware
	   expectedTriggerDataSize =  nbOfTriggerPerEvent * SINGLE_T2K_TRIGGER_DATA_SIZE; 
	   
   }
	   
   if(enableToTMeasurement == FALSE)
   {
	  
	   
       if(crateParams->CommonParams.EnableAutoConversionMode == FALSE)
	   {
		   
		   samplesInitIndex = 16 + sampicHeaderOffset;
		   timeStampAIndex = 10 + sampicHeaderOffset;
	   	   timeStampBIndex = 12 + sampicHeaderOffset;  
	
			  
			   
	   }
	   else   // AutoConv
	   {
		   startOfADCRampIndex = 10 + sampicHeaderOffset;
		   timeStampAIndex = 12 + sampicHeaderOffset;
   	   	   timeStampBIndex = 14 + sampicHeaderOffset;
		   samplesInitIndex = 18 + sampicHeaderOffset;
 		   
	   }
   }
   else
   {
	   totIndex = 10 + sampicHeaderOffset;

	   if(enableAutoConv == TRUE)
	   {
		   
		   startOfADCRampIndex = 12 + sampicHeaderOffset;
		   timeStampAIndex = 14 + sampicHeaderOffset;
   	   	   timeStampBIndex = 16 + sampicHeaderOffset;
		   samplesInitIndex = 20 + sampicHeaderOffset;

		   
	   }
	   else
	   {
		   timeStampAIndex = 12 + sampicHeaderOffset;
   	   	   timeStampBIndex = 14 + sampicHeaderOffset;
		   samplesInitIndex = 18 + sampicHeaderOffset;
   	   }
	   
   }			

   
 	*numberOfHits = 0;

	hitNumber = 0;
	
	event->TriggerData.RawDataSize = 0;
	event->TriggerData.NbOfTriggers = 0;
	event->NbOfHitsInEvent = 0;
	
	 for(frame = 0; frame < nframes; frame++)
	 {  
		
		 
		if((mf_array[frame].path[0] == -1 /* layer 0 */) /*&& ((mf_array[frame].sub_address & 0x04) == 0x04)*/) //Trigger Data  
		{
			
		  /* if(mf_array[frame].data_size != expectedTriggerDataSize)
		   {
			   
				errCode = SAMPIC256CH_ErrInvalidTriggerDataEvent;
				return errCode;
   
		   } */
		   
		   for(n = 0; n < mf_array[frame].data_size; n++)
		   {
			  event->TriggerData.RawData[ event->TriggerData.RawDataSize + n] = mf_array[frame].user_data[n];  																		 
		   }
	   
		   event->TriggerData.RawDataSize += mf_array[frame].data_size;
		  
		   
		}
	    else  // Hit Data
		{
	 
			feBoardIndex = Get_FeBoardIndexFromFramePathIndex(crateInfoParams, mf_array[frame].path[0]);
			sampicIndex =  mf_array[frame].path[1];
		 
		    totRampCurrentDac =  crateParams->FeBoardParams[feBoardIndex].FeFpgaParams[sampicIndex].SAMPICIndividualParams.InternalTOTRampCurrentDAC;
 
		   totdacIndex = Get_TOTCurrentRampDacIndex(totRampCurrentDac);

		   nframe = 0; 
		   hitSize = (int) (mf_array[frame].data_size/(nbOfFramesPerBlock));
		  
		   
    	   if(hitSize  == 0)       
		   {
				errCode = SAMPIC256CH_ErrInvalidEvent;
				return errCode;
		   }
		   
		   
		   while(nframe < nbOfFramesPerBlock)
		   {		  
		    
			    offsetIndex = (int)hitSize*nframe;
	
				Reset_HitInEvent(event, hitNumber);
				
				event->Hit[hitNumber].HitNumber = hitNumber;
				event->Hit[hitNumber].FeBoardIndex  = feBoardIndex;
				event->Hit[hitNumber].SampicIndex = sampicIndex;
				
	    		if((unsigned char)mf_array[frame].user_data[headerIndex + offsetIndex] != DATA_HEADER)       
				{
					errCode = SAMPIC256CH_ErrInvalidEvent;
		
					return errCode; 
				}
	
				rawGrayData = ((int)mf_array[frame].user_data[adcLatchedIndex + offsetIndex]) + ((((int)mf_array[frame].user_data[adcLatchedIndex+1 + offsetIndex])&0x07)<<8); 
			
				event->Hit[hitNumber].AdvancedParams.ADCCounter_LatchedAtEndOfConv = Convert_FromGrayToBinary(rawGrayData,11);
		
				
			 	event->Hit[hitNumber].AdvancedParams.FPGATimeStamp = ((unsigned long long)mf_array[frame].user_data[fpgaTimeStampIndex + offsetIndex]) +
												 (((unsigned long long)mf_array[frame].user_data[fpgaTimeStampIndex+1 + offsetIndex])<<8) + 
												 (((unsigned long long)mf_array[frame].user_data[fpgaTimeStampIndex+2 + offsetIndex])<<16) + 
												 (((unsigned long long)mf_array[frame].user_data[fpgaTimeStampIndex+3 + offsetIndex])<<24) +
												 (((unsigned long long)mf_array[frame].user_data[fpgaTimeStampIndex+4 + offsetIndex])<<32); 
		
		
				//lastFPGATimeStamp = currentFPGATimeStamp;
	
		
				//currentFPGATimeStamp = event->Hit[hitNumber].AdvancedParams.FPGATimeStamp;
			
				event->Hit[hitNumber].ChannelIndex = (int)(((mf_array[frame].user_data[cellInfoIndex+1 + offsetIndex]&0x03)<< 2)+ ((mf_array[frame].user_data[cellInfoIndex + offsetIndex] & 0xC0)>>6));
				
			
				if(event->Hit[hitNumber].ChannelIndex >= NB_OF_CHANNELS_IN_SAMPIC)
				{
					errCode = SAMPIC256CH_ErrInvalidEvent;
					return errCode;
			
				}

				event->Hit[hitNumber].CellInfo =  mf_array[frame].user_data[cellInfoIndex + offsetIndex]& 0x3F;
				
				event->Hit[hitNumber].Channel= event->Hit[hitNumber].ChannelIndex + sampicIndex*NB_OF_CHANNELS_IN_SAMPIC;
			
			
				if(event->Hit[hitNumber].Channel >= NB_OF_CHANNELS_IN_FE_BOARD)
				{
					errCode = SAMPIC256CH_ErrInvalidEvent;
					return errCode;
			
				}

				    	
				//SampicTimeStampA
				
				rawGrayData = (((int)mf_array[frame].user_data[offsetIndex + timeStampAIndex]  // Bits 0 to 7
										+ ((int)(mf_array[frame].user_data[offsetIndex +timeStampAIndex+1]&0xF) <<8) // Bits 8 to 11
					                    + ((int)(mf_array[frame].user_data[offsetIndex +timeStampAIndex+2]&0xF) << 12)) &0xFFFF); // Bits 12 to 15  
	
				event->Hit[hitNumber].AdvancedParams.SampicTimeStampA= Convert_FromGrayToBinary(rawGrayData, 16);
				
				rawGrayData = 0xFFFF - (((((int)mf_array[frame].user_data[offsetIndex +timeStampBIndex]&0xF0)>>4) + 	// Bits 0 to 3
					          ((int)(mf_array[frame].user_data[offsetIndex +timeStampBIndex+1]&0xF)<<4) +   // Bits 4 to 7
							  ((int)(mf_array[frame].user_data[offsetIndex +timeStampBIndex+2])<<8) )&0xFFFF);// Bits 8 to 15
							  
							    
				event->Hit[hitNumber].AdvancedParams.SampicTimeStampB= Convert_FromGrayToBinary(rawGrayData, 16); 
				
		    	//START of ADC RAMP
				if(enableAutoConv == TRUE)
				{
					
					rawGrayData = ((((int)mf_array[frame].user_data[offsetIndex + startOfADCRampIndex])&0xFE)>>1) + ((int)mf_array[frame].user_data[offsetIndex +startOfADCRampIndex +1]<<7);    // On vire le bit de poids faible
					event->Hit[hitNumber].AdvancedParams.StartOfADCRamp = Convert_FromGrayToBinary(rawGrayData, 11);
				}
				
				if(enableToTMeasurement == TRUE)
				{
					//TOT
				
				
					rawGrayData = ((((int)mf_array[frame].user_data[offsetIndex +totIndex])&0xFE)>>1) + (((int)mf_array[frame].user_data[offsetIndex +totIndex+1]&0xF)<<7); // ADC Count 
 				    event->Hit[frame].RawTOTValue = (int)Convert_FromGrayToBinary(rawGrayData, 11);
					if(enableAutoConv == TRUE)
				    {
					  event->Hit[frame].RawTOTValue =  ((int)event->Hit[hitNumber].RawTOTValue -  event->Hit[hitNumber].AdvancedParams.StartOfADCRamp + maxADCValue)% maxADCValue;
				    } 
				    channel = event->Hit[hitNumber].Channel;
				    physicalIndex = MAX_NB_OF_SAMPLES -1;
					
					rawTOTValue =   event->Hit[frame].RawTOTValue;
					
					// on corrige la valeur avec une fonction qui fait les autres corrections également.
					event->Hit[frame].TOTValue = (crateInfoParams->CrateCalibInfo.CalibValues[feBoardIndex].TOTCalibSlope[totdacIndex][channel]* (float)rawTOTValue) + crateInfoParams->CrateCalibInfo.CalibValues[feBoardIndex].TOTCalibIntercept[totdacIndex][channel];
						
				}
				
				event->Hit[hitNumber].DataSize = 0;
		
				n = samplesInitIndex;
				sample = 0;
				lastTriggerPosition = 0;
				currentTriggerPosition = 0;
				event->Hit[hitNumber].FirstCellPhysicalIndex  = 0;
				event->Hit[hitNumber].AdvancedParams.TriggerPositionCell = 0;
					
				while((n< hitSize) && (sample < nbOfSamplesToRead ))   
				{   
					if((n%2 == 0) && (n >= samplesInitIndex))
					{
						
				

						rawGrayData = ((((int)mf_array[frame].user_data[offsetIndex +n])&0xFE)>>1) + ((int)mf_array[frame].user_data[offsetIndex +n+1]<<7);    // On vire le bit de poids faible
						
						event->Hit[hitNumber].RawDataSamples[sample] = (float)Convert_FromGrayToBinary(rawGrayData, 11);
				
						if(enableAutoConv == TRUE)
							event->Hit[hitNumber].RawDataSamples[sample] =  ((int)event->Hit[hitNumber].RawDataSamples[sample] -  event->Hit[hitNumber].AdvancedParams.StartOfADCRamp + maxADCValue)% maxADCValue;
					
						
						physicalIndex = (sample + event->Hit[hitNumber].CellInfo)%MAX_NB_OF_SAMPLES;
										  
				
						event->Hit[hitNumber].AdvancedParams.TriggerPosition[sample] = (int)mf_array[frame].user_data[offsetIndex +n]&0x1;
				
						lastTriggerPosition =  currentTriggerPosition;
				
						currentTriggerPosition = event->Hit[hitNumber].AdvancedParams.TriggerPosition[sample];
				
						if(currentTriggerPosition == 1 && lastTriggerPosition == 0)
						 event->Hit[hitNumber].AdvancedParams.FirstTriggerPositionCell = sample;
						
						if((currentTriggerPosition == 0) && (lastTriggerPosition == 1)  
							&& (nbOfSamplesToRead == MAX_NB_OF_SAMPLES) && (smartRead == FALSE)
							)
							event->Hit[hitNumber].FirstCellPhysicalIndex  = sample;
				
			
						(event->Hit[hitNumber].DataSize)++;
						sample++; 

					}

					n += 2;
				}
		
		
				
				
			
				
			   if(event->Hit[hitNumber].DataSize != nbOfSamplesToRead)
			   {
					errCode = SAMPIC256CH_ErrInvalidEvent;
					return errCode;
				
			   }
			  
			  
		       if((event->Hit[hitNumber].AdvancedParams.TriggerPosition[event->Hit[hitNumber].DataSize -1] == 1) &&  (event->Hit[hitNumber].AdvancedParams.TriggerPosition[0] == 0) 
				   &&  (nbOfSamplesToRead == MAX_NB_OF_SAMPLES) && (smartRead == FALSE)
					)
				   event->Hit[hitNumber].FirstCellPhysicalIndex = 0;
	   			
			   if( smartRead == FALSE )
			   {
					event->Hit[hitNumber].AdvancedParams.TriggerPositionCell = event->Hit[hitNumber].FirstCellPhysicalIndex;
			   }
			   else	   // smart Read
			   {
				    event->Hit[hitNumber].AdvancedParams.TriggerPositionCell = event->Hit[hitNumber].CellInfo; 
					event->Hit[hitNumber].AdvancedParams.FirstTriggerPositionCell  = (event->Hit[hitNumber].CellInfo -  crateParams->CommonParams.OffsetForStartOfRead + MAX_NB_OF_SAMPLES)%MAX_NB_OF_SAMPLES;
					event->Hit[hitNumber].FirstCellPhysicalIndex = event->Hit[hitNumber].CellInfo;   
			   }
				  

			    channel = event->Hit[hitNumber].Channel;
				
			    
			    for(n= 0; n <event->Hit[hitNumber].DataSize; n++)
				{
					if( smartRead == FALSE)  
						event->Hit[hitNumber].OrderedRawDataSamples[n]= event->Hit[hitNumber].RawDataSamples[(n+event->Hit[hitNumber].FirstCellPhysicalIndex)%MAX_NB_OF_SAMPLES];

					else
						event->Hit[hitNumber].OrderedRawDataSamples[n]= event->Hit[hitNumber].RawDataSamples[n];
					
					event->Hit[hitNumber].CorrectedDataSamples[n]= (float)event->Hit[hitNumber].OrderedRawDataSamples[n]; 
			
				}
				
			    if( smartRead == FALSE )
				{
					if(event->Hit[hitNumber].AdvancedParams.FirstTriggerPositionCell <= 32) 
						cell0TimeInstant = (double)(MAX_NB_OF_SAMPLES*(event->Hit[hitNumber].AdvancedParams.SampicTimeStampA + ((event->Hit[hitNumber].AdvancedParams.FPGATimeStamp - event->Hit[hitNumber].AdvancedParams.SampicTimeStampA) &0xFFFFFF0000)))*(samplingPeriod/1000);		//ns
					else
						cell0TimeInstant = (double)(MAX_NB_OF_SAMPLES*(event->Hit[hitNumber].AdvancedParams.SampicTimeStampB + ((event->Hit[hitNumber].AdvancedParams.FPGATimeStamp - event->Hit[hitNumber].AdvancedParams.SampicTimeStampB) &0xFFFFFF0000)))*(samplingPeriod/1000);		//ns
			
				}
				else // smartRead
				{
					if(event->Hit[hitNumber].AdvancedParams.FirstTriggerPositionCell <= 32) 
						cell0TimeInstant = (double)(MAX_NB_OF_SAMPLES*(event->Hit[hitNumber].AdvancedParams.SampicTimeStampA + ((event->Hit[hitNumber].AdvancedParams.FPGATimeStamp - event->Hit[hitNumber].AdvancedParams.SampicTimeStampA) &0xFFFFFF0000)))*(samplingPeriod/1000);		//ns
					else
						cell0TimeInstant = (double)(MAX_NB_OF_SAMPLES*(event->Hit[hitNumber].AdvancedParams.SampicTimeStampB + ((event->Hit[hitNumber].AdvancedParams.FPGATimeStamp - event->Hit[hitNumber].AdvancedParams.SampicTimeStampB) &0xFFFFFF0000)))*(samplingPeriod/1000);		//ns
			
				}
			
		
			    event->Hit[hitNumber].AdvancedParams.PhysicalCell0TimeStamp =  cell0TimeInstant;
		 
				if( smartRead == FALSE )
				{
  	
					if(event->Hit[hitNumber].AdvancedParams.FirstTriggerPositionCell > event->Hit[hitNumber].FirstCellPhysicalIndex)    
						event->Hit[hitNumber].FirstCellTimeStamp = cell0TimeInstant + (event->Hit[hitNumber].FirstCellPhysicalIndex)*(samplingPeriod/1000);
					else
						event->Hit[hitNumber].FirstCellTimeStamp = cell0TimeInstant - (MAX_NB_OF_SAMPLES - event->Hit[hitNumber].FirstCellPhysicalIndex)*(samplingPeriod/1000); 
  	
				}
				else // SmartRead
				{
					if(event->Hit[hitNumber].CellInfo < crateParams->CommonParams.OffsetForStartOfRead)    
						event->Hit[hitNumber].FirstCellTimeStamp = cell0TimeInstant	 + event->Hit[hitNumber].CellInfo*(samplingPeriod/1000);
					else
						event->Hit[hitNumber].FirstCellTimeStamp = cell0TimeInstant - (MAX_NB_OF_SAMPLES - event->Hit[hitNumber].CellInfo)*(samplingPeriod/1000); 
				}
				
		
			 // APPLY Corrections on DATA
			 if(crateParams->CommonParams.ADCLinearityCorrection == TRUE)
				 Correct_AdcLinearity(crateInfoParams,&event->Hit[hitNumber], smartRead);  
		
			 if(crateParams->CommonParams.ResidualPedestalCorrection == TRUE)
				 Correct_ResiudalPedestals(crateInfoParams,&event->Hit[hitNumber]);
			 
			 if(crateParams->CommonParams.INLCorrection == TRUE)
				 Correct_TimeLinearity(crateInfoParams,&event->Hit[hitNumber]);
			 
		     nframe++;
	   		 hitNumber++;
			
	
		   }
		
		}
	    
	 
	 }
	
	 
 	 *numberOfHits = hitNumber; 
	 event->NbOfHitsInEvent = hitNumber;   
	 
	 //decoding TriggerData
	 
	 if(standardFirmware == TRUE)
	 {
		 singleTriggerDataSize = SINGLE_TRIGGER_DATA_SIZE;
		 event->TriggerData.NbOfTriggers = event->TriggerData.RawDataSize/singleTriggerDataSize;
	 
	 	for(n=0; n <event->TriggerData.NbOfTriggers; n++)
	 	{
		   triggerDataOffset = n*singleTriggerDataSize;
			if(enableDetectTriggerIdDromExtTrig == FALSE)
			{
			  		event->TriggerData.TriggerIDFromFPGA[n] = (int)event->TriggerData.RawData[triggerDataOffset] +
				   						 (((int)event->TriggerData.RawData[triggerDataOffset+1])<<8) +
										 (((int)event->TriggerData.RawData[triggerDataOffset+2])<<16);
					
					event->TriggerData.TriggerIDFromExtTrig[n]= 0;
	
			}
			else // detect TriggerID from Ext Trig
			{
			    event->TriggerData.TriggerIDFromFPGA[n] = (int)event->TriggerData.RawData[triggerDataOffset];
				event->TriggerData.TriggerIDFromExtTrig[n]=  ((int)event->TriggerData.RawData[triggerDataOffset+1] + (((int)event->TriggerData.RawData[triggerDataOffset+2])<<8));
		
			 }

		   event->TriggerData.TriggerTimeStamp[n] =  (double)((unsigned long long)event->TriggerData.RawData[triggerDataOffset + 3] +
			   								(((unsigned long long)event->TriggerData.RawData[triggerDataOffset + 4])<<8) +
											(((unsigned long long)event->TriggerData.RawData[triggerDataOffset + 5])<<16) + 
											(((unsigned long long)event->TriggerData.RawData[triggerDataOffset + 6])<<24) + 
											(((unsigned long long)event->TriggerData.RawData[triggerDataOffset + 7])<<32));
	   
		   event->TriggerData.TriggerTimeStamp[n] = (double)((event->TriggerData.TriggerTimeStamp[n]*(double)samplingPeriod*MAX_NB_OF_SAMPLES)/1000.0);        
		   event->TriggerData.SpillNumberFromExtTrig[n]= 0;
		   event->TriggerData.RawExtraWord[n]= 0;
		 
		 
		}
	 }
	 else  // T2K firmware
	 {
		singleTriggerDataSize = SINGLE_T2K_TRIGGER_DATA_SIZE; 
		 
		event->TriggerData.NbOfTriggers = event->TriggerData.RawDataSize/SINGLE_T2K_TRIGGER_DATA_SIZE;
	    

		
	 	for(n=0; n <event->TriggerData.NbOfTriggers; n++)
	 	{
			
			triggerDataOffset = n*singleTriggerDataSize;
			
			if((unsigned char)event->TriggerData.RawData[triggerDataOffset] != DATA_HEADER)       
			{
				
				errCode = SAMPIC256CH_ErrInvalidTriggerDataEvent;
		
				return errCode; 
				
			}
			
			event->TriggerData.TriggerIDFromFPGA[n] = (int)(event->TriggerData.RawData[triggerDataOffset+1] +
			   						 (((int)event->TriggerData.RawData[triggerDataOffset+2])<<8));

			
			event->TriggerData.TriggerTimeStamp[n] =  (double)((unsigned long long)event->TriggerData.RawData[triggerDataOffset + 3] +
		   								(((unsigned long long)event->TriggerData.RawData[triggerDataOffset + 4])<<8) +
										(((unsigned long long)event->TriggerData.RawData[triggerDataOffset + 5])<<16) + 
										(((unsigned long long)event->TriggerData.RawData[triggerDataOffset + 6])<<24) + 
										(((unsigned long long)event->TriggerData.RawData[triggerDataOffset + 7])<<32));
	   
	   		event->TriggerData.TriggerTimeStamp[n] = (double)((event->TriggerData.TriggerTimeStamp[n]*(double)samplingPeriod*MAX_NB_OF_SAMPLES)/1000.0);
	
		    event->TriggerData.TriggerIDFromExtTrig[n]=   (unsigned int)((unsigned int)event->TriggerData.RawData[triggerDataOffset + 8] +
		   								(((unsigned int)event->TriggerData.RawData[triggerDataOffset + 9])<<8) +
										(((unsigned int)event->TriggerData.RawData[triggerDataOffset + 10])<<16) + 
										(((unsigned int)event->TriggerData.RawData[triggerDataOffset + 11])<<24));
		 
		 
		
	   		event->TriggerData.SpillNumberFromExtTrig[n]=   (unsigned short)((unsigned short)event->TriggerData.RawData[triggerDataOffset + 12] +
		   								(((unsigned short)event->TriggerData.RawData[triggerDataOffset + 13])<<8));
			
			event->TriggerData.RawExtraWord[n]=   (unsigned short)((unsigned short)event->TriggerData.RawData[triggerDataOffset + 14] +
		   								(((unsigned short)event->TriggerData.RawData[triggerDataOffset + 15])<<8));
			
		}
	 
		
	 }
	 
	
	 
		
	
	 
 return errCode; 
}

//============================================================================== //

//============================================================================== //
SAMPIC256CH_ErrCode SAMPIC256CH_Purge_AllDaqBuffersChain (CrateInfoStruct *crateInfoParams , int maxloops)
//============================================================================== //
{
     SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success; 

	 errCode =  Purge_AllDaqBuffersChain (crateInfoParams,maxloops);
	 
	 return errCode;
	
}

//============================================================================== //
SAMPIC256CH_ErrCode SAMPIC256CH_StopRun(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams)	   // to be called to stop the run. 
//============================================================================== //
{
													   
SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success;           
SAMPIC256CH_ErrCode purgeBufferErrCode = SAMPIC256CH_PurgeBufferIncomplete;  
int dummy= 0, feBoard, sampicIndex, n;

int deviceHandle;
    

 deviceHandle = crateInfoParams->ConnectionInfo.CtrlDeviceHandle;

  if(crateInfoParams->ConnectionInfo.ConnectionType == UDP_CONNECTION)
  {				  
	 crateParams->ControlBoardParams.EnableFlowControl = FALSE;
	  
	 lpdSetDAQFlowControl(deviceHandle,  crateParams->ControlBoardParams.EnableFlowControl, 20 /* ms*/);    
  										  
  }
 

  crateParams->ControlBoardParams.EnableTrigger = FALSE;
  
  Build_ControlBoardControlReg(crateParams);
  
  if(errCode == SAMPIC256CH_Success) errCode = Load_HardwareSetup(crateInfoParams, crateParams, CTRLB_CONTROL_REG, dummy, dummy, dummy);   
   
  //dsleep(10);
  if(crateInfoParams->ConnectionInfo.ConnectionType == USB_CONNECTION)
  { 
    Purge_AllDaqBuffersChain (crateInfoParams, -1);
  }
  else
  {
	 while(purgeBufferErrCode ==  SAMPIC256CH_PurgeBufferIncomplete)
	 {
	  	purgeBufferErrCode = Purge_AllDaqBuffersChain (crateInfoParams, 1); 	 
		Load_HardwareSetup(crateInfoParams, crateParams, CTRLB_CONTROL_REG, dummy, dummy, dummy);    
	 }
	  
  }
  
  
  for(feBoard = 0; feBoard < crateInfoParams->NbOfFeBoards; feBoard++)
  {
  	crateParams->FeBoardParams[feBoard].ControlFpgaParams.EnableTrigger = FALSE;
  }
  
 
  Build_FeBoardsCtrlFpgaControlReg (crateParams);
  
  if(errCode == SAMPIC256CH_Success) errCode = Load_HardwareSetup(crateInfoParams, crateParams, FEB_CTRL_CONTROL_REG, ALL_FE_BOARDs, dummy, dummy);   
  
  Purge_AllDaqBuffersChain (crateInfoParams, -1);
	   
	   
   for(feBoard = 0; feBoard < crateInfoParams->NbOfFeBoards; feBoard++)
   {
	  for(sampicIndex = 0; sampicIndex < NB_OF_FE_FPGAS_IN_FE_BOARD; sampicIndex++)
	  {
		  
	  	crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].EnableTrigFPGA = FALSE;
	  	crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].EnableTrigSampic = FALSE; 
	  }
	 	  
  }
 	

  Build_FeBoardsFeFpgasControlReg(crateParams);

  if(errCode == SAMPIC256CH_Success) errCode = Load_HardwareSetup(crateInfoParams, crateParams, FEB_FE_CONTROL_REG, ALL_FE_BOARDs, ALL_FE_BOARDs_FE_FPGAs, dummy);   

  if(errCode == SAMPIC256CH_Success) errCode = SAMPIC256CH_SetNbOfFramesPerBlock(crateInfoParams, crateParams, 1);  
	
  dsleep(10);   

  Purge_AllDaqBuffersChain (crateInfoParams, -1);  
 
  if(errCode == SAMPIC256CH_Success) errCode = Reset_ExtTrig(crateInfoParams);   
	
  if(errCode == SAMPIC256CH_Success) errCode =  Reset_SAMPICSlowControl(crateInfoParams); 
		
  if(errCode == SAMPIC256CH_Success) errCode = Reset_SAMPICDLL (crateInfoParams,crateParams);

  /* Compute_NbOfFramesPerBlock(); */
  
   Purge_AllDaqBuffersChain (crateInfoParams, -1);      
   
//	if(UdpId > 0)
//	{
//		
//	 CtrlFPGAParams.ReaqReqDelay = 0; //ns
//	 Load_HardwareSetup(CTRL_READ_REQ_DELAY, 0, 0);
//	 
//	}
  
  return errCode; 
}
		

//============================================================================== //
SAMPIC256CH_ErrCode  SAMPIC256CH_FreeEventMemory(void **eventBuffer, ML_Frame **mf_array)   // to call once at the beginning of the Soft and for each daqlink (1 to max 4 per crate).
//============================================================================== //
 {

  SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success;           
   
  free(*eventBuffer);
  free(*mf_array);


  return errCode; 
}


// ADVANCED FEATURES
//============================================================================== //

//============================================================================== //
SAMPIC256CH_ErrCode     SAMPIC256CH_SetAutoConversionMode(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, Boolean  autoConv)
//============================================================================== //
{
	
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;  
	int dummy = 0;
	
	crateParams->CommonParams.EnableAutoConversionMode  = autoConv;  
	Build_FeBoardsSampicsConfigReg2(crateParams);

	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_SAMPIC_SC_REG2, ALL_FE_BOARDs, ALL_FE_BOARDs_FE_FPGAs, dummy); 

	Compute_NbOfExtraWords(crateParams);
	
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_FE_SAMPIC_READ_LENGTH, ALL_FE_BOARDs, ALL_FE_BOARDs_FE_FPGAs, dummy);     
	return error;
	
}
//============================================================================== //
SAMPIC256CH_ErrCode     SAMPIC256CH_GetAutoConversionMode(CrateParamStruct *crateParams, Boolean  *autoConv)
//============================================================================== //
{
	
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;  
	*autoConv = crateParams->CommonParams.EnableAutoConversionMode;


	return error;
	
	
}


//============================================================================== //
SAMPIC256CH_ErrCode     SAMPIC256CH_SetNbOfFramesPerBlock(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int nbOfFramesPerBlock)
//============================================================================== //
{
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success; 
	
	int dummy = 0;  
		
	if(nbOfFramesPerBlock < 1 || nbOfFramesPerBlock > 31)
		return SAMPIC256CH_InvalidParam;
	
	 crateParams->CommonParams.NbOfFramesPerBlock =  nbOfFramesPerBlock;
	 
	 if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_FE_NB_OF_FRAMES_PER_BLOCK, ALL_FE_BOARDs, ALL_FE_BOARDs_FE_FPGAs, dummy); 

	
	 return error;
	
}

//============================================================================== //
SAMPIC256CH_ErrCode     SAMPIC256CH_GetNbOfFramesPerBlock(CrateParamStruct *crateParams, int *nbOfFramesPerBlock)
//============================================================================== //
{
	
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;
	
	*nbOfFramesPerBlock = crateParams->CommonParams.NbOfFramesPerBlock;
	
	return error;
	
	
}


//============================================================================== //
SAMPIC256CH_ErrCode     SAMPIC256CH_SetConversionLength(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, unsigned char convertLength)
//============================================================================== //
{
	int dummy = 0;
	
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;  
	
	crateParams->CommonParams.ConvertLength  = convertLength;  
	Build_FeBoardsSampicsConfigReg2(crateParams);
	
 	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_FE_CONVERT_LENGTH_REG, ALL_FE_BOARDs, ALL_FE_BOARDs_FE_FPGAs, dummy); 

	return error;
	
}

//============================================================================== //
SAMPIC256CH_ErrCode     SAMPIC256CH_GetConversionLength(CrateParamStruct *crateParams, unsigned char *convertLength)
//============================================================================== //
{
	
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;  
	*convertLength = crateParams->CommonParams.ConvertLength;
	
	return error;
	
	
	
}



//============================================================================== //
SAMPIC256CH_ErrCode     SAMPIC256CH_SetCrateSycnhronisationMode(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, Boolean  syncMode , Boolean masterMode , Boolean coincidenceMode)
//============================================================================== //
{
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;
	int dummy  =0;
	
	crateParams->ControlBoardParams.CrateIsInStandaloneMode = (1 - syncMode); 
	crateParams->ControlBoardParams.CrateIsInMasterSyncMode = masterMode; 
    crateParams->ControlBoardParams.EnableMasterSlaveCoincidence = 	coincidenceMode;
	
	Build_ControlBoardControlReg2(crateParams);
	
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams,CTRLB_CONTROL_REG2 , dummy, dummy, dummy); 
	
	if(coincidenceMode == TRUE)
		SAMPIC256CH_SetLevel2LatencyGateLength(crateInfoParams, crateParams, 7);
	else
		SAMPIC256CH_SetLevel2LatencyGateLength(crateInfoParams, crateParams, 3); 	
	
	if(error == SAMPIC256CH_Success) error = Reset_SAMPICDLL (crateInfoParams,crateParams); 
	
	
	return error;  
	
	
}

//============================================================================== //
SAMPIC256CH_ErrCode     SAMPIC256CH_GetCrateSycnhronisationMode(CrateParamStruct *crateParams, Boolean  *syncMode, Boolean *masterMode, Boolean *coincidenceMode)
//============================================================================== //
{
	

	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;
	
	*syncMode = (1- crateParams->ControlBoardParams.CrateIsInStandaloneMode);
	 
	*masterMode = 	crateParams->ControlBoardParams.CrateIsInMasterSyncMode;
	*coincidenceMode = crateParams->ControlBoardParams.EnableMasterSlaveCoincidence;
	
	return error;     	
}



//============================================================================== //
SAMPIC256CH_ErrCode     SAMPIC256CH_SetExternalTriggerCounterMode(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, Boolean  enableExtTriggerCounter /* '1' Crate is Enable, '0' Disable */, Boolean enableDetectExtTriggerID /* '1' enable detect external trigger ID from ExtTrig */)
//============================================================================== //
{
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;   	
	int dummy  =0;  
	
	crateParams->ControlBoardParams.EnableExtTrigCounter =  enableExtTriggerCounter;
	crateParams->ControlBoardParams.EnableDetectTriggerIDfromExtTrig =  enableDetectExtTriggerID; 
	
	Build_ControlBoardControlReg(crateParams);   
	
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams,CTRLB_CONTROL_REG , dummy, dummy, dummy); 
	
	return error;   
}

//============================================================================== //
SAMPIC256CH_ErrCode     SAMPIC256CH_GetExternalTriggerCounterMode(CrateParamStruct *crateParams, Boolean  *enableExtTriggerCounter /* '1' Crate is Enable, '0' Disable */, Boolean *enableDetectExtTriggerID /* '1' enable detect external trigger ID from ExtTrig */)
//============================================================================== //
{
	
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;  
	
	*enableExtTriggerCounter =  crateParams->ControlBoardParams.EnableExtTrigCounter;
	*enableDetectExtTriggerID = crateParams->ControlBoardParams.EnableDetectTriggerIDfromExtTrig ;
	 
	 
	return error;  
	
}


//============================================================================== //
SAMPIC256CH_ErrCode     SAMPIC256CH_SetMinNbOfTriggersPerEvent(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, unsigned char nbOfTriggersPerEvent /* between 1 and 255 */)
//============================================================================== //
{
	
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;   	
	int dummy  =0;  
	Boolean standardFirmware;
	
	if(((crateInfoParams->CrateBoardsInfo.ControlBoardInfo.FirmwareVersion.FPGAEvolution & 0x40) >> 5) == 0)
	  	standardFirmware = TRUE;
   	else
	    standardFirmware = FALSE; // T2K Firmware

	if((standardFirmware == TRUE) && (nbOfTriggersPerEvent <= 0 || nbOfTriggersPerEvent > 255 ))
		return SAMPIC256CH_InvalidParam;  
	
	if((standardFirmware == FALSE) && (nbOfTriggersPerEvent <= 0 || nbOfTriggersPerEvent > 127 ))	  // T2K
		return SAMPIC256CH_InvalidParam;  

	crateParams->ControlBoardParams.NbOfTriggersPerEvent =  nbOfTriggersPerEvent;
	
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, CTRLB_NB_OF_TRIGGER_PER_EVENT, dummy, dummy, dummy); 
	
	return error;   

	
}

//============================================================================== //
SAMPIC256CH_ErrCode     SAMPIC256CH_GetMinNbOfTriggersPerEvent(CrateParamStruct *crateParams, unsigned char *nbOfTriggersPerEvent)
//============================================================================== //
{
	
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;   	
	
	*nbOfTriggersPerEvent =  crateParams->ControlBoardParams.NbOfTriggersPerEvent;
	
	return error;  
	
}


//============================================================================== //
SAMPIC256CH_ErrCode 	SAMPIC256CH_SetSampicADCRampValue(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, float adcRampValue)  
//============================================================================== //
{
	
SAMPIC256CH_ErrCode error = SAMPIC256CH_Success; 
	
int startSampicIndex, endSampicIndex, sampicIdx;
int feboardIdx, startFeIndex,endFeIndex;  
int dummy = 0;

	if(feBoard >=  crateInfoParams->NbOfFeBoards)
		return SAMPIC256CH_InvalidParam;
	

	if(feBoard < 0)
	{
		startFeIndex = 0;
		endFeIndex = crateInfoParams->NbOfFeBoards -1;
	}
	else
	{
		startFeIndex = feBoard;
		endFeIndex = feBoard;
	}
  	

	if(sampicIndex < 0)
	{
		startSampicIndex = 0;
		endSampicIndex = NB_OF_SAMPICS_IN_FE_BOARD -1;
		
	}
	else
	{
	    startSampicIndex = sampicIndex;
		endSampicIndex = sampicIndex;
		
	}
	
	if(adcRampValue < MIN_DAC_RAW_VALUE || adcRampValue > MAX_DAC_RAW_VALUE)
		return SAMPIC256CH_InvalidParam;
	
	
	for(feboardIdx = startFeIndex; feboardIdx <=  endFeIndex;  feboardIdx++)
	{
	  for(sampicIdx = startSampicIndex; sampicIdx <= endSampicIndex; sampicIdx++)
	  {
		  
		crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.Vdac_ramp = adcRampValue;  
	  }
	  
	}

	Build_FeBoardsSampicsConfigReg6(crateParams);

   if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_SAMPIC_SC_REG6, feBoard, sampicIndex, dummy); 

   return error;  

}

//============================================================================== //
SAMPIC256CH_ErrCode 	SAMPIC256CH_GetSampicADCRampValue(CrateParamStruct *crateParams, int feBoard, int sampicIndex, float *adcRampValue)
//============================================================================== //
{
	
	
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;  
	if(feBoard >= MAX_NB_OF_FE_BOARDS)
		return SAMPIC256CH_InvalidParam;

	if(sampicIndex >= NB_OF_SAMPICS_IN_FE_BOARD)
		return SAMPIC256CH_InvalidParam; 

	if(feBoard < 0 || sampicIndex < 0)
		return SAMPIC256CH_InvalidParam;
	 
	*adcRampValue =  crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.Vdac_ramp;
	
	return error;
	
	
	
}



//============================================================================== //
SAMPIC256CH_ErrCode 	SAMPIC256CH_SetSampicVdacDLLValue(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, float vdacDllValue)  
//============================================================================== //
{
	
SAMPIC256CH_ErrCode error = SAMPIC256CH_Success; 
	
int startSampicIndex, endSampicIndex, sampicIdx;
int feboardIdx, startFeIndex,endFeIndex;  
int dummy = 0;

	if(feBoard >=  crateInfoParams->NbOfFeBoards)
		return SAMPIC256CH_InvalidParam;
	

	if(feBoard < 0)
	{
		startFeIndex = 0;
		endFeIndex = crateInfoParams->NbOfFeBoards -1;
	}
	else
	{
		startFeIndex = feBoard;
		endFeIndex = feBoard;
	}
  	

	if(sampicIndex < 0)
	{
		startSampicIndex = 0;
		endSampicIndex = NB_OF_SAMPICS_IN_FE_BOARD -1;
		
	}
	else
	{
	    startSampicIndex = sampicIndex;
		endSampicIndex = sampicIndex;
		
	}
	
	if(vdacDllValue < MIN_DAC_RAW_VALUE || vdacDllValue > MAX_DAC_RAW_VALUE)
		return SAMPIC256CH_InvalidParam;
	
	
	for(feboardIdx = startFeIndex; feboardIdx <=  endFeIndex;  feboardIdx++)
	{
	  for(sampicIdx = startSampicIndex; sampicIdx <= endSampicIndex; sampicIdx++)
	  {
		  
		crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.Vdac_DLL = vdacDllValue;  
		
		if(error == SAMPIC256CH_Success) error =  Load_DACs(crateInfoParams, crateParams,  FEB_VDAC_DLL,  feboardIdx, sampicIdx);  
	  }
	  
	}
	
	    				    
	if(error == SAMPIC256CH_Success) error = Reset_SAMPICDLL (crateInfoParams,crateParams);		 //??

   return error;  
	
	
}

//============================================================================== //
SAMPIC256CH_ErrCode 	SAMPIC256CH_GetSampicVdacDLLValue(CrateParamStruct *crateParams, int feBoard, int sampicIndex, float *vdacDllValue)
//============================================================================== //
{
	
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;  
	if(feBoard >= MAX_NB_OF_FE_BOARDS)
		return SAMPIC256CH_InvalidParam;

	if(sampicIndex >= NB_OF_SAMPICS_IN_FE_BOARD)
		return SAMPIC256CH_InvalidParam; 

	if(feBoard < 0 || sampicIndex < 0)
		return SAMPIC256CH_InvalidParam;
	 
	*vdacDllValue =  crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.Vdac_DLL;
	
	return error;
	
	
}





//============================================================================== //
SAMPIC256CH_ErrCode 	SAMPIC256CH_SetSampicVdacDLLContinuity(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, float vdacDllContinuityValue)  
//============================================================================== //
{
	
SAMPIC256CH_ErrCode error = SAMPIC256CH_Success; 
	
int startSampicIndex, endSampicIndex, sampicIdx;
int feboardIdx, startFeIndex,endFeIndex;  
int dummy = 0;

	if(feBoard >=  crateInfoParams->NbOfFeBoards)
		return SAMPIC256CH_InvalidParam;
	

	if(feBoard < 0)
	{
		startFeIndex = 0;
		endFeIndex = crateInfoParams->NbOfFeBoards -1;
	}
	else
	{
		startFeIndex = feBoard;
		endFeIndex = feBoard;
	}
  	

	if(sampicIndex < 0)
	{
		startSampicIndex = 0;
		endSampicIndex = NB_OF_SAMPICS_IN_FE_BOARD -1;
		
	}
	else
	{
	    startSampicIndex = sampicIndex;
		endSampicIndex = sampicIndex;
		
	}
	
	if(vdacDllContinuityValue < MIN_DAC_RAW_VALUE || vdacDllContinuityValue > MAX_DAC_RAW_VALUE)
		return SAMPIC256CH_InvalidParam;
	
	
	for(feboardIdx = startFeIndex; feboardIdx <=  endFeIndex;  feboardIdx++)
	{
	  for(sampicIdx = startSampicIndex; sampicIdx <= endSampicIndex; sampicIdx++)
	  {
		crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.Vdac_DLLContinuity = vdacDllContinuityValue;  
	  }
	  
	}
	
	Build_FeBoardsSampicsConfigReg3(crateParams);
    if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_SAMPIC_SC_REG3, feBoard, sampicIndex, dummy);  	    				    
	if(error == SAMPIC256CH_Success) error = Reset_SAMPICDLL (crateInfoParams,crateParams);		 //??

   return error;  
	
	
}

//============================================================================== //
SAMPIC256CH_ErrCode 	SAMPIC256CH_GetSampicVdacDLLContinuity(CrateParamStruct *crateParams, int feBoard, int sampicIndex, float *vdacDllContinuityValue)
//============================================================================== //
{
	
	
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;  
	if(feBoard >= MAX_NB_OF_FE_BOARDS)
		return SAMPIC256CH_InvalidParam;

	if(sampicIndex >= NB_OF_SAMPICS_IN_FE_BOARD)
		return SAMPIC256CH_InvalidParam; 

	if(feBoard < 0 || sampicIndex < 0)
		return SAMPIC256CH_InvalidParam;
	 
	*vdacDllContinuityValue =  crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.Vdac_DLLContinuity;
	
	return error;
	
	
}


//============================================================================== //
SAMPIC256CH_ErrCode 	SAMPIC256CH_SetSampicDLLSpeedMode(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, SampicDLLModeType_t dllMode)  
//============================================================================== //
{
	
SAMPIC256CH_ErrCode error = SAMPIC256CH_Success; 
	
int startSampicIndex, endSampicIndex, sampicIdx;
int feboardIdx, startFeIndex,endFeIndex;  
int dummy = 0;

	if(feBoard >=  crateInfoParams->NbOfFeBoards)
		return SAMPIC256CH_InvalidParam;
	
	if(feBoard < 0)
	{
		startFeIndex = 0;
		endFeIndex = crateInfoParams->NbOfFeBoards -1;
	}
	else
	{
		startFeIndex = feBoard;
		endFeIndex = feBoard;
	}

	if(sampicIndex < 0)
	{
		startSampicIndex = 0;
		endSampicIndex = NB_OF_SAMPICS_IN_FE_BOARD -1;
		
	}
	else
	{
	    startSampicIndex = sampicIndex;
		endSampicIndex = sampicIndex;
		
	}
	
	
	for(feboardIdx = startFeIndex; feboardIdx <=  endFeIndex;  feboardIdx++)
	{
	  for(sampicIdx = startSampicIndex; sampicIdx <= endSampicIndex; sampicIdx++)
	  {
		crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.DLLMode = dllMode;  
	  }
	  
	}
	
	Build_FeBoardsSampicsConfigReg1(crateParams);
    if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_SAMPIC_SC_REG1, feBoard, sampicIndex, dummy);  
	if(error == SAMPIC256CH_Success) error = Reset_SAMPICDLL (crateInfoParams,crateParams);		 //??

    return error;  
	
}

//============================================================================== //
SAMPIC256CH_ErrCode 	SAMPIC256CH_GetSampicDLLSpeedMode(CrateParamStruct *crateParams, int feBoard, int sampicIndex, SampicDLLModeType_t *dllMode)
//============================================================================== //
{
	
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;  
	if(feBoard >= MAX_NB_OF_FE_BOARDS)
		return SAMPIC256CH_InvalidParam;

	if(sampicIndex >= NB_OF_SAMPICS_IN_FE_BOARD)
		return SAMPIC256CH_InvalidParam; 

	if(feBoard < 0 || sampicIndex < 0)
		return SAMPIC256CH_InvalidParam;
	 
	*dllMode =  crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.DLLMode;
	
	return error;
	
	
}

//============================================================================== //
SAMPIC256CH_ErrCode 	SAMPIC256CH_SetSampicOverflowDacValue(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, float overflowDacValue)  
//============================================================================== //
{
	
SAMPIC256CH_ErrCode error = SAMPIC256CH_Success; 
	
int startSampicIndex, endSampicIndex, sampicIdx;
int feboardIdx, startFeIndex,endFeIndex;  
int dummy = 0;

	if(feBoard >=  crateInfoParams->NbOfFeBoards)
		return SAMPIC256CH_InvalidParam;
	

	if(feBoard < 0)
	{
		startFeIndex = 0;
		endFeIndex = crateInfoParams->NbOfFeBoards -1;
	}
	else
	{
		startFeIndex = feBoard;
		endFeIndex = feBoard;
	}
  	

	if(sampicIndex < 0)
	{
		startSampicIndex = 0;
		endSampicIndex = NB_OF_SAMPICS_IN_FE_BOARD -1;
		
	}
	else
	{
	    startSampicIndex = sampicIndex;
		endSampicIndex = sampicIndex;
		
	}
	
	if(overflowDacValue < MIN_DAC_RAW_VALUE || overflowDacValue > MAX_DAC_RAW_VALUE)
		return SAMPIC256CH_InvalidParam;
	
	
	for(feboardIdx = startFeIndex; feboardIdx <=  endFeIndex;  feboardIdx++)
	{
	  for(sampicIdx = startSampicIndex; sampicIdx <= endSampicIndex; sampicIdx++)
	  {
		crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.InternalOverflowDAC = overflowDacValue;  
	  }
	  
	}
	
	Build_FeBoardsSampicsConfigReg7(crateParams);
    if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_SAMPIC_SC_REG7, feBoard, sampicIndex, dummy);  	    				    


   return error;  
	
	
}

//============================================================================== //
SAMPIC256CH_ErrCode 	SAMPIC256CH_GetSampicOverflowDacValue(CrateParamStruct *crateParams, int feBoard, int sampicIndex, float *overflowDacValue)
//============================================================================== //
{
	
	
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;  
	if(feBoard >= MAX_NB_OF_FE_BOARDS)
		return SAMPIC256CH_InvalidParam;

	if(sampicIndex >= NB_OF_SAMPICS_IN_FE_BOARD)
		return SAMPIC256CH_InvalidParam; 

	if(feBoard < 0 || sampicIndex < 0)
		return SAMPIC256CH_InvalidParam;
	 
	*overflowDacValue =  crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.InternalOverflowDAC;
	
	return error;
	
	
}


//============================================================================== //
SAMPIC256CH_ErrCode 	SAMPIC256CH_SetSampicVdacRosc(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, float vdacRosc)
//============================================================================== //
{
	
SAMPIC256CH_ErrCode error = SAMPIC256CH_Success; 
	
int startSampicIndex, endSampicIndex, sampicIdx;
int feboardIdx, startFeIndex,endFeIndex;  
int dummy = 0;

	if(feBoard >=  crateInfoParams->NbOfFeBoards)
		return SAMPIC256CH_InvalidParam;
	

	if(feBoard < 0)
	{
		startFeIndex = 0;
		endFeIndex = crateInfoParams->NbOfFeBoards -1;
	}
	else
	{
		startFeIndex = feBoard;
		endFeIndex = feBoard;
	}
  	

	if(sampicIndex < 0)
	{
		startSampicIndex = 0;
		endSampicIndex = NB_OF_SAMPICS_IN_FE_BOARD -1;
		
	}
	else
	{
	    startSampicIndex = sampicIndex;
		endSampicIndex = sampicIndex;
		
	}
	
	if(vdacRosc < MIN_DAC_RAW_VALUE || vdacRosc > MAX_DAC_RAW_VALUE)
		return SAMPIC256CH_InvalidParam;
	
	
	for(feboardIdx = startFeIndex; feboardIdx <=  endFeIndex;  feboardIdx++)
	{
	  for(sampicIdx = startSampicIndex; sampicIdx <= endSampicIndex; sampicIdx++)
	  {
		crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.InternalRoscDAC = vdacRosc; 
		crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.Vdac_Rosc = vdacRosc;  
	  }
	  
	}
	
	Build_FeBoardsSampicsConfigReg8(crateParams);
    if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_SAMPIC_SC_REG8, feBoard, sampicIndex, dummy);  	    				    


   return error;  
	
	
	
}


//============================================================================== //
SAMPIC256CH_ErrCode 	SAMPIC256CH_GetSampicVdacRosc(CrateParamStruct *crateParams, int feBoard, int sampicIndex, float *vdacRosc)
//============================================================================== //
{
	
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;  
	if(feBoard >= MAX_NB_OF_FE_BOARDS)
		return SAMPIC256CH_InvalidParam;

	if(sampicIndex >= NB_OF_SAMPICS_IN_FE_BOARD)
		return SAMPIC256CH_InvalidParam; 

	if(feBoard < 0 || sampicIndex < 0)
		return SAMPIC256CH_InvalidParam;
	 
	*vdacRosc =  crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.InternalRoscDAC;
	
	return error;
	
	
	
}



//============================================================================== //
SAMPIC256CH_ErrCode     SAMPIC256CH_SetSampicLvdsLowCurrentMode(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard,int sampicIndex, Boolean mode)  
//============================================================================== //
{
SAMPIC256CH_ErrCode error = SAMPIC256CH_Success; 
	
int startSampicIndex, endSampicIndex, sampicIdx;
int feboardIdx, startFeIndex,endFeIndex;  
int dummy = 0;

	if(feBoard >=  crateInfoParams->NbOfFeBoards)
		return SAMPIC256CH_InvalidParam;
	
	if(feBoard < 0)
	{
		startFeIndex = 0;
		endFeIndex = crateInfoParams->NbOfFeBoards -1;
	}
	else
	{
		startFeIndex = feBoard;
		endFeIndex = feBoard;
	}

	if(sampicIndex < 0)
	{
		startSampicIndex = 0;
		endSampicIndex = NB_OF_SAMPICS_IN_FE_BOARD -1;
		
	}
	else
	{
	    startSampicIndex = sampicIndex;
		endSampicIndex = sampicIndex;
		
	}
	
	
	for(feboardIdx = startFeIndex; feboardIdx <=  endFeIndex;  feboardIdx++)
	{
	  for(sampicIdx = startSampicIndex; sampicIdx <= endSampicIndex; sampicIdx++)
	  {
		crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.SelHighLVDSCurrent = (1-mode);  
	  }
	  
	}
	
	Build_FeBoardsSampicsConfigReg2(crateParams);
    if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_SAMPIC_SC_REG2, feBoard, sampicIndex, dummy);  


    return error;  
	
	
	
}



//============================================================================== //
SAMPIC256CH_ErrCode     SAMPIC256CH_GetSampicLvdsLowCurrentMode(CrateParamStruct *crateParams,int feBoard,  int sampicIndex, Boolean *mode)  
//============================================================================== //
{
	
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;  
	if(feBoard >= MAX_NB_OF_FE_BOARDS)
		return SAMPIC256CH_InvalidParam;

	if(sampicIndex >= NB_OF_SAMPICS_IN_FE_BOARD)
		return SAMPIC256CH_InvalidParam; 

	if(feBoard < 0 || sampicIndex < 0)
		return SAMPIC256CH_InvalidParam;
	 
	*mode = (Boolean)(1- crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.SelHighLVDSCurrent);
	
	return error;
	
	
}





//============================================================================== //
SAMPIC256CH_ErrCode 	SAMPIC256CH_UpdateADCRampValuesFromCalibValues(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams)  
//============================================================================== //
{
	
 SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;
	
 int adcNbOfBits;
  
  adcNbOfBits = crateParams->CommonParams.ADCNbOfBits;
  
  error = Set_SystemADCNbOfBits(crateInfoParams, crateParams, adcNbOfBits);
	
  return error;
	
	
}

/* ============================================================================ */ 
SAMPIC256CH_ErrCode	SAMPIC256CH_UpdateChannelsInternalThresholdsFromCalibValues (CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams)
/* ============================================================================ */ 
{

	SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success;
	
	 Compute_SAMPICIndividualInternalThresholds(crateInfoParams, crateParams);
	 
	 errCode = Load_HardwareSetup(crateInfoParams, crateParams, FEB_SAMPIC_SC_CHANNEL_REG, ALL_FE_BOARDs, ALL_FE_BOARDs_FE_FPGAs, ALL_CHANNELs);

	
	return errCode;
	
}

 /* =================================================================================== */
void  SAMPIC256CH_ResetInternalTriggerThresholdOffsetCalibValues			(CrateInfoStruct *crateInfoParams, int feBoardIndex)
/* =================================================================================== */
{
	
	int startFeBoardIndex, endFeBoardIndex, channel;
	int feBoardIdx;
 	CalibStatusStruct	defaultCalibStatus;
	
	defaultCalibStatus.CalibStatus 		= CALIB_VALUES_NOT_LOADED;
	defaultCalibStatus.CalibFreqEch 	= DEFAULT_FREQ_ECH;
	defaultCalibStatus.CalibADCNbOfBits	= DEFAULT_ADC_NB_OF_BITS; 
	

	if(feBoardIndex < 0)
	{
		
		startFeBoardIndex = 0;
		endFeBoardIndex = MAX_NB_OF_FE_BOARDS -1;
		
	}
	else
	{
		
		startFeBoardIndex = feBoardIndex;
		endFeBoardIndex = feBoardIndex;
		
		
	}
	
			

	for(feBoardIdx = startFeBoardIndex; feBoardIdx <= endFeBoardIndex; feBoardIdx++)
	{
		crateInfoParams->CrateCalibInfo.CalibStatus.InternalTriggerThresholdCalibStatus[feBoardIdx] = defaultCalibStatus;
		
		for(channel = 0; channel <NB_OF_CHANNELS_IN_FE_BOARD; channel++)
		{	
		
		 	crateInfoParams->CrateCalibInfo.CalibValues[feBoardIdx].InternalTriggerThresholdOffset[channel]=  0.0; 
			crateInfoParams->CrateCalibInfo.CalibValues[feBoardIdx].InternalTriggerThresholdSlope[channel]=  0.0; 
			
			
		}
		
	}
	
 
	
}
/* =================================================================================== */

/* =================================================================================== */
void  SAMPIC256CH_ResetADCRampCalibValues								(CrateInfoStruct *crateInfoParams, int feBoardIndex)
/* =================================================================================== */
{
	
	int startFeBoardIndex, endFeBoardIndex, sampicIndex;
	int i, feBoardIdx;
 	CalibStatusStruct	defaultCalibStatus;
	
	defaultCalibStatus.CalibStatus 		= CALIB_VALUES_NOT_LOADED;
	defaultCalibStatus.CalibFreqEch 	= DEFAULT_FREQ_ECH;
	defaultCalibStatus.CalibADCNbOfBits	= DEFAULT_ADC_NB_OF_BITS; 
	

	if(feBoardIndex < 0)
	{
		
		startFeBoardIndex = 0;
		endFeBoardIndex = MAX_NB_OF_FE_BOARDS -1;
		
	}
	else
	{
		
		startFeBoardIndex = feBoardIndex;
		endFeBoardIndex = feBoardIndex;
		
		
	}
	
	for(feBoardIdx = startFeBoardIndex; feBoardIdx <= endFeBoardIndex; feBoardIdx++)
	{
		crateInfoParams->CrateCalibInfo.CalibStatus.ADCRampCalibStatus[feBoardIdx] = defaultCalibStatus;
	
		
		for(sampicIndex = 0; sampicIndex < NB_OF_SAMPICS_IN_FE_BOARD; sampicIndex++)
		{
			
			crateInfoParams->CrateCalibInfo.CalibValues[feBoardIdx].VDAC_Ramp[sampicIndex][0] = DEFAULT_VDAC_RAMP_SAMPICV3_11BITS;  //DEFAULT_VDAC_RAMP_SAMPICV3_11BITS;
			crateInfoParams->CrateCalibInfo.CalibValues[feBoardIdx].VDAC_Ramp[sampicIndex][1] = DEFAULT_VDAC_RAMP_SAMPICV3_10BITS; 
			crateInfoParams->CrateCalibInfo.CalibValues[feBoardIdx].VDAC_Ramp[sampicIndex][2] = DEFAULT_VDAC_RAMP_SAMPICV3_9BITS;   
			for(i= 3; i < MAX_NB_OF_ADC_MODES; i++)
			  crateInfoParams->CrateCalibInfo.CalibValues[feBoardIdx].VDAC_Ramp[sampicIndex][i] = DEFAULT_VDAC_RAMP_SAMPICV3_8BITS; 
			
		}
		
	}
}

/* =================================================================================== */
void  SAMPIC256CH_ResetTOTCalibValues									(CrateInfoStruct *crateInfoParams, int feBoardIndex)
/* =================================================================================== */
{
	int startFeBoardIndex, endFeBoardIndex, channel;
	int i, feBoardIdx;
 	CalibStatusStruct	defaultCalibStatus;
	
	defaultCalibStatus.CalibStatus 		= CALIB_VALUES_NOT_LOADED;
	defaultCalibStatus.CalibFreqEch 	= DEFAULT_FREQ_ECH;
	defaultCalibStatus.CalibADCNbOfBits	= DEFAULT_ADC_NB_OF_BITS; 
	

	if(feBoardIndex < 0)
	{
		startFeBoardIndex = 0;
		endFeBoardIndex = MAX_NB_OF_FE_BOARDS -1;
	}
	else
	{
		startFeBoardIndex = feBoardIndex;
		endFeBoardIndex = feBoardIndex;
		
	}
	
	for(feBoardIdx = startFeBoardIndex; feBoardIdx <= endFeBoardIndex; feBoardIdx++)
	{
		crateInfoParams->CrateCalibInfo.CalibStatus.TOTCalibStatus[feBoardIdx] = defaultCalibStatus;
		
		for(channel = 0; channel <NB_OF_CHANNELS_IN_FE_BOARD; channel++)
		{	
		
			for(i=0; i < MAX_NB_OF_TOT_RAMP_CURRENT_VALUES; i++)
			{
				crateInfoParams->CrateCalibInfo.CalibValues[feBoardIdx].TOTCalibSlope[i][channel] = 1.0;
				crateInfoParams->CrateCalibInfo.CalibValues[feBoardIdx].TOTCalibIntercept[i][channel] = 1.0;  
			
			}

			
		}
		
			
	}	
	
}

/* =================================================================================== */
void  SAMPIC256CH_ResetTOTDACOffsetsValues									(CrateInfoStruct *crateInfoParams, int feBoardIndex)
/* =================================================================================== */
{
	
	int startFeBoardIndex, endFeBoardIndex, sampicIndex;
	int feBoardIdx;
 	

	if(feBoardIndex < 0)
	{
		startFeBoardIndex = 0;
		endFeBoardIndex = MAX_NB_OF_FE_BOARDS -1;
	}
	else
	{
		startFeBoardIndex = feBoardIndex;
		endFeBoardIndex = feBoardIndex;
	}
	
	for(feBoardIdx = startFeBoardIndex; feBoardIdx <= endFeBoardIndex; feBoardIdx++)
	{
		for(sampicIndex = 0; sampicIndex < NB_OF_SAMPICS_IN_FE_BOARD; sampicIndex++)
		{
				
			crateInfoParams->CrateCalibInfo.CalibValues[feBoardIdx].TOTDacOffset[sampicIndex]= 0.0;
			crateInfoParams->CrateCalibInfo.CalibValues[feBoardIdx].TOTFilterDacOffset[sampicIndex]= 0.0;  
		}
		
	}	
}

/* =================================================================================== */
void  SAMPIC256CH_ResetTimeINLCalibValues								(CrateInfoStruct *crateInfoParams, int feBoardIndex, int channel)
/* =================================================================================== */
{
	
	int startFeBoardIndex, endFeBoardIndex,sample;
	int startChannelIndex, endChannelIndex, channelIdx;
	
	int feBoardIdx;
 	CalibStatusStruct	defaultCalibStatus;
	
	defaultCalibStatus.CalibStatus 		= CALIB_VALUES_NOT_LOADED;
	defaultCalibStatus.CalibFreqEch 	= DEFAULT_FREQ_ECH;
	defaultCalibStatus.CalibADCNbOfBits	= DEFAULT_ADC_NB_OF_BITS; 
	

	if(feBoardIndex < 0)
	{
		
		startFeBoardIndex = 0;
		endFeBoardIndex = MAX_NB_OF_FE_BOARDS -1;
		
	}
	else
	{
		
		startFeBoardIndex = feBoardIndex;
		endFeBoardIndex = feBoardIndex;
		
		
	}
	if(channel < 0)
	{
		
		startChannelIndex =  0;
		endChannelIndex  = NB_OF_CHANNELS_IN_FE_BOARD -1;
		
	}
	else
	{
		startChannelIndex =  channel;
		endChannelIndex  = channel;
	
		
	}
		
		
	
	for(feBoardIdx = startFeBoardIndex; feBoardIdx <= endFeBoardIndex; feBoardIdx++)
	{
		
		for(channelIdx = startChannelIndex; channelIdx <= endChannelIndex; channelIdx++)
		{	
		
			crateInfoParams->CrateCalibInfo.CalibStatus.TimeINLCalibStatus[feBoardIdx][channelIdx]= defaultCalibStatus;	   
	
			for(sample = 0; sample <MAX_NB_OF_SAMPLES; sample++)
			{
		
				crateInfoParams->CrateCalibInfo.CalibValues[feBoardIdx].TimeINLValues[channelIdx][sample]=  0.0; 
				
			}
			
		}
	}	
}

/* =================================================================================== */
void SAMPIC256CH_ResetResidualPedestalCalibValues		(CrateInfoStruct *crateInfoParams, int feBoardIndex)
/* =================================================================================== */
{
	
	int startFeBoardIndex, endFeBoardIndex, channel,sample;
	int feBoardIdx;
 	CalibStatusStruct	defaultCalibStatus;
	
	defaultCalibStatus.CalibStatus 	= CALIB_VALUES_NOT_LOADED;
	defaultCalibStatus.CalibFreqEch = DEFAULT_FREQ_ECH;
	defaultCalibStatus.CalibADCNbOfBits	= DEFAULT_ADC_NB_OF_BITS; 
	

	if(feBoardIndex < 0)
	{
		
		startFeBoardIndex = 0;
		endFeBoardIndex = MAX_NB_OF_FE_BOARDS -1;
		
	}
	else
	{
		
		startFeBoardIndex = feBoardIndex;
		endFeBoardIndex = feBoardIndex;
		
		
	}
	
	for(feBoardIdx = startFeBoardIndex; feBoardIdx <= endFeBoardIndex; feBoardIdx++)
	{
		
	
		
		for(channel = 0; channel <NB_OF_CHANNELS_IN_FE_BOARD; channel++)
		{	
		
				
			for(sample = 0; sample <MAX_NB_OF_SAMPLES; sample++)
			{
				crateInfoParams->CrateCalibInfo.CalibStatus.ResidualPedestalCalibStatus[feBoardIdx][channel] = defaultCalibStatus;	  
				crateInfoParams->CrateCalibInfo.CalibValues[feBoardIdx].ResidualPedestalValues[channel][sample]=  0.0;
				
				
			}
		
			
		}
		
		
	}	
	
	
}

/* =================================================================================== */
void  SAMPIC256CH_ResetADCLinearityCalibValues						(CrateInfoStruct *crateInfoParams, int feBoardIndex)
/* =================================================================================== */
{

	int startFeBoardIndex, endFeBoardIndex, channel,sample;
	int feBoardIdx;
 	CalibStatusStruct	defaultCalibStatus;
	
	defaultCalibStatus.CalibStatus 		= CALIB_VALUES_NOT_LOADED;
	defaultCalibStatus.CalibFreqEch 	= DEFAULT_FREQ_ECH;
	defaultCalibStatus.CalibADCNbOfBits	= DEFAULT_ADC_NB_OF_BITS; 
	

	if(feBoardIndex < 0)
	{
		
		startFeBoardIndex = 0;
		endFeBoardIndex = MAX_NB_OF_FE_BOARDS -1;
		
	}
	else
	{
		
		startFeBoardIndex = feBoardIndex;
		endFeBoardIndex = feBoardIndex;
		
		
	}
	
	
	for(feBoardIdx = startFeBoardIndex; feBoardIdx <= endFeBoardIndex; feBoardIdx++)
	{
		 for(channel = 0; channel < NB_OF_CHANNELS_IN_SAMPIC; channel++)
		 {
					
			for(sample = 0; sample <MAX_NB_OF_SAMPLES; sample++)
			{
				crateInfoParams->CrateCalibInfo.CalibValues[feBoardIdx].CellLinearity_a2[channel][sample] = 0.0;
				crateInfoParams->CrateCalibInfo.CalibValues[feBoardIdx].CellLinearity_a1[channel][sample] = 1.0;     
				crateInfoParams->CrateCalibInfo.CalibValues[feBoardIdx].CellLinearity_a0[channel][sample] = 0.0;     
		
			}
		}
		 
		 crateInfoParams->CrateCalibInfo.CalibStatus.ADCLinearityCalibStatus[feBoardIdx] = defaultCalibStatus;
		 
	}
	
	crateInfoParams->CrateCalibInfo.CalibStatus.ADCLinearityCrateCalibStatus = defaultCalibStatus;
	
}



/* =============================================================================================== */
/* ===========    For Calib Functions   ========================================================= */
/* =============================================================================================== */


/* =============================================================================================== */
SAMPIC256CH_ErrCode  	SAMPIC256CH_SetAutoINLCalibMode					(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, Boolean mode)
/* =============================================================================================== */
{
	
SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;   
int sampicIdx;
int feboardIdx;  
int dummy = 0;


	for(feboardIdx = 0; feboardIdx < crateInfoParams->NbOfFeBoards;  feboardIdx++)
	{
	
	  for(sampicIdx = 0; sampicIdx < NB_OF_SAMPICS_IN_FE_BOARD; sampicIdx++)
	  {
		 
		  if(mode == TRUE)
		  {
		  
			//REG10
				crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.EnableTimeINLCalib = TRUE;
				crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.EnableReturnGnd = FALSE;   
							
			//REG9	
				crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.EnableDiffN = TRUE;
				crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.EnableDiffP  = FALSE;
				crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.EnableReturnBuffer = FALSE; 
				
			//REG 8
					
			  crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.EnableTranslator = TRUE; 
						  
			 //REG 3
			 // crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.EnableDigitalInput = TRUE;
			  
			 //REG 2 
			  crateParams->CommonParams.EnableAutoConversionMode  = FALSE;
			  crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.EnCoincidenceModeForCentralTrigger = TRUE;
		  }
		  else
		  {
			  
				//REG10
				crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.EnableTimeINLCalib = FALSE;
				crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.EnableReturnGnd = FALSE;  
				
				if((crateInfoParams->SystemType == SAMPET_SYSTEM) || (crateInfoParams->SystemType == SAMPIC_BYPASS))
				{
					crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.EnableDiffN = TRUE;
					crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.EnableDiffP  = TRUE;
					crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.EnableReturnBuffer = FALSE; 
		
				}
				else
				{
				 	crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.EnableReturnBuffer = FALSE;  
					crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.EnableDiffN = FALSE;
					crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.EnableDiffP = FALSE;
				}
		
				
			//REG 8
					
			  crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.EnableTranslator = FALSE; 
						  
			 //REG 3
			 // crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.EnableDigitalInput = TRUE;
			  
			 //REG 2 
			   crateParams->CommonParams.EnableAutoConversionMode = TRUE;
			  crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.EnCoincidenceModeForCentralTrigger = FALSE;
  
			  
			  
			  
		  }
	
	  }
	}
	
	Build_FeBoardsSampicsConfigReg10(crateParams);
	Build_FeBoardsSampicsConfigReg9(crateParams); 
	Build_FeBoardsSampicsConfigReg8(crateParams);
	Build_FeBoardsSampicsConfigReg2(crateParams); 
	
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_SAMPIC_SC_REG10, ALL_FE_BOARDs, ALL_SAMPICs, dummy); 
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_SAMPIC_SC_REG9, ALL_FE_BOARDs, ALL_SAMPICs, dummy); 
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_SAMPIC_SC_REG8, ALL_FE_BOARDs, ALL_SAMPICs, dummy); 
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_SAMPIC_SC_REG2, ALL_FE_BOARDs, ALL_SAMPICs, dummy); 

   return error;
	
	
}

/* =============================================================================================== */
SAMPIC256CH_ErrCode  	SAMPIC256CH_GetAutoINLCalibMode	(CrateParamStruct *crateParams, Boolean *mode)
/* =============================================================================================== */
{
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;  
	*mode =  crateParams->FeBoardParams[0].FeFpgaParams[0].SAMPICIndividualParams.EnableTimeINLCalib; 
		
	return error;	
	
}

/* =============================================================================================== */
SAMPIC256CH_ErrCode  	SAMPIC256CH_SetSampicAutoINLCalibParams				(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, int feBoard, int sampicIndex, Boolean enDigitalLevel, int freq, int risingEdgeSlope, int fallingEdgeSlope, int highLevel, int lowLevel)
/* =============================================================================================== */
{
	
SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;   
int startSampicIndex, endSampicIndex, sampicIdx;
int feboardIdx, startFeIndex,endFeIndex;  
int dummy = 0;

	
	if(feBoard >= MAX_NB_OF_FE_BOARDS)
		return SAMPIC256CH_InvalidParam;
	
	if(sampicIndex >= NB_OF_SAMPICS_IN_FE_BOARD)
		return SAMPIC256CH_InvalidParam;
	
	if(fallingEdgeSlope < 0 || fallingEdgeSlope > 15)
		return SAMPIC256CH_InvalidParam; 
	
	if(risingEdgeSlope < 0 || risingEdgeSlope > 15)
		return SAMPIC256CH_InvalidParam;
	
	if( freq  <0 || freq > 7)
		return SAMPIC256CH_InvalidParam; 
	
	if( highLevel  <0 || highLevel > 7)
		return SAMPIC256CH_InvalidParam; 
	
	if( lowLevel  <0 || lowLevel > 7)
		return SAMPIC256CH_InvalidParam;
	
	if(feBoard < 0)
	{
		startFeIndex = 0;
		endFeIndex = crateInfoParams->NbOfFeBoards -1;
	}
	else
	{
		startFeIndex = feBoard;
		endFeIndex = feBoard;
	}
  	

	if(sampicIndex < 0)
	{
		startSampicIndex = 0;
		endSampicIndex = NB_OF_SAMPICS_IN_FE_BOARD -1;
		
	}
	else
	{
	    startSampicIndex = sampicIndex;
		endSampicIndex = sampicIndex;
		
	}
	
	
	for(feboardIdx = startFeIndex; feboardIdx <=  endFeIndex;  feboardIdx++)
	{
	
	  for(sampicIdx = startSampicIndex; sampicIdx <= endSampicIndex; sampicIdx++)
	  {
		  
		  // REG 8
		    crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.TimeINLCalibClockFreqV3 =  freq;
			  
		   //REG 3
		  
			crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.EnableDigitalInput = enDigitalLevel;
	 	 
		   //REG10
		    crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.TimeINLCalibRisingEdgeSlope = risingEdgeSlope;
		    crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.TimeINLCalibFallingEdgeSlope = fallingEdgeSlope;
		    crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.TimeINLCalibVHigh = highLevel;
			crateParams->FeBoardParams[feboardIdx].FeFpgaParams[sampicIdx].SAMPICIndividualParams.TimeINLCalibVLow = lowLevel;
	  }
	}
	
	Build_FeBoardsSampicsConfigReg8(crateParams);
	Build_FeBoardsSampicsConfigReg3(crateParams); 
	Build_FeBoardsSampicsConfigReg10(crateParams);
	
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_SAMPIC_SC_REG8, feBoard, sampicIndex, dummy); 
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_SAMPIC_SC_REG3, feBoard, sampicIndex, dummy); 
	if(error == SAMPIC256CH_Success) error = Load_HardwareSetup(crateInfoParams, crateParams, FEB_SAMPIC_SC_REG10, feBoard, sampicIndex, dummy); 
	
   return error;
	
	
}

/* =============================================================================================== */
SAMPIC256CH_ErrCode  	SAMPIC256CH_GetSampicAutoINLCalibParams (CrateParamStruct *crateParams, int feBoard, int sampicIndex, Boolean *enDigitalLevel, int *freq, int *risingEdgeSlope, int *fallingEdgeSlope,int *highLevel, int *lowLevel)
/* =============================================================================================== */
{
	
	SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;  
	
	if(feBoard >= MAX_NB_OF_FE_BOARDS)
		return SAMPIC256CH_InvalidParam;
	
	if(sampicIndex >= NB_OF_SAMPICS_IN_FE_BOARD)
		return SAMPIC256CH_InvalidParam; 

	if(feBoard < 0 || sampicIndex < 0)
		return SAMPIC256CH_InvalidParam;
	
   *enDigitalLevel = crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.EnableDigitalInput;
   *freq = crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.TimeINLCalibClockFreqV3  ;
   *risingEdgeSlope = crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.TimeINLCalibRisingEdgeSlope;
   *fallingEdgeSlope = crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.TimeINLCalibFallingEdgeSlope ;
   *highLevel= crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.TimeINLCalibVHigh;
   *lowLevel= crateParams->FeBoardParams[feBoard].FeFpgaParams[sampicIndex].SAMPICIndividualParams.TimeINLCalibVLow ;
  
		
	return error;	
	
	
	
}



/* =============================================================================================== */
SAMPIC256CH_ErrCode  	SAMPIC256CH_SetControlBoardEnableTrigger	(CrateInfoStruct *crateInfoParams, CrateParamStruct *crateParams, Boolean mode)
/* =============================================================================================== */
{
	
  SAMPIC256CH_ErrCode error = SAMPIC256CH_Success;
  int dummy = 0;
 
  crateParams->ControlBoardParams.EnableTrigger = mode;
  
  Build_ControlBoardControlReg(crateParams);
  
  error = Load_HardwareSetup(crateInfoParams, crateParams, CTRLB_CONTROL_REG, dummy, dummy, dummy);   

  return error;
  
}
/* =============================================================================================== */
/* =============================================================================================== */

/* ================== END OF LIBRARY FUNCTIONS =========================== */
/* ======================================================================= */
