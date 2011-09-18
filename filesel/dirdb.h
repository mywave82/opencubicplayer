#ifndef _DIRDB_H
#define _DIRDB_H

#define DIRDB_NOPARENT 0xffffffff
#define DIRDB_NO_MDBREF 0xffffffff
#define DIRDB_NO_ADBREF 0xffffffff

extern const char dirdbsigv1[60];
extern const char dirdbsigv2[60];

extern void dirdbFlush(void); /* removes all nodes that hasn't been ref'ed yet aswell */

#define DIRDB_FULLNAME_NOBASE   1
#define DIRDB_FULLNAME_ENDSLASH 2
extern void dirdbGetFullName(uint32_t node, char *name /* PATH_MAX+1, ends not with a / */, int flags);
extern void dirdbGetname(uint32_t node, char *name /*NAME_MAX+1*/);
extern void dirdbUnref(uint32_t node);
extern void dirdbRef(uint32_t node);
extern uint32_t dirdbFindAndRef(uint32_t parent, const char *name /* NAME_MAX + 1 */);
extern uint32_t dirdbResolvePathWithBaseAndRef(uint32_t base, const char *name /* PATH_MAX + 1 */);
extern uint32_t dirdbResolvePathAndRef(const char *name /*PATH_MAX + 1 */);
extern int dirdbInit(void);
extern void dirdbClose(void);
extern uint32_t dirdbGetParentAndRef(uint32_t node);

extern void dirdbTagSetParent(uint32_t node);
extern void dirdbMakeMdbAdbRef(uint32_t node, uint32_t mdbref, uint32_t adbref);
extern void dirdbTagCancel(void);
extern void dirdbTagRemoveUntaggedAndSubmit(void);

extern int dirdbGetMdbAdb(uint32_t *dirdbnode, uint32_t *mdbnode, uint32_t *adbref, int *first);


#endif
