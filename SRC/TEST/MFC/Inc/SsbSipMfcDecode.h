//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Use of this sample source code is subject to the terms of the 
// Software License Agreement (SLA) under which you licensed this software product.
// If you did not accept the terms of the license agreement, 
// you are not authorized to use this sample source code. 
// THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//


#ifndef __SAMSUNG_SYSLSI_APDEV_MFCLIB_SSBSIPMFCDECODE_H__
#define __SAMSUNG_SYSLSI_APDEV_MFCLIB_SSBSIPMFCDECODE_H__


typedef struct
{
    int width;
    int height;
} SSBSIP_MFC_STREAM_INFO;


typedef unsigned int    MFC_DEC_CONF;

#define MFC_DEC_GETCONF_STREAMINFO            0x00001001
#define MFC_DEC_GETCONF_PHYADDR_FRAM_BUF    0x00001002



#define SSBSIPMFCDEC_MPEG4        0x3031
#define SSBSIPMFCDEC_H263        0x3032
#define SSBSIPMFCDEC_H264        0x3033
#define SSBSIPMFCDEC_VC1        0x3034



#ifdef __cplusplus
extern "C" {
#endif


void *SsbSipMfcDecodeInit(int dec_type);
int   SsbSipMfcDecodeExe(void *openHandle, long lengthBufFill);
int   SsbSipMfcDecodeDeInit(void *openHandle);

int   SsbSipMfcDecodeSetConfig(void *openHandle, MFC_DEC_CONF conf_type, void *value);
int   SsbSipMfcDecodeGetConfig(void *openHandle, MFC_DEC_CONF conf_type, void *value);


void *SsbSipMfcDecodeGetInBuf(void *openHandle, long *size);
void *SsbSipMfcDecodeGetOutBuf(void *openHandle, long *size);



#ifdef __cplusplus
}
#endif


// Error codes
#define SSBSIP_MFC_DEC_RET_OK                        (0)
#define SSBSIP_MFC_DEC_RET_ERR_INVALID_HANDLE        (-1)
#define SSBSIP_MFC_DEC_RET_ERR_INVALID_PARAM        (-2)
#define SSBSIP_MFC_DEC_RET_ERR_UNDEF_CODEC            (-3)

#define SSBSIP_MFC_DEC_RET_ERR_CONFIG_FAIL            (-100)
#define SSBSIP_MFC_DEC_RET_ERR_DECODE_FAIL            (-101)
#define SSBSIP_MFC_DEC_RET_ERR_GETCONF_FAIL            (-102)
#define SSBSIP_MFC_DEC_RET_ERR_SETCONF_FAIL            (-103)

#endif /* __SAMSUNG_SYSLSI_APDEV_MFCLIB_SSBSIPMFCDECODE_H__ */

