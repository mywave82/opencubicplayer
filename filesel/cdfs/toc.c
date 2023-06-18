/* OpenCP Module Player
 * copyright (c) 2022-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Parsing of .TOC files, describing the layout of a CDROM image.
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
#include "filesel/filesystem.h"
#include "toc.h"
#include "wave.h"

enum TOC_tokens
{
	TOC_TOKEN_unknown = 0,
	TOC_TOKEN_string = 1, // "foobar"
	TOC_TOKEN_offset = 2, // #12345
	TOC_TOKEN_msf = 3,    // 01:23:45  (can be translated from any 1:1 1:12 12:1 12:12 1:1:1 1:1:12 1:12:12 1:12:1 1:12:12 12:1:1 12:1:12 12:12:1)
	TOC_TOKEN_number = 4, // 12345
	TOC_TOKEN_open = 5,   // {
	TOC_TOKEN_close = 6,   // }
	TOC_TOKEN_colon = 7, // :
	TOC_TOKEN_comma = 8, // ,   used by binary data in TOC_INFO1, TOC_INFO2 and SIZE_INFO
	TOC_TOKEN_CATALOG,
	TOC_TOKEN_CD_DA,
	TOC_TOKEN_CD_ROM,
	TOC_TOKEN_CD_ROM_XA,
	TOC_TOKEN_CD_TEXT,
		TOC_TOKEN_LANGUAGE_MAP,
			TOC_TOKEN_EN,
		TOC_TOKEN_LANGUAGE,
			TOC_TOKEN_TITLE,
			TOC_TOKEN_PERFORMER,
			TOC_TOKEN_SONGWRITER,
			TOC_TOKEN_COMPOSER,
			TOC_TOKEN_ARRANGER,
			TOC_TOKEN_MESSAGE,
			TOC_TOKEN_DISC_ID,
			TOC_TOKEN_GENRE,
			TOC_TOKEN_TOC_INFO1,
			TOC_TOKEN_TOC_INFO2,
			TOC_TOKEN_UPC_EAN,
			TOC_TOKEN_SIZE_INFO,
	TOC_TOKEN_TRACK,
		TOC_TOKEN_AUDIO,
		TOC_TOKEN_MODE1,
		TOC_TOKEN_MODE1_RAW,
		TOC_TOKEN_MODE2,
		TOC_TOKEN_MODE2_FORM1,
		TOC_TOKEN_MODE2_FORM2,
		TOC_TOKEN_MODE2_FORM_MIX,
		TOC_TOKEN_MODE2_RAW,
			TOC_TOKEN_RW,
			TOC_TOKEN_RW_RAW,
	TOC_TOKEN_NO,
	TOC_TOKEN_COPY,
	TOC_TOKEN_PRE_EMPHASIS,
	TOC_TOKEN_TWO_CHANNEL_AUDIO,
	TOC_TOKEN_FOUR_CHANNEL_AUDIO,
	TOC_TOKEN_ISRC,
	TOC_TOKEN_SILENCE,
	TOC_TOKEN_ZERO,
	TOC_TOKEN_FILE,
	TOC_TOKEN_AUDIOFILE,
		TOC_TOKEN_SWAP,
	TOC_TOKEN_DATAFILE,
	TOC_TOKEN_FIFO,
	TOC_TOKEN_START,
	TOC_TOKEN_PREGAP,
	TOC_TOKEN_INDEX
};

struct TOC_token_keyword_pair
{
	enum TOC_tokens  e;
	const char      *s;
};

enum TOC_parser_state
{
	TOC_PARSER_STATE_ready = 0,
	TOC_PARSER_STATE_catalog,        /* expects a string to follow */
	TOC_PARSER_STATE_cd_text_0,      /* waiting for { */
	TOC_PARSER_STATE_cd_text_1,
	TOC_PARSER_STATE_language_map_0, /* waiting for { */
	TOC_PARSER_STATE_language_map_1, /* waiting for id */
	TOC_PARSER_STATE_language_map_2, /* waiting for : */
	TOC_PARSER_STATE_language_map_3, /* waiting for EN */

	TOC_PARSER_STATE_language_0,     /* waiting for id */
	TOC_PARSER_STATE_language_1,     /* waiting for { */
	TOC_PARSER_STATE_language_2,     /* waiting for parameter name */
	TOC_PARSER_STATE_language_3,     /* waiting for parameter value or open */
	TOC_PARSER_STATE_language_4,     /* waiting for parameter numbers, comma or close */

	TOC_PARSER_STATE_track_0,        /* waiting for mode */
	TOC_PARSER_STATE_track_1,        /* waiting for optional subchannel */

	TOC_PARSER_STATE_no,             /* waiting for COPY or PRE_EMPHASIS */

	TOC_PARSER_STATE_isrc,           /* waiting for a string */

	TOC_PARSER_STATE_zero,           /* waiting for a msf or audio or rw*/

	TOC_PARSER_STATE_audiofile_0,    /* waiting for a string */
	TOC_PARSER_STATE_audiofile_1,    /* waiting for SWAP offset or MSF */
	TOC_PARSER_STATE_audiofile_2,    /* waiting for optional MSF */

	TOC_PARSER_STATE_datafile_0,     /* waiting for a string */
	TOC_PARSER_STATE_datafile_1,     /* waiting for offset or MSF */

	TOC_PARSER_STATE_start,          /* waiting for optional msf */
	TOC_PARSER_STATE_pregap,         /* waiting for msf */

	TOC_PARSER_STATE_index,          /* waiting for msf */
};

enum toc_storage_mode_t
{
	AUDIO = 0,
	MODE1,
	MODE1_RAW,
	MODE2,
	MODE2_FORM1,
	MODE2_FORM2,
	MODE2_FORM_MIX,
	MODE2_RAW
};

enum toc_storage_mode_subchannel_t
{
	NONE = 0,
	SUBCHANNEL_RW,
	SUBCHANNEL_RW_RAW,
};

static enum cdfs_format_t toc_storage_mode_to_cdfs_format (enum toc_storage_mode_t mode, enum toc_storage_mode_subchannel_t subchannel, int swap)
{
	enum cdfs_format_t retval = 0;
	switch (mode)
	{
		case AUDIO:          retval = swap ? FORMAT_AUDIO_SWAP___NONE : FORMAT_AUDIO___NONE; break;
		case MODE1:          retval =        FORMAT_MODE1___NONE;                            break;
		case MODE1_RAW:      retval =        FORMAT_MODE1_RAW___NONE;                        break;
		case MODE2:          retval =        FORMAT_MODE2___NONE;                            break;
		case MODE2_FORM1:    retval =        FORMAT_XA_MODE2_FORM1___NONE;                   break;
		case MODE2_FORM2:    retval =        FORMAT_XA_MODE2_FORM2___NONE;                   break;
		case MODE2_FORM_MIX: retval =        FORMAT_XA_MODE2_FORM_MIX___NONE;                break;
		case MODE2_RAW:      retval =        FORMAT_MODE2_RAW___NONE;                        break;
	}
	switch (subchannel)
	{
		case NONE:                           break;
		case SUBCHANNEL_RW:     retval += 1; break;
		case SUBCHANNEL_RW_RAW: retval += 2; break;
	}
	return retval;
}

static int medium_sector_size (enum toc_storage_mode_t mode, enum toc_storage_mode_subchannel_t subchannel)
{
	int retval = 0;
	switch (mode)
	{
		case AUDIO:
		case MODE1_RAW:
		case MODE2_RAW:      retval = 2352; break;

		case MODE1:
		case MODE2_FORM1:    retval = 2048; break;

		case MODE2:
		case MODE2_FORM_MIX: retval = 2336; break; /* these two matches for the wrong reason. MODE2 skips the HEADER, MODE2_FORM_MIX skips the EDC */

		case MODE2_FORM2:    retval = 2324; break;
	}
	switch (subchannel)
	{
		case NONE:                           break;

		case SUBCHANNEL_RW:
		case SUBCHANNEL_RW_RAW: retval += 96; break;
	}
	return retval;
}

struct toc_parser_datasource_t
{
	char *filename; /* can be NULL for zero insertion, if mode==AUDIO and filename ends with .wav, treat file as a wave-file with audio... */
	int64_t length; /* -1 == not initialized  - given in sectors*/
	uint64_t offset; /* given in bytes..... */
	int swap;
};

struct toc_parser_track_t
{
	enum toc_storage_mode_t            storage_mode;
	enum toc_storage_mode_subchannel_t storage_mode_subchannel;

	char *title;
	char *performer;
	char *songwriter;
	char *composer;
	char *arranger;
	char *message;

	int four_channel_audio;
	int32_t offset; /* given in sectors */
	int32_t pregap; /* given in sectors */
/*
	char *genre;
	char *disc_id;
	char *toc_info1;
	char *toc_info2;
	char *upc_ean;
	char *size_info;
*/

	struct toc_parser_datasource_t *datasource;
	int datasourceN;
};

struct toc_parser_t
{
	enum TOC_parser_state state;
	char **parser_destination;
	int track;
	struct toc_parser_track_t track_data[100]; /* track 0 is global-common info */
};

static int toc_parser_append_source (struct toc_parser_t *toc_parser, const char *src)
{
	void *temp = realloc (toc_parser->track_data[toc_parser->track].datasource, sizeof (toc_parser->track_data[toc_parser->track].datasource[0]) * (toc_parser->track_data[toc_parser->track].datasourceN + 1));
	if (!temp)
	{
		return -1;
	}
	toc_parser->track_data[toc_parser->track].datasource = temp;
	toc_parser->track_data[toc_parser->track].datasource[toc_parser->track_data[toc_parser->track].datasourceN].filename = src ? strdup(src) : 0;
	toc_parser->track_data[toc_parser->track].datasource[toc_parser->track_data[toc_parser->track].datasourceN].length = -1;
	toc_parser->track_data[toc_parser->track].datasource[toc_parser->track_data[toc_parser->track].datasourceN].offset = 0;
	toc_parser->track_data[toc_parser->track].datasource[toc_parser->track_data[toc_parser->track].datasourceN].swap = 0;
	toc_parser->track_data[toc_parser->track].datasourceN++;

	return 0;
}

static void toc_parser_modify_length (struct toc_parser_t *toc_parser, const int64_t length)
{
	toc_parser->track_data[toc_parser->track].datasource[toc_parser->track_data[toc_parser->track].datasourceN - 1].length = length;
}

static void toc_parser_modify_offset (struct toc_parser_t *toc_parser, const char *src)
{
	unsigned long long length = strtoull (src + 1, 0, 10);
	toc_parser->track_data[toc_parser->track].datasource[toc_parser->track_data[toc_parser->track].datasourceN - 1].offset = length;
}

static void toc_parser_modify_offset_sector (struct toc_parser_t *toc_parser, const int offset)
{
	toc_parser->track_data[toc_parser->track].datasource[toc_parser->track_data[toc_parser->track].datasourceN - 1].offset = (long unsigned int)offset * SECTORSIZE_XA2;
}

static void toc_parser_modify_SWAP (struct toc_parser_t *toc_parser)
{
	toc_parser->track_data[toc_parser->track].datasource[toc_parser->track_data[toc_parser->track].datasourceN - 1].swap = 1;
}

static void toc_parser_modify_pregap (struct toc_parser_t *toc_parser, const int length)
{
	toc_parser->track_data[toc_parser->track].pregap = length;
}

static void toc_parser_append_index (struct toc_parser_t *toc_parser, const int length)
{
/*
	unsigned long long length = strtoull (src + 1, 0, 10);
	...
	we do not need use the index...
*/
}

static int toc_parse_token (struct toc_parser_t *toc_parser, enum TOC_tokens token, const char *src)
{
	if (toc_parser->state == TOC_PARSER_STATE_catalog)
	{
		if ((token == TOC_TOKEN_string) ||
		    (token == TOC_TOKEN_number))
		{
			toc_parser->state = TOC_PARSER_STATE_ready;
			return 0;
		}
		return -1;
	}

	if (toc_parser->state == TOC_PARSER_STATE_cd_text_0)
	{
		if (token == TOC_TOKEN_open)
		{
			toc_parser->state = TOC_PARSER_STATE_cd_text_1;
			return 0;
		}
		return -1;
	}

	if (toc_parser->state == TOC_PARSER_STATE_cd_text_1)
	{
		switch (token)
		{
			case TOC_TOKEN_close:
				toc_parser->state = TOC_PARSER_STATE_ready;
				return 0;
			case TOC_TOKEN_LANGUAGE_MAP:
				if (toc_parser->track == 0) /* only valid in the header */
				{
					toc_parser->state = TOC_PARSER_STATE_language_map_0;
					return 0;
				}
				return -1;
			case TOC_TOKEN_LANGUAGE:
				toc_parser->state = TOC_PARSER_STATE_language_0;
				return 0;
			default:
				return -1;
		}
	}

	if (toc_parser->state == TOC_PARSER_STATE_language_map_0)
	{
		if (token == TOC_TOKEN_open)
		{
			toc_parser->state = TOC_PARSER_STATE_language_map_1;
			return 0;
		}
		return -1;
	}

	if (toc_parser->state == TOC_PARSER_STATE_language_map_1)
	{
		switch (token)
		{
			case TOC_TOKEN_close:
				toc_parser->state = TOC_PARSER_STATE_cd_text_1;
				return 0;
			case TOC_TOKEN_number:
				toc_parser->state = TOC_PARSER_STATE_language_map_2;
				return 0;
			default:
				return -1;
		}
	}

	if (toc_parser->state == TOC_PARSER_STATE_language_map_2)
	{
		if (token == TOC_TOKEN_colon)
		{
			toc_parser->state = TOC_PARSER_STATE_language_map_3;
			return 0;
		}
		return -1;
	}

	if (toc_parser->state == TOC_PARSER_STATE_language_map_3)
	{
		/* seems like EN is the only accepted language...? */
		if ((token == TOC_TOKEN_EN) || (token == TOC_TOKEN_number))
		{
			toc_parser->state = TOC_PARSER_STATE_language_map_1;
			return 0;
		}
		return -1;
	}

	if (toc_parser->state == TOC_PARSER_STATE_language_0)
	{
		if (token == TOC_TOKEN_number)
		{
			toc_parser->state = TOC_PARSER_STATE_language_1;
			return 0;
		}
		return -1;
	}

	if (toc_parser->state == TOC_PARSER_STATE_language_1)
	{
		if (token == TOC_TOKEN_open)
		{
			toc_parser->state = TOC_PARSER_STATE_language_2;
			return 0;
		}
		return -1;
	}

	if (toc_parser->state == TOC_PARSER_STATE_language_2)
	{
		switch (token)
		{
			case TOC_TOKEN_close:
				toc_parser->state = TOC_PARSER_STATE_cd_text_1;
				return 0;
			case TOC_TOKEN_TITLE:
				toc_parser->parser_destination = &toc_parser->track_data[toc_parser->track].title;
				toc_parser->state = TOC_PARSER_STATE_language_3;
				return 0;
			case TOC_TOKEN_PERFORMER:
				toc_parser->parser_destination = &toc_parser->track_data[toc_parser->track].performer;
				toc_parser->state = TOC_PARSER_STATE_language_3;
				return 0;
			case TOC_TOKEN_SONGWRITER:
				toc_parser->parser_destination = &toc_parser->track_data[toc_parser->track].songwriter;
				toc_parser->state = TOC_PARSER_STATE_language_3;
				return 0;
			case TOC_TOKEN_COMPOSER:
				toc_parser->parser_destination = &toc_parser->track_data[toc_parser->track].composer;
				toc_parser->state = TOC_PARSER_STATE_language_3;
				return 0;
			case TOC_TOKEN_ARRANGER:
				toc_parser->parser_destination = &toc_parser->track_data[toc_parser->track].arranger;
				toc_parser->state = TOC_PARSER_STATE_language_3;
				return 0;
			case TOC_TOKEN_MESSAGE:
				toc_parser->parser_destination = &toc_parser->track_data[toc_parser->track].message;
				toc_parser->state = TOC_PARSER_STATE_language_3;
				return 0;
			case TOC_TOKEN_GENRE:
			case TOC_TOKEN_DISC_ID:
			case TOC_TOKEN_TOC_INFO1:
			case TOC_TOKEN_TOC_INFO2:
			case TOC_TOKEN_UPC_EAN:
			case TOC_TOKEN_SIZE_INFO:
			case TOC_TOKEN_ISRC:
				/* we ignore these */
				toc_parser->parser_destination = 0;
				toc_parser->state = TOC_PARSER_STATE_language_3;
				return 0;
			default:
				return -1;
		}
	}

	if (toc_parser->state == TOC_PARSER_STATE_language_3)
	{
		switch (token)
		{
			case TOC_TOKEN_string:
				if ((toc_parser->parser_destination) && (!*toc_parser->parser_destination))
				{
					*toc_parser->parser_destination = strdup (src);
				}
				toc_parser->state = TOC_PARSER_STATE_language_2;
				return 0;
			case TOC_TOKEN_open:
				toc_parser->state = TOC_PARSER_STATE_language_4;
				return 0;
			default:
				return -1;
		}
	}

	if (toc_parser->state == TOC_PARSER_STATE_language_4)
	{
		switch (token)
		{
			case TOC_TOKEN_number:
			case TOC_TOKEN_comma:
				return 0;
			case TOC_TOKEN_close:
				toc_parser->state = TOC_PARSER_STATE_language_2;
				return 0;
			default:
				return -1;
		}
	}

	if (toc_parser->state == TOC_PARSER_STATE_track_0)
	{
		switch (token)
		{
			case TOC_TOKEN_AUDIO:          toc_parser->track_data[toc_parser->track].storage_mode = AUDIO;          break; /* 2352 bytes of raw audio data */
			case TOC_TOKEN_MODE1:          toc_parser->track_data[toc_parser->track].storage_mode = MODE1;          break; /* 2048 bytes of data */
			case TOC_TOKEN_MODE1_RAW:      toc_parser->track_data[toc_parser->track].storage_mode = MODE1_RAW;      break; /* 2352 bytes of raw data */
			case TOC_TOKEN_MODE2:          toc_parser->track_data[toc_parser->track].storage_mode = MODE2;          break; /* 2336 bytes of data - special sector size known as XA */
			case TOC_TOKEN_MODE2_FORM1:    toc_parser->track_data[toc_parser->track].storage_mode = MODE2_FORM1;    break; /* 2048 bytes of data - different wrapper, not visible for filesystem layer */
			case TOC_TOKEN_MODE2_FORM2:    toc_parser->track_data[toc_parser->track].storage_mode = MODE2_FORM2;    break; /* 2324 bytes of data - different wrapper, special sector size */
			case TOC_TOKEN_MODE2_FORM_MIX: toc_parser->track_data[toc_parser->track].storage_mode = MODE2_FORM_MIX; break; /* 2336 XA MODE-FORM1/2, includes the prefix SUBHEADER - different wrapper and two possible sector sizes */
			case TOC_TOKEN_MODE2_RAW:      toc_parser->track_data[toc_parser->track].storage_mode = MODE2_RAW;      break; /* 2352 bytes of raw data */
			default: return -1;
		}
		toc_parser->state = TOC_PARSER_STATE_track_1;
		return 0;
	}

	if (toc_parser->state == TOC_PARSER_STATE_track_1)
	{
		switch (token)
		{
			case TOC_TOKEN_RW_RAW: toc_parser->track_data[toc_parser->track].storage_mode_subchannel = SUBCHANNEL_RW_RAW; toc_parser->state = TOC_PARSER_STATE_ready; return 0;
			case TOC_TOKEN_RW:     toc_parser->track_data[toc_parser->track].storage_mode_subchannel = SUBCHANNEL_RW;     toc_parser->state = TOC_PARSER_STATE_ready; return 0;
			default:
				break;
		}
		toc_parser->state = TOC_PARSER_STATE_ready;
		/* pass-throught */
	}

	if (toc_parser->state == TOC_PARSER_STATE_no)
	{
		switch (token)
		{
			case TOC_TOKEN_COPY: break;
			case TOC_TOKEN_PRE_EMPHASIS: break;
			default: return -1;
		}
		toc_parser->state = TOC_PARSER_STATE_ready;
		return 0;
	}

	if (toc_parser->state == TOC_PARSER_STATE_isrc)
	{
		if (token != TOC_TOKEN_string)
		{
			return -1;
		}
		toc_parser->state = TOC_PARSER_STATE_ready;
		return 0;
	}

	if (toc_parser->state == TOC_PARSER_STATE_zero)
	{
		if (token == TOC_TOKEN_AUDIO)
		{ /* implied by TRACK */
			return 0;
		}
		if (token == TOC_TOKEN_RW)
		{ /* implied by TRACK */
			return 0;
		}
		if (token == TOC_TOKEN_RW_RAW)
		{ /* implied by TRACK */
			return 0;
		}
		if (token != TOC_TOKEN_msf)
		{
			return -1;
		}
		if (toc_parser_append_source (toc_parser, 0))
		{
			return -1;
		}
		toc_parser_modify_length (toc_parser, (src[0]-'0') * 75*60*10 +
		                                      (src[1]-'0') * 75*60    +
		                                      (src[3]-'0') * 75   *10 +
		                                      (src[4]-'0') * 75       +
		                                      (src[6]-'0') *       10 +
		                                      (src[7]-'0'));
		toc_parser->state = TOC_PARSER_STATE_ready;
		return 0;
	}

	if (toc_parser->state == TOC_PARSER_STATE_audiofile_0)
	{
		if (token != TOC_TOKEN_string)
		{
			return -1;
		}
		if (toc_parser_append_source (toc_parser, src))
		{
			return -1;
		}
		toc_parser->state = TOC_PARSER_STATE_audiofile_1;
		return 0;
	}

	if (toc_parser->state == TOC_PARSER_STATE_audiofile_1)
	{
		switch (token)
		{
			case TOC_TOKEN_SWAP:
				toc_parser_modify_SWAP (toc_parser);
				return 0;
			case TOC_TOKEN_offset:
				toc_parser_modify_offset (toc_parser, src);
				return 0;
			case TOC_TOKEN_number:
				toc_parser_modify_offset_sector (toc_parser, (uint64_t)atoi(src));
				toc_parser->state = TOC_PARSER_STATE_audiofile_2;
				return 0;
			case TOC_TOKEN_msf:
				toc_parser_modify_offset_sector (toc_parser, (src[0]-'0') * 75*60*10 +
			                                                     (src[1]-'0') * 75*60    +
			                                                     (src[3]-'0') * 75   *10 +
			                                                     (src[4]-'0') * 75       +
			                                                     (src[6]-'0')        *10 +
			                                                     (src[7]-'0'));
				toc_parser->state = TOC_PARSER_STATE_audiofile_2;
				return 0;
			default:
				return -1;
		}
	}

	if (toc_parser->state == TOC_PARSER_STATE_audiofile_2)
	{
		if (token == TOC_TOKEN_msf)
		{
			toc_parser_modify_length (toc_parser, (src[0]-'0') * 75*60*10 +
		                                              (src[1]-'0') * 75*60    +
		                                              (src[3]-'0') * 75   *10 +
		                                              (src[4]-'0') * 75       +
		                                              (src[6]-'0')        *10 +
		                                              (src[7]-'0'));
			toc_parser->state = TOC_PARSER_STATE_ready;
			return 0;
		}
		toc_parser->state = TOC_PARSER_STATE_ready;
		/* fall-through */
	}

	if (toc_parser->state == TOC_PARSER_STATE_datafile_0)
	{
		if (token != TOC_TOKEN_string)
		{
			return -1;
		}
		if (toc_parser_append_source (toc_parser, src))
		{
			return -1;
		}
		toc_parser->state = TOC_PARSER_STATE_datafile_1;
		return 0;
	}

	if (toc_parser->state == TOC_PARSER_STATE_datafile_1)
	{
		switch (token)
		{
			case TOC_TOKEN_offset:
				toc_parser_modify_offset (toc_parser, src);
				return 0;
			case TOC_TOKEN_msf:
				toc_parser_modify_length (toc_parser, (src[0]-'0') * 75*60*10 +
			                                              (src[1]-'0') * 75*60    +
			                                              (src[3]-'0') * 75   *10 +
			                                              (src[4]-'0') * 75       +
			                                              (src[6]-'0')        *10 +
			                                              (src[7]-'0'));
				toc_parser->state = TOC_PARSER_STATE_ready;
				return 0;
			default:
				return -1;
		}
	}

	if (toc_parser->state == TOC_PARSER_STATE_start)
	{
		if (token == TOC_TOKEN_msf)
		{
			toc_parser_modify_pregap (toc_parser, (src[0]-'0') * 75*60*10 +
		                                              (src[1]-'0') * 75*60    +
		                                              (src[3]-'0') * 75   *10 +
		                                              (src[4]-'0') * 75       +
		                                              (src[6]-'0')        *10 +
		                                              (src[7]-'0'));
			toc_parser->state = TOC_PARSER_STATE_ready;
			return 0;
		}
		toc_parser->state = TOC_PARSER_STATE_ready;
		/* fall-through */
	}

	if (toc_parser->state == TOC_PARSER_STATE_pregap)
	{
		if (token != TOC_TOKEN_msf)
		{
			return -1;
		}
		if (toc_parser_append_source (toc_parser, 0))
		{
			return -1;
		}
		toc_parser_modify_length (toc_parser, (src[0]-'0') * 75*60*10 +
		                                      (src[1]-'0') * 75*60    +
		                                      (src[3]-'0') * 75   *10 +
		                                      (src[4]-'0') * 75       +
		                                      (src[6]-'0')        *10 +
		                                      (src[7]-'0'));
		toc_parser_modify_pregap (toc_parser, (src[0]-'0') * 75*60*10 +
	                                              (src[1]-'0') * 75*60    +
	                                              (src[3]-'0') * 75   *10 +
	                                              (src[4]-'0') * 75       +
	                                              (src[6]-'0')        *10 +
	                                              (src[7]-'0'));
		toc_parser->state = TOC_PARSER_STATE_ready;
		return 0;
	}

	if (toc_parser->state == TOC_PARSER_STATE_start)
	{
		if (token != TOC_TOKEN_msf)
		{
			return -1;
		}
		toc_parser_append_index (toc_parser, (src[0]-'0') * 75*60*10 +
		                                     (src[1]-'0') * 75*60    +
		                                     (src[3]-'0') * 75   *10 +
		                                     (src[4]-'0') * 75       +
		                                     (src[6]-'0')        *10 +
		                                     (src[7]-'0'));
		toc_parser->state = TOC_PARSER_STATE_ready;
		return 0;
	}

	if (toc_parser->state == TOC_PARSER_STATE_ready)
	{
		switch (token)
		{
			case TOC_TOKEN_CD_DA:
			case TOC_TOKEN_CD_ROM:
			case TOC_TOKEN_CD_ROM_XA: /* we do need the information this keyword provides */
				if (toc_parser->track) return -1;
				return 0;
			case TOC_TOKEN_CATALOG:
				toc_parser->state = TOC_PARSER_STATE_catalog;
				return 0;
			case TOC_TOKEN_CD_TEXT:
				toc_parser->state = TOC_PARSER_STATE_cd_text_0;
				return 0;
			case TOC_TOKEN_TRACK:
				if (toc_parser->track >= 99)
				{
					return -1;
				}
				toc_parser->track++;
				toc_parser->state = TOC_PARSER_STATE_track_0;
				return 0;
			case TOC_TOKEN_LANGUAGE:
				toc_parser->state = TOC_PARSER_STATE_language_0;
				return 0;
			case TOC_TOKEN_NO:
				toc_parser->state = TOC_PARSER_STATE_no;
				return 0;
			case TOC_TOKEN_COPY:
			case TOC_TOKEN_PRE_EMPHASIS: /* we need some examples of this in order to know what to do */
			case TOC_TOKEN_TWO_CHANNEL_AUDIO:
				return 0;
			case TOC_TOKEN_FOUR_CHANNEL_AUDIO:
				toc_parser->track_data[toc_parser->track].four_channel_audio = 1;
				return 0;
			case TOC_TOKEN_ISRC:
				toc_parser->state = TOC_PARSER_STATE_isrc;
				return 0;
			case TOC_TOKEN_SILENCE: /* same as ZERO, but should be used for AUDIO */
			case TOC_TOKEN_ZERO:
				toc_parser->state = TOC_PARSER_STATE_zero;
				return 0;
			case TOC_TOKEN_FILE:
			case TOC_TOKEN_AUDIOFILE:
				toc_parser->state = TOC_PARSER_STATE_audiofile_0;
				return 0;
			case TOC_TOKEN_DATAFILE:
				toc_parser->state = TOC_PARSER_STATE_datafile_0;
				return 0;
			case TOC_TOKEN_FIFO: /* we can not handle these.... */
				return -1;
			case TOC_TOKEN_START:
				toc_parser_modify_pregap (toc_parser, -1); /* default to use the entire data availably until now */
				toc_parser->state = TOC_PARSER_STATE_start;
				return 0;
			case TOC_TOKEN_PREGAP:
				toc_parser->state = TOC_PARSER_STATE_pregap;
				return 0;
			case TOC_TOKEN_INDEX:
				toc_parser->state = TOC_PARSER_STATE_index;
				return 0;
			default:
				return -1;
		}
	}

	return -1;
}

static int toc_check_keyword (const char *input, int l, const char *needle)
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

static void toc_parse_error (const char *orig, const char *input, const char *eol, const int lineno)
{
	int i = 0;
	fprintf (stderr, "Failed to parse .TOC file at line %d\n", lineno + 1);
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

static int _toc_parse_token (struct toc_parser_t *toc_parser, const char *orig, const char *input, const char *eol, const int lineno, enum TOC_tokens token, const char *buffer)
{
	if (toc_parse_token (toc_parser, token, buffer))
	{
		toc_parse_error (orig, input, eol, lineno);
		return -1;
	}
	return 0;
}

static int toc_parse_line (struct toc_parser_t *toc_parser, const char *input, const char *eol, const int lineno)
{
	const char *orig = input;
	char buffer[2048];
	int bufferfill = 0;
	int l = eol - input;

	while (l)
	{
		const char *current = input;
		bufferfill = 0;
		if ((input[0] == ' ') || (input[0] == '\t'))
		{
			input++; l--;
		} else if ((l >= 2) && (input[0] == '/') && (input[1] == '/'))
		{
			/* rest of the line is a comment */
			return 0;
		} else if (input[0] == '\"')
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
					toc_parse_error (orig, current, eol, lineno);
					return -1;
				}
				buffer[bufferfill++] = toadd;
			}
			buffer[bufferfill] = 0;
			if (_toc_parse_token (toc_parser, orig, current, eol, lineno, TOC_TOKEN_string, buffer)) return -1;
		} else if (input[0] == '#')
		{
			buffer[bufferfill++] = input[0];
			input++; l--;
			while (l && isdigit(input[0]))
			{
				if (bufferfill >= (sizeof(buffer)-2))
				{
					toc_parse_error (orig, current, eol, lineno);
					return -1;
				}
				buffer[bufferfill++] = input[0];
				input++; l--;
			}
			buffer[bufferfill] = 0;
			if (_toc_parse_token (toc_parser, orig, current, eol, lineno, TOC_TOKEN_offset, buffer)) return -1;
		} else if (isdigit(input[0]))
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

			     if (d0 && d1 && c2 && d3 && d4 && c5 && d6 && d7) { sprintf (buffer, "%c%c:%c%c:%c%c", input[0], input[1], input[3], input[4], input[6], input[7]); input += 8; l-= 8; if (_toc_parse_token (toc_parser, orig, current, eol, lineno, TOC_TOKEN_msf, buffer)) return -1; } // 12:12:12
			else if (      d0 && c1 && d2 && d3 && c4 && d5 && d6) { sprintf (buffer, "0%c:%c%c:%c%c",            input[0], input[2], input[3], input[5], input[6]); input += 7; l-= 7; if (_toc_parse_token (toc_parser, orig, current, eol, lineno, TOC_TOKEN_msf, buffer)) return -1; } //  1:12:12
			else if (d0 && d1 && c2 &&       d3 && c4 && d5 && d6) { sprintf (buffer, "%c%c:0%c:%c%c",  input[0], input[1],           input[3], input[5], input[6]); input += 7; l-= 7; if (_toc_parse_token (toc_parser, orig, current, eol, lineno, TOC_TOKEN_msf, buffer)) return -1; } // 12: 1:12
			else if (d0 && d1 && c2 && d3 && d4 && c5 &&       d6) { sprintf (buffer, "%c%c:%c%c:0%c",  input[0], input[1], input[3], input[4],           input[6]); input += 7; l-= 7; if (_toc_parse_token (toc_parser, orig, current, eol, lineno, TOC_TOKEN_msf, buffer)) return -1; } // 12:12: 1
			else if (      d0 && c1 &&       d2 && c3 && d4 && d5) { sprintf (buffer, "0%c:0%c:%c%c",             input[0],           input[2], input[4], input[5]); input += 6; l-= 6; if (_toc_parse_token (toc_parser, orig, current, eol, lineno, TOC_TOKEN_msf, buffer)) return -1; } //  1: 1:12
			else if (      d0 && c1 && d2 && d3 && c4 &&       d5) { sprintf (buffer, "0%c:%c%c:0%c",             input[0], input[2], input[3],           input[5]); input += 6; l-= 6; if (_toc_parse_token (toc_parser, orig, current, eol, lineno, TOC_TOKEN_msf, buffer)) return -1; } //  1:12: 1
			else if (d0 && d1 && c2 &&       d3 && c3 &&       d5) { sprintf (buffer, "%c%c:0%c:0%c",   input[0], input[1],           input[3],           input[5]); input += 6; l-= 6; if (_toc_parse_token (toc_parser, orig, current, eol, lineno, TOC_TOKEN_msf, buffer)) return -1; } // 12: 1: 1
			else if (      d0 && c1 &&       d2 && c3 &&       d4) { sprintf (buffer, "0%c:0%c:0%c",              input[0],           input[2],           input[4]); input += 5; l-= 5; if (_toc_parse_token (toc_parser, orig, current, eol, lineno, TOC_TOKEN_msf, buffer)) return -1; } //  1: 1: 1
			else if (                  d0 && d1 && c2 && d3 && d4) { sprintf (buffer, "00:%c%c:%c%c",                       input[0], input[1], input[3], input[4]); input += 5; l-= 5; if (_toc_parse_token (toc_parser, orig, current, eol, lineno, TOC_TOKEN_msf, buffer)) return -1; } //    12:12
			else if (                        d0 && c1 && d2 && d3) { sprintf (buffer, "00:0%c:%c%c",                                  input[0], input[2], input[3]); input += 4; l-= 4; if (_toc_parse_token (toc_parser, orig, current, eol, lineno, TOC_TOKEN_msf, buffer)) return -1; } //     1:12
			else if (                  d0 && d1 && c2 &&       d3) { sprintf (buffer, "00:%c%c:0%c",                                  input[0], input[1], input[3]); input += 4; l-= 4; if (_toc_parse_token (toc_parser, orig, current, eol, lineno, TOC_TOKEN_msf, buffer)) return -1; } //    12: 1
			else if (                        d0 && c1 &&       d2) { sprintf (buffer, "00:0%c:0%c",                                   input[0],           input[2]); input += 3; l-= 3; if (_toc_parse_token (toc_parser, orig, current, eol, lineno, TOC_TOKEN_msf, buffer)) return -1; } //     1: 1
			else {
				while (l && isdigit(input[0]))
				{
					if (bufferfill >= (sizeof(buffer)-2))
					{
						toc_parse_error (orig, current, eol, lineno);
						return -1;
					}
					buffer[bufferfill++] = input[0];
					input++; l--;
				}
				buffer[bufferfill] = 0;
				if (_toc_parse_token (toc_parser, orig, current, eol, lineno, TOC_TOKEN_number, buffer)) return -1;
			}
		} else if (input[0] == '{')
		{
			input++; l--;
			if (_toc_parse_token (toc_parser, orig, current, eol, lineno, TOC_TOKEN_open, "{")) return -1;
		} else if (input[0] == '}')
		{
			input++; l--;
			if (_toc_parse_token (toc_parser, orig, current, eol, lineno, TOC_TOKEN_close, "}")) return -1;
		} else if (input[0] == ':')
		{
			input++; l--;
			if (_toc_parse_token (toc_parser, orig, current, eol, lineno, TOC_TOKEN_colon, ":")) return -1;
		} else if (input[0] == ',')
		{
			input++; l--;
			if (_toc_parse_token (toc_parser, orig, current, eol, lineno, TOC_TOKEN_comma, ",")) return -1;
		} else {
			int i;
			struct TOC_token_keyword_pair d[] =
			{
				{TOC_TOKEN_CATALOG,            "CATALOG"},
				{TOC_TOKEN_CD_DA,              "CD_DA"},
				{TOC_TOKEN_CD_ROM,             "CD_ROM"},
				{TOC_TOKEN_CD_ROM_XA,          "CD_ROM_XA"},
				{TOC_TOKEN_CD_TEXT,            "CD_TEXT"},
				{TOC_TOKEN_LANGUAGE_MAP,       "LANGUAGE_MAP"},
				{TOC_TOKEN_EN,                 "EN"},
				{TOC_TOKEN_LANGUAGE,           "LANGUAGE"},
				{TOC_TOKEN_TITLE,              "TITLE"},
				{TOC_TOKEN_PERFORMER,          "PERFORMER"},
				{TOC_TOKEN_SONGWRITER,         "SONGWRITER"},
				{TOC_TOKEN_COMPOSER,           "COMPOSER"},
				{TOC_TOKEN_ARRANGER,           "ARRANGER"},
				{TOC_TOKEN_MESSAGE,            "MESSAGE"},
				{TOC_TOKEN_DISC_ID,            "DISC_ID"},
				{TOC_TOKEN_GENRE,              "GENRE"},
				{TOC_TOKEN_TOC_INFO1,          "TOC_INFO1"},
				{TOC_TOKEN_TOC_INFO2,          "TOC_INFO2"},
				{TOC_TOKEN_UPC_EAN,            "UPC_EAN"},
				{TOC_TOKEN_SIZE_INFO,          "SIZE_INFO"},
				{TOC_TOKEN_TRACK,              "TRACK"},
				{TOC_TOKEN_AUDIO,              "AUDIO"},
				{TOC_TOKEN_MODE1,              "MODE1"},
				{TOC_TOKEN_MODE1_RAW,          "MODE1_RAW"},
				{TOC_TOKEN_MODE2,              "MODE2"},
				{TOC_TOKEN_MODE2_FORM1,        "MODE2_FORM1"},
				{TOC_TOKEN_MODE2_FORM2,        "MODE2_FORM2"},
				{TOC_TOKEN_MODE2_FORM_MIX,     "MODE2_FORM_MIX"},
				{TOC_TOKEN_MODE2_RAW,          "MODE2_RAW"},
				{TOC_TOKEN_RW,                 "RW"},
				{TOC_TOKEN_RW_RAW,             "RW_RAW"},
				{TOC_TOKEN_NO,                 "NO"},
				{TOC_TOKEN_COPY,               "COPY"},
				{TOC_TOKEN_PRE_EMPHASIS,       "PRE_EMPHASIS"},
				{TOC_TOKEN_TWO_CHANNEL_AUDIO,  "TWO_CHANNEL_AUDIO"},
				{TOC_TOKEN_FOUR_CHANNEL_AUDIO, "FOUR_CHANNEL_AUDIO"},
				{TOC_TOKEN_ISRC,               "ISRC"},
				{TOC_TOKEN_SILENCE,            "SILENCE"},
				{TOC_TOKEN_ZERO,               "ZERO"},
				{TOC_TOKEN_FILE,               "FILE"},
				{TOC_TOKEN_AUDIOFILE,          "AUDIOFILE"},
				{TOC_TOKEN_DATAFILE,           "DATAFILE"},
				{TOC_TOKEN_SWAP,               "SWAP"},
				{TOC_TOKEN_FIFO,               "FIFO"},
				{TOC_TOKEN_START,              "START"},
				{TOC_TOKEN_PREGAP,             "PREGAP"},
				{TOC_TOKEN_INDEX,              "INDEX"}
			};
			for (i=0; i < (sizeof (d) / sizeof (d[0])); i++)
			{
				if (toc_check_keyword (input, l, d[i].s))
				{
					input+=strlen (d[i].s); l-= strlen (d[i].s);
					if (_toc_parse_token (toc_parser, orig, current, eol, lineno, d[i].e, d[i].s)) return -1;
					break;
				}
			}
			if (i != (sizeof (d) / sizeof (d[0])))
			{
				continue;
			}
			toc_parse_error (orig, current, eol, lineno);
			return -1;
		}
	}
	return 0;
}

OCP_INTERNAL void toc_parser_free (struct toc_parser_t *toc_parser)
{
	int i, j;
	for (i = 0; i < 100; i++)
	{
		free (toc_parser->track_data[i].title);
		free (toc_parser->track_data[i].performer);
		free (toc_parser->track_data[i].songwriter);
		free (toc_parser->track_data[i].composer);
		free (toc_parser->track_data[i].arranger);
		free (toc_parser->track_data[i].message);
		for (j = 0; j < toc_parser->track_data[i].datasourceN; j++)
		{
			free (toc_parser->track_data[i].datasource[j].filename);
		}
		free (toc_parser->track_data[i].datasource);
	}
	free (toc_parser);
}

OCP_INTERNAL struct toc_parser_t *toc_parser_from_data (const char *input)
{
	struct toc_parser_t *retval;
	const char *eof = input + strlen (input);

	int rN = 0;
	int rR = 0;

	retval = calloc (sizeof (*retval), 1);
	if (!retval)
	{
		fprintf (stderr, "toc_parser() calloc() failed\n");
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

			if (toc_parse_line (retval, input, eol, rR > rN ? rR : rN))
			{
				toc_parser_free (retval);
				return 0;
			}
		}
		input = eol + 1;
	}

	return retval;
}

OCP_INTERNAL struct cdfs_disc_t *toc_parser_to_cdfs_disc (struct ocpfile_t *parentfile, struct toc_parser_t *toc_parser)
{
	struct cdfs_disc_t *retval = cdfs_disc_new (parentfile);
	int i;
	uint32_t trackoffset = 0; /* start of current track */

	if (!retval)
	{
		fprintf (stderr, "toc_parser_to_cdfs_disc(): cdfs_disc_new() failed\n");
		return 0;
	}

	for (i=0; i <= toc_parser->track; i++)
	{
		uint32_t tracklength = 0; /* in sectors */
		int j;

		for (j=0; j < toc_parser->track_data[i].datasourceN; j++)
		{
			if (!toc_parser->track_data[i].datasource[j].length)
			{ /* ignore zero length entries */
				continue;
			}

			if (!toc_parser->track_data[i].datasource[j].filename)
			{ /* zero-fill */
				if (toc_parser->track_data[i].datasource[j].length < 0)
				{
					fprintf (stderr, "CDFS TOC zero-fill track %d source %d length %"PRId64"\n", i, j, toc_parser->track_data[i].datasource[j].length);
					goto fail_out;
				}

				cdfs_disc_datasource_append (retval,
				                             trackoffset+tracklength,                         /* medium sector offset */
				                             toc_parser->track_data[i].datasource[j].length,  /* medium sector count */
				                             0, 0,                                            /* source file-descriptor */
				                             FORMAT_RAW___NONE,                               /* source sector encoding */
				                             0,                                               /* source byte offset */
				                             0);                                              /* source byte length */

				tracklength += toc_parser->track_data[i].datasource[j].length;
				continue;
			}

			if ((toc_parser->track_data[i].storage_mode == AUDIO) &&
			    (toc_parser->track_data[i].storage_mode_subchannel == NONE) &&
			    wave_filename (toc_parser->track_data[i].datasource[j].filename))
			{
				struct ocpfile_t *file = 0;
				struct ocpfilehandle_t *fh = 0;
				uint64_t offset = 0;
				uint64_t length = 0;
				uint32_t lengthsectors;

				if (wave_openfile (parentfile->parent, toc_parser->track_data[i].datasource[j].filename, &file, &fh, &offset, &length))
				{
					fprintf (stderr, "Failed to open wave file %s (format must be stereo, 16bit, 44100 sample-rate)\n", toc_parser->track_data[i].datasource[j].filename);
					goto fail_out;
				}

				if (toc_parser->track_data[i].datasource[j].offset>=0)
				{
					if (toc_parser->track_data[i].datasource[j].offset >= length)
					{
						fprintf (stderr, "Wave file shorter than offset in .toc file\n");
						if (file) file->unref (file);
						if (fh) fh->unref (fh);
						goto fail_out;
					}
					offset += toc_parser->track_data[i].datasource[j].offset;
					length -= toc_parser->track_data[i].datasource[j].offset;
				}

				lengthsectors = (length + 2352 - 1) / 2352; /* round up to nearest sector */
				if (lengthsectors > toc_parser->track_data[i].datasource[j].length)
				{
					lengthsectors = toc_parser->track_data[i].datasource[j].length;
				}

				cdfs_disc_datasource_append (retval,
				                             trackoffset + tracklength,                       /* medium sector offset */
				                             lengthsectors,                                   /* medium sector count */
				                             file,
				                             fh,
				                             FORMAT_AUDIO_SWAP___NONE,                        /* source sector encoding */
				                             offset,                                          /* source byte offset */
				                             length);                                         /* source byte length */
				tracklength += lengthsectors;
			} else {
				struct ocpfile_t *file = 0;
				struct ocpfilehandle_t *fh = 0;
				uint64_t offset = 0;
				uint64_t length = 0;
				uint32_t lengthsectors;
				int ss;

				if (data_openfile (parentfile->parent, toc_parser->track_data[i].datasource[j].filename, &file, &fh, &length))
				{
					fprintf (stderr, "Failed to open data file %s\n", toc_parser->track_data[i].datasource[j].filename);
					goto fail_out;
				}

				if (toc_parser->track_data[i].datasource[j].offset>=0)
				{
					if (toc_parser->track_data[i].datasource[j].offset >= length)
					{
						fprintf (stderr, "Data file shorter than offset in .toc file\n");
						if (file) file->unref (file);
						if (fh) fh->unref (fh);
						goto fail_out;
					}
					offset += toc_parser->track_data[i].datasource[j].offset;
					length -= toc_parser->track_data[i].datasource[j].offset;
				}

				ss = medium_sector_size (toc_parser->track_data[i].storage_mode,
				                         toc_parser->track_data[i].storage_mode_subchannel);

				lengthsectors = (length + ss - 1) / ss; /* round up to nearest sector */
				if (lengthsectors > toc_parser->track_data[i].datasource[j].length)
				{
					lengthsectors = toc_parser->track_data[i].datasource[j].length;
				}

				cdfs_disc_datasource_append (retval,
				                             trackoffset + tracklength,                       /* medium sector offset */
				                             lengthsectors,                                   /* medium sector count */
				                             file,
				                             fh,
				                             toc_storage_mode_to_cdfs_format (toc_parser->track_data[i].storage_mode,
				                                                              toc_parser->track_data[i].storage_mode_subchannel,
				                                                              toc_parser->track_data[i].datasource[j].swap),
				                                                                              /* source sector encoding */
				                             offset,                                          /* source byte offset */
				                             length);                                         /* source byte length */
				tracklength += lengthsectors;
				if (file) file->unref (file);
				if (fh) fh->unref (fh);
			}
		}

		cdfs_disc_track_append (retval,
		                        toc_parser->track_data[i].pregap,
		                        trackoffset + toc_parser->track_data[i].pregap,
		                        tracklength - toc_parser->track_data[i].pregap,
		                        toc_parser->track_data[i].title,
		                        toc_parser->track_data[i].performer,
		                        toc_parser->track_data[i].songwriter,
		                        toc_parser->track_data[i].composer,
		                        toc_parser->track_data[i].arranger,
		                        toc_parser->track_data[i].message
		                       /* we ignore four_channel_audio - never used in real-life
				        *           [no] copy
		                        *           [no] pre-emphasis
		                        */
		                       );

		trackoffset += tracklength;
	}
	//cdfs_disc_unref (retval);
	return retval;
fail_out:
	cdfs_disc_unref (retval);
	return 0;
}
