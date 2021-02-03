#ifndef __PFILESEL_H
#define __PFILESEL_H

#include <stdio.h> /* FILE * */

struct ocpfilehandle_t;
struct moduleinfostruct;
extern int fsGetNextFile (struct moduleinfostruct *info, struct ocpfilehandle_t **filehandle); /* info comes from external buffer */
extern int fsGetPrevFile (struct moduleinfostruct *info, struct ocpfilehandle_t **filehandle); /* info comes from external buffer */
extern int fsFilesLeft(void);
extern signed int fsFileSelect(void);
/* extern char fsAddFiles(const char *);      use the playlist instead..*/
extern int fsPreInit(void);
extern int fsLateInit(void);
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
extern const char *fsTypeNames[256]; /* type description */

extern void fsRegisterExt(const char *ext);
extern int fsIsModule(const char *ext);

struct preprocregstruct
{
	void (*Preprocess)(struct moduleinfostruct *info, struct ocpfilehandle_t **f);
	struct preprocregstruct *next;
};

#define PREPROCREGSTRUCT_TAIL ,0

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
	int (*Init)(struct moduleinfostruct *info, struct ocpfilehandle_t *f);
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
#define RD_PUTDRIVES 8
#define RD_PUTRSUBS 16

#if 0
extern char mifMemRead(const char *name, unsigned short size, char *ptr);
#endif

int fsMatchFileName12(const char *a, const char *b);

extern void fsDraw(void); /* used by medialib to have backdrop for the dialogs they display on the screen */
extern void fsSetup(void);
extern void fsRescanDir(void);

struct modlist;
struct ocpdir_t;
extern int fsReadDir(struct modlist *ml, struct ocpdir_t *dir, const char *mask, unsigned long opt);
extern void fsForceRemove(const uint32_t dirdbref);

#endif
