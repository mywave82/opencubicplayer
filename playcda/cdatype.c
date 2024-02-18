/* OpenCP Module Player
 * copyright (c) 2005-'24 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * .cda file type detection routines for the file selector
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
#include "types.h"
#include "boot/plinkman.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h"
#include "stuff/err.h"
#include "cdatype.h"

static const char *CDA_description[] =
{
	//                                                                          |
	"CDA - CDROM Digital Audio, is a virtual file that links a cdrom audio",
	"track or complete disc to an actual device. The device can be both physical",
	"or virtual CDROM drive. Marking a random file in the filesystem with this",
	"filetype will not make it playable.",
	NULL
};

OCP_INTERNAL int cda_type_init (struct PluginInitAPI_t *API)
{
	struct moduletype mt;

	API->fsRegisterExt ("CDA");

	mt.integer.i = MODULETYPE("CDA");
	API->fsTypeRegister (mt, CDA_description, "plOpenCP", &cdaPlayer);

	return errOk;
}

OCP_INTERNAL void cda_type_done (struct PluginCloseAPI_t *API)
{
	struct moduletype mt;

	mt.integer.i = MODULETYPE("CDA");
	API->fsTypeUnregister (mt);
}
