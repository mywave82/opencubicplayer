#ifndef _DIRDB_H
#define _DIRDB_H

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#ifdef HAVE_PWD_H
#include <pwd.h>
#endif

#define DIRDB_NOPARENT 0xffffffff /* also is the magicial node of / */
#define DIRDB_CLEAR     0xffffffff
#define DIRDB_NO_MDBREF 0xffffffff
#define DIRDB_NO_ADBREF 0xffffffff

enum dirdb_use
{
	dirdb_use_children = 0,
	dirdb_use_dir = 1,
	dirdb_use_file = 2,
	dirdb_use_filehandle = 3,
	dirdb_use_drive_resolve = 4,
	dirdb_use_pfilesel = 5,
	dirdb_use_medialib = 6,
	dirdb_use_mdb_medialib = 7,
};

extern const char dirdbsigv1[60];
extern const char dirdbsigv2[60];

extern void dirdbFlush(void); /* removes all nodes that hasn't been ref'ed yet aswell */

#define DIRDB_FULLNAME_NODRIVE  1 /* without the drive: prefix */
#define DIRDB_FULLNAME_ENDSLASH 2
extern void dirdbGetFullname_malloc(uint32_t node, char **name, int flags);

extern void dirdbGetName_internalstr(uint32_t node, const char **name); /* gives a pointer that is valid as long as you do not call dirdbUnref(). The buffer MUST NOT be sent to free() */
extern void dirdbGetName_malloc(uint32_t node, char **name); /* does not allow / in name, but \\ */

extern uint32_t dirdbRef(uint32_t node, enum dirdb_use use);
extern void dirdbUnref(uint32_t node, enum dirdb_use use);

extern uint32_t dirdbFindAndRef(uint32_t parent, const char *name, enum dirdb_use use); /* does not allow / in name, but \\ */

/* This one supports:
   .
   ..
   initial / brings the drive
   initial file: changes the drive if DIRDB_RESOLVE_DRIVE is set
 */
#define DIRDB_RESOLVE_DRIVE          4  /* if name starts with file:, it is inteprated as a drive */
#define DIRDB_RESOLVE_NODRIVE        0  /* do not support name to start with a drive */
#define DIRDB_RESOLVE_TILDE_HOME     8  /* if the first path node is ~ we replace it with getenv("HOME");  example ~/Music/foobar.s3m */
#define DIRDB_RESOLVE_TILDE_USER    16  /* if the first path node is ~user, we replace it with the system home for the given user, example ~foo/Music/test.s3m */
#define DIRDB_RESOLVE_WINDOWS_SLASH 32  /* expect \ instead of /, and translate / into \ */
extern uint32_t dirdbResolvePathWithBaseAndRef(uint32_t base, const char *name, const int flags, enum dirdb_use use);

#define DIRDB_DIFF_WINDOWS_SLASH    32  /* generate \ instead of / */
extern char *dirdbDiffPath(uint32_t base, uint32_t node, const int flags);

#if 0
extern uint32_t dirdbResolvePathAndRef(const char *name); /* does not allow / in name, but \\ */
#else
#define dirdbResolvePathAndRef(name, use) dirdbResolvePathWithBaseAndRef (DIRDB_NOPARENT, name, 0, use)
#endif

extern int dirdbInit(void);
extern void dirdbClose(void);
extern uint32_t dirdbGetParentAndRef(uint32_t node, enum dirdb_use use);

/* Temporary marks a node in the tree. Used by media-database, to detect removed nodes during a directory scan.
 *
 * Usage:
 * 1: mark a node, then ref all the childred you want to keep in the database
 * 2: scan all possible children you want to keep by calling
 *    dirdbMakeMdbAdbRef(). mdbref that are soemthing else than DIRDB_NO_MDBREF,
 *    gets an internal RefCount.
 * 3: call dirdbTagRemoveUntaggedAndSubmit(), and it flushes all nodes under the
 *    initially marked node with the new mdbref numbers, and balances internal
 *    RefCounts
 */
extern void dirdbTagSetParent(uint32_t node);
extern void dirdbTagPreserveTree(uint32_t node); /* used if we want delete only a part of a tree, but preserve the rest */
extern void dirdbMakeMdbRef(uint32_t node, uint32_t mdbref);
extern void dirdbTagCancel(void);
extern void dirdbTagRemoveUntaggedAndSubmit(void);

/* iterate the internal database of all known songs - medialib: */
extern int dirdbGetMdb(uint32_t *dirdbnode, uint32_t *mdbnode, int *first);

void utf8_XdotY_name (const int X, const int Y, char *shortname, const char *source);

#endif
