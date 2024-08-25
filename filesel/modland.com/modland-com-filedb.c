#define MCDBDATAHOMEDIR configAPI->DataHomePath

static struct osfile_t *modland_com_filedb_File = 0;

struct __attribute__((packed)) modland_com_filedb_header_t
{
	char sig[60];
	uint8_t year_msb;
	uint8_t year_lsb;
	uint8_t month;
	uint8_t day;
};
static const char dbsig[60] = "Cubic Player modland.com Cache Data Base\x1B\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";

static void modland_com_filedb_save (void)
{
	struct modland_com_filedb_header_t header;
	unsigned int d, f;

	if (!modland_com_filedb_File)
	{
		return;
	}
	osfile_setpos (modland_com_filedb_File, 0);
	memcpy (header.sig, dbsig, 60);
	header.year_msb = modland_com.database.year >> 8;
	header.year_lsb = modland_com.database.year;
	header.month    = modland_com.database.month;
	header.day      = modland_com.database.day;
	if (osfile_write (modland_com_filedb_File, &header, sizeof(header)) < 0)
	{
		return;
	}

	for (d=0, f=0; (d<modland_com.database.direntries_n) && (f<modland_com.database.fileentries_n); d++)
	{
		unsigned int f2;
		uint8_t b3[3];
		for (f2=f; (f2<modland_com.database.fileentries_n) && (modland_com.database.fileentries[f2].dirindex == d); f2++)
		{ /* please just count */
		}
		if (f2 == f)
		{
			continue;
		}
		b3[0] = (f2-f) >> 8;
		b3[1] = (f2-f);
		b3[2] = strlen (modland_com.database.direntries[d]);
		if ((osfile_write (modland_com_filedb_File, b3, 3) < 0) ||
		    (osfile_write (modland_com_filedb_File, modland_com.database.direntries[d], b3[2]) < 0))
		{
			return;
		}
		for (f2=f; (f2<modland_com.database.fileentries_n) && (modland_com.database.fileentries[f2].dirindex == d); f2++)
		{
			uint8_t b4[4];
			uint8_t b1[1];
			b4[0] = modland_com.database.fileentries[f2].size >> 24;
			b4[1] = modland_com.database.fileentries[f2].size >> 16;
			b4[2] = modland_com.database.fileentries[f2].size >> 8;
			b4[3] = modland_com.database.fileentries[f2].size;
			b1[0] = strlen(modland_com.database.fileentries[f2].name);
			if ((osfile_write (modland_com_filedb_File, b4, 4) < 0) ||
			    (osfile_write (modland_com_filedb_File, b1, 1) < 0) ||
			    (osfile_write (modland_com_filedb_File, modland_com.database.fileentries[f2].name, b1[0]) < 0))
			{
				return;
			}
		}
		f=f2;
	}

	{
		uint8_t b2[2];
		b2[0] = 0;
		b2[1] = 0;
		if (osfile_write (modland_com_filedb_File, b2, 2) < 0)
		{
			return;
		}
	}

	osfile_truncate_at (modland_com_filedb_File, osfile_getpos (modland_com_filedb_File));
}

static void modland_com_filedb_close (void)
{
	if (modland_com_filedb_File)
	{
		osfile_close (modland_com_filedb_File);
		modland_com_filedb_File = 0;
	}
}

static int modland_com_filedb_load (const struct configAPI_t *configAPI)
{
	struct modland_com_filedb_header_t header;
	char *path;
	size_t len;
	struct modland_com_initialize_t s = {0};
	int ok = 0;

	if (modland_com_filedb_File)
	{
		fprintf (stderr, "modland_com_filedb_load: Already loaded\n");
		return 1;
	}

	len = strlen (MCDBDATAHOMEDIR) + strlen ("CPMDLAND.DAT") + 1;
	path = malloc (len);
	if (!path)
	{
		fprintf (stderr, "modland_com_filedb_load: malloc() failed\n");
		return 0;
	}
	snprintf (path, len, "%sCPMDLAND.DAT", MCDBDATAHOMEDIR);
	fprintf (stderr, "Loading %s .. ", path);

	modland_com_filedb_File = osfile_open_readwrite (path, 1, 0);
	free (path);
	path = 0;
	if (!modland_com_filedb_File)
	{
		fprintf (stderr, "Unable to open file\n");
		return 0;
	}

	if ((len=osfile_read(modland_com_filedb_File, &header, sizeof(header))) != sizeof(header))
	{
		fprintf (stderr, "No header\n");
		return 0;
	}

	if (memcmp(header.sig, dbsig, sizeof(dbsig)))
	{
		fprintf (stderr, "Invalid header\n");
		return 0;
	}

	modland_com.database.year = (header.year_msb << 8) | header.year_lsb;
	modland_com.database.month = header.month;
	modland_com.database.day = header.day;

	while (1)
	{
		uint8_t b2[2];
		uint8_t b4[4];
		uint8_t d[255+1+255+1];
		uint16_t num;
		int i;
		if (osfile_read (modland_com_filedb_File, b2, 2) != 2)
		{
			break;
		}
		num = (b2[0] << 8) | b2[1];
		if (!num)
		{
			ok = 1;
			break;
		}

		/* read dir stored as a pascal string */
		if (osfile_read (modland_com_filedb_File, b2, 1) != 1)
		{
			break;
		}
		if (osfile_read (modland_com_filedb_File, d, b2[0]) != b2[0])
		{
			break;
		}
		d[b2[0]] = '/';

		for (i=0; i<num; i++)
		{
			/* read file-size */
			if (osfile_read (modland_com_filedb_File, b4, 4) != 4)
			{
				break;
			}
			/* read file stored as a pascal string */
			if (osfile_read (modland_com_filedb_File, b2+1, 1) != 1)
			{
				break;
			}
			if (osfile_read (modland_com_filedb_File, d+b2[0]+1, b2[1]) != b2[1])
			{
				break;
			}
			d[b2[0]+1+b2[1]] = 0;
			modland_com_add_data_line (&s, (char *)d, (((uint32_t)(b4[0]))<<24) |
			                                          (((uint32_t)(b4[1]))<<16) |
			                                          (((uint32_t)(b4[2]))<< 8) |
			                                                       b4[3]);
		}
	}

	if (!ok)
	{
		fprintf (stderr, "(database truncated) ");
	}

	fprintf (stderr, "Done\n");
	return 1;
}
