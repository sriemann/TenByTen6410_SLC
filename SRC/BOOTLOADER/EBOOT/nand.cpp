//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Use of this sample source code is subject to the terms of the 
// Software License Agreement (SLA) under which you licensed this software product.
// If you did not accept the terms of the license agreement, 
// you are not authorized to use this sample source code. 
// THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//

extern "C"
{
#include <windows.h>
#include <bsp.h>
#include "loader.h"
#include <fmd.h>
#include "s3c6410_ldi.h"
#include "LogoCraft.h"
#include "FMD_LB.H"
#include "s3c6410_ldi.h"


extern DWORD        g_ImageType;
extern UCHAR        g_TOC[LB_PAGE_SIZE];
extern const PTOC   g_pTOC;
extern DWORD        g_dwTocEntry;
extern PBOOT_CFG    g_pBootCfg;
extern BOOL         g_bBootMediaExist;
extern MultiBINInfo g_BINRegionInfo;
extern BOOL         g_bWaitForConnect;      // Is there a SmartMedia card on this device?
extern char*        BootDeviceString[];
extern DWORD        g_DefaultBootDevice;

}
BOOL WriteBlock(DWORD dwBlock, LPBYTE pbBlock, PSectorInfo pSectorInfoTable);
BOOL ReadBlock(DWORD dwBlock, LPBYTE pbBlock, PSectorInfo pSectorInfoTable);


extern DWORD        g_dwLastWrittenLoc;                     //  Defined in bootpart.lib
extern PSectorInfo  g_pSectorInfoBuf;
extern FlashInfo    g_FlashInfo;
static UCHAR        toc[LB_PAGE_SIZE];

// Define a dummy SetKMode function to satisfy the NAND FMD.
//
DWORD SetKMode (DWORD fMode)
{
    return(1);
}


void BootConfigPrint(void)
{
    EdbgOutputDebugString( "BootCfg { \r\n");
    EdbgOutputDebugString( "  ConfigFlags: 0x%x\r\n", g_pBootCfg->ConfigFlags);
    EdbgOutputDebugString( "  BootDelay: 0x%x\r\n", g_pBootCfg->BootDelay);
    EdbgOutputDebugString( "  ImageIndex: %d \r\n", g_pBootCfg->ImageIndex);
    EdbgOutputDebugString ("  Boot Device: %s\r\n", BootDeviceString[g_pBootCfg->BootDevice]);
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
static void BootConfigInit(DWORD dwIndex)
{

    EdbgOutputDebugString("+BootConfigInit\r\n");

    g_pBootCfg = &g_pTOC->BootCfg;

    memset(g_pBootCfg, 0, sizeof(BOOT_CFG));

    g_pBootCfg->ImageIndex   = dwIndex;

    g_pBootCfg->BootDevice = g_DefaultBootDevice;
	//---------------------------------------------------	

    g_pBootCfg->powerCTL = pBSPArgs->powerCTL= PWRCTL_HITEG_FACTORY_DEFAULTS;		// Hiteg Ltd. default config;
    g_pBootCfg->displayType = pBSPArgs->displayType= SMDK6410_LCD_MODULE;				// undefined value results to attached display==NONE	
    g_pBootCfg->framebufferDepth =pBSPArgs->framebufferDepth= LCD_BPP;						// we set 16bpp as default, can be changed to 24/32 bit
    g_pBootCfg->backgroundColor = pBSPArgs->backgroundColor=DEFAULT_FRAMEBUFFER_COLOR;	// a nice classy grey ... 
	g_pBootCfg->logoHW=pBSPArgs->logoHW=DEFAULT_LOGO_POSITION;					// if set to ~0 no logo uploaded...		

	//----------------------------------------------------

    g_pBootCfg->ConfigFlags  = BOOT_TYPE_MULTISTAGE|BOOT_OPTION_CLEAN;

    g_pBootCfg->BootDelay    = CONFIG_BOOTDELAY_DEFAULT;

    // Setup some defaults so that KITL will work without having to change anything
    g_pBootCfg->ConfigFlags |= CONFIG_FLAGS_KITL | CONFIG_FLAGS_DHCP;
    g_pBootCfg->EdbgAddr.dwIP = inet_addr("169.254.1.101");
    g_pBootCfg->SubnetMask = inet_addr("255.255.255.0");
    g_pBootCfg->EdbgAddr.wMAC[0] = 0x1100;
    g_pBootCfg->EdbgAddr.wMAC[1] = 0x3322;
    g_pBootCfg->EdbgAddr.wMAC[2] = 0x5544;

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
    int i;

    EdbgOutputDebugString("g_dwTocEntry : %d\r\n", g_dwTocEntry);
    EdbgOutputDebugString("TOC {\r\n");
    EdbgOutputDebugString("dwSignature: 0x%x\r\n", g_pTOC->dwSignature);


    BootConfigPrint( );

    for (i = 0; i < MAX_TOC_DESCRIPTORS; i++) {
        if ( !VALID_IMAGE_DESCRIPTOR(&g_pTOC->id[i]) )
        {
            EdbgOutputDebugString("!!!Invalid Image Descriptor: id[%d]=0x%x\r\n", i, g_pTOC->id[i]);
            break;
        }
        ID_Print(i);
    }

    //  Print out Chain Information
    EdbgOutputDebugString("chainInfo.dwLoadAddress: 0X%X\r\n", g_pTOC->chainInfo.dwLoadAddress);
    EdbgOutputDebugString("chainInfo.dwFlashAddress: 0X%X\r\n", g_pTOC->chainInfo.dwFlashAddress);
    EdbgOutputDebugString("chainInfo.dwLength: 0X%X\r\n", g_pTOC->chainInfo.dwLength);

    EdbgOutputDebugString("}\r\n");
}


// init the EBOOT TOC to defaults
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
    BootConfigInit(dwEntry);

    // update our index
    g_dwTocEntry = dwEntry;

    // debugger enabled?
    g_bWaitForConnect = (g_pBootCfg->ConfigFlags & CONFIG_FLAGS_KITL) ? TRUE : FALSE;

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
    g_pTOC->id[0].dwTtlSectors  = FILE_TO_SECTOR_SIZE(EBOOT_RAM_IMAGE_SIZE);
    // 1 contigious segment
    g_pTOC->id[0].sgList[0].dwSector = BLOCK_TO_SECTOR(EBOOT_BLOCK);
    g_pTOC->id[0].sgList[0].dwLength = g_pTOC->id[0].dwTtlSectors;

    // init the TOC entry
    g_pTOC->id[dwEntry].dwVersion     = 0x001;
    g_pTOC->id[dwEntry].dwSignature   = dwSig;
    memset(g_pTOC->id[dwEntry].ucString, 0, IMAGE_STRING_LEN);
    g_pTOC->id[dwEntry].dwImageType   = dwImageType;
    g_pTOC->id[dwEntry].dwLoadAddress = dwImageStart;
    g_pTOC->id[dwEntry].dwJumpAddress = dwLaunchAddr;
    g_pTOC->id[dwEntry].dwStoreOffset = 0;
    g_pTOC->id[dwEntry].dwTtlSectors  = FILE_TO_SECTOR_SIZE(dwImageLength);
    // 1 contigious segment
    g_pTOC->id[dwEntry].sgList[0].dwSector = BLOCK_TO_SECTOR(IMAGE_START_BLOCK);
    g_pTOC->id[dwEntry].sgList[0].dwLength = g_pTOC->id[dwEntry].dwTtlSectors;

    TOC_Print();

    return TRUE;
}

//
// Retrieve EBOOT TOC from Nand.
//
BOOL TOC_Read(void)
{
    SectorInfo si;

    EdbgOutputDebugString("TOC_Read\r\n");

    if ( !g_bBootMediaExist ) {
        EdbgOutputDebugString("TOC_Read ERROR: no boot media\r\n");
        return FALSE;
    }

    if ( !FMD_ReadSector(TOC_SECTOR, (PUCHAR)g_pTOC, &si, 1) ) {
        EdbgOutputDebugString("TOC_Read ERROR: Unable to read TOC\r\n");
        return FALSE;
    }

    // is it a valid TOC?
    if ( !VALID_TOC(g_pTOC) ) {
        EdbgOutputDebugString("TOC_Read ERROR: INVALID_TOC Signature: 0x%x\r\n", g_pTOC->dwSignature);
        return FALSE;
    }

    // is it an OEM block?
    if ( (si.bBadBlock != BADBLOCKMARK) || (si.bOEMReserved & (OEM_BLOCK_RESERVED | OEM_BLOCK_READONLY)) ) {
        EdbgOutputDebugString("TOC_Read ERROR: SectorInfo verify failed: %x %x %x %x\r\n",
            si.dwReserved1, si.bOEMReserved, si.bBadBlock, si.wReserved2);
        return FALSE;
    }

    // invalidate TOC if Eboot version has been updated
    if (g_pTOC->id[0].dwVersion != ((EBOOT_VERSION_MAJOR << 16) | EBOOT_VERSION_MINOR))    
    {
        EdbgOutputDebugString("TOC_Read WARN: Eboot version mismatch: TOC->dwVersion: 0x%x , Expected: 0x%x\r\n", 
            g_pTOC->id[0].dwVersion, ((EBOOT_VERSION_MAJOR << 16) | EBOOT_VERSION_MINOR));
        EdbgOutputDebugString("Invalidating TOC for re-initializing.\r\n");
        return FALSE;
    }

    // update our boot config
    g_pBootCfg = &g_pTOC->BootCfg;

    // update our index
    g_dwTocEntry = g_pBootCfg->ImageIndex;

    // KITL enabled?
    g_bWaitForConnect = (g_pBootCfg->ConfigFlags & CONFIG_FLAGS_KITL) ? TRUE : FALSE;

    // cache image type
    g_ImageType = g_pTOC->id[g_dwTocEntry].dwImageType;

    EdbgOutputDebugString("-TOC_Read\r\n");

    return TRUE;

}
//
// Store EBOOT TOC to Nand
// BUGBUG: only uses 1 sector for now.
//
BOOL TOC_Write(void)
{
    SectorInfo si, si2;

    EdbgOutputDebugString("+TOC_Write\r\n");

    if ( !g_bBootMediaExist ) {
        EdbgOutputDebugString("TOC_Write WARN: no boot media\r\n");
        return FALSE;
    }

    // is it a valid TOC?
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

    // in order to write a sector we must erase the entire block first
    // !! BUGBUG: must cache the TOC first so we don't trash other image descriptors !!
    if ( !FMD_EraseBlock(TOC_BLOCK) ) {
        RETAILMSG(1, (TEXT("TOC_Write ERROR: EraseBlock[%d] \r\n"), TOC_BLOCK));
        return FALSE;
    }

    // setup our metadata so filesys won't stomp us
    si.dwReserved1 = 0;
    si.bOEMReserved = ~(OEM_BLOCK_RESERVED | OEM_BLOCK_READONLY);   // NAND cannot set bit 0->1
    si.bBadBlock = BADBLOCKMARK;
    si.wReserved2 = 0xffff;

    // write the sector & metadata
    if ( !FMD_WriteSector(TOC_SECTOR, (PUCHAR)&g_TOC, &si, 1) ) {
        EdbgOutputDebugString("TOC_Write ERROR: Unable to save TOC\r\n");
        return FALSE;
    }

    // read it back & verify both data & metadata
    if ( !FMD_ReadSector(TOC_SECTOR, (PUCHAR)&toc, &si2, 1) ) {
        EdbgOutputDebugString("TOC_Write ERROR: Unable to read/verify TOC\r\n");
        return FALSE;
    }

    if ( 0 != memcmp(&g_TOC, &toc, SECTOR_SIZE) ) {
        EdbgOutputDebugString("TOC_Write ERROR: TOC verify failed\r\n");
        return FALSE;
    }

    if ( 0 != memcmp(&si, &si2, sizeof(si)) ) {
        EdbgOutputDebugString("TOC_Write ERROR: SectorInfo verify failed: %x %x %x %x\r\n",
            si.dwReserved1, si.bOEMReserved, si.bBadBlock, si.wReserved2);
        return FALSE;
    }

    EdbgOutputDebugString("-TOC_Write\r\n");
    return TRUE;
}

/*
    @func   void | DumpXIPChainSummary | Dump XIPCHAIN_SUMMARY structure
    @rdesc  none
    @comm
    @xref
*/
void DumpXIPChainSummary(PXIPCHAIN_SUMMARY pChainInfo)
{    
    EdbgOutputDebugString("[Dump XIP Chain Summary]\r\n");
    EdbgOutputDebugString(" - pvAddr: 0x%x\r\n", pChainInfo->pvAddr);
    EdbgOutputDebugString(" - dwMaxLength : %d\r\n", pChainInfo->dwMaxLength);
    EdbgOutputDebugString(" - usOrder : 0x%x\r\n", pChainInfo->usOrder);
    EdbgOutputDebugString(" - usFlags : 0x%x\r\n", pChainInfo->usFlags);
    EdbgOutputDebugString(" - reserved : 0x%x\r\n", pChainInfo->reserved);    
}

/*
    @func   BOOL | WriteOSImageToBootMedia | Stores the image cached in RAM to the Boot Media.
    The image may be comprised of one or more BIN regions.
    @rdesc  TRUE = Success, FALSE = Failure.
    @comm
    @xref
*/
BOOL WriteOSImageToBootMedia(DWORD dwImageStart, DWORD dwImageLength, DWORD dwLaunchAddr)
{
    BYTE nCount;
    DWORD dwNumExts;
    PXIPCHAIN_SUMMARY pChainInfo = NULL;
    EXTENSION *pExt = NULL;
    DWORD dwBINFSPartLength = 0;
    HANDLE hPart, hPartEx;
    DWORD dwStoreOffset;
    DWORD dwMaxRegionLength[BL_MAX_BIN_REGIONS] = {0};
    DWORD dwChainStart, dwChainLength;
    DWORD dwBlock;

    //  Initialize the variables
    dwChainStart = dwChainLength = 0;

    OALMSG(OAL_FUNC, (TEXT("+WriteOSImageToBootMedia\r\n")));
    OALMSG(OAL_INFO, (TEXT("+WriteOSImageToBootMedia: g_dwTocEntry =%d, ImageStart: 0x%x, ImageLength: 0x%x, LaunchAddr:0x%x\r\n"),
                            g_dwTocEntry, dwImageStart, dwImageLength, dwLaunchAddr));

    if ( !g_bBootMediaExist )
    {
        OALMSG(OAL_ERROR, (TEXT("ERROR: WriteOSImageToBootMedia: device doesn't exist.\r\n")));
        return(FALSE);
    }

    if (g_ImageType == IMAGE_TYPE_RAMIMAGE)
    {
        dwBlock = IMAGE_START_BLOCK + 1;
//      dwImageStart += dwLaunchAddr;
//      dwImageLength = dwImageLength; //step loader can support 4k bytes only. NO SuperIPL case is 16K....... 3 Block
    }

    if ( !VALID_TOC(g_pTOC) )
    {
        OALMSG(OAL_WARN, (TEXT("WARN: WriteOSImageToBootMedia: INVALID_TOC\r\n")));
        if ( !TOC_Init(g_dwTocEntry, g_ImageType, dwImageStart, dwImageLength, dwLaunchAddr) )
        {
            OALMSG(OAL_ERROR, (TEXT("ERROR: INVALID_TOC\r\n")));
            return(FALSE);
        }
    }

    // Look in the kernel region's extension area for a multi-BIN extension descriptor.
    // This region, if found, details the number, start, and size of each BIN region.
    //
    for (nCount = 0, dwNumExts = 0 ; (nCount < g_BINRegionInfo.dwNumRegions); nCount++)
    {
        // Does this region contain nk.exe and an extension pointer?
        //
        pExt = (EXTENSION *)GetKernelExtPointer(g_BINRegionInfo.Region[nCount].dwRegionStart,
                                                g_BINRegionInfo.Region[nCount].dwRegionLength );
        EdbgOutputDebugString("INFO: %s: [%d] RegionStart:0x%x, RegionLength:0x%x, pExt:0x%x\r\n",
                                                __FUNCTION__, nCount,
                                                g_BINRegionInfo.Region[nCount].dwRegionStart,
                                                g_BINRegionInfo.Region[nCount].dwRegionLength,
                                                pExt);
        if ( pExt != NULL)
        {
            // If there is an extension pointer region, walk it until the end.
            //
            while (pExt)
            {
                DWORD dwBaseAddr = g_BINRegionInfo.Region[nCount].dwRegionStart;
                pExt = (EXTENSION *)OEMMapMemAddr(dwBaseAddr, (DWORD)pExt);
                EdbgOutputDebugString("INFO: %s: [%d] Found chain extension: '%s' @ 0x%x\r\n", __FUNCTION__, nCount, pExt->name, dwBaseAddr);
                if ((pExt->type == 0) && !strcmp(pExt->name, "chain information"))
                {
                    pChainInfo = (PXIPCHAIN_SUMMARY) OEMMapMemAddr(dwBaseAddr, (DWORD)pExt->pdata);
                    dwNumExts = (pExt->length / sizeof(XIPCHAIN_SUMMARY));
                    EdbgOutputDebugString("INFO: %s: [%d] Found 'chain information' (pChainInfo=0x%x  Extensions=0x%x).\r\n", __FUNCTION__, nCount, (DWORD)pChainInfo, dwNumExts);
                    DumpXIPChainSummary(pChainInfo);
                    dwChainStart = (DWORD)pChainInfo->pvAddr;
                    dwChainLength = pChainInfo->dwMaxLength;
                    goto FoundChainAddress;
                }
                pExt = (EXTENSION *)pExt->pNextExt;
            }
        }
        else {
            //  Search for Chain region. Chain region doesn't have the ROMSIGNATURE set        
            EdbgOutputDebugString("Not Found pExtPointer, Check if this is Chain region.\n");        

            DWORD   dwRegionStart = g_BINRegionInfo.Region[nCount].dwRegionStart;
            DWORD   dwSig = *(LPDWORD) OEMMapMemAddr(dwRegionStart, dwRegionStart + ROM_SIGNATURE_OFFSET);

            if ( dwSig != ROM_SIGNATURE) {
                //  It is the chain
                dwChainStart = dwRegionStart;
                dwChainLength = g_BINRegionInfo.Region[nCount].dwRegionLength;
                OALMSG(TRUE, (TEXT("Found the Chain region: StartAddress: 0x%X; Length: 0x%X\n"), dwChainStart, dwChainLength));
            }
        }
    }
FoundChainAddress:    

    // Determine how big the Total BINFS partition needs to be to store all of this.
    //
    if (pChainInfo && dwNumExts == g_BINRegionInfo.dwNumRegions)    // We're downloading all the regions in a multi-region image...
    {
        DWORD i;
        OALMSG(TRUE, (TEXT("Writing multi-regions : Total %d regions\r\n"), dwNumExts));

        for (nCount = 0, dwBINFSPartLength = 0 ; nCount < dwNumExts ; nCount++)
        {
            dwBINFSPartLength += (pChainInfo + nCount)->dwMaxLength;
            OALMSG(OAL_ERROR, (TEXT("[%u] BINFSPartMaxLength: 0x%x, TtlBINFSPartLength: 0x%x \r\n"),
                nCount, (pChainInfo + nCount)->dwMaxLength, dwBINFSPartLength));

            // MultiBINInfo does not store each Regions MAX length, and pChainInfo is not in any particular order.
            // So, walk our MultiBINInfo matching up pChainInfo to find each regions MAX Length
            for (i = 0; i < dwNumExts; i++) {
                if ( g_BINRegionInfo.Region[i].dwRegionStart == (DWORD)((pChainInfo + nCount)->pvAddr) ) {
                    dwMaxRegionLength[i] = (pChainInfo + nCount)->dwMaxLength;
                    OALMSG(TRUE, (TEXT("dwMaxRegionLength[%u]: 0x%x \r\n"), i, dwMaxRegionLength[i]));
                    break;
                }
            }
        }

    }
    else    // A single BIN file or potentially a multi-region update (but the partition's already been created in this latter case).
    {
        dwBINFSPartLength = g_BINRegionInfo.Region[0].dwRegionLength;
        OALMSG(TRUE, (TEXT("Writing single region/multi-region update, dwBINFSPartLength: %u \r\n"), dwBINFSPartLength));
    }

    // Open/Create the BINFS partition where images are stored.  This partition starts immediately after the MBR on the Boot Media and its length is
    // determined by the maximum image size (or sum of all maximum sizes in a multi-region design).
    // Parameters are LOGICAL sectors.
    //
    hPart = BP_OpenPartition( (IMAGE_START_BLOCK+1)*PAGES_PER_BLOCK,    // next block of MBR
                              SECTOR_TO_BLOCK_SIZE(FILE_TO_SECTOR_SIZE(dwBINFSPartLength))*PAGES_PER_BLOCK, // align to block
                              PART_BINFS,
                              TRUE,
                              PART_OPEN_ALWAYS);

    if (hPart == INVALID_HANDLE_VALUE )
    {
        OALMSG(OAL_ERROR, (TEXT("ERROR: WriteOSImageToBootMedia: Failed to open/create partition.\r\n")));
        return(FALSE);
    }

    // Are there multiple BIN files in RAM (we may just be updating one in a multi-BIN solution)?
    //
    for (nCount = 0, dwStoreOffset = 0; nCount < g_BINRegionInfo.dwNumRegions ; nCount++)
    {
        EdbgOutputDebugString("BIN Region Number : %d\r\n", nCount);
        DWORD dwRegionStart  = (DWORD)OEMMapMemAddr(0, g_BINRegionInfo.Region[nCount].dwRegionStart);
        DWORD dwRegionLength = g_BINRegionInfo.Region[nCount].dwRegionLength;

        // Media byte offset where image region is stored.
        dwStoreOffset += nCount ? dwMaxRegionLength[nCount-1] : 0;

        // Set the file pointer (byte indexing) to the correct offset for this particular region.
        //
        if ( !BP_SetDataPointer(hPart, dwStoreOffset) )
        {
            OALMSG(OAL_ERROR, (TEXT("ERROR: StoreImageToBootMedia: Failed to set data pointer in partition (offset=0x%x).\r\n"), dwStoreOffset));
            return(FALSE);
        }

        // Write the region to the BINFS partition.
        if ( !BP_WriteData(hPart, (LPBYTE)dwRegionStart, dwRegionLength) )
        {
            EdbgOutputDebugString("ERROR: StoreImageToBootMedia: Failed to write region to BINFS partition (start=0x%x, length=0x%x).\r\n", dwRegionStart, dwRegionLength);
            return(FALSE);
        }
        if(nCount > 1)  // We will skip Region 0 -> for bootloader
        {
            
        }
        // update TOC, when we reach to last TOC entry.
        if ((g_pTOC->id[g_dwTocEntry].dwLoadAddress == g_BINRegionInfo.Region[nCount].dwRegionStart) &&
             g_pTOC->id[g_dwTocEntry].dwTtlSectors == FILE_TO_SECTOR_SIZE(dwRegionLength) )
        {
            g_pTOC->id[g_dwTocEntry].dwStoreOffset = dwStoreOffset;
            g_pTOC->id[g_dwTocEntry].dwJumpAddress = 0; // Filled upon return to OEMLaunch

            g_pTOC->id[g_dwTocEntry].dwImageType = g_ImageType;

            g_pTOC->id[g_dwTocEntry].sgList[0].dwSector = FILE_TO_SECTOR_SIZE(g_dwLastWrittenLoc);
            g_pTOC->id[g_dwTocEntry].sgList[0].dwLength = g_pTOC->id[g_dwTocEntry].dwTtlSectors;

            // copy Kernel Region to SDRAM for jump
            memcpy((void*)g_pTOC->id[g_dwTocEntry].dwLoadAddress, (void*)dwRegionStart, dwRegionLength);

            OALMSG(TRUE, (TEXT("Update Last TOC[%d] Entry to TOC cache in memory!\r\n"), g_pTOC->id[g_dwTocEntry]));
        }
        
    /* needed only if RAMIMAGE type of multiple-XIP is used
            
        else if( (dwChainStart == g_BINRegionInfo.Region[nCount].dwRegionStart) &&
                 (dwChainLength == g_BINRegionInfo.Region[nCount].dwRegionLength))
        {
            //  Update our TOC for Chain region
            g_pTOC->chainInfo.dwLoadAddress = dwChainStart;
            g_pTOC->chainInfo.dwFlashAddress = FILE_TO_SECTOR_SIZE(g_dwLastWrittenLoc);
            g_pTOC->chainInfo.dwLength = FILE_TO_SECTOR_SIZE(dwMaxRegionLength[nCount]);

            OALMSG(TRUE, (TEXT("Written Chain Region to the Flash\n")));
            OALMSG(TRUE, (TEXT("LoadAddress = 0x%X; FlashAddress = 0x%X; Length = 0x%X\n"),
                                  g_pTOC->chainInfo.dwLoadAddress,
                                  g_pTOC->chainInfo.dwFlashAddress,
                                  g_pTOC->chainInfo.dwLength));
            // Now copy it to the SDRAM
            memcpy((void *)g_pTOC->chainInfo.dwLoadAddress, (void *)dwRegionStart, dwRegionLength);
        }
        else    // Other TOC Type Just fill it. 
        {
            g_pTOC->id[nCount].dwStoreOffset = dwStoreOffset;
            g_pTOC->id[nCount].dwJumpAddress = 0; // Filled upon return to OEMLaunch

            g_pTOC->id[nCount].dwImageType = g_ImageType;

            g_pTOC->id[nCount].sgList[0].dwSector = FILE_TO_SECTOR_SIZE(g_dwLastWrittenLoc);
            g_pTOC->id[nCount].sgList[0].dwLength = g_pTOC->id[nCount].dwTtlSectors;

            // copy Kernel Region to SDRAM for jump (only if RAMIMAGE multiple-XIP is used)
            //memcpy((void*)g_pTOC->id[nCount].dwLoadAddress, (void*)dwRegionStart, dwRegionLength);

            OALMSG(TRUE, (TEXT("Update Other TOC Last TOC[%d] Entry to TOC cache in memory!\r\n"), g_pTOC->id[nCount]));
        }
    */
            
    }

    TOC_Print();
    // create extended partition in whatever is left
    //
    hPartEx = BP_OpenPartition( NEXT_FREE_LOC,
                                USE_REMAINING_SPACE,
                                PART_DOS32,
                                TRUE,
                                PART_OPEN_ALWAYS);

    if (hPartEx == INVALID_HANDLE_VALUE )
    {
        OALMSG(OAL_WARN, (TEXT("*** WARN: StoreImageToBootMedia: Failed to open/create Extended partition ***\r\n")));
    }

    OALMSG(OAL_FUNC, (TEXT("-WriteOSImageToBootMedia\r\n")));

    return(TRUE);
}


/*
    @func   BOOL | ReadKernelRegionFromBootMedia |
            BinFS support. Reads the kernel region from Boot Media into RAM.  The kernel region is fixed up
            to run from RAM and this is done just before jumping to the kernel entry point.
    @rdesc  TRUE = Success, FALSE = Failure.
    @comm
    @xref
*/
BOOL ReadOSImageFromBootMedia()
{
    HANDLE hPart;
    SectorInfo si;
    DWORD   chainaddr, flashaddr;
    DWORD i;

    OALMSG(OAL_FUNC, (TEXT("+ReadOSImageFromBootMedia\r\n")));

    if (!g_bBootMediaExist)
    {
        OALMSG(OAL_ERROR, (TEXT("ERROR: ReadOSImageFromBootMedia: device doesn't exist.\r\n")));
        return(FALSE);
    }

    if ( !VALID_TOC(g_pTOC) )
    {
        OALMSG(OAL_ERROR, (TEXT("ERROR: ReadOSImageFromBootMedia: INVALID_TOC\r\n")));
        return(FALSE);
    }

    if ( !VALID_IMAGE_DESCRIPTOR(&g_pTOC->id[g_dwTocEntry]) )
    {
        OALMSG(OAL_ERROR, (TEXT("ReadOSImageFromBootMedia: ERROR_INVALID_IMAGE_DESCRIPTOR: 0x%x\r\n"),
            g_pTOC->id[g_dwTocEntry].dwSignature));
        return FALSE;
    }

    if ( !OEMVerifyMemory(g_pTOC->id[g_dwTocEntry].dwLoadAddress, sizeof(DWORD)) ||
         !OEMVerifyMemory(g_pTOC->id[g_dwTocEntry].dwJumpAddress, sizeof(DWORD)) ||
         !g_pTOC->id[g_dwTocEntry].dwTtlSectors )
    {
        OALMSG(OAL_ERROR, (TEXT("ReadOSImageFromBootMedia: ERROR_INVALID_ADDRESS: (address=0x%x, sectors=0x%x, launch address=0x%x)...\r\n"),
            g_pTOC->id[g_dwTocEntry].dwLoadAddress, g_pTOC->id[g_dwTocEntry].dwTtlSectors, g_pTOC->id[g_dwTocEntry].dwJumpAddress));
        return FALSE;
    }

    // Open the BINFS partition (it must exist).
    //
    hPart = BP_OpenPartition( NEXT_FREE_LOC,
                              USE_REMAINING_SPACE,
                              PART_BINFS,
                              TRUE,
                              PART_OPEN_EXISTING);

    if (hPart == INVALID_HANDLE_VALUE )
    {
        OALMSG(OAL_ERROR, (TEXT("ERROR: ReadOSImageFromBootMedia: Failed to open existing partition.\r\n")));
        return(FALSE);
    }

    // Set the partition file pointer to the correct offset for the kernel region.
    //
    if ( !BP_SetDataPointer(hPart, g_pTOC->id[g_dwTocEntry].dwStoreOffset) )
    {
        OALMSG(OAL_ERROR, (TEXT("ERROR: ReadOSImageFromBootMedia: Failed to set data pointer in partition (offset=0x%x).\r\n"),
            g_pTOC->id[g_dwTocEntry].dwStoreOffset));
        return(FALSE);
    }

    // Read the kernel region from the Boot Media into RAM.
    //
    if ( !BP_ReadData( hPart,
                       (LPBYTE)(g_pTOC->id[g_dwTocEntry].dwLoadAddress),
                       SECTOR_TO_FILE_SIZE(g_pTOC->id[g_dwTocEntry].dwTtlSectors)) )
    {
        OALMSG(OAL_ERROR, (TEXT("ERROR: ReadOSImageFromBootMedia: Failed to read kernel region from partition.\r\n")));
        return(FALSE);
    }

    if (!g_pTOC->chainInfo.dwLoadAddress)
    {
        chainaddr = g_pTOC->chainInfo.dwLoadAddress;
        flashaddr = g_pTOC->chainInfo.dwFlashAddress;
        for ( i = 0; i < (g_pTOC->chainInfo.dwLength); i++ )
        {
            OALMSG(TRUE, (TEXT("chainaddr=0x%x, flashaddr=0x%x\r\n"), chainaddr, flashaddr+i));

            if ( !FMD_ReadSector(flashaddr+i, (PUCHAR)(chainaddr), &si, 1) ) {
                OALMSG(OAL_ERROR, (TEXT("TOC ERROR: Unable to read/verify TOC\r\n")));
                return FALSE;
            }
            chainaddr += 512;
        }
    }
    OALMSG(OAL_FUNC, (TEXT("_ReadOSImageFromBootMedia\r\n")));
    return(TRUE);
}


BOOL ReadBlock(DWORD dwBlock, LPBYTE pbBlock, PSectorInfo pSectorInfoTable)
{
    for (int iSector = 0; iSector < g_FlashInfo.wSectorsPerBlock; iSector++) {
        if (!FMD_ReadSector(dwBlock * g_FlashInfo.wSectorsPerBlock + iSector, pbBlock, pSectorInfoTable, 1))
            return FALSE;
        if (pbBlock)
            pbBlock += g_FlashInfo.wDataBytesPerSector;
        if (pSectorInfoTable)
            pSectorInfoTable++;
    }
    return TRUE;
}

BOOL WriteBlock(DWORD dwBlock, LPBYTE pbBlock, PSectorInfo pSectorInfoTable)
{
    for (int iSector = 0; iSector < g_FlashInfo.wSectorsPerBlock; iSector++) {

#ifdef _IROMBOOT_
        if((dwBlock==0) && (iSector<(IMAGE_STEPLOADER_SIZE/SECTOR_SIZE))){
            if (!FMD_WriteSector_Stepldr(dwBlock * g_FlashInfo.wSectorsPerBlock + iSector, pbBlock, pSectorInfoTable, 1))
                return FALSE;
        } else
#endif
        if (!FMD_WriteSector(dwBlock * g_FlashInfo.wSectorsPerBlock + iSector, pbBlock, pSectorInfoTable, 1))
            return FALSE;

//#endif  // !_IROMBOOT_

        if (pbBlock)
            pbBlock += g_FlashInfo.wDataBytesPerSector;
        if (pSectorInfoTable)
            pSectorInfoTable++;
    }
    return TRUE;
}

BOOL WriteRawImageToBootMedia(DWORD dwImageStart, DWORD dwImageLength, DWORD dwLaunchAddr)
{
    DWORD dwBlock,dwNumBlocks;
    LPBYTE pbBuffer;
    SectorInfo si;

    OALMSG(OAL_FUNC, (TEXT("+WriteRawImageToBootMedia\r\n")));

    if ( !g_bBootMediaExist )
    {
        OALMSG(OAL_ERROR, (TEXT("ERROR: WriteRawImageToBootMedia: device doesn't exist.\r\n")));
        return(FALSE);
    }

    if (g_ImageType == IMAGE_TYPE_LOADER)
    {
        dwBlock = EBOOT_BLOCK;
        if ( !VALID_TOC(g_pTOC) )
        {
            OALMSG(OAL_WARN, (TEXT("WARN: WriteRawImageToBootMedia: INVALID_TOC\r\n")));

            if ( !TOC_Init(g_dwTocEntry, g_ImageType, dwImageStart, dwImageLength, dwLaunchAddr) )
            {
                OALMSG(OAL_ERROR, (TEXT("ERROR: INVALID_TOC\r\n")));
                return(FALSE);
            }
        }
    }
    else if (g_ImageType == IMAGE_TYPE_STEPLDR)
    {
        dwBlock = NBOOT_BLOCK;
        dwImageStart += dwLaunchAddr;
        dwImageLength = STEPPINGSTONE_SIZE; //step loader can support 8k bytes.
    }

    pbBuffer = OEMMapMemAddr(dwImageStart, dwImageStart);

    // Compute number of blocks.
    //dwNumBlocks = (dwImageLength / 0x4000) + 1;
    dwNumBlocks = (dwImageLength / (g_FlashInfo.wDataBytesPerSector*g_FlashInfo.wSectorsPerBlock)) + (dwImageLength%(g_FlashInfo.wDataBytesPerSector*g_FlashInfo.wSectorsPerBlock) ? 1: 0);
    OALMSG(TRUE, (TEXT("dwImageLength = 0x%x \r\n"), dwImageLength));
    OALMSG(TRUE, (TEXT("dwNumBlocks = 0x%x \r\n"), dwNumBlocks));

    while (dwNumBlocks--)
    {
        // If the block is marked bad, skip to next block.  Note that the assumption in our error checking
        // is that any truely bad block will be marked either by the factory during production or will be marked
        // during the erase and write verification phases.  If anything other than a bad block fails ECC correction
        // in this routine, it's fatal.

        OALMSG(TRUE, (TEXT("dwBlock(0x%x) X "), dwBlock));
        OALMSG(TRUE, (TEXT("g_FlashInfo.wSectorsPerBlock(0x%x)"), g_FlashInfo.wSectorsPerBlock));
        OALMSG(TRUE, (TEXT(" = 0x%x \r\n"), dwBlock*g_FlashInfo.wSectorsPerBlock));

        if(g_ImageType != IMAGE_TYPE_STEPLDR && g_ImageType != IMAGE_TYPE_LOADER)
        {
            // This will skip BadBlock for OS Images
            if((FMD_GetBlockStatus(dwBlock) & (BLOCK_STATUS_BAD | BLOCK_STATUS_RESERVED)))
            {
                OALMSG(TRUE, (TEXT("FMD_GetBlockStatus Error \r\n")));
                ++dwBlock;
                ++dwNumBlocks;
                continue;
            }
            if (!FMD_ReadSector(dwBlock*g_FlashInfo.wSectorsPerBlock, NULL, &si, 1))
            {
                OALMSG(TRUE, (TEXT("%s: Failed to get block(0x%x)'s sector info.\r\n"), _T(__FUNCTION__), dwBlock));
                ++dwBlock;
                ++dwNumBlocks;      
            }
        }
        else
        {
            OALMSG(TRUE, (TEXT("Ignore BadBlock Checking for Step loader and eboot\r\n")));
            FMD_ReadSector(dwBlock*g_FlashInfo.wSectorsPerBlock, NULL, &si, 1);
        }

#ifdef _IROMBOOT_
        if(dwBlock != NBOOT_BLOCK)
        {
#endif        
            // Stepldr & Eboot image in nand flash
            // block mark as BLOCK_STATUS_RESERVED & BLOCK_STATUS_READONLY & BLOCK_STATUS_BAD
            if ((si.bBadBlock == BADBLOCKMARK) && (si.bOEMReserved != 0xFC ))
            {
                ++dwBlock;
                ++dwNumBlocks;        // Compensate for fact that we didn't write any blocks.
                continue;
            }
			
            if (!ReadBlock(dwBlock, NULL, g_pSectorInfoBuf))
            {
                OALMSG(OAL_ERROR, (TEXT("WriteData: failed to read block (0x%x).\r\n"), dwBlock));
                return(FALSE);
            }
			

#ifdef _IROMBOOT_            
        }
#endif        

  
        if (!FMD_EraseBlock(dwBlock))
        {
            OALMSG(OAL_ERROR, (TEXT("WriteData: failed to erase block (0x%x).\r\n"), dwBlock));
            return FALSE;
        }

        if (!WriteBlock(dwBlock, pbBuffer, g_pSectorInfoBuf))
        {
            OALMSG(OAL_ERROR, (TEXT("WriteData: failed to write block (0x%x).\r\n"), dwBlock));
            return(FALSE);
        }

        ++dwBlock;
        pbBuffer += g_FlashInfo.dwBytesPerBlock;    //c ksk 20060311
        OALMSG(TRUE,  (TEXT("dwBytesPerBlock : %d\r\n"), g_FlashInfo.dwBytesPerBlock));
    }

    if (g_ImageType == IMAGE_TYPE_LOADER)
    {
        g_pTOC->id[0].dwLoadAddress = dwImageStart;
        g_pTOC->id[0].dwJumpAddress = 0;
        g_pTOC->id[0].dwTtlSectors  = FILE_TO_SECTOR_SIZE(dwImageLength);
        g_pTOC->id[0].sgList[0].dwSector = BLOCK_TO_SECTOR(EBOOT_BLOCK);
        g_pTOC->id[0].sgList[0].dwLength = g_pTOC->id[0].dwTtlSectors;
    }

    OALMSG(OAL_FUNC, (TEXT("_WriteRawImageToBootMedia\r\n")));

    return TRUE;
}
/* 
  Write the Logo data to Nand Flash 
*/ 

extern DWORD OEMgetLCDBpp();
extern DWORD OEMgetDisplayType();


VOID EraseLogoPart()
{
	int n;
	for(n=0; n<LOGO_SECTOR_SIZE; n++)
	{
		EdbgOutputDebugString("Erasing Sector: %d ", LOGO_SECTOR+n);
		if(!FMD_LB_EraseSector(LOGO_SECTOR+n)) 
		{
			EdbgOutputDebugString("failed\r\n");
			NAND_LB_MarkSectorBad(LOGO_SECTOR+n);
		}
		
		EdbgOutputDebugString("OK\r\n");
	}
}
BOOL LOGO_Store()
{
	SectorInfo si; 
	DWORD numSectors;
	int sectorsWritten=0;
	DWORD sector;
	DWORD data;
	
	DWORD status;
	char page[2048];
	DWORD width, height, BPP, size;
	BPP=LDI_getBPP();
	width=LDI_GetDisplayWidth(LDI_getDisplayType());
	height=LDI_GetDisplayHeight(LDI_getDisplayType());
	size=(width * height) * (BPP>>3);
	sector=LOGO_SECTOR;
	numSectors=(size/g_FlashInfo.wDataBytesPerSector) + ((size % g_FlashInfo.wDataBytesPerSector) ? 1: 0);

	data = IMAGE_FRAMEBUFFER_PA_START;

	EraseLogoPart();	

	while(sectorsWritten<numSectors)
	{
		if(sector>(LOGO_SECTOR_SIZE+LOGO_SECTOR) )
		{
			OALMSG(TRUE, (TEXT("Too many bad Blocks, reformat NAND flash don't format for BINFS, just yet\r\n"))); 
			return (FALSE);
		}
		if(!NAND_LB_IsSectorBad(sector))
		{
			if (!FMD_WriteSector( sector,(LPBYTE) data, &si, 1))
			{
				 NAND_LB_MarkSectorBad(sector);
			}
			else
			{
				if(!FMD_ReadSector(sector, (LPBYTE)page, &si, 1) )
				{
					NAND_LB_MarkSectorBad(sector);
				}
				else
				{
					if(0!=memcmp((const void *)data,(const void *) page, 2048))
					{

						NAND_LB_MarkSectorBad(sector);
					}
					else
					{
						sectorsWritten++;
						OALMSG(TRUE, (TEXT("Wrote Sector %d of %d at %d\r\n"), sectorsWritten, numSectors, sector)); 
						if (data)
							data += g_FlashInfo.wDataBytesPerSector;					
					}
				}

			}
		}
		sector++;
    }
    return TRUE;
}

void LOGO_Restore()
{
	char si[128]; 
	DWORD numSectors;
	int sectorsRead=0;
	DWORD sector, status, dst;
	char bytes[2112];

	sector=LOGO_SECTOR;
	DWORD width, height, BPP, size;
	BPP=LDI_getBPP();
	width=LDI_GetDisplayWidth(LDI_getDisplayType());
	height=LDI_GetDisplayHeight(LDI_getDisplayType());
	size=(width * height) * (BPP/8);
	numSectors=(size/g_FlashInfo.wDataBytesPerSector) + ((size % g_FlashInfo.wDataBytesPerSector) ? 1: 0);
	OALMSG(TRUE, (TEXT("++Restore Logo, %d \r\n"), numSectors)); 
	dst = IMAGE_FRAMEBUFFER_PA_START;

	while(sectorsRead<numSectors)
	{
		if(sector>(LOGO_SECTOR_SIZE+LOGO_SECTOR) )
		{
			OALMSG(OAL_ERROR, (TEXT("Too many bad Blocks, reformat NAND flash don't format for BINFS, just yet\r\n"))); 
			OALMSG(OAL_FUNC, (TEXT("--Restore Logo\r\n")));
			return;
		}
		status=FMD_LB_GetSectorStatus(sector);
		if(status == 1 )
		{
			//we found a bad block as it is marked and doesn't have a RESERVED entry
			OALMSG(OAL_FUNC, (TEXT("ReadStatus: Bad Sector skipped %d, because: %d\r\n"), sector, status)); 
		}
		else
		{
			if( !RAW_LB_ReadSector(sector, (LPBYTE)bytes,(LPBYTE) si) )
			{
				// status says its good, but we can't read it....well
				OALMSG(OAL_FUNC, (TEXT("ReadSector: Bad Sector skipped %d\r\n"), sector));
			}
			else
			{
				OALMSG(OAL_FUNC, (TEXT("Read %d/%d Sectors\r"), sectorsRead, sector));
				memcpy((void *)dst, (const void *)bytes, g_FlashInfo.wDataBytesPerSector);
				sectorsRead++;
				dst+=g_FlashInfo.wDataBytesPerSector;
			}
		}

		sector++;
	}
	OALMSG(TRUE, (TEXT("--Restore Logo, %d, %d \r\n"), sector, (LOGO_SECTOR_SIZE+LOGO_SECTOR) ));
}
