#define VERSION	"0.4"

/*
=============================================================================

								IDLINK

							by John Carmack

To do
-----
Allow .. in filenames
lump compression
allow block alignment for dynamic paging

=============================================================================
*/

#include <dir.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <io.h>
#include <dos.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <bios.h>
#include <alloc.h>

#include "\miscsrc\types.h"
#include "\miscsrc\minmisc.h"
#include "\miscsrc\script.h"

#pragma hdrstop




typedef struct
{
	char	id[4];
	int		numlumps;
	long	headeroffset;
	int		headerlength;
} compprologue_t;

typedef	struct
{
	char	name[14];
	int		lumptype;			// the command number used to produce the lump
	long	dataoffset;
	long	datalength;
} compheader_t;


typedef enum
{
	co_uncompressed,
	co_rle,
	co_rlew,
	co_huffman,
	co_carmacized
} compress_t;


typedef struct
{
	long		filepos;		// debug: make these three byte values?
	long		size;
	unsigned	nameofs;
	compress_t	compress;
} lumpinfo_t;


typedef struct
{
	int		numlumps;
	long	infotableofs;
	long	infotablesize;
} fileinfo_t;


fileinfo_t	fileinfo,oldfileinfo;
lumpinfo_t	*lumpinfo,*oldlumpinfo;
int			*contentbuffer,*oldcontent;
char		*content_p;

boolean			incompfile;
int				comphandle;
compprologue_t	compprologue;
compheader_t	*compheader,*compheader_p;

char	filename[MAXPATH];
char	filepath[MAXPATH];
char	sourcepath[MAXPATH];
char	destpath[MAXPATH];
char	scriptfilename[MAXPATH];
char	datafilename[MAXPATH];
char	contentfilename[MAXPATH];
char	compfilepath[MAXPATH];
byte	*lumpalloc;					// for lumpinfo and the names
char	*name_p;

int		datahandle;
int		filescopied;

boolean	fullbuild;

unsigned long	oldtime;			// time stamp of old file (bit fields)
unsigned long	newtime;


/*


output filename

; any line starting with a non alphanumeric char is a comment
filename [-compression] [lumpname]
LABEL [lumpname]


idlink [-b][-path FILENAME] scriptfile

put in a SPARSE command

*/



/*
=============================================================================

						   GENERIC FUNCTIONS

=============================================================================
*/

int SafeOpenWrite (char *filename)
{
	int	handle;

	handle = open(filename,O_RDWR | O_BINARY | O_CREAT | O_TRUNC
	, S_IREAD | S_IWRITE);

	if (handle == -1)
		MS_Quit ("Error opening %s: %s\n",filename,strerror(errno));

	return handle;
}

int SafeOpenRead (char *filename)
{
	int	handle;

	handle = open(filename,O_RDONLY | O_BINARY);

	if (handle == -1)
		MS_Quit ("Error opening %s: %s\n",filename,strerror(errno));

	return handle;
}


void SafeRead (int handle, void far *buffer, unsigned count)
{
	unsigned	iocount;

	_dos_read (handle,buffer,count,&iocount);
	if (iocount != count)
		MS_Quit ("File read failure\n");
}


void SafeWrite (int handle, void far *buffer, unsigned count)
{
	unsigned	iocount;

	_dos_write (handle,buffer,count,&iocount);
	if (iocount != count)
		MS_Quit ("File write failure\n");
}


void far *SafeMalloc (long size)
{
	void far *ptr;

	ptr = farmalloc (size);

	if (!ptr)
		MS_Quit ("Malloc failure for %l bytes\n",size);

	return ptr;
}


/*
=============================================================================

						   COPOSITE FILE STUFF

=============================================================================
*/


/*
=================
=
= OpenComposite
=
=================
*/

void OpenComposite (void)
{
	char	*str;

	if (incompfile)
	{
		close (comphandle);
		free (compheader);
	}

//
// get and qualify composite data file name
//
	GetToken (false);

	for (str = token ; *str ; str++)
		if (*str == '.')
			break;

	if (!*str)
		strcat (str,".DAT");

	if (token[0] == '\\')
		strcpy (compfilepath,token);
	else
	{
		strcpy (compfilepath,sourcepath);
		strcat (compfilepath,token);
	}

//
// open it and read in header
//
	comphandle = SafeOpenRead (compfilepath);
	SafeRead (comphandle,&compprologue,sizeof(compprologue));

	if (strncmp(compprologue.id,"SGRB",4))
		MS_Quit ("Composite file %s doesn't have SGRB id\n",compfilepath);

	compheader = SafeMalloc (compprologue.headerlength);

	lseek (comphandle,compprologue.headeroffset,SEEK_SET);

	SafeRead (comphandle,compheader,compprologue.headerlength);

	getftime(comphandle,(struct ftime *)&newtime);

	incompfile = true;
}


/*
=================
=
= CloseComposite
=
=================
*/

void CloseComposite (void)
{
	if (!incompfile)
		MS_Quit ("$CLOSECOMP issued without an open composite file\n");

	close (comphandle);
	free (compheader);
	incompfile = false;
}


/*
=============================================================================

						   IDLINK FUNCTIONS

=============================================================================
*/

/*
===================
=
= QualifyFilename
=
===================
*/

void QualifyFilename (void)
{
	if (incompfile)
	{
		if (filename[0] == '\\' || strlen(filename)>13)
			MS_Quit ("Illegal comp file member name: %s\n",filename);

		strcpy (filepath,compfilepath);
		strcat (filepath,":");
		strcat (filepath,filename);
		return;
	}

	if (filename[0] == '\\')
		strcpy (filepath,filename);
	else
	{
		strcpy (filepath,sourcepath);
		strcat (filepath,filename);
	}
}


/*
=================
=
= ReadOldFile
=
= Tries to read in info from old file.
= If there is a problem, a full rebuild will be specified
=
=================
*/

void ReadOldFile (void)
{
	int		iocount,size;
	int		handle;

//
// open and read data file
//
	datahandle = open(datafilename,O_RDWR | O_BINARY);
	if (datahandle == -1)
	{
		printf ("No current file to do a partial build from.  Rebuilding.\n");
		fullbuild = true;
		return;
	}

	iocount = read (datahandle,&oldfileinfo,sizeof(oldfileinfo));
	if (iocount != sizeof(oldfileinfo))
	{
		printf ("Couldn't read old fileinfo.  Rebuilding.\n");
		close (datahandle);
		fullbuild = true;
		return;
	}

	size = oldfileinfo.infotablesize;

	oldlumpinfo = malloc (size);
	if (!oldlumpinfo)
	{
		printf ("Couldn't allocate %u bytes for oldinfotable.  Rebuilding.\n"
		,size);
		close (datahandle);
		fullbuild = true;
		return;
	}

	lseek (datahandle,oldfileinfo.infotableofs,SEEK_SET);
	iocount = read (datahandle , oldlumpinfo, size);
	if (iocount != size)
	{
		printf ("Couldn't read old fileinfo.  Rebuilding.\n");
		close (datahandle);
		fullbuild = true;
		return;
	}

//
// get timestamp
//
	getftime(datahandle,(struct ftime *)&oldtime);

//
// load in the content file
//
	handle = open(contentfilename,O_RDWR | O_BINARY);
	if (handle == -1)
	{
		printf ("No content file.  Rebuilding.\n");
		close (datahandle);
		fullbuild = true;
		return;
	}

	size = filelength (handle);
	oldcontent = malloc (size);
	if (!oldcontent)
	{
		printf ("Couldn't allocate %u bytes for oldcontent.  Rebuilding.\n"
		,size);
		close (handle);
		close (datahandle);
		fullbuild = true;
		return;
	}

	iocount = read (handle,oldcontent,size);
	if (iocount != size)
	{
		printf ("Couldn't read old content file.  Rebuilding.\n");
		close (handle);
		close (datahandle);
		fullbuild = true;
		return;
	}
}


/*
===================
=
= ParseScript
=
===================
*/

void ParseScript (void)
{
	char	*fname_p;

//
// load the script file into memory
//
	LoadScriptFile (scriptfilename);
	incompfile = false;

//
// get the output filename
//
	GetToken (true);
	strcpy (datafilename,destpath);
	strcat (datafilename,token);

//
// the content file holds a list of the pathnames used to build the
// data file.  Used for future makes
//
	strcpy (contentfilename,datafilename);
	fname_p = contentfilename+strlen(contentfilename);
	while ( *fname_p != '.' )
		if (--fname_p == datafilename)
		{	// filename didn't have an extension
			fname_p = contentfilename+strlen(contentfilename);
			break;
		}
	strcpy (fname_p,".ICN");


	printf ("Data file		: %s\n",datafilename);
	printf ("Content file		: %s\n",contentfilename);

	if (!fullbuild)
		ReadOldFile ();

	if (fullbuild)
	{
		unlink (datafilename);
		datahandle = open(datafilename,O_RDWR | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);
		if (datahandle == -1)
			MS_Quit ("Error opening data file: %s\n",strerror(errno));
	}

//
// count the lumps in the script
//
	fileinfo.numlumps = 0;

	do
	{
		while (TokenAvailable ())	// skip to the end of line
			GetToken (false);

		GetToken (true);			// get the next token
		if (!endofscript && token[0] != '$')	// don't count commands
			fileinfo.numlumps++;

	} while (!endofscript);

	printf ("%i lumps in output file\n",fileinfo.numlumps);

//
// allocate space for the lump directory and names
//
	lumpalloc = malloc (0xfff0);
	if (!lumpalloc)
		MS_Quit ("Couldn't allocate lump directory!\n");

	lumpinfo = (lumpinfo_t *) (lumpalloc + 16-FP_OFF(lumpalloc));
	name_p = (char *)(lumpinfo + fileinfo.numlumps);

//
// allocate space for the content file
//
	contentbuffer = malloc (0xfff0);
	if (!contentbuffer)
		MS_Quit ("Couldn't allocate lump directory!\n");
	content_p = (char *)(contentbuffer + fileinfo.numlumps);
	*content_p++ = 0;	// in case the first lump is a label

//
// position the file pointer to begin writing data
//
	if (fullbuild)
	// leave space in the data file for the header
		lseek (datahandle,sizeof(fileinfo),SEEK_SET);
	else
	// go to the end of the file
		lseek (datahandle,filelength (datahandle),SEEK_SET);
}


/*
===================
=
= CopyFiles
=
===================
*/

#define BUFFERSIZE	0xfff0

void CopyFiles (void)
{
	int		i;
	int		lump,oldlump;
	int		inputhandle;
	long	size;
	char	*buffer,*oldname_p;

	filescopied = 0;

	script_p = scriptbuffer;
	GetToken (true);		// skip output name

	buffer = malloc (BUFFERSIZE);

	for (lump=0 ; lump<fileinfo.numlumps ; lump++)
	{
		memset (&lumpinfo[lump],0,sizeof(lumpinfo[0]));
	//
	// check for abort out
	//
		if ( (bioskey(1)&0xff) == 27)
		{
			bioskey (0);
			fprintf (stderr,"\nAborted.");
			exit (1);
		}
	//
	// get file to copy or label
	//
		GetToken (true);

		if (token[0] == '$')
		{
		//
		// link commands
		//
			lump--;				// this line isn't a lump

			if (!strcmpi(token,"$OPENCOMP") )
			{
				OpenComposite ();
				continue;
			}

			if (!strcmpi(token,"$CLOSECOMP") )
			{
				CloseComposite ();
				continue;
			}

			MS_Quit ("Unrocognized command %s\n",token);
		}

		strcpy (filename,token);

	//
	// check for a lump name
	//
		if (TokenAvailable ())
		{
			GetToken (false);

			lumpinfo[lump].nameofs = name_p-(char *)lumpinfo;
			strcpy (name_p,token);
			name_p += strlen(token)+1;
		}
		else
			lumpinfo[lump].nameofs = 0;


	//
	// deal with labels
	//
		if (strcmpi(filename,"LABEL") == 0)
		{
			// point the content listing at the previous 0
			contentbuffer[lump] = content_p-1 - (char *)contentbuffer;

			if (lumpinfo[lump].nameofs)
				printf ("%4i is LABEL: %s\n",lump,token);
			else
				printf ("%4i is a LABEL\n",lump);
			continue;
		}


	//
	// qualify the filename and add it to the content list
	//
		QualifyFilename ();
		contentbuffer[lump] = content_p - (char *)contentbuffer;
		strcpy (content_p,filepath);
		content_p += strlen(filepath)+1;


	//
	// open the source file
	//
		if (incompfile)
		{
		//
		// look for the filename in the comp file header
		//
			for (i=0;i<compprologue.numlumps;i++)
				if (!strcmp (filename,compheader[i].name) )
					break;

			if (i == compprologue.numlumps)
				MS_Quit ("lump %s is not in comp file %s\n"
				,filename,compfilepath);

			compheader_p = compheader+i;
		}
		else
		{
		//
		// open the file on disk
		//
			inputhandle = open(filepath,O_RDONLY | O_BINARY);

			if (inputhandle == -1)
				MS_Quit ("Error opening data file %s: %s\n"
				,filepath,strerror(errno));
		}

	//
	// if partial build, see if the file is present in
	// old file and check time
	//
		if (!fullbuild)
		{
			for (oldlump=0 ; oldlump<oldfileinfo.numlumps ; oldlump++)
			{
				oldname_p = (char *)oldcontent + oldcontent[oldlump];
				if (strcmp(filepath , oldname_p) == 0)
					break;
			}

			if (oldlump != oldfileinfo.numlumps)
			{
				if (!incompfile)
					getftime(inputhandle,(struct ftime *)&newtime);

				if ( newtime <= oldtime)
				{
				//
				// use the old information
				//
					lumpinfo[lump].filepos = oldlumpinfo[oldlump].filepos;
					lumpinfo[lump].size = oldlumpinfo[oldlump].size;
					lumpinfo[lump].compress = oldlumpinfo[oldlump].compress;

					if (!incompfile)
						close (inputhandle);
					continue;			// done with lump
				}
			}
		}

	//
	// copy the file
	//
		filescopied++;

		if (incompfile)
		{
			lseek (comphandle,compheader_p->dataoffset,SEEK_SET);
			size = compheader_p->datalength;
		}
		else
			size = filelength (inputhandle);

		printf ("%4i = %s (%lu bytes)\n"
		,lump,filepath,size);

		lumpinfo[lump].filepos = tell(datahandle);
		lumpinfo[lump].size = size;
		lumpinfo[lump].compress = co_uncompressed;

		do
		{
			read (inputhandle,buffer,BUFFERSIZE);
			if (size < BUFFERSIZE)
				write (datahandle,buffer,size);
			else
				write (datahandle,buffer,BUFFERSIZE);

			size -= BUFFERSIZE;

		} while (size > 0);

		if (!incompfile)
			close (inputhandle);
	}

	free (buffer);

}


/*
===================
=
= WriteDirectory
=
===================
*/

void WriteDirectory (void)
{
//
// write lumpinfo
//
	fileinfo.infotableofs = tell(datahandle);
	fileinfo.infotablesize = name_p - (char *)lumpinfo;
	write (datahandle,lumpinfo,fileinfo.infotablesize);

//
// write fileinfo
//
	lseek (datahandle,0,SEEK_SET);
	write (datahandle,&fileinfo,sizeof(fileinfo));

	close (datahandle);
}



/*
===================
=
= WriteContentFile
=
===================
*/

void WriteContentFile (void)
{
	int	handle,size,iocount;

	unlink (contentfilename);
	handle = open(contentfilename,O_RDWR | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);
	if (handle == -1)
		MS_Quit ("Error opening content file: %s\n",strerror(errno));

	size = content_p - (char *)contentbuffer;
	iocount = write (handle,contentbuffer,size);
	if (iocount != size)
		MS_Quit ("Write error on content file\n");

	close (handle);
}


/*
===================
=
= main
=
===================
*/

int main (void)
{
	int		parmnum,parmsleft;

	printf ("\nIDLINK "VERSION" by John Carmack, copyright (c) 1992 Id Software\n");

	parmsleft = _argc;

//
// check for help
//
	if (MS_CheckParm ("?"))
	{
		printf (
"Usage: idlink [-b] [-source path] [-dest path] [-script scriptfile]\n\n"

"-b		Force a full rebuild of the file, rather than a file\n"
"		bulking partial update\n"
"\n"
"-source path	To place the source for the files in another directory\n"
"\n"
"-dest path	To place the linked file in another directory\n"
"\n"
"-script file	The script name defaults to LINKFILE.ILN if not specified\n"
);
		exit (1);
	}


//
// check for full or partial build
//
	if (MS_CheckParm ("b"))
	{
		printf ("Full rebuild\n");
		fullbuild = true;
		parmsleft--;
	}
	else
	{
		printf ("Partial make (file size may increase, use -b to rebuild)\n");
		fullbuild = false;
	}


//
// get source directory for data files
//
	parmnum = MS_CheckParm ("source");

	if (parmnum)
	{
		strcpy (sourcepath,_argv[parmnum+1]);
		parmsleft -= 2;
	}
	else
	{
		strcpy(sourcepath, "X:\\");
		sourcepath[0] = 'A' + getdisk();
		getcurdir(0, sourcepath+3);
	}
	if (sourcepath[strlen(sourcepath)-1] != '\\')
		strcat (sourcepath,"\\");

	printf ("Source directory	: %s\n",sourcepath);

//
// get destination directory for link file
//
	parmnum = MS_CheckParm ("dest");

	if (parmnum)
	{
		strcpy (destpath,_argv[parmnum+1]);
		parmsleft -= 2;
	}
	else
	{
		strcpy(destpath, "X:\\");
		destpath[0] = 'A' + getdisk();
		getcurdir(0, destpath+3);
	}
	if (destpath[strlen(destpath)-1] != '\\')
		strcat (destpath,"\\");

	printf ("Destination directory	: %s\n",destpath);

//
// get script file
//
	parmnum = MS_CheckParm ("script");

	if (parmnum)
	{
		strcpy (scriptfilename,_argv[parmnum+1]);
		parmsleft -= 2;
	}
	else
	{
		strcpy(scriptfilename, "X:\\");
		scriptfilename[0] = 'A' + getdisk();
		getcurdir(0, scriptfilename+3);
		strcat (scriptfilename,"\\LINKFILE.ILN");
	}

	printf ("Script file		: %s\n",scriptfilename);

	if (parmsleft != 1)
		MS_Quit ("Improper parameters.  IDLINK -? for help.\n");

//
// start doing stuff
//
	ParseScript ();
	CopyFiles ();

	if (filescopied)
	{
		WriteDirectory ();
		WriteContentFile ();
	}
	else
	{
		close (datahandle);
	}

	printf ("\n%u files copied.\n",filescopied);
	return 0;
}
