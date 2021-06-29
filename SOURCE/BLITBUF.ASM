	.386
	.MODEL  small

SKIPPRIMITIVES  =       0                       ; set to 1 to skip unwound drawing

INCLUDE viewsize.inc
INCLUDE macros.inc


SCREEN          =       0a0000h
SCREENWIDTH     =       320

PEL_WRITE_ADR   =       03c8h
PEL_DATA        =       03c9h

	.DATA

EXTRN  viewbuffer:WORD
EXTRN  viewLocation:DWORD
EXTRN  windowWidth:DWORD
EXTRN  windowHeight:DWORD

	.CODE

;=========================================================================
;= VI_DrawMaskedPicToBuffer                                              =
;=                                                                       =
;= C prototype:                                                          =
;= void VI_DrawMaskedPicToBuffer (int x, int y, pic_t *pic)              =
;=========================================================================

	 @Proc VI_DrawMaskedPicToBuffer, <eax,ebx,ecx,edx,esi,edi>, <<x,DWORD>,<y,DWORD>,<pic,DWORD>>


	 mov   edi,OFFSET viewbuffer
	 mov   esi,[pic]              ;  si -> pic

;-----  Set up y and height

	 xor   ecx,ecx
	 mov   cx,[esi+2]             ;  get height (row count) of image
	 mov   eax,[y]                ;  get row location
	 sub   ax,[esi+6]             ;  subtract orgy

;-----  Adjust for y < 0

	 or    eax,eax                ;  is y < 0 ?
	 jnl   short @@findheight     ;    if not, go to height adjust
	 add   ecx,eax                ;  if so, add y to height (h -= (-y))
	 xor   eax,eax                ;  y = 0

;-----  Check for bitmap going off the bottom edge

@@findheight:
	 push  eax                    ;  save y
	 add   eax,ecx                ;  bottom row of image
	 cmp   eax,windowHeight
	 pop   eax                    ;  restore y
	 jle   short @@heightok             ;  if not, get on with it
	 mov   ecx,windowHeight
	 sub   ecx,eax                ;      h = bottom - y

@@heightok:

;;;;     set es:edi to di+(eax*MAX_VIEW_WIDTH)

	 mov   ebx,windowWidth
	 mul   ebx                    ; oh no, not a mul!
	 add   edi,eax                ; move down Viewbuffer to proper row

;-----  Set up x and width

	 mov   edx,[x]                ;  get column location
	 sub   dx,[esi+4]             ;  subtract orgx
	 mov   bx,[esi]               ;  get width (col count) of image

;-----  Set up esi to pic->data

	 add   esi,8

;-----  Adjust for x < 0

	 or    edx,edx                ;  is x < 0:
	 jnl   short @@findwidth            ;      if not, go to width adjust
	 add   ebx,edx                ;  if so, add x to width (w -= (-x))

;-----  Check for bitmap going off the right edge

@@findwidth:
	 push  edx                    ;  save x
	 add   edx,ebx                ;  rightmost col of image
	 cmp   edx,windowWidth     ;  is (w+x) > right ?
	 pop   edx                    ;  restore x
	 jle   short @@widthok              ;  if not, get on with it
	 mov   ebx,windowWidth     ;  if so,
	 sub   ebx,edx                ;      w = right - x

@@widthok:

;-----  Set up edi to point to Veiwbuffer[x,y]

	 add   edi,edx

	 mov   edx,windowWidth     ;  set eax to pixels in viewbuffer
	 sub   edx,ebx                ;     to right of our image

@@rowloop:

	 push  ecx                    ;  save height (row count)

;------ Copy this row (masked) to Viewbuffer

	 mov   ecx,ebx                ;  get width (col count) of image
	 shr ecx,2

@@colloop:                            ;  for each pixel in row
;        lodsb                        ;  get the pixel
;        test  al,al                  ;  check for zero (mask)
;        jnz   short @@putit                ;  if not zero, put it in Viewbuffer
;        inc   di                     ;  otherwise, increment Viewbuffer ptr

	 lodsd
	 test  eax,eax
	 jnz   short @@try1
	 add   edi,4
	 loop  @@colloop
	 jmp   short @@rowdone

@@try1:
	 test al,al
	 jz   short @@try2
	 mov  [es:edi],al
@@try2:
	 inc  edi
	 shr  eax,8
	 test al,al
	 jz   short @@try3
	 mov  [es:edi],al
@@try3:
	 inc  edi
	 shr  eax,8
	 test al,al
	 jz   short @@try4
	 mov  [es:edi],al
@@try4:
	 inc  edi
	 shr  eax,8
	 test al,al
	 jz   short @@next4
	 mov  [es:edi],al
@@next4:
	 inc  edi
	 loop @@colloop              ;     and go to next pixel
;         jmp short @@rowdone              ;  we must be done with this row
;@@putit:                              ;  put the pixel in Viewbuffer
;         stosb                         ;  go ahead, put it there
;         loop   @@colloop              ;  go to the next pixel
@@rowdone:                            ;  this row is finished

;------

	 pop   ecx                    ;  restore height (row count)

	 add   edi,edx                ;  increment Viewbuffer to next row
	 loop  @@rowloop              ;  go to the next row

	 @exitp    <eax,ebx,ecx,edx,esi,edi>
	     ENDP
END
