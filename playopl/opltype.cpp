/* OpenCP Module Player
 * copyright (c) '05-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
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
#include "adplug-git/src/adplug.h"
#include "adplug-git/src/players.h"
#include "adplug-git/src/player.h"
#include "adplug.h"
#include "players.h"
#include "player.h"
#include "types.h"
extern "C" {
#include "boot/plinkman.h"
#include "boot/psetting.h"
#include "cpiface/cpiface.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h"
#include "stuff/compat.h"
#include "stuff/err.h"
}
#include "opltype.h"

static int oplReadInfo(struct moduleinfostruct *m, struct ocpfilehandle_t *f, const char *buf, size_t len, const struct mdbReadInfoAPI_t *API)
{
	const char *filename = 0;
	CPlayers::const_iterator i;
	int j;

	dirdbGetName_internalstr (f->dirdb_ref, &filename);

	for(i = CAdPlug::players.begin(); i != CAdPlug::players.end(); i++)
	{
		for(j = 0; (*i)->get_extension(j); j++)
		{
			if(CFileProvider::extension(filename, (*i)->get_extension(j)))
			{
				snprintf(m->comment, sizeof(m->comment), "%s", (*i)->filetype.c_str());
				m->modtype.integer.i=MODULETYPE("OPL");
				return 0; /* we are not a dominant plugin */
			}
		}
	}
	return 0;

}

static const char *OPL_description[] =
{
	//                                                                          |
	"OPL style music is collection of fileformats that all have in common that",
	"they are to be played back on a OPL2/OPL3 chip. This chip (and clones) was",
	"standard on PC Adlib, Sound Blaster and many other cards. Open Cubic Player",
	"relies on libadplug for loading and rendering of these files.",
	NULL
};

static struct mdbreadinforegstruct oplReadInfoReg = {"adplug", oplReadInfo MDBREADINFOREGSTRUCT_TAIL};

static class CAdPlugDatabase *adplugdb_ocp;

int __attribute__ ((visibility ("internal"))) opl_type_init (PluginInitAPI_t *API)
{
	char *path=0;
	const char *home = getenv ("HOME");

	adplugdb_ocp = new CAdPlugDatabase();
	if (adplugdb_ocp)
	{
		makepath_malloc (&path, 0, cfDataDir, "adplug.db", 0);
		if (path)
		{
			adplugdb_ocp->load(path);
			free (path); path=0;
		}
		adplugdb_ocp->load("/usr/com/adplug/adplug.db");
		adplugdb_ocp->load("/usr/share/adplug/adplug.db");
		if (home && home[0])
		{
			path = (char *)malloc (strlen(home) + 19);
			if (path)
			{
				sprintf (path, "%s%s.adplug/adplug.db", home, home[strlen(home)]-1=='/'?"":"/");
				adplugdb_ocp->load(path);
				free (path); path=0;
			}
		}
		CAdPlug::set_database (adplugdb_ocp);
	}

	{
		CPlayers::const_iterator i;
		int j;
		const char *s;
		char _s[6];
		struct moduletype mt;

		for(i = CAdPlug::players.begin(); i != CAdPlug::players.end(); i++)
		{
			for(j = 0; (s=(*i)->get_extension(j)); j++)
			{
				strncpy(_s, s+1, 5);
				_s[5]=0;
				strupr(_s);
				API->fsRegisterExt(_s);
			}
		}

		mt.integer.i = MODULETYPE("OPL");
		API->fsTypeRegister (mt, OPL_description, "plOpenCP", &oplPlayer);
	}

	API->mdbRegisterReadInfo(&oplReadInfoReg);

	return errOk;
}

void __attribute__ ((visibility ("internal"))) opl_type_done (PluginCloseAPI_t *API)
{
	if (adplugdb_ocp)
	{
		CAdPlug::set_database (0);
		delete (adplugdb_ocp);
		adplugdb_ocp = 0;
	}

	{
		struct moduletype mt;

		mt.integer.i = MODULETYPE("OPL");
		API->fsTypeUnregister (mt);
	}

	API->mdbUnregisterReadInfo(&oplReadInfoReg);
}
