#ifndef __POUTPUT_H
#define __POUTPUT_H

#include "boot/console.h" /* currently from boot/console.h... to be moved later */

#ifndef _CONSOLE_DRIVER

#define vga13() (_vga13())
#define plSetTextMode(x) (_plSetTextMode(x))
#define plSetBarFont(x) (_plSetBarFont(x))
#define displaystr(y, x, attr, str, len) (_displaystr(y, x, attr, str, len))
#define displaystrattr(y, x, buf, len) (_displaystrattr(y, x, buf, len))
#define displaystrattrdi(y, x, txt, attr, len) (_displaystrattrdi(y, x, txt, attr, len))
#define displayvoid(y, x, len) (_displayvoid(y, x, len))
#define plSetGraphMode(size) (_plSetGraphMode(size))
#define gdrawchar(x, y, c, f, b) (_gdrawchar(x, y, c, f, b))
#define gdrawchart(x, y, c, f) (_gdrawchart(x, y, c, f))
#define gdrawcharp(x, y, c, f, picp) (_gdrawcharp(x, y, c, f, picp))
#define gdrawchar8(x, y, c, f, b) (_gdrawchar8(x, y, c, f, b))
#define gdrawchar8t(x, y, c, f) (_gdrawchar8t(x, y, c, f))
#define gdrawchar8p(x, y, c, f, picp) (_gdrawchar8p(x, y, c, f, picp))
#define gdrawstr(y, x, s, len, f, b) (_gdrawstr(y, x, s, len, f, b))
#define gupdatestr(y, x, str, len, old) (_gupdatestr(y, x, str, len, old))
#define drawbar(x, yb, yh, hgt, c) (_drawbar(x, yb, yh, hgt, c))
#define idrawbar(x, yb, yh, hgt, c) (_idrawbar(x, yb, yh, hgt, c))
#define gupdatepal(c,r,g,b) (_gupdatepal(c,r,g,b))
#define gflushpal() (_gflushpal())
#define plDisplaySetupTextmode() (_plDisplaySetupText())
#define plGetDisplayTextModeName() (_plGetDisplayTextModeName())
#define Screenshot() (_Screenshot())
#define TextScreenshot(scrType) (_TextScreenshot(scrType))
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
extern void markstring(uint16_t *buf, unsigned short ofs, unsigned short len);
extern void fillstr(uint16_t *buf, const unsigned short ofs, const unsigned char chr, const unsigned char attr, unsigned short len);


enum { vidNorm, vidVESA};

extern unsigned int plScrHeight;               /* How many textlines can we currently fit. Undefined for wurfel-mode */
extern unsigned int plScrWidth;                /* How many characters can we currently fir on a line */
extern char plVidType;                  /* vidNorm for textmode only, or vidVESA for graphical support also */
extern unsigned char plScrType;         /* Last set textmode */
extern int plScrMode;                   /* If we are in graphical mode, this value is set to either 13 (for wurfel), 100 for 640x480 or 101 for 1024x768 */
extern char *plVidMem;                  /* This points to the current selected bank, and should atleast provide 64k of available bufferspace */
extern int plScrLineBytes;              /* How many bytes does one line from plVidMem use (can be padded) */
extern int plScrLines;                  /* How many graphical lines do we have, should always be 480 or 768, but can be padded */
extern void make_title(char *part);

#ifdef _CONSOLE_DRIVER
extern unsigned char plpalette[256];

extern void generic_gdrawstr(unsigned short y, unsigned short x, const char *str, unsigned short len, unsigned char f, unsigned char b);
extern void generic_gdrawchar8(unsigned short x, unsigned short y, unsigned char c, unsigned char f, unsigned char b);
extern void generic_gdrawchar8t(unsigned short x, unsigned short y, unsigned char c, unsigned char f);
extern void generic_gdrawchar8p(unsigned short x, unsigned short y, unsigned char c, unsigned char f, void *picp);
extern void generic_gdrawstr(unsigned short y, unsigned short x, const char *str, unsigned short len, unsigned char f, unsigned char b);
extern void generic_gdrawcharp(unsigned short x, unsigned short y, unsigned char c, unsigned char f, void *picp);
extern void generic_gdrawchar(unsigned short x, unsigned short y, unsigned char c, unsigned char f, unsigned char b);
extern void generic_gupdatestr(unsigned short y, unsigned short x, const uint16_t *str, unsigned short len, uint16_t *old);
#endif

#endif
