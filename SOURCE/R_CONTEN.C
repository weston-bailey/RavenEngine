// R_conten.c
#include "D_global.h"
#include "d_disk.h"
#include "R_refdef.h"
#include "d_misc.h"
#include "d_ints.h"

#define MINDIST         (FRACUNIT*4)
#define PLAYERSIZE      MINDIST // almost a half tile
#define FRACTILESHIFT   (FRACBITS+TILESHIFT)

scaleobj_t firstscaleobj, lastscaleobj; // just placeholders for links
scaleobj_t scaleobjlist[MAXSPRITES], *freescaleobj_p;

doorobj_t doorlist[MAXDOORS];
int numdoors;

int doorxl, doorxh;

void DrawDoor(void);

//============================================================

/*                                                                                                                                              */
/*==================                                                                                                    */
/*=                                                                                                                                             */
/*= TransformPoint                                                                                                              */
/*=                                                                                                                                             */
/*= Returns a vertex pointer, but the only fields filled in are                 */
/*= tx, tz, and px (if tz >= MINZ)                                                                              */
/*=                                                                                                                                             */
/*==================                                                                                                    */
/*                                                                                                                                              */

vertex_t *TransformPoint(fixed_t x, fixed_t y)
{
	fixed_t trx, try;
	fixed_t gxt, gyt;
	fixed_t scale;
	vertex_t *point;

	point=vertexlist_p++;
#ifdef VALIDATE
	if (point>=&vertexlist[MAXVISVERTEXES])
		MS_Error("TransformPoint: Vertexlist overflow");
#endif
	trx=x-viewx;
	try=y-viewy;

	gxt=FIXEDMUL(trx, viewsin);
	gyt=FIXEDMUL(try, viewcos);
	point->tx=gyt+gxt;

	gxt=FIXEDMUL(trx, viewcos);
	gyt=FIXEDMUL(try, viewsin);
	point->tz=gxt-gyt;

	if (point->tz>=MINZ) {
		scale=FIXEDDIV(SCALE, point->tz);
		point->px=CENTERX+(FIXEDMUL(point->tx, scale)>>FRACBITS);
	}
	return point;
}

/*                                                                                                                                              */
/*==================                                                                                                    */
/*=                                                                                                                                             */
/*= ClipDoor                                                                                                                    */
/*=                                                                                                                                             */
/*= Sets p1->px and p2->px correctly for Z values < MINZ                                */
/*=                                                                                                                                             */
/*= Returns false if entire door is too close or far away                               */
/*=                                                                                                                                             */
/*==================                                                                                                    */
/*                                                                                                                                              */

boolean ClipDoor(void)
{
	fixed_t frac, clip;

	if (p1->tz>MAXZ&&p2->tz>MAXZ)
		return false;           // entire face is too far away

	if (p1->tz<0&&p2->tz<0)
		return false;           // totally behind the projection plane

	if (p1->tz<MINZ) {
		if (p1->tz==0)
			clip=p1->tx;
		else {
			frac=FIXEDDIV(p2->tz, (p2->tz-p1->tz));
			clip=p2->tx+FIXEDMUL((p1->tx-p2->tx), frac);
		}
		p1->px=clip<0?0:windowWidth;
	}
	else if (p2->tz<MINZ) {
		if (p2->tz==0)
			clip=p2->tx;
		else {
			frac=FIXEDDIV(p1->tz, (p1->tz-p2->tz));
			clip=p1->tx+FIXEDMUL((p2->tx-p1->tx), frac);
		}
		p2->px=clip<0?0:windowWidth;
	}
	return true;
}

/*                                                                                                                                              */
/*===============                                                                                                               */
/*=                                                                                                                                             */
/*= RenderDoor                                                                                                                  */
/*=                                                                                                                                             */
/*= Posts one pixel wide span events for each visible post of the door a*/
/*= tilex / tiley / xclipl / xcliph                                                                     */
/*=                                                                                                                                             */
/*= sets doorxl, doorxh based on the position of the door.  One of the t*/
/*= in the tile bounds, the other will be off the edge of the view.  The*/
/*= restrict the flowing into other tiles bounds.                                               */
/*=                                                                                                                                             */
/*===============                                                                                                               */
/*                                                                                                                                              */

void RenderDoor(fixed_t playerx, fixed_t playery)
{
	doorobj_t *door_p, *last_p;
	fixed_t tx, ty;
	byte **postindex;               // start of the 64 entry texture table for t
	fixed_t pointz;                 // transformed distance to wall post
	fixed_t anglecos;
	fixed_t ceilingheight;  // top of the wall
	fixed_t floorheight;    // bottom of the wall
	int angle;                              // the ray angle that strikes the current po
	int texture;                    // 0-63 post number
	int x, x1, x2;                  // collumn and ranges
	fixed_t px,py;                      // player position
	span_t *span_p;
	unsigned span;
	fixed_t distance, absdistance;
	int baseangle;
	fixed_t textureadjust;  // the amount the texture p1ane is shifted
	spanobj_t spantype;

	// scan the doorlist for matching tilex/tiley
	// this only happens a couple times / frame max, so it's not a big d
	last_p=&doorlist[numdoors];

	for (door_p=doorlist; ; door_p++) {
		if (door_p==last_p)
			MS_Error("RenderDoor: Door not located");

		if (door_p->tilex==tilex&&door_p->tiley==tiley)
			break;
	}
	walltype=door_p->pic;

	//
	// transform both endpoints of the door
	// p1 is the anchored point, p2 is the moveable point
	//
	tx=tilex<<(TILESHIFT+FRACBITS);
	ty=tiley<<(TILESHIFT+FRACBITS);

	px = (int)((playerx) >> FRACTILESHIFT);
	py = (int)((playery) >> FRACTILESHIFT);

	switch (door_p->orientation) {
		case dr_horizontal:
			ty+=FRACUNIT *32;
			p1=TransformPoint(tx, ty);
			p2=TransformPoint(tx+door_p->position, ty);
			textureadjust=viewx+FRACUNIT *TILESIZE-(tx+door_p->position);
			baseangle=TANANGLES *2;
			distance=viewy-ty;
			break;
		case dr_vertical:
			tx+=FRACUNIT *32;
			p1=TransformPoint(tx, ty);
			p2=TransformPoint(tx, ty+door_p->position);
			textureadjust=viewy+FRACUNIT *TILESIZE-(ty+door_p->position);
			baseangle=TANANGLES;
			distance=tx-viewx;
			break;
	}

	if (!door_p->position||!ClipDoor()) {
		doorxl=windowWidth+1;
		doorxh=-1;
		return;
	}
	if (p1->px<p2->px) {
		doorxl=p1->px;
		doorxh=p2->px-1;
		x1=p1->px;
		x2=p2->px;
	}
	else {
		doorxl=p2->px;
		doorxh=p1->px-1;
		x1=p2->px;
		x2=p1->px;
	}
	//
	// calculate the textures to post into the span list
	//
	if (x1<xclipl)
		x1=xclipl;
	if (x2>xcliph+1)
		x2=xcliph+1;
	if (x1>x2)
		return;                         // totally clipped off side

	//
	// set up for loop
	//
	if (door_p->transparent)
	{
		spantype=sp_maskeddoor;
		doortile = false;
	}
	else
		spantype=sp_door;

	ceilingheight=vertex[0]->ceilingheight;
	floorheight=-vertex[0]->floorheight;

#ifdef VALIDATE
	if (walltype>=numwalls)
		MS_Error("DrawDoor: Invalid source walltype");
#endif
	walltype=walltranslation[walltype];     // global animation
#ifdef VALIDATE
	if (walltype>=numwalls)
		MS_Error("DrawDoor: Invalid translated walltype");
#endif
	walltype--;                             // make 0 based
	postindex=wallposts+(walltype<<6);      // 64 pointers to texture starts
	baseangle+=viewfineangle;

	absdistance=distance<0?-distance : distance;

	//
	// step through the individual posts
	//
	for (x=x1; x<x2; x++) {
		angle=baseangle+pixelangle[x];
		angle&=TANANGLES *2-1;

		//
		// calculate the texture post along the wall that was hit
		//
		texture=(textureadjust+FIXEDMUL(distance, tangents[angle]))>>FRACBITS;
		texture&=63;
		sp_source=postindex[texture];

		//
		// the z distance of the post hit = walldistance*cos(screenangle
		//
		anglecos=cosines[(angle-TANANGLES)&(TANANGLES *4-1)];
		pointz=FIXEDDIV(absdistance, anglecos);
		pointz=FIXEDMUL(pointz, pixelcosine[x]);

		if (pointz>MAXZ||pointz<MINZ)
			continue;
		//
		// post the span in the draw list
		//
		span=(pointz<<ZTOFRAC)&ZMASK;
	span|=(x^0x1ff)<<XSHIFT; // invert x1 so they are sorted in ascen
		// while Z is sorted in decending order
		span|=numspans;
		spantags[0][numspans]=span;

		span_p=&spans[numspans];
		span_p->spantype=spantype;
		span_p->picture=sp_source;
	span_p->y=ceilingheight;
	span_p->yh=floorheight;

		span_p->structure=door_p;

		numspans++;

	}
}

//===========================================================

/*                                                                                                                                              */
/*===============                                                                                                               */
/*=                                                                                                                                             */
/*= RenderSprites                                                                                                               */
/*=                                                                                                                                             */
/*= For each sprite, if the sprite's bounding rect touches a tile with a*/
/*= vertex, transform and clip the projected view rect.  If still visibl*/
/*= a span into the span list                                                                                   */
/*=                                                                                                                                             */
/*===============                                                                                                               */
/*                                                                                                                                              */

void RenderSprites(fixed_t x, fixed_t y, fixed_t z, int angle, byte showBlast)
{
	scaleobj_t *sprite;
	fixed_t deltax, deltay;
	fixed_t pointx, pointz;
	fixed_t gxt, gyt;
	int picnum;
	unsigned span;
	span_t *span_p;
	byte   animationGraphic;
	byte   animationMax;
	byte   animationDelay;

	for (sprite=firstscaleobj.next; sprite!=&lastscaleobj;
		 sprite=sprite->next) 
	{
		deltax=sprite->x-viewx;
		if (deltax<-MAXZ||deltax>MAXZ)
			continue;
		deltay=sprite->y-viewy;
		if (deltay<-MAXZ||deltay>MAXZ)
			continue;

		//
		// transform the point
		//
		gxt=FIXEDMUL(deltax, viewsin);
		gyt=FIXEDMUL(deltay, viewcos);
		pointx=gyt+gxt;

		gxt=FIXEDMUL(deltax, viewcos);
		gyt=FIXEDMUL(deltay, viewsin);
		pointz=gxt-gyt;

		if (pointz<MINZ||pointz>MAXZ)
			continue;

		//
		// calculate which image to display
		//
		picnum=sprite->basepic;
		if (sprite->rotate) {   // this is only aproximate, but ok for 8
		   if (sprite->rotate == rt_eight)
		     picnum+=((viewangle-sprite->angle+0x90)>>5)&7;
		   else
		     picnum+=((viewangle-sprite->angle+0x90)>>5)&3;
		}
		// TML 9-24-94
		// okay to animate sprites here because we don't need to unless
		// the player can see them (they only need to move when out of
		// sight)

		if ((sprite->animation) &&
		    (timecount > sprite->animationTime))
		{
		  animationGraphic = (sprite->animation & ANIM_CG_MASK) >> 1;
		  animationMax = (sprite->animation & ANIM_MG_MASK) >> 5;
		  animationDelay = (sprite->animation & ANIM_DELAY_MASK) >> 9;

		  if (animationGraphic < animationMax-1)
		    animationGraphic++;
		  else if (sprite->animation & ANIM_LOOP_MASK)
		    animationGraphic = 0;

		  picnum+=animationGraphic;

		  sprite->animation = (sprite->animation & ANIM_LOOP_MASK) +
				      (animationGraphic << 1) +
				      (animationMax << 5) +
				      (animationDelay << 9);

		  sprite->animationTime = timecount + animationDelay;
		}

#ifdef VALIDATE
/*
		if (picnum>=numsprites)
			MS_Error("RenderSprites: picnum > numsprites");
*/
#endif

		//
		// post the span event
		//
#ifdef VALIDATE
		if (numspans==MAXSPANS)
			MS_Error("MAXSPANS exceeded");
#endif
		span=(pointz<<ZTOFRAC)&ZMASK;
		// sprite spans have 0 in the tagx field to diferentiate them
		span|=numspans;
		spantags[0][numspans]=span;

		span_p=&spans[numspans];
		span_p->spantype=sp_shape;
		span_p->picture=lumpmain[picnum];
		span_p->x2=pointx;
		span_p->y=sprite->z-viewz;
		span_p->structure=sprite;

		numspans++;
	}
}
