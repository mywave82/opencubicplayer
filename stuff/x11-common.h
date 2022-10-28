#ifndef _X11_COMMON
#define _X11_COMMON 1

#include <X11/X.h>
#include <X11/Xlib.h>
#include <stdint.h>

extern uint32_t x11_palette32[256];
extern uint16_t x11_palette16[256];
extern uint16_t x11_palette15[256];

void x11_gUpdatePal (uint8_t color, uint8_t _red, uint8_t _green, uint8_t _blue);
void x11_gFlushPal (void);

extern Display *mDisplay;
extern int mLocalDisplay;
extern int mScreen;
extern int x11_depth;

int x11_connect (void);
void x11_disconnect (void);

#endif
