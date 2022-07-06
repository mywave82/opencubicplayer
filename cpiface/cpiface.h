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

#define MAXLCHAN 64

struct cpifaceSessionAPI_t;
struct moduleinfostruct;
struct ocpfilehandle_t;
struct cpifaceplayerstruct
{
	const char *playername;
	int (*OpenFile) (struct cpifaceSessionAPI_t *cpifaceSession,
	                 struct moduleinfostruct *info,
	                 struct ocpfilehandle_t *f,
	                 const char *ldlink, // some player "plugins" uses loaders. This is the name of that "loader plugin"
	                 const char *loader); // And this is the loader symbol used
	void (*CloseFile)(struct cpifaceSessionAPI_t *cpifaceSession);
};

struct cpifaceSessionAPI_t
{
	/* configured by devp/devw */
	void (*GetRealMasterVolume)(int *l, int *r); /* filled in by devp/devw driver */
	void (*GetMasterSample)(int16_t *, unsigned int len, uint32_t rate, int mode); /* filled in by devp/devw driver */

	/* configured by playback plugin during intialization of the given playback file */
	uint_fast16_t LogicalChannelCount;  /* number of logical channels. Used by "Channel" viewer and selector, note-dot viewer, can be used by scope viewers, and is the default value used by track viewer */
	uint_fast16_t PhysicalChannelCount; /* number of physical audio channels. Sometimes a format uses shadow channels for effects or smooth transitions. Can be used by scope viewers. */
	int (*GetLChanSample)(struct cpifaceSessionAPI_t *cpifacesession, unsigned int ch, int16_t *, unsigned int len, uint32_t rate, int opt); /* Get sample data for a given logical channel, used by visualizers */
	int (*GetPChanSample)(struct cpifaceSessionAPI_t *cpifacesession, unsigned int ch, int16_t *, unsigned int len, uint32_t rate, int opt); /* Get sample data for a given physical channel, used by visualizers */

	/* Callbacks and status from cpiface to plugin */
	uint8_t MuteChannel[MAXLCHAN]; /* Reflects the status of channel muting used by channel visualizers. Should be controlled by the playback plugin */
	void (*SetMuteChannel)(struct cpifaceSessionAPI_t *cpifaceSession, int LogicalChannel, int IsMuted); /* Callback from cpiface to set the Mute channel for a given logical channel */
	void (*DrawGStrings)(struct cpifaceSessionAPI_t *cpifaceSession); /* Draw the header, usually utilizes some of the fixed provides */
	int (*ProcessKey) (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key); /* Decode keyboard presses from the user. Return value 1 = key is swalled, 0 = key is not swallowed.  */
	int (*IsEnd)(struct cpifaceSessionAPI_t *cpifaceSession, int LoopMod); /* The main "idle" function that should forward audio to the devw/devp. It should return 1 if looped and EOF detected. LoopMod dictates if the user has selected loop-single or not at any given time */

	/* Normally controlled by playback plugin */
	uint8_t InPause; /* used by cpiface UI elements to know if the playback is paused or not */
	uint8_t PanType; /* If this is one, it causes the visual channel-layout to swap right and left channel for every second channel group - currenly only used by some S3M files */

	/* Normally controlled by cpiface */
	uint8_t SelectedChannel; /* Used by most viewers*/
	uint8_t SelectedChannelChanged; /* Used to cache redraws of channels */
};

extern void cpiKeyHelp(uint16_t key, const char *shorthelp);
extern void cpiKeyHelpClear(void);
extern int cpiKeyHelpDisplay(void); /* recall until it returns zero. This function WILL call ekbhit and egetch, but not framelock */

struct cpimoderegstruct
{
  char handle[9];
  void (*SetMode)();
  void (*Draw)(struct cpifaceSessionAPI_t *cpifaceSession);
  int (*IProcessKey)(struct cpifaceSessionAPI_t *cpifaceSession, uint16_t);
  int (*AProcessKey)(struct cpifaceSessionAPI_t *cpifaceSession, uint16_t);
  int (*Event)(struct cpifaceSessionAPI_t *cpifaceSession, int ev);
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
  int (*GetWin)(struct cpifaceSessionAPI_t *cpifaceSession, struct cpitextmodequerystruct *q);
  void (*SetWin)(struct cpifaceSessionAPI_t *cpifaceSession, int xmin, int xwid, int ymin, int ywid);
  void (*Draw)(struct cpifaceSessionAPI_t *cpifaceSession, int focus);
  int (*IProcessKey)(struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key);
  int (*AProcessKey)(struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key);
  int (*Event)(struct cpifaceSessionAPI_t *cpifaceSession, int ev);
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

extern void cpiDrawGStrings (struct cpifaceSessionAPI_t *cpifaceSession);
extern void cpiSetGraphMode(int big);
extern void cpiSetTextMode(int size);
extern void cpiResetScreen(void);
extern void cpiRegisterDefMode(struct cpimoderegstruct *m); /* this are stuck until unregister is called */
extern void cpiUnregisterDefMode(struct cpimoderegstruct *m);
extern void cpiRegisterMode(struct cpimoderegstruct *m); /* This list is cleared automatically on song close, and deflist is appended on load */
extern void cpiUnregisterMode(struct cpimoderegstruct *m);


extern void cpiSetMode(const char *hand);
extern void cpiGetMode(char *hand);
extern void cpiTextRegisterMode (struct cpifaceSessionAPI_t *cpifaceSession, struct cpitextmoderegstruct *mode);
extern void cpiTextUnregisterMode (struct cpifaceSessionAPI_t *cpifaceSession, struct cpitextmoderegstruct *m);
extern void cpiTextRegisterDefMode(struct cpitextmoderegstruct *mode);
extern void cpiTextUnregisterDefMode(struct cpitextmoderegstruct *m);
extern void cpiTextSetMode(struct cpifaceSessionAPI_t *cpifaceSession, const char *name);
extern void cpiTextRecalc (struct cpifaceSessionAPI_t *cpifaceSession);

void cpiForwardIProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key);

extern void plUseMessage(char **);

struct insdisplaystruct
{
	int height, bigheight;
	char *title80; /* cp437 */
	char *title132; /* cp437 */
	void (*Mark)(struct cpifaceSessionAPI_t *cpifaceSession);
	void (*Clear)(struct cpifaceSessionAPI_t *cpifaceSession);
	void (*Display)(struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int len, int n, int mode);
	void (*Done)(struct cpifaceSessionAPI_t *cpifaceSession);
};

extern void plUseInstruments(struct cpifaceSessionAPI_t *cpifaceSession, struct insdisplaystruct *x);

extern void plUseChannels(struct cpifaceSessionAPI_t *cpifaceSession, void (*Display)(struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int len, int i));

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

extern void cpiTrkSetup  (struct cpifaceSessionAPI_t *cpifaceSession, const struct cpitrakdisplaystruct *c, int npat);
extern void cpiTrkSetup2 (struct cpifaceSessionAPI_t *cpifaceSession, const struct cpitrakdisplaystruct *c, int npat, int tracks);

extern char plNoteStr[132][4];
extern char plCompoMode;

struct moduleinfostruct;
void mcpDrawGStringsFixedLengthStream (struct cpifaceSessionAPI_t *cpifaceSession,
                                       const char                    *filename8_3,
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

void mcpDrawGStringsSongXofY (struct cpifaceSessionAPI_t *cpifaceSession,
                              const char                    *filename8_3,
                              const char                    *filename16_3,
                              const int                      songX,
                              const int                      songY,
                              const uint_fast8_t             inpause,
                              const uint_fast16_t            seconds,
                              const struct moduleinfostruct *mdbdata);

void mcpDrawGStringsTracked (struct cpifaceSessionAPI_t *cpifaceSession,
                             const char                    *filename8_3,
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
                             const uint_fast8_t             inpause,
                             const uint_fast16_t            seconds,
                             const struct moduleinfostruct *mdbdata);


/* For the sliding pause effect, range 64 = normal speed, 1 = almost complete stop.
 * For complete stop with wavetable use mcpSet (-1, mcpMasterPause, 1) and for stream playback the stream has to send zero-data
 */
void mcpSetMasterPauseFadeParameters (struct cpifaceSessionAPI_t *cpifaceSession, int i);

enum mcpNormalizeType
{
	mcpNormalizeNoFilter = 0,
	mcpNormalizeFilterAOIFOI = 1,

	mcpNormalizeMustSpeedPitchLock = 0,
	mcpNormalizeCanSpeedPitchUnlock = 4,

	mcpNormalizeCannotEcho = 0,
	mcpNormalizeCanEcho = 8, /* no wave-table can actually do this (yet) */

	mcpNormalizeCannotAmplify = 0,
	mcpNormalizeCanAmplify = 16,

	mcpNormalizeDefaultPlayW = 21,
	mcpNormalizeDefaultPlayP = 0,
};

void mcpNormalize (struct cpifaceSessionAPI_t *cpifaceSession, enum mcpNormalizeType Type);

#endif
