/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'25 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Fileselector file type detection routines (covers play lists and internal
 * cache files)
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
 *  -kb980717   Tammo Hinrichs <opencp@gmx.net>
 *    -first release
 */

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "types.h"
#include "dirdb.h"
#include "filesystem.h"
#include "fsptype.h"
#include "mdb.h"


static int fsReadInfo(struct moduleinfostruct *m, struct ocpfilehandle_t *fp, const char *buf, size_t len, const struct mdbReadInfoAPI_t *API)
{
	if (!memcmp(buf, "CPArchiveCache\x1B\x00", 16))
		strcpy(m->title, "openCP archive data base (old!)");
	if (!memcmp(buf, "CPArchiveCache\x1B\x01", 16))
		strcpy(m->title, "openCP archive data base (old)");
	if (!memcmp(buf, "OCPArchiveMeta\x1b\x00", 16))
		strcpy(m->title, "openCP archive data base");
	if (!memcmp(buf, "Cubic Player Module Information Data Base\x1B\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 60))
		strcpy(m->title, "openCP module info data base (old)");
	if (!memcmp(buf, "Cubic Player Module Information Data Base II\x1B\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 60))
		strcpy(m->title, "openCP module info data base (big-endian)");
	if (!memcmp(buf, "Cubic Player Module Information Data Base II\x1B\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01", 60))
		strcpy(m->title, "openCP module info data base (little-endian)");
	if (!memcmp(buf, dirdbsigv1, sizeof(dirdbsigv1)))
		strcpy(m->title, "openCP dirdb/medialib: db v1");
	if (!memcmp(buf, dirdbsigv2, sizeof(dirdbsigv2)))
		strcpy(m->title, "openCP dirdb/medialib: db v2");
	if (!memcmp(buf, "Cubic Player MusicBrainz Data Base\x1B\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 64))
		strcpy(m->title, "openCP MusicBrainz Data Base");
	return 0;
}

struct mdbreadinforegstruct fsReadInfoReg = {"DataBases", fsReadInfo MDBREADINFOREGSTRUCT_TAIL};
