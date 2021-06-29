// D_MISC.H

#ifndef __MISC__
#define __MISC__

extern  int             my_argc;
extern  char    **my_argv;

int     MS_CheckParm (char *check);
void    MS_Error (char *error, ...);
void    MS_ExitClean (void);
byte    MS_RndT (void);

#endif
