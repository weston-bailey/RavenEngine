// d_font.c

#include <stdarg.h>
#include <stdio.h>
#include <i32.h>

#include "d_global.h"
#include "d_video.h"
#include "d_misc.h"
#include "d_font.h"


font_t  *font;
int     fontbasecolor = 0;
int     fontspacing = 1;

char    str[MAXPRINTF];                 // general purpose string buffer

int             printx = 0,printy = 0;  // the printing position (top left corner)

int             windowx = 0;                    // window size for text positioning
int             windowy = 0;
int             windoww = 320;
int             windowh = 200;


/*
========================
=
= FN_RawPrint
=
= Draws a string of characters to the screen
=
========================
*/

void FN_RawPrint (char *str)
{
	byte    b;
	byte    *dest,  *source;
	int             width,height;
	char    ch;
	int             x,y;
	int             oldpx;

	oldpx = printx;

	dest = ylookup[printy]+printx;

	height = font->height;

	while ( (ch=*str++) != 0)
	{
		width = font->width[ch];
		source = ((byte  *)font) + font->charofs[ch];

		while (width--)
		{
			for (y=0;y<height;y++)
			{
				b = *source++;
				if (b)
					dest[y*SCREENWIDTH] = b+fontbasecolor;
			}

			dest++;
			printx++;
		}

		dest += fontspacing;
		printx += fontspacing;
	}

	VI_MarkUpdateBlock (oldpx,printy,printx-1,printy+font->height);
}


/*
========================
=
= FN_RawWidth
=
= Returns the width of a string
= Does NOT handle newlines
=
========================
*/

int FN_RawWidth (char *str)
{
	int             width;

	width = 0;

	while (*str)
	{
		width += font->width[*str++];
		width += fontspacing;
	}

	return width;
}


/*
========================
=
= FN_Print
=
= Prints a string in the current window, with newlines
= going down a line and back to windowx
=
========================
*/

void FN_Print(char  *s)
{
	char            c, *se;
	unsigned        h;

	h = font->height;

	while (*s)
	{
		se = s;

		c = *se;
		while (c && c != '\n')
			c= *++se;

		*se = '\0';

		FN_RawPrint (s);

		s = se;
		if (c)
		{
			*se = c;
			s++;

			printx = windowx;
			printy += h;
		}
	}
}


/*
=====================
=
= FN_PrintCentered
=
= Prints a multi line string with each line centered
=
=====================
*/

void FN_PrintCentered (char  *s)
{
	char            c, *se;
	unsigned        w,h;

	h = font->height;

	while (*s)
	{
		se = s;

		c = *se;
		while (c && c != '\n')
			c= *++se;

		*se = '\0';

		w = FN_RawWidth (s);
		printx = windowx + (windoww-w)/2;
		FN_RawPrint (s);

		s = se;
		if (c)
		{
			*se = c;
			s++;

			printx = windowx;
			printy += h;
		}
	}
}


/*
=====================
=
= FN_Printf
=
= Prints a printf style formatted string at the current print position
= using the current print routines
=
=====================
*/

void FN_Printf (char *fmt, ...)
{
	va_list argptr;
	int             cnt;

	va_start (argptr,fmt);
	cnt = vsprintf (str,fmt,argptr);
	va_end (argptr);

	if (cnt>=MAXPRINTF)
		MS_Error ("FN_Printf: String too long: %s",fmt);

	FN_Print (str);
}


/*
=====================
=
= FN_CenterPrintf
=
= As FN_Printf, but centers each line of text in the window bounds
=
=====================
*/

void FN_CenterPrintf (char *fmt, ...)
{
	va_list argptr;
	int             cnt;

	va_start (argptr,fmt);
	cnt = vsprintf (str,fmt,argptr);
	va_end (argptr);

	if (cnt>=MAXPRINTF)
		MS_Error ("FN_CPrintf: String too long: %s",fmt);

	FN_PrintCentered (str);
}


/*
=====================
=
= FN_BlockCenterPrintf
=
= As FN_CenterPrintf, but also enters the entire set of lines vertically in
= the window bounds
=
=====================
*/

void FN_BlockCenterPrintf (char *fmt, ...)
{
	va_list argptr;
	int             cnt;
	char  *s;
	int             height;

	va_start (argptr,fmt);
	cnt = vsprintf (str,fmt,argptr);
	va_end (argptr);

	if (cnt>=MAXPRINTF)
		MS_Error ("FN_CCPrintf: String too long: %s",fmt);

	height = 1;
	s = str;

	while (*s)
	{
		if (*s++=='\n')
			height++;
	}

	height *= font->height;

	printy = windowy + (windowh-height)/2;
	FN_PrintCentered (str);
}


