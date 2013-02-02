#include <windows.h>
#include <bsp.h>
#include <hsmmcDrv.h>

//#include "../loader.h"


#define HSMMC_DATA_BUSWIDTH  4
#define SD_DATA_BUSWIDTH 4
#define SD_CARD 0
#define MMC_CARD 1
#define _DBG_ 0

#define SD_CHANNEL1
// these functions are defined in mmccopy.s
void GetData512(PUCHAR pBuf, volatile UINT32 *dwFIFOAddr);
void PutData512(PUCHAR pBuf, volatile UINT32 *dwFIFOAddr);

#ifdef IT_MUST_BE_REMOVED
extern PBOOT_CFG		g_pBootCfg;
extern UCHAR			g_TOC[512];
extern const PTOC 		g_pTOC;
extern DWORD			g_dwImageStartBlock;
extern DWORD			g_dwTocEntry;
extern DWORD			g_ImageType;
extern BOOL 			g_bWaitForConnect;


extern void BootConfigInit(DWORD dwIndex);
#endif
extern void delayLoop(int count);


MMC_PARSED_REGISTER_EXTCSD ___EXTCSDRegister;
MMC_PARSED_REGISTER_EXTCSD *g_pEXTCSDRegister;

volatile int OCRcheck=0; 
volatile int m_ucMMCSpecVer = 0;
volatile int g_dwSupportHihgSpeed = 0;
 
volatile S3C6410_HSMMC_REG *g_vpHSMMCReg;	
volatile S3C6410_SYSCON_REG *g_vpSYSCONReg;	
volatile S3C6410_GPIO_REG *g_vpIOPortReg;
unsigned int ARMCLK, HCLK, PCLK;

UINT32 g_dwSectorCount;
UINT32 g_dwMMCSpec42;
UINT32 g_dwIsSDSPEC20;
UINT32 g_dwCmdRetryCount;
UINT32 g_dwIsMMC;
UINT32 g_dwRCA=0;
UINT32 g_dwSDSPEC;
UINT32 g_dwISHihgSpeed;
UINT32 g_dwIsSDHC;

volatile UINT32 g_bDMADone=0;

static void LoopForDelay(int count) 
{ 
	volatile int j; 
	for(j = 0; j < count; j++)  ; 
}


int HSMMCInit(void)
{
    g_dwMMCSpec42 = FALSE;
    g_dwCmdRetryCount = 0;
    g_dwIsMMC = FALSE;
    g_dwRCA=0;
    g_dwSDSPEC = 0;
    g_dwISHihgSpeed = FALSE;
    g_dwIsSDHC = FALSE;    
    g_dwSupportHihgSpeed = FALSE;
    g_dwIsSDSPEC20 = TRUE;

    HCLK = 100000000;

    g_pEXTCSDRegister = &___EXTCSDRegister;

#ifdef SD_CHANNEL1 // james 
	g_vpHSMMCReg = (S3C6410_HSMMC_REG *)OALPAtoVA( S3C6410_BASE_REG_PA_HSMMC1  , FALSE);
#else
    g_vpHSMMCReg = (S3C6410_HSMMC_REG *)OALPAtoVA( S3C6410_BASE_REG_PA_HSMMC0  , FALSE);
#endif
    if (g_vpHSMMCReg == NULL)
	{
		RETAILMSG (1,(TEXT("[HSMMCLIB:ERR] HSMMCInit() HSMMC registers not mapped\r\n")));
		return FALSE;
	}
        
    g_vpSYSCONReg = (S3C6410_SYSCON_REG *)OALPAtoVA(S3C6410_BASE_REG_PA_SYSCON, FALSE);
	if (g_vpSYSCONReg == NULL)
	{
		RETAILMSG (1,(TEXT("[HSMMCLIB:ERR] HSMMCInit() SysCon registers not mapped\r\n")));
		return FALSE;
	}
    
    g_vpIOPortReg = (S3C6410_GPIO_REG *)OALPAtoVA(S3C6410_BASE_REG_PA_GPIO, FALSE);
	if (g_vpIOPortReg == NULL)
	{
		RETAILMSG (1,(TEXT("[HSMMCLIB:ERR] HSMMCInit() GPIO registers not mapped\r\n")));
		return FALSE;
	}

    InitClkPwr();
    InitGPIO();
    InitHSMMCRegister();

 
    g_vpHSMMCReg->SWRST = 0x6;

    g_vpHSMMCReg->NORINTSIGEN = 0x0;

	ClockOnOff(0);

	SetClock(SD_EXTCLK, 0xff);

	g_vpHSMMCReg->TIMEOUTCON = 0xe;

	InterruptEnable(0xff, 0x1ff);
    
    IssueCommand(0,0,0);//Idle State

    OCRcheck = 1;

#if 1
    // to detect spec 2.0 SD card.
    while ( g_dwCmdRetryCount <= 10 )
    {
        if (IssueCommand(8,(0x1<<8)|(0xaa),0)) // only 2.0 card can accept this cmd.
            break;
        
        ClearErrInterruptStatus();  // if SD card is not SPEC 2.0, it should clear ErrorBit.
    }
    if ( g_dwCmdRetryCount <= 10 )
    {
        if (g_dwIsSDSPEC20)
            RETAILMSG(1,(TEXT("This SD card is made on SPEC 2.0\n")));
        else
        {
            IssueCommand(0,0,0);//Idle State
            RETAILMSG(1,(TEXT("This SD card is made on SPEC 1.x\n")));
        }
    }
    g_dwCmdRetryCount = 0;
#endif



    if ( g_dwIsSDSPEC20 )
    {
        g_dwIsMMC = SD_CARD;
        SDInit();
    }

    else
    {
        while ( g_dwCmdRetryCount <= 10 )
        {
            if (IssueCommand(1, (1<<30)|0xff8000, 0)) // only MMC card can accept this cmd.
                break;
            
            ClearErrInterruptStatus();  // if the card is not MMC, it should clear ErrorBit.
        }
        if (g_dwCmdRetryCount <= 10)
            g_dwIsMMC = MMC_CARD ;
        else
            g_dwIsMMC = SD_CARD;

        if ( g_dwIsMMC == MMC_CARD )
            MMCInit();
        else
            SDInit();
    }
       
    return TRUE;
}

void MMCInit()
{
    UINT32 uSfr0;
    
    while (1)
    {
        IssueCommand(1, (1<<30)|0xff8000, 0);

        uSfr0 = g_vpHSMMCReg->RSPREG0;

        if ( uSfr0 & (1<<30) )
        {
            RETAILMSG(1,(TEXT("[HSMMCLIB:INF] HSMMCInit() This MMC is made on SPEC 4.2\n")));
            g_dwMMCSpec42 = TRUE;
        }

        if ( uSfr0 & (1<<31))
        {
             RETAILMSG(1,(TEXT("Card Power init finished.\n")));
             break;
        }
    }

    OCRcheck = 0;
    
	ClearErrInterruptStatus();    

	IssueCommand(2,0,0);	

	// Send RCA(Relative Card Address). It places the card in the STBY state
 	IssueCommand(3,0x0001,0);	
	
	IssueCommand(9, 0x0001, 0);//Send CSD
	
	
	DisplayCardInformation();


	IssueCommand(7, 0x0001, 0);	

	while (!SetDataTransferWidth(1));

    GetEXTCSDRegister();  

	if (!SetDataTransferWidth(HSMMC_DATA_BUSWIDTH))
	{
        g_vpHSMMCReg->NORINTSTS = 0xffff;      
        RETAILMSG(1,(TEXT("HSMMC:::SetDataTransferWidth Failed\n")));      
        return ;
	}

    if (g_dwSupportHihgSpeed == TRUE )
       	ClockConfig(SD_EPLL, 1);// Divisor 1 = Base clk /2	,Divisor 2 = Base clk /4, Divisor 4 = Base clk /8 ...
	else
        ClockConfig(SD_EPLL, 2);// Divisor 1 = Base clk /2	,Divisor 2 = Base clk /4, Divisor 4 = Base clk /8 ...

    LoopForDelay(0x100000);

	while (!IsCardInProgrammingState());

	while(!IssueCommand(16, 512, 0));//Set the One Block size	

	g_vpHSMMCReg->NORINTSTS = 0xffff;
}

void SDInit()
{
    if (!OCRResponseCheck())
    {
        RETAILMSG(1,(TEXT("Card Init is failed\r\n")));
        while(1);
    }
    
    LoopForDelay(0x100000);

    OCRcheck = 0;
    
	ClearErrInterruptStatus();    

	IssueCommand(2,0,0);	

	// Send RCA(Relative Card Address). It places the card in the STBY state
 	IssueCommand(3,0x0000,0);	

    g_dwRCA = ((g_vpHSMMCReg->RSPREG0)>>16)&0xFFFF; // get RCA
	
	IssueCommand(9, g_dwRCA, 0);//Send CSD
		
	DisplayCardInformation();

	IssueCommand(7, g_dwRCA, 0);

	while (!SetDataTransferWidth(1))

    GetSDSCR();

    if ( g_dwSDSPEC >= 1)
        SetSDSpeedMode();

    ClockConfig(SD_EPLL, (g_dwISHihgSpeed==TRUE) ? 1:2);// Divisor 1 = Base clk /2	,Divisor 2 = Base clk /4, Divisor 4 = Base clk /8 ...    
	while (!SetDataTransferWidth(SD_DATA_BUSWIDTH));	

	while (!IsCardInProgrammingState());

    IssueCommand(16, 512, 0);

	g_vpHSMMCReg->NORINTSTS = 0xffff;
}

int OCRResponseCheck(void)
{
    UINT32 uSfr0,i;

	for(i=0; i<500; i++)
	{
		// CMD55 (For ACMD)
		IssueCommand(55, 0x0, FALSE);
		// (Ocr:2.7V~3.6V)
		IssueCommand(41, 0xc0ff8000, TRUE );

    	uSfr0 = g_vpHSMMCReg->RSPREG0;

        if ( uSfr0&((UINT32)(0x1<<31)))
        {
			if(uSfr0&((UINT32)(0x1<<30))) {
                RETAILMSG(1,(TEXT("This SD card is High Capacity\r\n")));               
                g_dwIsSDHC = TRUE;
			}
            else
                g_dwIsSDHC = FALSE;                
            
			// Normal SD has Command Response Error.
			ClearErrInterruptStatus();
            return TRUE;
 		}
  	}
    return FALSE;
}

void SetSDSpeedMode(void)
{
	UINT32 buffer[16];
	UINT32 uArg = 0;
	int i;


	if(!IssueCommand(16, 64, 0))
		return ;

	SetBlockSizeReg(7, 64); // Maximum DMA Buffer Size, Block Size
	SetBlockCountReg(1); // Block Numbers to Write
	SetTransferModeReg(0, 1, 0, 0, 0);

    uArg = (0x1<<31)|(0xffff<<8)|(1);    

    IssueCommand(6, uArg, FALSE);

	if (!WaitForBufferReadReady())
		RETAILMSG(1,(TEXT("GETSDSCR::ReadBuffer NOT Ready\n")));
	else
		ClearBufferReadReadyStatus();

	for(i=0; i<64/4; i++)
	{
		buffer[i] = g_vpHSMMCReg->BDATA;
	}

	if(!WaitForTransferComplete())
	{
		RETAILMSG(1,(TEXT("GETSDSCR::Transfer NOT Complete\n")));
	}
	ClearTransferCompleteStatus();  

	if ( buffer[3] & (1<<9) ) 
    { // Function Group 1 <- access mode.
		RETAILMSG(1,(TEXT( "This Media support high speed mode.\n" )));
        g_dwISHihgSpeed = TRUE;
	}
	else 
    {
		RETAILMSG(1,(TEXT( "This Media can't support high speed mode.\n" )));
        g_dwISHihgSpeed = FALSE;
	}
}

UINT32 GetSDSCR(void)
{
	UINT32 i;
	UINT32 buffer[2];

	if(!IssueCommand(16, 8, 0))
		return FALSE;

	SetBlockSizeReg(7, 8); // Maximum DMA Buffer Size, Block Size
	SetBlockCountReg(1); // Block Numbers to Write
	SetTransferModeReg(0, 1, 0, 0, 0);

    IssueCommand(55, g_dwRCA, FALSE);
    IssueCommand(51, 0, TRUE);

	if (!WaitForBufferReadReady())
		RETAILMSG(1,(TEXT("GETSDSCR::ReadBuffer NOT Ready\n")));
	else
		ClearBufferReadReadyStatus();
	for(i=0; i<8/4; i++)
	{
		buffer[i] = g_vpHSMMCReg->BDATA;
	}

	if(!WaitForTransferComplete())
	{
		RETAILMSG(1,(TEXT("GETSDSCR::Transfer NOT Complete\n")));
	}
	ClearTransferCompleteStatus();    


//  Transfer mode is determined by capacity register at OCR setting.
//	sCh->m_eTransMode = SDHC_BYTE_MODE;
	if ((*buffer&0xf) == 0x0)
		g_dwSDSPEC= 0; // Version 1.0 ~ 1.01
	else if ((*buffer&0xf) == 0x1)
		g_dwSDSPEC = 1; // Version 1.10, support cmd6
	else if((*buffer&0xf) == 0x2) {
		g_dwSDSPEC = 2; // Version 2.0 support cmd6 and cmd8
	}
	else {
		g_dwSDSPEC = 0; // Error... Deda
	}

	RETAILMSG(1,(TEXT("SDSpecVer=%d\n"), g_dwSDSPEC));
	return TRUE;
}

void InitGPIO() 
{

#ifdef SD_CHANNEL1	//james
  RETAILMSG(_DBG_, (TEXT("[HSMMC1] Setting registers for the GPIO.\n")));

  g_vpIOPortReg->GPHCON0 = (g_vpIOPortReg->GPHCON0 & ~(0xFFFFFF)) | (0x222222);  // 4'b0010 for the MMC 1
  g_vpIOPortReg->GPHPUD &= ~(0xFFF); // Pull-up/down disabled

  g_vpIOPortReg->GPFCON &= ~(0x3<<26);  // WP_SD1
  g_vpIOPortReg->GPFPUD &= ~(0x3<<26);  // Pull-up/down disabled

   g_vpIOPortReg->GPGCON  = (g_vpIOPortReg->GPGCON & ~(0xF<<24)) | (0x3<<24); // MMC CDn1
  g_vpIOPortReg->GPGPUD &= ~(0x3<<12); // Pull-up/down disabled
	
#else

    RETAILMSG(_DBG_, (TEXT("[HSMMC0] Setting registers for the GPIO.\n")));
    // This Eboot that can fuse Image from SD/MMC card to NAND flash is only working on CH1.
    // hsjang 0804028
	g_vpIOPortReg->GPGCON  = (g_vpIOPortReg->GPGCON & ~(0xFFFFFF)) | (0x222222);  // 4'b0010 for the MMC 0
	g_vpIOPortReg->GPGPUD &= ~(0xFFF); // Pull-up/down disabled

	g_vpIOPortReg->GPNCON &= ~(0x3<<24);  // WP_SD0
	g_vpIOPortReg->GPNPUD &= ~(0x3<<24);  // Pull-up/down disabled

	g_vpIOPortReg->GPGCON = (g_vpIOPortReg->GPGCON & ~(0xF<<24)) | (0x2<<24); // SD CD0
	g_vpIOPortReg->GPGPUD &= ~(0x3<<12); // Pull-up/down disabled    
#endif

}

void InitHSMMCRegister() {

    RETAILMSG(_DBG_, (TEXT("[HSMMC1] Setting registers for the EPLL : HSMMCCon.\n")));

	g_vpHSMMCReg->CONTROL2 = (g_vpHSMMCReg->CONTROL2 & ~(0xffffffff)) |
		(0x3<<9) |  // Debounce Filter Count 0x3=64 iSDCLK
		(0x1<<8) |  // SDCLK Hold Enable
		(0x3<<4);   // Base Clock Source = External Clock
}

void InitClkPwr() {
	volatile OTG_PHY_REG *pOtgPhyReg = (OTG_PHY_REG *)OALPAtoVA(S3C6410_BASE_REG_PA_USBOTG_PHY, FALSE);

    // This Eboot that can fuse Image from SD/MMC card to NAND flash is only using EPLL clk.
    RETAILMSG(_DBG_, (TEXT("[HSMMC1] Setting registers for the EPLL (for SDCLK) : SYSCon.\n")));
	// enable USB_PHY clock
	g_vpSYSCONReg->HCLK_GATE |= (1<<20);

	g_vpSYSCONReg->OTHERS |= (1<<16);

	pOtgPhyReg->OPHYPWR = 0x0;  // OTG block, & Analog bock in PHY2.0 power up, normal operation
	pOtgPhyReg->OPHYCLK = 0x20; // Externel clock/oscillator, 48MHz reference clock for PLL
	pOtgPhyReg->ORSTCON = 0x1;
	LoopForDelay(10000);	
	pOtgPhyReg->ORSTCON = 0x0;
	LoopForDelay(10000);	//10000

	g_vpSYSCONReg->HCLK_GATE &= ~(1<<20);

	g_vpSYSCONReg->OTHERS    |= (0x1<<16);  // set USB_SIG_MASK

	#ifdef SD_CHANNEL1 
	g_vpSYSCONReg->HCLK_GATE |= (0x1<<18);	// Gating HCLK for HSMMC1
	g_vpSYSCONReg->SCLK_GATE |= (0x1<<28);	// Gating special clock for HSMMC1 (SCLK_MMC1_48)	
	#else
	g_vpSYSCONReg->HCLK_GATE |= (0x1<<17);	// Gating HCLK for HSMMC0
	g_vpSYSCONReg->SCLK_GATE |= (0x1<<27);	// Gating special clock for HSMMC1 (SCLK_MMC0_48)	
	#endif

	//pSysConReg->CLK_DIV1;
	//pSysConReg->CLK_DIV2;	
}

void ClockOnOff(int OnOff)
{
	if (OnOff == 0)
	{
		g_vpHSMMCReg->CLKCON &=~(0x1<<2);
	}
		
	else
	{
        g_vpHSMMCReg->CONTROL4 |=  (0x3<<16);
		g_vpHSMMCReg->CLKCON |=(0x1<<2);		

		while (1)
		{
			if (g_vpHSMMCReg->CLKCON&(0x1<<3)) // SD_CLKSRC is Stable
				break;
		}
	}
}

void SetClock(UINT32 ClkSrc, UINT16 Divisor)
{
    /*
#ifdef USE_MOVINAND
    if ( g_dwSupportHihgSpeed )
        g_vpHSMMCReg->CONTROL2 = (g_vpHSMMCReg->CONTROL2 & ~(0xffffffff)) | (0x1<<30) | (0x0<<15)|(0x1<<14)|(0x1<<8)|(ClkSrc<<4);
    else
        g_vpHSMMCReg->CONTROL2 = (g_vpHSMMCReg->CONTROL2 & ~(0xffffffff)) | (0x1<<30)| (0x0<<15)|(0x0<<14)|(0x1<<8)|(ClkSrc<<4);
#else
	g_vpHSMMCReg->CONTROL2 = (g_vpHSMMCReg->CONTROL2 & ~(0xffffffff)) | (0x3<<9) | (0x1<<8)|(ClkSrc<<4);
#endif
	//g_vpHSMMCReg->CONTROL2 = (g_vpHSMMCReg->CONTROL2 & ~(0xffffffff)) |(0x1<<14)|(0x1<<8)|(ClkSrc<<4);
	g_vpHSMMCReg->CONTROL3 = (0<<31) | (0<<23) | (0<<15) | (1<<7);//SD OK
	//g_vpHSMMCReg->CONTROL3 = (1<<31) | (1<<23) | (1<<15) | (1<<7);
*/	
    g_vpHSMMCReg->CONTROL2 &= ~(0x3<<14) ; // Turn off the feedback dealy.
    g_vpHSMMCReg->CONTROL2 |= ( (1<<31) | (1<<30) );  // 

	g_vpHSMMCReg->CLKCON &= ~(0xff<<8);
	

	// SDCLK Value Setting + Internal Clock Enable	
	g_vpHSMMCReg->CLKCON = (g_vpHSMMCReg->CLKCON & ~((0xff<<8)|(0x1))) | (Divisor<<8)|(1<<0);

	// CheckInternalClockStable
	while (!(g_vpHSMMCReg->CLKCON&0x2));
	    ClockOnOff(1);
}

void HostCtrlSpeedMode(UINT8 SpeedMode)
{
	UINT8  ucSpeedMode;
	ucSpeedMode = (SpeedMode == HIGH) ? 1 : 0;
	g_vpHSMMCReg->HOSTCTL &= ~(0x1<<2);
	g_vpHSMMCReg->HOSTCTL |= ucSpeedMode<<2;	
}


void InterruptEnable(UINT16 NormalIntEn, UINT16 ErrorIntEn)
{
	ClearErrInterruptStatus();	
	g_vpHSMMCReg->NORINTSTSEN = NormalIntEn;
	g_vpHSMMCReg->ERRINTSTSEN = ErrorIntEn;
}


void ClearErrInterruptStatus(void)
{
    g_vpHSMMCReg->ERRINTSTSEN = 0;
    g_vpHSMMCReg->NORINTSTSEN = 0;


	while (g_vpHSMMCReg->NORINTSTS&(0x1<<15))
	{
		g_vpHSMMCReg->NORINTSTS =g_vpHSMMCReg->NORINTSTS;
	}

    while (	g_vpHSMMCReg->ERRINTSTS )
    {
        g_vpHSMMCReg->ERRINTSTS =g_vpHSMMCReg->ERRINTSTS;
    }


    while (	g_vpHSMMCReg->NORINTSTS )
    {
        g_vpHSMMCReg->NORINTSTS = g_vpHSMMCReg->NORINTSTS;
    }

    g_vpHSMMCReg->ERRINTSTSEN = 0x1ff;
    g_vpHSMMCReg->NORINTSTSEN = 0xff;
    
}


int IssueCommand( UINT16 uCmd, UINT32 uArg, UINT32 uIsAcmd)
{
	UINT32 uLong=1;

	while ((g_vpHSMMCReg->PRNSTS&0x1)); // Check CommandInhibit_CMD
    

	if (!uIsAcmd)//R1b type commands have to check CommandInhibit_DAT bit
	{
		/*if((uCmd==6&&g_dwIsMMC)||uCmd==7||uCmd==12||uCmd==28||uCmd==29||uCmd==38||((uCmd==42||uCmd==56)&&(!g_dwIsMMC)))
		{
			do	{
				uSfr = g_vpHSMMCReg->PRNSTS;
                if (!(uSfr&0x2))
                    break;
                uLong++;
                #if 1
                if ( uLong == 0x10000000)
                {
                    RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] IssueCommand() Cmd Data Line not ready : Cmd %d\n"),uCmd));
                    return FALSE;
                }
                #endif
			} while(TRUE); // Check CommandInhibit_DAT
		}*/
	}
   //RETAILMSG(1,(TEXT("HSMMC:::IssueCommand:::%d Cmd Data Line not ready # 2\n"),uCmd));	
	// Argument Setting
	if (!uIsAcmd)// <------------------- Normal Command (CMD)
	{
		if(uCmd==3||uCmd==4||uCmd==7||uCmd==9||uCmd==10||uCmd==13||uCmd==15||uCmd==55)
			g_vpHSMMCReg->ARGUMENT = uArg<<16;
		else
			g_vpHSMMCReg->ARGUMENT = uArg;
	}
	else// <--------------------------- APP.Commnad (ACMD)
		g_vpHSMMCReg->ARGUMENT = uArg;

	SetCommandReg(uCmd,uIsAcmd);


	if (!WaitForCommandComplete())
    {
        RETAILMSG(1,(TEXT("[HSMMCLIB:INF] IssueCommand() Command NOT Complete 0x%x 0x%x\n"),g_vpHSMMCReg->NORINTSTS, g_vpHSMMCReg->ERRINTSTS));
        return FALSE;
    }
	else
		ClearCommandCompleteStatus();

	if (!(g_vpHSMMCReg->NORINTSTS&0x8000))
	{
        if ( (uCmd == 8) && (OCRcheck == 1))
        {
            RETAILMSG(1,(TEXT("#### this SD card is made on SPEC 2.0\r\n")));                    
        }
		return 1;
	}
	else
	{
		if(OCRcheck == 1)
		{
            g_dwCmdRetryCount++;
            g_dwIsSDSPEC20 = FALSE;
			return 0;
		}
		else
		{
            RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] IssueCommand() Command = %d, Error Stat = %x\n"),(g_vpHSMMCReg->CMDREG>>8),g_vpHSMMCReg->ERRINTSTS));		
            if((g_vpHSMMCReg->CMDREG>>8) == 0x8)
            {
            	RETAILMSG(1,(TEXT("\n[HSMMCLIB:INF] IssueCommand() This SD Card version is NOT 2.0\n")));
            }
            return 0;								
		}
	}
    if (uCmd == 7)
    {
        g_vpHSMMCReg->SWRST = 0x6;
    }
	return 0;

}


int WaitForCommandComplete(void)
{
	UINT32 Loop=1;

	while (!(g_vpHSMMCReg->NORINTSTS&0x1))
	{
		if (Loop%0x7fffffff==0&&Loop>0)
		{			
			return FALSE;
		}
		Loop++;
	}
	return TRUE;
}

int WaitForDataInhibit(void)
{
	UINT32 Loop=1;

	while ((g_vpHSMMCReg->PRNSTS&0x2))
	{
		if (Loop%0x2000000==0)
		{			
			return FALSE;
		}
		Loop++;
	}
	return TRUE;
    
}

int WaitForCmdInhibit(void)
{
	UINT32 Loop=1;

	while ((g_vpHSMMCReg->PRNSTS&0x1))
	{
		if (Loop%0x2000000==0)
		{			
			return FALSE;
		}
		Loop++;
	}
	return TRUE;
    
}
void SetCommandReg(UINT16 uCmd,UINT32 uIsAcmd)
{
	UINT16 usSfr = 0;
	
	//g_vpHSMMCReg->CMDREG &= ~(0xffff);
	
	if (!uIsAcmd)//No ACMD
	{
		/* R2: 136-bits Resp.*/
		if (uCmd==2||uCmd==9||uCmd==10)
			usSfr=(uCmd<<8)|(0<<4)|(1<<3)|(1<<0);

		/* R1,R6,R5: 48-bits Resp. */
		else if (uCmd==3||uCmd==13||uCmd==16||uCmd==27||uCmd==30||uCmd==32||uCmd==33||uCmd==35||uCmd==36||uCmd==42||uCmd==55||uCmd==56||((uCmd==8)&&(!g_dwIsMMC)))
            usSfr=(uCmd<<8)|(1<<4)|(1<<3)|(2<<0);
        
		else if ((uCmd==8 && g_dwIsMMC)||uCmd==11||uCmd==14||uCmd==17||uCmd==18||uCmd==19||uCmd==20||uCmd==24||uCmd==25)
			usSfr=(uCmd<<8)|(1<<5)|(1<<4)|(1<<3)|(2<<0);

		/* R1b,R5b: 48-bits Resp. */
		else if (uCmd==6||uCmd==7||uCmd==12||uCmd==28||uCmd==29||uCmd==38)
		{
			if (uCmd==12)
				usSfr=(uCmd<<8)|(3<<6)|(1<<4)|(1<<3)|(3<<0);
			else if (uCmd==6)
			{
				if(!g_dwIsMMC)	// SD card
					usSfr=(uCmd<<8)|(1<<5)|(1<<4)|(1<<3)|(2<<0);
				else			// MMC card
					usSfr=(uCmd<<8)|(1<<4)|(1<<3)|(3<<0);
					//usSfr=(uCmd<<8)|(1<<5)|(1<<4)|(1<<3)|(3<<0);
			}
			else
				usSfr=(uCmd<<8)|(1<<4)|(1<<3)|(3<<0);
		}

		/* R3,R4: 48-bits Resp.  */
		else if (uCmd==1)
			usSfr=(uCmd<<8)|(0<<4)|(0<<3)|(2<<0);

		/* No-Resp. */
		else
			usSfr=(uCmd<<8)|(0<<4)|(0<<3)|(0<<0);
	}
	else//ACMD
	{
		if (uCmd==6||uCmd==22||uCmd==23)		// R1
			usSfr=(uCmd<<8)|(1<<4)|(1<<3)|(2<<0);
		else if (uCmd==13||uCmd==51)
			usSfr=(uCmd<<8)|(1<<5)|(1<<4)|(1<<3)|(2<<0);
		else
			usSfr=(uCmd<<8)|(0<<4)|(0<<3)|(2<<0);
	}
	g_vpHSMMCReg->CMDREG = usSfr;
}


int ClearCommandCompleteStatus(void)
{
    g_vpHSMMCReg->NORINTSTSEN &= ~(1<<0);
        
    g_vpHSMMCReg->NORINTSTS=(1<<0);
	while (g_vpHSMMCReg->NORINTSTS&0x1)
	{
		g_vpHSMMCReg->NORINTSTS=(1<<0);
	}	
    
    g_vpHSMMCReg->NORINTSTSEN |= (1<<0);   
    return TRUE;
}


void ClockConfig(UINT32 Clksrc, UINT32 Divisior)
{
	UINT32 SrcFreq, WorkingFreq;

	//RETAILMSG("Clock Config\n");
	
	if (Clksrc == SD_HCLK)
		SrcFreq = HCLK;
	else if (Clksrc == SD_EPLL)//Epll Out 48MHz
		SrcFreq = 96000000;
	else
		SrcFreq = 48000000;

    if ( Divisior == 0)
    	WorkingFreq = SrcFreq;
    else
        WorkingFreq = SrcFreq/(Divisior*2);
	RETAILMSG(1,(TEXT("[HSMMCLIB:INF] ClockConfig() Card Working Frequency = %dMHz\n"),WorkingFreq/(1000000)));

	if (g_dwIsMMC)
	{
        if (WorkingFreq>=24000000)// It is necessary to enable the high speed mode in the card before changing the clock freq to a freq higher than 20MHz.
		{
			SetMMCSpeedMode(HIGH);					
			RETAILMSG(1,(TEXT("[HSMMCLIB:INF] ClockConfig() Set MMC High speed mode OK!!\n")));
		}
		else
		{
			SetMMCSpeedMode(NORMAL);	
			RETAILMSG(1,(TEXT("[HSMMCLIB:INF] ClockConfig() Set MMC Normal speed mode OK!!\n")));
		}
	}

	ClockOnOff(0); // when change the sd clock frequency, need to stop sd clock.
	SetClock(Clksrc, Divisior); 
	RETAILMSG(0,(TEXT("clock config g_vpHSMMCReg->HOSTCTL = %x\n"),g_vpHSMMCReg->HOSTCTL));
		
}


void SetSdhcCardIntEnable(UINT8 ucTemp)
{
    g_vpHSMMCReg->NORINTSTSEN &= 0xFEFF;
	g_vpHSMMCReg->NORINTSTSEN |= (ucTemp<<8);
}


int SetDataTransferWidth(UINT32 dwBusWidth)
{
	UINT8 uBitMode=0;
	UINT32 uArg=0;
	UINT8 m_ucHostCtrlReg = 0;
	UINT32 ucBusWidth;


	switch (dwBusWidth)
	{
		case 8:
			ucBusWidth = 8;
			break;
		case 4:
			ucBusWidth = 4;
			break;
		case 1:
			ucBusWidth = 1;
			break;
		default :
			ucBusWidth = 4;
			break;
	}

	//SetSdhcCardIntEnable(0); // Disable sd card interrupt

	if(!g_dwIsMMC)// <------------------------- SD Card Case
	{
		if (!IssueCommand(55, g_dwRCA, 0))
		{
			return 0;
		}
		else
		{
			if (ucBusWidth==1)
			{
				uBitMode = 0;
				if (!IssueCommand(6, 0, 1)) // 1-bits
		        {
					return 0;
				}
			}
			else
			{
				uBitMode = 1;
                    
				if (!IssueCommand(6, 2, 1)) // 4-bits
        		{
					return 0;
				}
			}
		}
	}
	else // <-------------------------------- MMC Card Case
	{
		if (ucBusWidth==1)
			uBitMode = 0;
		else if (ucBusWidth==4)
		{
          	RETAILMSG(1,(TEXT("[HSMMCLIB:INF] SetDataTransferWidth() 4Bit Data Bus enables\n")));
			uBitMode = 1;//4            		// 4-bit bus
		}
		else
		{
			uBitMode = 2;//8-bit bus
			RETAILMSG(1,(TEXT("[HSMMCLIB:INF] SetDataTransferWidth() 8Bit Data Bus enables\n")));
		}
		
		uArg=((3<<24)|(183<<16)|(uBitMode<<8));
		if(!IssueCommand(6, uArg, 0));
	}
	
	if (uBitMode==2)
	{
		m_ucHostCtrlReg &= 0xfd;
		m_ucHostCtrlReg |= 1<<5;
	}
	else
	{
		m_ucHostCtrlReg &= 0xfd;
		m_ucHostCtrlReg |= uBitMode<<1;
	}
	
	g_vpHSMMCReg->HOSTCTL = m_ucHostCtrlReg;
	SetSdhcCardIntEnable(1);
	//RETAILMSG(" transfer g_vpHSMMCReg->HOSTCTL = %x\n",g_vpHSMMCReg->HOSTCTL);
	return 1;
}

void SetMMCSpeedMode(UINT32 eSDSpeedMode)
{
	UINT8  ucSpeedMode;
	UINT32 uArg=0;
	ucSpeedMode = (eSDSpeedMode == HIGH) ? 1 : 0;
	uArg=(3<<24)|(185<<16)|(ucSpeedMode<<8); // Change to the high-speed mode
	while(!IssueCommand(6, uArg, 0));	
}


int IsCardInProgrammingState(void)
{
	// check the card status.
    while (TRUE)
    {
        if (g_dwIsMMC)
        {
        	if (!IssueCommand(13, 0x0001, 0))
        	{
         		return 0;
        	}
        	else
        	{
    			if(((g_vpHSMMCReg->RSPREG0>>9)&0xf) == 4)
        		{
        			//RETAILMSG(1,(TEXT("HSMMC:::Card is transfer status\n")));
        			return 1;
        		}
        		else
                {
                    RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] IsCardInProgrammingState() it is not trans state 0x%x\n"),
                        (g_vpHSMMCReg->RSPREG0>>9)&0xf));            
                    //return 0;
        		}
        	}
        }
        else
        {
        	if (!IssueCommand(13, g_dwRCA, 0))
        	{
         		return 0;
        	}
        	else
        	{
    			if(((g_vpHSMMCReg->RSPREG0>>9)&0xf) == 4)
        		{
        			//RETAILMSG(1,(TEXT("HSMMC:::Card is transfer status\n")));
        			return 1;
        		}
        		else
                {
                    RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] IsCardInProgrammingState() it is not trans state 0x%x\n"),
                        (g_vpHSMMCReg->RSPREG0>>9)&0xf));            
                    //return 0;
        		}
        	}
        }
        
    }
	return TRUE;
}


void DisplayCardInformation(void)
{
	UINT32 C_SIZE, C_SIZE_MULT, READ_BL_LEN, READ_BL_PARTIAL, CardSize, OneBlockSize;
    UINT32 ERASE_GROUP_SIZE,ERASE_GROUP_MULT;
	
	if(g_dwIsSDHC)
	{
    	C_SIZE = ((g_vpHSMMCReg->RSPREG1 >> 8) & 0x3fffff);

    	CardSize = (1024)*(C_SIZE+1);
    	OneBlockSize = (512); // it is fixed.
        g_dwSectorCount = (1024)*(C_SIZE+1);
        g_dwSectorCount &= 0xffffffff;   
        RETAILMSG(1,(TEXT("\n CardSize: %d sector\n"),g_dwSectorCount));        
	}
    else
    {
    	READ_BL_LEN = ((g_vpHSMMCReg->RSPREG2>>8) & 0xf) ;
    	READ_BL_PARTIAL = ((g_vpHSMMCReg->RSPREG2>>7) & 0x1) ;
    	C_SIZE = ((g_vpHSMMCReg->RSPREG2 & 0x3) << 10) | ((g_vpHSMMCReg->RSPREG1 >> 22) & 0x3ff);
    	C_SIZE_MULT = ((g_vpHSMMCReg->RSPREG1>>7)&0x7);

        ERASE_GROUP_SIZE = (g_vpHSMMCReg->RSPREG1 >> 9) & 0x1f;
        ERASE_GROUP_MULT = (g_vpHSMMCReg->RSPREG1 >> 4) & 0x1f;
    	
    	CardSize = (1<<READ_BL_LEN)*(C_SIZE+1)*(1<<(C_SIZE_MULT+2));
    	OneBlockSize = (1<<READ_BL_LEN);

    	RETAILMSG(0,(TEXT("\n READ_BL_LEN: %d"),READ_BL_LEN));	
    	RETAILMSG(0,(TEXT("\n READ_BL_PARTIAL: %d"),READ_BL_PARTIAL));	
    	RETAILMSG(0,(TEXT("\n C_SIZE: %d"),C_SIZE));	
    	RETAILMSG(0,(TEXT("\n C_SIZE_MULT: %d\n"),C_SIZE_MULT));	
        RETAILMSG(1,(TEXT("\n CardSize: %d\n"),CardSize));	

        g_dwSectorCount = CardSize / 512;
    }

}

void SetBlockSizeReg(UINT16 uDmaBufBoundary, UINT16 uBlkSize)
{
	g_vpHSMMCReg->BLKSIZE = (uDmaBufBoundary<<12)|(uBlkSize);
}


void SetBlockCountReg(UINT16 uBlkCnt)
{
	g_vpHSMMCReg->BLKCNT = uBlkCnt;
}


void SetArgumentReg(UINT32 uArg)
{
	g_vpHSMMCReg->ARGUMENT = uArg;
}


void SetTransferModeReg(UINT32 MultiBlk,UINT32 DataDirection, UINT32 AutoCmd12En,UINT32 BlockCntEn,UINT32 DmaEn)
{
	g_vpHSMMCReg->TRNMOD = (g_vpHSMMCReg->TRNMOD & ~(0xffff)) | (MultiBlk<<5)|(DataDirection<<4)|(AutoCmd12En<<2)|(BlockCntEn<<1)|(DmaEn<<0);
	//RETAILMSG("\ng_vpHSMMCReg->TRNMOD = %x\n",g_vpHSMMCReg->TRNMOD);
}


int ClearBufferWriteReadyStatus(void)
{
    UINT32 uLoop=2;
	g_vpHSMMCReg->NORINTSTS |= (1<<4);
	while (g_vpHSMMCReg->NORINTSTS & 0x10)
	{
		g_vpHSMMCReg->NORINTSTS |= (1<<4);
		if ( uLoop%0x10000000 == 0 )
		{			
			return FALSE;
		}
		uLoop++;        
	}
    return TRUE;    
}


int WaitForBufferWriteReady(void)
{
	UINT32 uLoop=1;

	while (!(g_vpHSMMCReg->NORINTSTS&0x10))
	{
		if ( (uLoop%0x10000000) == 0 )
		{			
			return FALSE;
		}
		uLoop++;
	}

	return TRUE;
}

int WaitForTransferComplete(void)
{
	UINT32 Loop=1;

	while (!(g_vpHSMMCReg->NORINTSTS&0x2))
    {
  
      if ( Loop % 0x10000000 == 0  )
      {
            return FALSE;
      }
      Loop++;

	}
    return 1;
}


int ClearTransferCompleteStatus(void)
{
    UINT32 uLoop=2;

	g_vpHSMMCReg->NORINTSTS |= (1<<1);
	while (g_vpHSMMCReg->NORINTSTS&0x2)
	{
		g_vpHSMMCReg->NORINTSTS |= (1<<1);
		if ( uLoop%0x10000 == 0 )
		{			
			return FALSE;
		}
		uLoop++;                 
    }
    return TRUE;
}


int ClearBufferReadReadyStatus(void)
{
    UINT32 uLoop=2;

	g_vpHSMMCReg->NORINTSTS |= (1<<5);
	while (g_vpHSMMCReg->NORINTSTS & (1<<5))
	{
		g_vpHSMMCReg->NORINTSTS |= (1<<5);
		if ( uLoop%0x10000 == 0 )
		{			
			return FALSE;
		}
		uLoop++;        
	}

    return TRUE;
}


int WaitForBufferReadReady(void)
{
	UINT32 uLoop=0;

	while (!(g_vpHSMMCReg->NORINTSTS&0x20));
	// ignore ready bit. because time out~!
/*	{
		if (uLoop%500000==0&&uLoop>0)
		{			
			return 0;
		}
		uLoop++;
	}
	*/
	return 1;
}

void GetResponseData(UINT32 uCmd)
{
	UINT32 uSfr0,uSfr1,uSfr2,uSfr3;

	uSfr0 = g_vpHSMMCReg->RSPREG0;
	uSfr1 = g_vpHSMMCReg->RSPREG1;
	uSfr2 = g_vpHSMMCReg->RSPREG2;
	uSfr3 = g_vpHSMMCReg->RSPREG3;

    RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] GetResponseData() RSPREG0 = %x\n"),uSfr0));
    RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] GetResponseData() RSPREG1 = %x\n"),uSfr1));
    RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] GetResponseData() RSPREG2 = %x\n"),uSfr2));
    RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] GetResponseData() RSPREG3 = %x\n"),uSfr3));
}


int GetEXTCSDRegister(void)
{
    int i,j;
    UINT32 *BufferToFuse = (UINT32 *)g_pEXTCSDRegister;
    UINT32 dwCountBlock=0;

   
	if (!IsCardInProgrammingState())
	{
        return FALSE;
	}

 
	SetBlockSizeReg(7, 512); // Maximum DMA Buffer Size, Block Size
	SetBlockCountReg(1); // Block Numbers to Write
	SetArgumentReg(0); // Card Address to Write
 
	SetTransferModeReg(0, 1, 0, 0, 0);
	SetCommandReg(8,0);
			//RETAILMSG(1,(TEXT("EXTCSD ReadBuffer NOT Ready 1\n")));
	if (WaitForCommandComplete() == FALSE)
	{   
        RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] GetEXTCSDRegister() Command not cleared.\n")));     
        return FALSE;
	}
    if ( ClearCommandCompleteStatus() == FALSE )
    {
        RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] GetEXTCSDRegister() Command status not cleared.\n")));
        return FALSE;
    }
	for(j=0; j<1; j++)
	{
		if (WaitForBufferReadReady() == FALSE)
		{
			RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] GetEXTCSDRegister() ReadBuffer NOT Ready\n")));
            return FALSE;
		}
		else if ( ClearBufferReadReadyStatus() == FALSE )
		{
            RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] GetEXTCSDRegister() ReadBuffer status not cleared.\n")));
            return FALSE;
		}
		for(i=0; i<512/4; i++)
		{
			*BufferToFuse++ = g_vpHSMMCReg->BDATA;
			dwCountBlock++;
		}
	}

	if( WaitForTransferComplete() == FALSE)
	{
		RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] GetEXTCSDRegister() Transfer NOT Complete\n")));
        return FALSE;
	}
    if ( ClearTransferCompleteStatus() == FALSE )
    {
		RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] GetEXTCSDRegister() Transfer status NOT cleared\n")));        
        return FALSE;
    }
    if ( g_dwMMCSpec42 )
    {
        g_dwSectorCount = g_pEXTCSDRegister->Sec_Count;
        RETAILMSG(1,(TEXT("[HSMMCLIB:INF] GetEXTCSDRegister() This MMC is Spec 4.2 , Total sector %d\n"),g_dwSectorCount));        
    }

    if (g_pEXTCSDRegister->CardType != 0x3)
    {
        RETAILMSG(1,(TEXT("[HSMMCLIB:INF] GetEXTCSDRegister() Card Type 0x%x\n"),g_pEXTCSDRegister->CardType));        
        g_dwSupportHihgSpeed = FALSE;
    }  
    else
    {
#ifdef USE_MOVINAND        
#else        
        RETAILMSG(1,(TEXT("[HSMMCLIB:INF] GetEXTCSDRegister() Card Type 0x%x\n"),g_pEXTCSDRegister->CardType));        
        g_dwSupportHihgSpeed = TRUE;
#endif        
    }  
        
        
    return TRUE;
    
}

int SetPreDefine(UINT32 dwSector)
{
    while (TRUE)
    {
    	if (!IssueCommand(23, dwSector, 0))
    	{
     		return 0;
    	}
    	else
    	{
    		if(((g_vpHSMMCReg->RSPREG0>>9)&0xf) == 4)
    		{
    			//RETAILMSG(1,(TEXT("HSMMC:::Card is transfer status\n")));
    			return 1;
    		}
    		else
            {
                RETAILMSG(0,(TEXT("HSMMC:::###CMD13 is completed. But it is not trans state 0x%x\n"),
                    (g_vpHSMMCReg->RSPREG0>>9)&0xf));            
                return 0;
    		}
    	}
    }
	return TRUE;
}


void SetSystemAddressReg(UINT32 SysAddr)
{
	g_vpHSMMCReg->SYSAD = SysAddr;
}

int FusingTomoviNAND(UINT32 dwStartSector, UINT32 dwSector, UINT32 dwAddr)
{
    UINT32 j;
    UINT32 *BufferToFuse = (UINT32 *)dwAddr;

    ClockOnOff(1);    

    if (!IsCardInProgrammingState())
	{
        goto write_fail;
	}

    SetBlockSizeReg(7, 512); // Maximum DMA Buffer Size, Block Size
	SetBlockCountReg(dwSector); // Block Numbers to Write
    if (g_dwMMCSpec42 || g_dwIsSDHC)
        SetArgumentReg(dwStartSector);// Card Start Block Address to Write
    else
        SetArgumentReg(dwStartSector*512);// Card Start Block Address to Write

    SetTransferModeReg(1, 0, 1, 1, 0);
	if (WaitForCmdInhibit() == FALSE)
	{   
        RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] FusingTomoviNAND() DataInhibit not cleared.\n")));     
        goto write_fail;
	}        
	if (WaitForDataInhibit() == FALSE)
	{   
        RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] FusingTomoviNAND() DataInhibit not cleared.\n")));     
        goto write_fail;
	}    
    SetCommandReg(25, 0);    

	if (WaitForCommandComplete() == FALSE)
	{   
        RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] FusingTomoviNAND() Command not cleared.\n")));     
        goto write_fail;
	}
    if ( ClearCommandCompleteStatus() == FALSE )
    {
        RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] FusingTomoviNAND() Command status not cleared.\n")));
        goto write_fail;
    }  


    for(j=0; j<dwSector; j++)
    {
        if (!WaitForBufferWriteReady())
		{
            goto write_fail;
		}
  		else if ( ClearBufferWriteReadyStatus() == FALSE )
		{
            goto write_fail;
		}          

#if 0
        __try
        {
            PutData512((PUCHAR)BufferToFuse, &(g_vpHSMMCReg->BDATA));
        }

        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] FusingTomoviNAND() EXCEPTION is occured\n")));
            if (HSMMCInit()== FALSE)
                return FALSE;
            return TRUE;
        }	
#endif
        PutData512((PUCHAR)BufferToFuse, &(g_vpHSMMCReg->BDATA));
        BufferToFuse += (512);
    }

	if( WaitForTransferComplete() == FALSE)
	{
		RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] FusingTomoviNAND() Transfer NOT Complete %x %x %x %x %x\n"),
          g_vpHSMMCReg->NORINTSTS,g_vpHSMMCReg->NORINTSTSEN,g_vpHSMMCReg->ERRINTSTS,
          g_vpHSMMCReg->ERRINTSTSEN,g_vpHSMMCReg->BLKCNT));
        goto write_fail;;
	}
    if ( ClearTransferCompleteStatus() == FALSE )
    {
		RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] FusingTomoviNAND() Transfer status NOT cleared \n")));        
        goto write_fail;
    }

    //RETAILMSG(1,(TEXT("FusingTomoviNAND()-----------\n"))); 

    ClockOnOff(0);    
    return TRUE;

write_fail:
    
    return FALSE;
    
}

DWORD ReadFrommoviNAND(UINT32 dwStartSector, UINT32 dwSector, UINT32 dwAddr)
{
    UINT32 i,j;
    UINT32 *BufferToFuse = (UINT32 *)dwAddr;
    UINT32 dwIsAlign = TRUE;
    UINT32 AlignBuffer[1024], *AlignTemp;

    //RETAILMSG(1,(TEXT("#### ReadFrommoviNAND from %d to %d buffer 0x%x\r\n"),dwStartSector,dwSector,dwAddr));

    ClockOnOff(1);    

    if (dwAddr % 4 != 0 )
    {
        dwIsAlign = FALSE;
        AlignTemp = AlignBuffer;
    }

    if(!IsCardInProgrammingState())
	{
        goto read_fail;
	}

	SetBlockSizeReg(7, 512); // Maximum DMA Buffer Size, Block Size
	SetBlockCountReg(dwSector); // Block Numbers to Write
	if (g_dwMMCSpec42 || g_dwIsSDHC)
        	SetArgumentReg(dwStartSector);// Card Start Block Address to Write
    	else
        	SetArgumentReg(dwStartSector*512);// Card Start Block Address to Write

	SetTransferModeReg(1, 1, 1, 1, 0);
	if (WaitForCmdInhibit() == FALSE)
	{   
        RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] ReadFrommoviNAND() DataInhibit not cleared.\n")));     
        goto read_fail;
	}        
    if (WaitForDataInhibit() == FALSE)
	{   
        RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] ReadFrommoviNAND() DataInhibit not cleared.\n")));     
        goto read_fail;
	}        
	SetCommandReg(18, 0); // CMD18: Multi-Read

	if (WaitForCommandComplete() == FALSE)
	{   
        RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] ReadFrommoviNAND() Command not cleared.\n")));     
        goto read_fail;
	}
    if ( ClearCommandCompleteStatus() == FALSE )
    {
        RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] ReadFrommoviNAND() Command status not cleared.\n")));
        goto read_fail;
    }

    if ( dwSector > 8 && dwIsAlign == FALSE )
    {
        RETAILMSG(1,(TEXT("FATAL ERROR ALIGN IS NOT by 4Byte\n")));
        while(1);
    }
 
	for(j=0; j<dwSector; j++)
	{
		if (WaitForBufferReadReady() == FALSE)
		{
			RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] ReadFrommoviNAND() ReadBuffer NOT Ready\n")));
            goto read_fail;
		}
            
		else if ( ClearBufferReadReadyStatus() == FALSE )
		{
            RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] ReadFrommoviNAND() ReadBuffer status not cleared.\n")));
            goto read_fail;
		}

#if 0
        __try
        {
            GetData512((PUCHAR)BufferToFuse, &(g_vpHSMMCReg->BDATA));
        }

        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] ReadFrommoviNAND() EXCEPTION is occured\n")));
            if (HSMMCInit()== FALSE)
                return FALSE;
            return TRUE;
        }	
#endif    
        for(i=0; i<512/4; i++)//512 byte should be writed.
        {
            if ( dwIsAlign == FALSE)
            {
                *AlignTemp++ = g_vpHSMMCReg->BDATA;
            }
            else
                *BufferToFuse++ = g_vpHSMMCReg->BDATA;
            //dwCountBlock++;						
        }
        //GetData512((PUCHAR)BufferToFuse, &(g_vpHSMMCReg->BDATA));
        //BufferToFuse += (512);
	}

    if ( dwIsAlign == FALSE )
    {
        memcpy((PVOID)BufferToFuse,(PVOID)AlignBuffer,dwSector*512);
    }
	
	if( WaitForTransferComplete() == FALSE)
	{
		RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] ReadFrommoviNAND() Transfer NOT Complete\n")));
        goto read_fail;
	}
    if ( ClearTransferCompleteStatus() == FALSE )
    {
		RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] ReadFrommoviNAND() Transfer status NOT cleared\n")));        
        goto read_fail;
    }

    //RETAILMSG(1,(TEXT("ReadFrommoviNAND()-----------\n"))); 

    ClockOnOff(0);        
    return TRUE;

read_fail:

    ClockOnOff(0);    

    RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] ReadFrommoviNAND() FAIL OUT-----------\n"))); 
    return FALSE;

}

#ifdef FOR_HSMMCDRV
int FusingTomoviNAND_DMA(UINT32 dwStartSector, UINT32 dwSector, UINT32 dwAddr)
{
    UINT32 *BufferToFuse = (UINT32 *)dwAddr;
    UINT32 *dwTemp;
    UINT32 dwBufferFlag=FALSE;
    DWORD dwRet;

    ClockOnOff(1);

    if ( dwAddr % 4 != 0 ) // buffer address is not aligned by 4bytes.
    {
        RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] FusingTomoviNAND_DMA() pBuf is not aligned by 4\n")));
        dwTemp = (UINT32 *)malloc(dwSector * 512);
        BufferToFuse = dwTemp;
        dwBufferFlag = TRUE;
        memcpy ( (PVOID)dwTemp, (PVOID)dwAddr, dwSector * 512 );        
    }

    if (!IsCardInProgrammingState())
	{
        goto write_fail;
	}

    if (SetPreDefine(dwSector) == FALSE ) // for moviNAND
    {	
        RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] FusingTomoviNAND_DMA() SetPreDefine ERROR\n")));
    }

    g_vpHSMMCReg->NORINTSIGEN = (g_vpHSMMCReg->NORINTSIGEN & ~(0xffff)) | TRANSFERCOMPLETE_SIG_INT_EN ;
	SetSystemAddressReg(dwAddr);// AHB System Address For Write   
	
    SetBlockSizeReg(7, 512); // Maximum DMA Buffer Size, Block Size
	SetBlockCountReg(dwSector); // Block Numbers to Write
    if (g_dwMMCSpec42)
        SetArgumentReg(dwStartSector);// Card Start Block Address to Write
    else
        SetArgumentReg(dwStartSector*512);// Card Start Block Address to Write

     SetTransferModeReg(1, 0, 0, 1, 1);
	if (WaitForCmdInhibit() == FALSE)
	{   
        RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] FusingTomoviNAND_DMA() CmdInhibit not cleared.\n")));     
        goto write_fail;
	}        
	if (WaitForDataInhibit() == FALSE)
	{   
        RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] FusingTomoviNAND_DMA() DataInhibit not cleared.\n")));     
        goto write_fail;
	}    
    SetCommandReg(25, 0);    

	if (WaitForCommandComplete() == FALSE)
	{   
        RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] FusingTomoviNAND_DMA() Command not cleared.\n")));     
        goto write_fail;
	}
    if ( ClearCommandCompleteStatus() == FALSE )
    {
        RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] FusingTomoviNAND_DMA() Command status not cleared.\n")));
        goto write_fail;
    }  

    dwRet = WaitForSingleObject(g_hSDDMADoneEvent, 0x1000);

    if ( dwRet != WAIT_OBJECT_0 )
    {
        RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] FusingTomoviNAND_DMA() Transfer DONE Interrupt does not occur\n")));        
    }

    g_vpHSMMCReg->NORINTSIGEN = (g_vpHSMMCReg->NORINTSIGEN & ~(0xffff));
    InterruptDone(SYSINTR_HSMMC);        

    if ( ClearTransferCompleteStatus() == FALSE )
    {
		RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] FusingTomoviNAND_DMA() Transfer status NOT cleared\n")));        
        goto write_fail;
    }

    if ( dwBufferFlag == TRUE )
        free(dwTemp);

    //RETAILMSG(1,(TEXT("FusingTomoviNAND()-----------\n")));     
    //ClockOnOff(0);    

    return TRUE;

write_fail:
    g_vpHSMMCReg->NORINTSIGEN = (g_vpHSMMCReg->NORINTSIGEN & ~(0xffff));    
    if ( dwBufferFlag == TRUE )
        free(dwTemp);

    //RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] FusingTomoviNAND_DMA() FAIL OUT-----------\n"))); 
    //ClockOnOff(0);    
    return FALSE;
    
}

DWORD ReadFrommoviNAND_DMA(UINT32 dwStartSector, UINT32 dwSector, UINT32 dwAddr)
{
    UINT32 *BufferToFuse = (UINT32 *)dwAddr;
    UINT32 *dwTemp;
    UINT32 dwBufferFlag=FALSE;
    UINT32 temp=0;
    DWORD dwRet;

    ClockOnOff(1);    

    RETAILMSG(1,(TEXT("### HSMMC::READ MMC from 0x%x to 0x%x 0x%x\n"),dwStartSector,dwSector,dwAddr));    

    if ( dwAddr % 4 != 0 ) // buffer address is not aligned by 4bytes.
    {
        RETAILMSG(0,(TEXT("[HSMMCLIB:ERR] ReadFrommoviNAND_DMA() pBuf is not aligned by 4\n")));
        dwTemp = (UINT32 *)malloc(dwSector * 512);
        BufferToFuse = dwTemp;
        dwBufferFlag = TRUE;
    }

    temp = 0;
    while (!IsCardInProgrammingState())
	{
        if ( temp == 0 )
        {
            goto read_fail;
        }
        temp++;
	}

    if (SetPreDefine(dwSector) == FALSE ) // for moviNAND
    {	
        RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] ReadFrommoviNAND_DMA() SetPreDefine ERROR\n")));
        
    }

    g_vpHSMMCReg->NORINTSIGEN = (g_vpHSMMCReg->NORINTSIGEN & ~(0xffff)) | TRANSFERCOMPLETE_SIG_INT_EN ;
	SetSystemAddressReg(dwAddr);// AHB System Address For Write    

	SetBlockSizeReg(7, 512); // Maximum DMA Buffer Size, Block Size
	SetBlockCountReg(dwSector); // Block Numbers to Write
	if (g_dwMMCSpec42)
        	SetArgumentReg(dwStartSector);// Card Start Block Address to Write
    	else
        	SetArgumentReg(dwStartSector*512);// Card Start Block Address to Write

	SetTransferModeReg(1, 1, 0, 1, 1);
	if (WaitForCmdInhibit() == FALSE)
	{   
        RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] ReadFrommoviNAND_DMA() CmdInhibit not cleared.\n")));     
        goto read_fail;
	}        
    if (WaitForDataInhibit() == FALSE)
	{   
        RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] ReadFrommoviNAND_DMA() DataInhibit not cleared.\n")));     
        goto read_fail;
	}        
	SetCommandReg(18, 0); // CMD18: Multi-Read

	if (WaitForCommandComplete() == FALSE)
	{   
        RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] ReadFrommoviNAND_DMA() Command not cleared.\n")));     
        goto read_fail;
	}
    if ( ClearCommandCompleteStatus() == FALSE )
    {
        RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] ReadFrommoviNAND_DMA() Command status not cleared.\n")));
        goto read_fail;
    }


    dwRet = WaitForSingleObject(g_hSDDMADoneEvent, 0x1000);

    if ( dwRet != WAIT_OBJECT_0 )
    {
        RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] ReadFrommoviNAND_DMA() Transfer DONE Interrupt does not occur\n")));        
        RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] ReadFrommoviNAND_DMA() 0x%x 0x%x 0x%x 0x%x\n"),dwStartSector,dwSector,dwAddr,g_dwMMCSpec42));        
    }

    g_vpHSMMCReg->NORINTSIGEN = (g_vpHSMMCReg->NORINTSIGEN & ~(0xffff));
    InterruptDone(SYSINTR_HSMMC);    
   
    if ( ClearTransferCompleteStatus() == FALSE )
    {
		RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] ReadFrommoviNAND_DMA() Transfer status NOT cleared\n")));        
        goto read_fail;
    }


    if ( dwBufferFlag == TRUE )
    {
        memcpy ( (PVOID)dwAddr, (PVOID)dwTemp, dwSector * 512 );
        free(dwTemp);        
    }
    
    //ClockOnOff(0);       
    return TRUE;

read_fail:

    g_vpHSMMCReg->NORINTSIGEN = (g_vpHSMMCReg->NORINTSIGEN & ~(0xffff));

    if ( dwBufferFlag == TRUE )
        free(dwTemp);

    //ClockOnOff(0);
    RETAILMSG(1,(TEXT("[HSMMCLIB:ERR] ReadFrommoviNAND_DMA() FAIL OUT-----------\n"))); 
    return FALSE;

}

#endif


