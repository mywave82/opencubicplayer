/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) '05-'21 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * WAVPlay file type detection routines for the fileselector
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
 *  -nb980510   Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *    -first release
 */

#include "config.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "filesel/filesystem.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h"

static unsigned char wavGetModuleType(const char *buf)
{
	if ((*(uint32_t *) buf     == uint32_little(0x46464952))&& // "RIFF"
	    (*(uint32_t *)(buf+ 8) == uint32_little(0x45564157))&& // "WAVE"
	    (*(uint32_t *)(buf+12) == uint32_little(0x20746D66))&& // "fmt "
	    (*(uint16_t *)(buf+20) == uint16_little(1)))           // PCM format
		return 1;
	return 0;
}


static int wavReadMemInfo(struct moduleinfostruct *m, const char *buf, size_t len)
{
	return 0;
}

static int RIFF_INFO (struct ocpfilehandle_t *fp, uint32_t len, char *dst, int dstlen)
{
	uint32_t toread;
	uint32_t toskip;

	if (len >= (dstlen - 1))
	{
		toread = dstlen - 1;
		toskip = len - toread;
	} else {
		toread = len;
		toskip = 0;
	}
	if (fp->read (fp, dst, toread))
	{
		return 1;
	}
	dst[toread] = 0;
	if (toskip)
	{
		if (fp->seek_cur (fp, toskip))
		{
			return -1;
		}
	}
	return 0;
}

static int wavReadInfo(struct moduleinfostruct *m, struct ocpfilehandle_t *fp, const char *buf, size_t len)
{
	if (len < 40)
	{
		return 0;
	}
	if (!wavGetModuleType (buf))
	{
		return 0;
	}

	bzero (m, sizeof (*m));

	m->modtype.integer.i = MODULETYPE("WAV");

	m->channels=uint16_little(*(uint16_t *)(buf+20+2));
	snprintf (m->comment, sizeof (m->comment), "%dHz, %2d bit, %s",
		(int)int32_little((*(uint32_t *)(buf+20+4))),
		*(uint16_t *)(buf+20+14),
		m->channels==1?"mono":"stereo");

	if (*(uint32_t *)(buf+20+16)==uint32_little(61746164)) // "data"
	{
		uint32_t datalen = uint32_little(*(uint32_t *)(buf+20+20));
		uint32_t LIST, LISTlen;
		m->playtime = datalen / uint32_little(*(uint32_t *)(buf+20+8));

		if (fp->seek_set (fp, 20 + 20 + 4 + datalen) ||
		    ocpfilehandle_read_uint32_le (fp, &LIST) ||
		    ocpfilehandle_read_uint32_le (fp, &LISTlen))
		{
			goto out;
		}
		if (LIST != uint32_little (0x5453494c))  // "LIST"
		{
			goto out;
		}
		while (LISTlen >= 8)
		{
			uint32_t CHUNK;
			uint32_t CHUNKlen;
			if (ocpfilehandle_read_uint32_le (fp, &CHUNK) ||
		            ocpfilehandle_read_uint32_le (fp, &CHUNKlen))
			{
				goto out;
			}
			LISTlen -= 8;
			switch (CHUNK)
			{
				case uint32_little(0x4d414e49): // "INAM"
					if (RIFF_INFO(fp, CHUNKlen, m->title, sizeof (m->title)))
					{
						goto out;
					}
					break;
				case uint32_little(0x44525049): // "IPRD"
					if (RIFF_INFO(fp, CHUNKlen, m->album, sizeof (m->album)))
					{
						goto out;
					}
					break;
				case uint32_little(0x54524149): // "IART"
					if (RIFF_INFO(fp, CHUNKlen, m->artist, sizeof (m->artist)))
					{
						goto out;
					}
					break;
				case uint32_little(0x44524349): // "ICRD"
					{
						char temp[16];
						if (RIFF_INFO(fp, CHUNKlen, temp, sizeof (temp)))
						{
							goto out;
						}
						if (isdigit(temp[0]) && isdigit(temp[1]) && isdigit(temp[2]) && isdigit(temp[3]))
						{
							if (!temp[4])
							{
								m->date = (uint32_t)(atoi(temp) << 16);
							} else if ((temp[4]=='-') && isdigit(temp[5]) && isdigit(temp[6]) && (temp[7] == '-') && isdigit(temp[8]) && isdigit(temp[9]))
							{
								temp[10] = 0;
								m->date = ((uint32_t)(atoi(temp)) << 16) | ((uint32_t)(atoi(temp + 5)) << 8) | (uint32_t)(atoi(temp + 8));
							}
						}
						break;
					}
				case uint32_little(0x544d4349): // "ICMT"
					if (RIFF_INFO(fp, CHUNKlen, m->comment, sizeof (m->comment)))
					{
						goto out;
					}
					break;
				case uint32_little(0x524e4749): // "IGNR"
					if (RIFF_INFO(fp, CHUNKlen, m->style, sizeof (m->style)))
					{
						goto out;
					}
					break;
				default:
					if (fp->seek_cur (fp, CHUNKlen))
					{
						goto out;
					}
					break;
			}
			LISTlen -= CHUNKlen;
		}
	}

out:
	return 1;
}

const char *WAV_description[] =
{
	//                                                                          |
	"WAV files as PCM uncompress raw audio samples stored inside a RIFF file",
	"container. Open Cubic Player are able to read WAV files stored with 8/16bit",
	"resolution in mono/stereo. Commonly used fileformat before short audio clips",
	"and high quality music before FLAC was invented.",
	NULL
};

struct interfaceparameters WAV_p =
{
	"playwav", "wavPlayer",
	0, 0
};

struct mdbreadinforegstruct wavReadInfoReg = {"WAVE", wavReadMemInfo, wavReadInfo, 0 MDBREADINFOREGSTRUCT_TAIL};
