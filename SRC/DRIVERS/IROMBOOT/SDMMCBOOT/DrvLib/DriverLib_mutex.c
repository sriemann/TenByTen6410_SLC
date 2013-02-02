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
#include <bsp_cfg.h>
#include <DrvLib.h>

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

HANDLE InitMutex(LPCTSTR name)
{
   HANDLE hMutex;

   hMutex = CreateMutex (
            NULL,                             // No security attributes
            FALSE,                            // Initially not owned
            name);  // Name of mutex object

   if (NULL == hMutex)
   {
      // Your code to deal with the error goes here.
      RETAILMSG(1, (_T("[DRVLIB]Cann't initialize mutex %s\r\n"),name));
   }

   return hMutex;
}

DWORD GetMutex(HANDLE handle)
{
   DWORD result;

   result = WaitForSingleObject( handle, INFINITE);

   switch(result)
   {
      case WAIT_OBJECT_0:
         break;
      case WAIT_TIMEOUT:
         RETAILMSG(1, (_T("[DRVLIB]ERROR:Mutex Timeout\r\n")));
         break;
      case WAIT_ABANDONED:
         RETAILMSG(1, (_T("[DRVLIB]ERROR:Mutex abandoned\r\n")));
         break;
      default:
         RETAILMSG(1, (_T("[DRVLIB]ERROR:others\r\n")));
         break;
   }
   return result;
}

