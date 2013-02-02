//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this sample source code is subject to the terms of the Microsoft
// license agreement under which you licensed this sample source code. If
// you did not accept the terms of the license agreement, you are not
// authorized to use this sample source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the LICENSE.RTF on your install media or the root of your tools installation.
// THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
//------------------------------------------------------------------------------
//
//  Header: sdfat.h
//
#ifndef SDFAT_H
#define SDFAT_H

#include <windows.h>


// signal error for function which return DWORD value
#define FAT_ERROR ((DWORD)-1)


// disk function
BOOL  FATInitDisk();
DWORD FATGetDiskSize();
DWORD FATReadDisk(PVOID pBuffer, DWORD dwLength, DWORD dwPos);


// partition function
DWORD FATGetPartitionNumber();
DWORD FATGetPartitionSize(DWORD dwPartNo);
DWORD FATReadPartition(DWORD dwPartNo, PVOID pBuffer, DWORD dwLength, DWORD dwPos);


// file function
typedef struct _FILEINFO{ // keep track file property
    DWORD PartitionNumber;
    DWORD StartingCluster; // 0 means root
    DWORD FileLength;
    DWORD FileAttribute;
    DWORD CurrentCluster;
    DWORD CurrentPostion;
}FILEINFO, *PFILEINFO;

BOOL  FATOpenFile(PFILEINFO pFileInfo, PCSTR pFileName);
BOOL  FATCloseFile(PFILEINFO pFileInfo); // close file isn't necessary

BOOL  FATIsReadOnly(PFILEINFO pFileInfo);
BOOL  FATIsHidden(PFILEINFO pFileInfo);
BOOL  FATIsSystem(PFILEINFO pFileInfo);
BOOL  FATIsVolumeLabel(PFILEINFO pFileInfo);
BOOL  FATIsDirectory(PFILEINFO pFileInfo);
BOOL  FATIsArchive(PFILEINFO pFileInfo);

BOOL  FATEndOfFile(PFILEINFO pFileInfo);
DWORD FATGetFileSize(PFILEINFO pFileInfo);

DWORD FATGetFilePos(PFILEINFO pFileInfo);
BOOL  FATSetFilePos(PFILEINFO pFileInfo, DWORD dwPos);

DWORD FATReadFile(PFILEINFO pFileInfo, PVOID pBuffer, DWORD dwLength);
DWORD FATReadFileEx(PFILEINFO pFileInfo, PVOID pBuffer, DWORD dwLength, DWORD dwPos);

// dump function for testing only
VOID DumpMasterBootRecord();
VOID DumpPartitionBootSector(DWORD dwPartNo);
VOID DumpFATTable(DWORD dwPartNo, DWORD dwStart, DWORD dwCount);
VOID DumpDirectory(PCSTR pDirName, DWORD dwStart, DWORD dwCount);
VOID DumpDirectoryEx(PCSTR pDirName);

#endif // SDFAT_H
