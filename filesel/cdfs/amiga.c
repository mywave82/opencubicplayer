static void decode_amiga_AS (struct Volume_Description_t *self, uint8_t *buffer)
{
#ifdef CDFS_DEBUG
	int l = buffer[2] - 4;
	uint8_t *b = buffer + 4;
	uint8_t flags;

	debug_printf ("       Amiga\n");

	if (buffer[2] < 5)
	{
		debug_printf ("WARNING - Length is way too short\n");
		return;
	}
	if (buffer[3] != 1)
	{
		debug_printf ("WARNING - Version is wrong\n");
		return;
	}
	debug_printf ("        Flags: 0x%02" PRIx8 "\n", buffer[4]);
	if (buffer[4] & 0x01) debug_printf ("         Protection present\n");
	if (buffer[4] & 0x02) debug_printf ("         Comment present\n");
	if (buffer[4] & 0x04) debug_printf ("         Comment continues in next AS record\n");

	flags = buffer[4];

	if (flags & 0x01)
	{
		if (l < 4)
		{
			debug_printf ("WARNING - Length is way too short #2\n");
			return;
		}
		debug_printf ("        Protection User:       0x%02" PRIx8 "\n", b[0]);
		debug_printf ("        Protection Reserved:   0x%02" PRIx8 "\n", b[1]);
		debug_printf ("        Protection Multiuser:  0x%02" PRIx8 "\n", b[2]);
		if (b[2] & 0x01) debug_printf ("         Deletable for group members\n");
		if (b[2] & 0x02) debug_printf ("         Executable for group members\n");
		if (b[2] & 0x04) debug_printf ("         Writable for group members\n");
		if (b[2] & 0x08) debug_printf ("         Readable for group members\n");
		if (b[2] & 0x10) debug_printf ("         Deletable for other users\n");
		if (b[2] & 0x20) debug_printf ("         Executable for other users\n");
		if (b[2] & 0x40) debug_printf ("         Writable for other users\n");
		if (b[2] & 0x80) debug_printf ("         Readable for other users\n");
		debug_printf ("        Protection Protection: 0x%02" PRIx8 "\n", b[3]);
		if (b[3] & 0x01) debug_printf ("         Not deletable for owner\n");
		if (b[3] & 0x02) debug_printf ("         Not executable for owner\n");
		if (b[3] & 0x04) debug_printf ("         Not writable for owner\n");
		if (b[3] & 0x08) debug_printf ("         Not readable for owner\n");
		if (b[3] & 0x10) debug_printf ("         Archived\n");
		if (b[2] & 0x20) debug_printf ("         Reentrant executable\n");
		if (b[3] & 0x40) debug_printf ("         Executable script\n");

		b+=4;
		l-=4;
	}

	if (flags & 0x02)
	{
		int i;
		if ((l < 1) || (l < b[0]))
		{
			debug_printf ("WARNING - Length is way too short #3\n");
			return;
		}
		debug_printf ("        Comment: \"");
		for (i=1; i < l; i++)
		{
			debug_printf ("%c", b[i]);
		}
		debug_printf ("\"\n");
		b += i;
		l -= i;
	}
#endif
}
