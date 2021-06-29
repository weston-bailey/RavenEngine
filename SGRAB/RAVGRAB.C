#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <process.h>
#include <dos.h>
#include <alloc.h>

#include "sgrab.h"
#pragma hdrstop



typedef struct
{
	unsigned	height;		// in blocks*2
	unsigned	collumnofs[64];
} wall_t;


typedef struct
{
	unsigned	postofs[4096];
} holo_t;
// post data: numruns [skipvalue(*2) runlen [data]]

typedef struct
{
	unsigned	width;
	unsigned	top,bottom;
	unsigned	postofs[];
} scaleshape_t;



/*
==============
=
= GrabWall
=
= filename WALL xblock yblock blockheight
=
==============
*/

void GrabWall (void)
{
	int		x,y,xl,yl,xh,yh,w,h;
	byte	far	*screen_p;
	int		linedelta;
	wall_t	far *header;

	GetToken (false);
	xl = atoi (token)*8;
	GetToken (false);
	yl = atoi (token)*8;
	GetToken (false);
	h = atoi (token)*8;
	w = 64;

	xh = xl+w;
	yh = yl+h;

	header = (wall_t far *)lump_p;
	header->height = h/4;							// block height*2

	for (x=0;x<64;x++)
		header->collumnofs[x] = 65*2+x*h - (96-h);	// adjusted for entry

	lump_p = (byte far *)&header->collumnofs[64];

	screen_p = MK_FP(0xa000,yl*SCREENWIDTH+xl);

	for (x=xl;x<xh;x++)
	{
		for (y=yl;y<yh;y++)
		{
			screen_p = MK_FP(0xa000,y*SCREENWIDTH+x);
			*lump_p++ = *screen_p;
			*screen_p = 0;
		}
	}
}


/*
==============
=
= GrabFlat
=
= filename FLAT xblock yblock
=
==============
*/

void GrabFlat (void)
{
	int		x,y,xl,yl,xh,yh,w,h;
	byte	far	*screen_p;
	int		linedelta;

	GetToken (false);
	xl = atoi (token)*8;
	GetToken (false);
	yl = atoi (token)*8;

	w = h =64;
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
= GrabBump
=
= filename BUMP xblock yblock
=
==============
*/

void GrabBump (void)
{
	int		x,y,xl,yl,xh,yh,w,h;
	byte	far	*screen_p;
	int		linedelta;

	GetToken (false);
	xl = atoi (token)*8;
	GetToken (false);
	yl = atoi (token)*8;

	w = 64;
	h = 64;

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

//
// grab the bump map multiplied by two
//
	yh += 64;
	for ( ; y<yh ; y++)
	{
		for (x=xl ; x<xh ; x++)
		{
			*lump_p++ = *screen_p * 2;
			*screen_p++ = 0;
		}
		screen_p += linedelta;
	}
}


/*
=============================================================================

						SCALED SHAPE GRABBING

=============================================================================
*/

#define TRANSCOLOR		0
#define WORLDHEIGHT		96

byte	post[WORLDHEIGHT];

/*
==============
=
= SparsePost
=
= Writes out
=
==============
*/

void SparsePost (void)
{
	int	y,count;
	byte	far	*count_p;

	if (post[0] != TRANSCOLOR)
		MS_Quit ("First byte of post is not transparent");

	y = 0;
	do
	{
	// count transparent

		count = 0;
		while (post[y] == TRANSCOLOR)
		{
			y++;
			count++;
			if (y==WORLDHEIGHT)
			{
				*lump_p++ = 0;		// end of post
				return;
			}
		}

		*lump_p++ = count*2;		// skip this many pixels (words)

	// count solid

		count_p = lump_p;
		lump_p++;

		count = 0;
		while (post[y] != TRANSCOLOR)
		{
			*lump_p++ = post[y];
			y++;
			count++;
			if (y==WORLDHEIGHT)
			{
				*count_p = count;
				*lump_p++ = 0;		// end of post
				return;
			}
		}

		*count_p = count;			// write this many pixels

	} while (1);

}


/*
==============
=
= GrabScale
=
= filename SCALE x y width height
=
==============
*/

void GrabScale (void)
{
	int		x,y,xl,yl,w,h,start,adj;
	byte	far	*screen_p,pixel;
	scaleshape_t	far *header;


	GetToken (false);
	xl = atoi (token);
	GetToken (false);
	yl = atoi (token);
	GetToken (false);
	w = atoi (token);
	GetToken (false);
	h = atoi (token);

	start = WORLDHEIGHT-h;
	screen_p = MK_FP(0xa000,yl*SCREENWIDTH+xl);
	header = (scaleshape_t far *)lump_p;
	header->width = w;
	lump_p = (byte far *)&header->postofs[w];
//
// copy the lines to post[], then let SparsePost () write the data
//
	header->top = WORLDHEIGHT;
	header->bottom = 0;

	for (x=0 ; x<w ; x++)
	{
		memset (post,0,sizeof(post));

		header->postofs[x] = lump_p - (byte far *)header;

		for (y=0 ; y<h ; y++)
		{
			pixel = *(screen_p+y*SCREENWIDTH);
			post[start+y] = pixel;
			if (pixel != TRANSCOLOR)
			{
				adj = y+start;
				if (adj < header->top)
					header->top = adj;
				if (adj > header->bottom)
					header->bottom = adj;
			}

			*(screen_p+y*SCREENWIDTH) = 0;
		}
		SparsePost ();
		screen_p++;
	}
}

/*
=============================================================================

							HOLOGRAM

=============================================================================
*/


/*
==============
=
= GrabHolo
=
= filename HOLO
=
==============
*/

#define HOLOSIZE	64

void GrabHolo (void)
{
	int		x,y,z,shift;
	int		runcount,runs,count,lastz;
	byte	far	*screen_p,pixel;
	byte	far	*runs_p, far *count_p;
	holo_t	far *header;
	byte	far	*plane[HOLOSIZE];
	boolean	inrun;

//
// initialize a blank hologram
// A plane is an X/Y horizontal slice through the hologram
//
	for (z=0;z<HOLOSIZE;z++)
	{
		plane[z] = farmalloc(HOLOSIZE*HOLOSIZE);
		_fmemset (plane[z],255,HOLOSIZE*HOLOSIZE);
		if (!plane[z])
			MS_Quit ("Couln't allocate hologram buffer\n");
	}

//
// cut the north plane all the way through
//
	for (z=0;z<HOLOSIZE;z++)
		for (x=0;x<HOLOSIZE;x++)
		{
			screen_p = MK_FP(0xa000,(2*8+z)*SCREENWIDTH+2*8+x);
			pixel = *screen_p;
			*screen_p = pixel^255;
			if (!pixel)
				for (y=0;y<HOLOSIZE;y++)
					*(plane[z]+y*HOLOSIZE+x) = 0;
			*screen_p = pixel;
		}

//
// cut the west plane all the way through
//
	for (z=0;z<HOLOSIZE;z++)
		for (y=0;y<HOLOSIZE;y++)
		{
			screen_p = MK_FP(0xa000,(2*8+z)*SCREENWIDTH+11*8+y);
			pixel = *screen_p;
			*screen_p = pixel^255;
			if (!pixel)
				for (x=0;x<HOLOSIZE;x++)
					*(plane[z]+y*HOLOSIZE+x) = 0;
			*screen_p = pixel;
		}


//
// cut the top plane all the way through
//
	for (y=0;y<HOLOSIZE;y++)
		for (x=0;x<HOLOSIZE;x++)
		{
			screen_p = MK_FP(0xa000,(2*8+y)*SCREENWIDTH+20*8+x);
			pixel = *screen_p;
			*screen_p = pixel^255;
			if (!pixel)
				for (z=0;z<HOLOSIZE;z++)
					*(plane[z]+y*HOLOSIZE+x) = 0;
			*screen_p = pixel;
		}

//
// color the north plane
//
	for (z=0;z<HOLOSIZE;z++)
		for (x=0;x<HOLOSIZE;x++)
		{
			screen_p = MK_FP(0xa000,(2*8+z)*SCREENWIDTH+2*8+x);
			pixel = *screen_p;
			*screen_p = pixel^255;
			if (pixel)
				for (y=0;y<HOLOSIZE;y++)
					if (*(plane[z]+y*HOLOSIZE+x))
					{
						if (y==0 || !*(plane[z]+(y-1)*HOLOSIZE+x))
							*(plane[z]+y*HOLOSIZE+x) = pixel;
					}
			*screen_p = pixel;
		}

//
// color the south plane
//
	for (z=0;z<HOLOSIZE;z++)
		for (x=0;x<HOLOSIZE;x++)
		{
			screen_p = MK_FP(0xa000,(11*8+z)*SCREENWIDTH+2*8+x);
			pixel = *screen_p;
			*screen_p = pixel^255;
			if (pixel)
				for (y=HOLOSIZE-1;y>=0;y--)
					if (*(plane[z]+y*HOLOSIZE+x))
					{
						if (y==HOLOSIZE-1 || !*(plane[z]+(y+1)*HOLOSIZE+x))
							*(plane[z]+y*HOLOSIZE+x) = pixel;
					}
			*screen_p = pixel;
		}

//
// color the west plane
//
	for (z=0;z<HOLOSIZE;z++)
		for (y=0;y<HOLOSIZE;y++)
		{
			screen_p = MK_FP(0xa000,(11*8+z)*SCREENWIDTH+11*8+y);
			pixel = *screen_p;
			*screen_p = pixel^255;
			if (pixel)
				for (x=0;x<HOLOSIZE;x++)
					if (*(plane[z]+y*HOLOSIZE+x))
					{
						if (x==0 || !*(plane[z]+y*HOLOSIZE+x-1))
							*(plane[z]+y*HOLOSIZE+x) = pixel;
					}
			*screen_p = pixel;
		}


//
// color the east plane
//
	for (z=0;z<HOLOSIZE;z++)
		for (y=0;y<HOLOSIZE;y++)
		{
			screen_p = MK_FP(0xa000,(2*8+z)*SCREENWIDTH+11*8+y);
			pixel = *screen_p;
			*screen_p = pixel^255;
			if (pixel)
				for (x=HOLOSIZE-1;x>=0;x--)
					if (*(plane[z]+y*HOLOSIZE+x))
					{
						if (x==HOLOSIZE-1 || !*(plane[z]+y*HOLOSIZE+x+1))
							*(plane[z]+y*HOLOSIZE+x) = pixel;
					}
			*screen_p = pixel;
		}


//
// color the top plane
//
	for (y=0;y<HOLOSIZE;y++)
		for (x=0;x<HOLOSIZE;x++)
		{
			screen_p = MK_FP(0xa000,(2*8+y)*SCREENWIDTH+20*8+x);
			pixel = *screen_p;
			*screen_p = pixel^255;
			if (pixel)
				for (z=0;z<HOLOSIZE;z++)
					if (*(plane[z]+y*HOLOSIZE+x))
					{
						if (z==0 || !*(plane[z-1]+y*HOLOSIZE+x))
							*(plane[z]+y*HOLOSIZE+x) = pixel;
					}
			*screen_p = pixel;
		}

//
// color the bottom plane
//
	for (y=0;y<HOLOSIZE;y++)
		for (x=0;x<HOLOSIZE;x++)
		{
			screen_p = MK_FP(0xa000,(11*8+y)*SCREENWIDTH+20*8+x);
			pixel = *screen_p;
			*screen_p = pixel^255;
			if (pixel)
				for (z=HOLOSIZE-1;z>=0;z--)
					if (*(plane[z]+y*HOLOSIZE+x))
					{
						if (z==HOLOSIZE-1 || !*(plane[z+1]+y*HOLOSIZE+x))
							*(plane[z]+y*HOLOSIZE+x) = pixel;
					}
			*screen_p = pixel;
		}


//
// draw an orthographic representation
//
	screen_p = MK_FP(0xa000,0);
	for (y=0;y<HOLOSIZE*2;y++,screen_p += SCREENWIDTH)
		_fmemset ( screen_p,0,HOLOSIZE*2 );

	for (y=0;y<HOLOSIZE;y++)
	{
		shift = (HOLOSIZE-y)/2;
		screen_p = MK_FP(0xa000,y/2*SCREENWIDTH+shift);
		for (z=0;z<HOLOSIZE;z++)
			for (x=0;x<HOLOSIZE;x++)
			{
				pixel = *(plane[z]+y*HOLOSIZE+x);
				if (pixel && pixel != 255)
					*(screen_p+SCREENWIDTH*z+x) = pixel;
			}
	}


//
// write out post lists (top view shows where posts are)
//
	header = (holo_t far *)lump_p;
	_fmemset (header,0,sizeof(*header));
	lump_p = (byte far *)&header->postofs[4096];

	for (y=0;y<HOLOSIZE;y++)
		for (x=0;x<HOLOSIZE;x++)
		{
			screen_p = MK_FP(0xa000,(2*8+y)*SCREENWIDTH+20*8+x);
			pixel = *screen_p;
			*screen_p = pixel^255;
			if (!pixel)
				continue;

			// it is still possible to have an empt post if the
			// cut views are not consistant

			header->postofs[y*HOLOSIZE+x] = lump_p - (byte far *)header;
			runs_p = lump_p++;

			runs = 0;
			lastz = 0;
			inrun = false;

			for (z=0;z<HOLOSIZE;z++)
			{
				pixel = *(plane[z]+y*HOLOSIZE+x);
				if (inrun)
				{
					if (pixel != 0 && pixel != 255)
					{
						*lump_p++ = pixel;
						count++;
						continue;
					}
					*count_p = count;
					lastz = z+1;
					inrun = false;
				}
				else
				{
					if (pixel == 0 || pixel == 255)
						continue;

					*lump_p++ = (z-lastz)*2;	// height adjustment
					count = 1;
					count_p = lump_p;
					lump_p++;
					*lump_p++ = pixel;
					inrun = true;
					runs++;
				}
			}
			if (inrun)
				*count_p = count;

			if (!runs)	// inconsistant post
			{
				header->postofs[y*HOLOSIZE+x] = 0;
				*screen_p = 255;
				lump_p--;
				continue;
			}
			*runs_p = runs;

			*screen_p = pixel;
		}


	for (z=0;z<HOLOSIZE;z++)
		farfree (plane[z]);
}


