/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * Player devices system
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
 *    -changed INI reading of driver symbols to _dllinfo lookup
 */

#include "../config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "types.h"
#include "deviplay.h"
#include "filesel/dirdb.h"
#include "filesel/pfilesel.h"
#include "filesel/mdb.h"
#include "filesel/modlist.h"
#include "imsdev.h"
#include "boot/psetting.h"
#include "boot/plinkman.h"
#include "stuff/err.h"
#include "devigen.h"
#include "player.h"
#include "stuff/compat.h"

int (*plrProcessKey)(uint16_t);

int plrBufSize;

static struct devinfonode *plPlayerDevices;
static struct devinfonode *curplaydev;
static struct devinfonode *defplaydev;

static struct mdbreaddirregstruct plrReadDirReg;
static struct interfacestruct plrIntr;
static struct preprocregstruct plrPreprocess;

static struct devinfonode *getdevstr(struct devinfonode *n, const char *hnd)
{
	while (n)
	{
		if (!strcasecmp(n->handle, hnd))
			return n;
		n=n->next;
	}
	return 0;
}

static void setdevice(struct devinfonode **curdev, struct devinfonode *dev)
{
	if (*curdev==dev)
		return;
	if (*curdev)
	{
		if ((*curdev)->devinfo.devtype->addprocs)
			if ((*curdev)->devinfo.devtype->addprocs->Close)
				(*curdev)->devinfo.devtype->addprocs->Close();
		plrProcessKey=0;
		(*curdev)->devinfo.devtype->Close();
		if (!(*curdev)->keep)
		{
			lnkFree((*curdev)->linkhand);
			(*curdev)->linkhand=-1;
		}
	}
	(*curdev)=0;
	if (!dev)
		return;
	if (dev->linkhand<0)
	{
		char lname[22];
		strncpy(lname,cfGetProfileString(dev->handle, "link", ""),21);
		dev->linkhand=lnkLink(lname);
		if (dev->linkhand<0)
		{
			fprintf(stderr, "device load error\n");
			return;
		}
		dev->devinfo.devtype=(struct sounddevice *)_lnkGetSymbol(lnkReadInfoReg(dev->linkhand, "driver"));
		if (!dev->devinfo.devtype)
		{
			fprintf(stderr, "device symbol error\n");
			lnkFree(dev->linkhand);
			dev->linkhand=-1;
			return;
		}
	}
	fprintf(stderr, "%s selected...\n", dev->name);
	if (dev->devinfo.devtype->Init(&dev->devinfo))
	{
		if (dev->devinfo.devtype->addprocs)
			if (dev->devinfo.devtype->addprocs->Init)
				dev->devinfo.devtype->addprocs->Init(dev->handle);
		if (dev->devinfo.devtype->addprocs)
			if (dev->devinfo.devtype->addprocs->ProcessKey)
				plrProcessKey=dev->devinfo.devtype->addprocs->ProcessKey;
		(*curdev)=dev;
		return;
	}
	if (*curdev)
		if (!(*curdev)->keep)
		{
			lnkFree((*curdev)->linkhand);
			(*curdev)->linkhand=-1;
		}
	fprintf(stderr, "device init error\n");
}

static void plrSetDevice(const char *name, int def)
{
	setdevice(&curplaydev, getdevstr(plPlayerDevices, name));
	if (def)
		defplaydev=curplaydev;
}

static void plrResetDevice(void)
{
	setdevice(&curplaydev, defplaydev);
}

static struct dmDrive *dmSETUP;

static int playdevinit(void)
{
	const char *def;

#ifdef INITCLOSE_DEBUG
	fprintf(stderr, "playdevinit... trying to init all sound devices [sound]->playerdevices\n");
#endif

	mdbRegisterReadDir(&plrReadDirReg);

	plRegisterInterface (&plrIntr);
	plRegisterPreprocess (&plrPreprocess);

	dmSETUP=RegisterDrive("setup:");

	if (!strlen(cfGetProfileString2(cfSoundSec, "sound", "playerdevices", "")))
		return errOk;
	fprintf(stderr, "playerdevices:\n");
	if (!deviReadDevices(cfGetProfileString2(cfSoundSec, "sound", "playerdevices", ""), &plPlayerDevices))
	{
		fprintf(stderr, "could not install player devices!\n");
		return errGen;
	}

	curplaydev=0;
	defplaydev=0;

	def=cfGetProfileString("commandline_s", "p", cfGetProfileString2(cfSoundSec, "sound", "defplayer", ""));

	if (strlen(def))
		plrSetDevice(def, 1);
	else
		if (plPlayerDevices)
			plrSetDevice(plPlayerDevices->handle, 1);

	fprintf(stderr, "\n");

	plrBufSize=cfGetProfileInt2(cfSoundSec, "sound", "plrbufsize", 100, 10)*65;
	return errOk;
}

static void playdevclose(void)
{
#ifdef INITCLOSE_DEBUG
	fprintf(stderr, "playdevclose...\n");
#endif

	mdbUnregisterReadDir(&plrReadDirReg);

	plUnregisterInterface (&plrIntr);
	plUnregisterPreprocess (&plrPreprocess);

	setdevice(&curplaydev, 0);
	while (plPlayerDevices)
	{
		struct devinfonode *o=plPlayerDevices;
		plPlayerDevices=plPlayerDevices->next;
		free(o);
	}
}

static int plrReadDir(struct modlist *ml, const struct dmDrive *drive, const uint32_t path, const char *mask, unsigned long opt)
{
	struct modlistentry m;
	uint32_t node;

	if (drive!=dmSETUP)
		return 1;

	node = dirdbFindAndRef(dmSETUP->basepath, "DEVICES");

	if (opt&RD_PUTSUBS)
	{
		if (path==dmSETUP->basepath)
		{
			if (modlist_find(ml, node)<0)
			{
				memset(&m, 0, sizeof(m));
				m.drive=drive;
				strcpy(m.name, "DEVICES");
				strcpy(m.shortname, "DEVICES");
				m.dirdbfullpath=node;
				m.flags=MODLIST_FLAG_DIR;
				modlist_append(ml, &m);
			}
		}

	}

	if (path==node)
	{
		struct devinfonode *dev;
		for (dev=plPlayerDevices; dev; dev=dev->next)
		{
			char hnd[9];
			strcpy(hnd, dev->handle);
			memset(&m, 0, sizeof(m));

			fsConvFileName12(m.name, hnd, ".DEV");
/*
			if (fsMatchFileName12(m.name, mask))*/
			{
				char npath[64];
				m.fileref=mdbGetModuleReference(m.name, dev->devinfo.mem);
				if (m.fileref==0xffffffff)
					goto out;
				m.drive=drive;
				strncpy(m.shortname, m.name, 12);
				snprintf(npath, 64, "%s.DEV", hnd);
				m.dirdbfullpath=dirdbFindAndRef(path, npath);
				m.flags=MODLIST_FLAG_FILE|MODLIST_FLAG_VIRTUAL;
				if (mdbGetModuleType(m.fileref)!=mtDEVp)
				{
					struct moduleinfostruct mi;
					mdbGetModuleInfo(&mi, m.fileref);
					mi.flags1|=MDB_VIRTUAL;
					mi.channels=dev->devinfo.chan;
					strcpy(mi.modname, dev->name);
					mi.modtype=mtDEVp;
					mdbWriteModuleInfo(m.fileref, &mi);
				}
				modlist_append(ml, &m);
				dirdbUnref(m.dirdbfullpath);
			}
		}
	}
out:
	dirdbUnref(node);
	return 1;
}

static int plrSet(const char *path, struct moduleinfostruct *mi, FILE **fp)
{
	char name[9]; /* we never make names that are to big here */
	_splitpath(path, 0, 0, name, 0);
	plrSetDevice(name, 1);
	/*delay(1000); do we really need this ??? (doj)*/
	return 0;
}


static void plrPrep(const char *path, struct moduleinfostruct *m, FILE **fp)
{
	plrResetDevice();
}

static struct mdbreaddirregstruct plrReadDirReg = {plrReadDir MDBREADDIRREGSTRUCT_TAIL};
static struct interfacestruct plrIntr = {plrSet, 0, 0, "plrIntr" INTERFACESTRUCT_TAIL};
static struct preprocregstruct plrPreprocess = {plrPrep PREPROCREGSTRUCT_TAIL};
#ifndef SUPPORT_STATIC_PLUGINS
char *dllinfo = "";
#endif
DLLEXTINFO_PREFIX struct linkinfostruct dllextinfo = {"plrbase", "OpenCP Player Devices System (c) 1994-10 Niklas Beisert, Tammo Hinrichs", DLLVERSION, 0, Init: playdevinit, Close: playdevclose};
