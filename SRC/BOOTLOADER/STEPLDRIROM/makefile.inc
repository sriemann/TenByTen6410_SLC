!if 0
Copyright (c) Microsoft Corporation.  All rights reserved.
!endif
!if 0
Use of this source code is subject to the terms of the Microsoft end-user
license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
If you did not accept the terms of the EULA, you are not authorized to use
this source code. For a copy of the EULA, please see the LICENSE.RTF on your
install media.
!endif
!if 0


Module Name:

    makefile.inc

    romimage $(ROMIMAGE_FLAGS) stepldr_irom_sd.bib

!endif

stepldr_romimage:
!IF "$(NOLINK)" == ""
    romimage $(ROMIMAGE_FLAGS) stepldr_irom_sd.bib
    
 
	copy /y/b $(_PLATFORMROOT)\$(_TGTPLAT)\target\$(_TGTCPU)\$(WINCEDEBUG)\eboot.nb0 + $(_PLATFORMROOT)\$(_TGTPLAT)\target\$(_TGTCPU)\$(WINCEDEBUG)\STEPLDR_IROM_SD.nb0 $(_PLATFORMROOT)\$(_TGTPLAT)\target\$(_TGTCPU)\$(WINCEDEBUG)\IROM_SD_EBOOT.nb0

!IF "$(WINCEREL)"=="1"
    copy $(_PLATFORMROOT)\$(_TGTPLAT)\target\$(_TGTCPU)\$(WINCEDEBUG)\IROM_SD_EBOOT.* $(_FLATRELEASEDIR)
    copy $(_PLATFORMROOT)\$(_TGTPLAT)\target\$(_TGTCPU)\$(WINCEDEBUG)\STEPLDR_IROM_SD.* $(_FLATRELEASEDIR)
!ENDIF


!ENDIF
