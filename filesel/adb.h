#ifndef _ADB_H
#define _ADB_H

enum
{
	adbCallGet, adbCallDelete, adbCallMoveTo, adbCallMoveFrom, adbCallPut
};

struct adbregstruct
{
	const char *ext;
	int (*Scan)(const char *path);
	int (*Call)(const int act, const char *apath, const char *fullname, const int fd);
	struct adbregstruct *next;
};

#define ADBREGSTRUCT_TAIL ,0

#define ARC_PATH_MAX 127
struct __attribute__((packed)) arcentry
{
#define ADB_USED 1
#define ADB_DIRTY 2
#define ADB_ARC 4
	uint8_t flags;
	uint32_t parent;
	char name[ARC_PATH_MAX+1]; /* some stupid archives needs full path, which can be long */
	uint32_t size;
};

extern char adbInit(void);
extern void adbUpdate(void);
extern void adbClose(void);
extern int adbCallArc(const char *cmd, const char *arc, const char *name, const char *dir);

extern int adbAdd(const struct arcentry *a);
extern uint32_t adbFind(const char *arcname);
extern int isarchivepath(const char *p);

extern const char adbsigv1[16];
extern const char adbsigv2[16];

extern void adbRegister(struct adbregstruct *r);
extern void adbUnregister(struct adbregstruct *r);

#include <stdio.h>
struct modlistentry;
extern FILE *adb_ReadHandle(struct modlistentry *entry);
extern int adb_ReadHeader(struct modlistentry *entry, char *mem, size_t *size);
extern int adb_Read(struct modlistentry *entry, char **mem, size_t *size);


#endif
