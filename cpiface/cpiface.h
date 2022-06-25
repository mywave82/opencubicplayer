/* OpenCP Module Player
 * copyright (c) '94-'05 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * CPIface note dots mode
 *
 * revision history: (please note changes here)
 *  -nb980510   Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *    -first release
 *  -doj980928  Dirk Jagdmann <doj@cubic.org>
 *    -deleted plReadOpenCPPic() which is now found in cpipic.h
 *  -fd981119   Felix Domke <tmbinc@gmx.net>
 *    -added some really important declspec-stuff
 */

#ifndef __CPIFACE_H
#define __CPIFACE_H

#include <stdio.h>

struct cpifaceSessionAPI_t;
struct moduleinfostruct;
struct ocpfilehandle_t;
struct cpifaceplayerstruct
{
	const char *playername;
	int (*OpenFile) (struct moduleinfostruct *info,
	                 struct ocpfilehandle_t *f,
	                 const char *ldlink, // some player "plugins" uses loaders. This is the name of that "loader plugin"
	                 const char *loader, // And this is the loader symbol used
	                 struct cpifaceSessionAPI_t *cpiSessionAPI); // devp/devw and other APIs can hook up here!
	void (*CloseFile)();
};

struct cpifaceSessionAPI_t
{
	void (*GetRealMasterVolume)(int *l, int *r);
	void (*GetMasterSample)(int16_t *, unsigned int len, uint32_t rate, int mode);
	uint_fast16_t LogicalChannelCount;  /* number of logical channels. Used by "Channel" viewer and selector, note-dot viewer, can be used by scope viewers, and is the default value used by track viewer */
	uint_fast16_t PhysicalChannelCount; /* number of physical audio channels. Sometimes a format uses shadow channels for effects or smooth transitions. Can be used by scope viewers. */
};
extern __attribute__ ((visibility ("internal"))) struct cpifaceSessionAPI_t cpifaceSessionAPI;

#warning move all these into cpifaceAPISource_t
extern unsigned char plSelCh;
extern unsigned char plChanChanged;
extern char plPause;
extern char plMuteCh[];
extern char plPanType;
extern int (*plProcessKey)(uint16_t key);
extern void (*plDrawGStrings)(void);
extern int (*plIsEnd)(void);
extern void (*plIdle)(void);
extern void (*plSetMute)(int i, int m);
extern int (*plGetLChanSample)(unsigned int ch, int16_t *, unsigned int len, uint32_t rate, int opt);
extern int (*plGetPChanSample)(unsigned int ch, int16_t *, unsigned int len, uint32_t rate, int opt);

extern void cpiKeyHelp(uint16_t key, const char *shorthelp);
extern void cpiKeyHelpClear(void);
extern int cpiKeyHelpDisplay(void); /* recall until it returns zero. This function WILL call ekbhit and egetch, but not framelock */

struct cpimoderegstruct
{
  char handle[9];
  void (*SetMode)();
  void (*Draw)();
  int (*IProcessKey)(uint16_t);
  int (*AProcessKey)(uint16_t);
  int (*Event)(int ev);
  struct cpimoderegstruct *next;
  struct cpimoderegstruct *nextdef;
};

#define CPIMODEREGSTRUCT_TAIL ,0,0

struct cpitextmoderegstruct;

struct cpitextmodequerystruct
{
  unsigned char top;
  unsigned char xmode; /* bit0, want to be main left column
                          bit1, want to be the right column
                         0x00 = not visible / reserved
                         0x01 = left column (we might cover the hole width)
                         0x02 = right column (we might not be visible, if screen is to narrow)
                         0x03 = we want to cover the hole width
                       */
  unsigned char killprio;
  unsigned char viewprio;
  unsigned char size; /* used to demand more height than hgtmin. Can be bigger than hgtmax to increase the priority when space is distributed */
  int hgtmin;
  int hgtmax;
  struct cpitextmoderegstruct *owner;
};

struct cpitextmoderegstruct
{
  char handle[9];
  int (*GetWin)(struct cpitextmodequerystruct *q);
  void (*SetWin)(int xmin, int xwid, int ymin, int ywid);
  void (*Draw)(int focus);
  int (*IProcessKey)(unsigned short);
  int (*AProcessKey)(unsigned short);
  int (*Event)(int ev);
  int active;
  struct cpitextmoderegstruct *nextact;
  struct cpitextmoderegstruct *next;
  struct cpitextmoderegstruct *nextdef;
};

#define CPITEXTMODEREGSTRUCT_TAIL ,0,0,0,0

enum
{
  cpievOpen, cpievClose,
  cpievInit, cpievDone,       /* used to test if you can be added to Mode list (called on every song) */
  cpievInitAll, cpievDoneAll, /* used to test if you can be added to DefMode list (called by cpiRegisterDefMode) */
  cpievGetFocus, cpievLoseFocus, cpievSetMode,
  cpievKeepalive=42
};

extern void cpiDrawGStrings(void);
extern void cpiSetGraphMode(int big);
extern void cpiSetTextMode(int size);
extern void cpiResetScreen(void);
extern void cpiRegisterDefMode(struct cpimoderegstruct *m); /* this are stuck until unregister is called */
extern void cpiUnregisterDefMode(struct cpimoderegstruct *m);
extern void cpiRegisterMode(struct cpimoderegstruct *m); /* This list is cleared automatically on song close, and deflist is appended on load */
extern void cpiUnregisterMode(struct cpimoderegstruct *m);


extern void cpiSetMode(const char *hand);
extern void cpiGetMode(char *hand);
extern void cpiTextRegisterMode(struct cpitextmoderegstruct *mode);
extern void cpiTextUnregisterMode(struct cpitextmoderegstruct *m);
extern void cpiTextRegisterDefMode(struct cpitextmoderegstruct *mode);
extern void cpiTextUnregisterDefMode(struct cpitextmoderegstruct *m);
extern void cpiTextSetMode(const char *name);
extern void cpiTextRecalc(void);

void cpiForwardIProcessKey(uint16_t key);

extern void plUseMessage(char **);

struct insdisplaystruct
{
	int height, bigheight;
	char *title80;
	char *title132;
	void (*Mark)(void);
	void (*Clear)(void);
	void (*Display)(uint16_t *buf, int len, int n, int mode);
	void (*Done)(void);
};

extern void plUseInstruments(struct insdisplaystruct *x);

extern void plUseChannels(void (*Display)(unsigned short *buf, int len, int i));

struct notedotsdata
{
	uint8_t chan;
	uint16_t note;
	uint16_t voll,volr;
	uint8_t col;
};

extern void plUseDots(int (*get)(struct notedotsdata *, int));

struct cpitrakdisplaystruct
{
	int (*getcurpos)(void);
	int (*getpatlen)(int n);
	const char *(*getpatname)(int n);
	void (*seektrack)(int n, int c);
	int (*startrow)(void);
	int (*getnote)(uint16_t *bp, int small);
	int (*getins)(uint16_t *bp);
	int (*getvol)(uint16_t *bp);
	int (*getpan)(uint16_t *bp);
	void (*getfx)(uint16_t *bp, int n);
	void (*getgcmd)(uint16_t *bp, int n);
};

extern void cpiTrkSetup(const struct cpitrakdisplaystruct *c, int npat);
extern void cpiTrkSetup2(const struct cpitrakdisplaystruct *c, int npat, int tracks);

extern char plNoteStr[132][4];
extern unsigned char plChanChanged;
extern char plCompoMode;

/* mcpedit.c */
extern void mcpDrawGStrings(void);

struct moduleinfostruct;
void mcpDrawGStringsFixedLengthStream (const char                    *filename8_3,
                                       const char                    *filename16_3,
                                       const uint64_t                 pos,
                                       const uint64_t                 size, /* can be smaller than the file-size due to meta-data */
                                       const char                     sizesuffix, /* 0 = "" (MIDI), 1 = KB */
                                       const char                    *opt25,
                                       const char                    *opt50,
                                       const int_fast16_t             kbs,  /* kilo-bit-per-second */
                                       const uint_fast8_t             inpause,
                                       const uint_fast16_t            seconds,
                                       const struct moduleinfostruct *mdbdata);

void mcpDrawGStringsSongXofY (const char                    *filename8_3,
                              const char                    *filename16_3,
                              const int                      songX,
                              const int                      songY,
                              const uint_fast8_t             inpause,
                              const uint_fast16_t            seconds,
                              const struct moduleinfostruct *mdbdata);

void mcpDrawGStringsTracked (const char                    *filename8_3,
                             const char                    *filename16_3,
                             const int                      songX,
                             const int                      songY, /* 0 or smaller, disables this, else 2 digits.. */
                             const uint8_t                  rowX,
                             const uint8_t                  rowY, /* displayed as 2 hex digits */
                             const uint16_t                 orderX,
                             const uint16_t                 orderY, /* displayed as 1,2,3 or 4 hex digits, depending on this size */
                             const uint8_t                  speed, /* displayed as %3 (with no space prefix) decimal digits */
                             const uint8_t                  tempo, /* displayed as %3 decimal digits */
                             const int16_t                  gvol, /* -1 for disable, else 0x00..0xff */
                             const int                      gvol_slide_direction,
                             const uint8_t                  chanX,
                             const uint8_t                  chanY, /* set to zero to disable */
                             const int                      amplification, /* -1 for disable */
                             const char                    *filter, /* 3 character string if non-null */
                             const uint_fast8_t             inpause,
                             const uint_fast16_t            seconds,
                             const struct moduleinfostruct *mdbdata);

extern int mcpSetProcessKey(uint16_t key);


/* For the sliding pause effect, range 64 = normal speed, 1 = almost complete stop.
 * For complete stop with wavetable use mcpSet (-1, mcpMasterPause, 1) and for stream playback the stream has to send zero-data
 */
extern void mcpSetMasterPauseFadeParameters(int i);

#endif
