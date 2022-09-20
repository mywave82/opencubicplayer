/* OpenCP Module Player
 * copyright (c) 2019-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * HVL and AHX file type detection routines for the fileselector
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
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "boot/plinkman.h"
#include "filesel/filesystem.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h"
#include "stuff/cp437.h"
#include "stuff/err.h"
#include "hvltype.h"

static int hvlReadMemInfo_ahx(struct moduleinfostruct *m, const char *_buf, size_t buflen)
{
	const uint8_t *buf = (const uint8_t *)_buf;
	const uint8_t *bptr, *tptr;
	uint32_t  i, posn, insn, ssn, trkn, trkl, trk0;

	m->channels = 4; /* fixed */
	snprintf (m->comment, sizeof (m->comment), "AHX format");

	if (buflen < 14)
	{
		return 0;
	}

	trk0 = (buf[6]&0x80)==0;
	posn = ((buf[6]&0x0f)<<8)|buf[7];
	insn = buf[12];
	ssn  = buf[13];
	trkl = buf[10];
	trkn = buf[11];

	// Calculate the size of all instrument PList buffers
	bptr = buf + 14;
	bptr += ssn*2;    // Skip past the subsong list
	bptr += posn*4*2; // Skip past the positions
	bptr += trkn*trkl*3;
	if (trk0)
	{ /* Track0 stored on disk, or not */
		bptr += trkl*3;
	}

	// *NOW* we can finally calculate PList space
	for ( i=1; i<=insn; i++ )
	{
		if ((bptr+22-buf) > buflen)
		{
			return 0;
		}
		bptr += 22 + bptr[21]*4;
		if ((bptr-buf) > buflen)
		{
			return 0;
		}
	}

	tptr = bptr;
	while (1)
	{
		if ((tptr-buf) > buflen)
		{
			return 0;
		}
		if (!*(tptr++))
		{
			cp437_f_to_utf8_z ((char *)bptr, strlen ((char *)bptr), m->title, sizeof (m->title));
			return 1;
		}
	}
}

static int hvlReadMemInfo_hvl(struct moduleinfostruct *m, const char *_buf, size_t buflen)
{
	const uint8_t *buf = (const uint8_t *)_buf;
	const uint8_t *bptr, *tptr;
	uint32_t  i, j, posn, insn, ssn, trkn, trkl, trk0, chnn;

	snprintf (m->comment, sizeof (m->comment), "HVL format");

	if (buflen < 16)
	{
		return 0;
	}

	trk0 = (buf[6]&0x80)==0;
	posn = ((buf[6]&0x0f)<<8)|buf[7];
	insn = buf[12];
	ssn  = buf[13];
	chnn = (buf[8]>>2)+4;
	trkl = buf[10];
	trkn = buf[11];

	m->channels = chnn;

	// Calculate the size of all instrument PList buffers
	bptr = buf + 16;
	bptr += ssn*2;       // Skip past the subsong list
	bptr += posn*chnn*2; // Skip past the positions

	for ( i=trk0?0:1; i<=trkn; i++ )
	{
		for( j=0; j<trkl; j++ )
		{
			if ((bptr-buf) >= buflen)
			{
				return 0;
			}
			if( bptr[0] == 0x3f )
			{
				bptr++;
			} else {
				bptr += 5;
			}
			if ((bptr-buf) > buflen)
			{
				return 0;
			}
		}
	}

	for ( i=1; i<=insn; i++ )
	{
		if ((bptr+22-buf) > buflen)
		{
			return 0;
		}
		bptr += 22 + bptr[21]*5;
		if ((bptr-buf) > buflen)
		{
			return 0;
		}
	}

	tptr = bptr;
	while (1)
	{
		if ((tptr-buf) > buflen)
		{
			return 0;
		}
		if (!*(tptr++))
		{
			cp437_f_to_utf8_z ((char *)bptr, strlen ((char *)bptr), m->title, sizeof (m->title));
			return 1;
		}
	}
}

static int hvlReadMemInfo(struct moduleinfostruct *m, const char *buf, size_t len)
{

	if (len >= 4)
	{
		if( ( buf[0] == 'T' ) &&
		    ( buf[1] == 'H' ) &&
		    ( buf[2] == 'X' ) &&
		    ( buf[3] < 3 ) )
		{
			m->modtype.integer.i = MODULETYPE("HVL");
			return hvlReadMemInfo_ahx (m, buf, len);;
		}

		if( ( buf[0] == 'H' ) &&
		    ( buf[1] == 'V' ) &&
		    ( buf[2] == 'L' ) &&
		    ( buf[3] < 2 ) )
		{
			m->modtype.integer.i = MODULETYPE("HVL");
			return hvlReadMemInfo_hvl (m, buf, len);;
		}
	}

	return 0;
}

static int hvlReadInfo(struct moduleinfostruct *m, struct ocpfilehandle_t *fp, const char *buf, size_t len)
{
	size_t filelen;
	char *buffer;
	int retval = 0;

	if (len < 4)
	{
		return 0;
	}

	/* header miss-match */
	if ( ! ( ( ( buf[0] == 'T' ) &&
	           ( buf[1] == 'H' ) &&
	           ( buf[2] == 'X' ) &&
	           ( buf[3] < 3 ) ) ||
	         ( ( buf[0] == 'H' ) &&
	           ( buf[1] == 'V' ) &&
	           ( buf[2] == 'L' ) &&
	           ( buf[3] < 2 ) ) ) )
	{
		return 0;
	}

	filelen = fp->filesize(fp);

	/* hvlReadMemInfo already had the full file at the first pass */
	if (filelen == len)
	{
		return 0;
	}
	if (filelen > (1024*1024))
	{
		return 0;
	}

	if (filelen < 20)
	{
		return 0;
	}

	m->modtype.integer.i = MODULETYPE("HVL");
	buffer = malloc (filelen);
	fp->seek_set (fp, 0);
	if (fp->read (fp, buffer, filelen) == filelen)
	{
		retval = hvlReadMemInfo (m, buffer, filelen);
	}
	free (buffer);
	fp->seek_set (fp, 0);
	return retval;
}

static const char *HVL_description[] =
{
	//                                                                          |
	"HVL/AHX file format originates in file format created in the mid '90s by",
	"Dexter and Pink of Abyss. The fileformat does not store any samples, but",
	"uses parameterized synthesizers. Files are 4-16 channels and also features",
	"stereo. A modern editor also exists - Hively Tracker.",
	NULL

};

static struct mdbreadinforegstruct hvlReadInfoReg = {"HVL/AHX", hvlReadInfo MDBREADINFOREGSTRUCT_TAIL};

int __attribute__ ((visibility ("internal"))) hvl_type_init (struct PluginInitAPI_t *API)
{
	struct moduletype mt;

	API->fsRegisterExt ("HVL");
	API->fsRegisterExt ("AHX");

	mt.integer.i = MODULETYPE("HVL");
	API->fsTypeRegister (mt, HVL_description, "plOpenCP", &hvlPlayer);

	API->mdbRegisterReadInfo(&hvlReadInfoReg);

	return errOk;
}

void __attribute__ ((visibility ("internal"))) hvl_type_done (struct PluginCloseAPI_t *API)
{
	struct moduletype mt;

	mt.integer.i = MODULETYPE("HVL");
	API->fsTypeUnregister (mt);

	API->mdbUnregisterReadInfo(&hvlReadInfoReg);
}
