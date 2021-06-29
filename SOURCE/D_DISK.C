// D_DISK.C

#include <malloc.h>
#include <io.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

#include "d_global.h"
#include "d_disk.h"
#include "d_misc.h"

/*
=============================================================================

							   GLOBALS

=============================================================================
*/

fileinfo_t	fileinfo;		// the file header
lumpinfo_t	*infotable;		// pointers into the cache file
void		**lumpmain;		// pointers to the lumps in main memory

int			cachehandle;	// handle of current file

FILE		*debugstream;	// misc io stream

/*
============================================================================

						  LOW LEVEL ROUTINES

============================================================================
*/


/*
======================
=
= CA_ReadFile
=
======================
*/

void CA_ReadFile (char *name, void *buffer, unsigned length)
{
	int     handle;

	if ((handle = open(name,O_RDONLY | O_BINARY)) == -1)
		MS_Error ("CA_ReadFile: Open failed!");

	if (!read(handle,buffer,length))
	{
		close (handle);
		MS_Error ("CA_LoadFile: Read failed!");
	}

	close (handle);
}


/*
======================
=
= CA_LoadFile
=
======================
*/

void *CA_LoadFile (char *name)
{
	int     handle;
	unsigned     length;
	void    *buffer;

	if ((handle = open(name,O_RDONLY | O_BINARY)) == -1)
		MS_Error ("CA_LoadFile: Open failed!");

	length = filelength (handle);

	if (!(buffer = malloc(length)) )
		MS_Error ("CA_LoadFile: Malloc failed!");

	if (!read(handle,buffer,length))
	{
		close (handle);
		MS_Error ("CA_LoadFile: Read failed!");
	}

	close (handle);

	return buffer;
}


/*
============================================================================

							IDLINK STUFF

============================================================================
*/


/*
====================
=
= CA_InitFile
=
====================
*/

void CA_InitFile (char *filename)
{
	unsigned	size,i;

//
// if a file is allready open, shut it down
//
	if (cachehandle)
	{
		close (cachehandle);
		free (infotable);
		for (i=0 ; i < fileinfo.numlumps ; i++)
			if (lumpmain[i])
				free (lumpmain[i]);
		free (lumpmain);
	}

//
// load the header
//
	if ((cachehandle = open(filename,
		 O_RDONLY | O_BINARY, S_IREAD)) == -1)
		MS_Error ("Can't open %s!",filename);

	read(cachehandle, (void *)&fileinfo, sizeof(fileinfo));

//
// load the info list
//
	size = fileinfo.infotablesize;
	infotable = malloc(size);
	lseek (cachehandle,fileinfo.infotableofs,SEEK_SET);
	read (cachehandle, (void *)infotable, size);

	size = fileinfo.numlumps*sizeof(int);
	lumpmain = malloc(size);
	memset (lumpmain,0,size);
}


/*
====================
=
= CA_CheckNamedNum
=
= Returns -1 if name not found
=
====================
*/

int CA_CheckNamedNum (char *name)
{
	int	i,ofs;

	for (i=0 ; i<fileinfo.numlumps ; i++)
	{
		ofs = infotable[i].nameofs;
		if (!ofs)
			continue;
		if (stricmp(name,((char *)infotable)+ofs) == 0)
			return i;
	}

	return -1;
}


/*
====================
=
= CA_GetNamedNum
=
= Searches through the directory list for a matching name
= Bombs out if not found
=
====================
*/

int CA_GetNamedNum (char *name)
{
	int	i;

	i = CA_CheckNamedNum (name);
	if (i != -1)
		return i;

	MS_Error ("CA_GetNamedNum: %s not found!",name);
	return -1;
}



/*
====================
=
= CA_CacheLump
=
= Returns a pointer to a lump, caching it if needed
=
====================
*/

void *CA_CacheLump (int lump)
{
#ifdef PARMCHECK
	if (lump>=fileinfo.numlumps)
		MS_Error ("CA_LumpPointer: %i > numlumps!",lump);
#endif
	if (!lumpmain[lump]) {
		//
		// load the lump off disk
		//
		if (! (lumpmain[lump] = malloc(infotable[lump].size)) )
			MS_Error ("CA_LumpPointer: malloc failure of lump %d, with size %d",
				lump,infotable[lump].size);
		lseek (cachehandle, infotable[lump].filepos, SEEK_SET);
		read (cachehandle,lumpmain[lump],infotable[lump].size);
	}

	return lumpmain[lump];
}


/*
====================
=
= CA_ReadLump
=
= Reads a lump into an allready allocated buffer
= Does NOT consider it to be cached there, so it is ok to change it
=
====================
*/

void CA_ReadLump (int lump, void *dest)
{
#ifdef PARMCHECK
	if (lump>=fileinfo.numlumps)
		MS_Error ("CA_ReadLump: %i > numlumps!",lump);
#endif
	lseek (cachehandle, infotable[lump].filepos, SEEK_SET);
	read (cachehandle,dest,infotable[lump].size);
}



/*
====================
=
= CA_FreeLump
=
= Frees the memory associated with a lump
=
====================
*/

void CA_FreeLump (unsigned lump)
{
#ifdef PARMCHECK
	if (lump>=fileinfo.numlumps)
		MS_Error ("CA_FreeLump: %i > numlumps!",lump);
#endif
	free (lumpmain[lump]);
	lumpmain[lump] = NULL;
}


/*
====================
=
= CA_WriteLump
=
= Writes a lump back out to disk.  It must be currently cached in.
= No size change is possible
=
====================
*/

void CA_WriteLump (unsigned lump)
{
#ifdef PARMCHECK
	if (lump>=fileinfo.numlumps)
		MS_Error ("CA_WriteLump: %i > numlumps!",lump);
	if (!lumpmain[lump])
		MS_Error ("CA_WriteLump: %i not cached in!",lump);
#endif

	lseek (cachehandle,infotable[lump].filepos, SEEK_SET);
	write (cachehandle,lumpmain[lump],infotable[lump].size);
}


/*
======================
=
= CA_OpenDebug
=
======================
*/

void CA_OpenDebug (void)
{
	debugstream = fopen("DEBUG.TXT","w");
}


/*
======================
=
= CA_CloseDebug
=
======================
*/

void CA_CloseDebug (void)
{
	fclose (debugstream);
}

