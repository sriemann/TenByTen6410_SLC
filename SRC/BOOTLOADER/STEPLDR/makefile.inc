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

!endif

!IF ("$(BSP_IROM_SDMMC_CH0_BOOT)"!="1")&&("$(BSP_IROM_SDMMC_CH1_BOOT)"!="1")
stepldr_romimage:
    romimage $(ROMIMAGE_FLAGS) stepldr.bib

#   This cuts upper 4Kbytes, In Runnable ROM code, relocation(.rdata section) is not needed
    copy /y/b $(_PLATFORMROOT)\$(_TGTPLAT)\target\$(_TGTCPU)\$(WINCEDEBUG)\stepldr.nb1 + $(_PLATFORMROOT)\$(_TGTPLAT)\target\$(_TGTCPU)\$(WINCEDEBUG)\stepldr.nb2 $(_PLATFORMROOT)\$(_TGTPLAT)\target\$(_TGTCPU)\$(WINCEDEBUG)\stepldr.nb0
!IF "$(WINCEREL)"=="1"
    copy $(_PLATFORMROOT)\$(_TGTPLAT)\target\$(_TGTCPU)\$(WINCEDEBUG)\stepldr.* $(_FLATRELEASEDIR)
!ENDIF
!ENDIF

