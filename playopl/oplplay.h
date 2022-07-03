#ifndef _PLAYOPL_H
#define _PLAYOPL_H 1

extern "C"
{
	struct ocpfilehandle_t;
	struct cpifaceSessionAPI_t;
}
extern void oplClosePlayer(void);
extern int oplOpenPlayer (const char *filename /* needed for detection */, uint8_t *content /* data is stolen */, const size_t len, struct ocpfilehandle_t *file, struct cpifaceSessionAPI_t *cpifaceSession);
extern void oplSetLoop(int);
extern int oplIsLooped(void);
extern void oplPause(uint8_t p);
extern void oplIdle(void);

struct oplChanInfo {
	unsigned long freq; /* Herz */
	unsigned char wave; /* 0 -> 3 */
	int vol; /* 0 -> 63 */
};

struct oplTuneInfo {
	int songs;
	int currentSong;
	char title[64];
	char author[64];
};

extern void oplpGetChanInfo(int i, oplChanInfo &ci);
extern void oplpGetGlobInfo(oplTuneInfo &si);
extern void oplSetSong (int song);
extern void oplMute (struct cpifaceSessionAPI_t *cpifaceSession, int i, int m);

#endif
