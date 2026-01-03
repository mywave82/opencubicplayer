/* OpenCP Module Player
 * copyright (c) 2019-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Game Music Emulator file type detection routines for the fileselector
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
#include "stuff/cp437.h"
#include "stuff/err.h"
#include "gmetype.h"

#ifndef MIN
# define MIN(a,b) (((a)<(b))?(a):(b))
#endif

static int gmeReadInfoGBS (struct moduleinfostruct *m, const char *buf, size_t len, const struct mdbReadInfoAPI_t *API)
{
	if (len > 112)
	{
		struct __attribute__((packed)) header_t
		{
			char    tag[3];
			uint8_t vers;
			uint8_t track_count;
			uint8_t first_track;
			uint8_t load_addr[2];
			uint8_t init_addr[2];
			uint8_t play_addr[2];
			uint8_t stack_ptr[2];
			uint8_t timer_modulo;
			uint8_t timer_mode;
			char    game[32];
			char    author[32];
			char    copyright[32];
		};
		const struct header_t *h = (const struct header_t *)buf;

		API->cp437_f_to_utf8_z (h->game, sizeof (h->game), m->title, sizeof (m->title));
		API->cp437_f_to_utf8_z (h->author, sizeof (h->author), m->composer, sizeof (m->composer));
		API->cp437_f_to_utf8_z (h->copyright, sizeof (h->copyright), m->comment, sizeof (m->comment));

		m->modtype.integer.i = MODULETYPE("GBS");
		return 1;
	}
	return 0;
}

static int gmeReadInfoGYM (struct moduleinfostruct *m, const char *buf, size_t len, const struct mdbReadInfoAPI_t *API)
{
	if (len > 428)
	{
		struct __attribute__((packed)) header_t
		{
			char    tag[4];
			char    song[32];
			char    game[32];
			char    copyright[32];
			char    emulator[32];
			char    dumper[32];
			char    comment[256];
			uint8_t loop_start[4]; // in 1/60 seconds, 0 if not looped
			uint8_t packed[4];
		};
		const struct header_t *h = (const struct header_t *)buf;

		API->cp437_f_to_utf8_z (h->game, sizeof (h->game), m->title, sizeof (m->title));
		if (strlen (m->title) < (sizeof (m->title) - 4))
		{
			strcat (m->title, " / ");
			API->cp437_f_to_utf8_z (h->song, sizeof (h->song), m->title + strlen (m->title), sizeof (m->title) - strlen (m->title));
		}
		API->cp437_f_to_utf8_z (h->comment, sizeof (h->comment), m->comment, sizeof (m->comment));

		m->modtype.integer.i = MODULETYPE("GYM");
		return 1;
	}

	return 0;
}

static const uint8_t *hes_field (const uint8_t *in, const char **out, int *out_len)
{
	if ( in )
	{
		int len = 0x20;
		int i;
		if ( in [0x1F] && !in [0x2F] )
			len = 0x30; // fields are sometimes 16 bytes longer (ugh)

		// since text fields are where any data could be, detect non-text
		// and fields with data after zero byte terminator

		for ( i = 0; i < len && in [i]; i++ )
			if ( ((in [i] + 1) & 0xFF) < ' ' + 1 ) // also treat 0xFF as non-text
				return 0; // non-ASCII found

		for ( ; i < len; i++ )
			if ( in [i] )
				return 0; // data after terminator

		*out = (const char *)in;
		*out_len = len;
		in += len;
	}
	return in;
}

static int gmeReadInfoHES (struct moduleinfostruct *m, const char *buf, size_t len, const struct mdbReadInfoAPI_t *API)
{
	if (len > (64 + 48 * 3))
	{
		struct __attribute__((packed)) header_t
		{
			uint8_t tag[4];
			uint8_t vers;
			uint8_t first_track;
			uint8_t init_addr[2];
			uint8_t banks[8];
			uint8_t data_tag[4];
			uint8_t size[4];
			uint8_t addr[4];
			uint8_t unused1[4];
			uint8_t unused2[32];
			uint8_t fields[48*3];
		};
		const struct header_t *h = (const struct header_t *)buf;
		const uint8_t *in = h->fields;
		const char *game = 0;
		int game_len = 0;
		const char *author = 0;
		int author_len = 0;
		const char *copyright = 0;
		int copyright_len = 0;

		if (*in >= ' ')
		{
			in = hes_field (in, &game, &game_len);
			in = hes_field (in, &author, &author_len);
			in = hes_field (in, &copyright, &copyright_len);
			if (game)
			{
				API->cp437_f_to_utf8_z (game, game_len, m->title, sizeof (m->title));
			}
			if (author)
			{
				API->cp437_f_to_utf8_z (author, author_len, m->composer, sizeof (m->composer));
			}
			if (copyright)
			{
				API->cp437_f_to_utf8_z (copyright, copyright_len, m->comment, sizeof (m->comment));
			}
		}

		m->modtype.integer.i = MODULETYPE("HES");
		return 1;
	}
	return 0;
}

static int gmeReadInfoKSS (struct moduleinfostruct *m, const char *buf, size_t len, const struct mdbReadInfoAPI_t *API)
{
	if (len > 16)
	{
		struct __attribute__((packed)) header_t
		{
			uint8_t tag [4];
			uint8_t load_addr [2];
			uint8_t load_size [2];
			uint8_t init_addr [2];
			uint8_t play_addr [2];
			uint8_t first_bank;
			uint8_t bank_mode;
			uint8_t extra_header;
			uint8_t device_flags;
		};
		const struct header_t *h = (const struct header_t *)buf;

		if (h->device_flags & 0x02)
		{
			snprintf (m->comment, sizeof (m->comment), "%s", "Game Gear");
		} else {
			snprintf (m->comment, sizeof (m->comment), "%s", "Sega Master System");
		}

		m->modtype.integer.i = MODULETYPE("KSS");
		return 1;
	}
	return 0;
}

static int gmeReadInfoNSF (struct moduleinfostruct *m, const char *buf, size_t len, const struct mdbReadInfoAPI_t *API)
{
	if (len > 128)
	{
		struct __attribute__((packed)) header_t
		{
			char    tag[5];
			uint8_t vers;
			uint8_t track_count;
			uint8_t first_track;
			uint8_t load_addr[2];
			uint8_t init_addr[2];
			uint8_t play_addr[2];
			char    game[32];
			char    author[32];
			char    copyright[32];
			uint8_t ntsc_speed[2];
			uint8_t banks[8];
			uint8_t pal_speed[2];
			uint8_t speed_flags;
			uint8_t chip_flags;
			uint8_t unused[4];
		};
		const struct header_t *h = (const struct header_t *)buf;

		API->cp437_f_to_utf8_z (h->game, sizeof (h->game), m->title, sizeof (m->title));
		API->cp437_f_to_utf8_z (h->author, sizeof (h->author), m->composer, sizeof (m->composer));
		API->cp437_f_to_utf8_z (h->copyright, sizeof (h->copyright), m->comment, sizeof (m->comment));

		m->modtype.integer.i = MODULETYPE("NSF");
		return 1;
	}
	return 0;
}

static int gmeReadInfoNSFe (struct moduleinfostruct *m, struct ocpfilehandle_t *fp, const char *buf, size_t len, const struct mdbReadInfoAPI_t *API)
{
	char *freeme = 0;
	const char *mydata = buf;
	uint64_t mylen = len;
	uint64_t fs = fp->filesize (fp);

	if (fs > 1024*1024)
	{
		return 0;
	}

	if (len != fs)
	{
		freeme = malloc (fs);
		if (!freeme)
		{
			return 0;
		}
		mydata = freeme;
		mylen = fp->filesize (fp);
		fp->seek_set (fp, 0);
		fp->read(fp, freeme, mylen);
		fp->seek_set (fp, 0);
	}
	mydata += 4;
	mylen -= 4;

	while (mylen >= 8)
	{
		uint32_t chunklen =  ((uint_fast32_t)(mydata[0]))       |
		                    (((uint_fast32_t)(mydata[1])) <<  8) |
		                    (((uint_fast32_t)(mydata[1])) << 16) |
		                    (((uint_fast32_t)(mydata[1])) << 24);
		if (chunklen >= mylen)
		{
			break;
		}
		if ((chunklen + 8) >= mylen)
		{
			break;
		}
		mydata += 8;
		mylen -= 8;
		mydata += chunklen;
		mylen -= chunklen;
		if (!memcmp (mydata + 4, "auth", 4))
		{
			const char *game = mydata + 8;
			const char *author = 0;
			const char *copyright = 0;
			//const char *ripper = 0;
			int a, z;
			for (a = 0, z = 0; a < chunklen; a++)
			{
				if (!game[a])
				{
					z++;
				}
			}
			if (z < 4)
			{
				break;
			}
			author = game + strlen (game);
			copyright = author + strlen (author);
			//ripper = copyright + strlen (copyright);  /* not used */

			API->cp437_f_to_utf8_z (game, strlen (game), m->title, sizeof (m->title));
			API->cp437_f_to_utf8_z (author, strlen (author), m->composer, sizeof (m->composer));
			API->cp437_f_to_utf8_z (copyright, strlen (copyright), m->comment, sizeof (m->comment));
			break;
		}
	}
	free (freeme);

	m->modtype.integer.i = MODULETYPE("NFSe");
	return  1;
}

static void sap_field (const uint8_t *in, const uint8_t *end, const char **out, int *out_len)
{
	const uint8_t *start = in;
	if ( *in++ == '\"' )
	{
		start++;
		while ( in < end && *in != '\"' )
		{
			in++;
		}
	} else {
		in = end;
	}
	*out_len = in - start;
	*out = (const char *)start;
}

static int gmeReadInfoSAP (struct moduleinfostruct *m, const char *buf, size_t len, const struct mdbReadInfoAPI_t *API)
{
	if (len >= 16)
	{
		const uint8_t *in = (const uint8_t *)buf + 5;
		const uint8_t *file_end = (const uint8_t *)in + len - 5;
		while ( in < file_end && (in [0] != 0xff || in [1] != 0xff) )
		{
			const uint8_t *line_end = in;
			const char *tag = (const char *)in;
			int tag_len;
			while ( line_end < file_end && *line_end != 0x0d )
			{
				line_end++;
			}

			while ( in < line_end && *in > ' ' )
			{
				in++;
			}
			tag_len = (const char*) in - tag;

			while ( in < line_end && *in <= ' ' )
			{
				in++;
			}

			if ( tag_len <= 0 )
			{
				// skip line
			} else if ( !strncmp( "INIT", tag, tag_len ) )
			{
				//init_addr = from_hex( in );
				//if ( (unsigned long) init_addr > 0xFFFF )
				//	return "Invalid init address";
			} else if ( !strncmp( "PLAYER", tag, tag_len ) )
			{
				//play_addr = from_hex( in );
				//if ( (unsigned long) play_addr > 0xFFFF )
				//	return "Invalid play address";
			} else if ( !strncmp( "MUSIC", tag, tag_len ) )
			{
				//music_addr = from_hex( in );
				//if ( (unsigned long) out->music_addr > 0xFFFF )
				//	return "Invalid music address";
			} else if ( !strncmp( "SONGS", tag, tag_len ) )
			{
				//track_count = from_dec( in, line_end );
				//if ( out->track_count <= 0 )
				//	return "Invalid track count";
			} else if ( !strncmp( "TYPE", tag, tag_len ) )
			{
				//switch ( out->type = *in )
				//{
				//	case 'C':
				//	case 'B':
				//		break;
				//	case 'D':
				//		return "Digimusic not supported";
				//	default:
				//		return "Unsupported player type";
				//}
			} else if ( !strncmp( "STEREO", tag, tag_len ) )
			{
				// stereo = true;
			} else if ( !strncmp( "FASTPLAY", tag, tag_len ) )
			{
				//fastplay = from_dec( in, line_end );
				//if ( fastplay <= 0 )
				//	return "Invalid fastplay value";
			} else if ( !strncmp( "AUTHOR", tag, tag_len ) )
			{
				const char *t; int l;
				sap_field (in, line_end, &t, &l);
				API->cp437_f_to_utf8_z (t, l, m->composer, sizeof (m->composer));
			} else if ( !strncmp( "NAME", tag, tag_len ) )
			{
				const char *t; int l;
				sap_field (in, line_end, &t, &l);
				API->cp437_f_to_utf8_z (t, l, m->title, sizeof (m->title));
			} else if ( !strncmp( "DATE", tag, tag_len ) )
			{
				//const char *t; int l;
				//sap_field (in, line_end, &t, &l);
				//
			}

			in = line_end + 2;
		}

		m->modtype.integer.i = MODULETYPE("SAP");
		return 1;
	}

	return 0;
}

static int gmeReadInfoSPC (struct moduleinfostruct *m, struct ocpfilehandle_t *fp, const char *buf, size_t len, const struct mdbReadInfoAPI_t *API)
{
	#warning TODO extended file format, data at the end...
	if (len > 256)
	{
		struct __attribute__((packed)) header_t
		{
			char    tag[35];
			uint8_t format;
			uint8_t version;
			uint8_t pc[2];
			uint8_t a, x, y, psw, sp;
			uint8_t unused[2];
			char    song[32];
			char    game[32];
			char    dumper[16];
			char    comment[32];
			uint8_t date[11];
			uint8_t len_secs[3];
			uint8_t fade_msec[4];
			char    author[32]; // sometimes first char should be skipped (see official SPC spec)
			uint8_t mute_mask;
			uint8_t emulator;
			uint8_t unused2[46];
		};
		const struct header_t *h = (const struct header_t *)buf;

		API->cp437_f_to_utf8_z (h->game, sizeof (h->game), m->title, sizeof (m->title));
		if (strlen (m->title) < (sizeof (m->title) - 4))
		{
			strcat (m->title, " / ");
			API->cp437_f_to_utf8_z (h->song, sizeof (h->song), m->title + strlen (m->title), sizeof (m->title) - strlen (m->title));
		}

		{
			int offset = (h->author [0] < ' ' || (unsigned int)(h->author [0] - '0') <= 9);
			API->cp437_f_to_utf8_z (h->author + offset, sizeof (h->author) - offset, m->composer, sizeof (m->composer));
		}
		API->cp437_f_to_utf8_z (h->comment, sizeof (h->comment), m->comment, sizeof (m->comment));

		m->modtype.integer.i = MODULETYPE("SPC");
		return 1;
	}
	return 0;
}


static const uint8_t *skip_gd3_str (const uint8_t *in, const uint8_t *end )
{
	while ( end - in >= 2 )
	{
		in += 2;
		if ( !(in [-2] | in [-1]) )
		{
			break;
		}
	}
	return in;
}

static const uint8_t *get_gd3_str (const uint8_t *in, const uint8_t *end, char *field )
{
	const uint8_t *next = skip_gd3_str( in, end );
	int len = (next - in) / 2 - 1;
	if ( len > 0 )
	{
		int i;

		len = MIN (len, 256-1);

		field[len] = 0;

		for (i = 0; i < len; i++ )
		{
			field [i] = (in [i * 2 + 1] ? '?' : in [i * 2]);
		}
	} else {
		field[0] = 0;
	}
	return next;
}

static void parse_gd3 (struct moduleinfostruct *m, const uint8_t *in, const uint8_t *end, const struct mdbReadInfoAPI_t *API)
{
	char temp1[256];
	char temp2[256];

	in = get_gd3_str  (in, end, temp1);  // song english
	in = skip_gd3_str (in, end);        // (song japanese)

	in = get_gd3_str  (in, end, temp2);  // game english
	API->cp437_f_to_utf8_z (temp2, strlen (temp2), m->title, sizeof (m->title));
	if (strlen (m->title) < (sizeof (m->title) - 4))
	{
		strcat (m->title, " / ");
		API->cp437_f_to_utf8_z (temp1, strlen (temp1), m->title + strlen (m->title), sizeof (m->title) - strlen (m->title));
	}
	in = skip_gd3_str (in, end);        // (game japanese)

	in = get_gd3_str  (in, end, temp1);  // system english
	API->cp437_f_to_utf8_z (temp1, strlen (temp1), m->comment, sizeof (m->comment));
	in = skip_gd3_str (in, end);        // (system japanese)

	in = get_gd3_str  (in, end, temp1);  // author english
	API->cp437_f_to_utf8_z (temp1, strlen (temp1), m->composer, sizeof (m->composer));
	in = skip_gd3_str (in, end);        // (author japanese)

	in = get_gd3_str  (in, end, temp1);  // date
	if (isdigit(temp1[0]) && isdigit(temp1[1]) && isdigit(temp1[2]) && isdigit(temp1[3]))
	{
		m->date = ((temp1[0] - '0') * 1000 + (temp1[1] - '0') * 100 + (temp1[2] - '0') * 10 + (temp1[3] - '0')) << 16;
		if (temp1[4] == '/' && isdigit(temp1[5]) && isdigit(temp1[6]))
		{
			m->date |= ((temp1[5] - '0') * 10 + (temp1[6] - '0')) << 8;
			if (temp1[7] == '/' && isdigit(temp1[8]) && isdigit(temp1[9]))
			{
				m->date |= ((temp1[8] - '0') * 10 + (temp1[9] - '0'));
			}
		}
	}

	in = get_gd3_str  (in, end, temp1);  // dumper

	in = get_gd3_str  (in, end, temp1);  // notes
}

static int gmeReadInfoVGM (struct moduleinfostruct *m, struct ocpfilehandle_t *fp, const char *buf, size_t len, const struct mdbReadInfoAPI_t *API)
{
	if (len > 64)
	{
		struct __attribute__((packed)) header_t
		{
			char    tag[4];
			uint8_t data_size[4];
			uint8_t version[4];
			uint8_t psg_rate[4];
			uint8_t ym2413_rate[4];
			uint8_t gd3_offset[4];
			uint8_t track_duration[4];
			uint8_t loop_offset[4];
			uint8_t loop_duration[4];
			uint8_t frame_rate[4];
			uint8_t noise_feedback[2];
			uint8_t noise_width;
			uint8_t unused1;
			uint8_t ym2612_rate[4];
			uint8_t ym2151_rate[4];
			uint8_t data_offset[4];
			uint8_t unused2[8];
		};
		const struct header_t *h = (const struct header_t *)buf;
		uint32_t gd3_pos =  ((uint_fast32_t)(h->gd3_offset[0]))        |
				   (((uint_fast32_t)(h->gd3_offset[1])) << 8 ) |
				   (((uint_fast32_t)(h->gd3_offset[2])) << 16) |
				   (((uint_fast32_t)(h->gd3_offset[3])) << 24);

		gd3_pos -= 0x14;
		if (gd3_pos > 0x40)
		{
			struct __attribute__((packed)) gd3_header_t
			{
				char    tag[4];
				uint8_t version[4];
				uint8_t size[4];
			};
			struct gd3_header_t gd3_header;
			fp->seek_set (fp, gd3_pos);
			if (fp->read (fp, &gd3_header, sizeof (gd3_header)) == sizeof (gd3_header))
			{
				uint32_t version =  ((uint_fast32_t)(gd3_header.version[0]))        |
						   (((uint_fast32_t)(gd3_header.version[1])) <<  8) |
						   (((uint_fast32_t)(gd3_header.version[2])) << 16) |
						   (((uint_fast32_t)(gd3_header.version[3])) << 24);
				uint32_t size =  ((uint_fast32_t)(gd3_header.size[0]))        |
						(((uint_fast32_t)(gd3_header.size[1])) <<  8) |
						(((uint_fast32_t)(gd3_header.size[2])) << 16) |
						(((uint_fast32_t)(gd3_header.size[3])) << 24);
				if ((version < 0x200) && (!memcmp (gd3_header.tag, "Gd3 ", 4)) && (size < 65536) && size)
				{
					uint8_t *gd3 = malloc (size);
					if (gd3)
					{
						if (fp->read (fp, gd3, size) == size)
						{
							parse_gd3 (m, gd3, gd3 + size, API);
						}
					}
					free (gd3);
				}
			}
			fp->seek_set (fp, 0);
		}
		if (h->psg_rate[0]    || h->psg_rate[1]    || h->psg_rate[2]    || h->psg_rate[3]    ||
		    h->ym2413_rate[0] || h->ym2413_rate[1] || h->ym2413_rate[2] || h->ym2413_rate[3] ||
		    h->ym2612_rate[0] || h->ym2612_rate[1] || h->ym2612_rate[2] || h->ym2612_rate[3] ||
		    h->ym2151_rate[0] || h->ym2151_rate[1] || h->ym2151_rate[2] || h->ym2151_rate[3])
		{
			m->modtype.integer.i = MODULETYPE("VGM");
			return 1;
		}
		if ((len > 256) && (
		    buf[0x50] || buf[0x51] || buf[0x52] || buf[0x53] || // OPL2
		    buf[0x5c] || buf[0x5d] || buf[0x5e] || buf[0x5f])) // OPL3
		{
			m->modtype.integer.i = MODULETYPE("OPL");
			return 1;
		}
	}
	return 0;
}

static int gmeReadInfo (struct moduleinfostruct *m, struct ocpfilehandle_t *fp, const char *buf, size_t len, const struct mdbReadInfoAPI_t *API)
{
	// we skip AY detection, since that is taken care of by the playay plugin

	if (len < 5)
	{
		return 0;
	}

	if (!memcmp (buf, "GBS", 3))
	{
		return gmeReadInfoGBS (m, buf, len, API);
	}
	if (!memcmp (buf, "GYMX", 4))
	{
		return gmeReadInfoGYM (m, buf, len, API);
	}
	if (!memcmp (buf, "HESM", 4))
	{
		return gmeReadInfoHES (m, buf, len, API);
	}
	if ((!memcmp (buf, "KSCC", 4)) || (!memcmp (buf, "KSSX", 4)))
	{
		return gmeReadInfoKSS (m, buf, len, API);
	}
	if (!memcmp (buf, "NESM\x1a", 5))
	{
		return gmeReadInfoNSF (m, buf, len, API);
	}
	if (!memcmp (buf, "NFSE", 4))
	{
		return gmeReadInfoNSFe (m, fp, buf, len, API);
	}
	if (!memcmp (buf, "SAP\x0d\x0a", 5))
	{
		return gmeReadInfoSAP (m, buf, len, API);
	}
	if ((len >= 27) && !memcmp (buf, "SNES-SPC700 Sound File Data", 27))
	{
		return gmeReadInfoSPC (m, fp, buf, len, API);
	}
	if (!memcmp (buf, "Vgm ", 4))
	{
		return gmeReadInfoVGM (m, fp, buf, len, API);

	}

	return 0;
}

static const char *AY2_description[] =
{
	//                                                                          |
	"AY files are executable code that runs a virtual Z80 machine with a virtual",
	"AY-3-8910 sound IC. This IC a 3 channel programmable sound generator (PSG)",
	"that can generate sawtooth and pulse-wave (square) sounds. Playback using",
	"Blargg's video music game music emulator.",
	NULL
};

static const char *GBS_description[] =
{
	//                                                                          |
	"GBS (GameBoy Sound System) files are executable code for Nintendo Game Boy",
	"that runs on a virtual modified Z80 CPU with an integrated audio chip that",
	"features 2 pulse-, 1 saw-, 1 noise generator, 1 DPCM 4-bit, and 1 input",
	"from the cartridge. Playback using Blargg's video music game music emulator.",
	NULL
};

static const char *GYM_description[] =
{
	//                                                                          |
	"GYM (Genesis YM2612) files are register dumps from Sega Genesis/Mega Drive.",
	"It features a Texas Instruments SN76489 that has 3 square wave generators",
	"and 1 noise generator and a YM2612 aka OPN2 audio IC which features 6 FM",
	" channels. Playback using Blargg's video music game music emulator",
	NULL
};

static const char *HES_description[] =
{
	//                                                                          |
	"HES (Hudson Entertainment Sound) files are executable code for NEC",
	"TurboGrafx-16/PC Engine that runs on a modified 6502 CPU with an integrated",
	"audio chip. This IC features a 6 channel PSG that can run in DAC mode.",
	"Playback using Blargg's video music game music emulator",
	NULL
};

static const char *KSS_description[] =
{
	//                                                                          |
	"KSS (Konami Sound System?) files are executable code for MSX Home Computer/",
	"other Z80 systems that runs on Z80 CPU. Playback using Blargg's video music",
	"game music emulator",
	NULL
};

static const char *NSF_description[] =
{
	// (Nintendo Sound Format)
	//                                                                          |
	"NSF files are executable code for Nintendo NES that runs on a virtual",
	"modified Z80 CPU with an integrated custom audio chip. This IC features 2",
	"pulse-, 1 saw-, 1 noise generator, 1 DPCM 4-bit, and 1 input from the game",
	"cartridge. Playback using Blargg's video music game music emulator",
	NULL
};

#warning library is missing NSF2 https://www.nesdev.org/wiki/NSF2

static const char *NSFe_description[] =
{
	//                                                                          |
	"NSFe (Nintendo Sound Format extended) files are NSF files with extra meta",
	"data. Playback using Blargg's video music game music emulator",
	NULL
};

static const char *SAP_description[] =
{
	//                                                                          |
	"SAP (Slight Atari Player) files are executable code for Atari systems using",
	"POKEY sound chip that runs on a virtual 6502 CPU. POKEY is a 4 channel",
	"square wave generator that also could generate noise. Playback using",
	"Blargg's video music game music emulator",
	NULL
};

static const char *SPC_description[] =
{
	// SPC700, name of the audio co-processor
	//                                                                          |
	"SPC files are executable code for Super Nintendo/Super Famicom that runs on",
	"virtual on a virtual Ricoh 5A22 CPU based on WDC 65C816 and a custom audio",
	"IC that has an internal dedicated CPU and features 8 DSP channels. Playback",
	"using Blargg's video music game music emulator",
	NULL
};

static const char *VGM_description[] =
{
	//                                                                          |
	"VGM (Video Game Music) files are generic register dumps from various",
	"systems. Playback using using Blargg's video music game music emulator",
	"which is known to support music dumps from Sega Master System/Mark III,",
	"Sega Genesis/Mega Drive and BBC Micro.",
	NULL
};

static struct mdbreadinforegstruct gmeReadInfoReg = {"GME", gmeReadInfo MDBREADINFOREGSTRUCT_TAIL};

OCP_INTERNAL int gme_type_init (struct PluginInitAPI_t *API)
{
	struct moduletype mt;

	API->fsRegisterExt ("AY");
	mt.integer.i = MODULETYPE("AY2");
	API->fsTypeRegister (mt, AY2_description, "plOpenCP", &gmePlayer);

	API->fsRegisterExt ("GBS");
	mt.integer.i = MODULETYPE("GBS");
	API->fsTypeRegister (mt, GBS_description, "plOpenCP", &gmePlayer);

	API->fsRegisterExt ("GYM");
	mt.integer.i = MODULETYPE("GYM");
	API->fsTypeRegister (mt, GYM_description, "plOpenCP", &gmePlayer);

	API->fsRegisterExt ("HES");
	mt.integer.i = MODULETYPE("HES");
	API->fsTypeRegister (mt, HES_description, "plOpenCP", &gmePlayer);

	API->fsRegisterExt ("KSS");
	mt.integer.i = MODULETYPE("KSS"); /* GME uses MSX as internal name */
	API->fsTypeRegister (mt, KSS_description, "plOpenCP", &gmePlayer);

	API->fsRegisterExt ("NSF");
	mt.integer.i = MODULETYPE("NSF");
	API->fsTypeRegister (mt, NSF_description, "plOpenCP", &gmePlayer);

	API->fsRegisterExt ("NSFE");
	mt.integer.i = MODULETYPE("NSFe");
	API->fsTypeRegister (mt, NSFe_description, "plOpenCP", &gmePlayer);

	API->fsRegisterExt ("SAP");
	mt.integer.i = MODULETYPE("SAP"); /* GME uses Atari XL as internal name */
	API->fsTypeRegister (mt, SAP_description, "plOpenCP", &gmePlayer);

	API->fsRegisterExt ("SPC");
	mt.integer.i = MODULETYPE("SPC");
	API->fsTypeRegister (mt, SPC_description, "plOpenCP", &gmePlayer);

	API->fsRegisterExt ("VGM");
	API->fsRegisterExt ("VGZ");
	mt.integer.i = MODULETYPE("VGM");
	API->fsTypeRegister (mt, VGM_description, "plOpenCP", &gmePlayer);

	API->mdbRegisterReadInfo(&gmeReadInfoReg);

	return errOk;
}

OCP_INTERNAL void gme_type_done (struct PluginCloseAPI_t *API)
{
	struct moduletype mt;

	mt.integer.i = MODULETYPE("AY2");
	API->fsTypeUnregister (mt);

	mt.integer.i = MODULETYPE("GBS");
	API->fsTypeUnregister (mt);

	mt.integer.i = MODULETYPE("GYM");
	API->fsTypeUnregister (mt);

	mt.integer.i = MODULETYPE("HES");
	API->fsTypeUnregister (mt);

	mt.integer.i = MODULETYPE("KSS"); /* GME uses MSX as internal name */
	API->fsTypeUnregister (mt);

	mt.integer.i = MODULETYPE("NSF");
	API->fsTypeUnregister (mt);

	mt.integer.i = MODULETYPE("NSFe");
	API->fsTypeUnregister (mt);

	mt.integer.i = MODULETYPE("SAP"); /* GME uses Atari XL as internal name */
	API->fsTypeUnregister (mt);

	mt.integer.i = MODULETYPE("SPC");
	API->fsTypeUnregister (mt);

	mt.integer.i = MODULETYPE("VGM");
	API->fsTypeUnregister (mt);

	API->mdbUnregisterReadInfo(&gmeReadInfoReg);
}
