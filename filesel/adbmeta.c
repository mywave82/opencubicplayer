/* OpenCP Module Player
 * copyright (c) 2020-'22 Stian Sebastian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Archive Meta DataBase (replaces previous adb.c Archive DataBase)
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
 */

#include "config.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "types.h"
#include "adbmeta.h"
#include "boot/psetting.h"

#ifndef ADBMETA_SILENCE_OPEN_ERRORS
 #define ADBMETA_SILENCE_OPEN_ERRORS 0
#endif

const char adbMetaTag[16] = "OCPArchiveMeta\x1b\x00";
/*
 16 bytes header
  4 bytes entries#N

  X bytes FILENAME\0
  X bytes SIG\0
  8 bytes filesize
  4 bytes datasize
  X bytes data

 */

struct adbMetaHeader
{
	char Signature[16];
	uint32_t entries;
};

struct adbMetaEntry_t
{
	char          *filename;
	uint64_t       filesize;
	char          *SIG;
	uint32_t       datasize;
	unsigned char *data;
};

static struct adbMetaEntry_t **adbMetaEntries;
static uint_fast32_t           adbMetaCount;
static uint_fast32_t           adbMetaSize; /* slots allocated */
static char                   *adbMetaPath;
static uint8_t                 adbMetaDirty;

static struct adbMetaEntry_t *adbMetaInit_CreateBlob (const char          *filename,
                                                      uint64_t             filesize,
                                                      const char          *signature,
                                                      const unsigned char *data,
                                                      uint32_t             datasize)
{
	struct adbMetaEntry_t *retval;

	long filename_length_sizeof = strlen (filename) + 1;
	long signature_length_sizeof = strlen (signature) + 1;
#if 0
/* the famous standard power-of-two rounding up */
#define _LONGSIZE(n) ((n + sizeof(long) - 1) & ~(sizeof(long) - 1))
	filename_length_sizeof = _LONGSIZE (filename_length_sizeof + 1);
	signature_length_sizeof = _LONGSIZE (signature_length_sizeof + 1);
#endif
	retval = calloc (sizeof (*retval) +
                         filename_length_sizeof +
                         signature_length_sizeof +
                         datasize, 1); /* last one is rounded up up malloc() if needed */

	if (!retval)
	{
		return 0;
	}

#ifdef ADBMETA_DEBUG
	fprintf (stderr, "adbMetaInit_CreateBlob (\"%s\", %"PRId64", \"%s\", %p, %"PRId32")\n", filename, filesize, signature, data, datasize);
#endif

	retval->filename = (char *)&(retval[1]);
	retval->filesize = filesize;
	retval->SIG      = retval->filename + filename_length_sizeof;
	retval->data     = (unsigned char *)(retval->SIG) + signature_length_sizeof;
	retval->datasize = datasize;

	strcpy (retval->filename, filename);
	strcpy (retval->SIG, signature);
	memcpy (retval->data, data, datasize);

	return retval;
}

static int adbMetaInit_ParseFd (const int f)
{
	/* record counter */
	uint_fast32_t counter;
	/* read buffer */
	uint_fast32_t fill;
	uint_fast32_t size;
	uint8_t *data;

	fill = 0;
	size = 65536;
	data = malloc (size);
	if (!data)
	{
		fprintf (stderr, "adbMetaInit: malloc() readbuffer failed\n");
		return -1;
	}

	for (counter = 0; counter < adbMetaSize; counter++)
	{
		uint_fast32_t offset;
		uint_fast64_t filesize;
		uint_fast32_t datasize;

		/* char *filename == data */
		char *signature;

#ifdef ADBMETA_DEBUG
		fprintf (stderr, "adbMeta: attempt to parse entry %d/%d\n", (int)counter + 1, (int)adbMetaSize);
#endif
		if (fill < 16)
		{
			int result;
fillmore:
			if (fill == size)
			{
				uint8_t *temp;
				size += 65536;
				temp = realloc (data, size);
				if (!temp)
				{
					fprintf (stderr, "realloc() readbuffer failed\n");
					adbMetaCount = counter;
					free (data);
					return 1;
				}
#ifdef ADBMETA_DEBUG
				fprintf (stderr, "adbMeta: increased buffer size up to %d bytes\n", (int)size);
#endif
				data = temp;
			}
			result = size - fill;
			if (result > 65536) result = 65536;
			result = read (f, data + fill, result);
			if (result < 0)
			{
				perror ("adbMetaInit: read");
				adbMetaCount = counter;
				free (data);
				return 1;
			}
			if (result == 0)
			{
				fprintf (stderr, "ran out of data\n");
				adbMetaCount = counter;
				free (data);
				return 1;
			}
			fill += result;
#ifdef ADBMETA_DEBUG
			fprintf (stderr, "adbMeta: input buffer is filled with %d (of %d possible)\n", (int)fill, (int)size);
#endif
		}

/* filename always start at data + 0 */
		for (offset = 0; ; offset++)
		{
			if (offset >= fill)
			{
				goto fillmore;
			}
			if (data[offset])
			{
				continue;
			}
			//filename_length = offset;
			offset++;
			break;
		}
		signature = (char *)data + offset;

		for (; ; offset++)
		{
			if (offset >= fill)
			{
				goto fillmore;
			}
			if (data[offset])
			{
				continue;
			}
			//signature_length = (data + offset) - signature;
			offset++;
			break;
		}

		if (offset + 12 > fill)
		{
			goto fillmore;
		}
		filesize = ((uint_fast64_t)data[offset+0] << 56) |
		           ((uint_fast64_t)data[offset+1] << 48) |
		           ((uint_fast64_t)data[offset+2] << 40) |
		           ((uint_fast64_t)data[offset+3] << 32) |
		           ((uint_fast64_t)data[offset+4] << 24) |
		           ((uint_fast64_t)data[offset+5] << 16) |
		           ((uint_fast64_t)data[offset+6] << 8) |
		           ((uint_fast64_t)data[offset+7]);
		offset += 8;
		datasize = ((uint_fast64_t)data[offset+0] << 24) |
		           ((uint_fast64_t)data[offset+1] << 16) |
		           ((uint_fast64_t)data[offset+2] << 8) |
		           ((uint_fast64_t)data[offset+3]);
		offset += 4;

		if (offset + datasize > fill)
		{
			goto fillmore;
		}

		adbMetaEntries[counter] = adbMetaInit_CreateBlob ((char *)data,
		                                                  filesize,
		                                                  signature,
		                                                  data + offset,
		                                                  datasize);
		if (!adbMetaEntries[counter])
		{
			fprintf (stderr, "adbMetaInit: failed to allocate memory for entry #%ld\n", (long)counter);
			adbMetaCount = counter;
			free (data);
			return -1;
		}

		offset += datasize;

		memmove (data, data + offset, fill - offset);
		fill -= offset;
	}
	adbMetaCount = counter;
	free (data);

	return 0;
}

int adbMetaInit (void)
{
	int f, retval;
	struct adbMetaHeader header;

	adbMetaPath = malloc(strlen(cfDataHomeDir)+13+1);
	if (!adbMetaPath)
	{
		fprintf (stderr, "adbMetaInit: malloc() failed\n");
		return 1;
	}
	strcpy(adbMetaPath, cfDataHomeDir);
	strcat(adbMetaPath, "CPARCMETA.DAT");

	if ((f=open(adbMetaPath, O_RDONLY))<0)
	{
		if (!ADBMETA_SILENCE_OPEN_ERRORS)
		{
			perror ("adbMetaInit: open(cfDataHomeDir/CPARCMETA.DAT)");
		}
		return 1;
	}

	fprintf(stderr, "Loading %s ..\n", adbMetaPath);

	if (read(f, &header, sizeof (header)) != sizeof (header))
	{
		fprintf (stderr, "No header\n");
		close (f);
		return 1;
	}

	if (memcmp (header.Signature, adbMetaTag, 16))
	{
		fprintf (stderr, "Invalid header\n");
		close (f);
		return 1;
	}

	adbMetaSize = uint32_big (header.entries);
	if (!adbMetaSize)
	{
		close (f);
		return 0;
	}
	adbMetaEntries = malloc (sizeof (adbMetaEntries[0]) * adbMetaSize);
	if (!adbMetaEntries)
	{
		fprintf (stderr, "malloc() failed\n");
		close (f);
		return 1;
	}

	retval = adbMetaInit_ParseFd (f);

	close (f);

	return retval;
}

void adbMetaCommit (void)
{
	int f;
	struct adbMetaHeader header;
	/* record counter */
	uint_fast32_t counter;

	memcpy (header.Signature, adbMetaTag, sizeof (adbMetaTag));
	header.entries = uint32_big (adbMetaCount);

	if ((!adbMetaPath) || (!adbMetaDirty))
	{
		return;
	}

	f = open(adbMetaPath, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	if (!f)
	{
		perror ("adbMetaCommit: open(cfDataHomeDir/CPARCMETA.DAT)");
		return;
	}

	if (write (f, &header, sizeof (header)) != sizeof (header)) { perror ("adbMetaCommit write #1"); };

	for (counter = 0; counter < adbMetaCount; counter++)
	{
		uint8_t buffer[12];
		if (write (f, adbMetaEntries[counter]->filename, strlen (adbMetaEntries[counter]->filename) + 1) < 0) { perror ("adbMetaCommit write #2"); };
		if (write (f, adbMetaEntries[counter]->SIG, strlen (adbMetaEntries[counter]->SIG) + 1) < 0) { perror ("adbMetaCommit write #3"); };
		buffer[ 0] = adbMetaEntries[counter]->filesize >> 56;
		buffer[ 1] = adbMetaEntries[counter]->filesize >> 48;
		buffer[ 2] = adbMetaEntries[counter]->filesize >> 40;
		buffer[ 3] = adbMetaEntries[counter]->filesize >> 32;
		buffer[ 4] = adbMetaEntries[counter]->filesize >> 24;
		buffer[ 5] = adbMetaEntries[counter]->filesize >> 16;
		buffer[ 6] = adbMetaEntries[counter]->filesize >> 8;
		buffer[ 7] = adbMetaEntries[counter]->filesize;
		buffer[ 8] = adbMetaEntries[counter]->datasize >> 24;
		buffer[ 9] = adbMetaEntries[counter]->datasize >> 16;
		buffer[10] = adbMetaEntries[counter]->datasize >> 8;
		buffer[11] = adbMetaEntries[counter]->datasize;
		if (write (f, buffer, 12) != 12) { perror ("adbMetaCommit write #4"); };
		if (write (f, adbMetaEntries[counter]->data, adbMetaEntries[counter]->datasize) != adbMetaEntries[counter]->datasize) { perror ("adbMetaCommit write #5"); };
	}

	close (f);
	adbMetaDirty = 0;
}

void adbMetaClose (void)
{
	int i;
	adbMetaCommit();
	for (i=0; i < adbMetaCount; i++)
	{
		free (adbMetaEntries[i]);
		adbMetaEntries[i]=0;
	}
	free (adbMetaEntries);
	adbMetaEntries = 0;
	adbMetaCount = adbMetaSize = 0;
	free (adbMetaPath);
	adbMetaPath = 0;
	adbMetaDirty = 0;
}

static uint32_t adbMetaBinarySearchFilesize (const size_t filesize)
{
	uint_fast32_t searchbase = 0, searchlen = adbMetaCount;

//	fprintf (stderr, "\n SEARCH %d..\n", (int)filesize);

	if (adbMetaCount == 0)
	{
		return 0;
	}

	while (searchlen > 1)
	{
		uint_fast32_t halfmark;

//		int i;

		halfmark = searchlen >> 1;

//		for (i=0; i < searchlen; i++)
//		{
//			fprintf (stderr, " %d(%d)%s ", (int)(adbMetaEntries[searchbase + i]->filesize), (int)(searchbase + i), (i == halfmark)?"*":"");
//		}
//		fprintf (stderr, "\n");

		if (filesize > adbMetaEntries[searchbase + halfmark]->filesize)
		{
			searchbase += halfmark;
			searchlen -= halfmark;
		} else {
			searchlen = halfmark;
		}

//		for (i=0; i < searchlen; i++)
//		{
//			fprintf (stderr, " %d(%d) ", (int)(adbMetaEntries[searchbase + i]->filesize), (int)(searchbase + i));
//		}
//		fprintf (stderr, "\n\n");

	}

	/* fine-tune the position */
	if (searchbase < adbMetaCount)
	{
/*
		while (searchbase && (adbMetaEntries[searchbase]->filesize == filesize))
		{
			searchbase--;
		}
*/
		if (adbMetaEntries[searchbase]->filesize < filesize)
		{
			searchbase++;
		}
	}

	return searchbase;
}

int adbMetaAdd (const char *filename, const size_t filesize, const char *SIG, const unsigned char *data, const size_t datasize)
{
	uint_fast32_t searchindex = adbMetaBinarySearchFilesize (filesize);
	int_fast32_t search;
	struct adbMetaEntry_t *temp;

#ifdef ADBMETA_DEBUG
	fprintf (stderr, "adbMetaAdd (\"%s\", %"PRId64", \"%s\", %p, %ld)\n", filename, filesize, SIG, data, datasize);
#endif

	if (searchindex == adbMetaCount)
	{
		goto DoInsert;
	}

	assert (adbMetaEntries[searchindex]->filesize >= filesize);

	assert (datasize);

	if (adbMetaEntries[searchindex]->filesize > filesize)
	{
		goto DoInsert;
	}

	for (search = searchindex; search < adbMetaCount; search++)
	{
		if (adbMetaEntries[search]->filesize != filesize)
		{
			break;
		}
		if (strcmp (adbMetaEntries[search]->filename, filename))
		{
			continue;
		}
		if (strcmp (adbMetaEntries[search]->SIG, SIG))
		{
			continue;
		}
		if ((adbMetaEntries[search]->datasize == datasize) && (!memcmp (adbMetaEntries[search]->data, data, datasize)))
		{
			return 0;
		}

		/* replace the entry */
		temp = adbMetaInit_CreateBlob (filename, filesize, SIG, data, datasize);
		if (!temp)
		{
			fprintf (stderr, "adbMetaAdd: error allocating memory for an entry\n");
			return -1;
		}
		free (adbMetaEntries[search]);
		adbMetaEntries[search] = temp;

		adbMetaDirty = 1;
		return 0;
	}

DoInsert:
	if (adbMetaCount >= adbMetaSize)
	{
		struct adbMetaEntry_t **r;
		r = realloc (adbMetaEntries, (adbMetaSize + 8) * sizeof (adbMetaEntries[0]));
		if (!r)
		{
			fprintf (stderr, "adbMetaAdd: error allocating memory for index\n");
			return -1;
		}
		adbMetaEntries = r;
		adbMetaSize += 8;
	}

	temp = adbMetaInit_CreateBlob (filename, filesize, SIG, data, datasize);
	if (!temp)
	{
		fprintf (stderr, "adbMetaAdd: error allocating memory for an entry\n");
		return -1;
	}
	memmove (adbMetaEntries + searchindex + 1, adbMetaEntries + searchindex, (adbMetaCount - searchindex) * sizeof (adbMetaEntries[0]));
	adbMetaEntries[searchindex] = temp;
	adbMetaCount++;

	adbMetaDirty = 1;

	return 0;
}

int adbMetaRemove (const char *filename, const size_t filesize, const char *SIG)
{
	uint_fast32_t searchindex = adbMetaBinarySearchFilesize (filesize);
	int_fast32_t search;

#ifdef ADBMETA_DEBUG
	fprintf (stderr, "adbMetaRemove (\"%s\", %ld, \"%s\")\n", filename, filesize, SIG);
#endif

	if (searchindex == adbMetaCount)
	{
		return 1; /* not found */
	}

	assert (adbMetaEntries[searchindex]->filesize >= filesize);

	if (adbMetaEntries[searchindex]->filesize > filesize)
	{
		return 1; /* not found */
	}

	for (search = searchindex; search < adbMetaCount; search++)
	{
		if (adbMetaEntries[search]->filesize != filesize)
		{
			return 1; /* not found */
		}
		if (strcmp (adbMetaEntries[search]->filename, filename))
		{
			continue;
		}
		if (strcmp (adbMetaEntries[search]->SIG, SIG))
		{
			continue;
		}

		free (adbMetaEntries[search]);
		memmove (adbMetaEntries + search, adbMetaEntries + search + 1, (adbMetaCount - search - 1) * sizeof (adbMetaEntries[0]));
		adbMetaCount--;
		adbMetaDirty = 1;
		return 0;
	}

	return 1; /* not found */
}

int adbMetaGet (const char *filename, const size_t filesize, const char *SIG, unsigned char **data, size_t *datasize)
{
	uint_fast32_t searchindex = adbMetaBinarySearchFilesize (filesize);
	int_fast32_t search;

#ifdef ADBMETA_DEBUG
	fprintf (stderr, "adbMetaGet (\"%s\", %"PRId64", \"%s\") ", filename, filesize, SIG);
#endif

	*data = 0;
	*datasize = 0;

	if (searchindex == adbMetaCount)
	{
#ifdef ADBMETA_DEBUG
		fprintf (stderr, " => NULL #1\n");
#endif
		return 1; /* not found */
	}

	assert (adbMetaEntries[searchindex]->filesize >= filesize);

	if (adbMetaEntries[searchindex]->filesize > filesize)
	{
#ifdef ADBMETA_DEBUG
		fprintf (stderr, " => NULL #2\n");
#endif
		return 1; /* not found */
	}

	for (search = searchindex; search < adbMetaCount; search++)
	{
		if (adbMetaEntries[search]->filesize != filesize)
		{
#ifdef ADBMETA_DEBUG
		fprintf (stderr, " => NULL #3\n");
#endif
			return 1; /* not found */
		}
		if (strcmp (adbMetaEntries[search]->filename, filename))
		{
			continue;
		}
		if (strcmp (adbMetaEntries[search]->SIG, SIG))
		{
			continue;
		}

		*data = malloc (adbMetaEntries[search]->datasize);
		if (!*data)
		{
			fprintf (stderr, "adbMetaGet: failed to allocate memory for BLOB\n");
			return -1;
		}
		memcpy (*data, adbMetaEntries[search]->data, adbMetaEntries[search]->datasize);
		*datasize = adbMetaEntries[search]->datasize;

#ifdef ADBMETA_DEBUG
		fprintf (stderr, " => %p %ld\n", *data, *datasize);
#endif
		return 0;
	}


#ifdef ADBMETA_DEBUG
	fprintf (stderr, " => NULL #4\n");
#endif
	return 1; /* not found */
}
