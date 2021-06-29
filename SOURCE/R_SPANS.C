// R_spans.c
#include "D_global.h"
#include "R_refdef.h"

/*                                                                                                                                              */
/*MAXZ must be 511 or less! (384 now)                                                                   */
/*                                                                                                                                              */
/*(pointz<<7)&0xfff00000                // 9 unit bits, 3 frac bits                             */
/*ceiling                                       // 1 bit                                                                        */
/*x1<<12                                        // 8 bits                                                                       */
/*span number                           // 12 bits                                                                      */
/*                                                                                                                                              */
/*a scaled object is just encoded like a span                                                   */
/*                                                                                                                                              */
/*                                                                                                                                              */

unsigned spantags[2][MAXSPANS];
unsigned *starttaglist_p, *endtaglist_p;        // set by SortSpans
span_t spans[MAXSPANS], *spans_p;
int numspans;

int spanx;
fixed_t pointz;
span_t *span_p;

//================================================================

int size1;
int size2;
unsigned *src1, *src2, *dest;

#ifdef NeXT

/*                                                                                                                                              */
/*==========                                                                                                                    */
/*=                                                                                                                                             */
/*= Merge                                                                                                                               */
/*=                                                                                                                                             */
/*= Merges src1/size1 and src2/size2 to dest                                                    */
/*=                                                                                                                                             */
/*==========                                                                                                                    */
/*                                                                                                                                              */

void Merge(void)
{
	//
	// merge two parts of the unsorted array to the sorted array
	//
	if (*src1<*src2)
		goto mergefrom2;

mergefrom1:
	*dest++=*src1++;
	if (!--size1)
		goto finishfrom2;
	if (*src1>*src2)
		goto mergefrom1;

mergefrom2:
	*dest++=*src2++;
	if (!--size2)
		goto finishfrom1;
	if (*src1>*src2)
		goto mergefrom1;
	goto mergefrom2;

finishfrom2:
	while (size2--)*dest++=*src2++;
	return;

finishfrom1:
	while (size1--)*dest++=*src1++;
}


#else
void Merge(void);                       // in assembly

#endif

/*                                                                                                                                              */
/*==================                                                                                                    */
/*=                                                                                                                                             */
/*= SortSpans                                                                                                                   */
/*=                                                                                                                                             */
/*= Sorts the <numspans> unsigned values in spantags[0]                                 */
/*= Sets starttaglist_p and endtaglist_p to the sorted list, which can b*/
/*=                                                                                                                                             */
/*==================                                                                                                    */
/*                                                                                                                                              */

void SortSpans(void)
{
	int size, start;
	int sort;
	unsigned *sorted, *unsorted, *temp;

	if (numspans<2) {
		starttaglist_p=&spantags[0][0];
		endtaglist_p=starttaglist_p+numspans;
		return;
	}
	size=1;
	sort=0;

	sorted=spantags[sort];
	unsorted=spantags[!sort];

	do {
		start=0;
		dest=unsorted;          // this will be incremented by the merge

		do {
			src1=sorted+start;
			size1=size;
			src2=src1+size;
			start+=size;
			size2=numspans-start;
			if (size2>size)
				size2=size;
			start+=size2;   // for next iteration

			Merge();
		} while (numspans-start>size);

		//
		// copy any remnants (0-size possible)
		//
		while (start!=numspans) {
			*dest++=spantags[sort][start];
			start++;
		}
		//
		// get ready to sort back to the other array
		//
		sort^=1;
		temp=sorted;
		sorted=unsorted;
		unsorted=temp;
		size<<=1;

#if 0
		{
			int i;
			for (i=0; i<numspans; i++)
				printf("%4i", (int)spantags[sort][i]);
			printf("\n\n");
		}
#endif
	} while (size<numspans);

	starttaglist_p=sorted;
	endtaglist_p=starttaglist_p+numspans;
}

//==================================================

/*                                                                                                                                              */
/*===================                                                                                                   */
/*=                                                                                                                                             */
/*= DrawDoorPost                                                                                                                */
/*=                                                                                                                                             */
/*===================                                                                                                   */
/*                                                                                                                                              */

void DrawDoorPost(void)
{
	fixed_t top, bottom;    // precise y coordinates for post
	int topy, bottomy;              // pixel y coordinates for post
	fixed_t fracadjust;             // the amount to prestep for the top pixel

	sp_source=span_p->picture;
    sp_colormap=zcolormap[pointz>>FRACBITS];
	sp_fracstep=FIXEDMUL(pointz, ISCALE);

	top=FIXEDDIV(span_p->y, sp_fracstep);
	topy=top>>FRACBITS;
	fracadjust=top&(FRACUNIT-1);
	sp_frac=FIXEDMUL(fracadjust, sp_fracstep);
	topy=CENTERY-topy;
	if (topy<0) {
		sp_frac-=topy *sp_fracstep;
		topy=0;
	}
	bottom=FIXEDDIV(span_p->yh, sp_fracstep)+FRACUNIT;
	bottomy=bottom>=(CENTERY<<FRACBITS)?windowHeight-1:CENTERY+(bottom>>FRACBITS);

	if ((bottomy<=0)||(topy>=windowHeight))
		return;

	sp_count=bottomy-topy+1;
	sp_dest=viewylookup[bottomy]+spanx;

	if (span_p->spantype==sp_maskeddoor) ScaleMaskedPost();
	else ScalePost();
}

//==========================================================

/*                                                                                                                                              */
/*===================                                                                                                   */
/*=                                                                                                                                             */
/*= DrawSprite                                                                                                                  */
/*=                                                                                                                                             */
/*===================                                                                                                   */
/*                                                                                                                                              */

void DrawSprite(void)
{
	int x;
	fixed_t leftx;
	fixed_t scale;
	fixed_t xfrac, fracstep;
	fixed_t shapebottom, topheight, bottomheight;
	int post;
	int topy, bottomy;
	scalepic_t *pic;
	byte *collumn;

    sp_colormap=zcolormap[pointz>>FRACBITS];
	pic=(scalepic_t *)span_p->picture;
	shapebottom=span_p->y;
	//
	// project the x and height
	//
	scale=FIXEDDIV(SCALE, pointz);
	fracstep=FIXEDMUL(pointz, ISCALE);
	sp_fracstep=fracstep;
	leftx=span_p->x2;
	leftx-=pic->leftoffset<<FRACBITS;

	x=CENTERX+(FIXEDMUL(leftx, scale)>>FRACBITS);

	//
	// step through the shape, drawing posts where visible
	//
	xfrac=0;
	if (x<0) {
		xfrac-=fracstep *x;
		x=0;
	}
	for (; x<windowWidth; x++) {
		post=xfrac>>FRACBITS;
		xfrac+=fracstep;

		if (post>=pic->width)
			return;                 // entire shape is too far away
		if (pointz>wallz[x])
			continue;               // this post is obscured by a closer wall

// Temporary Mod by Ray and Ben to attempt a fix for empty sprite posts
// 1992-12-02 11:58
// If the offset of the columns is zero then there is no data for the post
		if (pic->collumnofs[post]==0)
			continue;
		collumn=(byte *)pic+pic->collumnofs[post];

		topheight=shapebottom+(*collumn<<FRACBITS);
		bottomheight=shapebottom+(*(collumn+1)<<FRACBITS);
		collumn+=2;
		//
		// scale a post
		//
		topy=CENTERY-(FIXEDMUL(topheight, scale)>>FRACBITS);
		if (topy<0) {
			sp_frac=-topy *sp_fracstep;
			topy=0;
		}
		else
			sp_frac=0;

		bottomy=CENTERY-(FIXEDMUL(bottomheight, scale)>>FRACBITS);
		if (bottomy>windowHeight-1) bottomy=windowHeight-1;

		if ((bottomy<0)||(topy>=windowHeight)) continue;

		sp_count=bottomy-topy+1;
		sp_dest=viewylookup[bottomy]+x;
		sp_source=collumn;

		ScaleMaskedPost();
	}
}

//==========================================================

/*                                                                                                                                              */
/*==============                                                                                                                */
/*=                                                                                                                                             */
/*= DrawSpans                                                                                                                   */
/*=                                                                                                                                             */
/*= Spans farther than MAXZ away should NOT have been entered into the l*/
/*                                                                                                                                              */
/*==============                                                                                                                */
/*                                                                                                                                              */

void DrawSpans(void)
{
	unsigned *spantag_p, tag;
	int spannum;
	int x2;
	fixed_t lastz;                  // the pointz for which xystep is valid
	fixed_t length;
	fixed_t zerocosine, zerosine;
	fixed_t zeroxfrac, zeroyfrac;
	fixed_t xf2, yf2;               // endpoint texture for sloping spans
	int angle;

	//
	// set up for drawing
	//
	SortSpans();
	spantag_p=starttaglist_p;

	angle=viewfineangle+pixelangle[0];
	angle&=TANANGLES *4-1;
	zerocosine=cosines[angle];
	zerosine=sines[angle];

	//
	// draw from back to front
	//
	x2=-1;
	lastz=-1;

	//
	// draw everything else
	//
	while (spantag_p!=endtaglist_p) {
		tag=*spantag_p++;

		pointz=(tag&ZMASK)>>ZTOFRAC;
		spannum=tag&SPANMASK;
		span_p=&spans[spannum];
		spanx=tag&XMASK;
        spanx=(spanx>>XSHIFT)^0x1ff;     // invert back to regular x

		if (span_p->spantype==sp_flat) {        // its just a floor/ ceiling
			//===============
			//
			// floor / ceiling span
			//
			//===============
			if (pointz!=lastz) {
				lastz=pointz;
				mr_xstep=FIXEDMUL(pointz, xscale);
				mr_ystep=FIXEDMUL(pointz, yscale);
                mr_colormap=zcolormap[pointz>>FRACBITS];
				//
				// calculate starting texture point
				//
				length=FIXEDDIV(pointz, pixelcosine[0]);
				zeroxfrac=mr_xfrac=viewx+FIXEDMUL(length, zerocosine);
				zeroyfrac=mr_yfrac=viewy-FIXEDMUL(length, zerosine);
				x2=0;
			}
			if (spanx!=x2) {
				mr_xfrac=zeroxfrac+mr_xstep *spanx;
				mr_yfrac=zeroyfrac+mr_ystep *spanx;
			}
			mr_dest=viewylookup[span_p->y]+spanx;
			mr_picture=span_p->picture;
			x2=span_p->x2;
			mr_count=x2-spanx;

			MapRow();
			continue;
		}
		if (span_p->spantype==sp_slope) {       // its just a floor/ ceiling
			//===============
			//
			// sloping floor / ceiling span
			//
			//===============
			lastz=-1;               // we are going to get out of order here, so

            mr_colormap=zcolormap[pointz>>FRACBITS];
			x2=span_p->x2;
			mr_dest=viewylookup[span_p->y]+spanx;
			mr_picture=span_p->picture;
			mr_count=x2-spanx;
			//
			// calculate starting texture point
			//
			length=FIXEDDIV(pointz, pixelcosine[spanx]);
			angle=viewfineangle+pixelangle[spanx];
			angle&=TANANGLES *4-1;
			mr_xfrac=viewx+FIXEDMUL(length, cosines[angle]);
			mr_yfrac=viewy-FIXEDMUL(length, sines[angle]);
			//
			// calculate ending texture point
			//      (yh is pointz2 for ending point)
			length=FIXEDDIV(span_p->yh, pixelcosine[x2]);
			angle=viewfineangle+pixelangle[x2];
			angle&=TANANGLES *4-1;
			xf2=viewx+FIXEDMUL(length, cosines[angle]);
			yf2=viewy-FIXEDMUL(length, sines[angle]);

			mr_xstep=(xf2-mr_xfrac)/mr_count;
			mr_ystep=(yf2-mr_yfrac)/mr_count;
			MapRow();
			continue;
		}
		//================
		//
		// other spans
		//
		//================
		if (span_p->spantype==sp_shape) {
			DrawSprite();
			continue;
		}
		if (span_p->spantype==sp_door||span_p->spantype==sp_maskeddoor) {
			DrawDoorPost();
			continue;
		}
	}
}
