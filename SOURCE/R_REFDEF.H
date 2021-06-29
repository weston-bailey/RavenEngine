// R_refdef.h

#include "R_public.h"

#define VALIDATE

#ifdef NeXT

#define FLOATCOORD

#endif

#define   rint(x)   (int)(x+0.5)
void MS_Error(char *error, ...);

//=================
// CONSTANTS
//=================

#define   TANANGLES      2048 // one quadrant
#define   FINESHIFT      5
#define   MAXVISVERTEXES 128  // max tile corners visible at once
#define   INTEGRALDISTS  128
#define   SCALE               ((windowWidth/2)<<FRACBITS)
#define   ISCALE              (FRACUNIT/(windowWidth/2))
#define   ISCALEFUDGE    (FRACUNIT/(windowWidth/2+5))

//
// for spans
//
#define   MAXSPANS  4096
#define ZSHIFT      21 // tml 9-26-94; shifted 1 left to increase x size
#define ZTOFRAC          7         // to shift the Z into frac position
#define ZMASK       (0xfff<<ZSHIFT)
#define CEILINGBIT  (1<<20) // tml; 9-26; shifted to increase x size
#define XSHIFT      11
#define XMASK       (0x1ff<<XSHIFT) // tml; 9-26-94
#define SPANMASK    0x000007ff

//=================
// TYPES
//=================

typedef struct  {
     fixed_t floorheight;
     fixed_t ceilingheight;

     fixed_t tx, tz;               // transformed x / distance

     int px;                       // projected x if tz > 0
     int floory;
     int ceilingy;
#ifdef FLOATCOORD
     float ftx, ftz;
#endif
} vertex_t;

typedef struct  {
     int tilex, tiley;
     int xmin, xmax;
} entry_t;

/*                                                                                        */
/*The span_t structure is used with the spantags bit fields to determine*/
/*after sorting all the draw events.                                            */
/*                                                                                        */
/*        floor/ceiling       doorpost            scaled shape        slop*/
/*        ---------------     ------------            -----------------   */
/*tagz         pointz              pointz              pointz                     p*/
/*tagx         ~xstart             ~xposition          0                           */
/*                                                                                        */
/*picture texture block       top of post         scale pic               */
/*x2      x end + 1           ??                  pointx                  */
/*y       screen y            ceiling height      pointy                  */
/*yh      ??                  floor height        ??                      */
/*structure    ??                  doorobj             scaleobj                 ??   */
/*                                                                                        */
/*                                                                                        */

typedef enum  {
     sp_flat, sp_slope, sp_door, sp_shape, sp_maskeddoor
} spanobj_t;

typedef struct  {
     spanobj_t spantype;
     byte *picture;
     void *structure;         // either doorobj or scaleobj
     int x2;
     int y;
     int yh;
} span_t;

typedef struct  {
     short leftoffset, width;
     short collumnofs[256];   // only uses [width] entries
} scalepic_t;

typedef struct  {
     fixed_t tx, ty, tz;
     int px, py;
} clippoint_t;

//=================
// R_render
//=================
extern void (*actionhook)(void);

extern int framevalid[MAPROWS][MAPCOLS];
extern vertex_t *cornervertex[MAPROWS][MAPCOLS];
extern vertex_t vertexlist[MAXVISVERTEXES], *vertexlist_p;

extern int yslope[MAX_VIEW_HEIGHT], xslope[MAX_VIEW_WIDTH+1];

extern byte **wallposts;

extern byte *colormaps;
extern int numcolormaps;
extern byte *zcolormap[(MAXZ>>FRACBITS)
+1];

extern fixed_t viewx, viewy, viewz;
extern fixed_t viewcos, viewsin;
extern fixed_t xscale, yscale;
extern int viewangle, viewfineangle;
extern int viewtilex, viewtiley;

extern int side;
extern int walltype;
extern int wallshadow;
extern vertex_t *vertex[4];   // points to the for corner vertexes in vert
extern vertex_t *p1, *p2;

extern int xclipl, xcliph;    // clip window for current tile
extern int tilex, tiley; // coordinates of the tile being rendered
extern int mapspot;           // tiley*MAPSIZE+tilex

extern    boolean doortile;        // true if the tile being renderd has a door

void SetupFrame(void);
vertex_t *TransformVertex(int tilex, int tiley);
void FlowView(fixed_t x, fixed_t y);

//=================
// R_walls
//=================

extern fixed_t tangents[TANANGLES *2];
extern fixed_t sines[TANANGLES *5];
extern fixed_t *cosines; // point 1/4 phase into sines

extern int pixelangle[MAX_VIEW_WIDTH+1];     // +1 because span ends go one past
extern fixed_t pixelcosine[MAX_VIEW_WIDTH+1];

extern int wallnumber[MAX_VIEW_WIDTH];
extern int walltopy[MAX_VIEW_WIDTH];
extern int walltexture[MAX_VIEW_WIDTH];
extern fixed_t wallz[MAX_VIEW_WIDTH];

void InitWalls(void);
void ClearWalls(void);
void RenderTileWalls(entry_t *e, fixed_t playerx, fixed_t playery);
void DrawWall(void);

//=================
// R_plane
//=================

extern byte *mr_picture; // pointer to a raw 64*64 pixel picture
extern fixed_t mf_deltaheight;
extern int spancount;

void InitPlane(void);
void ClearMapCache(void);
void RenderTileEnds(void);
void FindBackVertex(void);

//=================
// R_conten
//=================

extern scaleobj_t firstscaleobj, lastscaleobj;
extern scaleobj_t scaleobjlist[MAXSPRITES], *freescaleobj_p;

extern doorobj_t doorlist[MAXDOORS];
extern int numdoors;

extern int doorxl, doorxh;

void RenderDoor(fixed_t playerx, fixed_t playery);
void RenderSprites(fixed_t x, fixed_t y, fixed_t z, int angle, byte showBlast);

//=================
// R_draw
//=================

extern byte *sp_dest;         // the bottom most pixel to be drawn (in vie
extern byte *sp_source;       // the first pixel in the vertical post (may
extern byte *sp_colormap;     // pointer to a 256 byte color number to pal
extern int sp_frac;           // fixed point location past sp_source
extern int sp_fracstep;       // fixed point step value
extern int sp_count;          // the number of pixels to draw

void ScalePost(void);
void ScaleMaskedPost(void);

extern byte *mr_dest;         // the left most pixel to be drawn (in viewb
extern byte *mr_picture; // pointer to a raw 64*64 pixel picture
extern byte *mr_colormap;     // pointer to a 256 byte color number to pal
extern int mr_xfrac;          // starting texture coordinate
extern int mr_yfrac;          // starting texture coordinate
extern int mr_xstep;          // fixed point step value
extern int mr_ystep;          // fixed point step value
extern int mr_count;          // the number of pixels to draw

void MapRow(void);

//=================
// R_spans
//=================

extern unsigned spantags[2][MAXSPANS];
extern unsigned *starttaglist_p, *endtaglist_p;
extern span_t spans[MAXSPANS], *spans_p;
extern int numspans;

void DrawSpans(void);
