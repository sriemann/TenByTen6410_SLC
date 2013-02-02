
#include <windows.h>
#include <nkintr.h>
#include <pm.h>
#include "pmplatform.h"
#include "Pkfuncs.h"
#include "s3c6410.h"
#include "gpio.h"


#define IO_CTL_GPIO_1_ON 0x01
#define IO_CTL_GPIO_2_ON 0x02
#define IO_CTL_GPIO_3_ON 0x03
#define IO_CTL_GPIO_4_ON 0x04
#define IO_CTL_GPIO_5_ON 0x05
#define IO_CTL_GPIO_6_ON 0x06
#define IO_CTL_GPIO_7_ON 0x07
#define IO_CTL_GPIO_8_ON 0x08
#define IO_CTL_GPIO_ALL_ON 0x09
#define IO_CTL_GPIO_1_OFF 0x06
#define IO_CTL_GPIO_2_OFF 0x07
#define IO_CTL_GPIO_3_OFF 0x08
#define IO_CTL_GPIO_4_OFF 0x09
#define IO_CTL_GPIO_5_OFF 0x0A
#define IO_CTL_GPIO_6_OFF 0x0B
#define IO_CTL_GPIO_7_OFF 0x0C
#define IO_CTL_GPIO_8_OFF 0x0D
#define IO_CTL_GPIO_ALL_OFF 0x0E


volatile IOPreg 	*s2440IOP = (IOPreg *)IOP_BASE;
volatile INTreg 	*s2440INT = (INTreg *)INT_BASE;



BOOL mInitialized;
void Virtual_Alloc();						// Virtual allocation



void Virtual_Alloc()
{
  // GPIO Virtual alloc
	s2440IOP = (volatile IOPreg *) VirtualAlloc(0,sizeof(IOPreg),MEM_RESERVE, PAGE_NOACCESS);
	if(s2440IOP == NULL) {
		RETAILMSG(1,(TEXT("GPIO driver : VirtualAlloc failed !\r\n")));
	}	else {
		if(!VirtualCopy((PVOID)s2440IOP,(PVOID)(IOP_BASE),sizeof(IOPreg),PAGE_READWRITE | PAGE_NOCACHE )) {
			RETAILMSG(1,(TEXT("GPIO driver : VirtualCopy failed !\r\n")));
		}
	}
}

// returns address of GPXCON register of the port 
volatile UINT32* getGPXCON(UCHAR portNumber) {
	volatile UINT32* gpxCon = 0;
	switch(portNumber) {
		case PORT_A:		// port A
			gpxCon = &s2440IOP->rGPACON;
			break;
		case PORT_B:		// port B
			gpxCon = &s2440IOP->rGPBCON;
			break;
		case PORT_C:		// port C
			gpxCon = &s2440IOP->rGPCCON;
			break;
		case PORT_D:		// port D
			gpxCon = &s2440IOP->rGPDCON;
			break;
		case PORT_E:		// port E
			gpxCon = &s2440IOP->rGPECON;
			break;
		case PORT_F:		// port F
			gpxCon = &s2440IOP->rGPFCON;
			break;
		case PORT_G:		// port G
			gpxCon = &s2440IOP->rGPGCON;
			break;
		case PORT_H:		// port H
			gpxCon = &s2440IOP->rGPHCON;
			break;
		default:
			RETAILMSG(1,(TEXT("GPIO : bad portNumber (getGPXCON) : %d\r\n"), portNumber));
			break;
	}
	return gpxCon;
}

// returns address of GPXDAT register of the port 
volatile UINT32* getGPXDAT(UCHAR portNumber) {
	volatile UINT32* gpxDat = 0;
	switch(portNumber) {
		case PORT_A:		// port A
			gpxDat = &s2440IOP->rGPADAT;
			break;
		case PORT_B:		// port B
			gpxDat = &s2440IOP->rGPBDAT;
			break;
		case PORT_C:		// port C
			gpxDat = &s2440IOP->rGPCDAT;
			break;
		case PORT_D:		// port D
			gpxDat = &s2440IOP->rGPDDAT;
			break;
		case PORT_E:		// port E
			gpxDat = &s2440IOP->rGPEDAT;
			break;
		case PORT_F:		// port F
			gpxDat = &s2440IOP->rGPFDAT;
			break;
		case PORT_G:		// port G
			gpxDat = &s2440IOP->rGPGDAT;
			break;
		case PORT_H:		// port H
			gpxDat = &s2440IOP->rGPHDAT;
			break;
		default:
			RETAILMSG(1,(TEXT("GPIO : bad portNumber (getGPXDAT) : %d\r\n"), portNumber));
			break;
	}
	return gpxDat;
}

// returns address of GPXDAT register of the port 
volatile UINT32* getGPXUP(UCHAR portNumber) {
	volatile UINT32* gpxUp = 0;
	switch(portNumber) {
		case PORT_A:		// port A
			gpxUp = 0;		// no pullup on port A
			break;
		case PORT_B:		// port B
			gpxUp = &s2440IOP->rGPBUP;
			break;
		case PORT_C:		// port C
			gpxUp = &s2440IOP->rGPCUP;
			break;
		case PORT_D:		// port D
			gpxUp = &s2440IOP->rGPDUP;
			break;
		case PORT_E:		// port E
			gpxUp = &s2440IOP->rGPEUP;
			break;
		case PORT_F:		// port F
			gpxUp = &s2440IOP->rGPFUP;
			break;
		case PORT_G:		// port G
			gpxUp = &s2440IOP->rGPGUP;
			break;
		case PORT_H:		// port H
			gpxUp = &s2440IOP->rGPHUP;
			break;
		default:
			RETAILMSG(1,(TEXT("GPIO : bad portNumber (getGPXUP) : %d\r\n"), portNumber));
			break;
	}
	return gpxUp;
}

// configuration of a pin
void pinConfiguration(GPIO_SET_PIN_CONFIGURATION* pPinConfig) {
	volatile UINT32* pGPXCON = getGPXCON(pPinConfig->portNumber);
	volatile UINT32* pGPXDAT = getGPXDAT(pPinConfig->portNumber);
	volatile UINT32* pGPXUP  = getGPXUP(pPinConfig->portNumber);

	if ((pGPXDAT != 0) && (pGPXCON != 0)) {
		switch (pPinConfig->pinConfiguration) {
			case OUTPUT:
				if (pPinConfig->pinValue == ON) {
					*pGPXDAT = *pGPXDAT & ~(0x1<<pPinConfig->pinNumber);										
				} else {
					*pGPXDAT = *pGPXDAT | (0x1<<pPinConfig->pinNumber);															
				}
				*pGPXCON =  ((*pGPXCON) &~(3 << (pPinConfig->pinNumber*2))) | (1<< (pPinConfig->pinNumber*2));	// pin as output
				break;
			case INPUT_WITH_PULLUP:
				if (pPinConfig->portNumber == PORT_A) {
					RETAILMSG(1, (TEXT("GPIO : PORT A can't be configured as input")));
				} else {
					*pGPXCON =  (*pGPXCON) &~(3 << (pPinConfig->pinNumber*2));		// pin as input
					*pGPXUP	 =  (*pGPXUP) & ~(0x1<<pPinConfig->pinNumber);			// pullup active
				}
				break;
			case INPUT_WITHOUT_PULLUP:
				if (pPinConfig->portNumber == PORT_A) {
					RETAILMSG(1, (TEXT("GPIO : PORT A can't be configured as input")));
				} else {
					*pGPXCON =  (*pGPXCON) &~(3 << (pPinConfig->pinNumber*2));		// pin as input
					*pGPXUP	 =  (*pGPXUP) | (0x1<<pPinConfig->pinNumber);				// pullup disabled
				}
				break;
		}
	} else {
		RETAILMSG(1, (TEXT("GPIO : Bad port number")));			
	}
}

void pinSetValue(GPIO_SET_PIN_OUTPUT_VALUE* pPinValue) {
	volatile UINT32* pGPXDAT = getGPXDAT(pPinValue->portNumber);

	if (pGPXDAT != 0) {
		if (pPinValue->pinValue == ON) {
			*pGPXDAT = *pGPXDAT & ~(0x1<<pPinValue->pinNumber);										
		} else {
			*pGPXDAT = *pGPXDAT | (0x1<<pPinValue->pinNumber);															
		}				
	} else {
		RETAILMSG(1, (TEXT("GPIO : Bad port number")));			
	}
}

void pinGetValue(GPIO_GET_PIN_INPUT_VALUE* pPinValue) {
	volatile UINT32* pGPXDAT = getGPXDAT(pPinValue->portNumber);

	if (pGPXDAT != 0) {
		if (pPinValue->portNumber == PORT_A) {
			RETAILMSG(1, (TEXT("GPIO : PORT A can't be configured as input")));
		} else {
			if (*pGPXDAT & 0x1<<pPinValue->pinNumber) {
				pPinValue->pinValue = ON;
			} else {
				pPinValue->pinValue = OFF;
			}
		}
	} else {
		RETAILMSG(1, (TEXT("GPIO : Bad port number")));			
	}

}

BOOL WINAPI DllEntry(HANDLE	hinstDLL, DWORD dwReason, LPVOID /* lpvReserved */)
{
	switch(dwReason)
	{
	case DLL_PROCESS_ATTACH:
		DEBUGREGISTER((HINSTANCE)hinstDLL);
		return TRUE;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		break;
#ifdef UNDER_CE
	case DLL_PROCESS_EXITING:
		break;
	case DLL_SYSTEM_STARTED:
		break;
#endif
	}

	return TRUE;
}


BOOL GPI_Deinit(DWORD hDeviceContext)
{
	RETAILMSG(1,(TEXT("GPIO: GPIO_Deinit\r\n")));
	return TRUE;
} 

DWORD GPI_Init(DWORD dwContext)
{
	
	RETAILMSG(1,(TEXT("Chargement du driver GPIO...\r\n")));

	Virtual_Alloc();
	
	mInitialized = TRUE;
	return TRUE;
}

	
BOOL GPI_IOControl(DWORD hOpenContext, 
				   DWORD dwCode, 
				   PBYTE pBufIn, 
				   DWORD dwLenIn, 
				   PBYTE pBufOut, 
				   DWORD dwLenOut, 
				   PDWORD pdwActualOut)
{
	switch(dwCode)
	{
	case IOCTL_GPIO_SET_PIN_CONFIGURATION:
		if (dwLenIn != sizeof(GPIO_SET_PIN_CONFIGURATION)) {
			RETAILMSG(1, (TEXT("GPIO driver : IOCTL_GPIO_SET_PIN_CONFIGURATION : bad dwLenIn value !\r\n"))); 
		} else {
			pinConfiguration((GPIO_SET_PIN_CONFIGURATION*)pBufIn);
		}
		break;
	case IOCTL_GPIO_SET_PIN_OUTPUT_VALUE:
		if (dwLenIn != sizeof(GPIO_SET_PIN_OUTPUT_VALUE)) {
			RETAILMSG(1, (TEXT("GPIO driver : IOCTL_GPIO_SET_PIN_OUTPUT_VALUE : bad dwLenIn value !\r\n"))); 
		} else {
			pinSetValue((GPIO_SET_PIN_OUTPUT_VALUE*)pBufIn);
		}
		break;
	case IOCTL_GPIO_GET_PIN_INPUT_VALUE:
		if (dwLenIn != sizeof(GPIO_GET_PIN_INPUT_VALUE)) {
			RETAILMSG(1, (TEXT("GPIO driver : IOCTL_GPIO_GET_PIN_INPUT_VALUE : bad dwLenIn value !\r\n"))); 
		} else {
			pinGetValue((GPIO_GET_PIN_INPUT_VALUE*)pBufIn);
		}
		break;
	default:
		break;		
	}
 	return TRUE;
} 

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DWORD GPI_Open(DWORD hDeviceContext, DWORD AccessCode, DWORD ShareMode)
{
	RETAILMSG(1,(TEXT("GPIO : GPIO_Open\r\n")));
	return TRUE;
} 

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
BOOL GPI_Close(DWORD hOpenContext)
{
	RETAILMSG(1,(TEXT("GPIO : GPIO_Close\r\n")));
	return TRUE;
} 

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void GPI_PowerDown(DWORD hDeviceContext)
{
	RETAILMSG(1,(TEXT("GPIO : GPIO_PowerDown\r\n")));
	} 

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void GPI_PowerUp(DWORD hDeviceContext)
{
	RETAILMSG(1,(TEXT("GPIO : GPIO_PowerUp\r\n")));
} 

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DWORD GPI_Read(DWORD hOpenContext, LPVOID pBuffer, DWORD Count)
{
	RETAILMSG(1,(TEXT("GPIO : GPIO_Read\r\n")));
	return TRUE;
} 

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DWORD GPI_Seek(DWORD hOpenContext, long Amount, DWORD Type)
{
	RETAILMSG(1,(TEXT("GPIO : GPIO_Seek\r\n")));
	return 0;
} 

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DWORD GPI_Write(DWORD hOpenContext, LPCVOID pSourceBytes, DWORD NumberOfBytes)
{
	RETAILMSG(1,(TEXT("GPIO : GPIO_Write\r\n")));
	return 0;
}




