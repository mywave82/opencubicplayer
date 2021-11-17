#ifndef __POUTPUT_H
#define __POUTPUT_H

#include "boot/console.h" /* currently from boot/console.h... to be moved later */

#ifndef _CONSOLE_DRIVER

#define vga13() (_vga13())
#define plSetTextMode(x) (_plSetTextMode(x))
void displaychr (const uint16_t y, const uint16_t x, const uint8_t attr, const char chr, const uint16_t len);
#define displayvoid(y, x, len) (_displayvoid(y, x, len))
#define displaystr(y, x, attr, str, len) (_displaystr(y, x, attr, str, len))
#define displaystrattr(y, x, buf, len) (_displaystrattr(y, x, buf, len))
#define displaystr_utf8(y, x, attr, str, len) (_displaystr_utf8(y, x, attr, str, len))
#define measurestr_utf8(str, strlen) (_measurestr_utf8(str, strlen))
#define plSetGraphMode(size) (_plSetGraphMode(size))

/* 8x16 OCP font, front and back color */
#define gdrawchar(x, y, c, f, b) (_gdrawchar(x, y, c, f, b))
/* 8x16 OCP font, front color, picp for background (or zero if no picture present) -  picp needs to be same format/size as plScrLineBytes */
#define gdrawcharp(x, y, c, f, picp) (_gdrawcharp(x, y, c, f, picp))

/* 8x8 OCP font, front and back color */
#define gdrawchar8(x, y, c, f, b) (_gdrawchar8(x, y, c, f, b))
/* 8x8 OCP font, front color, picp for background (or zero if no picture present) -  picp needs to be same format/size as plScrLineBytes */
#define gdrawchar8p(x, y, c, f, picp) (_gdrawchar8p(x, y, c, f, picp))

#define gdrawstr(y, x, attr, s, len) (_gdrawstr(y, x, attr, s, len))
#define gupdatestr(y, x, str, len, old) (_gupdatestr(y, x, str, len, old))
#define drawbar(x, yb, yh, hgt, c) (_drawbar(x, yb, yh, hgt, c))
#define idrawbar(x, yb, yh, hgt, c) (_idrawbar(x, yb, yh, hgt, c))
#define gupdatepal(c,r,g,b) (_gupdatepal(c,r,g,b))
#define gflushpal() (_gflushpal())
#define plDisplaySetupTextmode() (_plDisplaySetupText())
#define plGetDisplayTextModeName() (_plGetDisplayTextModeName())
#define RefreshScreen() (_RefreshScreen())

#define ekbhit() (_ekbhit())
#define egetch() (_egetch())
#define validkey(k) (_validkey(k))

#define setcur(y, x) _setcur(y, x)
#define setcurshape(shape) _setcurshape(shape)
#define conRestore() _conRestore()
#define conSave() _conSave()

#define plDosShell() _plDosShell()

#endif

/* standard functions that can be used to embed in ekbhit and egetch when
 * escaped key-codes are used, or you want to push ready keys (values above 256)
 */
extern void ___push_key(uint16_t);
extern int ___peek_key(void);
extern /*uint16_t*/int ___pop_key(void);
extern void ___setup_key(int(*kbhit)(void), int(*getch)(void));

extern char *convnum(unsigned long num, char *buf, unsigned char radix, unsigned short len, char clip0/*=1*/);
#define _convnum(num,buf,radix,len) convnum(num,buf,radix,len,1)

extern void writenum(uint16_t *buf, unsigned short ofs, unsigned char attr, unsigned long num, unsigned char radix, unsigned short len, char clip0/*=1*/);
#define _writenum(buf, ofs, attr, num, radix, len) writenum(buf, ofs, attr, num, radix, len, 1)
extern void writestring(uint16_t *buf, unsigned short ofs, unsigned char attr, const char *str, unsigned short len);
extern void writestringattr(uint16_t *buf, unsigned short ofs, const uint16_t *str, unsigned short len);
extern void fillstr(uint16_t *buf, const unsigned short ofs, const unsigned char chr, const unsigned char attr, unsigned short len);

enum vidType
{
	vidNorm, /* text-only systems - console, curses etc.. can support setting video-mode */
	vidVESA, /* text and graphical systems.. can support setting video-mode */
	vidModern, /* text and graphical systems.. try to not override video-mode */
};

extern unsigned int plScrHeight;        /* How many textlines can we currently fit. Undefined for wurfel-mode */
extern unsigned int plScrWidth;         /* How many characters can we currently fir on a line */
extern enum vidType plVidType;                  /* vidNorm for textmode only, or vidVESA for graphical support also */
extern unsigned char plScrType;         /* Last set textmode */
extern int plScrMode;                   /* If we are in graphical mode, this value is set to either 13 (for wurfel), 100 for 640x480, 101 for 1024x768, 255 for custom */
extern uint8_t *plVidMem;               /* This points to the current selected bank, and should atleast provide 64k of available bufferspace */
extern int plScrLineBytes;              /* How many bytes does one line from plVidMem use (can be padded) */
extern int plScrLines;                  /* How many graphical lines do we have */

void make_title (const char *part, int escapewarning);
struct settings;
void cpiDrawG1String (struct settings *g1);


extern int plScrTextGUIOverlay;         /* Is text rendered virtually into a framebuffer, AND supports overlays? */

typedef enum {
	_8x8 = 0,
	_8x16 = 1,
	_FONT_MAX = 1
} FontSizeEnum;

struct FontSizeInfo_t
{
	uint8_t w, h;
};
extern const struct FontSizeInfo_t FontSizeInfo[_FONT_MAX+1];

extern FontSizeEnum plCurrentFont; /* Only drivers can change this, and their helper functions can use it */

#ifdef _CONSOLE_DRIVER

extern unsigned char plpalette[256];

extern void generic_gdrawstr(uint16_t y, uint16_t x, uint8_t attr, const char *str, uint16_t len);
extern void generic_gdrawchar8(unsigned short x, unsigned short y, unsigned char c, unsigned char f, unsigned char b);
extern void generic_gdrawchar8p(unsigned short x, unsigned short y, unsigned char c, unsigned char f, void *picp);
extern void generic_gdrawchar(unsigned short x, unsigned short y, unsigned char c, unsigned char f, unsigned char b);
extern void generic_gdrawcharp(unsigned short x, unsigned short y, unsigned char c, unsigned char f, void *picp);
extern void generic_gupdatestr(unsigned short y, unsigned short x, const uint16_t *str, unsigned short len, uint16_t *old);
#endif

#endif
