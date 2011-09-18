#ifndef _PLAYOPL_H
#define _PLAYOPL_H 1

extern void oplClosePlayer(void);
extern int __attribute__ ((visibility ("internal"))) oplOpenPlayer(const char *filename);
extern void oplSetLoop(int);
extern int oplIsLooped(void);
extern void oplPause(uint8_t p);
extern void oplSetAmplify(uint32_t amp);
extern void oplSetVolume(uint8_t vol_, int8_t bal_, int8_t pan_, uint8_t opt);
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
extern void oplMute(int i, int m);
extern void oplSetSpeed(uint16_t sp);

#endif
