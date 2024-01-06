#ifndef _FILESEL_FILESYSTEM_H
#define _FILESEL_FILESYSTEM_H 1

struct ocpdir_t;
struct ocpfile_t;
struct ocpfilehandle_t;
struct ocpfiledecompressor_t;
struct ocpdirdecompressor_t;

typedef void *ocpdirhandle_pt;

#include "filesystem-filehandle-cache.h"

#define FILESIZE_STREAM  UINT64_C(0xffffffffffffffff) /* STREAM - so we recommend to open and analyze the file asap */
#define FILESIZE_ERROR   UINT64_C(0xfffffffffffffffe)

#define COMPRESSION_NONE   0 /* available directly on the file-system */
#define COMPRESSION_STORE  1 /* .tar, .zip(STORE)                     */
#define COMPRESSION_STREAM 2 /* .gz .bzip2 .zip                       */
#define COMPRESSION_SOLID  3 /* solid .tar.gz .tar.bz2                */
#define COMPRESSION_SOLID1 4 /* .gz.tar.gz ....                       */
#define COMPRESSION_SOLID2 5 /* ... */
#define COMPRESSION_SOLID3 6 /* ... */
#define COMPRESSION_SOLID4 7 /* ... */

static inline uint8_t COMPRESSION_ADD_STORE (uint8_t parent)
{
	uint8_t retval = (parent >= COMPRESSION_STREAM) ? parent+1 : parent | 1;
	if (retval > COMPRESSION_SOLID4)
	{
		retval = COMPRESSION_SOLID4;
	}
	return retval;
}
static inline uint8_t COMPRESSION_ADD_STREAM (uint8_t parent)
{
	uint8_t retval =  parent + 2;
	if (retval > COMPRESSION_SOLID4)
	{
		retval = COMPRESSION_SOLID4;
	}
	return retval;
}
static inline uint8_t COMPRESSION_ADD_SOLID (uint8_t parent)
{
	uint8_t retval =  parent + 3;
	if (retval > COMPRESSION_SOLID4)
	{
		retval = COMPRESSION_SOLID4;
	}
	return retval;
}

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
	uint8_t compression;
};

struct ocpdir_t  *ocpdir_t_fill_default_readdir_dir  (struct ocpdir_t *, uint32_t dirdb_ref);
struct ocpfile_t *ocpdir_t_fill_default_readdir_file (struct ocpdir_t *, uint32_t dirdb_ref);

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
	uint8_t is_playlist,
	uint8_t compression)
{
	s->ref                  = ref;
	s->unref                = unref;
	s->parent               = parent;
	s->readdir_start        = readdir_start;
	s->readflatdir_start    = readflatdir_start;
	s->readdir_cancel       = readdir_cancel;
	s->readdir_iterate      = readdir_iterate;
	s->readdir_dir          = readdir_dir  ? readdir_dir  : ocpdir_t_fill_default_readdir_dir;
	s->readdir_file         = readdir_file ? readdir_file : ocpdir_t_fill_default_readdir_file;
	s->charset_override_API = charset_override_API;
	s->dirdb_ref            = dirdb_ref;
	s->refcount             = refcount;
	s->is_archive           = is_archive;
	s->is_playlist          = is_playlist;
	s->compression          = compression;
}

#ifdef _DIRDB_H
static inline struct ocpdir_t *ocpdir_readdir_dir (struct ocpdir_t *s, const char *name, const struct dirdbAPI_t *dirdb)
{
	uint32_t dirdb_ref;
	struct ocpdir_t *retval = 0;

	if (!s) return 0;

	dirdb_ref = dirdb->FindAndRef (s->dirdb_ref, name, dirdb_use_file);
	if (dirdb_ref == DIRDB_CLEAR) return 0;

	retval = s->readdir_dir (s, dirdb_ref);

	dirdb->Unref (dirdb_ref, dirdb_use_file);

	return retval;
}

static inline struct ocpfile_t *ocpdir_readdir_file (struct ocpdir_t *s, const char *name, const struct dirdbAPI_t *dirdb)
{
	uint32_t dirdb_ref;
	struct ocpfile_t *retval = 0;

	if (!s) return 0;

	dirdb_ref = dirdb->FindAndRef (s->dirdb_ref, name, dirdb_use_file);
	if (dirdb_ref == DIRDB_CLEAR) return 0;

	retval = s->readdir_file (s, dirdb_ref);

	dirdb->Unref (dirdb_ref, dirdb_use_file);

	return retval;
}
#endif

struct ocpfile_t /* can be virtual */
{
	void (*ref)(struct ocpfile_t *);
	void (*unref)(struct ocpfile_t *);
	struct ocpdir_t *parent;
	struct ocpfilehandle_t *(*open)(struct ocpfile_t *);
#ifndef FILEHANDLE_CACHE_DISABLE
	struct ocpfilehandle_t *(*real_open)(struct ocpfile_t *);
#endif

	uint64_t (*filesize)(struct ocpfile_t *); // can return FILESIZE_STREAM FILESIZE_ERROR
	int (*filesize_ready)(struct ocpfile_t *); // if this is false, asking for filesize might be very slow
	const char *(*filename_override) (struct ocpfile_t *); // returns NULL if we do not override the dirdb name

	int dirdb_ref;
	int refcount; /* internal use by all object variants */
	uint8_t is_nodetect; /* do not use mdbReadInfo on this file please */
	uint8_t compression;
};

const char *ocpfile_t_fill_default_filename_override (struct ocpfile_t *);

#ifndef FILEHANDLE_CACHE_DISABLE
static struct ocpfilehandle_t *ocpfilehandle_cache_open_wrap (struct ocpfile_t *f);
#endif

static inline void ocpfile_t_fill (
	struct ocpfile_t *s,
	void (*ref)(struct ocpfile_t *),
	void (*unref)(struct ocpfile_t *),
	struct ocpdir_t *parent, /* ref-counting is the callers and unref handlers responsibility */
	struct ocpfilehandle_t *(*open)(struct ocpfile_t *),
	uint64_t (*filesize)(struct ocpfile_t *),
	int (*filesize_ready)(struct ocpfile_t *),
	const char *(*filename_override) (struct ocpfile_t *),
	int dirdb_ref,
	int refcount,
	uint8_t is_nodetect,
	uint8_t compression)
{
	s->ref            = ref;
	s->unref          = unref;
	s->parent         = parent;
#ifndef FILEHANDLE_CACHE_DISABLE
	s->open           = ocpfilehandle_cache_open_wrap;
	s->real_open      = open;
#else
	s->open           = open;
#endif
	s->filesize       = filesize;
	s->filesize_ready = filesize_ready;
	s->filename_override = filename_override ? filename_override : ocpfile_t_fill_default_filename_override;
	s->dirdb_ref      = dirdb_ref;
	s->refcount       = refcount;
	s->is_nodetect    = is_nodetect;
	s->compression    = compression;
}

struct ocpfilehandle_t
{
	void (*ref)(struct ocpfilehandle_t *);
	void (*unref)(struct ocpfilehandle_t *);

	struct ocpfile_t *origin;

	int (*seek_set)(struct ocpfilehandle_t *, int64_t pos); /* returns 0 for OK, and -1 on error, should use positive numbers */
	uint64_t (*getpos)(struct ocpfilehandle_t *);

	int (*eof)(struct ocpfilehandle_t *);   /* 0 = more data, 1 = EOF, -1 = error - probably tried to read beyond EOF */
	int (*error)(struct ocpfilehandle_t *); /* 0 or 1 */

	int (*read)(struct ocpfilehandle_t *, void *dst, int len); /* returns 0 or the number of bytes read - short reads only happens if EOF or error is hit! */

	int (*ioctl)(struct ocpfilehandle_t *, const char *cmd, void *ptr);

// can be FILESIZE_STREAM
	uint64_t (*filesize)(struct ocpfilehandle_t *); // can be FILESIZE_STREAM
	int (*filesize_ready)(struct ocpfilehandle_t *); // if this is false, asking for filesize might be very slow
	const char *(*filename_override) (struct ocpfilehandle_t *);
	int dirdb_ref;
	int refcount; /* internal use by all object variants */
};

#ifndef FILEHANDLE_CACHE_DISABLE
static struct ocpfilehandle_t *ocpfilehandle_cache_open_wrap (struct ocpfile_t *f)
{
	struct ocpfilehandle_t *retval1, *retval2;
	retval1 = f->real_open (f);
	if (!retval1)
	{
		return 0;
	}
	retval2 = cache_filehandle_open (retval1);
	if (!retval2)
	{
		return retval1;
	}
	retval1->unref (retval1);
	return retval2;
}
#endif

int ocpfilehandle_t_fill_default_ioctl (struct ocpfilehandle_t *, const char *cmd, void *ptr);
const char *ocpfilehandle_t_fill_default_filename_override (struct ocpfilehandle_t *);

static inline void ocpfilehandle_t_fill (
	struct ocpfilehandle_t *s,
	void (*ref)(struct ocpfilehandle_t *),
	void (*unref)(struct ocpfilehandle_t *),
	struct ocpfile_t *origin,
	int (*seek_set)(struct ocpfilehandle_t *, int64_t pos),
	uint64_t (*getpos)(struct ocpfilehandle_t *),
	int (*eof)(struct ocpfilehandle_t *),
	int (*error)(struct ocpfilehandle_t *),
	int (*read)(struct ocpfilehandle_t *, void *dst, int len),
	int (*_ioctl)(struct ocpfilehandle_t *, const char *cmd, void *ptr),
	uint64_t (*filesize)(struct ocpfilehandle_t *),
	int (*filesize_ready)(struct ocpfilehandle_t *),
	const char *(*filename_override) (struct ocpfilehandle_t *),
	int dirdb_ref)
{
	s->ref               = ref;
	s->unref             = unref;
	s->origin            = origin;
	s->seek_set          = seek_set;
	s->getpos            = getpos;
	s->eof               = eof;
	s->error             = error;
	s->read              = read;
	s->ioctl             = _ioctl ? _ioctl : ocpfilehandle_t_fill_default_ioctl;
	s->filesize          = filesize;
	s->filesize_ready    = filesize_ready;
	s->filename_override = filename_override ? filename_override : ocpfilehandle_t_fill_default_filename_override;
	s->dirdb_ref         = dirdb_ref;
}

static inline int ocpfilehandle_read_uint8     (struct ocpfilehandle_t *s, uint8_t *dst) /* returns 0 for OK, and -1 on error */
{
	if (s->read (s, dst, 1) != 1) return -1;
	return 0;
}
static inline int ocpfilehandle_read_uint16_be (struct ocpfilehandle_t *s, uint16_t *dst) /* returns 0 for OK, and -1 on error */
{
	if (s->read (s, dst, 2) != 2) return -1;
	*dst = uint16_big (*dst);
	return 0;
}
static inline int ocpfilehandle_read_uint16_le (struct ocpfilehandle_t *s, uint16_t *dst) /* returns 0 for OK, and -1 on error */
{
	if (s->read (s, dst, 2) != 2) return -1;
	*dst = uint16_little (*dst);
	return 0;
}
static inline int ocpfilehandle_read_uint24_be (struct ocpfilehandle_t *s, uint32_t *dst) /* returns 0 for OK, and -1 on error */
{
	uint8_t t[3];
	if (s->read (s, t, 3) != 3) return -1;
	*dst = (t[0] << 16) | (t[1]<<8) | t[2];
	return 0;
}
static inline int ocpfilehandle_read_uint24_le (struct ocpfilehandle_t *s, uint32_t *dst) /* returns 0 for OK, and -1 on error */
{
	uint8_t t[3];
	if (s->read (s, t, 3) != 3) return -1;
	*dst = (t[2] << 16) | (t[1]<<8) | t[0];
	return 0;
}
static inline int ocpfilehandle_read_uint32_be (struct ocpfilehandle_t *s, uint32_t *dst) /* returns 0 for OK, and -1 on error */
{
	if (s->read (s, dst, 4) != 4) return -1;
	*dst = uint32_big (*dst);
	return 0;
}
static inline int ocpfilehandle_read_uint32_le (struct ocpfilehandle_t *s, uint32_t *dst) /* returns 0 for OK, and -1 on error */
{
	if (s->read (s, dst, 4) != 4) return -1;
	*dst = uint32_little (*dst);
	return 0;
}
static inline int ocpfilehandle_read_uint64_be (struct ocpfilehandle_t *s, uint64_t *dst) /* returns 0 for OK, and -1 on error */
{
	if (s->read (s, dst, 8) != 8) return -1;
	*dst = uint64_big (*dst);
	return 0;
}
static inline int ocpfilehandle_read_uint64_le (struct ocpfilehandle_t *s, uint64_t *dst) /* returns 0 for OK, and -1 on error */
{
	if (s->read (s, dst, 8) != 8) return -1;
	*dst = uint64_little (*dst);
	return 0;
}

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
