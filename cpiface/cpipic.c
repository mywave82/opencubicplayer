/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * CPIFace background picture loader
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * revision history: (please note changes here)
 *  -nb980510   Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *    -first release
 *  -kb980717   Tammo Hinrichs <opencp@gmx.net>
 *    -some minor changes
 *  -doj980928  Dirk Jagdmann <doj@cubic.org>
 *    -wrote a more compatible TGA loader which is found in the file tga.cpp
 *    -changed this file to meet dependencies from the new TGA loader
 *    -added comments
 *  -doj980930  Dirk Jagdmann <doj@cubic.org>
 *    -added gif loader
 *  -doj981105  Dirk Jagdmann <doj@cubic.org>
 *    -now the plOpenCPPict is cleared if no file is found / valid
 *    -modified memory allocation a bit to remove unnecessary new/delete
 *  -fd981119   Felix Domke <tmbinc@gmx.net>
 *    -added the really important 'NO_CPIFACE_IMPORT'
 *  -doj20020902 Dirk Jagdmann <doj@cubic.org>
 *    -added wildcard for picture loading
 */

#include "config.h"
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "types.h"
#include "boot/psetting.h"
#include "cpiface/cpiface.h"
#include "cpiface/cpipic.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "filesel/filesystem-drive.h"
#include "stuff/compat.h"

#ifdef HAVE_LZW
#include "gif.h"
#endif
#include "tga.h"

unsigned char *plOpenCPPict=0; /* an array containing the raw picture */
unsigned char plOpenCPPal[768]; /* the palette for the picture */

static const int picWidth=640;
static const int picHeight=384;

struct node_t
{
	struct ocpfile_t *file;
	struct node_t *next;
};

static struct node_t *files=0;
static int filesCount=0;

static int match(const char *name)
{
/* check file suffixes*/
	int l=strlen(name);
	if(l<5)
		return 0;
	if(name[l-4]!='.')
		return 1;
#ifdef HAVE_LZW
	if(tolower(name[l-3])=='g' && tolower(name[l-2])=='i' && tolower(name[l-1])=='f')
		return 1;
#endif
	if(tolower(name[l-3])=='t' && tolower(name[l-2])=='g' && tolower(name[l-1])=='a')
		return 1;
	return 0;
}

static void wildcard_file (void *token, struct ocpfile_t *file)
{
	const struct dirdbAPI_t *dirdb = token;
	const char *name;

	dirdb->GetName_internalstr (file->dirdb_ref, &name);

	if(match(name))
	{
		struct node_t *nd = calloc(1, sizeof(struct node_t));
		file->ref (file);
		nd->file = file;
		nd->next = files;
		files = nd;
		filesCount++;
	}
}

static void wildcard_dir (void *token, struct ocpdir_t *dir)
{
}

void plReadOpenCPPic (const struct configAPI_t *configAPI, const struct dirdbAPI_t *dirdb)
{
	int i;
	int n;

	static int lastN=-1;
	struct ocpfilehandle_t *f;
	uint64_t filesize;
	unsigned char *filecache;

	int low;
	int high;
	int move;

	/* setup file names */

	if(lastN==-1)
	{
		int wildcardflag=0;
		const char *picstr = configAPI->GetProfileString2 (configAPI->ScreenSec, "screen", "usepics", "");

		/* n contains the number of background pictures */
		n=configAPI->CountSpaceList (picstr, 12);
		for(i=0; i<n; i++)
		{
			char name[128];
			if(configAPI->GetSpaceListEntry (name, &picstr, sizeof(name)) == 0)
				break;
			if(!match(name))
				continue;
			/* check for wildcard */
#ifdef HAVE_LZW
			if(!strncasecmp(name, "*.gif", 5) || !strncasecmp(name, "*.tga", 5))
#else
			if(!strncasecmp(name, "*.tga", 5))
#endif
			{
				ocpdirhandle_pt h;

				if(wildcardflag)
					continue;

				h = configAPI->DataDir->readdir_start (configAPI->DataDir, wildcard_file, wildcard_dir, (void *)dirdb);
				if (h)
				{
					while (configAPI->DataDir->readdir_iterate (h));
					configAPI->DataDir->readdir_cancel (h);
				}

				h = configAPI->DataHomeDir->readdir_start (configAPI->DataHomeDir, wildcard_file, wildcard_dir, (void *)dirdb);
				if (h)
				{
					while (configAPI->DataHomeDir->readdir_iterate (h));
					configAPI->DataHomeDir->readdir_cancel (h);
				}

				wildcardflag=1;
			} else {
				uint32_t dirdb_ref;
				struct ocpfile_t *f = 0;

				if (!f)
				{
					dirdb_ref = dirdb->ResolvePathWithBaseAndRef (configAPI->DataDir->dirdb_ref, name, DIRDB_RESOLVE_NODRIVE | DIRDB_RESOLVE_TILDE_HOME | DIRDB_RESOLVE_TILDE_USER, dirdb_use_file);
					filesystem_resolve_dirdb_file (dirdb_ref, 0, &f);
					dirdb->Unref (dirdb_ref, dirdb_use_file);
				}

				if (!f)
				{
					dirdb_ref = dirdb->ResolvePathWithBaseAndRef (configAPI->DataHomeDir->dirdb_ref, name, DIRDB_RESOLVE_NODRIVE | DIRDB_RESOLVE_TILDE_HOME | DIRDB_RESOLVE_TILDE_USER, dirdb_use_file);
					filesystem_resolve_dirdb_file (dirdb_ref, 0, &f);
					dirdb->Unref (dirdb_ref, dirdb_use_file);
				}

				if (f)
				{
					struct node_t *nd = calloc (1, sizeof(struct node_t));
					nd->file = f;
					nd->next = files;
					files = nd;
					filesCount++;
				}
			}
		}
	}

	/* if there are no background pictures we can skip the rest */
	if (filesCount<=0)
		return;
	/* choose a new picture */
	n=rand()%filesCount;
	/* we have loaded that picture already */
	if(n==lastN)
		return;
	lastN=n;

	{
		struct node_t *nd=files;
		for(i=0; i<n; i++)
			nd=files->next;

		/* read the file into a filecache */
		f = nd->file->open (nd->file);
		if(!f)
		{
			return;
		}
	}
	filesize=f->filesize (f);
	if (!filesize)
	{
		f->unref (f);
		return ;
	}
	filecache=calloc(sizeof(unsigned char), filesize);
	if(filecache==0)
	{
		f->unref (f);
		return ;
	}
	if (f->read (f, filecache, filesize) != filesize)
	{
		free(filecache);
		f->unref (f);
		return ;
	}
	f->unref (f);
	f = 0;

	/* allocate memory for a new picture */
	if(plOpenCPPict==0)
	{
		plOpenCPPict=calloc(sizeof(unsigned char), picWidth*picHeight);
		if(plOpenCPPict==0)
		{
			free (filecache);
			return;
		}
		memset(plOpenCPPict, 0, picWidth*picHeight);
	}

	/* read the picture */
#ifdef HAVE_LZW
	GIF87read(filecache, filesize, plOpenCPPict, plOpenCPPal, picWidth, picHeight);
#endif
	TGAread(filecache, filesize, plOpenCPPict, plOpenCPPal, picWidth, picHeight);

	free(filecache);

	/* determine if the lower or upper part of the palette is left blank for
	 * our own screen colors
	 */

	low=high=0;
	for (i=0; i<picWidth*picHeight; i++)
		if (plOpenCPPict[i]<0x30)
			low=1;
		else if (plOpenCPPict[i]>=0xD0)
			high=1;

	move=(low&&!high)*0x90;

	/* if the color indices are bad, we have to move every color map entry */
	if (move)
		for (i=0; i<picWidth*picHeight; i++)
		      plOpenCPPict[i]+=0x30;

	/* adjust the RGB palette to 6bit, because standard VGA is limited */
	for (i=0x2FD; i>=0x90; i--)
		plOpenCPPal[i]=plOpenCPPal[i-move]>>2;
}

void plOpenCPPicDone (void)
{
	struct node_t *curr, *next;

	free (plOpenCPPict);
	plOpenCPPict = 0;

	for (curr = files; curr; curr = next)
	{
		next = curr->next;
		curr->file->unref (curr->file);
		free (curr);
	}
	files = 0;
	filesCount = 0;
}
