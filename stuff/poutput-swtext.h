#ifndef _STUFF_POUTPUT_SWTEXT_H
#define _STUFF_POUTPUT_SWTEXT_H

/* These renders to plVidMem, so this virtual buffer need to be render-buffer for these helper functions to be used
 */

#if 0
/* codepage is optional, to translate...*/
void swtext_displaystrattr_cpfont_8x8(uint16_t y, uint16_t x, const uint16_t *buf, uint16_t len, const uint8_t *codepage);

void swtext_displaystr_cpfont_8x8(uint16_t y, uint16_t x, uint8_t attr, const char *str, uint16_t len, const uint8_t *codepage);
#endif

void swtext_displayvoid(uint16_t y, uint16_t x, uint16_t len);

void swtext_displaystrattr_cp437(uint16_t y, uint16_t x, const uint16_t *buf, uint16_t len);

void swtext_displaystr_cp437(uint16_t y, uint16_t x, uint8_t attr, const char *str, uint16_t len);

void swtext_displaystr_utf8(uint16_t y, uint16_t x, uint8_t attr, const char *str, uint16_t len);

int swtext_measurestr_utf8 (const char *src, int srclen);

void swtext_drawbar(uint16_t x, uint16_t yb, uint16_t yh, uint32_t hgt, uint32_t c);

void swtext_idrawbar(uint16_t x, uint16_t yb, uint16_t yh, uint32_t hgt, uint32_t c);

void swtext_setcur(uint16_t y, uint16_t x);

void swtext_setcurshape(uint16_t shape);

void swtext_cursor_inject(void);

void swtext_cursor_eject(void);

/* only used by fontdebug */
void swtext_displaycharattr_single8x8(uint16_t y, uint16_t x, uint8_t *cp, uint8_t attr);
void swtext_displaycharattr_double8x8(uint16_t y, uint16_t x, uint8_t *cp, uint8_t attr);
void swtext_displaycharattr_single8x16(uint16_t y, uint16_t x, uint8_t *cp, uint8_t attr);
void swtext_displaycharattr_double8x16(uint16_t y, uint16_t x, uint8_t *cp, uint8_t attr);


#endif
