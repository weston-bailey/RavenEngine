// d_ints.c

#include <dos.h>
#include <i32.h>
#include <stk.h>
#include <string.h>

#include "d_global.h"
#include "d_video.h"
#include "d_ints.h"
#include "d_misc.h"

#define TIMERINT                8
#define KEYBOARDINT             9

#define ISRCOLOR                190
#define FRAMEHOOKCOLOR  200
#define TIMERHOOKCOLOR  210

#define VBLCOUNTER              16000   // this could be a bit higher, but
								// i'm givving it some latency elbow room

void            (*oldkeyboardisr)() = 0;
void            (*oldtimerisr)() = 0;

//===================================

void            (*framehook)() = 0;                     // called every frame (cursor)
void            (*timerhook)() = 0;                     // called every other frame (player)

boolean         timeractive = false;

int                     timerspeed;                                     // 16 bit hardware counter value
int                     colorborder = 0;                        // if true, ISRs will color border
int                     timecount;

int                     framesperhook,framecounter;
int                     intsperframe,intcounter;
int                     oldbordercolor;
byte            *pendingscreen;

//===================================

boolean         keyboard[NUMCODES];
boolean         paused,capslock;
char            lastascii;
byte            lastscan;

byte        ASCIINames[] =              // Unshifted ASCII for scan codes
					{
//       0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
	0  ,27 ,'1','2','3','4','5','6','7','8','9','0','-','=',8  ,9  ,        // 0
	'q','w','e','r','t','y','u','i','o','p','[',']',13 ,0  ,'a','s',        // 1
	'd','f','g','h','j','k','l',';',39 ,'`',0  ,92 ,'z','x','c','v',        // 2
	'b','n','m',',','.','/',0  ,'*',0  ,' ',0  ,0  ,0  ,0  ,0  ,0  ,        // 3
	0  ,0  ,0  ,0  ,0  ,0  ,0  ,'7','8','9','-','4','5','6','+','1',        // 4
	'2','3','0',127,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,        // 5
	0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,        // 6
	0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0           // 7
					},
					 ShiftNames[] =         // Shifted ASCII for scan codes
					{
//       0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
	0  ,27 ,'!','@','#','$','%','^','&','*','(',')','_','+',8  ,9  ,        // 0
	'Q','W','E','R','T','Y','U','I','O','P','{','}',13 ,0  ,'A','S',        // 1
	'D','F','G','H','J','K','L',':',34 ,'~',0  ,'|','Z','X','C','V',        // 2
	'B','N','M','<','>','?',0  ,'*',0  ,' ',0  ,0  ,0  ,0  ,0  ,0  ,        // 3
	0  ,0  ,0  ,0  ,0  ,0  ,0  ,'7','8','9','-','4','5','6','+','1',        // 4
	'2','3','0',127,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,        // 5
	0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,        // 6
	0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0           // 7
					},
					SpecialNames[] =        // ASCII for 0xe0 prefixed codes
					{
//       0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
	0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,        // 0
	0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,13 ,0  ,0  ,0  ,        // 1
	0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,        // 2
	0  ,0  ,0  ,0  ,0  ,'/',0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,        // 3
	0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,        // 4
	0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,        // 5
	0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,        // 6
	0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0           // 7
					};


int     scanbuttons[NUMBUTTONS] =
{
	SC_UPARROW,                     // bt_north
	SC_RIGHTARROW,          // bt_east
	SC_DOWNARROW,           // bt_south
	SC_LEFTARROW,           // bt_west
	SC_CONTROL,                     // bt_a
	SC_ALT,                         // bt_b
	SC_SPACE,                       // bt_c
	SC_RSHIFT                       // bt_d
};


int     in_button[NUMBUTTONS];          // frames the button has been down



//===========================================================================



/*
================
=
= INT_KeyboardISR
=
================
*/

#pragma interrupt(INT_KeyboardISR)
void INT_KeyboardISR ()
{
static  boolean special;
	byte            k,c;
	unsigned        short   temp;
   _XSTACK  *ptr;                      /* ptr to the stack frame             */

// Get the scan code

	k = _inbyte((unsigned short)0x60);

// acknowledge the interrupt

	_outbyte((unsigned short)0x20,(unsigned char)0x20);
	if (k == 0xe0)          // Special key prefix
		special = true;
	else if (k == 0xe1)     // Handle Pause key
		paused = true;
	else
	{
		if (k & 0x80)   // Break code
		{
			k &= 0x7f;
			keyboard[k] = false;
		}
		else                    // Make code
		{
			lastscan = k;
			keyboard[k] = true;

			if (special)
				c = SpecialNames[k];
			else
			{
				if (k == SC_CAPSLOCK)
				{
					capslock ^= true;
					// DEBUG - make caps lock light work
				}

				if (keyboard[SC_LSHIFT] || keyboard[SC_RSHIFT]) // If shifted
				{
					c = ShiftNames[k];
					if ((c >= 'A') && (c <= 'Z') && capslock)
						c += 'a' - 'A';
				}
				else
				{
					c = ASCIINames[k];
					if ((c >= 'a') && (c <= 'z') && capslock)
						c -= 'a' - 'A';
				}
			}
			if (c)
				lastascii = c;

			*(short *)0x41c = *(short *)0x41a;      // clear bios key buffer
		}

		special = false;
	}

}

//===========================================================================

/*
================
=
= INT_TimerISR
=
================
*/

#pragma interrupt(INT_TimerISR)
void INT_TimerISR ()
{
	_XSTACK  *ptr;                      // ptr to the stack frame

// don't chain this to the dos isr

	ptr = (_XSTACK *)_get_stk_frame();  // get ptr to the V86 _XSTACK frame
	ptr->opts |= _STK_NOINT;                // set _STK_NOINT to prevent V86 call

	_outbyte(0x20,0x20);    // Ack the interrupt
	CLI;                                    // don't let any more ints hit

	if (colorborder)
		VI_ColorBorder(ISRCOLOR);

	if (--intcounter <= 0)
	{
		intcounter = intsperframe;
	//
	// increment timing variables
	//
		timecount++;

		if (framehook)
		{
			if (colorborder)
				VI_ColorBorder(FRAMEHOOKCOLOR);
			framehook ();
		}

	//
	// call the user hook if needed
	//
		if (timerhook && --framecounter <= 0)
		{
			framecounter = framesperhook;

			if (colorborder)
				VI_ColorBorder(TIMERHOOKCOLOR);
			timerhook ();
		}
	}

	if (colorborder)
		VI_ColorBorder(oldbordercolor);
}

//===========================================================================

/*
=====================
=
= INT_SetTimer0
=
= Sets system timer 0 to the specified speed
=
=====================
*/

void INT_SetTimer0(int speed)
{
#ifdef PARMCHECK
	if (speed > 0 && speed < 150)
		MS_Error ("SD_SetTimer0: %i is a bad value",speed);
#endif

	timerspeed = speed;

	_outbyte(0x43,0x36);                            // Change timer 0
	_outbyte(0x40,timerspeed);
	_outbyte(0x40,timerspeed >> 8);
}

/*
=====================
=
= INT_SetIntsPerFrame
=
= Determines the number of interrupts that will occur each
= raster scan. (VGA 70 fps)
=
=====================
*/

void INT_SetIntsPerFrame (int ints)
{
	intsperframe = intcounter = ints;
	INT_SetTimer0(VBLCOUNTER/ints);
}



/*
================
=
= INT_TimerHook
=
================
*/

void INT_TimerHook(void (* hook)(void), int tics)
{
	framecounter = framesperhook = tics;
	timerhook = hook;
}


/*
================
=
= INT_FrameHook
=
================
*/

void INT_FrameHook(void (* hook)(void) )
{
	framehook = hook;
}


/*
=====================
=
= IN_ClearKeysDown
=
= Clears the keyboard array
=
=====================
*/

void IN_ClearKeysDown(void)
{
	lastscan = SC_NONE;
	lastascii = KEY_NONE;
	memset (keyboard,0,sizeof(keyboard));
}


/*
===================
=
= IN_ReadControls
=
===================
*/

void IN_ReadControls (void)
{
	int             i;
	boolean buttonstate[NUMBUTTONS];

	memset (buttonstate,0,sizeof(buttonstate));
//
// get keyboard
//
	for (i=0;i<NUMBUTTONS;i++)
		if (keyboard[scanbuttons[i]])
			buttonstate[i] = true;

//
// check joystick buttons
//

//
// check joystick motions
//

//
// adjust in_button matrix
//
	for (i=0;i<NUMBUTTONS;i++)
		if (buttonstate[i])
			in_button[i]++;
		else
			in_button[i] = 0;
}


/*
=============================================================================

				UNIVERSAL ACKNOWLEDGEMENT STUFF

=============================================================================
*/

boolean btnstate[8];


/*
=====================
=
= IN_StartAck
=
=====================
*/

void IN_StartAck(void)
{
	unsigned        i,buttons;

//
// get initial state of everything
//
	IN_ClearKeysDown();
	memset (btnstate,0,sizeof(btnstate));

// DBDOOM
#if 0
	buttons = IN_JoyButtons () << 4;
	if (mousepresent)
		buttons |= IN_MouseButtons ();

	for (i=0;i<8;i++,buttons>>=1)
		if (buttons&1)
			btnstate[i] = true;
	if (mouseb1)
		btnstate[0] = true;
	if (mouseb2)
		btnstate[1] = true;
#endif
}


/*
=====================
=
= IN_CheckAck
=
=====================
*/

boolean IN_CheckAck (void)
{
	unsigned        i,buttons;

//
// see if something has been pressed
//
	if (lastscan)
		return true;

#if 0
// DBDOOM
	buttons = IN_JoyButtons () << 4;
	if (mousepresent)
		buttons |= IN_MouseButtons ();

	for (i=0;i<8;i++,buttons>>=1)
		if ( buttons&1 )
		{
			if (!btnstate[i])
				return true;
		}
		else
			btnstate[i]=false;
	if ( mouseb1 )
	{
		if (!btnstate[0])
			return true;
	}
	else
		btnstate[0]=false;

	if ( mouseb2 )
	{
		if (!btnstate[1])
			return true;
	}
	else
		btnstate[1]=false;
#endif

	return false;
}


/*
=====================
=
= IN_Ack
=
=====================
*/

void IN_Ack (void)
{
	IN_StartAck ();

	while (!IN_CheckAck ())
	;
}


/*
=====================
=
= IN_AckTics
=
= Wait a certain number of tics or aborts out if any
= button is pressed
=
=====================
*/

boolean IN_AckTics(int delay)
{
	int             lasttime;

	lasttime = timecount;
	IN_StartAck ();
	do
	{
		if (IN_CheckAck())
			return true;
	} while (timecount - lasttime < delay);

	return false;
}



//==========================================================================


/*
================
=
= INT_Setup
=
================
*/

void INT_Setup (void)
{
	oldkeyboardisr = _dos_getvect(KEYBOARDINT);
	_dpmi_lockregion (INT_KeyboardISR,4096);
	_dos_setvect (KEYBOARDINT, INT_KeyboardISR);

	oldtimerisr = _dos_getvect(TIMERINT);
	_dpmi_lockregion (INT_TimerISR,4096);
	_dos_setvect (TIMERINT, INT_TimerISR);
	timeractive = true;

	INT_SetFPS(70);
}


/*
================
=
= INT_Shutdown
=
================
*/

void INT_Shutdown (void)
{
	if (oldkeyboardisr)
		_dos_setvect (KEYBOARDINT, oldkeyboardisr);

	if (oldtimerisr)
		_dos_setvect (TIMERINT, oldtimerisr);

	INT_SetTimer0 (0);              // back to 18.4 ips
}

void INT_SetFPS(int fps)
{
	INT_SetTimer0(1193180/fps);
}
