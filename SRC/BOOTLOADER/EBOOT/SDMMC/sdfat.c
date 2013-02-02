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
//  File:  sdfat.c implements FAT parser
//
#include <sdfat.h>
#include <sdmdd.h>
#include <oal.h>

// file system struct must be one byte alignment
#pragma pack(push, 1)

// detailed info can reference:
// 1.The MultiMediaCard System Specification Version 3.31 Chapter 12
// 2.SD Memory Card Specifications Part 2 FILE SYSTEM SPECIFICATION Version 1.0
// 3.ISO/IEC 9293:1994 Volume and file structure of disk cartridges for information interchange
// 4 Microsoft Extensible Firmware Initiative FAT32 File System Specification

#define DEFAULT_SECTOR_SIZE 512

// partition table
#define PARTITION_NUMBER 4    // for MBR

#define PARTITION_NORMAL 0x00 // BootIndicator
#define PARTITION_SYSTEM 0x80

#define PARTITION_EMPTY  0x00 // SystemId
#define PARTITION_FAT12  0x01
#define PARTITION_FAT16  0x04
#define PARTITION_EXT    0x05
#define PARTITION_FAT16B 0x06
#define PARTITION_FAT32  0x0B
#define PARTITION_FAT32L 0x0C
#define PARTITION_FAT16L 0x0E
#define PARTITION_EXTL   0x0F

typedef __unaligned struct _PARTITION_TABLE{
    UINT8  BootIndicator;
    UINT8  StartingHead;
    UINT16 StartingSector:6;
    UINT16 StartingCylinder:10;
    UINT8  SystemId;
    UINT8  EndingHead;
    UINT16 EndingSector:6;
    UINT16 EndingCylinder:10;
    UINT32 RelativeSector;
    UINT32 TotalSector;
}PARTITION_TABLE, *PPARTITION_TABLE;


// master boot record(MBR)
#define MASTERBOOT_CODELEN   446
#define MASTERBOOT_SIGNATURE 0xAA55

typedef __unaligned struct _MASTERBOOT_RECORD{
    UINT8           BootCode[MASTERBOOT_CODELEN];
    PARTITION_TABLE Partition[PARTITION_NUMBER];
    UINT16          Signature;
}MASTERBOOT_RECORD, *PMASTERBOOT_RECORD;


// partition boot sector
#define RAWPARTITION_JUMPCODELEN    3
#define RAWPARTITION_CREATORIDLEN   8
#define RAWPARTITION_VOLUMELABLELEN 11
#define RAWPARTITION_FILESYSTEMLEN  8
#define RAWPARTITION_RESERVED1LEN   12
#define RAWPARTITION_RESERVED2LEN   420

#define RAWPARTITION_SIGNATURE      0xAA55
#define RAWPARTITION_EFDCSIGNATURE  0x29

typedef __unaligned struct _RAWPARTITION{
    UINT8  JumpCode[RAWPARTITION_JUMPCODELEN]; // FDC part
    UINT8  CreatorID[RAWPARTITION_CREATORIDLEN];
    UINT16 SectorSize;
    UINT8  SectorPerCluster;
    UINT16 ReservedSector;
    UINT8  NumberOfFAT;
    UINT16 NumberOfRootEntry;
    UINT16 TotalSector;
    UINT8  MediumId;
    UINT16 SectorPerFAT;
    UINT16 SectorPerTrack;
    UINT16 NumberOfSide;
    UINT32 NumberOfHiddenSector;              // EFDC part
    UINT32 TotalSector2; // when TotalSector==0, use this
    union{
        __unaligned struct{ // FAT12/FAT16
            UINT8  PhysicalDiskNumber;
            UINT8  Reserved;
            UINT8  EFDCSignature;
            UINT32 VolumeSerialNumber;
            UINT8  VolumeLabel[RAWPARTITION_VOLUMELABLELEN];
            UINT8  FileSystemType[RAWPARTITION_FILESYSTEMLEN];
        };
        __unaligned struct{ // FAT32
            UINT32 SectorPerFAT;
            UINT16 ExtentFlag;
            UINT16 FileSystemVersion;
            UINT32 RootEntryCluster;
            UINT16 FileSystemInfomation;
            UINT16 BackupBootSector;
            UINT8  Reserved1[RAWPARTITION_RESERVED1LEN];
            UINT8  PhysicalDiskNumber;
            UINT8  Reserved;
            UINT8  EFDCSignature;
            UINT32 VolumeSerialNumber;
            UINT8  VolumeLabel[RAWPARTITION_VOLUMELABLELEN];
            UINT8  FileSystemType[RAWPARTITION_FILESYSTEMLEN];
        }FAT32;
    };
    UINT8  Reserved2[RAWPARTITION_RESERVED2LEN];
    UINT16 Signature;
}RAWPARTITION, *PRAWPARTITION;


// FAT12 entry
#define FAT12_BITS 12

#define FAT12_BYTES2ENTRIES(Bytes)   ((8*(Bytes))/FAT12_BITS)
#define FAT12_ENTRIES2BYTES(Entries) ((FAT12_BITS*(Entries))/8)

#define FAT12_FREE   0x000
#define FAT12_ALLOC  0x002 // to FATEntryNumber-1
#define FAT12_DEFECT 0xFF7
#define FAT12_END    0xFF8 // to 0xFFF

#define FAT12_ISFREE(Cluster)          ((Cluster)==FAT12_FREE)
#define FAT12_ISALLOC(Cluster, Number) ((Cluster)>=FAT12_ALLOC&&(Cluster)<(Number))
#define FAT12_ISDEFECT(Cluster)        ((Cluster)==FAT12_DEFECT)
#define FAT12_ISEND(Cluster)           ((Cluster)>=FAT12_END&&(Cluster)<=0xFFF)

// help macro to implement FAT12_GETENTRY, don't call them directly
#define FAT12_ISEVENENTRY(Cluster)           (Cluster%2==0)
#define FAT12_GETEVENENTRY(FatTable, Offset) (((FatTable[Offset+1]&0x0F)<<8)|FatTable[Offset])
#define FAT12_GETODDENTRY(FatTable, Offset)  ((FatTable[Offset+1]<<4)|(FatTable[Offset]>>4))

#define FAT12_GETENTRY(FatTable, Start, Cluster)                                                         \
    (FAT12_ISEVENENTRY((Cluster))                                                                        \
    ?FAT12_GETEVENENTRY(((UINT8*)(FatTable)), (FAT12_ENTRIES2BYTES(Cluster)-FAT12_ENTRIES2BYTES(Start))) \
    :FAT12_GETODDENTRY(((UINT8*)(FatTable)), (FAT12_ENTRIES2BYTES(Cluster)-FAT12_ENTRIES2BYTES(Start))))


// FAT16 entry
#define FAT16_BITS 16

#define FAT16_BYTES2ENTRIES(Bytes)   ((8*(Bytes))/FAT16_BITS)
#define FAT16_ENTRIES2BYTES(Entries) ((FAT16_BITS*(Entries))/8)

#define FAT16_FREE   0x0000
#define FAT16_ALLOC  0x0002 // to FATEntryNumber-1
#define FAT16_DEFECT 0xFFF7
#define FAT16_END    0xFFF8 // to 0xFFFF

#define FAT16_ISFREE(Cluster)          ((Cluster)==FAT16_FREE)
#define FAT16_ISALLOC(Cluster, Number) ((Cluster)>=FAT16_ALLOC&&(Cluster)<(Number))
#define FAT16_ISDEFECT(Cluster)        ((Cluster)==FAT16_DEFECT)
#define FAT16_ISEND(Cluster)           ((Cluster)>=FAT16_END&&(Cluster)<=0xFFFF)

#define FAT16_GETENTRY(FatTable, Start, Cluster) \
    (*(UINT16*)(((UINT8*)(FatTable))+(FAT16_ENTRIES2BYTES(Cluster)-FAT16_ENTRIES2BYTES(Start))))


// FAT32 entry
#define FAT32_BITS 32

#define FAT32_BYTES2ENTRIES(Bytes)   ((8*(Bytes))/FAT32_BITS)
#define FAT32_ENTRIES2BYTES(Entries) ((FAT32_BITS*(Entries))/8)

#define FAT32_FREE   0x00000000
#define FAT32_ALLOC  0x00000002 // to FATEntryNumber-1
#define FAT32_DEFECT 0x0FFFFFF7
#define FAT32_END    0x0FFFFFF8 // to 0x0FFFFFFF

#define FAT32_ISFREE(Cluster)          ((Cluster)==FAT32_FREE)
#define FAT32_ISALLOC(Cluster, Number) ((Cluster)>=FAT32_ALLOC&&(Cluster)<(Number))
#define FAT32_ISDEFECT(Cluster)        ((Cluster)==FAT32_DEFECT)
#define FAT32_ISEND(Cluster)           ((Cluster)>=FAT32_END&&(Cluster)<=0x0FFFFFFF)

#define FAT32_GETENTRY(FatTable, Start, Cluster) \
    ((*(UINT32*)(((UINT8*)(FatTable))+(FAT32_ENTRIES2BYTES(Cluster)-FAT32_ENTRIES2BYTES(Start))))&0x0FFFFFFF)


// FAT(shared by FAT12/FAT16/FAT32)
#define FAT_ROOTCLUSTER  0 // pseudo for FAT12/FAT16 root
#define FAT_STARTCLUSTER 2


// directory entry
#define DIRENTRY_NAMELEN      8 // short name
#define DIRENTRY_EXTENSIONLEN 3

#define DIRENTRY_NAMELEN1     5 // long name support
#define DIRENTRY_NAMELEN2     6
#define DIRENTRY_NAMELEN3     2

#define DIRENTRY_MAXCOMP  20
#define DIRENTRY_LASTCOMP 0x40
#define DIRENTRY_COMPMASK 0xBF

#define DIRENTRY_ENDCHAR   ' '  // special character for name
#define DIRENTRY_DELETED   0xE5
#define DIRENTRY_NEVERUSED 0x00
#define DIRENTRY_KANJI     0x05
#define DIRENTRY_PADDING   0xFFFF

#define DIRENTRY_READONLY    0x01 // Attribute
#define DIRENTRY_HIDDEN      0x02
#define DIRENTRY_SYSTEM      0x04
#define DIRENTRY_VOLUMELABEL 0x08
#define DIRENTRY_SUBDIR      0x10
#define DIRENTRY_ARCHIVE     0x20
#define DIRENTRY_LONGNAME    0x0f

#define DIRENTRY_HOUR(Time)   ((Time)/2048)      // Time
#define DIRENTRY_MINUTE(Time) (((Time)/32)&0x3f)
#define DIRENTRY_SECOND(Time) (((Time)&0x1f)*2)

#define DIRENTRY_SECONDEX(Time, TimeTenth) (DIRENTRY_SECOND(Time)+(TimeTenth)/100)
#define DIRENTRY_MILLISECOND(TimeTenth)    (10*((TimeTenth)%100))

#define DIRENTRY_YEAR(Date)  (1980+((Date)/512)) // Date
#define DIRENTRY_MONTH(Date) (((Date)/32)&0x0f)
#define DIRENTRY_DAY(Date)   ((Date)&0x1f)

typedef union _RAWDIRENTRY{
    __unaligned struct{ // short name
        UINT8  Name[DIRENTRY_NAMELEN];
        UINT8  Extension[DIRENTRY_EXTENSIONLEN];
        UINT8  Attribute;
        UINT8  Reserved;
        UINT8  CreatedTimeTenth;
        UINT16 CreatedTime;
        UINT16 CreatedDate;
        UINT16 AccessedDate;
        UINT16 StartingClusterHi;
        UINT16 ModifiedTime;
        UINT16 ModifiedDate;
        UINT16 StartingCluster;
        UINT32 FileLength;
    };
    __unaligned struct{ // long name
        UINT8  Ordinal;
        UINT16 Name1[DIRENTRY_NAMELEN1];
        UINT8  Reserved1;
        UINT8  Type; // always zero
        UINT8  CheckSum;
        UINT16 Name2[DIRENTRY_NAMELEN2];
        UINT16 Reserved2;
        UINT16 Name3[DIRENTRY_NAMELEN3];
    };
}RAWDIRENTRY, *PRAWDIRENTRY;

#pragma pack(pop)


// file system struct internal representation
#define PART_UNKNOWN 0
#define PART_FAT12   1
#define PART_FAT16   2
#define PART_FAT32   3

typedef struct _PARTITION{
    BOOL  Bootable;
    DWORD FileSystem;
    DWORD StartingPartition; // partition
    DWORD PartitionLength;
    DWORD StartingFATEntry;  // FAT
    DWORD FATEntryNumber;
    DWORD StartingRootEntry; // root
    DWORD RootEntryLength;
    DWORD StartingCluster;   // cluster
    DWORD LengthPerCluster;
    // cache for speed up
    BYTE  CacheFATEntry[DEFAULT_SECTOR_SIZE];
    DWORD CacheFATEntryBegin;
    DWORD CacheFATEntryEnd;
}PARTITION, *PPARTITION;

typedef struct _DIRENTRY{
    CHAR  FileName[MAX_PATH+1];
    DWORD Attribute;
    DWORD CheckSum;
    DWORD Ordinal;
    SYSTEMTIME Created;
    SYSTEMTIME Accessed;
    SYSTEMTIME Modified;
    DWORD StartingCluster;
    DWORD FileLength;
}DIRENTRY, *PDIRENTRY;


// global state variable
#define MAX_PARTITION   26

static DWORD             MasterBootRecordNumber;
static MASTERBOOT_RECORD MasterBootRecord[MAX_PARTITION];

static DWORD        PartitionNumber;
static RAWPARTITION RawPartition[MAX_PARTITION];
static PARTITION    Partition[MAX_PARTITION];


// help routine for FAT entry
static DWORD GetFATEntryBits(DWORD dwPartNo){
    DWORD dwBits = FAT_ERROR;
    switch(Partition[dwPartNo].FileSystem){
    case PART_FAT12:
        dwBits = FAT12_BITS;
        break;
    case PART_FAT16:
        dwBits = FAT16_BITS;
        break;
    case PART_FAT32:
        dwBits = FAT32_BITS;
        break;
    }
    return dwBits;
}

static DWORD GetFATEntryBytes(DWORD dwPartNo){
    DWORD dwBits = GetFATEntryBits(dwPartNo);
    if(dwBits != FAT_ERROR){
        dwBits = (dwBits+7)/8;
    }
    return dwBits;
}

static DWORD ConvertBytesToFATEntries(DWORD dwPartNo, DWORD dwBytes){
    DWORD dwEntries = FAT_ERROR;
    switch(Partition[dwPartNo].FileSystem){
    case PART_FAT12:
        dwEntries = FAT12_BYTES2ENTRIES(dwBytes);
        break;
    case PART_FAT16:
        dwEntries = FAT16_BYTES2ENTRIES(dwBytes);
        break;
    case PART_FAT32:
        dwEntries = FAT32_BYTES2ENTRIES(dwBytes);
        break;
    }
    return dwEntries;
}

static DWORD ConvertFATEntriesToBytes(DWORD dwPartNo, DWORD dwEntries){
    DWORD dwBytes = FAT_ERROR;
    switch(Partition[dwPartNo].FileSystem){
    case PART_FAT12:
        dwBytes = FAT12_ENTRIES2BYTES(dwEntries);
        break;
    case PART_FAT16:
        dwBytes = FAT16_ENTRIES2BYTES(dwEntries);
        break;
    case PART_FAT32:
        dwBytes = FAT32_ENTRIES2BYTES(dwEntries);
        break;
    }
    return dwBytes;
}

static BOOL IsFreeFATEntry(DWORD dwPartNo, DWORD dwCluster){
    BOOL isFree = FALSE;
    switch(Partition[dwPartNo].FileSystem){
    case PART_FAT12:
        isFree = FAT12_ISFREE(dwCluster);
        break;
    case PART_FAT16:
        isFree = FAT16_ISFREE(dwCluster);
        break;
    case PART_FAT32:
        isFree = FAT32_ISFREE(dwCluster);
        break;
    }
    return isFree;
}

static BOOL IsAllocFATEntry(DWORD dwPartNo, DWORD dwCluster){
    BOOL isAlloc = FALSE;
    switch(Partition[dwPartNo].FileSystem){
    case PART_FAT12:
        isAlloc = FAT12_ISALLOC(dwCluster, Partition[dwPartNo].FATEntryNumber);
        break;
    case PART_FAT16:
        isAlloc = FAT16_ISALLOC(dwCluster, Partition[dwPartNo].FATEntryNumber);
        break;
    case PART_FAT32:
        isAlloc = FAT32_ISALLOC(dwCluster, Partition[dwPartNo].FATEntryNumber);
        break;
    }
    return isAlloc;
}

static BOOL IsDefectFATEntry(DWORD dwPartNo, DWORD dwCluster){
    BOOL isDefect = FALSE;
    switch(Partition[dwPartNo].FileSystem){
    case PART_FAT12:
        isDefect = FAT12_ISDEFECT(dwCluster);
        break;
    case PART_FAT16:
        isDefect = FAT16_ISDEFECT(dwCluster);
        break;
    case PART_FAT32:
        isDefect = FAT32_ISDEFECT(dwCluster);
        break;
    }
    return isDefect;
}

static BOOL IsEndFATEntry(DWORD dwPartNo, DWORD dwCluster){
    BOOL isEnd = FALSE;
    switch(Partition[dwPartNo].FileSystem){
    case PART_FAT12:
        isEnd = FAT12_ISEND(dwCluster);
        break;
    case PART_FAT16:
        isEnd = FAT16_ISEND(dwCluster);
        break;
    case PART_FAT32:
        isEnd = FAT32_ISEND(dwCluster);
        break;
    }
    return isEnd;
}

static BOOL FillCacheFATEntries(DWORD dwPartNo, DWORD dwCluster){
    DWORD dwByteOffset, dwReadBytes;
    // 1.invalidate CacheFATEntryBegin&CacheFATEntryEnd
    Partition[dwPartNo].CacheFATEntryBegin = FAT_ERROR;
    Partition[dwPartNo].CacheFATEntryEnd   = FAT_ERROR;
    // 2.convert dwCluster to dwByteOffset
    dwByteOffset = ConvertFATEntriesToBytes(dwPartNo, dwCluster);
    // 3.read FAT entry from partition
    dwReadBytes = FATReadPartition(dwPartNo, Partition[dwPartNo].CacheFATEntry,
        sizeof(Partition[dwPartNo].CacheFATEntry), Partition[dwPartNo].StartingFATEntry+dwByteOffset);
    // 4.check result
    if(dwReadBytes==FAT_ERROR || dwReadBytes<GetFATEntryBytes(dwPartNo)){
        return FALSE;
    }
    // 5.update cache variable
    Partition[dwPartNo].CacheFATEntryBegin = dwCluster;
    Partition[dwPartNo].CacheFATEntryEnd   = dwCluster+ConvertBytesToFATEntries(dwPartNo, dwReadBytes);
    return TRUE;
}

static DWORD GetFATEntry(DWORD dwPartNo, DWORD dwCluster){
    // check parameter
    if(dwCluster >= Partition[dwPartNo].FATEntryNumber){
        return FAT_ERROR;
    }
    // check cache miss and fill new entries, if need
    if(dwCluster<Partition[dwPartNo].CacheFATEntryBegin || dwCluster>=Partition[dwPartNo].CacheFATEntryEnd){
        if(!FillCacheFATEntries(dwPartNo, dwCluster)){ // fill cache
            return FAT_ERROR;
        }
    }
    // dispatch according file system type
    switch(Partition[dwPartNo].FileSystem){
    case PART_FAT12:
        return FAT12_GETENTRY(Partition[dwPartNo].CacheFATEntry, Partition[dwPartNo].CacheFATEntryBegin, dwCluster);
    case PART_FAT16:
        return FAT16_GETENTRY(Partition[dwPartNo].CacheFATEntry, Partition[dwPartNo].CacheFATEntryBegin, dwCluster);
    case PART_FAT32:
        return FAT32_GETENTRY(Partition[dwPartNo].CacheFATEntry, Partition[dwPartNo].CacheFATEntryBegin, dwCluster);
    default:
        return FAT_ERROR;
    }
}

static DWORD CalculateFATEntries(DWORD dwPartNo, DWORD dwStart){
    DWORD dwCount = 0;
    while(IsAllocFATEntry(dwPartNo, dwStart)){
        ++dwCount;
        dwStart = GetFATEntry(dwPartNo, dwStart);
    }
    if(!IsEndFATEntry(dwPartNo, dwStart)){
        dwCount = FAT_ERROR;
    }
    return dwCount;
}


// help routine for file
static VOID ClearFileInfo(PFILEINFO pFileInfo){
    pFileInfo->PartitionNumber = FAT_ERROR;
    pFileInfo->StartingCluster = FAT_ERROR;
    pFileInfo->FileLength      = FAT_ERROR;
    pFileInfo->FileAttribute   = FAT_ERROR;
    pFileInfo->CurrentCluster  = FAT_ERROR;
    pFileInfo->CurrentPostion  = FAT_ERROR;
}

static VOID InitFileInfo(PFILEINFO pFileInfo, DWORD dwPartNo, DWORD dwStartingCluster, DWORD dwFileLength, DWORD dwAttribute){
    if(dwFileLength == 0){
        // assume length align on cluster length
        DWORD dwCount = CalculateFATEntries(dwPartNo, dwStartingCluster);
        if(dwCount != FAT_ERROR){
            dwFileLength = Partition[dwPartNo].LengthPerCluster*dwCount;
        }
    }
    pFileInfo->PartitionNumber = dwPartNo;
    pFileInfo->StartingCluster = dwStartingCluster;
    pFileInfo->FileLength      = dwFileLength;
    pFileInfo->FileAttribute   = dwAttribute;
    pFileInfo->CurrentCluster  = dwStartingCluster;
    pFileInfo->CurrentPostion  = 0;
}


// help routine for directory
static UINT8 ComputeCheckSum(PUINT8 pName){
    int i;
    UINT8 Sum = 0;

    for(i = 0; i < DIRENTRY_NAMELEN+DIRENTRY_EXTENSIONLEN; ++i){
        // NOTE: The operation is an unsigned char rotate right
        Sum = ((Sum&1)?0x80:0) + (Sum>>1) + *pName++;
    }
    return Sum;
}

static DWORD GetDirEntryNumber(PFILEINFO pFileInfo){
    return FATGetFileSize(pFileInfo)/sizeof(RAWDIRENTRY);
}

static BOOL GetDirEntry(PFILEINFO pFileInfo, PDIRENTRY pDirEntry, DWORD dwPos){
    RAWDIRENTRY drEntry;
    // set all field to zero
    memset(pDirEntry, 0, sizeof(*pDirEntry));
    // seek to right position
    if(dwPos != 0){
        if(!FATSetFilePos(pFileInfo, dwPos*sizeof(drEntry))){
            return FALSE;
        }
    }
    // read raw entry
    if(FATReadFile(pFileInfo, &drEntry, sizeof(drEntry)) != sizeof(drEntry)){
        return FALSE;
    }
    // according entry type, fill in
    pDirEntry->Attribute = drEntry.Attribute;
    if(drEntry.Attribute == DIRENTRY_LONGNAME){ // long format
        // filename
        int i, j = 0;
        for(i = 0; i<DIRENTRY_NAMELEN1 && drEntry.Name1[i]!=DIRENTRY_PADDING; ++i){
            pDirEntry->FileName[j++] = (CHAR)drEntry.Name1[i];
        }
        for(i = 0; i<DIRENTRY_NAMELEN2 && drEntry.Name2[i]!=DIRENTRY_PADDING; ++i){
            pDirEntry->FileName[j++] = (CHAR)drEntry.Name2[i];
        }
        for(i = 0; i<DIRENTRY_NAMELEN3 && drEntry.Name3[i]!=DIRENTRY_PADDING; ++i){
            pDirEntry->FileName[j++] = (CHAR)drEntry.Name3[i];
        }
        // checksum&ordinal
        pDirEntry->CheckSum = drEntry.CheckSum;
        pDirEntry->Ordinal  = drEntry.Ordinal;
    }
    else{ // short format
        // filename
        int i, j = 0;
        for(i = 0; i<DIRENTRY_NAMELEN && drEntry.Name[i]!=DIRENTRY_ENDCHAR; ++i){
            pDirEntry->FileName[j++] = drEntry.Name[i];
        }
        for(i = 0; i<DIRENTRY_EXTENSIONLEN && drEntry.Extension[i]!=DIRENTRY_ENDCHAR; ++i){
            if(i == 0){ // add '.'
                pDirEntry->FileName[j++] = '.';
            }
            pDirEntry->FileName[j++] = drEntry.Extension[i];
        }
        // checksum
        pDirEntry->CheckSum = ComputeCheckSum(drEntry.Name);
        // create time
        pDirEntry->Created.wYear         = DIRENTRY_YEAR(drEntry.CreatedDate);
        pDirEntry->Created.wMonth        = DIRENTRY_MONTH(drEntry.CreatedDate);
        pDirEntry->Created.wDay          = DIRENTRY_DAY(drEntry.CreatedDate);
        pDirEntry->Created.wHour         = DIRENTRY_HOUR(drEntry.CreatedTime);
        pDirEntry->Created.wMinute       = DIRENTRY_MINUTE(drEntry.CreatedTime);
        pDirEntry->Created.wSecond       = DIRENTRY_SECONDEX(drEntry.CreatedTime, drEntry.CreatedTimeTenth);
        pDirEntry->Created.wMilliseconds = DIRENTRY_MILLISECOND(drEntry.CreatedTimeTenth);
        // access time
        pDirEntry->Accessed.wYear  = DIRENTRY_YEAR(drEntry.AccessedDate);
        pDirEntry->Accessed.wMonth = DIRENTRY_MONTH(drEntry.AccessedDate);
        pDirEntry->Accessed.wDay   = DIRENTRY_DAY(drEntry.AccessedDate);
        // modify time
        pDirEntry->Modified.wYear         = DIRENTRY_YEAR(drEntry.ModifiedDate);
        pDirEntry->Modified.wMonth        = DIRENTRY_MONTH(drEntry.ModifiedDate);
        pDirEntry->Modified.wDay          = DIRENTRY_DAY(drEntry.ModifiedDate);
        pDirEntry->Modified.wHour         = DIRENTRY_HOUR(drEntry.ModifiedTime);
        pDirEntry->Modified.wMinute       = DIRENTRY_MINUTE(drEntry.ModifiedTime);
        pDirEntry->Modified.wSecond       = DIRENTRY_SECOND(drEntry.ModifiedTime);
        // cluster&length
        pDirEntry->StartingCluster = (drEntry.StartingClusterHi<<16)+drEntry.StartingCluster;
        pDirEntry->FileLength      = drEntry.FileLength;
    }
    return TRUE;
}

static BOOL GetDirEntryEx(PFILEINFO pFileInfo, PDIRENTRY pDirEntry){
    int i, j = 0;
    DWORD dwPrevPos;
    DIRENTRY drEntry[DIRENTRY_MAXCOMP];
    while(1){
        // 1.save current position
        dwPrevPos = FATGetFilePos(pFileInfo);
        // 2.try to get one entry
        if(!GetDirEntry(pFileInfo, pDirEntry, 0)){
            return FALSE;
        }
        // 3.short format? break loop
        if(pDirEntry->Attribute != DIRENTRY_LONGNAME){
            break;
        }
        // 4.otherwise, copy to DirEntry
        if(j < DIRENTRY_MAXCOMP){
            memcpy(&drEntry[j++], pDirEntry, sizeof(*pDirEntry));
        }
    }
    if(j != 0){ // long format
        CHAR FileName[MAX_PATH+1];
        FileName[0] = 0;
        // 1.verify last component flag
        if(!(drEntry[0].Ordinal&DIRENTRY_LASTCOMP)){
            return TRUE; // no flag
        }
        // 2.form long file name
        for(i = j-1; i >= 0; --i){
            // verify ordinal
            if((drEntry[i].Ordinal&DIRENTRY_COMPMASK) != j-i){
                return TRUE; // wrong ordinal
            }
            // verify checksum
            if(drEntry[i].CheckSum != pDirEntry->CheckSum){
                return TRUE; // wrong checksum
            }
            strcat(FileName, drEntry[i].FileName);
        }
        // 3.change to long file name
        strcpy(pDirEntry->FileName, FileName);
        // 4.restore file position for next operation
        FATSetFilePos(pFileInfo, dwPrevPos);
    }
    return TRUE;
}

// if match, it will update *ppCurItem to next item
static BOOL CompareFileName(PCSTR pFileName, PCSTR* ppCurItem){
    PCSTR pCurItem;
    for(pCurItem = *ppCurItem; *pFileName != 0; ++pCurItem, ++pFileName){
        if(*pCurItem == 0){ // pCurItem finish, but pFileName not
            return FALSE;
        }
        if(toupper(*pCurItem) != toupper(*pFileName)){ // no match
            return FALSE;
        }
    }
    // check endpoint
    switch(*pCurItem){
    case '/':
    case '\\':
        ++pCurItem; // skip separator
        // go through
    case '\0':
        *ppCurItem = pCurItem; // update to next
        return TRUE;
    }
    return FALSE;
}

static BOOL FindRootDirEntry(PFILEINFO pFileInfo, PCSTR* ppCurItem){
    DWORD dwPartNo = FAT_ERROR, dwLoop;
    // find partition No.
    if((*ppCurItem)[0]!=0 && (*ppCurItem)[1]==':'){ // specify disk no.
        CHAR DiskName[] = "A:"; // search from A:
        for(dwLoop = 0; dwLoop < PartitionNumber; ++dwLoop, ++DiskName[0]){
            // compare root, if match update ppCurItem to next
            if(CompareFileName(DiskName, ppCurItem)){
                dwPartNo = dwLoop;
                break;
            }
        }
    }
    else if(PartitionNumber > 0){
        dwPartNo = 0; // default partition is zero
        // search active partition
        for(dwLoop = 0; dwLoop < PartitionNumber; ++dwLoop){
            if(Partition[dwLoop].Bootable){
                dwPartNo = dwLoop; // active partition is high priority
                break;
            }
        }
        CompareFileName("", ppCurItem); // skip optional first '/'
    }
    // init pFileInfo
    if(dwPartNo != FAT_ERROR){
        if(Partition[dwPartNo].FileSystem == PART_FAT32){
            InitFileInfo(pFileInfo, dwPartNo, Partition[dwPartNo].StartingRootEntry, 0, DIRENTRY_SUBDIR);
        }
        else{
            InitFileInfo(pFileInfo, dwPartNo, FAT_ROOTCLUSTER, Partition[dwPartNo].RootEntryLength, DIRENTRY_SUBDIR);
        }
        return TRUE;
    }
    else{
        return FALSE;
    }
}

static BOOL FindNextDirEntry(PFILEINFO pFileInfo, PCSTR* ppCurItem){
    DIRENTRY drEntry;
    // search...
    while(1){
        // read one DIRENTRY 
        if(!GetDirEntryEx(pFileInfo, &drEntry)){
            break; // fail, exit loop
        }
        if(drEntry.FileName[0] == DIRENTRY_NEVERUSED){
            break; // no used entry, exit
        }
        // compare filename, if match update ppCurItem to next
        if(CompareFileName(drEntry.FileName, ppCurItem)){
            // update pFileInfo to new entry
            InitFileInfo(pFileInfo, pFileInfo->PartitionNumber,
                drEntry.StartingCluster, drEntry.FileLength, drEntry.Attribute);
            return TRUE;
        }
    }
    return FALSE;
}


// help routine for initialization
static DWORD GetDefaultSectorSize(){
    return DEFAULT_SECTOR_SIZE;
}

static BOOL InitPartition(BOOL fBootable, DWORD dwFileSystem, DWORD dwStart, DWORD dwLength){
    DWORD dwFileType;
    DWORD dwFATSectors, dwTotalSectors;
    DWORD dwRootStart, dwRootSectors, dwRootLength;
    DWORD dwDataStart, dwDataSectors, dwDataClusters;
    UINT8 buf[DEFAULT_SECTOR_SIZE];
    PRAWPARTITION pPart = (PRAWPARTITION)buf;
    // we just support 26 partition
    if(PartitionNumber >= MAX_PARTITION){
        return FALSE;
    }
    // read first sector
    if(FATReadDisk(pPart, sizeof(*pPart), dwStart) != sizeof(*pPart)){
        return FALSE;
    }
    // check field
    if(pPart->SectorSize == 0){
        return FALSE;
    }
    if(pPart->SectorPerCluster == 0){
        return FALSE;
    }
    if(pPart->ReservedSector == 0){
        return FALSE;
    }
    if(pPart->NumberOfFAT == 0){
        return FALSE;
    }
    if(pPart->NumberOfRootEntry==0 && pPart->FAT32.RootEntryCluster==0){
        return FALSE;
    }
    if(pPart->TotalSector != 0){
        if(pPart->TotalSector2 != 0){
            return FALSE;
        }
    }
    else if(pPart->TotalSector2 == 0){
        return FALSE;
    }
    if(pPart->SectorPerFAT==0 && pPart->FAT32.SectorPerFAT==0){
        return FALSE;
    }
    if(pPart->EFDCSignature!=RAWPARTITION_EFDCSIGNATURE &&
       pPart->FAT32.EFDCSignature!=RAWPARTITION_EFDCSIGNATURE){
        return FALSE;
    }
    if(pPart->Signature != RAWPARTITION_SIGNATURE){
        return FALSE;
    }
    // determine FAT type
    if(pPart->SectorPerFAT){
        dwFATSectors = pPart->NumberOfFAT*pPart->SectorPerFAT;
    }
    else{
        dwFATSectors = pPart->NumberOfFAT*pPart->FAT32.SectorPerFAT;
    }
    if(pPart->TotalSector){
        dwTotalSectors = pPart->TotalSector;
    }
    else{
        dwTotalSectors = pPart->TotalSector2;
    }
    dwRootStart    = pPart->SectorSize*(pPart->ReservedSector+dwFATSectors);
    dwRootLength   = 32*pPart->NumberOfRootEntry;
    dwRootSectors  = (dwRootLength+pPart->SectorSize-1)/pPart->SectorSize;
    dwDataStart    = dwRootStart+pPart->SectorSize*dwRootSectors;
    dwDataSectors  = dwTotalSectors-pPart->ReservedSector-dwFATSectors-dwRootSectors;
    dwDataClusters = dwDataSectors/pPart->SectorPerCluster;
    if(dwDataClusters < 4085){
        dwFileType = PART_FAT12;
    }
    else if(dwDataClusters < 65525){
        dwFileType = PART_FAT16;
    }
    else{
        dwFileType = PART_FAT32;
        dwRootStart = pPart->FAT32.RootEntryCluster;
    }
    // check consistent
    if(dwFileSystem != dwFileType){
        if(dwFileSystem != PART_UNKNOWN){
            return FALSE;
        }
    }
    // validated FAT, update global state
    Partition[PartitionNumber].Bootable          = fBootable;
    Partition[PartitionNumber].FileSystem        = dwFileType;
    Partition[PartitionNumber].StartingPartition = dwStart;
    Partition[PartitionNumber].PartitionLength   = dwLength;
    Partition[PartitionNumber].StartingFATEntry  = pPart->SectorSize*pPart->ReservedSector;
    Partition[PartitionNumber].FATEntryNumber    = dwDataClusters+2; // +2 for format identifier
    Partition[PartitionNumber].StartingRootEntry = dwRootStart;
    Partition[PartitionNumber].RootEntryLength   = dwRootLength;
    Partition[PartitionNumber].StartingCluster   = dwDataStart;
    Partition[PartitionNumber].LengthPerCluster  = pPart->SectorSize*pPart->SectorPerCluster;
    memcpy(&RawPartition[PartitionNumber++], pPart, sizeof(*pPart));
    return TRUE;
}

static BOOL InitMasterBootRecord(DWORD dwStart, DWORD dwLength){
    int i, j;
    BOOL fBootable;
    UINT8 buf[DEFAULT_SECTOR_SIZE];
    PARTITION_TABLE EmptyPart = {0}; // all zero
    PPARTITION_TABLE pPart1, pPart2;
    DWORD dwPartStart, dwPartLength;
    PMASTERBOOT_RECORD pMBR = (PMASTERBOOT_RECORD)buf;
    // we just support 26 MBR
    if(MasterBootRecordNumber >= MAX_PARTITION){
        return FALSE;
    }
    // read first sector
    if(FATReadDisk(pMBR, sizeof(*pMBR), dwStart) != sizeof(*pMBR)){
        return FALSE;
    }
    // check signature
    if(pMBR->Signature != MASTERBOOT_SIGNATURE){
        return FALSE;
    }
    // check partition table
    for(i = 0; i < PARTITION_NUMBER; ++i){
        pPart1 = &pMBR->Partition[i];
        // check empty partition
        if(pPart1->SystemId == PARTITION_EMPTY){
            if(memcmp(pPart1, &EmptyPart, sizeof(EmptyPart)) != 0){
                return FALSE; // empty partition should be all zero
            }
            continue; // skip follow check
        }
        // check boot indicator
        if(pPart1->BootIndicator!=PARTITION_NORMAL && pPart1->BootIndicator!=PARTITION_SYSTEM){
            return FALSE;
        }
        // check header(0-254)
        if(pPart1->StartingHead==0xFF || pPart1->EndingHead==0xFF){
            return FALSE;
        }
        // check sector(1-63)
        if(pPart1->StartingSector==0 || pPart1->EndingSector==0){
            return FALSE;
        }
        // check start&length
        dwPartStart = pPart1->RelativeSector*GetDefaultSectorSize();
        dwPartLength = pPart1->TotalSector*GetDefaultSectorSize();
        if(dwPartStart >= dwLength){
            return FALSE;
        }
        if(dwPartLength==0 || dwPartStart+dwPartLength>dwLength){
            return FALSE;
        }
        // check overlap
        for(j = i+1; j < PARTITION_NUMBER; ++j){
            pPart2 = &pMBR->Partition[j];
            // skip empty
            if(pPart2->SystemId == PARTITION_EMPTY){
                continue;
            }
            // check start point
            if(pPart2->RelativeSector>=pPart1->RelativeSector &&
               pPart2->RelativeSector<pPart1->RelativeSector+pPart1->TotalSector){
                return FALSE;
            }
            // check end point
            if(pPart2->RelativeSector+pPart2->TotalSector>pPart1->RelativeSector &&
               pPart2->RelativeSector+pPart2->TotalSector<=pPart1->RelativeSector+pPart1->TotalSector){
               return FALSE;
            }
        }
    }
    // validated MBR, copy it!
    memcpy(&MasterBootRecord[MasterBootRecordNumber++], pMBR, sizeof(*pMBR));
    // init individual partition 
    for(i = 0; i < PARTITION_NUMBER; ++i){
        pPart1 = &pMBR->Partition[i];
        fBootable = (pPart1->BootIndicator==PARTITION_SYSTEM);
        dwPartStart = dwStart + GetDefaultSectorSize()*pPart1->RelativeSector;
        dwPartLength = GetDefaultSectorSize()*pPart1->TotalSector;
        switch(pPart1->SystemId){
        case PARTITION_FAT12: // FAT12
            InitPartition(fBootable, PART_FAT12, dwPartStart, dwPartLength);
            break;
        case PARTITION_FAT16: // FAT16
        case PARTITION_FAT16B:
        case PARTITION_FAT16L:
            InitPartition(fBootable, PART_FAT16, dwPartStart, dwPartLength);
            break;
        case PARTITION_FAT32: // FAT32
        case PARTITION_FAT32L:
            InitPartition(fBootable, PART_FAT32, dwPartStart, dwPartLength);
            break;
        case PARTITION_EXT:   // Extend
        case PARTITION_EXTL:
            InitMasterBootRecord(dwPartStart, dwPartLength); // recursion!
            break;
        case PARTITION_EMPTY:
            break; // no action
        default:
            //InitPartition(PART_UNKNOWN, dwPartStart, dwPartLength); // try it?
            break;
        }
    }
    return TRUE;
}


// disk function
BOOL FATInitDisk(){
    // init hardware
    if(!SDInitializeHardware()){
        return FALSE;
    }
    // try to init MBR
    if(InitMasterBootRecord(0, FATGetDiskSize())){
        return TRUE;
    }
    // no MBR, try to init partition
    if(InitPartition(TRUE, PART_UNKNOWN, 0, FATGetDiskSize())){
        return TRUE;
    }
    // no FAT partition, fail!
    return FALSE;
}

DWORD FATGetDiskSize(){
    return 0xFFFFFFFF; // TODO: get from SD Card
}

DWORD FATReadDisk(PVOID pBuffer, DWORD dwLength, DWORD dwPos){
    // parameter check
    if(dwPos >= FATGetDiskSize()){
        return 0;
    }
    // adjust dwLength if need
    if(dwPos+dwLength > FATGetDiskSize()){
        dwLength = FATGetDiskSize()-dwPos;
    }
    return SDReadDataBlock(pBuffer, dwLength, dwPos)?dwLength:FAT_ERROR;
}


// partition function
DWORD FATGetPartitionNumber(){
    return PartitionNumber;
}

DWORD FATGetPartitionSize(DWORD dwPartNo){
    return dwPartNo<PartitionNumber?Partition[dwPartNo].PartitionLength:FAT_ERROR;
}

DWORD FATReadPartition(DWORD dwPartNo, PVOID pBuffer, DWORD dwLength, DWORD dwPos){
    // parameter check
    if(dwPartNo >= PartitionNumber){
        return FAT_ERROR;
    }
    if(dwPos >= Partition[dwPartNo].PartitionLength){
        return 0;
    }
    // adjust dwLength if need
    if(dwPos+dwLength > Partition[dwPartNo].PartitionLength){
        dwLength = Partition[dwPartNo].PartitionLength-dwPos;
    }
    return FATReadDisk(pBuffer, dwLength, Partition[dwPartNo].StartingPartition+dwPos);
}


// file function
BOOL FATOpenFile(PFILEINFO pFileInfo, PCSTR pFileName){
    FILEINFO fnFileInfo;
    // invalid pFileInfo
    ClearFileInfo(pFileInfo);
    // find root directory
    if(!FindRootDirEntry(&fnFileInfo, &pFileName)){
        return FALSE;
    }
    // find each directory and last file
    while(*pFileName){
        if(!FATIsDirectory(&fnFileInfo)){ // not directory
            return FALSE;
        }
        if(!FindNextDirEntry(&fnFileInfo, &pFileName)){
            return FALSE;
        }
    }
    // copy it
    memcpy(pFileInfo, &fnFileInfo, sizeof(*pFileInfo));
    return TRUE;
}

BOOL FATCloseFile(PFILEINFO pFileInfo){
    ClearFileInfo(pFileInfo);
    return TRUE;
}

BOOL FATIsReadOnly(PFILEINFO pFileInfo){
    return (pFileInfo->FileAttribute&DIRENTRY_READONLY) != 0;
}

BOOL FATIsHidden(PFILEINFO pFileInfo){
    return (pFileInfo->FileAttribute&DIRENTRY_HIDDEN) != 0;
}

BOOL FATIsSystem(PFILEINFO pFileInfo){
    return (pFileInfo->FileAttribute&DIRENTRY_SYSTEM) != 0;
}

BOOL FATIsVolumeLabel(PFILEINFO pFileInfo){
    return (pFileInfo->FileAttribute&DIRENTRY_VOLUMELABEL) != 0;
}

BOOL FATIsDirectory(PFILEINFO pFileInfo){
    return (pFileInfo->FileAttribute&DIRENTRY_SUBDIR) != 0;
}

BOOL FATIsArchive(PFILEINFO pFileInfo){
    return (pFileInfo->FileAttribute&DIRENTRY_ARCHIVE) != 0;
}

BOOL FATEndOfFile(PFILEINFO pFileInfo){
    return pFileInfo->CurrentPostion >= pFileInfo->FileLength;
}

DWORD FATGetFileSize(PFILEINFO pFileInfo){
    return pFileInfo->FileLength;
}

DWORD FATGetFilePos(PFILEINFO pFileInfo){
    return pFileInfo->CurrentPostion;
}

BOOL FATSetFilePos(PFILEINFO pFileInfo, DWORD dwPos){
    PPARTITION pPartition;
    // parameter check
    if(pFileInfo->PartitionNumber >= PartitionNumber){
        return FAT_ERROR;
    }
    // adjust dwPos, if need
    if(dwPos > pFileInfo->FileLength){
        dwPos = pFileInfo->FileLength;
    }
    // start seeking...
    pPartition = &Partition[pFileInfo->PartitionNumber];
    if(pFileInfo->StartingCluster == FAT_ROOTCLUSTER){ // handle root specially
        // root area is continuous, simply assign new position
        pFileInfo->CurrentPostion = dwPos;
    }
    else{ // not root...
        DWORD dwStartCluster, dwClusterNumber;
        if(dwPos < pFileInfo->CurrentPostion){ // prev
            dwStartCluster  = pFileInfo->StartingCluster;
            dwClusterNumber = dwPos/pPartition->LengthPerCluster;
        }
        else{ // next
            dwStartCluster  = pFileInfo->CurrentCluster;
            dwClusterNumber = dwPos/pPartition->LengthPerCluster-
                pFileInfo->CurrentPostion/pPartition->LengthPerCluster;
        }
        // one time pass one cluster
        while(dwClusterNumber--){
            // go to next cluster
            dwStartCluster = GetFATEntry(pFileInfo->PartitionNumber, dwStartCluster);
            // verify it
            if(!IsAllocFATEntry(pFileInfo->PartitionNumber, dwStartCluster) && dwPos<pFileInfo->FileLength){
                return FAT_ERROR; // invalid cluster
            }
        }
        // update state
        pFileInfo->CurrentPostion = dwPos;
        pFileInfo->CurrentCluster = dwStartCluster;
    }
    return TRUE;
}

DWORD FATReadFile(PFILEINFO pFileInfo, PVOID pBuffer, DWORD dwLength){
    DWORD dwSavedLength;
    PPARTITION pPartition;
    // parameter check
    if(pFileInfo->PartitionNumber >= PartitionNumber){
        return FAT_ERROR;
    }
    if(pFileInfo->CurrentPostion >= pFileInfo->FileLength){ // end of file
        return 0; // no more data to read
    }
    // adjust dwLength, if need
    if(pFileInfo->CurrentPostion+dwLength > pFileInfo->FileLength){
        dwLength = pFileInfo->FileLength-pFileInfo->CurrentPostion;
    }
    // save length for later use
    dwSavedLength = dwLength;
    // start reading...
    pPartition = &Partition[pFileInfo->PartitionNumber];
    if(pFileInfo->StartingCluster == FAT_ROOTCLUSTER){ // handle root specially
        // root area is continuous, just need read one time
        DWORD dwReadBytes = FATReadPartition(pFileInfo->PartitionNumber,
            pBuffer, dwLength, pPartition->StartingRootEntry+pFileInfo->CurrentPostion);
        // error check
        if(dwReadBytes == FAT_ERROR){
            return FAT_ERROR;
        }
        // update state info
        (PBYTE)pBuffer += dwReadBytes;
        dwLength -= dwReadBytes;
        pFileInfo->CurrentPostion += dwReadBytes;
    }
    else{ // not root...
        FILEINFO fnFileInfo = *pFileInfo; // make copy for error tolerance
        while(dwLength){ // not end
            // offset from current cluster
            DWORD dwOffset = fnFileInfo.CurrentPostion%pPartition->LengthPerCluster;
            // maximum bytes we can read at one time
            DWORD dwReadBytes = pPartition->LengthPerCluster-dwOffset;
            // adjust dwReadBytes, if need
            if(dwReadBytes > dwLength){
                dwReadBytes = dwLength;
            }
            // read one cluster
            dwReadBytes = FATReadPartition(fnFileInfo.PartitionNumber, pBuffer, dwReadBytes, pPartition->StartingCluster+
                pPartition->LengthPerCluster*(fnFileInfo.CurrentCluster-FAT_STARTCLUSTER)+dwOffset);
            // error check
            if(dwReadBytes == FAT_ERROR){
                return FAT_ERROR;
            }
            // update state info
            (PBYTE)pBuffer += dwReadBytes;
            dwLength -= dwReadBytes;
            fnFileInfo.CurrentPostion += dwReadBytes;
            if(fnFileInfo.CurrentPostion%pPartition->LengthPerCluster == 0){ // end of cluster
                // go to next cluster
                fnFileInfo.CurrentCluster = GetFATEntry(fnFileInfo.PartitionNumber, fnFileInfo.CurrentCluster);
                //  verify it
                if(!IsAllocFATEntry(fnFileInfo.PartitionNumber, fnFileInfo.CurrentCluster) &&
                   fnFileInfo.CurrentPostion<fnFileInfo.FileLength){ // not end of file){
                    return FAT_ERROR; // invalid cluster
                }
            }
        }
        *pFileInfo = fnFileInfo; // everything is ok, copy back
    }
    return dwSavedLength-dwLength;
}

DWORD FATReadFileEx(PFILEINFO pFileInfo, PVOID pBuffer, DWORD dwLength, DWORD dwPos){
    return FATSetFilePos(pFileInfo, dwPos)?FATReadFile(pFileInfo, pBuffer, dwLength):FAT_ERROR;
}

// dump function for testing only
VOID DumpMasterBootRecord(){
    DWORD dwMBRNo, dwPartNo;
    // dumping...
    for(dwMBRNo = 0; dwMBRNo < MasterBootRecordNumber; ++dwMBRNo){
        OALLog(L"Signature:%04X\r\n", MasterBootRecord[dwMBRNo].Signature);
        //        123 1234 1234 12345 123456  1234 12345 123456 123456 123456789012 1234567890123
        OALLog(L"|No.|Boot|Head/Track/Sector--Head/Track/Sector|System|Start Sector|Sector Number|\r\n");
        for(dwPartNo = 0; dwPartNo < PARTITION_NUMBER; ++dwPartNo){
            OALLog(L"|%3d|  %02X|%4u/%5u/%6u  %4u/%5u/%6u|    %02X|%12u|%13u|\r\n",
                   dwPartNo,
                   MasterBootRecord[dwMBRNo].Partition[dwPartNo].BootIndicator,
                   MasterBootRecord[dwMBRNo].Partition[dwPartNo].StartingHead,
                   MasterBootRecord[dwMBRNo].Partition[dwPartNo].StartingCylinder,
                   MasterBootRecord[dwMBRNo].Partition[dwPartNo].StartingSector,
                   MasterBootRecord[dwMBRNo].Partition[dwPartNo].EndingHead,
                   MasterBootRecord[dwMBRNo].Partition[dwPartNo].EndingCylinder,
                   MasterBootRecord[dwMBRNo].Partition[dwPartNo].EndingSector,
                   MasterBootRecord[dwMBRNo].Partition[dwPartNo].SystemId,
                   MasterBootRecord[dwMBRNo].Partition[dwPartNo].RelativeSector,
                   MasterBootRecord[dwMBRNo].Partition[dwPartNo].TotalSector
                );
        }
    }
}

VOID DumpPartitionBootSector(DWORD dwPartNo){
    // parameter check
    if(dwPartNo >= PartitionNumber){
        OALMSG(OAL_ERROR, (L"Partition Number beyond scope\r\n"));
        return;
    }
    // dumping...
    OALLog(L"JumpCode            : %06X\r\n" , *((UINT32*)(RawPartition[dwPartNo].JumpCode))&0x00FFFFFF);
    OALLog(L"CreatorID           : %.8S\r\n" , RawPartition[dwPartNo].CreatorID);
    OALLog(L"SectorSize          : %u\r\n"   , RawPartition[dwPartNo].SectorSize);
    OALLog(L"SectorPerCluster    : %u\r\n"   , RawPartition[dwPartNo].SectorPerCluster);
    OALLog(L"ReservedSector      : %u\r\n"   , RawPartition[dwPartNo].ReservedSector);
    OALLog(L"NumberOfFAT         : %u\r\n"   , RawPartition[dwPartNo].NumberOfFAT);
    OALLog(L"NumberOfRootEntry   : %u\r\n"   , RawPartition[dwPartNo].NumberOfRootEntry);
    OALLog(L"TotalSector         : %u\r\n"   , RawPartition[dwPartNo].TotalSector);
    OALLog(L"MediumId            : %02X\r\n" , RawPartition[dwPartNo].MediumId);
    OALLog(L"SectorPerFAT        : %u\r\n"   , RawPartition[dwPartNo].SectorPerFAT);
    OALLog(L"SectorPerTrack      : %u\r\n"   , RawPartition[dwPartNo].SectorPerTrack);
    OALLog(L"NumberOfSide        : %u\r\n"   , RawPartition[dwPartNo].NumberOfSide);
    OALLog(L"NumberOfHiddenSector: %u\r\n"   , RawPartition[dwPartNo].NumberOfHiddenSector);
    OALLog(L"TotalSector2        : %u\r\n"   , RawPartition[dwPartNo].TotalSector2);
    if(Partition[dwPartNo].FileSystem == PART_FAT32){
        OALLog(L"SectorPerFAT2       : %u\r\n"   , RawPartition[dwPartNo].FAT32.SectorPerFAT);
        OALLog(L"ExtentFlag          : %04X\r\n" , RawPartition[dwPartNo].FAT32.ExtentFlag);
        OALLog(L"FileSystemVersion   : %04X\r\n" , RawPartition[dwPartNo].FAT32.FileSystemVersion);
        OALLog(L"RootEntryCluster    : %u\r\n"   , RawPartition[dwPartNo].FAT32.RootEntryCluster);
        OALLog(L"FileSystemInfomation: %u\r\n"   , RawPartition[dwPartNo].FAT32.FileSystemInfomation);
        OALLog(L"BackupBootSector    : %u\r\n"   , RawPartition[dwPartNo].FAT32.BackupBootSector);
        OALLog(L"PhysicalDiskNumber  : %02X\r\n" , RawPartition[dwPartNo].FAT32.PhysicalDiskNumber);
        OALLog(L"EFDCSignature       : %02X\r\n" , RawPartition[dwPartNo].FAT32.EFDCSignature);
        OALLog(L"VolumeSerialNumber  : %08X\r\n" , RawPartition[dwPartNo].FAT32.VolumeSerialNumber);
        OALLog(L"VolumeLabel         : %.11S\r\n", RawPartition[dwPartNo].FAT32.VolumeLabel);
        OALLog(L"FileSystemType      : %.8S\r\n" , RawPartition[dwPartNo].FAT32.FileSystemType);
    }
    else{ // FAT12/FAT16
        OALLog(L"PhysicalDiskNumber  : %02X\r\n" , RawPartition[dwPartNo].PhysicalDiskNumber);
        OALLog(L"EFDCSignature       : %02X\r\n" , RawPartition[dwPartNo].EFDCSignature);
        OALLog(L"VolumeSerialNumber  : %08X\r\n" , RawPartition[dwPartNo].VolumeSerialNumber);
        OALLog(L"VolumeLabel         : %.11S\r\n", RawPartition[dwPartNo].VolumeLabel);
        OALLog(L"FileSystemType      : %.8S\r\n" , RawPartition[dwPartNo].FileSystemType);
    }
    OALLog(L"Signature           : %04X\r\n" , RawPartition[dwPartNo].Signature);
}

VOID DumpFATTable(DWORD dwPartNo, DWORD dwStart, DWORD dwCount){
    DWORD dwBits, dwLine, dwLoop;
    // parameter check
    if(dwPartNo >= PartitionNumber){
        OALMSG(OAL_ERROR, (L"Partition Number beyond scope\r\n"));
        return;
    }
    if(dwStart >= Partition[dwPartNo].FATEntryNumber){
        OALMSG(OAL_ERROR, (L"Start position beyond FAT entry number\r\n"));
        return;
    }
    // adjust dwCount if need
    if(dwStart+dwCount > Partition[dwPartNo].FATEntryNumber){
        dwCount = Partition[dwPartNo].FATEntryNumber-dwStart;
    }
    // get FAT entry bit size
    dwBits = GetFATEntryBits(dwPartNo);
    dwLine = 32/GetFATEntryBytes(dwPartNo); // one line display 32 bytes 
    // begin dump
    for(dwLoop = 0; dwLoop < dwCount; ++dwLoop){
        if(dwLoop%dwLine == 0){
            OALLog(L"%04X   ", dwStart+dwLoop);
        }
        OALLog(L"%0*X ", dwBits/4, GetFATEntry(dwPartNo, dwStart+dwLoop));
        if(dwLoop%dwLine==dwLine-1 || dwLoop+1==dwCount){
            OALLog(L"\r\n");
        }
    }
}

VOID DumpDirectory(PCSTR pDirName, DWORD dwStart, DWORD dwCount){
    FILEINFO fnFileInfo;
    DIRENTRY drEntry;
    DWORD    dwEntries;

    // open dir&verify it
    if(!FATOpenFile(&fnFileInfo, pDirName)){
        OALMSG(OAL_ERROR, (L"Can't open %S\r\n", pDirName));
        return;
    }
    // get dir entries
    dwEntries = GetDirEntryNumber(&fnFileInfo);
    // check&adjust parameter
    if(dwStart >= dwEntries){
        OALMSG(OAL_ERROR, (L"Start position beyond directory entry number\r\n"));
        return;
    }
    if(dwStart+dwCount > dwEntries){
        dwCount = dwEntries-dwStart;
    }
    //       123456789012345678901**12345678901234567**12345678**
    OALLog(L"Created                Modified           Accessed  "
    //       123*123*123*12345678**12345678*1234567890123
           L"Att Sum Ord Cluster   Length   FileName\r\n"
        );
    // process each directory entry
    while(dwCount--){
        // read item
        if(!GetDirEntry(&fnFileInfo, &drEntry, dwStart++)){
            OALMSG(OAL_ERROR, (L"Fail to read %S\r\n", pDirName));
            break;
        }
        // output
        OALLog(L"%02u/%02u/%02u-%02u:%02u:%02u:%03u  %02u/%02u/%02u-%02u:%02u:%02u  "
               L"%02u/%02u/%02u  %02X  %02X  %02X  %08X  %-8u %-13S\r\n",
               drEntry.Created.wYear%100, drEntry.Created.wMonth, drEntry.Created.wDay,
               drEntry.Created.wHour, drEntry.Created.wMinute, drEntry.Created.wSecond,
               drEntry.Created.wMilliseconds,
               drEntry.Modified.wYear%100, drEntry.Modified.wMonth, drEntry.Modified.wDay,
               drEntry.Modified.wHour, drEntry.Modified.wMinute, drEntry.Modified.wSecond,
               drEntry.Accessed.wYear%100, drEntry.Accessed.wMonth, drEntry.Accessed.wDay,
               drEntry.Attribute, drEntry.CheckSum, drEntry.Ordinal,  
               drEntry.StartingCluster, drEntry.FileLength, drEntry.FileName
            );
    }
}

VOID DumpDirectoryEx(PCSTR pDirName){
    FILEINFO fnFileInfo;
    DIRENTRY drEntry;

    // open dir&verify it
    if(!FATOpenFile(&fnFileInfo, pDirName)){
        OALMSG(OAL_ERROR, (L"Can't open %S\r\n", pDirName));
        return;
    }
    if(!FATIsDirectory(&fnFileInfo)){
        OALMSG(OAL_ERROR, (L"%S isn't directory\r\n", pDirName));
        return;
    }
    //       12345678901234567**123*12345678**12345678*1...
    OALLog(L"Modified           Att Cluster   Length   FileName\r\n");
    // process each directory entry
    while(1){
        // read item
        if(!GetDirEntryEx(&fnFileInfo, &drEntry)){
            break;
        }
        if(drEntry.FileName[0] == DIRENTRY_NEVERUSED){
            break; // finish?
        }
        // output
        OALLog(L"%02u/%02u/%02u-%02u:%02u:%02u  %02X  %08X  %-8u %S\r\n",
               drEntry.Modified.wYear%100, drEntry.Modified.wMonth, drEntry.Modified.wDay,
               drEntry.Modified.wHour, drEntry.Modified.wMinute, drEntry.Modified.wSecond,
               drEntry.Attribute, drEntry.StartingCluster, drEntry.FileLength, drEntry.FileName
            );
    }
}
