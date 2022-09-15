/* OpenCP Module Player
 * copyright (c) 2007-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * FLACPlay file type detection routines for the fileselector
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
#include "types.h"
#include "boot/plinkman.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h"
#include "stuff/err.h"
#include "flactype.h"

static int flacReadInfo(struct moduleinfostruct *m, struct ocpfilehandle_t *fp, const char *buf, size_t len)
{
	const uint8_t *mybuf;
	size_t mylen;

	if (len<4)
		return 0;

	if (memcmp(buf, "fLaC", 4))
		return 0;
	m->modtype.integer.i=MODULETYPE("FLAC");

	mybuf = (const uint8_t *)buf + 4;
	mylen = len - 4;

	while (mylen>=4)
	{
		uint8_t BLOCK_TYPE;
		uint_least32_t length;

		BLOCK_TYPE = *mybuf++;
		length = (mybuf[0]<<16) | (mybuf[1]<<8) | (mybuf[2]);
		mybuf+=3;
		mylen-=4;

		if (length>mylen)
			break; /* chunk goes outside the range we got */

		switch (BLOCK_TYPE&0x7f)
		{
			case (0x00): /* STREAMINFO */
				if (length>=18)
				{
					uint_least64_t l;
					uint_least32_t rate;
/*
					fprintf(stderr, "min block size = %d\n", (mybuf[0]<<8) | mybuf[1]);
					fprintf(stderr, "max block size = %d\n", (mybuf[2]<<8) | mybuf[3]);
					fprintf(stderr, "min frame size = %d\n", (mybuf[4]<<16) | (mybuf[5]<<8) | mybuf[6]);
					fprintf(stderr, "max frame size = %d\n", (mybuf[7]<<16) | (mybuf[8]<<8) | mybuf[9]);
					fprintf(stderr, "sample rate = %d\n", (mybuf[10]<<12) | (mybuf[11]<<4) | mybuf[12]>>4);
					fprintf(stderr, "channels = %d\n", ((mybuf[12]>>1)&0x07) + 1);
					fprintf(stderr, "bits per sample = %d\n", (((mybuf[12]<<5)&0x10)|mybuf[13]>>4)+1);
*/
					l = ((((uint_least64_t)mybuf[13])<<32)&0xf00000000ll) | (mybuf[14]<<24) | (mybuf[15]<<16) | (mybuf[16]<<8) | mybuf[17];
					rate = (mybuf[10]<<12) | (mybuf[11]<<4) | mybuf[12]>>4;
/*
					fprintf(stderr, "length = %lld\n", l);
*/
					m->channels = (((mybuf[12]>>1)&0x07)+1);
					m->playtime = l/rate;
				}
				break;
			case (0x01): /* PADDING */
				break;
			case (0x02): /* APPLICATION */
				break;
			case (0x03): /* SEEKTABLE */
				break;
			case (0x04): /* VORBIS_COMMENT */
				{
					const uint8_t *mymybuf = mybuf;
					uint32_t mymylen = length;

					uint32_t l;
					uint32_t num, n;
					/*int i;*/

					if (mymylen<4)
						break;
					l = (mymybuf[0]) | (mymybuf[1]<<8) | (mymybuf[2]<<16) | (mymybuf[3]<<24);
					mymylen-=4;
					mymybuf+=4;

					if (mymylen<l)
						break;
/*
					fprintf(stderr, "VENDORSTRING=");
					for (i=0;i<l;i++)
						fprintf(stderr, "%c", mymybuf[i]);
					fprintf(stderr, "\n");
*/
					mymylen-=l;
					mymybuf+=l;

					if (mymylen<4)
						break;
					num = (mymybuf[0]) | (mymybuf[1]<<8) | (mymybuf[2]<<16) | (mymybuf[3]<<24);
					mymylen-=4;
					mymybuf+=4;

					for (n=0;n<num;n++)
					{
						if (mymylen<4)
							break;
						l = (mymybuf[0]) | (mymybuf[1]<<8) | (mymybuf[2]<<16) | (mymybuf[3]<<24);
						mymylen-=4;
						mymybuf+=4;

						if (mymylen<l)
							break;

						if ((l >= 7) && (!strncasecmp((char *)mymybuf, "artist=", 7)))
						{
							int copy = l - 7;
							if (copy >= sizeof (m->artist))
							{
								copy = sizeof (m->artist)-1;
							}
							bzero (m->artist, sizeof (m->artist));
							memcpy (m->artist, mymybuf + 7, copy);
						} else if ((l >= 6) && (!strncasecmp((char *)mymybuf, "title=", 6)))
						{
							int copy = l - 6;
							if (copy >= sizeof (m->title))
							{
								copy = sizeof (m->title)-1;
							}
							bzero (m->title, sizeof (m->title));
							memcpy (m->title, mymybuf + 6, copy);
						} else if ((l>= 6) && (!strncasecmp((char *)mymybuf, "album=", 6)))
						{
							int copy = l - 6;
							if (copy >= sizeof (m->album))
							{
								copy = sizeof (m->album)-1;
							}
							bzero (m->album, sizeof (m->album));
							memcpy (m->album, mymybuf + 6, copy);
						} else if ((l >= 6) &&(!strncasecmp((char *)mymybuf, "genre=", 6)))
						{
							int copy = l - 6;
							if (copy >= sizeof (m->style))
							{
								copy = sizeof (m->style)-1;
							}
							bzero (m->style, sizeof (m->style));
							memcpy (m->style, mymybuf + 6, copy);
						} else if ((l >= 9) &&(!strncasecmp((char *)mymybuf, "composer=", 9)))
						{
							int copy = l - 9;
							if (copy >= sizeof (m->composer))
							{
								copy = sizeof (m->composer)-1;
							}
							bzero (m->composer, sizeof (m->composer));
							memcpy (m->composer, mymybuf + 9, copy);
						}
/*
						fprintf(stderr, "COMMENT(%d/%d)=", n+1, num);
						for (i=0;i<l;i++)
							fprintf(stderr, "%c(%d)", mymybuf[i], mymybuf[i]);
						fprintf(stderr, "\n");
*/

						mymylen-=l;
						mymybuf+=l;
					}
				}
				break;
			case (0x05): /* CUESHEET */
				break;
			case (0x06): /* PICTURE */
				break;
		}

		if (BLOCK_TYPE&0x80)
			break; /* This terminates the BLOCK list */

		mylen-=length;
		mybuf+=length;
	}
	return 1;
}

static struct mdbreadinforegstruct flacReadInfoReg = {"FLAC", flacReadInfo, 0 MDBREADINFOREGSTRUCT_TAIL};

static const char *FLAC_description[] =
{
	//                                                                          |
	"FLAC is an open format, royalty free, lossless, audio compressed file",
	"format. Ideal for storing perfect backup of music collections.",
	NULL
};

static const struct interfaceparameters FLAC_p =
{
	"autoload/40-playflac", "flacPlayer",
	0, 0
};

int __attribute__ ((visibility ("internal"))) flac_type_init (void)
{
	struct moduletype mt;

	fsRegisterExt("FLA");
	fsRegisterExt("FLAC");
	fsRegisterExt("FLC");

	mt.integer.i = MODULETYPE("FLAC");
	fsTypeRegister (mt, FLAC_description, "plOpenCP", &FLAC_p);

	mdbRegisterReadInfo(&flacReadInfoReg);

	return errOk;
}

void __attribute__ ((visibility ("internal"))) flac_type_done(void)
{
	mdbUnregisterReadInfo(&flacReadInfoReg);
}
