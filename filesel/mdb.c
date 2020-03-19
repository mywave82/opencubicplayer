/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
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
#include "mdb.h"
#include "modlist.h"
#include "pfilesel.h"
#include "stuff/compat.h"
#include "stuff/imsrtns.h"

struct mdbreaddirregstruct *mdbReadDirs = 0;

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

int fsReadDir(struct modlist *ml, const struct dmDrive *drive, const uint32_t path, const char *mask, unsigned long opt)
{
	struct mdbreaddirregstruct *readdirs;
	for (readdirs=mdbReadDirs; readdirs; readdirs=readdirs->next)
		if (!readdirs->ReadDir(ml, drive, path, mask, opt))
			return 0;
	return 1;
}

const char *mdbGetModTypeString(unsigned char type)
{
	return fsTypeNames[type&0xFF];
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
		return -1;
	if ((mdbData[mdb_ref].flags&(MDB_USED|MDB_BLOCKTYPE))!=(MDB_USED|MDB_GENERAL))
		return -1;
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

	for (rinfos=mdbReadInfos; rinfos; rinfos=rinfos->next)
		if (rinfos->ReadMemInfo)
			if (rinfos->ReadMemInfo(m, buf, len))
				return 1;
	return 0;
}


int mdbReadInfo(struct moduleinfostruct *m, FILE *f)
{
	char mdbScanBuf[1084];
	struct mdbreadinforegstruct *rinfos;
	int maxl;

	memset(mdbScanBuf, 0, 1084);
	maxl=1084;
	maxl=fread(mdbScanBuf, 1, maxl, f);

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
			return 0xFFFFFFFF;
		mdbData=(struct modinfoentry *)t;
		memset(mdbData+i, 0, (mdbNum-i)*sizeof(*mdbData));
		for (j=i; j<mdbNum; j++)
			mdbData[j].flags|=MDB_DIRTY;
	}
	mdbDirty=1;
	return i;
}

int mdbWriteModuleInfo(uint32_t mdb_ref, struct moduleinfostruct *m)
{
	if (mdb_ref>=mdbNum)
	{
		fprintf(stderr, "mdbWriteModuleInfo, mdb_ref(%d)<mdbNum(%d)\n", mdb_ref, mdbNum);
		return 0;
	}
	if ((mdbData[mdb_ref].flags&(MDB_USED|MDB_BLOCKTYPE))!=(MDB_USED|MDB_GENERAL))
	{
		 fprintf(stderr, "mdbWriteModuleInfo (mdbData[mdb_ref].flags&(MDB_USED|MDB_BLOCKTYPE))!=(MDB_USED|MDB_GENERAL) Failed\n");
		return 0;
	}

	m->flags1=MDB_USED|MDB_DIRTY|MDB_GENERAL|(m->flags1&(MDB_VIRTUAL|MDB_BIGMODULE|MDB_PLAYLIST));
	m->flags2=MDB_DIRTY|MDB_COMPOSER;
	m->flags3=MDB_DIRTY|MDB_COMMENT;
	m->flags4=MDB_DIRTY|MDB_FUTURE;

	if (*m->composer||*m->style)
		m->flags2|=MDB_USED;
	if (*m->comment)
		m->flags3|=MDB_USED;

	/* free the old references */
	if (m->comref!=0xFFFFFFFF)
		mdbData[m->comref].flags=MDB_DIRTY;
	if (m->compref!=0xFFFFFFFF)
		mdbData[m->compref].flags=MDB_DIRTY;
	if (m->futref!=0xFFFFFFFF)
		mdbData[m->futref].flags=MDB_DIRTY;
	m->compref=0xFFFFFFFF;
	m->comref=0xFFFFFFFF;
	m->futref=0xFFFFFFFF;

	/* allocate new ones */
	if (m->flags2&MDB_USED)
	{
		m->compref=mdbGetNew();
		if (m->compref!=0xFFFFFFFF)
			memcpy(mdbData+m->compref, &m->flags2, sizeof(*mdbData));
	}
	if (m->flags3&MDB_USED)
	{
		m->comref=mdbGetNew();
		if (m->comref!=0xFFFFFFFF)
			memcpy(mdbData+m->comref, &m->flags3, sizeof(*mdbData));
	}
	if (m->flags4&MDB_USED)
	{
		m->futref=mdbGetNew();
		if (m->futref!=0xFFFFFFFF)
			memcpy(mdbData+m->futref, &m->flags4, sizeof(*mdbData));
	}

	memcpy(mdbData+mdb_ref, m, sizeof(*mdbData));
	mdbDirty=1;
	return 1;
}

void mdbScan(struct modlistentry *m)
{
	if (!(m->flags&MODLIST_FLAG_FILE))
		return;

	if (!mdbInfoRead(m->mdb_ref)) /* use mdbReadInfo again here ? */
	{
		struct moduleinfostruct mdbEditBuf;
		FILE *f;
		if (m->flags&MODLIST_FLAG_VIRTUAL) /* don't scan virtual files */
			return;
		if (!(f=m->ReadHandle(m)))
			return;
		mdbGetModuleInfo(&mdbEditBuf, m->mdb_ref);
		mdbReadInfo(&mdbEditBuf, f);
		fclose(f);
		mdbWriteModuleInfo(m->mdb_ref, &mdbEditBuf);
	}
}

void mdbRegisterReadDir(struct mdbreaddirregstruct *r)
{
	r->next=mdbReadDirs;
	mdbReadDirs=r;
}

void mdbUnregisterReadDir(struct mdbreaddirregstruct *r)
{
	struct mdbreaddirregstruct *root=mdbReadDirs;
	if (root==r)
	{
		mdbReadDirs=r->next;
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
		return 1;
	}
	close(f);

	for (i=0; i<mdbNum; i++)
	{
		if ((mdbData[i].flags&(MDB_BLOCKTYPE|MDB_USED))==(MDB_USED|MDB_GENERAL))
			mdbGenMax++;
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
		lseek(f, 64+i*sizeof(*mdbData), SEEK_SET);
		while (1)
		{
			ssize_t res;
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

uint32_t mdbGetModuleReference(const char *name, uint32_t size)
{
	uint32_t i;

	uint32_t *min=mdbReloc;
	uint32_t num=(unsigned short)mdbGenNum;
	uint32_t mn;
	struct modinfoentry *m;

	/* Iterate fast.. If size to to big, set current to be the minimum, and
	 * set the current to point in the new center, else half the current.
	 *
	 * That code is VERY clever. Took me some minutes to read it :-) nice
	 * work guys
	 *   - Stian
	 */
	while (num)
	{
		struct modinfoentry *m=&mdbData[min[num>>1]];
		int ret;
		if (size==m->gen.size)
			ret=memcmp(name, m->gen.name, 12);
		else
			if (size<m->gen.size)
				ret=-1;
			else
				ret=1;
		if (!ret)
			return min[num>>1];
		if (ret<0)
			num>>=1;
		else {
			min+=(num>>1)+1;
			num=(num-1)>>1;
		}
	}
	mn=min-mdbReloc;

	i=mdbGetNew();
	if (i==0xFFFFFFFF)
		return 0xFFFFFFFF;
	if (mdbGenNum==mdbGenMax)
	{
		void *n;
		mdbGenMax+=512;
		if (!(n=realloc(mdbReloc, sizeof (*mdbReloc)*mdbGenMax)))
			return 0xFFFFFFFF;
		mdbReloc=(uint32_t *)n;
	}

	memmovel(mdbReloc+mn+1, mdbReloc+mn, mdbGenNum-mn);
	mdbReloc[mn]=(uint32_t)i;
	mdbGenNum++;

	m=&mdbData[i];
	m->flags=MDB_DIRTY|MDB_USED|MDB_GENERAL;
	memcpy(m->gen.name, name, 12);
	m->gen.size=size;
	m->gen.modtype=0xFF;
	m->gen.comref=0xFFFFFFFF;
	m->gen.compref=0xFFFFFFFF;
	m->gen.futref=0xFFFFFFFF;
	memset(m->gen.modname, 0, 32);
	m->gen.date=0;
	m->gen.playtime=0;
	m->gen.channels=0;
	m->gen.moduleflags=0;
	mdbDirty=1;
	return (uint32_t)i;
}

int mdbGetModuleInfo(struct moduleinfostruct *m, uint32_t mdb_ref)
{
	memset(m, 0, sizeof(struct moduleinfostruct));
	if (mdb_ref>=mdbNum) /* needed, since we else might index mdbData wrong */
		goto invalid;
	if ((mdbData[mdb_ref].flags&(MDB_USED|MDB_BLOCKTYPE))!=(MDB_USED|MDB_GENERAL))
	{
invalid:
		m->modtype=0xFF;
		m->comref=0xFFFFFFFF;
		m->compref=0xFFFFFFFF;
		m->futref=0xFFFFFFFF;
		return 0;
	}
	memcpy(m, mdbData+mdb_ref, sizeof(*mdbData));
	if (m->compref!=0xFFFFFFFF)
	{
		if ((m->compref < mdbNum) && ((mdbData[m->compref].flags & MDB_BLOCKTYPE) == MDB_COMPOSER))
		{
			memcpy(&m->flags2, mdbData+m->compref, sizeof(*mdbData));
		} else {
			fprintf (stderr, "[mdb] warning - invalid compref\n");
			m->compref=0xFFFFFFFF;
		}
	}
	if (m->comref!=0xFFFFFFFF)
	{
		if ((m->comref < mdbNum) && ((mdbData[m->comref].flags & MDB_BLOCKTYPE) == MDB_COMMENT))
		{
			memcpy(&m->flags3, mdbData+m->comref, sizeof(*mdbData));
		} else {
			fprintf (stderr, "[mdb] warning - invalid comref\n");
			m->comref=0xFFFFFFFF;
		}
	}
	if (m->futref!=0xFFFFFFFF)
	{
		if ((m->futref < mdbNum) && ((mdbData[m->comref].flags & MDB_BLOCKTYPE) == MDB_FUTURE))
		{
			memcpy(&m->flags4, mdbData+m->futref, sizeof(*mdbData));
		} else {
			fprintf (stderr, "[mdb] warning - invalid futref\n");
			m->futref=0xFFFFFFFF;
		}
	}
	return 1;
}
