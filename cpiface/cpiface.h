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

struct moduleinfostruct;
struct cpifaceplayerstruct
{
	int (*OpenFile)(const char *path, struct moduleinfostruct *info, FILE *f);
	void (*CloseFile)();
};

enum
{
  cpiGetSampleStereo=1, cpiGetSampleHQ=2
};

extern unsigned short plNLChan;
extern unsigned short plNPChan;
extern unsigned char plSelCh;
extern unsigned char plChanChanged;
extern char plPause;
extern char plMuteCh[];
extern char plPanType;
extern int (*plProcessKey)(uint16_t key);
extern void (*plDrawGStrings)(uint16_t (*plTitleBuf)[CONSOLE_MAX_X]);
extern void (*plGetRealMasterVolume)(int *l, int *r);
extern void (*plGetMasterSample)(int16_t *, unsigned int len, uint32_t rate, int mode);
extern int (*plIsEnd)(void);
extern void (*plIdle)(void);
extern void (*plSetMute)(int i, int m);
extern int (*plGetLChanSample)(unsigned int ch, int16_t *, unsigned int len, uint32_t rate, int opt);
extern int (*plGetPChanSample)(unsigned int ch, int16_t *, unsigned int len, uint32_t rate, int opt);

extern void cpiKeyHelp(uint16_t key, const char *shorthelp);
extern void cpiKeyHelpDisplay(void);
extern void (*cpiKeyHelpReset)(void); /* internal use only */

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
  unsigned char xmode;
  unsigned char killprio;
  unsigned char viewprio;
  unsigned char size;
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

extern char plNoteStr[132][4];
extern unsigned char plChanChanged;
extern char plCompoMode;

/* mcpedit.c */
extern void mcpNormalize(int hasfilter);
extern void mcpDrawGStrings(uint16_t (*buf)[CONSOLE_MAX_X]);
extern int mcpSetProcessKey(uint16_t key);
extern void mcpSetFadePars(int i);
extern uint16_t globalmcpspeed;
extern uint16_t globalmcppitch;

#endif
