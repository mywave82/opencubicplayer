#ifndef __MIDI_H
#define __MIDI_H

struct msample
{
	char name[32];
	uint8_t sampnum;    /* instrument-mode sample number */
	int16_t handle;     /* mcp handle */
	uint16_t normnote;
	uint32_t volrte[6];
	uint16_t volpos[6];
	uint8_t end;
	int8_t sustain;
	uint16_t tremswp;
	uint16_t tremrte;
	uint16_t tremdep;
	uint16_t vibswp;
	uint16_t vibrte;
	uint16_t vibdep;
	uint16_t sclfac;
	uint8_t sclbas;
};

struct minstrument
{
	char name[32];
	uint8_t prognum;
	uint16_t sampnum; /* number of (struct msample *) below */
	struct msample *samples;
	uint8_t note[128];
};

struct miditrack
{
	uint8_t *trk;
	uint8_t *trkend;
};

struct midifile
{
	uint32_t opt;
	uint16_t tracknum;
	uint16_t tempo;
	struct miditrack *tracks;
	uint32_t ticknum;
	uint8_t instmap[129];
	uint16_t instnum;
	uint16_t sampnum;
	struct minstrument *instruments;
	struct sampleinfo *samples;

};
extern void __attribute__ ((visibility ("internal"))) mid_free(struct midifile *);
extern void __attribute__ ((visibility ("internal"))) mid_reset(struct midifile *);
extern int __attribute__ ((visibility ("internal"))) mid_loadsamples(struct midifile *);

struct mchaninfo
{
	uint8_t ins;
	uint8_t pan;
	uint8_t gvol;
	int16_t pitch;
	uint8_t reverb;
	uint8_t chorus;
	uint8_t notenum;
	uint8_t pedal;
	uint8_t note[32];
	uint8_t vol[32];
	uint8_t opt[32];
};

struct mchaninfo2
{
	uint8_t mute;
	uint8_t notenum;
	uint8_t opt[32];
	uint8_t ins[32];
	uint16_t note[32];
	uint8_t voll[32];
	uint8_t volr[32];
};

struct mglobinfo
{
	uint32_t curtick;
	uint32_t ticknum;
	uint32_t speed;
};

extern int __attribute__ ((visibility ("internal"))) midInit(void);
extern void __attribute__ ((visibility ("internal"))) midClose(void);

#define MID_DRUMCH16 1

#include <stdio.h>
extern char __attribute__ ((visibility ("internal"))) midLoadMidi(struct midifile *, FILE *, uint32_t opt);
extern int __attribute__ ((visibility ("internal"))) midPlayMidi(const struct midifile *, uint8_t voices);
extern void __attribute__ ((visibility ("internal"))) midStopMidi(void);
extern uint32_t __attribute__ ((visibility ("internal"))) midGetPosition(void);
extern void __attribute__ ((visibility ("internal"))) midSetPosition(uint32_t pos);
extern void __attribute__ ((visibility ("internal"))) midGetChanInfo(uint8_t ch, struct mchaninfo *ci);
extern void __attribute__ ((visibility ("internal"))) midGetRealNoteVol(uint8_t ch, struct mchaninfo2 *ci);
extern void __attribute__ ((visibility ("internal"))) midGetGlobInfo(struct mglobinfo *gi);
extern void __attribute__ ((visibility ("internal"))) midSetMute(int ch, int p);
extern uint8_t __attribute__ ((visibility ("internal"))) midGetMute(uint8_t ch);
extern int __attribute__ ((visibility ("internal"))) midLooped(void);
extern void __attribute__ ((visibility ("internal"))) midSetLoop(int s);
extern int __attribute__ ((visibility ("internal"))) midGetChanSample(unsigned int ch, int16_t *buf, unsigned int len, uint32_t rate, int opt);


/* private API */

extern void __attribute__ ((visibility ("internal"))) gmiClearInst(void);
extern void __attribute__ ((visibility ("internal"))) gmiInsSetup(const struct midifile *);
extern void __attribute__ ((visibility ("internal"))) gmiChanSetup(const struct midifile *mid);
#include "cpiface/cpiface.h"
extern int __attribute__ ((visibility ("internal"))) gmiGetDots(struct notedotsdata *, int);

extern __attribute__ ((visibility ("internal"))) char midInstrumentNames[256][NAME_MAX+1];

extern int __attribute__ ((visibility ("internal"))) midInitUltra(void);
extern int __attribute__ ((visibility ("internal"))) midInitFFF(void);
extern int __attribute__ ((visibility ("internal"))) midInitFreePats(void);
extern int __attribute__ ((visibility ("internal"))) midInitTimidity(void);

extern int __attribute__ ((visibility ("internal")))
	(*loadpatch)( struct minstrument *ins,
	              uint8_t             program,
	              uint8_t            *sampused,
	              struct sampleinfo **smps,
	              uint16_t           *samplenum);

extern int __attribute__ ((visibility ("internal")))
	(*addpatch)( struct minstrument *ins,
	             uint8_t             program,
	             uint8_t             sn,
	             uint8_t             sampnum,
	             struct sampleinfo  *sip,
	             uint16_t           *samplenum);
extern void __attribute__ ((visibility ("internal")))
	(*_midClose)(void);

#endif
