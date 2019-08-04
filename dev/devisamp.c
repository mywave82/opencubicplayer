/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * Sampler devices system
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
#include <stdio.h>
#include <string.h>
#include "types.h"
#include "devisamp.h"
#include "boot/plinkman.h"
#include "boot/psetting.h"
#include "devigen.h"
#include "filesel/dirdb.h"
#include "filesel/mdb.h"
#include "filesel/modlist.h"
#include "filesel/pfilesel.h"
#include "imsdev.h"
#include "sampler.h"
#include "stuff/compat.h"
#include "stuff/err.h"

int (*smpProcessKey)(uint16_t);

unsigned char plsmpOpt;
unsigned short plsmpRate;

static struct devinfonode *plSamplerDevices;
static struct devinfonode *cursampdev;
static struct devinfonode *defsampdev;
static struct mdbreaddirregstruct smpReadDirReg;
static struct interfacestruct smpIntr;
static struct preprocregstruct smpPreprocess;

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

/*
static void setdevice(devinfonode *&curdev, devinfonode *dev)
{
  if (curdev==dev)
    return;
  if (curdev)
  {
    if (curdev->addprocs&&curdev->addprocs->Close)
      curdev->addprocs->Close();
    smpProcessKey=0;
    curdev->dev.dev->Close();
  }
  curdev=0;
  if (!dev)
    return;
  printf("%s selected...\n", dev->dev.dev->name);
  if (dev->dev.dev->Init(dev->dev))
  {
    if (dev->addprocs&&dev->addprocs->Init)
      dev->addprocs->Init(dev->handle);
    if (dev->addprocs&&dev->addprocs->ProcessKey)
      smpProcessKey=dev->addprocs->ProcessKey;
    curdev=dev;
    return;
  }
  printf("device init error\n");
}
*/
static void setdevice(struct devinfonode **curdev, struct devinfonode *dev)
{
	if (*curdev==dev)
		return;
	if (*curdev)
	{
		if ((*curdev)->devinfo.devtype->addprocs)
			if ((*curdev)->devinfo.devtype->addprocs->Close)
				(*curdev)->devinfo.devtype->addprocs->Close();
		smpProcessKey=0;
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
				smpProcessKey=dev->devinfo.devtype->addprocs->ProcessKey;
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


static void smpSetDevice(const char *name, int def)
{
	setdevice(&cursampdev, getdevstr(plSamplerDevices, name));
	if (def)
		defsampdev=cursampdev;
}

static void smpResetDevice(void)
{
	setdevice(&cursampdev, defsampdev);
}

static struct dmDrive *dmSETUP;

static int smpdevinited = 0;
static int sampdevinit(void)
{
	const char *def;
	int playrate;
	int playopt;

	smpdevinited = 1;

#ifdef INITCLOSE_DEBUG
	fprintf(stderr, "sampdevinit.... trying to find all samplers   [sound]->samplerdevices\n");
#endif

	mdbRegisterReadDir(&smpReadDirReg);

	plRegisterInterface (&smpIntr);
	plRegisterPreprocess (&smpPreprocess);

	dmSETUP=RegisterDrive("setup:");

	if (!strlen(cfGetProfileString2(cfSoundSec, "sound", "samplerdevices", "")))
		return errOk;
	fprintf(stderr, "samplerdevices:\n");
	if (!deviReadDevices(cfGetProfileString2(cfSoundSec, "sound", "samplerdevices", ""), &plSamplerDevices))
	{
		fprintf(stderr, "could not install sampler devices!\n");
		return errGen;
	}
	cursampdev=0;
	defsampdev=0;

	def=cfGetProfileString("commandline_s", "s", cfGetProfileString2(cfSoundSec, "sound", "defsampler", ""));

	if (strlen(def))
		smpSetDevice(def, 1);
	else
		if (plSamplerDevices)
			smpSetDevice(plSamplerDevices->handle, 1);

	fprintf(stderr, "\n");

	smpBufSize=cfGetProfileInt2(cfSoundSec, "sound", "smpbufsize", 100, 10)*65;

	playrate=cfGetProfileInt2(cfSoundSec, "sound", "samprate", 44100, 10);
	playrate=cfGetProfileInt("commandline_s", "r", playrate, 10);
	if (playrate<65)
	{
		if (playrate%11)
			playrate*=1000;
		else
			playrate=playrate*11025/11;
	}

	playopt=0;
	if (!cfGetProfileBool("commandline_s", "8", !cfGetProfileBool2(cfSoundSec, "sound", "samp16bit", 1, 1), 1))
		playopt|=SMP_16BIT;
	if (!cfGetProfileBool("commandline_s", "m", !cfGetProfileBool2(cfSoundSec, "sound", "sampstereo", 1, 1), 1))
		playopt|=SMP_STEREO;
	plsmpOpt=playopt;
	plsmpRate=playrate;

	if (!cursampdev)
	{
		fprintf (stderr, "Input device not set\n");
		return errGen;
	}

	return errOk;
}

static void sampdevclose(void)
{
#ifdef INITCLOSE_DEBUG
	fprintf(stderr, "sampdevclose...\n");
#endif
	if (smpdevinited)
	{
		mdbUnregisterReadDir(&smpReadDirReg);

		plUnregisterInterface (&smpIntr);
		plUnregisterPreprocess (&smpPreprocess);

		smpdevinited = 0;
	}

	setdevice(&cursampdev, 0);

	while (plSamplerDevices)
	{
		struct devinfonode *o=plSamplerDevices;
		plSamplerDevices=plSamplerDevices->next;
		free(o);
	}
}

static int smpReadDir(struct modlist *ml, const struct dmDrive *drive, const uint32_t path, const char *mask, unsigned long opt)
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
		for (dev=plSamplerDevices; dev; dev=dev->next)
		{
			char npath[64];
			snprintf (npath, sizeof(npath), "%s.DEV", dev->handle);
			memset(&m, 0, sizeof(m));

			fsConvFileName12 (m.shortname, dev->handle, ".DEV");
//			if (fsMatchFileName12(m.shortname, mask))
			{
				m.mdb_ref=mdbGetModuleReference(m.shortname, dev->devinfo.mem);
				if (m.mdb_ref==0xffffffff)
					goto out;
				m.drive=drive;
				m.dirdbfullpath=dirdbFindAndRef(path, npath);
				m.flags=MODLIST_FLAG_FILE|MODLIST_FLAG_VIRTUAL;
				if (mdbGetModuleType(m.mdb_ref)!=mtDEVs)
				{
					struct moduleinfostruct mi;
					mdbGetModuleInfo(&mi, m.mdb_ref);
					mi.flags1|=MDB_VIRTUAL;
					mi.channels=dev->devinfo.chan;
					strcpy(mi.modname, dev->name);
					mi.modtype=mtDEVs;
					mdbWriteModuleInfo(m.mdb_ref, &mi);
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

static int smpSet(const uint32_t dirdbref, struct moduleinfostruct *mi, FILE **fp)
{
	char *path, *name;

	if (mi->modtype != mtDEVs)
	{
		return 0;
	}

	dirdbGetName_internalstr (dirdbref, &path);
	splitpath_malloc (path, 0, 0, &name, 0);

	smpSetDevice(name, 1);

	free (name);

	return 0;
}

static void smpPrep(const uint32_t dirdbref, struct moduleinfostruct *m, FILE **fp)
{
	smpResetDevice();
}

static struct mdbreaddirregstruct smpReadDirReg = {smpReadDir MDBREADDIRREGSTRUCT_TAIL};
static struct interfacestruct smpIntr = {smpSet, 0, 0, "smpIntr" INTERFACESTRUCT_TAIL};
static struct preprocregstruct smpPreprocess = {smpPrep PREPROCREGSTRUCT_TAIL};
#ifndef SUPPORT_STATIC_PLUGINS
char *dllinfo = "";
#endif
DLLEXTINFO_PREFIX struct linkinfostruct dllextinfo = {.name = "smpbase", .desc = "OpenCP Sampler Devices System (c) 1994-10 Niklas Beisert, Tammo Hinrichs", .ver = DLLVERSION, .size = 0, .Init = sampdevinit, .Close = sampdevclose};
