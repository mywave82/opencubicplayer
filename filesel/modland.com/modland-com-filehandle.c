struct modland_com_ocpfilehandle_t
{
	struct ocpfilehandle_t head;
	uint32_t filesize;
	uint32_t filepos;
	int error;
	int eof;
	osfile *handle;
};

static void modland_com_ocpfilehandle_ref (struct ocpfilehandle_t *_h)
{
	struct modland_com_ocpfilehandle_t *h = (struct modland_com_ocpfilehandle_t *)_h;
	h->head.refcount++;
}

static void modland_com_ocpfilehandle_unref (struct ocpfilehandle_t *_h)
{
	struct modland_com_ocpfilehandle_t *h = (struct modland_com_ocpfilehandle_t *)_h;
	if (!--h->head.refcount)
	{
		dirdbUnref (h->head.dirdb_ref, dirdb_use_filehandle);

		if (h->head.origin)
		{
			h->head.origin->unref (h->head.origin);
			h->head.origin = 0;
		}

		if (h->handle)
		{
			osfile_close (h->handle);
		}
		free (h);
	}
}

static int modland_com_ocpfilehandle_seek_set (struct ocpfilehandle_t *_h, int64_t pos)
{
	struct modland_com_ocpfilehandle_t *h = (struct modland_com_ocpfilehandle_t *)_h;
	h->error = 0;
	osfile_setpos (h->handle, pos);
	h->error = (osfile_getpos (h->handle) == pos) ? 0 : 1;
	h->eof = h->filepos >= h->filesize;
	return h->error ? -1 : 0;
}

static uint64_t modland_com_ocpfilehandle_getpos (struct ocpfilehandle_t *_h)
{
	struct modland_com_ocpfilehandle_t *h = (struct modland_com_ocpfilehandle_t *)_h;
	// return osfile_getpos (h->handle);
	return h->filepos;
}

static uint64_t modland_com_ocpfilehandle_filesize (struct ocpfilehandle_t *_h)
{
	struct modland_com_ocpfilehandle_t *h = (struct modland_com_ocpfilehandle_t *)_h;
	return h->filesize;
}

static int modland_com_ocpfilehandle_filesize_ready (struct ocpfilehandle_t *_h)
{
	return 1;
}

static int modland_com_ocpfilehandle_read (struct ocpfilehandle_t *_h, void *dst, int len)
{
	struct modland_com_ocpfilehandle_t *h = (struct modland_com_ocpfilehandle_t *)_h;
	int got;
	int retval = 0;

	if (h->error)
	{
		return 0;
	}
	if (h->filepos >= h->filesize)
	{
		return 0;
	}
	if ((uint64_t)h->filepos + len > h->filesize)
	{
		len = h->filesize - h->filepos;
	}
	while (len)
	{
		got = osfile_read (h->handle, dst, len);
		if (!got)
		{
			h->eof = 1;
			break;
		}
		h->filepos += got;
		len -= got;
		retval += got;
	}
	return retval;
}

static int modland_com_ocpfilehandle_error (struct ocpfilehandle_t *_h)
{
	struct modland_com_ocpfilehandle_t *h = (struct modland_com_ocpfilehandle_t *)_h;

	return h->error;
}

static int modland_com_ocpfilehandle_eof (struct ocpfilehandle_t *_h)
{
	struct modland_com_ocpfilehandle_t *h = (struct modland_com_ocpfilehandle_t *)_h;

	return h->eof || (h->filepos >= h->filesize);
}

static struct osfile_t *modland_com_ocpfile_tryopen (const char *pathname, long filesize)
{
	struct osfile_t *retval = osfile_open_readonly (pathname, 0);

	if (!retval)
	{
		return NULL;
	}
	if (osfile_getfilesize (retval) != filesize)
	{
		osfile_close (retval);
		return NULL;
	}
	return retval;
}
