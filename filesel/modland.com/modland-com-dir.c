struct modland_com_ocpdir_t
{
	struct ocpdir_t head;
	char *dirname; /* survives main database changes */
};

static void modland_com_ocpdir_ref (struct ocpdir_t *d)
{
	struct modland_com_ocpdir_t *self = (struct modland_com_ocpdir_t *)d;
	self->head.refcount++;
}

static void modland_com_ocpdir_unref (struct ocpdir_t *d)
{
	struct modland_com_ocpdir_t *self = (struct modland_com_ocpdir_t *)d;
	if (!--self->head.refcount)
	{
		if (self->head.parent)
		{
			self->head.parent->unref (self->head.parent);
			self->head.parent = 0;
		}
		dirdbUnref (self->head.dirdb_ref, dirdb_use_dir);
		free (self->dirname);
		free (self);
	}
}

struct modland_com_readdir_t
{
	struct modland_com_ocpdir_t *dir;
	unsigned int fileoffset;
	unsigned int diroffset;
	unsigned int dirmax; // used by flatdir
	unsigned int direxact;
	unsigned int dirnamelength; /* cache for strncmp */
	int flatdir;
	int sentdevs;
	void(*callback_file)(void *token, struct ocpfile_t *);
	void(*callback_dir )(void *token, struct ocpdir_t *);
	void *token;
};

static ocpdirhandle_pt modland_com_ocpdir_readdir_start_common (struct ocpdir_t *d,
                                                                void(*callback_file)(void *token, struct ocpfile_t *),
                                                                void(*callback_dir )(void *token, struct ocpdir_t *),
                                                                void *token,
                                                                int flatdir)
{
	struct modland_com_ocpdir_t *self = (struct modland_com_ocpdir_t *)d;
	struct modland_com_readdir_t *iter = calloc (sizeof (*iter), 1);

	if (!iter)
	{
		return 0;
	}
	iter->dir = self;
	iter->dirnamelength = strlen (self->dirname);
	iter->callback_file = callback_file;
	iter->callback_dir = callback_dir;
	iter->token = token;
	iter->flatdir = flatdir;

	/* perform binary search for the exact directory entry */
	if (modland_com.database.direntries_n)
	{
		unsigned long start = 0;
		unsigned long stop = modland_com.database.direntries_n;

		while (1)
		{
			unsigned long distance = stop - start;
			unsigned long half;

			if ((distance <= 1) ||
			    (!strcmp (modland_com.database.direntries[start], self->dirname)))
			{
				if (!strcmp (modland_com.database.direntries[start], self->dirname))
				{
					iter->direxact = start;
				} else {
					iter->direxact = UINT_MAX;
				}
				break;
			}

			half = (stop - start) / 2 + start;

			if (strcmp (modland_com.database.direntries[half], self->dirname) > 0)
			{
				stop = half;
			} else {
				start = half;
			}
		}
	} else {
		iter->direxact = UINT_MAX;
	}

	iter->diroffset = iter->direxact;
	if (iter->diroffset != UINT_MAX)
	{
		if (flatdir)
		{ /* need to find dirlimit */
			iter->dirmax = iter->diroffset + 1;
			while ((iter->dirmax < modland_com.database.direntries_n) &&
			       (!strncmp (modland_com.database.direntries[iter->dirmax], self->dirname, iter->dirnamelength)) &&
			       (modland_com.database.direntries[iter->dirmax][iter->dirnamelength]=='/'))
			{
				iter->dirmax++;
			}
		} else { /* need to skip first directory, it is "." */
			iter->diroffset++;
			if ((iter->diroffset >= modland_com.database.direntries_n) ||
			    strncmp (modland_com.database.direntries[iter->diroffset], self->dirname, iter->dirnamelength))
			{
				iter->diroffset = UINT_MAX;
			}
		}
	}

	/* perform binary search for the first file with the correct dirindex */
	if (iter->direxact != UINT_MAX)
	{
		unsigned long start = 0;
		unsigned long stop = modland_com.database.fileentries_n;

		while (1)
		{
			unsigned long distance = stop - start;
			unsigned long half = distance / 2 + start;

			if (distance <= 1)
			{
				iter->fileoffset = start;
				break;
			}

			if (modland_com.database.fileentries[half].dirindex == iter->direxact)
			{
				if (modland_com.database.fileentries[half-1].dirindex >= iter->direxact)
				{
					stop = half;
				} else {
					start = half;
				}
			} else {
				if (modland_com.database.fileentries[half].dirindex >= iter->direxact)
				{
					stop = half;
				} else {
					start = half;
				}
			}
		}
		/* if fileoffset did not find exact match, it will point to one or more directories too early */
		while ((iter->fileoffset < modland_com.database.fileentries_n) && (modland_com.database.fileentries[iter->fileoffset].dirindex < iter->direxact))
		{
			iter->fileoffset++;
		}
	} else {
		iter->fileoffset=UINT_MAX;
	}

	self->head.ref (&self->head);
	return iter;
}

static ocpdirhandle_pt modland_com_ocpdir_readdir_start (struct ocpdir_t *d,
                                                         void(*callback_file)(void *token, struct ocpfile_t *),
                                                         void(*callback_dir )(void *token, struct ocpdir_t *),
                                                         void *token)
{
	return modland_com_ocpdir_readdir_start_common (d, callback_file, callback_dir, token, 0);
}

static ocpdirhandle_pt modland_com_ocpdir_readflatdir_start (struct ocpdir_t *d,
                                                             void(*callback_file)(void *token, struct ocpfile_t *),
                                                             void *token)
{
	return modland_com_ocpdir_readdir_start_common (d, callback_file, 0, token, 1);
}

static void modland_com_ocpdir_readdir_cancel (ocpdirhandle_pt v)
{
	struct modland_com_readdir_t *iter = (struct modland_com_readdir_t *)v;
	iter->dir->head.unref (&iter->dir->head);
	free (iter);
}

static int modland_com_ocpdir_readdir_iterate (ocpdirhandle_pt v)
{
	struct modland_com_readdir_t *iter = (struct modland_com_readdir_t *)v;

	if ((!iter->sentdevs) &&
	    (!iter->dirnamelength))
	{
		iter->callback_file (iter->token, modland_com.initialize);
		iter->sentdevs = 1;
	}

	if (iter->flatdir)
	{
		int n = 0;

		while (n < 1000)
		{
			struct ocpfile_t *f;

			if ((iter->fileoffset >= modland_com.database.fileentries_n) ||
			    (modland_com.database.fileentries[iter->fileoffset].dirindex >= iter->dirmax))
			{
				iter->fileoffset = UINT_MAX;
				return 0;
			}

			f = modland_com_file_spawn (&iter->dir->head, iter->fileoffset);
			if (f)
			{
				iter->callback_file (iter->token, f);
				f->unref (f);
			}
			iter->fileoffset++;
			n++;
		}
		return 1;
	}

	if (iter->diroffset != UINT_MAX)
	{
		if ((iter->diroffset >= modland_com.database.direntries_n) ||
		    (strncmp (modland_com.database.direntries[iter->diroffset], iter->dir->dirname, iter->dirnamelength)) ||
		    (iter->dirnamelength && (modland_com.database.direntries[iter->diroffset][iter->dirnamelength] != '/')))
		{
			iter->diroffset = UINT_MAX;
		} else {
			char *ptr = strchr (modland_com.database.direntries[iter->diroffset] + iter->dirnamelength + (iter->dirnamelength?1:0), '/');
			unsigned long len;
			unsigned long next;
			if (ptr)
			{ /* this should not be reachable */
				iter->diroffset++;
				return 1;
			}
			len = strlen (modland_com.database.direntries[iter->diroffset]);
			{
				struct modland_com_ocpdir_t *d = calloc (sizeof (*d), 1);
				if (d)
				{
					iter->dir->head.ref (&iter->dir->head);
					ocpdir_t_fill (
						&d->head,
						modland_com_ocpdir_ref,
						modland_com_ocpdir_unref,
						&iter->dir->head, /* parent */
						modland_com_ocpdir_readdir_start,
						modland_com_ocpdir_readflatdir_start,
						modland_com_ocpdir_readdir_cancel,
						modland_com_ocpdir_readdir_iterate,
						0, /* readdir_dir */
						0, /* readdir_file */
						0, /* charset_override_API */
						dirdbFindAndRef (iter->dir->head.dirdb_ref, modland_com.database.direntries[iter->diroffset] + iter->dirnamelength + (iter->dirnamelength?1:0), dirdb_use_dir),
						1, /* refcount */
						0, /* is_archive */
						0, /* is_playlist */
						0  /* compression */
					);
					d->dirname = strdup (modland_com.database.direntries[iter->diroffset]);
					if (d->dirname)
					{
						iter->callback_dir (iter->token, &d->head);
					}
					modland_com_ocpdir_unref (&d->head);
				}
			}

			next = iter->diroffset;
			while (1)
			{
				next++;
				if (next >= modland_com.database.direntries_n)
				{
					break;
				}
				if ((strncmp (modland_com.database.direntries[iter->diroffset], modland_com.database.direntries[next], len)) || (modland_com.database.direntries[next][len] != '/'))
				{
					break;
				}
			}
			iter->diroffset = next;
			return 1;
		}
	}

	if (iter->fileoffset != UINT_MAX)
	{
		struct ocpfile_t *f;

		if ((iter->fileoffset >= modland_com.database.fileentries_n) ||
		    (modland_com.database.fileentries[iter->fileoffset].dirindex != iter->direxact))
		{ /* no more files */
			iter->fileoffset = UINT_MAX;
			return 1;
		}

		f = modland_com_file_spawn (&iter->dir->head, iter->fileoffset);
		if (f)
		{
			iter->callback_file (iter->token, f);
			f->unref (f);
		}

		iter->fileoffset++;
		return 1;
	}

	return 0;
}

static struct ocpdir_t *modland_com_init_root(void)
{
	struct modland_com_ocpdir_t *retval = calloc (sizeof (*retval), 1);
	if (!retval)
	{
		return 0;
	}
	ocpdir_t_fill (
		&retval->head,
		modland_com_ocpdir_ref,
		modland_com_ocpdir_unref,
		0, /* parent */
		modland_com_ocpdir_readdir_start,
		modland_com_ocpdir_readflatdir_start,
		modland_com_ocpdir_readdir_cancel,
		modland_com_ocpdir_readdir_iterate,
		0, /* readdir_dir */
		0, /* readdir_file */
		0, /* charset_override_API */
		dirdbFindAndRef (DIRDB_NOPARENT, "modland.com:", dirdb_use_dir),
		1, /* refcount */
		0, /* is_archive */
		0, /* is_playlist */
		0  /* compression */
	);
	retval->dirname = strdup ("");
	if (!retval->dirname)
	{
		modland_com_ocpdir_unref (&retval->head);
		return 0;
	}
	return &retval->head;
}
