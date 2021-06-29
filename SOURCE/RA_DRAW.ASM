	.386
	.MODEL  small

SKIPPRIMITIVES = 0         ; set to 1 to skip unwound drawing

INCLUDE viewsize.inc
INCLUDE macros.inc


SCREEN        = 0a0000h
SCREENWIDTH   = 320

PEL_WRITE_ADR = 03c8h
PEL_DATA      = 03c9h

	.DATA

EXTRN  viewbuffer:WORD
EXTRN  viewLocation:DWORD
EXTRN  viewylookup;DWORD
EXTRN  windowWidth:DWORD
EXTRN  windowHeight:DWORD


	.CODE

;============================================================================
;
; Fixed point math
;
;============================================================================

@Proc   FIXEDMUL, <>, <<op1,DWORD>,<op2,DWORD>>
	mov     eax,op1
	imul    [op2]
	shrd    eax,edx,16
	@exitp <>
ENDP

@Proc   FIXEDDIV, <>, <<op1,DWORD>,<op2,DWORD>>
	mov     eax,op1
	cdq
	shld    edx,eax,16
	sal     eax,16
	idiv    [op2]
	endz:
@exitp <>

ENDP



;============================================================================
;
; RF_BlitView
;
;============================================================================

@Proc   RF_BlitView, <ebx, esi, edi>

	mov     esi,OFFSET viewbuffer
	mov     edi,[viewLocation]
	mov     ebx,[windowHeight]
	mov     eax,SCREENWIDTH
	sub     eax,[windowWidth]
	mov     edx,eax                ; x modulos
	mov     eax,[windowWidth]
	shr     eax,2

ALIGN   4
@@blitloop1:
	mov     ecx,eax
	rep     movsd
	add     edi,edx
	dec     ebx
	jnz     @@blitloop1

	@exitp <ebx, esi, edi>
ENDP

;============================================================================
;
; ClearViewBuffer
;
;============================================================================

@Proc   ClearViewBuffer, <ebx, esi, edi>

	mov     ebx,[windowHeight]
	mov     esi,[windowWidth]
	mov     edx,esi
	shr     esi,2
	xor     eax,eax

ALIGN   4
@@clearloop:
	dec     ebx
	mov     ecx,esi
	mov     edi,OFFSET viewylookup
	mov     edi,[edi+ebx*4]
	rep     stosd
	add     edi,edx
	cmp     ebx,0
	jnz     @@clearloop

	@exitp <ebx, esi, edi>
ENDP

;============================================================================
;
; Merge
;
; merge two parts of the unsorted array to the sorted array
;
;============================================================================

.DATA

EXTRN   src1:DWORD
EXTRN   src2:DWORD
EXTRN   dest:DWORD
EXTRN   size1:DWORD
EXTRN   size2:DWORD

PUBLIC  mergefrom1
PUBLIC  finishfrom1
PUBLIC  mergefrom2
PUBLIC  finishfrom2
PUBLIC  mergedone

.CODE

	@Proc   Merge, <ebx,esi,edi>

	mov     ebx,[src1]
	mov     esi,[src2]
	mov     edi,[dest]
	mov     ecx,[size1]
	mov     edx,[size2]

	mov     eax,[ebx]
	cmp     eax,[esi]
	jb      SHORT mergefrom2                ; if (*src1 < *src2) goto mergefrom2;

mergefrom1:
	stosd
	add     ebx,4                   ; *dest++ = *src1++;
	mov     eax,[ebx]
	dec     ecx
	jz      finishfrom2             ; if (!--size1) goto finishfrom2;
	cmp     eax,[esi]
	ja      SHORT mergefrom1                ; if (*src1 > *src2) goto mergefrom1;

mergefrom2:
	movsd                           ; *dest++ = *src2++;
	dec     edx
	jz      finishfrom1             ; if (!--size2) goto finishfrom1;
	cmp     eax,[esi]
	ja      SHORT mergefrom1                ; if (*src1 > *src2) goto mergefrom1;
	jmp     SHORT mergefrom2

	ALIGN   4
finishfrom2:
	movsd                           ; *dest++ = *src2++;
	dec     edx             ; while (size2--)
	jnz     SHORT finishfrom2
	jmp     SHORT mergedone

	ALIGN   4
finishfrom1:
	stosd
	add     ebx,4                   ; *dest++ = *src1++;
	mov     eax,[ebx]
	loop SHORT finishfrom1  ; while (size1--)

mergedone:
	mov     [dest],edi
	@exitp <ebx, esi, edi>
	ENDP


;============================================================================
;
; unwound vertical scaling code
;
; eax   light table pointer, 0 lowbyte overwritten
; ebx   all 0, low byte overwritten
; ecx   fractional step value
; edx   fractional scale value
; esistart of source pixels
; edi   bottom pixel in screenbuffer to blit into
;
; ebx should be set to 0 0 0 dh to feed the pipeline
;============================================================================

; the assembler creates vscale<MAXWINDOWHEIGHT-1>: numbers of the following
; code segement.  LINE is used as the arg to SCALELABEL.   there exists
; vscale<MAXWINDOWHEIGHT: to vscale0: units of code.  vscale0 is used as
; the exit point.


SCALELABEL MACRO number
vscale&number:
ENDM

LINE = MAXWINDOWHEIGHT
REPT MAXWINDOWHEIGHT - 1
	SCALELABEL %LINE                     ; LINE represents how many
					     ; ypixels need to be drawn
	mov     al,[esi+ebx]                 ; get source pixel
	add     edx,ecx                      ; calculate next location
	mov     al,[eax]                     ; translate the color
	xor     ebx,ebx                      ; clear ebx
	shld    ebx,edx,16                   ; get address of next location
	mov     [edi-(LINE-1)*MAXWINDOWWIDTH],al

LINE = LINE - 1
ENDM

vscale1:
	mov     al,[esi+ebx]
	add     edx,ecx
	mov     al,[eax]
	mov     [edi],al
vscale0:
	ret



.DATA

; the following 2 macros allocate a block of dd or 4 byte locations that are
; ALIGN'ed on a 4 byte or 32 bit boundary.  the starting 4 bytes may be
; referred to as scalecalls.  the others may not because in the macro
; SCALEDEFINE they are not given a name.  the label scalecalls refers to a
; position in the code which happens to be the start of the allocated block.   there are
; MAXWINDOWHEIGHT + 1 4 byte word's are allocated and each word is initialized
; with the address of vscale<LINE>:.  the value of LINE ranges from 0 to
; MAXWINDOWWIDTH.  scalecalls is made visible to the other modules with the
; PUBLIC directive.


SCALEDEFINE MACRO number
  dd vscale&number
ENDM

ALIGN 4
scalecalls LABEL UNKNOWN
LINE = 0
REPT MAXWINDOWHEIGHT + 1
  SCALEDEFINE %LINE
LINE = LINE + 1
ENDM

scalecall dd 0

PUBLIC scalecalls

;=================================================

;
; parameters for RN_ScalePost
;
sp_dest       dd  0
sp_count      dd  0
sp_fracstep   dd  0
sp_frac       dd  0
sp_source     dd  0
sp_colormap   dd  0

PUBLIC  sp_dest, sp_count, sp_fracstep, sp_frac, sp_source, sp_colormap

.CODE


;================
;
; ScalePost
;
;================

	@Proc   ScalePost, <ebx, esi, edi, ebp>

IFE SKIPPRIMITIVES

 	mov     edx,[sp_frac]
	mov     ecx,[sp_fracstep]
	mov     esi,[sp_source]
	mov     eax,[sp_colormap]
	mov     edi,[sp_dest]
	xor     ebx,ebx
	shld    ebx,edx,16                                              ; get address of first location
	mov     ebp,[sp_count]
	call [scalecalls+ebp*4]

ENDIF

	@exitp <ebx, esi, edi, ebp>
	ENDP

;============================================================================

    	@Proc  GetScaleRoutines

        mov    eax,OFFSET vscale200

   	@exitp
       	ENDP

;
;============================================================================
;
; unwound masked vertical scaling code
;
; eax   light table pointer, 0 lowbyte overwritten
; ebx   all 0, low byte overwritten
; ecx   all 0, low byte overwritten
; edx   fractional scale value
; esi   start of source pixels
; edi   bottom pixel in screenbuffer to blit into
; ebp   fractional step value
;
; ebx should be set to 0 0 0 dh to feed the pipeline
;============================================================================

; the assembler creates mvscale<MAXWINDOWHEIGHT-1>: numbers of the following
; code segement.  all sections of the code are ALINGN'ed on a 32 bit
; boundary.  LINE is used as the arg to MSCALELABEL.   there exists
; mvscale<MAXWINDOWHEIGHT: to mvscale0: units of code.  mvscale0 is used as
; the exit point.

ALIGN 4
MSCALELABEL MACRO number
mvscale&number:
ENDM

LINE = MAXWINDOWHEIGHT
REPT MAXWINDOWHEIGHT - 1
	MSCALELABEL %LINE
	mov     al,[esi+ebx]            ; get source pixel
	add     edx,ebp                 ; calculate next location
	mov     cl,al                   ; save original color for jcxz
	mov     al,[eax]                ; translate the color
	xor     ebx,ebx
	shld    ebx,edx,16              ; get address of next location
	jcxz    $+9                     ; jumps 14 bytes to start of next macro
	mov     [edi-(LINE-1)*MAXWINDOWWIDTH],al  ; draw a pixel to the buffer

LINE = LINE - 1
ENDM

mvscale1:
	mov     al,[esi+ebx]
	add     edx,ecx
	mov     al,[eax]
	test    al,0
	jz      mvscale0
	mov     [edi],al
mvscale0:
	ret


.DATA

; the following 2 macros allocate a block of dd or 4 byte locations that are
; ALIGN'ed on a 4 byte or 32 bit boundary.  the starting 4 bytes may be
; referred to as mscalecalls.  the others may not because in the macro
; MSCALEDEFINE they are not given a name.  the label mscalecalls refers to a
; position in the code which happens to be the start of the allocated block.   there are
; MAXWINDOWHEIGHT + 1 4 byte word's are allocated and each word is initialized
; with the address of mvscalev<LINE:.  the value of LINE ranges from 0 to
; to MAXWINDOWHEIGHT.  mscalecalls is made visible to the other modules with
; the PUBLIC directive.

MSCALEDEFINE MACRO number
  dd mvscale&number
ENDM

ALIGN 4
mscalecalls LABEL UNKNOWN
LINE = 0
REPT MAXWINDOWHEIGHT + 1
  MSCALEDEFINE %LINE
LINE = LINE + 1
ENDM

PUBLIC mscalecalls

mscalecall dd 0

.CODE


;================
;
; ScaleMaskedPost
;
; Same parameters as ScalePost, but 0 pixels are not drawn
;
;================

	@Proc   ScaleMaskedPost, <ebx, esi, edi, ebp>

IFE SKIPPRIMITIVES

	mov     ebp,[sp_count]
	mov     eax,[mscalecalls+ebp*4]
	mov     [mscalecall],eax

	mov     edx,[sp_frac]
	mov     ebp,[sp_fracstep]
	mov     esi,[sp_source]
	mov     eax,[sp_colormap]
	mov     edi,[sp_dest]
	xor     ebx,ebx
	xor     ecx,ecx
	shld    ebx,edx,16                                              ; get address of first location
	call    [mscalecall]

ENDIF
	@exitp <ebx, esi, edi, ebp>
	ENDP

;============================================================================

    	@Proc  GetMScaleRoutines

        mov    eax,OFFSET mvscale200

   	@exitp
       	ENDP

;============================================================================
;
; unwound horizontal texture mapping code
;
; eax   lighttable
; ebx   xtotal  6 bits units 26 bits frac
; ecx   ytotal  6 bits units 26 bits frac
; edx   xstep
; esi   start of block
; edi   dest
; ebp   scratch offset
;
; [ystep]
;
; ebp should by preset from ebx / ecx before calling
;============================================================================

; the assembler creates hmap<MAXWINDOWWIDTH>: numbers of the following
; code segement.  all sections of the code are not ALINGN'ed on a 32 bit
; boundary.  LINE is used as the arg to MAPLABEL.  there exists
; hmapMAXWINDOWWIDTH: to hmap0: units of code.  map0 is used as the exit
; point.

MAPLABEL MACRO number
hmap&number:
ENDM

LINE = MAXWINDOWWIDTH
REPT MAXWINDOWWIDTH
	MAPLABEL %LINE
	mov     al,[esi+ebp]            ; get source pixel
	add     ebx,edx                 ; xtotal += xstep
	add     ecx,[ystep]             ; ytotal += ystep
	xor     ebp,ebp
	mov     al,[eax]                ; translate color
	shld    ebp,ecx,6               ; shift in new y/x position
	shld    ebp,ebx,6
	mov     [edi-(LINE-1)],al       ; write pixel
LINE = LINE - 1
ENDM
hmap0:
	ret


.DATA

ystep dd 0
mapcall dd  0

; the following 2 macros allocate a block of dd or 4 byte locations that are
; ALIGN'ed on a 4 byte or 32 bit boundary.  the starting 4 bytes may be
; referred to as mapcalls.  the others may not because in the macro MAPDEFINE
; they are not given a name.  the label mapcalls refers to a position in the
; code which happens to be the start of the allocated block.   there are
; MAXWINDOWWIDTH + 1 4 byte word's are allocated and each word is initialized
; with the address of hmap<LINE>:.  the value of LINE ranges from 0 to
; MAXWINDOWWIDTH.  mapcalls is made visible to the other modules with the
; PUBLIC directive.

MAPDEFINE MACRO number
  dd hmap&number
ENDM

ALIGN 4
mapcalls LABEL UNKNOWN
LINE = 0
REPT MAXWINDOWWIDTH + 1
  MAPDEFINE %LINE
LINE = LINE + 1
ENDM

PUBLIC  mapcalls




;
; parameters for RN_MapLine
;
mr_dest     dd  0          ; pointer to first pixel in view buffer
mr_count    dd  0
mr_picture  dd  0          ; pointer to 4096 pixel block
mr_colormap dd  0          ; page aligned light table
mr_xfrac    dd  0          ; 16 frac bits
mr_yfrac    dd  0          ; 16 frac bits
mr_xstep    dd  0
mr_ystep    dd  0

PUBLIC  mr_dest, mr_picture, mr_colormap, mr_xfrac
PUBLIC  mr_yfrac, mr_xstep, mr_ystep, mr_count

.CODE


;================
;
; MapRow
;
; Horizontal texture mapping
;
;================


@Proc   MapRow, <ebx, esi, edi>

IFE SKIPPRIMITIVES

	mov     eax,[mr_count]
	mov     ebx,[mapcalls+eax*4]
	mov     [mapcall],ebx       ; spot to jump into unwound

	mov     edi,[mr_dest]
	add     edi,[mr_count]
	dec     edi

	mov     eax,[mr_ystep]
	shl     eax,10
	mov     [ystep],eax

	mov     eax,[mr_colormap]
	mov     ebx,[mr_xfrac]
	shl     ebx,10
	mov     ecx,[mr_yfrac]
	shl     ecx,10
	mov     edx,[mr_xstep]
	shl     edx,10
	mov     esi,[mr_picture]

	xor     ebp,ebp
	shld    ebp,ecx,6
	shld    ebp,ebx,6      ; do first step for pipeline

	call    [mapcall]

	shr     ebx,10
	mov     [mr_xfrac],ebx
	shr     ecx,10
	mov     [mr_yfrac],ecx

ENDIF

	@exitp <ebx, esi, edi>
ENDP

END
