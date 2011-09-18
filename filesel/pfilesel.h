#ifndef __PFILESEL_H
#define __PFILESEL_H

#include <stdio.h> /* FILE * */

struct moduleinfostruct;
extern int fsGetNextFile(char *path, struct moduleinfostruct *info, FILE **); /* info comes from external buffer */
extern int fsGetPrevFile(char *path, struct moduleinfostruct *info, FILE **); /* info comes from external buffer */
extern int fsFilesLeft(void);
extern signed int fsFileSelect(void);
/* extern char fsAddFiles(const char *);      use the playlist instead..*/
extern int fsPreInit(void);
extern int fsInit(void);
extern void fsClose(void);

extern int fsFPS; /* see stuff/framelock.c */
extern int fsFPSCurrent; /* see stuff/framelock.c */
extern int fsListScramble;
extern int fsListRemove;
extern int fsLoopMods;
extern int fsScanNames;
extern int fsScanArcs;
extern int fsScanInArc;
extern int fsScanMIF;
extern int fsScrType;
extern int fsEditWin;
extern int fsColorTypes;
extern int fsInfoMode;
extern int fsPutArcs;
extern int fsWriteModInfo;
extern const char *(fsTypeNames[256]); /* type description */

extern void fsRegisterExt(const char *ext);
extern int fsIsModule(const char *ext);

struct preprocregstruct
{
	void (*Preprocess)(const char *path, struct moduleinfostruct *info, FILE **f);
	struct preprocregstruct *next;
};

#define PREPROCREGSTRUCT_TAIL ,0

struct filehandlerstruct
{
	int (*Process)(const char *path, struct moduleinfostruct *m, FILE **f);
};

typedef enum {
	interfaceReturnContinue=0,
	interfaceReturnNextAuto=1,
	interfaceReturnQuit=2,
	interfaceReturnNextManuel=3,
	interfaceReturnPrevManuel=6,
	interfaceReturnCallFs=4,
	interfaceReturnDosShell=5
} interfaceReturnEnum;

struct interfacestruct
{
	int (*Init)(const char *path, struct moduleinfostruct *info, FILE **f);
	interfaceReturnEnum (*Run)(void);
	void (*Close)(void);
	const char *name;
	struct interfacestruct *next;
};
#define INTERFACESTRUCT_TAIL ,0


extern void plRegisterInterface(struct interfacestruct *interface);
extern void plUnregisterInterface(struct interfacestruct *interface);
extern struct interfacestruct *plFindInterface(const char *name);
extern struct preprocregstruct *plPreprocess;
extern void plRegisterPreprocess(struct preprocregstruct *r);
extern void plUnregisterPreprocess(struct preprocregstruct *r);

#define RD_PUTSUBS 1
#define RD_ARCSCAN 2
#define RD_SUBNOSYMLINK 4
#define RD_PUTRSUBS 16

#if 0
struct modlist;

extern char *dmGetPath(char *path, unsigned short ref);
extern unsigned short dmGetPathReference(const char *p);
extern unsigned short dmGetDriveDir(int drv);
#endif

#if 0
extern char mifMemRead(const char *name, unsigned short size, char *ptr);
#endif
struct dmDrive
{
	char drivename[13];
	/*char currentpath[PATH_MAX+1];*/
	uint32_t basepath;
	uint32_t currentpath;
	struct dmDrive *next;
};
extern struct dmDrive *dmDrives;
extern struct dmDrive *RegisterDrive(const char *dmDrive);
extern struct dmDrive *dmFindDrive(const char *dmDrive); /* to get the correct drive from a given string */
extern struct dmDrive *dmFILE;

void fsConvFileName12(char *c, const char *f, const char *e);
void convfilename12wc(char *c, const char *f, const char *e);
int fsMatchFileName12(const char *a, const char *b);

extern void fsSetup(void);
extern void fsRescanDir(void);
extern void fsForceRemove(const char *path);

struct modlistentry;
extern int dosfile_Read(struct modlistentry *entry, char **mem, size_t *size); /* used by medialib */
extern int dosfile_ReadHeader(struct modlistentry *entry, char *mem, size_t *size); /* used by medialib */
extern FILE *dosfile_ReadHandle(struct modlistentry *entry);

#endif
