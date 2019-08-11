/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * MEDIALIBRARY filebrowser
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
 *  -ss050430   Stian Skjelstad <stian@nixia.no>
 *    -first release
 */

#include "config.h"
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "types.h"
#include "boot/plinkman.h"
#include "filesel/dirdb.h"
#include "filesel/modlist.h"
#include "filesel/mdb.h"
#include "filesel/adb.h"
#include "filesel/pfilesel.h"
#include "boot/psetting.h"
#include "stuff/err.h"
#include "stuff/poutput.h"
#include "stuff/framelock.h"

#define MAX(a,b) ((a)>(b)?(a):(b))

static struct mdbreaddirregstruct mlReadDirReg;

static struct dmDrive *dmMEDIALIB;

static int mlDrawBox(void)
{
	unsigned int mlTop=plScrHeight/2-2;
	unsigned int i;

	displayvoid(mlTop+1, 5, plScrWidth-10);
	displayvoid(mlTop+2, 5, plScrWidth-10);
	displayvoid(mlTop+3, 5, plScrWidth-10);
	displaystr(mlTop, 4, 0x04, "\xda", 1);
	for (i=5;i<(plScrWidth-5);i++)
		displaystr(mlTop, i, 0x04, "\xc4", 1);
	displaystr(mlTop, plScrWidth-5, 0x04, "\xbf", 1);
	displaystr(mlTop+1, 4, 0x04, "\xb3", 1);
	displaystr(mlTop+2, 4, 0x04, "\xb3", 1);
	displaystr(mlTop+3, 4, 0x04, "\xb3", 1);
	displaystr(mlTop+1, plScrWidth-5, 0x04, "\xb3", 1);
	displaystr(mlTop+2, plScrWidth-5, 0x04, "\xb3", 1);
	displaystr(mlTop+3, plScrWidth-5, 0x04, "\xb3", 1);
	displaystr(mlTop+4, 4, 0x04, "\xc0", 1);
	for (i=5;i<(plScrWidth-5);i++)
		displaystr(mlTop+4, i, 0x04, "\xc4", 1);
	displaystr(mlTop+4, plScrWidth-5, 0x04, "\xd9", 1);

	return mlTop;
}

static int mlSubScan(const uint32_t dirdbnode, int mlTop)
{
	struct modlist *ml = modlist_create();
	struct modlistentry *mle;
	unsigned int i;
	char *npath;

	dirdbGetFullname_malloc (dirdbnode, &npath, DIRDB_FULLNAME_NOBASE|DIRDB_FULLNAME_ENDSLASH);
	displaystr(mlTop+2, 5, 0x0f, npath, plScrWidth-10);
	free (npath);
	fsReadDir(ml, dmFILE, dirdbnode, "*", RD_SUBNOSYMLINK|RD_PUTSUBS/*|(fsScanArcs?RD_ARCSCAN:0)*/);

	if (ekbhit())
	{
		uint16_t key=egetch();
		if (key==27)
			return -1;
	}

	for (i=0;i<ml->num;i++)
	{
		mle=modlist_get(ml, i);
		if (mle->flags&MODLIST_FLAG_DIR)
		{
			char *name;
			dirdbGetName_internalstr (mle->dirdbfullpath, &name);
			if (strcmp (name, ".."))
			if (strcmp (name, "."))
			if (strcmp (name, "/"))
			{
				if (mlSubScan(mle->dirdbfullpath, mlTop))
					return -1;
			}
		} else if (mle->flags&MODLIST_FLAG_FILE)
		{
			if (!mdbInfoRead(mle->mdb_ref))
				mdbScan(mle);
			dirdbMakeMdbAdbRef(mle->dirdbfullpath, mle->mdb_ref, mle->adb_ref);
		}
	}
	modlist_free(ml);
	return 0;
}

static int mlScan(const uint32_t dirdbnode)
{
	int mlTop=mlDrawBox();

	dirdbTagSetParent(dirdbnode);

	displaystr(mlTop+1, 5, 0x0b, "Scanning filesystem, current directory:", 39);
	displaystr(mlTop+3, 5, 0x0b, "-- Abort with escape --", 23);

	if (mlSubScan(dirdbnode, mlTop))
	{
		dirdbTagCancel();
		return -1;
	}

	dirdbTagRemoveUntaggedAndSubmit();
	dirdbFlush();

	return 0;
}

/* can never return a FILE handle */
static FILE *mlSourcesAdd(struct modlistentry *entry)
{
	unsigned int mlTop=mlDrawBox();
	int editpath=0;

	/* these are for editing the path */
	int str_size=512;
	char *str=malloc(str_size);
	unsigned int curpos;
	unsigned int cmdlen;
	int insmode=1;
	unsigned int scrolled=0;

	if (!str)
	{
		fprintf (stderr, "mlSourcesAdd(): str=malloc() failed\n");
		return NULL;
	}

	strcpy(str, "file:/");
	curpos=strlen(str);
	cmdlen=strlen(str);

	displaystr(mlTop+3, 5, 0x0b, "Abort with escape, or finish selection by pressing enter", 56);

	while (1)
	{
		uint16_t key;
		displaystr(mlTop+1, 5, (editpath?0x8f:0x0f), str+scrolled, plScrWidth-10);
		if (editpath)
			setcur(mlTop+1, 5+curpos-scrolled);

		displaystr(mlTop+2, 5, (editpath?0x0f:0x8f), "current file: directory", plScrWidth-10);

		while (!ekbhit())
			framelock();
		key=egetch();

		if ((key>=0x20)&&(key<=0xFF))
		{
			if (editpath)
			{
				/* grow the buffer if needed */
				if ((cmdlen+2) > str_size)
				{
					char *temp;
					str_size += 512;
					temp = realloc(str, str_size+=512);
					if (!temp)
					{
						fprintf (stderr, "mlSourcesAdd(): str=realloc() failed\n");
						free (str);
						return NULL;
					}
					str = temp;
				}

				if (insmode)
				{
					memmove(str+curpos+1, str+curpos, cmdlen-curpos+1);
					str[curpos]=key;
					curpos++;
					cmdlen++;
				} else if (curpos==cmdlen)
				{

					str[curpos++]=key;
					str[curpos]=0;
					cmdlen++;
				} else {
					str[curpos++]=key;
				}
			}
		} else switch (key)
		{
			case 27:
				setcurshape(0);
				free (str);
				return NULL;
			case KEY_LEFT:
				if (editpath)
					if (curpos)
						curpos--;
				break;
			case KEY_RIGHT:
				if (editpath)
					if (curpos<cmdlen)
						curpos++;
				break;
			case KEY_HOME:
				if (editpath)
					curpos=0;
				break;
			case KEY_END:
				if (editpath)
					curpos=cmdlen;
				break;
			case KEY_INSERT:
				if (editpath)
				{
					insmode=!insmode;
					setcurshape(insmode?1:2);
				}
				break;
			case KEY_DELETE:
				if (editpath)
					if (curpos!=cmdlen)
					{
						memmove(str+curpos, str+curpos+1, cmdlen-curpos);
						cmdlen--;
					}
				break;
			case KEY_BACKSPACE:
				if (editpath)
					if (curpos)
					{
						memmove(str+curpos-1, str+curpos, cmdlen-curpos+1);
						curpos--;
						cmdlen--;
					}
				break;
			case _KEY_ENTER:
				if (!editpath)
				{
					struct dmDrive *_dmDrives=dmDrives;
					while (_dmDrives)
					{
						if (!strcmp(_dmDrives->drivename, "file:"))
						{
							mlScan(_dmDrives->currentpath);
							break;
						}
						_dmDrives=_dmDrives->next;
					}
				} else {
					uint32_t node;
					if (str[0]==0)
					{
						free (str);
						return NULL;
					}
					node=dirdbResolvePathAndRef(str);
					mlScan(node);
					dirdbUnref(node);
				}
				setcurshape(0);
				free (str);
				return NULL;
			case KEY_UP:
			case KEY_DOWN:
				if ((editpath^=1))
					setcurshape((insmode?1:2));
				else
					setcurshape(0);
				break;
		}
		while ((curpos-scrolled)>=(plScrWidth-10))
			scrolled+=8;
		while (((signed)curpos-(signed)scrolled)<0)
			scrolled-=8;
	}
}

static int mlReadDir(struct modlist *ml, const struct dmDrive *drive, const uint32_t path, const char *mask, unsigned long opt)
{
	struct modlistentry entry;
	uint32_t dmadd, dmall, dmsearch, dmparent;

	if (drive!=dmMEDIALIB)
		return 1;

	dmadd=dirdbFindAndRef(drive->basepath, "addfiles");
	dmall=dirdbFindAndRef(drive->basepath, "listall");
	dmsearch=dirdbFindAndRef(drive->basepath, "search");
	dmparent=dirdbGetParentAndRef(path);

	if (path==drive->basepath)
	{
		if (!(opt&RD_PUTSUBS))
			return 1;

		entry.drive=drive;
		entry.flags=MODLIST_FLAG_DIR;
		entry.mdb_ref=0xffffffff;
		entry.adb_ref=0xffffffff;
		entry.Read=0; entry.ReadHeader=0; entry.ReadHandle=0;

		strcpy(entry.shortname, "all");
		fs12name(entry.shortname, "all");
		entry.dirdbfullpath=dmall;
		modlist_append(ml, &entry);

		strcpy(entry.shortname, "search");
		fs12name(entry.shortname, "search");
		entry.dirdbfullpath=dmsearch;
		modlist_append(ml, &entry);

		strcpy(entry.shortname, "addfiles");
		fs12name(entry.shortname, "addfiles");
		entry.dirdbfullpath=dmadd;
		entry.flags=MODLIST_FLAG_FILE|MODLIST_FLAG_VIRTUAL;
		entry.ReadHandle=mlSourcesAdd;
		modlist_append(ml, &entry);

		goto out;
	}
	if (path==dmall)
	{
		int first=1;
		uint32_t dirdbnode;
		uint32_t mdb_ref;
		uint32_t adb_ref;
		while (!dirdbGetMdbAdb(&dirdbnode, &mdb_ref, &adb_ref, &first))
		{
			char *cachefile;
			dirdbGetName_internalstr (dirdbnode, &cachefile);
			fs12name(entry.shortname, cachefile);

			entry.drive=dmFILE;
			entry.dirdbfullpath=dirdbnode;
			entry.flags=MODLIST_FLAG_FILE;
			entry.mdb_ref=mdb_ref;
			entry.adb_ref=adb_ref;
			if (adb_ref==DIRDB_NO_ADBREF)
			{
				entry.Read=dosfile_Read;
				entry.ReadHeader=dosfile_ReadHeader;
				entry.ReadHandle=dosfile_ReadHandle;
			} else {
				entry.Read=adb_Read;
				entry.ReadHeader=adb_ReadHeader;
				entry.ReadHandle=adb_ReadHandle;
			}
			modlist_append(ml, &entry);
/*
			dirdbUnref(entry.dirdbfullpath);*/
		}
		goto out;
	}
	if (path==dmsearch)
	{
		unsigned int mlTop=mlDrawBox();
		char str[1024];
		unsigned int curpos;
		unsigned int cmdlen;
		int insmode=1;
		unsigned int scrolled=0;

		displaystr(mlTop+1, 5, 0x0b, "Give me something to crunch!!", 29);
		displaystr(mlTop+3, 5, 0x0b, "-- Finish with enter --", 23);

		str[0]=0;
		curpos=0;
		cmdlen=0;
		setcurshape(1);

		while (1)
		{
			uint16_t key;
			displaystr(mlTop+2, 5, 0x8f, str+scrolled, plScrWidth-10);
			setcur(mlTop+2, 5+curpos-scrolled);

			while (!ekbhit())
				framelock();
			key=egetch();

			if ((key>=0x20)&&(key<=0xFF))
			{
				if (insmode)
				{
					if ((cmdlen+1)<sizeof(str))
					{
						memmove(str+curpos+1, str+curpos, cmdlen-curpos+1);
						str[curpos++]=key;
						cmdlen++;
					}
				} else if (curpos==cmdlen)
				{
					if ((cmdlen+1)<sizeof(str))
					{
						str[curpos++]=key;
						str[curpos]=0;
						cmdlen++;
					}
				} else
				str[curpos++]=key;
			} else switch (key)
			{
				case KEY_LEFT:
					if (curpos)
						curpos--;
					break;
				case KEY_RIGHT:
					if (curpos<cmdlen)
						curpos++;
					break;
				case KEY_HOME:
					curpos=0;
					break;
				case KEY_END:
					curpos=cmdlen;
					break;
				case KEY_INSERT:
					{
						insmode=!insmode;
						setcurshape(insmode?1:2);
					}
					break;
				case KEY_DELETE:
					if (curpos!=cmdlen)
					{
						memmove(str+curpos, str+curpos+1, cmdlen-curpos);
						cmdlen--;
					}
					break;
				case KEY_BACKSPACE:
					if (curpos)
					{
						memmove(str+curpos-1, str+curpos, cmdlen-curpos+1);
						curpos--;
						cmdlen--;
					}
					break;
				case KEY_ESC:
					setcurshape(insmode?1:2);
					goto out;
				case _KEY_ENTER:
					{
						unsigned int i, j;
						int first=1;
						uint32_t dirdbnode;
						uint32_t mdb_ref;
						uint32_t adb_ref;

						setcurshape(0);

						for (i=0;i<cmdlen;i++)
							str[i]=toupper(str[i]);

						while (!dirdbGetMdbAdb(&dirdbnode, &mdb_ref, &adb_ref, &first))
						{
							char *cachefile, *cachefileUpper;

							struct moduleinfostruct info;
							char buffer[
								MAX(sizeof(info.modname)+1,
								    MAX(sizeof(info.composer)+1,
								        sizeof(info.comment)+1))
							];


							dirdbGetName_internalstr (dirdbnode, &cachefile);

							cachefileUpper = malloc (strlen (cachefile) + 1);
							for (i=0; cachefile[i]; i++)
							{
								cachefileUpper[i] = toupper(cachefile[i]);
							}
							cachefileUpper[i]=0;

							if (strstr(cachefileUpper, str))
							{
								free (cachefileUpper);
								goto add;
							}
							free (cachefileUpper);

							mdbGetModuleInfo(&info, mdb_ref);

							for (j=0;j<sizeof(info.modname);j++)
								buffer[j]=toupper(info.modname[j]);
							buffer[j]=0;
							if (strstr(buffer, str))
								goto add;

							for (j=0;j<sizeof(info.composer);j++)
								buffer[j]=toupper(info.composer[j]);
							buffer[j]=0;
							if (strstr(buffer, str))
								goto add;

							for (j=0;j<sizeof(info.comment);j++)
								buffer[j]=toupper(info.comment[j]);
							buffer[j]=0;
							if (strstr(buffer, str))
								goto add;

							continue;
							{
							add:
								fs12name(entry.shortname, cachefile);

								entry.drive=dmFILE;
								entry.dirdbfullpath=dirdbnode; /*dirdbResolvePathAndRef(files[i].name);*/
								entry.flags=MODLIST_FLAG_FILE;
								entry.mdb_ref=mdb_ref;
								entry.adb_ref=adb_ref;
								if (adb_ref==DIRDB_NO_ADBREF)
								{
									entry.Read=dosfile_Read;
									entry.ReadHeader=dosfile_ReadHeader;
									entry.ReadHandle=dosfile_ReadHandle;
								} else {
									entry.Read=adb_Read;
									entry.ReadHeader=adb_ReadHeader;
									entry.ReadHandle=adb_ReadHandle;
								}
								modlist_append(ml, &entry);
								/* dirdbUnref(entry.dirdbfullpath);*/
							}
						}
						goto out;
					}
			}
			while ((curpos-scrolled)>=(plScrWidth-10))
				scrolled+=8;
			while (((signed)curpos-(signed)scrolled)<0)
				scrolled-=8;
		}
	}
out:
	dirdbUnref(dmsearch);
	dirdbUnref(dmall);
	if (dmparent!=DIRDB_NOPARENT)
		dirdbUnref(dmparent);
	return 1;
}


static int mlint(void)
{
	mdbRegisterReadDir(&mlReadDirReg);
	dmMEDIALIB=RegisterDrive("medialib:");
	return errOk;
}

static void mlclose(void)
{
	mdbUnregisterReadDir(&mlReadDirReg);
}

static struct mdbreaddirregstruct mlReadDirReg = {mlReadDir MDBREADDIRREGSTRUCT_TAIL};

char *dllinfo = "";
struct linkinfostruct dllextinfo = {.name = "medialib", .desc = "OpenCP medialib (c) 2005-09 Stian Skjelstad", .ver = DLLVERSION, .size = 0, .Init = mlint, .Close = mlclose};
