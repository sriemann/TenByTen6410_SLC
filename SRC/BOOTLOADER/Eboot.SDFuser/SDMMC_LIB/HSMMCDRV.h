#ifndef __HS_MMC_H__
#define __HS_MMC_H__

#ifdef __cplusplus
extern "C" {
#endif


typedef struct _MMC_PARSED_REGISTER_EXTCSD 
{    // structure of Extended CSD register 
    UCHAR Reserved180[181]; 

    UCHAR ErasedMemCont;    // initial value after erase operation 
    UCHAR Reserved182; 
    UCHAR BusWidth;    // bus width 
    UCHAR Reserved184; 

    UCHAR HSTiming;    // high speed timing 

    UCHAR Reserved186; 
    UCHAR PowClass;    // power class  
    UCHAR Reserved188; 
    UCHAR CmdSetRev;    // command set revision 

    UCHAR Reserved190; 

    UCHAR CmdSet;    // contains a binary code of the command set 
    UCHAR EXTCSDRev;    // Extended CSD revision 
    UCHAR Reserved193; 
    UCHAR CSDStruct;    // CSD structure field in CSD register 

    UCHAR Reserved195; 

    UCHAR CardType;    // MMC card type 
    UCHAR Reserved197[3]; 
    UCHAR PwrCl52195; 
    UCHAR PwrCl26195; 

    UCHAR PwrCl52360; 

    UCHAR PwrCl26360;    // supported power class by the card 
    UCHAR Reserved204; 
    UCHAR MinPerfR0426;    // min. read performance with 4 bit bus width & 26MHz 
    UCHAR MinPerfW0426;    // min. write performance with 4 bit bus width & 26MHz 

    UCHAR MinPerfR08260452;  // min. read performance with 8 bit bus width & 26MHz 

          // min. read performance with 4 bit bus width & 52MHz 
    UCHAR MinPerfW08260452;  // min. write performance with 8 bit bus width & 26MHz 
            // min. write performance with 4 bit bus width & 26MHz 
    UCHAR MinPerfR0852;    // min. read performance with 8 bit bus width & 52MHz 

    UCHAR MinPerfW0852;    // min. write performance with 8 bit bus width & 52MHz 

    UCHAR Reserved211; 
    ULONG Sec_Count;    // sector count 
    UCHAR Reserved216[288]; 
    UCHAR sCmdSet;    // command sets are supported by the card 

    UCHAR Reserved505[7]; 

} MMC_PARSED_REGISTER_EXTCSD, *PMMC_PARSED_REGISTER_EXTCSD; 

//////////////////////////////////////////////////////////////////////////////////////////////////

DWORD ReadFrommoviNAND(UINT32 dwStartSector, UINT32 dwSector, UINT32 dwAddr);
DWORD ReadFrommoviNAND_DMA(UINT32 dwStartSector, UINT32 dwSector, UINT32 dwAddr);
int FusingTomoviNAND(UINT32 dwStartSector, UINT32 dwSector, UINT32 dwAddr);
int FusingTomoviNAND_DMA(UINT32 dwStartSector, UINT32 dwSector, UINT32 dwAddr);

void FusingSDBootTomoviNAND(void);

int HSMMCInit(void);
int GetEXTCSDRegister(void);

void GetResponseData(UINT32 uCmd);
void ClockOnOff(int OnOff);
void ClockConfig(UINT32 Clksrc, UINT32 Divisior);

int WaitForCommandComplete(void);
int WaitForTransferComplete(void);
int WaitForBufferWriteReady(void);

int ClearBufferWriteReadyStatus(void);
int ClearBufferReadReadyStatus(void);
int ClearCommandCompleteStatus(void);
int ClearTransferCompleteStatus(void);
void ClearErrInterruptStatus(void);
int WaitForBufferReadReady(void);
void SetTransferModeReg(UINT32 MultiBlk,UINT32 DataDirection, UINT32 AutoCmd12En,UINT32 BlockCntEn,UINT32 DmaEn);
void SetArgumentReg(UINT32 uArg);
void SetBlockCountReg(UINT16 uBlkCnt);
void SetSystemAddressReg(UINT32 SysAddr);
void SetBlockSizeReg(UINT16 uDmaBufBoundary, UINT16 uBlkSize);
void SetMMCSpeedMode(UINT32 eSDSpeedMode);

void SetCommandReg(UINT16 uCmd,UINT32 uIsAcmd);
void SetClock(UINT32 ClkSrc, UINT16 Divisor);
void SetSdhcCardIntEnable(UINT8 ucTemp);
int SetDataTransferWidth(UINT32 dwBusWidth);

int IsCardInProgrammingState(void);
int IssueCommand( UINT16 uCmd, UINT32 uArg, UINT32 uIsAcmd);
void InterruptEnable(UINT16 NormalIntEn, UINT16 ErrorIntEn);
void HostCtrlSpeedMode(UINT8 SpeedMode);

void DisplayCardInformation(void);

void InitGPIO(void);
void InitHSMMCRegister(void);
void InitClkPwr();
void MMCInit();
void SDInit();
int OCRResponseCheck(void);
void SetSDSpeedMode(void);
UINT32 GetSDSCR(void);

//extern UINT32 g_dwSectorCount; // it stores a whole sector of the SD/MMC card.

#define	SD_HCLK	1
#define	SD_EPLL		2
#define	SD_EXTCLK	3

#define	NORMAL	0
#define	HIGH	1

//Normal Interrupt Signal Enable
#define	READWAIT_SIG_INT_EN				(1<<10)
#define	CARD_SIG_INT_EN					(1<<8)
#define	CARD_REMOVAL_SIG_INT_EN			(1<<7)
#define	CARD_INSERT_SIG_INT_EN			(1<<6)
#define	BUFFER_READREADY_SIG_INT_EN		(1<<5)
#define	BUFFER_WRITEREADY_SIG_INT_EN	(1<<4)
#define	DMA_SIG_INT_EN					(1<<3)
#define	BLOCKGAP_EVENT_SIG_INT_EN		(1<<2)
#define	TRANSFERCOMPLETE_SIG_INT_EN		(1<<1)
#define	COMMANDCOMPLETE_SIG_INT_EN		(1<<0)

//Normal Interrupt Status Enable
#define	READWAIT_STS_INT_EN				(1<<10)
#define	CARD_STS_INT_EN					(1<<8)
#define	CARD_REMOVAL_STS_INT_EN			(1<<7)
#define	CARD_INSERT_STS_INT_EN			(1<<6)
#define	BUFFER_READREADY_STS_INT_EN		(1<<5)
#define	BUFFER_WRITEREADY_STS_INT_EN	(1<<4)
#define	DMA_STS_INT_EN					(1<<3)
#define	BLOCKGAP_EVENT_STS_INT_EN		(1<<2)
#define	TRANSFERCOMPLETE_STS_INT_EN		(1<<1)
#define	COMMANDCOMPLETE_STS_INT_EN		(1<<0)



#ifdef __cplusplus
}
#endif
#endif /*__HS_MMC_H__*/

