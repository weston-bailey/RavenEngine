// D_DISK.H

#ifndef __DISK__
#define __DISK__

#include <stdio.h>

#pragma noalign (fileinfo_s)

typedef struct fileinfo_s			// must be noaligned, or the first
{									// short will be padded to 4 bytes
	short	numlumps;
	int		infotableofs;
	int		infotablesize;
} fileinfo_t;


typedef struct
{
	int			filepos;
	unsigned	size;
	short		nameofs;
	short		compress;
} lumpinfo_t;



extern	fileinfo_t	fileinfo;		// the file header
extern	lumpinfo_t	*infotable;		// pointers into the cache file
extern	void		**lumpmain;		// pointers to the lumps in main memory

extern	int			cachehandle;	// handle of current file

extern	FILE		*debugstream;	// misc io stream


void CA_ReadFile (char *name, void *buffer, unsigned length);
void *CA_LoadFile (char *name);

void CA_InitFile (char *filename);
int CA_CheckNamedNum (char *name);
int CA_GetNamedNum (char *name);
void *CA_CacheLump (int lump);
void CA_ReadLump (int lump, void *dest);
void CA_FreeLump (unsigned lump);
void CA_WriteLump (unsigned lump);
void CA_OpenDebug (void);
void CA_CloseDebug (void);

#endif
