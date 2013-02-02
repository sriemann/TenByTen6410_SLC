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
//  File: sdpdd.c
//
//  This file implements SD PDD driver.
//
// 
#include <sdpdd.h>
#include <utilities.h>
#include <oal.h>

#include <omap5912_armio.h>
#include <omap5912_config.h>
#include <omap5912_mmcsd.h>
#include <omap5912_base_regs.h>

#define MPUIO1_IS_CARD_DETECTED		0x02
#define MPUIO2_WRITE_PROTECTABLE	0x04
#define MPUIO5_VDD_PER_TO_SDMMC		0x20

static BOOL   g_openDrainMode;

//------------------------------------------------------------------------------
//
//  SDBoot - PDD Layer
//
//------------------------------------------------------------------------------

static void DumpResponse() {
	OMAP5912_MMCSD_REGS  *pMMCSDRegs;
	pMMCSDRegs  = (OMAP5912_MMCSD_REGS*)  OALPAtoUA(OMAP5912_MMCSD_REGS_PA);

	OALMSG(1, (TEXT("SDBootPDD: Response: %x %x %x %x %x %x %x %x\r\n"),
		pMMCSDRegs->MMC_RSP0,pMMCSDRegs->MMC_RSP1,pMMCSDRegs->MMC_RSP2,pMMCSDRegs->MMC_RSP3,
		pMMCSDRegs->MMC_RSP4,pMMCSDRegs->MMC_RSP5,pMMCSDRegs->MMC_RSP6,pMMCSDRegs->MMC_RSP7));
}

BOOL PDD_SDInitializeHardware() {

	OMAP5912_CONFIG_REGS *pCONFRegs;
	OMAP5912_ARMIO_REGS *pMPUIORegs;
	OMAP5912_MMCSD_REGS *pMMCSDRegs;

	pCONFRegs  = (OMAP5912_CONFIG_REGS*) OALPAtoUA(OMAP5912_CONFIG_REGS_PA);
	pMPUIORegs = (OMAP5912_ARMIO_REGS*) OALPAtoUA(OMAP5912_ARMIO_REGS_PA);
	pMMCSDRegs = (OMAP5912_MMCSD_REGS*) OALPAtoUA(OMAP5912_MMCSD_REGS_PA);

	// Enable MMC/SD controller clock
	SETREG32(&pCONFRegs->MOD_CONF_CTRL_0, CONF_MOD_MMCSD_CLK_REQ);

	// Issue soft reset
	SETREG32(&pMMCSDRegs->MMC_SYSC, MMC_SYSC_SRST);
	OALStall(10000);

	OALMSG(1, (TEXT("SDBootPDD: %s: OMAP5912 MMCSD revision %x\r\n"),TEXT(__FUNCTION__),pMMCSDRegs->MMC_REV));
	// MMCSD1 Configuration
	OUTREG16(&pMMCSDRegs->MMC_CON,  MMC_CON_POWER_UP | 480); // 48MHz/100KHz identification phase
	OUTREG16(&pMMCSDRegs->MMC_IE,   0);				//No interrupts -- in the bootloader!
	OUTREG16(&pMMCSDRegs->MMC_CTO,  0xFF);			//Max timeout
	OUTREG16(&pMMCSDRegs->MMC_DTO,  0xFFFF);			//Max timeout
	OUTREG16(&pMMCSDRegs->MMC_BLEN, BLOCK_LEN - 1);	//512 byte blocks supported by all SD/MMC cards
	OUTREG16(&pMMCSDRegs->MMC_NBLK, 0);
	OUTREG16(&pMMCSDRegs->MMC_SDIO, MMC_SDIO_DPE);	// enable data timeout scale
	OUTREG16(&pMMCSDRegs->MMC_BUF,  0);

	// Enable MPUIO
	pCONFRegs->RESET_CONTROL |= CONF_ARMIO_RESET_R;
	if( !(pCONFRegs->RESET_CONTROL & CONF_ARMIO_RESET_R) ) {
		OALMSG(1, (TEXT("SDBootPDD: %s: MPUIO is in reset\r\n"),TEXT(__FUNCTION__)));
		return FALSE;
	}

	// MPUIO Configuration
	// MPUIO1 ==> input , SD card detect signal
	// MPUIO2 ==> input , SD card protect signal
	// MPUIO5 ==> output, Enable VDD_PER power to SD card
	CLRREG16(&pMPUIORegs->IO_CONTROL, MPUIO5_VDD_PER_TO_SDMMC); // MPUIO5 as output
	SETREG16(&pMPUIORegs->IO_CONTROL, MPUIO1_IS_CARD_DETECTED | MPUIO2_WRITE_PROTECTABLE); //MPUIO1,2 as input

	// MPUIO5 drive low to supply power
	CLRREG16(&pMPUIORegs->OUTPUT_REG, MPUIO5_VDD_PER_TO_SDMMC);
	if( pMPUIORegs->INPUT_LATCH & MPUIO5_VDD_PER_TO_SDMMC ) {
		OALMSG(1, (TEXT("SDBootPDD: %s: MPUIO not staying low, SD POWER IS OFF\r\n"),TEXT(__FUNCTION__)));
		return FALSE;
	}

	// OMAP5912 -- Need to send INAB to enter idle-state after power-up
	if( !PDD_SDSendCommand(0x80,0,RESP_TYPE_NONE,CMD_TYPE_BC,0) ) {
		OALMSG(1, (TEXT("SDBootPDD: %s: Failure sending INAB to card\r\n"),TEXT(__FUNCTION__)));
		return FALSE;
	}

	return TRUE;
}

BOOL PDD_SDSendCommand(UINT8 cmd, UINT32 arg, RESP_TYPE resp, CMD_TYPE type, BOOL read) {
	// In the OMAP5912, commands are issued by
		// 1) Writing all arguments (card relative address, for example) to ARG registers
		// 2) Populating the command info -- type, response type, read/write, etc.
		// 3) Writing the appropriate info to MMC_CMD issues the command to hardware
	// Wait to retrieve your response by polling on MMC_STAT[EOC] (end of command)
	// Retrieve response by reading RSP0-RSP6 registers

	OMAP5912_MMCSD_REGS  *pMMCSDRegs;
	UINT16 tempMMC_CMD = 0;

	pMMCSDRegs  = (OMAP5912_MMCSD_REGS*)  OALPAtoUA(OMAP5912_MMCSD_REGS_PA);

	// If the command is greater than 64 it is invalid
	if( (cmd & 0xBF) != cmd ) {
		OALMSG(1, (TEXT("SDBootPDD: %s: Attempt to issue invalid SD/MMC command: %d\r\n"),TEXT(__FUNCTION__),cmd));
		return FALSE;
	}

	// If the response is out of bounds it is invalid
	if( !( ( (resp >= 0) && (resp <= 6) ) || (resp == 9) ) ) {
		OALMSG(1, (TEXT("SDBootPDD: %s: Invalid response specified: R%d\r\n"),TEXT(__FUNCTION__),resp));
		return FALSE;
	}

	// If the response type isn't bc, bcr, ac, atdc it is invalid
	if( (type < 0) || (type > 3) ) {
		OALMSG(1, (TEXT("SDBootPDD: %s: Invalid response type specified: %d\r\n"),TEXT(__FUNCTION__),resp));
		return FALSE;
	}

	// Clear all status bits
	pMMCSDRegs->MMC_STAT = pMMCSDRegs->MMC_STAT;

	// Disable CRC check if R4 type response
	if( resp == RESP_TYPE_R4) {
		pMMCSDRegs->MMC_SDIO |= 0x80;
	} else {
		pMMCSDRegs->MMC_SDIO &= ~0x80;
	}

	//Fill MMC_CMD[5:0]   -> INDX: passed in cmd argument
	//Fill MMC_CMD[6]     -> ODTO: controlled by MDD via PDD_SDSetPDDCapabilities(ENABLE_OPEN_DRAIN)
	//Fill MMC_CMD[7]     -> INAB: can be passed in cmd argument
	//Fill MMC_CMD[10:8]  -> RSP:  passed in resp argument
	//Fill MMC_CMD[11]    -> BUSY: passing a special '9' into resp argument
	//Fill MMC_CMD[13:12] -> TYPE: passed in resp_type argument
	//Fill MMC_CMD[14]    -> SHR:  not implemented
	//Fill MMC_CMD[15]	  -> DDIR: passed in read argument
	tempMMC_CMD |= ( (cmd) | (g_openDrainMode << 6) | (resp << 8) | (type << 12) | (read << 15));
		
	// Write arg registers
	pMMCSDRegs->MMC_ARG1 = (arg >> 0 ) & 0xFFFF; //TODO: LOWORD
	pMMCSDRegs->MMC_ARG2 = (arg >> 16) & 0xFFFF; //TODO: HIWORD

	// Write MMC_CMD to issue command
	pMMCSDRegs->MMC_CMD = tempMMC_CMD;

	// Wait on response
	while( !(pMMCSDRegs->MMC_STAT & MMC_STAT_EOC) ) {
		if( pMMCSDRegs->MMC_STAT & ( MMC_STAT_CERR | MMC_STAT_CCRC | MMC_STAT_CTO )) {
			OALMSG(1, (TEXT("SDBootPDD: %s: Error condition on command(cmd=%d, stat=%2x)\r\n"),
				TEXT(__FUNCTION__),(cmd & 0x3F),pMMCSDRegs->MMC_STAT));
			DumpResponse();
			return FALSE;
		}
	}

	return TRUE;		
}

BOOL PDD_SDReceiveData(UINT8* pBuffer) {

	OMAP5912_MMCSD_REGS  *pMMCSDRegs;
	UINT32 wCount;
	UINT16 wBuf;
	BOOL bIsAlign;
	UINT16* pwBuffer;

	pMMCSDRegs  = (OMAP5912_MMCSD_REGS*)  OALPAtoUA(OMAP5912_MMCSD_REGS_PA);

	wCount = 0;
	bIsAlign = (((UINT32)pBuffer&1) == 0);
	pwBuffer = (UINT16*)pBuffer;

	while( wCount < (BLOCK_LEN >> 1) ) {
		if( pMMCSDRegs->MMC_STAT & ( MMC_STAT_DCRC | MMC_STAT_DTO ) ) {
			OALMSG(1, (TEXT("SDBootPDD: %s: Error during data transfer(%x)\r\n"),TEXT(__FUNCTION__), pMMCSDRegs->MMC_STAT));
			return FALSE;
		}
		if( (pMMCSDRegs->MMC_STAT & MMC_STAT_AF) ) {
			pMMCSDRegs->MMC_STAT |= MMC_STAT_AF;
			wBuf = pMMCSDRegs->MMC_DATA;
			if( bIsAlign ) {
				*pwBuffer++ = wBuf;
			} else {
				*pBuffer++ = (UINT8)wBuf;
				*pBuffer++ = (UINT8)(wBuf>>8);
			}
			++wCount;
		}
	}

	return TRUE;
}

UINT32 PDD_SDGetResponse(SD_RESPONSE whichResp) {
	OMAP5912_MMCSD_REGS  *pMMCSDRegs;
	pMMCSDRegs  = (OMAP5912_MMCSD_REGS*)  OALPAtoUA(OMAP5912_MMCSD_REGS_PA);

	switch( whichResp ) {
		case RCA_REGISTER:	//16-bit response
			return (UINT32) pMMCSDRegs->MMC_RSP7;

		case CARD_STATUS_REGISTER:
		case OCR_REGISTER:  //32-bit responses
			return ( ( ( (UINT32) pMMCSDRegs->MMC_RSP7 ) << 16 ) | pMMCSDRegs->MMC_RSP6 );

		case SCR_REGISTER:  //64-bit response
		case CID_REGISTER:  //128-bit responses
		case CSD_REGISTER:
		default:
			return 0;      //Not implemented
	}
}

VOID PDD_SDSetPDDCapabilities(PDD_IOCTL whichAbility, UINT32 Ability) {
	OMAP5912_MMCSD_REGS  *pMMCSDRegs;
	pMMCSDRegs  = (OMAP5912_MMCSD_REGS*)  OALPAtoUA(OMAP5912_MMCSD_REGS_PA);

	switch( whichAbility ) {
		case SET_OPEN_DRAIN:
			g_openDrainMode = (Ability!=0);
			break;

		case SET_4BIT_MODE:
			if( Ability ) {
				pMMCSDRegs->MMC_CON |= MMC_CON_DW;
			} else {
				pMMCSDRegs->MMC_CON &= ~MMC_CON_DW;
			}
			break;

		case SET_CLOCK_RATE:
			pMMCSDRegs->MMC_CON &= ~0x3ff;
			pMMCSDRegs->MMC_CON |= (MMCSD_CLOCK_INPUT/Ability)&0x3ff;
			break;
	}
}

UINT32 PDD_SDGetPDDCapabilities(PDD_IOCTL whichAbility) {
	OMAP5912_MMCSD_REGS  *pMMCSDRegs;
	pMMCSDRegs  = (OMAP5912_MMCSD_REGS*)  OALPAtoUA(OMAP5912_MMCSD_REGS_PA);

	switch( whichAbility ) {
		case GET_SUPPORTED_OCR_MMC:
			return OMAP5912_OCR_SUPPORTED_VOLTAGES_MMC;

		case GET_SUPPORTED_OCR_SD:
			return OMAP5912_OCR_SUPPORTED_VOLTAGES_SD;

		default:
			return 0;
	}
}

VOID PDD_SDStallExecute(UINT32 waitMs) {
	OALStall(waitMs*1000);
}
