#ifndef __SAMPIC256_TYPE_H
#define __SAMPIC256_TYPE_H
 
/****************************************************************
 *	File	: 	SAMPIC_256Ch_Type.h								*
 *																*
 *	Date	:	April 2020  									*
 *																*
 *	Authors	:   J.Maalmi / D.Breton	                            *
 *	            								                *
 *								 								*
 ***************************************************************/


/* =============================================================================================== */
/* =============================================================================================== */


 /* ========================================================= */
/* ========            Constants                ============= */
/* ========================================================= */



#define TRUE 1
#define FALSE 0

#define MAX_DEVICE_DESCR_LENGTH 20
#define MAX_SERNUM_LENGTH		20
#define MAX_IP_ADDRESS_LENGTH   16


#define MAX_PATHNAME_LENGTH 260
#define MAX_FILENAME_LENGTH 256
	
#define MAX_BYTES_TO_READ		65536
#define MAX_LAYER_IN_USBML		8
#define SIZE_OF_DATA_FIFO		MAX_BYTES_TO_READ
#define SIZE_OF_ML_BUFFER		(2 * SIZE_OF_DATA_FIFO) 
#define DATA_HEADER				0x69
#define TRIGGER_DATA_SIZE       8  //Bytes
#define MAX_NB_OF_TRIGGERS_IN_EVENT (int)(MAX_BYTES_TO_READ/TRIGGER_DATA_SIZE)

#define MAX_SINGLE_FRAME_SIZE		160
#define MAX_NB_OF_FRAMES_PER_BLOCK  31
#define MAX_FRAME_SIZE (MAX_NB_OF_FRAMES_PER_BLOCK * MAX_SINGLE_FRAME_SIZE)
#define MIN_FRAME_SIZE				8
#define MAX_EXPECTED_FRAMES 		(MAX_BYTES_TO_READ/MIN_FRAME_SIZE)
#define MAX_LAYER_IN_USBML			8
#define NB_OF_LALUSBML_FRAME_EXTRA_BYTES   5 // Nb de bytes en plus des données dans une trame LALUSBML


#define ADC_11BITS_MAX_VALUE		2047		/* ADC 11 bits */ 
#define ADC_10BITS_MAX_VALUE		1023		/* ADC 10 bits */ 
#define ADC_9BITS_MAX_VALUE			511 		/* ADC 9 bits */ 
#define ADC_8BITS_MAX_VALUE			256		    /* ADC 8 bits */ 

#define ADC_11BITS_MAX_CONVERT_LENGTH 250

#define	ADCTOVOLTS			 0.00044
#define VOLTSTOADC			 2275

//#define VBASELINE_TO_VDAC_SLOPE  (-1.081)
//#define VBASELINE_TO_VDAC_OFFSET   1.7436

#define VBASELINE_TO_VDAC_OFFSET   	  1.6129  

#define MAX_ADC_VOLT_VALUE 1.2
#define MIN_ADC_VOLT_VALUE 0

#define MAX_NB_OF_VDAC_LINEARITY_STEP 1000

#define MAX_NB_OF_TOT_RAMP_CURRENT_VALUES 5
#define MAX_NB_OF_FE_BOARDS	4
#define NB_OF_FE_FPGAS_IN_FE_BOARD 4 
#define NB_OF_SAMPICS_IN_FE_BOARD NB_OF_FE_FPGAS_IN_FE_BOARD 
#define NB_OF_CHANNELS_IN_SAMPIC	16  
#define	NB_OF_CHANNELS_IN_FE_BOARD	(NB_OF_FE_FPGAS_IN_FE_BOARD * NB_OF_CHANNELS_IN_SAMPIC)

#define NB_OF_LAYERS_IN_SYSTEM		3
#define NB_OF_LAYERS_IN_FE_BOARD   	2
#define MAX_NB_OF_SAMPLES			64 	   


#define MAX_SAMPIC_DAC_VALUE			1023	 		// DACs 10 bits non signé
#define MIN_DAC_RAW_VALUE				0
#define MAX_DAC_RAW_VALUE				1.8
#define MAX_DAC_REAL_VALUE_V1_TO_V3		1.665  	// Tension moyenne mesurée sur les blocs front-end V1 à V3 pour les DACs externes de SAMPIC 
#define MAX_DAC_REAL_VALUE_V4			1.73  		// Tension moyenne mesurée sur les blocs front-end V4 pour les DACs externes de SAMPIC
#define MAX_EXT_SIG_DAC_VALUE   		3.3


#define NB_OF_SAMPIC_DAC_BITS   10
#define NB_OF_EXT_DAC_BITS      24

#define CLOCK_LOW				0x00
#define CLOCK_HIGH				0X04

#define MAX_NB_OF_ADC_MODES 4		//11 Bits, 10, 9 and 8 Bits

#define ALL_FE_BOARDs			-1  
#define ALL_FE_BOARDs_FE_FPGAs	-1
#define ALL_SAMPICs				-1
#define ALL_CHANNELs			-1

#define SI5332_GM2_REVD_REG_CONFIG_NUM_REGS 69   // Nombre d'accès nécessaires pour la config complète en I2C du Si5332
#define SI5332_FE_GM2_REVD_REG_CONFIG_NUM_REGS 56   // Nombre d'accès nécessaires pour la config complète en I2C du Si5332

#define SI5332_GM2_REVD_REG_CONFIG_SINGLE_REG 3  // Nombre d'accès nécessaires pour la config en I2C d'un seul registre du Si5332

#define SI5332_REG_O0_ADD 			0x2B
#define SI5332_REG_MUX0_CELL_ADD 	0x25
#define SI5332_REG_CELL0_ADD 		0x2A  
#define SI5332_REG_IDPA_ADD 		0x67
#define SI5332_REG_P_VALUE_ADD  	0x75
#define SI5332_REG_OUT0_DIV_ADD 	0x7B
#define SI5332_REG_OUT1_DIV_ADD 	0x80   
#define SI5332_REG_OUT2_DIV_ADD 	0x8A   
#define SI5332_REG_OUT4_DIV_ADD 	0x99   
#define SI5332_REG_OUT6_DIV_ADD 	0xA8 
#define SI5332_REG_OUT7_DIV_ADD 	0xAD  
#define SI5332_REG_PLL_MODE_ADD 	0xBE

// DEFAULT SETUP VALUES 

#define DEFAULT_UDP_CTRL_PORT 27015
#define DEFAULT_UDP_DAQ_PORT  27126
#define DEFAULT_CTRL_IP_ADDRESS "192.168.0.0"


#define DEFAULT_SAMPIC_VERSION 3  
#define DEFAULT_SAMPIC_EVOLUTION 'C'  


#define DEFAULT_SAMPIC_V2_VDAC_DLL 0.8
#define DEFAULT_SAMPIC_V3_C_VDAC_DLL 1.0
#define DEFAULT_SAMPIC_V3_D_VDAC_DLL 1.1
#define DEFAULT_SAMPIC_V5_SLOW_VDAC_DLL  1.3

#define DEFAULT_SAMPIC_V5_SLOW_VDAC_CONTINUITY 1.15
#define DEFAULT_SAMPIC_V5_VDAC_CONTINUITY 1.1  


#define DEFAULT_FE_BLOCK_VERSION 4


#define DEFAULT_SAMPIC_BASELINE 0.5
#define DEFAULT_SAMPET_BASELINE	0.2

#define DEFAULT_FREQ_ECH 6400

#define DEFAULT_TOT_RAMP_DAC0 0.5	
#define DEFAULT_TOT_RAMP_DAC1 0.216  
#define DEFAULT_TOT_RAMP_DAC2 0.117
#define DEFAULT_TOT_RAMP_DAC3 0.036


#define DEFAULT_SAMPIC_V3_TOT_RAMP_DAC0 0.450		  // Objectif ADC à 1600 pour la durée nominale
#define DEFAULT_SAMPIC_V3_TOT_RAMP_DAC1 0.220
#define DEFAULT_SAMPIC_V3_TOT_RAMP_DAC2 0.105
#define DEFAULT_SAMPIC_V3_TOT_RAMP_DAC3 0.031
#define DEFAULT_SAMPIC_V3_TOT_RAMP_DAC4 0.010


#define DEFAULT_SAMPET_NB_OF_SAMPLES 25
#define DEFAULT_SAMPET_OFFSET_FOR_START_OF_READ 25

//#define DEFAULT_VDAC_RAMP_SAMPICV3_11BITS  0.053
#define DEFAULT_VDAC_RAMP_SAMPICV3_11BITS  0.045   
#define DEFAULT_VDAC_RAMP_SAMPICV3_10BITS  0.080
#define DEFAULT_VDAC_RAMP_SAMPICV3_9BITS   0.145
#define DEFAULT_VDAC_RAMP_SAMPICV3_8BITS   0.340
#define MAX_VDAC_RAMP_SAMPICV2 0.5

#define DEFAULT_EXT_TRIG_GATE 5 			/* clock periods of Fe Clock */ 
#define DEFAULT_ADC_NB_OF_BITS 11
#define DEFAULT_PULSER_WIDTH 10 			/* clock periods of 100 MHz */


#define DEFAULT_NB_OF_TRIGGERS_PER_TRIGGER_EVENT  127

#define DEFAULT_LEVEL_2_LATENCY_GATE 3 // clock periods of 10 ns */
#define DEFAULT_PRIMITIVES_GATE_LENGTH 5 // clock periods of 10 ns */
	
#define DEFAULT_AUTO_PULSE_PERIOD 10 /* corresponds to 100 kHz?   */

#define MIN_TOT_FILTER_WIDTH_FOR_WIDE_CAP  20.0 /* ns*/
#define MAX_TOT_FILTER_WIDTH_FOR_WIDE_CAP  185.0 

#define MIN_TOT_FILTER_WIDTH_FOR_SMALL_CAP 4.0
#define MAX_TOT_FILTER_WIDTH_FOR_SMALL_CAP 28.0


#define MIN_TRIG_GATE_VALUE 3
#define MAX_TRIG_GATE_VALUE 255

/* standard firmware  in bytes */  
#define SINGLE_TRIGGER_DATA_SIZE 8

/* special T2K firmware in bytes */  

#define SINGLE_T2K_TRIGGER_DATA_SIZE 16
/*

#define DEFAULT_VDAC_RAMP_SAMPICV3_11BITS  0.018
#define DEFAULT_VDAC_RAMP_SAMPICV3_10BITS  0.055
#define DEFAULT_VDAC_RAMP_SAMPICV3_9BITS   0.15
#define DEFAULT_VDAC_RAMP_SAMPICV3_8BITS   0.35

*/


#define SET_ALL_BITS 0xFFFF
#define RESET_ALL_BITS 0x0000
#define SET_BIT0 	0x0001
#define RESET_BIT0  0xFFFE
#define SET_BIT1 	0x0002
#define RESET_BIT1  0xFFFD
#define SET_BIT2 	0x0004
#define RESET_BIT2  0xFFFB
#define SET_BIT3 	0x0008
#define RESET_BIT3  0xFFF7
#define SET_BIT4 	0x0010
#define RESET_BIT4  0xFFEF
#define SET_BIT5 	0x0020
#define RESET_BIT5  0xFFDF
#define SET_BIT6 	0x0040
#define RESET_BIT6  0xFFBF
#define SET_BIT7 	0x0080
#define RESET_BIT7  0xFF7F
#define SET_BIT8 	0x0100
#define RESET_BIT8  0xFEFF
#define SET_BIT9 	0x0200
#define RESET_BIT9  0xFDFF
#define SET_BIT10 	0x0400
#define RESET_BIT10 0xFBFF
#define SET_BIT11 	0x0800
#define RESET_BIT11 0xF7FF
#define SET_BIT12 	0x1000
#define RESET_BIT12 0xEFFF
#define SET_BIT13 	0x2000
#define RESET_BIT13 0xDFFF
#define SET_BIT14 	0x4000
#define RESET_BIT14 0xBFFF
#define SET_BIT15 	0x8000
#define RESET_BIT15 0x7FFF


#define BOARD_TYPE_IS_MEZZA_MOTHER 		'A'
#define BOARD_TYPE_IS_MEZZA_MOTHER_6U 	'F'
#define BOARD_TYPE_IS_SAMPIC_64CH 		'G'
#define BOARD_TYPE_IS_SAMPET_64CH 		'H'

/* ------ Constantes liées à l'EEPROM ------ */

#define EEPROM_NB_OF_BYTES_FOR_CALIB_CODES  1
#define EEPROM_FULL_SIZE        131072 	// in Bytes
#define EEPROM_PAGE_SIZE		256 	// in Bytes
#define EEPROM_NB_OF_PAGES		512

// Localisation des paramètres dans l'Eeprom (en pages de 256 bytes)
/*-----  sub-addresses in USER EEPROM  --------------------------  */

/* COMMON FIELDS IN MOTHER-BOARD AND DAUGTHER-BOARD EEPROMs */

#define  EEPROMBoardSerNumAdd     				0
#define  EEPROMBoardSerNumNbOfBytes 			8

#define  EEPROMBoardTypeAdd						14
#define  EEPROMBoardTypeNbOfBytes				1

#define  EEPROMBoardTypeCodeAdd 				58  

#define  EEPROMBoardSerNumCodeAdd 				55  


/*FE BOARD CTRL / CONTROL BOARD EEPROM */

#define  EEPROMIpAddressUDPAdd					8      
#define  EEPROMIpAddressUDPNbOfBytes			4

#define  EEPROMUDPCtrlPortAdd					12      
#define  EEPROMUDPCtrlPortNbOfBytes				2

#define  EEPROMUDPDaqPortAdd					12      
#define  EEPROMUDPDaqPortNbOfBytes				2


#define  EEPROMIpAddressCodeAdd 				56
#define  EEPROMUDPCtrlPortCodeAdd 				57
#define  EEPROMUDPDaqPortCodeAdd 				58


/* FE BOARD FRONT END BLOCK EEPROM   */  
	
#define EEPROMDaughterBoardTypeAdd 					8
#define EEPROMDaughterBoardTypeNbOfBytes			1

#define EEPROMTOTDacOffsetAdd						24    // signed char in mV
#define EEPROMTOTDacOffsetNbOfBytes					1

#define EEPROMTOTFilterDacOffsetAdd					25    // signed char in mV
#define EEPROMTOTFilterDacOffsetNbOfBytes			1

#define EEPROMDaughterBoardSampicVersionAdd			9
#define EEPROMDaughterBoardSampicVersionNbOfBytes	1

#define EEPROMDaughterBoardSampicEvolutionAdd	    10
#define EEPROMDaughterBoardSampicEvolutionNbOfBytes	1


#define EEPROMDaughterBoardTypeCodeAdd				56
#define EEPROMTOTDacOffsetCodeAdd  					57   
#define EEPROMTOTFilterDacOffsetCodeAdd  			58   
#define EEPROMDaughterBoardSampicVersionCodeAdd  	59  
#define EEPROMDaughterBoardSampicEvolutionCodeAdd  	60  

   


/* =============================== Registers  ================================= */ 

// CONTROL BOARD CTL FPGA
	
#define ad_control_board_FeBoardPresence   		0x2A
#define ad_control_board_dac_reg		   		0x28
#define ad_control_board_clock_reg		   		0x29 
#define ad_control_board_control_reg	   		0x21
#define ad_control_board_trigger_reg	   		0x22
#define ad_control_board_pulse_reg				0x2B
#define ad_control_board_ctrl_eeprom_reg   	   	0x30	 // R/W - 
#define ad_control_board_ctrl_eeprom_access    	0x31	 //	R/W - 

#define ad_control_board_trigtag_size           0x37     // nb of triggers per event
#define ad_control_board_si5332_access	        0x38 

#define ad_control_board_control_reg2			0x35
#define ad_control_board_ext_trig_gate_for_l3   0x34
#define ad_control_board_trigger_combi_for_l3   0x36



// CONTROL BOARD COMMANDS
	
#define ad_control_board_resync			    	0x26
#define ad_control_board_send_pulse		    	0x32
#define ad_control_board_send_reset_to_FEBs		0x33
#define ad_control_board_send_soft_trig			0x25
#define ad_control_fpga_version			    	0x23  	
#define ad_control_fpga_evolution		    	0x24
	
	
	
/* =============================== sub-addresses ============================== */ 

/* ----------     64-CH board CTRL FPGA    ------- */

 // COMMON REGISTERS 

#define ad_fe_board_ctrl_identity_reg			   0x00		//  R   -
#define ad_fe_board_ctrl_control_register2   	   0x35		//  R/W
#define ad_fe_board_ctrl_control_register 		   0x21		// 	R/W -  
#define ad_fe_board_ctrl_trigger_register 		   0x22		// 	R/W -  
#define ad_fe_board_ctrl_FPGA_version			   0x23		// 	R   -  
#define ad_fe_board_ctrl_FPGA_evolution	  	   	   0x24		// 	R   -  
#define ad_fe_board_ctrl_FeBoardPresence		   0x28     //  R/W -
#define ad_fe_board_ctrl_clock_register 		   0x29		// 	R/W - 
#define ad_fe_board_ctrl_dac_access			   	   0x2C		//  Accès DAC
#define ad_fe_board_ctrl_read_req_delay 		   0x2E  	//  R/W - 
#define ad_fe_board_ctrl_eeprom_reg     		   0x30		// 	R/W - 
#define ad_fe_board_ctrl_eeprom_access  	 	   0x31		//	R/W - 
#define ad_fe_board_ctrl_si5332_access			   0x32     //  R/W -
#define ad_fe_board_ctrl_ext_trigger_gate_for_l2   0x34		//	R/W - 
#define ad_fe_board_ctrl_trigger_combi_for_l2	   0x36		//	R/W 32 Bits



 // COMMON COMMANDS
  
#define ad_fe_board_ctrl_global_reset 				0x20		// 	W -  
#define ad_fe_board_ctrl_soft_trig	 				0x25		// 	W -  
#define ad_fe_board_ctrl_resync	 					0x26		// 	W -  
 

/* ----------     64-CH board FE FPGA  registers  ------- */    

 // COMMON REGISTERS 	 

 // COMMON COMMANDS

#define ad_fe_board_fe_global_reset      			0x00	// W -
#define ad_fe_board_fe_pulse_cmd					0x11


// INDIVIDUAL REGISTERS

#define ad_fe_board_fe_control_reg   					0x01	//  R/W - 
#define ad_fe_board_fe_eeprom_reg     					0x02	// 	R/W - 
#define ad_fe_board_fe_eeprom_access  					0x03	//	R/W - 

#define ad_fe_board_fe_fpga_version  					0x04	//	R - 
#define ad_fe_board_fe_fpga_evolution  					0x05	//	R - 

#define ad_fe_board_fe_evt_fifo							0x06     // R -

#define ad_fe_board_fe_sampic_read_length				0x07	//	R/W - 

#define ad_fe_board_fe_sampic_slow_control_access		0x08	//	R/W - 
#define ad_fe_board_fe_sampic_slow_reg_add				0x0E	//	R/W -   


#define ad_fe_board_fe_dac_a_access						0x09	//	W - 
#define ad_fe_board_fe_dac_b_access						0x0A	//	W - 

#define ad_fe_board_fe_test_reg							0x0D
#define ad_fe_board_fe_pulser_reg						0x10

#define ad_fe_board_fe_convert_length					0x12    // W/R
#define ad_fe_board_fe_convert_delay					0x13    // W/R
#define ad_fe_board_fe_ext_trig_gate_length  			0x14    // W/R
#define ad_fe_board_fe_nb_of_frames_per_block  			0x15    // W/R
#define ad_fe_board_fe_nb_of_triggers_per_block  		0x16    // W/R
#define ad_fe_board_fe_pulser_width_reg  				0x17    // W/R

#define ad_fe_board_fe_stop_throttle_reg				0x18 	// W/R	   (in 64 bytes blocks)   // 8 bits
#define ad_fe_board_fe_L2_coincidence_reg				0x19	// WR/R : 16 Bits, 8 Bits LSB = primitive gate, 8 Bits MSB: latency gate

// INDIVIDUAL COMMANDS 

#define ad_fe_board_fe_sampic_soft_trig	 				0x0B	// W -  
#define ad_fe_board_fe_sampic_rst_ext_trig 				0x0C	// W -  
 
/* ------ sous-adresses dans les circuits SAMPIC ------------------ */

#define SampicChannelRegOffsetAdd			1
#define SampicConfigReg1Add					17
#define SampicConfigReg2Add					18 
#define SampicConfigReg3Add					19 
#define SampicConfigReg4Add 				20
#define SampicConfigReg5Add 				21 
#define SampicConfigReg6Add 				22 
#define SampicConfigReg7Add 				23 
#define SampicConfigReg8Add 				24 
#define SampicConfigReg9Add 				25 
#define SampicConfigReg10Add 				26



#pragma pack(1) 

/* ================================================================================ */
/* ================================================================================ */

typedef unsigned long ulong;
typedef unsigned short ushort;
typedef unsigned char uchar;
typedef int Boolean;  

// Definition des enumérations pour les accès hardware


typedef enum
{
	
	SAMPIC_BOARD = 0,
	SAMPET_BOARD = 1,
	SAMPIC_BYPASS_BOARD = 2,
	SAMPIC_SLOW_BOARD
	
} FEBoardType_t;


typedef enum
{

 SAMPIC_SYSTEM = 0,
 SAMPET_SYSTEM = 1,  // DIGITAL
 SAMPIC_BYPASS = 2,
 SAMPIC_SLOW
	
	
}SystemType_t;


typedef enum
{
    CB_CTRL_EEPROM,
	FEB_CTRL_EEPROM,
  	FEB_FE_EEPROM
}
EepromSourceType_t;


typedef enum
{
	CB_CTRL_ALL_FPGA,
	FEB_CTRL_ALL_FPGA,
	FEB_FE_ALL_FPGA,
	
	ALL_FPGAS
	
}ResetType_t;

typedef enum
{
    CTRLB_CONTROL_REG,
	CTRLB_CONTROL_REG2,
	CTRLB_TRIGGER_REG,
	CTRLB_NB_OF_TRIGGER_PER_EVENT,
	CTRLB_TRIGGER_COMBI_LOGIC_FOR_L3,
	CTRLB_EXT_TRIG_GATE_FOR_L3,
	CTRLB_CLOCK_REG,
	CTRLB_PULSE_REG,
	
	
	CTRLB_ALL_REGS,
		
	FEB_CTRL_CONTROL_REG,
	FEB_CTRL_CONTROL_REG2,
	FEB_CTRL_TRIGGER_COMBI_LOGIC_FOR_L2,
	FEB_CTRL_FE_BOARD_PRESENCE,
	FEB_CTRL_TRIGGER_REG,
	FEB_CTRL_CLOCK_REG,
	FEB_CTRL_READ_REQ_DELAY,
	FEB_CTRL_EXT_TRIG_GATE_FOR_L2,

	FEB_CTRL_ALL_REGS,
	
	FEB_FE_CONTROL_REG,
	FEB_FE_PULSER_REG,
	FEB_FE_PULSER_WIDTH,
	FEB_FE_CONVERT_LENGTH_REG,
	FEB_FE_CONVERT_DELAY_REG,
	FEB_FE_EXT_TRIG_GATE_LENGTH,
	FEB_FE_NB_OF_FRAMES_PER_BLOCK,
	FEB_FE_NB_OF_TRIGGERS_PER_EVT,
	FEB_FE_STOP_THROTTLE_REG,
	FEB_FE_L2_COINCIDENCE_REG,
	
	FEB_FE_ALL_REGS, 
	FEB_FE_SAMPIC_READ_LENGTH, 
	
	
	
	FEB_SAMPIC_SC_CHANNEL_REG,
	FEB_SAMPIC_SC_REG1,
	FEB_SAMPIC_SC_REG2,
	FEB_SAMPIC_SC_REG3,
	FEB_SAMPIC_SC_REG4,
	FEB_SAMPIC_SC_REG5,
	FEB_SAMPIC_SC_REG6,
	FEB_SAMPIC_SC_REG7,
	FEB_SAMPIC_SC_REG8,
	FEB_SAMPIC_SC_REG9,
	FEB_SAMPIC_SC_REG10,
	
	FEB_SAMPIC_SC_ALL_REGS,
	
	ALL_REGS

}BoardRegType_t;

typedef enum
{
	EXT_SIG_DAC_TYPE,
	SAMPIC_DAC_TYPE
	
}DacRangeType_t;

// Autres énumérations

 typedef enum
{
	
	FEB_VDAC_RESET,
	FEB_VDAC_START_RAMP, 			// can be ADC or TOT depending on SAMPIC and FRONT END Block versions 
	FEB_VDAC_EXT_THRESHOLD,
	FEB_VDAC_DLL,
	
	CB_EXT_TRIG_THRESHOLD,  
	CB_EXT_SYNC_THRESHOLD,
  	
	ALL_VDACS
	
}DacType_t;
 
typedef enum
{
    SAMPIC_CHANNEL_SELF_TRIGGER_MODE = 0,
   	SAMPIC_CHANNEL_CHAINED_TO_PREVIOUS_CH_MODE = 1, 
	SAMPIC_CHANNEL_CENTRAL_TRIGGER_MODE = 2,   
	SAMPIC_CHANNEL_EXT_TRIGGER_MODE = 3
	
}SAMPIC_ChannelTriggerMode_t;


typedef enum
{

	PULSER_SRC_IS_SOFT_CMD,
	PULSER_SRC_IS_AUTO
	
	
}PulserSourceType_t;

typedef enum
{
	SAMPIC_TOT_RANGE_MAX_25_NS,
	SAMPIC_TOT_RANGE_MAX_50_NS,
	SAMPIC_TOT_RANGE_MAX_100_NS,
	SAMPIC_TOT_RANGE_MAX_200_NS,
	SAMPIC_TOT_RANGE_MAX_400_NS,
	
}SAMPIC_TOTRange_t;

typedef enum
 {
	 END_OF_CONV,
	 END_OF_READ,
	 EXT_RES_TRIG,
	 POR
	 
 }SampicTriggerResetSrcType_t;

typedef enum
{
	ADC_COUNTER_IS_11BITS,   // 2047
	ADC_COUNTER_IS_10BITS,   // 1023
	ADC_COUNTER_IS_9BITS,    // 511
	ADC_COUNTER_IS_8BITS     // 255
   
	
}EndOfADCGrayCounterType_t;

typedef enum
 {
	NORMAL,
	CENTRAL_TRIGGER,
	NORMAL_DELAYED,
	NORMAL_DOUBLE_DELAYED
	 
 }SampicSelfTriggerModeType_t;



typedef enum
{
	
	DLL_ULTRA_SLOW = 0,
	DLL_SLOW = 1,
	DLL_MEDIUM = 2,
	DLL_FAST = 3
	
}SampicDLLModeType_t;


typedef enum
{

	TRIG_CHANNEL_ONLY_IF_PARTICIPATING_TO_CT,
	TRIG_ALL_CHANNELS_AFTER_CT,
}SampicCentralTriggerEffect_t;


typedef enum
{
	
	CENTRAL_OR,
	CENTRAL_MULTIPLICITY_2,
	CENTRAL_MULTIPLICITY_3
	
}SampicCentralTriggerMode_t;

	

typedef enum
{
 
 RAW_PRIMITIVES_FOR_CT,
 GATED_PRIMITIVES_FOR_CT
	
	
}SAMPIC_CT_PrimitivesMode_t;

typedef enum
{
  RING_OSC,
  FCK_QUARTER,
  HALF_RING_OSC,
  RING_OSC_QUARTER
}
SampicADCClockSrcType_t;


typedef enum
{
	NO_CLOCK =  1,
	ROSC_1_4 =  0,
	ROSC_1_8 =  2,
	ROSC_1_16 = 3,
	
}TimeINLCalibClockFreqType_t;


typedef enum
{
	COUNTER_OVERFLOW = 0,
	RAMP_OVERFLOW =1
	
}ReloadRampMode_t;



typedef enum
{
	V3_NO_CLOCK =  0,
	V3_ROSC_1_4 =  1,
	V3_ROSC_1_8 =  2,
	V3_ROSC_1_16 = 3,
	V3_TSCLK_1_2 = 4,
	V3_TSCLK_1_4 = 5,
	V3_TSCLK_1_8 = 6,
	V3_TSCLK_1_16 = 7
	
}TimeINLCalibClockFreqTypeV3_t;


typedef enum
{
 COMPOSITE = 0,
 ADC_COUNTER = 1,
 ADC_COUNTER_LATCHED = 2,
 TIMESTAMP_COUNTER = 3,
 DEBUG = 7
}
SampicTestOutModeType_t;

typedef enum
{
 SOFTWARE = 0,
 INTERNAL_OSC = 2,
 EXT_SIG = 4
	
}ExternalTriggerType_t;

typedef enum
{
	CB_CTRL_FPGA,
	FEB_CTRL_FPGA,
	FEB_FE_FPGA
	
}FpgaType_t;
 

typedef enum
{
	RISING_EDGE,
	FALLING_EDGE
	
}EdgeType_t;

typedef enum 
{
	
	TTL_SIG,
	NIM_SIG
	 
}SignalLevel_t;

typedef enum
{
	LOGIC_AND, 
	LOGIC_OR,
	LEFT,
	RIGHT,
	FORCE_0,
	FORCE_1
	
}CombiTriggerLogic_t;
  
 typedef enum
 {
	EEPROM_BOTTOM_PAGE,
	EEPROM_TOP_PAGE
 }EepromPage_t;
 


 
 typedef enum
 {
	INTERNAL_CLK = 0,
	EXT_SIGNAL = 1
	 
 }ExtAsyncTriggerSourceType_t;
   


 typedef enum
 {
   NO_CONNECTION = -1,
   USB_CONNECTION = 0,
   UDP_CONNECTION = 1
	 
 }ConnectionType_t;
 
 typedef enum
 {
	CTRL_ONLY,
	CTRL_AND_DAQ
	 
	 
 }ControlType_t;
 
 
 typedef enum
 {
	 CTRL_ACCESS,
	 DAQ_ACCESS,
	 
 }AccessType_t;
  
 typedef enum
 {

	CALIB_VALUES_NOT_LOADED = 0,
	CALIB_VALUES_LOADED_FROM_FILE = 1,
	CALIB_VALUES_LOADED_FROM_SOFTWARE = 2
	 
 }CalibStatus_t;
 
 
 
 typedef enum
{

	SAMPIC_1_6_GS = 1600,
	SAMPIC_2_133_GS = 2133,
	SAMPIC_3_2_GS= 3200,
	SAMPIC_4_252_GS = 4252,
 	SAMPIC_6_4_GS = 6400,
	SAMPIC_8_512_GS = 8512,
	SAMPIC_10_24_GS = 10240

	
}SAMPIC_InternalSamplingFreqType_t;



typedef enum
{
  SAMPIC_INT_THRESHOLD_SOURCE = 0,
  SAMPIC_EXT_THRESHOLD_SOURCE = 1
	
}SAMPIC_ThesholdSource_t;


typedef enum
{
	
	FEB_GLOBAL_TRIGGER_IS_L2,
	FEB_GLOBAL_TRIGGER_IS_L3
	
}FebGlobalTrigger_t;


typedef enum
{
	
	SAMPIC_TRIGGER_IS_L1,
	SAMPIC_TRISSER_IS_FEB_GT
	
}SampicTriggerOption_t;

 // Structures
   
 typedef struct{
	 
CalibStatus_t 							CalibStatus;
int									    CalibFreqEch; /* in MHz */
int			  							CalibADCNbOfBits;
	 
	 
 }CalibStatusStruct;
 
 typedef struct{
	

	
	// ADC Linearity Calib Values
	 
	double			CellLinearity_a2[NB_OF_CHANNELS_IN_FE_BOARD][MAX_NB_OF_SAMPLES];
	double			CellLinearity_a1[NB_OF_CHANNELS_IN_FE_BOARD][MAX_NB_OF_SAMPLES];
	double			CellLinearity_a0[NB_OF_CHANNELS_IN_FE_BOARD][MAX_NB_OF_SAMPLES];


	double			ResidualPedestalValues[NB_OF_CHANNELS_IN_FE_BOARD][MAX_NB_OF_SAMPLES];

	double 			TimeINLValues[NB_OF_CHANNELS_IN_FE_BOARD][MAX_NB_OF_SAMPLES]; // en ps 

	float			InternalTriggerThresholdOffset[NB_OF_CHANNELS_IN_FE_BOARD];
	float			InternalTriggerThresholdSlope[NB_OF_CHANNELS_IN_FE_BOARD]; 
	
	float			VDAC_Ramp[NB_OF_SAMPICS_IN_FE_BOARD][MAX_NB_OF_ADC_MODES]; //for 11 Bits, 10 Bits, 9 Bits and 8 Bits
	
	float			TOTCalibSlope[MAX_NB_OF_TOT_RAMP_CURRENT_VALUES][NB_OF_CHANNELS_IN_FE_BOARD];
	float			TOTCalibIntercept[MAX_NB_OF_TOT_RAMP_CURRENT_VALUES][NB_OF_CHANNELS_IN_FE_BOARD];
	
	float 			TOTDacOffset[NB_OF_SAMPICS_IN_FE_BOARD];
	float			TOTFilterDacOffset[NB_OF_SAMPICS_IN_FE_BOARD];

 }CalibrationValuesStruct;



typedef struct {
	
	  CalibStatusStruct		TimeINLCalibStatus[MAX_NB_OF_FE_BOARDS][NB_OF_CHANNELS_IN_FE_BOARD];
	  CalibStatusStruct		ADCLinearityCalibStatus[MAX_NB_OF_FE_BOARDS];
	 
	  
	  CalibStatusStruct		ResidualPedestalCalibStatus[MAX_NB_OF_FE_BOARDS][NB_OF_CHANNELS_IN_FE_BOARD];
	  CalibStatusStruct		InternalTriggerThresholdCalibStatus[MAX_NB_OF_FE_BOARDS];
	  CalibStatusStruct		ADCRampCalibStatus[MAX_NB_OF_FE_BOARDS];
	  CalibStatusStruct		TOTCalibStatus[MAX_NB_OF_FE_BOARDS];  

	  CalibStatusStruct     ADCLinearityCrateCalibStatus;
	  CalibStatusStruct     TOTCrateCalibStatus;
	
	
}CrateCalibStatusStruct;

 
/* =========================================================================*/
/* --------------------------  Structures --------------------------------- */
/* =========================================================================*/

 typedef struct{
	 
	ConnectionType_t ConnectionType;
	ControlType_t	 ControlBoardControlType;
	
	char    UsbDeviceDescr[MAX_DEVICE_DESCR_LENGTH];
	char    UsbSerNum[MAX_SERNUM_LENGTH];

	int  	CtrlDeviceHandle;
	int     NbOfDAQConnections; 
	
	int     DaqHandle[MAX_NB_OF_FE_BOARDS];
	char    DaqIpAddress[MAX_NB_OF_FE_BOARDS][MAX_IP_ADDRESS_LENGTH];
	char    DaqPort[MAX_NB_OF_FE_BOARDS]; 

	char    CtrlIpAddress[MAX_IP_ADDRESS_LENGTH];
	int     CtrlPort;
	
	 
 }CrateConnectionInfoStruct;	
 
 typedef struct{
	 
	ConnectionType_t ConnectionType;
	ControlType_t	 ControlBoardControlType;

	char    CrateSerNum[MAX_SERNUM_LENGTH];
	
	int     NbOfDAQConnections; 
	 
	char    CtrlIpAddress[MAX_IP_ADDRESS_LENGTH];
	int     CtrlPort;
	
	char    DaqIpAddress[MAX_NB_OF_FE_BOARDS][MAX_IP_ADDRESS_LENGTH];
	char    DaqPort[MAX_NB_OF_FE_BOARDS]; 

	 
 }CrateConnectionParamStruct;
 
 
 typedef struct{
	 
int BoardVersion;
int FPGAVersion;
int FPGAEvolution;
	 
 }FirmwareVersionStruct;
	 
 typedef struct{
	 
	 
	 
	 // clock reg
	 
	 unsigned short ClockRegister;
	 
	 
	 unsigned char   	Si5332RegO0; // Registre O0 du Si5332 => fixe la fréquence de sortie
	 unsigned short		Si5332Idpa; 
	 unsigned char 	    Si5332PDivider;
	 unsigned char		Si5332OutDivider; // R Out0 = Out1 = Out2 = Out4 = Out6)
	 unsigned char 		Si5332PllMode;
     unsigned char		Si5332OMux0_Cell;   // default 0 => PLL, sinon 0x73 pour le bypass direct

	 
	 // control reg
	 
	 unsigned char ControlRegister;
	 	Boolean		EnableTrigger;
		Boolean		EnableAutoPulse;
		
		// Trigger Counter
	
		Boolean					EnableExtTrigCounter;   // Bit2
		Boolean					EnableDetectTriggerIDfromExtTrig;   //Bit3
		
		
		
	unsigned char 	 NbOfTriggersPerEvent;
		
	//control Reg2
		
	unsigned char	ControlRegister2;
		Boolean			EnableCoincidenceWithExtTrigGateForL3;   // bit 0
		Boolean			CrateIsInStandaloneMode; // bit 1, if '0' => crate synchronised with an other crate.
		Boolean			CrateIsInMasterSyncMode;  //Bit 2
		Boolean			EnableMasterSlaveCoincidence; // Bit 3

		
	unsigned int	TriggerCombiLogicForL3;
		Boolean						EnableBuildL3;   // bit 26
		int							SelFeb0ForL3; //bits 0-1
		int							SelFeb1ForL3; //bits 2-3
		int							SelFeb2ForL3; //bits 4-5
		int							SelFeb3ForL3; //bits 6-7
		CombiTriggerLogic_t	  		Layer1TriggerLogic0; // bits 8-10
		CombiTriggerLogic_t	  		Layer1TriggerLogic1; // bits 11-13
		CombiTriggerLogic_t	  		Layer1TriggerLogic2; // bits 14-16
		CombiTriggerLogic_t	  		Layer2TriggerLogic0; // bits 17-19
		CombiTriggerLogic_t	  		Layer2TriggerLogic1; // bits 20-22
		CombiTriggerLogic_t	  		Layer3TriggerLogic; 	   // bits 23-25
	 
	unsigned short	PulseReg; // Clock Divider for Pulse  (base clock = fe_clock /16)
	 
	 //trigger reg
	 unsigned char TriggerRegister;
	 
	ExternalTriggerType_t	ExternalTriggerType; 
	 
	SignalLevel_t			ExternalTriggerSigLevel; 
	EdgeType_t				ExternalTriggerEdge;
	
	SignalLevel_t			ExternalSyncSigLevel; 
	EdgeType_t				ExternalSyncEdge;  
	
	unsigned char 			ExternalTrigGateForL3; 
	
	Boolean					EnableFlowControl;
	
	

		
	
 }ControlBoardParamStruct; 
 

  typedef struct{
	 
	int							SelInput0; //bits 0-1
	int							SelInput1; //bits 2-3
	int							SelInput2; //bits 4-5
	int							SelInput3; //bits 6-7
	CombiTriggerLogic_t	  		Layer1TriggerLogic0; // bits 8-10
	CombiTriggerLogic_t	  		Layer1TriggerLogic1; // bits 11-13
	CombiTriggerLogic_t	  		Layer1TriggerLogic2; // bits 14-16
	CombiTriggerLogic_t	  		Layer2TriggerLogic0; // bits 17-19
	CombiTriggerLogic_t	  		Layer2TriggerLogic1; // bits 20-22
	CombiTriggerLogic_t	  		Layer3TriggerLogic; 	   // bits 23-25
	
	 
	 
 }TriggerLogicParamStruct;
  
/* ---------- Paramètres Individuels aux SAMPICs   ------------ */

typedef struct{
		    
	/* --------------------------------------------------------- */
	/* -------------- Internal SAMPIC REGISTERS   -------------- */
	/* --------------------------------------------------------- */
	
// CHANNEL REGISTERs (Registers 1 to 16) 
	
		unsigned short	ChannelRegister[NB_OF_CHANNELS_IN_SAMPIC];   					// 16-Bits register
			float				RelativeInternalThreshold[NB_OF_CHANNELS_IN_SAMPIC]; // relative to baseline
		    float			    InternalThreshold[NB_OF_CHANNELS_IN_SAMPIC];   			
			Boolean				ExtDiscriThresholdSource[NB_OF_CHANNELS_IN_SAMPIC];
			Boolean				EnableTriggerChannel[NB_OF_CHANNELS_IN_SAMPIC];
			Boolean				SelfTriggerOn[NB_OF_CHANNELS_IN_SAMPIC]; 				// OFF = External
			EdgeType_t			TriggerEdge[NB_OF_CHANNELS_IN_SAMPIC];
			
			// Modification for SAMPIC V2
			SAMPIC_ChannelTriggerMode_t	ChannelTriggerMode[NB_OF_CHANNELS_IN_SAMPIC]; // Self-Trigger, Central Trigger, Channel_Chained,External_Trigger (and No Trigger = channel OFF)
			Boolean						DisableChannelForCentralTrigger[NB_OF_CHANNELS_IN_SAMPIC];
			
	
			
// CONFIG REG1		(Register 17)
			
	   	unsigned short	ConfigReg1;   	 // 16-Bits register
			SampicTriggerResetSrcType_t	 TriggerResetSource;
			SampicSelfTriggerModeType_t	 SelfTriggerMode; 
			Boolean 					 DisableDelayDLL;
			Boolean						 DLLFastMode; 	//	1 = Fast, 	 0 = Slow 
			Boolean						 DLLOut; 		//	1 = Enabled, 0 = Disabled
			Boolean						 DisableDelayClk; 		// 	1 = Enabled, 0 = Disabled
			
			Boolean						 ThresholdDACsPowerDown;
			
			// Modification for SAMPIC V2 
			
	 		Boolean 					  EnCoincidenceModeForCentralTrigger;
			
			//Modification for SAMPIC V3
		
			Boolean						  ResetChargePumps; // permanent reset of both charge pumps  (main DLL and Delay Servo Control)
			Boolean						  ResetAllSequencers; // Reset of all FlipFlops and Sequencers inmplied in the event conversion and readout
			SampicDLLModeType_t			  DLLMode; 
			
// CONFIG REG2 (Register 18)    
			
		unsigned short	ConfigReg2;   	 // 16-Bits register
			Boolean							ForceRingOscAlwaysON;
			SampicADCClockSrcType_t			ADCClockSource;
			SampicTestOutModeType_t			TestOutMode;
			Boolean							InhibRstADC;
			Boolean							InhibReadoutData;
			Boolean							InhibTokenExtReset;
			Boolean							SelHighLVDSCurrent;
			Boolean							SCDataResync;
			Boolean							SCEdgeDataResync;
			
		 	
		   // Modification for SAMPIC V2 
			
		    EndOfADCGrayCounterType_t 		EndOfADCGrayCounter;
			Boolean							EnablePostTrigger;
			
			// Modification for SAMPIC V3
			
			SampicCentralTriggerEffect_t	CentralTriggerEffect;	
			Boolean							EnableTripleCoincidenceMode;
		
			
// CONFIG REG3 	(Register 19)    	
	   unsigned short ConfigReg3; // 16-Bits register
	        float 							Vdac_DLLContinuity; 
			Boolean							DLLContinuityPowerDown;
			
		   // Modification for SAMPIC V2 
		
		    Boolean							EnableADCCompOverflow;  // '1' Enables using the overflow option for stopping the ADC Conversion
			Boolean							EnableInhibComp;  		// '1' Enables automatic inhibition of the cell comparators out of the conversion phase 
			Boolean							DisablePrimitiveDeglitcher; // = NoShapeChannel:if '0' => raw discri used for the self trigger, '1': shaped discri used instead
			Boolean							EnableTOTFilterWideCap; // '1' the TOT filter ramp uses the 100fF capacitor, '0' 1pF is used instead.
			Boolean							SelGatedDiscriForCTPrimitives;  // or EnShaperTrig	  : '1' gated primitives used for Central Trigger, '0' raw shaped discri used instead
		
		  // Modification for SAMPIC V3
			
			Boolean							EnableDigitalInput;
			
			
			
	//New Registers for Sampic Versions >= 2
			
// CONFIG REG4 	(Register 20)    
			
	unsigned short 		ConfigReg4;
		float			InternalTOTFilterDAC;
		float 			InternalTOTFilterWidth; // in ns   // conversion of the above value in ns

		Boolean			EnableExternalTOTFilterDAC;
		Boolean		 	EnableTOTFilter;
		unsigned char 	AutoConversionDelayThird; //value between 0 and 7
		Boolean			EnablePingPong;
		
	
// CONFIG REG5 	(Register 21) 
	
	unsigned short ConfigReg5;
	    SAMPIC_TOTRange_t	TOTRange;
		float				InternalTOTRampCurrentDAC; // DAC voltage is applied to an external 10k reistor to produce the current. Ramp current range is 3.5 to 50µA
		Boolean				EnableExternalTOTRampCurrentDAC;
//		Boolean				EnableTOTMeasurement;
		unsigned char		TourDePisteDelay; //value between 0 and 7 : it is the delay between enable_write in the analog Memory and trigger enabling, units are 1/8 of the clock period
		Boolean				DisableTOTOverflow; // '1' disables using the TOT voltage overflow to stop the TOT ramp.
	
	
// CONFIG REG6 	(Register 22)
	
	unsigned short 	ConfigReg6;  
		//float    		InternalADCRampDAC;
		Boolean	 		EnableExternalADCRampDAC;
		Boolean	 		EnableSyncConversion; //'1' enable synchronising the start of the ADC ramp with ADC Clock/16
		unsigned char 	PostTrig; // between 0 and 7; Units are 1/8 of Clock Period
		Boolean			EnableCommonDeadTime;
	
	
// CONFIG REG7 	(Register 23) 
	
  	unsigned short ConfigReg7; 
		float 			InternalOverflowDAC;  // DAC for Voltage of the overflow of the ADC Ramps. Each ramp will stop as soon as this voltage is crossed
		Boolean			EnableExternalOverflowDAC;
		unsigned char	PrimitivesGateLength; // Units are 1/8 of clock Period
		
	//Modification Sampic V3
		Boolean				EnableDataResynchro;
		Boolean				InhibReloadRampData; // inhibits the reload of the channel Cell Latches in case of conversion overflow
		ReloadRampMode_t	ReloadRampSrce; // if '0' the srce of the reload of the channel cell latches is the overflow of the counter, '1': the srce is the overflow of the ramp
		
	
// CONFIG REG8 	(Register 24) 
	
  	unsigned short ConfigReg8;
		float						InternalRoscDAC;
		Boolean 					EnableExternalRoscDAC;
		TimeINLCalibClockFreqType_t	TimeINLCalibClockFreq;
	
	//Modification Sampic V3
		TimeINLCalibClockFreqTypeV3_t 	TimeINLCalibClockFreqV3;
		Boolean							EnableExternalVreset;
		Boolean							EnableTranslator; 

	
// CONFIG REG9 	(Register 25) 
	
  	unsigned short ConfigReg9; 
		float   	InternalADCCalibDAC ; // DAC for setting the voltage used by the ADC Calibration
		int			InternalADCCalibDACIntValue;
		Boolean 	EnableReturnBuffer;  
		Boolean 	EnableDiffN;
		Boolean 	EnableDiffP;
		Boolean 	LVDSPolarityIsNegative;
		Boolean		EnableInternalVReset;
		Boolean 	EnableADCCalibDAC;
	
// CONFIG REG10 (Register 26) 
	
  	unsigned short ConfigReg10; 
		unsigned char	TimeINLCalibRisingEdgeSlope;	// value between 0 and 15
		unsigned char	TimeINLCalibFallingEdgeSlope;	// value between 0 and 15
		unsigned char 	TimeINLCalibVLow; 				// value between 0 and 7
		unsigned char   TimeINLCalibVHigh;              // value between 0 and 7
		Boolean			EnableTimeINLCalib;
		Boolean			EnableReturnGnd;
			
	/* --------------------------------------------------------- */
	/* ---------- END OF INTERNAL SAMPIC REGISTERS  ------------ */
	/* --------------------------------------------------------- */
	
			
			float							ExternalThreshold;
			
			float							Vdac_Rosc;
			float							Vdac_StartRamp;
			float							Vdac_DLL;
			float							Vdac_Dacm;
			float							Vdac_Dacp;
			float							Vdac_ramp;
	
			float							Vdac_reset;
			float							VBaseline;
			
					
}SAMPICIndividualParamStruct, *SAMBPICIndividualParamStruct;



/* ----------64-ch Front End Board Control FPGA Parameters ------------ */

typedef struct{  
	

		unsigned char	ControlRegister;
		
			Boolean								EnableTrigger;
			Boolean								EnablePulserMode;
			Boolean								EnableClockOnSync;
			ExtAsyncTriggerSourceType_t			Sel_Async_Ext_Trig_Source;
		
		unsigned char	ControlRegister2;
			Boolean			EnableCoincidenceWithExtTrigGateForL2;	 // bit 0
			Boolean			FEBGlobalTriggerIsL3; //bit 1, if '0' => FEB Global Trigger is L2 else  FEB_GT is L3 
	
		unsigned char		ExternalTrigGateForL2; // à partir de la version de firmware 1.1.12 du fpga contrôleur
	
		unsigned int	TriggerCombiLogicForL2;
			int							SelSAMPIC0ForL2; //bits 0-1
			int							SelSAMPIC1ForL2; //bits 2-3
			int							SelSAMPIC2ForL2; //bits 4-5
			int							SelSAMPIC3ForL2; //bits 6-7
			CombiTriggerLogic_t	  		Layer1TriggerLogic0; // bits 8-10
			CombiTriggerLogic_t	  		Layer1TriggerLogic1; // bits 11-13
			CombiTriggerLogic_t	  		Layer1TriggerLogic2; // bits 14-16
			CombiTriggerLogic_t	  		Layer2TriggerLogic0; // bits 17-19
			CombiTriggerLogic_t	  		Layer2TriggerLogic1; // bits 20-22
			CombiTriggerLogic_t	  		Layer3TriggerLogic; 	   // bits 23-25
		
		
		unsigned char	TriggerRegister;
		
			ExternalTriggerType_t	ExternalTriggerType; 
			EdgeType_t				ExternalTriggerEdge;
			
	
		unsigned char		FeBoardsPresence;
			Boolean			FeBoardIsPresent[NB_OF_FE_FPGAS_IN_FE_BOARD];
		
		unsigned short   	ClockRegister;
		
	    unsigned char		Si5332OMux0_Cell;   // 0x73 pour le bypass direct
											    
		unsigned int		ReaqReqDelay; //delay between frames

		float 				Vthreshold_ExtTrig;
		float 				Vthreshold_ExtSync;
		    
}FeBoardControlFpgaParamStruct, *FeBoardControlFpgaParamStructPtr; 

/* ---------- Paramètres du FPGA Front End ------------ */

typedef struct{  

		unsigned short	ControlRegister;
			Boolean			EnableTrigFPGA;
			Boolean			RstDLL;
			Boolean			EnableAsyncExtTrig;
			Boolean			EnableTrigSampic;
			Boolean 		EnableExtTrigGate;
			Boolean			EnExtTrisAsEnableTrig;
			Boolean			UdpOk;
			Boolean			EnableExtTrigCounter;
			EdgeType_t		ClkEdgeForDataSampling;
			Boolean  		EnableOrTriggerMode;
			Boolean			SlowSamplingFrequency;
			
			Boolean			EnableL2Coincidence;
			
			Boolean			EnableDetectTriggerIDfromExtTrig;
   			
			unsigned char 	ExternalTrigGate;   
		
		
		unsigned int       PulserRegister;
			Boolean			EnablePulseChannel[NB_OF_CHANNELS_IN_SAMPIC];
			Boolean			PulserSourceIsSync;
			unsigned char	PulserWidth; 
	  
			
		unsigned char	StopThrottleBufferWidth; // in blocks of 64 bytes
		
		unsigned short  L2CoincidenceReg;
		
		
		SAMPICIndividualParamStruct SAMPICIndividualParams;

}FeBoardFeFpgaParamStruct, *FeBoardFeFpgaParamStructPtr; 



 typedef struct{
	 
	FeBoardControlFpgaParamStruct 	ControlFpgaParams;
	
	FeBoardFeFpgaParamStruct 		FeFpgaParams[NB_OF_FE_FPGAS_IN_FE_BOARD]; 
	 
	 
 }FeBoardParamStruct;	  
 
 
 typedef struct{
	 
	 
	  Boolean				ADCLinearityCorrection;
	  Boolean				ResidualPedestalCorrection;
	  Boolean				INLCorrection;
	  

	  
	  Boolean							UseExternalClock;
	  int								FreqEch;  // in MHz
	  
	  
	  Boolean  							EnPulseMode;
	  Boolean							EnPulseReSync;

	  Boolean							EnableAutoConversionMode;
	  Boolean							EnableTOTMeasurement;
	  Boolean							EnablePingPongMode;
	  int								ADCNbOfBits;  
	  Boolean							EnableBuildL2;
	  
	
	  //FEB FE FPGA Common params
	  int								NbOfSamplesToRead; //Nb of Samples
	  int								NbOfExtraWords;
	  Boolean							SmartRead;
	  unsigned char				        OffsetForStartOfRead; // ND: 6-Bits 
	  
	  unsigned char	 					ConvertLength;
	  unsigned short 					ConvertDelay;
		
	  unsigned char						NbOfFramesPerBlock;
	  unsigned char						NbOfTriggersPerEvent;
	  
	 unsigned char 						L2SAMPICPrimitivesGateLength;	 // number of clock periods of 10ns
	 unsigned char 					    L2CoincidenceLatencyLength; 	 // number of clock periods of 10ns       
	

	 
	 
 }CommonParamStruct;
 
 
 
 typedef struct{
 
	 
 	CrateCalibStatusStruct			CalibStatus;
	  
 	CalibrationValuesStruct			CalibValues[MAX_NB_OF_FE_BOARDS];
	
	char 				 			LastCalibDirectory[MAX_PATHNAME_LENGTH];
	 
	 
 } CrateCalibStruct;
 
 typedef struct {
	 
	 FirmwareVersionStruct  FirmwareVersion;   
	 
	 	
	 int					BoardVersion;
	 int					BoardSerNum;
	 unsigned char			FeBoardsPresence;
	 
 	
	 char    				IpAddressFromEEPROM[MAX_IP_ADDRESS_LENGTH];
	 
	 int     				CtrlPortFromEEPROM;
	 int     				DaqPortFromEEPROM; 
	 
 	Boolean					Si5332IsPresent;    

 	 
 } ControlBoardInfoStruct;
 
 typedef struct {
	 
	 FirmwareVersionStruct  ControlFpgaFirmwareVersion;
	 FirmwareVersionStruct  FeFpgaFirmwareVersion[NB_OF_FE_FPGAS_IN_FE_BOARD];
	
	 
	 int				BoardVersion;
	 int				BoardSerNum;
	 
	 Boolean			Si5332IsPresent;  

	 
	 char   IpAddressFromEEPROM[MAX_IP_ADDRESS_LENGTH];
	 
	 int    CtrlPortFromEEPROM;
	 int   	DaqPortFromEEPROM; 
	 
	 char					FeBoardTypeCharFromEEPROM;
	 
	 int					FeBlockVersion[NB_OF_FE_FPGAS_IN_FE_BOARD];
	 int					FeBlockSerNum[NB_OF_FE_FPGAS_IN_FE_BOARD];

	 FEBoardType_t			FeBoardType[NB_OF_FE_FPGAS_IN_FE_BOARD];
	 int					SampicVersion[NB_OF_FE_FPGAS_IN_FE_BOARD]; 
	 char					SampicEvolution[NB_OF_FE_FPGAS_IN_FE_BOARD]; 
	 
 }FeBoardInfoStruct;
 
 
 typedef struct{
	 
	 ControlBoardInfoStruct	  ControlBoardInfo;
	 
	 FeBoardInfoStruct		  FeBoardInfo[MAX_NB_OF_FE_BOARDS];
	 
 }
CrateBoardsInfoStruct;

typedef struct  {
	 
	AccessType_t AccessType;
	FpgaType_t FpgaType;
	int FeBoardTarget;
	int FeFpgaTarget;
	
	char SubAddress;
	int  WordCountReq;
	int  WordCountAcq;
	
	
}CommErrorInfoStruct;


typedef struct{
	 
  	CrateConnectionInfoStruct  		ConnectionInfo;
	
	int								FrontEndBoardsPathIndex[MAX_NB_OF_FE_BOARDS];

  	SystemType_t					SystemType;
  	int								SampicVersion;
	
	int								NbOfFeBoards;
	
	CrateBoardsInfoStruct 		    CrateBoardsInfo;
	
	CommErrorInfoStruct				LastCommErrorInfo;
	
	
	CrateCalibStruct			    CrateCalibInfo;
	

 }CrateInfoStruct;
 
 
  typedef struct{
	  
	  CommonParamStruct 		 		CommonParams;

   	  ControlBoardParamStruct    		ControlBoardParams;
	  
	  FeBoardParamStruct		 		FeBoardParams[MAX_NB_OF_FE_BOARDS];
	  		 
  }CrateParamStruct;


typedef struct
{
	unsigned int address; /* 8-bit register address */
	unsigned char value; /* 8-bit register data */

} Si5332_I2CRegAccessStruct;


	
typedef struct  
{
unsigned short			SampicDataHeader;

int						FirstTriggerPositionCell;
int						TriggerPositionCell;
double					PhysicalCell0TimeStamp;

int						SampicTimeStampA;
int						SampicTimeStampB;
unsigned long long		FPGATimeStamp;

int 					ADCCounter_LatchedAtEndOfConv;
Boolean					TriggerPosition[MAX_NB_OF_SAMPLES];
int						TimePhysicalIndex;

int 					StartOfADCRamp;
						
	
} AdvancedHitStruct, *AdvancedHitStructPtr;
 
typedef struct{
	
	int 		FeBoardIndex;
	int 		Channel;   // 0 to 63

	int			HitNumber; // in event?

	int 		SampicIndex;  
	int 		ChannelIndex;

	int 		DataSize;
	
	Boolean 	INLCorrected;
	Boolean 	ADCCorrected;
	Boolean		ResidualPedestalCorrected;

	int				CellInfo;										// physical index of the waveform sent by SAMPIC
	
	int 			FirstCellPhysicalIndex;   						// of the ordered waveformex  CellInfoForOrderedData
	
   	unsigned short	RawDataSamples[MAX_NB_OF_SAMPLES];  			// non corrected non ordered
	unsigned short 	OrderedRawDataSamples[MAX_NB_OF_SAMPLES];	  	// ordered  in adc count.

	float   		CorrectedDataSamples[MAX_NB_OF_SAMPLES];	    // corrected data samples with resepct to the enabled corrections (adc + time)

														 
	int 			RawTOTValue; // ADC Count
	float 			TOTValue;//ns  	 

	float			Amplitude;
	float 			Baseline;
	float 			Peak;
	
	float 		    TimeIndex;
	double 		    TimeInstant;  				//ns 			//ex CFDTimeIndex
	float  			TimeAmplitude; 
	double 			FirstCellTimeStamp; 		// ns		   // TriggerCellTimeInstant
	
	AdvancedHitStruct AdvancedParams;
	
	
}HitStruct, *HitStructPtr;

typedef struct{
	 
int					NbOfTriggers; 
unsigned char   	RawData[MAX_BYTES_TO_READ];
int					RawDataSize;

int 				TriggerIDFromFPGA[MAX_NB_OF_TRIGGERS_IN_EVENT];	   // from FPGA
double		    	TriggerTimeStamp[MAX_NB_OF_TRIGGERS_IN_EVENT];

unsigned int		TriggerIDFromExtTrig[MAX_NB_OF_TRIGGERS_IN_EVENT];
unsigned short		SpillNumberFromExtTrig[MAX_NB_OF_TRIGGERS_IN_EVENT];    // T2K-mode
unsigned short		RawExtraWord[MAX_NB_OF_TRIGGERS_IN_EVENT];  			// T2K-mode 


	 
}TriggerDataStruct, *TriggerDataStructPtr;

 typedef struct{
	 
HitStruct  			Hit[MAX_EXPECTED_FRAMES];
int		   			NbOfHitsInEvent;

TriggerDataStruct   TriggerData;

	 
}EventStruct, *EventStructPtr;
 
 

 
 
#pragma pack() 




/* =============================================================================================== */
/* =============================================================================================== */


#endif
