// d_ints.h

//
// keyboard scan codes
//
#define	NUMCODES	128


#define	SC_NONE			0
#define	SC_BAD			0xff
#define	SC_ENTER		0X1c
#define	SC_ESCAPE		0x01
#define	SC_SPACE		0x39
#define	SC_BACKSPACE	0x0e
#define	SC_TAB			0x0f
#define	SC_ALT			0x38
#define	SC_CONTROL		0x1d
#define	SC_CAPSLOCK		0x3a
#define	SC_LSHIFT		0x2a
#define	SC_RSHIFT		0x36
#define	SC_UPARROW		0x48
#define	SC_DOWNARROW	0x50
#define	SC_LEFTARROW	0x4b
#define	SC_RIGHTARROW	0x4d
#define	SC_INSERT		0x52
#define	SC_DELETE		0x53
#define	SC_HOME			0x47
#define	SC_END			0x4f
#define	SC_PGUP			0x49
#define	SC_PGDN			0x51
#define	SC_F1			0x3b
#define	SC_F2			0x3c
#define	SC_F3			0x3d
#define	SC_F4			0x3e
#define	SC_F5			0x3f
#define	SC_F6			0x40
#define	SC_F7			0x41
#define	SC_F8			0x42
#define	SC_F9			0x43
#define	SC_F10			0x44
#define	SC_F11			0x57
#define	SC_F12			0x59

#define	SC_1			0x02
#define	SC_2			0x03
#define	SC_3			0x04
#define	SC_4			0x05
#define	SC_5			0x06
#define	SC_6			0x07
#define	SC_7			0x08
#define	SC_8			0x09
#define	SC_9			0x0a
#define	SC_0			0x0b

#define	SC_A			0x1e
#define	SC_B			0x30
#define	SC_C			0x2e
#define	SC_D			0x20
#define	SC_E			0x12
#define	SC_F			0x21
#define	SC_G			0x22
#define	SC_H			0x23
#define	SC_I			0x17
#define	SC_J			0x24
#define	SC_K			0x25
#define	SC_L			0x26
#define	SC_M			0x32
#define	SC_N			0x31
#define	SC_O			0x18
#define	SC_P			0x19
#define	SC_Q			0x10
#define	SC_R			0x13
#define	SC_S			0x1f
#define	SC_T			0x14
#define	SC_U			0x16
#define	SC_V			0x2f
#define	SC_W			0x11
#define	SC_X			0x2d
#define	SC_Y			0x15
#define	SC_Z			0x2c

#define	KEY_NONE		0
#define	KEY_RETURN		0x0d
#define	KEY_ENTER		0X0d
#define	KEY_ESCAPE		0x1b
#define	KEY_SPACE		0x20
#define	KEY_BACKSPACE	0x08
#define	KEY_TAB			0x09
#define	KEY_DELETE		0x7f


extern	byte		*pendingscreen;

#define NUMBUTTONS	9

enum {
	bt_nobutton = -1,
	bt_north = 0,
	bt_east,
	bt_south,
	bt_west,
	bt_a,
	bt_b,
	bt_c,
	bt_d
};


extern	boolean		keyboard[NUMCODES];
extern	boolean		paused,capslock;
extern	char		lastascii;
extern	byte		lastscan;

extern	boolean		timeractive;

extern	int			timecount;

extern	int			scanbuttons[NUMBUTTONS];
extern	int			in_button[NUMBUTTONS];		// frames the button has been down


void INT_Setup (void);
void INT_Shutdown (void);

void INT_FrameHook(void (* hook)(void));
void INT_TimerHook(void (* hook)(void), int tics);

void IN_ClearKeysDown (void);
void IN_Ack (void);
void IN_ReadControls (void);
void INT_SetFPS(int fps);
