/* OpenCP Module Player
 * copyright (c) 2022-'24 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Parsing of El-Torito (boot information)
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

#ifdef CDFS_DEBUG
static int ElTorito_ValiationEntry (uint8_t buffer[0x20], uint8_t *arch)
{
	int retval = 0;
	uint16_t checksumA = 0;

	int i;

	debug_printf ("  [ElTorito Valiation Header]\n");

	if (buffer[0] != 0x01)
	{
		debug_printf ("  Warning - Header ID invalid (expected 0x01, got 0x%02" PRIx8 "\n", buffer[0]);
		retval = 1;
	}

	*arch = buffer[1];
	switch (buffer[1])
	{
		case 0x00: debug_printf ("   PlatformID=80x86\n"); break;
		case 0x01: debug_printf ("   PlatformID=PowerPC\n"); break;
		case 0x02: debug_printf ("   PlatformID=Mac\n"); break;
		case 0xef: debug_printf ("   PlatformID=EFI?\n"); break; /* guess based on data found */
		default:   debug_printf ("   PlatformID=Unknown 0x%02" PRIx8 "\n", buffer[1]);
	}

	if (buffer[2] != 0x00)
	{
		debug_printf ("   Warning - Reserved header at offset 2 if not 0x00, but 0x%02" PRIx8 "\n", buffer[2]);
	}

	if (buffer[3] != 0x00)
	{
		debug_printf ("   Warning - Reserved header at offset 3 if not 0x00, but 0x%02" PRIx8 "\n", buffer[3]);
	}

	debug_printf ("   IDstring=\"");
	for (i=0x04; i <= 0x1b; i++)
	{
		if (!buffer[i])
		{
			break;
		}
		debug_printf ("%c", buffer[i]);
	}
	debug_printf ("\"\n");

	/* not much tested... */
	for (i=0; i < 0x10; i++)
	{
		uint16_t t = (buffer[(i<<1) + 0x01] << 8) |
                              buffer[(i<<1) + 0x00];
		checksumA += t;
	}
	if (checksumA)
	{
		debug_printf ("   Warning, Checksum failed (got 0x%04" PRIx16", but expected 0x0000 )!\n", checksumA);
		retval = 1;
	} else {
		debug_printf ("   Checksum correct\n");
	}

	if (buffer[0x1e] != 0x55)
	{
		debug_printf ("   KeySignature #1 is not 0x55, but 0x%02" PRIx8 "\n", buffer[0x1e]);
	}

	if (buffer[0x1f] != 0xaa)
	{
		debug_printf ("   KeySignature #2 is not 0xaa, but 0x%02" PRIx8 "\n", buffer[0x1f]);
	}

	return retval;
}

static int ElTorito_SectionHeaderEntry (uint8_t buffer[0x20], int index1, int *last, uint8_t *arch, uint16_t *sections)
{
	int retval = 0;
	int i;

	debug_printf ("  [Section Header Entry %d]\n", index1);

	switch (buffer[0])
	{
		case 0x90: debug_printf ("   More section headers will follow\n"); *last = 0; break;
		case 0x91: debug_printf ("   This is the last header\n"); *last = 1; break;
		default: debug_printf ("   Invalid header ID: 0x%02" PRIx8 "\n", buffer[0]); retval = 1; break;
	}

	*arch = buffer[1];
	switch (buffer[1])
	{
		case 0x00: debug_printf ("   PlatformID=80x86\n"); break;
		case 0x01: debug_printf ("   PlatformID=PowerPC\n"); break;
		case 0x02: debug_printf ("   PlatformID=Mac\n"); break;
		case 0xef: debug_printf ("   PlatformID=EFI?\n"); break; /* guess based on data found */
		default:   debug_printf ("   PlatformID=Unknown 0x%02" PRIx8 "\n", buffer[1]);
	}

	*sections = (buffer[3] << 8) | buffer[2];
	debug_printf ("   Sections: %" PRId16 "\n", *sections);

	debug_printf ("   IDstring=\"");
	for (i=0x04; i <= 0x1f; i++)
	{
		if (!buffer[i])
		{
			break;
		}
		debug_printf ("%c", buffer[i]);
	}
	debug_printf ("\"\n");

	return retval;
}

static int ElTorito_SectionEntry (const uint8_t arch, uint8_t buffer[0x20], int index1, int index2)
{
	int retval = 0;
	int i;

	if (index1 == 0)
	{
		debug_printf ("   [Initial/Default Entry]\n");
	} else {
		debug_printf ("   [Section Entry %d.%d\n", index1, index2+1);
	}

	switch (buffer[0])
	{
		case 0x88: debug_printf ("    Bootable\n"); break;
		case 0x00: debug_printf ("    Not bootable\n"); break;
		default:   debug_printf ("    Invalid indicator: 0x%02" PRIx8 "\n", buffer[0]); retval = 1; break;
	}

	switch (buffer[1] & 0x0f)
	{
		case 0x00: debug_printf ("    No Emulation (?)\n"); break;
		case 0x01: debug_printf ("    1.2 meg diskette\n"); break;
		case 0x02: debug_printf ("    1.44 meg diskette\n"); break;
		case 0x03: debug_printf ("    2.88 meg diskette\n"); break;
		case 0x04: debug_printf ("    Hard Disk (drive 80)\n"); break;
		default: debug_printf ("    Invalid media type: 0x%02" PRIx8 "\n", buffer[1]); retval = 1; break;
	}

	if (arch == 0x00) /* 80x86 */
	{
		uint16_t segment = (buffer[3] << 8) | buffer[2];
		if (segment == 0x0000)
		{
			debug_printf ("    Load into default memory =>  [07c0:0000]\n");
		} else {
			debug_printf ("    Load into memory [%04" PRIx16 ":0000]\n", segment);
		}
	} else {
		uint32_t addr = (buffer[2] << 12) | (buffer[3] << 4);
		debug_printf ("    Load into memory [%08" PRIx32"]\n", addr);
	}

	debug_printf ("    System Type (should match the active partion from disk image if hard drive): %s\n", PartitionType(buffer[4]));

	if (buffer[5] != 0x00)
	{
		debug_printf ("    Warning - Reserved header at offset 5 if not 0x00, but 0x%02" PRIx8 "\n", buffer[5]);
	}

	{
		uint16_t sectorcount = (buffer[7] << 8) | buffer[6];
		debug_printf ("    Sectors to load: %" PRId16 "\n", sectorcount);
	}

	{
		int32_t rba = (buffer[11] << 24) | (buffer[10] << 16) | (buffer[9] << 8) | buffer[8];
		debug_printf ("    Relative offset to start of image: %" PRId32"\n", rba);
	}

	if (index1 == 0)
	{
		for (i=0x0c; i <= 0x1f; i++)
		{
			if (buffer[i])
			{
				debug_printf ("    Warning - Reserved header at offset %d if not 0x00, but 0x%02" PRIx8 "\n", i, buffer[i]);
			}
		}
	} else {
		if (buffer[0x0c] == 0x00)
		{
			debug_printf ("   No selection criteria\n");
		} else if (buffer[0x0c] == 0x01)
		{
			debug_printf ("    Language and Version Information (IBM)\n");
			debug_printf ("   ");
			for (i=0x0d; i <= 0x1f; i++)
			{
				debug_printf (" 0x%02" PRIx8 "\n", buffer[i]);
			}
			debug_printf ("\n");
		} else {
			debug_printf ("   Invalid selection criteria type: 0x%02" PRIx8 "\n", buffer[0x0c]);
			retval = 1;
		}
	}

	return retval;
}

static int ElTorito_SectionEntryExtension (uint8_t buffer[0x20], int index1, int index2, int index3, int *last)
{
	int retval = 0;
	int i;

	debug_printf ("    [Section Entry Extension %d.%d.%d\n", index1, index2+1, index3);

	if (buffer[0] != 0x44)
	{
		debug_printf ("     Invalid ID indicator: 0x%02" PRIx8 "\n", buffer[0]);
		retval = 1;
	}

	*last = !!(buffer[1] & 0x20);

	for (i=0x02; i <= 0x1f; i++)
	{
		debug_printf (" 0x%02" PRIx8 "\n", buffer[i]);
	}
	debug_printf ("\n");

	return retval;
}

static int ElTorito_check_offset (struct cdfs_disc_t *disc, uint32_t *sector, uint8_t *buffer, int *offset)
{
	if (*offset >= SECTORSIZE)
	{
		*sector = (*sector) + 1;
		*offset = 0;

		if (cdfs_fetch_absolute_sector_2048 (disc, *sector, buffer))
		{
			debug_printf ("\n Failed to fetch next El Torito boot description at absolute sector%"PRId32"\n", *sector);
			return -1;
		}

		debug_printf ("\n ElTorito data at absolute sector %"PRId32"\n", *sector);
	}
	return 0;
}

static void ElTorito_abs_sector (struct cdfs_disc_t *disc, uint32_t sector)
{
	uint8_t buffer[SECTORSIZE];
	uint8_t arch;
	int i, j;
	int offset = 0;
	int lastheader = 0;

	if (cdfs_fetch_absolute_sector_2048 (disc, sector, buffer))
	{
		debug_printf ("Failed to fetch El Torito boot description at absolute sector%"PRId32"\n", sector);
		return;
	}

	debug_printf ("\n ElTorito data at absolute sector %"PRId32"\n", sector);
	debug_printf ("\n (offset=0x%04x)\n", offset);
	if (ElTorito_ValiationEntry (buffer + offset, &arch))
	{
		return;
	}
	offset += 0x020;

	debug_printf ("\n (offset=0x%04x)\n", offset);
	if (ElTorito_SectionEntry (arch, buffer + offset, 0, 0))
	{
		return;
	}
	offset += 0x020;


	if (buffer[offset] == 0x00)
	{
		debug_printf ("\n no sections available for further parsing\n");
	} else {
		for (i=1;; i++)
		{
			uint16_t entries = 0;

			if (ElTorito_check_offset (disc, &sector, buffer, &offset))
			{
				return;
			}

			debug_printf ("\n (offset=0x%04x)\n", offset);
			if (ElTorito_SectionHeaderEntry (buffer + offset, i, &lastheader, &arch, &entries))
			{
				return;
			}
			offset += 0x020;

			for (j=0; j < entries; j++)
			{
				int last = 0;
				int k = 0;

				if (ElTorito_check_offset (disc, &sector, buffer, &offset))
				{
					return;
				}

				debug_printf ("\n (offset=0x%04x)\n", offset);
				if (ElTorito_SectionEntry (arch, buffer + offset, i, j))
				{
					return;
				}
				offset += 0x020;

				while (!last)
				{
					k++;

					if (ElTorito_check_offset (disc, &sector, buffer, &offset))
					{
						return;
					}

					if (buffer[offset] == 0x00)
					{
						debug_printf ("\n no sections entry extension available for further parsing (not according to standard)\n");
						last = 1;
						break;
					}

					debug_printf ("\n (offset=0x%04x)\n", offset);
					if (ElTorito_SectionEntryExtension (buffer + offset, i, j, k, &last))
					{
						return;
					}
					offset += 0x020;
				}
			}

			if (lastheader)
			{
				break;
			}
		}
	}

	return;
}
#endif
