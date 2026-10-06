// Tab size = 4 characters
// lpDevC.h : user header file of lpDevCLib project
// C interface to LPDevice class
// Author C. CHEIKALI
// Created 29/08/2019
//

#ifndef __LPDEVC_H__
#define __LPDEVC_H__

//#if defined(ML_Frame)
#undef ML_Frame
//#endif

#if defined(WIN32) || defined(_WINDOWS_)

	#define _CRT_SECURE_NO_WARNINGS
	#define WIN32_LEAN_AND_MEAN		// Exclude rarely used headers

	#ifndef LPDEVCLIB_EXPORTS
		#define LPDEVCLIB_API __declspec(dllimport)
		#include <windows.h>
		
	/*	typedef enum _interfacetype_
		{
			NONE,
			USB2,
			USB3,
			UDP,
			TCP,
			MaxInterfaceType
		} InterfaceType; */
	#endif
#else
	#include <WinTypes.h>
	#include <unistd.h>
	#define LPDEVCLIB_API
	#define Sleep(msecs) usleep(1000*msecs)
#endif

#if !defined(ML_Frame)

	typedef int IO_Error;

	#define MAXLAYER 4

	typedef enum
	{
		RXSTATUS,
		TXSTATUS
	}TXRX_STATUS;

// Errors that may occur

	#define IO_NoErr = 0;
	#define  FrameNoErr = 0;

	#if !defined (PerrorFunc)
		typedef void (*PerrorFunc)(char *msg);
	#endif

// Printf-like function type

	#if !defined (PrintfFunc)
		typedef int (*PrintfFunc)(char *format, ...);
	#endif


	#pragma pack(1)
	typedef struct ml_frame
	{
		unsigned int frame_id;		// frame index
		int frame_num;				// number of frames
		int data_size;				// data size
		char sub_address;			// target (or source) sub_address
		int nlayer;					// number of layers for this frame
		int path[MAXLAYER];		// LP target (or source) path
		unsigned char *user_data;	// payload
	}ML_Frame;
	#pragma pack()

#endif // #if !defined(ML_Frame)

#ifdef __cplusplus
extern "C" {
#endif

// Debug and trace	

void  LPDEVCLIB_API dbg_OpenLogfile(int id);
void  LPDEVCLIB_API dbg_CloseLogfile(int id);

// Initialization									

	// USB Only
BOOL  LPDEVCLIB_API LPD_FindDevices		(char *DeviceDescriptionStr);					
BOOL  LPDEVCLIB_API LPD_GetDeviceSerNum	(char *sernum, int index);					
BOOL  LPDEVCLIB_API LPD_GetDeviceDesc	(char *buffer, int index);					
int   LPDEVCLIB_API LPD_GetNumberOfDevs	(void);					
	// USB Only end									   

BOOL  LPDEVCLIB_API LPD_Init			(int id, BOOL verbose);					
BOOL  LPDEVCLIB_API LPD_ResetDevice		(int id);					
									
// Open and close devices									
									
int   LPDEVCLIB_API LPD_OpenDevice		(char *sernum_or_address, int it);
int   LPDEVCLIB_API LPD_OpenUsb2Device	(char *sernumstr);					
int   LPDEVCLIB_API LPD_OpenUsb3Device	(char *sernumstr);					
int   LPDEVCLIB_API LPD_OpenUdpDevice	(char *address, int port);

	// UDP Only
int   LPDEVCLIB_API LPD_OpenDeviceRaw	(char *address);
int   LPDEVCLIB_API LPD_OpenDeviceRawEx	(char *address, int port);
	// UDP Only end

void  LPDEVCLIB_API LPD_CloseDevice		(int id);
void  LPDEVCLIB_API LPD_CloseAll		(void);

// Buffers and I/O operation									
									
BOOL  LPDEVCLIB_API LPD_PurgeBuffers	(int id);					
BOOL  LPDEVCLIB_API LPD_Write			(int id, void *buf, int count, int *written);					
BOOL  LPDEVCLIB_API LPD_Read			(int id, void *buf, int maxcnt, int *rdcount);											
#if defined(WIN32) || defined(_WINDOWS_)
int   LPDEVCLIB_API LPD_GetStatus		(int id, TXRX_STATUS);
#else
int   LPDEVCLIB_API LPD_GetStatus		(int id, int status);
#endif
									
// Parameters									
									
BOOL  LPDEVCLIB_API LPD_SetTimeouts		(int id, int tx_timeout, int rx_timeout);					
#if defined(WIN32) || defined(_WINDOWS_)
BOOL  LPDEVCLIB_API LPD_SetXferSize		(int id, unsigned long txsize, unsigned long rxsize);
#else
BOOL  LPDEVCLIB_API LPD_SetXferSize		(int id, int txsize, int rxsize);
#endif

	// USB Only
BOOL  LPDEVCLIB_API LPD_SetLatencyTimer		(int id, unsigned char msecs);					
BOOL  LPDEVCLIB_API LPD_GetLatencyTimer		(int id, unsigned char *msecs);					
BOOL  LPDEVCLIB_API LPD_SetBaudRate		(int id, int baud);					
	// USB Only end

// IO transfer mode management (asynchronous or synchronous)					
					
	// USB Only
BOOL  LPDEVCLIB_API LPD_ResetMode(int id);   // Tranfer mode is set to asynchronous i/o
BOOL  LPDEVCLIB_API LPD_SetSynchronousMode(int id, int sleep_time);	// sleep_time is the time (in ms) between resetting tranfer mode and setting it to synchronous	
BOOL  LPDEVCLIB_API LPD_SetFlowControlToHardware(int id);		
	// USB Only end

// Error handling				
				
void  LPDEVCLIB_API LPD_SetErrno		(int id, IO_Error error_code);
IO_Error LPDEVCLIB_API LPD_GetErrno		(int id);
void  LPDEVCLIB_API LPD_Perror			(int id, IO_Error err_code);
IO_Error LPDEVCLIB_API LPD_GetLastError	(int id);

void  LPDEVCLIB_API LPD_SetPerrorFunc	(int id, PerrorFunc func);
void  LPDEVCLIB_API LPD_SetPrintfFunc	(int id, PrintfFunc func);
									
// ******************************************************************************************************
//	Multi-layer section (LPBus section)

int   LPDEVCLIB_API lpdWrt				(int id, int *target_path_array, unsigned char sub_addr, void *buffer, int usercount);
int   LPDEVCLIB_API lpdRd				(int id, int *target_path_array, unsigned char sub_addr, void *buffer, int usercount);

#ifndef LPDEVCLIB_EXPORTS
	int   LPDEVCLIB_API lpdRdEx				(int id, void *array, ML_Frame *mf_array, int max_num_bytes, int *frames);
#else
	int   LPDEVCLIB_API lpdRdEx				(int id, void *array, lpDev::ML_Frame *(mf_array), int max_num_bytes, int *frames);
#endif

int   LPDEVCLIB_API lpdRequestForRd		(int id, int *target_path_array, unsigned char sub_addr, void *buffer, int usercount);
	
	// UDP Only
int   LPDEVCLIB_API lpdReadDAQStream	(int id, void *array, ML_Frame *mf_array, int max_num_bytes, int *frames);
int   LPDEVCLIB_API lpdSetDAQMaxFrameSize(int id, int maxFrameSize);	
int   LPDEVCLIB_API lpdSetDAQFlowControl(int id,  unsigned int enableHandshake, double timeoutForRetransmit);
	// UDP Only end

void  LPDEVCLIB_API LPD_EnableInterrupts (int id, BOOL truefalse);
void  LPDEVCLIB_API LPD_ShowInterruptInfo(int id, BOOL yesno);

void  LPDEVCLIB_API LPD_PrintFrameInfo	(void);
int   LPDEVCLIB_API 	LPD_GetLastFrameStatus(void);
void  LPDEVCLIB_API LPD_ResetFrameErrors(void);

void  LPDEVCLIB_API LPD_ResetLostFrames (void);
void  LPDEVCLIB_API LPD_SetLostFrames	(unsigned long val);
unsigned long LPDEVCLIB_API LPD_GetLostFrames	(void);
void  LPDEVCLIB_API IncLostFrames		(void);
unsigned long LPDEVCLIB_API  LPD_GetTotalByteCount	(void);
void  LPDEVCLIB_API LPD_IncTotalByteCount(unsigned long count);
void  LPDEVCLIB_API LPD_ResetTotalByteCount(void);

int   LPDEVCLIB_API	*lpdGetTargetPathArray(int id);
int   LPDEVCLIB_API	lpdGetNlayers		(int id);
int   LPDEVCLIB_API	lpdSetMaxLayer		(int id, int num_layers);
int   LPDEVCLIB_API	lpdGetMaxLayer		(int id, int *num_layers);

#ifdef __cplusplus
}
#endif

#endif // __LPDEVC_H__
