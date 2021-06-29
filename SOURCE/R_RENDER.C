// R_render.c
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "D_global.h"
#include "R_refdef.h"

/*                                                                                                                                              */
/*===================                                                                                                   */
/*CONSTANTS                                                                                                                             */
/*===================                                                                                                   */
/*                                                                                                                                              */

/*                                                                                                                                              */
/*===================                                                                                                   */
/*TYPES                                                                                                                                 */
/*===================                                                                                                   */
/*                                                                                                                                              */

/*                                                                                                                                              */
/*===================                                                                                                   */
/*GLOBALS                                                                                                                               */
/*===================                                                                                                   */
 /*                                                                                                                                              */
hit_t id_type;
int id_tilex, id_tiley;         // not valid for sprites
int id_side;                            // 0(north)-3(west) for walls
int id_px, id_py;                       // position in texture
void *id_structure;                     // either doorobj_t or scaleobj_t

byte floorpic[MAPROWS*MAPCOLS];
byte ceilingpic[MAPROWS*MAPCOLS];
byte floorheight[MAPROWS*MAPCOLS];
byte ceilingheight[MAPROWS*MAPCOLS];
byte northwall[MAPROWS*MAPCOLS];
byte westwall[MAPROWS*MAPCOLS];
byte northbottom[MAPROWS*MAPCOLS];
byte westbottom[MAPROWS*MAPCOLS];
byte mapflags[MAPROWS*MAPCOLS];
byte mapsprites[MAPROWS*MAPCOLS];

void (*actionhook)(void);
int actionflag;

// each visible vertex is used up to four times, so to prevent recalcula
// the vertex info is reused if it has been calculated previously that f
// The calculated flag is also used to determine if a moving sprite is i
// is at least partially visable.
//
// frameon is incremented at the start of each frame, so it is 1 on the
// framevalid[][] holds the frameon number for which vertex[][] is valid
//      set to 0 at initialization, so no points are valid
// cornervertex[][] is a pointer into vertexlist[]
// vertexlist[] holds the currently valid transformed vertexes
// vertexlist_p is set to vertexlist[0] at the start of each frame, and
//      after transforming a new vertex
int frameon;
int framevalid[MAPROWS][MAPCOLS];
vertex_t *cornervertex[MAPROWS][MAPCOLS];
vertex_t vertexlist[MAXVISVERTEXES], *vertexlist_p;

fixed_t costable[ANGLES];
fixed_t sintable[ANGLES];

pixel_t viewbuffer[MAX_VIEW_WIDTH*MAX_VIEW_HEIGHT];
pixel_t *viewylookup[MAX_VIEW_HEIGHT];

fixed_t yslope[MAX_VIEW_HEIGHT], xslope[MAX_VIEW_WIDTH+1];

byte **wallposts;

byte *colormaps;
int numcolormaps;
byte *zcolormap[(MAXZ>>FRACBITS)+1];

fixed_t viewx, viewy, viewz;
fixed_t viewcos, viewsin;
fixed_t xscale, yscale;         // SCALE/viewcos , SCALE/viewsin
int viewangle, viewfineangle;
int viewtilex, viewtiley;

vertex_t *vertex[4];            // points to the for corner vertexes in vert
vertex_t *p1, *p2;
int side;                                       // wall number 0-3
int walltype;                           // wall number (picture) of p1-p2 edge
int wallshadow;                         // degree of shadow for a tile   

int xclipl, xcliph;                     // clip window for current tile
int tilex, tiley;                       // coordinates of the tile being rendered
int mapspot;                            // tiley*MAPSIZE+tilex

int *flattranslation;           // global animation tables
int *walltranslation;

int spritelump, walllump, flatlump;
int numsprites, numwalls, numflats;

boolean doortile;               // true if the tile being renderd has a door

/*                                                                                                                                              */
/*===================                                                                                                   */
/*LOCALS                                                                                                                                */
/*===================                                                                                                   */
/*                                                                                                                                              */

int adjacentx[4]=
{
	0, 1, 0, -1
};

// deltas to the tile facing the given wall
int adjacenty[4]=
{
	-1, 0, 1, 0
};



entry_t entries[2][30], *entry_p;       // holds tile numbers and clip windo
// entries are read out of one list, checked for
// duplication, and passed to RenderTile
// The other list collects new entries from the
// exits detected by RenderTile

/*                                                                                                                                              */
/*==================                                                                                                    */
/*=                                                                                                                                             */
/*= TransformVertex                                                                                                             */
/*=                                                                                                                                             */
/*= Returns a pointer to the vertex for a given coordinate                              */
/*= tx,tz will be the transformed coordinates                                                   */
/*= px, floorheight, ceilingheight will be valid if tz >= MINZ                  */
/*=                                                                                                                                             */
/*==================                                                                                                    */
/*                                                                                                                                              */

vertex_t *TransformVertex(int tilex, int tiley)
{
	fixed_t trx, try;
	fixed_t gxt, gyt;
	fixed_t scale;
	vertex_t *point;
	int mapspot;

	mapspot=tiley*MAPROWS+tilex;

	if (framevalid[tiley][tilex]==frameon)
		return cornervertex[tiley][tilex];      // vertex has already been t

	point=vertexlist_p++;
#ifdef VALIDATE
	if (point>=&vertexlist[MAXVISVERTEXES])
		MS_Error("Vertexlist overflow");
#endif
	framevalid[tiley][tilex]=frameon;
	cornervertex[tiley][tilex]=point;

	point->floorheight=(floorheight[mapspot]<<FRACBITS)-viewz;
	point->ceilingheight=(ceilingheight[mapspot]<<FRACBITS)-viewz;

	trx=(tilex<<(FRACBITS+TILESHIFT))-viewx;
	try=(tiley<<(FRACBITS+TILESHIFT))-viewy;

	gxt=FIXEDMUL(trx, viewsin);
	gyt=FIXEDMUL(try, viewcos);
	point->tx=gyt+gxt;

	gxt=FIXEDMUL(trx, viewcos);
	gyt=FIXEDMUL(try, viewsin);
	point->tz=gxt-gyt;

#ifdef FLOATCOORD
	point->ftx=(float)point->tx/FRACUNIT;
	point->ftz=(float)point->tz/FRACUNIT;
#endif
	if (point->tz>=MINZ) {
		scale=FIXEDDIV(SCALE, point->tz);
		point->px=CENTERX+(FIXEDMUL(point->tx, scale)>>FRACBITS);
		point->floory=CENTERY-(FIXEDMUL(point->floorheight, scale)>>FRACBITS);
		point->ceilingy=CENTERY-(FIXEDMUL(point->ceilingheight,scale)>>FRACBITS);
	}
	return point;
}

/*                                                                                                                                              */
/*==================                                                                                                    */
/*=                                                                                                                                             */
/*= ClipEdge                                                                                                                    */
/*=                                                                                                                                             */
/*= Sets p1->px and p2->px correctly for Z values < MINZ                                */
/*=                                                                                                                                             */
/*= Returns false if entire edge is too close or far away                               */
/*=                                                                                                                                             */
/*==================                                                                                                    */
/*                                                                                                                                              */

boolean ClipEdge(void)
{
	fixed_t leftfrac, rightfrac, clipz;
	fixed_t dx,dz;

	if (p1->tz>MAXZ&&p2->tz>MAXZ)
		return false;           // entire face is too far away

	if (p1->tz<0&&p2->tz<0)
		return false;           // totally behind the projection plane

	if (p1->tz<MINZ || p2->tz<MINZ)
	{
		dx = p2->tx - p1->tx;
		dz = p2->tz - p1->tz;
		if (p1->tz<MINZ)
		{
			if ( abs(dx+dz) < 1024)
				return false;
			leftfrac = FIXEDDIV(-p1->tx - p1->tz , dx+dz);
		}
		if (p2->tz<MINZ)
		{
			if ( abs(dz-dx) < 1024)
				return false;
			rightfrac = FIXEDDIV(p1->tx - p1->tz , -dx+dz);
			if (p1->tz<MINZ && rightfrac < leftfrac)
				return false;           // back face
			clipz = p1->tz + FIXEDMUL(dz,rightfrac);
			if (clipz<0)
				return false;
			p2->px = windowWidth;
		}
	}

	if (p1->tz<MINZ) {
		clipz = p1->tz + FIXEDMUL(dz,leftfrac);
		if (clipz<0)
			return false;
		p1->px = 0;
	}

	if (p1->px==p2->px)
		return false;

	return true;
}


/*                                                                                                                                              */
/*===============                                                                                                               */
/*=                                                                                                                                             */
/*= RenderTileWalls                                                                                                             */
/*=                                                                                                                                             */
/*===============                                                                                                               */
/*                                                                                                                                              */

void RenderTileWalls(entry_t *e, fixed_t playerx, fixed_t playery)
{
	int xl, xh;

	tilex=e->tilex;
	tiley=e->tiley;
	mapspot=tiley*MAPROWS+tilex;

	xclipl=e->xmin;
	xcliph=e->xmax;

#ifdef VALIDATE
	if ((tilex<0)||(tilex>=MAPCOLS)||(tiley<0)||(tiley>=MAPROWS)||(xclipl<0)||
		(xclipl>=windowWidth)||(xcliph<0)||(xcliph>=windowWidth)||(xclipl>xcliph))
		MS_Error("Invalid RenderTile (%i, %i, %i, %i)\n", e->tilex, e->tiley,
				 e->xmin, e->xmax);
#endif

	//
	// validate or transform the four corner vertexes
	//
	vertex[0]=TransformVertex(tilex, tiley);
	vertex[1]=TransformVertex(tilex+1, tiley);
	vertex[2]=TransformVertex(tilex+1, tiley+1);
	vertex[3]=TransformVertex(tilex, tiley+1);

	//
	// handle a door if present
	//
	if (mapflags[mapspot]&FL_DOOR) {
		doortile=true;
		RenderDoor(playerx,playery);      // sets doorxl / doorxh
	}
	else
		doortile=false;

	//
	// draw or flow through the walls
	//
	for (side=0; side<4; side++) {
		p1=vertex[side];
		p2=vertex[(side+1)&3];
		if (!ClipEdge()) continue;
		if (p1->px>=p2->px) continue; // back face
		switch (side) {
			// only 64 wall types allowed so &63 ensures that
			// all values are mapped into a correct value;
			// any portion above 63 is assumed to be a shadowing
			// factor
			case 0: 
			  wallshadow=northwall[mapspot] >> 6;
			  walltype=northwall[mapspot] & 63; 
			  break;
			case 1: 
			  wallshadow=westwall[mapspot+1] >> 6;
			  walltype=westwall[mapspot+1] & 63; 
			  break;
			case 2: 
			  wallshadow=northwall[mapspot+MAPSIZE] >> 6;
			  walltype=northwall[mapspot+MAPSIZE] & 63; 
			  break;
			case 3: 
			  wallshadow=westwall[mapspot] >> 6;
			  walltype=westwall[mapspot] & 63; 
			  break;
		}
	if (walltype)
	{
	    DrawWall();
	}
		else {
			//
			// restrict outward flow by the door, if present
			//
			xl=p1->px;
			xh=p2->px-1;

			if (doortile) {
				if ((doorxl<=xclipl)&&(doorxh>=xl)) xl=doorxh+1;
				if ((doorxh>=xcliph)&&(doorxl<=xh)) xh=doorxl-1;
			}
			//
			// restrict by clipping window
			//
			if (xl<xclipl) xl=xclipl;
			if (xh>xcliph) xh=xcliph;
			//
			// flow into the adjacent tile if there is at lest a one pix
			//
			if (xh>=xl) {
				entry_p->tilex=tilex+adjacentx[side];
				entry_p->tiley=tiley+adjacenty[side];
				entry_p->xmin=xl;
				entry_p->xmax=xh;
				entry_p++;
			}
		}
	}
}

//============================================================

/*                                                                                                                                              */
/*================                                                                                                              */
/*=                                                                                                                                             */
/*= SetupFrame                                                                                                                  */
/*=                                                                                                                                             */
/*================                                                                                                              */
/*                                                                                                                                              */
void SetupFrame(void)
{
	//
	// clear buffers
	//
	memset(viewbuffer, 0, sizeof(viewbuffer));
	ClearWalls();                   // no walls drawn yet
	ClearMapCache();                // invalidate cached pointz calculations
	numspans=0;
	frameon++;                              // vertexes from last frame are now invalid
	vertexlist_p=&vertexlist[0];    // put the first transformed vertex
	// begining of the list

	viewtilex=viewx>>TILEFRACSHIFT;
	viewtiley=viewy>>TILEFRACSHIFT;

	viewfineangle=viewangle<<FINESHIFT;
	viewcos=costable[viewangle];
	viewsin=sintable[viewangle];

	xscale=FIXEDDIV(viewsin, SCALE);
	yscale=FIXEDDIV(viewcos, SCALE);

	FindBackVertex();

}

//============================================================

/*                                                                                                                                              */
/*===============                                                                                                               */
/*=                                                                                                                                             */
/*= FlowView                                                                                                                    */
/*=                                                                                                                                             */
/*===============                                                                                                               */
/*                                                                                                                                              */

void FlowView(fixed_t playerx, fixed_t playery)
{
	int table;
	entry_t *process_p, *nextprocess_p, *endprocess_p;

	//
	// spread out from the view tile into all the visible tiles
	//
	process_p=&entries[0][0];
	process_p->tilex=viewtilex;
	process_p->tiley=viewtiley;
	process_p->xmin=0;
	process_p->xmax=windowWidth-1;
	endprocess_p=process_p+1;
	table=1;
	do {
		entry_p=&entries[table][0];
		while (process_p!=endprocess_p) {
			if (process_p->tilex==-1)       { // the entry was combined away
				process_p++;
				continue;
			}
			//
			// check for combining two entries
			//
	    /*
	    for (nextprocess_p=process_p+1; nextprocess_p!=endprocess_p;nextprocess_p++) {
				if ((nextprocess_p->tilex!=process_p->tilex)||
					(nextprocess_p->tiley!=process_p->tiley))       continue;

				if (nextprocess_p->xmin==process_p->xmax+1)
					process_p->xmax=nextprocess_p->xmax;
				else if (nextprocess_p->xmax==process_p->xmin-1)
					process_p->xmin=nextprocess_p->xmin;
				else {
// 1992-12-02 12:38 REB adding more output info for this error
					unsigned char buf[256];
					unsigned char msg[256];
					unsigned char inf[256];
					buf[0]=msg[0]=inf[0]=0;
					sprintf(buf,"nextprocess_p=%d,process_p=%d/n",(int)nextprocess_p,(int)process_p);
					strcat(inf,buf);
					sprintf(buf,"nextprocess_p->xmin=%d,nextprocess_p->xmax=%d\n",nextprocess_p->xmin,nextprocess_p->xmax);
					strcat(inf,buf);
					sprintf(buf,"process_p->xmin=%d,process_p->xmax=%d\n",process_p->xmin,process_p->xmax);
					strcat(inf,buf);
					strcat(msg,"Bad tile event combination\n");
					strcat(msg,inf);
					MS_Error(msg);
//                                      MS_Error("Bad tile event combination");
				}
				nextprocess_p->tilex=-1;        // don't use again
				break;
			}
	    */
			//
			// draw or post everything for process_p
			//
	    RenderTileWalls(process_p,playerx,playery);
	    RenderTileEnds();

			process_p++;
		}
		endprocess_p=entry_p;
		process_p=&entries[table][0];
		table^=1;
	} while (endprocess_p!=process_p);

}
