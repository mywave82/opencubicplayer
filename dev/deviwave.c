/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * Wavetable devices system
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

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "types.h"
#include "deviwave.h"
#include "filesel/dirdb.h"
#include "filesel/mdb.h"
#include "filesel/modlist.h"
#include "filesel/pfilesel.h"
#include "imsdev.h"
#include "player.h"
#include "boot/psetting.h"
#include "boot/plinkman.h"
#include "stuff/err.h"
#include "devigen.h"
#include "mcp.h"
#include "stuff/compat.h"

int (*mcpProcessKey)(uint16_t);

static struct devinfonode *plWaveTableDevices;
static struct devinfonode *curwavedev;
static struct devinfonode *defwavedev;
static struct mdbreaddirregstruct mcpReadDirReg;
static struct interfacestruct mcpIntr;
static struct preprocregstruct mcpPreprocess;

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
		mcpProcessKey=0;
		(*curdev)->devinfo.devtype->Close();
		if (!(*curdev)->keep)
		{
			lnkFree((*curdev)->linkhand);
			(*curdev)->linkhand=-1;
		}
	}
	*curdev=0;
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
				mcpProcessKey=dev->devinfo.devtype->addprocs->ProcessKey;
		*curdev=dev;
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


static void mcpSetDevice(const char *name, int def)
{
	setdevice(&curwavedev, getdevstr(plWaveTableDevices, name));
	if (def)
		defwavedev=curwavedev;
}

static void mcpResetDevice(void)
{
	setdevice(&curwavedev, defwavedev);
}

static struct dmDrive *dmSETUP;

static int wavedevinit(void)
{
	const char *def;
	int playrate;

#ifdef INITCLOSE_DEBUG
	fprintf(stderr, "wavedevinit... trying to find all wavetables    [sound]->wavetabledevices\n");
#endif

	mdbRegisterReadDir(&mcpReadDirReg);

	plRegisterInterface (&mcpIntr);
	plRegisterPreprocess (&mcpPreprocess);

	dmSETUP=RegisterDrive("setup:");

	if (!strlen(cfGetProfileString2(cfSoundSec, "sound", "wavetabledevices", "")))
		return errOk;
	fprintf(stderr, "wavetabledevices:\n");
	if (!deviReadDevices(cfGetProfileString2(cfSoundSec, "sound", "wavetabledevices", ""), &plWaveTableDevices))
	{
		fprintf(stderr, "could not install wavetable devices!\n");
		return errGen;
	}

	curwavedev=0;
	defwavedev=0;

	def=cfGetProfileString("commandline_s", "w", cfGetProfileString2(cfSoundSec, "sound", "defwavetable", ""));

	if (strlen(def))
		mcpSetDevice(def, 1);
	else
		if (plWaveTableDevices)
			mcpSetDevice(plWaveTableDevices->handle, 1);

	fprintf(stderr, "\n");

	playrate=cfGetProfileInt("commandline_s", "r", cfGetProfileInt2(cfSoundSec, "sound", "mixrate", 44100, 10), 10);
	if (playrate<66)
	{
		if (playrate%11)
			playrate*=1000;
		else
			playrate=playrate*11025/11;
	}
	mcpMixOpt=0;
	if (!cfGetProfileBool("commandline_s", "8", !cfGetProfileBool2(cfSoundSec, "sound", "mix16bit", 1, 1), 1))
		mcpMixOpt|=PLR_16BIT;
	if (!cfGetProfileBool("commandline_s", "m", !cfGetProfileBool2(cfSoundSec, "sound", "mixstereo", 1, 1), 1))
		mcpMixOpt|=PLR_STEREO;
	mcpMixMaxRate=playrate;
	mcpMixProcRate=cfGetProfileInt2(cfSoundSec, "sound", "mixprocrate", 1536000, 10);
	mcpMixBufSize=cfGetProfileInt2(cfSoundSec, "sound", "mixbufsize", 100, 10)*65;
	mcpMixPoll=mcpMixBufSize;
	mcpMixMax=mcpMixBufSize;

	return errOk;
}

static void wavedevclose(void)
{
#ifdef INITCLOSE_DEBUG
	fprintf(stderr, "wavedevclose....\n");
#endif

	mdbUnregisterReadDir(&mcpReadDirReg);

	plUnregisterInterface (&mcpIntr);
	plUnregisterPreprocess (&mcpPreprocess);

	setdevice(&curwavedev, 0);
	while (plWaveTableDevices)
	{
		struct devinfonode *o=plWaveTableDevices;
		plWaveTableDevices=plWaveTableDevices->next;
		free(o);
	}
}

static int mcpReadDir(struct modlist *ml, const struct dmDrive *drive, const uint32_t path, const char *mask, unsigned long opt)
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
		for (dev=plWaveTableDevices; dev; dev=dev->next)
		{
			char hnd[9];
			strcpy(hnd, dev->handle);
			memset(&m, 0, sizeof(m));

			fsConvFileName12(m.name, hnd, ".DEV");
/*
			if (fsMatchFileName12(m.name, mask))
*/
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
				if (mdbGetModuleType(m.fileref)!=mtDEVw)
				{
					struct moduleinfostruct mi;
					mdbGetModuleInfo(&mi, m.fileref);
					mi.flags1|=MDB_VIRTUAL;
					mi.channels=dev->devinfo.chan;
					strcpy(mi.modname, dev->name);
					mi.modtype=mtDEVw;
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

static int mcpSetDev(const char *path, struct moduleinfostruct *mi, FILE **fp)
{
	char name[9]; /* we never make names that are too long */
	_splitpath(path, 0, 0, name, 0);
	mcpSetDevice(name, 1);
	/*delay(1000); do we really need this ??? (doj)*/
	return 0;
}

static void mcpPrep(const char *path, struct moduleinfostruct *info, FILE **bp)
{
	mcpResetDevice();
/*
	if (info->gen.moduleflags&MDB_BIGMODULE)           TODO
		mcpSetDevice(cfGetProfileString2(cfSoundSec, "sound", "bigmodules", ""), 0);
*/
}


static struct mdbreaddirregstruct mcpReadDirReg = {mcpReadDir MDBREADDIRREGSTRUCT_TAIL};
static struct interfacestruct mcpIntr = {mcpSetDev, 0, 0, "mcpIntr" INTERFACESTRUCT_TAIL};
static struct preprocregstruct mcpPreprocess = {mcpPrep PREPROCREGSTRUCT_TAIL};
#ifndef SUPPORT_STATIC_PLUGINS
char *dllinfo = "";
#endif
DLLEXTINFO_PREFIX struct linkinfostruct dllextinfo = {.name = "mcpbase", .desc = "OpenCP Wavetable Devices System (c) 1994-10 Niklas Beisert, Tammo Hinrichs", .ver = DLLVERSION, .size = 0, .Init = wavedevinit, .Close = wavedevclose};
