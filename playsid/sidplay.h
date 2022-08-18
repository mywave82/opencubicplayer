#ifndef __PLAYSID_H
#define __PLAYSID_H

struct sidChanInfo
{
	uint16_t freq;
	uint16_t pulse;
	uint8_t wave;
	uint8_t ad;
	uint8_t sr;
	int filtenabled;
	uint8_t filttype;
	uint16_t leftvol, rightvol;
};

struct ocpfilehandle_t;
struct cpifaceSessionAPI_t;
extern unsigned char __attribute__ ((visibility ("internal"))) sidOpenPlayer(struct ocpfilehandle_t *, struct cpifaceSessionAPI_t *cpifaceSession);
extern int __attribute__ ((visibility ("internal"))) sidNumberOfChips(void);
extern int __attribute__ ((visibility ("internal"))) sidNumberOfComments(void);
extern int __attribute__ ((visibility ("internal"))) sidNumberOfInfos(void);
extern const char __attribute__ ((visibility ("internal"))) *sidInfoString(int i);
extern const char __attribute__ ((visibility ("internal"))) *sidCommentString(int i);
extern const char __attribute__ ((visibility ("internal"))) *sidFormatString(void);
extern const char __attribute__ ((visibility ("internal"))) *sidROMDescKernal(void);
extern const char __attribute__ ((visibility ("internal"))) *sidROMDescBasic(void);
extern const char __attribute__ ((visibility ("internal"))) *sidROMDescChargen(void);
extern const float __attribute__ ((visibility ("internal"))) sidGetCPUSpeed(void);
extern const char __attribute__ ((visibility ("internal"))) *sidGetVICIIModelString(void);
extern const char __attribute__ ((visibility ("internal"))) *sidGetCIAModelString(void);
extern const char __attribute__ ((visibility ("internal"))) *sidChipModel(int i);
extern const char __attribute__ ((visibility ("internal"))) *sidTuneStatusString(void);
extern const char __attribute__ ((visibility ("internal"))) *sidTuneInfoClockSpeedString(void);
extern uint16_t __attribute__ ((visibility ("internal"))) sidChipAddr(int i);
extern void __attribute__ ((visibility ("internal"))) sidClosePlayer (struct cpifaceSessionAPI_t *cpifaceSession);
extern void __attribute__ ((visibility ("internal"))) sidIdle (struct cpifaceSessionAPI_t *cpifaceSession);
extern void __attribute__ ((visibility ("internal"))) sidPause(unsigned char p);
extern void __attribute__ ((visibility ("internal"))) sidStartSong(uint8_t sng);
extern uint8_t __attribute__ ((visibility ("internal"))) sidGetSong(void);
extern uint8_t __attribute__ ((visibility ("internal"))) sidGetSongs(void);
extern char __attribute__ ((visibility ("internal"))) sidGetVideo(void);
extern void __attribute__ ((visibility ("internal"))) sidMute (struct cpifaceSessionAPI_t *cpifaceSession, int i, int m);
extern void __attribute__ ((visibility ("internal"))) sidGetChanInfo(int i, sidChanInfo &ci);

int __attribute__ ((visibility ("internal"))) sidGetLChanSample (struct cpifaceSessionAPI_t *cpifaceSession, unsigned int ch, int16_t *s, unsigned int len, uint32_t rate, int opt);
int __attribute__ ((visibility ("internal"))) sidGetPChanSample (struct cpifaceSessionAPI_t *cpifaceSession, unsigned int ch, int16_t *s, unsigned int len, uint32_t rate, int opt);

#endif
