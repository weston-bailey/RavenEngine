// d_video.h

#ifndef __VIDEO__
#define __VIDEO__

#define _outbyte(x,y)   (_outbyte((unsigned short)(x),(unsigned char)(y)))
#define _outhword(x,y)  (_outhword((unsigned short)(x),(unsigned short)(y)))

#define _inbyte(x)              (_inbyte((unsigned short)(x)))
#define _inhword(x)             (_inhword((unsigned short)(x)))

/*
=============================================================================

							CONSTANTS

=============================================================================
*/


#define SC_INDEX                0x3C4
#define SC_RESET                0
#define SC_CLOCK                1
#define SC_MAPMASK              2
#define SC_CHARMAP              3
#define SC_MEMMODE              4

#define CRTC_INDEX              0x3D4
#define CRTC_H_TOTAL    0
#define CRTC_H_DISPEND  1
#define CRTC_H_BLANK    2
#define CRTC_H_ENDBLANK 3
#define CRTC_H_RETRACE  4
#define CRTC_H_ENDRETRACE 5
#define CRTC_V_TOTAL    6
#define CRTC_OVERFLOW   7
#define CRTC_ROWSCAN    8
#define CRTC_MAXSCANLINE 9
#define CRTC_CURSORSTART 10
#define CRTC_CURSOREND  11
#define CRTC_STARTHIGH  12
#define CRTC_STARTLOW   13
#define CRTC_CURSORHIGH 14
#define CRTC_CURSORLOW  15
#define CRTC_V_RETRACE  16
#define CRTC_V_ENDRETRACE 17
#define CRTC_V_DISPEND  18
#define CRTC_OFFSET             19
#define CRTC_UNDERLINE  20
#define CRTC_V_BLANK    21
#define CRTC_V_ENDBLANK 22
#define CRTC_MODE               23
#define CRTC_LINECOMPARE 24


#define GC_INDEX                0x3CE
#define GC_SETRESET             0
#define GC_ENABLESETRESET 1
#define GC_COLORCOMPARE 2
#define GC_DATAROTATE   3
#define GC_READMAP              4
#define GC_MODE                 5
#define GC_MISCELLANEOUS 6
#define GC_COLORDONTCARE 7
#define GC_BITMASK              8

#define ATR_INDEX               0x3c0
#define ATR_MODE                16
#define ATR_OVERSCAN    17
#define ATR_COLORPLANEENABLE 18
#define ATR_PELPAN              19
#define ATR_COLORSELECT 20

#define STATUS_REGISTER_1    0x3da

#define PEL_WRITE_ADR   0x3c8
#define PEL_READ_ADR    0x3c7
#define PEL_DATA                0x3c9


//=================================

#define SCREEN                          0xa0000

#define SCREENWIDTH             320
#define SCREENHEIGHT    200

// make sure equates in NID_VH_A.ASM are also correct

/*
=============================================================================

							 TYPES

=============================================================================
*/

typedef struct
{
	short   width,height;
	short   orgx,orgy;
	byte    data;
} pic_t;


/*
=============================================================================

							 GLOBALS

=============================================================================
*/

extern  byte            *screen;
extern  byte            *ylookup[SCREENHEIGHT];         // into video screen

/*
=============================================================================

							FUNCTIONS

=============================================================================
*/

#define CLI     _setflags((unsigned short)(_getflags()&~_FLAG_INTERRUPT))
#define STI     _setflags((unsigned short)(_getflags()|_FLAG_INTERRUPT))


void VI_Init (void);

void VI_SetVGAMode(void);
void VI_SetTextMode(void);


//
// vga register ops
//

void VI_WaitVBL(int vbls);

//
// palette ops
//
void VI_SetPalette(byte *pallete);
void VI_GetPalette(byte *pallete);
void VI_FillPalette(int red, int green, int blue);
void VI_SetColor(int color, int red, int green, int blue);
void VI_GetColor(int color, int *red, int *green, int *blue);
void VI_FadeOut (int start, int end, int red, int green, int blue, int steps);
void VI_FadeIn(int start, int end, byte *pallete, int steps);
void VI_ColorBorder(int color);


//
// pixel ops
//
void VI_Plot(int x, int y, int color);
void VI_Hlin(int x, int y, int width, int color);
void VI_Vlin(int x, int y, int height, int color);
void VI_Bar(int x, int y, int width, int height, int color);

//
// block ops
//
void VI_DrawPic(int x, int y, pic_t  *pic);
void VI_DrawMaskedPic2(int x, int y, pic_t  *pic);
void VI_DrawMaskedPicToBuffer(int x, int y, pic_t *pic);

#endif
