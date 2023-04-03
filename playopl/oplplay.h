#ifndef _PLAYOPL_H
#define _PLAYOPL_H 1

extern "C"
{
	struct ocpfilehandle_t;
	struct cpifaceSessionAPI_t;
}

extern void oplClosePlayer (struct cpifaceSessionAPI_t *cpifaceSession);
extern int oplOpenPlayer (const char *filename /* needed for detection */, uint8_t *content /* data is stolen */, const size_t len, struct ocpfilehandle_t *file, struct cpifaceSessionAPI_t *cpifaceSession);
extern void oplSetLoop(int);
extern int oplIsLooped(void);
extern void oplPause(uint8_t p);
extern void oplIdle (struct cpifaceSessionAPI_t *cpifaceSession);

struct oplTuneInfo {
	int songs;
	int currentSong;
	char title[64];
	char author[64];
};

extern void oplpGetGlobInfo(oplTuneInfo &si);
extern void oplSetSong (struct cpifaceSessionAPI_t *cpifaceSession, int song);
extern void oplMute (struct cpifaceSessionAPI_t *cpifaceSession, int i, int m);
extern void OPLChanInit (struct cpifaceSessionAPI_t *cpifaceSession);

struct oplStatus;
extern struct oplStatus oplLastStatus; // Delayed data
extern int              oplLastPos;    // Delayed data

#endif
