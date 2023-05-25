#ifndef __PFILESEL_H
#define __PFILESEL_H

#include <stdio.h> /* FILE * */
#include "mdb.h" /* struct moduletype */

struct ocpfilehandle_t;
struct moduleinfostruct;
struct cpifaceplayerstruct;
struct configAPI_t;
extern int fsGetNextFile (struct moduleinfostruct *info, struct ocpfilehandle_t **filehandle); /* info comes from external buffer */
extern int fsGetPrevFile (struct moduleinfostruct *info, struct ocpfilehandle_t **filehandle); /* info comes from external buffer */
extern int fsFilesLeft(void);
extern signed int fsFileSelect(void);
/* extern char fsAddFiles(const char *);      use the playlist instead..*/
extern int fsPreInit (const struct configAPI_t *configAPI);
extern int fsInit(void);
extern int fsLateInit (const struct configAPI_t *configAPI);
extern void fsClose(void);
extern void fsLateClose(void);

extern int fsFPS; /* see stuff/framelock.c */
extern int fsFPSCurrent; /* see stuff/framelock.c */
extern int fsListScramble;
extern int fsListRemove;
extern int fsLoopMods;
extern int fsScanNames;
extern int fsScanArcs;
extern int fsScanInArc;
extern int fsScrType;
extern int fsEditWin;
extern int fsColorTypes;
extern int fsInfoMode;
extern int fsPutArcs;
extern int fsWriteModInfo;
extern int fsShowAllFiles;
#if 0
extern const char *fsTypeNames[256]; /* type description */
#endif

/* description: is NULL terminating string array of up to 6 lines of 76 characters
 *
 * interfacename: Usually "plOpenCP" or "VirtualInterface" - last is not user-editable
 */
void fsTypeRegister (struct moduletype modtype, const char **description, const char *interfacename, const struct cpifaceplayerstruct *cp);
void fsTypeUnregister (struct moduletype modtype);
uint8_t fsModTypeColor(struct moduletype modtype); /* replaces fsTypeCols[] */

extern void fsRegisterExt(const char *ext);
extern int fsIsModule(const char *ext);

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
	int (*Init)(struct moduleinfostruct *info, struct ocpfilehandle_t *f, const struct cpifaceplayerstruct *cp);
	interfaceReturnEnum (*Run)(void);
	void (*Close)(void);
	const char *name;
	struct interfacestruct *next;
};
#define INTERFACESTRUCT_TAIL ,0

struct DevInterface
{
	int (*Init)(struct moduleinfostruct *info, struct ocpfilehandle_t *f, const struct cpifaceplayerstruct *cp);
	interfaceReturnEnum (*Run)(const struct cpifaceplayerstruct *cp);
	void (*Close)(const struct cpifaceplayerstruct *cp);
};

void plRegisterInterface(struct interfacestruct *_interface);
void plUnregisterInterface(struct interfacestruct *_interface);
void plFindInterface(struct moduletype modtype, const struct interfacestruct **i, const struct cpifaceplayerstruct **cp);

#define RD_PUTSUBS 1
#define RD_ARCSCAN 2
#define RD_SUBNOSYMLINK 4
#define RD_PUTDRIVES 8
#define RD_PUTRSUBS 16
#define RD_ISMODONLY 32

int fsMatchFileName12(const char *a, const char *b);

extern void fsSetup(void);
extern void fsRescanDir(void);

struct modlist;
struct ocpdir_t;
extern int fsReadDir(struct modlist *ml, struct ocpdir_t *dir, const char *mask, unsigned long opt);
extern void fsForceRemove(const uint32_t dirdbref);

#endif
