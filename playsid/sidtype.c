/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * SIDPlay file type detection routines for the fileselector
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
 *  -kb980717  Tammo Hinrichs <opencp@gmx.net>
 *    -first release
 */

#include "config.h"
#include <string.h>
#include "types.h"
#include "boot/plinkman.h"
#include "cpiface/cpiface.h"
#include "filesel/filesystem.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h"
#include "stuff/err.h"
#include "stuff/latin1.h"
#include "sidtype.h"

struct __attribute__((packed)) psidHeader
{
	/* All values in big-endian order. */
	char id[4];          /* 'PSID'                                         */
	char version[2];     /* 0x0001 or 0x0002                               */
	char data[2];        /* 16-bit offset to binary data in file           */
	char load[2];        /* 16-bit C64 address to load file to             */
	char init[2];        /* 16-bit C64 address of init subroutine          */
	char play[2];        /* 16-bit C64 address of play subroutine          */
	char songs[2];       /* number of songs                                */
	char start[2];       /* start song (1-256 !)                           */
	char speed[4];       /* 32-bit speed info                              */
	                     /* bit: 0=50 Hz, 1=CIA 1 Timer A (default: 60 Hz) */
	char name[32];       /* ASCII strings, 31 characters long and          */
	char author[32];     /* terminated by a trailing zero                  */
	char copyright[32];
	char flags[2];       /* only version 0x0002                            */
	char reserved[4];    /* only version 0x0002                            */
};


static int sidReadInfo(struct moduleinfostruct *m, struct ocpfilehandle_t *fp, const char *buf, size_t len)
{
	int i;
	struct psidHeader *ph;

	if (len<sizeof(struct psidHeader)+2)
		return 0;

	ph = (struct psidHeader *) buf;

	if ((!memcmp(ph->id,"PSID",4))||(!memcmp(ph->id,"RSID",4)))
	{
		m->modtype.integer.i=MODULETYPE("SID");
		m->channels=ph->songs[1];

		latin1_f_to_utf8_z (ph->name, sizeof (ph->name), m->title, sizeof (m->title));

		latin1_f_to_utf8_z (ph->author, sizeof (ph->author), m->composer, sizeof (m->composer));

		if (ph->copyright[0])
		{
			strcpy (m->comment, "(C)");
			latin1_f_to_utf8_z (ph->copyright, sizeof (ph->copyright), m->comment + 3, sizeof (m->comment) - 3);
		}
		return 1;
	}

	if ((buf[0]==0x00) & (buf[1]>=0x03) & (buf[2]==0x4c) &
	    (buf[4]>=buf[1]) & (buf[5]==0x4c) & (buf[7]>=buf[1]) )
	{
		char snginfo[33];

		m->modtype.integer.i=MODULETYPE("SID");
		m->channels=1;

		snginfo[32]=0;
		memcpy(snginfo,buf+0x22,0x20);
		for (i=0; i<0x20; i++)
		{
			if (snginfo[i] && snginfo[i]<27)
				snginfo[i]|=0x40;
			if (snginfo[i]>=0x60)
				snginfo[i]=0;
		}
		if (strlen(snginfo)<6)
			strcpy(snginfo,"raw SID file");

		latin1_f_to_utf8_z (snginfo, strlen (snginfo), m->title, sizeof (m->title));
		m->composer[0] = 0;
		m->comment[0] = 0;
		return 1;
	}

	if (!memcmp(buf,"SIDPLAY INFOFILE",16) && (((char *)buf)[16]==0x0a || ((char *)buf)[16]==0x0d))
	{
		strcpy(m->title, "SIDPlay info file");
		m->modtype.integer.i = 0;
		return 1;
	}

	return 0;
}

static struct mdbreadinforegstruct sidReadInfoReg = {"SID", sidReadInfo MDBREADINFOREGSTRUCT_TAIL};

static const char *SID_description[] =
{
	//                                                                          |
	"SID files are memory dumps of executable code for Commodore 64. This code",
	"controls the SID audio chip. Open Cubic Player uses libsidplayfp that",
	"emulates all the needed hardware from the C64.",
	NULL
};

int __attribute__ ((visibility ("internal"))) sid_type_init (struct PluginInitAPI_t *API)
{
	struct moduletype mt;

	API->fsRegisterExt("SID");
	API->fsRegisterExt("RSID");

	mt.integer.i = MODULETYPE("SID");
	API->fsTypeRegister (mt, SID_description, "plOpenCP", &sidPlayer);

	API->mdbRegisterReadInfo(&sidReadInfoReg);

	return errOk;
}

void __attribute__ ((visibility ("internal"))) sid_type_done (struct PluginCloseAPI_t *API)
{
	struct moduletype mt;

	mt.integer.i = MODULETYPE("SID");
	API->fsTypeUnregister (mt);

	API->mdbUnregisterReadInfo(&sidReadInfoReg);
}
