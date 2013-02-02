//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
//
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//

#include <windows.h>
#include <bsp.h>
#include <ethdbg.h>
#include <fmd.h>
#include <pcireg.h>
#include "loader.h"
#include "usb.h"
//#include "fmd_lb.h"
//#include "fmd_sb.h"

#include "s3c6410_ldi.h"
#include "s3c6410_display_con.h"
//#include "InitialImage_rgb16_320x240.h"


// For USB Download function
extern BOOL InitializeUSB();
extern void InitializeInterrupt();
extern BOOL UbootReadData(DWORD cbData, LPBYTE pbData);
extern void OTGDEV_SetSoftDisconnect();
// To change Voltage for higher clock.
extern void LTC3714_Init();
extern void LTC3714_VoltageSet(UINT32,UINT32,UINT32);

extern int HSMMCInit(void);
extern void ChooseImageFromSD();
extern bool SDReadData(DWORD cbData, LPBYTE pbData);

// For Ethernet Download function.
char *inet_ntoa(DWORD dwIP);
DWORD inet_addr( char *pszDottedD );
BOOL EbootInitEtherTransport(EDBG_ADDR *pEdbgAddr, LPDWORD pdwSubnetMask,
                                    BOOL *pfJumpImg,
                                    DWORD *pdwDHCPLeaseTime,
                                    UCHAR VersionMajor, UCHAR VersionMinor,
                                    char *szPlatformString, char *szDeviceName,
                                    UCHAR CPUId, DWORD dwBootFlags);
BOOL EbootEtherReadData(DWORD cbData, LPBYTE pbData);
EDBG_OS_CONFIG_DATA *EbootWaitForHostConnect (EDBG_ADDR *pDevAddr, EDBG_ADDR *pHostAddr);
void SaveEthernetAddress();

// Eboot Internal static function
static void InitializeDisplay(void);
static void SpinForever(void);
static USHORT GetIPString(char *);

// Globals
//
DWORD               g_ImageType;
MultiBINInfo        g_BINRegionInfo;
PBOOT_CFG           g_pBootCfg;
UCHAR               g_TOC[LB_PAGE_SIZE];
const PTOC          g_pTOC = (PTOC)&g_TOC;
DWORD               g_dwTocEntry;
BOOL                g_bBootMediaExist = FALSE;
BOOL                g_bDownloadImage  = TRUE;
BOOL                g_bWaitForConnect = TRUE;
BOOL                g_bUSBDownload = FALSE;
BOOL                *g_bCleanBootFlag;
BOOLEAN			g_bSDDownload = FALSE;
BOOLEAN         g_bAutoDownload = FALSE;

//for KITL Configuration Of Args
OAL_KITL_ARGS       *g_KITLConfig;

//for Device ID Of Args
UCHAR               *g_DevID;


EDBG_ADDR         g_DeviceAddr; // NOTE: global used so it remains in scope throughout download process
                        // since eboot library code keeps a global pointer to the variable provided.

DWORD            wNUM_BLOCKS;

void main(void)
{
    //GPIOTest_Function();
    OTGDEV_SetSoftDisconnect();
    BootloaderMain();

    // Should never get here.
    //
    SpinForever();
}

static USHORT GetIPString(char *szDottedD)
{
    USHORT InChar = 0;
    USHORT cwNumChars = 0;

    while(!((InChar == 0x0d) || (InChar == 0x0a)))
    {
        InChar = OEMReadDebugByte();
        if (InChar != OEM_DEBUG_COM_ERROR && InChar != OEM_DEBUG_READ_NODATA)
        {
            // If it's a number or a period, add it to the string.
            //
            if (InChar == '.' || (InChar >= '0' && InChar <= '9'))
            {
                if (cwNumChars < 16)        // IP string cannot over 15.  xxx.xxx.xxx.xxx
                {
                    szDottedD[cwNumChars++] = (char)InChar;
                    OEMWriteDebugByte((BYTE)InChar);
                }
            }
            // If it's a backspace, back up.
            //
            else if (InChar == 8)
            {
                if (cwNumChars > 0)
                {
                    cwNumChars--;
                    OEMWriteDebugByte((BYTE)InChar);
                }
            }
        }
    }

    return cwNumChars;

}

/*
    @func   void | SetIP | Accepts IP address from user input.
    @rdesc  N/A.
    @comm
    @xref
*/

static void SetIP(PBOOT_CFG pBootCfg)
{
    CHAR   szDottedD[16];   // The string used to collect the dotted decimal IP address.
    USHORT cwNumChars = 0;
    //USHORT InChar = 0;

    EdbgOutputDebugString("\r\nEnter new IP address: ");

    cwNumChars = GetIPString(szDottedD);

    // If it's a carriage return with an empty string, don't change anything.
    //
    if (cwNumChars && cwNumChars < 16)
    {
        szDottedD[cwNumChars] = '\0';
        pBootCfg->EdbgAddr.dwIP = inet_addr(szDottedD);
    }
    else
    {
        EdbgOutputDebugString("\r\nIncorrect String");
    }
}

/*
    @func   void | SetMask | Accepts subnet mask from user input.
    @rdesc  N/A.
    @comm
    @xref
*/
static void SetMask(PBOOT_CFG pBootCfg)
{
    CHAR szDottedD[16]; // The string used to collect the dotted masks.
    USHORT cwNumChars = 0;
    //USHORT InChar = 0;

    EdbgOutputDebugString("\r\nEnter new subnet mask: ");

    cwNumChars = GetIPString(szDottedD);

    // If it's a carriage return with an empty string, don't change anything.
    //
    if (cwNumChars && cwNumChars < 16)
    {
        szDottedD[cwNumChars] = '\0';
        pBootCfg->SubnetMask = inet_addr(szDottedD);
    }
    else
    {
        EdbgOutputDebugString("\r\nIncorrect String");
    }
    
}


/*
    @func   void | SetDelay | Accepts an autoboot delay value from user input.
    @rdesc  N/A.
    @comm
    @xref
*/
static void SetDelay(PBOOT_CFG pBootCfg)
{
    CHAR szCount[16];
    USHORT cwNumChars = 0;
    USHORT InChar = 0;

    EdbgOutputDebugString("\r\nEnter maximum number of seconds to delay [1-255]: ");

    while(!((InChar == 0x0d) || (InChar == 0x0a)))
    {
        InChar = OEMReadDebugByte();
        if (InChar != OEM_DEBUG_COM_ERROR && InChar != OEM_DEBUG_READ_NODATA)
        {
            // If it's a number or a period, add it to the string.
            //
            if ((InChar >= '0' && InChar <= '9'))
            {
                if (cwNumChars < 16)
                {
                    szCount[cwNumChars++] = (char)InChar;
                    OEMWriteDebugByte((BYTE)InChar);
                }
            }
            // If it's a backspace, back up.
            //
            else if (InChar == 8)
            {
                if (cwNumChars > 0)
                {
                    cwNumChars--;
                    OEMWriteDebugByte((BYTE)InChar);
                }
            }
        }
    }

    // If it's a carriage return with an empty string, don't change anything.
    //
    if (cwNumChars)
    {
        szCount[cwNumChars] = '\0';
        pBootCfg->BootDelay = atoi(szCount);
        if (pBootCfg->BootDelay > 255)
        {
            pBootCfg->BootDelay = 255;
        }
        else if (pBootCfg->BootDelay < 1)
        {
            pBootCfg->BootDelay = 1;
        }
    }
}


static ULONG mystrtoul(PUCHAR pStr, UCHAR nBase)
{
    UCHAR nPos=0;
    BYTE c;
    ULONG nVal = 0;
    UCHAR nCnt=0;
    ULONG n=0;

    // fulllibc doesn't implement isctype or iswctype, which are needed by
    // strtoul, rather than including coredll code, here's our own simple strtoul.

    if (pStr == NULL)
        return(0);

    for (nPos=0 ; nPos < strlen(pStr) ; nPos++)
    {
        c = tolower(*(pStr + strlen(pStr) - 1 - nPos));
        if (c >= '0' && c <= '9')
            c -= '0';
        else if (c >= 'a' && c <= 'f')
        {
            c -= 'a';
            c  = (0xa + c);
        }

        for (nCnt = 0, n = 1 ; nCnt < nPos ; nCnt++)
        {
            n *= nBase;
        }
        nVal += (n * c);
    }

    return(nVal);
}


static void CvtMAC(USHORT MacAddr[3], char *pszDottedD )
{
    DWORD cBytes;
    char *pszLastNum;
    int atoi (const char *s);
    int i=0;
    BYTE *p = (BYTE *)MacAddr;

    // Replace the dots with NULL terminators
    pszLastNum = pszDottedD;
    for(cBytes = 0 ; cBytes < 6 ; cBytes++)
    {
        while(*pszDottedD != '.' && *pszDottedD != '\0')
        {
            pszDottedD++;
        }
        if (pszDottedD == '\0' && cBytes != 5)
        {
            // zero out the rest of MAC address
            while(i++ < 6)
            {
                *p++ = 0;
            }
            break;
        }
        *pszDottedD = '\0';
        *p++ = (BYTE)(mystrtoul(pszLastNum, 16) & 0xFF);
        i++;
        pszLastNum = ++pszDottedD;
    }
}


static void SetCS8900MACAddress(PBOOT_CFG pBootCfg)
{
    CHAR szDottedD[24];
    USHORT cwNumChars = 0;
    USHORT InChar = 0;

    memset(szDottedD, '0', 24);

    EdbgOutputDebugString ( "\r\nEnter new MAC address in hexadecimal (hh.hh.hh.hh.hh.hh): ");

    while(!((InChar == 0x0d) || (InChar == 0x0a)))
    {
        InChar = OEMReadDebugByte();
        InChar = tolower(InChar);
        if (InChar != OEM_DEBUG_COM_ERROR && InChar != OEM_DEBUG_READ_NODATA)
        {
            // If it's a hex number or a period, add it to the string.
            //
            if (InChar == '.' || (InChar >= '0' && InChar <= '9') || (InChar >= 'a' && InChar <= 'f'))
            {
                if (cwNumChars < 17)
                {
                    szDottedD[cwNumChars++] = (char)InChar;
                    OEMWriteDebugByte((BYTE)InChar);
                }
            }
            else if (InChar == 8)       // If it's a backspace, back up.
            {
                if (cwNumChars > 0)
                {
                    cwNumChars--;
                    OEMWriteDebugByte((BYTE)InChar);
                }
            }
        }
    }

    EdbgOutputDebugString ( "\r\n");

    // If it's a carriage return with an empty string, don't change anything.
    //
    if (cwNumChars)
    {
        szDottedD[cwNumChars] = '\0';
        CvtMAC(pBootCfg->EdbgAddr.wMAC, szDottedD);

        EdbgOutputDebugString("INFO: MAC address set to: %x:%x:%x:%x:%x:%x\r\n",
                  pBootCfg->EdbgAddr.wMAC[0] & 0x00FF, pBootCfg->EdbgAddr.wMAC[0] >> 8,
                  pBootCfg->EdbgAddr.wMAC[1] & 0x00FF, pBootCfg->EdbgAddr.wMAC[1] >> 8,
                  pBootCfg->EdbgAddr.wMAC[2] & 0x00FF, pBootCfg->EdbgAddr.wMAC[2] >> 8);
    }
    else
    {
        EdbgOutputDebugString("WARNING: SetCS8900MACAddress: Invalid MAC address.\r\n");
    }
}

UINT32 SetBlockPage(void)
{
    CHAR szCount[16];
    USHORT cwNumChars = 0;
    USHORT InChar = 0;
    UINT32 block=0, page=0;
    UINT32 i;
 
    EdbgOutputDebugString("\r\nEnter Block # : ");
 
    while(!((InChar == 0x0d) || (InChar == 0x0a)))
    {
        InChar = OEMReadDebugByte();
        if (InChar != OEM_DEBUG_COM_ERROR && InChar != OEM_DEBUG_READ_NODATA) 
        {
            // If it's a number or a period, add it to the string.
            //
            if ((InChar >= '0' && InChar <= '9')) 
            {
                if (cwNumChars < 16) 
                {
                    szCount[cwNumChars++] = (char)InChar;
                    OEMWriteDebugByte((BYTE)InChar);
                }
            }
            // If it's a backspace, back up.
            //
            else if (InChar == 8) 
            {
                if (cwNumChars > 0) 
                {
                    cwNumChars--;
                    OEMWriteDebugByte((BYTE)InChar);
                }
            }
        }
    }
 
    // If it's a carriage return with an empty string, don't change anything.
    //
    if (cwNumChars) 
    {
        szCount[cwNumChars] = '\0';
        block = atoi(szCount);
    }
 
    EdbgOutputDebugString("\r\nEnter Page # : ");
    InChar = 0;
    for (i=0; i<16; i++) szCount[i] = '0';
    
    while(!((InChar == 0x0d) || (InChar == 0x0a)))
    {
        InChar = OEMReadDebugByte();
        if (InChar != OEM_DEBUG_COM_ERROR && InChar != OEM_DEBUG_READ_NODATA) 
        {
            // If it's a number or a period, add it to the string.
            //
            if ((InChar >= '0' && InChar <= '9')) 
            {
                if (cwNumChars < 16) 
                {
                    szCount[cwNumChars++] = (char)InChar;
                    OEMWriteDebugByte((BYTE)InChar);
                }
            }
            // If it's a backspace, back up.
            //
            else if (InChar == 8) 
            {
                if (cwNumChars > 0) 
                {
                    cwNumChars--;
                    OEMWriteDebugByte((BYTE)InChar);
                }
            }
        }
    }
 
    // If it's a carriage return with an empty string, don't change anything.
    //
    if (cwNumChars) 
    {
        szCount[cwNumChars] = '\0';
        page = atoi(szCount);
    }
 
    if (IS_LB) return ((block<<6)|page);
    else       return ((block<<5)|page);
}
 
void PrintPageData(DWORD nMData, DWORD nSData, UINT8* pMBuf, UINT8* pSBuf)
{
    DWORD i;
 
    OALMSG(TRUE, (TEXT("=========================================================")));
    for (i=0;i<nMData;i++)
    {
        if ((i%16)==0)
            OALMSG(TRUE, (TEXT("\r\n 0x%03x |"), i));
        OALMSG(TRUE, (TEXT(" %02x"), pMBuf[i]));
        if ((i%512)==511)
            OALMSG(TRUE, (TEXT("\r\n ------------------------------------------------------- ")));
    }
    for (i=0;i<nSData;i++)
    {
        if ((i%16)==0)
            OALMSG(TRUE, (TEXT("\r\n 0x%03x |"), i));
        OALMSG(TRUE, (TEXT(" %02x"), pSBuf[i]));
    }
    OALMSG(TRUE, (TEXT("\r\n=========================================================")));
}

/*
    @func   BOOL | MainMenu | Manages the Samsung bootloader main menu.
    @rdesc  TRUE == Success and FALSE == Failure.
    @comm
    @xref
*/

static BOOL MainMenu(PBOOT_CFG pBootCfg)
{
    BYTE KeySelect = 0;
    BOOL bConfigChanged = FALSE;
    BOOLEAN bDownload = TRUE;

    while(TRUE)
    {
        KeySelect = 0;

        EdbgOutputDebugString ( "\r\nEthernet Boot Loader Configuration:\r\n\r\n");
        EdbgOutputDebugString ( "0) IP address: %s\r\n",inet_ntoa(pBootCfg->EdbgAddr.dwIP));
        EdbgOutputDebugString ( "1) Subnet mask: %s\r\n", inet_ntoa(pBootCfg->SubnetMask));
        EdbgOutputDebugString ( "2) DHCP: %s\r\n", (pBootCfg->ConfigFlags & CONFIG_FLAGS_DHCP)?"Enabled":"Disabled");
        EdbgOutputDebugString ( "3) Boot delay: %d seconds\r\n", pBootCfg->BootDelay);
        EdbgOutputDebugString ( "4) Reset to factory default configuration\r\n");
        EdbgOutputDebugString ( "5) Startup image: %s\r\n", (g_pBootCfg->ConfigFlags & BOOT_TYPE_DIRECT) ? "LAUNCH EXISTING" : "DOWNLOAD NEW");
        EdbgOutputDebugString ( "6) Program disk image into SmartMedia card: %s\r\n", (pBootCfg->ConfigFlags & TARGET_TYPE_NAND)?"Enabled":"Disabled");
        EdbgOutputDebugString ( "7) Program CS8900 MAC address (%B:%B:%B:%B:%B:%B)\r\n",
                               g_pBootCfg->EdbgAddr.wMAC[0] & 0x00FF, g_pBootCfg->EdbgAddr.wMAC[0] >> 8,
                               g_pBootCfg->EdbgAddr.wMAC[1] & 0x00FF, g_pBootCfg->EdbgAddr.wMAC[1] >> 8,
                               g_pBootCfg->EdbgAddr.wMAC[2] & 0x00FF, g_pBootCfg->EdbgAddr.wMAC[2] >> 8);
        EdbgOutputDebugString ( "8) KITL Configuration: %s\r\n", (pBootCfg->ConfigFlags & CONFIG_FLAGS_KITL) ? "ENABLED" : "DISABLED");
        EdbgOutputDebugString ( "9) Format Boot Media for BinFS\r\n");

        // N.B: we need this option here since BinFS is really a RAM image, where you "format" the media
        // with an MBR. There is no way to parse the image to say it's ment to be BinFS enabled.
        EdbgOutputDebugString ( "A) Erase All Blocks\r\n");
        EdbgOutputDebugString ( "B) Mark Bad Block at Reserved Block \r\n");
        EdbgOutputDebugString ( "C) Clean Boot Option: %s\r\n", (pBootCfg->ConfigFlags & BOOT_OPTION_CLEAN)?"TRUE":"FALSE");
        EdbgOutputDebugString ( "D) Download image now\r\n");
        EdbgOutputDebugString ( "E) Erase Reserved Block \r\n");
        EdbgOutputDebugString ( "F) Low-level format the Smart Media card\r\n");
        EdbgOutputDebugString ( "L) LAUNCH existing Boot Media image\r\n");
        EdbgOutputDebugString ( "R) Read Configuration \r\n");
        EdbgOutputDebugString ( "S) DOWNLOAD image now(SDMMC card)\r\n");
        EdbgOutputDebugString ( "W) Write Configuration Right Now\r\n");
        EdbgOutputDebugString ( "\r\nEnter your selection: ");

        while (! ( ( (KeySelect >= '0') && (KeySelect <= '9') ) ||
                   ( (KeySelect == 'A') || (KeySelect == 'a') ) ||
                   ( (KeySelect == 'B') || (KeySelect == 'b') ) ||
                   ( (KeySelect == 'C') || (KeySelect == 'c') ) ||
                   ( (KeySelect == 'D') || (KeySelect == 'd') ) ||
                   ( (KeySelect == 'E') || (KeySelect == 'e') ) ||
                   ( (KeySelect == 'F') || (KeySelect == 'f') ) ||
                   ( (KeySelect == 'L') || (KeySelect == 'l') ) ||
                   ( (KeySelect == 'N') || (KeySelect == 'n') ) ||                   
                   ( (KeySelect == 'R') || (KeySelect == 'r') ) ||
                   ( (KeySelect == 'S') || (KeySelect == 's') ) ||
                   ( (KeySelect == 'W') || (KeySelect == 'w') ) ))
        {
            KeySelect = OEMReadDebugByte();
        }

        EdbgOutputDebugString ( "%c\r\n", KeySelect);

        switch(KeySelect)
        {
        case '0':           // Change IP address.
            SetIP(pBootCfg);
            pBootCfg->ConfigFlags &= ~CONFIG_FLAGS_DHCP;   // clear DHCP flag
            bConfigChanged = TRUE;
            break;
        case '1':           // Change subnet mask.
            SetMask(pBootCfg);
            bConfigChanged = TRUE;
            break;
        case '2':           // Toggle static/DHCP mode.
            pBootCfg->ConfigFlags = (pBootCfg->ConfigFlags ^ CONFIG_FLAGS_DHCP);
            bConfigChanged = TRUE;
            break;
        case '3':           // Change autoboot delay.
            SetDelay(pBootCfg);
            bConfigChanged = TRUE;
            break;
        case '4':           // Reset the bootloader configuration to defaults.
            OALMSG(TRUE, (TEXT("Resetting default TOC...\r\n")));
            TOC_Init(DEFAULT_IMAGE_DESCRIPTOR, (IMAGE_TYPE_RAMIMAGE|IMAGE_TYPE_BINFS), 0, 0, 0);
            if ( !TOC_Write() ) {
                OALMSG(OAL_WARN, (TEXT("TOC_Write Failed!\r\n")));
            }
            OALMSG(TRUE, (TEXT("...TOC complete\r\n")));
            break;
        case '5':           // Toggle download/launch status.
            pBootCfg->ConfigFlags = (pBootCfg->ConfigFlags ^ BOOT_TYPE_DIRECT);
            bConfigChanged = TRUE;
            break;
        case '6':           // Toggle image storage to Smart Media.
            pBootCfg->ConfigFlags = (pBootCfg->ConfigFlags ^ TARGET_TYPE_NAND);
            bConfigChanged = TRUE;
            break;
        case '7':           // Configure Crystal CS8900 MAC address.
            SetCS8900MACAddress(pBootCfg);
            bConfigChanged = TRUE;
            break;
        case '8':           // Toggle KD
            pBootCfg->ConfigFlags = (pBootCfg->ConfigFlags ^ CONFIG_FLAGS_KITL);
            g_bWaitForConnect = (pBootCfg->ConfigFlags & CONFIG_FLAGS_KITL) ? TRUE : FALSE;
            bConfigChanged = TRUE;
            continue;
            break;
        case '9':
            // format the boot media for BinFS
            // N.B: this does not destroy our OEM reserved sections (TOC, bootloaders, etc)
            if ( !g_bBootMediaExist ) {
                OALMSG(OAL_ERROR, (TEXT("ERROR: BootMonitor: boot media does not exist.\r\n")));
                continue;
            }
            // N.B: format offset by # of reserved blocks,
            // decrease the ttl # blocks available by that amount.
            if ( !BP_LowLevelFormat( IMAGE_START_BLOCK,
                                     wNUM_BLOCKS - IMAGE_START_BLOCK,
                                     FORMAT_SKIP_BLOCK_CHECK) )
            {
                OALMSG(OAL_ERROR, (TEXT("ERROR: BootMonitor: Low-level boot media format failed.\r\n")));
                continue;
            }
            break;
        case 'A':
        case 'a':
            {
                DWORD i;

                OALMSG(TRUE, (TEXT("All block(%d) Erase...\r\n"), wNUM_BLOCKS));
                for (i = 0; i < wNUM_BLOCKS; i++) {
                    FMD_EraseBlock(i);
                }
            }
            break;
        case 'B':
        case 'b':
            // low-level format
            // N.B: this erases images, BinFs, FATFS, user data, etc.
            // However, we don't format Bootloaders & TOC bolcks; use JTAG for this.
            if ( !g_bBootMediaExist ) {
                OALMSG(OAL_ERROR, (TEXT("ERROR: BootMonitor: boot media does not exist.\r\n")));
                continue;
            } else {
                DWORD i;
                SectorInfo si;

                // to keep bootpart off of our reserved blocks we must mark it as bad, reserved & read-only
                si.bOEMReserved = ~(OEM_BLOCK_RESERVED | OEM_BLOCK_READONLY);
                si.bBadBlock    = BADBLOCKMARK;
//                si.bBadBlock    = 0xff;
                si.dwReserved1  = 0xffffffff;
                si.wReserved2   = 0xffff;

                OALMSG(TRUE, (TEXT("Reserving Blocks [0x%x - 0x%x] ...\r\n"), 0, IMAGE_START_BLOCK-1));

                #ifdef _IROMBOOT_
                i = (IS_LB)? 64: 32;

                #else
                i = 0;

                #endif  // !_IROMBOOT_

              for (; i < IMAGE_START_SECTOR; i++) {
                    FMD_WriteSector(i, NULL, &si, 1);
                }
                OALMSG(TRUE, (TEXT("...reserve complete.\r\n")));
            } break;
        case 'C':    // Toggle image storage to Smart Media.
        case 'c':
            pBootCfg->ConfigFlags= (pBootCfg->ConfigFlags ^ BOOT_OPTION_CLEAN);
            bConfigChanged = TRUE;
            break;
        case 'D':           // Download? Yes.
        case 'd':
            bDownload = TRUE;
            goto MENU_DONE;
        case 'E':
        case 'e':
            // low-level format
            // N.B: this erases images, BinFs, FATFS, user data, etc.
            // However, we don't format Bootloaders & TOC bolcks; use JTAG for this.
            if ( !g_bBootMediaExist ) {
                OALMSG(OAL_ERROR, (TEXT("ERROR: BootMonitor: boot media does not exist.\r\n")));
                continue;
            } else {
                UINT32 i;

                OALMSG(TRUE, (TEXT("Low-level format Blocks [0x%x - 0x%x] ...\r\n"), 0, IMAGE_START_BLOCK-1));
                for (i = NBOOT_BLOCK; i < (NBOOT_BLOCK+NBOOT_BLOCK_SIZE); i++) {
                    FMD_EraseBlock(i);
                }
                for (i = EBOOT_BLOCK; i < (EBOOT_BLOCK+EBOOT_BLOCK_SIZE); i++) {
                    FMD_EraseBlock(i);
                }
                OALMSG(TRUE, (TEXT("...erase complete.\r\n")));
            } break;
        case 'F':
        case 'f':
            // low-level format
            // N.B: this erases images, BinFs, FATFS, user data, etc.
            // However, we don't format Bootloaders & TOC bolcks; use JTAG for this.
            if ( !g_bBootMediaExist ) {
                OALMSG(OAL_ERROR, (TEXT("ERROR: BootMonitor: boot media does not exist.\r\n")));
                continue;
            } else {
                DWORD i;
                SectorInfo si;

                // to keep bootpart off of our reserved blocks we must mark it as bad, reserved & read-only
                si.bOEMReserved = ~(OEM_BLOCK_RESERVED | OEM_BLOCK_READONLY);
                si.bBadBlock    = BADBLOCKMARK;
                si.dwReserved1  = 0xffffffff;
                si.wReserved2   = 0xffff;

                OALMSG(TRUE, (TEXT("Reserving Blocks [0x%x - 0x%x] ...\r\n"), 0, IMAGE_START_BLOCK-1));

                #ifdef _IROMBOOT_
                i = (IS_LB)? 64: 32;

                #else
                i = 0;

                #endif  // !_IROMBOOT_
                for (; i < IMAGE_START_SECTOR; i++) {
                    FMD_WriteSector(i, NULL, &si, 1);
                }
                OALMSG(TRUE, (TEXT("...reserve complete.\r\n")));

                OALMSG(TRUE, (TEXT("Low-level format Blocks [0x%x - 0x%x] ...\r\n"), IMAGE_START_BLOCK, wNUM_BLOCKS-1));
                for (i = IMAGE_START_BLOCK; i < wNUM_BLOCKS; i++) {
                    FMD_EraseBlock(i);
                }
                OALMSG(TRUE, (TEXT("...erase complete.\r\n")));
            } break;
        case 'L':           // Download? No.
        case 'l':
            bDownload = FALSE;
            goto MENU_DONE;
/*
        case 'N':
        case 'n':
            {
                #define READ_LB_NAND_M_SIZE 2048
                #define READ_LB_NAND_S_SIZE 64
                #define READ_SB_NAND_M_SIZE 512
                #define READ_SB_NAND_S_SIZE 16
 
                UINT8  pMBuf[READ_LB_NAND_M_SIZE], pSBuf[READ_LB_NAND_S_SIZE];
                UINT32 rslt;
                UINT32 nMainloop, nSpareloop;
 
                rslt = SetBlockPage();
                OALMSG(TRUE, (TEXT("\r\n rlst : 0x%x\r\n"), rslt));
                memset(pMBuf, 0xff, READ_LB_NAND_M_SIZE);
                memset(pSBuf, 0xff, READ_LB_NAND_S_SIZE);
 
                if (IS_LB)
                {
                    nMainloop  = READ_LB_NAND_M_SIZE;
                    nSpareloop = READ_LB_NAND_S_SIZE;
 
                    //RAW_LB_ReadSector(rslt, pMBuf, pSBuf);
                    FMD_LB_ReadSector(rslt, pMBuf, pSBuf, 1,USE_NFCE);
                }
                else
                {
                    nMainloop  = READ_SB_NAND_M_SIZE;
                    nSpareloop = READ_SB_NAND_S_SIZE;
 
                    //RAW_SB_ReadSector(rslt, pMBuf, pSBuf);
                    FMD_SB_ReadSector(rslt, pMBuf, pSBuf,1,USE_NFCE);
                }
 
                PrintPageData(nMainloop, nSpareloop, pMBuf, pSBuf);
            }
            break;            
*/
        case 'R':
        case 'r':
            TOC_Read();
            TOC_Print();
            // TODO
            break;
        case 'S':           // Download? No.
        case 's':
            //bConfigChanged = TRUE;  // mask for using MLC type NAND flash device because of writing TOC frequently
			g_bSDDownload = TRUE;
			g_bUSBDownload = FALSE;
            bDownload = TRUE;
            goto MENU_DONE;
        case 'W':           // Configuration Write
        case 'w':
            if (!TOC_Write())
            {
                OALMSG(OAL_WARN, (TEXT("WARNING: MainMenu: Failed to store updated eboot configuration in flash.\r\n")));
            }
            else
            {
                OALMSG(OAL_INFO, (TEXT("Successfully Written\r\n")));
                bConfigChanged = FALSE;
            }
            break;
        default:
            break;
        }
    }

MENU_DONE:

    // If eboot settings were changed by user, save them to flash.
    //
    if (bConfigChanged && !TOC_Write())
    {
        OALMSG(OAL_WARN, (TEXT("WARNING: MainMenu: Failed to store updated bootloader configuration to flash.\r\n")));
    }

    return(bDownload);
}


/*
    @func   BOOL | OEMPlatformInit | Initialize the Samsung SMD6410 platform hardware.
    @rdesc  TRUE = Success, FALSE = Failure.
    @comm
    @xref
*/
BOOL OEMPlatformInit(void)
{
    ULONG BootDelay;
    UINT8 KeySelect;
    UINT32 dwStartTime, dwPrevTime, dwCurrTime;
    BOOL bResult = FALSE;
    FlashInfo flashInfo;
    // This is actually not PCI bus, but we use this structure to share FMD library code with FMD driver
    PCI_REG_INFO    RegInfo;    

    OALMSG(OAL_FUNC, (TEXT("+OEMPlatformInit.\r\n")));

    // Check if Current ARM speed is not matched to Target Arm speed
    // then To get speed up, set Voltage
#if (APLL_CLK == CLK_1332MHz)
    //LTC3714_Init();
   // LTC3714_VoltageSet(1,1200,100);     // ARM
   // LTC3714_VoltageSet(2,1300,100);     // INT
#endif

    EdbgOutputDebugString("Microsoft Windows CE Bootloader for the Samsung SMDK6410 Version %d.%d Built %s\r\n\r\n",
    EBOOT_VERSION_MAJOR, EBOOT_VERSION_MINOR, __DATE__);

    // Initialize the display.
    InitializeDisplay();

    // Initialize the BSP args structure.
    OALArgsInit(pBSPArgs);

    g_bCleanBootFlag = (BOOL*)OALArgsQuery(BSP_ARGS_QUERY_CLEANBOOT) ;
    g_KITLConfig = (OAL_KITL_ARGS *)OALArgsQuery(OAL_ARGS_QUERY_KITL);
    g_DevID = (UCHAR *)OALArgsQuery( OAL_ARGS_QUERY_DEVID);

    InitializeInterrupt();

    // Try to initialize the boot media block driver and BinFS partition.
    //
    ///*
	OALMSG(TRUE, (TEXT("HSMMC init\r\n")));
	HSMMCInit();
    OALMSG(TRUE, (TEXT("BP_Init\r\n")));

    memset(&RegInfo, 0, sizeof(PCI_REG_INFO));
    RegInfo.MemBase.Num = 2;
    RegInfo.MemBase.Reg[0] = (DWORD)OALPAtoVA(S3C6410_BASE_REG_PA_NFCON, FALSE);
    RegInfo.MemBase.Reg[1] = (DWORD)OALPAtoVA(S3C6410_BASE_REG_PA_SYSCON, FALSE);

    if (!BP_Init((LPBYTE)BINFS_RAM_START, BINFS_RAM_LENGTH, NULL, &RegInfo, NULL) )
    {
        OALMSG(OAL_WARN, (TEXT("WARNING: OEMPlatformInit failed to initialize Boot Media.\r\n")));
        g_bBootMediaExist = FALSE;
    }
    else
    {
        g_bBootMediaExist = TRUE;
    }

    // Get flash info
    if (!FMD_GetInfo(&flashInfo))
    {
        OALMSG(OAL_ERROR, (L"ERROR: BLFlashDownload: FMD_GetInfo call failed\r\n"));
    }

    wNUM_BLOCKS = flashInfo.dwNumBlocks;
    RETAILMSG(1, (TEXT("wNUM_BLOCKS : %d(0x%x) \r\n"), wNUM_BLOCKS, wNUM_BLOCKS));
    stDeviceInfo = GetNandInfo();

    // Try to retrieve TOC (and Boot config) from boot media
    if ( !TOC_Read( ) )
    {
        // use default settings
        TOC_Init(DEFAULT_IMAGE_DESCRIPTOR, (IMAGE_TYPE_RAMIMAGE), 0, 0, 0);
    }

    // Display boot message - user can halt the autoboot by pressing any key on the serial terminal emulator.
    BootDelay = g_pBootCfg->BootDelay;

    if (g_pBootCfg->ConfigFlags & BOOT_TYPE_DIRECT)
    {
        OALMSG(TRUE, (TEXT("Press [ENTER] to launch image stored on boot media, or [SPACE] to enter boot monitor.\r\n")));
        OALMSG(TRUE, (TEXT("\r\nInitiating image launch in %d seconds. "),BootDelay--));
    }
    else
    {
        OALMSG(TRUE, (TEXT("Press [ENTER] to download image stored on boot media, or [SPACE] to enter boot monitor.\r\n")));
        OALMSG(TRUE, (TEXT("\r\nInitiating image download in %d seconds. "),BootDelay--));
    }

    dwStartTime = OEMEthGetSecs();
    dwPrevTime  = dwStartTime;
    dwCurrTime  = dwStartTime;
    KeySelect   = 0;


    // Allow the user to break into the bootloader menu.
    while((dwCurrTime - dwStartTime) < g_pBootCfg->BootDelay)
    {
        KeySelect = OEMReadDebugByte();

        if ((KeySelect == 0x20) || (KeySelect == 0x0d))
        {
            break;
        }

        dwCurrTime = OEMEthGetSecs();

        if (dwCurrTime > dwPrevTime)
        {
            int i, j;

            // 1 Second has elapsed - update the countdown timer.
            dwPrevTime = dwCurrTime;

            // for text alignment
            if (BootDelay < 9)
            {
                i = 11;
            }
            else if (BootDelay < 99)
            {
                i = 12;
            }
            else if (BootDelay < 999)
            {
                i = 13;
            }
            else 
            {
                i = 14;     //< we don't care about this value when BootDelay over 1000 (1000 seconds)
            }
                

            for(j = 0; j < i; j++)
            {
                OEMWriteDebugByte((BYTE)0x08); // print back space
            }

            EdbgOutputDebugString ( "%d seconds. ", BootDelay--);
        }
    }

    OALMSG(OAL_INFO, (TEXT("\r\n")));

    // Boot or enter bootloader menu.
    //
    switch(KeySelect)
    {
    case 0x20: // Boot menu.
        g_pBootCfg->ConfigFlags &= ~BOOT_OPTION_CLEAN;        // Always clear CleanBoot Flags before Menu
        g_bDownloadImage = MainMenu(g_pBootCfg);
        break;
    case 0x00: // Fall through if no keys were pressed -or-
    case 0x0d: // the user cancelled the countdown.
    default:
#if 0 // It dose not need booting setting. it always fuses the OS image to NAND.       
		if (g_pBootCfg->ConfigFlags & BOOT_TYPE_DIRECT)
		{
			OALMSG(TRUE, (TEXT("\r\nLaunching image from boot media ... \r\n")));
			g_bDownloadImage = FALSE;
		}
		else
#endif            
        {
            OALMSG(TRUE, (TEXT("\r\nStarting auto-download ... \r\n")));
            g_bDownloadImage = TRUE;
            g_bAutoDownload = TRUE;
        }
        {
            DWORD i;

            OALMSG(TRUE, (TEXT("All block(%d) Erase...\r\n"), wNUM_BLOCKS));
            for (i = 0; i < wNUM_BLOCKS; i++) {
                FMD_EraseBlock(i);
            }
        }

        break;
    }

    //Update  Argument Area Value(KITL, Clean Option)

    if(g_pBootCfg->ConfigFlags &  BOOT_OPTION_CLEAN)
    {
        *g_bCleanBootFlag =TRUE;
    }

    else
    {
        *g_bCleanBootFlag =FALSE;
    }

    if(g_pBootCfg->ConfigFlags & CONFIG_FLAGS_KITL)
    {
        g_KITLConfig->flags=OAL_KITL_FLAGS_ENABLED | OAL_KITL_FLAGS_VMINI;
    }
    else
    {
        g_KITLConfig->flags&=~(OAL_KITL_FLAGS_ENABLED | OAL_KITL_FLAGS_VMINI);
    }

    g_KITLConfig->ipAddress= g_pBootCfg->EdbgAddr.dwIP;
    g_KITLConfig->ipMask     = g_pBootCfg->SubnetMask;
    g_KITLConfig->devLoc.IfcType    = Internal;
    g_KITLConfig->devLoc.BusNumber  = 0;
    g_KITLConfig->devLoc.LogicalLoc = BSP_BASE_REG_PA_DM9000A_IOBASE;

    memcpy(g_KITLConfig->mac, g_pBootCfg->EdbgAddr.wMAC, 6);

    OALKitlCreateName(BSP_DEVICE_PREFIX, g_KITLConfig->mac, g_DevID);

    if ( !g_bDownloadImage )
    {
        // User doesn't want to download image - load it from the boot media.
        // We could read an entire nk.bin or nk.nb0 into ram and jump.
        if ( !VALID_TOC(g_pTOC) )
        {
            OALMSG(OAL_ERROR, (TEXT("OEMPlatformInit: ERROR_INVALID_TOC, can not autoboot.\r\n")));
            return FALSE;
        }

        switch (g_ImageType)
        {
        case IMAGE_TYPE_STEPLDR:
            OALMSG(TRUE, (TEXT("Don't support launch STEPLDR.bin\r\n")));
            break;
        case IMAGE_TYPE_LOADER:
            OALMSG(TRUE, (TEXT("Don't support launch EBOOT.bin\r\n")));
            break;
        case IMAGE_TYPE_RAMIMAGE:
            OTGDEV_SetSoftDisconnect();
            OALMSG(TRUE, (TEXT("OEMPlatformInit: IMAGE_TYPE_RAMIMAGE\r\n")));
            if ( !ReadOSImageFromBootMedia( ) )
            {
                OALMSG(OAL_ERROR, (TEXT("OEMPlatformInit ERROR: Failed to load kernel region into RAM.\r\n")));
                return FALSE;
            }
            break;

        default:
            OALMSG(OAL_ERROR, (TEXT("OEMPlatformInit ERROR: unknown image type: 0x%x \r\n"), g_ImageType));
            return FALSE;
        }
    }

    // Configure Ethernet controller.
    if ( g_bAutoDownload != TRUE && g_bDownloadImage && (g_bUSBDownload == FALSE) && (g_bSDDownload == FALSE) )
	{
		if (!InitEthDevice(g_pBootCfg))
		{
			OALMSG(OAL_ERROR, (TEXT("ERROR: OEMPlatformInit: Failed to initialize Ethernet controller.\r\n")));
			goto CleanUp;
		}
	}

    bResult = TRUE;

    CleanUp:

    OALMSG(OAL_FUNC, (TEXT("_OEMPlatformInit.\r\n")));

    return(bResult);
}


/*
    @func   DWORD | OEMPreDownload | Complete pre-download tasks - get IP address, initialize TFTP, etc.
    @rdesc  BL_DOWNLOAD = Platform Builder is asking us to download an image, BL_JUMP = Platform Builder is requesting we jump to an existing image, BL_ERROR = Failure.
    @comm
    @xref
*/
DWORD OEMPreDownload(void)
{
    BOOL  bGotJump = TRUE;

	if ( (g_bDownloadImage != FALSE) && (g_bAutoDownload == FALSE) )
	{	
		OALMSG(TRUE, (TEXT("Please choose the Image on SD.\r\n")));
		bGotJump = FALSE;
		ChooseImageFromSD();
	}

    return(bGotJump ? BL_JUMP : BL_DOWNLOAD);
}


/*
    @func   BOOL | OEMReadData | Generically read download data (abstracts actual transport read call).
    @rdesc  TRUE = Success, FALSE = Failure.
    @comm
    @xref
*/
BOOL OEMReadData(DWORD dwData, PUCHAR pData)
{
	BOOL ret;
	//DWORD i;
   	OALMSG(OAL_FUNC, (TEXT("+OEMReadData.\r\n")));
	//OALMSG(TRUE, (TEXT("\r\nINFO: dwData = 0x%x, pData = 0x%x \r\n"), dwData, pData));

    if ( g_bAutoDownload == TRUE )
    {
		ret = SDReadData(dwData, pData);        
    }
	else if ( g_bUSBDownload == FALSE && g_bSDDownload == FALSE  )
	{
		ret = EbootEtherReadData(dwData, pData);
	}
	else if ( g_bSDDownload == TRUE )
	{
		// TODO :: This function have to be implemented.
		ret = SDReadData(dwData, pData);
	}
	return(ret);
}

void OEMReadDebugString(CHAR * szString)
{
//    static CHAR szString[16];   // The string used to collect the dotted decimal IP address.
    USHORT cwNumChars = 0;
    USHORT InChar = 0;

    while(!((InChar == 0x0d) || (InChar == 0x0a)))
    {
        InChar = OEMReadDebugByte();
        if (InChar != OEM_DEBUG_COM_ERROR && InChar != OEM_DEBUG_READ_NODATA)
        {
            if ((InChar >= 'a' && InChar <='z') || (InChar >= 'A' && InChar <= 'Z') || (InChar >= '0' && InChar <= '9'))
            {
                if (cwNumChars < 16)
                {
                    szString[cwNumChars++] = (char)InChar;
                    OEMWriteDebugByte((BYTE)InChar);
                }
            }
            // If it's a backspace, back up.
            //
            else if (InChar == 8)
            {
                if (cwNumChars > 0)
                {
                    cwNumChars--;
                    OEMWriteDebugByte((BYTE)InChar);
                }
            }
        }
    }

    // If it's a carriage return with an empty string, don't change anything.
    //
    if (cwNumChars)
    {
        szString[cwNumChars] = '\0';
        EdbgOutputDebugString("\r\n");
    }
}


/*
    @func   void | OEMShowProgress | Displays download progress for the user.
    @rdesc  N/A.
    @comm
    @xref
*/
void OEMShowProgress(DWORD dwPacketNum)
{
    OALMSG(OAL_FUNC, (TEXT("+OEMShowProgress.\r\n")));
}


/*
    @func   void | OEMLaunch | Executes the stored/downloaded image.
    @rdesc  N/A.
    @comm
    @xref
*/

void OEMLaunch( DWORD dwImageStart, DWORD dwImageLength, DWORD dwLaunchAddr, const ROMHDR *pRomHdr )
{
    DWORD dwPhysLaunchAddr;

    OALMSG(OAL_FUNC, (TEXT("+OEMLaunch.\r\n")));


    // If the user requested that a disk image (stored in RAM now) be written to the SmartMedia card, so it now.
    //
	if (g_bDownloadImage) // && (g_pBootCfg->ConfigFlags & TARGET_TYPE_NAND))
    {
        // Since this platform only supports RAM images, the image cache address is the same as the image RAM address.
        //

        switch (g_ImageType)
        {
        	case IMAGE_TYPE_STEPLDR:
		        if (!WriteRawImageToBootMedia(dwImageStart, dwImageLength, dwLaunchAddr))
		        {
            		OALMSG(OAL_ERROR, (TEXT("ERROR: OEMLaunch: Failed to store image to Smart Media.\r\n")));
            		goto CleanUp;
        		}
		        OALMSG(TRUE, (TEXT("INFO: Step loader image stored to Smart Media.  Please Reboot.  Halting...\r\n")));
            if (g_bAutoDownload == TRUE)
            {
                return ;
            }
			while(1){};
        		break;

            case IMAGE_TYPE_LOADER:
				g_pTOC->id[0].dwLoadAddress = dwImageStart;
				g_pTOC->id[0].dwTtlSectors = FILE_TO_SECTOR_SIZE(dwImageLength);
		        if (!WriteRawImageToBootMedia(dwImageStart, dwImageLength, dwLaunchAddr))
		        {
            		OALMSG(OAL_ERROR, (TEXT("ERROR: OEMLaunch: Failed to store image to Smart Media.\r\n")));
            		goto CleanUp;
        		}
				if (dwLaunchAddr && (g_pTOC->id[0].dwJumpAddress != dwLaunchAddr))
				{
					g_pTOC->id[0].dwJumpAddress = dwLaunchAddr;
					if ( !TOC_Write() ) {
	            		EdbgOutputDebugString("*** OEMLaunch ERROR: TOC_Write failed! Next boot may not load from disk *** \r\n");
					}
	        		TOC_Print();
				}
			OALMSG(TRUE, (TEXT("INFO: Eboot image stored to Smart Media.  Please Reboot.  Halting...\r\n")));
            if (g_bAutoDownload == TRUE)
            {
                return ;
            }

			while(1){};
			break;

            case IMAGE_TYPE_RAMIMAGE:
				g_pTOC->id[g_dwTocEntry].dwLoadAddress = dwImageStart;
				g_pTOC->id[g_dwTocEntry].dwTtlSectors = FILE_TO_SECTOR_SIZE(dwImageLength);
		        if (!WriteOSImageToBootMedia(dwImageStart, dwImageLength, dwLaunchAddr))
		        {
            		OALMSG(OAL_ERROR, (TEXT("ERROR: OEMLaunch: Failed to store image to Smart Media.\r\n")));
            		goto CleanUp;
        		}

				if (dwLaunchAddr && (g_pTOC->id[g_dwTocEntry].dwJumpAddress != dwLaunchAddr))
				{
					g_pTOC->id[g_dwTocEntry].dwJumpAddress = dwLaunchAddr;
					if ( !TOC_Write() ) {
	            		EdbgOutputDebugString("*** OEMLaunch ERROR: TOC_Write failed! Next boot may not load from disk *** \r\n");
					}
	        		TOC_Print();
				}
				else
				{
					dwLaunchAddr= g_pTOC->id[g_dwTocEntry].dwJumpAddress;
					EdbgOutputDebugString("INFO: using TOC[%d] dwJumpAddress: 0x%x\r\n", g_dwTocEntry, dwLaunchAddr);
				}

                break;
        }
    }

    OALMSG(1, (TEXT("waitforconnect\r\n")));
    // Wait for Platform Builder to connect after the download and send us IP and port settings for service
    // connections - also sends us KITL flags.  This information is used later by the OS (KITL).
    //


    // save ethernet address for ethernet kitl // added by jjg 06.09.18
    SaveEthernetAddress();

    // If a launch address was provided, we must have downloaded the image, save the address in case we
    // want to jump to this image next time.  If no launch address was provided, retrieve the last one.
    //
    if (dwLaunchAddr && (g_pTOC->id[g_dwTocEntry].dwJumpAddress != dwLaunchAddr))
    {
        g_pTOC->id[g_dwTocEntry].dwJumpAddress = dwLaunchAddr;
    }
    else
    {
        dwLaunchAddr= g_pTOC->id[g_dwTocEntry].dwJumpAddress;
        OALMSG(OAL_INFO, (TEXT("INFO: using TOC[%d] dwJumpAddress: 0x%x\r\n"), g_dwTocEntry, dwLaunchAddr));
    }

    // Jump to downloaded image (use the physical address since we'll be turning the MMU off)...
    //
    dwPhysLaunchAddr = (DWORD)OALVAtoPA((void *)dwLaunchAddr);
    OALMSG(TRUE, (TEXT("INFO: OEMLaunch: Jumping to Physical Address 0x%Xh (Virtual Address 0x%Xh)...\r\n\r\n\r\n"), dwPhysLaunchAddr, dwLaunchAddr));

    // Jump...
    //
    Launch(dwPhysLaunchAddr);


CleanUp:

    OALMSG(TRUE, (TEXT("ERROR: OEMLaunch: Halting...\r\n")));
    SpinForever();
}


//------------------------------------------------------------------------------
//
//  Function Name:  OEMVerifyMemory( DWORD dwStartAddr, DWORD dwLength )
//  Description..:  This function verifies the passed address range lies
//                  within a valid region of memory. Additionally this function
//                  sets the g_ImageType if the image is a boot loader.
//  Inputs.......:  DWORD           Memory start address
//                  DWORD           Memory length
//  Outputs......:  BOOL - true if verified, false otherwise
//
//------------------------------------------------------------------------------

BOOL OEMVerifyMemory( DWORD dwStartAddr, DWORD dwLength )
{

    OALMSG(OAL_FUNC, (TEXT("+OEMVerifyMemory.\r\n")));

    // Is the image being downloaded the stepldr?
    if ((dwStartAddr >= STEPLDR_RAM_IMAGE_BASE) &&
        ((dwStartAddr + dwLength - 1) < (STEPLDR_RAM_IMAGE_BASE + STEPLDR_RAM_IMAGE_SIZE)))
    {
        OALMSG(OAL_INFO, (TEXT("Stepldr image\r\n")));
        g_ImageType = IMAGE_TYPE_STEPLDR;     // Stepldr image.
        return TRUE;
    }
    // Is the image being downloaded the bootloader?
    else if ((dwStartAddr >= EBOOT_STORE_ADDRESS) &&
        ((dwStartAddr + dwLength - 1) < (EBOOT_STORE_ADDRESS + EBOOT_STORE_MAX_LENGTH)))
    {
        OALMSG(OAL_INFO, (TEXT("Eboot image\r\n")));
        g_ImageType = IMAGE_TYPE_LOADER;     // Eboot image.
        return TRUE;
    }

    // Is it a ram image?
    else if ((dwStartAddr >= ROM_RAMIMAGE_START) &&
        ((dwStartAddr + dwLength - 1) < (ROM_RAMIMAGE_START + ROM_RAMIMAGE_SIZE)))
    {
        OALMSG(OAL_INFO, (TEXT("RAM image\r\n")));
        g_ImageType = IMAGE_TYPE_RAMIMAGE;
        return TRUE;
    }
    else if (!dwStartAddr && !dwLength)
    {
        OALMSG(TRUE, (TEXT("Don't support raw image\r\n")));
        g_ImageType = IMAGE_TYPE_RAWBIN;
        return FALSE;
    }

    // HACKHACK: get around MXIP images with funky addresses
    OALMSG(TRUE, (TEXT("BIN image type unknow\r\n")));

    OALMSG(OAL_FUNC, (TEXT("_OEMVerifyMemory.\r\n")));

    return FALSE;
}

/*
    @func   void | OEMMultiBINNotify | Called by blcommon to nofity the OEM code of the number, size, and location of one or more BIN regions,
                                       this routine collects the information and uses it when temporarily caching a flash image in RAM prior to final storage.
    @rdesc  N/A.
    @comm
    @xref
*/
void OEMMultiBINNotify(const PMultiBINInfo pInfo)
{
    BYTE nCount;
    DWORD g_dwMinImageStart;

    OALMSG(OAL_FUNC, (TEXT("+OEMMultiBINNotify.\r\n")));

    if (!pInfo || !pInfo->dwNumRegions)
    {
        OALMSG(OAL_WARN, (TEXT("WARNING: OEMMultiBINNotify: Invalid BIN region descriptor(s).\r\n")));
        return;
    }

    if (!pInfo->Region[0].dwRegionStart && !pInfo->Region[0].dwRegionLength)
    {
        return;
    }

    g_dwMinImageStart = pInfo->Region[0].dwRegionStart;

    OALMSG(TRUE, (TEXT("\r\nDownload BIN file information:\r\n")));
    OALMSG(TRUE, (TEXT("-----------------------------------------------------\r\n")));
    for (nCount = 0 ; nCount < pInfo->dwNumRegions ; nCount++)
    {
        OALMSG(TRUE, (TEXT("[%d]: Base Address=0x%x  Length=0x%x\r\n"),
            nCount, pInfo->Region[nCount].dwRegionStart, pInfo->Region[nCount].dwRegionLength));
        if (pInfo->Region[nCount].dwRegionStart < g_dwMinImageStart)
        {
            g_dwMinImageStart = pInfo->Region[nCount].dwRegionStart;
            if (g_dwMinImageStart == 0)
            {
                OALMSG(OAL_WARN, (TEXT("WARNING: OEMMultiBINNotify: Bad start address for region (%d).\r\n"), nCount));
                return;
            }
        }
    }

    memcpy((LPBYTE)&g_BINRegionInfo, (LPBYTE)pInfo, sizeof(MultiBINInfo));

    OALMSG(TRUE, (TEXT("-----------------------------------------------------\r\n")));
    OALMSG(OAL_FUNC, (TEXT("_OEMMultiBINNotify.\r\n")));
}

#if    0    // by dodan2
/////////////////////// START - Stubbed functions - START //////////////////////////////
/*
    @func   void | SC_WriteDebugLED | Write to debug LED.
    @rdesc  N/A.
    @comm
    @xref
*/

void SC_WriteDebugLED(USHORT wIndex, ULONG dwPattern)
{
    // Stub - needed by NE2000 EDBG driver...
    //
}


ULONG HalSetBusDataByOffset(IN BUS_DATA_TYPE BusDataType,
                            IN ULONG BusNumber,
                            IN ULONG SlotNumber,
                            IN PVOID Buffer,
                            IN ULONG Offset,
                            IN ULONG Length)
{
    return(0);
}


ULONG
HalGetBusDataByOffset(IN BUS_DATA_TYPE BusDataType,
                      IN ULONG BusNumber,
                      IN ULONG SlotNumber,
                      IN PVOID Buffer,
                      IN ULONG Offset,
                      IN ULONG Length)
{
    return(0);
}


BOOLEAN HalTranslateBusAddress(IN INTERFACE_TYPE  InterfaceType,
                               IN ULONG BusNumber,
                               IN PHYSICAL_ADDRESS BusAddress,
                               IN OUT PULONG AddressSpace,
                               OUT PPHYSICAL_ADDRESS TranslatedAddress)
{

    // All accesses on this platform are memory accesses...
    //
    if (AddressSpace)
        *AddressSpace = 0;

    // 1:1 mapping...
    //
    if (TranslatedAddress)
    {
        *TranslatedAddress = BusAddress;
        return(TRUE);
    }

    return(FALSE);
}


PVOID MmMapIoSpace(IN PHYSICAL_ADDRESS PhysicalAddress,
                   IN ULONG NumberOfBytes,
                   IN BOOLEAN CacheEnable)
{
    DWORD dwAddr = PhysicalAddress.LowPart;

    if (CacheEnable)
        dwAddr &= ~CACHED_TO_UNCACHED_OFFSET;
    else
        dwAddr |= CACHED_TO_UNCACHED_OFFSET;

    return((PVOID)dwAddr);
}


VOID MmUnmapIoSpace(IN PVOID BaseAddress,
                    IN ULONG NumberOfBytes)
{
}

VOID WINAPI SetLastError(DWORD dwErrCode)
{
}
/////////////////////// END - Stubbed functions - END //////////////////////////////
#endif

/*
    @func   PVOID | GetKernelExtPointer | Locates the kernel region's extension area pointer.
    @rdesc  Pointer to the kernel's extension area.
    @comm
    @xref
*/
PVOID GetKernelExtPointer(DWORD dwRegionStart, DWORD dwRegionLength)
{
    DWORD dwCacheAddress = 0;
    ROMHDR *pROMHeader;
    DWORD dwNumModules = 0;
    TOCentry *pTOC;

    if (dwRegionStart == 0 || dwRegionLength == 0)
        return(NULL);

    if (*(LPDWORD) OEMMapMemAddr (dwRegionStart, dwRegionStart + ROM_SIGNATURE_OFFSET) != ROM_SIGNATURE)
        return NULL;

    // A pointer to the ROMHDR structure lives just past the ROM_SIGNATURE (which is a longword value).  Note that
    // this pointer is remapped since it might be a flash address (image destined for flash), but is actually cached
    // in RAM.
    //
    dwCacheAddress = *(LPDWORD) OEMMapMemAddr (dwRegionStart, dwRegionStart + ROM_SIGNATURE_OFFSET + sizeof(ULONG));
    pROMHeader     = (ROMHDR *) OEMMapMemAddr (dwRegionStart, dwCacheAddress);

    // Make sure sure are some modules in the table of contents.
    //
    if ((dwNumModules = pROMHeader->nummods) == 0)
        return NULL;

    // Locate the table of contents and search for the kernel executable and the TOC immediately follows the ROMHDR.
    //
    pTOC = (TOCentry *)(pROMHeader + 1);

    while(dwNumModules--) {
        LPBYTE pFileName = OEMMapMemAddr(dwRegionStart, (DWORD)pTOC->lpszFileName);
        if (!strcmp((const char *)pFileName, "nk.exe")) {
            return ((PVOID)(pROMHeader->pExtensions));
        }
        ++pTOC;
    }
    return NULL;
}


/*
    @func   BOOL | OEMDebugInit | Initializes the serial port for debug output message.
    @rdesc  TRUE == Success and FALSE == Failure.
    @comm
    @xref
*/
BOOL OEMDebugInit(void)
{

    // Set up function callbacks used by blcommon.
    //
    g_pOEMVerifyMemory   = OEMVerifyMemory;      // Verify RAM.
    g_pOEMMultiBINNotify = OEMMultiBINNotify;

    // Call serial initialization routine (shared with the OAL).
    //
    OEMInitDebugSerial();

    return(TRUE);
}

/*
    @func   void | SaveEthernetAddress | Save Ethernet Address on IMAGE_SHARE_ARGS_UA_START for Ethernet KITL
    @rdesc
    @comm
    @xref
*/
void    SaveEthernetAddress()
{
    memcpy(pBSPArgs->kitl.mac, g_pBootCfg->EdbgAddr.wMAC, 6);
    if (!(g_pBootCfg->ConfigFlags & CONFIG_FLAGS_DHCP))
    {
        // Static IP address.
        pBSPArgs->kitl.ipAddress  = g_pBootCfg->EdbgAddr.dwIP;
        pBSPArgs->kitl.ipMask     = g_pBootCfg->SubnetMask;
        pBSPArgs->kitl.flags     &= ~OAL_KITL_FLAGS_DHCP;
    }
    else
    {
        pBSPArgs->kitl.ipAddress  = g_DeviceAddr.dwIP;
        pBSPArgs->kitl.flags     |= OAL_KITL_FLAGS_DHCP;
    }
}

//
//
//
//

static void InitializeDisplay(void)
{
    tDevInfo RGBDevInfo;

    volatile S3C6410_GPIO_REG *pGPIOReg = (S3C6410_GPIO_REG *)OALPAtoVA(S3C6410_BASE_REG_PA_GPIO, FALSE);
    volatile S3C6410_DISPLAY_REG *pDispReg = (S3C6410_DISPLAY_REG *)OALPAtoVA(S3C6410_BASE_REG_PA_DISPLAY, FALSE);
    volatile S3C6410_SPI_REG *pSPIReg = (S3C6410_SPI_REG *)OALPAtoVA(S3C6410_BASE_REG_PA_SPI0, FALSE);
    volatile S3C6410_MSMIF_REG *pMSMIFReg = (S3C6410_MSMIF_REG *)OALPAtoVA(S3C6410_BASE_REG_PA_MSMIF_SFR, FALSE);

    volatile S3C6410_SYSCON_REG *pSysConReg = (S3C6410_SYSCON_REG *)OALPAtoVA(S3C6410_BASE_REG_PA_SYSCON, FALSE);

    EdbgOutputDebugString("[Eboot] ++InitializeDisplay()\r\n");

    // Initialize Display Power Gating
    if(!(pSysConReg->BLK_PWR_STAT & (1<<4))) {
        pSysConReg->NORMAL_CFG |= (1<<14);
        while(!(pSysConReg->BLK_PWR_STAT & (1<<4)));
        }

    // Initialize Virtual Address
    LDI_initialize_register_address((void *)pSPIReg, (void *)pDispReg, (void *)pGPIOReg);
    Disp_initialize_register_address((void *)pDispReg, (void *)pMSMIFReg, (void *)pGPIOReg);

    // Set LCD Module Type
#if        (SMDK6410_LCD_MODULE == LCD_MODULE_LTS222)
    LDI_set_LCD_module_type(LDI_LTS222QV_RGB);
#elif    (SMDK6410_LCD_MODULE == LCD_MODULE_LTV350)
    LDI_set_LCD_module_type(LDI_LTV350QV_RGB);
#elif    (SMDK6410_LCD_MODULE == LCD_MODULE_LTE480)
    LDI_set_LCD_module_type(LDI_LTE480WV_RGB);
#elif    (SMDK6410_LCD_MODULE == LCD_MODULE_EMUL48_D1)
    LDI_set_LCD_module_type(LDI_LTE480WV_RGB);
#elif    (SMDK6410_LCD_MODULE == LCD_MODULE_EMUL48_QV)
    LDI_set_LCD_module_type(LDI_LTE480WV_RGB);
#elif    (SMDK6410_LCD_MODULE == LCD_MODULE_EMUL48_PQV)
    LDI_set_LCD_module_type(LDI_LTE480WV_RGB);
#elif    (SMDK6410_LCD_MODULE == LCD_MODULE_EMUL48_ML)
    LDI_set_LCD_module_type(LDI_LTE480WV_RGB);
#elif    (SMDK6410_LCD_MODULE == LCD_MODULE_EMUL48_MP)
    LDI_set_LCD_module_type(LDI_LTE480WV_RGB);
#elif    (SMDK6410_LCD_MODULE == LCD_MODULE_LTP700)
    LDI_set_LCD_module_type(LDI_LTP700WV_RGB);
#else
    EdbgOutputDebugString("[Eboot:ERR] InitializeDisplay() : Unknown Module Type [%d]\r\n", SMDK6410_LCD_MODULE);
#endif

    // Get RGB Interface Information from LDI Library
    LDI_fill_output_device_information(&RGBDevInfo);

    // Setup Output Device Information
    Disp_set_output_device_information(&RGBDevInfo);

    // Initialize Display Controller
    Disp_initialize_output_interface(DISP_VIDOUT_RGBIF);

#if        (LCD_BPP == 16)
    Disp_set_window_mode(DISP_WIN1_DMA, DISP_16BPP_565, LCD_WIDTH, LCD_HEIGHT, 0, 0);
#elif    (LCD_BPP == 32)    // XRGB format (RGB888)
    Disp_set_window_mode(DISP_WIN1_DMA, DISP_24BPP_888, LCD_WIDTH, LCD_HEIGHT, 0, 0);
#else
    EdbgOutputDebugString("[Eboot:ERR] InitializeDisplay() : Unknown Color Depth %d bpp\r\n", LCD_BPP);
#endif

    Disp_set_framebuffer(DISP_WIN1, EBOOT_FRAMEBUFFER_PA_START);
    Disp_window_onfoff(DISP_WIN1, DISP_WINDOW_ON);

#if    (SMDK6410_LCD_MODULE == LCD_MODULE_LTS222)
    // This type of LCD need MSM I/F Bypass Mode to be Disabled
    pMSMIFReg->MIFPCON &= ~(0x1<<3);    // SEL_BYPASS -> Normal Mode
#endif

    // Initialize LCD Module
    LDI_initialize_LCD_module();

    // LCD Clock Source as MPLL_Dout
    pSysConReg->CLK_SRC = (pSysConReg->CLK_SRC & ~(0xFFFFFFF0))
                            |(0<<31)    // TV27_SEL    -> 27MHz
                            |(0<<30)    // DAC27        -> 27MHz
                            |(0<<28)    // SCALER_SEL    -> MOUT_EPLL
                            |(0<<26)    // LCD_SEL    -> Dout_MPLL
                            |(0<<24)    // IRDA_SEL    -> MOUT_EPLL
                            |(0<<22)    // MMC2_SEL    -> MOUT_EPLL
                            |(0<<20)    // MMC1_SEL    -> MOUT_EPLL
                            |(0<<18)    // MMC0_SEL    -> MOUT_EPLL
                            |(0<<16)    // SPI1_SEL    -> MOUT_EPLL
                            |(0<<14)    // SPI0_SEL    -> MOUT_EPLL
                            |(0<<13)    // UART_SEL    -> MOUT_EPLL
                            |(0<<10)    // AUDIO1_SEL    -> MOUT_EPLL
                            |(0<<7)        // AUDIO0_SEL    -> MOUT_EPLL
                            |(0<<5)        // UHOST_SEL    -> 48MHz
                            |(0<<4);        // MFCCLK_SEL    -> HCLKx2 (0:HCLKx2, 1:MoutEPLL)

    // Video Output Enable
    Disp_envid_onoff(DISP_ENVID_ON);

    // Fill Framebuffer
#if    (SMDK6410_LCD_MODULE == LCD_MODULE_LTV350)
    memcpy((void *)EBOOT_FRAMEBUFFER_UA_START, (void *)InitialImage_rgb16_320x240, 320*240*2);
#elif    (SMDK6410_LCD_MODULE == LCD_MODULE_EMULQV)
    memcpy((void *)EBOOT_FRAMEBUFFER_UA_START, (void *)InitialImage_rgb16_320x240, 320*240*2);
#elif    (LCD_BPP == 16)
    {
        int i;
        unsigned short *pFB;
        pFB = (unsigned short *)EBOOT_FRAMEBUFFER_UA_START;

        for (i=0; i<LCD_WIDTH*LCD_HEIGHT; i++)
            *pFB++ = 0x001F;        // Blue
    }
#elif    (LCD_BPP == 32)
    {
        int i;
        unsigned int *pFB;
        pFB = (unsigned int *)EBOOT_FRAMEBUFFER_UA_START;

        for (i=0; i<LCD_WIDTH*LCD_HEIGHT; i++)
            *pFB++ = 0x000000FF;        // Blue
    }
#endif
    // Backlight Power On
    //Set PWM GPIO to control Back-light  Regulator  Shotdown Pin (GPF[15])
    //pGPIOReg->GPFDAT |= (1<<15);
   // pGPIOReg->GPFCON = (pGPIOReg->GPFCON & ~(3<<30)) | (1<<30);    // set GPF[15] as Output
    pGPIOReg->GPFDAT |= (1<<14);
    pGPIOReg->GPFCON = (pGPIOReg->GPFCON & ~(3<<28)) | (1<<28);    // set GPF[15] as Output
    EdbgOutputDebugString("[Eboot] --InitializeDisplay()\r\n");
}

static void SpinForever(void)
{
    EdbgOutputDebugString("SpinForever...\r\n");

    while(1)
    {
        ;
    }
}

void FormatNANDFlash()
{

	// low-level format
	// N.B: this erases images, BinFs, FATFS, user data, etc.
	// However, we don't format Bootloaders & TOC bolcks; use JTAG for this.
	if ( !g_bBootMediaExist ) {
		OALMSG(OAL_ERROR, (TEXT("ERROR: BootMonitor: boot media does not exist.\r\n")));
	} else {
	    DWORD i;
	    SectorInfo si;

	    // to keep bootpart off of our reserved blocks we must mark it as bad, reserved & read-only
	    si.bOEMReserved = ~(OEM_BLOCK_RESERVED | OEM_BLOCK_READONLY);
	    si.bBadBlock    = BADBLOCKMARK;
	    si.dwReserved1  = 0xffffffff;
	    si.wReserved2   = 0xffff;

	    OALMSG(TRUE, (TEXT("Reserving Blocks [0x%x - 0x%x] ...\r\n"), 0, IMAGE_START_BLOCK-1));

	    i = (IS_LB)? 64: 32;

	    for (; i < IMAGE_START_SECTOR; i++) {
	        FMD_WriteSector(i, NULL, &si, 1);
	    }
	    OALMSG(TRUE, (TEXT("...reserve complete.\r\n")));

	    OALMSG(TRUE, (TEXT("Low-level format Blocks [0x%x - 0x%x] ...\r\n"), IMAGE_START_BLOCK, wNUM_BLOCKS-1));
	    for (i = IMAGE_START_BLOCK; i < wNUM_BLOCKS; i++) {
	        FMD_EraseBlock(i);
	    }
	    
	}

    if ( !g_bBootMediaExist ) {
		OALMSG(OAL_ERROR, (TEXT("ERROR: BootMonitor: boot media does not exist.\r\n")));
    }
    // N.B: format offset by # of reserved blocks,
    // decrease the ttl # blocks available by that amount.
    if ( !BP_LowLevelFormat( IMAGE_START_BLOCK,
                             wNUM_BLOCKS - IMAGE_START_BLOCK,
                             FORMAT_SKIP_BLOCK_CHECK) )
    {
        OALMSG(OAL_ERROR, (TEXT("ERROR: BootMonitor: Low-level boot media format failed.\r\n")));
    }
		OALMSG(TRUE, (TEXT("BinFS format done.\r\n")));	
}



