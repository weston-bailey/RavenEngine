#include <STDIO.H>
#include <STRING.H>
#include <DOS.H>
//#include <graphics.h>
#include "xlib_all.h"

/*-------------------------------------------------------------------------*/
/* ILBM320 by Mark E. Kern.  The following source code may be used,        */
/* modified, copied and shared with no obligation to the author            */
/* whatsoever, as long as this notice accompanies the source.              */
/* Please direct any questions to GEnie:MKERN1 or CS:70670,3120.           */
/* Code written in Borland C++ in ANSI C mode. Sept 9,1991                 */
/* ------------------------------------------------------------------------*/


/***************************************************************************\
**                                                                         **
*  SHOWLBM by Todd Michael Lewis                                            *
*                                                                           *
*  August 27, 1994                                                          *
*                                                                           *
*  Using Mark E. Kern's code for the display of ILBMs, this routine was     *
*  developed to show ILBMs using the XLIB graphic display library with a    *
*  call to showLBM(x, y, LBM_FILE).                                         *
*                                                                           *
**                                                                         **
\***************************************************************************/

char testPBM[14] =
{
 3, 1,
 1,1,1,
 1,1,1,
 1,1,0,
 1,0,0,
};

/* This defines the key information header contained in the ILBM format */
struct headform{
		long flen;
		char msg2[8];
		long hlen;
		int width;
		int length;
		int xoff;
		int yoff;
		char planes;
		char masking;
		char compression;
		char padbyte;
		int transparent;
		char x_aspect;
		char y_aspect;
		int screenWidth;
		int screenHeight;
};

/* Structure used to store RGB values as they are read from the file */
struct RGBColor {
	char red;
	char green;
	char blue;
};

char RGBpalette[768];

int  ReadHeader(FILE *picFile, struct headform *header);
int  ReadCMap(FILE *picFile, struct headform *header);
int  DumpToScreen(FILE *picFile, struct headform *header, long page);
void drawline(int *yoffset,unsigned char Vbuff[322],
	      int *scanline,long page,int maxLines);
long ReverseLong(long num);
int  ReverseWord(int num);
int  expo(int x,int y);
void setDAC(struct RGBColor pallete[256]);

/* expo calculates x^y and returns it as an integer value. */
int expo(int x,int y){
	int answer=x;

	while(y > 1){
		answer = answer * x;
		--y;
	}
	return(answer);
}

/* This code reverses the byte order in the long values read in from the
   file.  This is to satisfy Intel's byte ordering scheme.              */
long ReverseLong(long num){
	long actualnum;

	actualnum = ((num >> 24) & 0x000000ff) |
		    ((num >>  8) & 0x0000ff00) |
		    ((num <<  8) & 0x00ff0000) |
		    ((num << 24) & 0xff000000);
	return(actualnum);
}

/* This code reverses the byte order in the word values read in from the
   file.  This is to satisfy Intel's byte ordering scheme.               */
int ReverseWord(int num){
	int actualnum;

	actualnum = ((num >> 8) & 0x00ff) |
		    ((num <<  8) & 0xff00);
	return(actualnum);
}

/* main asks for the ILBM file to diplay, opens the file, and then proceeds
   to: 1)Init the graphics.
       2)Read in the major header information.
       3)Read in the color map and set the DAC registers.
       4)Dump the image line by line from the file to the screen.
       5)Wait for a keypress before closing up the file and shutting down. */

void showLBM(int startX, int startY, char *filename, long page)
{
	FILE *picFile;
	struct headform header;

	picFile = fopen(filename,"rb");
	ReadHeader(picFile,&header);
	ReadCMap(picFile,&header);
	DumpToScreen(picFile,&header, page);
	fclose(picFile);
}

/* ReadHeader takes a file pointer and starts to read in values into the
   header structure previously defined.  It first checks to see if the
   file is an ILBM file by looking for the word "FORM", which should be
   in the beginning of the file.                                         */
int ReadHeader(FILE *picFile, struct headform *header){
	int err;
	char form[5];

	fread(form,4,1,picFile);
	form[4]='\0';
	if(strcmp("FORM",form) != 0)
	  return(1);
	fread(&header->flen,sizeof(header->flen),1,picFile);
	fread(&header->msg2,8,1,picFile);
	fread(&header->hlen,4,1,picFile);
	fread(&header->width,sizeof(header->width),1,picFile);
	fread(&header->length,sizeof(header->length),1,picFile);
	fread(&header->xoff,sizeof(header->xoff),1,picFile);
	fread(&header->yoff,sizeof(header->yoff),1,picFile);
	fread(&header->planes,sizeof(header->planes),1,picFile);
	fread(&header->masking,sizeof(header->masking),1,picFile);
	fread(&header->compression,sizeof(header->compression),1,picFile);
	fread(&header->padbyte,sizeof(header->padbyte),1,picFile);
	fread(&header->transparent,sizeof(header->transparent),1,picFile);
	fread(&header->x_aspect,sizeof(header->x_aspect),1,picFile);
	fread(&header->y_aspect,sizeof(header->y_aspect),1,picFile);
	fread(&header->screenWidth,sizeof(header->screenWidth),1,picFile);
	fread(&header->screenHeight,sizeof(header->screenHeight),1,picFile);

	/* Finished loading in the header, now reverse the values to make
	   them Intel compatible. */
	header->flen = ReverseLong(header->flen);
	header->hlen = ReverseLong(header->hlen);
	header->width = ReverseWord(header->width);
	header->length = ReverseWord(header->length);
	header->xoff = ReverseWord(header->xoff);
	header->yoff = ReverseWord(header->yoff);
	header->transparent = ReverseWord(header->transparent);
	header->screenWidth = ReverseWord(header->screenWidth);
	header->screenHeight = ReverseWord(header->screenHeight);

	return(0);
}

/* ReadCMap first looks for the CMAP text in the file which denotes the
   beginning of the color information.  Once it finds the CMAP header, it
   checks to see if the number of colors(x 3 for numbytes) matches the
   length of the CMAP, which is read in after the header. If everything
   is ok, it then loads up the array pallete with the rgb values contained
   in the CMAP section of the file.  ReadCMap then calls setDAC to load
   the video DAC registers with the appropriate values. */
int ReadCMap(FILE *picFile, struct headform *header){
	char form[5];
	long CMAPsize;
	struct palettetype pal;
	unsigned char rgbTuple[3];
	int x;
	int numColors;
	struct RGBColor pallete[256];

	fread(form,4,1,picFile);
	form[4]='\0';
	if(strcmp("CMAP",form) != 0)
	  return(1);
	fread(&CMAPsize,sizeof(CMAPsize),1,picFile);
	CMAPsize=ReverseLong(CMAPsize);
	numColors=expo(2,header->planes);
	if(CMAPsize != (numColors*3))
	  return(1);
	for(x=0;x<numColors;++x){
		fread(rgbTuple,3,1,picFile);
		pallete[x].red = rgbTuple[0]>>2;
		pallete[x].green = rgbTuple[1]>>2;
		pallete[x].blue = rgbTuple[2]>>2;
		RGBpalette[x*3]   = rgbTuple[0]>>2;
		RGBpalette[x*3+1] = rgbTuple[1]>>2;
		RGBpalette[x*3+2] = rgbTuple[2]>>2;
	}
	setDAC(pallete);
	return(0);
}

/* setDAC uses some pretty sophisticated funtions of Borland C.  We first
   set up a bunch of variables to hold CPU register values in type REGS.
   Once we have done this, we can make some low level calls to the video
   BIOS to set up the colors we want in the picture.  We enter a loop that
   sets each register to a RGB color combination and calls the BIOS routine
   to set the specific DAC register we are interesed in.                   */
void setDAC(struct RGBColor pallete[256])
{
   union REGS regs;
   int i;

   /* This sets up each of the 16 pallete entries to enable the proper
      DAC register when the pallete value is combined with the pixel
      value. */
/*   for(i=0;i<16;++i){
	regs.h.ah = 0x10;
	regs.h.al = 0x00;
	regs.h.bh = i;
	regs.h.bl = i;
	int86(0x10,&regs,&regs);
   }

   regs.h.bl = 0x00;
   for (i=0;i<256;++i){
	regs.h.ah = 0x10;  		    /* set specific DAC rgb register value */
//	regs.h.al = 0x10;            	    /* subfunction number, set register */
//	regs.h.ch = pallete[i].green;       /* CH contains the green value */
//	regs.h.cl = pallete[i].blue;        /* CL contains the blue value */
//	regs.h.dh = pallete[i].red;         /* DH contains the red value */
//	int86(0x10, &regs, &regs);          /* int 10h */
//	++regs.h.bl;
//   }*/
  x_put_pal_raw(RGBpalette, 256, 0);

}

/* DumpToScreen unpacks the graphics information contained in the file. It
   first searches for the BODY header which precedes the actual graphics
   data. If we don't find the BODY header, the program will just crash, since
   I don't look out for the end of the file here.  Once it finds the header,
   it reads in the next value, which is the length of the body.  We use this
   value to read in the the next n number of bytes as specified by the body
   length.  ILBM seems to use a run length encoding scheme to pack the data.
   The function looks at the first byte read. If the first byte read in is
   between -1 and -127, then this means we are to read in the next byte
   value, and repeat this value 1 to 127 times depending on the first byte
   value we read. I.E. if the first value we read was -1, we read the next
   value and repeat this value in the scanline 2 times (1-(-1)). If the
   header value was -128, we would do nothing, as this is the no-operation
   code. If the header value, call this n,is between 0 and 128, we interpret
   this to mean the next n bytes are to be read in normally and stuffed
   into the scanline without any sort of expansion or processing. Once this
   function has read enough bytes to make up a scanline (320 bytes in VGA
   mode 13h), we call a routine that dumps the scanline we just built to
   the screen. We keep doing this until we run out of bytes to read.  Note
   that it is possible for an image to have more than 200 scanlines of data,
   but our scanline dump routine ignores lines past 200.                  */
int DumpToScreen(FILE *picFile, struct headform *header, long page){
	int yoffset=0; 			/* the y coord of the scanline */
	int index=0;                    /* index into the scanline array */
	int pixelsToGo=0;              /* number of pixels to go to form a line*/
	int i;                          /* loop counter */
	int maxLines = header->length;
	unsigned char Vbuff[322];       /* buffer to hold the scanline. It
					   is 4 bytes longer than the regular
					   length so it can hold hsize and
					   vsize data for the putimage call */
	int repeat;                     /* how many times to repeat the byte*/
	char repeatValue;               /* raw byte value from file */
	char bufValue;                  /* raw buffer value read from file*/
	long size;                      /* size of the BODY segment */
	long bytesToGo;                 /* bytes to go till end of BODY */
	char form[5];                   /* holds header */
	fread(form,4,1,picFile);
	form[4]='\0';
	while(strcmp("BODY",form) != 0){      /* find the BODY */
		form[0]=form[1];
		form[1]=form[2];
		form[2]=form[3];
		form[3]=fgetc(picFile);
	}
	fread(&size,sizeof(size),1,picFile);
	bytesToGo = ReverseLong(size);

//	Vbuff[0]=(320-1) & 0xff;
					     /* set up height and width of
						our scanline. Since we use
						putimage to dump our scanline,
						we have to tell it how big
						the 'shape' we are drawing
						to the screen is. In our case
						the shape is 320x1 in size.*/
//	Vbuff[1]=(320-1) >> 8;
//	Vbuff[2]=1;
//	Vbuff[3]=0;

	Vbuff[0] = 160;
	Vbuff[1] = 1;

	index = index+2;

//	index = index+4;      /* update the index into the scanline */

	/* Check to see if the compression is of the proper type, which in
	   our case is 1. If it is uncompressed, or if we don't know the
	   compression type, we exit the program. */
	if(header->compression != 1)
		return(1);
	while(bytesToGo > 0)
	{
		fread(&repeatValue,1,1,picFile);
		if(ferror(picFile))
		{
		}
		--bytesToGo;
		repeat = repeatValue;

		if (repeat == -128);
		else if((repeat <= -1) && (repeat >= -127))
			{
				fread(&bufValue,1,1,picFile);
				if(ferror(picFile))
				{
				}
				--bytesToGo;
				for (i=0;i<(1-repeat);++i)
				{
					Vbuff[pixelsToGo+4-index] = bufValue;
					++pixelsToGo;
					if(pixelsToGo == 320)
					drawline(&yoffset,Vbuff,&pixelsToGo,page,
						 maxLines);
				}
			}

			else if((repeat >= 0) && (repeat <= 127))
			{
				for(i=0;i<=repeat;++i)
				{
					fread(&(Vbuff[pixelsToGo+4-index]),1,1,picFile);
					if(ferror(picFile))
					{
					}
					--bytesToGo;
					++pixelsToGo;
					if(pixelsToGo == 320)
						drawline(&yoffset,Vbuff,&pixelsToGo,page,maxLines);
				}
		} /*end if*/
	}/*end while bodysize*/
	return(0);
}


/* drawline takes an array containing the scanline data we have just
   read in, and dumps it to the screen using the putimage call in the
   Borland BGI.  The function then increments the yvalue to point to
   the next scanline, then resets the pixels to go value (scanline) to
   0 again.  If we are currently working on a scanline greater than can
   fit on the screen (i.e. greater than 200), we just ignore it and don't
   draw it to the screen. */
void drawline(int *yoffset,unsigned char Vbuff[322],
	      int *scanline,long page, int maxLines)
{

   // must split image because CHAR cannot hold a length of 320

	unsigned char PBMbuff[162];
	char *imagePtr;

	if(*yoffset <= maxLines)
//	putimage(0,*yoffset,Vbuff,COPY_PUT);
	imagePtr = Vbuff;
	x_bm_to_pbm(imagePtr, PBMbuff);
	x_put_pbm(0,*yoffset,page,PBMbuff);
	Vbuff[160] = 160;
	Vbuff[161] = 1;
	imagePtr+=160;
	x_bm_to_pbm(imagePtr, PBMbuff);
	x_put_pbm(160,*yoffset,page,PBMbuff);
	++*yoffset;
	*scanline=0;
}


