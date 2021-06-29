// P_VIDEO.C

#include <BIOS.H>
#include <I32.H>
#include <STRING.H>
#include <MALLOC.H>
#include <STDLIB.H>

#include "d_global.h"
#include "d_ints.h"
#include "d_video.h"
#include "d_misc.h"
// added to allow drawing directly to buffer where frame is built
// TML 9-20-94
#include "r_public.h"

#define CRTCOFF (_inbyte(STATUS_REGISTER_1)&1)

void VI_CopyUpdate (void);
void VI_BlitMaskedPic (byte *source,byte *dest,int width,int height,int mask);

/*
=============================================================================

							 GLOBALS

=============================================================================
*/

byte            *screen = (byte *)SCREEN;

byte            *ylookup[SCREENHEIGHT];         // into video screen

/*
=============================================================================

			VGA REGISTER MANAGEMENT ROUTINES

=============================================================================
*/


/*
=================
=
= VI_SetTextMode
=
=================
*/

void VI_SetTextMode (void)
{
	union REGS      r;

	r.x.ax = 3;
	int86 (0x10,(const union REGS *)&r,&r);
}


//=========================================================================

/*
=================
=
= VI_SetVGAMode
=
=================
*/

void VI_SetVGAMode (void)
{
	union REGS      r;

	r.x.ax = 0x13;
	int86 (0x10,(const union REGS *)&r,&r);
}


//=========================================================================

/*
=================
=
= VI_WaitVBL
=
=================
*/

void VI_WaitVBL (int vbls)
{
	int     old;
	int     i,stat;

	while (vbls--)
	{
	// wait for display enabled, so we know it isn't just after vsync
	waitdisplay:
		CLI;
		while (CRTCOFF)
		;

	// wait for display just turned off
	waitbottom:
		STI;
		i--;            // time for an interrupt
		CLI;

		for (i=0;i<10;i++)
		{
			stat = _inbyte(STATUS_REGISTER_1);
			if (stat & 8)
				goto waitdisplay;               // vsync...
			if (!(stat&1))
				goto waitbottom;
		}
		STI;
	}
}


/*
=============================================================================

						PALETTE OPS

=============================================================================
*/


/*
=================
=
= VI_FillPalette
=
=================
*/

void VI_FillPalette (int red, int green, int blue)
{
	int     i;

	_outbyte (PEL_WRITE_ADR,0);
	for (i=0;i<256;i++)
	{
		_outbyte (PEL_DATA,red);
		_outbyte (PEL_DATA,green);
		_outbyte (PEL_DATA,blue);
	}
}

//===========================================================================

/*
=================
=
= VI_SetColor
=
=================
*/

void VI_SetColor        (int color, int red, int green, int blue)
{
	_outbyte (PEL_WRITE_ADR,color);
	_outbyte (PEL_DATA,red);
	_outbyte (PEL_DATA,green);
	_outbyte (PEL_DATA,blue);
}

//===========================================================================

/*
=================
=
= VI_GetColor
=
=================
*/

void VI_GetColor        (int color, int *red, int *green, int *blue)
{
	_outbyte (PEL_READ_ADR,color);
	*red = _inbyte (PEL_DATA);
	*green = _inbyte (PEL_DATA);
	*blue = _inbyte (PEL_DATA);
}

//===========================================================================

/*
=================
=
= VI_SetPalette
=
=================
*/

void VI_SetPalette (byte *palette)
{
	int     i;

	_outbyte (PEL_WRITE_ADR,0);
	for (i=0;i<768;i++)
		_outbyte(PEL_DATA,*palette++);
}


//===========================================================================

/*
=================
=
= VI_GetPalette
=
=================
*/

void VI_GetPalette (byte *palette)
{
	int     i;

	_outbyte (PEL_READ_ADR,0);
	for (i=0;i<768;i++)
		*palette++ = _inbyte(PEL_DATA);
}


//===========================================================================

/*
=================
=
= VI_FadeOut
=
= Fades the current palette to the given color in the given number of steps
=
=================
*/

void VI_FadeOut (int start, int end, int red, int green, int blue, int steps)
{
	byte    basep[256][3],work[256][3];
	int             i,j,delta;

	VI_GetPalette (&basep[0][0]);

//
// fade through intermediate frames
//
	for (i=0;i<steps;i++)
	{
		for (j=start;j<=end;j++)
		{
			delta = red-basep[j][0];
			work[j][0] = basep[j][0] + delta * i / steps;
			delta = green-basep[j][1];
			work[j][1] = basep[j][1] + delta * i / steps;
			delta = blue-basep[j][2];
			work[j][2] = basep[j][2] + delta * i / steps;
		}

		VI_WaitVBL(1);
		VI_SetPalette (&work[0][0]);
	}

//
// final color
//
	VI_FillPalette (red,green,blue);
}


/*
=================
=
= VI_FadeIn
=
=================
*/

void VI_FadeIn (int start, int end, byte *palette, int steps)
{
	byte    basep[768],work[768];
	int             i,j,delta;

	VI_GetPalette (basep);
	start *= 3;
	end *= 3;

//
// fade through intermediate frames
//
	for (i=0;i<steps;i++)
	{
		for (j=start;j<=end;j++)
		{
			delta = palette[j]-basep[j];
			work[j] = basep[j] + delta * i / steps;
		}

		VI_WaitVBL(1);
		VI_SetPalette (work);
	}

//
// final color
//
	VI_SetPalette (palette);
}


/*
=================
=
= VI_ColorBorder
=
=================
*/

void VI_ColorBorder (int color)
{
	_inbyte(STATUS_REGISTER_1);
	_outbyte(ATR_INDEX,ATR_OVERSCAN);
	_outbyte(ATR_INDEX,color);
	_outbyte(ATR_INDEX,0x20);
}



/*
=============================================================================

							PIXEL OPS

=============================================================================
*/


/*
=================
=
= VI_Plot
=
=================
*/

void VI_Plot (int x, int y, int color)
{
	*(ylookup[y]+x) = color;
}


/*
=================
=
= VI_Hlin
=
=================
*/

void VI_Hlin (int x, int y, int width, int color)
{
	memset (ylookup[y]+x,color,(size_t)width);
}


/*
=================
=
= VI_Vlin
=
=================
*/

void VI_Vlin (int x, int y, int height, int color)
{
	byte    *dest;

	dest = ylookup[y]+x;
	while (height--) {
		*dest = color;
		dest += SCREENWIDTH;
	}
}


/*
=================
=
= VI_Bar
=
=================
*/

void VI_Bar (int x, int y, int width, int height, int color)
{
	byte    *dest;

	dest = ylookup[y]+x;
	while (height--) {
		memset (dest,color,(size_t)width);
		dest += SCREENWIDTH;
	}
}


/*
=============================================================================

							BLOCK OPS

=============================================================================
*/


/*
=================
=
= VI_DrawPic
=
= Draws a block shape (in planar format) to the screen
=
=================
*/

void VI_DrawPic (int x, int y, pic_t *pic)
{
	byte    *dest, *source;
	int             width,height;

	width = pic->width;
	height = pic->height;
	source = &pic->data;

	dest = ylookup[y]+x;
	while (height--) {
		memcpy (dest,source,(size_t)width);
		source += width;
		dest += SCREENWIDTH;
	}
}



/*
=================
=
= VI_DrawMaskedPic
=
= Masks a block of main memory to the screen.
= Uses orgx,orgy to offset the block
= If the hot spot is inside the block shape, the offsets are positive
=
=================
*/

void VI_DrawMaskedPic (int x, int y, pic_t  *pic)
{
	byte    *dest, *source;
	int             width,height,xcor,col;

	x -= pic->orgx;
	y -= pic->orgy;
	height = pic->height;
	source = &pic->data;
	while (y<0) {
		source += pic->width;
		height--;
	}
	while (height--) {
		if (y<200) {
			dest = ylookup[y]+x;
			xcor = x;
			width = pic->width;
			while (width--) {
				if ((xcor>=0)&&(xcor<=319)) {
					if (*source) *dest = *source;
				}
				xcor++;
				source++;
				dest++;
			}
		}
		y++;
	}
}

/*
=================
=
= VI_DrawMaskedPicToBuffer
=
= Masks a block of main memory to the offscreen buffer where frame is built
= Uses orgx,orgy to offset the block
= If the hot spot is inside the block shape, the offsets are positive
=
=================
*/

// TML - Added 9-20-94 (allows image transfer to viewbuffer)

void VI_DrawMaskedPicToBuffer2 (int x, int y, pic_t  *pic)
{
	byte    *dest,*source;
	int             width,height,xcor,col;

	x -= pic->orgx;
	y -= pic->orgy;
	height = pic->height;
	source = &pic->data;
	while (y<0) {
		source += pic->width;
		height--;
	}
	while (height--) {
		if (y<200) {
//                        dest = ylookup[y]+x; // for blitting to video mem.
			dest = viewbuffer+(y*MAX_VIEW_WIDTH+x);     
			xcor = x;
			width = pic->width;
			while (width--) {
				if ((xcor>=0)&&(xcor<=319)) {
					if (*source) *dest = *source;
				}
				xcor++;
				source++;
				dest++;
			}
		}
		y++;
	}
}

void VI_Init (void)
{
	int     y;

	for (y=0;y<SCREENHEIGHT;y++)
		ylookup[y] = screen + y*SCREENWIDTH;
	VI_SetVGAMode ();
}

