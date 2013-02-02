//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this sample source code is subject to the terms of the Microsoft
// license agreement under which you licensed this sample source code. If
// you did not accept the terms of the license agreement, you are not
// authorized to use this sample source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the LICENSE.RTF on your install media or the root of your tools installation.
// THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
//------------------------------------------------------------------------------
//
//  Header: sdpdd.h
//

#ifndef __SDPDD_H
#define __SDPDD_H

#include <windows.h>

#define BLOCK_LEN  512
#define BLOCK_MASK (~(BLOCK_LEN-1))

//----------------------------------------------------------------------------------
//
//							LIST OF COMMAND TYPES
//
//----------------------------------------------------------------------------------

typedef enum {
	CMD_TYPE_BC   = 0,	// Broadcast Command no response (to all cards)
	CMD_TYPE_BCR  = 1,	// Broadcast Command w/ response (to all cards)
	CMD_TYPE_AC   = 2,	// Addressed Command no data 	 (to selected card)
	CMD_TYPE_ADTC = 3	// Addressed Command w/ data	 (to selected card)
} CMD_TYPE;


//----------------------------------------------------------------------------------
//
//		         			LIST OF RESPONSE TYPES
//
//----------------------------------------------------------------------------------

typedef enum {
	RESP_TYPE_NONE = 0,	// No Response
	RESP_TYPE_R1   = 1,	// Normal Response 
	RESP_TYPE_R1B  = 9,	// Normal Response with Busy
	RESP_TYPE_R2   = 2,	// CID/CSD Register Response (the long response)
	RESP_TYPE_R3   = 3,	// OCR (Operating condition) register
	RESP_TYPE_R4   = 4,	// Fast I/O Response			(MMC only)
	RESP_TYPE_R5   = 5,	// Interrupt Request Response		(MMC only)
	RESP_TYPE_R6   = 6,	// Published RCA Response		(SD only)
} RESP_TYPE;


//----------------------------------------------------------------------------------
//
//					      		 CARD REGISTERS
//
//----------------------------------------------------------------------------------

typedef enum {
	CARD_STATUS_REGISTER,
	OCR_REGISTER,	// OCR: Operating Conditions 
 	CID_REGISTER,   // CID: Card IDentification
	CSD_REGISTER,	// CSD: Card-Specific Data
	RCA_REGISTER,   // RCA: Relative Card Address
	SCR_REGISTER,   // SCR: SD-card Configuration (SD only)
} SD_RESPONSE;


//----------------------------------------------------------------------------------
//
//							CARD STATUS REGISTER BITS
//
//----------------------------------------------------------------------------------

#define CSTAT_OUT_OF_RANGE			0x80000000
#define CSTAT_ADDRESS_ERROR			0x40000000
#define CSTAT_BLOCK_LEN_ERROR		0x20000000
#define CSTAT_ERASE_SEQ_ERROR		0x10000000
#define CSTAT_ERASE_PARAM			0x08000000
#define CSTAT_WP_VIOLATION			0x04000000
#define CSTAT_CARD_IS_LOCKED		0x02000000
#define CSTAT_LOCK_UNLOCK_FAILED	0x01000000
#define CSTAT_COM_CRC_ERROR			0x00800000
#define CSTAT_ILLEGAL_COMMAND		0x00400000
#define CSTAT_CARD_ECC_FAILED		0x00200000
#define CSTAT_CC_ERROR				0x00100000
#define CSTAT_ERROR					0x00080000
#define CSTAT_UNDERRUN				0x00040000
#define CSTAT_OVERRUN				0x00020000
#define CSTAT_CIDCSD_OVERWRITE		0x00010000
#define CSTAT_WP_ERASE_SKIP			0x00008000
#define CSTAT_CARD_ECC_DISABLED		0x00004000
#define CSTAT_CARD_ERASE_RESET		0x00002000
#define CSTAT_CURRENT_STATE			0x00001E00
#define	CSTAT_READY_FOR_DATA		0x00000100
#define	CSTAT_APP_CMD				0x00000020
#define CSTAT_AKE_SEQ_ERROR			0x00000008


//----------------------------------------------------------------------------------
//
//					OPERATING CONDITIONS (OCR) REGISTER BITS
//
//----------------------------------------------------------------------------------

#define OCR_POWER_UP_BUSY			0x80000000
#define	OCR_35_36_V					(1 << 23)
#define OCR_34_35_V					(1 << 22)
#define OCR_33_34_V					(1 << 21)
#define OCR_32_33_V					(1 << 20)
#define OCR_31_32_V					(1 << 19)
#define OCR_30_31_V					(1 << 18)
#define OCR_29_30_V					(1 << 17)
#define OCR_28_29_V					(1 << 16)
#define OCR_27_28_V					(1 << 15)
#define OCR_26_27_V					(1 << 14)
#define OCR_25_26_V					(1 << 13)
#define OCR_24_25_V					(1 << 12)
#define OCR_23_24_V					(1 << 11)
#define OCR_22_23_V					(1 << 10)
#define OCR_21_22_V					(1 << 9)
#define OCR_20_21_V					(1 << 8)
#define OCR_19_20_V					(1 << 7)
#define OCR_18_19_V					(1 << 6)
#define OCR_17_18_V					(1 << 5)
#define OCR_16_17_V					(1 << 4)


//----------------------------------------------------------------------------------
//
//					LIST OF SUPPORTED CAPABLILITIES
//
//----------------------------------------------------------------------------------

typedef enum {
	SET_OPEN_DRAIN			= 1,
	SET_4BIT_MODE			= 2,
	SET_CLOCK_RATE			= 3,
	GET_SUPPORTED_OCR_MMC	= 4,
	GET_SUPPORTED_OCR_SD	= 5,
} PDD_IOCTL;


//----------------------------------------------------------------------------------
//
//					LIST OF MDD<==>PDD INTERFACE
//
//----------------------------------------------------------------------------------

BOOL   PDD_SDInitializeHardware();

BOOL   PDD_SDSendCommand(UINT8 cmd, UINT32 arg, RESP_TYPE resp, CMD_TYPE type, BOOL read);
UINT32 PDD_SDGetResponse(SD_RESPONSE whichResp);

VOID   PDD_SDSetPDDCapabilities(PDD_IOCTL whichAbility, UINT32 Ability);
UINT32 PDD_SDGetPDDCapabilities(PDD_IOCTL whichAbility);

BOOL   PDD_SDReceiveData(UINT8* pBuffer);
VOID   PDD_SDStallExecute(UINT32 waitMs);

#endif
