/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) '04-'21 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Module information DataBase (and some other related stuff=
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
 *  -ss04??????   Stian Skjelstad <stian@nixia.no
 *    -first release (splited out from pfilesel.c)
 */

#include "config.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "types.h"
#include "boot/plinkman.h"
#include "boot/psetting.h"
#include "dirdb.h"
#include "filesystem.h"
#include "mdb.h"
#include "pfilesel.h"
#include "stuff/compat.h"
#include "stuff/imsrtns.h"

#ifdef MDB_DEBUG
#define DEBUG_PRINT(...) do { fprintf(stderr, __VA_ARGS__); } while (0)
#else
#define DEBUG_PRINT(...) {}
#endif

struct __attribute__((packed)) modinfoentry
{
	uint8_t flags;
	union
	{
		struct __attribute__((packed))
		{
			uint8_t modtype;    /*  1 */
			uint32_t comref;    /*  5 */
			uint32_t compref;   /*  9 */
			uint32_t futref;    /* 13 */
			char name[12];      /* 25 */
			uint32_t size;      /* 29 */
			char modname[32];   /* 61 */
			uint32_t date;      /* 65 */
			uint16_t playtime;  /* 67 */
			uint8_t channels;   /* 68 */
			uint8_t moduleflags;/* 69 */
			/* last uint8_t flags2 is up-padding for the top uint8_t and so on.. */
		} gen;

		struct __attribute__((packed))
		{
			char unusedfill1[6]; /*  1 */
			char comment[63];
		} com;

		struct __attribute__((packed))
		{
			char composer[32];
			char style[31];
		} comp;
	} mie;
};
#define gen mie.gen
#define comp mie.comp

struct __attribute__((packed)) mdbheader
{
	char sig[60];
	uint32_t entries;
};
const char mdbsigv1[60] = "Cubic Player Module Information Data Base\x1B\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";

static struct modinfoentry *mdbData;
static uint32_t mdbNum;
static int mdbDirty;
static uint32_t *mdbReloc;
static uint32_t mdbGenNum;
static uint32_t mdbGenMax;

const char *mdbGetModTypeString(unsigned char type)
{
	return fsTypeNames[type&UINT8_MAX];
}

int mdbGetModuleType(uint32_t mdb_ref)
{
	if (mdb_ref>=mdbNum)
		return -1;
	if ((mdbData[mdb_ref].flags&(MDB_USED|MDB_BLOCKTYPE))!=(MDB_USED|MDB_GENERAL))
		return -1;
	return mdbData[mdb_ref].gen.modtype;
}

uint8_t mdbReadModType(const char *str)
{
	int v=255;
	int i;
	for (i=0; i<256; i++)
		if (!strcasecmp(str, fsTypeNames[i]))
			v=i;
	return v;
}


int mdbInfoRead(uint32_t mdb_ref)
{
	if (mdb_ref>=mdbNum)
	{
		DEBUG_PRINT ("mdbInfoRead(0x%08"PRIx32") => -1 due to out of range\n", mdb_ref);
		return -1;
	}
	if ((mdbData[mdb_ref].flags&(MDB_USED|MDB_BLOCKTYPE))!=(MDB_USED|MDB_GENERAL))
	{
		DEBUG_PRINT ("mdbInfoRead(0x%08"PRIx32") => -1 due to entry not being USED + GENERAL\n", mdb_ref);
		return -1;
	}
#ifdef MDB_DEBUG
	if (mdbData[mdb_ref].gen.modtype==mtUnRead)
	{
		DEBUG_PRINT ("mdbInfoRead(0x%08"PRIx32") => 0 due to mtUnRead\n", mdb_ref);
	} else {
		DEBUG_PRINT ("mdbInfoRead(0x%08"PRIx32") => 1 due to modtype != mtUnRead\n", mdb_ref);
	}
#endif
	return mdbData[mdb_ref].gen.modtype!=mtUnRead;
}

/* This thing will end up with a register of all valid pre-interprators for modules and friends
 */
static struct mdbreadinforegstruct *mdbReadInfos=NULL;

void mdbRegisterReadInfo(struct mdbreadinforegstruct *r)
{
	r->next=mdbReadInfos;
	mdbReadInfos=r;
	if (r->Event)
		r->Event(mdbEvInit);
}

void mdbUnregisterReadInfo(struct mdbreadinforegstruct *r)
{
	struct mdbreadinforegstruct *root=mdbReadInfos;
	if (root==r)
	{
		mdbReadInfos=r->next;
		return;
	}
	while (root)
	{
		if (root->next==r)
		{
			root->next=root->next->next;
			return;
		}
		if (!root->next)
			return;
		root=root->next;
	}
}

int mdbReadMemInfo(struct moduleinfostruct *m, const char *buf, int len)
{
	struct mdbreadinforegstruct *rinfos;

	DEBUG_PRINT ("mdbReaadMemInfo(buf=%p len=%d)\n", buf, len);

	for (rinfos=mdbReadInfos; rinfos; rinfos=rinfos->next)
		if (rinfos->ReadMemInfo)
			if (rinfos->ReadMemInfo(m, buf, len))
				return 1;
	return 0;
}

int mdbReadInfo(struct moduleinfostruct *m, struct ocpfilehandle_t *f)
{
	char mdbScanBuf[1084];
	struct mdbreadinforegstruct *rinfos;
	int maxl;

	DEBUG_PRINT ("mdbReadInfo(f=%p)\n", f);

	if (f->seek_set (f, 0) < 0)
	{
		return 1;
	}
	memset (mdbScanBuf, 0, sizeof (mdbScanBuf));
	maxl = f->read (f, mdbScanBuf, sizeof (mdbScanBuf));

	{
		char *path;
		dirdbGetName_internalstr (f->dirdb_ref, &path);
		DEBUG_PRINT ("   mdbReadMemInfo(%s %p %d)\n", path, mdbScanBuf, maxl);
	}

	if (mdbReadMemInfo(m, mdbScanBuf, maxl))
		return 1;

	for (rinfos=mdbReadInfos; rinfos; rinfos=rinfos->next)
		if (rinfos->ReadInfo)
			if (rinfos->ReadInfo(m, f, mdbScanBuf, maxl))
				return 1;

	return m->modtype==mtUnRead;
}

static uint32_t mdbGetNew(void)
{
	uint32_t i;

	for (i=0; i<mdbNum; i++)
		if (!(mdbData[i].flags&MDB_USED))
			break;
	if (i==mdbNum)
	{
		void *t;
		uint32_t j;
		mdbNum+=64;
		if (!(t=realloc(mdbData, mdbNum*sizeof(*mdbData))))
			return UINT32_MAX;
		mdbData=(struct modinfoentry *)t;
		memset(mdbData+i, 0, (mdbNum-i)*sizeof(*mdbData));
		for (j=i; j<mdbNum; j++)
			mdbData[j].flags|=MDB_DIRTY;
	}
	mdbDirty=1;

	DEBUG_PRINT("mdbGetNew() => 0x%08"PRIx32"\n", i);

	return i;
}

int mdbWriteModuleInfo(uint32_t mdb_ref, struct moduleinfostruct *m)
{
	DEBUG_PRINT("mdbWriteModuleInfo(0x%"PRIx32", %p)\n", mdb_ref, m);

	if (mdb_ref>=mdbNum)
	{
		DEBUG_PRINT ("mdbWriteModuleInfo, mdb_ref(%d)<mdbNum(%d)\n", mdb_ref, mdbNum);
		return 0;
	}
	if ((mdbData[mdb_ref].flags&(MDB_USED|MDB_BLOCKTYPE))!=(MDB_USED|MDB_GENERAL))
	{
		DEBUG_PRINT ("mdbWriteModuleInfo (mdbData[mdb_ref].flags&(MDB_USED|MDB_BLOCKTYPE))!=(MDB_USED|MDB_GENERAL) Failed\n");
		return 0;
	}

	m->flags1=MDB_USED|MDB_DIRTY|MDB_GENERAL|(m->flags1&(MDB_VIRTUAL|MDB_BIGMODULE|MDB_RESERVED));
	m->flags2=MDB_DIRTY|MDB_COMPOSER;
	m->flags3=MDB_DIRTY|MDB_COMMENT;
	m->flags4=MDB_DIRTY|MDB_FUTURE;

	if (*m->composer||*m->style)
		m->flags2|=MDB_USED;
	if (*m->comment)
		m->flags3|=MDB_USED;

	/* free the old references */
	if (m->comref!=UINT32_MAX)
		mdbData[m->comref].flags=MDB_DIRTY;
	if (m->compref!=UINT32_MAX)
		mdbData[m->compref].flags=MDB_DIRTY;
	if (m->futref!=UINT32_MAX)
		mdbData[m->futref].flags=MDB_DIRTY;
	m->compref=UINT32_MAX;
	m->comref=UINT32_MAX;
	m->futref=UINT32_MAX;

	/* allocate new ones */
	if (m->flags2&MDB_USED)
	{
		m->compref=mdbGetNew();
		if (m->compref!=UINT32_MAX)
			memcpy(mdbData+m->compref, &m->flags2, sizeof(*mdbData));
	}
	if (m->flags3&MDB_USED)
	{
		m->comref=mdbGetNew();
		if (m->comref!=UINT32_MAX)
			memcpy(mdbData+m->comref, &m->flags3, sizeof(*mdbData));
	}
	if (m->flags4&MDB_USED)
	{
		m->futref=mdbGetNew();
		if (m->futref!=UINT32_MAX)
			memcpy(mdbData+m->futref, &m->flags4, sizeof(*mdbData));
	}

	memcpy(mdbData+mdb_ref, m, sizeof(*mdbData));
	mdbDirty=1;
	return 1;
}

void mdbScan(struct ocpfile_t *file, uint32_t mdb_ref)
{
	DEBUG_PRINT ("mdbScan(file=%p, mdb_ref=0x%08"PRIx32")\n", file, mdb_ref);
	if (!file)
	{
		return;
	}

	if (file->is_nodetect)
	{
		return;
	}

	if (!mdbInfoRead(mdb_ref)) /* use mdbReadInfo again here ? */
	{
		struct moduleinfostruct mdbEditBuf;
		struct ocpfilehandle_t *f;
		if (!(f=file->open(file)))
		{
			return;
		}
		mdbGetModuleInfo(&mdbEditBuf, mdb_ref);
		mdbReadInfo(&mdbEditBuf, f);
		f->unref (f);
		mdbWriteModuleInfo(mdb_ref, &mdbEditBuf);
	}
}

static int miecmp(const void *a, const void *b)
{
	struct modinfoentry *c=&mdbData[*(uint32_t *)a];
	struct modinfoentry *d=&mdbData[*(uint32_t *)b];
	if (c->gen.size==d->gen.size)
		return memcmp(c->gen.name, d->gen.name, 12);
	if (c->gen.size<d->gen.size)
		return -1;
	else
		return 1;
}

int mdbInit(void)
{
	char *path;
	int f;
	struct mdbheader header;
	uint32_t i;

	mdbDirty=0;
	mdbData=0;
	mdbNum=0;
	mdbReloc=0;
	mdbGenNum=0;
	mdbGenMax=0;

	makepath_malloc (&path, 0, cfConfigDir, "CPMODNFO.DAT", 0);

	if ((f=open(path, O_RDONLY))<0)
	{
		fprintf (stderr, "open(%s): %s\n", path, strerror (errno));
		free (path);
		return 1;
	}

	fprintf(stderr, "Loading %s .. ", path);
	free (path); path = 0;

	if (read(f, &header, sizeof(header))!=sizeof(header))
	{
		fprintf(stderr, "No header\n");
		close(f);
		return 1;
	}

	if (memcmp(header.sig, mdbsigv1, sizeof(mdbsigv1)))
	{
		fprintf(stderr, "Invalid header\n");
		close(f);
		return 1;
	}

	mdbNum=uint32_little(header.entries);
	if (!mdbNum)
	{
		close(f);
		fprintf(stderr, "EOF\n");
		return 1;
	}

	mdbData=malloc(sizeof(struct modinfoentry)*mdbNum);
	if (!mdbData)
		return 0;
	if (read(f, mdbData, mdbNum*sizeof(*mdbData))!=(signed)(mdbNum*sizeof(*mdbData)))
	{
		mdbNum=0;
		free(mdbData);
		mdbData=0;
		close(f);
		fprintf(stderr, "EOF\n");
		return 1;
	}
	close(f);

	for (i=0; i<mdbNum; i++)
	{
		if ((mdbData[i].flags&(MDB_BLOCKTYPE|MDB_USED))==(MDB_USED|MDB_GENERAL))
		{
			DEBUG_PRINT("0x%08"PRIx32" is USED GENERAL\n", i);
			mdbGenMax++;
		}
	}

	if (mdbGenMax)
	{
		mdbReloc=malloc(sizeof(uint32_t)*mdbGenMax);
		if (!mdbReloc)
			return 0;
		for (i=0; i<mdbNum; i++)
			if ((mdbData[i].flags&(MDB_BLOCKTYPE|MDB_USED))==(MDB_USED|MDB_GENERAL))
				mdbReloc[mdbGenNum++]=i;

		qsort(mdbReloc, mdbGenNum, sizeof(*mdbReloc), miecmp);
#ifdef MDB_DEBUG
		for (i=0; i<mdbGenMax; i++)
		{
			DEBUG_PRINT("%5"PRId32" => 0x%08"PRIx32" %"PRId32" %c%c%c%c%c%c%c%c%c%c%c%c\n", i, mdbReloc[i], mdbData[mdbReloc[i]].gen.size, mdbData[mdbReloc[i]].gen.name[0], mdbData[mdbReloc[i]].gen.name[1], mdbData[mdbReloc[i]].gen.name[2], mdbData[mdbReloc[i]].gen.name[3], mdbData[mdbReloc[i]].gen.name[4], mdbData[mdbReloc[i]].gen.name[5], mdbData[mdbReloc[i]].gen.name[6], mdbData[mdbReloc[i]].gen.name[7], mdbData[mdbReloc[i]].gen.name[8], mdbData[mdbReloc[i]].gen.name[9], mdbData[mdbReloc[i]].gen.name[10], mdbData[mdbReloc[i]].gen.name[11]);
		}
#endif
	}

	fprintf(stderr, "Done\n");

	return 1;
}

void mdbUpdate(void)
{
	char *path;
	int f;
	uint32_t i, j;
	struct mdbheader header;

	DEBUG_PRINT("mdbUpdate: mdbDirty=%d fsWriteModInfo=%d\n", mdbDirty, fsWriteModInfo);

	if (!mdbDirty||!fsWriteModInfo)
		return;
	mdbDirty=0;

	makepath_malloc (&path, 0, cfConfigDir, "CPMODNFO.DAT", 0);
	if ((f=open(path, O_WRONLY|O_CREAT, S_IREAD|S_IWRITE))<0)
	{
		fprintf (stderr, "open(%s): %s\n", path, strerror (errno));
		free (path);
		return;
	}

	lseek(f, 0, SEEK_SET);
	memcpy(header.sig, mdbsigv1, sizeof(mdbsigv1));
	header.entries = uint32_little(mdbNum);
	while (1)
	{
		ssize_t res;
		res = write(f, &header, sizeof(header));
		if (res < 0)
		{
			if (errno==EAGAIN)
				continue;
			if (errno==EINTR)
				continue;
			fprintf(stderr, __FILE__ " write() to %s failed: %s\n", path, strerror(errno));
			exit(1);
		} else if (res != sizeof(header))
		{
			fprintf(stderr, __FILE__ " write() to %s returned only partial data\n", path);
			exit(1);
		} else
			break;
	}
	i=0;
	while (i<mdbNum)
	{
		if (!(mdbData[i].flags&MDB_DIRTY))
		{
			i++;
			continue;
		}
		for (j=i; j<mdbNum; j++)
			if (mdbData[j].flags&MDB_DIRTY)
				mdbData[j].flags&=~MDB_DIRTY;
			else
				break;
		lseek(f, (uint64_t)64+(uint64_t)i*sizeof(*mdbData), SEEK_SET);
		while (1)
		{
			ssize_t res;

			DEBUG_PRINT("  [0x%08"PRIx32" -> 0x%08"PRIx32"] DIRTY\n", i, j);
			res = write(f, mdbData+i, (j-i)*sizeof(*mdbData));
			if (res < 0)
			{
				if (errno==EAGAIN)
					continue;
				if (errno==EINTR)
					continue;
				fprintf(stderr, __FILE__ " write() to %s failed: %s\n", path, strerror(errno));
				exit(1);
			} else if (res != (signed)((j-i)*sizeof(*mdbData)))
			{
				fprintf(stderr, __FILE__ " write() to %s returned only partial data\n", path);
				exit(1);
			} else
				break;
		}
		i=j;
	}
	free (path);
	lseek(f, 0, SEEK_END);
	close(f);
}

void mdbClose(void)
{
	mdbUpdate();
	free(mdbData);
	free(mdbReloc);
}

static uint32_t mdbGetModuleReference(const char *name, uint32_t size)
{
	uint32_t i;

	uint32_t *min=mdbReloc;
	uint32_t num=mdbGenNum;
	uint32_t mn;
	struct modinfoentry *m;

	/* Iterate fast.. If size to to big, set current to be the minimum, and
	 * set the current to point in the new center, else half the current.
	 *
	 * That code is VERY clever. Took me some minutes to read it :-) nice
	 * work guys
	 *   - Stian
	 */

	DEBUG_PRINT("mdbGetModuleReference(%s %"PRId32")\n", name, size);
	while (num)
	{
		struct modinfoentry *m=&mdbData[min[num>>1]];
		int ret;
#ifdef MDB_DEBUG
		{
			uint32_t x;
			for (x = 0; x < num; x++)
			{
				DEBUG_PRINT("  %08x %"PRId32" %c%c%c%c%c%c%c%c%c%c%c%c\n",
					min[x],
					mdbData[min[x]].gen.size,
					mdbData[min[x]].gen.name[0],
					mdbData[min[x]].gen.name[1],
					mdbData[min[x]].gen.name[2],
					mdbData[min[x]].gen.name[3],
					mdbData[min[x]].gen.name[4],
					mdbData[min[x]].gen.name[5],
					mdbData[min[x]].gen.name[6],
					mdbData[min[x]].gen.name[7],
					mdbData[min[x]].gen.name[8],
					mdbData[min[x]].gen.name[9],
					mdbData[min[x]].gen.name[10],
					mdbData[min[x]].gen.name[11]);
			}
			DEBUG_PRINT("----------\n");
		}
#endif
		if (size==m->gen.size)
			ret=memcmp(name, m->gen.name, 12);
		else
			if (size<m->gen.size)
				ret=-1;
			else
				ret=1;
		if (!ret)
		{
			DEBUG_PRINT("mdbGetModuleReference(%s %"PRId32") => mdbReloc => 0x%08"PRIx32"\n", name, size, min[num>>1]);
			return min[num>>1];
		}
		if (ret<0)
			num>>=1;
		else {
			min+=(num>>1)+1;
			num=(num-1)>>1;
		}
	}
	mn=min-mdbReloc;

	i=mdbGetNew();
	if (i==UINT32_MAX)
		return UINT32_MAX;
	if (mdbGenNum==mdbGenMax)
	{
		void *n;
		mdbGenMax+=512;
		if (!(n=realloc(mdbReloc, sizeof (*mdbReloc)*mdbGenMax)))
			return UINT32_MAX;
		mdbReloc=(uint32_t *)n;
	}

	memmovel(mdbReloc+mn+1, mdbReloc+mn, mdbGenNum-mn);
	mdbReloc[mn]=(uint32_t)i;
	mdbGenNum++;

	m=&mdbData[i];
	m->flags=MDB_DIRTY|MDB_USED|MDB_GENERAL;
	memcpy(m->gen.name, name, 12);
	m->gen.size=size;
	m->gen.modtype=UINT8_MAX;
	m->gen.comref=UINT32_MAX;
	m->gen.compref=UINT32_MAX;
	m->gen.futref=UINT32_MAX;
	memset(m->gen.modname, 0, 32);
	m->gen.date=0;
	m->gen.playtime=0;
	m->gen.channels=0;
	m->gen.moduleflags=0;
	mdbDirty=1;
	DEBUG_PRINT("mdbGetModuleReference(%s %"PRId32") => new => 0x%08"PRIx32"\n", name, size, i);
	return (uint32_t)i;
}

#warning Remake the hash to acculate overflow characters into the character 6 and 7... it will break old databases
uint32_t mdbGetModuleReference2 (uint32_t dirdb_ref, uint32_t size)
{
	char shortname[13];
	char *temppath;
	char *lastdot;
	int length;

	dirdbGetName_internalstr (dirdb_ref, &temppath);
	if (!temppath)
	{
		return DIRDB_NOPARENT;
	}

	/* the "hash" is created using the former fs12name() function */
	length = strlen (temppath);

	shortname[12] = 0;
	if ((lastdot=rindex(temppath + 1, '.'))) /* we allow files to start with, hence temppath + 1 */
	{
		/* delta is the length until the dot */
		int delta = lastdot - temppath;

		if ((delta) < 8)
		{ /* if the text before the dot is shorter than 8, pad it with spaces */
			strncpy (shortname,         temppath,   delta);
			strncpy (shortname + delta, "        ", 8 - delta);
		} else { /* or we take only the first 8 characters */
			strncpy (shortname, temppath, 8);
		}

		/* grab the dot, and upto 3 characters following it */
		if (strlen (lastdot) < 4)
		{ /* if the text including the dot is shorter than 4, pad it with spaces */
			strcpy  (shortname + 8,                    lastdot);
			strncpy (shortname + 8 + strlen (lastdot), "   ", 4 - strlen(lastdot));
		} else { /* or we take only the first 4 characters,   eg .foo  instead of .foobar, and also accept things like .mod as is */
			strncpy (shortname + 8, lastdot, 4);
		}
	} else { /* we would normally never HASH such a filename */
		strncpy(shortname, temppath, 12);
		if ((length=strlen(temppath))<12)
		{
			strncpy(shortname+length, "            ", 12-length);
		}
	}

	return mdbGetModuleReference (shortname, size);
}

int mdbGetModuleInfo(struct moduleinfostruct *m, uint32_t mdb_ref)
{
	memset(m, 0, sizeof(struct moduleinfostruct));
	if (mdb_ref>=mdbNum) /* needed, since we else might index mdbData wrong */
		goto invalid;
	if ((mdbData[mdb_ref].flags&(MDB_USED|MDB_BLOCKTYPE))!=(MDB_USED|MDB_GENERAL))
	{
invalid:
		m->modtype=UINT8_MAX;
		m->comref=UINT32_MAX;
		m->compref=UINT32_MAX;
		m->futref=UINT32_MAX;
		return 0;
	}
	memcpy(m, mdbData+mdb_ref, sizeof(*mdbData));
	if (m->compref!=UINT32_MAX)
	{
		if ((m->compref < mdbNum) && ((mdbData[m->compref].flags & MDB_BLOCKTYPE) == MDB_COMPOSER))
		{
			memcpy(&m->flags2, mdbData+m->compref, sizeof(*mdbData));
		} else {
			fprintf (stderr, "[mdb] warning - invalid compref\n");
			m->compref=UINT32_MAX;
		}
	}
	if (m->comref!=UINT32_MAX)
	{
		if ((m->comref < mdbNum) && ((mdbData[m->comref].flags & MDB_BLOCKTYPE) == MDB_COMMENT))
		{
			memcpy(&m->flags3, mdbData+m->comref, sizeof(*mdbData));
		} else {
			fprintf (stderr, "[mdb] warning - invalid comref\n");
			m->comref=UINT32_MAX;
		}
	}
	if (m->futref!=UINT32_MAX)
	{
		if ((m->futref < mdbNum) && ((mdbData[m->comref].flags & MDB_BLOCKTYPE) == MDB_FUTURE))
		{
			memcpy(&m->flags4, mdbData+m->futref, sizeof(*mdbData));
		} else {
			fprintf (stderr, "[mdb] warning - invalid futref\n");
			m->futref=UINT32_MAX;
		}
	}
	return 1;
}
