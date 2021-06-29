// R_public.h

#include "viewsize.h"

extern int windowHeight;
extern int windowWidth;
extern int viewLocation;
extern int windowSize;
extern int windowLeft;
extern int windowTop;

//=================
// CONSTANTS
//=================

#define WALL_CONTACT            1
#define DOOR_CONTACT            2

#define FRACBITS                16
#define FRACUNIT                (1<<FRACBITS)
#define TILEUNIT                (64*FRACUNIT)
#define PI                                      3.14159265
#define MAPSIZE         64              // there must not be any 65th vertexes
#define MAPROWS         64
#define MAPCOLS         64

#define TILESHIFT               6
#define TILESIZE                (1<<TILESHIFT)  // pixels to tile
#define TILEFRACSHIFT           (TILESHIFT+FRACBITS)
#define TILEGLOBAL              (1<<TILEFRACSHIFT)
#define ANGLES                  256
#define CENTERX                 (windowWidth/2)
#define CENTERY                 (windowHeight/2)
#define MINZ                    (FRACUNIT/4)

#define MAXZ                    (6<<(FRACBITS+TILESHIFT))
#define MAXZ_LITUP              (9<<(FRACBITS+TILESHIFT))
#define MAXDOORS                64
#define MAXSPRITES      256


// TML - Flags for animation field of scaleobj
#define ANIM_LOOP_MASK    1
#define ANIM_CG_MASK      30
#define ANIM_MG_MASK      480
#define ANIM_DELAY_MASK   32256
//

//
// flags in mapflags
//
#define FL_DOOR         0x80
#define FL_FLOOR                7
#define FL_CEILING      56
#define FL_AUX                  64      // not used right now

#define FLS_FLOOR               0
#define FLS_CEILING     3
#define POLY_FLAT               0
#define POLY_SLOPE      1
#define POLY_ULTOLR     2
#define POLY_URTOLL     3
// additional POLY_??? can be defined from 4-7



//=================
// TYPES
//=================

typedef byte pixel_t;
typedef int fixed_t;

typedef enum  {
        dt_high, dt_medium
} detail_t;

typedef enum  {
        rt_one, rt_four, rt_eight
} rotate_t;

typedef struct scaleobj_s {
        //
        // list links, don't touch
        //
        struct scaleobj_s *prev, *next;
        //
        // modify this part whenever you want
        int animation;
        int  animationTime; // must accept all possible tick values
        byte  hitPoints;
        fixed_t moveSpeed;
        //
        fixed_t x, y, z;        // global position of the BOTTOM of the shap
        int basepic;                    // lumpnum is spritelump+basepic+rotation
        rotate_t rotate;
        int angle;                              // 0 - ANGLES-1
        void *userinfo;
} scaleobj_t;

typedef enum  {
        dr_horizontal, dr_vertical
} orientation_t;

typedef struct doorobj_s {
        int tilex, tiley;
        //
        // modify this part whenever you want
        //
        // Added 9-14-94 by Todd M. Lewis
        boolean doorOpen;
        boolean doorOpening;
        boolean doorClosing;
        boolean doorBlocked;
        boolean doorBumpable;
        int doorSize;
        int doorTimer;
        byte doorLocks;
        //
        //
        orientation_t orientation;      // probably only want to set this once
        boolean transparent;                            // set true if the pic has any masked areas
        int pic;                                                                                // lumpnum is doorlump+pic
        int height;                                                                     // should generally be set to the floor height
        fixed_t position;                                               // range from 0 (open) - FRACUNIT*64 (closed
        void *userinfo;
} doorobj_t;

typedef enum  {
        id_empty, id_wall, id_floor, id_ceiling, id_door, id_sprite
} hit_t;

//=================
// VARIABLES
//=================
extern int frameon;
extern int actionflag;          // if set non 0, the action hook is called

extern pixel_t viewbuffer[MAX_VIEW_WIDTH*MAX_VIEW_HEIGHT];
extern pixel_t *viewylookup[MAX_VIEW_HEIGHT];

extern int spritelump, walllump, flatlump;
extern int numsprites, numwalls, numflats;

extern int *flattranslation;
extern int *walltranslation;

extern fixed_t costable[ANGLES];
extern fixed_t sintable[ANGLES];

extern byte floorpic[MAPROWS*MAPCOLS];
extern byte ceilingpic[MAPROWS*MAPCOLS];
extern byte floorheight[MAPROWS*MAPCOLS];
extern byte ceilingheight[MAPROWS*MAPCOLS];
extern byte northwall[MAPROWS*MAPCOLS];
extern byte westwall[MAPROWS*MAPCOLS];
extern byte northbottom[MAPROWS*MAPCOLS];
extern byte westbottom[MAPROWS*MAPCOLS];
extern byte mapflags[MAPROWS*MAPCOLS];
extern byte mapsprites[MAPROWS*MAPCOLS];

// id_?? variables are filled in after a call to RF_PixelIdentity (sx,sy
extern hit_t id_type;
extern int id_tilex, id_tiley;  // not valid for sprites
extern int id_side;                     // 0(north)-3(west) for walls
extern int id_px, id_py;        // position in texture
extern void *id_structure;      // either doorobj_t or scaleobj_t

//=================
// PUBLIC FUNCTIONS
//=================
fixed_t FIXEDMUL(fixed_t x, fixed_t y);
fixed_t FIXEDDIV(fixed_t x, fixed_t y);

void RF_Startup(void);
void RF_ClearWorld(void);
doorobj_t *RF_GetDoor(int tilex, int tiley);
scaleobj_t *RF_GetSprite(void);
void RF_RemoveSprite(scaleobj_t *spr);
fixed_t RF_GetFloorZ(fixed_t x, fixed_t y);
fixed_t RF_GetCeilingZ(fixed_t x, fixed_t y);
void RF_PixelIdentity(int sx, int sy);
void RF_SetLights(int intensity);
void RF_SetActionHook(void (*hook)(void));
void RF_CheckActionFlag(void);
void RF_RenderView(fixed_t x, fixed_t y, fixed_t z, int angle, byte showBlast);
void RF_BlitView(void);
void SetViewSize(int width, int height);
