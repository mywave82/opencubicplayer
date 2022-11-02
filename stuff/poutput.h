#ifndef __POUTPUT_H
#define __POUTPUT_H

#include "boot/console.h"

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

struct consoleDriver_t
{
	void (*vga13)(void); /* Used by würfel-mode only */

	void (*SetTextMode)(uint8_t x); /* configures text-mode */
		/* index  text   font  physical
		 *  0     80x25  8x16   640x400
		 *  1     80x30  8x16   640x480
		 *  2     80x50  8x8    640x400
		 *  3     80x60  8x8    640x480
		 *  4    128x48  8x16  1024x768
		 *  5    160x64  8x16  1280x1024
		 *  6    128x96  8x8   1024x768
		 *  7    160x128 8x8   1280x1024
		 *  8 custom window
		 * 255 = close window / release keyboard (formed gdb helper, and called during shutdown */

	void        (*DisplaySetupTextMode)  (void); /* Used by ALT-C, lets the driver configure itself, usually font-size - Driver can assume that TextMode is active */
	const char *(*GetDisplayTextModeName)(void); /* Used by ALT-C dialog to resume the current "mode" (e.g. font-size) */

	int  (*MeasureStr_utf8)(const char *src, int srclen);

	void (*DisplayStr_utf8)(uint16_t y, uint16_t x, uint8_t attr, const char *str,                      uint16_t len); /* len = maxlength to use on screen, not input length */
	void (*DisplayChr)     (uint16_t y, uint16_t x, uint8_t attr,       char chr,                       uint16_t len);
	void (*DisplayStr)     (uint16_t y, uint16_t x, uint8_t attr, const char *str,                      uint16_t len);
	void (*DisplayStrAttr) (uint16_t y, uint16_t x,                                const uint16_t *buf, uint16_t len);
	void (*DisplayVoid)    (uint16_t y, uint16_t x,                                                     uint16_t len);
        void (* DrawBar)       (uint16_t x, uint16_t yb, uint16_t yh, uint32_t hgt, uint32_t c);
        void (*iDrawBar)       (uint16_t x, uint16_t yb, uint16_t yh, uint32_t hgt, uint32_t c);

	void *(*TextOverlayAddBGRA)(unsigned int x, unsigned int y, unsigned int width, unsigned int height, unsigned int pitch, uint8_t *data_bgra);
	void  (*TextOverlayRemove) (void *handle);

	int (*SetGraphMode) (int size);
		/* index
		 *  0     640x480
		 *  1    1024x768
		 */
	void (*gDrawChar16) (uint16_t x, uint16_t y, uint8_t c, uint8_t f, uint8_t b);
	void (*gDrawChar16P)(uint16_t x, uint16_t y, uint8_t c, uint8_t f, void *picp);
	void (*gDrawChar8)  (uint16_t x, uint16_t y, uint8_t c, uint8_t f, uint8_t b);
	void (*gDrawChar8P) (uint16_t x, uint16_t y, uint8_t c, uint8_t f, void *picp);
	void (*gDrawStr)    (uint16_t y, uint16_t x, uint8_t attr, const char *str, uint16_t len); /* matches DisplayStr */
	void (*gUpdateStr)  (uint16_t y, uint16_t x, const uint16_t *str, uint16_t len, uint16_t *old);
	void (*gUpdatePal)  (uint8_t color, uint8_t red, uint8_t green, uint8_t blue);
	void (*gFlushPal)   (void);

	int (*HasKey)        (uint16_t);

	void (*SetCursorPosition)(uint16_t y, uint16_t x);
	void (*SetCursorShape)   (uint16_t shape);

	int  (*consoleRestore)(void);
	void (*consoleSave)   (void);

	void (*DosShell)(void);
};

enum vidType
{
	vidNorm, /* text-only systems - console, curses etc.. can support setting video-mode */
	vidVESA, /* text and graphical systems.. can support setting video-mode */
	vidModern, /* text and graphical systems.. try to not override video-mode */
};

struct consoleStatus_t
{
	/* console resolutiom */
	unsigned int TextHeight; /* range 25..inifinity */
	unsigned int TextWidth;  /* range 80..CONSOLE_MAX_X  CONSOLE_MAX_X is 1024 */

	int TextGUIOverlay;   /* TextOverlayAddBGRA / TextOverlayRemove  is supported */

	enum vidType VidType; /* Does driver support text, text+graph, text+graph+flag(try to avoid changing size) */

	unsigned int LastTextMode;  /* Last set TextMode (left as is if entering graph mode */
	unsigned int CurrentMode; /* Last set GraphMode, most drivers merges including graphic modes (100 = LoRes, 101 = HiRes, 13=vga13() */

	uint8_t *VidMem; /* framebuffer used by Graph, Scope etc. when in graph mode*/
	unsigned int GraphBytesPerLine; /* How many bytes does one line from plVidMem use (can be padded) */
	unsigned int GraphLines;     /* How many graphical lines do we have */

	FontSizeEnum CurrentFont; /* Only drivers can change this, and their helper functions can use it. For end-users, its usage is only usefull in combination with plScrTextGUIOverlay API */
};

struct consoleFunctions_t
{
	void (*DisplayPrintf) (uint16_t y, uint16_t x, uint8_t color, uint16_t width, const char *fmt, ...); /* display_nprintf() */

	void (*WriteNum)        (uint16_t *buf, uint16_t ofs, uint8_t attr, unsigned long num, uint8_t radix, uint16_t len, int clip0/*=1*/);
	void (*WriteString)     (uint16_t *buf, uint16_t ofs, uint8_t attr, const char *str, uint16_t len);
	void (*WriteStringAttr) (uint16_t *buf, uint16_t ofs, const uint16_t *str, uint16_t len);

	int  (*KeyboardHit) (void); /* ekbhit */
	int  (*KeyboardGetChar) (void); /* egetch */
	void (*FrameLock) (void);
};
extern const struct consoleFunctions_t conFunc;

/* display_nprintf() behaves a lot like printf(), with some exceptions and additions:
 *
 *   * The final result is always expanded into width
 *   * We support flags - + 0 # and space
 *   * We support width alone  ( %10d )
 *   * We support precision    ( %10.8d )
 *   * %d and %u respects the precision, saturating the digits to all 9
 *   * %c with precision repeats the character
 *   * %C behaves the same a %c, except it does not pop from arguments, but from fmt string instead
 *   * We support *  (pop an integer from the list)
 *   * %n.mo to change colors, where n and m are integers, can be omitted or use *
 *   * %s is CP437 string
 *   * %S is UTF-8 string (and space prefix overflows at the left instead of the right)
 *   * We support %d %ld %lld %u %lu %llu %x %lx %llx %X %lX %llX %% %s %c     %o %S
 *
 *   * No support for %f %lf %llf %p %n
 */
void display_nprintf (uint16_t y, uint16_t x, uint8_t color, uint16_t width, const char *fmt, ...);

#ifndef _CONSOLE_DRIVER

#define vga13                                   conDriver->vga13
#define plSetTextMode(x)                        conDriver->SetTextMode(x)
#define plDisplaySetupTextMode()                conDriver->DisplaySetupTextMode()
#define plGetDisplayTextModeName()              conDriver->GetDisplayTextModeName()
#define measurestr_utf8(s,l)                    conDriver->MeasureStr_utf8(s,l)
#define displaystr_utf8(y,x,a,s,l)              conDriver->DisplayStr_utf8(y,x,a,s,l)
#define displaychr(y,x,a,c,l)                   conDriver->DisplayChr(y,x,a,c,l)
#define displaystr(y,x,a,s,l)                   conDriver->DisplayStr(y,x,a,s,l)
#define displaystrattr(y,x,b,l)                 conDriver->DisplayStrAttr(y,x,b,l)
#define displayvoid(y,x,l)                      conDriver->DisplayVoid(y,x,l)
#define drawbar(x,yb,yh,hgt,c)                  conDriver->DrawBar(x,yb,yh,hgt,c)
#define idrawbar(x,yb,yh,hgt,c)                 conDriver->iDrawBar(x,yb,yh,hgt,c)
#define plScrTextGUIOverlayAddBGRA(x,y,w,h,p,d) conDriver->TextOverlayAddBGRA(x,y,w,h,p,d)
#define plScrTextGUIOverlayRemove(h)            conDriver->TextOverlayRemove(h)
#define plSetGraphMode(s)                       conDriver->SetGraphMode(s)

/* 8x16 OCP font, front and back color */
#define gdrawchar(x,y,c,f,b)                    conDriver->gDrawChar16(x,y,c,f,b)
/* 8x16 OCP font, front color, picp for backgroud (or zero if no picture present) -  picp needs to be same format/size as plScrLineBytes */
#define gdrawcharp(x,y,c,f,picp)                conDriver->gDrawChar16P(x,y,c,f,picp)
/* 8x8 OCP font, front and back color */
#define gdrawchar8(x,y,c,f,b)                   conDriver->gDrawChar8(x,y,c,f,b)
/* 8x8 OCP font, front color, picp for background (or zero if no picture present) -  picp needs to be same format/size as plScrLineBytes */
#define gdrawchar8p(x,y,c,f,picp)               conDriver->gDrawChar8P(x,y,c,f,picp)
#define gdrawstr(y,x,a,s,l)                     conDriver->gDrawStr(y,x,a,s,l)
#define gupdatestr(y,x,s,l,old)                 conDriver->gUpdateStr(y,x,s,l,old)
#define gupdatepal(c,r,g,b)                     conDriver->gUpdatePal(c,r,g,b)
#define gflushpal()                             conDriver->gFlushPal()

#define validkey(k)                             conDriver->HasKey(k)

#define setcur(y,x)                             conDriver->SetCursorPosition(y, x)
#define setcurshape(shape)                      conDriver->SetCursorShape(shape)

#define conRestore()                            conDriver->consoleRestore()
#define conSave()                               conDriver->consoleSave()

#define plDosShell()                            conDriver->DosShell()

#define plScrHeight                             conStatus.TextHeight /* How many textlines can we currently fit. Undefined for wurfel-mode */
#define plScrWidth                              conStatus.TextWidth  /* How many characters can we currently fir on a line */
#define plScrTextGUIOverlay                     conStatus.TextGUIOverlay /* Is text rendered virtually into a framebuffer, AND supports overlays? */
#define plVidType                               conStatus.VidType      /* do we support, text, GUI or GUI without real resolutions */
#define plScrType                               conStatus.LastTextMode /* Last set textmode */
#define plScrMode                               conStatus.CurrentMode  /* If we are in graphical mode, this value is set to either 13 (for wurfel), 100 for 640x480, 101 for 1024x768, 255 for custom */
#define plVidMem                                conStatus.VidMem      /* This points to the current selected bank, and should atleast provide 64k of available bufferspace */
#define plScrLineBytes                          conStatus.GraphBytesPerLine /* How many bytes does one line from plVidMem use (can be padded) */
#define plScrLines                              conStatus.GraphLines   /* How many graphical lines do we have */
#define plCurrentFont                           conStatus.CurrentFont

#else

void generic_gdrawstr (uint16_t y, uint16_t x, uint8_t attr, const char *str, uint16_t len);
void generic_gdrawchar8 (uint16_t x, uint16_t y, uint8_t c, uint8_t f, uint8_t b);
void generic_gdrawchar8p (uint16_t x, uint16_t y, uint8_t c, uint8_t f, void *picp);
void generic_gdrawchar (uint16_t x, uint16_t y, uint8_t c, uint8_t f, uint8_t b);
void generic_gdrawcharp (uint16_t x, uint16_t y, uint8_t c, uint8_t f, void *picp);
void generic_gupdatestr (uint16_t y, uint16_t x, const uint16_t *str, uint16_t len, uint16_t *old);

#endif

// TODO move us into API
void writenum (uint16_t *buf, uint16_t ofs, uint8_t attr, unsigned long num, uint8_t radix, uint16_t len, int clip0/*=1*/);
void writestring (uint16_t *buf, uint16_t ofs, uint8_t attr, const char *str, uint16_t len);
void writestringattr (uint16_t *buf, uint16_t ofs, const uint16_t *str, uint16_t len);

void make_title (const char *part, int escapewarning);

extern unsigned char plpalette[256]; /* palette might be overridden via ocp.ini */

#endif
