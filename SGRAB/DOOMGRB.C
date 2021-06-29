#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <process.h>
#include <dos.h>
#include <alloc.h>

#include "sgrab.h"
#pragma hdrstop

#define TRANSCOLOR		0


typedef struct
{
	short		leftoffset;	// source pixels to the left of midpoint
	short		width;		// last row used - first row + 1
	short		collumnofs[256];	// only [width] used, [0] is &collumnofs[width]
							// 0 collumnofs means empty row
							// the data is : top, bottom, pixels...
} dsprite_t;



/*
==============
=
= GrabDSprite
=
= filename dsprite x y width height
=
==============
*/

void GrabDSprite (void)
{
	int		x,y,xl,yl,w,h;
	byte	far	*screen_p;
	int		linedelta;
	dsprite_t	far *header;
	int		first,last;

	GetToken (false);
	xl = atoi (token);
	GetToken (false);
	yl = atoi (token);
	GetToken (false);
	w = atoi (token);
	GetToken (false);
	h = atoi (token);


//
// find first and last collumn
//
	screen_p = MK_FP(0xa000,yl*SCREENWIDTH+xl);
	for (first=0 ; first<w ; first++)
	{
		for (y=0 ; y<h ; y++)
			if ( *(screen_p+y*SCREENWIDTH) != TRANSCOLOR )
				goto gotfirst;
		screen_p++;
	}
	MS_Quit ("GrabDSprite: Nothing in block");
gotfirst:

	screen_p = MK_FP(0xa000,yl*SCREENWIDTH+xl+w-1);
	for (last=w-1 ; last>=0 ; last--)
	{
		for (y=0 ; y<h ; y++)
			if ( *(screen_p+y*SCREENWIDTH) != TRANSCOLOR )
				goto gotlast;
		screen_p--;
	}
gotlast:

	w = last - first + 1;
	header = (dsprite_t far *)lump_p;
	header->leftoffset = w/2 - first;
	header->width = w;

//
// start grabbing rows
//
	lump_p = (byte far *)&header->collumnofs[header->width];

	screen_p = MK_FP(0xa000,yl*SCREENWIDTH+xl+first);
	for (x=0 ; x< w ; x++,screen_p++)
	{
	// find first good pixel
		for (first=0 ; first<h ; first++)
		{
			if ( *(screen_p+first*SCREENWIDTH) != TRANSCOLOR )
				break;
			*(screen_p+first*SCREENWIDTH) ^= 0xff;
		}
		if (first==h)
		{
			header->collumnofs[x] = 0;	// nothing in this collumn
			continue;
		}
	// find last good pixel
		for (last=h-1 ; last>=0 ; last--)
		{
			if ( *(screen_p+last*SCREENWIDTH) != TRANSCOLOR )
				break;
			*(screen_p+last*SCREENWIDTH) ^= 0xff;
		}

	// grab that segment
		header->collumnofs[x] = lump_p - (byte far *)header;
		*lump_p++ = h - 1 - first;
		*lump_p++ = h - 1 - last;
		for (y=first ; y<=last ; y++)
		{
			*lump_p++ = *(screen_p+y*SCREENWIDTH);
			*(screen_p+y*SCREENWIDTH) = 0xff;
		}
	}
}


