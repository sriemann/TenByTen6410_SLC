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

#ifndef _SDMMC_LOADER_CFG_H_
#define _SDMMC_LOADER_CFG_H_

#define SHIFT_FOR_SDHC         (1024)
#define STEPLDRBUFFER       (0x50200000)
#define STEPLDRSTARTADDRESS    (0x50000000)
#define SECTOROFSTEPLDR     (16)
//#define STEPLDRSTARTSECTOR  (g_dwSectorCount-SECTOROFSTEPLDR-2) // 2: Signature(1) + Reserved(1)
#define STEPLDRSTARTSECTOR  (g_dwSectorCount-SECTOROFSTEPLDR-2-((g_dwIsSDHC == TRUE) ? SHIFT_FOR_SDHC:0)) // 2: Signature(1) + Reserved(1)

#define SECTOROFTOC         (1)
#define TOCSTARTSECTOR      (STEPLDRSTARTSECTOR-SECTOROFTOC)

#define SDBOOTBUFFER        (0x50200000)
#define SDBOOTSTARTADDRESS    (0x50030000)
#define SDBOOTSIZE          (IMAGE_EBOOT_SIZE)
#define SECTOROFSDBOOT      (SDBOOTSIZE/512)
#define SDBOOTSTARTSECTOR   (TOCSTARTSECTOR-SECTOROFSDBOOT)

#define IMAGEBUFFER         (0x52000000)
#define IMAGESTARTADDRESS   (0x50100000)
#define IMAGESIZE           (IMAGE_NK_SIZE)
#define SECTOROFIMAGE       (IMAGESIZE/512)
#define IMAGESTARTSECTOR    (SDBOOTSTARTSECTOR-SECTOROFIMAGE-SECTOROFFATFS)

#define FATFS_FOR_IMAGE        (0x1000000)
#define SECTOROFFATFS        (FATFS_FOR_IMAGE/512)

#define SECTOROFMBR         (0x100)
#define MBRSTARTSECTOR      (IMAGESTARTSECTOR-SECTOROFMBR) // to align 4kb

#endif  // _IROM_SDMMC_LOADER_CFG_H_

