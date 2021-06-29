#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <process.h>
#include <dos.h>

#include "sgrab.h"
#pragma hdrstop

#define TILEBOXWIDTH	24
#define TILEBOXHEIGHT	32
#define TILESPERLINE	12

#define TILEWIDTH		16
#define TILEHEIGHT		30

#define MASKCOLOR		255

/*
==============
=
= GrabLynxWalls
=
= filename LYNXWALLS numtiles
=
==============
*/

void GrabLynxWalls (void)
{
	int		x,y,xl,yl,w,h;
	int		t,tiles;
	byte	far	*screen_p;

	GetToken (false);
	tiles = atoi (token);

	for (t=0 ; t<tiles ; t++)
	{
		x = t%TILESPERLINE;
		y = t/TILESPERLINE;

		xl = x*TILEBOXWIDTH;
		yl = y*TILEBOXHEIGHT + TILEHEIGHT/2;

		screen_p = MK_FP(0xa000,yl*SCREENWIDTH+xl);

		for (x=0 ; x<TILEWIDTH ; x++,screen_p++)
		{
		// grab down quadrant
			for (y=0 ; y<TILEHEIGHT/2 ; y++)
			{
				*lump_p++ = *screen_p;
				*screen_p = 0;
				screen_p += SCREENWIDTH;
			}
			screen_p -= SCREENWIDTH*(TILEHEIGHT/2+1);

		// grab up quadrant
			for (y=0 ; y<TILEHEIGHT/2 ; y++)
			{
				*lump_p++ = *screen_p;
				*screen_p = 0;
				screen_p -= SCREENWIDTH;
			}
			screen_p += SCREENWIDTH*(TILEHEIGHT/2+1);

		// pad to power of 2
			*lump_p++ = 0;
			*lump_p++ = 0;
		}
	}
}

//===========================================================================

byte	source[64][64];
int		counts[17];

#define QUADWIDTH	32
#define QUADHEIGHT	30
byte	quadrant[QUADHEIGHT][QUADWIDTH];


/*
==============
=
= GrabQuadrant
=
==============
*/

void GrabQuadrant (void)
{
	int	lastline,lastx;
	int	x,y,rle;
	byte	*length_p;
	unsigned	pixel;
	unsigned	databyte,bit;

//
// count lines in quadrant
//
	for (lastline=QUADHEIGHT-1;lastline>=0;lastline--)
	{
		for (x=0;x<QUADWIDTH;x++)
			if (quadrant[lastline][x] != MASKCOLOR)
				break;					// there is something on this line

		if (x != QUADWIDTH)
			break;						// got lastline
	}

//
// grab the lines
//
	for (y=0;y<=lastline;y++)
	{
	// see how long the line is
		for (lastx=QUADWIDTH-1;lastx>=0;lastx--)
			if (quadrant[y][lastx] != MASKCOLOR)
				break;

	// leave space for line length in bytes
		length_p = lump_p;
		lump_p++;

	// grab the line, packing pixels if 3 in a row
		databyte = bit = 0;

		for (x=0 ; x<=lastx ; x++)
		{
			pixel = quadrant[y][x];
			if (pixel==MASKCOLOR)
				pixel = 0;
			pixel <<= bit;
			databyte |= pixel;
			bit += 4;
			if (bit>=8)
			{
				*lump_p++ = (byte)databyte;
				bit-=8;
				databyte>>=8;
			}
#if 0
		// check for a run of pixels
			for (rle=0 ; x+rle<=lastx ; rle++)
				if (quadrant[y][x+rle+1] != pixel)
					break;
#endif

		}
		if (bit || *(lump_p-1)&1 || lump_p == length_p+1)
		// if bits left, lynx bug, or nothing on line, write a final byte out
			*lump_p++ = (byte)databyte;

	// write out the length
		*length_p = lump_p-length_p;
	}

}


/*
==============
=
= GrabLynxScale
=
= filename LYNXSCALE spot
=
==============
*/

void GrabLynxScale (void)
{
	int	spot,x,y,xl,yl,xh,yh;
	byte	far	*screen_p,pixel;
	int		linedelta;

	GetToken (false);
	spot = atoi (token);

	xl = spot%5*64+1;
	yl = spot/5*62+1;

//
// grab the source block
//
	xh = xl+63;
	yh = yl+60;

	screen_p = MK_FP(0xa000,yl*SCREENWIDTH+xl);
	linedelta = SCREENWIDTH - 63;

	memset (counts,0,sizeof(counts));

	for (y=yl ; y<yh ; y++)
	{
		for (x=xl ; x<xh ; x++)
		{
			pixel = *screen_p;
			*screen_p++ = 0;
			source[y-yl][x-xl] = pixel;
			if (pixel<16)
				counts[pixel]++;
		}
		source[y-yl][63] = MASKCOLOR;
		screen_p += linedelta;
	}

//
// write quadrants
//
	for (y=0;y<QUADHEIGHT;y++)
		for (x=0;x<QUADWIDTH;x++)
			quadrant[y][x] = source[QUADHEIGHT+y][QUADWIDTH+x];
	GrabQuadrant ();
	*lump_p++ = 1;		// mark quadrant change

	for (y=0;y<QUADHEIGHT;y++)
		for (x=0;x<QUADWIDTH;x++)
			quadrant[y][x] = source[QUADHEIGHT-1-y][QUADWIDTH+x];
	GrabQuadrant ();
	*lump_p++ = 1;		// mark quadrant change

	for (y=0;y<QUADHEIGHT;y++)
		for (x=0;x<QUADWIDTH;x++)
			quadrant[y][x] = source[QUADHEIGHT-1-y][QUADWIDTH-1-x];
	GrabQuadrant ();
	*lump_p++ = 1;		// mark quadrant change

	for (y=0;y<QUADHEIGHT;y++)
		for (x=0;x<QUADWIDTH;x++)
			quadrant[y][x] = source[QUADHEIGHT+y][QUADWIDTH-1-x];
	GrabQuadrant ();
	*lump_p++ = 0;		// end of sprite

}



