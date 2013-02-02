#include <bsp.h>
#include <ethdbg.h>
#include <fmd.h>
//#include "FAT.h"
//#include "..\loader.h"
#include "loader.h"


#define LST_BUFFER_SIZE 8096

#define MAX_FILE_NUM	32
#define MAX_FILE_NAME   32

#define DBG_UPGRADE_SDIMG   1

#define GPIOG_NCD_SD                (1 << 10)
#define GPIOGCON_NCD_SD             (3 << 20)

#define BIN_HEADER_LENGTH			7
//#define DEF_DOWNLOAD_ADDR           0x32000000
#define DEF_DOWNLOAD_ADDR           0xA3000000


#define NB0IMAGE	0
#define BINIMAGE	1
#define LSTIMAGE	2
#define UBIIMAGE	3
#define DIOIMAGE	4

#define MAX_FILE_NUM	32
#define MAX_PATH        260

volatile UINT32 		readPtIndex;
volatile UINT8			*g_pDownPt;

extern DWORD        g_ImageType;

extern BOOL DownloadImage (LPDWORD pdwImageStart, LPDWORD pdwImageLength, LPDWORD pdwLaunchAddr);
BOOL Parsing ( const char *sFileName, UINT32 dwImageType, BYTE *Buffer );
void BigToSmall(LPBYTE pAlphabet);
DWORD ReadFrommoviNAND(UINT32 dwStartSector, UINT32 dwSector, UINT32 dwAddr);

extern int FATGetFileSize ( const char *sFileName, BYTE *Buffer ); // to support lst file 080609 hsjang
extern BOOL FATReadFile ( const char *sFileName, BYTE *Buffer );
extern UINT32 FATFileSize (void);
extern BOOL FATInit (void);


char strLSTFileBuffer[LST_BUFFER_SIZE];

void uSleep(UINT32 cnt)
{
	volatile UINT32 i, j;
	for ( i = 0; i < cnt; i++ )
	{
		for ( j=0; i < 10; j++ ){};
	}
}

BOOL mmc_read ( UINT32 nDev, UINT32 dwStartSector, UINT32 dwAddr, UINT32 dwSector )
{
	ReadFrommoviNAND(dwStartSector, dwSector, dwAddr);
	return TRUE;
}

BOOL IsCardInserted(void)
{
    BOOL        result = FALSE;
    int         count = 0;
    count++;

	EdbgOutputDebugString ( "IsCardInserted\r\n" );
/*	
    while((count % 3) != 0)
    {
		EdbgOutputDebugString ( "while...\r\n" );
        s24500IOP->GPFCON &= ~(0x3<<2);   // as input
//        uSleep(10000);

        if ( s24500IOP->GPFDAT & (1<<1))
        {
            if (result != FALSE)
                count = 0;
            result = FALSE;
			EdbgOutputDebugString ( "SD Card is NOT detected\r\n" );
        }
        else
        {
            if (result != TRUE)
                count = 0;
            result = TRUE;
			EdbgOutputDebugString ( "SD Card is detected\r\n" );
        }
        count++;
    }

    return result;
*/
    return TRUE;
}

#pragma optimize ("",off)
BOOL Parsing ( const char *sFileName, UINT32 dwImageType, BYTE *Buffer )
{
	ULONG nFileNumber;
    ULONG i, j;
	BYTE * ptxBuf;
	unsigned int nCheckSum = 0;
    ULONG fileSize;
	char szBinFileName[MAX_FILE_NUM][MAX_FILE_NAME];

	if ( dwImageType == LSTIMAGE )
	{
#if 1
        for ( i = 0 ; i < LST_BUFFER_SIZE ; i++)
            strLSTFileBuffer[i] = (char)(NULL);


        if ( FATGetFileSize(sFileName,NULL) > LST_BUFFER_SIZE )
        {
            EdbgOutputDebugString("%s file contain more thatn %d bytes\n",sFileName, LST_BUFFER_SIZE);
            return FALSE;
        }

        if ( !FATReadFile(sFileName,strLSTFileBuffer))
        {
            EdbgOutputDebugString("%s file Read Error\n",sFileName);
            return FALSE;
        }

		// Read chain.lst file
		for ( nFileNumber = 0,j=0; nFileNumber < MAX_FILE_NUM; nFileNumber++ )
		{
            for ( i = 0 ; i < MAX_FILE_NAME; )
            {
                if ( strLSTFileBuffer[j] == '\0' ) 
                {
                    szBinFileName[nFileNumber][i] = '\0';
                    j = LST_BUFFER_SIZE;
                    break;
                }
                else if ( (strLSTFileBuffer[j] == '\r') || (strLSTFileBuffer[j] == '\n') || (strLSTFileBuffer[j] == '\t') || (strLSTFileBuffer[j] == '+'))
                {
                    j++;
                    if ( i == 0 )
                        continue;
                    else
                    {
                        szBinFileName[nFileNumber][i] = '\0';                        
                        break;
                    }
                }
    			szBinFileName[nFileNumber][i] = strLSTFileBuffer[j];
                j++;
                i++;
            }
            
            if ( j == LST_BUFFER_SIZE )
                break;
		}

		*((BYTE *)Buffer+0)=0x4E;
		*((BYTE *)Buffer+1)=0x30;
		*((BYTE *)Buffer+2)=0x30;
		*((BYTE *)Buffer+3)=0x30;
		*((BYTE *)Buffer+4)=0x46;
		*((BYTE *)Buffer+5)=0x46;
		*((BYTE *)Buffer+6)=0xa;

		ptxBuf = Buffer  + 7 /* X000FF\n */
						+ 4 /* check sum */
						+ 4 /* num Regions */
						+ nFileNumber*(8+260); /* start address + length */
        
        
		for ( i = 0; i < nFileNumber; i++ )
		{
            BigToSmall(szBinFileName[i]);
            EdbgOutputDebugString("%dth File Read %s AT 0x%x\n",i, szBinFileName[i],ptxBuf);   
            
            fileSize = FATGetFileSize (szBinFileName[i],NULL);
                
            if ( !fileSize )
            {
                EdbgOutputDebugString("%s file Get size Error\n",szBinFileName[i]);
                return FALSE;
            }
            
            if ( !FATReadFile(szBinFileName[i],ptxBuf))
            {
                EdbgOutputDebugString("%s file Read Error\n",szBinFileName[i]);
                return FALSE;
            }

			for ( j = 0; j < 8 + 260; j++ )
			{
				if ( j < 8 )
				{
					*((BYTE *)Buffer+15+(i*(8+260)+j))=(BYTE)(ptxBuf[7+j]);
					nCheckSum += (BYTE)(ptxBuf[7+j]);
				}
				else if ( j >= 8 && j < (8+strlen(szBinFileName[i])) )
				{
					*((BYTE *)Buffer+15+(i*(8+260)+j))=(BYTE)(szBinFileName[i][j-8]);
					nCheckSum += (BYTE)(szBinFileName[i][j-8]);
				}
				else
				{
					*((BYTE *)Buffer+15+(i*(8+260)+j))=0;
				}
			}

			ptxBuf += fileSize;
		}

        
		ptxBuf = Buffer + 7;

		//*((DWORD *)ptxBuf+0)=nCheckSum;   //checksum
		//*((DWORD *)ptxBuf+1)=nFileNumber;   //checksum

        memcpy(ptxBuf, (PVOID)(&nCheckSum), 4);
        memcpy((PVOID)(ptxBuf+4), (PVOID)(&nFileNumber), 4);
        
#endif
	}
	else if ( dwImageType == UBIIMAGE || dwImageType == BINIMAGE )
	{
		FATReadFile(sFileName, Buffer);
		g_pDownPt += FATFileSize ();
	}
	else if ( dwImageType == NB0IMAGE || dwImageType == DIOIMAGE )
	{
		nFileNumber = 1;

//		for ( i = 0; i < 7+4+4+4+4+MAX_PATH; i++ )
//		{
//			*((BYTE *)(Buffer + i)) = 0;
//		}
		memset((void *)Buffer, 0, 7+4+4+4+4+MAX_PATH);

		ptxBuf = Buffer;
		
		*(ptxBuf++)=0x4E;
		*(ptxBuf++)=0x30;
		*(ptxBuf++)=0x30;
		*(ptxBuf++)=0x30;
		*(ptxBuf++)=0x46;
		*(ptxBuf++)=0x46;
		*(ptxBuf++)=0xa;

		// Read nb0 file
		if (!FATReadFile(sFileName, (BYTE *)(Buffer+7+4+4+4+4+MAX_PATH)))
		{
            RETAILMSG(1,(TEXT("#### File READ ERROR\r\n")));
            while(1);
		}

		ptxBuf = Buffer + 7 + 4 + 4;

		*(ptxBuf+0) = 0;				//nb0 start address == 0
		*(ptxBuf+1) = 0;				//nb0 start address == 0
		*(ptxBuf+2) = 0;				//nb0 start address == 0
		*(ptxBuf+3) = 0;				//nb0 start address == 0
		fileSize = FATFileSize();		//nb0 filesize
		*(ptxBuf+4) = (BYTE)((fileSize >> 0) & 0xff);
		*(ptxBuf+5) = (BYTE)((fileSize >> 8) & 0xff);
		*(ptxBuf+6) = (BYTE)((fileSize >> 16) & 0xff);
		*(ptxBuf+7) = (BYTE)((fileSize >> 24) & 0xff);

		strcpy((char *)(ptxBuf+8), sFileName);

		nCheckSum = 0;
		for ( i = 0; i < 4+4+MAX_PATH; i++ )
		{
			nCheckSum += (unsigned char)(*(ptxBuf+i));
		}

		ptxBuf = Buffer+7;

		*(ptxBuf+0) = (BYTE)((nCheckSum >> 0) & 0xff);
		*(ptxBuf+1) = (BYTE)((nCheckSum >> 8) & 0xff);
		*(ptxBuf+2) = (BYTE)((nCheckSum >> 16) & 0xff);
		*(ptxBuf+3) = (BYTE)((nCheckSum >> 24) & 0xff);
		
		*(ptxBuf+4) = (BYTE)((nFileNumber >> 0) & 0xff);
		*(ptxBuf+5) = (BYTE)((nFileNumber >> 8) & 0xff);
		*(ptxBuf+6) = (BYTE)((nFileNumber >> 16) & 0xff);
		*(ptxBuf+7) = (BYTE)((nFileNumber >> 24) & 0xff);
//		while(1);
		g_pDownPt += (7+4+4+4+4+MAX_PATH);
		g_pDownPt += FATFileSize();
	}
    return TRUE;
}
#pragma optimize ("",on)


void ChooseImageFromSD()
{
    BYTE KeySelect = 0;
   
	EdbgOutputDebugString ( "\r\nChoose Download Image:\r\n\r\n" );
	EdbgOutputDebugString ( "0) stepldr.nb0\r\n" );
	EdbgOutputDebugString ( "1) EBOOT.BIN\r\n" );
	EdbgOutputDebugString ( "2) nk.bin\r\n" );
	EdbgOutputDebugString ( "3) chain.lst\r\n" );    
	//EdbgOutputDebugString ( "4) Everything (block0img.nb0 + eboot.bin + nk.bin)\r\n" );
	//EdbgOutputDebugString ( "5) Everything (block0img.nb0 + eboot.bin + chain.lst)\r\n" );    
    EdbgOutputDebugString ( "\r\nEnter your selection: ");
	
    while (!(((KeySelect >= '0') && (KeySelect <= '3'))))
    {
        KeySelect = OEMReadDebugByte();
    }

    EdbgOutputDebugString ( "%c\r\n", KeySelect);

    g_pDownPt = (UINT8 *)DEF_DOWNLOAD_ADDR;
    readPtIndex = (UINT32)DEF_DOWNLOAD_ADDR;

    if (IsCardInserted())
    {
		FATInit();
		
	    switch(KeySelect)
	    {
	    case '0':
			Parsing("stepldr.nb0", NB0IMAGE, (BYTE *)DEF_DOWNLOAD_ADDR);
	        break;
	    case '1':
			Parsing("Eboot.bin", BINIMAGE, (BYTE *)DEF_DOWNLOAD_ADDR);
	        break;
	    case '2':
		//	Parsing("OS.BIN", BINIMAGE, (BYTE *)DEF_DOWNLOAD_ADDR);
    		Parsing("nk.bin", BINIMAGE, (BYTE *)DEF_DOWNLOAD_ADDR);
	        break;
	    case '3':
		//	Parsing("OS.BIN", BINIMAGE, (BYTE *)DEF_DOWNLOAD_ADDR);
    		Parsing("chain.lst", LSTIMAGE, (BYTE *)DEF_DOWNLOAD_ADDR);
	        break;

    	EdbgOutputDebugString ( "Read Finished \r\n");
    	
    	EdbgOutputDebugString ( "g_pDownPt = 0x%x \r\n", g_pDownPt);
	    }
    }
}

void BL1ReadFromSD()
{
    
    g_pDownPt = (UINT8 *)DEF_DOWNLOAD_ADDR;
    readPtIndex = (UINT32)DEF_DOWNLOAD_ADDR;

    if (IsCardInserted())
    {
		FATInit();
		
		Parsing("stepldr.nb0", NB0IMAGE, (BYTE *)DEF_DOWNLOAD_ADDR);
    	
    	EdbgOutputDebugString ( "g_pDownPt = 0x%x \r\n", g_pDownPt);
	}
}

void MULTIXIPReadFromSD()
{

    g_pDownPt = (UINT8 *)DEF_DOWNLOAD_ADDR;
    readPtIndex = (UINT32)DEF_DOWNLOAD_ADDR;

    if (IsCardInserted())
    {
		//FATInit();
		
        Parsing("chain.lst", LSTIMAGE, (BYTE *)DEF_DOWNLOAD_ADDR);
    	
    	EdbgOutputDebugString ( "g_pDownPt = 0x%x \r\n", g_pDownPt);
	}
}

void NKReadFromSD()
{
    
    g_pDownPt = (UINT8 *)DEF_DOWNLOAD_ADDR;
    readPtIndex = (UINT32)DEF_DOWNLOAD_ADDR;

    if (IsCardInserted())
    {
		//FATInit();
		
        Parsing("nk.bin", BINIMAGE, (BYTE *)DEF_DOWNLOAD_ADDR);
    	
    	EdbgOutputDebugString ( "g_pDownPt = 0x%x \r\n", g_pDownPt);
	}
}


void EbootReadFromSD()
{

    memset((PVOID)DEF_DOWNLOAD_ADDR,0xff,0x100000);
    g_pDownPt = (UINT8 *)DEF_DOWNLOAD_ADDR;
    readPtIndex = (UINT32)DEF_DOWNLOAD_ADDR;

    if (IsCardInserted())
    {
		//FATInit();
		
		Parsing("Eboot.bin", BINIMAGE, (BYTE *)DEF_DOWNLOAD_ADDR);
    	
    	EdbgOutputDebugString ( "g_pDownPt = 0x%x \r\n", g_pDownPt);
	}
}



BOOL SDReadData(DWORD cbData, LPBYTE pbData)
{
   	UINT8* pbuf = NULL;

    //EdbgOutputDebugString ("#### check point SDintf #1 cbdata = 0x%x pbData = 0x%x readPt = 0x%x\r\n",cbData,pbData,readPtIndex);    

	while(1)
	{
      
		if (1)// ((UINT32)g_pDownPt >= readPtIndex + cbData ) it has already copied data from SD card.
		{
			pbuf = (PVOID)readPtIndex;
			memcpy((PVOID)pbData, pbuf, cbData);
			//pbuf = (PVOID)OALPAtoUA(readPtIndex);
			// clear partial download memory to 0xff because data is already copied to buffer(pbData)
			memset(pbuf, 0xff, cbData);
            readPtIndex += cbData;
			break;
		}
	}

	return TRUE;
}

void BigToSmall(LPBYTE pAlphabet)
{
    int i;

    for ( i = 0 ; pAlphabet[i] != '\0' ; i++)
    {
        if ( pAlphabet[i] >= 65 && pAlphabet[i] <= 90 )
            pAlphabet[i] += 32;
    }
}

