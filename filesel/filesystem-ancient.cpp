/* OpenCP Module Player
 * copyright (c) 2021-'24 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Code to decompress via libancient (C++ library)
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


extern "C"
{
#include "../config.h"
}
#include <ancient/ancient.hpp>
#include <stdlib.h>
#include <string.h>
extern "C"
{
#include "../types.h"
#include "cpiface/cpiface.h"
#include "filesystem.h"
#include "filesystem-ancient.h"
#include "filesystem-file-mem.h"
}

#if defined(ANCIENT_DEBUG)
static int do_ancient_debug_print=1;
#endif

#ifdef ANCIENT_DEBUG
#define DEBUG_PRINT(...) do { if (do_ancient_debug_print) { fprintf(stderr, __VA_ARGS__); } } while (0)
#else
#define DEBUG_PRINT(...) {}
#endif

#define ANCIENT_COMPRESSED_INCREMENTS  131072
#define ANCIENT_MAX_COMPRESSED_SIZE   4194304
#define FourCC(str) ((uint32_t)((uint8_t)str[3]) | ((uint8_t)(str[2])<<8) | ((uint8_t)(str[1])<<16) | ((uint8_t)(str[0])<<24))

static struct ocpfilehandle_t *ancient_filehandle_srcbuffer (char *compressionmethod, int compressionmethod_len, struct ocpfilehandle_t *s, char *buffer, int fill)
{
	std::optional<ancient::Decompressor> decompressor;

        try
        {
		char *ptr;
		struct ocpfilehandle_t *retval;

                decompressor.emplace ((const uint8_t *)buffer, fill, true, true);

		if (compressionmethod) snprintf (compressionmethod, compressionmethod_len, "%s", decompressor->getName().c_str());
		DEBUG_PRINT ("Compressed with %s\n", decompressor->getName().c_str());

		auto data = decompressor->decompress (true);
		if (!data.size())
		{
			/* zero length data */
			return 0;
		}

		ptr = (char *)malloc (data.size());
		if (!ptr)
		{
			fprintf (stderr, "malloc() failed\n");
			return 0;
		}
		memcpy (ptr, data.data(), data.size());

		retval = mem_filehandle_open (s->dirdb_ref, ptr, data.size());
		if (!retval)
		{
			free (ptr);
			fprintf (stderr, "malloc() failed\n");
			return 0;
		}
		return retval;
        }
        catch (const ancient::InvalidFormatError&)
        {
		DEBUG_PRINT ("Unknown or invalid compression format in file\n");
                return 0;
        }
        catch (const ancient::VerificationError&)
        {
                DEBUG_PRINT ("Decompressor verification failed\n");
                return 0;
        }
	catch (const std::bad_alloc&)
	{
		fprintf (stderr, "malloc() failed\n");
		return 0;
	}
	return 0;
}

struct ocpfilehandle_t *ancient_filehandle (char *compressionmethod, int compressionmethod_len, struct ocpfilehandle_t *s)
{
	uint32_t magic;

	s->seek_set (s, 0);
	if (ocpfilehandle_read_uint32_be (s, &magic))
	{
		s->seek_set (s, 0);
		return 0;
	}
	s->seek_set (s, 0);

	if ((magic & 0xffU) >= 0x08U &&
            (magic & 0xffU) <= 0x0eU)
	{
		uint8_t byte0 = magic>>24U;
		uint8_t byte1 = magic>>16U;
		uint8_t byte2 = magic>>8U;
		if (byte0 != byte1 &&
		    byte0 != byte2 &&
		    byte1 != byte2)
		{
			goto isancient;
		}
	}

	if (((magic & 0xffffff00U)==FourCC("BZh\0") && (magic & 0xffU) >= '1' && (magic & 0xffU) <= '9') ||
	    ((magic >> 16) == 0x1f8b) ||
	    (magic == FourCC("CrM!")) ||
	    (magic == FourCC("CrM2")) ||
	    (magic == FourCC("Crm!")) ||
	    (magic == FourCC("Crm2")) ||
	    (magic == FourCC("DMS!")) ||
	    (magic == FourCC("\001LOB")) ||
	    (magic == FourCC("\002LOB")) ||
	    (magic == FourCC("ziRC")) ||
	    (magic == FourCC("PP11")) ||
	    (magic == FourCC("PP20")) ||
	    (magic == FourCC("PX20")) ||
	    (magic == FourCC("RNC\001")) ||
	    (magic == FourCC("RNC\002")) ||
	    (magic >= 0x08090a08U && magic <=0x08090a0eU && magic != 0x08090a09U) ||
	    (magic == FourCC("S300")) ||
	    (magic == FourCC("S310")) ||
	    (magic == FourCC("S400")) ||
	    (magic == FourCC("S401")) ||
	    (magic == FourCC("S403")) ||
	    (magic == FourCC("S404")) ||
	    (magic == FourCC("TPWM")) ||
	    (magic == FourCC("XPKF")) ||
	    ((magic >> 16) == 0x1fff) ||    /* Compact,     added in v2.1.0 src/CompactDecompressor.cpp */
	    ((magic >> 16) == 0x1f9d) ||    /* Compress,    added in v2.1.0 src/CompressDecompressor.cpp */
	    ((magic >> 16) == 0x1f9e) ||    /* Freeze,      added in v2.1.0 src/FreezeDecompressor.cpp */
	    ((magic >> 16) == 0x1f9f) ||    /* Freeze,      added in v2.1.0 src/FreezeDecompressor.cpp */
	    (magic == FourCC("\001LOB")) || /* LOB,         added in v2.1.0 src/LOBDecompressor.cpp */
	    (magic == FourCC("\002LOB")) || /* LOB,         added in v2.1.0 src/LOBDecompressor.cpp */
	    (magic == FourCC("\003LOB")) || /* LOB,         added in v2.1.0 src/LOBDecompressor.cpp */
	    (magic == FourCC("PX20")) ||    /* PP varient,  added in v2.1.0 src/PPDecompressor.cpp */
	    (magic == FourCC("PPMQ")) ||    /* PPMQ,        added in v2.1.0 src/PPMQDecompressor.cpp */
	    ((magic >> 16) == 0x1f1e) ||    /* Pack,        added in v2.1.0 src/PackDecompressor.cpp */
	    ((magic >> 16) == 0x1f1f) ||    /* Pack,        added in v2.1.0 src/PackDecompressor.cpp */
	    ((magic >> 16) == 0x1fa0) ||    /* SCOCompress, added in v2.1.0 src/SCOCompressDecompressor.cpp */
	    (magic == FourCC("SHR3")) ||                   /*                                   will be added in v2.2.0 src/SHR3Decompressor.cpp */
	    (magic == FourCC("SHRI")) ||                   /*                                   will be added in v2.2.0 src/src/SHRXDecompressor.cpp */
	    ((magic & 0xffffff00) == FourCC("1AM\000")) || /* Reunion,                          will be added in v2.2.0 src/StoneCrackerDecompressor.cpp */
	    ((magic & 0xffffff00) == FourCC("2AM\000")) || /* Reunion,                          will be added in v2.2.0 src/StoneCrackerDecompressor.cpp */
	    (magic == FourCC("Z&G!")) ||                   /* Switchback / Rebels,              will be added in v2.2.0 src/StoneCrackerDecompressor.cpp */
	    (magic == FourCC("ZULU")) ||                   /* Whammer Slammer / Rebels,         will be added in v2.2.0 src/StoneCrackerDecompressor.cpp */
	    (magic == FourCC("AYS!")) ||                   /* High Anxiety / Abyss,             will be added in v2.2.0 src/StoneCrackerDecompressor.cpp */
	    (magic == 0x18051973) ||                       /* Fears,                            will be added in v2.2.0 src/CRMDecompressor.cpp */
	    (magic == FourCC("CD\xb3\xb9")) ||             /* BiFi 2,                           will be added in v2.2.0 src/CRMDecompressor.cpp */
	    (magic == FourCC("DCS!")) ||                   /* Sonic Attack/DualCrew-Shining,    will be added in v2.2.0 src/CRMDecompressor.cpp */
	    (magic == FourCC("Iron")) ||                   /* Sun / TRSI,                       will be added in v2.2.0 src/CRMDecompressor.cpp */
	    (magic == FourCC("MSS!")) ||                   /* Infection / Mystic,               will be added in v2.2.0 src/CRMDecompressor.cpp */
	    (magic == FourCC("mss!")) ||                   /*                                   will be added in v2.2.0 src/CRMDecompressor.cpp */
	    (magic == FourCC("CHFC")) ||                   /* Sky High Stuntman,                will be added in v2.2.0 src/PPDecompressor.cpp */
	    (magic == FourCC("DEN!")) ||                   /* Jewels - Crossroads,              will be added in v2.2.0 src/PPDecompressor.cpp */
	    (magic == FourCC("DXS9")) ||                   /* Hopp oder Top, Punkt Punkt Punkt, will be added in v2.2.0 src/PPDecompressor.cpp */
	    (magic == FourCC("H.D.")) ||                   /* F1 Challenge,                     will be added in v2.2.0 src/PPDecompressor.cpp */
	    (magic == FourCC("RVV!")) ||                   /* Hoi AGA Remix,                    will be added in v2.2.0 src/PPDecompressor.cpp */
	    (magic == FourCC("...\001")) ||                /* Total Carnage,                    will be added in v2.2.0 src/RNCDecompressor.cpp */
	    (magic == FourCC("Vice")) ||                   /*                                   will be added in v2.2.0 src/VicXDecompressor.cpp */
	    (magic == FourCC("Vic2"))                      /*                                   will be added in v2.2.0 src/VicXDecompressor.cpp */
	   )
	{
isancient:
		char *buffer = NULL;
		unsigned int fill = 0;
		unsigned int size = 0;
		do
		{
			if (size >= ANCIENT_MAX_COMPRESSED_SIZE)
			{
				free (buffer);
				s->seek_set (s, 0);
				DEBUG_PRINT ("File too large to decompress (limit is %d bytes)\n", ANCIENT_MAX_COMPRESSED_SIZE);
				return 0;
			}

			size += ANCIENT_COMPRESSED_INCREMENTS;
			if (size > ANCIENT_MAX_COMPRESSED_SIZE) size = ANCIENT_MAX_COMPRESSED_SIZE;

			char *temp = (char *)realloc (buffer, size);
			if (!temp)
			{
				free (buffer);
				s->seek_set (s, 0);
				fprintf (stderr, "malloc() failed\n");
				return 0;
			}
			buffer = temp;

			fill += s->read (s, buffer + fill, size - fill);
			if (s->eof(s))
			{
				break;
			}
		} while (1);

		struct ocpfilehandle_t *retval = ancient_filehandle_srcbuffer (compressionmethod, compressionmethod_len, s, buffer, fill);
		free (buffer);
		s->seek_set (s, 0);
		return retval;
	}

	return 0;
}
