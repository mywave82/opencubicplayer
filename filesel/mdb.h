#ifndef _MDB_H
#define _MDB_H

struct ocpfilehandle_t;

struct __attribute__((packed)) moduleinfostruct
{
#define MDB_USED 1
#define MDB_DIRTY 2
#define MDB_BLOCKTYPE 12
#define MDB_VIRTUAL 16
#define MDB_BIGMODULE 32
#define MDB_RESERVED 64 /* used to be MDB_PLAYLIST */
	uint8_t flags1;      /*  0 */
#define MDB_GENERAL 0
	/* sorry dudes, but I upgraded the references to 32 bit */
	uint8_t modtype;     /*  1 */
	uint32_t comref;     /*  5 */
	uint32_t compref;    /*  9 */
	uint32_t futref;     /* 13 */
	char name[12];       /* 25 */
	uint32_t size;       /* 29 */
	char modname[32];    /* 61 */
	uint32_t date;       /* 65 */
	uint16_t playtime;   /* 67 */
	uint8_t channels;    /* 68 */
	uint8_t moduleflags; /* 69 => 70 */

	uint8_t flags2;      /*  0 */
#define MDB_COMPOSER 4
	char composer[32];   /*  1 */
	char style[31];      /* 33 => 65 */

	uint8_t flags3;      /*  0 */
	char unusedfill1[6]; /*  1 */
#define MDB_COMMENT 8
	char comment[63];    /*  7 => 70 */

	uint8_t flags4;      /*  0 */
	char unusedfill2[6]; /*  1 */
#define MDB_FUTURE 12
	char dum[63];        /*  7 => 70 */
};

enum
{
	mtMOD=0, mtMODd=1, mtMODt=2, mtM31=3, mtM15=6, mtM15t=7, mtWOW=8,
	mtS3M=9, mtXM=10, mtMTM=11, mt669=12, mtULT=13, mtDMF=14, mtOKT=15,
	mtMID=16, mtCDA=17, mtMIDd=18, mtPTM=19, mtMED=20, mtMDL=21, mtAMS=22,
	mtINP=23, /* These three now uses a common mtDEVv instead mtDEVp=24, mtDEVs=25, mtDEVw=26, */ mtIT=27, mtWAV=28, mtVOC=29,
	mtMPx=30, mtSID=31, mtMXM=32, mtMODf=33, mtWMA=34, mtOGG=35, mtOPL=36,
	mtAY=37, mtFLAC=38, mtYM=39, mtSTM=40, mtHVL=41,
	/* These two no longer makes it into the mdb mtPLS=128, mtM3U=129, */ mtANI=130,

	mtDEVv=0xfe, mtUnRead=0xFF
};

enum
{
  mdbEvInit, mdbEvDone
};

struct mdbreadinforegstruct /* this is to test a file, and give it a tag..*/
{
	int (*ReadMemInfo)(struct moduleinfostruct *m, const char *buf, size_t len);
	int (*ReadInfo)(struct moduleinfostruct *m, struct ocpfilehandle_t *f, const char *buf, size_t len);
	void (*Event)(int mdbEv);
	struct mdbreadinforegstruct *next;
};

#define MDBREADINFOREGSTRUCT_TAIL ,0

struct ocpfile_t;

extern const char *mdbGetModTypeString(unsigned char type);
extern int mdbGetModuleType(uint32_t fileref);
extern uint8_t mdbReadModType(const char *str);
extern int mdbInfoRead(uint32_t fileref);
extern int mdbReadMemInfo(struct moduleinfostruct *m, const char *buf, int len);
extern int mdbReadInfo(struct moduleinfostruct *m, struct ocpfilehandle_t *f);
extern int mdbWriteModuleInfo(uint32_t fileref, struct moduleinfostruct *m);
void mdbScan(struct ocpfile_t *file, uint32_t mdb_ref);
extern int mdbInit(void);
extern void mdbUpdate(void);
extern void mdbClose(void);
extern uint32_t mdbGetModuleReference2(const uint32_t dirdb_ref, uint32_t size);
extern int mdbGetModuleInfo(struct moduleinfostruct *m, uint32_t fileref);

extern void mdbRegisterReadInfo(struct mdbreadinforegstruct *r);
extern void mdbUnregisterReadInfo(struct mdbreadinforegstruct *r);

extern const char mdbsigv1[60];

#endif
