#ifndef _MDB_H
#define _MDB_H

struct ocpfilehandle_t;

/* includes the zero termination */
#define MDB_TITLE_LEN    127
#define MDB_COMPOSER_LEN 127 // Mostly used by tracked module
#define MDB_ARTIST_LEN   127 // Mostly used by released music
#define MDB_STYLE_LEN    127
#define MDB_COMMENT_LEN  127

/* flags */
#define MDB_VIRTUAL    64  /* used by external API, to be removed? This entry shall not be stored to disk... */
#define MDB_BIGMODULE 128  /* used by external API, to be removed? */

#define MODULETYPE(str) ((uint32_t)(((uint8_t)str[0]) | (((uint8_t)(str[0]?str[1]:0))<<8) | (((uint8_t)((str[0]&&str[1])?str[2]:0))<<16) | (((uint8_t)((str[0]&&str[1]&&str[2])?str[3]:0))<<24)) )

struct __attribute__((packed)) moduletype
{
	union
	{
		struct __attribute__((packed))
		{
			char c[4];
		} string;
		struct __attribute__((packed))
		{
			uint32_t i;
		} integer;
	};
};

struct moduleinfostruct
{
	uint64_t size;             /* read-only */

	struct moduletype modtype;
	uint8_t flags;
	uint8_t channels;
	uint16_t playtime;
	uint32_t date;

	char title[MDB_TITLE_LEN];
	char composer[MDB_TITLE_LEN];
	char artist[MDB_COMPOSER_LEN];
	char style[MDB_COMPOSER_LEN];
	char comment[MDB_COMPOSER_LEN];
	char album[MDB_COMPOSER_LEN];
};

#if 0
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
#endif

enum
{
  mdbEvInit, mdbEvDone
};

struct mdbreadinforegstruct /* this is to test a file, and give it a tag..*/
{
	const char *name; /* for debugging */
#warning Merge these two, and only use the "slow" I/O version
	int (*ReadMemInfo)(struct moduleinfostruct *m, const char *buf, size_t len);
	int (*ReadInfo)(struct moduleinfostruct *m, struct ocpfilehandle_t *f, const char *buf, size_t len);
	void (*Event)(int mdbEv);
	struct mdbreadinforegstruct *next;
};

#define MDBREADINFOREGSTRUCT_TAIL ,0

struct ocpfile_t;

int mdbGetModuleType (uint32_t fileref, struct moduletype *dst);
int mdbInfoIsAvailable (uint32_t fileref); // used to be mdbInfoRead
int mdbReadInfo(struct moduleinfostruct *m, struct ocpfilehandle_t *f);
int mdbWriteModuleInfo(uint32_t fileref, struct moduleinfostruct *m); // returns zero on error
void mdbScan(struct ocpfile_t *file, uint32_t mdb_ref);
int mdbInit(void); // returns zero on error
void mdbUpdate(void);
void mdbClose(void);
uint32_t mdbGetModuleReference2(const uint32_t dirdb_ref, uint32_t size);
int mdbGetModuleInfo(struct moduleinfostruct *m, uint32_t fileref); // returns zero on error

void mdbRegisterReadInfo(struct mdbreadinforegstruct *r);
void mdbUnregisterReadInfo(struct mdbreadinforegstruct *r);

extern const char mdbsigv1[60];
extern const char mdbsigv2[60];

extern uint8_t mdbCleanSlate; /* media-db needs to know that we used to be previous version database before we hashed filenames */


#endif
