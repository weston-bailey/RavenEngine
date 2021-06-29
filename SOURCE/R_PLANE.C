// R_plane.c
#include <math.h>
#include <string.h>
#include "D_global.h"
#include "d_disk.h"
#include "R_refdef.h"
int backvertex;                         // the farthest vertex in a tile, whcih is t

int ceilingbit;                         // set to CEILINGBIT when on ceiling, else 0

int mr_y, mr_x1, mr_x2;         // used by mapplane to calculate texture end
fixed_t mr_deltaheight;         //

fixed_t mapcache_height[MAX_VIEW_HEIGHT];
fixed_t mapcache_pointz[MAX_VIEW_HEIGHT];

//
// vertexes for drawable polygon
//
int numvertex;
int vertexy[10];
int vertexx[10];

//
// vertexes in need of Z clipping
//
clippoint_t vertexpt[10];

//
// coefficients of the plane equation for sloping polygons
//
fixed_t planeA, planeB, planeC, planeD;

#define COPYFLOOR(s,d)  \
vertexpt[d].tx = vertex[s]->tx; \
vertexpt[d].ty = vertex[s]->floorheight;        \
vertexpt[d].tz = vertex[s]->tz; \
vertexpt[d].px = vertex[s]->px; \
vertexpt[d].py = vertex[s]->floory;

#define COPYCEILING(s,d)        \
vertexpt[d].tx = vertex[s]->tx; \
vertexpt[d].ty = vertex[s]->ceilingheight;      \
vertexpt[d].tz = vertex[s]->tz; \
vertexpt[d].px = vertex[s]->px; \
vertexpt[d].py = vertex[s]->ceilingy;


//============================================================

/*                                                                                                                                              */
/*===============                                                                                                               */
/*=                                                                                                                                             */
/*= ClearMapCache                                                                                                               */
/*=                                                                                                                                             */
/*= Invalidates any cached calculations                                                                 */
/*=                                                                                                                                             */
/*===============                                                                                                               */
/*                                                                                                                                              */

void ClearMapCache(void)
{
	memset(mapcache_height, 0xf0, sizeof(mapcache_height));
}

//=========================================================

/*                                                                                                                                              */
/*==================                                                                                                    */
/*=                                                                                                                                             */
/*= FlatSpan                                                                                                                    */
/*=                                                                                                                                             */
/*= used for flat floors and ceilings, coordinates must be pre clipped  */
/*=                                                                                                                                             */
/*= mr_deltaheight is planeheight - viewheight, with height values incre*/
/*=                                                                                                                                             */
/*= mr_picture and mr_deltaheight are set once per polygon                              */
/*=                                                                                                                                     */
/*==================                                                                                                    */
/*                                                                                                                                              */

void FlatSpan(void)
{
	fixed_t pointz;                 // row's distance to view plane
	span_t *span_p;
	unsigned span;

#ifdef VALIDATE
	if (numspans==MAXSPANS)
		MS_Error("MAXSPANS exceeded");
	if ((mr_x1<0)||(mr_x2>windowWidth)||(mr_x1>=mr_x2)||(mr_y<0)||(mr_y>=windowWidth))
		MS_Error("Bad MapPlane coordinates");
#endif

	//
	// use cached pointz if valid
	//
	if (mapcache_height[mr_y]==mr_deltaheight)
		pointz=mapcache_pointz[mr_y];
	else {
		mapcache_height[mr_y]=mr_deltaheight;
		pointz=mapcache_pointz[mr_y]=FIXEDDIV(mr_deltaheight, yslope[mr_y]);
	}
	if (pointz>MAXZ)
		return;
	//
	// post the span in the draw list
	//
	span=(pointz<<ZTOFRAC)&ZMASK;
	span|=ceilingbit;
	span|=(mr_x1^0x1ff)<<XSHIFT;     // invert x1 so they are sorted in ascen
	// while Z is sorted in decending order
	span|=numspans;
	spantags[0][numspans]=span;

	span_p=&spans[numspans];
	span_p->spantype=sp_flat;
	span_p->picture=mr_picture;
	span_p->x2=mr_x2;
	span_p->y=mr_y;

	numspans++;
}

//=========================================================

/*                                                                                                                                              */
/*==================                                                                                                    */
/*=                                                                                                                                             */
/*= SlopeSpan                                                                                                                   */
/*=                                                                                                                                             */
/*= used for sloping floors and ceilings                                                                */
/*=                                                                                                                                             */
/*= planeA, planeB, planeC, planeD must be precalculated                                */
/*=                                                                                                                                             */
/*= mr_picture is set once per polygon                                                                  */
/*=                                                                                                                                     */
/*==================                                                                                                    */
/*                                                                                                                                              */

void SlopeSpan(void)
{
	fixed_t pointz, pointz2;        // row's distance to view plane
	fixed_t partial, denom;
	span_t *span_p;
	unsigned span;

#ifdef VALIDATE
	if (numspans==MAXSPANS)
		MS_Error("MAXSPANS exceeded");
	if ((mr_x1<0)||(mr_x2>windowWidth)||(mr_x1>=mr_x2)||(mr_y<0)||(mr_y>=windowWidth))
		MS_Error("Bad MapPlane coordinates");
#endif
	//
	// calculate the Z values for each end of the span
	//
	partial=FIXEDMUL(planeB, yslope[mr_y])+planeC;
	denom=FIXEDMUL(planeA, xslope[mr_x1])+partial;
	pointz=FIXEDDIV(planeD, denom);
	denom=FIXEDMUL(planeA, xslope[mr_x2])+partial;
	pointz2=FIXEDDIV(planeD, denom);

	if (pointz>MAXZ||pointz2>MAXZ)
		return;

	//
	// post the span in the draw list
	//
	span=(pointz<<ZTOFRAC)&ZMASK;
	span|=ceilingbit;
    span|=(mr_x1^0x1ff)<<XSHIFT;     // invert x1 so they are sorted in ascen
	// while Z is sorted in decending order
	span|=numspans;
	spantags[0][numspans]=span;

	span_p=&spans[numspans];
	span_p->spantype=sp_slope;
	span_p->picture=mr_picture;
	span_p->x2=mr_x2;
	span_p->y=mr_y;
	span_p->yh=pointz2;

	numspans++;
}

//=========================================================

/*                                                                                                                                              */
/*==================                                                                                                    */
/*=                                                                                                                                             */
/*= RenderPolygon                                                                                                               */
/*=                                                                                                                                             */
/*= Vertex list must be precliped, convex, and in clockwise order               */
/*= Backfaces (not in clockwise order) generate no pixels                               */
/*=                                                                                                                                             */
/*= The polygon is divided into trapezoids (from 1 to numvertex-1 can be*/
/*= which have a constant slope on both sides                                                   */
/*=                                                                                                                                             */
/*= mr_x1                       screen coordinates of the span to draw, used by map     */
/*= mr_x2                       plane to calculate textures at the endpoints            */
/*= mr_y                        along with mr_deltaheight                                                       */
/*=                                                                                                                                             */
/*= mr_dest             pointer inside viewbuffer where span starts                             */
/*= mr_count            length of span to draw (mr_x2 - mr_x1)                          */
/*=                                                                                                                                             */
/*= spanfunction is a pointer to a function that will handle determining*/
/*= in the calculated span (FlatSpan or SlopeSpan)                                              */
/*=                                                                                                                                             */
/*==================                                                                                                    */
/*                                                                                                                                              */

void RenderPolygon(void (*spanfunction)(void))
{
	int stopy;
	fixed_t leftfrac, rightfrac;
	fixed_t leftstep, rightstep;
	int leftvertex, rightvertex;
	int deltax, deltay;
	int oldx;

	//
	// find topmost vertex
	//
	rightvertex=0;                  // topmost so far
	for (leftvertex=1; leftvertex<numvertex; leftvertex++)
		if (vertexy[leftvertex]<vertexy[rightvertex])
			rightvertex=leftvertex;

		//
		// ride down the left and right edges
		//
	leftvertex=rightvertex;
	mr_y=vertexy[rightvertex];

	if (mr_y>=windowHeight)
		return;                         // totally off bottom

	do {
		if (mr_y==vertexy[rightvertex]) {
			skiprightvertex : oldx=vertexx[rightvertex];
			if (++rightvertex==numvertex)
				rightvertex=0;
			deltay=vertexy[rightvertex]-mr_y;
			if (!deltay) {
				if (leftvertex==rightvertex)
					return; // the last edge is exactly horizontal
				goto skiprightvertex;
			}
			deltax=vertexx[rightvertex]-oldx;
			rightfrac=(oldx<<FRACBITS);     // fix roundoff
			rightstep=(deltax<<FRACBITS)/deltay;
		}
		if (mr_y==vertexy[leftvertex]) {
			skipleftvertex : oldx=vertexx[leftvertex];
			if (--leftvertex==-1)
				leftvertex=numvertex-1;
			deltay=vertexy[leftvertex]-mr_y;
			if (!deltay)
				goto skipleftvertex;
			deltax=vertexx[leftvertex]-oldx;
			leftfrac=(oldx<<FRACBITS);      // fix roundoff
			leftstep=(deltax<<FRACBITS)/deltay;
		}
		if (vertexy[rightvertex]<vertexy[leftvertex])
			stopy=vertexy[rightvertex];
		else
			stopy=vertexy[leftvertex];

		//
		// draw a trapezoid
		//
		if (stopy<=0) {
			leftfrac+=leftstep *(stopy-mr_y);
			rightfrac+=rightstep *(stopy-mr_y);
			mr_y=stopy;
			continue;
		}
		if (mr_y<0) {
			leftfrac-=leftstep *mr_y;
			rightfrac-=rightstep *mr_y;
			mr_y=0;
		}
		if (stopy>windowHeight)
			stopy=windowHeight;

		for (; mr_y<stopy; mr_y++) {
			mr_x1=leftfrac>>FRACBITS;
			mr_x2=rightfrac>>FRACBITS;
			if (mr_x1<xclipl)
				mr_x1=xclipl;
			if (mr_x2>xcliph)
				mr_x2=xcliph;

			if (mr_x1<xcliph&&mr_x2>mr_x1)
				spanfunction(); // different functions for flat and slop

			leftfrac+=leftstep;
			rightfrac+=rightstep;
		}
	} while ((rightvertex!=leftvertex)&&(mr_y!=windowHeight));

}


//============================================================

/*                                                                                                                                              */
/*==================                                                                                                    */
/*=                                                                                                                                             */
/*= CalcPlaneEquation                                                                                                   */
/*=                                                                                                                                             */
/*= Calculates planeA, planeB, planeC, planeD                                                   */
/*= planeD is actually -planeD                                                                                  */
/*=                                                                                                                                             */
/*= for vertexpt[0-2]                                                                                                   */
/*=                                                                                                                                             */
/*==================                                                                                                    */
/*                                                                                                                                              */

void CalcPlaneEquation(void)
{
	fixed_t x1, y1, z1;
	fixed_t x2, y2, z2;
	fixed_t check1, check2;

	//
	// calculate two vectors going away from the middle vertex
	//
	x1=vertexpt[0].tx-vertexpt[1].tx;
	y1=vertexpt[0].ty-vertexpt[1].ty;
	z1=vertexpt[0].tz-vertexpt[1].tz;

	x2=vertexpt[2].tx-vertexpt[1].tx;
	y2=vertexpt[2].ty-vertexpt[1].ty;
	z2=vertexpt[2].tz-vertexpt[1].tz;

	//
	// the A, B, C coefficients are the cross product of v1 and v2
	// shift over to save some precision bits
	planeA=(FIXEDMUL(y1, z2)-FIXEDMUL(z1, y2))>>8;
	planeB=(FIXEDMUL(z1, x2)-FIXEDMUL(x1, z2))>>8;
	planeC=(FIXEDMUL(x1, y2)-FIXEDMUL(y1, x2))>>8;

	//
	// calculate D based on A,B,C and one of the vertex points
	//
	planeD=FIXEDMUL(planeA, vertexpt[0].tx)
	+FIXEDMUL(planeB, vertexpt[0].ty)
	+FIXEDMUL(planeC, vertexpt[0].tz);

	check1=FIXEDMUL(planeA, vertexpt[1].tx)
	+FIXEDMUL(planeB, vertexpt[1].ty)
	+FIXEDMUL(planeC, vertexpt[1].tz);
	check2=FIXEDMUL(planeA, vertexpt[2].tx)
	+FIXEDMUL(planeB, vertexpt[2].ty)
	+FIXEDMUL(planeC, vertexpt[2].tz);
}

//============================================================

/*                                                                                                                                              */
/*==================                                                                                                    */
/*=                                                                                                                                             */
/*= ZClipPolygon                                                                                                                */
/*=                                                                                                                                             */
/*= Returns true if the polygon should be rendered                                              */
/*=                                                                                                                                             */
/*==================                                                                                                    */
/*                                                                                                                                              */

boolean ZClipPolygon(int numvertexpts, fixed_t minz)
{
	int v;
	fixed_t scale;
	fixed_t frac, cliptx, clipty;
	clippoint_t *p1, *p2;

	numvertex=0;

	if (minz<MINZ)                  // less than this will cause problems
		minz=MINZ;

	p1=&vertexpt[0];

	for (v=1; v<=numvertexpts; v++) {
		p2=p1;                          // p2 is old point
		if (v!=numvertexpts)
			p1=&vertexpt[v];        // p1 is new point
		else
			p1=&vertexpt[0];

		if ((p1->tz<minz)^(p2->tz<minz)) {
			scale=FIXEDDIV(SCALE, minz);
			frac=FIXEDDIV((p1->tz-minz), (p1->tz-p2->tz));

			cliptx=p1->tx+FIXEDMUL((p2->tx-p1->tx), frac);
			clipty=p1->ty+FIXEDMUL((p2->ty-p1->ty), frac);

			vertexx[numvertex]=CENTERX+(FIXEDMUL(cliptx, scale)>>FRACBITS);
			vertexy[numvertex]=CENTERY-(FIXEDMUL(clipty, scale)>>FRACBITS);
			numvertex++;
		}
		if (p1->tz>=minz) {
			vertexx[numvertex]=p1->px;
			vertexy[numvertex]=p1->py;
			numvertex++;
		}
	}
	if (!numvertex)
		return false;

	return true;
}

//================================================================

/*                                                              */
/*==================*/
/*=                 */
/*= RenderTileEnds  */
/*=                 */
/*= Draw the floor  */
/*= and ceiling for */
/*= a tile          */                                                               
/*==================*/
/*                  */

void RenderTileEnds(void)
{
	int flatpic;
	int flags, polytype;

	xcliph++;                               // debug: handle this globally
	flags=mapflags[mapspot];

	//
	// draw the floor
	//
	flatpic=floorpic[mapspot];
#ifdef VALIDATE
	if (flatpic>=numflats)
		MS_Error("RenderTileEnds: Invalid floorpic at (%i,%i)", tilex, tiley);
#endif
	flatpic=flattranslation[flatpic];
#ifdef VALIDATE
	if (flatpic>=numflats)
		MS_Error("RenderTileEnds: Invalid translated floor");
#endif
	mr_picture=lumpmain[flatlump+flatpic];
	ceilingbit=0;

	polytype=(flags&FL_FLOOR)>>FLS_FLOOR;
	switch (polytype) {
		case POLY_FLAT:
			mr_deltaheight=vertex[0]->floorheight;
			if (mr_deltaheight<0) {
				COPYFLOOR(0, 0);
				COPYFLOOR(1, 1);
				COPYFLOOR(2, 2);
				COPYFLOOR(3, 3);
				if (ZClipPolygon(4, -mr_deltaheight))
					RenderPolygon(FlatSpan);
			}
			break;

		case POLY_SLOPE:
			COPYFLOOR(0, 0);
			COPYFLOOR(1, 1);
			COPYFLOOR(2, 2);
			COPYFLOOR(3, 3);
			CalcPlaneEquation();
			if (ZClipPolygon(4, MINZ))
				RenderPolygon(SlopeSpan);
			break;

		case POLY_ULTOLR:
			COPYFLOOR(0, 0);
			COPYFLOOR(1, 1);
			COPYFLOOR(2, 2);
			CalcPlaneEquation();
			if (ZClipPolygon(3, MINZ))
				RenderPolygon(SlopeSpan);

			COPYFLOOR(2, 0);
			COPYFLOOR(3, 1);
			COPYFLOOR(0, 2);
			CalcPlaneEquation();
			if (ZClipPolygon(3, MINZ))
				RenderPolygon(SlopeSpan);
			break;

		case POLY_URTOLL:
			COPYFLOOR(0, 0);
			COPYFLOOR(1, 1);
			COPYFLOOR(3, 2);
			CalcPlaneEquation();
			if (ZClipPolygon(3, MINZ))
				RenderPolygon(SlopeSpan);

			COPYFLOOR(1, 0);
			COPYFLOOR(2, 1);
			COPYFLOOR(3, 2);
			CalcPlaneEquation();
			if (ZClipPolygon(3, MINZ))
				RenderPolygon(SlopeSpan);
			break;
	}
	//
	// draw the ceiling
	//

    //return;  // remove to show ceilings

	flatpic=ceilingpic[mapspot];
#ifdef VALIDATE
	if (flatpic>=numflats)
		MS_Error("RenderTileEnds: Invalid ceilingpic at (%i,%i)", tilex,
				 tiley);
#endif
	flatpic=flattranslation[flatpic];
#ifdef VALIDATE
	if (flatpic>=numflats)
		MS_Error("RenderTileEnds: Invalid ceiling translation for %i",
				 flatlump);
#endif
	mr_picture=lumpmain[flatlump+flatpic];

	ceilingbit=CEILINGBIT;

	polytype=(flags&FL_CEILING)>>FLS_CEILING;
	switch (polytype) {
		case POLY_FLAT:
			mr_deltaheight=vertex[0]->ceilingheight;
			if (mr_deltaheight>0) {
				COPYCEILING(3, 0);
				COPYCEILING(2, 1);
				COPYCEILING(1, 2);
				COPYCEILING(0, 3);
				if (ZClipPolygon(4, mr_deltaheight))
					RenderPolygon(FlatSpan);
			}
			break;

		case POLY_SLOPE:
			COPYCEILING(3, 0);
			COPYCEILING(2, 1);
			COPYCEILING(1, 2);
			COPYCEILING(0, 3);
			CalcPlaneEquation();
			if (ZClipPolygon(4, MINZ))
				RenderPolygon(SlopeSpan);
			break;

		case POLY_ULTOLR:
			COPYCEILING(3, 0);
			COPYCEILING(2, 1);
			COPYCEILING(1, 2);
			CalcPlaneEquation();
			if (ZClipPolygon(3, MINZ))
				RenderPolygon(SlopeSpan);

			COPYCEILING(3, 0);
			COPYCEILING(1, 1);
			COPYCEILING(0, 2);
			CalcPlaneEquation();
			if (ZClipPolygon(3, MINZ))
				RenderPolygon(SlopeSpan);
			break;

		case POLY_URTOLL:
			COPYCEILING(3, 0);
			COPYCEILING(2, 1);
			COPYCEILING(0, 2);
			CalcPlaneEquation();
			if (ZClipPolygon(3, MINZ))
				RenderPolygon(SlopeSpan);

			COPYCEILING(2, 0);
			COPYCEILING(1, 1);
			COPYCEILING(0, 2);
			CalcPlaneEquation();
			if (ZClipPolygon(3, MINZ))
				RenderPolygon(SlopeSpan);
			break;
	}
}

//===============================================================

/*                                                                                                                                              */
/*====================                                                                                                  */
/*=                                                                                                                                             */
/*= FindBackVertex                                                                                                              */
/*=                                                                                                                                             */
/*====================                                                                                                  */
/*                                                                                                                                              */

void FindBackVertex(void)
{
	int v;
	fixed_t greatestz;

	//
	// transform the view tile and find the vertex with the greatest Z
	//
	vertex[0]=TransformVertex(viewtilex, viewtiley);
	vertex[1]=TransformVertex(viewtilex+1, viewtiley);
	vertex[2]=TransformVertex(viewtilex+1, viewtiley+1);
	vertex[3]=TransformVertex(viewtilex, viewtiley+1);

	backvertex=0;
	greatestz=vertex[0]->tz;

	for (v=1; v<4; v++)
		if (vertex[v]->tz>greatestz) {
			backvertex=v;
			greatestz=vertex[v]->tz;
		}
}
