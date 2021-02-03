/* OpenCP Module Player
 * copyright (c) '05-'21 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * OPL-compatible file type detection routines for the file selector
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <adplug/adplug.h>
#include <adplug/players.h>
#include <adplug/player.h>
#include "types.h"
extern "C" {
#include "boot/plinkman.h"
#include "filesel/filesystem.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h"
#include "stuff/compat.h"
}

static int oplReadMemInfo(struct moduleinfostruct *m, const char *buf, size_t len)
{
	CPlayers::const_iterator i;
	int j;

	char name[13];
	strncpy(name, m->name, 12);
	name[12]=0;

	for(i = CAdPlug::players.begin(); i != CAdPlug::players.end(); i++)
		for(j = 0; (*i)->get_extension(j); j++)
		{
			if(CFileProvider::extension(name, (*i)->get_extension(j)))
			{
				snprintf(m->comment, sizeof(m->comment), "%s", (*i)->filetype.c_str());
				m->modtype=mtOPL;
				return 0; /* we are not a dominant plugin */
			}
		}
	return 0;
}
static int oplReadInfo(struct moduleinfostruct *m, struct ocpfilehandle_t *f, const char *buf, size_t len)
{
	return oplReadMemInfo(m, buf, len);
}

static void oplEvent(int event)
{
	switch (event)
	{
		case mdbEvInit:
		{
			CPlayers::const_iterator i;
			int j;
			const char *s;
			char _s[6];
			for(i = CAdPlug::players.begin(); i != CAdPlug::players.end(); i++)
				for(j = 0; (s=(*i)->get_extension(j)); j++)
				{
					strncpy(_s, s+1, 5);
					_s[5]=0;
					strupr(_s);
					fsRegisterExt(_s);
				}
		}
	}
}

static struct mdbreadinforegstruct oplReadInfoReg = {oplReadMemInfo, oplReadInfo, oplEvent MDBREADINFOREGSTRUCT_TAIL};

static void __attribute__((constructor))init(void)
{
	mdbRegisterReadInfo(&oplReadInfoReg);
}

static void __attribute__((destructor))done(void)
{
	mdbUnregisterReadInfo(&oplReadInfoReg);
}

extern "C" {
	const char *dllinfo = "";
	struct linkinfostruct dllextinfo =
	{
		"opltype" /* name */,
		"OpenCP OPL Detection (c) 2005-'21 Stian Skjelstad" /* desc */,
		DLLVERSION /* ver */
	};
}
