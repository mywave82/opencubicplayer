/* OpenCP Module Player
 * copyright (c) 2026 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * QOAPlay file type detection routines for the fileselector
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
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "boot/plinkman.h"
#include "filesel/filesystem.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h"
#include "stuff/err.h"
#include "qoatype.h"

static int qoaReadInfo(struct moduleinfostruct *m, struct ocpfilehandle_t *fp, const char *buf, size_t len, const struct mdbReadInfoAPI_t *API)
{
	uint32_t samples;
	uint8_t channels;
	uint32_t samplerate;

	if (len < 16)
	{
		return 0;
	}
	if ((buf[0] != 'q') ||
	    (buf[1] != 'o') ||
	    (buf[2] != 'a') ||
	    (buf[3] != 'f'))
	{
		return 0;
	}
	samples = (uint8_t)buf[7]        |
	         ((uint8_t)buf[6] <<  8) |
	         ((uint8_t)buf[5] << 16) |
	         ((uint8_t)buf[4] << 24);
	channels = buf[8];
	samplerate = (uint8_t)buf[11]        |
	            ((uint8_t)buf[10] <<  8) |
	            ((uint8_t)buf[ 9] << 16);
	if (channels == 0)
	{
		return 0;
	}
	if (samplerate == 0)
	{
		return 0;
	}

	memset (m, 0, sizeof (*m));
	m->modtype.integer.i = MODULETYPE("QOA");

	m->channels = channels;
	snprintf (m->comment, sizeof (m->comment), "%uHz, %u channels",
		(unsigned int)samplerate,
		channels);

	m->playtime = samples / samplerate;

	return 1;
}

static const char *QOA_description[] =
{
	//                                                                          |
	"QOA (The Quite OK Audio Format for Fast, Lossy Compression) is a file format",
	"created by Dominic Szablewski. All information is available on",
	"https://qoaformat.org .",
	NULL
};

static struct mdbreadinforegstruct qoaReadInfoReg = {"QOA", qoaReadInfo MDBREADINFOREGSTRUCT_TAIL};

OCP_INTERNAL int qoa_type_init (struct PluginInitAPI_t *API)
{
	struct moduletype mt;

	API->fsRegisterExt ("QOA");

	mt.integer.i = MODULETYPE("QOA");
	API->fsTypeRegister (mt, QOA_description, "plOpenCP", &qoaPlayer);

	API->mdbRegisterReadInfo(&qoaReadInfoReg);

	return errOk;
}

OCP_INTERNAL void qoa_type_done (struct PluginCloseAPI_t *API)
{
	struct moduletype mt;

	mt.integer.i = MODULETYPE("QOA");
	API->fsTypeUnregister (mt);

	API->mdbUnregisterReadInfo(&qoaReadInfoReg);
}
