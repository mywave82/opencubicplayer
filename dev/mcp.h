#ifndef __MCP_H
#define __MCP_H

struct ocpfilehandle_t;
struct cpifaceSessionAPI_t;

struct sampleinfo
{
	int type;
	void *ptr;
	uint32_t length;
	uint32_t samprate;
	uint32_t loopstart;
	uint32_t loopend;
	uint32_t sloopstart;
	uint32_t sloopend;
};

enum
{
	mcpSampUnsigned=1,
	mcpSampDelta=2,
	mcpSamp16Bit=4,
	mcpSampBigEndian=8,
	mcpSampLoop=16,
	mcpSampBiDi=32,
	mcpSampSLoop=64,
	mcpSampSBiDi=128,
	mcpSampStereo=256,
	mcpSampFloat=512,
	mcpSampRedBits=(int)0x80000000,
	mcpSampRedRate2=0x40000000,
	mcpSampRedRate4=0x20000000,
	mcpSampRedStereo=0x10000000
};

enum
{
	mcpMasterVolume, mcpMasterPanning, mcpMasterBalance, mcpMasterSurround,
	mcpMasterSpeed, mcpMasterPitch, mcpMasterBass, mcpMasterTreble,
	mcpMasterReverb, mcpMasterChorus, mcpMasterPause, mcpMasterFilter,
	mcpMasterAmplify,
	mcpGSpeed,
	mcpCVolume, mcpCPanning, mcpCPanY, mcpCPanZ, mcpCSurround, mcpCPosition,
	mcpCPitch, mcpCPitchFix, mcpCPitch6848, mcpCStop, mcpCReset,
	mcpCBass, mcpCTreble, mcpCReverb, mcpCChorus, mcpCMute, mcpCStatus,
	mcpCInstrument, mcpCLoop, mcpCDirect, mcpCFilterFreq, mcpCFilterRez,
	mcpGTimer, mcpGCmdTimer,
};

extern int mcpReduceSamples(struct sampleinfo *s, int n, long m, int o);
enum
{
	mcpRedAlways16Bit=1,
	mcpRedNoPingPong=2,
	mcpRedGUS=4,
	mcpRedToMono=8,
	mcpRedTo8Bit=16,
	mcpRedToFloat=32
};


enum
{
	mcpGetSampleMono=0, mcpGetSampleStereo=1, mcpGetSampleHQ=2
};

struct mcpDevAPI_t
{
	int (*OpenPlayer)(int, void (*p)(struct cpifaceSessionAPI_t *cpifaceSession), struct ocpfilehandle_t *source_file, struct cpifaceSessionAPI_t *cpifaceSession);
	int (*LoadSamples)(struct sampleinfo* si, int n);
	void (*Idle)(struct cpifaceSessionAPI_t *cpifaceSession);
	void (*ClosePlayer)(void);
};

extern const struct mcpDevAPI_t *mcpDevAPI;

extern unsigned int mcpMixMaxRate;
extern unsigned int mcpMixProcRate;

extern int mcpGetFreq6848(int note);
extern int mcpGetFreq8363(int note);
extern int mcpGetNote6848(int freq);
extern int mcpGetNote8363(int freq);

#endif
