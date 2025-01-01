/* OpenCP Module Player
 * copyright (c) 2022-'25 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Parsing of .CUE files, describing the layout of a CDROM image.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "types.h"

#include "cdfs.h"
#include "cue.h"
#include "filesel/filesystem.h"
#include "wave.h"

enum CUE_tokens
{
	CUE_TOKEN_unknown = 0,
	CUE_TOKEN_string = 1, // "foobar"
	CUE_TOKEN_msf = 2,    // 01:23:45  (can be translated from any 1:1 1:12 12:1 12:12 1:1:1 1:1:12 1:12:12 1:12:1 1:12:12 12:1:1 12:1:12 12:12:1)
	CUE_TOKEN_number = 3, // 12345
	CUE_TOKEN_CATALOG,
	CUE_TOKEN_CDTEXTFILE,
	CUE_TOKEN_FILE,
		CUE_TOKEN_WAVE,
		CUE_TOKEN_MP3,
		CUE_TOKEN_AIFF,
		CUE_TOKEN_INTEL,
		CUE_TOKEN_BINARY,
		CUE_TOKEN_MOTOROLA,
	CUE_TOKEN_FLAGS,
		CUE_TOKEN_DCP,
		CUE_TOKEN_4CH,
		CUE_TOKEN_PRE,
		CUE_TOKEN_SCMS,
	CUE_TOKEN_INDEX,
	CUE_TOKEN_ISRC,
	CUE_TOKEN_PERFORMER,
	CUE_TOKEN_POSTGAP,
	CUE_TOKEN_PREGAP,
	CUE_TOKEN_REM,
#if 0
		CUE_TOKEN_ORIGINAL,
			CUE_TOKEN_MEDIA_TYPE, /* MEDIA-TYPE */
				CUE_TOKEN_CD,
				CUE_TOKEN_CD_RW,        /* "CD-RW" */
				CUE_TOKEN_CD_MRW,       /* "CD-MRW" "CD-(MRW)" */
				CUE_TOKEN_DVD,
				CUE_TOKEN_DVD_MRW,      /* "DVD+MRW" "DVD+(MRW)"*/
				CUE_TOKEN_DVD_MRW_DL,   /* "DVD+MRW DL" "DVD+(MRW) DL" */
				CUE_TOKEN_DVDplusR,     /* "DVD+R" */
				CUE_TOKEN_DVDplusR_DL,  /* "DVD+R DL" */
				CUE_TOKEN_DVDplusRW,    /* "DVD+RW" */
				CUE_TOKEN_DVDplusRW_DL, /* "DVD+RW DL" */
				CUE_TOKEN_DVDplusVR,    /* "DVD+VR" */
				CUE_TOKEN_DVD_RAM,      /* "DVD-RAM" */
				CUE_TOKEN_DVD_R,        /* "DVD-R" */
				CUE_TOKEN_DVD_R_DL,     /* "DVD-R DL" */
				CUE_TOKEN_DVD_RW,       /* "DVD-RW" */
				CUE_TOKEN_DVD_RW_DL,    /* "DVD-RW DL" */
				CUE_TOKEN_DVD_VR,       /* "DVD-VR" */
				CUE_TOKEN_DVDRW,        /* "DVDRW"  */
				CUE_TOKEN_HD_DVD,       /* "HD DVD" */
				CUE_TOKEN_HD_DVD_RAM,   /* "HD DVD-RAM" */
				CUE_TOKEN_HD_DVD_R,     /* "HD DVD-R" */
				CUE_TOKEN_HD_DVD_R_DL,  /* "HD DVD-R DL" */
				CUE_TOKEN_HD_DVD_RW,    /* "HD DVD-RW" */
				CUE_TOKEN_HD_DVD_RW_DL, /* "HD DVD-RW DL" */
				CUE_TOKEN_BD,           /* "BD" */
				CUE_TOKEN_BD_R,         /* "BD-R" */
				CUE_TOKEN_BD_R_DL,      /* "BD-R DL" */
				CUE_TOKEN_BD_RE,        /* "BD-RE" */
				CUE_TOKEN_BD_RE_DL,     /* "BD-RE DL" */
		CUE_TOKEN_SESSION,
		CUE_TOKEN_LEAD_OUT, /* LEAD-OUT */
		CUE_TOKEN_RUN_OUT, /* RUN-OUT */
#endif
		CUE_TOKEN_GENRE,
		CUE_TOKEN_DATE,
		CUE_TOKEN_DISCID,
		CUE_TOKEN_COMMENT,
#if 0
		CUE_TOKEN_UPC,
		CUE_TOKEN_ARRANGER,
		CUE_TOKEN_COMPOSER,
		CUE_TOKEN_MESSAGE,
		CUE_TOKEN_DISC_ID,
		CUE_TOKEN_TOC_INFO1,
		CUE_TOKEN_TOC_INFO2,
		CUE_TOKEN_UPC_EAN,
		CUE_TOKEN_SIZE_INFO,
#endif
	CUE_TOKEN_SONGWRITER,
	CUE_TOKEN_TITLE,
	CUE_TOKEN_TRACK,
		CUE_TOKEN_AUDIO,
		CUE_TOKEN_CDG,
		CUE_TOKEN_MODE1_RAW,
		CUE_TOKEN_MODE1_2048, /* MODE1/2048 */
		CUE_TOKEN_MODE1_2352, /* MODE1/2352 */
		CUE_TOKEN_MODE2_RAW,
		CUE_TOKEN_MODE2_2048, /* MODE2/2048 */
		CUE_TOKEN_MODE2_2324, /* MODE2/2324 */
		CUE_TOKEN_MODE2_2336, /* MODE2/2336 */
		CUE_TOKEN_MODE2_2352, /* MODE2/2352 */
		CUE_TOKEN_CDI_2336,   /* CDI/2336 */
		CUE_TOKEN_CDI_2352,   /* CDI/2352 */
};

struct CUE_token_keyword_pair
{
	enum CUE_tokens  e;
	const char      *s;
};

enum CUE_parser_state
{
	CUE_PARSER_STATE_ready = 0,
	CUE_PARSER_STATE_catalog,        /* expects a string to follow */
	CUE_PARSER_STATE_cdtextfile,       /* expects a string to follow */
	CUE_PARSER_STATE_postgap,        /* waiting for msf */
	CUE_PARSER_STATE_pregap,         /* waiting for msf */
	CUE_PARSER_STATE_track_0,        /* waiting for number */
	CUE_PARSER_STATE_track_1,        /* waiting for mode */
	CUE_PARSER_STATE_index_0,        /* waiting for number */
	CUE_PARSER_STATE_index_1,        /* waiting for msf */
	CUE_PARSER_STATE_isrc,           /* waiting for a string */
	CUE_PARSER_STATE_flags,          /* waiting for optional DCP, 4CH, PRE or SCMS */
	CUE_PARSER_STATE_file_0,         /* waiting for a string */
	CUE_PARSER_STATE_file_1,         /* waiting for a content-mode */
	CUE_PARSER_STATE_rem,            /* waiting for subcommand */
	CUE_PARSER_STATE_meta,           /* waiting for string */
};

enum cue_track_mode_t
{
	AUDIO = 0,
	CDG,
	MODE1_RAW,
	MODE1_2048,
	MODE1_2352,
	MODE2_RAW,
	MODE2_2048,
	MODE2_2324,
	MODE2_2336,
	MODE2_2352,
	CDI_2336,
	CDI_2352,
};

static enum cdfs_format_t cue_track_mode_to_cdfs_format (enum cue_track_mode_t mode, int SWAP)
{
	switch (mode)
	{
		case AUDIO:      return SWAP ? FORMAT_AUDIO___NONE : FORMAT_AUDIO_SWAP___NONE;
		case CDG:        return FORMAT_RAW___RAW_RW; /* maybe not a 100% match */
		case MODE1_RAW:  return FORMAT_MODE1_RAW___NONE;
		case MODE1_2048: return FORMAT_MODE1___NONE;
		case MODE1_2352: return FORMAT_MODE1_RAW___NONE;
		case MODE2_RAW:  return FORMAT_MODE2_RAW___NONE;
		case MODE2_2048: return FORMAT_XA_MODE2_FORM1___NONE;
		case MODE2_2324: return FORMAT_XA_MODE2_FORM2___NONE;
		case MODE2_2336: return FORMAT_MODE2___NONE;
		case MODE2_2352: return FORMAT_MODE2_RAW___NONE;
		case CDI_2336:   return FORMAT_MODE2___NONE; /* maybe not a 100% match */
		case CDI_2352:   return FORMAT_RAW___NONE; /* maybe not a 100% match */
		default:         return FORMAT_RAW___NONE;
	}
}

static int medium_sector_size (enum cue_track_mode_t mode)
{
	switch (mode)
	{
		case CDG:        return 2352 + 96;
		default:
		case AUDIO:
		case MODE1_RAW:
		case MODE1_2352:
		case MODE2_RAW:
		case MODE2_2352:
		case CDI_2352:   return 2352;
		case MODE1_2048:
		case MODE2_2048: return 2048;
		case MODE2_2324: return 2324;
		case MODE2_2336:
		case CDI_2336:   return 2336;
	}
}

struct cue_parser_datasource_t
{
	char *filename; /* can be NULL for zero insertion, if mode==AUDIO and filename ends with .wav, treat file as a wave-file with audio... */
//	int64_t length; /* -1 == not initialized  - given in sectors*/
//	uint64_t offset; /* given in bytes..... */
	int swap;
	int wave;
	uint32_t discoffset;
};

struct cue_parser_track_index_t
{
	uint32_t offset;
};

struct cue_parser_track_t
{
	enum cue_track_mode_t track_mode;

	int datasource;

	int index;
	struct cue_parser_track_index_t index_data[100];
	char *title;
	char *performer;
	char *songwriter;
#if 0
	char *composer;
	char *arranger;
	char *message;

	int four_channel_audio;
#endif
	int32_t pregap;  /* given in sectors */
	int32_t postgap; /* given in sectors */
#if 0
/*
	char *genre;
	char *disc_id;
	char *toc_info1;
	char *toc_info2;
	char *upc_ean;
	char *size_info;
*/
#endif
};

struct cue_parser_t
{
	enum CUE_parser_state state;
	char **parser_destination;
	int track;
	struct cue_parser_track_t track_data[100]; /* track 0 is global-common info */

	struct cue_parser_datasource_t *datasource;
	int datasourceN;
};

static int cue_parser_append_source (struct cue_parser_t *cue_parser, const char *src)
{
	void *temp = realloc (cue_parser->datasource, sizeof (cue_parser->datasource[0]) * (cue_parser->datasourceN + 1));
	if (!temp)
	{
		return -1;
	}
	cue_parser->datasource = temp;
	cue_parser->datasource[cue_parser->datasourceN].filename = src ? strdup(src) : 0;
//	cue_parser->datasource[cue_parser->datasourceN].length = -1;
//	cue_parser->datasource[cue_parser->datasourceN].offset = 0;
	cue_parser->datasource[cue_parser->datasourceN].swap = 0;
	cue_parser->datasource[cue_parser->datasourceN].wave = 0;
	cue_parser->datasourceN++;
	return 0;
}

static void cue_parser_modify_source_WAVE (struct cue_parser_t *cue_parser)
{
	cue_parser->datasource[cue_parser->datasourceN - 1].wave = 1;
}

static void cue_parser_modify_source_LITTLEENDIAN (struct cue_parser_t *cue_parser)
{
	cue_parser->datasource[cue_parser->datasourceN - 1].swap = 0;
}

static void cue_parser_modify_source_BINARY (struct cue_parser_t *cue_parser)
{
	cue_parser->datasource[cue_parser->datasourceN - 1].swap = 2; // we need to detect
}

static void cue_parser_modify_source_BIGENDIAN (struct cue_parser_t *cue_parser)
{
	cue_parser->datasource[cue_parser->datasourceN - 1].swap = 1;
}

static int cue_parse_token (struct cue_parser_t *cue_parser, enum CUE_tokens token, const char *src)
{
	if (cue_parser->state == CUE_PARSER_STATE_catalog)
	{
		if ((token == CUE_TOKEN_string) ||
		    (token == CUE_TOKEN_number))
		{
			cue_parser->state = CUE_PARSER_STATE_ready;
			return 0;
		}
		return -1;
	}

	if (cue_parser->state == CUE_PARSER_STATE_track_0)
	{
		int track = atoi (src);
		if (token != CUE_TOKEN_number)
		{
			return -1;
		}
		if ((track > 99) || (track < 0))
		{
			return -1;
		}
		if (track < cue_parser->track)
		{
			return -1;
		}
		if (!cue_parser->datasourceN)
		{
			return -1;
		}
		cue_parser->track = track;
		cue_parser->state = CUE_PARSER_STATE_track_1;
		cue_parser->track_data[cue_parser->track].datasource = cue_parser->datasourceN - 1;
		return 0;
	}

	if (cue_parser->state == CUE_PARSER_STATE_track_1)
	{
		switch (token)
		{
			case CUE_TOKEN_AUDIO:      cue_parser->track_data[cue_parser->track].track_mode = AUDIO;      break;
			case CUE_TOKEN_CDG:        cue_parser->track_data[cue_parser->track].track_mode = CDG;        break;
			case CUE_TOKEN_MODE1_RAW:  cue_parser->track_data[cue_parser->track].track_mode = MODE1_RAW;  break;
			case CUE_TOKEN_MODE1_2048: cue_parser->track_data[cue_parser->track].track_mode = MODE1_2048; break;
			case CUE_TOKEN_MODE1_2352: cue_parser->track_data[cue_parser->track].track_mode = MODE1_2352; break;
			case CUE_TOKEN_MODE2_RAW:  cue_parser->track_data[cue_parser->track].track_mode = MODE2_RAW;  break;
			case CUE_TOKEN_MODE2_2048: cue_parser->track_data[cue_parser->track].track_mode = MODE2_2048; break;
			case CUE_TOKEN_MODE2_2324: cue_parser->track_data[cue_parser->track].track_mode = MODE2_2324; break;
			case CUE_TOKEN_MODE2_2336: cue_parser->track_data[cue_parser->track].track_mode = MODE2_2336; break;
			case CUE_TOKEN_MODE2_2352: cue_parser->track_data[cue_parser->track].track_mode = MODE2_2352; break;
			case CUE_TOKEN_CDI_2336:   cue_parser->track_data[cue_parser->track].track_mode = CDI_2336;   break;
			case CUE_TOKEN_CDI_2352:   cue_parser->track_data[cue_parser->track].track_mode = CDI_2352;   break;
			default:
				return -1;
				break;
		}
		cue_parser->state = CUE_PARSER_STATE_ready;
		return 0;
	}

	if (cue_parser->state == CUE_PARSER_STATE_index_0)
	{
		int index = atoi (src);
		if (token != CUE_TOKEN_number)
		{
			return -1;
		}
		if ((index > 99) || (index < 0))
		{
			return -1;
		}
		if (index < cue_parser->track_data[cue_parser->track].index)
		{
			return -1;
		}
		cue_parser->track_data[cue_parser->track].index = index + 1;
		cue_parser->state = CUE_PARSER_STATE_index_1;
		return 0;
	}

	if (cue_parser->state == CUE_PARSER_STATE_index_1)
	{
		if (token != CUE_TOKEN_msf)
		{
			return -1;
		}
		cue_parser->track_data[cue_parser->track].index_data[cue_parser->track_data[cue_parser->track].index - 1].offset = 
			(src[0]-'0') * 75*60*10 +
			(src[1]-'0') * 75*60    +
			(src[3]-'0') * 75   *10 +
			(src[4]-'0') * 75       +
			(src[6]-'0') *       10 +
			(src[7]-'0');
		cue_parser->state = CUE_PARSER_STATE_ready;
		return 0;
	}

	if (cue_parser->state == CUE_PARSER_STATE_isrc)
	{
		if ((token == CUE_TOKEN_string) ||
		    (token == CUE_TOKEN_number))
		{
			cue_parser->state = CUE_PARSER_STATE_ready;
			return 0;
		}
		return -1;
	}

	if (cue_parser->state == CUE_PARSER_STATE_flags)
	{
		switch (token)
		{
			case CUE_TOKEN_DCP:
			case CUE_TOKEN_4CH:
			case CUE_TOKEN_PRE:
			case CUE_TOKEN_SCMS:
				return 0;
			default:
				cue_parser->state = CUE_PARSER_STATE_ready;
				/* fall-through */
		}
		/* fall-through */
	}

	if (cue_parser->state == CUE_PARSER_STATE_file_0)
	{
		if (token != CUE_TOKEN_string)
		{
			return -1;
		}
		if (cue_parser_append_source (cue_parser, src))
		{
			return -1;
		}
		cue_parser->state = CUE_PARSER_STATE_file_1;
		return 0;
	}

	if (cue_parser->state == CUE_PARSER_STATE_file_1)
	{
		switch (token)
		{
			case CUE_TOKEN_WAVE:
				cue_parser_modify_source_WAVE (cue_parser);
				break;
#warning support MP3?
#if 0
			case CUE_TOKEN_MP3:
				cue_parser_modify_source_MP3 (cue_parser);
				break;
#endif
#warning support AIFF?
#if 0
			case CUE_TOKEN_AIFF:
				cue_parser_modify_source_AIFF (cue_parser);
				break;
#endif
			case CUE_TOKEN_INTEL:
				cue_parser_modify_source_LITTLEENDIAN (cue_parser);
				break;
			case CUE_TOKEN_BINARY:
				cue_parser_modify_source_BINARY (cue_parser); // should be little, but we need to detect
				break;
			case CUE_TOKEN_MOTOROLA:
				cue_parser_modify_source_BIGENDIAN (cue_parser);
				break;
			default:
				return -1;
		}
		cue_parser->state = CUE_PARSER_STATE_ready;
		return 0;
	}

	if (cue_parser->state == CUE_PARSER_STATE_postgap)
	{
		if (token != CUE_TOKEN_msf)
		{
			return -1;
		}
		cue_parser->track_data[cue_parser->track].postgap = 
			(src[0]-'0') * 75*60*10 +
			(src[1]-'0') * 75*60    +
			(src[3]-'0') * 75   *10 +
			(src[4]-'0') * 75       +
			(src[6]-'0')        *10 +
			(src[7]-'0');
		cue_parser->state = CUE_PARSER_STATE_ready;
		return 0;
	}

	if (cue_parser->state == CUE_PARSER_STATE_pregap)
	{
		if (token != CUE_TOKEN_msf)
		{
			return -1;
		}
		cue_parser->track_data[cue_parser->track].pregap = 
			(src[0]-'0') * 75*60*10 +
			(src[1]-'0') * 75*60    +
			(src[3]-'0') * 75   *10 +
			(src[4]-'0') * 75       +
			(src[6]-'0')        *10 +
			(src[7]-'0');
		cue_parser->state = CUE_PARSER_STATE_ready;
		return 0;
	}

	if (cue_parser->state == CUE_PARSER_STATE_rem)
	{
		switch (token)
		{
			case CUE_TOKEN_GENRE:
			case CUE_TOKEN_DATE: /* we ignore dates, in the few examples I have seen, date only contains year as a number */
			case CUE_TOKEN_DISCID:
				cue_parser->parser_destination = 0;
				cue_parser->state = CUE_PARSER_STATE_meta;
				return 0;
			default:
			case CUE_TOKEN_string:
			case CUE_TOKEN_number:
				fprintf (stderr, "REM with unknown token: %s\n", src);
				/* fall-through */
			case CUE_TOKEN_COMMENT:
				return 1;
		}

	}

	if (cue_parser->state == CUE_PARSER_STATE_meta)
	{
		switch (token)
		{
			case CUE_TOKEN_number:
			case CUE_TOKEN_string:
				if ((cue_parser->parser_destination) && (!*cue_parser->parser_destination))
				{
					*cue_parser->parser_destination = strdup (src);
				}
				cue_parser->state = CUE_PARSER_STATE_ready;
				return 0;
			default:
				/* pass-through */
				break;
		}
		/* pass-through */
	}

#warning TODO implement CDTEXTFILE (page 8-2)
	if (cue_parser->state == CUE_PARSER_STATE_cdtextfile)
	{
		if ((token == CUE_TOKEN_string) ||
		    (token == CUE_TOKEN_number))
		{
			cue_parser->state = CUE_PARSER_STATE_ready;
			return 0;
		}
		return -1;
	}

	if (cue_parser->state == CUE_PARSER_STATE_ready)
	{
		switch (token)
		{
			case CUE_TOKEN_CATALOG:
				cue_parser->state = CUE_PARSER_STATE_catalog;
				return 0;

			case CUE_TOKEN_CDTEXTFILE:
				cue_parser->state = CUE_PARSER_STATE_cdtextfile;
				return 0;

			case CUE_TOKEN_TRACK:
				if (cue_parser->track >= 99)
				{
					return -1;
				}
				cue_parser->track++;
				cue_parser->state = CUE_PARSER_STATE_track_0;
				return 0;
			case CUE_TOKEN_ISRC:
				cue_parser->state = CUE_PARSER_STATE_isrc;
				return 0;
			case CUE_TOKEN_FLAGS:
				cue_parser->state = CUE_PARSER_STATE_flags;
				return 0;
			case CUE_TOKEN_FILE:
				cue_parser->state = CUE_PARSER_STATE_file_0;
				return 0;
			case CUE_TOKEN_INDEX:
				cue_parser->state = CUE_PARSER_STATE_index_0;
				return 0;
			case CUE_TOKEN_REM:
				cue_parser->state = CUE_PARSER_STATE_rem;
				return 0;
			case CUE_TOKEN_POSTGAP:
				cue_parser->state = CUE_PARSER_STATE_postgap;
				return 0;
			case CUE_TOKEN_PREGAP:
				cue_parser->state = CUE_PARSER_STATE_pregap;
				return 0;
			case CUE_TOKEN_PERFORMER:
				cue_parser->parser_destination = &cue_parser->track_data[cue_parser->track].performer;
				cue_parser->state = CUE_PARSER_STATE_meta;
				return 0;
			case CUE_TOKEN_SONGWRITER:
				cue_parser->parser_destination = &cue_parser->track_data[cue_parser->track].songwriter;
				cue_parser->state = CUE_PARSER_STATE_meta;
				return 0;
			case CUE_TOKEN_TITLE:
				cue_parser->parser_destination = &cue_parser->track_data[cue_parser->track].title;
				cue_parser->state = CUE_PARSER_STATE_meta;
				return 0;
			default:
				return -1;
		}
	}

	return -1;
}

static int cue_check_keyword (const char *input, int l, const char *needle)
{
	int nl = strlen (needle);
	if (l < nl) return 0;
	if (!memcmp (input, needle, nl))
	{
		if (nl == l) return 1;
		if (input[nl] == ' ') return 1;
		if (input[nl] == '\t') return 1;
		if (input[nl] == '\r') return 1;
		if (input[nl] == '\n') return 1;
	}
	return 0;
}

static void cue_parse_error (const char *orig, const char *input, const char *eol, const int lineno)
{
	int i = 0;
	fprintf (stderr, "Failed to parse .CUE file at line %d\n", lineno + 1);
	while (1)
	{
		if (orig[i] == '\r') break;
		if (orig[i] == '\n') break;
		if (orig[i] == '\t') fprintf (stderr, " ");
		else fprintf (stderr, "%c", orig[i]);
		i++;
	}
	i = 0;
	fprintf (stderr, "\n");
	while (1)
	{
		if (orig[i] == '\r') break;
		if (orig[i] == '\n') break;
		if (orig[i] == '\t') fprintf (stderr, " ");
		if (orig + i == input)
		{
			fprintf (stderr, "^ here\n");
			break;
		}
		fprintf (stderr, " ");
		i++;
	}
	fprintf (stderr, "\n");
}

static int _cue_parse_token (struct cue_parser_t *cue_parser, const char *orig, const char *input, const char *eol, const int lineno, enum CUE_tokens token, const char *buffer)
{
	int res;
	if ((res = cue_parse_token (cue_parser, token, buffer)) < 0)
	{
		cue_parse_error (orig, input, eol, lineno);
		return -1;
	}
	return res;
}

static int cue_parse_line (struct cue_parser_t *cue_parser, const char *input, const char *eol, const int lineno)
{
	const char *orig = input;
	char buffer[2048];
	int bufferfill = 0;
	int l = eol - input;

	if (cue_parser->state == CUE_PARSER_STATE_rem)
	{
		cue_parser->state = CUE_PARSER_STATE_ready;
	}

	while (l)
	{
		const char *current = input;

		if (cue_parser->state == CUE_PARSER_STATE_rem)
		{
			cue_parser->state = CUE_PARSER_STATE_ready;
			return 0;
		}

		bufferfill = 0;
		if ((input[0] == ' ') || (input[0] == '\t'))
		{
			input++; l--;
			continue;
		}

		if ((l >= 2) && (input[0] == '/') && (input[1] == '/'))
		{
			/* rest of the line is a comment */
			return 0;
		}

		if (input[0] == ';')
		{
			/* rest of the line is a comment */
			return 0;
		}

		if (input[0] == '\"')
		{
			input++; l--;
			while (l)
			{
				char toadd = 0;
				if (input[0] == '\"')
				{
					input++; l--;
					break;
				}
				if (input[0] == '\\' && l > 1)
				{
					switch (input[1])
					{
						case 'n': toadd = '\n'; break;
						case 'r': toadd = '\r'; break;
						case 't': toadd = '\t'; break;
						default: toadd = input[1]; break;
					}
					input+=2; l-=2;
				} else {
					toadd = input[0];
					input++; l--;
				}
				if (bufferfill >= (sizeof(buffer)-2))
				{
					cue_parse_error (orig, current, eol, lineno);
					return -1;
				}
				buffer[bufferfill++] = toadd;
			}
			buffer[bufferfill] = 0;
			if (_cue_parse_token (cue_parser, orig, current, eol, lineno, CUE_TOKEN_string, buffer)) return -1;
			continue;
		}

		if (isdigit(input[0]))
		{
			int d0 = 1;
			int d1 = (l >= 2) && isdigit(input[1]);
			int d2 = (l >= 3) && isdigit(input[2]);
			int d3 = (l >= 4) && isdigit(input[3]);
			int d4 = (l >= 5) && isdigit(input[4]);
			int d5 = (l >= 6) && isdigit(input[5]);
			int d6 = (l >= 7) && isdigit(input[6]);
			int d7 = (l >= 8) && isdigit(input[7]);
			int c1 = (l >= 2) && (input[1] == ':');
			int c2 = (l >= 3) && (input[2] == ':');
			int c3 = (l >= 4) && (input[3] == ':');
			int c4 = (l >= 5) && (input[4] == ':');
			int c5 = (l >= 6) && (input[5] == ':');

			     if (d0 && d1 && c2 && d3 && d4 && c5 && d6 && d7) { sprintf (buffer, "%c%c:%c%c:%c%c", input[0], input[1], input[3], input[4], input[6], input[7]); input += 8; l-= 8; if (_cue_parse_token (cue_parser, orig, current, eol, lineno, CUE_TOKEN_msf, buffer)) return -1; continue; } // 12:12:12
			else if (      d0 && c1 && d2 && d3 && c4 && d5 && d6) { sprintf (buffer, "0%c:%c%c:%c%c",            input[0], input[2], input[3], input[5], input[6]); input += 7; l-= 7; if (_cue_parse_token (cue_parser, orig, current, eol, lineno, CUE_TOKEN_msf, buffer)) return -1; continue; } //  1:12:12
			else if (d0 && d1 && c2 &&       d3 && c4 && d5 && d6) { sprintf (buffer, "%c%c:0%c:%c%c",  input[0], input[1],           input[3], input[5], input[6]); input += 7; l-= 7; if (_cue_parse_token (cue_parser, orig, current, eol, lineno, CUE_TOKEN_msf, buffer)) return -1; continue; } // 12: 1:12
			else if (d0 && d1 && c2 && d3 && d4 && c5 &&       d6) { sprintf (buffer, "%c%c:%c%c:0%c",  input[0], input[1], input[3], input[4],           input[6]); input += 7; l-= 7; if (_cue_parse_token (cue_parser, orig, current, eol, lineno, CUE_TOKEN_msf, buffer)) return -1; continue; } // 12:12: 1
			else if (      d0 && c1 &&       d2 && c3 && d4 && d5) { sprintf (buffer, "0%c:0%c:%c%c",             input[0],           input[2], input[4], input[5]); input += 6; l-= 6; if (_cue_parse_token (cue_parser, orig, current, eol, lineno, CUE_TOKEN_msf, buffer)) return -1; continue; } //  1: 1:12
			else if (      d0 && c1 && d2 && d3 && c4 &&       d5) { sprintf (buffer, "0%c:%c%c:0%c",             input[0], input[2], input[3],           input[5]); input += 6; l-= 6; if (_cue_parse_token (cue_parser, orig, current, eol, lineno, CUE_TOKEN_msf, buffer)) return -1; continue; } //  1:12: 1
			else if (d0 && d1 && c2 &&       d3 && c3 &&       d5) { sprintf (buffer, "%c%c:0%c:0%c",   input[0], input[1],           input[3],           input[5]); input += 6; l-= 6; if (_cue_parse_token (cue_parser, orig, current, eol, lineno, CUE_TOKEN_msf, buffer)) return -1; continue; } // 12: 1: 1
			else if (      d0 && c1 &&       d2 && c3 &&       d4) { sprintf (buffer, "0%c:0%c:0%c",              input[0],           input[2],           input[4]); input += 5; l-= 5; if (_cue_parse_token (cue_parser, orig, current, eol, lineno, CUE_TOKEN_msf, buffer)) return -1; continue; } //  1: 1: 1
			else if (                  d0 && d1 && c2 && d3 && d4) { sprintf (buffer, "00:%c%c:%c%c",                       input[0], input[1], input[3], input[4]); input += 5; l-= 5; if (_cue_parse_token (cue_parser, orig, current, eol, lineno, CUE_TOKEN_msf, buffer)) return -1; continue; } //    12:12
			else if (                        d0 && c1 && d2 && d3) { sprintf (buffer, "00:0%c:%c%c",                                  input[0], input[2], input[3]); input += 4; l-= 4; if (_cue_parse_token (cue_parser, orig, current, eol, lineno, CUE_TOKEN_msf, buffer)) return -1; continue; } //     1:12
			else if (                  d0 && d1 && c2 &&       d3) { sprintf (buffer, "00:%c%c:0%c",                                  input[0], input[1], input[3]); input += 4; l-= 4; if (_cue_parse_token (cue_parser, orig, current, eol, lineno, CUE_TOKEN_msf, buffer)) return -1; continue; } //    12: 1
			else if (                        d0 && c1 &&       d2) { sprintf (buffer, "00:0%c:0%c",                                   input[0],           input[2]); input += 3; l-= 3; if (_cue_parse_token (cue_parser, orig, current, eol, lineno, CUE_TOKEN_msf, buffer)) return -1; continue; } //     1: 1
			else {
				while (l)
				{
					if ((input[0] == ' ') || (input[0] == '\t'))
					{
						break;
					}
					if (!isdigit(input[0]))
					{
						goto instring;
					}
					if (bufferfill >= (sizeof(buffer)-2))
					{
						cue_parse_error (orig, current, eol, lineno);
						return -1;
					}
					buffer[bufferfill++] = input[0];
					input++; l--;
				}
				buffer[bufferfill] = 0;
				if (_cue_parse_token (cue_parser, orig, current, eol, lineno, CUE_TOKEN_number, buffer)) return -1;
				continue;
			}
		}

		if (1)
		{
			int i;
			struct CUE_token_keyword_pair d[] =
			{
				{CUE_TOKEN_CATALOG,            "CATALOG"},
				{CUE_TOKEN_CDTEXTFILE,         "CDTEXTFILE"},
				{CUE_TOKEN_TITLE,              "TITLE"},
				{CUE_TOKEN_PERFORMER,          "PERFORMER"},
				{CUE_TOKEN_SONGWRITER,         "SONGWRITER"},
				{CUE_TOKEN_POSTGAP,            "POSTGAP"},
				{CUE_TOKEN_PREGAP,             "PREGAP"},
				{CUE_TOKEN_TRACK,              "TRACK"},
				{CUE_TOKEN_AUDIO,              "AUDIO"},
				{CUE_TOKEN_CDG,                "CDG"},
				{CUE_TOKEN_MODE1_RAW,          "MODE1_RAW"},
				{CUE_TOKEN_MODE1_2048,         "MODE1/2048"},
				{CUE_TOKEN_MODE1_2352,         "MODE1/2352"},
				{CUE_TOKEN_MODE2_RAW,          "MODE2_RAW"},
				{CUE_TOKEN_MODE2_2048,         "MODE2/2048"},
				{CUE_TOKEN_MODE2_2324,         "MODE2/2324"},
				{CUE_TOKEN_MODE2_2336,         "MODE2/2336"},
				{CUE_TOKEN_MODE2_2352,         "MODE2/2352"},
				{CUE_TOKEN_CDI_2336,           "CDI/2336"},
				{CUE_TOKEN_CDI_2352,           "CDI/2352"},
				{CUE_TOKEN_INDEX,              "INDEX"},
				{CUE_TOKEN_ISRC,               "ISRC"},
				{CUE_TOKEN_REM,                "REM"},
				{CUE_TOKEN_GENRE,              "GENRE"},
				{CUE_TOKEN_DATE,               "DATE"},
				{CUE_TOKEN_DISCID,             "DISCID"},
				{CUE_TOKEN_COMMENT,            "COMMENT"},
				{CUE_TOKEN_FILE,               "FILE"},
				{CUE_TOKEN_WAVE,               "WAVE"},
				{CUE_TOKEN_AIFF,               "AIFF"},
				{CUE_TOKEN_INTEL,              "INTEL"},
				{CUE_TOKEN_BINARY,             "BINARY"},
				{CUE_TOKEN_MOTOROLA,           "MOTOROLA"},
				{CUE_TOKEN_FLAGS,              "FLAGS"},
				{CUE_TOKEN_DCP,                "DCP"},
				{CUE_TOKEN_4CH,                "4CH"},
				{CUE_TOKEN_PRE,                "PRE"},
				{CUE_TOKEN_SCMS,               "SCMS"},
			};
			for (i=0; i < (sizeof (d) / sizeof (d[0])); i++)
			{
				if (cue_check_keyword (input, l, d[i].s))
				{
					int res;

					input+=strlen (d[i].s); l-= strlen (d[i].s);
					res = _cue_parse_token (cue_parser, orig, current, eol, lineno, d[i].e, d[i].s);
					if (res < 0)
					{
						return -1;
					}
					if (res > 0)
					{
						cue_parser->state = CUE_PARSER_STATE_ready;
						return 0; /* rest of this line is a comment */
					}
					break;
				}
			}
			if (i != (sizeof (d) / sizeof (d[0])))
			{
				continue;
			}
			/* fall-through */
		}

		if (1)
		{
			while (l)
			{
				char toadd = 0;
instring:
				if (input[0] == ' ')
				{
					input++; l--;
					break;
				}
				if (input[0] == '\\' && l > 1)
				{
					switch (input[1])
					{
						case 'n': toadd = '\n'; break;
						case 'r': toadd = '\r'; break;
						case 't': toadd = '\t'; break;
						default: toadd = input[1]; break;
					}
					input+=2; l-=2;
				} else {
					toadd = input[0];
					input++; l--;
				}
				if (bufferfill >= (sizeof(buffer)-2))
				{
					cue_parse_error (orig, current, eol, lineno);
					return -1;
				}
				buffer[bufferfill++] = toadd;
			}
			buffer[bufferfill] = 0;
			if (_cue_parse_token (cue_parser, orig, current, eol, lineno, CUE_TOKEN_string, buffer)) return -1;
			continue;
		}
	}
	return 0;
}

OCP_INTERNAL void cue_parser_free (struct cue_parser_t *cue_parser)
{
	int i;
	for (i = 0; i < 100; i++)
	{
		free (cue_parser->track_data[i].title);
		free (cue_parser->track_data[i].performer);
		free (cue_parser->track_data[i].songwriter);
#if 0
		free (cue_parser->track_data[i].composer);
		free (cue_parser->track_data[i].arranger);
		free (cue_parser->track_data[i].message);
#endif
	}
	for (i = 0; i < cue_parser->datasourceN; i++)
	{
		free (cue_parser->datasource[i].filename);
	}
	free (cue_parser->datasource);

	free (cue_parser);
}

OCP_INTERNAL struct cue_parser_t *cue_parser_from_data (const char *input)
{
	struct cue_parser_t *retval;
	const char *eof = input + strlen (input);

	int rN = 0;
	int rR = 0;

	retval = calloc (sizeof (*retval), 1);
	if (!retval)
	{
		fprintf (stderr, "cue_parser() calloc() failed\n");
		return 0;
	}

	while (*input)
	{
		const char *eol = eof;
		char *eol1 = strchr (input, '\r');
		char *eol2 = strchr (input, '\n');
		if (eol1 && eol1 < eol) eol = eol1;
		if (eol2 && eol2 < eol) eol = eol2;

		if (input != eol)
		{
			if (*eol == '\r') rR++;
			if (*eol == '\n') rN++;

			if (cue_parse_line (retval, input, eol, rR > rN ? rR : rN))
			{
				cue_parser_free (retval);
				return 0;
			}
		}
		input = eol + 1;
	}

	return retval;
}

OCP_INTERNAL void detect_endian (const uint8_t *buffer, int *little, int *big)
{
	int16_t prev_big_left = 0;
	int16_t prev_big_right = 0;
	int16_t prev_little_left = 0;
	int16_t prev_little_right = 0;
	uint_fast32_t big_accumulated = 0;
	uint_fast32_t little_accumulated = 0;
	int i;
	for (i=0; i < 588; i++)
	{
		int16_t big_left     = buffer[(i<<2)+1] | (buffer[(i<<2)+0] << 8);
		int16_t big_right    = buffer[(i<<2)+3] | (buffer[(i<<2)+2] << 8);
		int16_t little_left  = buffer[(i<<2)+0] | (buffer[(i<<2)+1] << 8);
		int16_t little_right = buffer[(i<<2)+2] | (buffer[(i<<2)+3] << 8);
		big_accumulated += abs((int32_t)prev_big_left  - big_left);
		big_accumulated += abs((int32_t)prev_big_right - big_right);
		little_accumulated += abs((int32_t)prev_little_left  - little_left);
		little_accumulated += abs((int32_t)prev_little_right - little_right);
		prev_big_left     = big_left;
		prev_big_right    = big_right;
		prev_little_left  = little_left;
		prev_little_right = little_right;
	}
	if (big_accumulated < little_accumulated)
	{
		(*big) += 1;
	} else if (little_accumulated < big_accumulated)
	{
		(*little) += 1;
	}
}

OCP_INTERNAL struct cdfs_disc_t *cue_parser_to_cdfs_disc (struct ocpfile_t *parentfile, struct cue_parser_t *cue_parser)
{
	struct cdfs_disc_t *retval = cdfs_disc_new (parentfile);
	int i;
	uint32_t discoffset = 0;
	int trackcounter = 1;

	if (!retval)
	{
		fprintf (stderr, "cue_parser_to_cdfs_disc(): cdfs_disc_new() failed\n");
		return 0;
	}

	for (i=0; i < cue_parser->datasourceN; i++)
	{
		struct ocpfile_t *file = 0;
		struct ocpfilehandle_t *fh = 0;
		uint64_t offset = 0;
		uint64_t length;
		enum cue_track_mode_t mode = AUDIO;
		int j;
		int ms;
		uint32_t sectorcount;

		if (cue_parser->datasource[i].wave)
		{
			if (wave_openfile (parentfile->parent, cue_parser->datasource[i].filename, &file, &fh, &offset, &length))
			{
				fprintf (stderr, "Failed to open wave file %s (format must be stereo, 16bit, 44100 sample-rate)\n", cue_parser->datasource[i].filename);
				goto fail_out;
			}
		} else {
			if (data_openfile (parentfile->parent, cue_parser->datasource[i].filename, &file, &fh, &length))
			{
				fprintf (stderr, "Failed to open data file %s\n", cue_parser->datasource[i].filename);
				goto fail_out;
			}
		}
		/* We ignore post-gap and pre-gap. The structure of .CUE is not 100% given if
		* multiple-tracks uses the same data, or multple data maps to the same track
		 */

		/* first iteration, figure out mode */ 
		for (j = 0; j <= cue_parser->track; j++)
		{
			if ( cue_parser->track_data[j].datasource > i) goto superbreak;
			mode = cue_parser->track_data[j].track_mode;
			if ( cue_parser->track_data[j].datasource == i) break;
		}

		ms = medium_sector_size (mode);
		sectorcount = (length + ms - 1) / ms;

		if (cue_parser->datasource[i].swap == 2) // we need to detect endian, broken images out in the wild
		{
			int datatracks = 0;
			int audiotracks_big = 0;
			int audiotracks_little = 0;
			int copytrackcounter = trackcounter;
			uint8_t buffer[2352];

			for (; copytrackcounter <= cue_parser->track; copytrackcounter++)
			{
				if (cue_parser->track_data[copytrackcounter].datasource > i) break;

				if (cue_parser->track_data[copytrackcounter].track_mode == AUDIO)
				{
					int offset = cue_parser->track_data[copytrackcounter].index_data[1].offset;
					int length = ((copytrackcounter + 1 > cue_parser->track) ||
					              (cue_parser->track_data[copytrackcounter].datasource != cue_parser->track_data[copytrackcounter+1].datasource)) ?
					                      sectorcount - cue_parser->track_data[copytrackcounter].index_data[1].offset :
					                      cue_parser->track_data[copytrackcounter+1].index_data[1].offset - cue_parser->track_data[copytrackcounter].index_data[1].offset;
					int i;
					for (i=0; i < 5; i++)
					{
						if (i * 75 >= length)
						{
							break;
						}
						fh->seek_set (fh, (offset + i) * 2352);
						if (fh->read (fh, buffer, 2352) == 2352)
						{
							detect_endian (buffer, &audiotracks_little, &audiotracks_big);
						}
					}
				} else if ((cue_parser->track_data[copytrackcounter].track_mode == MODE1_2352) ||
				           (cue_parser->track_data[copytrackcounter].track_mode == MODE2_2352))
				{
					datatracks++; // safe bet
					break;
				}
			}

			if (datatracks) /* DATA => little endian, no swap needed */
			{
				cue_parser->datasource[i].swap = 0;
			} else if (audiotracks_big > audiotracks_little)
			{
				cue_parser->datasource[i].swap = 1;
			} else {
				cue_parser->datasource[i].swap = 0;
			}
		}

		cdfs_disc_datasource_append (retval,
		                             discoffset,
		                             sectorcount,
		                             file,
		                             fh,
		                             cue_track_mode_to_cdfs_format (mode, cue_parser->datasource[i].swap),
		                             offset,
		                             length);

		if (file) file->unref (file);
		if (fh) fh->unref (fh);

		/* add TRACK00 */
		cdfs_disc_track_append (retval,
		                        0, /* we ignore pregaps, since they require zero insertions in CUE files */
		                        0,
		                        0,
		                        0, /* title */
		                        0, /* performer */
		                        0, /* songwriter */
		                        0, /* composer */
		                        0, /* arranger */
		                        0); /* message */

		/* second iteration, add referenced tracks */ 
		for (; trackcounter <= cue_parser->track; trackcounter++)
		{
			uint32_t pregap = 0;
			if (cue_parser->track_data[trackcounter].datasource > i) break;

			if (trackcounter == 1)
			{
				pregap = cue_parser->track_data[trackcounter].index_data[1].offset;
			} else {
				if (cue_parser->track_data[trackcounter].index_data[0].offset && cue_parser->track_data[trackcounter].index_data[1].offset)
				{
					pregap = cue_parser->track_data[trackcounter].index_data[1].offset - cue_parser->track_data[trackcounter].index_data[0].offset;
				}
			}

			cdfs_disc_track_append (retval,
			                        pregap,
			                        cue_parser->track_data[trackcounter].index_data[1].offset + discoffset,
			                        ((trackcounter + 1 > cue_parser->track) ||
						(cue_parser->track_data[trackcounter].datasource != cue_parser->track_data[trackcounter+1].datasource)) ?
							sectorcount - cue_parser->track_data[trackcounter].index_data[1].offset :
							cue_parser->track_data[trackcounter+1].index_data[1].offset - cue_parser->track_data[trackcounter].index_data[1].offset,
			                        cue_parser->track_data[trackcounter].title,
			                        cue_parser->track_data[trackcounter].performer,
			                        cue_parser->track_data[trackcounter].songwriter,
			                        0, /* composer */
			                        0, /* arranger */
			                        0); /* message */
		}

		discoffset += sectorcount;
	}

superbreak:
	//cdfs_disc_unref (retval);
	return retval;
fail_out:
	cdfs_disc_unref (retval);
	return 0;
}
