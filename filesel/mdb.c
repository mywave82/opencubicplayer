/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'24 Stian Skjelstad <stian.skjelstad@gmail.com>
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
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "types.h"
#include "boot/plinkman.h"
#include "boot/psetting.h"
#include "dirdb.h"
#include "filesystem.h"
#include "filesystem-ancient.h"
#include "mdb.h"
#include "pfilesel.h"
#include "stuff/cp437.h"
#include "stuff/compat.h"
#ifndef CFDATAHOMEDIR_OVERRIDE
#include "stuff/file.h"
#endif
#include "stuff/imsrtns.h"
#include "stuff/latin1.h"

#ifdef MDB_DEBUG
#define DEBUG_PRINT(...) do { fprintf(stderr, __VA_ARGS__); } while (0)
#else
#define DEBUG_PRINT(...) {}
#endif

#ifdef CFDATAHOMEDIR_OVERRIDE
# define CFDATAHOMEDIR CFDATAHOMEDIR_OVERRIDE
#else
# define CFDATAHOMEDIR configAPI->DataHomePath
#endif

#define MDB_USED 1
#define MDB_STRING_MORE 6        /* STRING, and more data follows in the next node */
#define MDB_STRING_TERMINATION 2 /* STRING, last (or only) node */
/* 64 and 128 is used by MDB_VIRTUAL and MDB_BIGMODULE */

struct modinfoentry
{
	union
	{
		struct  __attribute__((__aligned__(4)))
		        __attribute__((packed))
		{
			uint8_t record_flags;
			uint8_t filename_hash[7];  /*  1 */
			uint64_t size;             /*  8 */
			struct moduletype modtype; /* 16 */
			uint8_t module_flags;      /* 20 */
			uint8_t channels;          /* 21 */
			uint16_t playtime;         /* 22 */
			uint32_t date;             /* 24 */
			uint32_t title_ref;        /* 28 */
			uint32_t composer_ref;     /* 32 */
			uint32_t artist_ref;       /* 36 */
			uint32_t style_ref;        /* 40 */
			uint32_t comment_ref;      /* 44 */
			uint32_t album_ref;        /* 48 */
			uint8_t lastscanversion[3];/* 52-54*/
			uint8_t reserved[8];       /* 55-63*/
		} general;
		struct __attribute__((packed))
		{
			uint8_t flags;
			uint8_t data[63]; /* if not terminated, is uses the node following */
		} string;
	} mie;
};

struct __attribute__((packed)) mdbheader
{
	char sig[60];
	uint32_t entries;
};
const char mdbsigv1[60] = "Cubic Player Module Information Data Base\x1B\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";
#ifdef WORDS_BIGENDIAN
const char mdbsigv2[60] = "Cubic Player Module Information Data Base II\x1B\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";
#else
const char mdbsigv2[60] = "Cubic Player Module Information Data Base II\x1B\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01";
#endif
/* future mmap
static void  *mdbRawData;
static size_t mdbRawMapSize;
*/

static osfile              *mdbFile;

static struct modinfoentry *mdbData;
static uint32_t             mdbDataSize;
static uint32_t             mdbDataNextFree;

static uint8_t              mdbDirty;
static uint8_t             *mdbDirtyMap;
static unsigned int         mdbDirtyMapSize;  /* should be >= mdbDataSize */

       uint8_t              mdbCleanSlate; /* media-db needs to know that we used to be previous version database before we hashed filenames */

static uint32_t            *mdbSearchIndexData;  /* lookup sorted index */
static uint32_t             mdbSearchIndexCount; /* Number of entries in sorted index */
static uint32_t             mdbSearchIndexSize;  /* Allocated size in sorted index */

int mdbGetModuleType (uint32_t mdb_ref, struct moduletype *dst)
{
	if (mdb_ref>=mdbDataSize)
		return -1;
	if (mdbData[mdb_ref].mie.general.record_flags != MDB_USED)
		return -1;
	*dst = mdbData[mdb_ref].mie.general.modtype;
	return 0;
}

/* Is the mdb reference scanned yet? */
int mdbInfoIsAvailable (uint32_t mdb_ref)
{
	assert (mdb_ref < mdbDataSize);
	assert (mdbData[mdb_ref].mie.general.record_flags & MDB_USED);
	assert (!(mdbData[mdb_ref].mie.general.record_flags & MDB_STRING_MORE));

	if (mdbData[mdb_ref].mie.general.modtype.integer.i == 0)
	{
		DEBUG_PRINT ("mdbInfoIsAvailable(0x%08"PRIx32") => 0 due to mtUnRead\n", mdb_ref);
	} else {
		DEBUG_PRINT ("mdbInfoIsAvailable(0x%08"PRIx32") => 1 due to modtype != mtUnRead\n", mdb_ref);
	}

	if (mdbData[mdb_ref].mie.general.modtype.integer.i == mtUnknown)
	{ /* Avoid rescan, if file is already scanned by this version or newer */
		if (mdbData[mdb_ref].mie.general.lastscanversion[0] < OCP_MAJOR_VERSION) return 0;
		if (mdbData[mdb_ref].mie.general.lastscanversion[0] > OCP_MAJOR_VERSION) return 1;

		if (mdbData[mdb_ref].mie.general.lastscanversion[1] < OCP_MINOR_VERSION) return 0;
		if (mdbData[mdb_ref].mie.general.lastscanversion[1] > OCP_MINOR_VERSION) return 1;

		if (mdbData[mdb_ref].mie.general.lastscanversion[2] < OCP_PATCH_VERSION) return 0;
		return 1; /* >= is always true here */
	}

	return mdbData[mdb_ref].mie.general.modtype.integer.i != mtUnRead;
}

/* This thing will end up with a register of all valid pre-interprators for modules and friends
 */
static struct mdbreadinforegstruct *mdbReadInfos=NULL;
void mdbRegisterReadInfo (struct mdbreadinforegstruct *r)
{
	DEBUG_PRINT ("mdbRegisterReadInfo(%s)\n", r->name);

	r->next=mdbReadInfos;
	mdbReadInfos=r;
}

void mdbUnregisterReadInfo (struct mdbreadinforegstruct *r)
{
	struct mdbreadinforegstruct **prev = &mdbReadInfos, *iter = mdbReadInfos;

	while (iter)
	{
		if (iter == r)
		{
			DEBUG_PRINT ("mdbUnregisterReadInfo(%s)\n", r->name);

			*prev = iter->next;
			return;
		}
		prev = &iter->next;
		iter = iter->next;
	}

	DEBUG_PRINT ("mdbUnregisterReadInfo(%s) # WARNING, unable to find entry\n", r->name);
}

/* detect file infomation using 'plugins' */
static const struct mdbReadInfoAPI_t mdbReadInfoAPI =
{
	cp437_f_to_utf8_z,
	latin1_f_to_utf8_z,
	&dirdbAPI
};
int mdbReadInfo (struct moduleinfostruct *m, struct ocpfilehandle_t *f)
{
	char mdbScanBuf[4096];
	struct mdbreadinforegstruct *rinfos;
	int maxl;

	DEBUG_PRINT ("mdbReadInfo(f=%p)\n", f);

	if (f->seek_set (f, 0) < 0)
	{
		return 1;
	}
	memset (mdbScanBuf, 0, sizeof (mdbScanBuf));
	maxl = f->read (f, mdbScanBuf, sizeof (mdbScanBuf));
	f->seek_set (f, 0);

#ifdef MDB_DEBUG
	{
		char *fp = 0;
		dirdbGetFullname_malloc (f->dirdb_ref, &fp, DIRDB_FULLNAME_DRIVE);
		DEBUG_PRINT ("   mdbReadInfo(%s %p %d) # %s\n", fp ? fp : "", mdbScanBuf, maxl);
		free (fp);
	}
#endif

	m->modtype.integer.i = mtUnRead;

	/* slow version that also allows more I/O */
	for (rinfos=mdbReadInfos; rinfos; rinfos=rinfos->next)
	{
		if (rinfos->ReadInfo)
		{
			if (rinfos->ReadInfo(m, f, mdbScanBuf, maxl, &mdbReadInfoAPI))
			{
				return 1;
			}
		}
	}

	{
		struct ocpfilehandle_t *ancient;
		char compressionmethod[256];

		if ((ancient = ancient_filehandle (compressionmethod, sizeof (compressionmethod), f)))
		{
			snprintf (m->comment, sizeof (m->comment), "Compressed with: %.*s", (int)(sizeof (m->comment) - 17 - 1), compressionmethod);

			maxl = ancient->read (ancient, mdbScanBuf, sizeof (mdbScanBuf));
			ancient->seek_set (ancient, 0);

			for (rinfos=mdbReadInfos; rinfos; rinfos=rinfos->next)
			{
				if (rinfos->ReadInfo)
				{
					if (rinfos->ReadInfo(m, ancient, mdbScanBuf, maxl, &mdbReadInfoAPI))
					{
						ancient->unref (ancient);
						return 1;
					}
				}
			}

			ancient->unref (ancient);
		}
	}

	if (m->modtype.integer.i == mtUnRead)
	{ /* If no file-detection worked out, mark the file as unknown, the mdbWriteModuleInfo will tag the version to disk */
		m->modtype.integer.i = mtUnknown;
	}

	return m->modtype.integer.i != mtUnknown;
}

/* Unit test available */
static uint32_t mdbNew (int size)
{
	uint32_t i, j;

	for (i=mdbDataNextFree; i+size <= mdbDataSize; i++)
	{
		for (j=0; j < size; j++)
		{
			if (mdbData[i+j].mie.general.record_flags & MDB_USED)
			{
				break;
			}
		}
		if (j == size)
		{
			goto ready;
		}
	}

	{
		void *t;
		uint32_t N;
		uint32_t M;

#define GROW 64       /* must be same size or bigger than the biggest possible size */
#define GROWDIRTY 256 /* must be same as GROW or bigger, AND 8 or bigger */

		/* new target mdbDataSize */
		N = (mdbDataSize + GROW + GROW - 1) & ~(GROW-1);

		/* Grow DirtyMapSize if needed, in chunks of GROWDIRTY */
		if (mdbDirtyMapSize < N)
		{
			M = ((mdbDataSize + GROWDIRTY + GROWDIRTY - 1) & ~(GROWDIRTY - 1));
			t = realloc (mdbDirtyMap, M / 8);
			if (!t)
			{
				DEBUG_PRINT ("mdbNew() realloc(mdbDirtyMap) failed\n");
				return UINT32_MAX;
			}
			mdbDirtyMap = (uint8_t *)t;
			memset (mdbDirtyMap + mdbDirtyMapSize / 8, 0, (M - mdbDirtyMapSize) / 8);
			mdbDirtyMapSize = M;
		}

		/* grow mdbData, in GROW chunks */
		t=realloc(mdbData, N * sizeof(mdbData[0]));
		if (!t)
		{
			DEBUG_PRINT ("mdbNew() realloc(mdbData) failed\n");
			return UINT32_MAX;
		}
		mdbData=(struct modinfoentry *)t;
		memset (mdbData + mdbDataSize, 0, (N - mdbDataSize) * sizeof(mdbData[0]));
		mdbDataSize = N;
		for (j=i; j<mdbDataSize; j++) /* all appended entries are dirty */
		{
			mdbDirtyMap[j>>3] |= 1 << (j & 0x07);
		}
	}
ready:
	for (j = 0; j < size; j++)
	{
		mdbData[i+j].mie.general.record_flags = MDB_USED;
		mdbDirty=1;
		mdbDirtyMap[(i+j)>>3] |= 1 << ((i+j) & 0x07);
	}

	DEBUG_PRINT("mdbNew(size=%d) => 0x%08" PRIx32 "\n", size, i);

	if ((size == 1)||(mdbDataNextFree == i))
	{
		mdbDataNextFree = i + size; /* it might not be free, but it is atleast not before this */
	}

	return i;
}

/* Unit test available */
static void mdbFree (uint32_t ref, int size)
{
	int j;

	DEBUG_PRINT("  mdbFree(ref=0x%08" PRIx32 " size=%d)\n", ref, size);
	assert (ref > 0);
	assert (ref < mdbDataSize);

	for (j = 0; j < size; j++)
	{
		memset (mdbData + ref + j, 0, sizeof (mdbData[0]));
		mdbDirty=1;
		mdbDirtyMap[(ref + j)>>3] |= 1 << ((ref + j) & 0x07);
	}

	if (ref < mdbDataNextFree)
	{
		mdbDataNextFree = ref;
	}
}

/* Unit test available */
static int mdbWriteString (char *string, uint32_t *ref)
{
	int oldlen = 0;
	int newlen = (strlen (string) + 62) / 63; /* no need to zero-terminate if we end at a boundary */
	if (((*ref) < mdbDataSize) && ((*ref) != 0))
	{
		while (1)
		{
			if (((*ref) + oldlen) > mdbDataSize)
				goto b; /* this is an assertion */
			if (!(mdbData[(*ref) + oldlen].mie.general.record_flags & MDB_USED))
				goto b; /* this is an assertion */
			switch (mdbData[(*ref) + oldlen].mie.general.record_flags & MDB_STRING_MORE)
			{
				default:
					goto b; /* this is an assertion */
				case MDB_STRING_MORE:
					oldlen++;
					break;
				case MDB_STRING_TERMINATION:
					oldlen++;
					goto b;
			}
		}
	}

	DEBUG_PRINT("mdbWriteString(strlen=%d, old_ref=0x%08" PRIx32 ") oldlen=%d newlen=%d\n", (int)strlen (string), *ref, oldlen, newlen);
b:
	if (newlen == 0)
	{
		if (oldlen)
		{
			mdbFree (*ref, oldlen);
		}
		*ref = UINT32_MAX;
		return 0; /* no error */
	}
	if (oldlen != newlen)
	{
		if (oldlen)
		{
			mdbFree (*ref, oldlen);
		}
		*ref = mdbNew (newlen);
		if (*ref == UINT32_MAX)
		{
			return 1; /* error */
		}
	}

	{
		uint32_t iter = *ref;
		int len = strlen (string);
		char *s = string;
		while (len)
		{
			mdbData[iter].mie.string.flags |= len > 63 ? MDB_STRING_MORE : MDB_STRING_TERMINATION;
			memcpy (mdbData[iter].mie.string.data, s, (len >= 63) ? 63 : len + 1);
			s += 63;
			len -= (len >= 63) ? 63 : len;

			mdbDirty=1;
			mdbDirtyMap[iter>>3] |= 1 << (iter & 0x07);

			iter++;
		}
	}
	return 0;
}

int mdbWriteModuleInfo (uint32_t mdb_ref, struct moduleinfostruct *m)
{
	int retval = 0;
	uint32_t temp;

	DEBUG_PRINT("mdbWriteModuleInfo(0x%"PRIx32", %p)\n", mdb_ref, m);

	assert (mdb_ref > 0);
	assert (mdb_ref < mdbDataSize);
	assert (mdbData[mdb_ref].mie.general.record_flags == MDB_USED);

	/* ensure that there is only zeroes after a possible zero-termination */
	if (!m->modtype.string.c[0]) m->modtype.string.c[1] = 0;
	if (!m->modtype.string.c[1]) m->modtype.string.c[2] = 0;
	if (!m->modtype.string.c[2]) m->modtype.string.c[3] = 0;

	/* first transfer all the direct entries */
	mdbData[mdb_ref].mie.general.modtype = m->modtype;
	mdbData[mdb_ref].mie.general.module_flags = m->flags;
	mdbData[mdb_ref].mie.general.channels = m->channels;
	mdbData[mdb_ref].mie.general.playtime = m->playtime;
	mdbData[mdb_ref].mie.general.date = m->date;

	/* mdbData might move while inside mdbWriteString due to mdbNew() */
	temp = mdbData[mdb_ref].mie.general.title_ref;    retval |= mdbWriteString (m->title,    &temp); mdbData[mdb_ref].mie.general.title_ref    = temp;
	temp = mdbData[mdb_ref].mie.general.composer_ref; retval |= mdbWriteString (m->composer, &temp); mdbData[mdb_ref].mie.general.composer_ref = temp;
	temp = mdbData[mdb_ref].mie.general.artist_ref;   retval |= mdbWriteString (m->artist,   &temp); mdbData[mdb_ref].mie.general.artist_ref   = temp;
	temp = mdbData[mdb_ref].mie.general.style_ref;    retval |= mdbWriteString (m->style,    &temp); mdbData[mdb_ref].mie.general.style_ref    = temp;
	temp = mdbData[mdb_ref].mie.general.comment_ref;  retval |= mdbWriteString (m->comment,  &temp); mdbData[mdb_ref].mie.general.comment_ref  = temp;
	temp = mdbData[mdb_ref].mie.general.album_ref;    retval |= mdbWriteString (m->album,    &temp); mdbData[mdb_ref].mie.general.album_ref    = temp;

	if (m->modtype.integer.i == mtUnknown)
	{ /* tag the version number to disk */
		mdbData[mdb_ref].mie.general.lastscanversion[0] = OCP_MAJOR_VERSION;
		mdbData[mdb_ref].mie.general.lastscanversion[1] = OCP_MINOR_VERSION;
		mdbData[mdb_ref].mie.general.lastscanversion[2] = OCP_PATCH_VERSION;
	}

	mdbDirty=1;
	mdbDirtyMap[mdb_ref>>3] |= 1 << (mdb_ref & 0x07);

	return !retval;
}

/* if retain is non-zero, do not unref filehandle, but pass it to caller when done */
void mdbScan (struct ocpfile_t *file, uint32_t mdb_ref, struct ocpfilehandle_t **retain)
{
	DEBUG_PRINT ("mdbScan(file=%p, mdb_ref=0x%08"PRIx32")\n", file, mdb_ref);
	assert (mdb_ref > 0);
	assert (mdb_ref < mdbDataSize);
	assert (mdbData[mdb_ref].mie.general.record_flags == MDB_USED);
	if (!file)
	{
		return;
	}

	if (file->is_nodetect)
	{
		return;
	}

	if (!mdbInfoIsAvailable(mdb_ref))
	{
		struct moduleinfostruct mdbEditBuf;
		struct ocpfilehandle_t *f;
		if (!(f=file->open(file)))
		{
			return;
		}
		mdbGetModuleInfo(&mdbEditBuf, mdb_ref);
		mdbReadInfo(&mdbEditBuf, f);
		if (retain)
		{
			*retain = f;
		} else {
			f->unref (f);
		}
		mdbWriteModuleInfo(mdb_ref, &mdbEditBuf);
	}
}

static int miecmp (const void *a, const void *b)
{
	struct modinfoentry *c=&mdbData[*(uint32_t *)a];
	struct modinfoentry *d=&mdbData[*(uint32_t *)b];
	if (c->mie.general.size==d->mie.general.size)
	{
		return memcmp(c->mie.general.filename_hash, d->mie.general.filename_hash, sizeof (c->mie.general.filename_hash));
	}
	if (c->mie.general.size < d->mie.general.size)
	{
		return -1;
	} else {
		return 1;
	}
}

/* Unit test is available */
int mdbInit (const struct configAPI_t *configAPI)
{
	char *path;
	struct mdbheader header;
	uint32_t i;
	int retval = 1;

	mdbData = 0;
	mdbDataSize = 0;
	mdbDataNextFree = 0;

	mdbDirty = 0;
	mdbDirtyMap = 0;
	mdbDirtyMapSize = 0;

	mdbCleanSlate = 1;

	mdbSearchIndexData = 0;
	mdbSearchIndexCount = 0;
	mdbSearchIndexSize = 0;

	if (mdbFile)
	{
		fprintf (stderr, "mdbInit: Already loaded\n");
		return 1;
	}

	path = malloc (strlen (CFDATAHOMEDIR) + strlen ("CPMODNFO.DAT") + 1);
	if (!path)
	{
		fprintf (stderr, "mdbInit: malloc() failed\n");
		return 0;
	}
	sprintf (path, "%sCPMODNFO.DAT", CFDATAHOMEDIR);
	fprintf(stderr, "Loading %s .. ", path);

	mdbFile = osfile_open_readwrite (path, 1, 0);
	free (path);
	path = 0;
	if (!mdbFile)
	{
		retval = 0; /* fatal error */
		goto errorout;
	}

	if (osfile_read(mdbFile, &header, sizeof(header)) != sizeof(header))
	{
		fprintf(stderr, "No header\n");
		goto errorout;
	}

	if (!memcmp(header.sig, mdbsigv1, sizeof(mdbsigv1)))
	{
		fprintf(stderr, "Old header - discard data\n");
		goto errorout;
	}

	if (memcmp(header.sig, mdbsigv2, sizeof(mdbsigv2)))
	{
		fprintf(stderr, "Invalid header\n");
		goto errorout;
	}

	mdbDataSize = header.entries;
	if (!mdbDataSize)
	{
		fprintf(stderr, "No records\n");
		goto errorout;
	}

	mdbData = malloc(sizeof(struct modinfoentry) * mdbDataSize);
	if (!mdbData)
	{
		fprintf (stderr, "malloc() failed\n");
		goto errorout;
	}
	memcpy (mdbData, &header, 64);

	if (osfile_read(mdbFile, &mdbData[1], (mdbDataSize-1)*sizeof(*mdbData)) != (signed)((mdbDataSize-1)*sizeof(*mdbData)))
	{
		fprintf(stderr, "Failed to read records\n");
		goto errorout;
	}

	mdbDirtyMapSize = (mdbDataSize + 255) & ~255;
	mdbDirtyMap = calloc (mdbDirtyMapSize / 8, 1);
	if (!mdbDirtyMap)
	{
		fprintf (stderr, "Failed to allocated dirtyMap\n");
		goto errorout;
	}

	mdbDataNextFree = mdbDataSize;
	for (i=0; i<mdbDataSize; i++)
	{
		if (!mdbData[i].mie.general.record_flags)
		{
			mdbDataNextFree = i;
			break;
		}
	}

	for (i=0; i<mdbDataSize; i++)
	{
		if (mdbData[i].mie.general.record_flags==MDB_USED)
		{
			DEBUG_PRINT("0x%08"PRIx32" is USED GENERAL\n", i);
			mdbSearchIndexCount++;
		}
	}
	if (mdbSearchIndexCount)
	{
		mdbSearchIndexSize = (mdbSearchIndexCount + 31) & ~31;
		mdbSearchIndexCount = 0;
		mdbSearchIndexData = malloc(sizeof(uint32_t)*mdbSearchIndexSize);
		if (!mdbSearchIndexData)
		{
			fprintf (stderr, "Failed to allocated mdbSearchIndex\n");
			goto errorout;
		}
		for (i=0; i<mdbDataSize; i++)
		{
			if (mdbData[i].mie.general.record_flags==MDB_USED)
			{
				mdbSearchIndexData[mdbSearchIndexCount++] = i;
			}
		}

		qsort(mdbSearchIndexData, mdbSearchIndexCount, sizeof(*mdbSearchIndexData), miecmp);

		for (i=0; i<mdbSearchIndexCount; i++)
		{
			DEBUG_PRINT("%5d => 0x%08"PRIx32" %"PRIu64" 0x%02x%02x%02x%02x%02x%02x%02x\n",
				i, mdbSearchIndexData[i], mdbData[mdbSearchIndexData[i]].mie.general.size,
				mdbData[mdbSearchIndexData[i]].mie.general.filename_hash[0],
				mdbData[mdbSearchIndexData[i]].mie.general.filename_hash[1],
				mdbData[mdbSearchIndexData[i]].mie.general.filename_hash[2],
				mdbData[mdbSearchIndexData[i]].mie.general.filename_hash[3],
				mdbData[mdbSearchIndexData[i]].mie.general.filename_hash[4],
				mdbData[mdbSearchIndexData[i]].mie.general.filename_hash[5],
				mdbData[mdbSearchIndexData[i]].mie.general.filename_hash[6]);
		}
	}

	mdbCleanSlate = 0;

	osfile_purge_readahead_cache (mdbFile);

	fprintf(stderr, "Done\n");
	return 1;
errorout:
	if (!retval)
	{
		if (mdbFile)
		{
			osfile_close (mdbFile);
			mdbFile = 0;
		}
	}

	free (mdbData);
	free (mdbDirtyMap);
	free (mdbSearchIndexData);
	mdbData = 0;
	mdbDataSize = 0;
	mdbDataNextFree = 1; /* hack to ignore entry #0, which is header */
	mdbDirtyMap = 0;
	mdbDirtyMapSize = 0;
	mdbSearchIndexData = 0;
	mdbSearchIndexCount = 0;
	mdbSearchIndexSize = 0;
	return retval;
}

/* Unit test available */
void mdbUpdate (void)
{
	uint32_t i;
	struct mdbheader *header = (struct mdbheader *)mdbData;

	DEBUG_PRINT("mdbUpdate: mdbDirty=%d fsWriteModInfo=%d\n", mdbDirty, fsWriteModInfo);

	if ((!mdbDirty)||(!fsWriteModInfo)||(!mdbFile))
	{
		return;
	}
	mdbDirty=0;

	if (!mdbDataSize)
	{ /* should not happen */
		return;
	}

	osfile_setpos (mdbFile, 0);
	memcpy(header->sig, mdbsigv2, sizeof(mdbsigv2));
	header->entries = mdbDataSize;
	mdbDirtyMap[0] |= 1;

	for (i = 0; i < mdbDataSize; i += 8)
	{
		int64_t res;
		/* we write in 512 byte chunks - fits old hard-drives sector size */
		if (!mdbDirtyMap[i / 8])
		{
			continue;
		}

		osfile_setpos(mdbFile, (uint64_t)i*sizeof(*mdbData));
		DEBUG_PRINT("  [0x%08"PRIx32" -> 0x%08"PRIx32"] DIRTY\n", i, i+7);
		res = osfile_write(mdbFile, mdbData + i, 8 * sizeof(*mdbData));
		if (res < 0)
		{
			fprintf(stderr, __FILE__ " write() to \"CPMODNFO.DAT\" failed\n");
			exit(1);
		}

		mdbDirtyMap[i / 8] = 0;
	}
}

void mdbClose (void)
{
	mdbUpdate();
	if (mdbFile)
	{
		osfile_close (mdbFile);
		mdbFile = 0;
	}
	free(mdbData);
	free(mdbDirtyMap);
	free(mdbSearchIndexData);

	mdbData = 0;
	mdbDataSize = 0;
	mdbDataNextFree = 1;
	mdbDirty = 0;
	mdbDirtyMap = 0;
	mdbDirtyMapSize = 0;
	mdbSearchIndexData = 0;
	mdbSearchIndexCount = 0;
	mdbSearchIndexSize = 0;
}

/* Unit test available */
static uint32_t mdbGetModuleReference (const char *name, uint64_t size)
{
	uint32_t i;

	uint32_t *min=mdbSearchIndexData;
	uint32_t num=mdbSearchIndexCount;
	uint32_t mn;
	struct modinfoentry *m;
	uint8_t hash[8]; /* to byte align with header, byte 0 is ignored */

	for (i=0; i < 8; i++)
	{
		hash[i] = 0;
	}
	for (i=0; name[i]; i++)
	{
		hash[1+i%7]       += name[i];
		hash[1+((i+1)%7)] ^= name[i];
	}

	/* Iterate fast.. If size to to big, set current to be the minimum, and
	 * set the current to point in the new center, else half the current.
	 *
	 * That code is VERY clever. Took me some minutes to read it :-) nice
	 * work guys
	 *   - Stian
	 */

	DEBUG_PRINT("mdbGetModuleReference(%s=>0x%02x%02x%0x%02x%02x%02x%02x %"PRIu64")\n", name, hash[1], hash[2], hash[3], hash[4], hash[5], hash[6], hash[7], size);
	while (num)
	{
		struct modinfoentry *m=&mdbData[min[num>>1]];
		int ret;
#ifdef MDB_DEBUG
#if MDB_DEBUG > 1
		{
			uint32_t x;
			for (x = 0; x < num; x++)
			{
				DEBUG_PRINT("  %08x %"PRIu64" 0x%02x%02x%02x%02x%02x%02x%02x\n",
					min[x],
					mdbData[min[x]].mie.general.size,
					mdbData[min[x]].mie.general.filename_hash[0],
					mdbData[min[x]].mie.general.filename_hash[1],
					mdbData[min[x]].mie.general.filename_hash[2],
					mdbData[min[x]].mie.general.filename_hash[3],
					mdbData[min[x]].mie.general.filename_hash[4],
					mdbData[min[x]].mie.general.filename_hash[5],
					mdbData[min[x]].mie.general.filename_hash[6]);
			}
			DEBUG_PRINT("  ----------\n");
		}
#endif
#endif
		if (size==m->mie.general.size)
		{
			ret = memcmp(hash+1, m->mie.general.filename_hash, 7);
		} else {
			if (size < m->mie.general.size)
			{
				ret = -1;
			} else {
				ret = 1;
			}
		}
		if (!ret)
		{
			DEBUG_PRINT("mdbGetModuleReference(\"%s\" %"PRIu64") => mdbSearchIndexData => 0x%08"PRIx32"\n", name, size, min[num>>1]);
			return min[num>>1];
		}
		if (ret<0)
		{
			num >>= 1;
		} else {
			min += (num >> 1)  + 1;
			num  = (num  - 1) >> 1;
		}
	}
	mn = min - mdbSearchIndexData;

	i=mdbNew(1);
	if (i==UINT32_MAX)
	{
		return UINT32_MAX;
	}
	if (mdbSearchIndexCount == mdbSearchIndexSize)
	{
		void *n;
		mdbSearchIndexSize += 512;
		if (!(n = realloc(mdbSearchIndexData, sizeof (*mdbSearchIndexData) * mdbSearchIndexSize)))
		{
			mdbFree (i, 1);
			return UINT32_MAX;
		}
		mdbSearchIndexData = (uint32_t *)n;
	}

	memmove( mdbSearchIndexData + mn + 1,
	         mdbSearchIndexData + mn,
		(mdbSearchIndexCount - mn ) * sizeof (*mdbSearchIndexData));
	mdbSearchIndexData[mn]=i;
	mdbSearchIndexCount++;

	m = &mdbData[i];
	memcpy (m->mie.general.filename_hash, hash + 1, 7);
	m->mie.general.size = size;
	m->mie.general.modtype.integer.i = mtUnRead;
	m->mie.general.module_flags = 0;
	m->mie.general.channels = 0;
	m->mie.general.playtime = 0;
	m->mie.general.date = 0;
	m->mie.general.title_ref = UINT32_MAX;
	m->mie.general.composer_ref = UINT32_MAX;
	m->mie.general.artist_ref = UINT32_MAX;
	m->mie.general.style_ref = UINT32_MAX;
	m->mie.general.comment_ref = UINT32_MAX;
	m->mie.general.album_ref = UINT32_MAX;
	memset (m->mie.general.reserved, 0, sizeof (m->mie.general.reserved));
	DEBUG_PRINT("mdbGetModuleReference(\"%s\" %"PRIu64") => new => 0x%08"PRIx32"\n", name, size, i);
	return i;
}

uint32_t mdbGetModuleReference2 (uint32_t dirdb_ref, uint64_t size)
{
	const char *temppath;

	dirdbGetName_internalstr (dirdb_ref, &temppath);
	if (!temppath)
	{
		return UINT32_MAX;
	}

	return mdbGetModuleReference (temppath, size);
}

/* Unit test available */
static void mdbGetString (char *dst, int dstlen, uint32_t mdb_ref)
{
	dstlen--; /* reserve last byte for zero-termination */
	while (1)
	{
		int l;
		*dst = 0; /* parent is memset() already, but does not hurt to ensure */
		if ((mdb_ref == 0) || (mdb_ref >= mdbDataSize) || (!dstlen))
		{
			return;
		}
		switch (mdbData[mdb_ref].mie.general.record_flags & MDB_STRING_MORE)
		{
			default:
				return;
			case MDB_STRING_TERMINATION:
			case MDB_STRING_MORE:
				l = (dstlen > 63) ? 63 : dstlen;
				memcpy (dst, mdbData[mdb_ref].mie.string.data, l);
				dstlen -= l;
				dst += l;
				break;
		}
		if ((mdbData[mdb_ref].mie.general.record_flags & MDB_STRING_MORE) == MDB_STRING_TERMINATION)
		{
			mdb_ref = 0;
		} else {
			mdb_ref++;
		}
	}
}

/* Partially unit test in mdbInit */
int mdbGetModuleInfo (struct moduleinfostruct *m, uint32_t mdb_ref)
{
	memset (m, 0, sizeof(*m));
	assert (mdb_ref > 0);
	assert (mdb_ref < mdbDataSize);
	assert (mdbData[mdb_ref].mie.general.record_flags == MDB_USED);

	if ((mdb_ref >= mdbDataSize) || (mdb_ref == 0) || (mdbData[mdb_ref].mie.general.record_flags != MDB_USED))
	{ /* invalid reference */
		return 0;
	}
	m->size = mdbData[mdb_ref].mie.general.size;
	m->modtype = mdbData[mdb_ref].mie.general.modtype;
	m->flags = mdbData[mdb_ref].mie.general.module_flags;
	m->channels = mdbData[mdb_ref].mie.general.channels;
	m->playtime = mdbData[mdb_ref].mie.general.playtime;
	m->date = mdbData[mdb_ref].mie.general.date;
	mdbGetString (m->title,    sizeof (m->title),    mdbData[mdb_ref].mie.general.title_ref);
	mdbGetString (m->composer, sizeof (m->composer), mdbData[mdb_ref].mie.general.composer_ref);
	mdbGetString (m->artist,   sizeof (m->artist),   mdbData[mdb_ref].mie.general.artist_ref);
	mdbGetString (m->style,    sizeof (m->style),    mdbData[mdb_ref].mie.general.style_ref);
	mdbGetString (m->comment,  sizeof (m->comment),  mdbData[mdb_ref].mie.general.comment_ref);
	mdbGetString (m->album,    sizeof (m->album),    mdbData[mdb_ref].mie.general.album_ref);
	return 1;
}
