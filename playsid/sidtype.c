/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'24 Stian Skjelstad <stian.skjelstad@gmail.com>
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
#include "filesel/dirdb.h"
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


static int sidReadInfo(struct moduleinfostruct *m, struct ocpfilehandle_t *fp, const char *buf, size_t len, const struct mdbReadInfoAPI_t *API)
{
	int i;
	struct psidHeader *ph;
	const char *filename = 0;
	unsigned int filenamelen;

	if (len<sizeof(struct psidHeader)+2)
		return 0;

	ph = (struct psidHeader *) buf;

	if ((!memcmp(ph->id,"PSID",4))||(!memcmp(ph->id,"RSID",4)))
	{
		m->modtype.integer.i=MODULETYPE("SID");
		m->channels=ph->songs[1];

		API->latin1_f_to_utf8_z (ph->name, sizeof (ph->name), m->title, sizeof (m->title));

		API->latin1_f_to_utf8_z (ph->author, sizeof (ph->author), m->composer, sizeof (m->composer));

		if (ph->copyright[0])
		{
			strcpy (m->comment, "(C)");
			API->latin1_f_to_utf8_z (ph->copyright, sizeof (ph->copyright), m->comment + 3, sizeof (m->comment) - 3);
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

		API->latin1_f_to_utf8_z (snginfo, strlen (snginfo), m->title, sizeof (m->title));
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

	API->dirdb->GetName_internalstr (fp->dirdb_ref, &filename);
	filenamelen = strlen (filename);

	if ((filenamelen > 4) && (
	     (!strcasecmp(filename + filenamelen - 4, ".mus")) ||
	     (!strcasecmp(filename + filenamelen - 4, ".sid"))) &&
	    (len >= 8))
	{
		uint_least32_t voice1Index, voice2Index, voice3Index;
		const uint_least16_t SIDTUNE_MUS_HLT_CMD = 0x14F;
		const unsigned char *buf2 = (const unsigned char *)buf;

		// Skip load address and 3x length entry.
		voice1Index = 2 + 3 * 2;
		// Add length of voice 1 data.
		voice1Index += buf2[2] | (buf2[3] << 8);
		// Add length of voice 2 data.
		voice2Index = voice1Index + (buf2[4] | (buf2[5] << 8));
		// Add length of voice 3 data.
		voice3Index = voice2Index + (buf2[6] | (buf2[7] << 8));

		if (
		/* only scan voice1Index if we have enough data available */
		     (((voice1Index <= len) &&
		       ((buf2[voice1Index - 1] | (buf2[voice1Index - 2] << 8)) == SIDTUNE_MUS_HLT_CMD)) ||
		      ((voice1Index > len) &&
		       (voice1Index <= fp->filesize(fp)))) &&
		/* only scan voice2Index if we have enough data available */
		     (((voice2Index <= len) &&
		       ((buf2[voice2Index - 1] | (buf2[voice2Index - 2] << 8)) == SIDTUNE_MUS_HLT_CMD)) ||
		      ((voice2Index > len) &&
		       (voice2Index <= fp->filesize(fp)))) &&
		/* only scan voice3Index if we have enough data available */
		     (((voice3Index <= len) &&
		       ((buf2[voice3Index - 1] | (buf2[voice3Index - 2] << 8)) == SIDTUNE_MUS_HLT_CMD)) ||
		      ((voice3Index > len) &&
		       (voice3Index <= fp->filesize(fp))))
		)
		{
			m->modtype.integer.i=MODULETYPE("SID");
			m->channels=1;
			strcpy(m->comment, "Sidplayer MUS file");
			return 1;
		}
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

OCP_INTERNAL int sid_type_init (struct PluginInitAPI_t *API)
{
	struct moduletype mt;

	API->fsRegisterExt("MUS");
	API->fsRegisterExt("SID");
	API->fsRegisterExt("RSID");
	API->fsRegisterExt("PSID");

	mt.integer.i = MODULETYPE("SID");
	API->fsTypeRegister (mt, SID_description, "plOpenCP", &sidPlayer);

	API->mdbRegisterReadInfo(&sidReadInfoReg);

	return errOk;
}

OCP_INTERNAL void sid_type_done (struct PluginCloseAPI_t *API)
{
	struct moduletype mt;

	mt.integer.i = MODULETYPE("SID");
	API->fsTypeUnregister (mt);

	API->mdbUnregisterReadInfo(&sidReadInfoReg);
}
