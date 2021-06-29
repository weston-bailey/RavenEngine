
#include <stdlib.h>
#include <math.h>
#include <string.h>
//#include "r_public.h"
#include "D_global.h"
#include "d_disk.h"
#include "R_refdef.h"

int windowHeight = INIT_VIEW_HEIGHT;
int windowWidth = INIT_VIEW_WIDTH;
int windowLeft = (320-INIT_VIEW_WIDTH) >> 1;
int windowTop = (200-INIT_VIEW_HEIGHT) >> 1;
int windowSize = INIT_VIEW_HEIGHT*INIT_VIEW_WIDTH;
int viewLocation = 0xA0000+VIEW_TOP*320+VIEW_LEFT;

//=============================================================

/*                                                                                                                                              */
/*=====================                                                                                                 */
/*=                                                                                                                                             */
/*= RF_PreloadGraphics                                                                                                  */
/*=                                                                                                                                             */
/*=====================                                                                                                 */
/*                                                                                                                                              */

void RF_PreloadGraphics(void)
{
	int i, x;
	byte *base;
	byte *wall;
	int size;

	//
	// find the number of lumps of each type
	//
	spritelump=CA_GetNamedNum("startsprites");
	numsprites=CA_GetNamedNum("endsprites")-spritelump;

	walllump=CA_GetNamedNum("startwalls");
	numwalls=CA_GetNamedNum("endwalls")-walllump;

	flatlump=CA_GetNamedNum("startflats");
	numflats=CA_GetNamedNum("endflats")-flatlump;

	//
	// load the lumps
	//
	for (i=1; i<numsprites; i++)
		CA_CacheLump(spritelump+i);
	for (i=1; i<numwalls; i++)
		CA_CacheLump(walllump+i);
	for (i=1; i<numflats; i++)
		CA_CacheLump(flatlump+i);


	//
	// set up pointers
	//
	wallposts=malloc((size_t)(numwalls+1)*64*4);

	for (i=0; i<numwalls-1; i++) {
		wall=lumpmain[walllump+i+1];
		base=wall+65*2;

		size=*wall *4;
		for (x=0; x<64; x++)
			wallposts[i *64+x]=base+size *x;
	}
}


/*                                                                                                                                              */
/*==================                                                                                                    */
/*=                                                                                                                                             */
/*= RF_Startup                                                                                                                  */
/*=                                                                                                                                             */
/*==================                                                                                                    */
/*                                                                                                                                              */

void RF_Startup(void)
{
	int i;
	double angle;
	int lightlump;

	InitWalls();

	memset(framevalid, 0, sizeof(framevalid));
	frameon=0;
	//
	// trig tables
	//
	for (i=0; i<ANGLES; i++) {
		angle=i *PI *2/ANGLES;
		sintable[i]=rint(sin(angle)*FRACUNIT);
		costable[i]=rint(cos(angle)*FRACUNIT);
	}
	SetViewSize(windowWidth,windowHeight);
	//
	// set up lights
	// Allocates a page aligned buffer and load in the light tables
	//
	lightlump=CA_GetNamedNum("lights");
	numcolormaps=infotable[lightlump].size/256;
	colormaps=malloc((size_t)256*(numcolormaps+1));
	colormaps=(byte *)(((int)colormaps+255)&~0xff);

	CA_ReadLump(lightlump, colormaps);

	RF_SetLights(MAXZ);
	RF_ClearWorld();
	RF_PreloadGraphics();

	//
	// initialize the translation to no animation
	//
	flattranslation=malloc((size_t)(numflats+1)*4);
	for (i=0; i<=numflats; i++)
		flattranslation[i]=i;
	walltranslation=malloc((size_t)(numwalls+1)*4);
	for (i=0; i<=numwalls; i++)
		walltranslation[i]=i;

	actionhook=NULL;
	actionflag=0;
}

//=============================================================

/*                                                                                                                                              */
/*================                                                                                                              */
/*=                                                                                                                                             */
/*= RF_ClearWorld                                                                                                               */
/*=                                                                                                                                             */
/*= Clears out all sprites and doors                                                                    */
/*=                                                                                                                                             */
/*================                                                                                                              */
/*                                                                                                                                              */

void RF_ClearWorld(void)
{
	int i;

	firstscaleobj.prev=NULL;
	firstscaleobj.next=&lastscaleobj;

	lastscaleobj.prev=&firstscaleobj;
	lastscaleobj.next=NULL;

	freescaleobj_p=scaleobjlist;

	memset(scaleobjlist, 0, sizeof(scaleobjlist));

	for (i=0; i<MAXSPRITES-1; i++)
		scaleobjlist[i].next=&scaleobjlist[i+1];

	numdoors=0;
}


//==========================================================

/*                                                                                                                                              */
/*================                                                                                                              */
/*=                                                                                                                                             */
/*= RF_GetDoor                                                                                                                  */
/*=                                                                                                                                             */
/*================                                                                                                              */
/*                                                                                                                                              */

doorobj_t *RF_GetDoor(int tilex, int tiley)
{
	doorobj_t *door;

	if (numdoors==MAXDOORS)
		MS_Error("Too many doors placed!");

	door=&doorlist[numdoors];
	numdoors++;
	door->tilex=tilex;
	door->tiley=tiley;

	mapflags[tiley*MAPROWS+tilex] |= FL_DOOR;

	return door;
}

//==========================================================

/*                                                                                                                                              */
/*================                                                                                                              */
/*=                                                                                                                                             */
/*= RF_GetSprite                                                                                                                */
/*=                                                                                                                                             */
/*= Return a free sprite structure that has been added to the end of    */
/*= the active list                                                                                                             */
/*=                                                                                                                                             */
/*================                                                                                                              */
/*                                                                                                                                              */

scaleobj_t *RF_GetSprite(void)
{
	scaleobj_t *new;

	if (!freescaleobj_p)
		MS_Error("GetSprite: Out of spots in scaleobjlist!");

	new=freescaleobj_p;
	freescaleobj_p=freescaleobj_p->next;

	memset(new, 0, sizeof(*new));
	new->next=(scaleobj_t *)&lastscaleobj;
	new->prev=lastscaleobj.prev;
	lastscaleobj.prev=new;
	new->prev->next=new;

	return new;
}

//==========================================================

/*                                                                              */
/*====================*/
/*=                                                                             */
/*= RF_RemoveSprite             */
/*=                                                                             */
/*= Unlink the sprite */
/*= from the active   */
/*= list                                                        */
/*=                                                                             */
/*====================*/
/*                                                                              */

void RF_RemoveSprite(scaleobj_t *spr)
{
	spr->next->prev=spr->prev;
	spr->prev->next=spr->next;
	
	memset(spr, 0, sizeof(spr));
	spr->next=freescaleobj_p;
	freescaleobj_p=spr;
}

//==========================================================


/*                                                                               */
/*=====================*/
/*=                                                                              */
/*= RF_GetFloorZ                         */
/*=                                                                              */
/*=====================*/
/*                                                                               */

fixed_t RF_GetFloorZ(fixed_t x, fixed_t y)
{
	fixed_t h1, h2, h3, h4;
	int tilex, tiley, mapspot;
	int polytype;
	fixed_t fx, fy;
	fixed_t top, bottom;

	tilex=x>>(FRACBITS+TILESHIFT);
	tiley=y>>(FRACBITS+TILESHIFT);

	mapspot=tiley *MAPSIZE+tilex;
	polytype=(mapflags[mapspot]&FL_FLOOR)>>FLS_FLOOR;

	//
	// flat
	//
	if (polytype==POLY_FLAT)
	  return floorheight[mapspot]<<FRACBITS;

	//
	// constant slopes
	//
	h1=floorheight[mapspot]<<FRACBITS;
	h2=floorheight[mapspot+1]<<FRACBITS;
	h3=floorheight[mapspot+MAPSIZE]<<FRACBITS;
	h4=floorheight[mapspot+MAPSIZE+1]<<FRACBITS;

	fx=(x&(TILEUNIT-1))>>6; // range from 0 to fracunit-1
	fy=(y&(TILEUNIT-1))>>6;

	if (polytype==POLY_SLOPE) {
		if (h1==h2)                     // north/south slope
			return h1+FIXEDMUL(h3-h1, fy);
		else                            // east/west slope
			return h1+FIXEDMUL(h2-h1, fx);
	}
	//
	// triangulated slopes
	//

	// set the outside corner of the triangle that the point is NOT in s
	// plane with the other three

	if (polytype==POLY_ULTOLR) {
		if (fx>fy)
			h3=h1-(h2-h1);
		else
			h2=h1+(h1-h3);
	}
	else {
		if (fx<FRACUNIT-fy)
			h4=h2+(h2-h1);
		else
			h1=h2-(h4-h2);
	}
	top=h1+FIXEDMUL(h2-h1, fx);
	bottom=h3+FIXEDMUL(h4-h3, fx);
	return top+FIXEDMUL(bottom-top, fy);
}


//==========================================================

/*                                                                                                                                              */
/*=====================                                                                                                 */
/*=                                                                                                                                             */
/*= RF_GetCeilingZ                                                                                                              */
/*=                                                                                                                                             */
/*=====================                                                                                                 */
/*                                                                                                                                              */

fixed_t RF_GetCeilingZ(fixed_t x, fixed_t y)
{
	fixed_t h1, h2, h3, h4;
	int tilex, tiley, mapspot;
	int polytype;
	fixed_t fx, fy;
	fixed_t top, bottom;

	tilex=x>>(FRACBITS+TILESHIFT);
	tiley=y>>(FRACBITS+TILESHIFT);

	mapspot=tiley *MAPSIZE+tilex;
	polytype=(mapflags[mapspot]&FL_CEILING)>>FLS_CEILING;

	//
	// flat
	//
	if (polytype==POLY_FLAT)
		return ceilingheight[mapspot]<<FRACBITS;

	//
	// constant slopes
	//
	h1=ceilingheight[mapspot]<<FRACBITS;
	h2=ceilingheight[mapspot+1]<<FRACBITS;
	h3=ceilingheight[mapspot+MAPSIZE]<<FRACBITS;
	h4=ceilingheight[mapspot+MAPSIZE+1]<<FRACBITS;

	fx=(x&(TILEUNIT-1))>>6; // range from 0 to fracunit-1
	fy=(y&(TILEUNIT-1))>>6;

	if (polytype==POLY_SLOPE) {
		if (h1==h2)                     // north/south slope
			return h1+FIXEDMUL(h3-h1, fy);
		else                            // east/west slope
			return h1+FIXEDMUL(h2-h1, fx);
	}
	//
	// triangulated slopes
	//

	// set the outside corner of the triangle that the point is NOT in s
	// plane with the other three

	if (polytype==POLY_ULTOLR) {
		if (fx>fy)
			h3=h1-(h2-h1);
		else
			h2=h1+(h1-h3);
	}
	else {
		if (fx<FRACUNIT-fy)
			h4=h2+(h2-h1);
		else
			h1=h2-(h4-h2);
	}
	top=h1+FIXEDMUL(h2-h1, fx);
	bottom=h3+FIXEDMUL(h4-h3, fx);
	return top+FIXEDMUL(bottom-top, fy);
}


//==========================================================

/*                                                                               */
/*===================   */
/*=                                                                              */
/*= CheckSpriteContact */
/*=                                                                              */
/*=====================*/
/*                                                                               */

boolean CheckSpriteContact(span_t *span_p, fixed_t pointz, int sx, int sy)
{
	int x;
	fixed_t leftx;
	fixed_t scale;
	fixed_t fracstep;
	fixed_t shapebottom, topheight, bottomheight;
	int post;
	int topy, bottomy;
	scalepic_t *pic;
	byte *collumn;
	int deltascreen, posty;

	pic=(scalepic_t *)span_p->picture;
	shapebottom=span_p->y;

	scale=FIXEDDIV(SCALE, pointz);
	fracstep=FIXEDMUL(pointz, ISCALE);
	sp_fracstep=fracstep;
	leftx=span_p->x2;
	leftx-=pic->leftoffset<<FRACBITS;

	x=CENTERX+(FIXEDMUL(leftx, scale)>>FRACBITS);
	if (x>sx)
		return false;           // the sprite is to the right of the point
	deltascreen=sx-x;
	post=(fracstep *deltascreen)>>FRACBITS;

	if (post>=pic->width)
		return false;           // the sprite is to the left of the point

	if (pointz>wallz[sx])
		return false;           // this post is obscured by a closer wall

	collumn=(byte *)pic+pic->collumnofs[post];

	topheight=shapebottom+(*collumn<<FRACBITS);
	bottomheight=shapebottom+(*(collumn+1)<<FRACBITS);
	collumn+=2;

	topy=CENTERY-(FIXEDMUL(topheight, scale)>>FRACBITS);
	if (topy>sy)
		return false;           // shape is below point
	bottomy=CENTERY-(FIXEDMUL(bottomheight, scale)>>FRACBITS);
	if (bottomy<sy)
		return false;           // shape is above point

	deltascreen=sy-topy;
	posty=(sp_fracstep *deltascreen)>>FRACBITS;

	if (!collumn[posty])
		return false;           // point is in a transparent area

	id_type=id_sprite;
	id_structure=span_p->structure;
	return true;
}

/*                                                                                              */
/*========================*/
/*=                                                                                      =*/
/*= RF_PixelIdentity             =*/
/*=                                                                                      =*/
/*= You can only call    =*/
/*= this between frames, =*/
/*= not during an action =*/
/*= hook routine call            =*/
/*=                                                                                      =*/
/*========================*/
/*                                                                                              */

void RF_PixelIdentity(int sx, int sy)
{
	unsigned tag, *spantag_p;
	int spannum;
	span_t *span_p;
	int spanx;
	fixed_t pointz, length, fracstep;
	int deltay;
	int angle;

	//
	// scan the sorted span lists from closest to farthest
	//
	spantag_p=endtaglist_p-1;
	while (spantag_p>=starttaglist_p) {
		tag=*spantag_p--;

		spannum=tag&SPANMASK;
		span_p=&spans[spannum];
		spanx=tag&XMASK;
		pointz=(tag&ZMASK)>>ZTOFRAC;

		if (span_p->spantype==sp_shape) {
			// check for intersection with the sprite's pixels
			if (CheckSpriteContact(span_p, pointz, sx, sy))
				return;
			continue;
		}
	spanx=(spanx>>XSHIFT)^0x1ff;     // invert back to regular x

		if ((span_p->spantype==sp_door)||(span_p->spantype==sp_maskeddoor)) {
			// if it got here, it didn't hit a floor or ceiling in front
			if (spanx!=sx)
				continue;
			id_type=id_door;
			id_structure=span_p->structure;
			return;
		}
		/// its just a floor/ ceiling span
		// check the extent of the span
		if (span_p->y!=sy||spanx>sx||span_p->x2<=sx)
			continue;
		if (tag&CEILINGBIT)
			id_type=id_ceiling;
		else
			id_type=id_floor;

		angle=viewfineangle+pixelangle[sx];
		angle&=TANANGLES *4-1;

		length=FIXEDDIV(pointz, pixelcosine[sx]);
		id_px=viewx+FIXEDMUL(length, cosines[angle]);
		id_py=viewy-FIXEDMUL(length, sines[angle]);
		return;
	}
	//
	// if there is a wall post at sx, the click was somewhere in it
	//
	if (wallz[sx]==MAXZ+1) {
		// didn't click on anything
		id_type=id_empty;
		return;
	}
	id_type=id_wall;
	id_tilex=wallnumber[sx]>>17;
	id_tiley=(wallnumber[sx]>>2)&63;
	id_side=wallnumber[sx]&3;
	id_px=walltexture[sx];
	deltay=sy-walltopy[sx];
	fracstep=FIXEDMUL(wallz[sx], ISCALEFUDGE);
	id_py=(deltay *fracstep)>>FRACBITS;
	if (id_py<0)
		id_py=0;

}

/*                                                                               */
/*=====================*/
/*=                                                                              */
/*= RF_SetLights                         */
/*=                                                                              */
/*= Spreads the light  */
/*= translation tables */
/*= from 0 - MAXZ, with*/
/*= the black point at */
/*= blackz. It is OK to*/
/*= have blackz>MAXZ,  */
/*= things will be     */
/*= brighter when they */
/*= appear                                               */
/*=====================*/
/*                                                                               */

void RF_SetLights(fixed_t blackz)
{
	// linear diminishing
	int i;
	int table;

	blackz>>=FRACBITS;
	for (i=0; i<=MAXZ>>FRACBITS; i++) {
		table=numcolormaps *i/blackz;
		if (table>=numcolormaps)
			table=numcolormaps-1;
		zcolormap[i]=colormaps+table *256;
	}
}

//==========================================================

/*                                                                                                                                              */
/*=====================                                                                                                 */
/*=                                                                                                                                             */
/*= RF_CheckActionFlag                                                                                                  */
/*=                                                                                                                                             */
/*=====================                                                                                                 */
/*                                                                                                                                              */

void RF_CheckActionFlag(void)
{
	if (!actionflag)
		return;

	if (!actionhook)
		MS_Error("RF_CheckActionFlag: Actionhook not set");

	actionhook();
	actionflag=0;
}

/*                                                                                                                                              */
/*=====================                                                                                                 */
/*=                                                                                                                                             */
/*= RF_SetActionHook                                                                                                    */
/*=                                                                                                                                             */
/*=====================                                                                                                 */
/*                                                                                                                                              */

void RF_SetActionHook(void (*hook)(void))
{
	actionhook=hook;
}

//==========================================================

/*                                                                                                                                              */
/*===============                                                                                                               */
/*=                                                                                                                                             */
/*= RF_RenderView                                                                                                               */
/*=                                                                                                                                             */
/*===============                                                                                                               */
/*                                                                                                                                              */

void RF_RenderView(fixed_t x, fixed_t y, fixed_t z, int angle, byte showBlast)
{
	if ((x<=0)||(x>=((MAPSIZE-1)<<(FRACBITS+TILESHIFT)))||(y<=0)||
		(y>=((MAPSIZE-1)<<(FRACBITS+TILESHIFT))||angle<0||angle>=ANGLES))
		MS_Error("Invalid RF_RenderView (%p, %p, %p, %i)\n", x, y, z, angle);

	viewx=(x&~0xfff) + 0x800;
	viewy=(y&~0xfff) + 0x800;
	viewz=(z&~0xfff) + 0x800;
	viewangle=angle;

	SetupFrame();
	RF_CheckActionFlag();
    FlowView(x,y);
    RF_CheckActionFlag();
    RenderSprites(x, y, z, angle, showBlast);
    RF_CheckActionFlag();
    DrawSpans();
    RF_CheckActionFlag();
}

void SetViewSize(int width, int height)
{
	int i;

	if (width>MAX_VIEW_WIDTH) width = MAX_VIEW_WIDTH;
	if (height>MAX_VIEW_HEIGHT) height = MAX_VIEW_HEIGHT;
	windowHeight = height;
	windowWidth = width;
	windowSize = width*height;
	for (i=0;i<height;i++) viewylookup[i]=viewbuffer+i*width;
	//
	// slopes for rows and collumns of screen pixels
	// slightly biased to account for the truncation in coordinates
	for (i=0;i<=width;i++)
		xslope[i] = rint((float)(i+1-CENTERX)/CENTERX*FRACUNIT);
	for (i=0;i<height;i++)
		yslope[i] = rint(-(float)(i-0.5-CENTERY)/CENTERX*FRACUNIT);
}
