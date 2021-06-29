// d_font.h

#ifndef __FONT__
#define __FONT__

#define MAXPRINTF       256

typedef struct
{
	short   height;
	char    width[256];
	short   charofs[256];
} font_t;


extern  font_t  *font;
extern  int     fontbasecolor;
extern  int     fontspacing;

extern  int     printx,printy;
extern  int     windowx,windowy,windoww,windowh;


int FN_RawWidth (char *str);

void FN_Printf (char *fmt, ...);
void FN_CenterPrintf (char *fmt, ...);
void FN_BlockCenterPrintf (char *fmt, ...);


#endif
