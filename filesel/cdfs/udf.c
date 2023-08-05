/* OpenCP Module Player
 * copyright (c) 2022-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Parsing of UDF filesystems
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
#include <assert.h>
#include <ctype.h>
#include <iconv.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "types.h"

#include "cdfs.h"
#include "main.h"
#include "udf.h"

#ifdef CDFS_DEBUG
#define debug_printf(...) fprintf (stderr, __VA_ARGS__)
#else
#define debug_printf(format,args...) ((void)0)
#endif

#define MAX_INDIRECT_RECURSION 1024

static struct UDF_LogicalVolume_Common *UDF_GetLogicalPartition (struct cdfs_disc_t *disc, uint16_t PartId);
static struct UDF_PhysicalPartition_t *UDF_GetPhysicalPartition (struct cdfs_disc_t *disc, uint16_t PartId);

static void SequenceRawdisk (int n, struct cdfs_disc_t *disc, struct UDF_extent_ad *L, void (*Handler)(int n, struct cdfs_disc_t *disc, struct UDF_Partition_Common *PartitionCommon, uint32_t TagLocation, uint8_t *buffer, uint32_t bufferlen, void *userpointer), void *userpointer);

static void TerminatingDescriptor (int n, struct cdfs_disc_t *disc, uint8_t *buffer);

static void VolumeDescriptorSequence (int n, struct cdfs_disc_t *disc, struct UDF_Partition_Common *PartitionCommon, uint32_t TagLocation, uint8_t *buffer, uint32_t bufferlen, void *userpointer);
static void LogicalVolumeIntegritySequence (int n, struct cdfs_disc_t *disc, struct UDF_Partition_Common *PartitionCommon, uint32_t TagLocation, uint8_t *buffer, uint32_t bufferlen, void *userpointer);

static void UDF_Session_Add_PrimaryVolumeDescriptor (struct cdfs_disc_t *disc, uint32_t VolumeDescriptorSequenceNumber, char *VolumeIdentifier, uint16_t VolumeSequenceNumber);

static void UDF_Session_Add_PhysicalPartition (struct cdfs_disc_t *disc, uint32_t VolumeDescriptorSequenceNumber, uint16_t PartitionNumber, enum PhysicalPartition_Content Content, uint32_t SectorSize, uint32_t Start, uint32_t Length);

static struct UDF_LogicalVolumes_t *UDF_LogicalVolumes_Create (uint32_t VolumeDescriptorSequenceNumber, char *LogicalVolumeIdentifier, uint8_t DescriptorCharacterSet[64]);

static void UDF_LogicalVolume_FileSetDescriptor_SetLocation (struct UDF_LogicalVolumes_t *self, uint32_t FileSetDescriptor_LogicalBlockNumber, uint16_t FileSetDescriptor_PartitionReferenceNumber);

static void UDF_LogicalVolume_Append_Type1 (struct UDF_LogicalVolumes_t *self, uint16_t PartId, uint16_t PhysicalPartition_VolumeSequenceNumber, uint16_t PhysicalPartition_PartitionNumber);
static void UDF_LogicalVolume_Append_Type2_VAT (struct UDF_LogicalVolumes_t *self, uint16_t PartId, uint16_t PhysicalPartition_VolumeSequenceNumber, uint16_t PhysicalPartition_PartitionNumber);
static void UDF_LogicalVolume_Append_Type2_SparingPartition (struct UDF_LogicalVolumes_t *self, uint16_t, uint16_t VolumeSequenceNumber, uint16_t PartitionNumber, uint16_t PacketLength, uint8_t NumberOfSparingTables, uint32_t SizeOfEachSparingTable, uint32_t *SparingTableLocations);
static void UDF_LogicalVolume_Append_Type2_Metadata (struct UDF_LogicalVolumes_t *self, uint16_t PartId, uint16_t VolumeSequenceNumber, uint16_t PartitionNumber, uint32_t MetadataFileLocation, uint32_t MetadataMirrorFileLocation, uint32_t MetadataBitmapFileLocation, uint32_t AllocationUnitSize, uint32_t AlignmentUnitSize, uint8_t Flags);

static void UDF_LogicalVolumes_Free (struct UDF_LogicalVolumes_t *self);

static void UDF_Session_Set_LogicalVolumes (struct cdfs_disc_t *disc, struct UDF_LogicalVolumes_t *LogicalVolumes);

static void ExtendedAttributesCommon (int n, uint8_t *b, uint32_t l, uint32_t TagLocation, int isfile, struct UDF_FileEntry_t *extendedattributes_target);
static void ExtendedAttributesInline (int n, uint8_t *buffer, uint32_t ExtentLocation, uint32_t ExtentLength, int isfile, struct UDF_FileEntry_t *extendedattributes_target);
static void ExtendedAttributes (int n, struct cdfs_disc_t *disc, struct UDF_longad *L, int isfile, struct UDF_FileEntry_t *extendedattributes_target);

static void N(int n)
{
	int i;
	for (i=0; i < n; i++)
	{
		debug_printf(" ");
	}
}

#ifdef CDFS_DEBUG
static const char *TagIdentifierName(const uint16_t TagIdentifier)
{
	switch (TagIdentifier)
	{
		case 0x0000: return "Sparing Table layout";                 // Not part of ECMA 167 - only UDF x.xx
		case 0x0001: return "Primary Volume Descriptor";            // ECMA 167 - Part 3: Volume Structure 7.2.1  (3/10.1)
		case 0x0002: return "Anchor Volume Descriptor Pointer";     // ECMA 167 - Part 3: Volume Structure 7.2.1  (3/10.2)
		case 0x0003: return "Volume Descriptor Pointer";            // ECMA 167 - Part 3: Volume Structure 7.2.1  (3/10.3)
		case 0x0004: return "Implementation Use Volume Descriptor"; // ECMA 167 - Part 3: Volume Structure 7.2.1  (3/10.4)
		case 0x0005: return "Partition Descriptor";                 // ECMA 167 - Part 3: Volume Structure 7.2.1  (3/10.5)
		case 0x0006: return "Logical Volume Descriptor";            // ECMA 167 - Part 3: Volume Structure 7.2.1  (3/10.6)
		case 0x0007: return "Unallocated Space Descriptor";         // ECMA 167 - Part 3: Volume Structure 7.2.1  (3/10.8)
		case 0x0008: return "Terminating Descriptor";               // ECMA 167 - Part 3: Volume Structure 7.2.1  (3/10.9) - Part 4: File structure 7.2.1  (4/14.2)
		case 0x0009: return "Logical Volume Integrity Descriptor";  // ECMA 167 - Part 3: Volume Structure 7.2.1  (3/10.10)
		case 0x0100: return "File Set Descriptor";                  // ECMA 167 - Part 4: File structure 7.2.1  (4/14.1)
		case 0x0101: return "File Identifier Descriptor";           // ECMA 167 - Part 4: File structure 7.2.1  (4/14.4)
		case 0x0102: return "Allocation Extent Descriptor";         // ECMA 167 - Part 4: File structure 7.2.1  (4/14.5)
		case 0x0103: return "Indirect Entry";                       // ECMA 167 - Part 4: File structure 7.2.1  (4/14.7)
		case 0x0104: return "Terminal Entry";                       // ECMA 167 - Part 4: File structure 7.2.1  (4/14.8)
		case 0x0105: return "File Entry";                           // ECMA 167 - Part 4: File structure 7.2.1  (4/14.9)
		case 0x0106: return "Extended Attribute Header Descriptor"; // ECMA 167 - Part 4: File structure 7.2.1  (4/14.10.1)
		case 0x0107: return "Unallocated Space Entry";              // ECMA 167 - Part 4: File structure 7.2.1  (4/14.11)
		case 0x0108: return "Space Bitmap Descriptor";              // ECMA 167 - Part 4: File structure 7.2.1  (4/14.12)
		case 0x0109: return "Partition Integrity Entry";            // ECMA 167 - Part 4: File structure 7.2.1  (4/14.13)
		case 0x010a: return "Extended File Entry";                  // ECMA 167 - Part 4: File structure 7.2.1  (4/14.17)
		default: return "Unknown";
	}
}
#endif

static void print_1_7_2_1 (uint8_t buffer[64]) //Character Set Type (RBP 0)
{
	int i;
	switch (buffer[0])
	{
		case 0x00: debug_printf ("CS0 <="); break; // any character set
		case 0x01: debug_printf ("CS1 <="); break; // the whole or any subset of the graphic characters specified by ECMA-6
		case 0x02: debug_printf ("CS2 <="); break; // 38 graphical characters + standard from ECMA 119 Volume Descriptor
		case 0x03: debug_printf ("CS3 <="); break; // the 63 graphic characters of the portable ISO/IEC 9945-1 file name set
		case 0x04: debug_printf ("CS4 <="); break; // the 95 graphic characters of the International Reference Version of ECMA-6
		case 0x05: debug_printf ("CS5 <="); break; // the 191 graphic characters of ECMA-94, Latin Alphabet No. 1
		case 0x06: debug_printf ("CS6 <="); break; // a set of graphic characters that may be identified by ECMA-35 and ECMA-48
		case 0x07: debug_printf ("CS7 <="); break; // a set of graphic characters that may be identified by ECMA-35 and ECMA-48 and, optionally, code extension characters using ECMA-35 and ECMA-48
		case 0x08: debug_printf ("CS8 <="); break; // a set of 53 graphic characters that are highly portable to most personal computers
		default:   debug_printf ("??? <="); break;
	}
	if (buffer[1])
	{
		debug_printf (" \"");
		for (i=1; i < 64; i++)
		{
			if (buffer[i] == 0x00)
			{
				break;
			}
			if ((buffer[i] >= 0x20) && (buffer[i] < 0x7f) && (buffer[i] != '"') && (buffer[i] != '\\'))
			{
				debug_printf ("%c", buffer[i]);
			} else {
				debug_printf ("\\x%02x", buffer[i]);
			}
		}
		debug_printf ("\"");
	}
}

#ifdef CDFS_DEBUG
static unsigned int unhex (uint8_t i)
{
	if ((i >= '0') && (i <= '9')) return i - '0';
	if ((i >= 'a') && (i <= 'f')) return i - 'a' + 10;
	if ((i >= 'A') && (i <= 'F')) return i - 'A' + 10;
	return 0;
}
#endif

static int ishex (uint8_t i)
{
	if ((i >= '0') && (i <= '9')) return 1;
	if ((i >= 'a') && (i <= 'f')) return 1;
	if ((i >= 'A') && (i <= 'F')) return 1;
	return 0;
}


// Special case
static void print_1_7_2_12_VolumeSetIdentifier2 (uint8_t *buffer, int len)
{
	/* known broken implementation */
	if (len > 16)
	{
		if (isdigit(buffer[0]) &&
		    isdigit(buffer[1]) &&
		    isdigit(buffer[2]) &&
		    isdigit(buffer[3]) &&
		    isdigit(buffer[4]) &&
		    isdigit(buffer[5]) &&
		    isdigit(buffer[6]) &&
		    isdigit(buffer[7]) &&
		    (buffer[8] == 0) &&
		    (buffer[9] == 0) &&
		    (buffer[10] == 0) &&
		    (buffer[11] == 0) &&
		    (buffer[12] == 0) &&
		    (buffer[13] == 0) &&
		    (buffer[14] == 0) &&
		    (buffer[15] == 0))
		{
			debug_printf ("unique_timestamp=%c%c%c%c-%c%c-%c%c",
				buffer[0],
				buffer[1],
				buffer[2],
				buffer[3],
				buffer[4],
				buffer[5],
				buffer[6],
				buffer[7]);
			return;
		}
	}

	/* known broken implementation */
	if ((len == 23) && !strncmp ((char *)buffer + 8, " UDF Volume Set", 15) &&
	    ishex(buffer[0]) &&
	    ishex(buffer[1]) &&
	    ishex(buffer[2]) &&
	    ishex(buffer[3]) &&
	    ishex(buffer[4]) &&
	    ishex(buffer[5]) &&
	    ishex(buffer[6]) &&
	    ishex(buffer[7]))
	{
		goto skipme;
	}

#ifdef CDFS_DEBUG
	if (len >= 16)
	{
		uint32_t Stamp;
		time_t t1;
		struct tm t2;


		Stamp  = unhex(buffer[7]) << 28;
		Stamp |= unhex(buffer[6]) << 24;
		Stamp |= unhex(buffer[5]) << 20;
		Stamp |= unhex(buffer[4]) << 16;
		Stamp |= unhex(buffer[3]) << 12;
		Stamp |= unhex(buffer[2]) <<  8;
		Stamp |= unhex(buffer[1]) <<  4;
		Stamp |= unhex(buffer[0]);
		t1 = Stamp;

		if (!localtime_r (&t1, &t2))
		{
			memset (&t2, 0, sizeof (t2));
		}

		debug_printf ("unique_timestamp=%04d-%02d-%02d_%02d:%02d:%02d",
			t2->tm_year + 1900,
			t2->tm_mon + 1,
			t2->tm_mday,
			t2->tm_hour,
			t2->tm_min,
			t2->tm_sec);
		debug_printf (" ");
		buffer += 8;
		len -= 8;
	}
#endif

skipme:
	if ((len >= 8) &&
	    ishex(buffer[0]) &&
	    ishex(buffer[1]) &&
	    ishex(buffer[2]) &&
	    ishex(buffer[3]) &&
	    ishex(buffer[4]) &&
	    ishex(buffer[5]) &&
	    ishex(buffer[6]) &&
	    ishex(buffer[7]))
	{
		int i;
		debug_printf ("unique_id=0x");
		for (i=0; i< 8; i++)
		{
			debug_printf ("%c", buffer[i]);
		}
		debug_printf (" ");
		buffer += 8;
		len -= 8;
	}
	if (len)
	{
		debug_printf (" \"%.*s\"", len, buffer);
	}
}

static void print_1_7_2_12_VolumeSetIdentifier (uint8_t *buffer, uint8_t len, uint8_t *encoding)
{
	int rlen = buffer[len-1];
	int i;

	if (!memcmp (encoding, "\x00OSTA Compressed Unicode", 25)) /* This comes from OSTA UDF 2.60 documentation */
	{
		switch (buffer[0])
		{
			case 0:
			{
				debug_printf ("(null)");
				break;
			}

			case 8: /* UTF8 */
			{
				print_1_7_2_12_VolumeSetIdentifier2 (buffer + 1, rlen - 1);
				break;
			}

			case 16: /* UTF-16BE */
			{
				uint8_t outbuffer[255*4]; /* maximum 128 UTF16BE codepoints, 4 is the maxlength of a codepoint in UTF-8 */
				char *inbuf = (char *)buffer + 1;
				size_t inbytesleft = rlen - 1;
				char *outbuf = (char *)outbuffer;
				size_t outbytesleft = sizeof (outbuffer);
				//size_t res;

				/* res = */ iconv (UTF16BE_cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);

				debug_printf ("%.*s", (int)((uint8_t *)outbuf - outbuffer), outbuffer);
				print_1_7_2_12_VolumeSetIdentifier2 (outbuffer, (uint8_t *)outbuf - outbuffer);
				break;
			}

			case 254: /* UTF-8 empty string */
			case 255: /* UTF-16BE empty string */
				print_1_7_2_12_VolumeSetIdentifier2 ((uint8_t *)"", 0);
				break;

			default:
				debug_printf ("WARNING - Invalid OSTA Compression Unicode prefix: 0x%02" PRIx8 " ", buffer[0]);
				goto fallback;
		}
	} else {
fallback: /* The not recommented path according to OSTA */
		/* fallback to ASCII / what-ever */
		debug_printf ("\"");
		for (i=0; i < rlen; i++)
		{
			if ((buffer[i] >= 0x20) && (buffer[i] < 0x7f) && (buffer[i] != '"') && (buffer[i] != '\\'))
			{
				debug_printf ("%c",  buffer[i]);
			} else {
				debug_printf ("\\x%02x", buffer[i]);
			}
		}
		debug_printf ("\"");
	}
}

static void print_1_7_2 (uint8_t *buffer, uint8_t rlen, uint8_t *encoding, char **output) // Fixed-length character fields
{
	int i;

	if (!memcmp (encoding, "\x00OSTA Compressed Unicode", 25)) /* This comes from OSTA UDF 2.60 documentation */
	{
		switch (buffer[0])
		{
			case 0:
			{
				debug_printf ("(null)");
				if (output)
				{
					*output = 0;
				}
				break;
			}

			case 8: /* UTF8 */
			{
				debug_printf ("\"%.*s\"", rlen - 1, buffer + 1);

				if (output)
				{
					*output = malloc (rlen);
					memcpy (*output, buffer + 1, rlen - 1);
					(*output)[rlen-1] = 0;
				}
				break;
			}

			case 16: /* UTF-16BE */
			{
				uint8_t outbuffer[255*4]; /* maximum 128 UTF16BE codepoints, 4 is the maxlength of a codepoint in UTF-8 */
				char *inbuf = (char *)buffer + 1;
				size_t inbytesleft = rlen - 1;
				char *outbuf = (char *)outbuffer;
				size_t outbytesleft = sizeof (outbuffer);
				//size_t res;

				/* res = */ iconv (UTF16BE_cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);

				debug_printf ("\"%.*s\"", (int)((uint8_t *)outbuf - outbuffer), outbuffer);

				if (output)
				{
					*output = malloc (((uint8_t *)outbuf - outbuffer) + 1);
					memcpy (*output, outbuffer, (uint8_t *)outbuf - outbuffer);
					(*output)[(uint8_t *)outbuf - outbuffer] = 0;
				}
				break;
			}

			case 254: /* UTF-8 empty string */
			case 255: /* UTF-16BE empty string */
				debug_printf ("\"\"");
				if (output)
				{
					*output = strdup ("");
				}
				break;

			default:
				debug_printf ("WARNING - Invalid OSTA Compression Unicode prefix: %02" PRIx8 " ", buffer[0]);
				goto fallback;
		}
	} else {
fallback: /* The not recommented path according to OSTA */
		/* fallback to ASCII / what-ever */
		debug_printf ("\"");
		for (i=0; i < rlen; i++)
		{
			if ((buffer[i] >= 0x20) && (buffer[i] < 0x7f) && (buffer[i] != '"') && (buffer[i] != '\\'))
			{
				debug_printf ("%c", buffer[i]);
			} else {
				debug_printf ("\\x%02x", buffer[i]);
			}
		}
		debug_printf ("\"");

		if (output)
		{
			*output = malloc (rlen + 1);
			memcpy (*output, buffer, rlen);
			(*output)[rlen] = 0;
		}
	}
}

static void print_1_7_2_12 (uint8_t *buffer, uint8_t len, uint8_t *encoding, char **output) // Fixed-length character fields
{
	int rlen = buffer[len-1];
	int i;

	if (rlen >= len)
	{
		debug_printf ("(WARNING: length overflow)");
		rlen = len - 1;
	}

	if (!memcmp (encoding, "\x00OSTA Compressed Unicode", 25)) /* This comes from OSTA UDF 2.60 documentation */
	{
		switch (buffer[0])
		{
			case 0:
			{
				debug_printf ("(null)");
				if (output)
				{
					*output = 0;
				}
				break;
			}

			case 8: /* UTF8 */
			{
				debug_printf ("\"%.*s\"", rlen - 1, buffer + 1);

				if (output)
				{
					*output = malloc (rlen);
					memcpy (*output, buffer + 1, rlen - 1);
					(*output)[rlen-1] = 0;
				}
				break;
			}

			case 16: /* UTF-16BE */
			{
				uint8_t outbuffer[255*4]; /* maximum 128 UTF16BE codepoints, 4 is the maxlength of a codepoint in UTF-8 */
				char *inbuf = (char *)buffer + 1;
				size_t inbytesleft = rlen - 1;
				char *outbuf = (char *)outbuffer;
				size_t outbytesleft = sizeof (outbuffer);
				//size_t res;

				/* res = */ iconv (UTF16BE_cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);

				debug_printf ("\"%.*s\"", (int)((uint8_t *)outbuf - outbuffer), outbuffer);

				if (output)
				{
					*output = malloc (((uint8_t *)outbuf - outbuffer) + 1);
					memcpy (*output, outbuffer, (uint8_t *)outbuf - outbuffer);
					(*output)[(uint8_t *)outbuf - outbuffer] = 0;
				}
				break;
			}

			case 254: /* UTF-8 empty string */
			case 255: /* UTF-16BE empty string */
				debug_printf ("\"\"");
				if (output)
				{
					*output = strdup ("");
				}
				break;

			default:
				debug_printf ("WARNING - Invalid OSTA Compression Unicode prefix: %02" PRIx8 " ", buffer[0]);
				goto fallback;
		}
	} else {
fallback: /* The not recommented path according to OSTA */
		/* fallback to ASCII / what-ever */
		debug_printf ("\"");
		for (i=0; i < rlen; i++)
		{
			if ((buffer[i] >= 0x20) && (buffer[i] < 0x7f) && (buffer[i] != '"') && (buffer[i] != '\\'))
			{
				debug_printf ("%c", buffer[i]);
			} else {
				debug_printf ("\\x%02x", buffer[i]);
			}
		}
		debug_printf ("\"");

		if (output)
		{
			*output = malloc (rlen + 1);
			memcpy (*output, buffer, rlen);
			(*output)[rlen] = 0;
		}
	}
}

static void print_1_7_3 (uint8_t buffer[12]) // Time Stamp
{
	uint16_t TypeTimeZone = (buffer[1] << 8) | buffer[0];

	debug_printf ("%04d-%02d-%02d %02d:%02d:%02d.%02d.%02d.%02d",
		(buffer[3]<<8) | buffer[2], /* year */
		buffer[4],                  /* month */
		buffer[5],                  /* day */
		buffer[6],                  /* hour */
		buffer[7],                  /* minute */
		buffer[8],                  /* seconds */
		buffer[9],                  /* centiseconds */
		buffer[10],                 /* hundreds of microseconds */
		buffer[11]);                /* microseconds */

	switch (TypeTimeZone >> 12)
	{
		case 0: debug_printf ("UTC"); return; /* Not allowed according to OSTA */
		case 1: /* OSTA requires this path */
		{
			uint16_t a = TypeTimeZone & 0x0fff;
			int b;
			if (a & 0x0800) a |= 0xf000;
			b = (int16_t)a;
			if (b != -2047)
			{
				debug_printf ("%+05d", (b / 60) * 100 + b % 60);
			}
			return;
		}
		//case 2: debug_printf ("agreement???");
		default: debug_printf ("\?\?\?\?(0x%1x 0x%03x)", TypeTimeZone >> 12, TypeTimeZone & 0x0fff); return;
	}
}

#ifdef CDFS_DEBUG
static const char *GetOSClass(uint8_t Class)
{
	switch (Class)
	{
		case 0: return "Undefined";
		case 1: return "DOS";
		case 2: return "OS/2";
		case 3: return "Machintosh OS";
		case 4: return "UNIX";
		case 5: return "Windows 9x";
		case 6: return "Windows NT";
		case 7: return "OS/400";
		case 8: return "BeOS";
		case 9: return "Windows CE";
		default: return "Reserved";
	}
}
#endif

#ifdef CDFS_DEBUG
static const char *GetOSIdentifier(uint8_t Class, uint8_t Identifier)
{
	if (Class == 0)
	{
		return "Undefined";
	} else if (Class == 1)
	{
		if (Identifier == 0)
		{
			return "DOS/Windows 3.x";
		}
	} else if (Class == 2)
	{
		if (Identifier == 0)
		{
			return "OS/2";
		}
	} else if (Class == 3)
	{
		switch (Identifier)
		{
			case 0: return "Macintosh OS 9 and older.";
			case 1: return "Macintosh OS X and later releases.";
		}
	} else if (Class == 4)
	{
		switch (Identifier)
		{
			case 0: return "UNIX - Generic";
			case 1: return "UNIX - IBM AIX";
			case 2: return "UNIX - SUN OS / Solaris";
			case 3: return "UNIX - HP/UX";
			case 4: return "UNIX - Silicon Graphics Irix";
			case 5: return "UNIX - Linux";
			case 6: return "UNIX - MKLinux";
			case 7: return "UNIX - FreeBSD";
			case 8: return "UNIX - NetBSD";
		}
	} else if (Class == 5)
	{
		if (Identifier == 0)
		{
			return "Windows 9x - generic (includes Windows 98/ME)";
		}
	} else if (Class == 6)
	{
		if (Identifier == 0)
		{
			return "Windows NT - generic (includes Windows 2000,XP,Server 2003, and later releases based on the same code base)";
		}
	} else if (Class == 7)
	{
		if (Identifier == 0)
		{
			return "OS/400";
		}
	} else if (Class == 8)
	{
		if (Identifier == 0)
		{
			return "BeOS - generic";
		}
	} else if (Class == 9)
	{
		if (Identifier == 0)
		{
			return "Windows CE - generic";
		}
	}

	return "Reserved";
}
#endif

static void print_1_7_4 (uint8_t buffer[32], const int IsImplementation) // regid
{
#ifdef CDFS_DEBUG
	int i;

	switch (buffer[1])
	{
		case 0x00: debug_printf ("(null)"); return;
		case 0x2b: debug_printf ("(ECMA-167 / ECMA-168) "); break;
		case 0x2d: debug_printf ("(private) "); break;
		default:   debug_printf ("(may be registered according to ISO/IEC 13800)%c ", buffer[1]); break;
	}
	debug_printf ("\"");
	for (i=2; i < 24; i++)
	{
		if (buffer[i] == 0x00)
		{
			break;
		}
		if ((buffer[i] >= 0x20) && (buffer[i] < 0x7f) && (buffer[i] != '"') && (buffer[i] != '\\'))
		{
			debug_printf ("%c", buffer[i]);
		} else {
			debug_printf ("\\x%02x", buffer[i]);
		}
	}
	debug_printf ("\"");

	if (buffer[0] & 0x01) debug_printf (" DIRTY");
	if (buffer[0] & 0x02) debug_printf (" PROTECTED"); else debug_printf (" EDITABLE");

	debug_printf (" \"");
	for (i=24; i < 32; i++)
	{
		if ((buffer[i] >= 0x20) && (buffer[i] < 0x7f) && (buffer[i] != '"') && (buffer[i] != '\\'))
		{
			debug_printf("%c", buffer[i]);
		} else {
			debug_printf ("\\x%02x", buffer[i]);
		}
	}
	debug_printf ("\"");

	/* Test for UDF Identifier Suffix - This is according to OSTA */
	if ((!memcmp (buffer + 1, "*UDF LV Info", 13)) ||
	    (!memcmp (buffer + 1, "*UDF Virtual Partition", 23)) ||
	    (!memcmp (buffer + 1, "*UDF Sparable Partition", 23)) || /* fills the entire space */
	    (!memcmp (buffer + 1, "*UDF Sparing Table", 19)) ||
	    (!memcmp (buffer + 1, "*UDF Metadata Partition", 23)) || /* fills the entire space */
	    (!memcmp (buffer + 1, "*UDF FreeAppEASpace", 20)) ||
	    (!memcmp (buffer + 1, "*UDF FreeEASpace", 17)))
	{ /* UDF Identifier Suffix */
		uint16_t UDF_Revision = (buffer[25] << 8) | buffer[24];
		debug_printf (" UDF_Revision=%d.%d%d", UDF_Revision >> 8, (UDF_Revision & 0xf0) >> 4, UDF_Revision & 0x0f);
		debug_printf (" OS_Class=\"%s\"", GetOSClass (buffer[26]));
		debug_printf (" OS_Identifier=\"%s\"", GetOSIdentifier (buffer[26], buffer[27]));
	} else if ((!memcmp (buffer + 1, "*OSTA UDF Compliant", 20)))
	{ /* Domain Identifier Suffix */

		uint16_t UDF_Revision = (buffer[25] << 8) | buffer[24];
		debug_printf (" UDF_Revision=%d.%d%d", UDF_Revision >> 8, (UDF_Revision & 0xf0) >> 4, UDF_Revision & 0x0f);
		if (buffer[26] & 0x01) debug_printf (" HardWriteProtect");
		if (buffer[26] & 0x02) debug_printf (" SoftWriteProtect");
	} else if (IsImplementation)
	{
		debug_printf (" OS_Class=\"%s\"", GetOSClass (buffer[24]));
		debug_printf (" OS_Identifier=\"%s\"", GetOSIdentifier (buffer[24], buffer[25]));
	}
#endif
}

static void print_4_14_6 (int n, const char *prefix, uint8_t *buffer, uint16_t *Flags, enum eFileType *FileType, int *strategy4096) /* icbtag */
{
	uint16_t StrategyType = (buffer[5] << 8) | buffer[4]; // Type 4096 = WORM, see section 6.6 in OSTA
	uint16_t MaximumNumberofEntries = ((buffer[9] << 8) | buffer[8]); /* should be 1 for Strategy 4, and 2 for Strategy 4096 */

	N(n); debug_printf ("%s.Prior Recorded Number of Direct Entries:        %" PRId32 "\n", prefix, (buffer[3] << 24) | (buffer[2] << 16) | (buffer[1] << 8) | buffer[0]);
	N(n); debug_printf ("%s.Strategy Type:                                  %" PRId16 "\n", prefix, StrategyType); // Type 4096 = WORM, see section 6.6 in OSTA

	(*strategy4096) = (StrategyType==4096);

	switch (StrategyType)
	{
		case 0: N(n+2); debug_printf ("Unspecified - implies direct entry\n"); break;
		case 1: N(n+2); debug_printf ("INVALID - The strategy specificied in ECMA-167 4/A.2 is not valid according to UDF-2.60\n"); break;
		case 2: N(n+2); debug_printf ("INVALID - The strategy specificied in ECMA-167 4/A.3 is not valid according to UDF-2.60\n"); break;
		case 3: N(n+2); debug_printf ("INVALID - The strategy specificied in ECMA-167 4/A.4 is not valid according to UDF-2.60\n"); break;
		case 4: N(n+2); debug_printf ("Recorded as a direct entry (ECMA-167 4/A.5)\n"); break;
		case 4096: N(n+2); debug_printf ("Recorded as a direct entry followed by an possible indirect entry referencing a rewritten version (according to UDF 2.60 section 6.6\n"); break; // not part of ECMA-167 specification break;
		default: N(n+2); debug_printf ("INVALID - Unknown strategy\n"); break;
	}

	N(n); debug_printf ("%s.Strategy Parameter:                             %d %d\n", prefix, buffer[6], buffer[7]);
	N(n); debug_printf ("%s.Maximum Number of Entries:                      %" PRId16 "\n", prefix, MaximumNumberofEntries); /* should be 1 for Strategy 4, and 2 for Strategy 4096 */
	if ((StrategyType == 4) && (MaximumNumberofEntries != 1))
	{
		N(n+2); debug_printf ("WARNING - This value was expected to be 1 for Strategy Type 4\n");
	}
	if ((StrategyType == 4096) && (MaximumNumberofEntries != 2))
	{
		N(n+2); debug_printf ("WARNING - This value was expected to be 2 for Strategy Type 4096\n");
	}
	*FileType = buffer[11];
	N(n); debug_printf ("%s.File Type:                                      %" PRId8 "\n", prefix, *FileType);
	switch (*FileType)
	{
		default:                                     N(n+1); debug_printf ("Unknown\n"); break;
		case FILETYPE_UNSET:                         N(n+1); debug_printf ("Unset\n"); break;
		case FILETYPE_UNSPECIFIED:                   N(n+1); debug_printf ("Unspecified\n"); break;
		case FILETYPE_UNALLOCATED_SPACE_ENTRY:       N(n+1); debug_printf ("Unallocated Space Entry\n"); break; // (see 4/14.11)
		case FILETYPE_PARTITION_INTEGRITY_ENTRY:     N(n+1); debug_printf ("Partition Integrity Entry\n"); break; // (see 4/14.13)
		case FILETYPE_INDIRECT_ENTRY:                N(n+1); debug_printf ("Indirect Entry\n"); break; // (see 4/14.7)
		case FILETYPE_DIRECTORY:                     N(n+1); debug_printf ("Directory\n"); break; // (see 4/14.7) (see 4/8.6)
		case FILETYPE_FILE:                          N(n+1); debug_printf ("File (Random Access)\n"); break;
		case FILETYPE_BLOCK_SPECIAL_DEVICE:          N(n+1); debug_printf ("Block special device\n"); break; // ISO/IEC 9945-1
		case FILETYPE_CHARACTER_SPECIAL_DEVICE:      N(n+1); debug_printf ("Character special device\n"); break; // ISO/IEC 9945-1
		case FILETYPE_RECORDING_EXTENDED_ATTRIBUTES: N(n+1); debug_printf ("Recording Extended Attributes\n"); break; // see  4/9.1
		case FILETYPE_FIFO:                          N(n+1); debug_printf ("FIFO\n"); break; // ISO/IEC 9945-1
		case FILETYPE_C_ISSOCK:                      N(n+1); debug_printf ("C_ISSOCK\n"); break; // ISO/IEC 9945-1
		case FILETYPE_TERMINAL_ENTRY:                N(n+1); debug_printf ("Terminal Entry\n"); break; // (see 4/14.8)
		case FILETYPE_SYMLINK:                       N(n+1); debug_printf ("Symlink\n"); break; // (see 4/8.7)
		case FILETYPE_STREAM_DIRECTORY:              N(n+1); debug_printf ("Stream Directory\n"); break; // (see 4/9.2)

		case FILETYPE_THE_VIRTUAL_ALLOCATED_TABLE:   N(n+1); debug_printf ("The Virtual Allocation Table (VAT)\n"); break; // OSTA 2.2.11
		case FILETYPE_REAL_TIME_FILE:                N(n+1); debug_printf ("Real-Time File"); break; // OSTA Appendix 6.11.1)
		case FILETYPE_METADATA_FILE:                 N(n+1); debug_printf ("Metadata File\n"); break; // OSTA 2.2.13.1
		case FILETYPE_METADATA_MIRROR_FILE:          N(n+1); debug_printf ("Metadata Mirror File\n"); break; // OSTA 2.2.13.1
		case FILETYPE_METADATA_BITMAP_FILE:          N(n+1); debug_printf ("Metadata Bitmap File\n"); break; // OSTA 2.2.13.2
	}
	N(n); debug_printf ("%s.Parent ICB Location.Logical Block Number:       %" PRId32 "\n", prefix, (buffer[15] << 24) | (buffer[14] << 16) | (buffer[13] << 8) | buffer[12]);
	N(n); debug_printf ("%s.Parent ICB Location.Partition Reference Number: %" PRId16 "\n", prefix, (buffer[17] << 8) | buffer[16]);
	*Flags=(buffer[19] << 8) | buffer[18];
	N(n); debug_printf ("%s.Flags:                                          %" PRId16 "\n", prefix, *Flags);
	switch (*Flags & 0x03)
	{
		case 0x00: N(n+1); debug_printf("Short Allocation Descriptors (4/14.14.1) are used\n"); break;
		case 0x01: N(n+1); debug_printf("Long Allocation Descriptors(4/14.14.2) are used\n"); break;
		case 0x02: N(n+1); debug_printf("Extended Allocation Descriptors (4/14.14.3) are used\n");
		case 0x03: N(n+1); debug_printf("File stored directly in extent\n"); break;
		default: N(n+2); debug_printf("WARNING - Illegal store method used\n"); break;
	}
	if ((*FileType == FILETYPE_DIRECTORY) || (*FileType == FILETYPE_STREAM_DIRECTORY))
	{
		if (*Flags & 0x04)
		{
			N(n+1); debug_printf("Entries are sorted according to 4/8.6.1\n");
		} else {
			N(n+1); debug_printf("Entries are NOT sorted according to 4/8.6.1\n");
		}
	}
	if (*Flags & 0x08)
	{
		N(n+1); debug_printf("Not relocatable\n");
	} else {
		N(n+1); debug_printf("Is relocatable\n");
	}
	N(n+1); debug_printf("Archived: %s\n", (*Flags & 0x10) ? "Yes": "No");
	if (*Flags & 0x20)
	{
		N(n+1); debug_printf("S_ISUID (SetUID)\n");
	}
	if (*Flags & 0x40)
	{
		N(n+1); debug_printf("S_ISGID (setGID)\n");
	}
	if (*Flags & 0x80)
	{
		N(n+2); debug_printf("S_ISVTX (Sticky)\n");
	}
	if (*Flags & 0x100)
	{
		N(n+2); debug_printf("S_ISVTX (Sticky)\n");
	}
	N(n+1); debug_printf("Contiguous: %s\n", (*Flags & 0x200) ? "Yes": "No"); // If no, file can contain holes
	//N(n+1); debug_printf("System: %s\n", (*Flags & 0x400) ? "Yes": "No");
	N(n+1); debug_printf("Transformed: %s\n", (*Flags & 0x800) ? "Yes": "No"); // File properties has been converted in a non-reversiable manner
	//N(n+1); debug_printf("Multi-versions: %s\n", (*Flags & 0x1000) ? "Yes": "No"); // Hardlinks
	N(n+1); debug_printf("Stream: %s\n", (*Flags & 0x2000) ? "Yes": "No"); // 4/9.2 TODO
}

static uint16_t crc16(uint8_t *ptr, int count)
{
	uint_fast16_t crc = 0;
	int i;

	while (count)
	{
		crc ^= (*ptr << 8);

		for (i=0; i < 8; i++)
		{
			if (crc & 0x8000)
			{
				crc = crc << 1 ^ 0x1021;
			} else {
				crc = crc << 1;
			}
		}
		ptr++;
		count--;
	}
	return crc;
}

static int print_tag_format (int n, char *prefix, uint8_t buffer[SECTORSIZE], uint32_t _TagLocation, int WrongTagIsFatal, uint16_t *TagIdentifier) // 3_7_2 4_7_2
{
	uint8_t CheckSum =
		buffer[ 0] + buffer[ 1] + buffer[ 2] + buffer[ 3] +
		             buffer[ 5] + buffer[ 6] + buffer[ 7] +
		buffer[ 8] + buffer[ 9] + buffer[10] + buffer[11] +
		buffer[12] + buffer[13] + buffer[14] + buffer[15];
	uint16_t DescriptorCRC =                                             (buffer[ 9]<<8) | buffer[8];
	uint16_t _DescriptorCRC = DescriptorCRC;
	uint16_t DescriptorCRCLength =                                       (buffer[11]<<8) | buffer[10];
	uint32_t TagLocation =         (buffer[15]<<24) | (buffer[14]<<16) | (buffer[13]<<8) | buffer[12];

	if (DescriptorCRCLength <= (SECTORSIZE - 16))
	{
		_DescriptorCRC = crc16(buffer + 16, DescriptorCRCLength);
	}

	*TagIdentifier = (buffer[1] << 8) | buffer[0];

	N(n); debug_printf ("%sDescriptorTag.TagIdentifier:       %d - %s\n", prefix, *TagIdentifier, TagIdentifierName (*TagIdentifier));
	N(n); debug_printf ("%sDescriptorTag.DescriptorVersion:   %d\n", prefix, (buffer[3]<<8) | buffer[2]);
	N(n); debug_printf ("%sDescriptorTag.TagChecksum:         0x%02x", prefix, buffer[4]); if (buffer[4] != CheckSum) debug_printf (" EXPECTED 0x%02x", CheckSum); debug_printf ("\n");
	//N(n); debug_printf ("%sDescriptorTag.Reserved:            %d\n", prefix, buffer[5]);
	N(n); debug_printf ("%sDescriptorTag.TagSerialNumber:     %d\n", prefix, (buffer[7]<<8) | buffer[6]);
	N(n); debug_printf ("%sDescriptorTag.DescriptorCRC:       0x%04x", prefix, (buffer[9]<<8) | buffer[8]); if (_DescriptorCRC != DescriptorCRC) debug_printf (" EXPECTED 0x%04x", _DescriptorCRC); debug_printf ("\n");
	N(n); debug_printf ("%sDescriptorTag.DescriptorCRCLength: %d%s\n", prefix, DescriptorCRCLength, (DescriptorCRCLength <= (SECTORSIZE - 16)) ? "" : " - WARNING too big");
	N(n); debug_printf ("%sDescriptorTag.TagLocation:         %d", prefix, TagLocation);
	if (_TagLocation != TagLocation)
	{
		debug_printf (" - WARNING wrong, expected %"PRIu32, _TagLocation);
	}
	debug_printf ("\n");

	return (buffer[4] == CheckSum) &&
	       ((_TagLocation == TagLocation) || (!WrongTagIsFatal)) &&
	       (_DescriptorCRC == DescriptorCRC) &&
	       (DescriptorCRCLength <= (SECTORSIZE - 16)) ? 0 : -1;
}

/* ECMA-167 4/14.14.1 */
static void UDF_shortad_from_data (int n, const char *prefix, struct UDF_shortad *target, uint8_t *source)
{
	target->ExtentLength   = ((source[3] & 0x3f)<<24) | (source[2]<<16) | (source[1]<<8) | source[0];
	target->ExtentPosition = ( source[7]        <<24) | (source[6]<<16) | (source[5]<<8) | source[4];
	target->ExtentInterpretation = (enum eExtentInterpretation)(source[3]>>6);
	N(n); debug_printf ("%sExtentLength:         %"PRIu32"\n", prefix, target->ExtentLength);
	N(n); debug_printf ("%sExtentPosition:       %"PRIu32"\n", prefix, target->ExtentPosition);
	N(n); debug_printf ("%sExtentInterpretation: %"PRIu8"\n", prefix, target->ExtentInterpretation);
}

/* ECMA-167 4/14.14.2 */
static void UDF_longad_from_data (int n, const char *prefix, struct UDF_longad *target, uint8_t *source)
{
	target->ExtentLength                      = (source[3]<<24) | (source[2]<<16) | (source[1]<<8) | source[0];
	target->ExtentLocation.LogicalBlockNumber = (source[7]<<24) | (source[6]<<16) | (source[5]<<8) | source[4];
	target->ExtentLocation.PartitionReferenceNumber =                               (source[9]<<8) | source[8];
	target->ExtentErased = !!(((source[11]<<8) | source[10]) & 0x0001);
	N(n); debug_printf ("%sExtentLength:                            %"PRIu32"\n", prefix, target->ExtentLength);
	N(n); debug_printf ("%sExtentLocation.LogicalBlockNumber:       %"PRIu32"\n", prefix, target->ExtentLocation.LogicalBlockNumber);
	N(n); debug_printf ("%sExtentLocation.PartitionReferenceNumber: %"PRIu16"\n", prefix, target->ExtentLocation.PartitionReferenceNumber);
	N(n); debug_printf ("%sExtentErased:                            %"PRIu8"\n",  prefix, target->ExtentErased);
}

static void UDF_extent_ad_from_data (int n, const char *prefix, struct UDF_extent_ad *target, uint8_t *source)
{
	target->ExtentLength   = (source[3]<<24) | (source[2]<<16) | (source[1]<<8) | source[0];
	target->ExtentLocation = (source[7]<<24) | (source[6]<<16) | (source[5]<<8) | source[4];
	N(n); debug_printf ("%sExtentLength:       %"PRIu32"\n", prefix, target->ExtentLength);
	N(n); debug_printf ("%sExtentLocation:     %"PRIu32"\n", prefix, target->ExtentLocation);
}

static void UDF_shortad_from_longad (struct UDF_shortad *target, struct UDF_longad *source)
{
	target->ExtentLength   = source->ExtentLength;
	target->ExtentPosition = source->ExtentLocation.LogicalBlockNumber;
}


static struct UDF_LogicalVolume_Common *UDF_GetLogicalPartition (struct cdfs_disc_t *disc, uint16_t PartId)
{
	int j;
	for (j=0; j < disc->udf_session->LogicalVolumes->LogicalVolume_N; j++)
	{
		if (disc->udf_session->LogicalVolumes->LogicalVolume[j]->PartId == PartId)
		{
			return disc->udf_session->LogicalVolumes->LogicalVolume[j];
		}
	}
	return 0;
}

static struct UDF_PhysicalPartition_t *UDF_GetPhysicalPartition (struct cdfs_disc_t *disc, uint16_t PartId)
{
	int j;
	for (j=0; j < disc->udf_session->PhysicalPartition_N; j++)
	{
		if (disc->udf_session->PhysicalPartition[j].PartitionNumber == PartId)
		{
			return &disc->udf_session->PhysicalPartition[j];
		}
	}
	return 0;
}

/* ExtentLength is rounded up to SECTORSIZE */
static uint8_t *UDF_FetchSectors (int n, struct cdfs_disc_t *disc, struct UDF_Partition_Common *source, uint32_t ExtentLocation, uint32_t ExtentLength)
{
	uint8_t *buffer;
	int i;

	if (!source)
	{
		return 0;
	}
	if (!ExtentLength)
	{
		return 0;
	}
	ExtentLength = (ExtentLength + SECTORSIZE - 1) & ~(SECTORSIZE - 1);

	buffer = calloc (1, ExtentLength);
	if (!buffer)
	{
		N(n); debug_printf ("Error - UDF_FetchSectors() calloc(%"PRIu32") failed\n", ExtentLength);
		return 0;
	}

	for (i=0; i < ExtentLength / SECTORSIZE; i++)
	{
		if (source->FetchSector (disc, source, buffer + i * SECTORSIZE, ExtentLocation + i))
		{
			N(n); debug_printf ("Error - UDF_FetchSectors() FetchSector(%" PRIu32 " %d) failed\n", ExtentLocation, i);
			free (buffer);
			return 0;
		}
	}

	return buffer;
}

////////////////////////////////////////////////////////////////// File structure //////////////////////////////////////////////////////////////////////////////

// 0x0103
static int IndirectEntry (int n, struct cdfs_disc_t *disc, struct UDF_Partition_Common *PartitionCommon, uint32_t ExtentLocation, struct UDF_longad *Target)
{
	uint8_t *buffer;
	uint16_t TagIdentifier;
	enum eFileType FileType;
	uint16_t Flags;
	int strategy4096;

	N(n); debug_printf ("[Indirect Entry]\n");

#if 0 // Length is always SECTORSIZE
	if (ExtentLength < 46)
	{
		N(n+1); debug_printf ("Error - Length too small to contain header\n");
		return -1;
	}

	if (ExtentLength != SECTORSIZE)
	{
		N(n+1); debug_printf ("Warning - ExtentLength != SECTORSIZE\n");
	}
#endif
	buffer = UDF_FetchSectors (n+1, disc, PartitionCommon, ExtentLocation, SECTORSIZE);
	if (!buffer)
	{
		N(n+1); debug_printf ("Error - failed fetching data");
		return -1;
	}

	if (print_tag_format (n+1, "", buffer, ExtentLocation, 1, &TagIdentifier))
	{
		free (buffer);
		return -1;
	}

	if (TagIdentifier != 0x0103)
	{
		N(n+1); debug_printf ("Error - Wrong TagIdentifier\n");
		free (buffer);
		return -1;
	}

	print_4_14_6 (n+1, "ICB TAG", buffer + 16, &Flags, &FileType, &strategy4096 /* ignore */);
	if (FileType != FILETYPE_INDIRECT_ENTRY)
	{
		N(n+2); debug_printf ("Error - FileType should have been \"Indirect Entry\"\n");
		free (buffer);
		return -1;
	}

	UDF_longad_from_data (n+2, "Indirect ICB.", Target, buffer + 36);

	free (buffer);
	return 0;
}

#if 0
/* 0x0104 */
static void TerminalEntry (int n, struct cdfs_disc_t *disc, uint8_t *buffer)
{
	enum eFileType FileType;
	uint16_t Flags;
	uint8_t *b;
	int l;
	int strategy4096;

	N(n);   debug_printf ("[Terminal Entry]\n");
	print_4_14_6 (n+1, "ICB TAG", buffer + 16, &Flags, &FileType, &strategy4096 /* ignore */);
	if (FileType != FILETYPE_TERMINAL_ENTRY)
	{
		N(n+2); debug_printf ("Error - FileType should have been \"Terminal Entry\"\n");
		return;
	}
}
#endif

/* 0x0107, indirect entries handled */
static void SpaceEntryDumpData (int n, struct cdfs_disc_t *disc, uint8_t *b, uint32_t l, struct UDF_Partition_Common *PartitionCommon, uint16_t Flags, uint8_t *buffer /*b can point into this */)
{
	int recursion = 0;
	uint32_t OuterExtentLength = 0;
	uint32_t OuterExtentLocation = 0;

	/* samt format as FileEntry_Allocations(), except no targetleft */
	while (l)
	{
		uint32_t DataExtentLength = 0;
		uint32_t DataExtentLocation = 0;
		uint16_t DataExtentPartId = 0;
#ifdef CDFS_DEBUG
		uint32_t RecordedLength;
		uint32_t InformationLength;
#endif
		uint8_t  DataFlags = 0;
		struct UDF_Partition_Common *DataExtentVolume = PartitionCommon;
		struct UDF_LogicalVolume_Common *lv;

		recursion++;
		if (recursion > 10000)
		{
			N(n+2); debug_printf ("Warning - recursion limit hit\n"); break;
		}

		switch (Flags & 0x03)
		{
			case 0:
				if (l < 8)
				{
					N(n+1); debug_printf ("WARNING - Ran out of extent data (l=%d)\n", l);
					return;
				}
#ifdef CDFS_DEBUG
				RecordedLength = InformationLength =
#endif
				DataExtentLength =  ((b[3] << 24) | (b[2] << 16) | (b[1] << 8) | b[0]) & 0x3fffffff;
				DataExtentLocation = (b[7] << 24) | (b[6] << 16) | (b[5] << 8) | b[4];
				DataFlags =           b[3] >> 6;

				N(n+1); debug_printf ("Data.Extent.Length:    %" PRId32 "\n", DataExtentLength);
				N(n+1); debug_printf ("Data.Extent.Location:  %" PRId32 "\n", DataExtentLocation);
				l -= 8;
				b += 8;
				break;

			case 1:
				if (l < 16)
				{
					N(n+1); debug_printf ("WARNING - Ran out of extent data (l=%d)\n", l);
					return;
				}
#ifdef CDFS_DEBUG
				RecordedLength = InformationLength =
#endif
				DataExtentLength =  ((b[3] << 24) | (b[2] << 16) | (b[ 1] << 8) | b[ 0]) & 0x3fffffff;
				DataExtentLocation = (b[7] << 24) | (b[6] << 16) | (b[ 5] << 8) | b[ 4];
				DataExtentPartId =                                 (b[ 9] << 8) | b[ 8];
				DataFlags =           b[3] >> 6;
				/* Flags2 =                                        (b[11] << 8) | b[10];
				if (Flags2 & 0x0001)
				{
					Extent erased
				}*/
				/* Implementation Use:  b[12], b[13], b[14], b[15] */
				N(n+1); debug_printf ("Data.Extent.Length:    %" PRId32 "\n", DataExtentLength);
				N(n+1); debug_printf ("Data.Extent.Location:  %" PRId32 "\n", DataExtentLocation);
				N(n+1); debug_printf ("Data.Extent.Partition: %" PRId32 "\n", DataExtentPartId);

				DataExtentVolume = 0;
				lv = UDF_GetLogicalPartition (disc, DataExtentPartId);
				if (lv->Type == 1)
				{
					DataExtentVolume = &lv->PartitionCommon;
				}
				l -= 16;
				b += 16;
				break;

			case 2:
				if (l < 20)
				{
					N(n+1); debug_printf ("WARNING - Ran out of extent data (l=%d)\n", l);
					return;
				}
				DataExtentLength =  ((b[ 3] << 24) | (b[ 2] << 16) | (b[ 1] << 8) | b[ 0]) & 0x3fffffff;
#ifdef CDFS_DEBUG
				RecordedLength =    ((b[ 7] << 24) | (b[ 6] << 16) | (b[ 5] << 8) | b[ 4]) & 0x3fffffff;
				InformationLength = ((b[11] << 24) | (b[10] << 16) | (b[ 9] << 8) | b[ 8]) & 0x3fffffff;
#endif
				DataExtentLocation = (b[15] << 24) | (b[14] << 16) | (b[13] << 8) | b[12];
				DataExtentPartId =                                   (b[17] << 8) | b[16];

				N(n+1); debug_printf ("Data.Extent.Length:    %" PRId32 "\n", DataExtentLength);
				N(n+1); debug_printf ("Recorded.Length:       %" PRId32 "\n", RecordedLength);
				N(n+1); debug_printf ("Information.Length:    %" PRId32 "\n", InformationLength);
				N(n+1); debug_printf ("Data.Extent.Location:  %" PRId32 "\n", DataExtentLocation);
				N(n+1); debug_printf ("Data.Extent.Partition: %" PRId32 "\n", DataExtentPartId);

				/* Implementation Use: b[18], b[19] */
				DataExtentVolume = 0;
				lv = UDF_GetLogicalPartition (disc, DataExtentPartId);
				if (lv)
				{
					DataExtentVolume = &lv->PartitionCommon;
				}
				l -= 20;
				b += 20;
				break;

			case 3:
				N(n+1); debug_printf ("(Data stored directly in the FileEntry record\n");
				return;
		}

		N(n+1); debug_printf ("Data.Flags:            0x%" PRIx8 "\n", DataFlags);

		switch (DataFlags)
		{
			case 0: /* Extent recorded and allocated */
				N(n+2); debug_printf ("Extent recorded and allocated\n");
				if (!DataExtentVolume)
				{
					N(n+1); debug_printf ("WARNING - Unable to find partition\n");
					return;
				}
				break;

			case 1: /* Extent not recorded but allocated */
				N(n+2); debug_printf ("Extent not recorded but allocated\n");
				break;

			case 2: /* Extent not recorded and not allocated */
				N(n+2); debug_printf ("Extent not recorded and not allocated\n");
				break;

			case 3: /* The extent is the next extent of allocation descriptors */
				N(n+2); debug_printf ("The extent is the next extent of allocation descriptors\n");
				OuterExtentLength = DataExtentLength;
				OuterExtentLocation = DataExtentLocation;
				if (!DataExtentVolume)
				{
					N(n+1); debug_printf ("WARNING - Unable to find partition\n");
					return;
				}
				PartitionCommon = DataExtentVolume;
				l = 0; /* force full break */
				break;
		}
		if (OuterExtentLength)
		{
			if (PartitionCommon->FetchSector (disc, PartitionCommon, buffer, OuterExtentLocation))
			{
				N(n+1); debug_printf ("WARNING - Failed to fetch Chain-Extent: %"PRIu32"\n", OuterExtentLocation);
				return;
			}
			l = OuterExtentLength > SECTORSIZE ? SECTORSIZE : OuterExtentLength;
			b = buffer;
			OuterExtentLength -= l;
			OuterExtentLocation++;
		}
	}

}

static void SpaceEntry (int n, struct cdfs_disc_t *disc, struct UDF_Partition_Common *PartitionCommon, struct UDF_shortad *L, const char *prefix, int recursion /* set to zero */)
{
	uint8_t buffer[SECTORSIZE];
	uint16_t TagIdentifier;
	int strategy4096 = 0;

	uint32_t L_AD;
	enum eFileType FileType;
	uint16_t Flags;
	uint8_t *b;
	int l;
	struct UDF_longad NLong;
	struct UDF_shortad NShort;
	struct UDF_PhysicalPartition_t *NPartition;

	N(n); debug_printf ("[%s Space Entry Sequence]\n", prefix);
	if (L->ExtentLength < SECTORSIZE)
	{
		N(n+1); debug_printf ("WARNING - ExtentLength < BUFFERSIZE\n");
	}

	if (PartitionCommon->FetchSector (disc, PartitionCommon, buffer, L->ExtentPosition))
	{
		N(n+1); debug_printf ("Error - Failed fetching data\n");
		return;
	}

	if (print_tag_format (n+1, "", buffer, L->ExtentPosition, 1, &TagIdentifier))
	{
		return;
	}
	if (TagIdentifier != 0x0107)
	{
		N(n+1); debug_printf ("Error - Wrong TagIdentifier\n");
		return;
	}

	print_4_14_6 (n+1, "ICB TAG", buffer + 16, &Flags, &FileType, &strategy4096);

	if (FileType != FILETYPE_UNALLOCATED_SPACE_ENTRY)
	{
		N(n+1); debug_printf ("Error - Wrong FileType\n");
		return;
	}

	L_AD = (buffer[39] << 24) | (buffer[38] << 16) | (buffer[37] << 8) | buffer[36];
	N(n+1); debug_printf ("Length of Allocation Descriptors: %" PRId32 "\n", L_AD);
	N(n+1); debug_printf ("Allocation descriptors:\n");
	b = buffer + 40;
	l = L_AD;
	if (L_AD > (SECTORSIZE - 40))
	{
		N(n+2); debug_printf ("WARNING - buffer shrunk due to size overflow\n");
	}

	SpaceEntryDumpData (n, disc, b, l, PartitionCommon, Flags, buffer);

	if (!strategy4096)
	{
		return;
	}

	/* try to find newer version */
	if (recursion > MAX_INDIRECT_RECURSION)
	{
		N(n+2); debug_printf ("Error - indirect recursion limit reached\n");
		return;
	}

	if (IndirectEntry (n+2, disc, PartitionCommon, L->ExtentPosition + 1, &NLong))
	{
		return;
	}
	NPartition = UDF_GetPhysicalPartition (disc, NLong.ExtentLocation.PartitionReferenceNumber);
	if (!NPartition)
	{
		N(n+2); debug_printf ("Error - Partition not found\n");
		return;
	}
	UDF_shortad_from_longad (&NShort, &NLong);
	SpaceEntry (n+3, disc, &NPartition->PartitionCommon, &NShort, prefix, recursion + 1);
}

static int FileEntryLoadData (struct cdfs_disc_t *disc, struct UDF_FileEntry_t *FE, uint8_t **filedata, uint64_t MaxLength)
{
	uint8_t *b;
	uint64_t l;
	int i, j;

	*filedata = 0;

	if (!FE->InformationLength)
	{
		return 0;
	}

	if (FE->InformationLength > MaxLength)
	{
		return -1;
	}

	*filedata = calloc (FE->InformationLength + SECTORSIZE - 1, 1); /* taking into account a possible overshoot */
	b = *filedata;
	l = FE->InformationLength;

	if (FE->InlineData)
	{
		memcpy (b, FE->InlineData, l);
		return 0;
	}

	for (i=0; i < FE->FileAllocations; i++)
	{
		if (!FE->FileAllocation[i].Partition)
		{
			b += FE->FileAllocation[i].InformationLength;
			if (l < FE->FileAllocation[i].InformationLength)
			{ /* should not happend if UDF image is healthy */
				return 0;
			}
			l -= FE->FileAllocation[i].InformationLength;
			continue;
		}
		for (j=0; j < FE->FileAllocation[i].InformationLength; j += SECTORSIZE)
		{
			uint32_t r = FE->FileAllocation[i].InformationLength - j;
			if (r > SECTORSIZE)
			{
				r = SECTORSIZE;
			}
			FE->FileAllocation[i].Partition->FetchSector (disc, FE->FileAllocation[i].Partition, b, FE->FileAllocation[i].ExtentLocation + (j / SECTORSIZE));
			b += r;
			if (l < r)
			{ /* should not happend if UDF image is healthy */
				return 0;
			}
			l -= r;
		}
	}
	
	return 0;
}

static int FileEntryAllocations (int n, struct cdfs_disc_t *disc, struct UDF_Partition_Common *PartitionCommon, struct UDF_FileEntry_t **target, uint16_t Flags, uint8_t *b, int l, int InitialOffset)
{
	uint32_t OuterExtentLength = 0;
	uint32_t OuterExtentLocation = 0;
	uint64_t targetleft = (*target)->InformationLength;
	uint8_t OuterBuffer[SECTORSIZE];

	int                    Size = 0;
	int                    Fill = 0;
	struct FileAllocation *Data = 0;

	N(n); debug_printf ("Allocation descriptors: (l=%d)\n", l);
	while (targetleft && l)
	{
		uint32_t DataExtentLength = 0;
		uint32_t DataExtentLocation = 0;
		uint16_t DataExtentPartId = 0;
		uint32_t RecordedLength;
		uint32_t InformationLength = 0;
		uint8_t  DataFlags = 0;
		struct UDF_Partition_Common *DataExtentVolume = PartitionCommon;
		struct UDF_LogicalVolume_Common *lv;

		switch (Flags & 0x03)
		{
			case 0:
				if (l < 8)
				{
					N(n+1); debug_printf ("WARNING - Ran out of extent data (targetleft=%" PRIu64 " l=%d)\n", targetleft, l);
					free (Data);
					return -1;
				}

				RecordedLength = InformationLength =
				DataExtentLength =  ((b[3] << 24) | (b[2] << 16) | (b[1] << 8) | b[0]) & 0x3fffffff;
				DataExtentLocation = (b[7] << 24) | (b[6] << 16) | (b[5] << 8) | b[4];
				DataFlags =           b[3] >> 6;

				N(n+1); debug_printf ("Data.Extent.Length:    %" PRId32 "\n", DataExtentLength);
				N(n+1); debug_printf ("Data.Extent.Location:  %" PRId32 "\n", DataExtentLocation);
				l -= 8;
				b += 8;
				break;

			case 1:
				if (l < 16)
				{
					N(n+1); debug_printf ("WARNING - Ran out of extent data (targetleft=%" PRIu64 " l=%d)\n", targetleft, l);
					free (Data);
					return -1;
				}

				RecordedLength = InformationLength =
				DataExtentLength =  ((b[3] << 24) | (b[2] << 16) | (b[ 1] << 8) | b[ 0]) & 0x3fffffff;
				DataExtentLocation = (b[7] << 24) | (b[6] << 16) | (b[ 5] << 8) | b[ 4];
				DataExtentPartId =                                 (b[ 9] << 8) | b[ 8];
				DataFlags =           b[3] >> 6;
				/* Flags2 =                                        (b[11] << 8) | b[10];
				if (Flags2 & 0x0001)
				{
					Extent erased
				}*/
				/* Implementation Use:  b[12], b[13], b[14], b[15] */
				N(n+1); debug_printf ("Data.Extent.Length:    %" PRId32 "\n", DataExtentLength);
				N(n+1); debug_printf ("Data.Extent.Location:  %" PRId32 "\n", DataExtentLocation);
				N(n+1); debug_printf ("Data.Extent.Partition: %" PRId32 "\n", DataExtentPartId);

				DataExtentVolume = 0;
				lv = UDF_GetLogicalPartition (disc, DataExtentPartId);
				if (lv->Type == 1)
				{
					DataExtentVolume = &lv->PartitionCommon;
				}
				l -= 16;
				b += 16;
				break;

			case 2:
				if (l < 20)
				{
					N(n+1); debug_printf ("WARNING - Ran out of extent data (targetleft=%" PRIu64 " l=%d)\n", targetleft, l);
					free (Data);
					return -1;
				}
				DataExtentLength =  ((b[ 3] << 24) | (b[ 2] << 16) | (b[ 1] << 8) | b[ 0]) & 0x3fffffff;
				RecordedLength =    ((b[ 7] << 24) | (b[ 6] << 16) | (b[ 5] << 8) | b[ 4]) & 0x3fffffff;
				InformationLength = ((b[11] << 24) | (b[10] << 16) | (b[ 9] << 8) | b[ 8]) & 0x3fffffff;
				DataExtentLocation = (b[15] << 24) | (b[14] << 16) | (b[13] << 8) | b[12];
				DataExtentPartId =                                   (b[17] << 8) | b[16];

				N(n+1); debug_printf ("Data.Extent.Length:    %" PRId32 "\n", DataExtentLength);
				N(n+1); debug_printf ("Recorded.Length:       %" PRId32 "\n", RecordedLength);
				N(n+1); debug_printf ("Information.Length:    %" PRId32 "\n", InformationLength);
				N(n+1); debug_printf ("Data.Extent.Location:  %" PRId32 "\n", DataExtentLocation);
				N(n+1); debug_printf ("Data.Extent.Partition: %" PRId32 "\n", DataExtentPartId);

				/* Implementation Use: b[18], b[19] */
				DataExtentVolume = 0;
				lv = UDF_GetLogicalPartition (disc, DataExtentPartId);
				if (lv)
				{
					DataExtentVolume = &lv->PartitionCommon;
				}
				l -= 20;
				b += 20;
				break;

			case 3:
				N(n+1); debug_printf ("(Data stored directly in the FileEntry record\n");
				if (targetleft > l)
				{
					N(n+1); debug_printf ("Error - inline-data is smalled than the required InformationLength\n");
					free (Data);
					return -1;
				}

				{
					void *temp;
					temp = realloc (*target, sizeof (**target) + sizeof ((*target)->FileAllocation[0]) + targetleft);
					if (!temp)
					{
						N(n+2); debug_printf ("Error - FileEntryAllocations() realloc() failed\n");
						free (Data);
						return -1;
					}
					*target = temp;
				}
				(*target)->InlineData = (uint8_t *)(*target) + sizeof (**target) + sizeof ((*target)->FileAllocation[0]);
				memcpy ((*target)->InlineData, b, targetleft);
				(*target)->FileAllocations = 1;
				(*target)->FileAllocation[0].Partition = PartitionCommon;
				(*target)->FileAllocation[0].ExtentLocation = (*target)->ExtentLocation;
				(*target)->FileAllocation[0].SkipLength = InitialOffset;
				(*target)->FileAllocation[0].InformationLength = (*target)->InformationLength;
				return 0;
		}

		N(n+1); debug_printf ("Data.Flags:            0x%" PRIx8 "\n", DataFlags);

		if (Fill >= Size)
		{
			void *temp = realloc (Data, sizeof(Data[0]) * (Size + 100));
			if (!temp)
			{
				N(n+2); debug_printf ("Error - FileEntryAllocations() realloc() failed\n");
				free (Data);
				return -1;
			}
			Data = temp;
			Size += 100;
		}

		switch (DataFlags)
		{
			case 0: /* Extent recorded and allocated */
				N(n+2); debug_printf ("Extent recorded and allocated\n");
				if (!DataExtentVolume)
				{
					N(n+1); debug_printf ("WARNING - Unable to find partition\n");
					free (Data);
					return -1;
				}

				Data[Fill].Partition = DataExtentVolume;
				Data[Fill].ExtentLocation = DataExtentLocation;
				Data[Fill].SkipLength = 0;
				Data[Fill].InformationLength = InformationLength;
				Fill++;

				while (DataExtentLength && InformationLength)
				{
					uint32_t Go = SECTORSIZE;
					Go = InformationLength > SECTORSIZE ? SECTORSIZE : InformationLength;
					if (Go > targetleft)
					{
						N(n+1); debug_printf ("WARNING - Data-over-shoot\n");
						Go = targetleft;
					}
					RecordedLength -= RecordedLength > SECTORSIZE ? SECTORSIZE : RecordedLength;
					InformationLength -= InformationLength > SECTORSIZE ? SECTORSIZE : InformationLength;
					DataExtentLength -= DataExtentLength > SECTORSIZE ? SECTORSIZE : DataExtentLength;
					DataExtentLocation++;
					targetleft -= Go;
				}
				break;

			case 1: /* Extent not recorded but allocated */
				N(n+2); debug_printf ("Extent not recorded but allocated\n");

				Data[Fill].Partition = 0;
				Data[Fill].ExtentLocation = DataExtentLocation;
				Data[Fill].SkipLength = 0;
				Data[Fill].InformationLength = InformationLength;
				Fill++;

				if (InformationLength > targetleft)
				{
					InformationLength = targetleft;
				}
				targetleft -= InformationLength;
				break;

			case 2: /* Extent not recorded and not allocated */
				N(n+2); debug_printf ("Extent not recorded and not allocated\n");

				Data[Fill].Partition = 0;
				Data[Fill].ExtentLocation = DataExtentLocation;
				Data[Fill].SkipLength = 0;
				Data[Fill].InformationLength = InformationLength;
				Fill++;

				if (InformationLength > targetleft)
				{
					InformationLength = targetleft;
				}
				targetleft -= InformationLength;
				break;

			case 3: /* The extent is the next extent of allocation descriptors */
				N(n+2); debug_printf ("The extent is the next extent of allocation descriptors\n");
				OuterExtentLength = DataExtentLength;
				OuterExtentLocation = DataExtentLocation;
				if (!DataExtentVolume)
				{
					N(n+1); debug_printf ("WARNING - Unable to find partition\n");
					free (Data);
					return -1;
				}
				PartitionCommon = DataExtentVolume;
				l = 0; /* force full break */
				break;
		}

		if (OuterExtentLength)
		{
			if (PartitionCommon->FetchSector (disc, PartitionCommon, OuterBuffer, OuterExtentLocation))
			{
				N(n+1); debug_printf ("WARNING - Failed to fetch Chain-Extent: %"PRIu32"\n", OuterExtentLocation);
				free (Data);
				return -1;
			}
			l = OuterExtentLength > SECTORSIZE ? SECTORSIZE : OuterExtentLength;
			b = OuterBuffer;
			OuterExtentLength -= l;
			OuterExtentLocation++;
		}
	}

	if (targetleft)
	{
		N(n+1); debug_printf ("WARNING - Did not find enough extents\n");
		free (Data);
		return -1;
	}

	{
		void *temp;
		temp = realloc (*target, sizeof (**target) + sizeof ((*target)->FileAllocation[0]) * Fill);
		if (!temp)
		{
			N(n+2); debug_printf ("Error - FileEntryAllocations() realloc() failed\n");
			free (Data);
			return -1;
		}
		*target = temp;
	}
	(*target)->FileAllocations = Fill;
	memcpy ((*target)->FileAllocation, Data, sizeof (Data[0]) * Fill);
	free (Data);

	return 0;
}

/* 0x0105 */
/* 0x010a */
static struct UDF_FileEntry_t *FileEntry (int n, struct cdfs_disc_t *disc, uint32_t TagLocation, struct UDF_Partition_Common *PartitionCommon, int recursion /* set to zero */)
{
	struct UDF_FileEntry_t *retval;
	uint32_t L_AD;
	uint32_t L_EA;
	uint8_t *b;
	int l;
	uint8_t buffer[SECTORSIZE];
	struct UDF_longad ExtendedAttributeICB;

	int strategy4096 = 0;
	int isextended;

	N(n); debug_printf ("..[File Entry]\n");

	if (!PartitionCommon)
	{
		N(n+1); debug_printf ("Error - unable to find Partition\n");
		return 0;
	}

	retval = calloc (1, sizeof (*retval));
	if (!retval)
	{
		N(n+1); debug_printf ("Error - FileEntry() calloc failed\n");
		return 0;
	}
	retval->PartitionCommon = PartitionCommon;
	retval->ExtentLocation = TagLocation;

	if (PartitionCommon->FetchSector (disc, PartitionCommon, buffer, TagLocation))
	{
		N(n+1); debug_printf ("Error - unable to fetch sector\n");
		free (retval);
		return 0;
	}

	if (print_tag_format (n, "", buffer, TagLocation, 1, &retval->TagIdentifier))
	{
		free (retval);
		return 0;
	}

	switch (retval->TagIdentifier)
	{
		case 0x0105: isextended = 0; N(n); debug_printf ("[File Entry]\n"); break;
		case 0x010a: isextended = 1; N(n); debug_printf ("[Extended File Entry]\n"); break;
		default:
			debug_printf ("WARNING - unexpected TagIdentifier\n");
			free (retval);
			return 0;
	}

	print_4_14_6 (n+1, "ICB TAG", buffer + 16, &retval->Flags, &retval->FileType, &strategy4096);
	retval->UID =         (buffer[39] << 24) | (buffer[38] << 16) | (buffer[37] <<  8) |  buffer[36];
	retval->GID =         (buffer[43] << 24) | (buffer[42] << 16) | (buffer[41] <<  8) |  buffer[40];
	retval->Permissions = (buffer[47] << 24) | (buffer[46] << 16) | (buffer[45] <<  8) |  buffer[44];
	N(n+1); debug_printf ("UID:                               %" PRId32 "\n", retval->UID);
	N(n+1); debug_printf ("GID:                               %" PRId32 "\n", retval->GID);
	N(n+1); debug_printf ("Permissions:                       %" PRId32 "\n", retval->Permissions);
	N(n+1); debug_printf ("File Link Count:                   %" PRIu16 "\n", (buffer[49] <<  8) |  buffer[48]);
	N(n+1); debug_printf ("Record Format:                     %" PRIu8  "\n",                       buffer[50]);
	switch (buffer[50])
	{
		case  0: N(n+2); debug_printf ("structure of the information recorded in the file is not specified by this field.\n"); break;
		case  1: N(n+2); debug_printf ("information in the file is a sequence of padded fixed-length records.\n"); break; // (see 5/9.2.1)
		case  2: N(n+2); debug_printf ("information in the file is a sequence of fixed-length records.\n"); break; // (see 5/9.2.2)
		case  3: N(n+3); debug_printf ("information in the file is a sequence of variable-length-8 records.\n"); break; // (see 5/9.2.3.1)
		case  4: N(n+3); debug_printf ("information in the file is a sequence of variable-length-16 records.\n"); break; // (see 5/9.2.3.2)
		case  5: N(n+3); debug_printf ("information in the file is a sequence of variable-length-16-MSB records.\n"); break; // (see 5/9.2.3.3)
		case  6: N(n+3); debug_printf ("information in the file is a sequence of variable-length-32 records.\n"); break; // (see 5/9.2.3.4)
		case  7: N(n+3); debug_printf ("information in the file is a sequence of stream-print records.\n"); break; // (see 5/9.2.4)
		case  8: N(n+3); debug_printf ("information in the file is a sequence of stream-LF records.\n"); break; // (see 5/9.2.5)
		case  9: N(n+3); debug_printf ("information in the file is a sequence of stream-CR records.\n"); break; // (see 5/9.2.6)
		case 10: N(n+3); debug_printf ("information in the file is a sequence of stream-CRLF records.\n"); break; // (see 5/9.2.7)
		case 11: N(n+3); debug_printf ("information in the file is a sequence of stream-LFCR records.\n"); break; // (see 5/9.2.8
		default: N(n+3); debug_printf ("record format value is reserved.\n");
	}
	N(n+1); debug_printf ("Record Display Attributes:         %" PRIu8  "\n",                                                                 buffer[51]);
	N(n+1); debug_printf ("Record Length:                     %" PRId32 "\n", (buffer[55] << 24) | (buffer[54] << 16) | (buffer[53] <<  8) |  buffer[52]);
	N(n+1); debug_printf ("Information Length:                %" PRId64 "\n", retval->InformationLength = (
		((uint64_t)(buffer[63]) << 56) |
		((uint64_t)(buffer[62]) << 48) |
		((uint64_t)(buffer[61]) << 40) |
		((uint64_t)(buffer[60]) << 32) |
		((uint64_t)(buffer[59]) << 24) |
		((uint64_t)(buffer[58]) << 16) |
		((uint64_t)(buffer[57]) <<  8) |
		            buffer[56]));
	if (!isextended)
	{
		N(n+1); debug_printf ("Logical Blocks Recorded:           %" PRId64 "\n",
			((uint64_t)(buffer[71]) << 56) |
			((uint64_t)(buffer[70]) << 48) |
			((uint64_t)(buffer[69]) << 40) |
			((uint64_t)(buffer[68]) << 32) |
			((uint64_t)(buffer[67]) << 24) |
			((uint64_t)(buffer[66]) << 16) |
			((uint64_t)(buffer[65]) <<  8) |
			            buffer[64]);
	} else {
		N(n+1); debug_printf ("Object Size:                       %" PRId64 "\n",
			((uint64_t)(buffer[71]) << 56) |
			((uint64_t)(buffer[70]) << 48) |
			((uint64_t)(buffer[69]) << 40) |
			((uint64_t)(buffer[68]) << 32) |
			((uint64_t)(buffer[67]) << 24) |
			((uint64_t)(buffer[66]) << 16) |
			((uint64_t)(buffer[65]) <<  8) |
			            buffer[64]);
		N(n+1); debug_printf ("Logical Blocks Recorded:           %" PRId64 "\n",
			((uint64_t)(buffer[79]) << 56) |
			((uint64_t)(buffer[78]) << 48) |
			((uint64_t)(buffer[77]) << 40) |
			((uint64_t)(buffer[76]) << 32) |
			((uint64_t)(buffer[75]) << 24) |
			((uint64_t)(buffer[74]) << 16) |
			((uint64_t)(buffer[73]) <<  8) |
			buffer[72]);
	}

	memcpy (retval->atime, buffer + (isextended?80:72), 12);
	memcpy (retval->mtime, buffer + (isextended?92:84), 12);
	memcpy (retval->ctime, buffer + (isextended?104:84), 12); /* use mtime if not extended */
	memcpy (retval->attrtime, buffer + (isextended?116:112), 12);

	N(n+1); debug_printf ("Access Date and Time:              "); print_1_7_3 (buffer + (isextended?80:72)); debug_printf ("\n");
	N(n+1); debug_printf ("Modification Date and Time:        "); print_1_7_3 (buffer + (isextended?92:84)); debug_printf ("\n");
	memcpy (retval->TimeStamp, buffer + (isextended?92:84), 12);
	if (isextended)
	{
		N(n+1); debug_printf ("Creation Date and Time:            "); print_1_7_3 (buffer + 104); debug_printf ("\n");
	}
	N(n+1); debug_printf ("Attribute Date and Time:           "); print_1_7_3 (buffer + (isextended?116:96)); debug_printf ("\n");
	N(n+1); debug_printf ("Checkpoint:                        %" PRId32 "\n", (buffer[isextended?131:111] << 24) | (buffer[isextended?130:110] << 16) | (buffer[isextended?129:109] <<  8) |  buffer[isextended?128:108]);
	/* if extended, 4 bytes are reserved */

	UDF_longad_from_data(n+1, "Extended Attribute ICB.", &ExtendedAttributeICB, buffer + (isextended?136:112));

	if (isextended)
	{
		N(n+1); debug_printf ("Stream Directory ICB.Extent Length: %" PRId32 "\n", (buffer[155] << 24) | (buffer[154] << 16) | (buffer[153] <<  8) |  buffer[152]);
		N(n+1); debug_printf ("Stream Directory ICB.Extent Location.Logical Block Number: %" PRId32 "\n", (buffer[159] << 24) | (buffer[158] << 16) | (buffer[157] <<  8) |  buffer[156]);
		N(n+1); debug_printf ("Stream Directory ICB.Extent Location.Partition Reference Number: %" PRId16 "\n", (buffer[161] <<  8) |  buffer[160]);
		/* next 6 bytes are reserved in the "Stream Directory ICB" */
	}
	N(n+1); debug_printf ("Implementation Identifier:         "); print_1_7_4 (buffer + (isextended?168:128), 1 /* IsImplementation */); debug_printf ("\n");
	N(n+1); debug_printf ("Unique Id:                         0x%02x%02x%02x%02x%02x%02x%02x%02x\n", buffer[isextended?207:167], buffer[isextended?206:166], buffer[isextended?205:165], buffer[isextended?204:164], buffer[isextended?203:163], buffer[isextended?202:162], buffer[isextended?201:161], buffer[isextended?200:160]);
	L_EA = (buffer[isextended?211:171] << 24) | (buffer[isextended?210:170] << 16) | (buffer[isextended?209:169] <<  8) |  buffer[isextended?208:168];
	L_AD = (buffer[isextended?215:175] << 24) | (buffer[isextended?214:174] << 16) | (buffer[isextended?213:173] <<  8) |  buffer[isextended?212:172];
	N(n+1); debug_printf ("Length of Extended Attributes:     %" PRId32 "\n", L_EA);
	N(n+1); debug_printf ("Length of Allocation Descriptors:  %" PRId32 "\n", L_AD);
	N(n+1); debug_printf ("Extended Attributes:\n");

	b = buffer + (isextended?216:176);
	l = L_EA;
	if (L_EA > (SECTORSIZE - (isextended?216:176)))
	{
		N(n+2); debug_printf ("WARNING - buffer shrunk due to size overflow\n");
		l = SECTORSIZE - (isextended?216:176);
	}
	if (l)
	{
		ExtendedAttributesInline (n+2, b, l, TagLocation, (retval->FileType != FILETYPE_DIRECTORY)&&(retval->FileType != FILETYPE_STREAM_DIRECTORY), retval);
	}
	if (ExtendedAttributeICB.ExtentLength)
	{
		ExtendedAttributes (n, disc, &ExtendedAttributeICB, (retval->FileType != FILETYPE_DIRECTORY)&&(retval->FileType != FILETYPE_STREAM_DIRECTORY), retval);
	}

	b = buffer + (isextended?216:176) + L_EA;
	l = L_AD;
	if (L_AD + L_EA > (SECTORSIZE - (isextended?216:176)))
	{
		N(n+2); debug_printf ("WARNING - buffer not big enough for allocation entries");
		free (retval);
		return 0;
	}

	if (FileEntryAllocations (n+1, disc, PartitionCommon, &retval, retval->Flags, b, l, b - buffer))
	{
		free (retval);
		return 0;
	}

	if (strategy4096)
	{
		struct UDF_FileEntry_t *retval2;
		struct UDF_longad NLong;
		struct UDF_LogicalVolume_Common *NPartition;

		/* try to find newer version */
		if (recursion > MAX_INDIRECT_RECURSION)
		{
			N(n+2); debug_printf ("Error - indirect recursion limit reached\n");
			return retval;
		}

		if (IndirectEntry (n+2, disc, PartitionCommon, TagLocation + 1, &NLong))
		{
			return retval;
		}
		NPartition = UDF_GetLogicalPartition (disc, NLong.ExtentLocation.PartitionReferenceNumber);
		if (!NPartition)
		{
			N(n+2); debug_printf ("Error - Partition not found\n");
			return retval;
		}

		retval2 = FileEntry (n+3, disc, NLong.ExtentLocation.LogicalBlockNumber, &NPartition->PartitionCommon, recursion + 1);
		if (retval2)
		{
			retval2->PreviousVersion = retval;
			return retval2;
		}
	}

	return retval;
#undef buffer
}

static void FileEntry_Free (struct UDF_FileEntry_t *FE)
{
	struct UDF_FileEntry_t *next;
	while (FE)
	{
		next = FE->PreviousVersion;
		free (FE);
		FE=next;
	}
}

/* 0x0106 */
static uint16_t UDF_ComputeExtendedAttributeChecksum(uint8_t *data)
{
	uint16_t retval = 0;
	int i;
	for (i=0; i < 48; i++)
	{
		retval += data[i];
	}
	return retval;
}

static void ExtendedAttributesCommon (int n, uint8_t *b, uint32_t l, uint32_t TagLocation, int isfile, struct UDF_FileEntry_t *extendedattributes_target)
{
	int i = 0;
	uint16_t TagIdentifier = 0;

	if (l < 24)
	{
		debug_printf ("WARNING - Extended Attributes is not big enough to contain the initial header (%d < 24)\n", (int)l);
		return;
	}

	if (print_tag_format (n, "", b, TagLocation, 1, &TagIdentifier))
	{
		return;
	}

	if (TagIdentifier != 0x0106)
	{
		N(n+1); debug_printf ("Error - TagIdentifier was not the expected 0x0106\n");
		return;
	}

	N(n+2); debug_printf ("Implementation Attributes Location: %"PRIu32"%s\n", (b[19] << 24) | (b[18] << 16) | (b[17] << 8) | (b[16]), ((b[19] << 24) | (b[18] << 16) | (b[17] << 8) | (b[16])) > l ? " OUT OF RANGE":"");
	N(n+2); debug_printf ("Application Attributes Location: %"PRIu32"%s\n", (b[23] << 24) | (b[22] << 16) | (b[21] << 8) | (b[20]), ((b[23] << 24) | (b[22] << 16) | (b[21] << 8) | (b[20])) > l ? " OUT OF RANGE":"");
	b += 24;
	l -= 24;

	while (l >= 12)
	{
		uint32_t AttributeType =   (b[ 3] << 24) | (b[ 2] << 16) | (b[1] << 8) | (b[0]);
#ifdef CDFS_DEBUG
		uint8_t  AttributeSubtype =                                               b[4];
#endif
		uint32_t AttributeLength = (b[11] << 24) | (b[10] << 16) | (b[9] << 8) | (b[8]);
		N(n+2); debug_printf ("ExtendedAttribute.%d.AttributeType:    %"PRIu32"\n", i, AttributeType);
		N(n+2); debug_printf ("ExtendedAttribute.%d.AttributeSubType: %"PRIu8"\n", i, AttributeSubtype);
		N(n+2); debug_printf ("ExtendedAttribute.%d.AttributeLength:  %"PRIu32"\n", i, AttributeLength);
		i++;

		if ((AttributeLength > l) || (AttributeLength < 12))
		{
			N(n+2); debug_printf ("WARNING - buffer overrun 12 >= %d <= %d\n", AttributeLength, l); return;
		}
		switch (AttributeType)
		{
			case 1: /* not mentioned in UDF */
				N(n+3); debug_printf ("[Character Set Information]\n"); // 4/14.10.3
				if (AttributeLength < 17)
				{
					N(n+4); debug_printf ("Error - Attribute too short\n");
					break;
				}
				{
					uint32_t ES_L = (b[15]<<24) | (b[14]<<16) | (b[13]<<8) | b[12];
#ifdef CDFS_DEBUG
					uint8_t CharacterSetType = b[16];
#endif
					int j;
					N(n+4); debug_printf ("EscapeSequencesLength: %"PRIu32"\n", ES_L);
					N(n+4); debug_printf ("CharacterSetType:      CS%"PRIu8"\n", CharacterSetType);
					if ((ES_L > AttributeLength) ||
					    (ES_L + 17 > AttributeLength))
					{
						N(n+4); debug_printf ("Error - Attribute too short to contain the data\n");
						break;
					}
					N(n+4); debug_printf ("Escape Sequence:    ");
					for (j=0; j < ES_L; j++)
					{
						debug_printf (" 0x%02" PRIu8, b[17+j]);
					}
					debug_printf ("\n");
				}
				break;
			case 3: /* UDF standard prohibits this one*/
				N(n+3); debug_printf ("[Alternate Permissions] (tag not allowed according to UDF standard)\n"); // 4/14.10.4
				if (AttributeLength < 18)
				{
					N(n+4); debug_printf ("Error - Attribute too short\n");
					break;
				}
				N(n+4); debug_printf ("Owner Identification:   %"PRIu16"\n", (b[13]<<8) | b[12]);
				N(n+4); debug_printf ("Group Identification:   %"PRIu16"\n", (b[15]<<8) | b[14]);
				N(n+4); debug_printf ("Permission:             0x%04"PRIx16"\n", (b[17]<<8) | b[16]);
				break;
			case 5:
				N(n+3); debug_printf ("[File Times Extended Attribute]\n"); // 4/14.10.5
				if (AttributeLength < 20)
				{
					N(n+4); debug_printf ("Error - Attribute too short\n");
					break;
				}
				{
					int index = 0;
					uint32_t D_L =               (b[15]<<24) | (b[14]<<16) | (b[13]<<8) | b[12];
					uint32_t FileTimeExistence = (b[19]<<24) | (b[18]<<16) | (b[17]<<8) | b[16];
					N(n+4); debug_printf ("DataLength:        %"PRIu32"\n", D_L);
					N(n+4); debug_printf ("FileTimeExistence: %"PRIu32"\n", FileTimeExistence);
					if ((D_L > AttributeLength) ||
					    (D_L + 20 > AttributeLength))
					{
						N(n+4); debug_printf ("Error - Attribute too short to contain the data\n");
						break;
					}
					if (FileTimeExistence & 1)
					{
						if (index * 12 > D_L)
						{
							N(n+4); debug_printf ("Error - Buffer overrun\n");
						}
						N(n+4); debug_printf ("Created Date/Time: "); print_1_7_3 (b + 20 + index * 12); debug_printf ("\n");
						index++;
					}
					/* 2 is reserved due to compatability with ECMA-168 */
					if (FileTimeExistence & 4)
					{
						if (index * 12 > D_L)
						{
							N(n+4); debug_printf ("Error - Buffer overrun\n");
						}
						N(n+4); debug_printf ("Delete Request Date/Time: "); print_1_7_3 (b + 20 + index * 12); debug_printf ("\n");
						index++;
					}
					if (FileTimeExistence & 8)
					{
						if (index * 12 > D_L)
						{
							N(n+4); debug_printf ("Error - Buffer overrun\n");
						}
						N(n+4); debug_printf ("Effective Date/Time: "); print_1_7_3 (b + 20 + index * 12); debug_printf ("\n");
						index++;
					}
					/* 16 is reserved due to compatability with ECMA-168 */
					if (FileTimeExistence & 32)
					{
						if (index * 12 > D_L)
						{
							N(n+4); debug_printf ("Error - Buffer overrun\n");
						}
						N(n+4); debug_printf ("Last Backup Date/Time: "); print_1_7_3 (b + 20 + index * 12); debug_printf ("\n");
						index++;
					}
				}
				break;
			case 6: /* not mentioned in UDF - when was then information inside the file created - user supplied information? */
				N(n+3); debug_printf ("[Information Times Extended Attribute]\n"); // 4/14.10.6
				if (AttributeLength < 20)
				{
					N(n+4); debug_printf ("Error - Attribute too short\n");
					break;
				}
				{
					int index = 0;
					uint32_t D_L =               (b[15]<<24) | (b[14]<<16) | (b[13]<<8) | b[12];
					uint32_t FileTimeExistence = (b[19]<<24) | (b[18]<<16) | (b[17]<<8) | b[16];
					N(n+4); debug_printf ("DataLength:        %"PRIu32"\n", D_L);
					N(n+4); debug_printf ("FileTimeExistence: %"PRIu32"\n", FileTimeExistence);
					if ((D_L > AttributeLength) ||
					    (D_L + 20 > AttributeLength))
					{
						N(n+4); debug_printf ("Error - Attribute too short to contain the data\n");
						break;
					}
					if (FileTimeExistence & 1)
					{
						if (index * 12 > D_L)
						{
							N(n+4); debug_printf ("Error - Buffer overrun\n");
						}
						N(n+4); debug_printf ("Information Created Date/Time: "); print_1_7_3 (b + 20 + index * 12); debug_printf ("\n");
						index++;
					}
					if (FileTimeExistence & 2)
					{
						if (index * 12 > D_L)
						{
							N(n+4); debug_printf ("Error - Buffer overrun\n");
						}
						N(n+4); debug_printf ("Information Last Modified Date/Time: "); print_1_7_3 (b + 20 + index * 12); debug_printf ("\n");
						index++;
					}
					if (FileTimeExistence & 4)
					{
						if (index * 12 > D_L)
						{
							N(n+4); debug_printf ("Error - Buffer overrun\n");
						}
						N(n+4); debug_printf ("Information Expiration Date/Time: "); print_1_7_3 (b + 20 + index * 12); debug_printf ("\n");
						index++;
					}
					if (FileTimeExistence & 8)
					{
						if (index * 12 > D_L)
						{
							N(n+4); debug_printf ("Error - Buffer overrun\n");
						}
						N(n+4); debug_printf ("Information Effective Date/Time: "); print_1_7_3 (b + 20 + index * 12); debug_printf ("\n");
						index++;
					}
				}
				break;
			case 12:
				N(n+3); debug_printf ("[Device Specification]\n"); // 4/14.10.7
				if (AttributeLength < 24)
				{
					debug_printf ("Error - Attribute too short\n");
					break;
				}
				{
					uint32_t I_UL = (b[15]<<24) | (b[14]<<16) | (b[13]<<8) | b[12];
					uint32_t major = (b[19]<<24) | (b[18]<<16) | (b[17]<<8) | b[16];
					uint32_t minor = (b[23]<<24) | (b[22]<<16) | (b[21]<<8) | b[20];
					N(n+4); debug_printf ("Implementation Use Length %"PRIu32"\n", I_UL);
					if ((I_UL        > AttributeLength) ||
					    ((I_UL + 24) > AttributeLength))
					{
						N(n+5); debug_printf ("WARNING - size will overflow buffer\n");
						I_UL = 0;
					}
					if (I_UL && I_UL < 32)
					{
						N(n+5); debug_printf ("WARNING - size is too small\n");
					}
					N(n+4); debug_printf ("Major: %"PRIu32"\n", major);
					N(n+4); debug_printf ("Minor: %"PRIu32"\n", minor);
					if (extendedattributes_target)
					{
						extendedattributes_target->HasMajorMinor = 1;
						extendedattributes_target->Major = major;
						extendedattributes_target->Minor = minor;
					}
					if (I_UL)
					{
						N(n+4); debug_printf ("Implementation Identifier: "); print_1_7_4 (b + 24, 1); debug_printf ("\n");
						if (I_UL > 32)
						{
							int j;
							N(n+5);
							for (j = 32; j < I_UL; j++)
							{
								debug_printf ("%s0x%02x", j!=32?" ":"", b[24+j]);
							}
							debug_printf ("\n");
						}
					}
				}
				break;
			case 2048:
				N(n+3); debug_printf ("[Implementation Use Extended Attribute]\n"); // 4/14.10.8
				if (AttributeLength < 48)
				{
					debug_printf ("Error - Attribute too short\n");
					break;
				}
				{
					uint16_t C_CheckSum = UDF_ComputeExtendedAttributeChecksum(b);
					uint16_t I_CheckSum = (b[49]<<8) | b[48];
					uint32_t I_UL = (b[15]<<24) | (b[14]<<16) | (b[13]<<8) | b[12];
					int j;
					N(n+4); debug_printf ("Implementation Use Length %"PRIu32"\n", I_UL);
					if ((I_UL        > AttributeLength) ||
					    ((I_UL + 48) > AttributeLength))
					{
						N(n+5); debug_printf ("WARNING - size will overflow buffer, capping (I_UL %d + 48 > AttributeLength %d)\n", I_UL, AttributeLength);
						I_UL = AttributeLength - 48;
					}
					N(n+4); debug_printf ("Implementation Identifier: "); print_1_7_4 (b + 16, 1); debug_printf ("\n");
					if (!memcmp (b + 16 + 1, "*UDF FreeEASpace", 17))
					{
						N(n+5); debug_printf ("[Free EASpace]\n"); // UDF 2.60 3.3.4.5.1.1 - this is a place-holder for potentially unused space that can be reused
						if (I_UL < 2)
						{
							N(n+5); debug_printf ("WARNING - buffer underflow %d < 2\n", I_UL);
							break;
						}
						N(n+6); debug_printf ("Header Checksum: %"PRIu16, I_CheckSum); if (I_CheckSum != C_CheckSum) debug_printf (" INVALID - expected %"PRIu16"\n", C_CheckSum); debug_printf ("\n");
						if (I_UL > 2)
						{
							N(n+6);
							for (j = 2; j < I_UL; j++)
							{
								debug_printf (" 0x%02x", b[48+j]);
							}
						}
					} else if (!memcmp (b + 16 + 1, "*UDF DVD CGMS Info", 19)) // UDF 2.60 3.3.4.5.1.2
					{
						N(n+5); debug_printf ("[DVD Copyright Management Information]\n");
						if (I_UL < 8)
						{
							N(n+5); debug_printf ("WARNING - buffer will underflow\n");
							break;
						}
						N(n+6); debug_printf ("Header Checksum: %"PRIu16, I_CheckSum); if (I_CheckSum != C_CheckSum) debug_printf (" INVALID - expected %"PRIu16"\n", C_CheckSum); debug_printf ("\n");
						N(n+6); debug_printf ("CGMS Information:              %"PRIu8"\n", b[50]); // Part of the DVD specification....
						N(n+6); debug_printf ("Data Structure Type:           %"PRIu8"\n", b[51]); // Part of the DVD specification....
						N(n+6); debug_printf ("Protection System Information: %"PRIu32"\n", (b[55]<<24) | (b[54]<<16) | (b[53]<<8) | b[52]);
					} else if (!memcmp (b + 16 + 1, "*UDF OS/2 EALength", 19))
					{
						N(n+5); debug_printf ("[OS2EALength]\n"); // UDF 2.60 3.3.4.5.3.1 - Optimization OS/2 needs during directory scanning in order to prepare something
						if (I_UL < 6)
						{
							N(n+5); debug_printf ("WARNING - buffer will underflow\n");
							break;
						}
						N(n+6); debug_printf ("Header Checksum: %"PRIu16, I_CheckSum); if (I_CheckSum != C_CheckSum) debug_printf (" INVALID - expected %"PRIu16"\n", C_CheckSum); debug_printf ("\n");
						N(n+6); debug_printf ("OS/2 Extended Attribute Length: %"PRIu32"\n", (b[53]<<24) | (b[52]<<16) | (b[51]<<8) | b[50]);
					} else if (!memcmp (b + 16 + 1, "*UDF Mac VolumeInfo", 20))
					{
						N(n+5); debug_printf ("[Macintosh volume information]\n"); // UDF 2.60 3.3.4.5.4.1
						if (I_UL < 58)
						{
							N(n+5); debug_printf ("WARNING - buffer will underflow\n");
							break;
						}
						N(n+6); debug_printf ("Header Checksum: %"PRIu16, I_CheckSum); if (I_CheckSum != C_CheckSum) debug_printf (" INVALID - expected %"PRIu16"\n", C_CheckSum); debug_printf ("\n");
						N(n+6); debug_printf ("Last Modification Date: "); print_1_7_3 (b+50); debug_printf ("\n");
						N(n+6); debug_printf ("Last Backup Date:        "); print_1_7_3 (b+62); debug_printf ("\n");
						N(n+6); debug_printf ("Volume Finder Information:\n");
						N(n+6);
						for (j = 0; j < 32; j++)
						{
							debug_printf (" 0x%02x", b[48+26+j]);
						}
						debug_printf ("\n");
					} else if (!memcmp (b + 16 + 1, "*UDF Mac FinderInfo", 20))
					{
						N(n+5); debug_printf ("[Macintosh Finder information]\n"); // UDF 2.60 3.3.4.5.4.2
						if (isfile)
						{
							if (I_UL < (40))
							{
								N(n+5); debug_printf ("WARNING - buffer will underflow\n");
								break;
							}
						} else {
							if (I_UL < (48))
							{
								N(n+5); debug_printf ("WARNING - buffer will underflow\n");
								break;
							}
						}

						N(n+6); debug_printf ("Header Checksum: %"PRIu16, I_CheckSum); if (I_CheckSum != C_CheckSum) debug_printf (" INVALID - expected %"PRIu16"\n", C_CheckSum); debug_printf ("\n");
						// 2 bytes padding, should be zero...
						N(n+6); debug_printf ("Parent Directory ID: %"PRIu32"\n", (b[55]<<24) | (b[54]<<16) | (b[53]<<8) | b[52]);
						if (isfile)
						{
							N(n+6); debug_printf ("File Information.FdType:             %"PRIu32"\n", (b[59]<<24) | (b[58]<<16) | (b[57]<<8) | b[56]);
							N(n+6); debug_printf ("File Information.FdCreator:          %"PRIu32"\n", (b[63]<<24) | (b[62]<<16) | (b[61]<<8) | b[60]);
							N(n+6); debug_printf ("File Information.FdFlags:            %"PRIu16"\n",                             (b[65]<<8) | b[64]);
							N(n+6); debug_printf ("File Information.FdLocation.V:       %"PRIu16"\n",                             (b[67]<<8) | b[66]);
							N(n+6); debug_printf ("File Information.FdLocation.H:       %"PRIu16"\n",                             (b[69]<<8) | b[68]);
							N(n+6); debug_printf ("File Information.FdFldr:             %"PRIu16"\n",                             (b[71]<<8) | b[70]);
							N(n+6); debug_printf ("File Extended Information.FdIconID:  %"PRIu16"\n",                             (b[73]<<8) | b[72]);
							// 6 bytes of unused data....
							N(n+6); debug_printf ("File Extended Information.FdScript:  %"PRIu8"\n",                                           b[80]);
							N(n+6); debug_printf ("File Extended Information.FdXflags:  %"PRIu8"\n",                                           b[81]);
							N(n+6); debug_printf ("File Extended Information.FrComment: %"PRIu16"\n",                             (b[83]<<8) | b[82]);
							N(n+6); debug_printf ("File Extended Information.FrPutAway: %"PRIu32"\n", (b[87]<<24) | (b[86]<<16) | (b[85]<<8) | b[84]);
							N(n+6); debug_printf ("Resource Fork Data Length:       %"PRIu32"\n", (b[91]<<24) | (b[90]<<16) | (b[89]<<8) | b[88]);
							N(n+6); debug_printf ("Resource Fork Allocation Length: %"PRIu32"\n", (b[95]<<24) | (b[94]<<16) | (b[93]<<8) | b[92]);
						} else {
							N(n+6); debug_printf ("Directory Information.FrRect.Top:           %"PRIu16"\n",                             (b[57]<<8) | b[56]);
							N(n+6); debug_printf ("Directory Information.FrRect.Left:          %"PRIu16"\n",                             (b[59]<<8) | b[58]);
							N(n+6); debug_printf ("Directory Information.FrRect.Bottom:        %"PRIu16"\n",                             (b[61]<<8) | b[60]);
							N(n+6); debug_printf ("Directory Information.FrRect.Right:         %"PRIu16"\n",                             (b[63]<<8) | b[62]);
							N(n+6); debug_printf ("Directory Information.FrFlags:              %"PRIu16"\n",                             (b[65]<<8) | b[64]);
							N(n+6); debug_printf ("Directory Information.FrLocation.V:         %"PRIu16"\n",                             (b[67]<<8) | b[66]);
							N(n+6); debug_printf ("Directory Information.FrLocation.H:         %"PRIu16"\n",                             (b[69]<<8) | b[68]);
							N(n+6); debug_printf ("Directory Information.FrView:               %"PRIu16"\n",                             (b[71]<<8) | b[70]);
							N(n+6); debug_printf ("Directory Extended Information.FrScroll.V:  %"PRIu16"\n",                             (b[73]<<8) | b[72]);
							N(n+6); debug_printf ("Directory Extended Information.FrScroll.H:  %"PRIu16"\n",                             (b[75]<<8) | b[74]);
							N(n+6); debug_printf ("Directory Extended Information.FrOpenChain: %"PRIu32"\n", (b[79]<<24) | (b[78]<<16) | (b[77]<<8) | b[76]);
							N(n+6); debug_printf ("Directory Extended Information.FrScript:    %"PRIu8"\n",                                           b[80]);
							N(n+6); debug_printf ("Directory Extended Information.FrXflags:    %"PRIu8"\n",                                           b[81]);
							N(n+6); debug_printf ("Directory Extended Information.FrComment:   %"PRIu16"\n",                             (b[83]<<8) | b[82]);
							N(n+6); debug_printf ("Directory Extended Information.FrPutAway:   %"PRIu32"\n", (b[87]<<24) | (b[86]<<16) | (b[85]<<8) | b[84]);
						}
					} else if (!memcmp (b + 16 + 1, "*UDF OS/400 DirInfo", 20))
					{
						N(n+5); debug_printf ("[OS/400 extended directory information]\n"); // UDF 2.60 3.3.4.5.6.1 - Cache for OS/400 Directory listing
						if (I_UL < (48))
						{
							N(n+5); debug_printf ("WARNING - buffer will underflow\n");
							break;
						}

						N(n+6); debug_printf ("Header Checksum: %"PRIu16, I_CheckSum); if (I_CheckSum != C_CheckSum) debug_printf (" INVALID - expected %"PRIu16"\n", C_CheckSum); debug_printf ("\n");
						// 2 bytes padding, should be zero...
						// 44 bytes of DirectoryInfo - information propetiary by IBM
					} else {
						N(n+5);
						for (j = 0; j < I_UL; j++)
						{
							debug_printf ("%s0x%02x", j?" ":"", b[48+j]);
						}
					}
					debug_printf ("\n");
				}
				break;
			case 65536: N(n+3); debug_printf ("[Application Use Extended Attribute]\n"); // 4/14.10.9
				if (AttributeLength < 48)
				{
					debug_printf ("Error - Attribute too short\n");
					break;
				}
				{
					uint16_t C_CheckSum = UDF_ComputeExtendedAttributeChecksum(b);
					uint16_t I_CheckSum = (b[49]<<8) | b[48];
					uint32_t AU_L = (b[15]<<24) | (b[14]<<16) | (b[13]<<8) | b[12];
					int j;
					N(n+4); debug_printf ("Application Use Length %"PRIu32"\n", AU_L);
					if ((AU_L        > AttributeLength) ||
					    ((AU_L + 48) > AttributeLength))
					{
						N(n+5); debug_printf ("WARNING - size will overflow buffer, capping (AU_L %d + 48 > AttributeLength %d)\n", AU_L, AttributeLength);
						AU_L = AttributeLength - 48;
					}
					N(n+4); debug_printf ("Application Identifier: "); print_1_7_4 (b + 16, 1); debug_printf ("\n");

					if (!memcmp (b + 16 + 1, "*UDF FreeEASpace", 17))
					{
						N(n+5); debug_printf ("[Free EASpace]\n"); // UDF 2.60 3.3.4.5.1.1 - this is a place-holder for potentially unused space that can be reused
						if (AU_L < 2)
						{
							N(n+5); debug_printf ("WARNING - buffer underflow %d < 2\n", AU_L);
							break;
						}
						N(n+6); debug_printf ("Header Checksum: %"PRIu16, I_CheckSum); if (I_CheckSum != C_CheckSum) debug_printf (" INVALID - expected %"PRIu16"\n", C_CheckSum); debug_printf ("\n");
						if (AU_L > 2)
						{
							N(n+6);
							for (j = 2; j < AU_L; j++)
							{
								debug_printf (" 0x%02x", b[48+j]);
							}
						}
					} else {
						N(n+5);
						for (j = 0; j < AU_L; j++)
						{
							debug_printf ("%s0x%02x", j?" ":"", b[48+j]);
						}
					}
					debug_printf ("\n");
				}

				break;
		}

		b += AttributeLength;
		l -= AttributeLength;
	}
}

static void ExtendedAttributesInline (int n, uint8_t *buffer, uint32_t ExtentLength, uint32_t ExtentLocation, int isfile, struct UDF_FileEntry_t *extendedattributes_target)
{
	N(n); debug_printf ("[Extended Attribute Header Descriptor]\n");
	ExtendedAttributesCommon (n, buffer, ExtentLength, ExtentLocation, isfile, extendedattributes_target);
}

static void ExtendedAttributes (int n, struct cdfs_disc_t *disc, struct UDF_longad *L, int isfile, struct UDF_FileEntry_t *extendedattributes_target)
{
	struct UDF_LogicalVolume_Common *lv = UDF_GetLogicalPartition (disc, L->ExtentLocation.PartitionReferenceNumber);
	uint8_t *buffer;

	N(n); debug_printf ("[Extended Attribute Header Descriptor]\n");

	if (L->ExtentLength < 24)
	{
		N(n+1); debug_printf ("Error - Length too small to contain header\n");
		return;
	}

	if (!lv)
	{
		N(n+1); debug_printf ("Warning - unable to find partition for ExtendedAttributes\n");
		return;
	}

	if (L->ExtentLength < SECTORSIZE)
	{
		N(n+1); debug_printf ("Warning - ExtentLength < SECTORSIZE\n");
	}

	buffer = UDF_FetchSectors (n+1, disc, &lv->PartitionCommon, L->ExtentLocation.LogicalBlockNumber, L->ExtentLength);
	if (!buffer)
	{
		N(n+1); debug_printf ("Error fetching data");
		return;
	}
	ExtendedAttributesCommon (n, buffer, L->ExtentLength, L->ExtentLocation.LogicalBlockNumber, isfile, extendedattributes_target);
	free (buffer);
}

/* 0x0108 - No strategy possible */
static void SpaceBitMapCommon (int n, uint8_t *buffer, uint32_t ExtentLocation, uint32_t ExtentLength)
{
	int i;
#ifdef CDFS_DEBUG
	uint32_t bits;
#endif
	uint32_t bytes;
	uint16_t TagIdentifier;

#if 0
	if (ExtentLength < 24)
	{
		N(n); debug_printf ("Error - Length too small to contain header\n");
		return;
	}
#endif

	if (print_tag_format (n, "", buffer, ExtentLocation, 1, &TagIdentifier))
	{
		free (buffer);
		return;
	}
	if (TagIdentifier != 0x0108)
	{
		N(n); debug_printf ("Error - Wrong TagIdentifier\n");
		free (buffer);
		return;
	}

#ifdef CDFS_DEBUG
	bits = (buffer[19]<<24) | (buffer[18]<<16) | (buffer[17]<<8) | buffer[16];
#endif
	bytes = (buffer[23]<<24) | (buffer[22]<<16) | (buffer[21]<<8) | buffer[20];
	N(n); debug_printf ("Number of bits:  %d\n", bits);
	N(n); debug_printf ("Number of bytes: %d\n", bytes);
	if (bytes > (ExtentLength - 24))
	{
		N(n); debug_printf ("Warning - too big, clamping value\n");
		bytes = ExtentLength - 24;
	}
	N(n); debug_printf ("Map:\n");
	for (i=0; i < bytes; i++)
	{
		if ((i & (32-1)) == 0)
		{
			if (i) debug_printf ("\n");
			N(n+1);
		}
		debug_printf (" 0x%02x", buffer[24+i]);
	}
	debug_printf ("\n");
}

static void SpaceBitMapInline (int n, uint8_t *buffer, uint32_t ExtentLocation, uint32_t ExtentLength)
{
	N(n); debug_printf ("[MetaData Space Bitmap]\n");
	if (ExtentLength < 24)
	{
		N(n+1); debug_printf ("Error - Length too small to contain header\n");
		return;
	}

	if (ExtentLength < SECTORSIZE)
	{
		N(n+1); debug_printf ("Warning - ExtentLength < SECTORSIZE\n");
	}

	SpaceBitMapCommon (n + 1, buffer, ExtentLocation, ExtentLength);
}

static void SpaceBitMap (int n, struct cdfs_disc_t *disc, struct UDF_Partition_Common *PartitionCommon, struct UDF_shortad *L, const char *prefix)
{
	uint8_t *buffer;

	N(n); debug_printf ("[%s Space Bitmap]\n", prefix);

	if (L->ExtentLength < 24)
	{
		N(n+1); debug_printf ("Error - Length too small to contain header\n");
		return;
	}

	if (L->ExtentLength < SECTORSIZE)
	{
		N(n+1); debug_printf ("Warning - ExtentLength < SECTORSIZE\n");
	}

	buffer = UDF_FetchSectors (n+1, disc, PartitionCommon, L->ExtentPosition, L->ExtentLength);
	if (!buffer)
	{
		N(n+1); debug_printf ("Error fetching data");
		return;
	}
	SpaceBitMapCommon (n, buffer, L->ExtentPosition, L->ExtentLength);
	free (buffer);
}

/* 0x0109 - Shall not be recorded according to UDF standard, strategy 4096 implemented - not tested*/
static void PartitionIntegrityEntry (int n, struct cdfs_disc_t *disc, struct UDF_Partition_Common *PartitionCommon, struct UDF_shortad *L, int recursion /* set to zero */)
{
	uint8_t *buffer;
	uint16_t Flags;
	enum eFileType FileType;
	uint16_t TagIdentifier;
	int i;

	int strategy4096 = 0;
	struct UDF_longad NLong;
	struct UDF_shortad NShort;
	struct UDF_PhysicalPartition_t *NPartition;

	N(n); debug_printf ("[Partition Integrity Entry]\n");
	N(n+1); debug_printf ("Warning - this entry should not be recorded according to UDF 2.60 standard\n");

	if (L->ExtentLength < 512)
	{
		N(n+1); debug_printf ("Error - Length too small to contain header\n");
		return;
	}

	if (L->ExtentLength != SECTORSIZE)
	{
		N(n+1); debug_printf ("Warning - ExtentLength != SECTORSIZE\n");
	}

	buffer = UDF_FetchSectors (n+1, disc, PartitionCommon, L->ExtentPosition, SECTORSIZE);
	if (!buffer)
	{
		N(n+1); debug_printf ("Error fetching data");
		return;
	}

	if (print_tag_format (n+1, "", buffer, L->ExtentPosition, 1, &TagIdentifier))
	{
		free (buffer);
		return;
	}
	if (TagIdentifier != 0x0109)
	{
		N(n+1); debug_printf ("Error - Wrong TagIdentifier\n");
		free (buffer);
		return;
	}

	print_4_14_6 (n+1, "ICBTab.", buffer + 16, &Flags, &FileType, &strategy4096);
	if (FileType != FILETYPE_PARTITION_INTEGRITY_ENTRY)
	{
		N(n+1); debug_printf ("Error - Wrong FileType\n");
		free (buffer);
		return;
	}

	N(n+1); debug_printf ("Recording Date and Time: "); print_1_7_3 (buffer + 36); debug_printf ("\n");

	N(n+1); debug_printf ("Integrity Type: %d\n", buffer[48]);
	switch (buffer[48])
	{
		case 0: N(n+2); debug_printf ("entry is an Open Integrity Entry\n"); break;
		case 1: N(n+2); debug_printf ("entry is an Close Integrity Entry\n"); break;
		case 2: N(n+2); debug_printf ("entry is an Stable Integrity Entry\n"); break;
		default: N(n+2); debug_printf ("Reserved value\n"); break;
	}

	/* 175 reserved bytes */

	debug_printf ("Implementation Identifier: "); print_1_7_4 (buffer + 224, 1); debug_printf ("\n");

	N(n+1); debug_printf ("Implementation Use:\n");
	for (i=256; i < 512; i++)
	{
		debug_printf (" 0x%02x", buffer[i]);
	}
	debug_printf ("\n");

	free(buffer);
	buffer = 0;

	if (!strategy4096)
	{
		return;
	}

	/* try to find newer version */
	if (recursion > MAX_INDIRECT_RECURSION)
	{
		N(n+2); debug_printf ("Error - indirect recursion limit reached\n");
		return;
	}

	if (IndirectEntry (n+2, disc, PartitionCommon, L->ExtentPosition + 1, &NLong))
	{
		return;
	}
	NPartition = UDF_GetPhysicalPartition (disc, NLong.ExtentLocation.PartitionReferenceNumber);
	if (!NPartition)
	{
		N(n+2); debug_printf ("Error - Partition not found\n");
		return;
	}
	UDF_shortad_from_longad (&NShort, &NLong);
	PartitionIntegrityEntry (n+3, disc, &NPartition->PartitionCommon, &NShort, recursion + 1);
}

/////////////////////////////////////////////////////////////////// Volume /////////////////////////////////////////////////////////////////////////////////////


/* 0x0001 */
static void PrimaryVolumeDescriptor (int n, struct cdfs_disc_t *disc, uint8_t *buffer)
{
	int i;
	uint32_t VolumeDescriptorSequenceNumber;
	char *VolumeIdentifier = 0;
	uint16_t VolumeSequenceNumber;
	uint16_t Flags;
	struct UDF_extent_ad VolumeAbstract;
	struct UDF_extent_ad VolumeCopyright;
	uint32_t CharacterSetList;

	N(n);   debug_printf ("[Primary Volume Descriptor]\n");
	VolumeDescriptorSequenceNumber =                                           (buffer[19] << 24) | (buffer[18] << 16) | (buffer[17] << 8) | buffer[16];
	N(n+1); debug_printf ("Volume Descriptor Sequence Number:  %" PRId32 "\n", VolumeDescriptorSequenceNumber);
	N(n+1); debug_printf ("Primary Volume Descriptor Number:   %" PRId32 "\n", (buffer[23] << 24) | (buffer[22] << 16) | (buffer[21] << 8) | buffer[20]);
	N(n+1); debug_printf ("Volume Identifier:                  "); print_1_7_2_12 (buffer + 24, 32, buffer + 200 /* Descriptor Character Set */, &VolumeIdentifier); debug_printf ("\n");
	VolumeSequenceNumber =                                                                                               (buffer[57] << 8) | buffer[56];
	N(n+1); debug_printf ("Volume Sequence Number:             %" PRId16 "\n", VolumeSequenceNumber);
	N(n+1); debug_printf ("Maximum Volume Sequence Number:     %" PRId16 "\n",                                           (buffer[59] << 8) | buffer[58]);
	N(n+1); debug_printf ("Interchange Level:                  %" PRId16 "\n",                                           (buffer[61] << 8) | buffer[60]);
	N(n+1); debug_printf ("Maximum Interchange Level:          %" PRId16 "\n",                                           (buffer[63] << 8) | buffer[62]);
	CharacterSetList =                                                         (buffer[67] << 24) | (buffer[66] << 16) | (buffer[65] << 8) | buffer[64];
	N(n+1); debug_printf ("Character Set List:                 %" PRId32 "\n", CharacterSetList);
	if (CharacterSetList & 0x001) { N(n+2); debug_printf ("CS0 in use\n"); }
	if (CharacterSetList & 0x002) { N(n+2); debug_printf ("CS1 in use\n"); }
	if (CharacterSetList & 0x004) { N(n+2); debug_printf ("CS2 in use\n"); }
	if (CharacterSetList & 0x008) { N(n+2); debug_printf ("CS3 in use\n"); }
	if (CharacterSetList & 0x010) { N(n+2); debug_printf ("CS4 in use\n"); }
	if (CharacterSetList & 0x020) { N(n+2); debug_printf ("CS5 in use\n"); }
	if (CharacterSetList & 0x040) { N(n+2); debug_printf ("CS6 in use\n"); }
	if (CharacterSetList & 0x080) { N(n+2); debug_printf ("CS7 in use\n"); }
	if (CharacterSetList & 0x100) { N(n+2); debug_printf ("CS8 in use\n"); }
	N(n+1); debug_printf ("Maximum Character Set List:         %" PRId32 "\n", (buffer[71] << 24) | (buffer[70] << 16) | (buffer[69] << 8) | buffer[68]);
	N(n+1); debug_printf ("Volume Set Identifier:              "); print_1_7_2_12_VolumeSetIdentifier (buffer + 72, 128, buffer + 200 /* Descriptor Character Set */); debug_printf ("\n");
	N(n+1); debug_printf ("Descriptor Character Set:           "); print_1_7_2_1 (buffer + 200); debug_printf ("\n");
	N(n+1); debug_printf ("Explanatory Character Set:          "); print_1_7_2_1 (buffer + 264); debug_printf ("\n");
	UDF_extent_ad_from_data (n+1, "Volume Abstract.", &VolumeAbstract, buffer + 328);
	UDF_extent_ad_from_data (n+1, "Volume Copyright Notice.", &VolumeCopyright, buffer + 336);
	N(n+1); debug_printf ("Application Identifier:             "); print_1_7_4 (buffer + 344, 0); debug_printf ("\n");
	N(n+1); debug_printf ("Recording Date and Time:            "); print_1_7_3 (buffer + 376); debug_printf ("\n");
	N(n+1); debug_printf ("Implementation Identifier:          "); print_1_7_4 (buffer + 388, 1); debug_printf ("\n");
	N(n+1); debug_printf ("Implementation Use:                 \"");
	for (i=420; i < 484; i++)
	{
		if ((buffer[i] >= 0x20) && (buffer[i] < 0x7f) && (buffer[i] != '"') && (buffer[i] != '\\'))
		{
			debug_printf ("%c", buffer[i]);
		} else {
			debug_printf ("\\x%02x", buffer[i]);
		}
	}
	debug_printf("\"\n");
	N(n+1); debug_printf ("Predecessor Volume Descriptor\n");
	N(n+1); debug_printf (" Sequence Location:                 %" PRId32 "\n", (buffer[487] << 24) | (buffer[486] << 16) | (buffer[485] << 8) | buffer[484]);
	Flags = (buffer[489] << 8) | buffer[488];
	N(n+1); debug_printf ("Flags:                              %" PRId16 "\n", Flags);
	if (Flags & 0x01) {N(n+2); debug_printf ("Volume Set Identification are all common / copies\n");} else { N(n+2); debug_printf ("Volume Set Identification are all unique\n"); }

// TODO Copyright       (uses "Explanatory Character Set")
// TODO Volume Abstract (uses "Explanatory Character Set")

	UDF_Session_Add_PrimaryVolumeDescriptor (disc, VolumeDescriptorSequenceNumber, VolumeIdentifier, VolumeSequenceNumber);
}

/* 0x0004 */
static void ImplementationUseVolumeDescriptor (int n, struct cdfs_disc_t *disc, uint8_t *buffer)
{
	int i;

	N(n);   debug_printf ("[Implementation Use Volume Descriptor]\n");
	N(n+1); debug_printf ("Volume Descriptor Sequence Number:  %" PRId32 "\n", (buffer[19] << 24) | (buffer[18] << 16) | (buffer[17] << 8) | buffer[16]);
// we configure one of the possible CS0..CS8 here
	N(n+1); debug_printf ("Implementation Identifier:          "); print_1_7_4 (buffer + 20, 1); debug_printf ("\n");
	N(n+1); debug_printf ("Implementation Use:                 \"");
	if (!memcmp (buffer + 20 + 1, "*UDF LV Info", 13))
	{
		N(n+1); debug_printf ("Implementation Use:\n");
		N(n+2); debug_printf ("LVICharset:                "); print_1_7_2_1 (buffer + 52); debug_printf ("\n");                     /*  52 +  64 = 116 */
		N(n+2); debug_printf ("LogicalVolumeIdentifier:   "); print_1_7_2_12 (buffer + 116, 128, buffer + 52, 0 /* output */); debug_printf ("\n"); /* 116 + 128 = 244 */
		N(n+2); debug_printf ("LVInfo1:                   "); print_1_7_2_12 (buffer + 244,  36, buffer + 52, 0 /* output */); debug_printf ("\n");  /* 244 +  36 = 280 */
		N(n+2); debug_printf ("LVInfo2:                   "); print_1_7_2_12 (buffer + 280,  36, buffer + 52, 0 /* output */); debug_printf ("\n");  /* 280 +  36 = 316 */
		N(n+2); debug_printf ("LVInfo3:                   "); print_1_7_2_12 (buffer + 316,  36, buffer + 52, 0 /* output */); debug_printf ("\n");  /* 316 +  36 = 352 */
		N(n+2); debug_printf ("Implementation Identifier: "); print_1_7_4 (buffer + 352, 1); debug_printf ("\n");                   /* 352 +  32 = 384 */
		N(n+2); debug_printf ("Implementation Use:        \"");
		for (i=384; i < 512; i++)
		{
			if ((buffer[i] >= 0x20) && (buffer[i] < 0x7f) && (buffer[i] != '"') && (buffer[i] != '\\'))
			{
				debug_printf ("%c", buffer[i]);
			} else {
				debug_printf ("\\x%02x", buffer[i]);
			}
		}
		debug_printf("\"\n");
	} else {
		N(n+1); debug_printf ("Implementation Use:                 \"");
		for (i=52; i < 512; i++)
		{
			if ((buffer[i] >= 0x20) && (buffer[i] < 0x7f) && (buffer[i] != '"') && (buffer[i] != '\\'))
			{
				debug_printf ("%c", buffer[i]);
			} else {
				debug_printf ("\\x%02x", buffer[i]);
			}
		}
		debug_printf("\"\n");
	}
}


/* 0x0005 */
static void PartitionDescriptor (int n, struct cdfs_disc_t *disc, uint8_t *buffer)
{
	int i;
	uint32_t VolumeDescriptorSequenceNumber;
	uint16_t PartitionFlags;
	uint16_t PartitionNumber;
	uint32_t AccessType;
	uint32_t PartitionStartingLocation;
	uint32_t PartitionLength;
	enum PhysicalPartition_Content Content = PhysicalPartition_Content_Unknown;

	struct UDF_PhysicalPartition_t *PhysicalPartition;

	struct UDF_shortad UnallocatedSpaceTable;
	struct UDF_shortad UnallocatedSpaceBitMap;
	struct UDF_shortad PartitionIntegrityTable;
	struct UDF_shortad FreedSpaceTable;
	struct UDF_shortad FreedSpaceBitMap;

	N(n);   debug_printf ("[Partition Descriptor]\n");
	VolumeDescriptorSequenceNumber = (buffer[19] << 24) | (buffer[18] << 16) | (buffer[17] << 8) | buffer[16];
// we configure one of the possible CS0..CS8 here
	PartitionFlags =  (buffer[21] << 8) | buffer[20];
	PartitionNumber = (buffer[23] << 8) | buffer[22];
	N(n+1); debug_printf ("Volume Descriptor Sequence Number:  %" PRId32 "\n", VolumeDescriptorSequenceNumber);
// we configure one of the possible CS0..CS8 here
	N(n+1); debug_printf ("Partition Flags:                    %" PRId16 "\n", PartitionFlags);
	N(n+2); if (PartitionFlags & 0x0001) debug_printf ("Space unallocated\n"); else debug_printf ("Space allocated\n");
	N(n+1); debug_printf ("Partition Number:                   %" PRId16 "\n", PartitionNumber);
	N(n+1); debug_printf ("Partition Contents:                 "); print_1_7_4 (buffer + 24, 0); debug_printf ("\n");

	     if (!memcmp (buffer + 25, "+FDC01", 7)) Content = PhysicalPartition_Content_ECMA107__FDC01;
	else if (!memcmp (buffer + 25, "+CD001", 7)) Content = PhysicalPartition_Content_ECMA119__CD001;
	else if (!memcmp (buffer + 25, "+NSR02", 7)) Content = PhysicalPartition_Content_ECMA167__NSR02;
	else if (!memcmp (buffer + 25, "+NSR03", 7)) Content = PhysicalPartition_Content_ECMA167__NSR03;
	else if (!memcmp (buffer + 25, "+CDW02", 7)) Content = PhysicalPartition_Content_ECMA168__CDW02;

	if ((Content == PhysicalPartition_Content_ECMA167__NSR03) || (Content == PhysicalPartition_Content_ECMA167__NSR02))
	{
		// EMCA-167 3nd edition - 14.3 Partition Header Descriptor
		// EMCA-167 3rd edition - 14.3 Partition Header Descriptor

		N(n+1); debug_printf ("Partition Contents Use:\n");
		N(n+2); debug_printf ("[Partition Header Descriptor]\n");
		UDF_shortad_from_data (n+3, "Unallocated Space Table.", &UnallocatedSpaceTable,   buffer + 56 +  0);
		UDF_shortad_from_data (n+3, "Unallocated Space Bitmap.", &UnallocatedSpaceBitMap,  buffer + 56 +  8);
		UDF_shortad_from_data (n+3, "Partition Integrity Table.", &PartitionIntegrityTable, buffer + 56 + 16);
		UDF_shortad_from_data (n+3, "Freed Space Table.", &FreedSpaceTable,         buffer + 56 + 24);
		UDF_shortad_from_data (n+3, "Freed Space Bitmap.", &FreedSpaceBitMap,        buffer + 56 + 32);
		N(n+1);
		for (i=56 + 44; i < 184; i++)
		{
			debug_printf (" 0x%02x", buffer[i]);
		}
		debug_printf ("\n");
	} else {
		N(n+1); debug_printf ("Partition Contents Use:             \"");
		for (i=56; i < 184; i++)
		{
			if ((buffer[i] >= 0x20) && (buffer[i] < 0x7f) && (buffer[i] != '"') && (buffer[i] != '\\'))
			{
				debug_printf ("%c", buffer[i]);
			} else {
				debug_printf ("\\x%02x", buffer[i]);
			}
		}
		debug_printf("\"\n");
	}
	AccessType = (buffer[187] << 24) | (buffer[186] << 16) | (buffer[185] << 8) | buffer[184];
	N(n+1); debug_printf ("Access Type:                        %" PRId32 " ", AccessType);
	switch (AccessType)
	{
		case 0: debug_printf ("unspecified\n"); break;
		case 1: debug_printf ("Read only\n"); break;
		case 2: debug_printf ("Write once\n"); break;
		case 3: debug_printf ("Rewritable\n"); break;
		case 4: debug_printf ("Overwritable\n"); break;
		default: debug_printf("???\n"); break;
	}
	PartitionStartingLocation = (buffer[191] << 24) | (buffer[190] << 16) | (buffer[189] << 8) | buffer[188];
	PartitionLength =           (buffer[195] << 24) | (buffer[194] << 16) | (buffer[193] << 8) | buffer[192];
	N(n+1); debug_printf ("Partition Starting Location:        %" PRId32 "\n", PartitionStartingLocation);
	N(n+1); debug_printf ("Partition Length:                   %" PRId32 "\n", PartitionLength);
	N(n+1); debug_printf ("Implementation Identifier:          "); print_1_7_4 (buffer + 196, 1); debug_printf ("\n");
	N(n+1); debug_printf ("Implementation Use:                 \"");
	for (i=228; i < 356; i++)
	{
		if ((buffer[i] >= 0x20) && (buffer[i] < 0x7f) && (buffer[i] != '"') && (buffer[i] != '\\'))
		{
			debug_printf ("%c", buffer[i]);
		} else {
			debug_printf ("\\x%02x", buffer[i]);
		}
	}
	debug_printf ("\"\n");

	UDF_Session_Add_PhysicalPartition (disc, VolumeDescriptorSequenceNumber, PartitionNumber, Content, SECTORSIZE, PartitionStartingLocation, PartitionLength);
	PhysicalPartition = UDF_GetPhysicalPartition (disc, PartitionNumber);
	if (PhysicalPartition)
	{
		if (UnallocatedSpaceTable.ExtentLength)
		{
			SpaceEntry              (n+4, disc, &PhysicalPartition->PartitionCommon, &UnallocatedSpaceTable, "Unallocated", 0);
		}
		if (UnallocatedSpaceBitMap.ExtentLength)
		{
			SpaceBitMap             (n+4, disc, &PhysicalPartition->PartitionCommon, &UnallocatedSpaceBitMap, "Unallocated");
		}
		if (PartitionIntegrityTable.ExtentLength)
		{
			PartitionIntegrityEntry (n+4, disc, &PhysicalPartition->PartitionCommon, &PartitionIntegrityTable, 0);
		}
		if (FreedSpaceTable.ExtentLength)
		{
			SpaceEntry              (n+4, disc, &PhysicalPartition->PartitionCommon, &FreedSpaceTable, "Freed", 0);
		}
		if (FreedSpaceBitMap.ExtentLength)
		{
			SpaceBitMap             (n+4, disc, &PhysicalPartition->PartitionCommon, &FreedSpaceBitMap, "Freed");
		}
	}
}

/* 0x0006 */
static void LogicalVolumeDescriptor (int n, struct cdfs_disc_t *disc, uint8_t *buffer)
{
	struct UDF_LogicalVolumes_t *volume;
	uint32_t VolumeDescriptorSequenceNumber;
	char * LogicalVolumeIdentifier = 0;
	uint32_t MT_L, N_PM;

	struct UDF_extent_ad IntegritySequenceExtent;
	int i, j;
	uint8_t *b;
	int l;

	N(n);   debug_printf ("[Logical Volume Descriptor]\n");
	VolumeDescriptorSequenceNumber = (buffer[19] << 24) | (buffer[18] << 16) | (buffer[17] << 8) | buffer[16];
	N(n+1); debug_printf ("Volume Descriptor Sequence Number:  %" PRId32 "\n", VolumeDescriptorSequenceNumber);
// we configure one of the possible CS0..CS8 here
	N(n+1); debug_printf ("Descriptor Character Set:           "); print_1_7_2_1 (buffer + 20); debug_printf ("\n");
	N(n+1); debug_printf ("Logical Volume Identifier:          "); print_1_7_2_12 (buffer + 84, 128, buffer + 20 /* Descriptor Character Set */, &LogicalVolumeIdentifier); debug_printf ("\n");
	N(n+1); debug_printf ("Logical Block Size:                 %" PRId32 "\n", (buffer[215] << 24) | (buffer[214] << 16) | (buffer[213] << 8) | buffer[212]);
	N(n+1); debug_printf ("Domain Identifier:                  "); print_1_7_4 (buffer + 216, 0); debug_printf ("\n");
	volume = UDF_LogicalVolumes_Create (VolumeDescriptorSequenceNumber, LogicalVolumeIdentifier, buffer + 20);
	N(n+1); debug_printf ("Logical Volume Contents Use:        \"");
	for (i=248; i < 264; i++)
	{
		if ((buffer[i] >= 0x20) && (buffer[i] < 0x7f) && (buffer[i] != '"') && (buffer[i] != '\\'))
		{
			debug_printf ("%c", buffer[i]);
		} else {
			debug_printf ("\\x%02x", buffer[i]);
		}
	}
	debug_printf ("\"\n");
	if (!memcmp (buffer + 216 + 1, "*OSTA UDF Compliant", 20))
	{ // OSTA UDF2.60 2.2.4.4
		struct UDF_longad RootLocation;
		UDF_longad_from_data (n+2, "RootLocation.", &RootLocation, buffer + 248);
		UDF_LogicalVolume_FileSetDescriptor_SetLocation (volume, RootLocation.ExtentLocation.LogicalBlockNumber, RootLocation.ExtentLocation.PartitionReferenceNumber);
		// Ignore AdImpUse.flags
	}


	MT_L = (buffer[267] << 24) | (buffer[266] << 16) | (buffer[265] << 8) | buffer[264];
	N_PM = (buffer[271] << 24) | (buffer[270] << 16) | (buffer[269] << 8) | buffer[268];
	N(n+1); debug_printf ("Map Table Length:                   %" PRId32 "\n", MT_L);
	N(n+1); debug_printf ("Number of Partition Maps:           %" PRId32 "\n", N_PM);
	N(n+1); debug_printf ("Implementation Identifier:          "); print_1_7_4 (buffer + 272, 1); debug_printf ("\n");
	N(n+1); debug_printf ("Implementation Use:                 \"");
	for (i=304; i < 432; i++)
	{
		if ((buffer[i] >= 0x20) && (buffer[i] < 0x7f) && (buffer[i] != '"') && (buffer[i] != '\\'))
		{
			debug_printf ("%c", buffer[i]);
		} else {
			debug_printf ("\\x%02x", buffer[i]);
		}
	}
	debug_printf ("\"\n");
	UDF_extent_ad_from_data (n+1, "Integrity Sequence Extent.", &IntegritySequenceExtent, buffer + 432);

	b = buffer + 440;
	l = MT_L;
	if ((l + 440) > SECTORSIZE)
	{
		N(n+1); debug_printf ("WARNING: buffer overflow, length will be clamped\n");
		l = SECTORSIZE - 440;
	}
	N(n+1); debug_printf ("Partition Maps:\n");
	for (i=0; i < N_PM; i++)
	{
		uint8_t PM_L;
		if (l < 2)
		{
			N(n+2); debug_printf ("WARNING: ran out of data in buffer\n");
			break;
		}
		N(n+2); debug_printf ("%d.Partition Map Type: ", i);
		switch (b[0])
		{
			case 0: debug_printf ("unspecified\n"); break;
			case 1: debug_printf ("Type 1 Partition Map\n"); break;
			case 2: debug_printf ("Type 2 Partition Map\n"); break;
			default: debug_printf ("Unknown (%d)\n", b[0]); break;
		}
		PM_L = b[1];
		N(n+2); debug_printf ("%d.Partition Map Length: %d\n", i, PM_L);

		if (l < PM_L)
		{
			N(n+2); debug_printf ("WARNING: ran out of data in buffer\n");
			break;
		}

		switch (b[0])
		{
			default:
				debug_printf ("     ");
				for (j=2; j < PM_L; j++)
				{
					debug_printf ("\\x%02x", b[j]);
				}
				debug_printf ("\n");
				break;
			case 1:
				if (PM_L != 6)
				{
					N(n+2); debug_printf ("%d WARNING: Wrong Length\n", i);
				} else {
					uint16_t PhysicalPartition_VolumeSequenceNumber = (b[3] << 8) | (b[2]);
					uint16_t PhysicalPartition_PartitionNumber = (b[5] << 8) | (b[4]);
					N(n+2); debug_printf ("%d.Volume Sequence Number: %d\n", i, PhysicalPartition_VolumeSequenceNumber);
					N(n+2); debug_printf ("%d.Partition Number:       %d\n", i, PhysicalPartition_PartitionNumber);
					UDF_LogicalVolume_Append_Type1 (volume, i, PhysicalPartition_VolumeSequenceNumber, PhysicalPartition_PartitionNumber);
				}
				break;
			case 2:
				if (PM_L != 64)
				{
					N(n+2); debug_printf ("%d WARNING: Wrong Length\n", i);
				} else {
					N(n+2); debug_printf ("%d.Partition Identifier: ", i); print_1_7_4 (b + 4, 0); debug_printf ("\n");
					if (!memcmp (b + 4 + 1, "*UDF Virtual Partition", 23))
					{
						uint16_t LogicalVolume_VolumeSequenceNumber = (b[37] << 8) | (b[36]);
						uint16_t LogicalVolume_PartitionNumber = (b[39] << 8) | (b[38]);
						N(n+2); debug_printf ("%d.Volume Sequence Number:  %" PRId16 " (Volume upon which the VAT and Partition is recorded)\n", i, LogicalVolume_VolumeSequenceNumber);
						N(n+2); debug_printf ("%d.Partition Number:        %" PRId16 " (a Type 1 Partition Map in the same logical volume descriptor)\n", i, LogicalVolume_PartitionNumber);
						UDF_LogicalVolume_Append_Type2_VAT (volume, i, LogicalVolume_VolumeSequenceNumber, LogicalVolume_PartitionNumber);
					} else if (!memcmp (b + 4 + 1, "*UDF Sparable Partition", 23))
					{
						int j;
						uint16_t VolumeSequenceNumber   = (b[37] << 8) | (b[36]);
						uint16_t PartitionNumber        = (b[39] << 8) | (b[38]);
						uint16_t PacketLength           = (b[41] << 8) | (b[40]);
						uint8_t  NumberOfSparingTables  = b[42];
						uint32_t SizeOfEachSparingTable = (b[47] << 24) | (b[46] << 16) | (b[45] << 8) | (b[44]);
						uint32_t SparingTableLocations[256];
						N(n+2); debug_printf ("%d.Volume Sequence Number:    %" PRId16 "\n", i, VolumeSequenceNumber);
						N(n+2); debug_printf ("%d.Partition Number:          %" PRId16 "\n", i, PartitionNumber);
						N(n+2); debug_printf ("%d.Packet Length:             %" PRId16 " (number of user data blocks per fixed packet)\n", i, PacketLength);
						N(n+2); debug_printf ("%d.Number of Sparing Tables   %" PRIu8  "\n", i, NumberOfSparingTables);
						if (NumberOfSparingTables < 0)
						{
							N(n+3); debug_printf ("Error - must be atleast 1\n");
						} else if (NumberOfSparingTables == 1)
						{
							N(n+3); debug_printf ("WARNING - should be bigger bigger than 1\n");
						} else if (NumberOfSparingTables > 4)
						{
							N(n+3); debug_printf ("WARNING - should be smaller than 5\n");
						}
						N(n+2); debug_printf ("%d.Size of Each Sparing Table %" PRId32 "\n", i, SizeOfEachSparingTable);
						for (j = 0; j < b[42]; j++)
						{
							if ((j * 4 + 48) > 64)
							{
								N(n+3); debug_printf ("%d WARNING - buffer overflow\n", j);
								NumberOfSparingTables = j;
								break;
							} else {
								SparingTableLocations[j] = (b[j*4 + 51] << 24) | (b[j*4 + 50] << 16) | (b[j*4 + 49] << 8) | (b[j*4 + 48]);
								N(n+3); debug_printf ("%d.Locations of Sparing Table.Position %" PRIu32 " 0x%08" PRIu32 "\n", j, SparingTableLocations[j], SparingTableLocations[j]);
							}
						}
						UDF_LogicalVolume_Append_Type2_SparingPartition (volume, i, VolumeSequenceNumber, PartitionNumber, PacketLength, NumberOfSparingTables, SizeOfEachSparingTable, SparingTableLocations);
					} else if (!memcmp (b + 4 + 1, "*UDF Metadata Partition", 23))
					{
						uint16_t VolumeSequenceNumber       =                                 (b[37] << 8) | (b[36]);
						uint16_t PartitionNumber            =                                 (b[39] << 8) | (b[38]);
						uint32_t MetadataFileLocation       = (b[43] << 24) | (b[42] << 16) | (b[41] << 8) | (b[40]);
						uint32_t MetadataMirrorFileLocation = (b[47] << 24) | (b[46] << 16) | (b[45] << 8) | (b[44]);
						uint32_t MetadataBitmapFileLocation = (b[51] << 24) | (b[50] << 16) | (b[49] << 8) | (b[48]);
						uint32_t AllocationUnitSize         = (b[55] << 24) | (b[54] << 16) | (b[53] << 8) | (b[52]);
						uint16_t AlignmentUnitSize          =                                 (b[57] << 8) | (b[56]);
						uint8_t  Flags                      =                                                (b[58]);
						// 5 bytes reserved */

						N(n+2); debug_printf ("%d.Volume Sequence Number:        %" PRId16 "\n", i, VolumeSequenceNumber);
						N(n+2); debug_printf ("%d.Partition Number:              %" PRId16 "\n", i, PartitionNumber);
						N(n+2); debug_printf ("%d.Metadata File Location:        %" PRId32 "\n", i, MetadataFileLocation);
						N(n+2); debug_printf ("%d.Metadata Mirror File Location: %" PRId32 "\n", i, MetadataMirrorFileLocation);
						N(n+2); debug_printf ("%d.Metadata Bitmap File Location: %" PRId32 "\n", i, MetadataBitmapFileLocation);
						N(n+2); debug_printf ("%d.Allocation Unit Size (blocks): %" PRId32 "\n", i, AllocationUnitSize);
						N(n+2); debug_printf ("%d.Alignment Unit Size (blocks):  %" PRIu16 "\n", i, AlignmentUnitSize);
						N(n+2); debug_printf ("%d.Flags:                         %" PRId8  "\n", i, (b[60]));
						N(n+3); debug_printf ("Duplicate Metadata Flag: %d\n", b[60] & 0x01);
						UDF_LogicalVolume_Append_Type2_Metadata (volume, i, VolumeSequenceNumber, PartitionNumber, MetadataFileLocation, MetadataMirrorFileLocation, MetadataBitmapFileLocation, AllocationUnitSize, AlignmentUnitSize, Flags);
					} else {
						N(n+3);  debug_printf ("\"");
						for (j=36; i < 64; i++)
						{
							if (!b[j]) break;
							if ((b[j] >= 0x20) && (b[j] < 0x7f) && (b[j] != '"') && (b[j] != '\\'))
							{
								debug_printf ("%c", b[j]);
							} else {
								debug_printf ("\\x%02x", b[j]);
							}
						}
						debug_printf ("\"\n");
					}
				}
				break;
		}
		l -= PM_L;
		b += PM_L;
	}

	SequenceRawdisk (n+2, disc, &IntegritySequenceExtent, LogicalVolumeIntegritySequence, 0);

	UDF_Session_Set_LogicalVolumes (disc, volume);
}

/* 0x0007 */
static void UnallocatedSpaceDescriptor (int n, struct cdfs_disc_t *disc, uint8_t *buffer)
{
	uint32_t N_AD;
	int i;

	N(n);   debug_printf ("[Unallocated Space Descriptor]\n");
	N(n+1); debug_printf ("Volume Descriptor Sequence Number: %" PRId32 "\n", (buffer[19] << 24) | (buffer[18] << 16) | (buffer[17] << 8) | buffer[16]);
	N_AD = (buffer[23] << 24) | (buffer[22] << 16) | (buffer[21] << 8) | buffer[20];
	N(n+1); debug_printf ("Number of Allocation Descriptors:  %" PRId32 "\n", N_AD);

	for (i=0; i < N_AD; i++)
	{
		struct UDF_extent_ad Allocation;
		char prefix[32];

		if ((i * 8 + 24) > SECTORSIZE)
		{
			debug_printf ("    WARNING - buffer overflow\n");
			break;
		} /* given in "Logical Sector" 512 bytes logic */
		snprintf (prefix, 32, "%d.Allocation.", i);
		UDF_extent_ad_from_data (n+2, prefix, &Allocation, buffer + (24 + i * 8));
	}
}

/* 0x0008 */
static void TerminatingDescriptor (int n, struct cdfs_disc_t *disc, uint8_t *buffer)
{
	N(n); debug_printf ("[Terminating Descriptor]\n");
	// no extra data in this type
}

/* 0x0009 */
static void LogicalVolumeIntegrityDescriptor (int n, struct cdfs_disc_t *disc, uint8_t *buffer)
{
	uint32_t IntegrityType;
	uint32_t N_P, L_IU;
	struct UDF_extent_ad NextIntegrityExtent;
	int i;
	uint8_t *b;
	int l;

	N(n);   debug_printf ("[Logical Volume Integrity Descriptor]\n");
	N(n+1); debug_printf ("Recording Date and Time:             "); print_1_7_3 (buffer + 16); debug_printf ("\n");
	IntegrityType = (buffer[31] << 24) | (buffer[30] << 16) | (buffer[29] << 8) | buffer[28];
	N(n+1); debug_printf ("Integrity Type:                      %" PRId32 " ", IntegrityType); if (IntegrityType == 0) debug_printf ("Open Integrity Descriptor\n"); else if (IntegrityType == 1) debug_printf ("Close Integrity Descriptor\n"); else debug_printf ("??? Integrity Descriptor\n");
	UDF_extent_ad_from_data (n+1, "Next Integrity Extent.", &NextIntegrityExtent, buffer + 32);
	N(n+1); debug_printf ("Logical Volume Contents Use:         \"");
	for (i=40; i < 72; i++)
	{
		if ((buffer[i] >= 0x20) && (buffer[i] < 0x7f) && (buffer[i] != '"') && (buffer[i] != '\\'))
		{
			debug_printf ("%c", buffer[i]);
		} else {
			debug_printf ("\\x%02x", buffer[i]);
		}
	}
	debug_printf ("\"\n");
	N(n+1); debug_printf ("(next available) UniqueID=0x%02x%02x%02x%02x.%02x%02x%02x%02x\n",
		buffer[47], buffer[46], buffer[45], buffer[44],
		buffer[43], buffer[42], buffer[41], buffer[40]);

	N_P = (buffer[75] << 24) | (buffer[74] << 16) | (buffer[73] << 8) | buffer[72];
	L_IU = (buffer[79] << 24) | (buffer[78] << 16) | (buffer[77] << 8) | buffer[76];
	N(n+1); debug_printf ("Number of Partitions:                %" PRId32 "\n", N_P);
	N(n+1); debug_printf ("Length of Implementation Use:        %" PRId32 "\n", L_IU);
	N(n+1); debug_printf ("Free Space Table:\n");

	b = buffer + 80;
	l = SECTORSIZE - 80;

	for (i=0; i < N_P; i++)
	{
		if (l < 4)
		{
			N(n+2); debug_printf ("WARNING: run out of buffer\n");
			break;
		}
		N(n+2); debug_printf ("%d.PartitionFreeSpace: %" PRId32 "\n", i, (b[3] << 24) | (b[2] << 16) || (b[1] << 8) | b[0]); // Given in Logical Blocks - 2048 bytes
		b += 4;
		l -= 4;
	}
	for (i=0; i < N_P; i++)
	{
		if (l < 4)
		{
			N(n+2); debug_printf ("WARNING: run out of buffer\n");
			break;
		}
		N(n+2); debug_printf ("%d.PartitionSize: %" PRId32 "\n", i, (b[3] << 24) | (b[2] << 16) || (b[1] << 8) | b[0]); // Given in Logical Blocks - 2048 bytes
		b += 4;
		l -= 4;
	}

	if (L_IU > l)
	{
		N(n+2); debug_printf ("WARNING: run out of buffer\n");
	} else {
		if (L_IU >= 32)
		{
			N(n+1); debug_printf ("Implementation Use:       "); print_1_7_4 (b, 1); debug_printf ("\n");
			L_IU -= 32;
			b += 32;
			l -= 32;

			if (l >= 14) // From OSTA UDF documentation
			{
#ifdef CDFS_DEBUG
				uint16_t v1 = (b[ 9] << 8) | b[8];
				uint16_t v2 = (b[11] << 8) | b[10];
				uint16_t v3 = (b[13] << 8) | b[12];
#endif

				N(n+3); debug_printf ("Number Of Files:       %" PRId32 "\n", (b[3]<<24) | (b[2] << 16) | (b[1] << 8) | b[0]);
				N(n+3); debug_printf ("Number Of Directories: %" PRId32 "\n", (b[7]<<24) | (b[6] << 16) | (b[5] << 8) | b[4]);
				N(n+3); debug_printf ("Minimum UDF Read Revision:  %d.%d%d\n", v1 >> 8, (v1 & 0xf0) >> 4, v1 & 0x0f);
				N(n+3); debug_printf ("Minimum UDF Write Revision: %d.%d%d\n", v2 >> 8, (v2 & 0xf0) >> 4, v2 & 0x0f);
				N(n+3); debug_printf ("Maximum UDF Write Revision: %d.%d%d\n", v3 >> 8, (v3 & 0xf0) >> 4, v3 & 0x0f);

				b += 14;
				l -= 14;
			}

			if (l)
			{
				N(n+3); debug_printf ("\"");
				while (L_IU)
				{
					if ((b[i] >= 0x20) && (b[i] < 0x7f) && (b[i] != '"') && (b[i] != '\\'))
					{
						debug_printf ("%c", b[i]);
					} else {
						debug_printf ("\\x%02x", b[i]);
					}
					b++;
					l--;
					L_IU--;
				}
				debug_printf ("\"\n");
			}
		}
	}

	SequenceRawdisk (n, disc, &NextIntegrityExtent, LogicalVolumeIntegritySequence, 0);
}

static void VolumeDescriptorSequence (int n, struct cdfs_disc_t *disc, struct UDF_Partition_Common *PartitionCommon, uint32_t TagLocation, uint8_t *buffer, uint32_t bufferlen, void *userpointer)
{
	int i;
	int Terminated = 0;
	N(n); debug_printf ("[Volume Descriptor Sequence]\n");
	for (i=0; i*SECTORSIZE < bufferlen; i++)
	{
		char prefix[16];
		uint16_t TagIdentifier;

		// This should not happen. Length should always be a multiple of 2048
		if ((bufferlen - i * SECTORSIZE) < SECTORSIZE)
		{
			break;
		}

#if 1
		snprintf (prefix, sizeof (prefix), "%d.", i + 1);
		if (print_tag_format (n+1, prefix, buffer + i * SECTORSIZE, TagLocation + i, 1, &TagIdentifier))
		{
			break;
		}
#else
		TagIdentifier = (buffer[i * SECTORSIZE + 1] << 8) | buffer[i * SECTORSIZE + 0];

		N(n+1); debug_printf ("%d.DescriptorTag.TagIdentifier:       %d - %s\n", i + 1, TagIdentifier, TagIdentifierName (TagIdentifier));
		N(n+1); debug_printf ("%d.DescriptorTag.DescriptorVersion:   %d\n", i + 1, (buffer[i * SECTORSIZE + 3]<<8) | buffer[i * SECTORSIZE + 2]);
		N(n+1); debug_printf ("%d.DescriptorTag.TagChecksum:         %d\n", i + 1, buffer[i * SECTORSIZE + 4]);
		//N(n+1); debug_printf ("%d.DescriptorTag.Reserved:            %d\n", i + 1, buffer[i * SECTORSIZE + 5]);
		N(n+1); debug_printf ("%d.DescriptorTag.TagSerialNumber:     %d\n", i + 1, (buffer[i * SECTORSIZE + 7]<<8) | buffer[i * SECTORSIZE + 6]);
		N(n+1); debug_printf ("%d.DescriptorTag.DescriptorCRC:       %d\n", i + 1, (buffer[i * SECTORSIZE + 9]<<8) | buffer[i * SECTORSIZE + 8]);
		N(n+1); debug_printf ("%d.DescriptorTag.DescriptorCRCLength: %d\n", i + 1, (buffer[i * SECTORSIZE + 11]<<8) | buffer[i * SECTORSIZE + 10]);
		N(n+1); debug_printf ("%d.DescriptorTag.TagLocation:         %d\n", i + 1, (buffer[i * SECTORSIZE + 15]<<24) | (buffer[i * SECTORSIZE + 14]<<16) | (buffer[i * SECTORSIZE + 13]<<8) | buffer[i * SECTORSIZE + 12]);
#endif

		switch (TagIdentifier)
		{
		//      case 0x0000: SparingTablelayout (n+2, disc, buffer + i * SECTORSIZE); break; // from the UDF specifications
			case 0x0001: PrimaryVolumeDescriptor (n+2, disc, buffer + i * SECTORSIZE); break;
		//	case 0x0002: AnchorVolumeDescriptorPointer (n+2, disc, buffer + i * SECTORSIZE); break;  <- not allowed
		//	case 0x0003: VolumeDescriptorPointer (n+2, disc, buffer + i * SECTORSIZE); break;
			case 0x0004: ImplementationUseVolumeDescriptor (n+2, disc, buffer + i * SECTORSIZE); break;
			case 0x0005: PartitionDescriptor (n+2, disc, buffer + i * SECTORSIZE); break;
			case 0x0006: LogicalVolumeDescriptor (n+2, disc, buffer + i * SECTORSIZE); break;
			case 0x0007: UnallocatedSpaceDescriptor (n+2, disc, buffer + i * SECTORSIZE); break;
			case 0x0008: TerminatingDescriptor (n+2, disc, buffer + i * SECTORSIZE); Terminated = 1; break;
		//	case 0x0009: LogicalVolumeIntegrityDescriptor (n+2, disc, buffer + i * SECTORSIZE); break;
			default:
				debug_printf ("Illegal sequence\n");
				break;
		}

		if (Terminated)
		{
			break;
		}
	}
	debug_printf ("\n");
}

static void LogicalVolumeIntegritySequence (int n, struct cdfs_disc_t *disc, struct UDF_Partition_Common *PartitionCommon, uint32_t TagLocation, uint8_t *buffer, uint32_t bufferlen, void *userpointer)
{
	int i;
	int Terminated = 0;
	N(n); debug_printf ("[Logical Volume Integrity Sequence]\n");
	for (i=0; i*SECTORSIZE < bufferlen; i++)
	{
		char prefix[16];
		uint16_t TagIdentifier;

		// This should not happen. Length should always be a multiple of 2048
		if ((bufferlen - i * SECTORSIZE) < SECTORSIZE)
		{
			break;
		}

#if 1
		snprintf (prefix, sizeof (prefix), "%d.", i + 1);
		if (print_tag_format (n+1, prefix, buffer + i * SECTORSIZE, TagLocation + i, 1, &TagIdentifier))
		{
			break;
		}
#else
		TagIdentifier = (buffer[i * SECTORSIZE + 1] << 8) | buffer[i * SECTORSIZE + 0];

		N(n+1); debug_printf ("%d.DescriptorTag.TagIdentifier:       %d - %s\n", i + 1, TagIdentifier, TagIdentifierName (TagIdentifier));
		N(n+1); debug_printf ("%d.DescriptorTag.DescriptorVersion:   %d\n", i + 1, (buffer[i * SECTORSIZE + 3]<<8) | buffer[i * SECTORSIZE + 2]);
		N(n+1); debug_printf ("%d.DescriptorTag.TagChecksum:         %d\n", i + 1, buffer[i * SECTORSIZE + 4]);
		//N(n+1); debug_printf ("%d.DescriptorTag.Reserved:            %d\n", i + 1, buffer[i * SECTORSIZE + 5]);
		N(n+1); debug_printf ("%d.DescriptorTag.TagSerialNumber:     %d\n", i + 1, (buffer[i * SECTORSIZE + 7]<<8) | buffer[i * SECTORSIZE + 6]);
		N(n+1); debug_printf ("%d.DescriptorTag.DescriptorCRC:       %d\n", i + 1, (buffer[i * SECTORSIZE + 9]<<8) | buffer[i * SECTORSIZE + 8]);
		N(n+1); debug_printf ("%d.DescriptorTag.DescriptorCRCLength: %d\n", i + 1, (buffer[i * SECTORSIZE + 11]<<8) | buffer[i * SECTORSIZE + 10]);
		N(n+1); debug_printf ("%d.DescriptorTag.TagLocation:         %d\n", i + 1, (buffer[i * SECTORSIZE + 15]<<24) | (buffer[i * SECTORSIZE + 14]<<16) | (buffer[i * SECTORSIZE + 13]<<8) | buffer[i * SECTORSIZE + 12]);
#endif

		switch (TagIdentifier)
		{
			case 0x0008: TerminatingDescriptor (n + 2, disc, buffer + i * SECTORSIZE); Terminated = 1; break;
			case 0x0009: LogicalVolumeIntegrityDescriptor (n + 2, disc, buffer + i * SECTORSIZE); break;
			default:
				debug_printf ("Illegal sequence\n");
				break;
		}

		if (Terminated)
		{
			break;
		}
	}
	debug_printf ("\n");
}

OCP_INTERNAL int AnchorVolumeDescriptorPointer (int n, uint8_t buffer[SECTORSIZE], uint32_t sector, struct UDF_extent_ad *MainVolumeDescriptorSequenceExtent, struct UDF_extent_ad *ReserveVolumeDescriptorSequenceExtent)
{
	uint16_t TagIdentifier;

	N(n); debug_printf ("[Anchor Volume Descriptor Pointer] (sector %" PRIu32 ")\n", sector);

	if (print_tag_format (n+1, "", buffer, sector, 1, &TagIdentifier))
	{
		return -1;
	}

	if (TagIdentifier != 0x0002)
	{
		N(n+1); debug_printf ("WARNING - TagIdentifier is wrong, should have been 2\n");
		return -1;
	}

	UDF_extent_ad_from_data (n+1, "Main Volume Descriptor Sequence Extent.", MainVolumeDescriptorSequenceExtent, buffer + 16);
	UDF_extent_ad_from_data (n+1, "Reserve Volume Descriptor Sequence Extent.", ReserveVolumeDescriptorSequenceExtent, buffer + 24);
	/* byte 31-511 is reserved */

	return 0;
}

static int LoadFileSetDescriptor (
	struct cdfs_disc_t *disc,
	uint16_t LogicalPartitionRef,
	uint32_t LogicalSector,
	uint8_t TimeStamp[12],

	uint8_t FileSetCharacterSet[32],

	struct UDF_longad *RootDirectoryICB,
	struct UDF_longad *SystemStreamDirectoryICB,
	struct UDF_longad *Prev)
{
	struct UDF_LogicalVolume_Common *LogicalPartition = 0;
	uint8_t buffer[SECTORSIZE];
	uint16_t TagIdentifier;
	uint32_t CharacterSetList;
#ifdef CDFS_DEBUG
	uint32_t FileSetNumber;
	uint32_t FileSetDescriptorNumber;
#endif

	LogicalPartition = UDF_GetLogicalPartition (disc, LogicalPartitionRef);
	if (!LogicalPartition)
	{
		debug_printf ("LoadRootDirectory: unable to find Partition\n");
		return -1;
	}
	if (LogicalPartition->PartitionCommon.FetchSector (disc, &LogicalPartition->PartitionCommon, buffer, LogicalSector))
	{
		debug_printf ("LoadRootDirectory: unable to fetch sector\n");
		return -1;
	}

	if (print_tag_format (0/*n+1*/, "RootDirectory.", buffer, LogicalSector, 1, &TagIdentifier))
	{
		return -1;
	}

	if (TagIdentifier != 0x0100)
	{
		debug_printf ("LoadRootDirectory: Unexpected TagIdentifier (0x%04x)\n", TagIdentifier);
		return -1;
	}
	      debug_printf ("[File Set Descriptor]\n");
	N(1); debug_printf ("Recording Date and Time:    "); print_1_7_3 (buffer + 16); debug_printf ("\n");
	memcpy (TimeStamp, buffer + 16, 12);
	N(1); debug_printf ("Interchange Level:          %" PRId16 "\n", (buffer[29] << 8) | buffer[28]);
	N(1); debug_printf ("Maximum Interchange Level:  %" PRId16 "\n", (buffer[31] << 8) | buffer[30]);
	CharacterSetList = ((buffer[35] << 24) | (buffer[34] << 16) | (buffer[33] << 8) | buffer[32]);
	N(1); debug_printf ("Character Set List:         %" PRId32 "\n", CharacterSetList);
	if (CharacterSetList & 0x001) { N(2); debug_printf ("CS0 in use\n"); }
	if (CharacterSetList & 0x002) { N(2); debug_printf ("CS1 in use\n"); }
	if (CharacterSetList & 0x004) { N(2); debug_printf ("CS2 in use\n"); }
	if (CharacterSetList & 0x008) { N(2); debug_printf ("CS3 in use\n"); }
	if (CharacterSetList & 0x010) { N(2); debug_printf ("CS4 in use\n"); }
	if (CharacterSetList & 0x020) { N(2); debug_printf ("CS5 in use\n"); }
	if (CharacterSetList & 0x040) { N(2); debug_printf ("CS6 in use\n"); }
	if (CharacterSetList & 0x080) { N(2); debug_printf ("CS7 in use\n"); }
	if (CharacterSetList & 0x100) { N(2); debug_printf ("CS8 in use\n"); }
	N(1); debug_printf ("Maximum Character Set List: %" PRId32 "\n", ((buffer[39] << 24) | (buffer[38] << 16) | (buffer[37] << 8) | buffer[36]));
#ifdef CDFS_DEBUG
	FileSetNumber =           ((buffer[43] << 24) | (buffer[42] << 16) | (buffer[41] << 8) | buffer[40]);
	FileSetDescriptorNumber = ((buffer[47] << 24) | (buffer[46] << 16) | (buffer[45] << 8) | buffer[44]);
#endif
	N(1); debug_printf ("File Set Number:            %" PRId32 "\n", FileSetNumber);
	N(1); debug_printf ("File Set Descriptor Number: %" PRId32 "\n", FileSetDescriptorNumber);
	N(1); debug_printf ("Logical Volume Identifier Character Set: "); print_1_7_2_1 (buffer + 48); debug_printf ("\n");
	N(1); debug_printf ("Logical Volume Identifier   "); print_1_7_2_12 (buffer + 112, 128, buffer + 48 /* encoding*/, 0); debug_printf ("\n");
	N(1); debug_printf ("File Set Character Set:     "); print_1_7_2_1 (buffer + 240); debug_printf ("\n");
	memcpy (FileSetCharacterSet, buffer + 240, 32);
	N(1); debug_printf ("File Set Identifier:        "); print_1_7_2_12 (buffer + 304, 32, buffer + 240 /* encoding*/, 0); debug_printf ("\n");
	N(1); debug_printf ("Copyright File Identifier:  "); print_1_7_2_12 (buffer + 336, 32, buffer + 240 /* encoding*/, 0); debug_printf ("\n");
	N(1); debug_printf ("Abstract File Identifier:   "); print_1_7_2_12 (buffer + 368, 32, buffer + 240 /* encoding*/, 0); debug_printf ("\n");

	UDF_longad_from_data (1, "Root Directory ICB.", RootDirectoryICB, buffer + 400);
	/* next 6 bytes are reserved */

	N(1); debug_printf ("Domain Identifier:          "); print_1_7_4 (buffer + 416, 0);  debug_printf ("\n");

	UDF_longad_from_data (1, "Next Extent.", Prev, buffer + 448);

	UDF_longad_from_data (1, "System Stream Directory ICB.", SystemStreamDirectoryICB, buffer + 464);

	return 0;
}

static int pathname_4_14_16 (int n, uint8_t *symlinkfiledata, uint64_t symlinkfilesize, uint8_t *encoding, char **symlink)
{
	uint_fast32_t length = 0;

	char *t;
	uint8_t *d;
	uint64_t l;
	int i = 0;

	*symlink = 0;

	/* iteration 1, calculate length */
	d = symlinkfiledata; l = symlinkfilesize;
	while (l >= 4)
	{
		uint8_t ComponentType = d[0];
		uint8_t L_CI = d[1];
#ifdef CDFS_DEBUG
		uint16_t ComponentFileVersionNumber = (d[3]<<8) | d[2];
#endif
		int j;
		N(n); debug_printf ("Component.%d.Component Type:      %" PRIu8 " ", i, ComponentType);
		switch (ComponentType)
		{
			case 0: debug_printf ("\"Reserved for future standardisation.\"\n"); return -1;
			case 1: /* subject to agreement between the originator and recipient of the medium */
			default:
				debug_printf ("undefined\n"); break;
			case 2: debug_printf ("/ - root (current drive)\n"); length++; break;
			case 3: debug_printf (".. - parent directory\n");    length += 3; break;
			case 4: debug_printf (". - same directory\n");       length += 3; break;
			case 5: debug_printf ("component\n");                length += 1 + (uint_fast32_t)L_CI * 3 / 2; /* estimate worst case if UTF16 is used, no other code-maps are valid */ break;
		}
		N(n); debug_printf ("Component.%d.Length of Component: %" PRIu8 "\n", i, L_CI);
		N(n); debug_printf ("Component.%d.Component File Version Number: %" PRIu16 "%s\n", i, ComponentFileVersionNumber, (ComponentFileVersionNumber==0)?"":" WARNING, should be zero according to UDF standard");
		d+=4; l-=4;
		if (L_CI)
		{
			if (L_CI > l)
			{
				N(n); debug_printf ("WARNING - Buffer overflow\n");
				return -1;
			}
			N(n); debug_printf ("Component.%d.Component Identifier", i);
			for (j=0; j < L_CI; j++)
			{
				debug_printf (" 0x%02x", d[j]);
			}
			debug_printf ("\n");
			d+=L_CI; l-=L_CI;
		}
		i++;
	}

	/* iteration 2, fill in the data */
	*symlink = calloc(1, length + 1);
	if (!*symlink)
	{
		N(n+1); debug_printf ("WARNING - calloc() failed in pathname_4_14_16\n"); return -1;
	}
	t = *symlink;
	d = symlinkfiledata; l = symlinkfilesize;
	while (l >= 4)
	{
		uint8_t ComponentType = d[0];
		uint8_t L_CI = d[1];
		switch (ComponentType)
		{
			case 1:
			default:
				break;
			case 2:
				*t = '/'; t++; break;
			case 3:
				if (t != *symlink) { *t = '/'; t++; }
				*t = '.'; t++;
				*t = '.'; t++; break;
			case 4:
				if (t != *symlink) { *t = '/'; t++; }
				*t = '.'; t++; break;
			case 5:
				if (t != *symlink) { *t = '/'; t++; }
				{
					char *temp = 0;
					N(n+1);
					print_1_7_2 (d+4, L_CI, encoding, &temp);
					debug_printf ("\n");
					if (temp)
					{
						strcpy (t, temp);
						t = t + strlen (t);
						free (temp);
					}
				}
				break;
		}
		d+=4; l-=4;
		d+=L_CI; l-=L_CI;
	}
	return 0;
}


static struct UDF_FS_FileEntry_t *FileDecoder2 (int n, struct cdfs_disc_t *disc, struct UDF_RootDirectory_t *rootdir, const char *pathname, int IsStream, struct UDF_FileEntry_t *FE)
{
	struct UDF_FS_FileEntry_t *retval;

	retval = calloc (1, sizeof (*retval));
	if (!retval)
	{
		fprintf (stderr, "FileDecoder - calloc() failed\n");
		return 0;
	}

	retval->FE = FE;
	retval->FileName = strdup (pathname);

	switch (FE->FileType)
	{
		default:
		case FILETYPE_UNALLOCATED_SPACE_ENTRY:       /* This will be loaded via UnallocatedSpaceEntry() */
		case FILETYPE_PARTITION_INTEGRITY_ENTRY:     /* This will be loaded via PartitionIntegrityEntry() */
		case FILETYPE_DIRECTORY:                     /* This will be loaded via DirectoryDecoder, since parent would have flagged us */
		case FILETYPE_THE_VIRTUAL_ALLOCATED_TABLE:   /* This will be loaded via Load_VAT() */
		case FILETYPE_RECORDING_EXTENDED_ATTRIBUTES: /* This will not be in the directory listing, but referenced in FileEntryCommon.Extended Attribute ICB */
		case FILETYPE_STREAM_DIRECTORY:              /* TODO: unsure about this... */
		case FILETYPE_INDIRECT_ENTRY:                /* This will be loaded via IndirectEntrySequencer() and TODO */
		case FILETYPE_METADATA_FILE:                 /* This will be loaded via Type2_Metadata_LoadData() */
		case FILETYPE_METADATA_MIRROR_FILE:          /* This will be loaded via Type2_Metadata_LoadData() */
		case FILETYPE_METADATA_BITMAP_FILE:          /* This will be loaded via Type2_MetaData_LoadBitmap() */
		case FILETYPE_TERMINAL_ENTRY:                /* This should be inside a Terminal Entry Descriptor */
			N(n+2); debug_printf ("WARNING - filetype was unexpected\n");
			free (retval->FileName);
			free (retval);
			return 0;


		case FILETYPE_FILE:           /* do not load data until demanded */
		case FILETYPE_REAL_TIME_FILE: /* do not load data until demanded */
		case FILETYPE_FIFO:           /* no data to load */
		case FILETYPE_C_ISSOCK:       /* no data to load */
			return retval;

		case FILETYPE_BLOCK_SPECIAL_DEVICE:
		case FILETYPE_CHARACTER_SPECIAL_DEVICE:
			/* Major/Minor is filled in by ExtendedAttributesCommon() */
			return retval;

		case FILETYPE_SYMLINK:
		{
			uint8_t *symlinkfiledata = 0;
			uint64_t symlinkfilesize = 0;
			char *symlink = 0;

			/* load data */
			if (FileEntryLoadData (disc, FE, &symlinkfiledata, 10*1024))
			{
				N(n+2); debug_printf ("WARNING - Unable to load the \"Symlink\" FileEntry\n");
				free (retval->FileName);
				free (retval);
				return 0;
			}
			symlinkfilesize = FE->InformationLength;

			if (symlinkfiledata)
			{
				N(n+2); debug_printf ("[Symlink]\n"); pathname_4_14_16 (n+3, symlinkfiledata, symlinkfilesize, rootdir->FileSetCharacterSet, &symlink); debug_printf ("\n");
				free (symlinkfiledata);
			}
			if (!symlink)
			{
				free (retval->FileName);
				free (retval);
				return 0;
			}
			retval->Symlink = symlink;
			return retval;
		}
	}

	/* this should be non-reachable */
	free (retval->FileName);
	free (retval);
	return 0;
}

static struct UDF_FS_FileEntry_t *FileDecoder (int n, struct cdfs_disc_t *disc, uint32_t LogicalPartitionRef, struct UDF_RootDirectory_t *rootdir, uint32_t LogicalSector, const char *pathname, int IsStream)
{
	struct UDF_FS_FileEntry_t *retval = 0;
	struct UDF_FS_FileEntry_t *iter;
	struct UDF_FS_FileEntry_t **prev = &retval;
	struct UDF_LogicalVolume_Common *LogicalPartition = 0;
	struct UDF_FileEntry_t *FE;

	N(n); debug_printf ("..[FileDecoder]\n");
	LogicalPartition = UDF_GetLogicalPartition (disc, LogicalPartitionRef);
	if (!LogicalPartition)
	{
		N(n+1); debug_printf ("Error - unable to find Partition\n");
		return 0;
	}

	FE = FileEntry (n+2, disc, LogicalSector, &LogicalPartition->PartitionCommon, 0);
	if (!FE)
	{
		free (retval);
		return 0;
	}

	while (FE)
	{
		*prev = FileDecoder2 (n + 2, disc, rootdir, pathname, IsStream, FE);
		if (!*prev)
		{
			break;
		}
		FE = FE->PreviousVersion;
		prev = &(*prev)->PreviousVersion;
	}
	if (!retval)
	{ /* if there is no UDF_FS_FileEntry_t to attach a FE too, release the whole chain... */
		FileEntry_Free (FE);
		return 0;
	}
	/* detach each FE node as long as there is another UDF_FS_FileEntry_t that owns it */
	for (iter = retval; iter; iter = iter->PreviousVersion)
	{
		if (iter->PreviousVersion)
		{
			assert (iter->FE);
			iter->FE->PreviousVersion = 0;
		}
	}
	return retval;
}

static struct UDF_FS_DirectoryEntry_t *DirectoryDecoder (int n, const char *_prefix, struct cdfs_disc_t *disc, uint16_t LogicalPartitionRef, struct UDF_RootDirectory_t *rootdir, uint32_t LogicalSector, const char *pathname, int IsStream);

static struct UDF_FS_DirectoryEntry_t *DirectoryDecoder2 (int n, const char *_prefix, struct cdfs_disc_t *disc, struct UDF_RootDirectory_t *rootdir, const char *pathname, int IsStream, struct UDF_FileEntry_t *FE)
{
	uint8_t *filedata = 0;
	uint64_t filesize;
	uint8_t *b;
	uint32_t l;
	uint32_t offset = 0;
	int index;
	struct UDF_FS_DirectoryEntry_t *retval = 0;
	struct UDF_FS_DirectoryEntry_t **PrevDirectory;
	struct UDF_FS_FileEntry_t      **PrevFile;

	int CFA_Index = 0;
	int CFA_Offset = 0;

	if (IsStream)
	{
		if (FE->FileType != FILETYPE_STREAM_DIRECTORY)
		{
			N(n+2); debug_printf ("WARNING - Didn't find an expected stream directory\n");
			return 0;
		}
	} else {
		if (FE->FileType != FILETYPE_DIRECTORY)
		{
			N(n+2); debug_printf ("WARNING - Didn't find an expected directory\n");
			return 0;
		}
	}

	if (FileEntryLoadData (disc, FE, &filedata, 1024 * 1024))
	{
		N(n+2); debug_printf ("WARNING - Unable to load the \"Directory\" FileEntry\n");
		return 0;
	}
	filesize = FE->InformationLength;

	if (!filedata)
	{
		return 0;
	}
	b = filedata;
	l = filesize;

	retval = calloc (1, sizeof (*retval));
	retval->FE = FE;
#if 0
	retval->Location_Partition = LogicalPartitionRef;
	retval->Location_Sector = LogicalSector;
#endif
	retval->DirectoryName = pathname ? strdup (pathname) : 0;

	PrevDirectory = &retval->DirectoryEntries;
	PrevFile      = &retval->FileEntries;

	for (index = 0; l >= 38; index++)
	{
		uint16_t TagIdentifier = 0;
		char prefix[128];
		uint8_t L_FI;
		uint16_t L_IU;
		int padlength;

		snprintf (prefix, sizeof (prefix), "%s%d.", _prefix, index);
		if (print_tag_format (n+2, prefix, b, FE->FileAllocation[CFA_Index].ExtentLocation + (CFA_Offset / SECTORSIZE) , 0, &TagIdentifier))
		{
			break;
		}
		if (TagIdentifier != 0x0101)
		{
			N(n+3); debug_printf ("Error - Invalid TagIdentifier, expected 0x0101\n");
			break;
		}
		N(n+3); debug_printf ("[File Identifier Descriptor]\n");
		N(n+4); debug_printf ("File Version Number:       %d%s\n", (b[17] << 8) | (b[16]), ((b[17] << 8) | (b[16]))==1?"":" WARNING - only \"1\" is allowed value according to the UDF standard\n");
		N(n+4); debug_printf ("File Characteristics:      0x%08" PRIx8 "\n", b[18]);
		N(n+5); debug_printf ("Existence:    %s\n", (b[18] & 0x01) ? "Hidden file" : "Visible");
		N(n+5); debug_printf ("Is directory: %s\n", (b[18] & 0x02) ? "Yes, entry is a directory" : "No, entry is a file");
		N(n+5); debug_printf ("Is deleted:   %s\n", (b[18] & 0x04) ? "Yes, entry is deleted" : "No");
		N(n+5); debug_printf ("Is parent:    %s\n", (b[18] & 0x08) ? "Yes, entry is the \"..\"" : "No");
		N(n+5); debug_printf ("Is metadata:  %s\n", (b[18] & 0x10) ? "Yes, but this is invalid for non-stream-directories" : "No");
		L_FI = b[19];
		N(n+4); debug_printf ("Length of File Identifier: %d\n", L_FI);
		N(n+4); debug_printf ("ICB.Extent Length:                              %d\n", (b[23]<<24) | (b[22]<<16) | (b[21]<<8) | b[20]);
		N(n+4); debug_printf ("ICB.Extent Location.Logical Block Number:       %d\n", (b[27]<<24) | (b[26]<<16) | (b[25]<<8) | b[24]);
		N(n+4); debug_printf ("ICB.Extent Location.Partition Reference Number: %d\n", (b[29]<<8) | b[28]);
		N(n+4); debug_printf ("ICB.Flags: 0x%04" PRIx16 "\n", (b[31] << 8) | b[30]);
		if (b[30] & 0x01) { N(n+5); debug_printf ("EXTENTErased: YES\n"); }
		N(n+4); debug_printf ("ICB.UDF UniqueID: %" PRIu32 "\n", (b[35]<<24) | (b[34]<<16) | (b[33]<<8) | b[32]);
		L_IU = ((b[37]<<8) | b[36]);
		N(n+4); debug_printf ("Length of Implementation Use: %" PRIu16 "\n", L_IU);
		if (l < (38 + L_IU))
		{
			N(n+5); debug_printf ("WARNING - buffer overrun\n");
			break;
		}
		if (L_IU >= 32)
		{
			N(n+4); debug_printf ("Implementation Use:\n");
			N(n+5); debug_printf ("Entity Identifier: "); print_1_7_4 (b + 38, 1);
			if (L_IU > 32)
			{
				int i;
				N(n+5);
				for (i=32; i < L_IU; i++)
				{
					debug_printf(" 0x%02" PRIx8, b[38 + i]);
				}
				debug_printf ("\n");
			}
		} else if (L_IU)
		{
				int i;
				N(n+4); debug_printf ("Implementation Use:\n");
				N(n+4);
				for (i=32; i < L_IU; i++)
				{
					debug_printf(" 0x%02" PRIx8, b[38 + i]);
				}
				debug_printf ("\n");

		}
		if (l < (38 + L_IU + L_FI))
		{
			N(n+5); debug_printf ("WARNING - buffer overrun\n");
			break;
		}
		N(n+4); debug_printf ("File Identifier:            ");
		if (L_FI)
		{
			char *childname = 0;
			uint32_t Location_Partition =                             (b[29]<<8) | b[28];
			uint32_t Location_Sector    = (b[27]<<24) | (b[26]<<16) | (b[25]<<8) | b[24];

			print_1_7_2(b + 38 + L_IU, L_FI, rootdir->FileSetCharacterSet, &childname);
			debug_printf ("\n");
			if (!childname)
			{
				continue; /* filename decoding failed */
			}

			if (b[18] & 0x08) /* .. */
			{
				/* ..  - parent directory */
				free (childname);
				continue;
			}

			if (b[18] & 0x02)
			{
				/* directory */
				*PrevDirectory = DirectoryDecoder (n + 5, prefix, disc, Location_Partition, rootdir, Location_Sector, childname, IsStream);
				if (*PrevDirectory)
				{
					PrevDirectory = &(*PrevDirectory)->Next;
				}
			} else {
				/* file */
				*PrevFile = FileDecoder (n + 5, disc, Location_Partition, rootdir, Location_Sector, childname, IsStream);
				debug_printf ("\n");
				if (*PrevFile)
				{
					PrevFile = &(*PrevFile)->Next;
				}
			}
			free (childname);
		} else {
			debug_printf("(null)\n");
		}
		padlength = (L_FI + L_IU + 38 + 3) & ~3;
		if (l >= padlength)
		{
			offset += padlength;
			l -= padlength;
			b += padlength;

			while (padlength)
			{
				if (padlength >= (FE->FileAllocation[CFA_Index].InformationLength + CFA_Offset))
				{
					padlength -= FE->FileAllocation[CFA_Index].InformationLength - CFA_Offset;
					CFA_Offset = 0;
					CFA_Index++;
				} else {
					CFA_Offset += padlength;
					break;
				}
			}
		} else {
			l = 0;
		}
	}

	free (filedata);

	return retval;
}

static struct UDF_FS_DirectoryEntry_t *DirectoryDecoder (int n, const char *_prefix, struct cdfs_disc_t *disc, uint16_t LogicalPartitionRef, struct UDF_RootDirectory_t *rootdir, uint32_t LogicalSector, const char *pathname, int IsStream)
{
	struct UDF_FS_DirectoryEntry_t *retval = 0;
	struct UDF_FS_DirectoryEntry_t *iter;
	struct UDF_FS_DirectoryEntry_t **prev = &retval;
	struct UDF_LogicalVolume_Common *LogicalPartition = 0;
	struct UDF_FileEntry_t *FE;

	N(n); debug_printf ("..[DirectoryDecoder]\n");
	LogicalPartition = UDF_GetLogicalPartition (disc, LogicalPartitionRef);
	if (!LogicalPartition)
	{
		N(n+1); debug_printf ("Error - unable to find Partition\n");
		return 0;
	}

	FE = FileEntry (n+2, disc, LogicalSector, &LogicalPartition->PartitionCommon, 0);
	if (!FE)
	{
		return 0;
	}

	while (FE)
	{
		*prev = DirectoryDecoder2 (n + 2, _prefix, disc, rootdir, pathname, IsStream, FE);
		if (!*prev)
		{
			break;
		}
		FE = FE->PreviousVersion;
		prev = &(*prev)->PreviousVersion;
	}
	if (!retval)
	{ /* if there is no UDF_FS_DirectoryEntry_t to attach a FE too, release the whole chain... */
		FileEntry_Free (FE);
		return 0;
	}
	/* detach each FE node as long as there is another UDF_FS_DirectoryEntry_t that can own it */
	for (iter = retval; iter; iter = iter->PreviousVersion)
	{
		if (iter->PreviousVersion)
		{
			assert (iter->FE);
			iter->FE->PreviousVersion = 0;
		}
	}
	return retval;
}

static int UDF_CompleteDiskIO_Initialize (struct cdfs_disc_t *disc, struct UDF_Partition_Common *self)
{
	return 0;
}
static int UDF_CompleteDiskIO_FetchSector (struct cdfs_disc_t *disc, struct UDF_Partition_Common *self, uint8_t *buffer, uint32_t sector)
{
	return cdfs_fetch_absolute_sector_2048 (disc, sector, buffer);
}
static void UDF_CompleteDiskIO_PushAbsoluteLocations (struct cdfs_disc_t *disc, struct UDF_Partition_Common *self, uint32_t sector, uint32_t length, int skiplength, int handle)
{
	CDFS_File_extent (disc, handle, sector, length, skiplength);
}

static void UDF_CompleteDiskIO_Free (void *self)
{
}

static void UDF_CompleteDiskIO_DefaultSession (struct cdfs_disc_t *disc, struct UDF_Partition_Common *self, uint32_t *LocationIterator, uint8_t TimeStamp[12])
{
	memset (TimeStamp, 0, 12);
}

static int UDF_CompleteDiskIO_SelectSession (struct cdfs_disc_t *disc, struct UDF_Partition_Common *self, uint32_t LocationIterator/*, uint8_t TimeStamp[12]*/)
{
	return 0;
}

static int UDF_CompleteDiskIO_NextSession (struct cdfs_disc_t *disc, struct UDF_Partition_Common *self, uint32_t *LocatinIterator, uint8_t TimeStamp[12])
{
	return 0;
}

OCP_INTERNAL void UDF_Descriptor (struct cdfs_disc_t *disc)
{
	const int n = 0;
	uint8_t buffer[SECTORSIZE];

	int invalid_256 = 1;
	struct UDF_extent_ad MainVolumeDescriptorSequenceExtent_256 = {0, 0};
	struct UDF_extent_ad ReserveVolumeDescriptorSequenceExtent_256 = {0, 0};

	int invalid_N = 1;
	struct UDF_extent_ad MainVolumeDescriptorSequenceExtent_N = {0, 0};
	struct UDF_extent_ad ReserveVolumeDescriptorSequenceExtent_N = {0, 0};

	int invalid_N_minus_256 = 1;
	struct UDF_extent_ad MainVolumeDescriptorSequenceExtent_N_minus_256 = {0, 0};
	struct UDF_extent_ad ReserveVolumeDescriptorSequenceExtent_N_minus_256 = {0, 0};

	{
		uint32_t SearchBegin = 256;
		uint32_t SearchEnd = 4500000;
		uint32_t SearchHalf;
		int iteration = 0;

		while ((SearchBegin < SearchEnd) && (SearchBegin + 1) != SearchEnd)
		{
			iteration++;

			SearchHalf = ((SearchEnd - SearchBegin) >> 1) + SearchBegin;

			debug_printf ("UDF_Descriptor iteration %d, SearchBegin=%"PRId32" SearchEnd=%"PRId32" SearchHalf=%"PRId32"\n", iteration, SearchBegin, SearchEnd, SearchHalf);
			if (cdfs_fetch_absolute_sector_2048 (disc, SearchHalf, buffer))
			{
				debug_printf (" FetchSector %" PRIu32 " failed\n", SearchHalf);
				SearchEnd = SearchHalf - 1;
			} else {
				SearchBegin = SearchHalf;
			}
		}
		debug_printf ("SearchEnd=%" PRId32 "\n", SearchEnd);
		if (SearchEnd > 256)
		{
			if (cdfs_fetch_absolute_sector_2048 (disc, SearchEnd, buffer))
			{
				debug_printf ("Failed to fetch sector N");
			} else {
				invalid_N = AnchorVolumeDescriptorPointer (n, buffer, SearchEnd, &MainVolumeDescriptorSequenceExtent_N, &ReserveVolumeDescriptorSequenceExtent_N);
			}

			if (cdfs_fetch_absolute_sector_2048 (disc, SearchEnd - 256, buffer))
			{
				debug_printf ("Failed to fetch sector N-256");
			} else {
				invalid_N_minus_256 = AnchorVolumeDescriptorPointer (n, buffer, SearchEnd - 256, &MainVolumeDescriptorSequenceExtent_N_minus_256, &ReserveVolumeDescriptorSequenceExtent_N_minus_256);
			}
		}
	}

	/* Anchor Volume Descriptor Pointer is always located at sector 256 in the given session */
	if (cdfs_fetch_absolute_sector_2048 (disc, 256, buffer))
	{
		return;
	}

	invalid_256 = AnchorVolumeDescriptorPointer (n, buffer, 256, &MainVolumeDescriptorSequenceExtent_256, &ReserveVolumeDescriptorSequenceExtent_256);

	if (((invalid_N) &&           (!MainVolumeDescriptorSequenceExtent_N.ExtentLength))           &&
	    ((invalid_N_minus_256) && (!MainVolumeDescriptorSequenceExtent_N_minus_256.ExtentLength)) &&
	    ((invalid_256) &&         (!MainVolumeDescriptorSequenceExtent_256.ExtentLength))         )
	{
		return;
	}

	if (!disc->udf_session)
	{
		disc->udf_session = calloc (1, sizeof (*disc->udf_session));
		if (!disc->udf_session)
		{
			fprintf (stderr, "disc->udf_session = calloc() failed\n");
			return;
		}
	}

	disc->udf_session->CompleteDisk.Initialize = UDF_CompleteDiskIO_Initialize;
	disc->udf_session->CompleteDisk.FetchSector = UDF_CompleteDiskIO_FetchSector;
	disc->udf_session->CompleteDisk.PushAbsoluteLocations = UDF_CompleteDiskIO_PushAbsoluteLocations;
	disc->udf_session->CompleteDisk.Free = UDF_CompleteDiskIO_Free;
	disc->udf_session->CompleteDisk.DefaultSession = UDF_CompleteDiskIO_DefaultSession;
	disc->udf_session->CompleteDisk.SelectSession = UDF_CompleteDiskIO_SelectSession;
	disc->udf_session->CompleteDisk.NextSession = UDF_CompleteDiskIO_NextSession;

	if ((!invalid_N) && MainVolumeDescriptorSequenceExtent_N.ExtentLength)
	{
		SequenceRawdisk (n + 2, disc, &MainVolumeDescriptorSequenceExtent_N, VolumeDescriptorSequence, 0);
	} else if ((!invalid_N_minus_256) && MainVolumeDescriptorSequenceExtent_N_minus_256.ExtentLength)
	{
		SequenceRawdisk (n + 2, disc, &MainVolumeDescriptorSequenceExtent_N_minus_256, VolumeDescriptorSequence, 0);
	} else /* if ((!invalid_256) && MainVolumeDescriptorSequenceExtent_256.ExtentLength) */ {
		SequenceRawdisk (n + 2, disc, &MainVolumeDescriptorSequenceExtent_256, VolumeDescriptorSequence, 0);
	}

	if (disc->udf_session && disc->udf_session->LogicalVolumes)
	{
		int i;
		struct UDF_LogicalVolume_Common *lv = 0;

		uint32_t lv_Location;
		uint8_t lv_TimeStamp[12];

		for (i=0; i < disc->udf_session->LogicalVolumes->LogicalVolume_N; i++)
		{
			disc->udf_session->LogicalVolumes->LogicalVolume[i]->PartitionCommon.Initialize (disc, &disc->udf_session->LogicalVolumes->LogicalVolume[i]->PartitionCommon);
			if (disc->udf_session->LogicalVolumes->LogicalVolume[i]->PartId == disc->udf_session->LogicalVolumes->FileSetDescriptor_PartitionReferenceNumber)
			{
				lv = disc->udf_session->LogicalVolumes->LogicalVolume[i];
			}
		}

		lv->PartitionCommon.DefaultSession (disc, &lv->PartitionCommon, &lv_Location, lv_TimeStamp);
		do {
			uint16_t fsd_partition = disc->udf_session->LogicalVolumes->FileSetDescriptor_PartitionReferenceNumber;
			uint32_t fsd_sector = disc->udf_session->LogicalVolumes->FileSetDescriptor_LogicalBlockNumber;
			uint8_t  fsd_timestamp[12];

			struct UDF_longad RootDirectoryICB;
			struct UDF_longad SystemStreamDirectoryICB;
			struct UDF_longad fsd_prev;

			uint8_t FileSetCharacterSet[32];

			do
			{
				int i;
				struct UDF_RootDirectory_t *temp;

				if (LoadFileSetDescriptor (
					disc,
					fsd_partition,
					fsd_sector,
					fsd_timestamp,
					FileSetCharacterSet,
					&RootDirectoryICB,
					&SystemStreamDirectoryICB,
					&fsd_prev))
				{
					break;
				}

				for (i=0; i < disc->udf_session->LogicalVolumes->RootDirectories_N; i++)
				{
					if ((disc->udf_session->LogicalVolumes->RootDirectories[i].FileSetDescriptor_Partition_Session == lv_Location) &&
					    (disc->udf_session->LogicalVolumes->RootDirectories[i].FileSetDescriptor_PartitionNumber == fsd_partition) &&
					    (disc->udf_session->LogicalVolumes->RootDirectories[i].FileSetDescriptor_Location == fsd_sector))
					{
						break; /* We already have this one.... */
					}
				}
				if (i != disc->udf_session->LogicalVolumes->RootDirectories_N)
				{
					break; /* We already have this one.... (level-2 break) */
				}

				temp = realloc (disc->udf_session->LogicalVolumes->RootDirectories, (disc->udf_session->LogicalVolumes->RootDirectories_N + 1) * sizeof (disc->udf_session->LogicalVolumes->RootDirectories[0]));
				if (!temp)
				{
					fprintf (stderr, "WARNING - UDF_Descriptor() - realloc(RootDirectories) failed\n");
					break;
				}
				disc->udf_session->LogicalVolumes->RootDirectories = temp;
				temp = &disc->udf_session->LogicalVolumes->RootDirectories[disc->udf_session->LogicalVolumes->RootDirectories_N];
				disc->udf_session->LogicalVolumes->RootDirectories_N++;

				temp->FileSetDescriptor_Partition_Session = lv_Location;
				temp->FileSetDescriptor_PartitionNumber = fsd_partition;
				temp->FileSetDescriptor_Location = fsd_sector;
				memcpy (temp->FileSetDescriptor_TimeStamp_1_7_3, fsd_timestamp, 12);
				if (lv_TimeStamp[ 0] || lv_TimeStamp[ 1] || lv_TimeStamp[ 2] || lv_TimeStamp[ 3] ||
				    lv_TimeStamp[ 4] || lv_TimeStamp[ 5] || lv_TimeStamp[ 6] || lv_TimeStamp[ 7] ||
				    lv_TimeStamp[ 8] || lv_TimeStamp[ 9] || lv_TimeStamp[10] || lv_TimeStamp[11])
				{
					memcpy (temp->FileSetDescriptor_TimeStamp_1_7_3, lv_TimeStamp, 12);
				}
				memcpy (temp->FileSetCharacterSet, FileSetCharacterSet, 32);
				temp->RootDirectory = RootDirectoryICB;
				temp->SystemStreamDirectory = SystemStreamDirectoryICB;
				temp->Root = 0;
				temp->SystemStream = 0;

				if (temp->RootDirectory.ExtentLength)
				{
					temp->Root = DirectoryDecoder (n+2, "ROOT.", disc, temp->RootDirectory.ExtentLocation.PartitionReferenceNumber, temp, temp->RootDirectory.ExtentLocation.LogicalBlockNumber, 0, 0);
				}
				if (temp->SystemStreamDirectory.ExtentLength)
				{
					temp->SystemStream = DirectoryDecoder (n+2, "ROOT.", disc, temp->SystemStreamDirectory.ExtentLocation.PartitionReferenceNumber, temp, temp->SystemStreamDirectory.ExtentLocation.LogicalBlockNumber, 0, 1);
				}

				if (!fsd_prev.ExtentLength)
				{
					break; /* no more history on this session */
				}
				fsd_partition = fsd_prev.ExtentLocation.PartitionReferenceNumber;
				fsd_sector = fsd_prev.ExtentLocation.LogicalBlockNumber;
			} while (1);

		} while (!lv->PartitionCommon.NextSession (disc, &lv->PartitionCommon, &lv_Location, lv_TimeStamp));
	}
}

static void SequenceRawdisk (int n, struct cdfs_disc_t *disc, struct UDF_extent_ad *L, void (*Handler)(int n, struct cdfs_disc_t *disc, struct UDF_Partition_Common *PartitionCommon, uint32_t TagLocation, uint8_t *buffer, uint32_t bufferlen, void *userpointer), void *userpointer)
{
	uint8_t *buffer;
	uint32_t left = L->ExtentLength;
	int pos = 0;


	if (!L->ExtentLength)
	{
		return;
	}

	buffer = calloc (1, (L->ExtentLength + SECTORSIZE - 1) & ~(SECTORSIZE - 1));
	if (!buffer)
	{
		N(n); fprintf (stderr, "Warning - Failed to malloc buffer\n");
		return;
	}

	while (left)
	{
		if (cdfs_fetch_absolute_sector_2048 (disc, L->ExtentLocation + pos, buffer + pos * SECTORSIZE))
		{
			N(n); debug_printf ("Warning - Failed to fetch sector\n");
			break;
		}
		pos++;
		left -= (left > 2048) ? 2048 : left;
	}
	if (!left)
	{
		Handler (n, disc, &disc->udf_session->CompleteDisk, L->ExtentLocation, buffer, L->ExtentLength, userpointer);
	}
	free (buffer);
}

OCP_INTERNAL void UDF_Session_Free (struct cdfs_disc_t *disc)
{
	if (!disc)
	{
		return;
	}
	if (!disc->udf_session)
	{
		return;
	}

	if (disc->udf_session->PrimaryVolumeDescriptor)
	{
		free (disc->udf_session->PrimaryVolumeDescriptor->VolumeIdentifier);
		      disc->udf_session->PrimaryVolumeDescriptor->VolumeIdentifier = 0;

		free (disc->udf_session->PrimaryVolumeDescriptor);
		      disc->udf_session->PrimaryVolumeDescriptor = 0;
	}

	free (disc->udf_session->PhysicalPartition);
	disc->udf_session->PhysicalPartition = 0;

	if (disc->udf_session->LogicalVolumes)
	{
		UDF_LogicalVolumes_Free (disc->udf_session->LogicalVolumes);
		disc->udf_session->LogicalVolumes = 0;
	}

	if (disc->udf_session)
	{
		free (disc->udf_session);
		disc->udf_session = 0;
	}
}

static void UDF_Session_Add_PrimaryVolumeDescriptor (struct cdfs_disc_t *disc, uint32_t VolumeDescriptorSequenceNumber, char *VolumeIdentifier, uint16_t VolumeSequenceNumber)
{
	if (!disc)
	{
		return;
	}
	if (!disc->udf_session)
	{
		return;
	}
	if (!disc->udf_session->PrimaryVolumeDescriptor)
	{
		disc->udf_session->PrimaryVolumeDescriptor = calloc (1, sizeof (*disc->udf_session->PrimaryVolumeDescriptor));
	}
	if (disc->udf_session->PrimaryVolumeDescriptor->VolumeDescriptorSequenceNumber >= VolumeSequenceNumber)
	{
		return;
	}
	disc->udf_session->PrimaryVolumeDescriptor->VolumeDescriptorSequenceNumber = VolumeDescriptorSequenceNumber;
	free (disc->udf_session->PrimaryVolumeDescriptor->VolumeIdentifier);
	disc->udf_session->PrimaryVolumeDescriptor->VolumeIdentifier = VolumeIdentifier;
	disc->udf_session->PrimaryVolumeDescriptor->VolumeSequenceNumber = VolumeSequenceNumber;
}

static int PhysicalPartitionInitialize (struct cdfs_disc_t *disc, struct UDF_Partition_Common *self)
{
	return 0;
}

static int PhysicalPartitionFetchSector (struct cdfs_disc_t *disc, struct UDF_Partition_Common *self, uint8_t *buffer, uint32_t sector)
{
	struct UDF_PhysicalPartition_t *_self = (struct UDF_PhysicalPartition_t *)self;
	return cdfs_fetch_absolute_sector_2048 (disc, sector + _self->Start, buffer);
}

static void PhysicalPartitionPushAbsoluteLocations (struct cdfs_disc_t *disc, struct UDF_Partition_Common *self, uint32_t sector, uint32_t length, int skiplength, int handle)
{
	struct UDF_PhysicalPartition_t *_self = (struct UDF_PhysicalPartition_t *)self;
	CDFS_File_extent (disc, handle, sector + _self->Start, length, skiplength);
}


static void UDF_Session_Add_PhysicalPartition (struct cdfs_disc_t *disc, uint32_t VolumeDescriptorSequenceNumber, uint16_t PartitionNumber, enum PhysicalPartition_Content Content, uint32_t SectorSize, uint32_t Start, uint32_t Length)
{
	struct UDF_PhysicalPartition_t *temp;
	int i;

	for (i = 0; i < disc->udf_session->PhysicalPartition_N; i++)
	{
		if (disc->udf_session->PhysicalPartition[i].PartitionNumber > PartitionNumber)
		{
			break;
		}
		/* If same partition re-appears. keep the one with the highest VolumeDescriptorSequenceNumber */
		if (disc->udf_session->PhysicalPartition[i].PartitionNumber == PartitionNumber)
		{
			if (disc->udf_session->PhysicalPartition[i].VolumeDescriptorSequenceNumber < VolumeDescriptorSequenceNumber)
			{
				disc->udf_session->PhysicalPartition[i].VolumeDescriptorSequenceNumber = VolumeDescriptorSequenceNumber;
				disc->udf_session->PhysicalPartition[i].Content = Content;
				disc->udf_session->PhysicalPartition[i].SectorSize = SectorSize;
				disc->udf_session->PhysicalPartition[i].Start = Start;
				disc->udf_session->PhysicalPartition[i].Length = Length;
				return;
			}
		}
	}
	temp = realloc (disc->udf_session->PhysicalPartition, sizeof (struct UDF_PhysicalPartition_t) * (disc->udf_session->PhysicalPartition_N  + 1));
	if (!temp)
	{
		fprintf (stderr, "UDF_Session_Add_PhysicalPartition: realloc() failed\n");
		return;
	}
	disc->udf_session->PhysicalPartition = temp;
	memmove (disc->udf_session->PhysicalPartition + i + 1, disc->udf_session->PhysicalPartition + i, sizeof (struct UDF_PhysicalPartition_t) * (disc->udf_session->PhysicalPartition_N  - i ));
	disc->udf_session->PhysicalPartition[i].VolumeDescriptorSequenceNumber = VolumeDescriptorSequenceNumber;
	disc->udf_session->PhysicalPartition[i].PartitionNumber = PartitionNumber;
	disc->udf_session->PhysicalPartition[i].PartitionCommon.Initialize = PhysicalPartitionInitialize;
	disc->udf_session->PhysicalPartition[i].PartitionCommon.FetchSector = PhysicalPartitionFetchSector;
	disc->udf_session->PhysicalPartition[i].PartitionCommon.PushAbsoluteLocations = PhysicalPartitionPushAbsoluteLocations;
	disc->udf_session->PhysicalPartition[i].Content = Content;
	disc->udf_session->PhysicalPartition[i].SectorSize = SectorSize;
	disc->udf_session->PhysicalPartition[i].Start = Start;
	disc->udf_session->PhysicalPartition[i].Length = Length;
	disc->udf_session->PhysicalPartition_N++;
}

static struct UDF_LogicalVolumes_t *UDF_LogicalVolumes_Create (uint32_t VolumeDescriptorSequenceNumber, char *LogicalVolumeIdentifier, uint8_t DescriptorCharacterSet[64])
{
	struct UDF_LogicalVolumes_t *retval = calloc (1, sizeof (*retval));

	if (!retval)
	{
		fprintf (stderr, "UDF_LogicalVolumes_Create: calloc() failed\n");
		return 0;
	}

	retval->VolumeDescriptorSequenceNumber = VolumeDescriptorSequenceNumber;
	retval->LogicalVolumeIdentifier = LogicalVolumeIdentifier;
	memcpy (retval->DescriptorCharacterSet, DescriptorCharacterSet, 64);

	return retval;
}

static void UDF_LogicalVolume_FileSetDescriptor_SetLocation (struct UDF_LogicalVolumes_t *self, uint32_t FileSetDescriptor_LogicalBlockNumber, uint16_t FileSetDescriptor_PartitionReferenceNumber)
{
	if (self)
	{
		self->FileSetDescriptor_LogicalBlockNumber = FileSetDescriptor_LogicalBlockNumber;
		self->FileSetDescriptor_PartitionReferenceNumber = FileSetDescriptor_PartitionReferenceNumber;
	}
}

static void UDF_File_Free (struct UDF_FS_FileEntry_t *File)
{
	while (File)
	{
		struct UDF_FS_FileEntry_t *next = File->Next;

		if (File->PreviousVersion)
		{
			UDF_File_Free (File->PreviousVersion);
		}
		free (File->FileName);
		free (File->Symlink);
		FileEntry_Free (File->FE);
		free (File);
		File = next;
	}
}

static void UDF_Directory_Free (struct UDF_FS_DirectoryEntry_t *Directory)
{
	while (Directory)
	{
		struct UDF_FS_DirectoryEntry_t *next = Directory->Next;
		if (Directory->DirectoryEntries)
		{
			UDF_Directory_Free (Directory->DirectoryEntries);
		}
		if (Directory->FileEntries)
		{
			UDF_File_Free (Directory->FileEntries);
		}
		if (Directory->PreviousVersion)
		{
			UDF_Directory_Free (Directory->PreviousVersion);
		}
		free (Directory->DirectoryName);
		FileEntry_Free (Directory->FE);
		free (Directory);
		Directory = next;
	}
}

static void UDF_RootDirectory_Free (struct UDF_RootDirectory_t *root)
{
	if (root->Root) UDF_Directory_Free (root->Root);
	if (root->SystemStream) UDF_Directory_Free (root->SystemStream);

}

static void UDF_LogicalVolumes_Free (struct UDF_LogicalVolumes_t *self)
{
	int i;

	if (!self)
	{
		return;
	}

	for (i=0; i < self->RootDirectories_N; i++)
	{
		UDF_RootDirectory_Free (&self->RootDirectories[i]);
	}
	free (self->RootDirectories);
	self->RootDirectories = 0;

	free (self->LogicalVolumeIdentifier);
	self->LogicalVolumeIdentifier = 0;

	for (i=0; i < self->LogicalVolume_N; i++)
	{
		self->LogicalVolume[i]->PartitionCommon.Free (self->LogicalVolume[i]);
	}
	self->LogicalVolume_N = 0;
	free (self->LogicalVolume);

	self->LogicalVolume = 0;

	free (self);
}

static void UDF_Session_Set_LogicalVolumes (struct cdfs_disc_t *disc, struct UDF_LogicalVolumes_t *LogicalVolumes)
{
	if (!disc)
	{
		return;
	}

	if (!disc->udf_session)
	{
		return;
	}

	if (disc->udf_session->LogicalVolumes)
	{
		if (disc->udf_session->LogicalVolumes->VolumeDescriptorSequenceNumber < LogicalVolumes->VolumeDescriptorSequenceNumber)
		{
			UDF_LogicalVolumes_Free (disc->udf_session->LogicalVolumes);
			disc->udf_session->LogicalVolumes->VolumeDescriptorSequenceNumber = 0;
		} else if (disc->udf_session->LogicalVolumes->VolumeDescriptorSequenceNumber >= LogicalVolumes->VolumeDescriptorSequenceNumber)
		{
			UDF_LogicalVolumes_Free (LogicalVolumes);
			return;
		}
	}
	disc->udf_session->LogicalVolumes = LogicalVolumes;
}

static int Type1_FetchSector_Real (struct cdfs_disc_t *disc, void *self, uint8_t *buffer, uint32_t sector)
{
	struct UDF_LogicalVolume_Type1 *t = (struct UDF_LogicalVolume_Type1 *)self;
	if (!t->PhysicalPartition)
	{
		return -1;
	}
	return t->PhysicalPartition->PartitionCommon.FetchSector (disc, &t->PhysicalPartition->PartitionCommon, buffer, sector);
}

static int Type1_FetchSector_Virtual (struct cdfs_disc_t *disc, struct UDF_Partition_Common *self, uint8_t *buffer, uint32_t sector)
{
	struct UDF_LogicalVolume_Type1 *t = (struct UDF_LogicalVolume_Type1 *)self;
	if (!t->PhysicalPartition)
	{
		return -1;
	}
	if (t->VAT)
	{
		return t->VAT->Common.PartitionCommon.FetchSector (disc, &t->VAT->Common.PartitionCommon, buffer, sector);
	}
	return t->PhysicalPartition->PartitionCommon.FetchSector (disc, &t->PhysicalPartition->PartitionCommon, buffer, sector);
}

static void Type1_PushAbsoluteLocations_Virtual (struct cdfs_disc_t *disc, struct UDF_Partition_Common *self, uint32_t sector, uint32_t length, int skiplength, int handle)
{
	struct UDF_LogicalVolume_Type1 *t = (struct UDF_LogicalVolume_Type1 *)self;
	if (!t->PhysicalPartition)
	{
		CDFS_File_zeroextent (disc, handle, length);
	}
	if (t->VAT)
	{
		t->VAT->Common.PartitionCommon.PushAbsoluteLocations (disc, &t->VAT->Common.PartitionCommon, sector, length, skiplength, handle);
		return;
	}
	t->PhysicalPartition->PartitionCommon.PushAbsoluteLocations (disc, &t->PhysicalPartition->PartitionCommon, sector, length, skiplength, handle);
}


static int Type1_Initialize (struct cdfs_disc_t *disc, struct UDF_Partition_Common *self)
{
	struct UDF_LogicalVolume_Type1 *t = (struct UDF_LogicalVolume_Type1 *)self;
	int i;
	if (!disc)
	{
		return -1;
	}
	if (!disc->udf_session)
	{
		return -1;
	}
	if (!disc->udf_session->PrimaryVolumeDescriptor)
	{
		return -1;
	}
	if (disc->udf_session->PrimaryVolumeDescriptor->VolumeSequenceNumber != t->PhysicalPartition_VolumeSequenceNumber)
	{	/* This is from another disc */
		return -1;
	}
	if (t->Initialized & 1)
	{
		return -1;
	}
	if (t->Initialized)
	{
		return t->PhysicalPartition ? 0 : -1;
	}
	t->Initialized++;
	for (i=0; i < disc->udf_session->PhysicalPartition_N; i++)
	{
		if (disc->udf_session->PhysicalPartition[i].PartitionNumber == t->PhysicalPartition_PartitionNumber)
		{
			t->PhysicalPartition = &disc->udf_session->PhysicalPartition[i];
			t->Initialized++;
			return 0;
		}
	}
	t->Initialized++;
	return -1;
}

static void Type1_DefaultSession (struct cdfs_disc_t *disc, struct UDF_Partition_Common *self, uint32_t *LocationIterator, uint8_t TimeStamp[12])
{
	struct UDF_LogicalVolume_Type1 *t = (struct UDF_LogicalVolume_Type1 *)self;

	if (t->VAT)
	{
		return t->VAT->Common.PartitionCommon.DefaultSession (disc, &t->VAT->Common.PartitionCommon, LocationIterator, TimeStamp);
	}

	*LocationIterator = t->PhysicalPartition->Start;
	memset (TimeStamp, 0, 12);
}

static int Type1_SelectSession (struct cdfs_disc_t *disc, struct UDF_Partition_Common *self, uint32_t LocationIterator/*, uint8_t TimeStamp[12]*/)
{
	struct UDF_LogicalVolume_Type1 *t = (struct UDF_LogicalVolume_Type1 *)self;

	if (t->VAT)
	{
		return t->VAT->Common.PartitionCommon.SelectSession (disc, &t->VAT->Common.PartitionCommon, LocationIterator/*, TimeStamp*/);
	}

	if (LocationIterator != t->PhysicalPartition->Start)
	{
		return -1;
	}
	/* memset (TimeStamp, 0, sizeof (TimeStamp)); */
	return 0;
}

static int Type1_NextSession (struct cdfs_disc_t *disc, struct UDF_Partition_Common *self, uint32_t *LocationIterator, uint8_t TimeStamp[12])
{
	struct UDF_LogicalVolume_Type1 *t = (struct UDF_LogicalVolume_Type1 *)self;

	if (t->VAT)
	{
		return t->VAT->Common.PartitionCommon.NextSession (disc, &t->VAT->Common.PartitionCommon, LocationIterator, TimeStamp);
	}

	return -1;
}

static void UDF_LogicalVolume_Append (struct UDF_LogicalVolumes_t *self, struct UDF_LogicalVolume_Common *t)
{
	struct UDF_LogicalVolume_Common **temp;

	temp = realloc (self->LogicalVolume, (self->LogicalVolume_N + 1) * sizeof (self->LogicalVolume[0]));
	if (!temp)
	{
		t->PartitionCommon.Free (&t->PartitionCommon);
		return;
	}

	self->LogicalVolume = temp;
	self->LogicalVolume[self->LogicalVolume_N] = t;
	self->LogicalVolume_N++;
}

static int Type2_VAT_FetchSector (struct cdfs_disc_t *disc, struct UDF_Partition_Common *self, uint8_t *buffer, uint32_t sector)
{
	struct UDF_LogicalVolume_Type2_VAT *t = (struct UDF_LogicalVolume_Type2_VAT *)self;
	if (!t->PhysicalPartition)
	{
		return -1;
	}
	if (sector >= t->ActiveEntry->Length)
	{
#if 0
		return -1;
#else
		return t->PhysicalPartition->PartitionCommon.FetchSector (disc, &t->PhysicalPartition->PartitionCommon, buffer, sector);
#endif
	}
	if (t->ActiveEntry->Entries[sector].RemappedTo == (uint32_t)0xffffffff)
	{
		return -1;
	}
	return t->PhysicalPartition->PartitionCommon.FetchSector (disc, &t->PhysicalPartition->PartitionCommon, buffer, t->ActiveEntry->Entries[sector].RemappedTo);
}

static void Type2_VAT_PushAbsoluteLocations (struct cdfs_disc_t *disc, struct UDF_Partition_Common *self, uint32_t sector, uint32_t length, int skiplength, int handle)
{
	struct UDF_LogicalVolume_Type2_VAT *t = (struct UDF_LogicalVolume_Type2_VAT *)self;
	if (!t->PhysicalPartition)
	{
		CDFS_File_zeroextent (disc, handle, length);
		return;
	}
	if (sector >= t->ActiveEntry->Length)
	{
#if 0
		CDFS_File_zeroextent (disc, handle, length);
#else
		t->PhysicalPartition->PartitionCommon.PushAbsoluteLocations (disc, &t->PhysicalPartition->PartitionCommon, sector, length, skiplength, handle);
#endif
	}
	while (length >= 2048)
	{
		if (t->ActiveEntry->Entries[sector].RemappedTo == (uint32_t)0xffffffff)
		{
			CDFS_File_zeroextent (disc, handle, length);
		} else {
			t->PhysicalPartition->PartitionCommon.PushAbsoluteLocations (disc, self, t->ActiveEntry->Entries[sector].RemappedTo, (length > 2048) ? 2048 : length, skiplength, handle);
		}
		skiplength = 0;
		sector++;
		if (length > 2048)
		{
			length -= 2048;
		} else {
			length = 0;
		}
	}
}

static void Type2_VAT_Free_Entries (struct UDF_VAT_Entries *e)
{
	if (e->Previous)
	{
		Type2_VAT_Free_Entries (e->Previous);
		free (e->Previous);
	}
	free (e->Entries);
}

static void Type2_VAT_Free (void *self)
{
	struct UDF_LogicalVolume_Type2_VAT *t = (struct UDF_LogicalVolume_Type2_VAT *)self;

	Type2_VAT_Free_Entries (&t->RootEntry);

	free (t);
}

static void Type2_VAT_DefaultSession (struct cdfs_disc_t *disc, struct UDF_Partition_Common *self, uint32_t *LocationIterator, uint8_t TimeStamp[12])
{
	struct UDF_LogicalVolume_Type2_VAT *t = (struct UDF_LogicalVolume_Type2_VAT *)self;

	*LocationIterator = t->RootEntry.VAT_Location;
	memcpy (TimeStamp, t->RootEntry.TimeStamp_1_7_3, 12);
	t->ActiveEntry = &t->RootEntry;
}

static int Type2_VAT_SelectSession (struct cdfs_disc_t *disc, struct UDF_Partition_Common *self, uint32_t LocationIterator/*, uint8_t TimeStamp[12]*/)
{
	struct UDF_LogicalVolume_Type2_VAT *t = (struct UDF_LogicalVolume_Type2_VAT *)self;
	struct UDF_VAT_Entries *iter;

	for (iter = &t->RootEntry; iter; iter = iter->Previous)
	{
		if (iter->VAT_Location == LocationIterator)
		{
			t->ActiveEntry = iter;
			/*memcpy (TimeStamp, t->ActiveEntry->TimeStamp_1_7_3);*/
			return 0;
		}
	}

	return -1;
}

static int Type2_VAT_NextSession (struct cdfs_disc_t *disc, struct UDF_Partition_Common *self, uint32_t *LocationIterator, uint8_t TimeStamp[12])
{
	struct UDF_LogicalVolume_Type2_VAT *t = (struct UDF_LogicalVolume_Type2_VAT *)self;

	if (t->ActiveEntry->Previous)
	{
		t->ActiveEntry = t->ActiveEntry->Previous;

		*LocationIterator = t->ActiveEntry->VAT_Location;
		memcpy (TimeStamp, t->ActiveEntry->TimeStamp_1_7_3, 12);
		return 0;
	}

	return -1;
}

static int Load_VAT (const int n, struct cdfs_disc_t *disc, struct UDF_PhysicalPartition_t *PhysicalPartition, struct UDF_VAT_Entries *target, uint32_t sector, uint32_t *previous_sector)
{
	uint8_t *filedata = 0;
	uint64_t filesize = 0;

	uint16_t L_HD, L_IU;
#ifdef CDFS_DEBUG
	uint16_t MinimumUDFReadRevision;
	uint16_t MinimumUDFWriteRevision;
	uint16_t MaximumUDFWriteRevision;
#endif
	int i = 0;
	uint64_t l;
	uint8_t *b;

	struct UDF_FileEntry_t *FE;

	target->VAT_Location = sector;

	N(n); debug_printf ("Testing logical sector %"PRId32"\n", sector);
	FE = FileEntry (n+2, disc, sector, &PhysicalPartition->PartitionCommon, 0);
	if (!FE)
	{
		return -1;
	}
	memcpy (target->TimeStamp_1_7_3, FE->TimeStamp, 12);

	if (FE->FileType != FILETYPE_THE_VIRTUAL_ALLOCATED_TABLE)
	{
		N(n+2); debug_printf ("WARNING - Unable to find the \"VAT\" FileEntry\n");
		FileEntry_Free (FE);
		return -1;
	}

	if (FileEntryLoadData (disc, FE, &filedata, 36 * 1024 * 1024))
	{
		N(n+2); debug_printf ("WARNING - Unable to load the \"VAT\" FileEntry\n");
		FileEntry_Free (FE);
		return -1;
	}
	filesize = FE->InformationLength;
	FileEntry_Free (FE);
	FE = 0;

	if (!filedata)
	{
		return -1;
	}

	i = 0;
	if (filesize < 156)
	{
		N(n+2); debug_printf ("WARNING - Virtual Allocation Table can not fit in the data loaded\n");
		free (filedata);
		return -1;
	}
	N(n+2); debug_printf ("[Virtual Allocation Table]\n");
	L_HD = (filedata[1] << 8) | filedata[0];
	N(n+3); debug_printf ("Length of Header:             %" PRIu16" \n", L_HD);
	if (L_HD > filesize)
	{
		N(n+4); debug_printf ("WARNING - Length is too big\n");
		free (filedata);
		return -1;
	}
	L_IU = (filedata[3] << 8) | filedata[2];
	N(n+3); debug_printf ("Length of Implementation Use: %" PRIu16" \n", L_IU);
	if (L_IU + 152 > L_HD)
	{
		N(n+4); debug_printf ("WARNING - Length is too big, it has been shrunk");
		L_IU = L_HD - 152;
	}
	N(n+3); debug_printf ("Logical Volume Identifier:    "); print_1_7_2_12 (filedata + 4, 128, disc->udf_session->LogicalVolumes->DescriptorCharacterSet, 0); debug_printf ("\n");
	*previous_sector = ((filedata[135] << 24) | (filedata[134] << 16) | (filedata[133] << 8) | filedata[132]);
	N(n+3); debug_printf ("Previous VAT ICB location:    %" PRIu32" \n", *previous_sector);
	N(n+3); debug_printf ("Number of Files:              %" PRIu32" \n", (filedata[139] << 24) | (filedata[138] << 16) | (filedata[137] << 8) | filedata[136]);
	N(n+3); debug_printf ("Number of Directories:        %" PRIu32" \n", (filedata[143] << 24) | (filedata[142] << 16) | (filedata[141] << 8) | filedata[140]);
#ifdef CDFS_DEBUG
	MinimumUDFReadRevision  = (filedata[145] << 8) | filedata[144];
	MinimumUDFWriteRevision = (filedata[147] << 8) | filedata[146];
	MaximumUDFWriteRevision = (filedata[149] << 8) | filedata[148];
#endif
	// 150 + 151 are reserved
	N(n+3); debug_printf ("Minimum UDF Read Revision:    %d.%d%d\n", MinimumUDFReadRevision >> 8, (MinimumUDFReadRevision & 0xf0) >> 4, MinimumUDFReadRevision & 0x0f);
	N(n+3); debug_printf ("Minimum UDF Write Revision:   %d.%d%d\n", MinimumUDFWriteRevision >> 8, (MinimumUDFWriteRevision & 0xf0) >> 4, MinimumUDFWriteRevision & 0x0f);
	N(n+3); debug_printf ("Maximum UDF Write Revision:   %d.%d%d\n", MaximumUDFWriteRevision >> 8, (MaximumUDFWriteRevision & 0xf0) >> 4, MaximumUDFWriteRevision & 0x0f);

	if (L_IU)
	{
		N(n+3); debug_printf ("Implementation Use:\n");
		if (L_IU >= 32)
		{
			N(n+4); debug_printf ("Identitity:     "); print_1_7_4 (filedata + 152, 1); debug_printf ("\n");
			if (L_IU > 32)
			{
				N(n+4);
				for (i=32; i < L_IU; i++)
				{
					debug_printf (" 0x%02" PRIx8, filedata[152 + i]);
				}
				debug_printf ("\n");
			}
		} else {
			/* Not compliant with UDF standards */
			N(n+4);
			for (i=0; i < L_IU; i++)
			{
				debug_printf (" 0x%02" PRIx8, filedata[152 + i]);
			}
			debug_printf ("\n");
		}
	}

	l = filesize - L_HD;
	b = filedata + L_HD;
	target->Length = l / sizeof (uint32_t);
	if (!target->Length)
	{
		N(n+4); debug_printf ("WARNING - VAT has no entries\n");
		free (filedata);
		return -1;
	}

	target->Entries = calloc (target->Length, sizeof (target->Entries[0]));
	if (!target->Entries)
	{
		fprintf (stderr, "WARNING - Type2_VAT_Initialize: calloc failed\n");
		free (filedata);
		return -1;
	}

	while (l >= 4)
	{
		uint32_t Location = (b[3]<<24) | (b[2]<<16) | (b[1] << 8) | b[0];
		if (Location == (uint32_t)0xffffffff)
		{
			N(n+4); debug_printf ("VAT Entry.%d=UNALLOCATED\n", i);
		} else {
			N(n+4); debug_printf ("VAT Entry.%d=%" PRIu32 "\n", i, Location);
		}
		target->Entries[i].RemappedTo = Location;
		i++;
		l -= 4;
		b += 4;
	}
	free (filedata);

	return 0;
}

static int Type2_VAT_Initialize (struct cdfs_disc_t *disc, struct UDF_Partition_Common *self)
{
	uint32_t SearchBegin;
	uint32_t SearchEnd; /* We try to find the first invalid sector */
	uint32_t SearchHalf;
	int iteration;
	uint8_t buffer[SECTORSIZE];
	struct UDF_LogicalVolume_Type2_VAT *t = (struct UDF_LogicalVolume_Type2_VAT *)self;
	int i;
	uint32_t next_sector = 0;

	if (!disc)
	{
		return -1;
	}
	if (!disc->udf_session)
	{
		return -1;
	}
	if (!disc->udf_session->PrimaryVolumeDescriptor)
	{
		return -1;
	}
	if (disc->udf_session->PrimaryVolumeDescriptor->VolumeSequenceNumber != t->PhysicalPartition_VolumeSequenceNumber)
	{	/* This is from another disc */
		return -1;
	}

	if (t->Initialized & 1)
	{
		return -1;
	}
	if (t->Initialized)
	{
		return t->PhysicalPartition ? 0 : -1;
	}
	t->Initialized++;

	for (i=0; i < disc->udf_session->LogicalVolumes->LogicalVolume_N; i++)
	{
		struct UDF_LogicalVolume_Type1 *s = (struct UDF_LogicalVolume_Type1 *)disc->udf_session->LogicalVolumes->LogicalVolume[i];
		if ( (s->Common.Type == 1) &&
		   /*(s->PhysicalPartition_VolumeSequenceNumber == t->PhysicalPartition_VolumeSequenceNumber) && */
		     (s->PhysicalPartition_PartitionNumber      == t->PhysicalPartition_PartitionNumber) )
		{
			t->LogicalVolume_Type1 = s;
			break;
		}
	}
	for (i=0; i < disc->udf_session->PhysicalPartition_N; i++)
	{
		if ( (disc->udf_session->PhysicalPartition[i].PartitionNumber == t->PhysicalPartition_PartitionNumber) /* &&
		     (disc[j]->udf_session->PrimaryVolumeDescriptor->VolumeSequenceNumber == t->PhysicalPartition_VolumeSequenceNumber) */ )
		{
			t->PhysicalPartition = &disc->udf_session->PhysicalPartition[i];
			break;
		}
	}

	if (!t->PhysicalPartition)
	{
		t->Initialized++;
		return -1;
	}

	if (t->LogicalVolume_Type1)
	{
		if (t->LogicalVolume_Type1->Common.PartitionCommon.Initialize (disc, &t->LogicalVolume_Type1->Common.PartitionCommon))
		{
			t->LogicalVolume_Type1 = 0;
		}
	}

	if (t->PhysicalPartition->PartitionCommon.Initialize (disc, &t->PhysicalPartition->PartitionCommon))
	{
		t->Initialized++;
		t->LogicalVolume_Type1 = 0;
		t->PhysicalPartition = 0;
		return -1;
	}

	SearchBegin = 0;
	SearchEnd = 4500000;
	iteration = 0;

	while ((SearchBegin < SearchEnd) && (SearchBegin + 1) != SearchEnd)
	{
		uint32_t FetchLength = 64;
		int j;

		iteration++;

		SearchHalf = ((SearchEnd - SearchBegin) >> 1) + SearchBegin;
		if ((SearchEnd - SearchHalf) < FetchLength)
		{
			FetchLength = SearchEnd - SearchHalf;
		}

		debug_printf ("VAT init iteration %d, SearchBegin=%"PRId32" SearchEnd=%"PRId32" SearchHalf=%"PRId32" FetchLength=%"PRId32"\n", iteration, SearchBegin, SearchEnd, SearchHalf, FetchLength);

		for (i=0; i < FetchLength; i++)
		{
			if (t->LogicalVolume_Type1->FetchSectorReal (disc, t->LogicalVolume_Type1, buffer, SearchHalf + i))
			{
				debug_printf (" FetchSector %" PRIu32 " failed\n", SearchHalf + i);
				SearchEnd = SearchHalf + i;
				break;
			}
			for (j=0; j < SECTORSIZE; j++)
			{
				if (buffer[j])
				{
					debug_printf (" Content hit in sector %d\n", i);
					SearchBegin = SearchHalf + i;
					break;
				}
			}
			if (j != SECTORSIZE)
			{
				break;
			}
		}
		if (i == FetchLength)
		{
			SearchEnd = SearchHalf;
		}
	}
	t->RootEntry.Length = SearchEnd;
	t->ActiveEntry = &t->RootEntry;

	debug_printf ("SearchEnd=%" PRId32 "\n", SearchEnd);

	for (i = 0; i < 16; i++)
	{
		if (!Load_VAT (1, disc, t->PhysicalPartition, &t->RootEntry, SearchEnd - i - 1, &next_sector))
		{
			break;
		}
	}
	if (i==16)
	{
		t->Initialized++;
		t->LogicalVolume_Type1 = 0;
		t->PhysicalPartition = 0;
		return -1;
	}

	t->Initialized++;
	/* Attach the VAT */
	if (t->LogicalVolume_Type1)
	{
		t->LogicalVolume_Type1->VAT = t;
	}

	{
		struct UDF_VAT_Entries *prev = &t->RootEntry;

		while ((next_sector != 0) && (next_sector != 0xffffffff))
		{
			struct UDF_VAT_Entries *next, *iter;

			for (iter = &t->RootEntry; iter; iter = iter->Previous)
			{
				if (iter->VAT_Location == next_sector)
				{
					fprintf (stderr, "WARNING - Type2_VAT_Initialize() - Circular references detected in VAT history\n");
					break;
				}
			}
			if (iter)
			{
				break;
			}

			next = calloc (1, sizeof (*next));
			if (!next)
			{
				fprintf (stderr, "WARNING - Type2_VAT_Initialize() - calloc failed\n");
				break;
			}

			if (Load_VAT (1, disc, t->PhysicalPartition, next, next_sector, &next_sector))
			{
				free (next);
				break;
			}

			prev->Previous = next;
			prev = next;
		}
	}

	return 0;
}

static void UDF_LogicalVolume_Append_Type1 (struct UDF_LogicalVolumes_t *self, uint16_t PartId, uint16_t PhysicalPartition_VolumeSequenceNumber, uint16_t PhysicalPartition_PartitionNumber)
{
	struct UDF_LogicalVolume_Type1 *t;

	if (!self)
	{
		return;
	}

	t = calloc (1, sizeof (*t));
	t->Common.PartId = PartId;
	t->Common.Type = 1;
	t->Common.PartitionCommon.FetchSector = Type1_FetchSector_Virtual;
	t->Common.PartitionCommon.PushAbsoluteLocations = Type1_PushAbsoluteLocations_Virtual;
	t->Common.PartitionCommon.Initialize = Type1_Initialize;
	t->Common.PartitionCommon.Free = free;
	t->Common.PartitionCommon.DefaultSession = Type1_DefaultSession;
	t->Common.PartitionCommon.SelectSession = Type1_SelectSession;
	t->Common.PartitionCommon.NextSession = Type1_NextSession;
	t->FetchSectorReal = Type1_FetchSector_Real;
	snprintf (t->Common.Info, sizeof (t->Common.Info), "Physical partition located on sequence=%" PRId16 " partition=%" PRId16, PhysicalPartition_VolumeSequenceNumber, PhysicalPartition_PartitionNumber);
	t->PhysicalPartition_VolumeSequenceNumber = PhysicalPartition_VolumeSequenceNumber;
	t->PhysicalPartition_PartitionNumber = PhysicalPartition_PartitionNumber;
	UDF_LogicalVolume_Append (self, &t->Common);
}

static void UDF_LogicalVolume_Append_Type2_VAT (struct UDF_LogicalVolumes_t *self, uint16_t PartId, uint16_t PhysicalPartition_VolumeSequenceNumber, uint16_t PhysicalPartition_PartitionNumber)
{
	struct UDF_LogicalVolume_Type2_VAT *t;

	if (!self)
	{
		return;
	}

	t = calloc (1, sizeof (*t));
	t->Common.PartId = PartId;
	t->Common.Type = 2;
	t->Common.PartitionCommon.FetchSector = Type2_VAT_FetchSector;
	t->Common.PartitionCommon.PushAbsoluteLocations = Type2_VAT_PushAbsoluteLocations;
	t->Common.PartitionCommon.Initialize = Type2_VAT_Initialize;
	t->Common.PartitionCommon.Free = Type2_VAT_Free;
	t->Common.PartitionCommon.DefaultSession = Type2_VAT_DefaultSession;
	t->Common.PartitionCommon.SelectSession = Type2_VAT_SelectSession;
	t->Common.PartitionCommon.NextSession = Type2_VAT_NextSession;
	snprintf (t->Common.Info, sizeof (t->Common.Info), "VAT ontop of Physical Partition sequence=%" PRId16 " partition=%" PRId16, PhysicalPartition_VolumeSequenceNumber, PhysicalPartition_PartitionNumber);
	UDF_LogicalVolume_Append (self, &t->Common);
	t->PhysicalPartition_VolumeSequenceNumber = PhysicalPartition_VolumeSequenceNumber;
	t->PhysicalPartition_PartitionNumber      = PhysicalPartition_PartitionNumber;
}

static int Type2_Metadata_LoadData (int n, struct cdfs_disc_t *disc, struct UDF_LogicalVolume_Type2_Metadata *t, uint32_t Location, int ismirror)
{
	uint8_t *metadata = 0;
	uint64_t metasize = 0;
	struct UDF_FileEntry_t *FE;

	FE = FileEntry (n+1, disc, Location, &t->Master->PartitionCommon, 0);
	if (!FE)
	{
		return -1;
	}

	switch (FE->FileType)
	{
		case FILETYPE_METADATA_FILE:
			if (ismirror)
			{
				N(n+2); debug_printf ("WARNING - filetype expected was FILETYPE_METADATA_MIRROR_FILE:\n");
			}
			break;

		case FILETYPE_METADATA_MIRROR_FILE:
			if (!ismirror)
			{
				N(n+2); debug_printf ("WARNING - filetype expected was FILETYPE_METADATA_FILE:\n");
			}
			break;

		default:
			N(n+2); debug_printf ("Error - filetype was unexpected\n");
			FileEntry_Free (FE);
			return -1;
	}

	if (FileEntryLoadData (disc, FE, &metadata, 16*1024*1024))
	{
		N(n+2); debug_printf ("Error - Unable to load the \"MetaData\" FileEntry\n");
		FileEntry_Free (FE);
		return -1;
	}
	metasize = FE->InformationLength;
	FileEntry_Free (FE);
	FE = 0;

	if (!metasize)
	{
		free (metadata);
		N(n+2); debug_printf ("Error - MetaData was 0 bytes\n");
	}

	if (!t->MetaData)
	{
		t->MetaData = metadata;
		t->MetaSize = metasize;
	} else {
		free (metadata);
	}

	return 0;
}

static int Type2_MetaData_LoadBitmap (int n, struct cdfs_disc_t *disc, struct UDF_LogicalVolume_Type2_Metadata *t)
{
	uint8_t *metadata = 0;

	struct UDF_FileEntry_t *FE;

	FE = FileEntry (n+1, disc, t->MetadataBitmapFileLocation, &t->Master->PartitionCommon, 0);
	if (!FE)
	{
		return -1;
	}

	switch (FE->FileType)
	{
		case FILETYPE_METADATA_BITMAP_FILE:
			break;

		default:
			N(n+2); debug_printf ("Error - filetype was unexpected\n");
			FileEntry_Free (FE);
			return -1;
	}

	if (FileEntryLoadData (disc, FE, &metadata, 8*1024))
	{
		N(n+2); debug_printf ("Error - Unable to load the \"MetaData BitMap\" FileEntry\n");
		FileEntry_Free (FE);
		return -1;
	}

	if (!FE->InformationLength)
	{
		free (metadata);
		N(n+2); debug_printf ("Error - MetaData BitMap was 0 bytes\n");
	} else {
		SpaceBitMapInline (n + 2, metadata, FE->FileAllocation[0].ExtentLocation, FE->InformationLength);
	}

	FileEntry_Free (FE);
	free (metadata);
	return 0;
}

static int Type2_Metadata_Initialize (struct cdfs_disc_t *disc, struct UDF_Partition_Common *self)
{
	struct UDF_LogicalVolume_Type2_Metadata *t = (struct UDF_LogicalVolume_Type2_Metadata *)self;
	int i;

	if (!disc)
	{
		return -1;
	}
	if (!disc->udf_session)
	{
		return -1;
	}
	if (!disc->udf_session->PrimaryVolumeDescriptor)
	{
		return -1;
	}
	if (disc->udf_session->PrimaryVolumeDescriptor->VolumeSequenceNumber != t->VolumeSequenceNumber)
	{	/* This is from another disc */
		return -1;
	}

	if (t->Initialized & 1)
	{
		return -1;
	}
	if (t->Initialized)
	{
		if (t->Master) return 0;
		return -1;
	}
	t->Initialized++;
	for (i=0; i < disc->udf_session->LogicalVolumes->LogicalVolume_N; i++)
	{
		struct UDF_LogicalVolume_Type1 *s1 = (struct UDF_LogicalVolume_Type1 *)disc->udf_session->LogicalVolumes->LogicalVolume[i];
		struct UDF_LogicalVolume_Type2_SparingPartition *s2 = (struct UDF_LogicalVolume_Type2_SparingPartition *)disc->udf_session->LogicalVolumes->LogicalVolume[i];

		if ( (s1->Common.Type == 1) &&
		     (s1->PhysicalPartition_VolumeSequenceNumber == t->VolumeSequenceNumber) &&
		     (s1->PhysicalPartition_PartitionNumber      == t->PartitionNumber) )
		{
			//t->LogicalVolume_Type1 = s;
			t->Master = &s1->Common;
			break;
		}
		if ( (s2->Common.Type == 2) &&
		     (s2->Common.IsSpareablePartitionMap) &&
		     (s2->Common.PartId == t->PartitionNumber) &&
		     (s2->VolumeSequenceNumber == t->VolumeSequenceNumber))
		{
			//t->SparablePartitionMap_Type2 = (struct UDF_SparablePartitionMap_Type2 *)disc->udf_session->LogicalVolumes->LogicalVolume[i];;
			t->Master = &s2->Common;
			break;
		}
	}

//	if ((!t->LogicalVolume_Type1) && (!t->SpareablePartitionMap_Type2))
	if (!t->Master)
	{
		t->Initialized++;
		return -1;
	}

	if (t->Master->PartitionCommon.Initialize (disc, &t->Master->PartitionCommon))
	{
		t->Initialized++;
		t->Master = 0;
		return -1;
	}

	{
		int errors = 0;

		if (!Type2_Metadata_LoadData (1, disc, t, t->MetadataFileLocation, 0))
		{
			errors++;
		}

		if (!Type2_Metadata_LoadData (1, disc, t, t->MetadataMirrorFileLocation, 1))
		{
			errors++;
		}

		if (errors == 2)
		{
			t->Initialized++;
			t->Master = 0;
			return -1;
		}
	}

	if (t->MetadataBitmapFileLocation != 0xffffffff)
	{
		Type2_MetaData_LoadBitmap (1, disc, t);
	}

	t->Initialized++;

	return 0;
}

static void Type2_Metadata_Free (void *self)
{
	struct UDF_LogicalVolume_Type2_Metadata *t = (struct UDF_LogicalVolume_Type2_Metadata *)self;

	free (t->MetaData);

	free (t);
}

static int Type2_Metadata_FetchSector (struct cdfs_disc_t *disc, struct UDF_Partition_Common *self, uint8_t *buffer, uint32_t sector)
{
	struct UDF_LogicalVolume_Type2_Metadata *t = (struct UDF_LogicalVolume_Type2_Metadata *)self;
	if (!t->MetaData)
	{
		return -1;
	}
	if (sector >= (t->MetaSize / SECTORSIZE))
	{
		return -1;
	}
	memcpy (buffer, t->MetaData + sector * SECTORSIZE, SECTORSIZE);
	return 0;
}

static void Type2_Metadata_PushAbsoluteLocations (struct cdfs_disc_t *disc, struct UDF_Partition_Common *self, uint32_t sector, uint32_t length, int skiplength, int handle)
{
#warning Type2 should not contain file-data..?
}

static void Type2_Metadata_DefaultSession (struct cdfs_disc_t *disc, struct UDF_Partition_Common *self, uint32_t *LocationIterator, uint8_t TimeStamp[12])
{
	memset (TimeStamp, 0, 12);
}

static int Type2_Metadata_SelectSession (struct cdfs_disc_t *disc, struct UDF_Partition_Common *self, uint32_t LocationIterator/*, uint8_t TimeStamp[12]*/)
{
	return 0;
}

static int Type2_Metadata_NextSession (struct cdfs_disc_t *disc, struct UDF_Partition_Common *self, uint32_t *LocationIterator, uint8_t TimeStamp[12])
{
	return -1;
}

static void UDF_LogicalVolume_Append_Type2_Metadata (struct UDF_LogicalVolumes_t *self, uint16_t PartId, uint16_t VolumeSequenceNumber, uint16_t PartitionNumber, uint32_t MetadataFileLocation, uint32_t MetadataMirrorFileLocation, uint32_t MetadataBitmapFileLocation, uint32_t AllocationUnitSize, uint32_t AlignmentUnitSize, uint8_t Flags)
{
	struct UDF_LogicalVolume_Type2_Metadata *t;

	if (!self)
	{
		return;
	}

	t = calloc (1, sizeof (*t));
	t->Common.PartId = PartId;
	t->Common.Type = 2;
	t->Common.PartitionCommon.FetchSector = Type2_Metadata_FetchSector;
	t->Common.PartitionCommon.PushAbsoluteLocations = Type2_Metadata_PushAbsoluteLocations;
	t->Common.PartitionCommon.Initialize = Type2_Metadata_Initialize;
	t->Common.PartitionCommon.Free = Type2_Metadata_Free;
	t->Common.PartitionCommon.DefaultSession = Type2_Metadata_DefaultSession;
	t->Common.PartitionCommon.SelectSession = Type2_Metadata_SelectSession;
	t->Common.PartitionCommon.NextSession = Type2_Metadata_NextSession;
	snprintf (t->Common.Info, sizeof (t->Common.Info), "Metadata ontop of Partition sequence=%" PRId16 " partition=%" PRId16, VolumeSequenceNumber, PartitionNumber);
	UDF_LogicalVolume_Append (self, &t->Common);
	t->VolumeSequenceNumber       = VolumeSequenceNumber;
	t->PartitionNumber            = PartitionNumber;
	t->MetadataFileLocation       = MetadataFileLocation;
	t->MetadataMirrorFileLocation = MetadataMirrorFileLocation;
	t->MetadataBitmapFileLocation = MetadataBitmapFileLocation;
	t->AllocationUnitSize         = AllocationUnitSize;
	t->AlignmentUnitSize          = AlignmentUnitSize;
	t->Flags                      = Flags;
}

static void UDF_Load_SparingTable (int n, struct cdfs_disc_t *disc, struct UDF_LogicalVolume_Type2_SparingPartition *t, uint32_t Location)
{
	uint8_t *buffer;
	unsigned int i;
	uint16_t TagIdentifier;
	uint16_t ReallocationTableLength;
#ifdef CDFS_DEBUG
	uint32_t SequenceNumber;
#endif
	int error = 0;
	struct UDF_SparingTable_MapEntry_t *MapEntries;

	if (t->SizeOfEachSparingTable < 64)
	{
		debug_printf ("UDF_Load_SparingTable: size must be atleast 64 bytes\n");
		return;
	}

	buffer = malloc ((t->SizeOfEachSparingTable + SECTORSIZE - 1) & ~(SECTORSIZE - 1));
	if (!buffer)
	{
		fprintf (stderr, "UDF_Load_SparingTable: malloc() failed\n");
		return;
	}

	for (i = 0; i * SECTORSIZE < t->SizeOfEachSparingTable; i++)
	{
#if 0
		This logic was wrong. Location is given in disk absolute, not partition we manage
		if (t->PhysicalPartition->PartitionCommon.FetchSector (disc, &t->PhysicalPartition->PartitionCommon, buffer + i * SECTORSIZE, Location + i))
#else
		if (cdfs_fetch_absolute_sector_2048 (disc, Location + i, buffer + i * SECTORSIZE))
#endif
		{
			free (buffer);
			return;
		}
	}
	if (print_tag_format (n, "", buffer, Location, 1, &TagIdentifier))
	{
		free (buffer);
		return;
	}
	if (TagIdentifier != 0x0000)
	{
		N(n); debug_printf ("Error - invalid TagIdentifier\n");
		free (buffer);
		return;
	}
	N(n+1); debug_printf ("[Sparing Table]\n");
	N(n+2); debug_printf ("Sparing Identifier: "); print_1_7_4 (buffer + 16, 0); debug_printf ("\n");
	if (memcmp (buffer + 16 + 1, "*UDF Sparing Table", 19))
	{
		N(n+2); debug_printf ("Error - wrong identifier, expected \"*UDF Sparing Table\"\n");
		free (buffer);
		return;
	}
	ReallocationTableLength = (buffer[49] << 8) | buffer[48];
	N(n+2); debug_printf ("Reallocation Table Length: %" PRIu16 "\n", ReallocationTableLength);
	if ((8 * ReallocationTableLength + 56 > t->SizeOfEachSparingTable) || (ReallocationTableLength == 0))
	{
		N(n+2); debug_printf ("Error - Table Length is too large\n");
		error = 1;
	}

#ifdef CDFS_DEBUG
	SequenceNumber = (buffer[55] << 24) | (buffer[54] << 16) | (buffer[53] << 8) | buffer[52];
#endif
	N(n+2); debug_printf ("Sequence Number:           %" PRIu32 "\n", SequenceNumber);

	MapEntries = malloc (sizeof (MapEntries[0]) * ReallocationTableLength);
	if (!MapEntries)
	{
		fprintf (stderr, "UDF_Load_SparingTable: malloc() failed #2\n");
		free (buffer);
		return;
	}

	for (i=0; i < ReallocationTableLength; i++)
	{
		if (8 * i + 56 > t->SizeOfEachSparingTable)
		{
			break;
		}
		MapEntries[i].OriginalLocation = (buffer[56 + 3 + i*8] << 24) | (buffer[56 + 2 + i*8] << 16) | (buffer[56 + 1 + i*8] << 8) | buffer[56 + 0 + i*8];
		MapEntries[i].MappedLocation   = (buffer[56 + 7 + i*8] << 24) | (buffer[56 + 6 + i*8] << 16) | (buffer[56 + 5 + i*8] << 8) | buffer[56 + 4 + i*8];
		N(n+2); debug_printf ("MapEntry.%u.OriginalLocation: %" PRIu32, i, MapEntries[i].OriginalLocation);      /* Logical location */
		if (MapEntries[i].OriginalLocation == 0xfffffff0) debug_printf (" packet defective");
		if (MapEntries[i].OriginalLocation == 0xffffffff) debug_printf (" packet available");

		debug_printf ("\n");
		N(n+2); debug_printf ("MapEntry.%u.MappedLocation:   %" PRIu32 "\n", i, MapEntries[i].MappedLocation);   /* Physical location */
	}

	if (!error)
	{
		// deploy if not already deployed
		if (!t->SparingTable)
		{
			t->SparingTable = MapEntries;
			t->SparingTableLength = ReallocationTableLength;
			MapEntries = 0;
		}
	}
	if (MapEntries)
	{
		free (MapEntries);
	}
	free (buffer);
}

static int Type2_SparingPartition_Initialize (struct cdfs_disc_t *disc, struct UDF_Partition_Common *self)
{
	struct UDF_LogicalVolume_Type2_SparingPartition *t = (struct UDF_LogicalVolume_Type2_SparingPartition *)self;
	int i;

	if (!disc)
	{
		return -1;
	}
	if (!disc->udf_session)
	{
		return -1;
	}
	if (!disc->udf_session->PrimaryVolumeDescriptor)
	{
		return -1;
	}
	if (disc->udf_session->PrimaryVolumeDescriptor->VolumeSequenceNumber != t->VolumeSequenceNumber)
	{	/* This is from another disc */
		return -1;
	}

	if (t->Initialized & 1)
	{
		return -1;
	}
	if (t->Initialized)
	{
		return t->SparingTable ? 0 : -1;
	}
	t->Initialized++;
	for (i=0; i < disc->udf_session->PhysicalPartition_N; i++)
	{
		if ( (disc->udf_session->PhysicalPartition[i].PartitionNumber == t->PhysicalPartition_PartitionNumber) /* &&
		     (disc[j]->udf_session->PrimaryVolumeDescriptor->VolumeSequenceNumber == t->PhysicalPartition_VolumeSequenceNumber) */ )
		{
			t->PhysicalPartition = &disc->udf_session->PhysicalPartition[i];
			break;
		}
	}

	if (!t->PhysicalPartition)
	{
		t->Initialized++;
		return -1;
	}

	if (t->PhysicalPartition->PartitionCommon.Initialize (disc, &t->PhysicalPartition->PartitionCommon))
	{
		t->Initialized++;
		t->PhysicalPartition = 0;
		return -1;
	}

	for (i=0; i < t->NumberOfSparingTables; i++)
	{
		UDF_Load_SparingTable (0, disc, t, t->SparingTableLocations[i]);
	}

	t->Initialized++;
	if (!t->SparingTable)
	{
		return -1;
	}

	return 0;
}

static void Type2_SparingPartition_DefaultSession (struct cdfs_disc_t *disc, struct UDF_Partition_Common *self, uint32_t *LocationIterator, uint8_t TimeStamp[12])
{
	memset (TimeStamp, 0, 12);
}

static int Type2_SparingPartition_SelectSession (struct cdfs_disc_t *disc, struct UDF_Partition_Common *self, uint32_t LocationIterator/*, uint8_t TimeStamp[12]*/)
{
	return 0;
}

static int Type2_SparingPartition_NextSession (struct cdfs_disc_t *disc, struct UDF_Partition_Common *self, uint32_t *LocationIterator, uint8_t TimeStamp[12])
{
	return -1;
}

static void Type2_SparingPartition_Free (void *self)
{
	struct UDF_LogicalVolume_Type2_SparingPartition *t = (struct UDF_LogicalVolume_Type2_SparingPartition *)self;

	free (t->SparingTableLocations);
	free (t->SparingTable);
	free (t);
}

static int Type2_SparingPartition_FetchSector (struct cdfs_disc_t *disc, struct UDF_Partition_Common *self, uint8_t *buffer, uint32_t sector)
{
	struct UDF_LogicalVolume_Type2_SparingPartition *t = (struct UDF_LogicalVolume_Type2_SparingPartition *)self;
	uint32_t     PacketIndex;
	uint_fast8_t PacketMinorIndex;
	uint32_t     i;

	if (!t->PhysicalPartition)
	{
		return -1;
	}
	if (!t->SparingTable)
	{
		return -1;
	}

	PacketMinorIndex = sector % t->PacketLength;
	PacketIndex      = sector - PacketMinorIndex;

	for (i=0; i < t->SparingTableLength; i++)
	{
		if (t->SparingTable[i].OriginalLocation == PacketIndex)
		{
			return t->PhysicalPartition->PartitionCommon.FetchSector (disc, &t->PhysicalPartition->PartitionCommon, buffer, t->SparingTable[i].MappedLocation + PacketMinorIndex);
		}
	}
	/* no sparing availble, try as-is */
	return t->PhysicalPartition->PartitionCommon.FetchSector (disc, &t->PhysicalPartition->PartitionCommon, buffer, sector);
}

static void Type2_SparingPartition_PushAbsoluteLocations (struct cdfs_disc_t *disc, struct UDF_Partition_Common *self, uint32_t sector, uint32_t length, int skiplength, int handle)
{
	struct UDF_LogicalVolume_Type2_SparingPartition *t = (struct UDF_LogicalVolume_Type2_SparingPartition *)self;
	uint32_t     PacketIndex;
	uint_fast8_t PacketMinorIndex;
	uint32_t     i;

	if (!t->PhysicalPartition)
	{
		return;
	}
	if (!t->SparingTable)
	{
		return;
	}

	while (length >= 2048)
	{
		PacketMinorIndex = sector % t->PacketLength;
		PacketIndex      = sector - PacketMinorIndex;

		for (i=0; i < t->SparingTableLength; i++)
		{
			if (t->SparingTable[i].OriginalLocation == PacketIndex)
			{
				t->PhysicalPartition->PartitionCommon.PushAbsoluteLocations (disc, &t->PhysicalPartition->PartitionCommon, t->SparingTable[i].MappedLocation + PacketMinorIndex, (length >= 2048) ? 2048 : length, skiplength, handle);
				skiplength = 0;
				goto next;
			}
		}
		/* no sparing availble, try as-is */
		t->PhysicalPartition->PartitionCommon.PushAbsoluteLocations (disc, &t->PhysicalPartition->PartitionCommon, sector, (length >= 2048) ? 2048 : length, skiplength, handle);
		skiplength = 0;
next:
		sector++;
		if (length >= 2048)
		{
			length -= 2048;
		} else {
			length = 0;
		}
	}
}

static void UDF_LogicalVolume_Append_Type2_SparingPartition (struct UDF_LogicalVolumes_t *self, uint16_t PartId, uint16_t VolumeSequenceNumber, uint16_t PartitionNumber, uint16_t PacketLength, uint8_t NumberOfSparingTables, uint32_t SizeOfEachSparingTable, uint32_t *SparingTableLocations)
{
	struct UDF_LogicalVolume_Type2_SparingPartition *t;

	if (!self)
	{
		return;
	}

	if (!NumberOfSparingTables)
	{
		debug_printf ("Error - NumberOfSparingTables is zero\n");
		return;
	}
	if (!SizeOfEachSparingTable)
	{
		debug_printf ("Error - SizeOfEachSparingTable is zero\n");
		return;
	}
	if (!PacketLength)
	{
		debug_printf ("Error - PacketLength is zero\n");
		return;
	}
	if (PacketLength & (7))
	{
		debug_printf ("WARNING - PacketLength is not a multiple of 32KB (using 4096KB sectors)\n");
	}

	t = calloc (1, sizeof (*t));
	t->Common.PartId = PartId;
	t->Common.Type = 2;
	t->Common.IsSpareablePartitionMap = 1;
	t->Common.PartitionCommon.FetchSector = Type2_SparingPartition_FetchSector;
	t->Common.PartitionCommon.PushAbsoluteLocations = Type2_SparingPartition_PushAbsoluteLocations;
	t->Common.PartitionCommon.Initialize = Type2_SparingPartition_Initialize;
	t->Common.PartitionCommon.Free = Type2_SparingPartition_Free;
	t->Common.PartitionCommon.DefaultSession = Type2_SparingPartition_DefaultSession;
	t->Common.PartitionCommon.SelectSession = Type2_SparingPartition_SelectSession;
	t->Common.PartitionCommon.NextSession = Type2_SparingPartition_NextSession;
	snprintf (t->Common.Info, sizeof (t->Common.Info), "SparingPartition ontop of Partition sequence=%" PRId16 " partition=%" PRId16, VolumeSequenceNumber, PartitionNumber);
	UDF_LogicalVolume_Append (self, &t->Common);
	t->VolumeSequenceNumber              = VolumeSequenceNumber;
	t->PhysicalPartition_PartitionNumber = PartitionNumber;
	t->PacketLength                      = PacketLength;
	t->NumberOfSparingTables             = NumberOfSparingTables;
	t->SizeOfEachSparingTable            = SizeOfEachSparingTable;
	t->SparingTableLocations             = calloc (sizeof (t->SparingTableLocations[0]), NumberOfSparingTables);
	if (t->SparingTableLocations)
	{
		memcpy (t->SparingTableLocations, SparingTableLocations, sizeof (t->SparingTableLocations[0]) * NumberOfSparingTables);
	}
}

#ifdef CDFS_DEBUG
static void DumpFS_UDF3 (struct cdfs_disc_t *disc, struct UDF_FS_DirectoryEntry_t *d, const char *prefix)
{
	struct UDF_FS_DirectoryEntry_t *di;
	struct UDF_FS_FileEntry_t *fi;

	char *newprefix = malloc (strlen (prefix) + 1 + (d->DirectoryName ? strlen (d->DirectoryName) : 0) + 1);
	if (!newprefix)
	{
		fprintf (stderr, "DumpFS_UDF3() malloc() failed\n");
		return;
	}
	sprintf (newprefix, "%s/%s", prefix, d->DirectoryName ? d->DirectoryName : "");
	debug_printf ("%s :\n", newprefix);

	for (di = d->DirectoryEntries; di; di = di->Next)
	{
		debug_printf ("d");
		debug_printf ("%c%c%c%c%c%c%c%c%c%c",
			(di->FE->Permissions & 0x1000) ? 'r' : '-', /* owner read */
			(di->FE->Permissions & 0x0800) ? 'w' : '-', /* owner write */
			(di->FE->Flags & 0x0040) ? (  (di->FE->Permissions & 0x0400) ? 's' : 'S'  ) : (  (di->FE->Permissions & 0x0400) ? 'x' : '-'  ), /* owner execute */
			(di->FE->Permissions & 0x0080) ? 'r' : '-', /* group read */
			(di->FE->Permissions & 0x0040) ? 'w' : '-', /* group write */
			(di->FE->Flags & 0x0080) ? (  (di->FE->Permissions & 0x0020) ? 's' : 'S'  ) : (  (di->FE->Permissions & 0x0020) ? 'x' : '-'  ), /* group execute */
			(di->FE->Permissions & 0x0004) ? 'r' : '-', /* other read */
			(di->FE->Permissions & 0x0002) ? 'w' : '-', /* other write */
			(di->FE->Permissions & 0x0001) ? 'x' : '-', /* other execute */
			(di->FE->Flags & 0x0100) ? 't' : '-'
			/*
			0x0008 - Other can change permissions
			0x0010 - Other can delete
			0x0100 - Group can change permissions
			0x0200 - Group can delete
			0x2000 - Owner can change permissions
			0x4000 - Owner can delete
			*/
		);

		debug_printf (" %4"PRId32" %4"PRId32, di->FE->UID, di->FE->GID);

		debug_printf ("            ");

		print_1_7_3 (di->FE->ctime);
		debug_printf (" %s%s\n", di->DirectoryName, di->PreviousVersion ? "  [Previous versions exists]":"");
	}

	for (fi = d->FileEntries; fi; fi = fi->Next)
	{
		switch (fi->FE->FileType)
		{
			case FILETYPE_BLOCK_SPECIAL_DEVICE:     debug_printf ("b"); break;
			case FILETYPE_CHARACTER_SPECIAL_DEVICE: debug_printf ("c"); break;
			case FILETYPE_FIFO:                     debug_printf ("p"); break;
			case FILETYPE_C_ISSOCK:                 debug_printf ("s"); break;
			case FILETYPE_SYMLINK:                  debug_printf ("l"); break;
			case FILETYPE_FILE:                     debug_printf ("-"); break;
			default:                                debug_printf ("?"); break;
		}
		debug_printf ("%c%c%c%c%c%c%c%c%c%c",
			(fi->FE->Permissions & 0x1000) ? 'r' : '-', /* owner read */
			(fi->FE->Permissions & 0x0800) ? 'w' : '-', /* owner write */
			(fi->FE->Flags & 0x0040) ? (  (fi->FE->Permissions & 0x0400) ? 's' : 'S'  ) : (  (fi->FE->Permissions & 0x0400) ? 'x' : '-'  ), /* owner execute */
			(fi->FE->Permissions & 0x0080) ? 'r' : '-', /* group read */
			(fi->FE->Permissions & 0x0040) ? 'w' : '-', /* group write */
			(fi->FE->Flags & 0x0080) ? (  (fi->FE->Permissions & 0x0020) ? 's' : 'S'  ) : (  (fi->FE->Permissions & 0x0020) ? 'x' : '-'  ), /* group execute */
			(fi->FE->Permissions & 0x0004) ? 'r' : '-', /* other read */
			(fi->FE->Permissions & 0x0002) ? 'w' : '-', /* other write */
			(fi->FE->Permissions & 0x0001) ? 'x' : '-', /* other execute */
			(fi->FE->Flags & 0x0100) ? 't' : '-'
			/*
			0x0008 - Other can change permissions
			0x0010 - Other can delete
			0x0100 - Group can change permissions
			0x0200 - Group can delete
			0x2000 - Owner can change permissions
			0x4000 - Owner can delete
			*/
		);

		debug_printf (" %4"PRId32" %4"PRId32, fi->FE->UID, fi->FE->GID);

		switch (fi->FE->FileType)
		{
			case FILETYPE_BLOCK_SPECIAL_DEVICE:
			case FILETYPE_CHARACTER_SPECIAL_DEVICE:
				if (fi->FE->HasMajorMinor)
				{
					debug_printf ("%4d %4d   ", fi->FE->Major, fi->FE->Minor);
				} else {
					debug_printf ("   ?    ?   ");
				}
				break;
			case FILETYPE_FILE:
				debug_printf ("%11" PRIu64 " ", fi->FE->InformationLength);
				break;
			default:
				debug_printf ("            ");
				break;
		}

		print_1_7_3 (fi->FE->mtime);
		debug_printf (" %s", fi->FileName);

		if (fi->FE->FileType == FILETYPE_SYMLINK)
		{
			debug_printf (" -> %s", fi->Symlink ? fi->Symlink : "???");
		}

		debug_printf ("%s\n", fi->PreviousVersion ? "  [Previous versions exists]":"");
	}


	for (di = d->DirectoryEntries; di; di = di->Next)
	{
		DumpFS_UDF3 (disc, di, newprefix);		
	}

	free (newprefix);
}

static void DumpFS_UDF2 (struct cdfs_disc_t *disc, struct UDF_RootDirectory_t *rd)
{
	debug_printf ("UDF ROOT "); print_1_7_3 (rd->FileSetDescriptor_TimeStamp_1_7_3); debug_printf ("\n");
	if (rd->Root)
	{
		struct UDF_LogicalVolume_Common *lv = UDF_GetLogicalPartition (disc, rd->FileSetDescriptor_PartitionNumber);
		if (lv)
		{
			lv->PartitionCommon.SelectSession (disc, &lv->PartitionCommon, rd->FileSetDescriptor_Partition_Session);
		}
		DumpFS_UDF3 (disc, rd->Root, ".");
	}
}

OCP_INTERNAL void DumpFS_UDF (struct cdfs_disc_t *disc)
{
	int i;

	if (!disc->udf_session->LogicalVolumes) return;
	for (i=0; i < disc->udf_session->LogicalVolumes->RootDirectories_N; i++)
	{
		DumpFS_UDF2 (disc, &disc->udf_session->LogicalVolumes->RootDirectories[i]);
	}	
}
#endif

static void CDFS_Render_UDF3 (struct cdfs_disc_t *disc, struct UDF_FS_DirectoryEntry_t *d, uint32_t parent_directory)
{
	struct UDF_FS_DirectoryEntry_t *di;
	struct UDF_FS_FileEntry_t *fi;

	for (fi = d->FileEntries; fi; fi = fi->Next)
	{
		int handle, j;

		switch (fi->FE->FileType)
		{
			default:
			case FILETYPE_BLOCK_SPECIAL_DEVICE:
			case FILETYPE_CHARACTER_SPECIAL_DEVICE:
			case FILETYPE_FIFO:
			case FILETYPE_C_ISSOCK:
			case FILETYPE_SYMLINK:
				goto next;
			case FILETYPE_FILE:
				break;
		}

#warning We could iterate older versions here!!
		handle = CDFS_File_add (disc, parent_directory, fi->FileName);
		for (j=0; j < fi->FE->FileAllocations; j++)
		{
			uint32_t InformationLength = fi->FE->FileAllocation[j].InformationLength;
			uint32_t ExtentLocation = fi->FE->FileAllocation[j].ExtentLocation;
			if (!fi->FE->FileAllocation[j].Partition)
			{
				CDFS_File_zeroextent (disc, handle, InformationLength);
				continue;
			}
			if (InformationLength)
			{
				int skiplength = 0;
				if (fi->FE->InlineData)
				{
					skiplength = fi->FE->FileAllocation[j].SkipLength;
				}
				fi->FE->FileAllocation[j].Partition->PushAbsoluteLocations (disc, fi->FE->FileAllocation[j].Partition, ExtentLocation, InformationLength, skiplength, handle);
			}
		}
next: {}
	}

	for (di = d->DirectoryEntries; di; di = di->Next)
	{
		uint32_t this_directory_handle = CDFS_Directory_add (disc, parent_directory, di->DirectoryName);

		CDFS_Render_UDF3 (disc, di, this_directory_handle);
	}
}


static void CDFS_Render_UDF2 (struct cdfs_disc_t *disc, struct UDF_RootDirectory_t *rd, uint32_t parent_directory)
{
	if (rd->Root)
	{
		struct UDF_LogicalVolume_Common *lv = UDF_GetLogicalPartition (disc, rd->FileSetDescriptor_PartitionNumber);
		if (lv)
		{
			lv->PartitionCommon.SelectSession (disc, &lv->PartitionCommon, rd->FileSetDescriptor_Partition_Session);
		}
		CDFS_Render_UDF3 (disc, rd->Root, parent_directory);
	}
}

OCP_INTERNAL void CDFS_Render_UDF (struct cdfs_disc_t *disc, uint32_t parent_directory) /* parent_directory should point to "UDF" */
{
	int i;
	if (!disc->udf_session->LogicalVolumes) return;
	for (i=0; i < disc->udf_session->LogicalVolumes->RootDirectories_N; i++)
	{
		CDFS_Render_UDF2 (disc, &disc->udf_session->LogicalVolumes->RootDirectories[i], parent_directory);
	}	
}
