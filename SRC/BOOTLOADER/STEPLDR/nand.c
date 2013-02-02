#include <windows.h>
#include <bsp_cfg.h>
#include <s3c6410.h>
#include "s3c6410_addr.h"
#include "utils.h"

//----------------------------------------------------------------------------------

#define NF_CMD(cmd)            {rNFCMD=cmd;}
#define NF_ADDR(addr)        {rNFADDR=addr;}

#define NF_nFCE_L()            {rNFCONT &= ~(1 << 1);}
#define NF_nFCE_H()            {rNFCONT |=  (1 << 1);}

#define NF_RSTECC()            {rNFCONT |=  ((1<<5)|(1<<4));}

#define NF_MECC_UnLock()    {rNFCONT &= ~(1<<7);}
#define NF_MECC_Lock()        {rNFCONT |= (1<<7);}

#define NF_SECC_UnLock()    {rNFCONT &= ~(1<<6);}
#define NF_SECC_Lock()        {rNFCONT |= (1<<6);}

#define NF_CLEAR_RB()        {rNFSTAT |=  (1 << 4);}
#define NF_DETECT_RB()        { while((rNFSTAT&0x11)!=0x11);}    // RnB_Transdetect & RnB
#define NF_WAITRB()            {while (!(rNFSTAT & (1 << 0)));}

#define NF_RDDATA()            (rNFDATA)
#define NF_RDDATA8()        (unsigned char)(rNFDATA8)
#define NF_RDDATA32()        (rNFDATA32)
#define NF_WRDATA(data)        {rNFDATA=data;}

#define NF_RDMECC0()        (rNFMECC0)
#define NF_RDMECC1()        (rNFMECC1)

#define NF_RDMECCD0()        (rNFMECCD0)
#define NF_RDMECCD1()        (rNFMECCD1)

#define NF_WRMECCD0(data)    {rNFMECCD0 = (data);}
#define NF_WRMECCD1(data)    {rNFMECCD1 = (data);}

#define NF_GET_MID(nid)     ((nid & 0x0000FF00) >>8)
#define NF_GET_DID(nid)     (nid & 0x000000FF)

//----------------------------------------------------------------------------------

#define CMD_READ        (0x00)    //  Read
#define CMD_READ1        (0x01)    //  Read1
#define CMD_READ2        (0x50)    //  Read2
#define CMD_READ3        (0x30)    //  Read3
#define CMD_RDO            (0x05)    //  Random Data Output
#define CMD_RDO2        (0xE0)    //  Random Data Output

//----------------------------------------------------------------------------------

#define TACLS                (NAND_TACLS)
#define TWRPH0                (NAND_TWRPH0)
#define TWRPH1                (NAND_TWRPH1)

//----------------------------------------------------------------------------------

typedef enum {
    ECC_CORRECT_MAIN = 0,  // correct Main ECC
    ECC_CORRECT_SPARE = 1,  // correct Main ECC
} ECC_CORRECT_TYPE;

//----------------------------------------------------------------------------------

static BOOL g_bLargeBlock = FALSE;

static void NF_Reset(void);
static BOOL ReadFlashID(DWORD *pID);
extern void _Read_512Byte(BYTE *bufPt);        // in "nand_opt.s"

BOOL ECC_CorrectData(unsigned int sectoraddr, UINT8* pData, UINT32 nRetEcc, ECC_CORRECT_TYPE nType)
{
    DWORD  nErrStatus;
    DWORD  nErrDataNo;
    DWORD  nErrBitNo;
    UINT32 nErrDataMask;
    UINT32 nErrBitMask = 0x7;
    BOOL bRet = TRUE;

    if (nType == ECC_CORRECT_MAIN)
    {
        nErrStatus   = 0;
        nErrDataNo   = 7;
        nErrBitNo    = 4;
        nErrDataMask = 0x7ff;
    }
    else if (nType == ECC_CORRECT_SPARE)
    {
        nErrStatus   = 2;
        nErrDataNo   = 21;
        nErrBitNo    = 18;
        nErrDataMask = 0xf;
    }
    else
    {
        return FALSE;
    }

    switch((nRetEcc>>nErrStatus) & 0x3)
    {
        case 0:    // No Error
            bRet = TRUE;
            break;
        case 1:    // 1-bit Error(Correctable)
            (pData)[(nRetEcc>>nErrDataNo)&nErrDataMask] ^= (1<<((nRetEcc>>nErrBitNo)&nErrBitMask));
            bRet = TRUE;
            break;
        case 2:    // Multiple Error
//            Uart_SendString("M ECC Err\n");
            //Uart_SendDWORD(sectoraddr,1);
            bRet = FALSE;
            break;
        case 3:    // ECC area Error
//            Uart_SendString("ECC Err\n");
        default:
            bRet = FALSE;
            break;
    }

    return bRet;
}

BOOL NAND_Init(void)
{
    DWORD ReadID;

    // Configure BUS Width and Chip Select for NAND Flash
    rMEM_SYS_CFG &= ~(1<<12);    // NAND Flash BUS Width -> 8 bit
    rMEM_SYS_CFG &= ~(0x1<<1);    // Xm0CS2 -> NFCON CS0

    // Initialize NAND Flash Controller
    rNFCONF = (TACLS << 12) | (TWRPH0 <<  8) | (TWRPH1 <<  4) | (0<<0);
    rNFCONT = (0<<17)|(0<<16)|(0<<10)|(0<<9)|(0<<8)|(1<<7)|(1<<6)|(1<<5)|(1<<4)|(0x3<<1)|(1<<0);
    rNFSTAT = (1<<4);

    NF_Reset();

    // Get manufacturer and device codes.
    if (!ReadFlashID(&ReadID))
    {
#if UART_DEBUG    
        Uart_SendString("ID Err\n");
        Uart_SendDWORD(ReadID,1);
        Uart_SendString("!\n");
#endif
    }
    if ((NF_GET_MID(ReadID) == 0xEC) && (NF_GET_DID(ReadID) >= 0xA0) ) g_bLargeBlock = TRUE;
    return g_bLargeBlock;
    
}

BOOL NAND_ReadPage(UINT32 block, UINT32 sector, UINT8 *buffer)
{
    register UINT8 * bufPt=buffer;
    unsigned int RowAddr, ColAddr;
    DWORD MECCBuf, rddata1, rddata2;
    UINT32 nRetEcc;
    int nRet = FALSE;
    int nSectorLoop;

    for (nSectorLoop = 0; nSectorLoop < (g_bLargeBlock==TRUE?4:1); nSectorLoop++)
    {
        ColAddr = nSectorLoop * 512;

        NF_nFCE_L();

        NF_CMD(CMD_READ);                    // Read command

        if (g_bLargeBlock == TRUE)
        {
            RowAddr = (block<<6) + sector;

            NF_ADDR((ColAddr)   &0xff);        // 1st cycle
            NF_ADDR((ColAddr>>8)&0xff);        // 2nd cycle
            NF_ADDR((RowAddr)   &0xff);        // 3rd cycle
            NF_ADDR((RowAddr>>8)&0xff);    // 4th cycle
#if    1    //    LB_NEED_EXT_ADDR
            NF_ADDR((RowAddr>>16)&0xff);    // 5th cycle
#endif
        }
        else
        {
            RowAddr = (block<<5) + sector;

            NF_ADDR((ColAddr)   &0xff);        // 1st cycle
            NF_ADDR((RowAddr)   &0xff);        // 2nd cycle
            NF_ADDR((RowAddr>>8)&0xff);    // 3rd cycle
#if    1    //    SB_NEED_EXT_ADDR
            NF_ADDR((RowAddr>>16)&0xff);    // 4th cycle
#endif
        }

        NF_CLEAR_RB();
        NF_CMD(CMD_READ3);                // Read command
        NF_DETECT_RB();

        NF_RSTECC();    // Initialize ECC
        NF_MECC_UnLock();

        _Read_512Byte(bufPt+nSectorLoop*512);    // Read 512 bytes.

        NF_MECC_Lock();

        if (g_bLargeBlock == TRUE)
        {
            ColAddr = 2048;

            NF_CMD(CMD_RDO);                // Random Data Output In a Page, 1st Cycle
            NF_ADDR((ColAddr)   &0xff);        // 1st cycle
            NF_ADDR((ColAddr>>8)&0xff);        // 2nd cycle
            NF_CMD(CMD_RDO2);                // Random Data Output In a Page. 2nd Cycle

            rddata1 = NF_RDDATA8();                // check bad block
            rddata2 = NF_RDDATA32();            // read dwReserved1
            rddata2 = NF_RDDATA8();                // check bOEMResetved

            if (((rddata1&0xff) != 0xff) && ((rddata2 & 0xff) != 0xfc))
            {
#if UART_DEBUG            
                Uart_SendString("BAD\n");
#endif
                return nRet;  // bad block && !(OEM_BLOCK_RESERVED | OEM_BLOCK_READONLY)
            }

        }
        else
        {
            rddata1 = NF_RDDATA32();
            rddata2 = NF_RDDATA32();                // check bOEMResetved and bad block

            if ((((rddata2>>8) & 0xff) != 0xff) && ((rddata2 & 0xff) != 0xfc))
        {
#if UART_DEBUG        
            Uart_SendString("BAD\n");
#endif
            return nRet;  // bad block && !(OEM_BLOCK_RESERVED | OEM_BLOCK_READONLY)
        }
        }

        if (g_bLargeBlock == TRUE)
        {
            ColAddr = 2048 + 8 + nSectorLoop*4;

            NF_CMD(CMD_RDO);                // Random Data Output In a Page, 1st Cycle
            NF_ADDR((ColAddr)   &0xff);        // 1st cycle
            NF_ADDR((ColAddr>>8)&0xff);        // 2nd cycle
            NF_CMD(CMD_RDO2);                // Random Data Output In a Page. 2nd Cycle
        }

        MECCBuf = NF_RDDATA32();

        NF_WRMECCD0( ((MECCBuf&0x0000ff00)<<8) | ((MECCBuf&0x000000ff)    ) );
        NF_WRMECCD1( ((MECCBuf&0xff000000)>>8) | ((MECCBuf&0x00ff0000)>>16) );

        nRetEcc = rNFECCERR0;

        nRet = ECC_CorrectData(RowAddr, bufPt+nSectorLoop*512, nRetEcc, ECC_CORRECT_MAIN);

        NF_nFCE_H();

        if (nRet == FALSE)
        {
            return nRet;
        }
    }

    return nRet;
}

static void NF_Reset(void)
{
    volatile int i;

    NF_nFCE_L();

    NF_CLEAR_RB();
    NF_CMD(0xFF);    //reset command

    for(i=0; i<10; i++);  //tWB = 100ns. //??????

    NF_DETECT_RB();

    NF_nFCE_H();
}

static BOOL ReadFlashID(DWORD *pID)
{
    BYTE MID, DID, i;

    NF_nFCE_L();

    NF_CLEAR_RB();

    NF_CMD(0x90);            // Send flash ID read command.

    NF_ADDR(0x00);

    for (i=0; i<100; i++);

    for (i=0; i<10; i++)
    {
        MID = (BYTE)NF_RDDATA8();    // Manufacturer ID

        if (MID == 0xEC || MID == 0x98) break;
    }

    DID = (BYTE)NF_RDDATA8();        // Device ID

    NF_nFCE_H();

    if (MID == 0xEC)        // Samsung NAND
    {
        *pID = (DWORD)((MID<<8) | (DID));

        return TRUE;
    }
    else if (MID == 0x98)    // xD-Picture Card
    {
        *pID = (DWORD)((MID<<8) | (DID));

        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

