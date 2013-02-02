; Optimize USB Transfer Performance
; chandolp 
;
; OTG Device Transfer Related Routines
;

	INCLUDE kxarm.h

	TEXTAREA


;----------------------------------------------
; void RxData512(unsigned char *bufPt, volatile ULONG *FifoPt);
; Buffer (r0) must be aligned.
;----------------------------------------------	
	LEAF_ENTRY GetData512

		stmfd		sp!,{r2 - r12}

		mov		r2, #0x1E0
1
		ldr		r3, [r1]
		ldr		r4, [r1]
		ldr		r5, [r1]
		ldr		r6, [r1]
		ldr		r7, [r1]
		ldr		r8, [r1]
		ldr		r9, [r1]
		ldr		r10, [r1]
		ldr		r11, [r1]
		ldr		r12, [r1]

		stmia		r0!, {r3 - r12}
		subs    	r2, r2, #40
		bne		%B1

		ldr		r3, [r1]
		ldr		r4, [r1]
		ldr		r5, [r1]
		ldr		r6, [r1]
		ldr		r7, [r1]
		ldr		r8, [r1]
		ldr		r9, [r1]
		ldr		r10, [r1]
		
		stmia		r0!, {r3 - r10}

		ldmfd		sp!, {r2 - r12}

		mov		pc, lr	; return


	
;----------------------------------------------------------
; void TxData512(unsigned char *bufPt, volatile ULONG *FifoPt);
; Buffer (r0) must be aligned.
;----------------------------------------------------------
	LEAF_ENTRY PutData512

		stmfd		sp!,{r2 - r12}

		mov		r2, #0x1E0
1
		ldmia		r0!, {r3 - r12}

		str		r3, [r1]
		str		r4, [r1]
		str		r5, [r1]
		str		r6, [r1]
		str		r7, [r1]
		str		r8, [r1]
		str		r9, [r1]
		str		r10, [r1]
		str		r11, [r1]
		str		r12, [r1]

		subs		r2, r2, #40
		bne		%B1

		ldmia		r0!, {r3 - r10}
		
		str		r3, [r1]
		str		r4, [r1]
		str		r5, [r1]
		str		r6, [r1]
		str		r7, [r1]
		str		r8, [r1]
		str		r9, [r1]
		str		r10, [r1]
		
		ldmfd		sp!, {r2 - r12}

		mov		pc, lr	; return

	END

