/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * GMDPlay file type detection routines for the fileselector
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
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "boot/plinkman.h"
#include "filesel/filesystem.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h"
#include "stuff/cp437.h"
#include "stuff/err.h"
#include "gmdtype.h"

static uint32_t gmdGetModuleType(const char *buf, const size_t len)
{
	if (len>=0x30)
	{ /* STM check */
		int i;

		/* first 20 bytes should be ASCII */
		for (i=0; i < 20; i++)
		{
			if (buf[i] & 0x80) goto nostm;
		}
		/* next bytes, is the tracker / converter software */
		for (i=0; i < 8; i++)
		{
			if (buf[i+0x14] & 0x80) goto nostm;
		}

		if ((buf[0x1c] != 0x1a) && (buf[i+0x1c] != 0x02)) goto nostm; /* signature */

		if (buf[0x1d] != 0x02) goto nostm; /* type, we only support modules */

		if (buf[0x1e] != 0x02) goto nostm; /* major */

		if ((buf[0x1f] != 10) && (buf[0x1f] != 20) && (buf[0x1f] != 21)) goto nostm; /* minor */

		if (memcmp (buf+0x14, "!Scream!", 4) && memcmp (buf+0x14, "BMOD2STM", 4) && memcmp (buf+0x14, "WUZAMOD!", 4)) goto nostm;

		return MODULETYPE("STM");
	}
nostm:
	if (len>=0x60)
	{
		/* TODO, endian */
		if (!memcmp(buf+44, "SCRM", 4))
		{
			int chan_opl=0;  /* channels, OPL3 */
			int chan_nopl=0; /* channels, PCM */
			int ins_opl = 0; /* instruments, OPL3 */
			int ins_nopl = 0; /* instruments, PCM */
			uint16_t orders = uint16_little (((uint16_t *)(buf+0x20))[0]);
			uint16_t instruments = uint16_little (((uint16_t *)(buf+0x22))[0]);
			int i;
			for (i=0;i<0x20;i++)
				if (((unsigned char)buf[0x40+i]>=0x10)&&((unsigned char)buf[0x40+i]<0x20))
					chan_opl++;
				else
					if ((unsigned char)buf[0x40+i]!=0xff)
						chan_nopl++;
			for (i=0; (i<instruments) && (len >= 0x60+orders+i*2); i++)
			{
				uint16_t paraptr = uint16_little (((uint16_t *)(buf+0x60+orders))[i]);
				uint32_t offset = paraptr * 16;
				if (len > (offset+1))
				{
					switch (buf[offset])
					{
						case 0: break;
						case 1: ins_nopl++; break;
						case 2:
						case 3:
						case 4:
						case 5:
						case 6:
						case 7: ins_opl++; break;
					}
				}
			}

			if (chan_opl && ins_opl)
				return MODULETYPE("OPL"); /* adlib sample, adplug handles these */
			if (chan_nopl && ins_nopl)
				return MODULETYPE("S3M");
			if (chan_nopl)
				return MODULETYPE("S3M");
			if (chan_opl)
				return MODULETYPE("OPL");
		}
	}
	if (len>=48)
		if (!memcmp(buf+44, "PTMF", 4))
			return MODULETYPE("PTM");

	if (len>=7)
		if (!memcmp(buf, "Extreme", 7))
			return MODULETYPE("AMS");

	if (len>=7)
		if (!memcmp(buf, "AMShdr\x1A", 7))
			return MODULETYPE("AMS");

	if (len>=14)
		if (!memcmp(buf, "MAS_UTrack_V00", 14))
			return MODULETYPE("ULT");

	if (len>=8)
		if (!memcmp(buf, "OKTASONG", 8))
			return MODULETYPE("OKT");

	if (len>=4)
	{
		if (!memcmp(buf, "DMDL", 4))
			return MODULETYPE("MDL");

		if (!memcmp(buf, "MTM", 3))
#warning Files newer than 1.0......?
			return MODULETYPE("MTM");

		if (!memcmp(buf, "DDMF", 4))
			return MODULETYPE("DMF");
	}
	if (len>=2)
		if ((!memcmp(buf, "if" /* 0x6669 */, 2))||(!memcmp(buf, "JN", 2)))
			return MODULETYPE("669");

	return 0;
}


static int gmdReadInfo(struct moduleinfostruct *m, struct ocpfilehandle_t *fp, const char *buf, size_t len, const struct mdbReadInfoAPI_t *API)
{
	uint32_t type;
	int i;

	if (!memcmp(buf, "ziRCONia", 8))
	{
		strcpy(m->title, "MMCMPed module");
		return 0;
	}

	if (!(type=gmdGetModuleType(buf, len)))
		return 0;

	m->modtype.integer.i=type;
	if (type==MODULETYPE("STM"))
	{
		if (len > 0x1f)
		{
			API->cp437_f_to_utf8_z (buf, 20, m->title, sizeof (m->title));
			m->channels = 4;
			if (!memcmp (buf+0x14, "!Scream!", 4))
			{
				if (buf[0x1f] == 21)
				{
					snprintf (m->comment, sizeof (m->comment), "ScreamTracker 2.21 or later");
				} else {
					snprintf (m->comment, sizeof (m->comment), "ScreamTracker 2.%d", (unsigned char)buf[0x1f]);
				}
			} else if (!memcmp (buf+0x14, "BMOD2STM", 4))
			{
				snprintf (m->comment, sizeof (m->comment), "BMOD2STM (STM 2.%d)", (unsigned char)buf[0x1f]);
			} else if (!memcmp (buf+0x14, "WUZAMOD!", 4))
			{
				snprintf (m->comment, sizeof (m->comment), "Wuzamod (STM 2.%d)", (unsigned char)buf[0x1f]);
			}
			return 1;
		}
	} else if ((type==MODULETYPE("OPL")) || (type==MODULETYPE("S3M")))
	{
		if (len>=(64+32))
		{
			uint16_t cwt;

			API->cp437_f_to_utf8_z (buf, 28, m->title, sizeof (m->title));
			m->channels=0;
			for (i=0; i<32; i++)
				if (buf[64+i]!=(char)0xFF)
					m->channels++;

			cwt = uint16_little(*(uint16_t *)(buf + 0x28));
			if ((cwt & 0xff00) == 0x1300)
			{
				snprintf (m->comment, sizeof (m->comment), "ScreamTracker 3.%02x", cwt&0x00ff);
			} else if ((cwt & 0xf000) == 0x2000)
			{
				snprintf (m->comment, sizeof (m->comment), "Imago Orpheus %d.%d", (cwt >> 8) & 0x000f, cwt&0x00ff);
			} else if ((cwt & 0xf000) == 0x3000)
			{
				snprintf (m->comment, sizeof (m->comment), "Impulse Tracker %d.%d", (cwt >> 8) & 0x000f, cwt&0x00ff);
			} else if (cwt == 0x4100)
			{
				snprintf (m->comment, sizeof (m->comment), "old BeRoTracker version from between 2004 and 2012");
			} else if ((cwt & 0xf000) == 0x4000)
			{
				snprintf (m->comment, sizeof (m->comment), "Schism Tracker %d.%d", (cwt >> 8) & 0x000f, cwt&0x00ff);
			} else if ((cwt & 0xf000) == 0x5000)
			{
				snprintf (m->comment, sizeof (m->comment), "OpenMPT %d.%d", (cwt >> 8) & 0x000f, cwt&0x00ff);
			} else if ((cwt & 0xf000) == 0x6000)
			{
				snprintf (m->comment, sizeof (m->comment), "BeRoTracker %d.%d", (cwt >> 8) & 0x000f, cwt&0x00ff);
			} else if ((cwt & 0xf000) == 0x7000)
			{
				snprintf (m->comment, sizeof (m->comment), "CreamTracker %d.%d", (cwt >> 8) & 0x000f, cwt&0x00ff);
			} else if (cwt == 0xca00)
			{
				snprintf (m->comment, sizeof (m->comment), "Camoto/libgamemusic");
			}


			return 1;
		}
	} else if (type==MODULETYPE("MDL"))
	{
		if (len>=(70+32))
		{
			if (buf[4]<0x10)
			{
				m->modtype.integer.i = 0;
				strcpy(m->title, "MDL: too old version");
				return 0;
			}

			snprintf (m->comment, sizeof (m->comment), "DigiTrakker %d.%d", buf[4]>>4, buf[4]&0x0f);

			API->cp437_f_to_utf8_z (buf+11, 32, m->title, sizeof (m->title));
			for (i=strlen(m->title); i>0; i--)
			{
				if (m->title[i-1]==' ')
				{
					m->title[i-1]=0;
				} else {
					break;
				}
			}

			API->cp437_f_to_utf8_z (buf+43, 20, m->composer, sizeof (m->composer));
			for (i=strlen(m->composer); i>0; i--)
			{
				if (m->composer[i-1]==' ')
				{
					m->composer[i-1]=0;
				} else {
					break;
				}
			}


			m->channels=0;
			for (i=0; i<32; i++)
			{
				if (!(buf[i+70]&0x80))
				{
					m->channels++;
				}
			}
			return 1;
		}
	} else if (type==MODULETYPE("PTM"))
	{
		if (len>=39)
		{
			API->cp437_f_to_utf8_z (buf, 28, m->title, sizeof (m->title));
			m->channels=buf[38];
			snprintf (m->comment, sizeof (m->comment), "PolyTracker v%d.%02d", buf[29], buf[30]);
			return 1;
		}
	} else if (type==MODULETYPE("AMS"))
	{
		if (len>=9)
		{
			if (len>=((unsigned char)buf[7])+(unsigned)8)
			{
				if (!memcmp(buf, "AMShdr\x1A", 7))
				{
					API->cp437_f_to_utf8_z (buf + 8, (unsigned char)buf[7], m->title, sizeof (m->title));
					snprintf (m->comment, sizeof (m->comment), "Advanced Module System %d.%02x (Velvet Studio)", buf[7 + 1 + 1 + (unsigned char)buf[7]], buf[7 + 1 + (unsigned char)buf[7]]);
				} else {
					if (len > 18)
					{
						uint16_t extra_len = ((uint8_t)buf[16]) | (((uint8_t)buf[17])<<8);
						int instrument_len = 17*(uint8_t)buf[10];
						if ((len >= (18 + extra_len + instrument_len)) &&
						    (len >= (18 + extra_len + 1 + (uint8_t)buf[18 + extra_len + instrument_len])))
						{
							API->cp437_f_to_utf8_z (buf + 18 + extra_len + instrument_len + 1, (uint8_t)buf[18+extra_len+instrument_len], m->title, sizeof (m->title));
						}
					}
					snprintf (m->comment, sizeof (m->comment), "Advanced Module System %d.%02x (Extreme Tracker)", buf[8], buf[7]);
				}
				return 1;
			}
		}
	} else if (type==MODULETYPE("MTM"))
	{
		if (len>=24)
		{
			API->cp437_f_to_utf8_z (buf + 4, 20, m->title, sizeof (m->title));
			snprintf (m->comment, sizeof (m->comment), "MultiTracker v%d.%d", buf[4]>>4, buf[4]&0x0f);
			m->channels=buf[33];
			return 1;
		}
	} else if (type==MODULETYPE("669"))
	{
		if (len>=(2+32))
		{
			API->cp437_f_to_utf8_z (buf + 2, 32, m->title, sizeof (m->title));
			m->channels=8;
			return 1;
		}
	} else if (type==MODULETYPE("OKT"))
	{
		if (len>=24)
		{
			m->channels=4+(buf[17]&1)+(buf[19]&1)+(buf[21]&1)+(buf[23]&1);
			snprintf (m->comment, sizeof (m->comment), "Oktalyzer tracker");
			return 1;
		}
	} else if (type==MODULETYPE("ULT"))
	{
		if (len>=(15+32))
		{
			API->cp437_f_to_utf8_z (buf + 15, 32, m->title, sizeof (m->title));

			switch (buf[14])
			{
				case '1': strcpy (m->comment, "UltraTracker v1.0"); break;
				case '2': strcpy (m->comment, "UltraTracker v1.4"); break;
				case '3': strcpy (m->comment, "UltraTracker v1.5"); break;
				case '4': strcpy (m->comment, "UltraTracker v1.6"); break;
				default: snprintf (m->comment, sizeof (m->comment), "UltraTracker (file version %d)", buf[14]); break;
			}

			if (len>=(48))
			{
				uint8_t t1, t2;
				if ((fp->seek_set (fp, 48 + buf[47] * 32) == 0) &&
				    (fp->read (fp, &t1, 1) == 1) &&
				    (fp->seek_set (fp, 256 + t1 * ( (buf[14]>='4') ? 66:64)) == 0) &&
				    (fp->read (fp, &t2, 1) == 1))
				{
					m->channels = t2 + 1;
					fp->seek_set (fp, 0);
					return 1;
				}
				fp->seek_set (fp, 0);
			}

			return 0;
		}
	} else if (type==MODULETYPE("DMF"))
	{
		if (len>=(43+20))
		{
			API->cp437_f_to_utf8_z (buf + 13, 30, m->title, sizeof (m->title));
			API->cp437_f_to_utf8_z (buf + 43, 20, m->composer, sizeof (m->composer));
			m->date=uint32_little(*(uint32_t *)(buf+63))&0xFFFFFF;
			switch (buf[4])
			{
				case 4: snprintf (m->comment, sizeof (m->comment), "XTracker 0.30beta"); break;
				default: snprintf (m->comment, sizeof (m->comment), "XTracker (file version %d)", buf[4]); break;
			}

			if (fp->seek_set (fp, 66) == 0)
			{
				m->channels=32;
				while (1)
				{
					uint32_t sig=0;
					uint32_t len=0;
					if (ocpfilehandle_read_uint32_le (fp, &sig))
					{
						break;
					}
					if (ocpfilehandle_read_uint32_le (fp, &len))
					{
						break;
					}
					if (sig == 0x54544150)
					{
						m->channels = 0;
						if (fp->seek_cur (fp, 1024) == 0)
						{
							uint8_t t;
							if (fp->read (fp, &t, 1) == 1)
							{
								m->channels = t;
							}
						}
						break;
					}
					if (fp->seek_cur (fp, len) < 0)
					{
						break;
					}
				}
				fp->seek_set (fp, 0);
				return 1;
			}

			return 0;
		}
	}
	/* if we reach this point, the file is broken in length... */
	return 0;
}

static const char *_669_description[] =
{
	//                                                                          |
	"669 files are created by Composer 669 by Renaissance (and UNIS669 Composer).",
	"This is tracker made for MS-DOS, and has a fixed 8 channel design. Open",
	"Cubic Player convers these internally into a generic module.",
	NULL
};

static const char *AMS_description[] =
{
	//                                                                          |
	"AMS - Advanced Module System - files are created by Extreme Tracker and its",
	"successor Velvet Studio by Velvet Development. Open Cubic Player converts",
	"these internally into a generic module with some quirks in the playback.", // quirk: MOD_EXPOFREQ MOD_EXPOPITCHENV
	NULL
};

static const char *DMF_description[] =
{
	//                                                                          |
	"DMF - Delusion/XTracker Digital Music File - files are created by X-Tracker",
	"by D-Lusion. Files can be up to 32 channels. Open Cubic Player convers these",
	"internally into a generic module with some quirks in the playback.", // quirk: MOD_TICK0 MOD_EXPOFREQ
	NULL
};

static const char *MDL_description[] =
{
	//                                                                          |
	"MDL files are created by DigiTrakker by Prodatron. It is a MSDOS based",
	"tracker with support of up to 32 channels. Open Cubic Player convers these",
	"internally into a generic module with some quirks in the playback.", // quirk: MOD_EXPOFREQ MP_OFFSETDIV2
	NULL
};

static const char *MTM_description[] =
{
	//                                                                          |
	"MTM files are created by Multi Tracker by DigiTrakker by Prodatron. It is a",
	"MSDOS based tracker, and was the first(?) to support 32 channels. Open Cubic",
	"Player convers these internally into a generic module.",
	NULL
};

static const char *OKT_description[] =
{
	//                                                                          |
	"OKT files are created by Oktalyzer by Armin Sander. This is an 8 channel",
	"tracker for Amiga, which is rare. It uses combination of software rendering",
	"and hardware rendering to achieve more than the usual 4 channels. Open Cubic",
	"Player converts these internally into a generic module with some quirks in",
	"the playback.", // quirk: MOD_TICK0
	NULL
};

/* http://fileformats.archiveteam.org/wiki/Poly_Tracker_module
 * .ptm files are modules produced by Poly Tracker. As Poly Tracker was intended
 * by the creator (Lone Ranger of AcmE) to be a better version of ScreamTracker,
 * the PTM format shares many similarities with the S3M format used by
 * ScreamTracker.
 *
 * There have been around a dozen versions of the PTM format, including
 * customized test versions.
 */
static const char *PTM_description[] =
{
	//                                                                          |
	"PTM files are created by PolyTracker by Lone Ranger of AcmE. This tracker",
	"was never released to the public, so only some few composers have used it",
	"so there are not much music released in this format. Open Cubic Player",
	"converts these internally into a generic module with some quirks in the",
	"playback.", // quirk: MOD_S3M
	NULL
};

static const char *STM_description[] =
{
	//                                                                          |
	"STM files are created by Scream Tracker II by Future Crew (or by BMOD2STM",
	"or WUZAMOD file converter). Scream Tracker II and 3 were among the earliest",
	"implementation of amiga style module editing and playback implementations",
	"on MSDOS/PC. Open Cubic Player converts these internally into a generic",
	"module with some quirks in the playback. The Speed and Tempo commands are",
	"estimated using speed/tempo and fine-speed/tempo commands.", // quirk: MOD_S3M
	NULL
};

static const char *S3M_description[] =
{
	//                                                                          |
	"S3M files are created by Scream Tracker 3 by Future Crew. Files support",
	"more channels and samples than a typical amiga module that was limited to",
	"4 channels and 31 samples. S3M files can also contain OPL2 samples; if so",
	"use the OPL file format instead (playback using adplug). Open Cubic Player",
	"converts these internally into a generic module with some quirks in the",
	"playback.", // quirk: MOD_S3M
	NULL
};

static const char *ULT_description[] =
{
	//                                                                          |
	"ULT files are created by UltraTracker by MAS. MS-DOS tracker made for use",
	"together with the Gravis Ultrasound Soundcard. It features up to 32 channels",
	"and 256K of sample data - these restrictions matches the Soundcard the",
	"software was designed for. Open Cubic Player converts these internally into",
	"a generic module with some quirks in the playback.", // quirk: MOD_GUSVOL
	NULL
};

static struct mdbreadinforegstruct gmdReadInfoReg = {"MOD", gmdReadInfo MDBREADINFOREGSTRUCT_TAIL};

int __attribute__ ((visibility ("internal"))) gmd_type_init (struct PluginInitAPI_t *API)
{
	struct moduletype mt;

	API->fsRegisterExt ("669");
	mt.integer.i = MODULETYPE("669");
	API->fsTypeRegister (mt, _669_description, "plOpenCP", &gmdPlayer669);

	API->fsRegisterExt ("AMS");
	mt.integer.i = MODULETYPE("AMS");
	API->fsTypeRegister (mt, AMS_description, "plOpenCP", &gmdPlayerAMS);

	API->fsRegisterExt ("DMF");
	mt.integer.i = MODULETYPE("DMF");
	API->fsTypeRegister (mt, DMF_description, "plOpenCP", &gmdPlayerDMF);

	API->fsRegisterExt ("MDL");
	mt.integer.i = MODULETYPE("MDL");
	API->fsTypeRegister (mt, MDL_description, "plOpenCP", &gmdPlayerMDL);

	API->fsRegisterExt ("MTM");
	mt.integer.i = MODULETYPE("MTM");
	API->fsTypeRegister (mt, MTM_description, "plOpenCP", &gmdPlayerMTM);

	API->fsRegisterExt ("OKT");
	API->fsRegisterExt ("OKTA");
	mt.integer.i = MODULETYPE("OKT");
	API->fsTypeRegister (mt, OKT_description, "plOpenCP", &gmdPlayerOKT);

	API->fsRegisterExt ("PTM");
	mt.integer.i = MODULETYPE("PTM");
	API->fsTypeRegister (mt, PTM_description, "plOpenCP", &gmdPlayerPTM);

	API->fsRegisterExt ("S3M");
	mt.integer.i = MODULETYPE("S3M");
	API->fsTypeRegister (mt, S3M_description, "plOpenCP", &gmdPlayerS3M);

	API->fsRegisterExt ("STM");
	mt.integer.i = MODULETYPE("STM");
	API->fsTypeRegister (mt, STM_description, "plOpenCP", &gmdPlayerSTM);

	API->fsRegisterExt ("ULT");
	mt.integer.i = MODULETYPE("ULT");
	API->fsTypeRegister (mt, ULT_description, "plOpenCP", &gmdPlayerULT);

	API->mdbRegisterReadInfo(&gmdReadInfoReg);

	return errOk;
}

void __attribute__ ((visibility ("internal"))) gmd_type_done (struct PluginCloseAPI_t *API)
{
	struct moduletype mt;

	mt.integer.i = MODULETYPE("669");
	API->fsTypeUnregister (mt);

	mt.integer.i = MODULETYPE("AMS");
	API->fsTypeUnregister (mt);

	mt.integer.i = MODULETYPE("DMF");
	API->fsTypeUnregister (mt);

	mt.integer.i = MODULETYPE("MDL");
	API->fsTypeUnregister (mt);

	mt.integer.i = MODULETYPE("MTM");
	API->fsTypeUnregister (mt);

	mt.integer.i = MODULETYPE("OKT");
	API->fsTypeUnregister (mt);

	mt.integer.i = MODULETYPE("PTM");
	API->fsTypeUnregister (mt);

	mt.integer.i = MODULETYPE("S3M");
	API->fsTypeUnregister (mt);

	mt.integer.i = MODULETYPE("STM");
	API->fsTypeUnregister (mt);

	mt.integer.i = MODULETYPE("ULT");
	API->fsTypeUnregister (mt);

	API->mdbUnregisterReadInfo(&gmdReadInfoReg);
}
