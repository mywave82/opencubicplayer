#ifndef _X11_COMMON
#define _X11_COMMON 1

#include <X11/X.h>
#include <X11/Xlib.h>

extern uint32_t x11_palette32[256];
extern uint16_t x11_palette16[256];
extern uint16_t x11_palette15[256];

extern void x11_gupdatepal(unsigned char color, unsigned char _red, unsigned char _green, unsigned char _blue);
extern void x11_gflushpal(void);

extern Display *mDisplay;
extern int mLocalDisplay;
extern int mScreen;
extern int plDepth;


extern int x11_connect(void);
extern void x11_disconnect(void);

#endif
