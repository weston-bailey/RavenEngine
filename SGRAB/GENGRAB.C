#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <process.h>
#include <dos.h>

#include "sgrab.h"
#pragma hdrstop



typedef struct
{
	unsigned	width,height;
	unsigned	planeofs[4];
	int			orgx,orgy;			// from here on out is optional
	int			hitxl,hityl,hitxh,hityh;
} pic_t;

typedef struct
{
	short	width,height;
	short	orgx,orgy;
	byte	data;
} lpic_t;

typedef struct
{
	unsigned	height;
	char		width[256];
	unsigned	charofs[256];
} font_t;




/*
==============
=
= GrabRaw
=
= filename RAW x y width height
=
==============
*/

void GrabRaw (void)
{
	int		x,y,xl,yl,xh,yh,w,h;
	byte	far	*screen_p;
	int		linedelta;

	GetToken (false);
	xl = atoi (token);
	GetToken (false);
	yl = atoi (token);
	GetToken (false);
	w = atoi (token);
	GetToken (false);
	h = atoi (token);

	xh = xl+w;
	yh = yl+h;

	screen_p = MK_FP(0xa000,yl*SCREENWIDTH+xl);
	linedelta = SCREENWIDTH - w;

	for (y=yl ; y<yh ; y++)
	{
		for (x=xl ; x<xh ; x++)
		{
			*lump_p++ = *screen_p;
			*screen_p++ = 0;
		}
		screen_p += linedelta;
	}
}


/*
==============
=
= GrabPic
=
= filename PIC x y width height [orgx orgy [hitxl hityl hitxh hityh]]
=
==============
*/

void GrabPic (void)
{
	int		x,y,xl,yl,xh,yh,w,h,p;
	int		clipplane;
	pic_t	far	*header;
	byte	far	*screen_p;

	GetToken (false);
	xl = atoi (token);
	GetToken (false);
	yl = atoi (token);
	GetToken (false);
	w = atoi (token);
	GetToken (false);
	h = atoi (token);

	clipplane = w&3;	// pad with 0 at end of x after this plane
	if (!clipplane)
		clipplane = 4;

	w = (w+3)/4;

	header = (pic_t far *)lump_p;
	header->width = w;
	header->height = h;

	lump_p = (byte far *)&header->orgx;

//
// optional origin offset
//
	if (TokenAvailable())
	{
		GetToken (false);
		header->orgx = atoi (token);
		GetToken (false);
		header->orgy = atoi (token);
		lump_p = (byte far *)&header->hitxl;
	//
	// optional hit rectangle
	//
		if (TokenAvailable())
		{
			GetToken (false);
			header->hitxl = atoi (token);
			GetToken (false);
			header->hitxh = atoi (token);
			GetToken (false);
			header->hityl = atoi (token);
			GetToken (false);
			header->hityh = atoi (token);
			lump_p = ((byte far *)&header->hityh)+1;
		}
	}


//
// grab in munged format
//
	for (p=0 ; p<4 ; p++)
	{
		header->planeofs[p] = lump_p - lumpbuffer;

		for (y=0 ; y<h ; y++)
		{
			screen_p = MK_FP(0xa000,(y+yl)*320+xl+p);
			for (x=0 ; x<w ; x++)
			{
				if (x==w-1 && p>=clipplane)
					*lump_p++ = 0;		// pad with transparent
				else
				{
					*lump_p++ = *screen_p;
					*screen_p = 0;
				}
				screen_p += 4;
			}
		}
	}

}


/*
==============
=
= GrabLinearPic
=
= filename LPIC x y width height [orgx orgy]
=
==============
*/

void GrabLinearPic (void)
{
	int		x,y,xl,yl,xh,yh,w,h,p;
	int		clipplane;
	lpic_t	far	*header;
	byte	far	*screen_p;

	GetToken (false);
	xl = atoi (token);
	GetToken (false);
	yl = atoi (token);
	GetToken (false);
	w = atoi (token);
	GetToken (false);
	h = atoi (token);

	header = (lpic_t far *)lump_p;
	header->width = w;
	header->height = h;


//
// optional origin offset
//
	if (TokenAvailable())
	{
		GetToken (false);
		header->orgx = atoi (token);
		GetToken (false);
		header->orgy = atoi (token);
	}
	else
		header->orgx = header->orgy = 0;

	lump_p = (byte far *)&header->data;


//
// grab it
//
	for (y=0 ; y<h ; y++)
	{
		screen_p = MK_FP(0xa000,(y+yl)*320+xl);
		for (x=0 ; x<w ; x++)
		{
			*lump_p++ = *screen_p;
			*screen_p++ = 0xff;
		}
	}

}



/*
==============
=
= GrabPalette
=
= filename PALETTE [startcolor endcolor]
=
==============
*/

void GrabPalette (void)
{
	int		start,end,color;

	if (TokenAvailable())
	{
		GetToken (false);
		start = atoi (token);
		GetToken (false);
		end = atoi (token);
	}
	else
	{
		start = 0;
		end = 255;
	}

	outportb (PEL_READ_ADR,start);
	for (color=start ; color<=end ; color++)
	{
		*lump_p++ = inportb (PEL_DATA);
		*lump_p++ = inportb (PEL_DATA);
		*lump_p++ = inportb (PEL_DATA);
	}
}


/*
=============================================================================

							FONT GRABBING

=============================================================================
*/


font_t	far *font;
int		sx,sy,ch;


/*
===================
=
= GrabChar
=
= Grabs the next character after sx,sy of height font->height, and
= advances sx,sy
=
===================
*/

int GrabChar (void)
{
	int		y;
	byte	far	*screen_p,b;
	int		count;

//
// look for a vertical line with a source pixel
//

	do
	{
		screen_p = MK_FP(0xa000,sy*SCREENWIDTH+sx);
		for (y=0 ; y<font->height ; y++)
			if (screen_p[y*SCREENWIDTH])
				goto startgrabing;
		if (++sx == SCREENWIDTH)
		{
			sx=0;
			sy += font->height+1;
			if (sy+font->height > SCREENHEIGHT)
			{
				printf ("Ran out of characters at char %i\n",ch);
				exit (1);
			}
		}

	} while (1);

startgrabing:
//
// grab the character
//
	font->width[ch] = 0;
	font->charofs[ch] = lump_p - (byte far *)font;

	do
	{
		font->width[ch]++;

		screen_p = MK_FP(0xa000,sy*SCREENWIDTH+sx);
		count = 0;

		for (y=0 ; y<font->height ; y++)
		{
			b = *screen_p;
			if (b)
				count++;
			if (b==254)		// 254 is a grabbable 0
				b = 0;
			*lump_p++ = b;
			screen_p += SCREENWIDTH;
		}

		if (count)	// color the grabbed collumn
			for (y=0 ; y<font->height ; y++)
			{
				screen_p -= SCREENWIDTH;
				*screen_p = 1;
			}

		if (++sx == SCREENWIDTH)
		{
			sx=0;
			sy += font->height+1;
			return;
		}

		if (!count)		// hit a blank row?
		{
			lump_p -= font->height;
			font->width[ch]--;
			return;
		}

	} while (1);
}


/*
===================
=
= GrabFont
=
= filename FONT startchar endchar [startchar endchar [...]]
=
===================
*/

void GrabFont (void)
{
	int		c;
	int		x,y;
	byte	far	*screen_p;
	int		top,bottom;
	int		startchar,endchar;

	font = (font_t far *)lump_p;
	_fmemset (font,0,sizeof(*font));

//
// find the height of the font by scanning for quide lines (color 255)
//
	screen_p = MK_FP(0xa000,0);

	top = -1;
	for (y=0;y<10;y++)
		if (screen_p[y*SCREENWIDTH] == 255)
		{
			top = y;
			break;
		}

	if (top == -1)
		MS_Quit ("No color 255 top guideline found!\n");

	bottom = -1;
	for ( y++ ; y<100 ; y++)
		if (screen_p[y*SCREENWIDTH] == 255)
		{
			bottom = y;
			break;
		}

	if (bottom == -1)
		MS_Quit ("No color 255 bottom guideline found!\n");

	font->height = bottom-top-1;
	lump_p = &(byte)font->charofs[256];

	sx = 0;
	sy = top+1;

//
// grab ranges of characters
//
	do
	{
		GetToken (false);
		startchar = atoi (token);
		GetToken (false);
		endchar = atoi (token);

		for (ch=startchar ; ch<=endchar ; ch++)
			GrabChar ();

	} while (TokenAvailable ());
}


