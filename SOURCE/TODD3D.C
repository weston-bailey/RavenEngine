// todd3d.c

/**************************************************************************\
**                                                                        **
**  Todd Michael Lewis                                                    **
**  Raven Engine Updates                                                  **
**------------------------------------------------------------------------**
**                                                                        **
**  September 19, 1994                                                    **
**    1) Enabled door checking whereby door is detected at its actual     **
**       recessed position                                                **
**    2) Enabled door opening with key click while in recessed position   **
**    3) Enabled coding doors to be openable only with certain items      **
**    4) Enabled doors to be opened by bumping or pushing                 **
**                                                                        **
**  September 20, 1994                                                    **
**    1) Learned how to draw planar bitmaps on screen (for status         **
**       updates and text, etc.)                                          **
**    2) Learned how to update view buffer before display (allows the     **
**       display of weapons in foreground)                                **
**                                                                        **
**  September 21, 1994                                                    **
**    1) Jon optimized routine for blasting bitmaps to viewbuffer which   ** 
**       resulted in a 39% increase in efficiency                         **
**                                                                        **
**  September 22, 1994                                                    **
**    1) Succesfully changed map then deciphered map structure            **
**    2) Added new tiles succesfully to engine                            **
**    3) Discovered door closing problem (would only close if in current  **                                                                    
**       frame rendering); fixed to check for door close separately       **
**    4) Successfully placed mulitiple and differing sprites              **
**                                                                        **
**  September 23, 1994                                                    **
**    1) Enabled sprite animation                                         **
**                                                                        **
**  September 26, 1994                                                    **
**    1) Added hit and death animations to sprites                        **
**                                                                        **
**  September 27, 1994                                                    **
**    1) Enabled successful rendering of walls and ceilings in widths     **
**       above 256                                                        **
**                                                                        **
**  September 28, 1994                                                    **
**    1) Enable successful rendering of doors in widths above 256         **
**                                                                        **
**  November 21-27, 1994                                                  **
**    1) Incredible progress with new map editor                          **
**    2) Enabled placing of player from map                               **
**                                                                        **
\**************************************************************************/

#include <CTYPE.H>
#include <IO.H>
#include <DOS.H>
#include <TIME.H>
#include <STDIO.H>
#include <STDLIB.H>
#include <STRING.H>
#include <BIOS.H>
#include <FCNTL.H>
#include <MATH.H>
#include <I32.H>

#include "d_global.h"
#include "d_disk.h"
#include "d_misc.h"
#include "d_video.h"
#include "d_ints.h"
#include "d_font.h"

#include "r_refdef.h"

#define PLAYERMOVESPEED FRACUNIT
#define PLAYERTURNSPEED 1

#define MINDIST         (FRACUNIT*4)
#define PLAYERSIZE      MINDIST// almost a half tile
#define FRACTILESHIFT   (FRACBITS+TILESHIFT)

#define         RED_KEY      1
#define         GREEN_KEY    2
#define         YELLOW_KEY   4
#define         BLUE_KEY     8


fixed_t         playerx;
fixed_t         playery;
fixed_t         playerz;
int             playerangle;          // 360 degree range spread from 0-ANGLES
int             player_keys = 0; 

int             tics;

byte            suspendRefresh = 0;
byte            resizeScreen = 0;
byte            biggerScreen = 0;
byte            showBlast = 0;
byte            warpActive = 0;
boolean         menuActive = false;
byte            menuValue = 0;
int             keyboardDelay = 0;
int             weaponFireDelay = 0;

int             viewSizes[] =
{     
  96,  56,
  128, 72,
  160, 88,
  192, 104,
  224, 120,
  256, 136,
  288, 152,
  320, 168,
  320, 200
};
/*
int             viewSizes[] =
{     
  320, 200,
  320, 200,
  320, 200,
  320, 200,
  320, 200,
  320, 200,
  320, 200,
  320, 200,
  320, 200
};
  */

byte            currentViewSize = 8;


int myArray[500];
int myOffset = 0;

void ChangeViewSize(byte MakeLarger);
void ResetScalePostWidth (int NewWindowWidth);
void FillRectangle (int x, int y, int width, int height, byte color);
extern void *GetMScaleRoutines (void);
extern void *GetScaleRoutines (void);


/*
=============================================================================


=============================================================================
*/

/*
===================
=
= TryDoor
= Todd Lewis - 9/13/94
=
===================
*/

boolean TryDoor (fixed_t xcenter, fixed_t ycenter, boolean doorCheck)
{
     int             xl,yl,xh,yh,x,y;
     struct cellStruct *cs;
     doorobj_t       *door_p, *last_p;
     int     dx,dy;

// These values will probably have to be tweaked for doors that are along
// the vertical opposite axis (northwall)

     xl = (int)((xcenter-PLAYERSIZE) >> FRACTILESHIFT);
     yl = (int)((ycenter-PLAYERSIZE - (TILEUNIT >> 1)) >> FRACTILESHIFT);

     xh = (int)((xcenter+PLAYERSIZE) >> FRACTILESHIFT);
     yh = (int)((ycenter+PLAYERSIZE - (TILEUNIT >> 1)) >> FRACTILESHIFT);

//
// check for doors on the north wall
//
     for (y=yl+1;y<=yh;y++)
	  for (x=xl;x<=xh;x++)
	  {
	 if (mapflags[y*64+x] & FL_DOOR) // if tile has a door
	 {
		   last_p=&doorlist[numdoors];

		   for (door_p=doorlist; door_p != last_p; door_p++) 
		   {
	       if ((door_p->tilex == x) &&
		   (door_p->tiley == y))
		       {
		 if (door_p->orientation != dr_horizontal)
		   continue;
		 if ((door_p->doorOpen) &&
		     (!door_p->doorClosing))
		   {
		     return true; // can move, door is open
		   }
		 else if ((!door_p->doorOpen) &&
		      (door_p->doorBumpable))
		   {
		     door_p->doorOpening = true;
		     return false;
		   }
		 else if ((!door_p->doorOpen) ||
			  (door_p->doorClosing))
		   return false;
		       }
	   }
	 }
	  }

// check for doors on the west wall

     xl = (int)((xcenter-PLAYERSIZE - (TILEUNIT >> 1)) >> FRACTILESHIFT);
     yl = (int)((ycenter-PLAYERSIZE) >> FRACTILESHIFT);

     xh = (int)((xcenter+PLAYERSIZE - (TILEUNIT >> 1)) >> FRACTILESHIFT);
     yh = (int)((ycenter+PLAYERSIZE) >> FRACTILESHIFT);


     for (y=yl;y<=yh;y++)
	  for (x=xl+1;x<=xh;x++)
	  {
	       if (mapflags[y*64+x] & FL_DOOR) // if tile has a door
	       {
		   last_p=&doorlist[numdoors];

		   for (door_p=doorlist; door_p != last_p; door_p++) 
		   {
	       if ((door_p->tilex == x) &&
		   (door_p->tiley == y))
		       {
		 if (door_p->orientation != dr_vertical)
		   continue;
		 if ((door_p->doorOpen) &&
		     (!door_p->doorClosing))
		 {
			       return true; // can move, door is open
		 }
		 else if ((!door_p->doorOpen) &&
		      (door_p->doorBumpable))
		 {
			       door_p->doorOpening = true;
			       return false;
		 }
		 else if ((!door_p->doorOpen) ||
			  (door_p->doorClosing))
		   return false;
		       }
		   }
	 }
	  }

     return true;

}

/*
===================
=
= TryMove
=
= returns true if move doesn't go into a solid wall
=
===================
*/

boolean TryMove (fixed_t xcenter, fixed_t ycenter)
{
     int             xl,yl,xh,yh,x,y;
     struct cellStruct *cs;
     doorobj_t       *doorobj;
     int     dx,dy;

     xl = (int)((xcenter-PLAYERSIZE) >> FRACTILESHIFT);
     yl = (int)((ycenter-PLAYERSIZE) >> FRACTILESHIFT);

     xh = (int)((xcenter+PLAYERSIZE) >> FRACTILESHIFT);
     yh = (int)((ycenter+PLAYERSIZE) >> FRACTILESHIFT);
//
// check for solid walls
//
     for (y=yl+1;y<=yh;y++)
	  for (x=xl;x<=xh;x++)
	  {
	       if (northwall[y*64+x]) // if tile has a wall
		    return false;
	  }

     for (y=yl;y<=yh;y++)
	  for (x=xl+1;x<=xh;x++)
	  {
	       if (westwall[y*64+x]) // if tile has a wall
		    return false;
	  }

     return true;
}

//==========================================================================

/*
===================
=
= ClipMove
=
===================
*/

byte ClipMove (fixed_t xmove, fixed_t ymove, fixed_t *x, fixed_t *y)
{
   boolean canMove;
   
   canMove = (TryMove(*x+xmove,*y+ymove) & 
	      TryDoor(*x+xmove,*y+ymove,false));
  
     if (canMove)
     {
	  *x += xmove;
	  *y += ymove;
	  return 1;
     }

//
// the move goes into a wall, so try and move along one axis
//
     canMove = (TryMove(*x+xmove,*y) &
		TryDoor(*x+xmove,*y,false));

     if (canMove)
     {
	  *x += xmove;
	  return 2;
     }

     canMove = (TryMove(*x,*y+ymove) &
		TryDoor(*x,*y+ymove,false));

     if (canMove)
     {
	  *y += ymove;
	  return 3;
     }

     return 0;
}

//==========================================================================

/*
===================
=
= Thrust
=
= Angle is an 8 bit value, speed is a global pixel value
= Tries to move the player in the given direction, clipping to walls
= if blocked
=
===================
*/

byte Thrust (int angle, fixed_t speed, fixed_t *x, fixed_t *y)
{
     fixed_t  xmove,ymove;

     angle &= (ANGLES-1);

     xmove = FIXEDMUL(speed,costable[angle]);
     ymove = -FIXEDMUL(speed,sintable[angle]);

     return ClipMove(xmove,ymove,x,y);
}

void MoveSprites(void)
{
     scaleobj_t *sprite;
     scaleobj_t *holdSprite;
     int picnum;
     byte counter;

     for (sprite=firstscaleobj.next; sprite!=&lastscaleobj;
	  sprite=sprite->next) 
     {
	 if (sprite->moveSpeed)
	 {
	   counter = 0;
	   while (counter++ < sprite->moveSpeed)
	   {
	     if (Thrust(sprite->angle, PLAYERMOVESPEED,
			&sprite->x, &sprite->y) != 1)
	     {
	       holdSprite = sprite;
	       sprite = sprite->prev;
	       RF_RemoveSprite(holdSprite);
	       counter = sprite->moveSpeed;
	     }
	   } // while
	 } // if
     }
}


boolean FindWarpDestination(int *x, int *y, byte warpValue)
{
  int searchX, searchY;

  if (!warpActive)
  {
    for (searchX = 0; searchX < 64; searchX ++)
      for (searchY = 0; searchY < 64; searchY ++)
	if ((mapsprites[searchY*64+searchX] == warpValue) &&
	   ((searchX != *x) || (searchY != *y)))
	{
	  *x = searchX;
	  *y = searchY;
	  warpActive = warpValue;

	  return true;
	} // if
  } // if

  return false;
} // FindDestinationWarp

void CheckWarps (int centerx, int centery)
{
     int x, y;

     x = (int)((centerx) >> FRACTILESHIFT);
     y = (int)((centery) >> FRACTILESHIFT);

     if ((mapsprites[y*64+x] >= 6) &&
	 (mapsprites[y*64+x] <= 8))
     {
	if (FindWarpDestination(&x, &y, mapsprites[y*64+x]))
	{
	   playerx = (x*64+32)*FRACUNIT;
	   playery = (y*64+32)*FRACUNIT;
	} // if
     } // if 
     else
       warpActive = 0;

} // CheckWarps

void CheckHere (int centerx, int centery, boolean openTheDoor)
{
     // check for door at centerx,centery
     int x, y;
     doorobj_t       *door_p, *last_p;     

     x = (int)((centerx) >> FRACTILESHIFT);
     y = (int)((centery) >> FRACTILESHIFT);

     if (mapflags[y*64+x] & FL_DOOR) // if tile has a door
     {
	 last_p=&doorlist[numdoors];

	 for (door_p=doorlist; door_p != last_p; door_p++) 
	 {
	     if ((door_p->tilex == x) &&
		 (door_p->tiley == y))
	     {
		 if ((!door_p->doorOpen) && (openTheDoor))// &&
			  //(player_keys & door_p->doorLocks))
		     door_p->doorOpening = true;
	     }
	 }
     }
}

void CheckSprites(void)
{
//
// Added 9-24-94 - Todd Lewis
//
   scaleobj_t *sprite;

   for (sprite=firstscaleobj.next; sprite!=&lastscaleobj;
	sprite=sprite->next) 
   {
   }
}

void CheckDoors(fixed_t centerx, fixed_t centery)
{
//
// Added 9-24-94 - Todd Lewis
//
     int x,y;
     doorobj_t       *door_p, *last_p;

     x = (int)((centerx) >> FRACTILESHIFT);
     y = (int)((centery) >> FRACTILESHIFT);

     last_p=&doorlist[numdoors];

     if ((door_p->tilex == x) && (door_p->tiley == y))
     {
	 if ((door_p->doorOpen) && (!door_p->doorClosing))
	     door_p->doorBlocked = true;
     }
     else
	 door_p->doorBlocked = false;

     for (door_p=doorlist; door_p != last_p; door_p++) 
     {
	 if ((door_p->tilex == x) &&
	     (door_p->tiley == y))
	 {
	     if ((door_p->doorOpen) &&
		 (!door_p->doorClosing))
		 door_p->doorBlocked = true;  
	 }     
	 else
	     door_p->doorBlocked = false;

	door_p->position = door_p->doorSize * FRACUNIT;

	if (door_p->doorOpening)
	{
      door_p->doorSize = door_p->doorSize - 4;
	  if (door_p->doorSize < 0)
	  {
	    door_p->doorSize = 0;
	    door_p->doorOpening = false;
	    door_p->doorOpen = true;
	    door_p->doorTimer = timecount + 350;
	  }
	}
	else if (door_p->doorClosing)
	{
	  door_p->doorSize = door_p->doorSize + 2;
	  if (door_p->doorSize > 64)
	  {
	    door_p->doorSize = 64;
	    door_p->doorClosing = false;
	    door_p->doorOpen = false;
	  }
	}
	else if (door_p->doorOpen)
	{
	  if (timecount > door_p->doorTimer)
	  {
	    if (!door_p->doorBlocked)
	      door_p->doorClosing = true;
	  }
	}
     }
} // CheckDoors

void SpawnSprite(byte value, int x, int y)
{
    scaleobj_t *sprite_p; 
    doorobj_t  *door_p;


	switch (value)
	{
	  case 1:
	    sprite_p = RF_GetSprite ();
	    sprite_p->animation = 0;
	    sprite_p->moveSpeed = 0;
	    sprite_p->angle = 0;
	    sprite_p->x = (x*64+32)*FRACUNIT;
	    sprite_p->y = (y*64+32)*FRACUNIT;
	    sprite_p->z = 0;
	    sprite_p->rotate = rt_one;
	    sprite_p->basepic = CA_GetNamedNum ("chacmool"); 
	  break;

	  case 2:
	    sprite_p = RF_GetSprite ();
	    sprite_p->animation = 0;
	    sprite_p->x = (x*64+32)*FRACUNIT;
	    sprite_p->y = (y*64+32)*FRACUNIT;
	    sprite_p->z = 0;
	    sprite_p->rotate = rt_one;
	    sprite_p->basepic = CA_GetNamedNum ("brazier"); 
	  break;

	  case 3:   
	    playerx = (x*64+32)*FRACUNIT;
	    playery = (y*64+32)*FRACUNIT;
	  break;

	  case 4: // door (temporarily)
	    door_p = RF_GetDoor (x,y);
	    door_p->orientation = dr_horizontal;
	    door_p->doorOpen = false;
	    door_p->doorOpening = false;
	    door_p->doorClosing = false;
	    door_p->doorBlocked = false;
	    door_p->doorBumpable = false;
	    door_p->doorSize = 64;
	    door_p->position = door_p->doorSize*FRACUNIT;
	    door_p->doorLocks = 0;
	    door_p->transparent = true;
	    door_p->pic = CA_GetNamedNum ("wolfdoor") - walllump;
	  break;

	  case 5:
	    sprite_p = RF_GetSprite ();
	    sprite_p->animation = 0;
	    sprite_p->x = (x*64+32)*FRACUNIT;
	    sprite_p->y = (y*64+32)*FRACUNIT;
	    sprite_p->z = 0;
	    sprite_p->rotate = rt_one;
	    sprite_p->basepic = CA_GetNamedNum ("snake"); 
	  break;

	  case 6:
	    
	  break;

	  case 9:
	    sprite_p = RF_GetSprite ();
	    sprite_p->animation = 0;
	    sprite_p->angle = 0;
	    sprite_p->x = (x*64+32)*FRACUNIT;
	    sprite_p->y = (y*64+32)*FRACUNIT;
	    sprite_p->z = 0;
	    sprite_p->rotate = rt_one;
	    sprite_p->basepic = CA_GetNamedNum ("urn1"); 
	  break;

	  case 10:
	    sprite_p = RF_GetSprite ();
	    sprite_p->animation = 0;
	    sprite_p->angle = 0;
	    sprite_p->x = (x*64+32)*FRACUNIT;
	    sprite_p->y = (y*64+32)*FRACUNIT;
	    sprite_p->z = 0;
	    sprite_p->rotate = rt_one;
	    sprite_p->basepic = CA_GetNamedNum ("urn2"); 
	  break;

	  case 11:
	    sprite_p = RF_GetSprite ();
	    sprite_p->animation = 0;
	    sprite_p->angle = 0;
	    sprite_p->x = (x*64+32)*FRACUNIT;
	    sprite_p->y = (y*64+32)*FRACUNIT;
	    sprite_p->z = 0;
	    sprite_p->rotate = rt_one;
	    sprite_p->basepic = CA_GetNamedNum ("bowl"); 
	  break;

	  case 12:
	    sprite_p = RF_GetSprite ();
	    sprite_p->animation = 0;
	    sprite_p->angle = 0;
	    sprite_p->x = (x*64+32)*FRACUNIT;
	    sprite_p->y = (y*64+32)*FRACUNIT;
	    sprite_p->z = 0;
	    sprite_p->rotate = rt_one;
	    sprite_p->basepic = CA_GetNamedNum ("cup"); 
	  break;

	  case 13:
	    sprite_p = RF_GetSprite ();
	    sprite_p->animation = 0;
	    sprite_p->angle = 0;
	    sprite_p->x = (x*64+32)*FRACUNIT;
	    sprite_p->y = (y*64+32)*FRACUNIT;
	    sprite_p->z = 0;
	    sprite_p->rotate = rt_one;
	    sprite_p->basepic = CA_GetNamedNum ("jade1"); 
	 break;

	  case 14: // door (temporarily)
	    door_p = RF_GetDoor (x,y);
	    door_p->orientation = dr_vertical;
	    door_p->doorOpen = false;
	    door_p->doorOpening = false;
	    door_p->doorClosing = false;
	    door_p->doorBlocked = false;
	    door_p->doorBumpable = false;
	    door_p->doorSize = 64;
	    door_p->position = door_p->doorSize*FRACUNIT;
	    door_p->doorLocks = 0;
	    door_p->transparent = true;
	    door_p->pic = CA_GetNamedNum ("wolfdoor") - walllump;
	  break;

	  case 15:
	    sprite_p = RF_GetSprite ();
	    sprite_p->animation = 0;
	    sprite_p->angle = 0;
	    sprite_p->x = (x*64+32)*FRACUNIT;
	    sprite_p->y = (y*64+32)*FRACUNIT;
	    sprite_p->z = 0;
	    sprite_p->rotate = rt_one;
	    sprite_p->basepic = CA_GetNamedNum ("ankh"); 
	 break;


	} // switch

} // SpawnSprite

void ActivateSpritesFromMap(void)
{
  int          x, y;


  for (x=0; x<64; x++)
    for (y=0; y<64; y++)
    {
      if (mapsprites[y*64+x])  // sprite or player at this position
	SpawnSprite(mapsprites[y*64+x], x, y);
    } // for
} // ActivateSpritesFromMap


//==========================================================================

/*
=======================
=
= ControlMovement
=
= Takes controlx,controly, and buttonstate[bt_strafe]
=
= Changes the player's angle and position
=
=======================
*/

void ControlMovement (void)
{
     fixed_t modifiedSpeed;
     int     modifiedTurn;
     scaleobj_t   *sprite_p;  


     if (keyboard[SC_ESCAPE])
     {
       MS_Error("Badee, badee, badee ... that's all folks!");
       return;
     }

     if ((keyboard[SC_INSERT]) && (!resizeScreen))
     {
       if (timecount > keyboardDelay)
       {
	 //ChangeViewSize(true);
	 resizeScreen = 1;
	 biggerScreen = true;
	 keyboardDelay = timecount + 10;
	 return;
       } // if
     }

     if ((keyboard[SC_DELETE]) && (!resizeScreen))
     {
       if (timecount > keyboardDelay)
       {
	 //ChangeViewSize(false);
	 resizeScreen = 1;
	 biggerScreen = false;
	 keyboardDelay = timecount + 10;
	 return;
       } // if
     }

// adjust movement speed for SLOW OR SPRINT ... TML 9-14-94
     if (keyboard[SC_RSHIFT])
     {
       modifiedTurn = 0;
       modifiedSpeed = PLAYERMOVESPEED/8;
     }
     else if (keyboard[SC_LSHIFT])
     {
       modifiedSpeed = PLAYERMOVESPEED*4;
       modifiedTurn  = PLAYERTURNSPEED*4;
     }
     else
     {
       modifiedSpeed = PLAYERMOVESPEED*2;
       modifiedTurn  = PLAYERTURNSPEED*2;
     }

// up / down movement

     if (keyboard[SC_PGUP] && playerz < 252*FRACUNIT)
	  playerz += FRACUNIT;
     if (keyboard[SC_PGDN] && playerz > 4*FRACUNIT)
	  playerz -= FRACUNIT;
     if (keyboard[SC_HOME])
	  playerz = 32*FRACUNIT;

// side to side move

     if (keyboard[SC_ALT])
     {
     //
     // strafing
     //
	  if (keyboard[SC_LEFTARROW])
	       Thrust (playerangle+64,modifiedSpeed,&playerx,&playery);
	  if (keyboard[SC_RIGHTARROW])
	       Thrust (playerangle-64,modifiedSpeed,&playerx,&playery);
     }
     else
     {
     //
     // not strafing
     //
	  if (keyboard[SC_RIGHTARROW])
	       playerangle -= modifiedTurn;
	  if (keyboard[SC_LEFTARROW])
	       playerangle += modifiedTurn;
     }
     playerangle &= (ANGLES-1);

//
// forward/backwards move
//
     if (keyboard[SC_UPARROW])
     {
	  Thrust (playerangle,modifiedSpeed,&playerx,&playery);
     }
     if (keyboard[SC_DOWNARROW])
     {
	  Thrust (playerangle+128,modifiedSpeed,&playerx,&playery);
     }

//
// try to open a door in front of player
//
     if (keyboard[SC_SPACE])
     {
	 CheckHere(playerx,playery,true);
     }

     if (keyboard[SC_CONTROL])
     {
	if (timecount > weaponFireDelay)
	{
	  sprite_p = RF_GetSprite ();
	  sprite_p->animation = 0;
	  sprite_p->animation = 1 + (4 << 5) + (5 << 9);
	  sprite_p->animationTime = 0;
	  sprite_p->moveSpeed = 20;
	  sprite_p->angle = playerangle;
	  sprite_p->x = playerx;
	  sprite_p->y = playery;
	  sprite_p->z = 16*FRACUNIT;
	  sprite_p->rotate = rt_one;
	  sprite_p->basepic = CA_GetNamedNum ("atk1"); 

	  weaponFireDelay = timecount + 18;
	} 
     }  

}

//==========================================================================

/*
================
=
= PlayerCommand
=
= Called by an interrupt, so if anything time consuming needs to be done,
= set actionflag to a non zero value and perform the action in the
= ActionHook function
=
================
*/

void PlayerCommand (void)
{
     IN_ReadControls ();
     ControlMovement ();
     tics++;
}


/*
================
=
= PlayLoop
=
================
*/

void PlayLoop (void)
{
     int myTime; // debug purposes

     char test[30];

     int  frametics;
     int  x;
     int i;    // get rid off after test

     tics = 0;
     INT_TimerHook (PlayerCommand,1);   // the players actions are
						  // sampled by an interrupt

     while (1)
     {
      MoveSprites();
      CheckDoors(playerx, playery);
      CheckWarps(playerx, playery);

      if (resizeScreen)
	ChangeViewSize(biggerScreen);

      RF_RenderView (playerx,playery,playerz,playerangle,showBlast);

      if (showBlast)
      {
     //   if (showBlast > 1)
     //     VI_DrawMaskedPicToBuffer ((windowWidth>>1) - 22,
     //       windowHeight - 62,
     //       CA_CacheLump (CA_GetNamedNum ("my_blast") + (3-showBlast)));
	showBlast = showBlast - 1;
      }

      //myTime = timecount;
	   
      //for (i=0;i<5000;i++)
      //VI_DrawMaskedPicToBuffer ((windowWidth>>1) - 64, windowHeight - 32,
      //  CA_CacheLump (CA_GetNamedNum ("weapons")));

      if (menuActive)
      { 
	//VI_DrawMaskedPicToBuffer ((windowWidth>>1) - 92, windowHeight - 131,
	//  CA_CacheLump (CA_GetNamedNum ("menu")));
	//VI_DrawMaskedPicToBuffer ((windowWidth>>1) - 92 + 26, 
	//  windowHeight - 131 + 25 + (menuValue*18), 
	//  CA_CacheLump (CA_GetNamedNum ("binky")));
      } // if


      //myTime = timecount - myTime;

       RF_BlitView ();

     }
}


/*
================
=
= ActionHook
=
= This routine is called by the refresh if actionflag is set, allowing
= time consuming functions (like loading something from disk) to be performed
= outside an the PlayerCommand function.
=
================
*/

void ActionHook (void)
{
     actionflag = 0;
}


/*
================
=
= LoadNewMap
=
================
*/

void LoadNewMap (void)
{
     int       lump;
     doorobj_t *door_p;
     scaleobj_t     *sprite_p;

     lump = CA_GetNamedNum("map");
     CA_ReadLump (lump,floorpic);
     CA_ReadLump (lump+1,ceilingpic);
     CA_ReadLump (lump+2,northwall);
     CA_ReadLump (lump+3,westwall);
     CA_ReadLump (lump+4,ceilingheight);
     CA_ReadLump (lump+5,floorheight);
     CA_ReadLump (lump+6,northbottom);
     CA_ReadLump (lump+7,westbottom);
     CA_ReadLump (lump+8,mapflags);
     CA_ReadLump (lump+9,mapsprites);

     // initialize to default in case not placed in mapsprites
     playerx = (3*64+32)*FRACUNIT;
     playery = (3*64+32)*FRACUNIT;
     playerz = 32*FRACUNIT;
     playerangle = 64; // face north to start
     
     ActivateSpritesFromMap();
}

/*
================
=
= main
=
================
*/

void main(int argc, char *argv[])
{
     int  i;
//
// start up the library
//
     //player_keys |= RED_KEY | YELLOW_KEY | GREEN_KEY | BLUE_KEY; 
     CA_InitFile ("TODD3D.TOD");

     VI_Init();
     VI_FillPalette (0,0,0);

     VI_DrawPic (0,0,CA_CacheLump (CA_GetNamedNum ("playscreen")));
     VI_FadeIn (0,255,CA_CacheLump(CA_GetNamedNum ("palette")),16);

     INT_Setup ();
     RF_Startup ();
     RF_SetActionHook (ActionHook);

//
// init the game
//
     LoadNewMap ();


//
// run the game
//
     PlayLoop ();
}

void ChangeViewSize(byte MakeLarger)
{
    resizeScreen = 0;

    if (MakeLarger)
    {
      if (currentViewSize < 8)
	currentViewSize++;
      else
	return;
    } // if
    else
    {
      if (currentViewSize > 0)
	currentViewSize--;
      else
	return;
    } // else

    FillRectangle(0, 0, 320, 200, 0);

    windowWidth = viewSizes[currentViewSize*2];
    windowHeight = viewSizes[currentViewSize*2+1];

    windowLeft = (320-windowWidth) >> 1;
    windowTop = (200-windowHeight) >> 1;

    windowSize = windowHeight*windowWidth;
    viewLocation = 0xA0000+windowTop*320+windowLeft;

    ResetScalePostWidth (windowWidth);
    InitWalls ();
    memset (framevalid, 0, sizeof (framevalid));
    frameon = 0;

    SetViewSize(windowWidth, windowHeight);
    RF_SetLights(MAXZ);
} // ChangeViewSize

void ResetScalePostWidth (int NewWindowWidth)
  {
   int i;
   int *iptr;
   byte *bptr;

   bptr = GetScaleRoutines ();
   for (i = MAX_VIEW_HEIGHT; i > 1; i--, bptr += 19)
     *(int *)(bptr+15) = (i - 1) * -NewWindowWidth;

   iptr = GetMScaleRoutines ();
   for (i = MAX_VIEW_HEIGHT; i > 1; i--, iptr += 6)
     *(iptr+5) = (i -1) * -NewWindowWidth;
  }
  
void FillRectangle (int x, int y, int width, int height, byte color)
  {
   int i;
   byte *bptr;

   bptr = (byte *) 0xA0000 + y * 320 + x;
   for (i = 0; i < height; i++, bptr += 320)
      memset (bptr,color,width);
  }
