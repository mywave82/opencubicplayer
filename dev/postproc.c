/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'24 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Wavetable postproc plugins system
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "types.h"
#include "dev/postproc.h"
#include "stuff/err.h"

static const struct PostProcFPRegStruct      **mcpPostProcFPList;
static int                                     mcpPostProcFPListEntries;
static const struct PostProcIntegerRegStruct **mcpPostProcIntegerList;
static int                                     mcpPostProcIntegerListEntries;

int mcpRegisterPostProcFP (const struct PostProcFPRegStruct *plugin)
{
	const struct PostProcFPRegStruct **temp;
	int i;

	for (i=0; i < mcpPostProcFPListEntries; i++)
	{
		if (!strcmp (mcpPostProcFPList[i]->name, plugin->name))
		{
			return errOk;
		}
	}

	temp = realloc (mcpPostProcFPList, sizeof (mcpPostProcFPList[0]) * (mcpPostProcFPListEntries + 1));
	if (!temp)
	{
		fprintf (stderr, "mcpRegisterPostProcFP: realloc() failed\n");
		return errAllocMem;
	}
	mcpPostProcFPList = temp;
	mcpPostProcFPList[mcpPostProcFPListEntries] = plugin;
	mcpPostProcFPListEntries++;
	return errOk;
}

void mcpUnregisterPostProcFP (const struct PostProcFPRegStruct *plugin)
{
	int i;

	for (i=0; i < mcpPostProcFPListEntries; i++)
	{
		if (!strcmp (mcpPostProcFPList[i]->name, plugin->name))
		{
			memmove (mcpPostProcFPList + i, mcpPostProcFPList + i + 1,  sizeof (mcpPostProcFPList[0]) * (mcpPostProcFPListEntries - i - 1));
			mcpPostProcFPListEntries--;
			if (!mcpPostProcFPListEntries)
			{
				free (mcpPostProcFPList);
				mcpPostProcFPList = 0;
			}
			return;
		}
	}
	return;
}

const struct PostProcFPRegStruct *mcpFindPostProcFP (const char *name)
{
	int i;

	for (i=0; i < mcpPostProcFPListEntries; i++)
	{
		if (!strcmp (mcpPostProcFPList[i]->name, name))
		{
			return mcpPostProcFPList[i];
		}
	}
	return 0;
}

void mcpListAllPostProcFP (const struct PostProcFPRegStruct ***postproc, int *count)
{
	*postproc = mcpPostProcFPList;
	*count = mcpPostProcFPListEntries;
}

int mcpRegisterPostProcInteger (const struct PostProcIntegerRegStruct *plugin)
{
	const struct PostProcIntegerRegStruct **temp;
	int i;

	for (i=0; i < mcpPostProcIntegerListEntries; i++)
	{
		if (!strcmp (mcpPostProcIntegerList[i]->name, plugin->name))
		{
			return errOk;
		}
	}

	temp = realloc (mcpPostProcIntegerList, sizeof (mcpPostProcIntegerList[0]) * (mcpPostProcIntegerListEntries + 1));
	if (!temp)
	{
		fprintf (stderr, "mcpRegisterPostProcInteger: realloc() failed\n");
		return errAllocMem;
	}
	mcpPostProcIntegerList = temp;
	mcpPostProcIntegerList[mcpPostProcIntegerListEntries] = plugin;
	mcpPostProcIntegerListEntries++;
	return errOk;
}

void mcpUnregisterPostProcInteger (const struct PostProcIntegerRegStruct *plugin)
{
	int i;

	for (i=0; i < mcpPostProcIntegerListEntries; i++)
	{
		if (!strcmp (mcpPostProcIntegerList[i]->name, plugin->name))
		{
			memmove (mcpPostProcIntegerList + i, mcpPostProcIntegerList + i + 1,  sizeof (mcpPostProcIntegerList[0]) * (mcpPostProcIntegerListEntries - i - 1));
			mcpPostProcIntegerListEntries--;
			if (!mcpPostProcIntegerListEntries)
			{
				free (mcpPostProcIntegerList);
				mcpPostProcIntegerList = 0;
			}
			return;
		}
	}
	return;
}

const struct PostProcIntegerRegStruct *mcpFindPostProcInteger (const char *name)
{
	int i;

	for (i=0; i < mcpPostProcIntegerListEntries; i++)
	{
		if (!strcmp (mcpPostProcIntegerList[i]->name, name))
		{
			return mcpPostProcIntegerList[i];
		}
	}
	return 0;
}

void mcpListAllPostProcInteger (const struct PostProcIntegerRegStruct ***postproc, int *count)
{
	*postproc = mcpPostProcIntegerList;
	*count = mcpPostProcIntegerListEntries;
}

