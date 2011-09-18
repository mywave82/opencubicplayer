//
// /home/ms/source/sidplay/libsidplay/emu/RCS/opstruct.h,v
//

#ifndef SIDPLAY1_OPSTRUCT_H
#define SIDPLAY1_OPSTRUCT_H


#include <sidplay/mytypes.h>
#include <sidplay/myendian.h>


typedef sbyte (*ptr2sidFunc)(struct sidOperator *);
typedef uword (*ptr2sidUwordFunc)(struct sidOperator *);
typedef void (*ptr2sidVoidFunc)(struct sidOperator *);

struct sw_storage
{
	uword len;
#if defined(DIRECT_FIXPOINT)
	udword stp;
#else
	udword pnt;
	sword stp;
#endif
};

struct sidOperator
{
	udword SIDfreq;
	uword SIDpulseWidth;
	ubyte SIDctrl;
	ubyte SIDAD, SIDSR;

	sidOperator* carrier;
	sidOperator* modulator;
	bool sync;

	uword pulseIndex, newPulseIndex;
	uword curSIDfreq;
	uword curNoiseFreq;

	ubyte output, outputMask;

	char filtVoiceMask;
	bool filtEnabled;
	filterfloat filtLow, filtRef;
	sbyte filtIO;

	uword gainLeft, gainRight;  // volume in highbyte
	uword gainSource, gainDest;
	uword gainLeftCentered, gainRightCentered;
	bool gainDirec;

	sdword cycleLenCount;
#if defined(DIRECT_FIXPOINT)
	cpuLword cycleLen, cycleAddLen;
#else
	udword cycleAddLenPnt;
	uword cycleLen, cycleLenPnt;
#endif

	ptr2sidFunc outProc;
	ptr2sidVoidFunc waveProc;

#if defined(DIRECT_FIXPOINT)
	cpuLword waveStep, waveStepAdd;
#else
	uword waveStep, waveStepAdd;
	udword waveStepPnt, waveStepAddPnt;
#endif
	uword waveStepOld;
	struct sw_storage wavePre[2];

#if defined(DIRECT_FIXPOINT) && defined(LARGE_NOISE_TABLE)
	cpuLword noiseReg;
#elif defined(DIRECT_FIXPOINT)
	cpuLBword noiseReg;
#else
	udword noiseReg;
#endif
	udword noiseStep, noiseStepAdd;
	ubyte noiseOutput;
	bool noiseIsLocked;

	ubyte ADSRctrl;
	bool gateOnCtrl, gateOffCtrl;
    ptr2sidUwordFunc ADSRproc;

#ifdef SID_FPUENVE
	filterfloat fenveStep, fenveStepAdd;
	udword enveStep;
#elif defined(DIRECT_FIXPOINT)
	cpuLword enveStep, enveStepAdd;
#else
	uword enveStep, enveStepAdd;
	udword enveStepPnt, enveStepAddPnt;
#endif
	ubyte enveVol, enveSusVol;
	uword enveShortAttackCount;
};


#endif  /* SIDPLAY1_OPSTRUCT_H */
