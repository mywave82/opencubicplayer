#ifndef __PLAYSID_H
#define __PLAYSID_H

struct sidChanInfo {
  unsigned long freq;
  char ad;
  char sr;
  unsigned short pulse;
  unsigned short wave;
  char filtenabled;
  char filttype;
  long leftvol;
  long rightvol;
};


struct sidDigiInfo {
  char l;
  char r;
  sidDigiInfo() { l=r=0; };
};


struct sidTuneInfo;

extern unsigned char __attribute__ ((visibility ("internal"))) sidpOpenPlayer(FILE *);
extern void __attribute__ ((visibility ("internal"))) sidpClosePlayer(void);
extern void __attribute__ ((visibility ("internal"))) sidpIdle(void);
extern void __attribute__ ((visibility ("internal"))) sidpPause(unsigned char p);
extern void __attribute__ ((visibility ("internal"))) sidpSetAmplify(unsigned long amp);
extern void __attribute__ ((visibility ("internal"))) sidpSetVolume(unsigned char vol, signed char bal, signed char pan, unsigned char opt);
extern void __attribute__ ((visibility ("internal"))) sidpGetGlobInfo(sidTuneInfo &si);
extern void __attribute__ ((visibility ("internal"))) sidpStartSong(char sng);
extern void __attribute__ ((visibility ("internal"))) sidpToggleVideo(void);
extern char __attribute__ ((visibility ("internal"))) sidpGetVideo(void);
extern char __attribute__ ((visibility ("internal"))) sidpGetFilter(void);
extern void __attribute__ ((visibility ("internal"))) sidpToggleFilter(void);
extern char __attribute__ ((visibility ("internal"))) sidpGetSIDVersion(void);
extern void __attribute__ ((visibility ("internal"))) sidpToggleSIDVersion(void);
extern void __attribute__ ((visibility ("internal"))) sidpMute(int i, int m);
extern void __attribute__ ((visibility ("internal"))) sidpGetChanInfo(int i, sidChanInfo &ci);
extern void __attribute__ ((visibility ("internal"))) sidpGetDigiInfo(sidDigiInfo &di);

#endif
