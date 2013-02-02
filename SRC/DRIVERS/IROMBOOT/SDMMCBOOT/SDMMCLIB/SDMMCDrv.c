//
// Copyright (c) Samsung Electronics. Co. LTD.  All rights reserved.
//
/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.
*/

#ifdef __cplusplus
extern "C" {
#endif

#include <windows.h>
#include <ceddk.h>
#include <bsp.h>
#include <DrvLib.h>
#include <HSMMCDrv.h>

#ifdef FOR_EBOOT
#include <IROM_SDMMC_loader_cfg.h>
#include "loader.h"
#endif


/*******************  Definitions  ***********************/
#define HSMMC_DATA_BUSWIDTH  4
#define SD_DATA_BUSWIDTH 4

#define SD_CARD 0
#define MMC_CARD 1

#define _DBG_ 0

// The SD channel is used as main storage
#define LOOP_FOR_TIMEOUT 0x1000000
#define SD_CLK_SRC SD_EPLL

/*********************************************************/


/****************  external functions  *******************/
// these functions are defined in mmccopy.s
void GetData512(PUCHAR pBuf, volatile UINT32 *dwFIFOAddr);
void PutData512(PUCHAR pBuf, volatile UINT32 *dwFIFOAddr);
/*********************************************************/


/****************  external variables  *******************/
#ifdef FOR_EBOOT
extern PBOOT_CFG        g_pBootCfg;
extern UCHAR            g_TOC[512];
extern const PTOC         g_pTOC;
extern DWORD            g_dwTocEntry;
extern DWORD            g_ImageType;
//extern BOOL             g_bWaitForConnect;

extern void BootConfigInit(DWORD dwIndex);
#endif

/*********************************************************/

void BootConfigPrint(void);


/*****************  global variables  ********************/
MMC_PARSED_REGISTER_EXTCSD ___EXTCSDRegister;
MMC_PARSED_REGISTER_EXTCSD *g_pEXTCSDRegister;

volatile S3C6410_HSMMC_REG *g_vpHSMMCReg;
volatile S3C6410_SYSCON_REG *g_vpSYSCONReg;
volatile S3C6410_GPIO_REG *g_vpIOPortReg;

UINT32 g_dwSectorCount;
UINT32 g_dwIsSDHC;

static UINT32 g_dwWhileOCRCheck=0;
static UINT32 g_dwMMCSpec42;
static UINT32 g_dwIsSDSPEC20;
static UINT32 g_dwCmdRetryCount;
static UINT32 g_dwIsMMC;
static UINT32 g_dwRCA=0;
static UINT32 g_dwSDSPEC;
static UINT32 g_dwISHihgSpeed;
static UINT32 g_dwSysintr;
static UINT32 g_dwMemInit=0;

#ifdef FOR_BIBDRV
static const wchar_t *g_strDrvName = {TEXT("BIBDRV")};
#elif defined (FOR_HSMMCDRV)
static const wchar_t *g_strDrvName = {TEXT("SDMMCDRV")};
#else
static const wchar_t *g_strDrvName = {TEXT("SDMMC")};
#endif

#ifdef USE_CHANNEL0
static const BOOL g_UseChannel0 = (TRUE);
#else
static const BOOL g_UseChannel0 = (FALSE);
#endif

/*********************************************************/

/*******************  macro functions ********************/
/*********************************************************/

#ifdef FOR_HSMMCDRV
HANDLE g_hSDDMADoneEvent; // trans done interrupt
#endif

static void LoopForDelay(UINT32 count)
{
    volatile UINT32 j;
    for(j = 0; j < count; j++)  ;
}


#ifdef FOR_EBOOT
void BootConfigPrint(void)
{
    EdbgOutputDebugString( "BootCfg { \r\n");
    EdbgOutputDebugString( "  ConfigFlags: 0x%x\r\n", g_pBootCfg->ConfigFlags);
    EdbgOutputDebugString( "  BootDelay: 0x%x\r\n", g_pBootCfg->BootDelay);
    EdbgOutputDebugString( "  ImageIndex: %d \r\n", g_pBootCfg->ImageIndex);
    EdbgOutputDebugString( "  IP: %s\r\n", inet_ntoa(g_pBootCfg->EdbgAddr.dwIP));
    EdbgOutputDebugString( "  MAC Address: %B:%B:%B:%B:%B:%B\r\n",
                           g_pBootCfg->EdbgAddr.wMAC[0] & 0x00FF, g_pBootCfg->EdbgAddr.wMAC[0] >> 8,
                           g_pBootCfg->EdbgAddr.wMAC[1] & 0x00FF, g_pBootCfg->EdbgAddr.wMAC[1] >> 8,
                           g_pBootCfg->EdbgAddr.wMAC[2] & 0x00FF, g_pBootCfg->EdbgAddr.wMAC[2] >> 8);
    EdbgOutputDebugString( "  Port: %s\r\n", inet_ntoa(g_pBootCfg->EdbgAddr.wPort));

    EdbgOutputDebugString( "  SubnetMask: %s\r\n", inet_ntoa(g_pBootCfg->SubnetMask));
    EdbgOutputDebugString( "}\r\n");
}


// Set default boot configuration values
void BootConfigInit(DWORD dwIndex)
{

    EdbgOutputDebugString("+BootConfigInit\r\n");

    g_pBootCfg = &g_pTOC->BootCfg;

    memset(g_pBootCfg, 0, sizeof(BOOT_CFG));

    g_pBootCfg->ImageIndex   = dwIndex;

    g_pBootCfg->ConfigFlags  = BOOT_TYPE_MULTISTAGE | CONFIG_FLAGS_KITL;

    g_pBootCfg->BootDelay    = CONFIG_BOOTDELAY_DEFAULT;

    g_pBootCfg->SubnetMask = inet_addr("255.255.255.0");

    EdbgOutputDebugString("-BootConfigInit\r\n");
    return;
}

void ID_Print(DWORD i) {
    DWORD j;
    EdbgOutputDebugString("ID[%u] {\r\n", i);
    EdbgOutputDebugString("  dwVersion: 0x%x\r\n",  g_pTOC->id[i].dwVersion);
    EdbgOutputDebugString("  dwSignature: 0x%x\r\n", g_pTOC->id[i].dwSignature);
    EdbgOutputDebugString("  String: '%s'\r\n", g_pTOC->id[i].ucString);
    EdbgOutputDebugString("  dwImageType: 0x%x\r\n", g_pTOC->id[i].dwImageType);
    EdbgOutputDebugString("  dwTtlSectors: 0x%x\r\n", g_pTOC->id[i].dwTtlSectors);
    EdbgOutputDebugString("  dwLoadAddress: 0x%x\r\n", g_pTOC->id[i].dwLoadAddress);
    EdbgOutputDebugString("  dwJumpAddress: 0x%x\r\n", g_pTOC->id[i].dwJumpAddress);
    EdbgOutputDebugString("  dwStoreOffset: 0x%x\r\n", g_pTOC->id[i].dwStoreOffset);
    for (j = 0; j < MAX_SG_SECTORS; j++) {
        if ( !g_pTOC->id[i].sgList[j].dwLength )
            break;
        EdbgOutputDebugString("  sgList[%u].dwSector: 0x%x\r\n", j, g_pTOC->id[i].sgList[j].dwSector);
        EdbgOutputDebugString("  sgList[%u].dwLength: 0x%x\r\n", j, g_pTOC->id[i].sgList[j].dwLength);
    }

    EdbgOutputDebugString("}\r\n");
}

void TOC_Print(void)
{
    UINT32 i;

    EdbgOutputDebugString("TOC {\r\n");
    EdbgOutputDebugString("dwSignature: 0x%x\r\n", g_pTOC->dwSignature);

    BootConfigPrint( );

    for (i = 0; i < MAX_TOC_DESCRIPTORS; i++) {
        if ( !VALID_IMAGE_DESCRIPTOR(&g_pTOC->id[i]) )
            break;

        ID_Print(i);
    }

    //  Print out Chain Information
    EdbgOutputDebugString("chainInfo.dwLoadAddress: 0X%X\r\n", g_pTOC->chainInfo.dwLoadAddress);
    EdbgOutputDebugString("chainInfo.dwFlashAddress: 0X%X\r\n", g_pTOC->chainInfo.dwFlashAddress);
    EdbgOutputDebugString("chainInfo.dwLength: 0X%X\r\n", g_pTOC->chainInfo.dwLength);

    EdbgOutputDebugString("}\r\n");
}

BOOL TOC_Read(void)
{

    UINT32 i,j,dwTimeout;
    UINT32 *BufferToFuse = (UINT32 *)g_pTOC;
    UINT32 dwCountBlock=0;

    //RETAILMSG(1,(TEXT("### HSMMC::READTOC MMC from 0x%x to 0x%x 0x%x\n"),TOCSTARTSECTOR,SECTOROFTOC,BufferToFuse));

    if (!SDHC_CLK_ON_OFF(1))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not on/ff clock in TOC_Read()\r\n"),g_strDrvName));
        goto TOC_READ_FAIL;
    }

    dwTimeout = 0;
    while (!SDHC_CHECK_TRASN_STATE())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] SD/MMC card state is not TRANS in TOC_READ()\r\n"),g_strDrvName));
            goto TOC_READ_FAIL;
        }
        LoopForDelay(500000);
    }

    SDHC_SET_BLOCK_SIZE(7, 512); // Maximum DMA Buffer Size, Block Size
    SDHC_SET_BLOCK_COUNT(SECTOROFTOC); // Block Numbers to Write

    if (g_dwMMCSpec42 || g_dwIsSDHC)
        SDHC_SET_ARG(TOCSTARTSECTOR); // Card setctor Address to Write
    else
        SDHC_SET_ARG(TOCSTARTSECTOR*512); // Card byte Address to Write

    SDHC_SET_TRANS_MODE(0, 1, 0, 1, 0);
    SDHC_SET_CMD(17, 0);

    dwTimeout = 0;
    while (!SDHC_WAIT_CMD_COMPLETE())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] No CMD Complete signal in TOC_READ()\r\n"),g_strDrvName));
            goto TOC_READ_FAIL;
        }
    }

    dwTimeout = 0;
    while (!SDHC_CLEAR_CMD_COMPLETE())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not CMD Complete signal bit in TOC_READ()\r\n"),g_strDrvName));
            goto TOC_READ_FAIL;
        }
    }

    for(j=0; j<SECTOROFTOC; j++)
    {
        dwTimeout = 0;
        while (!SDHC_WAIT_READ_READY())
        {
            if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
            {
                RETAILMSG(1,(TEXT("[%s:ERR] Not buffer ready in TOC_READ()\r\n"),g_strDrvName));
                goto TOC_READ_FAIL;
            }
        }

        dwTimeout = 0;
        while (!SDHC_CLEAR_READ_READY())
        {
            if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
            {
                RETAILMSG(1,(TEXT("[%s:ERR] Not clear buffer ready bit in TOC_READ()\r\n"),g_strDrvName));
                goto TOC_READ_FAIL;
            }
        }

        for(i=0; i<SECTOR_SIZE/4; i++)
        {
            *BufferToFuse++ = g_vpHSMMCReg->BDATA;
            dwCountBlock++;
        }
    }

    dwTimeout = 0;
    while(!SDHC_WAIT_TRANS_COMPLETE())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not clear buffer ready bit in TOC_READ()\r\n"),g_strDrvName));
            goto TOC_READ_FAIL;
        }
    }

    dwTimeout = 0;
    while(!SDHC_CLEAR_TRANS_COMPLETE())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not clear buffer ready bit in TOC_READ()\r\n"),g_strDrvName));
            goto TOC_READ_FAIL;
        }
    }


    // is it a valid TOC?
    if ( !VALID_TOC(g_pTOC) ) {
        EdbgOutputDebugString("TOC_Read ERROR: INVALID_TOC Signature: 0x%x\r\n", g_pTOC->dwSignature);
        return FALSE;
    }

    // update our boot config
    g_pBootCfg = &g_pTOC->BootCfg;

    // update our index
    g_dwTocEntry = g_pBootCfg->ImageIndex;

    // debugger enabled?
 //   g_bWaitForConnect = (g_pBootCfg->ConfigFlags & CONFIG_FLAGS_KITL) ? TRUE : FALSE;

    // cache image type
    g_ImageType = g_pTOC->id[g_dwTocEntry].dwImageType;

    if (!SDHC_CLK_ON_OFF(0))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not on/ff clock in TOC_Read() 2\r\n"),g_strDrvName));
        goto TOC_READ_FAIL;
    }

    return TRUE;

TOC_READ_FAIL:

    if (!SDHC_CLK_ON_OFF(0))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not on/ff clock in TOC_Read() 2\r\n"),g_strDrvName));
    }
    return FALSE;


}

BOOL TOC_Init(DWORD dwEntry, DWORD dwImageType, DWORD dwImageStart, DWORD dwImageLength, DWORD dwLaunchAddr)
{
    DWORD dwSig = 0;

    EdbgOutputDebugString("TOC_Init: dwEntry:%u, dwImageType: 0x%x, dwImageStart: 0x%x, dwImageLength: 0x%x, dwLaunchAddr: 0x%x\r\n",
        dwEntry, dwImageType, dwImageStart, dwImageLength, dwLaunchAddr);

    if (0 == dwEntry) {
        EdbgOutputDebugString("\r\n*** WARNING: TOC_Init blasting Eboot ***\r\n");
        return FALSE;
    }

    switch (dwImageType) {
        case IMAGE_TYPE_LOADER:
            dwSig = IMAGE_EBOOT_SIG;
            break;
        case IMAGE_TYPE_RAMIMAGE:
            dwSig = IMAGE_RAM_SIG;
            break;
        default:
            EdbgOutputDebugString("ERROR: OEMLaunch: unknown image type: 0x%x \r\n", dwImageType);
            return FALSE;
    }

    memset(g_pTOC, 0, sizeof(g_TOC));

    // init boof cfg
 //   BootConfigInit(dwEntry);

    g_pBootCfg = &g_pTOC->BootCfg;

    memset(g_pBootCfg, 0, sizeof(BOOT_CFG));

    g_pBootCfg->ImageIndex   = dwEntry;

    g_pBootCfg->ConfigFlags  = BOOT_TYPE_MULTISTAGE | CONFIG_FLAGS_KITL;

    g_pBootCfg->BootDelay    = CONFIG_BOOTDELAY_DEFAULT;

    g_pBootCfg->SubnetMask = inet_addr("255.255.255.0");

    // update our index
    g_dwTocEntry = dwEntry;

    // debugger enabled?
    //g_bWaitForConnect = (g_pBootCfg->ConfigFlags & CONFIG_FLAGS_KITL) ? TRUE : FALSE;

    // init TOC...
    //
    g_pTOC->dwSignature = TOC_SIGNATURE;

    //  init TOC entry for Eboot
    //  Those are hard coded numbers from boot.bib
    g_pTOC->id[0].dwVersion     = (EBOOT_VERSION_MAJOR << 16) | EBOOT_VERSION_MINOR;
    g_pTOC->id[0].dwSignature   = IMAGE_EBOOT_SIG;
    memcpy(g_pTOC->id[0].ucString, "eboot.nb0", sizeof("eboot.nb0")+1);   //  NUll terminate
    g_pTOC->id[0].dwImageType   = IMAGE_TYPE_RAMIMAGE;
    g_pTOC->id[0].dwLoadAddress = EBOOT_RAM_IMAGE_BASE;
    g_pTOC->id[0].dwJumpAddress = EBOOT_RAM_IMAGE_BASE;
    g_pTOC->id[0].dwTtlSectors  = 0;
    // 1 contigious segment
    g_pTOC->id[0].sgList[0].dwSector = 0;
    g_pTOC->id[0].sgList[0].dwLength = g_pTOC->id[0].dwTtlSectors;

    // init the TOC entry
    g_pTOC->id[dwEntry].dwVersion     = 0x001;
    g_pTOC->id[dwEntry].dwSignature   = dwSig;
    memset(g_pTOC->id[dwEntry].ucString, 0, IMAGE_STRING_LEN);
    g_pTOC->id[dwEntry].dwImageType   = dwImageType;
    g_pTOC->id[dwEntry].dwLoadAddress = dwImageStart;
    g_pTOC->id[dwEntry].dwJumpAddress = dwLaunchAddr;
    g_pTOC->id[dwEntry].dwStoreOffset = 0;
    g_pTOC->id[dwEntry].dwTtlSectors  = 0;
    // 1 contigious segment
    g_pTOC->id[dwEntry].sgList[0].dwSector = IMAGESTARTSECTOR;
    g_pTOC->id[dwEntry].sgList[0].dwLength = g_pTOC->id[dwEntry].dwTtlSectors;

    TOC_Print();

    return TRUE;
}

BOOL TOC_Write(void)
{
    UINT32 i,j,dwTimeout;
    UINT32 *BufferToFuse = (UINT32 *)g_pTOC;
    UINT32 dwCountBlock=0;
    UCHAR ucTOC[512];

    if (!SDHC_CLK_ON_OFF(1))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not on/ff clock in TOC_Write()\r\n"),g_strDrvName));
        goto TOC_WRITE_FAIL;
    }

    //RETAILMSG(1,(TEXT("### HSMMC::WRITETOC MMC from 0x%x to 0x%x 0x%x ga 0x%x\n"),TOCSTARTSECTOR,SECTOROFTOC,BufferToFuse,g_dwMMCSpec42));

    if ( !VALID_TOC(g_pTOC) ) {
        EdbgOutputDebugString("TOC_Write ERROR: INVALID_TOC Signature: 0x%x\r\n", g_pTOC->dwSignature);
        return FALSE;
    }

    // is it a valid image descriptor?
    if ( !VALID_IMAGE_DESCRIPTOR(&g_pTOC->id[g_dwTocEntry]) ) {
        EdbgOutputDebugString("TOC_Write ERROR: INVALID_IMAGE[%u] Signature: 0x%x\r\n",
            g_dwTocEntry, g_pTOC->id[g_dwTocEntry].dwSignature);
        return FALSE;
    }

    dwTimeout = 0;
    while (!SDHC_CHECK_TRASN_STATE())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] SD/MMC card state is not TRANS in TOC_Write()\r\n"),g_strDrvName));
            goto TOC_WRITE_FAIL;
        }
        LoopForDelay(500000);
    }

    SDHC_SET_BLOCK_SIZE(7, 512); // Maximum DMA Buffer Size, Block Size
    SDHC_SET_BLOCK_COUNT(SECTOROFTOC); // Block Numbers to Write

    if (g_dwMMCSpec42 || g_dwIsSDHC)
        SDHC_SET_ARG(TOCSTARTSECTOR); // Card Address to Write
    else
        SDHC_SET_ARG(TOCSTARTSECTOR*512); // Card Address to Write

    SDHC_SET_TRANS_MODE(0, 0, 0, 1, 0);
    SDHC_SET_CMD(24, 0);

    dwTimeout = 0;
    while (!SDHC_WAIT_CMD_COMPLETE())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] No CMD Complete signal in TOC_Write()\r\n"),g_strDrvName));
            goto TOC_WRITE_FAIL;
        }
    }

    dwTimeout = 0;
    while (!SDHC_CLEAR_CMD_COMPLETE())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not CMD Complete signal bit in TOC_Write()\r\n"),g_strDrvName));
            goto TOC_WRITE_FAIL;
        }
    }

    for(j=0; j<SECTOROFTOC; j++)
    {
        dwTimeout = 0;
        while (!SDHC_WAIT_WRITE_READY())
        {
            if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
            {
                RETAILMSG(1,(TEXT("[%s:ERR] No buffer ready signal in TOC_Write()\r\n"),g_strDrvName));
                goto TOC_WRITE_FAIL;
            }
        }
        dwTimeout = 0;
        while (!SDHC_CLEAR_WRITE_READY())
        {
            if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
            {
                RETAILMSG(1,(TEXT("[%s:ERR] Not clear buffer ready signal in TOC_Write()\r\n"),g_strDrvName));
                goto TOC_WRITE_FAIL;
            }
        }

        for(i=0; i<SECTOR_SIZE/4; i++)//512 byte should be writed.
        {
            g_vpHSMMCReg->BDATA = *(UINT32 *)BufferToFuse++;
            dwCountBlock++;
        }
    }

    dwTimeout = 0;
    while(!SDHC_WAIT_TRANS_COMPLETE())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] No trasn complete signal in TOC_Write()\r\n"),g_strDrvName));
            goto TOC_WRITE_FAIL;
        }
    }

    dwTimeout = 0;
    while(!SDHC_CLEAR_TRANS_COMPLETE())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not clear trasn complete signal in TOC_WRITE()\r\n"),g_strDrvName));
            goto TOC_WRITE_FAIL;
        }
    }

    // setup our metadata so filesys won't stomp us
    // write the sector & metadata

    memcpy ( &ucTOC , &g_TOC , 512 );
    TOC_Read();

    if ( 0 != memcmp(&g_TOC, &ucTOC, 512) ) {
        EdbgOutputDebugString("TOC_Write ERROR: TOC verify failed\r\n");
        return FALSE;
    }


    if (!SDHC_CLK_ON_OFF(0))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not on/ff clock in TOC_Write() 2\r\n"),g_strDrvName));
        goto TOC_WRITE_FAIL;
    }
    return TRUE;

TOC_WRITE_FAIL:

    if (!SDHC_CLK_ON_OFF(0))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not on/ff clock in TOC_Write() 3\r\n"),g_strDrvName));
    }
    return FALSE;

}
#endif


BOOL SDHC_INIT(void)
{
#ifdef FOR_HSMMCDRV
    UINT32 dwIRQ;
#endif
    UINT32 dwMemoryAccess;
    PHYSICAL_ADDRESS ioPhysicalBase = {0,0};

    RETAILMSG(1, (TEXT("++SDHC_INIT\n")));
    g_dwMMCSpec42 = FALSE;
    g_dwCmdRetryCount = 0;
    g_dwIsMMC = FALSE;
    g_dwRCA=0;
    g_dwSDSPEC = 0;
    g_dwISHihgSpeed = FALSE;
    g_dwIsSDHC = FALSE;
    g_dwIsSDSPEC20 = TRUE;


    g_pEXTCSDRegister = &___EXTCSDRegister;


    if ( g_dwMemInit != TRUE )
    {
#ifdef FOR_EBOOT
        dwMemoryAccess = TRUE;
#elif defined(FOR_IPL)
        dwMemoryAccess = TRUE;
#elif defined(FOR_OAL)
        dwMemoryAccess = TRUE;
#else
        dwMemoryAccess = FALSE;
#endif

        if ( dwMemoryAccess )
        {
            if (g_UseChannel0)  // Use channel0
            {
                g_vpHSMMCReg = (S3C6410_HSMMC_REG *)OALPAtoVA(S3C6410_BASE_REG_PA_HSMMC0, FALSE);
            }
            else                // Use channel1
            {
                g_vpHSMMCReg = (S3C6410_HSMMC_REG *)OALPAtoVA(S3C6410_BASE_REG_PA_HSMMC1, FALSE);
            }
            g_vpSYSCONReg = (S3C6410_SYSCON_REG *)OALPAtoVA(S3C6410_BASE_REG_PA_SYSCON, FALSE);
            g_vpIOPortReg = (S3C6410_GPIO_REG *)OALPAtoVA(S3C6410_BASE_REG_PA_GPIO, FALSE);
        }
        else
        {
            if (g_UseChannel0)  // Use channel0
            {
                ioPhysicalBase.LowPart = S3C6410_BASE_REG_PA_HSMMC0;
            }
            else                // Use channel1
            {
                ioPhysicalBase.LowPart = S3C6410_BASE_REG_PA_HSMMC1;
            }
            g_vpHSMMCReg = (volatile S3C6410_HSMMC_REG *)MmMapIoSpace(ioPhysicalBase, sizeof(S3C6410_HSMMC_REG), FALSE);
            if (g_vpHSMMCReg == NULL)
            {
                RETAILMSG (1,(TEXT("[%s:ERR] SDHC_INIT() HSMMC registers not mapped\r\n"),g_strDrvName));
                return FALSE;
            }

            ioPhysicalBase.LowPart = S3C6410_BASE_REG_PA_SYSCON;
            g_vpSYSCONReg = (volatile S3C6410_SYSCON_REG *)MmMapIoSpace(ioPhysicalBase, sizeof(S3C6410_SYSCON_REG), FALSE);
            if (g_vpSYSCONReg == NULL)
            {
                RETAILMSG (1,(TEXT("[%s:ERR] SDHC_INIT() SYSCON registers not mapped\r\n"),g_strDrvName));
                return FALSE;
            }

            ioPhysicalBase.LowPart = S3C6410_BASE_REG_PA_GPIO;
            g_vpIOPortReg = (volatile S3C6410_GPIO_REG *)MmMapIoSpace(ioPhysicalBase, sizeof(S3C6410_GPIO_REG), FALSE);
            if (g_vpIOPortReg == NULL)
            {
                RETAILMSG (1,(TEXT("[%s:ERR] SDHC_INIT() HSMMC registers not mapped\r\n"),g_strDrvName));
                return FALSE;
            }

#ifdef FOR_HSMMCDRV
            if (g_UseChannel0)  // Use channel0
            {
                dwIRQ = IRQ_HSMMC0;
            }
            else                // Use channel1
            {
                dwIRQ = IRQ_HSMMC1;
            }
            g_hSDDMADoneEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
            if(NULL == g_hSDDMADoneEvent)
            {
                RETAILMSG(1,(_T("[%s:ERR] SDHC_INIT() : CreateEvent() SDDMAEvent failed. \n\r"),g_strDrvName));
                return FALSE;
            }
            // For HSMMC driver request OAL a SYSINTR instead of ststic SYSINTR mapping, Below KernelIoControl function is called.
            if (!KernelIoControl(IOCTL_HAL_REQUEST_SYSINTR, &dwIRQ, sizeof(DWORD), &(g_dwSysintr), sizeof(DWORD), NULL))
            {
                RETAILMSG(1, (TEXT("%d IOCTL_HAL_REQUEST_SYSINTR HSMMC Failed \n\r"), dwIRQ));
                return FALSE;
            }

            if (!(InterruptInitialize(g_dwSysintr, g_hSDDMADoneEvent, 0, 0)))
            {
                RETAILMSG(1,(_T("[%s:ERR] SDHC_INIT() : InterruptInitialize() SDDMAEvent failed. \n\r"),g_strDrvName));
                return FALSE;
            }
#endif
        }
    }

    g_dwMemInit = TRUE;
    // SDMMC controller reset
    SDHC_SET_SW_RESET(0x7);

    // init H/W
    SDHC_CLK_INIT();
    SDHC_GPIO_INIT();
    SDHC_SDMMC_CONTROLLER_INIT(SD_CLK_SRC);

    // diable interrupt signal.
    g_vpHSMMCReg->NORINTSIGEN = 0x0;
    g_vpHSMMCReg->ERRINTSIGEN = 0x0;

    // clock off
    if (!SDHC_CLK_ON_OFF(0))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Clock is not stable in SDHC_CLK_ON_OFF()\r\n"),g_strDrvName));
        return FALSE;
    }

    if (!SDHC_SET_CLK(0xff))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Clock is not stable in SDHC_SET_CLK()\r\n"),g_strDrvName));
        return FALSE;
    }

    // set command timout interval as the maximum value
    g_vpHSMMCReg->TIMEOUTCON = 0xe;

    if (!SDHC_ENABLE_INTR_STS(0xf7, 0x1ff))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not enable interupt in SDHC_ENABLE_INTR_STS()\r\n"),g_strDrvName));
        return FALSE;
    }

    if (!SDHC_ISSUE_CMD(0,0,0))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not issue cmd in SDHC_INIT()\r\n"),g_strDrvName));
        return FALSE;
    }

    g_dwWhileOCRCheck = 1;

    // to detect spec 2.0 SD card.
    while ( g_dwCmdRetryCount <= 10 )
    {
        if (SDHC_ISSUE_CMD(8,(0x1<<8)|(0xaa),0)) // only 2.0 card can accept this cmd.
            break;

        if(!SDHC_CLEAR_INTR_STS())
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not clear intr in SDHC_INIT()\r\n"),g_strDrvName));
            return FALSE;
        }
    }
    if ( g_dwCmdRetryCount <= 10 )
    {
        if (g_dwIsSDSPEC20)
            RETAILMSG(1,(TEXT("[%s:INF]This SD card is made on SPEC 2.0\n"),g_strDrvName));
        else
        {
            SDHC_ISSUE_CMD(0,0,0);//Idle State
            RETAILMSG(1,(TEXT("[%s:INF]This SD card is made on SPEC 1.x\n"),g_strDrvName));
        }
    }
    g_dwCmdRetryCount = 0;

    if ( g_dwIsSDSPEC20 )
    {
        g_dwIsMMC = SD_CARD;
        if (!SDHC_SD_INIT())
        {
            RETAILMSG(1,(TEXT("[%s:ERR} error returned from SDHC_SD_INIT() \r\n"),g_strDrvName));
            return FALSE;
        }
    }
    else
    {
        while ( g_dwCmdRetryCount <= 10 )
        {
            if (SDHC_ISSUE_CMD(1, (1<<30)|0xff8000, 0)) // only MMC card can accept this cmd.
                break;

            if(!SDHC_CLEAR_INTR_STS())
            {
                RETAILMSG(1,(TEXT("[%s:ERR] Not clear intr in SDHC_INIT() 2\r\n"),g_strDrvName));
                return FALSE;
            }
        }
        if (g_dwCmdRetryCount <= 10)
            g_dwIsMMC = MMC_CARD ;
        else
            g_dwIsMMC = SD_CARD;

        if ( g_dwIsMMC == MMC_CARD )
        {
            if(!SDHC_MMC_INIT())
            {
                RETAILMSG(1,(TEXT("[%s:ERR] error returned from SDHC_MMC_INIT() \r\n"),g_strDrvName));
                return FALSE;
            }
        }
        else
        {
            if(!SDHC_SD_INIT())
            {
                RETAILMSG(1,(TEXT("[%s:ERR] error returned from SDHC_SD_INIT() 2\r\n"),g_strDrvName));
                return FALSE;
            }
        }
    }

    SDHC_CLK_ON_OFF(1);
    RETAILMSG(1, (TEXT("--SDHC_INIT\n")));

    return TRUE;
}

UINT32 SDHC_MMC_INIT()
{
    UINT32 dwSfr,dwTimeout;


    if (!SDHC_ISSUE_CMD(0,0,0))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not issue cmd in SDHC_INIT()\r\n"),g_strDrvName));
        return FALSE;
    }

    dwTimeout = 0;
    while (dwTimeout++ < LOOP_FOR_TIMEOUT)
    {
        if (!SDHC_ISSUE_CMD(1, (1<<30)|0xff8000, 0))
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not issue cmd in SDHC_MMC_INIT() 1\r\n"),g_strDrvName));
            return FALSE;
        }

        dwSfr = g_vpHSMMCReg->RSPREG0;

        if ( dwSfr & (1<<30) )
        {
            RETAILMSG(1,(TEXT("[%s:INF] This MMC is made on SPEC 4.2\n"),g_strDrvName));
            g_dwMMCSpec42 = TRUE;
        }

        if ( dwSfr & (1<<31))
        {
             break;
        }
        LoopForDelay(0x100000);
    }

    g_dwWhileOCRCheck = 0;

    if(!SDHC_CLEAR_INTR_STS())
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not clear intr in SDHC_MMC_INIT()\r\n"),g_strDrvName));
        return FALSE;
    }

    if (!SDHC_ISSUE_CMD(2,0,0))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not issue cmd in SDHC_MMC_INIT() 2\r\n"),g_strDrvName));
        return FALSE;
    }

    if (!SDHC_ISSUE_CMD(3,0x0001,0))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not issue cmd in SDHC_MMC_INIT() 3\r\n"),g_strDrvName));
        return FALSE;
    }

    if (!SDHC_ISSUE_CMD(9, 0x0001, 0))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not issue cmd in SDHC_MMC_INIT() 4\r\n"),g_strDrvName));
        return FALSE;
    }

    SDHC_GET_CARD_SIZE();

    if (!SDHC_ISSUE_CMD(7, 0x0001, 0))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not issue cmd in SDHC_MMC_INIT() 4\r\n"),g_strDrvName));
        return FALSE;
    }

    if (!SDHC_SET_BUS_WIDTH(1))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not set Data bus width in SDHC_SD_INIT()\r\n"),g_strDrvName));
        return FALSE;
    }

    if (!SDHC_GET_EXTCSD())
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not set Data bus width in SDHC_SD_INIT()\r\n"),g_strDrvName));
        return FALSE;
    }

    if ( !SDHC_CONFIG_CLK((g_dwISHihgSpeed==TRUE) ? 1:2)) // Divisor 1 = Base clk /2    ,Divisor 2 = Base clk /4, Divisor 4 = Base clk /8 ...
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not set SDHC_CONFIG_CLK in SDHC_CONFIG_CLK()\r\n"),g_strDrvName));
        return FALSE;
    }

    if (!SDHC_SET_BUS_WIDTH(HSMMC_DATA_BUSWIDTH))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not set Data bus width in SDHC_SD_INIT()\r\n"),g_strDrvName));
        return FALSE;
    }

    LoopForDelay(0x100000);

    if (!SDHC_CHECK_TRASN_STATE())
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not trans sd card state\r\n"),g_strDrvName));
        return FALSE;
    }

    if (!SDHC_ISSUE_CMD(16, 512, 0))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not issue cmd in SDHC_SD_INIT() 3\r\n"),g_strDrvName));
        return FALSE;
    }

    g_vpHSMMCReg->NORINTSTS = 0xffff;

    return TRUE;
}

UINT32 SDHC_SD_INIT()
{
    if (!SDHC_OCR_CHECK())
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not OCR ready bit ON in SDHC_SD_INIT()\r\n"),g_strDrvName));
        return FALSE;
    }

    // delay for next cmd.
    LoopForDelay(0x100000);

    // finish checking OCR
    g_dwWhileOCRCheck = 0;

    if(!SDHC_CLEAR_INTR_STS())
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not clear intr in SDHC_SD_INIT()\r\n"),g_strDrvName));
        return FALSE;
    }


    if (!SDHC_ISSUE_CMD(2,0,0))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not issue cmd in SDHC_SD_INIT() 1\r\n"),g_strDrvName));
        return FALSE;
    }

    // Send RCA(Relative Card Address). It places the card to the STBY state

    if (!SDHC_ISSUE_CMD(3,0x0000,0))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not issue cmd in SDHC_SD_INIT() 2\r\n"),g_strDrvName));
        return FALSE;
    }

    g_dwRCA = ((g_vpHSMMCReg->RSPREG0)>>16)&0xFFFF; // get RCA

    if (!SDHC_ISSUE_CMD(9, g_dwRCA, 0))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not issue cmd in SDHC_SD_INIT() 3\r\n"),g_strDrvName));
        return FALSE;
    }

    // by CSD information, we can measure the size of SD card.
    SDHC_GET_CARD_SIZE();

    if (!SDHC_ISSUE_CMD(7, g_dwRCA, 0))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not issue cmd in SDHC_SD_INIT() 3\r\n"),g_strDrvName));
        return FALSE;
    }

    if (!SDHC_SET_BUS_WIDTH(1))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not set Data bus width in SDHC_SD_INIT()\r\n"),g_strDrvName));
        return FALSE;
    }

    if (!SDHC_GET_SCR())
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not get SCR register in SDHC_GET_SCR()\r\n"),g_strDrvName));
        return FALSE;
    }

    if ( g_dwSDSPEC >= 1)
    {
        if (!SDHC_SD_SPEED_CHECK())
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not get SCR register in SDHC_GET_SCR()\r\n"),g_strDrvName));
            return FALSE;
        }
    }

    if (!SDHC_CONFIG_CLK((g_dwISHihgSpeed==TRUE) ? 1:2)) // Divisor 1 = Base clk /2    ,Divisor 2 = Base clk /4, Divisor 4 = Base clk /8 ...
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not set SDHC_CONFIG_CLK in SDHC_CONFIG_CLK()\r\n"),g_strDrvName));
        return FALSE;
    }

    if (!SDHC_SET_BUS_WIDTH(SD_DATA_BUSWIDTH))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not set Data bus width in SDHC_SD_INIT() 2\r\n"),g_strDrvName));
        return FALSE;
    }

    if (!SDHC_CHECK_TRASN_STATE())
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not trans sd card state\r\n"),g_strDrvName));
        return FALSE;
    }

    if (!SDHC_ISSUE_CMD(16, 512, 0))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not issue cmd in SDHC_SD_INIT() 3\r\n"),g_strDrvName));
        return FALSE;
    }

    g_vpHSMMCReg->NORINTSTS = 0xffff;

    return TRUE;
}

UINT32 SDHC_OCR_CHECK(void)
{
    UINT32 dwSfr,i;

    for(i=0; i<0x10000; i++)
    {
        // CMD55 (For ACMD)
        if (!SDHC_ISSUE_CMD(55, 0x0, FALSE))
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not issue cmd in SDHC_OCR_CHECK()\r\n"),g_strDrvName));
            return FALSE;
        }

        // (Ocr:2.7V~3.6V)
        if (!SDHC_ISSUE_CMD(41, 0xc0ff8000, TRUE ))
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not issue cmd in SDHC_OCR_CHECK()\r\n"),g_strDrvName));
            return FALSE;
        }
        dwSfr = g_vpHSMMCReg->RSPREG0;

        if ( dwSfr&((UINT32)(0x1<<31)))
        {
            if(dwSfr&((UINT32)(0x1<<30))) {
                RETAILMSG(1,(TEXT("This SD card is High Capacity\r\n"),g_strDrvName));
                g_dwIsSDHC = TRUE;
            }
            else
                g_dwIsSDHC = FALSE;

            if (!SDHC_CLEAR_INTR_STS())
            {
                RETAILMSG(1,(TEXT("[%s:ERR] Not clear intr in SDHC_OCR_CHECK()\r\n"),g_strDrvName));
                return FALSE;
            }
            return TRUE;
         }
      }
    return FALSE;
}

UINT32 SDHC_SD_SPEED_CHECK(void)
{
    UINT32 dwBuffer[16];
    UINT32 dwArg = 0, dwTimeout;
    UINT32 i;


    if (!SDHC_ISSUE_CMD(16, 64, 0))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not issue cmd in SDHC_SD_SPEED_CHECK() 1\r\n"),g_strDrvName));
        return FALSE;
    }

    SDHC_SET_BLOCK_SIZE(7, 64); // Maximum DMA dwBuffer Size, Block Size
    SDHC_SET_BLOCK_COUNT(1); // Block Numbers to Write
    SDHC_SET_TRANS_MODE(0, 1, 0, 0, 0);

    dwArg = (0x1<<31)|(0xffff<<8)|(1);

    if (!SDHC_ISSUE_CMD(6, dwArg, FALSE))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not issue cmd in SDHC_SD_SPEED_CHECK() 2\r\n"),g_strDrvName));
        return FALSE;
    }

    dwTimeout = 0;
    while (!SDHC_WAIT_READ_READY())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] No read ready signal in SDHC_SD_SPEED_CHECK()\r\n"),g_strDrvName));
            return FALSE;
        }
    }

    dwTimeout = 0;
    while (!SDHC_CLEAR_READ_READY())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not clear read ready signal in SDHC_SD_SPEED_CHECK()\r\n"),g_strDrvName));
            return FALSE;
        }
    }

    for(i=0; i<64/4; i++)
    {
        dwBuffer[i] = g_vpHSMMCReg->BDATA;
    }

    dwTimeout = 0;
    while(!SDHC_WAIT_TRANS_COMPLETE())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not clear bBuffer ready bit in SDHC_SD_SPEED_CHECK()\r\n"),g_strDrvName));
            return FALSE;
        }
    }

    dwTimeout = 0;
    while(!SDHC_CLEAR_TRANS_COMPLETE())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not clear bBuffer ready bit in SDHC_SD_SPEED_CHECK()\r\n"),g_strDrvName));
            return FALSE;
        }
    }

    if ( dwBuffer[3] & (1<<9) )
    { // Function Group 1 <- access mode.
        RETAILMSG(1,(TEXT( "This Media support high speed mode.\n" ),g_strDrvName));
        g_dwISHihgSpeed = TRUE;
    }
    else
    {
        RETAILMSG(1,(TEXT( "This Media can't support high speed mode.\n" ),g_strDrvName));
        g_dwISHihgSpeed = FALSE;
    }

    return TRUE;
}

UINT32 SDHC_GET_SCR(void)
{
    UINT32 i,dwTimeout;
    UINT32 bBuffer[2];

    if(!SDHC_ISSUE_CMD(16, 8, 0))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not issue cmd in SDHC_GET_SCR()\r\n"),g_strDrvName));
        return FALSE;
    }

    SDHC_SET_BLOCK_SIZE(7, 8); // Maximum DMA bBuffer Size, Block Size
    SDHC_SET_BLOCK_COUNT(1); // Block Numbers to Write
    SDHC_SET_TRANS_MODE(0, 1, 0, 0, 0);

    if(!SDHC_ISSUE_CMD(55, g_dwRCA, FALSE))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not issue cmd in SDHC_GET_SCR() 2\r\n"),g_strDrvName));
        return FALSE;
    }

    if(!SDHC_ISSUE_CMD(51, 0, TRUE))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not issue cmd in SDHC_GET_SCR() 3\r\n"),g_strDrvName));
        return FALSE;
    }


    dwTimeout = 0;
    while (!SDHC_WAIT_READ_READY())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] No read ready signall in SDHC_GET_SCR()\r\n"),g_strDrvName));
            return FALSE;
        }
    }

    dwTimeout = 0;
    while (!SDHC_CLEAR_READ_READY())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not clear read ready signal in SDHC_GET_SCR()\r\n"),g_strDrvName));
            return FALSE;
        }
    }

    for(i=0; i<8/4; i++) // SCR register is 64bit.
    {
        bBuffer[i] = g_vpHSMMCReg->BDATA;
    }

    dwTimeout = 0;
    while(!SDHC_WAIT_TRANS_COMPLETE())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not clear bBuffer ready bit in SDHC_GET_SCR()\r\n"),g_strDrvName));
            return FALSE;
        }
    }

    dwTimeout = 0;
    while(!SDHC_CLEAR_TRANS_COMPLETE())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not clear bBuffer ready bit in SDHC_GET_SCR()\r\n"),g_strDrvName));
            return FALSE;
        }
    }

    // SD card can accept CMD6 from SD Spec 1.1
    if ((*bBuffer&0xf) == 0x0)
    {
        g_dwSDSPEC= 0; // Version 1.0 ~ 1.01
    }
    else if ((*bBuffer&0xf) == 0x1)
    {
        g_dwSDSPEC = 1; // Version 1.10, support cmd6
    }
    else if((*bBuffer&0xf) == 0x2)
    {
        g_dwSDSPEC = 2; // Version 2.0 support cmd6 and cmd8
    }
    else
    {
        g_dwSDSPEC = 0; // Error...
    }

    RETAILMSG(1,(TEXT("[%s:INF] SDSpecVer of This SD card\n"), g_strDrvName));

    return TRUE;
}

void SDHC_GPIO_INIT()
{
    RETAILMSG(_DBG_, (TEXT("[HSMMC] Setting registers for the GPIO.\n")));

    if (g_UseChannel0)  // Use channel0
    {
        g_vpIOPortReg->GPGCON  = (g_vpIOPortReg->GPGCON & ~(0xFFFFFF)) | (0x222222);  // 4'b0010 for the MMC 0
        g_vpIOPortReg->GPGPUD &= ~(0xFFF); // Pull-up/down disabled
    }
    else                // Use channel1
    {
        g_vpIOPortReg->GPHCON0 = (g_vpIOPortReg->GPHCON0 & ~(0xFFFFFF)) | (0x222222);  // 4'b0010 for the MMC 1
        g_vpIOPortReg->GPHPUD &= ~(0xFFF); // Pull-up/down disabled
    }
}

void SDHC_SDMMC_CONTROLLER_INIT(UINT32 dwClksrc)
{

    RETAILMSG(_DBG_, (TEXT("[HSMMC] Setting registers for the EPLL : HSMMCCon.\n")));

    g_vpHSMMCReg->CONTROL2 = (g_vpHSMMCReg->CONTROL2 & ~(0xffffffff)) |    // clear all
    //(0x1<<15) |    // enable Feedback clock for Tx Data/Command Clock
    (0x1<<14) |    // enable Feedback clock for Rx Data/Command Clock
    (0x3<<9)  |    // Debounce Filter Count 0x3=64 iSDCLK
    (0x1<<8)  |    // SDCLK Hold Enable
    (dwClksrc<<4);    // Base Clock Source = EPLL (from SYSCON block)
    g_vpHSMMCReg->CONTROL3 = (0<<31) | (1<<23) | (0<<15) | (1<<7);    // controlling the Feedback Clock
}

void SDHC_CLK_INIT()
{
    RETAILMSG(_DBG_, (TEXT("[HSMMC] Setting registers for the EPLL (for SDCLK) : SYSCon.\n")));

    // SCLK_HSMMC#  : EPLLout, MPLLout, PLL_source_clk or CLK27 clock
    // (from SYSCON block, can be selected by MMC#_SEL[1:0] fields of the CLK_SRC register in SYSCON block)
    // Set the clock source to EPLL out for CLKMMC1

    if (g_UseChannel0)  // Use channel0
    {
        g_vpSYSCONReg->CLK_SRC   = (g_vpSYSCONReg->CLK_SRC & ~((0x3<<18)|(0x1<<2))) |  // Control MUX(MMC0:MOUT EPLL)
            (0x1<<2); // Control MUX(EPLL:FOUT EPLL)
        g_vpSYSCONReg->HCLK_GATE = (g_vpSYSCONReg->HCLK_GATE) | (0x1<<17);  // Gating HCLK for HSMMC0
        g_vpSYSCONReg->SCLK_GATE = (g_vpSYSCONReg->SCLK_GATE) | (0x1<<24);  // Gating special clock for HSMMC0 (SCLK_MMC0)
    }
    else                // Use channel1
    {
        g_vpSYSCONReg->CLK_SRC   = (g_vpSYSCONReg->CLK_SRC & ~((0x3<<20)|(0x1<<2))) |  // Control MUX(MMC1:MOUT EPLL)
            (0x1<<2); // Control MUX(EPLL:FOUT EPLL)
        g_vpSYSCONReg->HCLK_GATE = (g_vpSYSCONReg->HCLK_GATE) | (0x1<<18);  // Gating HCLK for HSMMC1
        g_vpSYSCONReg->SCLK_GATE = (g_vpSYSCONReg->SCLK_GATE) | (0x1<<25);  // Gating special clock for HSMMC1 (SCLK_MMC1)
    }
}

UINT32 SDHC_CLK_ON_OFF(UINT32 OnOff)
{
    UINT32 dwTimeout;
    if (OnOff == 0)
    {
        g_vpHSMMCReg->CLKCON &=~(0x1<<2);
    }

    else
    {
        g_vpHSMMCReg->CONTROL4 |=  (0x3<<16);
        g_vpHSMMCReg->CLKCON |=(0x1<<2);

        dwTimeout = 0;
        while (!(g_vpHSMMCReg->CLKCON&(0x1<<3))) // clock is not table.
        {
            if (dwTimeout++ > LOOP_FOR_TIMEOUT)
            {
                return FALSE;
            }
        }
    }
    return TRUE;
}

UINT32 SDHC_SET_CLK(UINT16 swDivisor)
{
    UINT32 dwTimeout;
    g_vpHSMMCReg->CONTROL2 &= ~(0x3<<14) ; // Turn off the feedback dealy.
    g_vpHSMMCReg->CONTROL2 |= ( (1<<31) | (1<<30) );  //

    g_vpHSMMCReg->CLKCON &= ~(0xff<<8);


    // SDCLK Value Setting + Internal Clock Enable
    g_vpHSMMCReg->CLKCON = (g_vpHSMMCReg->CLKCON & ~((0xff<<8)|(0x1))) | (swDivisor<<8)|(1<<0);

    // CheckInternalClockStable
    dwTimeout = 0;
    while (!(g_vpHSMMCReg->CLKCON&0x2))
    {
        if (dwTimeout++ > LOOP_FOR_TIMEOUT)
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Internal Clock is not stable in SDHC_SET_CLK()\r\n"),g_strDrvName));
            return FALSE;
        }
    }

    if (!SDHC_CLK_ON_OFF(1))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Clock is not stable in SDHC_CLK_ON_OFF()\r\n"),g_strDrvName));
        return FALSE;
    }

    return TRUE;

}

void SDHC_SET_HOST_SPEED(UINT8 SpeedMode)
{
    UINT8  ucSpeedMode;
    ucSpeedMode = (SpeedMode == HIGH) ? 1 : 0;
    g_vpHSMMCReg->HOSTCTL &= ~(0x1<<2);
    g_vpHSMMCReg->HOSTCTL |= ucSpeedMode<<2;
}


UINT32 SDHC_ENABLE_INTR_STS(UINT16 NormalIntEn, UINT16 ErrorIntEn)
{
    if(!SDHC_CLEAR_INTR_STS())
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not clear intr in SDHC_ENABLE_INTR_STS()\r\n"),g_strDrvName));
        return FALSE;
    }
    g_vpHSMMCReg->NORINTSTSEN = NormalIntEn;
    g_vpHSMMCReg->ERRINTSTSEN = ErrorIntEn;

    return TRUE;
}


UINT32 SDHC_CLEAR_INTR_STS(void)
{
    UINT32 dwTimeout;
    g_vpHSMMCReg->ERRINTSTSEN = 0;
    g_vpHSMMCReg->NORINTSTSEN = 0;

    dwTimeout=0;
    while (g_vpHSMMCReg->NORINTSTS&(0x1<<15))
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT)
        {
            return FALSE;
        }
        g_vpHSMMCReg->NORINTSTS =g_vpHSMMCReg->NORINTSTS;
    }

    dwTimeout=0;
    while (    g_vpHSMMCReg->ERRINTSTS )
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT)
        {
            return FALSE;
        }
        g_vpHSMMCReg->ERRINTSTS =g_vpHSMMCReg->ERRINTSTS;
    }

    dwTimeout=0;
    while (    g_vpHSMMCReg->NORINTSTS )
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT)
        {
            return FALSE;
        }
        g_vpHSMMCReg->NORINTSTS = g_vpHSMMCReg->NORINTSTS;
    }

    g_vpHSMMCReg->ERRINTSTSEN = 0x1ff;
    g_vpHSMMCReg->NORINTSTSEN = 0xff;

    return TRUE;

}


UINT32 SDHC_ISSUE_CMD( UINT16 swCmd, UINT32 dwArg, UINT32 dwIsAcmd)
{
    UINT32 dwTimeout;

    dwTimeout = 0;
    while ((g_vpHSMMCReg->PRNSTS&0x1))
    {
        if (dwTimeout++ > LOOP_FOR_TIMEOUT)
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not clear CMD line\r\n"),g_strDrvName));
            SDHC_SET_SW_RESET(0x6);
            return FALSE;
        }
    }

    if (!dwIsAcmd)//R1b type commands have to check CommandInhibit_DAT bit
    {
        if((swCmd==6&&g_dwIsMMC)||swCmd==7||swCmd==12||swCmd==28||swCmd==29||swCmd==38||((swCmd==42||swCmd==56)&&(!g_dwIsMMC)))
        {
            dwTimeout = 0;
            while ((g_vpHSMMCReg->PRNSTS&0x2))
            {
                if (dwTimeout++ > LOOP_FOR_TIMEOUT)
                {
                    RETAILMSG(1,(TEXT("[%s:ERR] Not clear CMD line\r\n"),g_strDrvName));
                    SDHC_SET_SW_RESET(0x6);
                    return FALSE;
                }
            }
        }
    }

    // Argument Setting
    if (!dwIsAcmd)// <------------------- Normal Command (CMD)
    {
        if(swCmd==3||swCmd==4||swCmd==7||swCmd==9||swCmd==10||swCmd==13||swCmd==15||swCmd==55)
            g_vpHSMMCReg->ARGUMENT = dwArg<<16;
        else
            g_vpHSMMCReg->ARGUMENT = dwArg;
    }
    else// <--------------------------- APP.Commnad (ACMD)
        g_vpHSMMCReg->ARGUMENT = dwArg;

    SDHC_SET_CMD(swCmd,dwIsAcmd);


    dwTimeout = 0;
    while (!SDHC_WAIT_CMD_COMPLETE())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] No %d CMD Complete signal in SDHC_ISSUE_CMD()\r\n"),g_strDrvName,swCmd));
            return FALSE;
        }
    }

    dwTimeout = 0;
    while (!SDHC_CLEAR_CMD_COMPLETE())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not clear %d CMD Complete signal bit in SDHC_ISSUE_CMD()\r\n"),g_strDrvName,swCmd));
            return FALSE;
        }
    }

    if (!(g_vpHSMMCReg->NORINTSTS&0x8000))
    {
        if ( (swCmd == 8) && (g_dwWhileOCRCheck == 1))
        {
            RETAILMSG(1,(TEXT("#### this SD card is made on SPEC 2.0\r\n")));
        }
        return TRUE;
    }
    else
    {
        if(g_dwWhileOCRCheck == 1)
        {
            g_dwCmdRetryCount++;
            g_dwIsSDSPEC20 = FALSE;
            return FALSE;
        }
        else
        {
            RETAILMSG(1,(TEXT("[%s:ERR] SDHC_ISSUE_CMD() Command = %d, Error Stat = %x\n"),g_strDrvName,(g_vpHSMMCReg->CMDREG>>8),g_vpHSMMCReg->ERRINTSTS));
            if((g_vpHSMMCReg->CMDREG>>8) == 0x8)
            {
                RETAILMSG(1,(TEXT("\n[%s:INF] SDHC_ISSUE_CMD() This SD Card version is NOT 2.0\n"),g_strDrvName));
            }
            return FALSE;
        }
    }

    if (swCmd == 7)
    {
        SDHC_SET_SW_RESET(0x6);
    }
    return FALSE;

}


UINT32 SDHC_WAIT_CMD_COMPLETE(void)
{
    return (g_vpHSMMCReg->NORINTSTS&0x1);
}

UINT32 SDHC_WAIT_DATALINE_READY(void)
{
    return ((g_vpHSMMCReg->PRNSTS&0x2)) != 0x2;
}

UINT32 SDHC_WAIT_CMDLINE_READY(void)
{
    return ((g_vpHSMMCReg->PRNSTS&0x1) != 0x1);
}
void SDHC_SET_CMD(UINT16 swCmd,UINT32 dwIsAcmd)
{
    UINT16 swSfr = 0;

    if (!dwIsAcmd)//No ACMD
    {
        /* R2: 136-bits Resp.*/
        if (swCmd==2||swCmd==9||swCmd==10)
            swSfr=(swCmd<<8)|(0<<4)|(1<<3)|(1<<0);

        /* R1,R6,R5: 48-bits Resp. */
        else if (swCmd==3||swCmd==13||swCmd==16||swCmd==23||swCmd==27||swCmd==30||swCmd==32||swCmd==33||swCmd==35||swCmd==36||swCmd==42||swCmd==55||swCmd==56||((swCmd==8)&&(!g_dwIsMMC)))
            swSfr=(swCmd<<8)|(1<<4)|(1<<3)|(2<<0);

        else if ((swCmd==8 && g_dwIsMMC)||swCmd==11||swCmd==14||swCmd==17||swCmd==18||swCmd==19||swCmd==20||swCmd==24||swCmd==25)
            swSfr=(swCmd<<8)|(1<<5)|(1<<4)|(1<<3)|(2<<0);

        /* R1b,R5b: 48-bits Resp. */
        else if (swCmd==6||swCmd==7||swCmd==12||swCmd==28||swCmd==29||swCmd==38)
        {
            if (swCmd==12)
                swSfr=(swCmd<<8)|(3<<6)|(1<<4)|(1<<3)|(3<<0);
            else if (swCmd==6)
            {
                if(!g_dwIsMMC)    // SD card
                    swSfr=(swCmd<<8)|(1<<5)|(1<<4)|(1<<3)|(2<<0);
                else            // MMC card
                {
                    swSfr=(swCmd<<8)|(1<<4)|(1<<3)|(3<<0);
                    //swSfr=(swCmd<<8)|(1<<5)|(1<<4)|(1<<3)|(3<<0);
                }
            }
            else
                swSfr=(swCmd<<8)|(1<<4)|(1<<3)|(3<<0);
        }

        /* R3,R4: 48-bits Resp.  */
        else if (swCmd==1)
            swSfr=(swCmd<<8)|(0<<4)|(0<<3)|(2<<0);

        /* No-Resp. */
        else
            swSfr=(swCmd<<8)|(0<<4)|(0<<3)|(0<<0);
    }
    else//ACMD
    {
        if (swCmd==6||swCmd==22||swCmd==23)        // R1
            swSfr=(swCmd<<8)|(1<<4)|(1<<3)|(2<<0);
        else if (swCmd==13||swCmd==51)
            swSfr=(swCmd<<8)|(1<<5)|(1<<4)|(1<<3)|(2<<0);
        else
            swSfr=(swCmd<<8)|(0<<4)|(0<<3)|(2<<0);
    }
    g_vpHSMMCReg->CMDREG = swSfr;
}


UINT32 SDHC_CLEAR_CMD_COMPLETE(void)
{
    g_vpHSMMCReg->NORINTSTSEN &= ~(1<<0);

    g_vpHSMMCReg->NORINTSTS=(1<<0);
    if (g_vpHSMMCReg->NORINTSTS&0x1)
    {
        g_vpHSMMCReg->NORINTSTS=(1<<0);
    }
    g_vpHSMMCReg->NORINTSTSEN |= (1<<0);

    return ((g_vpHSMMCReg->NORINTSTS&0x1) != 0x1);
}


UINT32 SDHC_CONFIG_CLK(UINT32 dwDivisior)
{
    UINT32 dwSrcFreq, dwWorkingFreq;

    //RETAILMSG("Clock Config\n");

    if (SD_CLK_SRC == SD_HCLK)
        dwSrcFreq = S3C6410_HCLK;
    else if (SD_CLK_SRC == SD_EPLL) //Epll Out 84MHz
        dwSrcFreq = S3C6410_ECLK;
    else
        dwSrcFreq = 48000000;

    if ( dwDivisior == 0)
        dwWorkingFreq = dwSrcFreq;
    else
        dwWorkingFreq = dwSrcFreq/(dwDivisior*2);

    RETAILMSG(1,(TEXT("[%s:INF] SDHC_CONFIG_CLK() Card Working Frequency = %dMHz\n"),g_strDrvName,dwWorkingFreq/(1000000)));

    if (g_dwIsMMC)
    {
        if (dwWorkingFreq>=24000000)// It is necessary to enable the high speed mode in the card before changing the clock freq to a freq higher than 20MHz.
        {
            if (!SDHC_SET_MMC_SPEED(HIGH))
            {
                RETAILMSG(1,(TEXT("[%s:ERR] Not set MMC SpeedMode in SDHC_SET_MMC_SPEED() 1\r\n"),g_strDrvName));
                return FALSE;
            }
            RETAILMSG(1,(TEXT("[%s:INF] SDHC_CONFIG_CLK() Set MMC High speed mode OK!!\n"),g_strDrvName));
        }
        else
        {
            if(!SDHC_SET_MMC_SPEED(NORMAL))
            {
                RETAILMSG(1,(TEXT("[%s:ERR] Not set MMC SpeedMode in SDHC_SET_MMC_SPEED() 2\r\n"),g_strDrvName));
                return FALSE;
            }
            RETAILMSG(1,(TEXT("[%s:INF] SDHC_CONFIG_CLK() Set MMC Normal speed mode OK!!\n"),g_strDrvName));
        }
    }

    if (!SDHC_CLK_ON_OFF(0))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Clock is not stable in SDHC_CONFIG_CLK()\r\n"),g_strDrvName));
        return FALSE;
    }

    if (!SDHC_SET_CLK(dwDivisior))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Clock is not stable in SDHC_SET_CLK()\r\n"),g_strDrvName));
        return FALSE;
    }

    RETAILMSG(0,(TEXT("clock config g_vpHSMMCReg->HOSTCTL = %x\n"),g_vpHSMMCReg->HOSTCTL));

    return TRUE;

}

UINT32 SDHC_SET_BUS_WIDTH(UINT32 dwBusWidth)
{
    UINT8 bBitMode=0;
    UINT32 dwArg=0;
    UINT8 bHostCtrlReg = 0;
    UINT32 bBusWidth;


    switch (dwBusWidth)
    {
        case 8:
            bBusWidth = 8;
            break;
        case 4:
            bBusWidth = 4;
            break;
        case 1:
            bBusWidth = 1;
            break;
        default :
            bBusWidth = 1;
            break;
    }

    if(!g_dwIsMMC)// <------------------------- SD Card Case
    {
        if (!SDHC_ISSUE_CMD(55, g_dwRCA, 0))
        {
            return FALSE;
        }
        else
        {
            if (bBusWidth==1)
            {
                bBitMode = 0;
                if (!SDHC_ISSUE_CMD(6, 0, 1)) // 1-bits
                {
                    RETAILMSG(1,(TEXT("[%s:ERR] Not issue cmd in SDHC_SET_BUS_WIDTH() 1\r\n"),g_strDrvName));
                    return FALSE;
                }
            }
            else
            {
                bBitMode = 1;

                if (!SDHC_ISSUE_CMD(6, 2, 1)) // 4-bits
                {
                    RETAILMSG(1,(TEXT("[%s:ERR] Not issue cmd in SDHC_SET_BUS_WIDTH() 2\r\n"),g_strDrvName));
                    return FALSE;
                }
            }
        }
    }
    else // <-------------------------------- MMC Card Case
    {
        if (bBusWidth==1)
            bBitMode = 0;
        else if (bBusWidth==4)
        {
              RETAILMSG(1,(TEXT("[%s:INF] SDHC_SET_BUS_WIDTH() 4Bit Data Bus enables\n"),g_strDrvName));
            bBitMode = 1;//4                    // 4-bit bus
        }
        else
        {
            bBitMode = 2;//8-bit bus
            RETAILMSG(1,(TEXT("[%s:INF] SDHC_SET_BUS_WIDTH() 8Bit Data Bus enables\n"),g_strDrvName));
        }

        dwArg=((3<<24)|(183<<16)|(bBitMode<<8));
        if(!SDHC_ISSUE_CMD(6, dwArg, 0))
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not issue cmd in SDHC_SET_BUS_WIDTH() 3\r\n"),g_strDrvName));
            return FALSE;
        }
    }

    if (bBitMode==2)
    {
        bHostCtrlReg &= 0xfd;
        bHostCtrlReg |= 1<<5;
    }
    else
    {
        bHostCtrlReg &= 0xfd;
        bHostCtrlReg |= bBitMode<<1;
    }

    g_vpHSMMCReg->HOSTCTL = bHostCtrlReg;

    //SetSdhcCardIntEnable(1);
    //RETAILMSG(" transfer g_vpHSMMCReg->HOSTCTL = %x\n",g_vpHSMMCReg->HOSTCTL);
    return TRUE;
}

UINT32 SDHC_SET_MMC_SPEED(UINT32 dwSDSpeedMode)
{
    UINT8  ucSpeedMode;
    UINT32 dwArg=0;
    ucSpeedMode = (dwSDSpeedMode == HIGH) ? 1 : 0;
    dwArg=(3<<24)|(185<<16)|(ucSpeedMode<<8); // Change to the high-speed mode
    if(!SDHC_ISSUE_CMD(6, dwArg, 0))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not issue cmd in SDHC_SET_MMC_SPEED()\r\n"),g_strDrvName));
        return FALSE;
    }
    return TRUE;
}


UINT32 SDHC_CHECK_TRASN_STATE(void)
{
    // check the card status.
    if (g_dwIsMMC)
    {
        if (!SDHC_ISSUE_CMD(13, 0x0001, 0))
        {
             return FALSE;
        }
        else
        {
            if(((g_vpHSMMCReg->RSPREG0>>9)&0xf) | 0x4)
            {
                return TRUE;
            }
            else
            {
                RETAILMSG(1,(TEXT("[%s:ERR] SDHC_CHECK_TRASN_STATE() it is not trans state 0x%x\n"),
                    g_strDrvName,(g_vpHSMMCReg->RSPREG0>>9)&0xf));
                return FALSE;
            }
        }
    }
    else
    {
        if (!SDHC_ISSUE_CMD(13, g_dwRCA, 0))
        {
             return FALSE;
        }
        else
        {
            if(((g_vpHSMMCReg->RSPREG0>>9)&0xf) | 0x4)
            {
                return TRUE;
            }
            else
            {
                RETAILMSG(1,(TEXT("[%s:ERR] SDHC_CHECK_TRASN_STATE() it is not trans state 0x%x\n"),
                    g_strDrvName,(g_vpHSMMCReg->RSPREG0>>9)&0xf));
                return FALSE;
            }
        }
    }

    return TRUE;
}


void SDHC_GET_CARD_SIZE(void)
{

    UINT32 dwCSize, dwCSizeMult, dwReadBlockLength, dwReadBlockPartial, dwCardSize, dwOneBlockSize;

    if(g_dwIsSDHC)
    {
        dwCSize = ((g_vpHSMMCReg->RSPREG1 >> 8) & 0x3fffff);

        dwCardSize = (1024)*(dwCSize+1);
        dwOneBlockSize = (512); // it is fixed.
        g_dwSectorCount = (1024)*(dwCSize+1);
        g_dwSectorCount &= 0xffffffff;
        RETAILMSG(1,(TEXT("\n dwCardSize: %d sector\n"),g_dwSectorCount));
    }
    else if(g_dwMMCSpec42)
    {
    }
    else
    {
        dwReadBlockLength = ((g_vpHSMMCReg->RSPREG2>>8) & 0xf) ;
        dwReadBlockPartial = ((g_vpHSMMCReg->RSPREG2>>7) & 0x1) ;
        dwCSize = ((g_vpHSMMCReg->RSPREG2 & 0x3) << 10) | ((g_vpHSMMCReg->RSPREG1 >> 22) & 0x3ff);
        dwCSizeMult = ((g_vpHSMMCReg->RSPREG1>>7)&0x7);

        dwCardSize = (1<<dwReadBlockLength)*(dwCSize+1)*(1<<(dwCSizeMult+2));
        dwOneBlockSize = (1<<dwReadBlockLength);

        RETAILMSG(0,(TEXT("\n dwReadBlockLength: %d"),dwReadBlockLength));
        RETAILMSG(0,(TEXT("\n dwReadBlockPartial: %d"),dwReadBlockPartial));
        RETAILMSG(0,(TEXT("\n dwCSize: %d"),dwCSize));
        RETAILMSG(0,(TEXT("\n dwCSizeMult: %d\n"),dwCSizeMult));
        RETAILMSG(1,(TEXT("\n dwCardSize: %d\n"),dwCardSize));

        g_dwSectorCount = dwCardSize / 512;
    }

}

void SDHC_SET_SW_RESET(UINT8 bReset)
{
    g_vpHSMMCReg->SWRST = bReset;
}


void SDHC_SET_BLOCK_SIZE(UINT16 uDmaBufBoundary, UINT16 uBlkSize)
{
    g_vpHSMMCReg->BLKSIZE = (uDmaBufBoundary<<12)|(uBlkSize);
}


void SDHC_SET_BLOCK_COUNT(UINT16 uBlkCnt)
{
    g_vpHSMMCReg->BLKCNT = uBlkCnt;
}


void SDHC_SET_ARG(UINT32 uArg)
{
    g_vpHSMMCReg->ARGUMENT = uArg;
}


void SDHC_SET_TRANS_MODE(UINT32 MultiBlk,UINT32 DataDirection, UINT32 AutoCmd12En,UINT32 BlockCntEn,UINT32 DmaEn)
{
    g_vpHSMMCReg->TRNMOD = (g_vpHSMMCReg->TRNMOD & ~(0xffff)) | (MultiBlk<<5)|(DataDirection<<4)|(AutoCmd12En<<2)|(BlockCntEn<<1)|(DmaEn<<0);
    //RETAILMSG("\ng_vpHSMMCReg->TRNMOD = %x\n",g_vpHSMMCReg->TRNMOD);
}


UINT32 SDHC_CLEAR_WRITE_READY(void)
{
    if (g_vpHSMMCReg->NORINTSTS & (1<<4))
        g_vpHSMMCReg->NORINTSTS |= (1<<4);

    return (((g_vpHSMMCReg->NORINTSTS) & (1<<4)) != (1<<4));
}


UINT32 SDHC_WAIT_WRITE_READY(void)
{
    return (g_vpHSMMCReg->NORINTSTS&0x10);
}

UINT32 SDHC_WAIT_TRANS_COMPLETE(void)
{
    return (g_vpHSMMCReg->NORINTSTS&0x2);
}


UINT32 SDHC_CLEAR_TRANS_COMPLETE(void)
{
    g_vpHSMMCReg->NORINTSTS |= (1<<1);
    return     ((g_vpHSMMCReg->NORINTSTS & (1<<1)) != (1<<1));
}

UINT32 SDHC_CLEAR_READ_READY(void)
{
    if (g_vpHSMMCReg->NORINTSTS & (1<<5))
        g_vpHSMMCReg->NORINTSTS |= (1<<5);

    return ((g_vpHSMMCReg->NORINTSTS & (1<<5)) != (1<<5));
}


UINT32 SDHC_WAIT_READ_READY(void)
{
    return (g_vpHSMMCReg->NORINTSTS&0x20);
}

UINT32 SDHC_GET_EXTCSD(void)
{
    UINT32 i,j,dwTimeout;
    UINT32 *dwpBuffer = (UINT32 *)g_pEXTCSDRegister;
    UINT32 dwCountBlock=0;


    dwTimeout = 0;
    while (!SDHC_CHECK_TRASN_STATE())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] SD/MMC card state is not TRANS in SDHC_GET_EXTCSD()\r\n"),g_strDrvName));
            return FALSE;
        }
        LoopForDelay(500000);
    }


    SDHC_SET_BLOCK_SIZE(7, 512); // Maximum DMA Buffer Size, Block Size
    SDHC_SET_BLOCK_COUNT(1); // Block Numbers to Write
    SDHC_SET_ARG(0); // Card Address to Write

    SDHC_SET_TRANS_MODE(0, 1, 0, 0, 0);
    SDHC_SET_CMD(8,0);

    dwTimeout = 0;
    while (!SDHC_WAIT_CMD_COMPLETE())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] No CMD Complete signal in SDHC_GET_EXTCSD()\r\n"),g_strDrvName));
            return FALSE;
        }
    }

    dwTimeout = 0;
    while (!SDHC_CLEAR_CMD_COMPLETE())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not CMD Complete signal bit in SDHC_GET_EXTCSD()\r\n"),g_strDrvName));
            return FALSE;
        }
    }

    for(j=0; j<1; j++)
    {
        dwTimeout = 0;
        while (!SDHC_WAIT_READ_READY())
        {
            if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
            {
                RETAILMSG(1,(TEXT("[%s:ERR] Not buffer ready in SDHC_GET_EXTCSD()\r\n"),g_strDrvName));
            return FALSE;
            }
        }

        dwTimeout = 0;
        while (!SDHC_CLEAR_READ_READY())
        {
            if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
            {
                RETAILMSG(1,(TEXT("[%s:ERR] Not clear buffer ready bit in SDHC_GET_EXTCSD()\r\n"),g_strDrvName));
            return FALSE;
            }
        }

        for(i=0; i<512/4; i++)
        {
            *dwpBuffer++ = g_vpHSMMCReg->BDATA;
            dwCountBlock++;
        }
    }

    dwTimeout = 0;
    while(!SDHC_WAIT_TRANS_COMPLETE())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not clear Buffer ready bit in SDHC_GET_EXTCSD()\r\n"),g_strDrvName));
            return FALSE;
        }
    }

    dwTimeout = 0;
    while(!SDHC_CLEAR_TRANS_COMPLETE())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not clear Buffer ready bit in SDHC_GET_EXTCSD()\r\n"),g_strDrvName));
            return FALSE;
        }
    }

    if ( g_dwMMCSpec42 )
    {
        g_dwSectorCount = g_pEXTCSDRegister->Sec_Count;
        RETAILMSG(1,(TEXT("[%s:INF] SDHC_GET_EXTCSD() This MMC is Spec 4.2 , Total sector %d\n"),g_strDrvName,g_dwSectorCount));
    }

    if (g_pEXTCSDRegister->CardType != 0x3)
    {
        RETAILMSG(1,(TEXT("[%s:INF] SDHC_GET_EXTCSD() Card Type 0x%x\n"),g_strDrvName,g_pEXTCSDRegister->CardType));
        g_dwISHihgSpeed = FALSE;
    }
    else
    {
        RETAILMSG(1,(TEXT("[%s:INF] SDHC_GET_EXTCSD() Card Type 0x%x\n"),g_strDrvName,g_pEXTCSDRegister->CardType));
        g_dwISHihgSpeed = TRUE;
    }


    return TRUE;

}

UINT32 SDHC_SET_PREDEFINE(UINT32 dwSector)
{
       if (!SDHC_ISSUE_CMD(23, dwSector, 0))
       {
        return FALSE;
    }
    else
    {
        if(((g_vpHSMMCReg->RSPREG0>>9)&0xf) == 4)
        {
            return TRUE;
        }
        else
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not set pre-define in SDHC_SET_PREDEFINE()\r\n"),g_strDrvName));
            return FALSE;
        }
    }

    return TRUE;
}


void SDHC_SET_SYSTEM_ADDR(UINT32 SysAddr)
{
    g_vpHSMMCReg->SYSAD = SysAddr;
}

UINT32 SDHC_WRITE(UINT32 dwStartSector, UINT32 dwSector, UINT32 dwAddr)
{
    UINT32 j,dwTimeout;
    UINT32 *BufferToFuse = (UINT32 *)dwAddr;
    UINT32 dwIsMultiWrite = TRUE;

#ifdef FOR_OSDRIVER
    UINT32 *dwTemp;
    UINT32 dwBufferFlag=FALSE;

    //RETAILMSG(1,(TEXT("### HSMMC::WRITE MMC from 0x%x to 0x%x 0x%x\n"),dwStartSector,dwSector,dwAddr));
    if ( dwAddr % 4 != 0 ) // buffer address is not aligned by 4bytes.
    {
        //RETAILMSG(1,(TEXT("[%s:INF] SDHC_WRITE() pBuf is not aligned by 4\n"),g_strDrvName));
        dwTemp = (UINT32 *)malloc(dwSector * 512);
        BufferToFuse = dwTemp;
        dwBufferFlag = TRUE;
    }
    if ( dwBufferFlag == TRUE )
        memcpy ( (PVOID)dwTemp, (PVOID)dwAddr, dwSector * 512 );
#endif

    if ( dwSector == 1)
    {
        dwIsMultiWrite = FALSE;
    }

    if (!SDHC_CLK_ON_OFF(1))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not on/ff clock in SDHC_WRITE()\r\n"),g_strDrvName));
        goto write_fail;
    }


    dwTimeout = 0;
    while (!SDHC_CHECK_TRASN_STATE())
    {
        RETAILMSG(1,(TEXT("[%s:ERR] SD/MMC card state is not TRANS in SDHC_WRITE()\r\n"),g_strDrvName));
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] SD/MMC card state is not TRANS in SDHC_WRITE()\r\n"),g_strDrvName));
            goto write_fail;
        }
        //LoopForDelay(500000);
    }
    if ( g_dwIsMMC && (dwIsMultiWrite == TRUE) )
    {
        if (!SDHC_SET_PREDEFINE(dwSector)) // for moviNAND
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not set pre-define in SDHC_WRITE()\r\n"),g_strDrvName));
            goto write_fail;
        }
    }

    SDHC_SET_BLOCK_SIZE(7, 512); // Maximum DMA Buffer Size, Block Size
    SDHC_SET_BLOCK_COUNT(dwSector); // Block Numbers to Write

    if (g_dwMMCSpec42 || g_dwIsSDHC)
        SDHC_SET_ARG(dwStartSector);// Card Start Block Address to Write
    else
        SDHC_SET_ARG(dwStartSector*512);// Card Start Block Address to Write

    if (dwIsMultiWrite == TRUE)
    {
        if ( g_dwIsMMC )
            SDHC_SET_TRANS_MODE(1, 0, 0, 1, 0);
        else
            SDHC_SET_TRANS_MODE(1, 0, 1, 1, 0);
    }
    else
    {
        SDHC_SET_TRANS_MODE(1, 0, 0, 1, 0);
    }
    dwTimeout = 0;
    while (!SDHC_WAIT_CMDLINE_READY())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not clear Cmd line in SDHC_WRITE().\r\n"),g_strDrvName));
            SDHC_SET_SW_RESET(0x6);
            goto write_fail;
        }
    }

    dwTimeout = 0;
    while (!SDHC_WAIT_DATALINE_READY())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not clear Data line in SDHC_WRITE().\r\n"),g_strDrvName));
            SDHC_SET_SW_RESET(0x6);
            goto write_fail;
        }
    }

    if (dwIsMultiWrite == TRUE)
        SDHC_SET_CMD(25, 0);
    else
        SDHC_SET_CMD(24, 0);

    dwTimeout = 0;
    while (!SDHC_WAIT_CMD_COMPLETE())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] No CMD Complete signal in SDHC_WRITE()\r\n"),g_strDrvName));
            goto write_fail;
        }
    }

    dwTimeout = 0;
    while (!SDHC_CLEAR_CMD_COMPLETE())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not CMD Complete signal bit in SDHC_WRITE()\r\n"),g_strDrvName));
            goto write_fail;
        }
    }

    for(j=0; j<dwSector; j++)
    {
        dwTimeout = 0;
        while (!SDHC_WAIT_WRITE_READY())
        {
            if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
            {
                RETAILMSG(1,(TEXT("[%s:ERR] Not buffer ready in SDHC_WRITE() Sector Loop %d \r\n"),g_strDrvName,j));
                goto write_fail;
            }
        }

        dwTimeout = 0;
        while (!SDHC_CLEAR_WRITE_READY())
        {
            if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
            {
                RETAILMSG(1,(TEXT("[%s:ERR] Not clear buffer ready bit in SDHC_WRITE()\r\n"),g_strDrvName));
                goto write_fail;
            }
        }

#ifdef FOR_IPL
        PutData512((PUCHAR)BufferToFuse, &(g_vpHSMMCReg->BDATA));
#else
        __try
        {
            PutData512((PUCHAR)BufferToFuse, &(g_vpHSMMCReg->BDATA));
        }

        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            RETAILMSG(1,(TEXT("[%s:ERR] A exception occurs in SDHC_WRITE() \r\n"),g_strDrvName));
            return FALSE;
        }
#endif
        BufferToFuse += (512/4);
    }

    dwTimeout = 0;
    while(!SDHC_WAIT_TRANS_COMPLETE())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not clear buffer ready bit in SDHC_WRITE()\r\n"),g_strDrvName));
            goto write_fail;
        }
    }

    dwTimeout = 0;
    while(!SDHC_CLEAR_TRANS_COMPLETE())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not clear buffer ready bit in SDHC_WRITE()\r\n"),g_strDrvName));
            goto write_fail;
        }
    }

#ifdef FOR_OSDRIVER
    if ( dwBufferFlag == TRUE )
        free(dwTemp);
#endif

    if (!SDHC_CLK_ON_OFF(0))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not on/ff clock in SDHC_WRITE() 2\r\n"),g_strDrvName));
        return FALSE;
    }
    return TRUE;

write_fail:

#ifdef FOR_OSDRIVER
    if ( dwBufferFlag == TRUE )
        free(dwTemp);
#endif

    if (!SDHC_CLK_ON_OFF(0))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not on/ff clock in SDHC_WRITE() 3\r\n"),g_strDrvName));
    }
    return FALSE;

}

UINT32 SDHC_READ(UINT32 dwStartSector, UINT32 dwSector, UINT32 dwAddr)
{
    UINT32 j,dwTimeout;
    UINT32 *BufferToFuse = (UINT32 *)dwAddr;
    UINT32 dwIsMultiRead = TRUE;

#ifdef FOR_OSDRIVER
    UINT32 *dwTemp;
    UINT32 dwBufferFlag=FALSE;

    if ( dwAddr % 4 != 0 ) // buffer address is not aligned by 4bytes.
    {
        //RETAILMSG(0,(TEXT("[%s:ERR] SDHC_READ() pBuf is not aligned by 4\n"),g_strDrvName));
        dwTemp = (UINT32 *)malloc(dwSector * 512);
        BufferToFuse = dwTemp;
        dwBufferFlag = TRUE;
    }
#endif

    if (dwSector == 1)
        dwIsMultiRead = FALSE;

    if (!SDHC_CLK_ON_OFF(1))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not on/ff clock in SDHC_READ()\r\n"),g_strDrvName));
        goto read_fail;
    }

    dwTimeout = 0;
    while (!SDHC_CHECK_TRASN_STATE())
    {
        RETAILMSG(1,(TEXT("[%s:ERR] SD/MMC card state is not TRANS in SDHC_READ()\r\n"),g_strDrvName));
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] SD/MMC card state is not TRANS in SDHC_READ()\r\n"),g_strDrvName));
            goto read_fail;
        }
        //LoopForDelay(500000);
    }


    if ( g_dwIsMMC && (dwIsMultiRead == TRUE) )
    {
        if (!SDHC_SET_PREDEFINE(dwSector)) // for moviNAND
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not set pre-define in SDHC_READ()\r\n"),g_strDrvName));
            goto read_fail;
        }
    }

    SDHC_SET_BLOCK_SIZE(7, 512); // Maximum DMA Buffer Size, Block Size
    SDHC_SET_BLOCK_COUNT(dwSector); // Block Numbers to Write
    if (g_dwMMCSpec42 || g_dwIsSDHC)
        SDHC_SET_ARG(dwStartSector);// Card Start Block Address to Write
    else
        SDHC_SET_ARG(dwStartSector*512);// Card Start Block Address to Write

    if (dwIsMultiRead == TRUE)
    {
        if ( g_dwIsMMC )
        {
            SDHC_SET_TRANS_MODE(1, 1, 0, 1, 0);
        }
        else
        {
            SDHC_SET_TRANS_MODE(1, 1, 1, 1, 0);
        }
    }
    else
        SDHC_SET_TRANS_MODE(1, 1, 0, 1, 0);

    dwTimeout = 0;
    while (!SDHC_WAIT_CMDLINE_READY())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not clear Cmd line in SDHC_READ().\r\n"),g_strDrvName));
            SDHC_SET_SW_RESET(0x2);
            goto read_fail;
        }
    }

    dwTimeout = 0;
    while (!SDHC_WAIT_DATALINE_READY())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not clear Data line in SDHC_READ().\r\n"),g_strDrvName));
            SDHC_SET_SW_RESET(0x4);
            goto read_fail;
        }
    }

    if (dwIsMultiRead == TRUE)
        SDHC_SET_CMD(18, 0); // CMD18: Multi-Read
    else
        SDHC_SET_CMD(17, 0); // CMD17: Single-Read

    dwTimeout = 0;
    while (!SDHC_WAIT_CMD_COMPLETE())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] No CMD Complete signal in SDHC_READ()\r\n"),g_strDrvName));
            SDHC_SET_SW_RESET(0x6);
            goto read_fail;
        }
    }

    dwTimeout = 0;
    while (!SDHC_CLEAR_CMD_COMPLETE())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not CMD Complete signal bit in SDHC_READ()\r\n"),g_strDrvName));
            SDHC_SET_SW_RESET(0x6);
            goto read_fail;
        }
    }

    for(j=0; j<dwSector; j++)
    {
        dwTimeout = 0;
        while (!SDHC_WAIT_READ_READY())
        {
            if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
            {
                RETAILMSG(1,(TEXT("[%s:ERR] Not buffer ready in SDHC_READ()\r\n"),g_strDrvName));
                goto read_fail;
            }
        }

        dwTimeout = 0;
        while (!SDHC_CLEAR_READ_READY())
        {
            if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
            {
                RETAILMSG(1,(TEXT("[%s:ERR] Not clear buffer ready bit in SDHC_READ()\r\n"),g_strDrvName));
                goto read_fail;
            }
        }

#ifdef FOR_IPL
        GetData512((PUCHAR)BufferToFuse, &(g_vpHSMMCReg->BDATA));
#else
        __try
        {
            GetData512((PUCHAR)BufferToFuse, &(g_vpHSMMCReg->BDATA));
        }

        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            RETAILMSG(1,(TEXT("[%s:ERR] A exception occurs in SDHC_READ() \r\n"),g_strDrvName));
            return FALSE;
        }

#endif
        BufferToFuse += (512/4);
    }

    dwTimeout = 0;
    while(!SDHC_WAIT_TRANS_COMPLETE())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not clear buffer ready bit in SDHC_READ()\r\n"),g_strDrvName));
            goto read_fail;
        }
    }

    dwTimeout = 0;
    while(!SDHC_CLEAR_TRANS_COMPLETE())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not clear buffer ready bit in SDHC_READ()\r\n"),g_strDrvName));
            goto read_fail;
        }
    }

#ifdef FOR_OSDRIVER
    if ( dwBufferFlag == TRUE )
    {
        memcpy ( (PVOID)dwAddr, (PVOID)dwTemp, dwSector * 512 );
        free(dwTemp);
    }

#endif

    if (!SDHC_CLK_ON_OFF(0))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not on/ff clock in SDHC_READ() 3\r\n"),g_strDrvName));
        return FALSE;
    }

    return TRUE;

read_fail:

#ifdef FOR_OSDRIVER
    if ( dwBufferFlag == TRUE )
        free(dwTemp);
#endif

    if (!SDHC_CLK_ON_OFF(0))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not on/ff clock in SDHC_READ() 3\r\n"),g_strDrvName));
    }
    return FALSE;
}

#ifdef FOR_HSMMCDRV
UINT32 SDHC_WRITE_DMA(UINT32 dwStartSector, UINT32 dwSector, UINT32 dwAddr)
{
    UINT32 *BufferToFuse = (UINT32 *)dwAddr;
    UINT32 *dwTemp;
    UINT32 dwBufferFlag=FALSE,dwTimeout;
    DWORD dwRet;
    UINT32 dwIsMultiWrite = TRUE;

    //RETAILMSG(1,(TEXT("### HSMMC::WRITE MMC from 0x%x to 0x%x 0x%x\n"),dwStartSector,dwSector,dwAddr));

    if ( dwSector == 1)
    {
        dwIsMultiWrite = FALSE;
    }

    if ( dwAddr % 4 != 0 ) // buffer address is not aligned by 4bytes.
    {
        RETAILMSG(1,(TEXT("[%s:ERR] SDHC_WRITE_DMA() pBuf is not aligned by 4\n"),g_strDrvName));
        dwTemp = (UINT32 *)malloc(dwSector * 512);
        BufferToFuse = dwTemp;
        dwBufferFlag = TRUE;
        memcpy ( (PVOID)dwTemp, (PVOID)dwAddr, dwSector * 512 );
    }

    if (!SDHC_CLK_ON_OFF(1))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not on/ff clock in SDHC_WRITE_DMA()\r\n"),g_strDrvName));
        goto write_fail;
    }

    dwTimeout = 0;
    while (!SDHC_CHECK_TRASN_STATE())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] SD/MMC card state is not TRANS in SDHC_WRITE_DMA()\r\n"),g_strDrvName));
            goto write_fail;
        }
    }

    if ( g_dwIsMMC && dwIsMultiWrite == TRUE )
    {
        if (!SDHC_SET_PREDEFINE(dwSector)) // for moviNAND
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not set pre-define in SDHC_WRITE_DMA()\r\n"),g_strDrvName));
            goto write_fail;
        }
    }

    g_vpHSMMCReg->NORINTSIGEN = (g_vpHSMMCReg->NORINTSIGEN & ~(0xffff)) | TRANSFERCOMPLETE_SIG_INT_EN ;
    SDHC_SET_SYSTEM_ADDR(dwAddr);// AHB System Address For Write

    SDHC_SET_BLOCK_SIZE(7, 512); // Maximum DMA Buffer Size, Block Size
    SDHC_SET_BLOCK_COUNT(dwSector); // Block Numbers to Write
    if (g_dwMMCSpec42 || g_dwIsSDHC)
        SDHC_SET_ARG(dwStartSector);// Card Start Block Address to Write
    else
        SDHC_SET_ARG(dwStartSector*512);// Card Start Block Address to Write

    if (dwIsMultiWrite == TRUE)
    {
        if ( g_dwIsMMC )
            SDHC_SET_TRANS_MODE(1, 0, 0, 1, 1);
        else
            SDHC_SET_TRANS_MODE(1, 0, 1, 1, 1);
    }
    else
        SDHC_SET_TRANS_MODE(1, 0, 0, 1, 1);

    dwTimeout = 0;
    while (!SDHC_WAIT_CMDLINE_READY())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not clear Cmd line in SDHC_WRITE_DMA().\r\n"),g_strDrvName));
            goto write_fail;
        }
    }

    dwTimeout = 0;
    while (!SDHC_WAIT_DATALINE_READY())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not clear Data line in SDHC_WRITE_DMA().\r\n"),g_strDrvName));
            goto write_fail;
        }
    }

    if (dwIsMultiWrite == TRUE)
        SDHC_SET_CMD(25, 0);
    else
        SDHC_SET_CMD(24, 0);

    dwTimeout = 0;
    while (!SDHC_WAIT_CMD_COMPLETE())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] No CMD Complete signal in SDHC_WRITE_DMA()\r\n"),g_strDrvName));
            goto write_fail;
        }
    }

    dwTimeout = 0;
    while (!SDHC_CLEAR_CMD_COMPLETE())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not CMD Complete signal bit in SDHC_WRITE_DMA()\r\n"),g_strDrvName));
            goto write_fail;
        }
    }

    dwRet = WaitForSingleObject(g_hSDDMADoneEvent, 0x1000);

    if ( dwRet != WAIT_OBJECT_0 )
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not Transfer DONT interrupt in SDHC_WRITE_DMA()\n"),g_strDrvName));
        goto write_fail;
    }

    g_vpHSMMCReg->NORINTSIGEN = (g_vpHSMMCReg->NORINTSIGEN & ~(0xffff));
    InterruptDone(g_dwSysintr);

    dwTimeout = 0;
    while(!SDHC_CLEAR_TRANS_COMPLETE())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not clear buffer ready bit in SDHC_WRITE_DMA()\r\n"),g_strDrvName));
            goto write_fail;
        }
    }

    if ( dwBufferFlag == TRUE )
        free(dwTemp);

    if (!SDHC_CLK_ON_OFF(0))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not on/ff clock in SDHC_WRITE_DMA() 2\r\n"),g_strDrvName));
        return FALSE;
    }

    return TRUE;

write_fail:
    g_vpHSMMCReg->NORINTSIGEN = (g_vpHSMMCReg->NORINTSIGEN & ~(0xffff));
    if ( dwBufferFlag == TRUE )
        free(dwTemp);

    //RETAILMSG(1,(TEXT("[%s:ERR] SDHC_WRITE_DMA() FAIL OUT-----------\n"),g_strDrvName));
    if (!SDHC_CLK_ON_OFF(0))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not on/ff clock in SDHC_WRITE_DMA() 3\r\n"),g_strDrvName));
    }
    return FALSE;

}

UINT32 SDHC_READ_DMA(UINT32 dwStartSector, UINT32 dwSector, UINT32 dwAddr)
{
    UINT32 *BufferToFuse = (UINT32 *)dwAddr;
    UINT32 *dwTemp;
    UINT32 dwBufferFlag=FALSE;
    UINT32 temp=0,dwTimeout;
    DWORD dwRet;
    UINT32 dwIsMultiRead = TRUE;

    SDHC_CLK_ON_OFF(1);

    //RETAILMSG(1,(TEXT("#### HSMMC::READ MMC from 0x%x to 0x%x 0x%x\n"),dwStartSector,dwSector,dwAddr));

    if (dwSector == 1)
        dwIsMultiRead = FALSE;

    if ( dwAddr % 4 != 0 ) // buffer address is not aligned by 4bytes.
    {
        RETAILMSG(0,(TEXT("[%s:ERR] SDHC_READ_DMA() pBuf is not aligned by 4\n"),g_strDrvName));
        dwTemp = (UINT32 *)malloc(dwSector * 512);
        BufferToFuse = dwTemp;
        dwBufferFlag = TRUE;
    }

    dwTimeout = 0;
    while (!SDHC_CHECK_TRASN_STATE())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] SD/MMC card state is not TRANS in SDHC_READ_DMA()\r\n"),g_strDrvName));
            goto read_fail;
        }

        LoopForDelay(0x100000);
    }

    if ( g_dwIsMMC && (dwIsMultiRead == TRUE))
    {
        if (!SDHC_SET_PREDEFINE(dwSector)) // for moviNAND
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not set pre-define in SDHC_READ_DMA()\r\n"),g_strDrvName));
            goto read_fail;
        }
    }

    g_vpHSMMCReg->NORINTSIGEN = (g_vpHSMMCReg->NORINTSIGEN & ~(0xffff)) | TRANSFERCOMPLETE_SIG_INT_EN ; // enable transfer done intr.

    SDHC_SET_SYSTEM_ADDR(dwAddr);// AHB System Address For Write

    SDHC_SET_BLOCK_SIZE(7, 512); // Maximum DMA Buffer Size, Block Size
    SDHC_SET_BLOCK_COUNT(dwSector); // Block Numbers to Write
    if (g_dwMMCSpec42 || g_dwIsSDHC)
        SDHC_SET_ARG(dwStartSector);// Card Start Block Address to Write
    else
        SDHC_SET_ARG(dwStartSector*512);// Card Start Block Address to Write

    if (dwIsMultiRead == TRUE)
    {
        if ( g_dwIsMMC )
            SDHC_SET_TRANS_MODE(1, 1, 0, 1, 1);
        else
            SDHC_SET_TRANS_MODE(1, 1, 1, 1, 1);
    }
    else
    {
        SDHC_SET_TRANS_MODE(1, 1, 0, 1, 1);
    }


    dwTimeout = 0;
    while (!SDHC_WAIT_CMDLINE_READY())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not clear Cmd line in SDHC_READ_DMA().\r\n"),g_strDrvName));
            goto read_fail;
        }
    }

    dwTimeout = 0;
    while (!SDHC_WAIT_DATALINE_READY())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not clear Data line in SDHC_READ_DMA().\r\n"),g_strDrvName));
            goto read_fail;
        }
    }

    if (dwIsMultiRead == TRUE)
        SDHC_SET_CMD(18, 0); // CMD18: Multi-Read
    else
        SDHC_SET_CMD(17, 0); // CMD18: Multi-Read

    dwTimeout = 0;
    while (!SDHC_WAIT_CMD_COMPLETE())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] No CMD Complete signal in SDHC_READ_DMA()\r\n"),g_strDrvName));
            goto read_fail;
        }
    }

    dwTimeout = 0;
    while (!SDHC_CLEAR_CMD_COMPLETE())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not CMD Complete signal bit in SDHC_READ_DMA()\r\n"),g_strDrvName));
            goto read_fail;
        }
    }


    dwRet = WaitForSingleObject(g_hSDDMADoneEvent, 0x1000);

    if ( dwRet != WAIT_OBJECT_0 )
    {
        RETAILMSG(1,(TEXT("[%s:ERR] SDHC_READ_DMA() Transfer DONE Interrupt does not occur\n"),g_strDrvName));
        RETAILMSG(1,(TEXT("[%s:ERR] SDHC_READ_DMA() 0x%x 0x%x 0x%x 0x%x\n"),g_strDrvName,dwStartSector,dwSector,dwAddr,g_dwMMCSpec42));
        goto read_fail;
    }

    g_vpHSMMCReg->NORINTSIGEN = (g_vpHSMMCReg->NORINTSIGEN & ~(0xffff));
    InterruptDone(g_dwSysintr);

    dwTimeout = 0;
    while(!SDHC_CLEAR_TRANS_COMPLETE())
    {
        if ( dwTimeout++ > LOOP_FOR_TIMEOUT )
        {
            RETAILMSG(1,(TEXT("[%s:ERR] Not clear buffer ready bit in SDHC_READ_DMA()\r\n"),g_strDrvName));
            goto read_fail;
        }
    }


    if ( dwBufferFlag == TRUE )
    {
        memcpy ( (PVOID)dwAddr, (PVOID)dwTemp, dwSector * 512 );
        free(dwTemp);
    }

    if (!SDHC_CLK_ON_OFF(0))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not on/ff clock in SDHC_READ_DMA() 2\r\n"),g_strDrvName));
        return FALSE;
    }
    return TRUE;

read_fail:

    g_vpHSMMCReg->NORINTSIGEN = (g_vpHSMMCReg->NORINTSIGEN & ~(0xffff));

    if ( dwBufferFlag == TRUE )
        free(dwTemp);


    if (!SDHC_CLK_ON_OFF(0))
    {
        RETAILMSG(1,(TEXT("[%s:ERR] Not on/ff clock in SDHC_READ_DMA() 3\r\n"),g_strDrvName));
    }
    RETAILMSG(1,(TEXT("[%s:ERR] #### SDHC_READ_DMA FAIL OUT ----\r\n"),g_strDrvName));
    return FALSE;
}

#endif



#ifdef __cplusplus
} // end extern "C"
#endif
