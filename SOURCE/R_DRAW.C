// R_draw.c

#include <math.h>
#include "D_global.h"
#include "R_refdef.h"

/*																		*/
/*====================													*/
/*=																		*/
/*= ScalePost															*/
/*=																		*/
/*= Primitive scaling operation, coordinates must be clipped			*/
/*=																		*/
/*= Imp1emented on the IBM by jumping into an unwound loop				*/
/*= The destination address is the BOTTOM pixel, but the texture start i*/
/*= pixel (bottom+1-count)												*/
/*=																		*/
/*====================													*/
/*																		*/

byte *sp_dest;				// the bottom most pixel to be drawn (in vie
byte *sp_source;			// the first pixel in the vertical post (may
byte *sp_colormap;			// pointer to a 256 byte color number to pal
int sp_frac;				// fixed point location past sp_source
int sp_fracstep;			// fixed point step value
int sp_count;				// the number of pixels to draw

void ScalePost(void)
{
	//return;	// debug
#ifdef VALIDATE
	if (sp_count<1)
		MS_Error("sp_count < 1");

	if (sp_dest>(viewbuffer+windowWidth*windowHeight))
		MS_Error("sp_dest > viewbuffer+windowWidth*windowHeight");
#endif
	sp_dest -= windowWidth*(sp_count-1);	// go to the top

#ifdef VALIDATE
	if (sp_dest<viewbuffer)
		MS_Error("sp_dest > viewbuffer");
#endif
	while (sp_count--) {
		*sp_dest=sp_colormap[sp_source[sp_frac>>FRACBITS]];
		sp_dest += windowWidth;
		sp_frac += sp_fracstep;
	}
}

/*																		*/
/*====================													*/
/*=																		*/
/*= ScaleMaskedPost														*/
/*=																		*/
/*= Primitive scaling operation, coordinates must be clipped			*/
/*=																		*/
/*= Imp1emented on the IBM by jumping into an unwound loop				*/
/*= The destination address is the BOTTOM pixel, but the texture start i*/
/*= pixel (bottom+1-count)												*/
/*=																		*/
/*====================													*/
/*																		*/

void ScaleMaskedPost(void)
{
	pixel_t color;

#ifdef VALIDATE
	if (sp_count<1)
		MS_Error("sp_count < 1");

	if (sp_dest>(viewbuffer+windowWidth*windowHeight)
		MS_Error("sp_dest > viewbuffer+windowWidth*windowHeight");
#endif
	sp_dest -= windowWidth*(sp_count-1);	// go to the top

#ifdef VALIDATE
	if (sp_dest<viewbuffer)
		MS_Error("sp_dest > viewbuffer");
#endif
	while (sp_count--) {
		color=sp_source[sp_frac>>FRACBITS];
		if (color)
			*sp_dest=sp_colormap[color];
		sp_dest += windowWidth;
		sp_frac += sp_fracstep;
	}
}

/*																		*/
/*====================													*/
/*=																		*/
/*= MapRow																*/
/*=																		*/
/*= Primitive scaling operation, coordinates must be clipped			*/
/*=																		*/
/*= The destination address and texture spot are for the				*/
/*= leftmost (first) pixel												*/
/*=																		*/
/*====================													*/
/*																		*/

byte *mr_dest;				// the left most pixel to be drawn (in viewb
byte *mr_picture;			// pointer to a raw 64*64 pixel picture
byte *mr_colormap;			// pointer to a 256 byte color number to pal
int mr_xfrac;				// starting texture coordinate
int mr_yfrac;				// starting texture coordinate
int mr_xstep;				// fixed point step value
int mr_ystep;				// fixed point step value
int mr_count;				// the number of pixels to draw

void MapRow(void)
{
	int spot;
	//return;	// debug

#ifdef VALIDATE
	if (mr_count<1)
		MS_Error("mr_count < 1");

	if (!mr_colormap)
		return;
	if ((mr_dest<viewbuffer)||(mr_dest+mr_count>viewbuffer+windowSize))
		MS_Error("bad destination for MapRow");
#endif
	while (mr_count--) {
		spot=((mr_yfrac>>(FRACBITS-6))&(63*64))+((mr_xfrac>>FRACBITS)&63);
		*mr_dest++=mr_colormap[mr_picture[spot]];
		mr_xfrac+=mr_xstep;
		mr_yfrac+=mr_ystep;
	}
}
