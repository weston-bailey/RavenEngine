#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <alloc.h>
#include <dos.h>
#include "xlib_all.h"

void load_user_fonts(void);
void exitfunc(void);

char far *userfnt1;

void main(void)
{
  int i;
  int j;

  userfnt1 = (char far *) farmalloc(256*16+4);

  x_text_mode();
  x_set_mode(X_MODE_320x200,320);
  x_text_init();
  x_mouse_init();
  MouseColor=15;
  atexit(exitfunc);
  load_user_fonts();


  getch();
} // main

void load_user_fonts(void)
{
  FILE *f;
  f=fopen("tiny4.fnt","rb");
  /* read char by char as fread wont read to far pointers in small model */
  { int i; char c;
	for (i=0;i<256*8+4;i++){
	  fread(&c,1,1,f);
	  *(userfnt1+i)=c;
	}
  }

  fclose(f);

  x_register_userfont(userfnt1);
}

void exitfunc(void){
  x_mouse_remove();
  x_text_mode();
  printf("Ta-da\n");
}