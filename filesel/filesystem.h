#ifndef _FILESEL_FILESYSTEM_H
#define _FILESEL_FILESYSTEM_H 1

struct ocpdir_t;
struct ocpfile_t;
struct ocpfilehandle_t;
struct ocpfiledecompressor_t;
struct ocpdirdecompressor_t;

typedef void *ocpdirhandle_pt;

#define FILESIZE_STREAM  __UINT64_C(0xffffffffffffffff) /* STREAM - so we recommend to open and analyze the file asap */
#define FILESIZE_ERROR   __UINT64_C(0xfffffffffffffffe)

struct ocpdir_charset_override_API_t
{
	void (*get_default_string)(struct ocpdir_t *self, const char **label, const char **charset);
	const char *(*get_byuser_string)(struct ocpdir_t *self); /* can return NULL, gives link to internal reference */
	void (*set_byuser_string)(struct ocpdir_t *self, const char *byuser); /* duplicates the string, if NULL, default string should be used - only valid if get_default_string is non-null */
	char **(*get_test_strings)(struct ocpdir_t *self); /* zero-terminated list of strings, should be freed, together will all the nodes when done */
};


struct ocpdir_t /* can be an archive */
{
	void (*ref)(struct ocpdir_t *);
	void (*unref)(struct ocpdir_t *);

	struct ocpdir_t *parent;

	/* read directory the usual way */
	ocpdirhandle_pt (*readdir_start)(struct ocpdir_t *, void(*callback_file)(void *token, struct ocpfile_t *),
	                                                    void(*callback_dir )(void *token, struct ocpdir_t *), void *token);
	/* dumps all files. Only archives will let this be non-null */
	ocpdirhandle_pt (*readflatdir_start)(struct ocpdir_t *, void(*callback_file)(void *token, struct ocpfile_t *), void *token);
	void (*readdir_cancel)(ocpdirhandle_pt);
	int (*readdir_iterate)(ocpdirhandle_pt); /* returns non-zero if more iterations is needed - ADB needs this */
	struct ocpdir_t  *(*readdir_dir)  (struct ocpdir_t *, uint32_t dirdb_ref); /* find a specific dir */
	struct ocpfile_t *(*readdir_file) (struct ocpdir_t *, uint32_t dirdb_ref); /* find a specific file */

	const struct ocpdir_charset_override_API_t *charset_override_API;

	int dirdb_ref;
	int refcount; /* internal use by all object variants */
	uint8_t is_archive;
	uint8_t is_playlist;
};

static inline void ocpdir_t_fill (
	struct ocpdir_t *s,
	void (*ref)(struct ocpdir_t *),
	void (*unref)(struct ocpdir_t *),
	struct ocpdir_t *parent, /* ref-counting is the callers and unref handlers responsibility */
	ocpdirhandle_pt (*readdir_start)(struct ocpdir_t *, void(*callback_file)(void *token, struct ocpfile_t *),
	                                                    void(*callback_dir )(void *token, struct ocpdir_t *), void *token),
	ocpdirhandle_pt (*readflatdir_start)(struct ocpdir_t *, void(*callback_file)(void *token, struct ocpfile_t *), void *token),
	void (*readdir_cancel)(ocpdirhandle_pt),
	int (*readdir_iterate)(ocpdirhandle_pt),
	struct ocpdir_t  *(*readdir_dir)  (struct ocpdir_t *, uint32_t dirdb_ref),
	struct ocpfile_t *(*readdir_file) (struct ocpdir_t *, uint32_t dirdb_ref),
	const struct ocpdir_charset_override_API_t *charset_override_API,
	int dirdb_ref,
	int refcount,
	uint8_t is_archive,
	uint8_t is_playlist)
{
	s->ref                  = ref;
	s->unref                = unref;
	s->parent               = parent;
	s->readdir_start        = readdir_start;
	s->readflatdir_start    = readflatdir_start;
	s->readdir_cancel       = readdir_cancel;
	s->readdir_iterate      = readdir_iterate;
	s->readdir_dir          = readdir_dir;
	s->readdir_file         = readdir_file;
	s->charset_override_API = charset_override_API;
	s->dirdb_ref            = dirdb_ref;
	s->refcount             = refcount;
	s->is_archive           = is_archive;
	s->is_playlist          = is_playlist;
}

struct ocpfile_t /* can be virtual */
{
	void (*ref)(struct ocpfile_t *);
	void (*unref)(struct ocpfile_t *);
	struct ocpdir_t *parent;
	struct ocpfilehandle_t *(*open)(struct ocpfile_t *);

	uint64_t (*filesize)(struct ocpfile_t *); // can return FILESIZE_STREAM FILESIZE_ERROR
	int (*filesize_ready)(struct ocpfile_t *); // if this is false, asking for filesize might be very slow

	int dirdb_ref;
	int refcount; /* internal use by all object variants */
	uint8_t is_nodetect; /* do not use mdbReadInfo on this file please */
};

static inline void ocpfile_t_fill (
	struct ocpfile_t *s,
	void (*ref)(struct ocpfile_t *),
	void (*unref)(struct ocpfile_t *),
	struct ocpdir_t *parent, /* ref-counting is the callers and unref handlers responsibility */
	struct ocpfilehandle_t *(*open)(struct ocpfile_t *),
	uint64_t (*filesize)(struct ocpfile_t *),
	int (*filesize_ready)(struct ocpfile_t *),
	int dirdb_ref,
	int refcount,
	uint8_t is_nodetect)
{
	s->ref            = ref;
	s->unref          = unref;
	s->parent         = parent;
	s->open           = open;
	s->filesize       = filesize;
	s->filesize_ready = filesize_ready;
	s->dirdb_ref      = dirdb_ref;
	s->refcount       = refcount;
	s->is_nodetect    = is_nodetect;
}

struct ocpfilehandle_t
{
	void (*ref)(struct ocpfilehandle_t *);
	void (*unref)(struct ocpfilehandle_t *);

	int (*seek_set)(struct ocpfilehandle_t *, int64_t pos); /* returns 0 for OK, and -1 on error, should use positive numbers */
	int (*seek_cur)(struct ocpfilehandle_t *, int64_t pos); /* returns 0 for OK, and -1 on error */
	int (*seek_end)(struct ocpfilehandle_t *, int64_t pos); /* returns 0 for OK, and -1 on error, should use negative numbers */
	uint64_t (*getpos)(struct ocpfilehandle_t *);

	int (*eof)(struct ocpfilehandle_t *);   /* 0 = more data, 1 = EOF, -1 = error - probably tried to read beyond EOF */
	int (*error)(struct ocpfilehandle_t *); /* 0 or 1 */

	int (*read)(struct ocpfilehandle_t *, void *dst, int len); /* returns 0 or the number of bytes read - short reads only happens if EOF or error is hit! */

// can be FILESIZE_STREAM
	uint64_t (*filesize)(struct ocpfilehandle_t *); // can be FILESIZE_STREAM
	int (*filesize_ready)(struct ocpfilehandle_t *); // if this is false, asking for filesize might be very slow
	int dirdb_ref;
	int refcount; /* internal use by all object variants */
};

static inline void ocpfilehandle_t_fill (
	struct ocpfilehandle_t *s,
	void (*ref)(struct ocpfilehandle_t *),
	void (*unref)(struct ocpfilehandle_t *),
	int (*seek_set)(struct ocpfilehandle_t *, int64_t pos),
	int (*seek_cur)(struct ocpfilehandle_t *, int64_t pos),
	int (*seek_end)(struct ocpfilehandle_t *, int64_t pos),
	uint64_t (*getpos)(struct ocpfilehandle_t *),
	int (*eof)(struct ocpfilehandle_t *),
	int (*error)(struct ocpfilehandle_t *),
	int (*read)(struct ocpfilehandle_t *, void *dst, int len),
	uint64_t (*filesize)(struct ocpfilehandle_t *),
	int (*filesize_ready)(struct ocpfilehandle_t *),
	int dirdb_ref)
{
	s->ref            = ref;
	s->unref          = unref;
	s->seek_set       = seek_set;
	s->seek_cur       = seek_cur;
	s->seek_end       = seek_end;
	s->getpos         = getpos;
	s->eof            = eof;
	s->error          = error;
	s->read           = read;
	s->filesize       = filesize;
	s->filesize_ready = filesize_ready;
	s->dirdb_ref      = dirdb_ref;
}

int ocpfilehandle_read_uint8     (struct ocpfilehandle_t *, uint8_t *dst); /* returns 0 for OK, and -1 on error */
int ocpfilehandle_read_uint16_be (struct ocpfilehandle_t *, uint16_t *dst); /* returns 0 for OK, and -1 on error */
int ocpfilehandle_read_uint16_le (struct ocpfilehandle_t *, uint16_t *dst); /* returns 0 for OK, and -1 on error */
int ocpfilehandle_read_uint24_be (struct ocpfilehandle_t *, uint32_t *dst); /* returns 0 for OK, and -1 on error */
int ocpfilehandle_read_uint24_le (struct ocpfilehandle_t *, uint32_t *dst); /* returns 0 for OK, and -1 on error */
int ocpfilehandle_read_uint32_be (struct ocpfilehandle_t *, uint32_t *dst); /* returns 0 for OK, and -1 on error */
int ocpfilehandle_read_uint32_le (struct ocpfilehandle_t *, uint32_t *dst); /* returns 0 for OK, and -1 on error */
int ocpfilehandle_read_uint64_be (struct ocpfilehandle_t *, uint64_t *dst); /* returns 0 for OK, and -1 on error */
int ocpfilehandle_read_uint64_le (struct ocpfilehandle_t *, uint64_t *dst); /* returns 0 for OK, and -1 on error */

/* .tar .zip .. */
struct ocpdirdecompressor_t
{
	const char *name;
	const char *description;
	struct ocpdir_t *(*check)(const struct ocpdirdecompressor_t *, struct ocpfile_t *, const char *filetype); /* wraps the handle if it matches */
};

void register_dirdecompressor(const struct ocpdirdecompressor_t *);

struct ocpdir_t  *ocpdirdecompressor_check (struct ocpfile_t *, const char *filetype);

#endif
