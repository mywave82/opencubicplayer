/* OpenCP Module Player
 * copyright (c) 2022 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Parsing of ISO9660 SUSP RRIP blocks
 * Rock Ridge Interchange Protocol
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

static void decode_rrip_RR (struct Volume_Description_t *self, uint8_t *buffer)
{
	debug_printf ("       Rock Ridge\n");
	if ((buffer[2] != 5))
	{
		debug_printf ("WARNING - Length is wrong\n");
		return;
	}
	if (buffer[3] != 1)
	{
		debug_printf ("WARNING - Version is wrong\n");
		return;
	}
	self->RockRidge = 1;
	debug_printf ("        Flags: 0x%02" PRIx8 "\n", buffer[4]);
	if (buffer[4] & 0x01) debug_printf ("         Expect PX\n");
	if (buffer[4] & 0x02) debug_printf ("         Expect PN\n");
	if (buffer[4] & 0x04) debug_printf ("         Expect SL\n");
	if (buffer[4] & 0x08) debug_printf ("         Expect NM\n");
	if (buffer[4] & 0x10) debug_printf ("         Expect CL\n");
	if (buffer[4] & 0x20) debug_printf ("         Expect PL\n");
	if (buffer[4] & 0x40) debug_printf ("         Expect RE\n");
	if (buffer[4] & 0x80) debug_printf ("         Expect TF\n");
}

static void decode_rrip_PX (struct Volume_Description_t *self, struct iso_dirent_t *de, uint8_t *buffer)
{
	//uint32_t st_mode;
	//uint32_t st_nlink;
	//uint32_t st_uid;
	//uint32_t st_gid;
	//uint32_t st_inod;

	debug_printf ("       POSIX\n");
	if ((buffer[2] != 44) && (buffer[2] != 36))
	{
		debug_printf ("WARNING - Length is wrong\n");
		return;
	}
	if (buffer[3] != 1)
	{
		debug_printf ("WARNING - Version is wrong\n");
		return;
	}

	self->RockRidge = 1;

	de->RockRidge_PX_Present = 1;

	de->RockRidge_PX_st_mode = decode_uint32_both (buffer + 4, "        st_mode");

	debug_printf ("         st_mode: %c%c%c%c%c%c%c%c%c%c\n",
		(de->RockRidge_PX_st_mode & 0000400) ? 'r' : '-', /* S_IRUSR */
		(de->RockRidge_PX_st_mode & 0000200) ? 'w' : '-', /* S_IWUSR */
		(de->RockRidge_PX_st_mode & 0004000) ?
			((de->RockRidge_PX_st_mode & 0000100) ? 's' : 'S'): /* S_IXUSR + SUID */
			((de->RockRidge_PX_st_mode & 0000100) ? 'x' : '-'), /* S_IXUSR */
		(de->RockRidge_PX_st_mode & 0000040) ? 'r' : '-', /* S_IRGRP */
		(de->RockRidge_PX_st_mode & 0000020) ? 'w' : '-', /* S_IWGRP */
		(de->RockRidge_PX_st_mode & 0002000) ?
			((de->RockRidge_PX_st_mode & 0000010) ? 's' : 'S'): /* S_IXGRP + GUID*/
			((de->RockRidge_PX_st_mode & 0000010) ? 'x' : '-'), /* S_IXGRP */
		(de->RockRidge_PX_st_mode & 0000004) ? 'r' : '-', /* S_IROTH */
		(de->RockRidge_PX_st_mode & 0000002) ? 'w' : '-', /* S_IWOTH */
		(de->RockRidge_PX_st_mode & 0000001) ? 'x' : '-', /* S_IXOTH */
		(de->RockRidge_PX_st_mode & 0001000) ? 't' : '-'); /* S_ISVTX (sticky) */
	debug_printf ("         st_mode.type: ");
	switch (de->RockRidge_PX_st_mode & 0170000)
	{
		case 0140000: debug_printf ("socket"); break; /* S_IFSOCK */
		case 0120000: debug_printf ("symbolic link"); break; /* S_IFLNK */
		case 0100000: debug_printf ("regular"); break; /* S_IFREG */
		case 0060000: debug_printf ("block special"); break; /* S_IFBLK */
		case 0020000: debug_printf ("character special"); break; /* S_IFCHR */
		case 0040000: debug_printf ("directory"); break; /* S_IFDIR */
		case 0010000: debug_printf ("pipe or FIFO"); break; /* S_IFIFO */
		default: debug_printf ("??"); break;
	}
	debug_printf ("\n");

	/* st_nlink = */ decode_uint32_both (buffer + 12 , "        st_nlink");
	de->RockRidge_PX_st_uid   = decode_uint32_both (buffer + 20 , "        st_uid");
	de->RockRidge_PX_st_gid   = decode_uint32_both (buffer + 28 , "        st_gid");
	if (buffer[2] == 44)
	{
		/* st_inod = */ decode_uint32_both (buffer + 36 , "        st_inod");
	}
}

static void decode_rrip_PN (struct Volume_Description_t *self, struct iso_dirent_t *de, uint8_t *buffer)
{
	//uint32_t major, minor;
	debug_printf ("       Node (char/block device major/minor)\n");
	if ((buffer[2] != 20))
	{
		debug_printf ("WARNING - Length is wrong\n");
		return;
	}
	if (buffer[3] != 1)
	{
		debug_printf ("WARNING - Version is wrong\n");
		return;
	}

	self->RockRidge = 1;

	de->RockRidge_PN_Present = 1;
	de->RockRidge_PN_major = decode_uint32_both (buffer +  4, "        major");
	de->RockRidge_PN_minor = decode_uint32_both (buffer + 12, "        minor");
}

static void decode_rrip_SL (struct Volume_Description_t *self, struct iso_dirent_t *de, uint8_t *buffer)
{
	uint8_t *b;
	int l;
	uint8_t *temp;

	int i;
	debug_printf ("       Symlink\n");
	if (buffer[2] < 6)
	{
		debug_printf ("WARNING - Length is wrong\n");
		return;
	}
	if (buffer[3] != 1)
	{
		debug_printf ("WARNING - Version is wrong\n");
		return;
	}

	self->RockRidge = 1;

	debug_printf ("        Flags: 0x%02" PRIx8 "\n", buffer[4]);
	if (buffer[4] & 0x01)
	{
		debug_printf ("         CONTINUE - Record continues in the next entry\n");
	}

	debug_printf ("        Component Area:");
	for (i = 5; i < buffer[2]; i++)
	{
		debug_printf (" %02" PRIx8, buffer[i]);
	}
	debug_printf ("\n");

	b = buffer + 5;
	l = buffer[2] - 5;

	temp = realloc (de->RockRidge_Symlink_Components, de->RockRidge_Symlink_Components_Length + l);
	if (temp)
	{
		de->RockRidge_Symlink_Components = temp;
		memcpy (de->RockRidge_Symlink_Components + de->RockRidge_Symlink_Components_Length, b, l);
		de->RockRidge_Symlink_Components_Length += l;
	}

	while (l >= 2)
	{
		debug_printf ("         Flags: 0x%02" PRIx8 "\n", b[0]);
		if (b[0] & 0x01) debug_printf ("          CONTINUE - Record continues in the next entry\n");
		if (b[0] & 0x02) debug_printf ("          CURRENT - '.'\n");
		if (b[0] & 0x04) debug_printf ("          PARENT - '..'\n");
		if (b[0] & 0x08) debug_printf ("          ROOT - '/'\n");
		if (b[0] & 0x10) debug_printf ("          RESERVED - root of the drive\n");
		if (b[0] & 0x20) debug_printf ("          RESERVED - network name of the current host\n");
		if (2 + b[1] > l) { debug_printf ("WARNING - ran out of data\n"); break; }
		debug_printf ("         Component: \"");
		for (i = 0; i < b[1]; i++)
		{
			debug_printf ("%c", b[2+i]);
		}
		debug_printf ("\"\n");
		l -= 2 + b[1];
		b += 2 + b[1];
	}
}

static void decode_rrip_NM (struct Volume_Description_t *self, struct iso_dirent_t *de, uint8_t *buffer)
{
	int i;
	uint8_t *temp;

	debug_printf ("       Alternate name\n");
	if (buffer[2] < 5)
	{
		debug_printf ("WARNING - Length is wrong\n");
		return;
	}
	if (buffer[3] != 1)
	{
		debug_printf ("WARNING - Version is wrong\n");
		return;
	}

	self->RockRidge = 1;

	debug_printf ("        Flags: 0x%02" PRIx8 "\n", buffer[4]);
	if (buffer[4] & 0x01) debug_printf ("         CONTINUE - Record continues in the next entry\n");
	if (buffer[4] & 0x02) debug_printf ("         CURRENT - This record should be for a '.' entry\n");
	if (buffer[4] & 0x04) debug_printf ("         PARENT - This record should be for a '..' entry\n");
	if (buffer[4] & 0x20) debug_printf ("         RESERVED - network name of the system\n");

	debug_printf ("        Name Content: \"");
	for (i = 5; i < buffer[2]; i++)
	{
		debug_printf ("%c",  buffer[i]);
	}
	debug_printf ("\"\n");

	temp = realloc (de->Name_RockRidge, de->Name_RockRidge_Length + buffer[2] - 5 + 1);
	if (temp)
	{
		de->Name_RockRidge = temp;
		memcpy (de->Name_RockRidge + de->Name_RockRidge_Length, buffer + 5, buffer[2] - 5);
		de->Name_RockRidge_Length += buffer[2] - 5;
		de->Name_RockRidge[de->Name_RockRidge_Length] = 0; /* zero-termination makes life so much easier */
	}
}

static void decode_rrip_CL (struct Volume_Description_t *self, struct iso_dirent_t *de, uint8_t *buffer)
{
	debug_printf ("       Child Location (replace file, with augmented directory)\n");
	if (buffer[2] != 12)
	{
		debug_printf ("WARNING - Length is wrong\n");
		return;
	}
	if (buffer[3] != 1)
	{
		debug_printf ("WARNING - Version is wrong\n");
		return;
	}

	self->RockRidge = 1;

	de->RockRidge_IsAugmentedDirectory = 1;
	de->RockRidge_AugmentedDirectoryFrom = decode_uint32_both (buffer + 4, "        Location");
	/* Ignore all attributes except name and NM tag. All other attributes should be taken from '.' in the augmented directory */
	/* We should not need to Queue, since the directory should normally be visible somewhere else in the non-rockridge version of the tree, and we are missing the Length */
}

static void decode_rrip_PL (struct Volume_Description_t *self, struct iso_dirent_t *de, uint8_t *buffer)
{
	self->RockRidge = 1;

	debug_printf ("       Parent Location (redirect the .. directory entry)\n");
	if (buffer[2] != 12)
	{
		debug_printf ("WARNING - Length is wrong\n");
		return;
	}
	if (buffer[3] != 1)
	{
		debug_printf ("WARNING - Version is wrong\n");
		return;
	}

	self->RockRidge = 1;

	de->RockRidge_DotDotIsRedirected = 1;
	de->RockRidge_DotDotRedirectedTo = decode_uint32_both (buffer + 4, "        Location");
}


static void decode_rrip_RE (struct Volume_Description_t *self, struct iso_dirent_t *de, uint8_t *buffer)
{
	debug_printf ("       Relocated Entry (This entry should be hidden if displayed as Rock Ridge)\n");
	if (buffer[2] != 4)
	{
		debug_printf ("WARNING - Length is wrong\n");
		return;
	}
	if (buffer[3] != 1)
	{
		debug_printf ("WARNING - Version is wrong\n");
		return;
	}

	self->RockRidge = 1;

	de->RockRidge_DirectoryIsRedirected = 1;
}

static void decode_rrip_TF (struct Volume_Description_t *self, struct iso_dirent_t *de, uint8_t *buffer)
{
#ifdef CDFS_DEBUG
	uint8_t *b;
	int len;
	debug_printf ("       Time fields\n");
	if (buffer[2] < 5)
	{
		debug_printf ("WARNING - Length is way too short\n");
		return;
	}

	len = 5 + ((!!(buffer[4] & 0x01)) + (!!(buffer[4] & 0x02)) + (!!(buffer[4] & 0x04)) + (!!(buffer[4] & 0x08)) + (!!(buffer[4] & 0x10)) + (!!(buffer[4] & 0x20)) + (!!(buffer[4] & 0x40))) * ((buffer[4] & 0x80) ? 17 : 7);
	if (buffer[2] < len)
	{
		debug_printf ("WARNING - Length is too short\n");
		return;
	}
	b = buffer + 5;

	if (buffer[4] & 0x01)
	{
		de->RockRidge_TF_Created_Present = 1;
		if (buffer[4] & 0x80) { decode_datetime_17 (b, "        created", &de->RockRidge_TF_Created); b += 17; } else { decode_datetime_7 (b, "        created", &de->RockRidge_TF_Created); b += 7; }
	}
	if (buffer[4] & 0x02)
	{
		if (buffer[4] & 0x80) { decode_datetime_17 (b, "        st_mtime", 0); b += 17; } else { decode_datetime_7 (b, "        st_mtime", 0); b += 7; }
	}
	if (buffer[4] & 0x04)
	{
		if (buffer[4] & 0x80) { decode_datetime_17 (b, "        st_atime", 0); b += 17; } else { decode_datetime_7 (b, "        st_atime", 0); b += 7; }
	}
	if (buffer[4] & 0x08)
	{
		if (buffer[4] & 0x80) { decode_datetime_17 (b, "        st_ctime", 0); b += 17; } else { decode_datetime_7 (b, "        st_ctime", 0); b += 7; }
	}
	if (buffer[4] & 0x10)
	{
		if (buffer[4] & 0x80) { decode_datetime_17 (b, "        backup", 0); b += 17; } else { decode_datetime_7 (b, "        backup", 0); b += 7; }
	}
	if (buffer[4] & 0x20)
	{
		if (buffer[4] & 0x80) { decode_datetime_17 (b, "        expiration", 0); b += 17; } else { decode_datetime_7 (b, "        expiration", 0); b += 7; }
	}
	if (buffer[4] & 0x40)
	{
		if (buffer[4] & 0x80) { decode_datetime_17 (b, "        effective", 0); b += 17; } else { decode_datetime_7 (b, "        effective", 0); b += 7; }
	}
#endif
}
