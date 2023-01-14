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
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "types.h"
#include "boot/psetting.h"
#include "cpiface.h"
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
	char *name;
	struct node_t *next;
};

static struct node_t *files=0;
static int filesSize=0;

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

void plReadOpenCPPic(void)
{
	int i;
	int n;
	struct node_t* nd;

	static int lastN=-1;
	int file, filesize;
	unsigned char *filecache;

	int low;
	int high;
	int move;

	/* setup file names */

	if(lastN==-1)
	{
		struct node_t* *current=&files;
		/* get the resource containing background pictures */
		const char *picstr=cfGetProfileString2(cfScreenSec, "screen", "usepics", "");

		int wildcardflag=0;

		/* n contains the number of background pictures */
		n=cfCountSpaceList(picstr, 12);
		for(i=0; i<n; i++)
		{
			char name[128];
			if(cfGetSpaceListEntry(name, &picstr, sizeof(name)) == 0)
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
				DIR *dir;
				if(wildcardflag)
					continue;
				dir=opendir(cfDataDir);
				if(dir)
				{
					struct dirent *dp;
					while((dp=readdir(dir)))
					{
						if(match(dp->d_name))
						{
							nd=calloc(1, sizeof(struct node_t));
							makepath_malloc(&nd->name, 0, cfDataDir, dp->d_name, 0);
							nd->next=0;
							*current=nd;
							current=&nd->next;
							filesSize++;
						}
					}
					closedir(dir);
				}
				dir=opendir(cfConfigDir);
				if(dir)
				{
					struct dirent *dp;
					while((dp=readdir(dir)))
					{
						if(match(dp->d_name))
						{
							nd=calloc(1, sizeof(struct node_t));
							makepath_malloc(&nd->name, 0, cfConfigDir, dp->d_name, 0);
							nd->next=0;
							*current=nd;
							current=&nd->next;
							filesSize++;
						}
					}
					closedir(dir);
				}
				wildcardflag=1;
			} else {
				nd=calloc(1, sizeof(struct node_t));
				nd->name=strdup(name);
				nd->next=0;
				*current=nd;
				current=&nd->next;
				filesSize++;
			}
		}
	}

	/* if there are no background pictures we can skip the rest */
	if (filesSize<=0)
		return;
	/* choose a new picture */
	n=rand()%filesSize;
	/* we have loaded that picture already */
	if(n==lastN)
		return;
	lastN=n;

	/* get filename */
	nd=files;
	for(i=0; i<n; i++)
		nd=files->next;

	/* read the file into a filecache */
	file=open(nd->name, O_RDONLY);
	if(file<0)
		return;
	filesize=lseek(file, 0, SEEK_END);
	if(filesize<0)
	{
		close(file);
		return ;
	}
	if(lseek(file, 0, SEEK_SET)<0)
	{
		close(file);
		return ;
	}
	filecache=calloc(sizeof(unsigned char), filesize);
	if(filecache==0)
	{
		close(file);
			return ;
	}
	if(read(file, filecache, filesize)!=filesize)
	{
		free(filecache);
		close(file);
		return ;
	}
	close(file);

	/* allocate memory for a new picture */
	if(plOpenCPPict==0)
	{
		plOpenCPPict=calloc(sizeof(unsigned char), picWidth*picHeight);
		if(plOpenCPPict==0)
		{
			free (filecache);
			close (file);
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
