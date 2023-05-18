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

OCP_INTERNAL int sidOpenPlayer (struct ocpfilehandle_t *, struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL int sidNumberOfChips (void);
OCP_INTERNAL int sidNumberOfComments (void);
OCP_INTERNAL int sidNumberOfInfos (void);
OCP_INTERNAL const char *sidInfoString (int i);
OCP_INTERNAL const char *sidCommentString (int i);
OCP_INTERNAL const char *sidFormatString (void);
OCP_INTERNAL const char *sidROMDescKernal (void);
OCP_INTERNAL const char *sidROMDescBasic (void);
OCP_INTERNAL const char *sidROMDescChargen (void);
OCP_INTERNAL const float sidGetCPUSpeed (void);
OCP_INTERNAL const char *sidGetVICIIModelString (void);
OCP_INTERNAL const char *sidGetCIAModelString (void);
OCP_INTERNAL const char *sidChipModel (int i);
OCP_INTERNAL const char *sidTuneStatusString (void);
OCP_INTERNAL const char *sidTuneInfoClockSpeedString (void);
OCP_INTERNAL uint16_t sidChipAddr (int i);
OCP_INTERNAL void sidClosePlayer (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void sidIdle (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void sidStartSong (uint8_t sng);
OCP_INTERNAL uint8_t sidGetSong (void);
OCP_INTERNAL uint8_t sidGetSongs (void);
OCP_INTERNAL char sidGetVideo (void);
OCP_INTERNAL void sidMute (struct cpifaceSessionAPI_t *cpifaceSession, int i, int m);
OCP_INTERNAL void sidGetChanInfo (int i, sidChanInfo &ci);

OCP_INTERNAL int sidGetLChanSample (struct cpifaceSessionAPI_t *cpifaceSession, unsigned int ch, int16_t *s, unsigned int len, uint32_t rate, int opt);
OCP_INTERNAL int sidGetPChanSample (struct cpifaceSessionAPI_t *cpifaceSession, unsigned int ch, int16_t *s, unsigned int len, uint32_t rate, int opt);

#endif
