/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * Export _dllinfo for FSTYPES.DLL
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
#include "types.h"
#include "boot/plinkman.h"
#include "mdb.h"

extern struct mdbreadinforegstruct
#ifdef HAVE_MAD
	ampegpReadInfoReg,
#endif
	itpReadInfoReg,
	oggReadInfoReg,
	gmdReadInfoReg,
	xmpReadInfoReg,
	gmiReadInfoReg,
	wavReadInfoReg;

static void __attribute__((constructor))init(void)
{
#ifdef HAVE_MAD
	mdbRegisterReadInfo(&ampegpReadInfoReg);
#endif
	mdbRegisterReadInfo(&itpReadInfoReg);
	mdbRegisterReadInfo(&oggReadInfoReg);
	mdbRegisterReadInfo(&gmdReadInfoReg);
	mdbRegisterReadInfo(&xmpReadInfoReg);
	mdbRegisterReadInfo(&gmiReadInfoReg);
	mdbRegisterReadInfo(&wavReadInfoReg);
}

static void __attribute__((destructor))done(void)
{
#ifdef HAVE_MAD
	mdbUnregisterReadInfo(&ampegpReadInfoReg);
#endif
	mdbUnregisterReadInfo(&itpReadInfoReg);
	mdbUnregisterReadInfo(&oggReadInfoReg);
	mdbUnregisterReadInfo(&gmdReadInfoReg);
	mdbUnregisterReadInfo(&xmpReadInfoReg);
	mdbUnregisterReadInfo(&gmiReadInfoReg);
	mdbUnregisterReadInfo(&wavReadInfoReg);
}

#ifndef SUPPORT_STATIC_PLUGINS
char *dllinfo = "";
#endif

DLLEXTINFO_PREFIX struct linkinfostruct dllextinfo = {"fstypes", "OpenCP Module Detection (c) 1994-09 Niklas Beisert, Tammo Hinrichs", DLLVERSION, 0 LINKINFOSTRUCT_NOEVENTS};
