/*
** ILBM file loader by John Romero (C) 1990 PCRcade
**
** Loads and decompresses an ILBM-format file to the
** screen in either CGA, EGA or MCGA -- compressed or
** or unpacked!
**
** Merely pass the filename of the image to LoadLBM
** and sit back! The proper graphics mode is initialized
** and the screen (or brush) is loaded and displayed!
**
*/

struct LBMinfo { int width,height,planes; } CurrentLBM;

#include <stdio.h>
#include <fcntl.h>
#include <dos.h>
#include <bios.h>
#include <mem.h>
#include <alloc.h>

#define SC_index	0x3c4
#define SC_mapmask	2

/*
** FUNCTION PROTOTYPES
*/

void GetChunkID(char huge *buffer,char *tempstr);
int  NextChunkID(char huge *buffer);
void huge *Decompress(char huge *buffer,char *unpacked,int bpwidth,char planes);
void huge *SetupLBM(char *filename);
void Do_CGA_Screen(char huge *buffer,char compress,char planes,int width,int height);
void Do_EGA_Screen(char huge *buffer,char compress,char planes,int width,int height,int scrwidth);
void Do_MCGA_Screen(char huge *buffer,char compress,int width,int height);
void EGA_MoveBitplane(char huge *from,char far *to,int bpwidth);

char typestr[5],huge *startbuff;

// EGA loader NOTE: the "scrwidth" var should be 0=320 mode,1=640 mode

void LoadLBM(char *filename,int scrwidth,int graphflag)
{

 char huge *buffer,huge *cmap;
 char planes,
      tempstr[5],
      compress;
 int handle,
     width,
     height;

  if((buffer = SetupLBM(filename))==NULL) exit(1);

  /*
  ** Need to get BMHD info, like:
  ** - screen width, height
  ** - # of bitplanes
  ** - compression flag (YES/NO)
  */

  CurrentLBM.width=width  = (*(buffer+9)&0xFF)+(*(buffer+8)*256);
  CurrentLBM.height=height = (*(buffer+11)&0xFF)+(*(buffer+10)*256);
  CurrentLBM.planes=planes = *(buffer+16);
  compress = *(buffer+18);

  /*
  if ((strcmp(typestr,"PBM ")!=0)&&(planes==8))
     {
      printf("This VGA screen is not in PBM format, but ILBM, which I\ncannot decode at this time.\n");
      exit(1);
     }*/

  /*
  ** Find the CMAP chunk so I can remap the registers...
  */

  movedata(FP_SEG(buffer),FP_OFF(buffer),_DS,(unsigned)tempstr,4);
  tempstr[4]=0;
  while (strcmp(tempstr,"CMAP")!=0)
     {
      buffer += NextChunkID(buffer);
      movedata(FP_SEG(buffer),FP_OFF(buffer),_DS,(unsigned)tempstr,4);
     }

  cmap = buffer+8;

  /*
  ** Now, find the BODY chunk...
  */

  movedata(FP_SEG(buffer),FP_OFF(buffer),_DS,(unsigned)tempstr,4);
  while (strcmp(tempstr,"BODY")!=0)
     {
      buffer += NextChunkID(buffer);
      movedata(FP_SEG(buffer),FP_OFF(buffer),_DS,(unsigned)tempstr,4);
     }

  /*
  ** Found the BODY chunk! Here we go!
  */

  buffer += 8; /* point to actual data */

  /* Turn graphics mode on, if desired */

  if (graphflag==1)
     {
      switch (planes)
             {
              case 2: { _AX=4; geninterrupt(0x10); break; }
              case 4: {
              		_AX=0x0d;
			geninterrupt(0x10);
			scrwidth=0;
                        break;
              	      }
              case 8: { _AX=0x13; geninterrupt(0x10); break; }
             }
     }

  switch (planes)
    {
      case 2: Do_CGA_Screen(buffer,compress,planes,width,height);
              break;
      case 4: Do_EGA_Screen(buffer,compress,planes,width,height,scrwidth);
              break;
      case 8: {
               unsigned int i;

               for (i=0;i<0x300;i++) (unsigned char)*cmap++ >>= 2;

	       cmap -= 0x300; // reset to beginning again

               _BX = 0;
               _CX = 0x100;
	       _ES = FP_SEG(cmap);
	       _DX = FP_OFF(cmap);
               _AX = 0x1012;
               geninterrupt(0x10);

	       Do_MCGA_Screen(buffer,compress,width,height);

               break;
              }

      default: {
		printf("This screen has %d bitplanes. I don't understand that sort of stuff.\n",planes);
		exit();
               }
    }
  farfree((void far *)startbuff);
}



//==============================================
//=
//= Load a *LARGE* file into a FAR buffer!
//= by John Romero (C) 1990 PCRcade
//=
//==============================================

unsigned long LoadFile(char *filename,char huge *buffer)
{
 unsigned int handle,flength1,flength2,buf1,buf2,foff1,foff2;

 buf1=FP_OFF(buffer);
 buf2=FP_SEG(buffer);

asm		mov	WORD PTR foff1,0  	// file offset = 0 (start)
asm		mov	WORD PTR foff2,0

asm		mov	dx,filename
asm		mov	ax,3d00h		// OPEN w/handle (read only)
asm		int	21h
asm		jc	out

asm		mov	handle,ax
asm		mov	bx,ax
asm		xor	cx,cx
asm		xor	dx,dx
asm		mov	ax,4202h
asm		int	21h			// SEEK (find file length)
asm		jc	out

asm		mov	flength1,ax
asm		mov	flength2,dx

asm		mov	cx,flength2
asm		inc	cx			// <- at least once!

L_1:

asm		push	cx

asm		mov	cx,foff2
asm		mov	dx,foff1
asm		mov	ax,4200h
asm		int	21h			// SEEK from start

asm		push	ds
asm		mov	bx,handle
asm		mov	cx,-1
asm		mov	dx,buf1
asm		mov	ax,buf2
asm		mov	ds,ax
asm		mov	ah,3fh			// READ w/handle
asm		int	21h
asm		pop	ds

asm		pop	cx
asm		jc	out
asm		cmp	ax,-1
asm		jne	out

asm		push	cx			// need to read the last byte
asm		push	ds			// into the segment! IMPORTANT!
asm		mov	bx,handle
asm		mov	cx,1
asm		mov	dx,buf1
asm		add	dx,-1
asm		mov	ax,buf2
asm		mov	ds,ax
asm		mov	ah,3fh
asm		int	21h
asm		pop	ds
asm		pop	cx

asm		add	buf2,1000h
asm		inc	WORD PTR foff2
asm		loop	L_1

out:

asm		mov	bx,handle		// CLOSE w/handle
asm		mov	ah,3eh
asm		int	21h

return (flength2*0x10000+flength1);

}



void huge *SetupLBM(char *filename)
{
 int handle;
 char huge *buffer;
 char tempstr[64];


 buffer = startbuff = (char huge *)farmalloc(0xf000);
 if (buffer==NULL) return(NULL);

 strupr(filename);
 if (strstr(filename,".")==NULL)
    strcat(filename,".LBM");


 LoadFile(filename,buffer);

 GetChunkID(buffer,tempstr);
 if (strcmp(tempstr,"FORM")!=0)
    {
     printf("This isn't in ILBM FORM format file!\n");
     return(NULL);
    }

  /*
  ** point past the FORM chunk
  ** and see if this really IS
  ** and ILBM file
  */

  buffer += 8;
  GetChunkID(buffer,tempstr);
  strcpy(typestr,tempstr); // save file type
  if ((strcmp(tempstr,"ILBM")!=0) && (strcmp(tempstr,"PBM ")!=0) )
     {
      printf("This isn't an ILBM format file!\n");
      return(NULL);
     }

  /*
  ** point to BMHD chunk, the first NORMAL chunk!
  */

  buffer += 4;
  GetChunkID(buffer,tempstr);
  if (strcmp(tempstr,"BMHD")!=0)
     {
      printf("What kind of ILBM is this? There's no BMHD chunk!\n");
      return(NULL);
     }
  return(buffer);
}



void GetChunkID(char huge *buffer,char *tempstr)
{
  movedata(FP_SEG(buffer),FP_OFF(buffer),_DS,(unsigned)tempstr,4);
  tempstr[4]=0;
}



int NextChunkID(char huge *buffer)
{
  unsigned int newoffset;

  newoffset = (*(buffer+7)&0xFF) + (*(buffer+6)*256);
  if ((newoffset & 1)==1) newoffset += 1;
  return(newoffset+8); /* +8 because chunk + offset = 8 bytes! */
}



/*
** CGA loader
*/

void Do_CGA_Screen(char huge *buffer,char compress,char planes,int width,int height)
{
 unsigned int bpwidth,
              loopY,
              loopX,
              loopB,
              offset,
              data;
 char far *screen,
          b1,
          b2,
          unpacked[80];


 bpwidth = width/8;

 for (loopY=0;loopY<height;loopY++)
     {
      if (compress==1)
         buffer=Decompress(buffer,unpacked,bpwidth,planes);

      offset=0;
      screen = MK_FP(0xb800,(0x2000*(loopY&1))+(80*(loopY/2)));
      for (loopX=0;loopX<bpwidth;loopX++)
          {
           if (compress==1)
              {
               b1 = *(unpacked+offset);
               b2 = *(unpacked+bpwidth+offset);
              }
           else
              {
               b1 = *(buffer+offset);
               b2 = *(buffer+bpwidth+offset);
              }
           offset++;

	   // This loop should be in INLINE(!) assembler!

                asm       mov   cx,8
                asm       mov   bh,b1
                asm       mov   bl,b2
                asm       xor   dx,dx
                LoopTop:
                asm       test  bh,1
                asm       jz    NoOR
                asm       or    dx,4000h
                NoOR:
                asm       test  bl,1
                asm       jz    NoOR1
                asm       or    dx,8000h
                NoOR1:
                asm       cmp   cx,1
                asm       je    NoShift
                asm       shr   dx,1
                asm       shr   dx,1
                NoShift:
                asm       shr   bh,1
                asm       shr   bl,1
                asm       loop  LoopTop

                asm       mov   data,dx

	   /* first draft of above loop:

           for (loopB=0;loopB<8;loopB++)
               {
                if (b1 & 1==1) data |= 0x4000;
                if (b2 & 1==1) data |= 0x8000;
                if (loopB<7) data >>= 2;
                b1 >>= 1;
                b2 >>= 1;
               }
           */

           *screen = data >> 8;
           *(screen+1) = data;
           screen += 2;
          }

      if (compress==0)
         buffer += bpwidth*planes;
     }
}



/*
** EGA loader (NOTE: the "scrwidth" var should be 0=320 mode,1=640 mode
*/

void Do_EGA_Screen(char huge *buffer,char compress,char planes,int width,int height,int scrwidth)
{
 unsigned int bpwidth,
              loopY,
              loopX,
              loopB,
              offset,
              data;
 char far *screen = (char far *)0xa0000000,
          b1,
          b2,
          unpacked[160];


 bpwidth = width/8;

 for (loopY=0;loopY<height;loopY++)
     {
      if (compress==1)
         {
          buffer=Decompress(buffer,unpacked,bpwidth,planes);
          EGA_MoveBitplane(unpacked,screen,bpwidth);
         }
      else
         {
          EGA_MoveBitplane(buffer,screen,bpwidth);
          buffer += bpwidth*planes;
         }
      screen = MK_FP(0xa000,FP_OFF(screen)+40*(scrwidth+1));
     }
}




/*
** MCGA loader
*/

void Do_MCGA_Screen(char huge *buffer,char compress,int width,int height)
{
 unsigned int bpwidth,
              loopY,
              loopX,
              loopB,
              offset,
              data;
 char far *screen = (char far *)0xa0000000,
          b1,
	  b2,
	  unpacked1[320],
	  unpacked[320];


 for (loopY=0;loopY<height;loopY++)
     {
      if (compress==1)
         {
	  buffer=Decompress(buffer,unpacked,width,1);
	  if (strcmp(typestr,"ILBM")==0)
	     {
	      int tloop;

	      memset(unpacked1,0,320);
	      for (tloop=0;tloop<40;tloop++)
		  {
		   int tloop1,tloop2;
		   unsigned char mask[8] = { 0x80,0x40,0x20,0x10,8,4,2,1 };

		   for (tloop1=0;tloop1<8;tloop1++)
		     for (tloop2=0;tloop2<8;tloop2++)
		      unpacked1[tloop*8+tloop1]|=((( (unsigned)unpacked[tloop+(7-tloop2)*40]
						     &mask[tloop1])
						     <<tloop1)
						     >>tloop2);
		  }
	      movedata(_DS,(unsigned)unpacked1,FP_SEG(screen),FP_OFF(screen),width);
	     }
	  else movedata(_DS,(unsigned)unpacked,FP_SEG(screen),FP_OFF(screen),width);
         }
      else
         {
          movedata(FP_SEG(buffer),FP_OFF(buffer),FP_SEG(screen),FP_OFF(screen),width);
          buffer += width;
         }
      screen = MK_FP(0xa000,FP_OFF(screen)+320);
     }
}




/*
** ILBM's RLE decompressor. Merely pass the address of the compressed
** ILBM bitplane data, where to unpack it, the # of bytes each bitplane
** takes up, and the # of bit planes to unpack.
*/

void huge *Decompress(char huge *buffer,char *unpacked,int bpwidth,char planes)
{
 int count,
     offset,
     loopP;
 unsigned char byte,
               rept;

 for (loopP=0;loopP<planes;loopP++)
     {
      count = 0;

      do {
          rept = *(buffer);
          if (rept > 0x80)
             {
              rept = (rept^0xff)+2;
              byte = *(buffer+1);
              buffer+=2;

              memset(unpacked,byte,rept);
             }
          else if (rept < 0x80)
             {
	      rept++;
	      movedata(FP_SEG(buffer),FP_OFF(buffer)+1,_DS,(unsigned) unpacked,rept);
	      buffer += rept+1;
             }
          count += rept;
          unpacked += rept;

         } while (count<bpwidth);
     }
 return(buffer);
}

void EGA_MoveBitplane(char huge *from,char far *to,int bpwidth)
{
 unsigned width;

asm		push	ds
asm		push	di
asm		push	si

asm		lds	si,from		//;DS:SI = from buffer
asm		les	di,to		//;ES:DI = screen
asm		mov	bx,bpwidth
asm		mov	width,bx

asm		mov	dx,SC_index	//;start writing to SC_mapmask register
asm		mov	al,SC_mapmask
asm		out	dx,al
asm		inc	dx

asm		mov	bh,4		//;4 bitplanes!
asm		mov	ah,1		//;start at bitplane 0

EGA1:

asm		mov	al,ah		//;select bitplane
asm		out	dx,al

asm		xor	ch,ch
asm		mov	cl,bl		//;cx = bitplane width
asm		rep movsb
asm		sub	di,width	//;start at beginning of line again...
asm		shl	ah,1
asm		dec	bh
asm		jnz	EGA1

asm		mov	al,15
asm		out	dx,al		//;write to ALL bitplanes again

asm		pop	si
asm		pop	di
asm		pop	ds
}
