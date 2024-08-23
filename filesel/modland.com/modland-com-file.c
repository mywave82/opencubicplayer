struct modland_com_ocpfile_t
{
	struct ocpfile_t head;
	char *filename; /* survives main database changes */
	uint32_t filesize;
};

static void modland_com_ocpfile_ref (struct ocpfile_t *_f)
{
	struct modland_com_ocpfile_t *f = (struct modland_com_ocpfile_t *)_f;
	f->head.refcount++;
}

static void modland_com_ocpfile_unref (struct ocpfile_t *_f)
{
	struct modland_com_ocpfile_t *f = (struct modland_com_ocpfile_t *)_f;
	if (!--f->head.refcount)
	{
		if (f->head.parent)
		{
			f->head.parent->unref (f->head.parent);
			f->head.parent = 0;
		}
		dirdbUnref (f->head.dirdb_ref, dirdb_use_file);
		free (f->filename);
		free (f);
	}
}

static int modland_com_ocpfile_mkdir_except_file (const char *path)
{
	char *temp = strdup (path);
	char *next;

	if (!temp)
	{
		return -1;
	}

#ifdef _WIN32
	next = strchr (temp + 3, '\\');
	while (next) /* guards about first iteration missing the slash */
	{
		struct st;
		DWORD D;
		if (!next[1])
		{ /* should not be reachable */
			break;
		}
		next = strchr (next + 1, '\\');
		if (!next)
		{ /* last item is a file, do not create it */
			break;
		}
		*next = 0;
		D = GetFileAttributes (temp);
		if (D == INVALID_FILE_ATTRIBUTES)
		{
			DWORD e = GetLastError();
			if ((e != ERROR_PATH_NOT_FOUND) && (e != ERROR_FILE_NOT_FOUND))
			{
				char *lpMsgBuf = NULL;
				if (FormatMessage (
					FORMAT_MESSAGE_ALLOCATE_BUFFER |
					FORMAT_MESSAGE_FROM_SYSTEM     |
					FORMAT_MESSAGE_IGNORE_INSERTS,             /* dwFlags */
					NULL,                                      /* lpSource */
					e,                                         /* dwMessageId */
					MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), /* dwLanguageId */
					(LPSTR) &lpMsgBuf,                         /* lpBuffer */
					0,                                         /* nSize */
					NULL                                       /* Arguments */
				))
				{
					fprintf(stderr, "GetFileAttributes(%s): %s", temp, lpMsgBuf);
					LocalFree (lpMsgBuf);
				}
				free (temp);
				return -1;
			}
			if (!CreateDirectory (temp, 0))
			{
				char *lpMsgBuf = NULL;
				if (FormatMessage (
					FORMAT_MESSAGE_ALLOCATE_BUFFER |
					FORMAT_MESSAGE_FROM_SYSTEM     |
					FORMAT_MESSAGE_IGNORE_INSERTS,             /* dwFlags */
					NULL,                                      /* lpSource */
					GetLastError(),                            /* dwMessageId */
					MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), /* dwLanguageId */
					(LPSTR) &lpMsgBuf,                         /* lpBuffer */
					0,                                         /* nSize */
					NULL                                       /* Arguments */
				))
				{
					fprintf(stderr, "CreateDirectory(%s): %s", temp, lpMsgBuf);
					LocalFree (lpMsgBuf);
				}
				free (temp);
				return -1;
			}
		} else {
			if (!(D & FILE_ATTRIBUTE_DIRECTORY))
			{
				fprintf (stderr, "GetFileAttributes(%s) & FILE_ATTRIBUTE_DIRECTORY failed\n", temp);
				free (temp);
				return -1;
			}
		}
		*next = '\\';
	}
#else
	next = strchr (temp + 1, '/');
	while (next) /* guards if initial search fails */
	{
		struct stat st;
		if (!next[1])
		{ /* should not be reachable */
			break;
		}
		next = strchr (next + 1, '/');
		if (!next)
		{ /* last item is a file, do not create it */
			break;
		}
		*next = 0;

		if (stat(temp, &st))
		{
			if (errno != ENOENT)
			{
				fprintf (stderr, "stat(%s): %s\n", temp, strerror(errno));
				free (temp);
				return -1;
			}
			if (mkdir (temp, S_IXUSR | S_IWUSR | S_IRUSR | S_IXGRP | S_IWGRP | S_IRGRP | S_IXOTH | S_IROTH))
			{
				fprintf (stderr, "mkdir(%s): %s\n", temp, strerror(errno));
				free (temp);
				return -1;
			}
		} else {
			if (!S_ISDIR(st.st_mode))
			{
				fprintf (stderr, "stat(%s) => S_ISDIR failed\n", temp);
				free (temp);
				return -1;
			}
		}
		*next = '/';
	}
#endif


	free (temp);
	return 0;
}

static int curl_download_magic (const char *targetfilename, const char *sourcepath)
{
	char *url;
	char *escaped;
	struct download_request_t *request;
	struct ocpfilehandle_t *temp_filehandle;
	struct osfile_t *target;
	char buffer[65536];
	int fill;
	size_t len;

	escaped = urlencode (sourcepath);
	if (!escaped)
	{
		return -1;
	}
	len = strlen (modland_com.mirror ? modland_com.mirror : "") + 12 + strlen (escaped) + 1;
	url = malloc (len);
	if (!url)
	{
		free (escaped);
		return -1;
	}
	snprintf (url, len, "%spub/modules/%s", modland_com.mirror ? modland_com.mirror : "", escaped);
	free (escaped);
	escaped = 0;

	request = download_request_spawn (&configAPI, 0, url);
	free (url);
	if (!request)
	{
		return -1;
	}
	while (download_request_iterate (request))
	{
		usleep (10000);
	}
	if (request->errmsg)
	{
		fprintf (stderr, "download failed: %s\n", request->errmsg);
		download_request_free (request);
		return -1;
	}
	temp_filehandle = download_request_getfilehandle (request);
	download_request_free (request);
	request = 0;
	if (!temp_filehandle)
	{
		fprintf (stderr, "open download failed #2\n");
		return -1;
	}
	target = osfile_open_readwrite (targetfilename, 0, 0);
	if (!target)
	{
		fprintf (stderr, "open target failed\n");
		temp_filehandle->unref (temp_filehandle);
		return -1;
	}
	while ((fill = temp_filehandle->read (temp_filehandle, buffer, sizeof (buffer))))
	{
		osfile_write (target, buffer, fill);
	}
	osfile_close (target);
	target = 0;

	temp_filehandle->unref (temp_filehandle);

	return 0;
}

static struct ocpfilehandle_t *modland_com_ocpfile_open (struct ocpfile_t *_f)
{
	struct modland_com_ocpfile_t *f = (struct modland_com_ocpfile_t *)_f;
	struct modland_com_ocpfilehandle_t *h;
	char *cachefilename;

	cachefilename = malloc (strlen (modland_com.cachepath) + 3 + 1 + 7 + 1 + strlen (f->filename) + 1);
	if (!cachefilename)
	{
		return 0;
	}
	sprintf (cachefilename, "%spub/modules/%s", modland_com.cachepath, f->filename);

#ifdef _WIN32
	{
		char *tmp;
		while ((tmp = strchr (cachefilename + strlen (modland_com.cachepath), '/')))
		{
			*tmp = '\\';
		}
	}
#else
	{
		char *tmp;
		while ((tmp = strchr (cachefilename + strlen (modland_com.cachepath), '\\')))
		{
			*tmp = '/';
		}
	}
#endif

	if (modland_com_ocpfile_mkdir_except_file (cachefilename))
	{
		return 0;
	}

	h = calloc (sizeof (*h), 1);
	if (!h)
	{
		free (cachefilename);
		return 0;
	}

	h->handle = modland_com_ocpfile_tryopen (cachefilename, f->filesize);
	if (!h->handle)
	{
		if (curl_download_magic (cachefilename, f->filename))
		{
			free (h);
			free (cachefilename);
			return 0;
		}
		h->handle = modland_com_ocpfile_tryopen (cachefilename, f->filesize);
		if (!h->handle)
		{
			free (h);
			free (cachefilename);
			return 0;
		}
	}
	free (cachefilename);

	_f->ref (_f);
	dirdbRef (_f->dirdb_ref, dirdb_use_filehandle);
	ocpfilehandle_t_fill
	(
		&h->head,
		modland_com_ocpfilehandle_ref,
		modland_com_ocpfilehandle_unref,
		_f,
		modland_com_ocpfilehandle_seek_set,
		modland_com_ocpfilehandle_getpos,
		modland_com_ocpfilehandle_eof,
		modland_com_ocpfilehandle_error,
		modland_com_ocpfilehandle_read,
		0, /* ioctl */
		modland_com_ocpfilehandle_filesize,
		modland_com_ocpfilehandle_filesize_ready,
		0, /* filename_override */
		_f->dirdb_ref,
		1
	);
	h->filesize = f->filesize;
	return &h->head;
}

static uint64_t modland_com_ocpfile_filesize (struct ocpfile_t *_f)
{
	struct modland_com_ocpfile_t *f = (struct modland_com_ocpfile_t *)_f;
	return f->filesize;
}

static int modland_com_ocpfile_filesize_ready (struct ocpfile_t *_f)
{
	return 1;
}


static struct ocpfile_t *modland_com_file_spawn (struct ocpdir_t *parent, unsigned int fileindex)
{
	struct modland_com_ocpfile_t *retval;
	char *filename;

	filename = malloc (strlen (modland_com.database.direntries[modland_com.database.fileentries[fileindex].dirindex]) + 1 + strlen (modland_com.database.fileentries[fileindex].name) + 1);
	if (!filename)
	{
		return 0;
	}
	sprintf (filename, "%s%s%s", modland_com.database.direntries[modland_com.database.fileentries[fileindex].dirindex], modland_com.database.fileentries[fileindex].dirindex ? "/":"", modland_com.database.fileentries[fileindex].name);

	retval = calloc (sizeof (*retval), 1);
	if (!retval)
	{
		free (filename);
		return 0;
	}

	if (parent)
	{
		parent->ref (parent);
	}

	ocpfile_t_fill (
		&retval->head,
		modland_com_ocpfile_ref,
		modland_com_ocpfile_unref,
		parent,
		modland_com_ocpfile_open,
		modland_com_ocpfile_filesize,
		modland_com_ocpfile_filesize_ready,
		0, /* filename_override */
		dirdbFindAndRef (parent ? parent->dirdb_ref : DIRDB_NOPARENT, modland_com.database.fileentries[fileindex].name, dirdb_use_file),
		1, /* refcount */
		0, /* is_nodetect */
		COMPRESSION_REMOTE
	);
	retval->filename = filename;
	retval->filesize = modland_com.database.fileentries[fileindex].size;
	return &retval->head;
}
