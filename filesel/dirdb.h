#ifndef _DIRDB_H
#define _DIRDB_H

#define DIRDB_NOPARENT 0xffffffff /* also is the magicial node of / */
#define DIRDB_CLEAR     0xffffffff
#define DIRDB_NO_MDBREF 0xffffffff
#define DIRDB_NO_ADBREF 0xffffffff

extern const char dirdbsigv1[60];
extern const char dirdbsigv2[60];

extern void dirdbFlush(void); /* removes all nodes that hasn't been ref'ed yet aswell */

#define DIRDB_FULLNAME_NOBASE   1 /* without the drive: prefix */
#define DIRDB_FULLNAME_ENDSLASH 2
extern void dirdbGetFullname_malloc(uint32_t node, char **name, int flags);

extern void dirdbGetName_internalstr(uint32_t node, char **name); /* gives a pointer that is valid as long as you do not call dirdbUnref(). The buffer MUST NOT be sent to free() */
extern void dirdbGetName_malloc(uint32_t node, char **name);

extern void dirdbUnref(uint32_t node);
extern void dirdbRef(uint32_t node);
extern uint32_t dirdbFindAndRef(uint32_t parent, const char *name);
extern uint32_t dirdbResolvePathWithBaseAndRef(uint32_t base, const char *name);
extern uint32_t dirdbResolvePathAndRef(const char *name);
extern int dirdbInit(void);
extern void dirdbClose(void);
extern uint32_t dirdbGetParentAndRef(uint32_t node);

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
extern void dirdbMakeMdbAdbRef(uint32_t node, uint32_t mdbref, uint32_t adbref);
extern void dirdbTagCancel(void);
extern void dirdbTagRemoveUntaggedAndSubmit(void);

/* iterate the internal database of all known songs - medialib: */
extern int dirdbGetMdbAdb(uint32_t *dirdbnode, uint32_t *mdbnode, uint32_t *adbref, int *first);

#endif
