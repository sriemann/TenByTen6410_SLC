//
// Copyright (c) Samsung Electronics. Co. LTD.  All rights reserved.
//
/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.
*/

#include <windows.h>
#include <bsp.h>
#include <bsp_cfg.h>
#include <DrvLib.h>
#include <HSMMCDRV.h>

#define TACLS            7  //7  //1
#define TWRPH0            7  //7  //6
#define TWRPH1            7  //7  //2

#ifdef    REMOVE_BEFORE_RELEASE
#define DRVLIB_MSG(x)
#define DRVLIB_INF(x)
#define DRVLIB_ERR(x)    RETAILMSG(TRUE, x)
#else
//#define DRVLIB_MSG(x)    RETAILMSG(TRUE, x)
#define DRVLIBP_MSG(x)
#define DRVLIB_INF(x)    RETAILMSG(TRUE, x)
//#define DRVLIB_INF(x)
#define DRVLIB_ERR(x)    RETAILMSG(TRUE, x)
//#define DRVLIB_ERR(x)
#endif

BOOL BootDeviceInit()
{
#ifdef _SDMMC_BOOT_
    int bRet= TRUE;
    while(!SDHC_INIT())
    {
    }
    return bRet;
#else
    volatile S3C6410_NAND_REG *s6410NAND;
    s6410NAND = (S3C6410_NAND_REG *)S3C6410_BASE_REG_PA_NFCON;

    s6410NAND->NFCONF =     (TACLS  <<  12) |    /* duration = HCLK * TACLS */
                (TWRPH0 <<  8) |    /* duration = HCLK * (TWRPH0 + 1) */
                (TWRPH1 <<  4);        /* duration = HCLK * (TWRPH1 + 1) */
    s6410NAND->NFCONT = (0<<17)|(0<<16)|(0<<10)|(0<<9)|(0<<8)|(1<<7)|(1<<6)|(1<<5)|(1<<4)|(0x3<<1)|(1<<0);
    s6410NAND->NFSTAT = (1<<4);

    return TRUE;
#endif
}



