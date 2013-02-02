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
//  File: sdmdd.c
//
//  This file implements SD MDD driver.
//

#include <sdmdd.h>
#include <sdpdd.h>
#include <oal.h>

//----------------------------------------------------------------------------------
//
//							SOME USEFUL MACRO
//
//----------------------------------------------------------------------------------
#define SDCARD_CLOCK_RATE   25000000  // 25MHz
#define MMCCARD_CLOCK_RATE  20000000  // 20MHz
#define ID_CLOCK_RATE       100000    // 100KHz


#define HIGH_SHIFT(x) (((UINT32)(x))<<16)
#define DIMOF(x)	  (sizeof(x)/sizeof(x[0]))

// If !condition, the array size is 0 and a compile-time error occurs.
#define COMPILE_TIME_ASSERT(condition) { int compile_time_assert[(condition)]; compile_time_assert; }


//----------------------------------------------------------------------------------
//
//							LIST OF CARD TYPES
//
//----------------------------------------------------------------------------------

typedef enum {
	CARD_TYPE_NONE,
	CARD_TYPE_MMC,
	CARD_TYPE_SD
} CARD_TYPE;


//----------------------------------------------------------------------------------
//
//							COMMAND DETAIL STRUCTURE
//
//----------------------------------------------------------------------------------

typedef struct _MMCSD_COMMAND{
	UINT8		index;
	BOOL		isAppCmd;
	BOOL		isRead;
	CMD_TYPE	type;
	RESP_TYPE	resp;
} MMCSD_COMMAND;


//----------------------------------------------------------------------------------
//
//					LIST OF SUPPORTED COMMANDS 
//
//----------------------------------------------------------------------------------

enum {
	GO_IDLE_STATE = 0,		//CMD0
	SEND_OP_COND,			//CMD1
	ALL_SEND_CID,			//CMD2
	SET_RELATIVE_ADDR,		//CMD3 (MMC)
	SEND_RELATIVE_ADDR,		//CMD3 (SD)
							//...
	SELECT_DESELECT_CARD,	//CMD7
							//...
	SEND_CSD,				//CMD9
	SEND_CID,				//CMD10
							//...
	SEND_STATUS,			//CMD13
							//...
	SET_BLOCKLEN,			//CMD16
	READ_SINGLE_BLOCK,		//CMD17
							//...
	APP_CMD,				//CMD55
							//...
	SET_BUS_WIDTH,			//ACMD6
							//...
	SD_APP_OP_COND,			//ACMD41
							//...
	TOTAL_COMMANDS				
};


//----------------------------------------------------------------------------------
//
//							DETAILED COMMAND INFO
//				    * Order Must match enums in .h file *
//
//----------------------------------------------------------------------------------

const MMCSD_COMMAND cmdTable[] = {	

//---------------------------------------------------------------------------------------------
//		#,	ACMD:CMD,READ,		TYPE,				RESPONSE,			   DESCRIPTION
//---------------------------------------------------------------------------------------------

	{	0,	FALSE,	FALSE,		CMD_TYPE_BC,		RESP_TYPE_NONE		},	// GO_IDLE_STATE
	{	1,	FALSE,	FALSE,		CMD_TYPE_BCR,		RESP_TYPE_R3		},	// SEND_OP_COND
	{	2,	FALSE,	FALSE,		CMD_TYPE_BCR,		RESP_TYPE_R2		},	// ALL_SEND_CID
	{	3,	FALSE,	FALSE,		CMD_TYPE_AC,		RESP_TYPE_R1		},	// SET_RELATIVE_ADDR
	{	3,	FALSE,	FALSE,		CMD_TYPE_BCR,		RESP_TYPE_R6		},	// SEND_RELATIVE_ADDR
																	// ...
	{	7,	FALSE,	FALSE,		CMD_TYPE_AC,		RESP_TYPE_R1B		},	// SELECT_DESELECT_CARD
																	// ...
	{	9,	FALSE,	FALSE,		CMD_TYPE_AC,		RESP_TYPE_R2		},  // SEND_CSD
	{	10,	FALSE,	FALSE,		CMD_TYPE_AC,		RESP_TYPE_R2		},	// SEND_CID
																	// ...
	{	13,	FALSE,	FALSE,		CMD_TYPE_AC,		RESP_TYPE_R1		},	// SEND_STATUS
																// ...
	{	16,	FALSE,	FALSE,		CMD_TYPE_AC,		RESP_TYPE_R1		},	// SET_BLOCKLEN
	{	17,	FALSE,	TRUE,		CMD_TYPE_ADTC,		RESP_TYPE_R1		},	// READ_SINGLE_BLOCK
																	// ...
	{	55,	FALSE,	FALSE,		CMD_TYPE_AC,		RESP_TYPE_R1		},	// APP_CMD
																		// ...
	{	6,	TRUE,	FALSE,		CMD_TYPE_AC,		RESP_TYPE_R1		},	// SET_BUS_WIDTH
	{	41,	TRUE,	FALSE,		CMD_TYPE_BCR,		RESP_TYPE_R3		}	// SD_APP_OP_COND
};


//----------------------------------------------------------------------------------
//
//							HELP VARIABLE AND FUNCTION
//
//----------------------------------------------------------------------------------

static UINT16	g_cardRCA;					// Relative Address of card

static BOOL SDSendCommand(UINT8 idx, UINT32 arg) {

	// sanity checking on parameters
	if(idx >= TOTAL_COMMANDS) {
		OALMSG(1, (TEXT("SDBootMDD: %s: command index is invalid\r\n"),TEXT(__FUNCTION__)));
		return FALSE;
	}

	if( cmdTable[idx].isAppCmd && !SDSendCommand( APP_CMD, HIGH_SHIFT(g_cardRCA) ) ) {
		return FALSE;
	} 

	return PDD_SDSendCommand(
		cmdTable[idx].index,
		arg,
		cmdTable[idx].resp,
		cmdTable[idx].type,
		cmdTable[idx].isRead);
}


//----------------------------------------------------------------------------------
//
//					DETAILED FUNCTION IMPLEMENT
//
//----------------------------------------------------------------------------------

BOOL SDInitializeHardware() {
	CARD_TYPE cardType = CARD_TYPE_NONE;
	int timeOut;

	COMPILE_TIME_ASSERT( DIMOF(cmdTable) == (TOTAL_COMMANDS) );
	// To check to see why your compile time assert failed...

	g_cardRCA = 0;

	// -------- INITIALIZE HARDWARE -------
	if( !PDD_SDInitializeHardware() ) {
		return FALSE;
	}

	// -------- DETECT CARD TYPE -------
	if( !SDSendCommand(GO_IDLE_STATE,0) ) {
		OALMSG(1, (TEXT("SDBootMDD: %s: GO_IDLE command returned error\r\n"),TEXT(__FUNCTION__)));
		return FALSE;
	}

	if( SDSendCommand(SD_APP_OP_COND,0) ) {
		cardType = CARD_TYPE_SD;
	} else if( SDSendCommand(SEND_OP_COND,0) ) {
		cardType = CARD_TYPE_MMC;
	}

	// If no card exists, exit. SDIO cards are not supported by this driver.
	if( cardType == CARD_TYPE_NONE ) {
		OALMSG(1, (TEXT("SDBootMDD: %s: No valid SD/MMC cards detected\r\n"),TEXT(__FUNCTION__)));
		return FALSE;
	}

	// ------ INITIALIZE CARD SEQUENCE ------- //

	// ---- 1. Reset to consistent state with CMD0 ---- //
	if( !SDSendCommand(GO_IDLE_STATE,0) ) {
		OALMSG(1, (TEXT("SDBootMDD: %s: GO_IDLE command returned error\r\n"),TEXT(__FUNCTION__)));
		return FALSE;
	}

	if( cardType == CARD_TYPE_MMC ) PDD_SDSetPDDCapabilities(SET_OPEN_DRAIN, TRUE);

	// ---- 2. Check voltages using CMD1/ACMD41 ---- //
	for( timeOut = 0; timeOut < 100; ++timeOut ) {
		if( cardType == CARD_TYPE_SD ) {
			if( !SDSendCommand( SD_APP_OP_COND, PDD_SDGetPDDCapabilities(GET_SUPPORTED_OCR_SD) )) {
				OALMSG(1, (TEXT("SDBootMDD: %s: Error issuing SD_APP_OP_COND command\r\n"),TEXT(__FUNCTION__)));
				return FALSE;
			}
		} else {
			if( !SDSendCommand( SEND_OP_COND, PDD_SDGetPDDCapabilities(GET_SUPPORTED_OCR_MMC) )) {
				OALMSG(1, (TEXT("SDBootMDD: %s: Error issuing SEND_OP_COND command\r\n"),TEXT(__FUNCTION__)));
				return FALSE;
			}
		}
		if( PDD_SDGetResponse(OCR_REGISTER) & OCR_POWER_UP_BUSY) { // card finish power init?
			break;
		}
		PDD_SDStallExecute(100);
	}
	if( timeOut >= 100 ) {
		OALMSG(1, (TEXT("SDBootMDD: %s: Failed to validate card OCR voltage range\r\n"),TEXT(__FUNCTION__)));
		return FALSE;
	}

	// ---- 3. Have first card identify itself (multiple cards unsupported) ---- //
	for( timeOut = 0; timeOut < 30; ++timeOut ) {
		if( SDSendCommand(ALL_SEND_CID,0) ) {
			break;
		}
		PDD_SDStallExecute(100);
	}
	if( timeOut >= 30 ) {
		OALMSG(1, (TEXT("SDBootMDD: %s: Failed to receive valid CID response from card\r\n"),TEXT(__FUNCTION__)));
		return FALSE;
	}

	// ---- 4. Give the card an address / get an address ---- //
	if( cardType == CARD_TYPE_SD ) {
		if( !SDSendCommand( SEND_RELATIVE_ADDR,0) ) {
			OALMSG(1, (TEXT("SDBootMDD: %s: Error receiving card relative address\r\n"),TEXT(__FUNCTION__)));
			return FALSE;
		}
		g_cardRCA = (UINT16) PDD_SDGetResponse(RCA_REGISTER);
	} else {
		if( !SDSendCommand( SET_RELATIVE_ADDR, HIGH_SHIFT(1) ) ) {
			OALMSG(1, (TEXT("SDBootMDD: %s: Error setting card relative address\r\n"),TEXT(__FUNCTION__)));
			return FALSE;
		}
		g_cardRCA = 1;
	}
	// To see the published RCA...
	OALMSG(OAL_INFO, (TEXT("SDBootMDD: Card address is %x\r\n"),g_cardRCA));

	if( cardType == CARD_TYPE_MMC ) PDD_SDSetPDDCapabilities(SET_OPEN_DRAIN, FALSE);

	// ---- 5. Select card for future commands using CMD7 ---- //
	if( !SDSendCommand(SELECT_DESELECT_CARD, HIGH_SHIFT(g_cardRCA)) ) {
		OALMSG(1, (TEXT("SDBootMDD: %s: Error selecting card for data transfer\r\n"),TEXT(__FUNCTION__)));
		return FALSE;
	}

	// ---- 6. Select clock rate and bus width ---- //
	if( cardType == CARD_TYPE_SD ) {
		if( SDSendCommand(SET_BUS_WIDTH, 0x2) ) {
			OALMSG(OAL_INFO, (TEXT("SDBootMDD: 4-bit data bus selected\r\n")));
			PDD_SDSetPDDCapabilities(SET_4BIT_MODE, TRUE);
		} else {
			OALMSG(OAL_INFO, (TEXT("SDBootMDD: 1-bit data bus selected\r\n")));
		}
		PDD_SDSetPDDCapabilities(SET_CLOCK_RATE, SDCARD_CLOCK_RATE);
	} else {
		OALMSG(OAL_INFO, (TEXT("SDBootMDD: 1-bit data bus selected\r\n")));
		PDD_SDSetPDDCapabilities(SET_CLOCK_RATE, MMCCARD_CLOCK_RATE);
	}

	// ---- 7. Prep card for reading by setting default block length ---- //
	if( !SDSendCommand(SET_BLOCKLEN, BLOCK_LEN) ) {
		OALMSG(1, (TEXT("SDBootMDD: %s: Error setting default block transfer length\r\n"),TEXT(__FUNCTION__)));
		return FALSE;
	}

	return TRUE;
}

BOOL SDReadDataBlock(PVOID pBuffer, DWORD dwLength, DWORD dwAddress){
	// cache for speed up
	static DWORD CacheAddress = -1;
	static BYTE  CacheBuffer[BLOCK_LEN];

	while(dwLength){
		// 1. check cache hit
		if(dwAddress>=CacheAddress && dwAddress<CacheAddress+BLOCK_LEN){
			DWORD dwOffset  = dwAddress-CacheAddress;
			DWORD dwCopyLen = BLOCK_LEN-dwOffset;
			if(dwLength > dwCopyLen){
				memcpy(pBuffer, CacheBuffer+dwOffset, dwCopyLen);
				(PBYTE)pBuffer   += dwCopyLen;
				dwAddress        += dwCopyLen;
				dwLength         -= dwCopyLen;
			}
			else{
				memcpy(pBuffer, CacheBuffer+dwOffset, dwLength);
				break;
			}
		}

		// 2.issue command to read single block
		// note: the address must be block alignment.
		if( !SDSendCommand(READ_SINGLE_BLOCK, dwAddress&BLOCK_MASK) ) {
			OALMSG(1, (TEXT("SDBootMDD: %s: READ_SINGLE_BLOCK command returned error\r\n"),TEXT(__FUNCTION__)));
			return FALSE;
		}

		// 3.harvest single block data
		if(dwLength<=BLOCK_LEN || dwAddress%BLOCK_LEN){
			// fill cache in following condition:
			// 1.read buffer less than one block length
			// 2.address doesn't align on block length
			if(PDD_SDReceiveData(CacheBuffer)) {
				CacheAddress = dwAddress&BLOCK_MASK; // update cache start address
				// no other action here, let next loop copy cache data
			}
			else{
				return FALSE;
			}
		}
		else{
			// directly read if space is enough and address is alignment
			if(PDD_SDReceiveData((UINT8*)pBuffer)){
				(PBYTE)pBuffer   += BLOCK_LEN;
				dwAddress        += BLOCK_LEN;
				dwLength         -= BLOCK_LEN;
			}
			else{
				return FALSE;
			}
		}
	}

	return TRUE;
}
