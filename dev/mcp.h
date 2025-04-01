#ifndef __MCP_H
#define __MCP_H

struct ocpfilehandle_t;
struct cpifaceSessionAPI_t;

enum mcpSamp
{
	mcpSampUnsigned=1,
	mcpSampDelta=2,
	mcpSamp16Bit=4,
	mcpSampBigEndian=8,
	mcpSampLoop=16,
	mcpSampBiDi=32,
	mcpSampSLoop=64,
	mcpSampSBiDi=128,
	mcpSampInterleavedStereo=256,
	mcpSampStereo=1024,
	mcpSampFloat=512,
	mcpSampRedBits=(uint32_t)0x80000000,
	mcpSampRedRate2=(uint32_t)0x40000000,
	mcpSampRedRate4=(uint32_t)0x20000000,
	mcpSampRedStereo=(uint32_t)0x10000000
};

struct sampleinfo
{
	enum mcpSamp type;
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

enum mcpRed
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
	int (*LoadSamples)(struct cpifaceSessionAPI_t *cpifaceSession, struct sampleinfo* si, int n);
	void (*Idle)(struct cpifaceSessionAPI_t *cpifaceSession);
	void (*ClosePlayer)(struct cpifaceSessionAPI_t *cpifaceSession);
	int (*ProcessKey)(uint16_t);
};
extern const struct mcpDevAPI_t *mcpDevAPI;

struct mcpAPI_t
{
	unsigned int MixMaxRate;
	unsigned int MixProcRate;

	int (*GetFreq6848) (int note);
	int (*GetFreq8363) (int note);
	int (*GetNote6848) (unsigned int freq);
	int (*GetNote8363) (unsigned int freq);
	int (*ReduceSamples) (struct sampleinfo *s, int n, long m, enum mcpRed);
};
extern const struct mcpAPI_t *mcpAPI;

#endif
