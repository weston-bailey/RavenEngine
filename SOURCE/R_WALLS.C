// RV_Walls.c
#include <math.h>
#include <string.h>
#include "D_global.h"
#include "R_refdef.h"
#include "d_disk.h"
int angleadjust[4]=
{
	0, TANANGLES, 0, TANANGLES
};



fixed_t tangents[TANANGLES *2];
fixed_t sines[TANANGLES *5];
fixed_t *cosines;                       // point 1/4 phase into sines

int pixelangle[MAX_VIEW_WIDTH+1];
fixed_t pixelcosine[MAX_VIEW_WIDTH+1];

//
// the wall_??[x] arrays are used to determine where a mouse click is
//
int wallnumber[MAX_VIEW_WIDTH]; // tilex<<17 + tiley<<2 + side, -1 = no wall
int walltopy[MAX_VIEW_WIDTH];
int walltexture[MAX_VIEW_WIDTH];        // 0-63
fixed_t wallz[MAX_VIEW_WIDTH];  // pointx

/*                                                                                                                                              */
/*==================                                                                                                    */
/*=                                                                                                                                             */
/*= InitWalls                                                                                                                   */
/*=                                                                                                                                             */
/*= Builds tangent tables for -90 degrees to +90 degrees                                */
/*= and pixel angle table                                                                                               */
/*=                                                                                                                                             */
/*==================                                                                                                    */
/*                                                                                                                                              */

void InitWalls(void)
{
	int i;
	int intval;
	double tang, value, ivalue;

	//
	// tangent values for wall tracing
	//
	for (i=0; i<TANANGLES/2; i++) {
		tang=(i+0.5)*PI/(TANANGLES *2);
		value=tan(tang);
		ivalue=1/value;
		value=rint(value *FRACUNIT);
		ivalue=rint(ivalue *FRACUNIT);

		tangents[TANANGLES+i]=-value;
		tangents[TANANGLES+TANANGLES-1 -i]=-ivalue;

		tangents[i]=ivalue;
		tangents[TANANGLES-1 -i]=value;
	}
	//
	// high precision sin / cos for distance calculations
	//
	for (i=0; i<TANANGLES; i++) {
		tang=(i+0.5)*PI/(TANANGLES *2);
		value=sin(tang);
		intval=rint(value *FRACUNIT);
		sines[i]=intval;
		sines[TANANGLES *4+i]=intval;
		sines[TANANGLES *2 -1 -i]=intval;
		sines[TANANGLES *2+i]=-intval;
		sines[TANANGLES *4 -1 -i]=-intval;
	}
	cosines=&sines[TANANGLES];

	//
	// calculate the angle deltas for each view post
	// VIEWWIDTH view posts covers TANANGLES angles
	// traces go through the RIGHT EDGE of the pixel to follow the direc
	//
	for (i=0;i<windowWidth+1; i++) {
		intval=rint(atan((double)(CENTERX-(i+1))/CENTERX)/PI *TANANGLES *2);
		pixelangle[i]=intval;

		pixelcosine[i]=cosines[intval&(TANANGLES *4-1)];
	}
}

//============================================================

/*                                                                                                                                              */
/*==================                                                                                                    */
/*=                                                                                                                                             */
/*= ClearWalls                                                                                                                  */
/*=                                                                                                                                             */
/*= Clears the wallz array, so posts that fade out into the distance won*/
/*= block sprites                                                                                                               */
/*=                                                                                                                                             */
/*===================                                                                                                   */
/*                                                                                                                                              */

void ClearWalls(void)
{
	int i;

	for (i=0; i<windowWidth; i++)
		wallz[i]=MAXZ+1;
}

//============================================================

/*                                                                                                                                              */
/*====================                                                                                                  */
/*=                                                                                                                                             */
/*= DrawWall                                                                                                                    */
/*=                                                                                                                                             */
/*= Draws the wall on side from p1->px to p2->px-1 with wall picture wal*/
/*= p1/p2 are projected and Z clipped, but unclipped to the view window */
/*=                                                                                                                                             */
/*====================                                                                                                  */
/*                                                                                                                                              */

void DrawWall(void)
{
	int baseangle, wallnum;
	byte **postindex;               // start of the 64 entry texture table for t
	fixed_t distance;               // horizontal / vertical dist to wall segmen
	fixed_t pointz;                 // transformed distance to wall post
	fixed_t anglecos;
	fixed_t textureadjust;  // the amount the texture p1ane is shifted
	fixed_t ceilingheight;  // top of the wall
	fixed_t floorheight;    // bottom of the wall
	fixed_t top, bottom;    // precise y coordinates for post
	int topy, bottomy;              // pixel y coordinates for post
	fixed_t fracadjust;             // the amount to prestep for the top pixel
	int angle;                              // the ray angle that strikes the current po
	int texture;                    // 0-63 post number
	int x, x1, x2;                  // collumn and ranges
	int p1mapspot, p2mapspot;
	short   *wall;

	x1=p1->px<xclipl?xclipl : p1->px;
	x2=p2->px-1>xcliph?xcliph : p2->px-1;
	if (x1>x2)
		return;                         // totally clipped off side

	//
	// set up for loop
	//

#ifdef VALIDATE
	if (walltype>=numwalls)
		MS_Error("DrawWall: Invalid source walltype");
#endif
	walltype=walltranslation[walltype];     // global animation
#ifdef VALIDATE
	if (walltype>=numwalls)
		MS_Error("DrawWall: Invalid translated walltype");
#endif
	wall=lumpmain[walllump+walltype];               // to get wall height
	postindex=wallposts+((walltype-1)<<6);  // 64 pointers to texture st
	baseangle=viewfineangle;
	wallnum=(tilex<<17)+(tiley<<2)+side;    // so a mouse click can be l

	switch (side) {
		case 0:                         // south facing wall
			distance=viewy-(tiley<<(FRACBITS+TILESHIFT));
			textureadjust=viewx;
			baseangle+=TANANGLES *2;
			floorheight=northbottom[mapspot];
			break;
		case 1:                         // west facing wall
			distance=((tilex+1)<<(FRACBITS+TILESHIFT))-viewx;
			textureadjust=viewy;
			baseangle+=TANANGLES;
			floorheight=westbottom[mapspot+1];
			break;
		case 2:                         // north facing wall
			distance=((tiley+1)<<(FRACBITS+TILESHIFT))-viewy;
			textureadjust=-viewx;
			baseangle+=TANANGLES *2;
			floorheight=northbottom[mapspot+MAPSIZE];
			break;
		case 3:                         // east facing wall
			distance=viewx-(tilex<<(FRACBITS+TILESHIFT));
			textureadjust=-viewy;
			baseangle+=TANANGLES;
			floorheight=westbottom[mapspot];
			break;
	}
	//
	// the floor and ceiling height is the max of the points
	//
     ceilingheight = floorheight + *wall *4; // based on height of wall pic
    //ceilingheight = floorheight + (*wall)*4 *4;

	ceilingheight=(ceilingheight<<FRACBITS)-viewz;
	floorheight=-((floorheight<<FRACBITS)-viewz);   // distance below vi

	//
	// step through the individual posts
	//
	for (x=x1; x<=x2; x++) {
		angle=baseangle+pixelangle[x];
		angle&=TANANGLES *2-1;

		//
		// calculate the texture post along the wall that was hit
		//
		texture=(textureadjust+FIXEDMUL(distance, tangents[angle]))>>FRACBITS;

		texture&=63;
		if (x==x1 && texture==63)
			texture = 0;            // this is a hack to ensure two tile wide
		else if (x==x2 && !texture)
			texture = 63;           // pics don't wrap inside their wall segments


		sp_source=postindex[texture];

		//
		// the z distance of the post hit = walldistance*cos(screenangle
		//
		anglecos=cosines[(angle-TANANGLES)&(TANANGLES *4-1)];
		pointz=FIXEDDIV(distance, anglecos);
		pointz=FIXEDMUL(pointz, pixelcosine[x]);

		if (pointz>MAXZ)
			continue;

		// test to adjust shadowing

		//sp_colormap=zcolormap[pointz>>FRACBITS];
		if (pointz > MAXZ)
		  continue;
		sp_colormap=zcolormap[(pointz>>FRACBITS)];
//if (*wall > 45)
//sp_colormap=zcolormap[0];

		//
		// calculate the size and scale of the post
		// the scale is calculated a little smaller than true to guarant
		// any adjacent floors / ceilings
		//
		sp_fracstep=FIXEDMUL(pointz, ISCALEFUDGE);

		top=FIXEDDIV(ceilingheight, sp_fracstep);
		topy=top>>FRACBITS;
		fracadjust=top&(FRACUNIT-1);
		sp_frac=FIXEDMUL(fracadjust, sp_fracstep);
		topy=CENTERY-topy;

		walltopy[x]=topy;

		if (topy<0) {
			sp_frac-=topy *sp_fracstep;
			topy=0;
		}
		bottom=FIXEDDIV(floorheight, sp_fracstep)+FRACUNIT;
		bottomy=bottom>=(CENTERY<<FRACBITS)?windowHeight-1:CENTERY+(bottom>>FRACBITS);

		if ((bottomy<0)||(topy>=windowHeight))
			continue;

		sp_count=bottomy-topy+1;
		sp_dest=viewylookup[bottomy]+x;

		ScalePost();

		wallz[x]=pointz;
		wallnumber[x]=wallnum;
		walltexture[x]=texture;
	}
}
