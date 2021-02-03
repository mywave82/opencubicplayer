/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) '11-'21 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Variables that are needed globally, and even by the very basic libs.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * revision history: (please note changes here)
 *  -ss040613   Stian Skjelstad <stian@nixia.no>
 *    -first release
 */

#include "config.h"
#include "types.h"
#include "console.h"
#include "stuff/poutput.h"

void (*_vga13)(void);
void (*_plSetTextMode)(uint8_t size) = 0;
void (*_plSetBarFont)(void);
void (*_plDisplaySetupTextMode)(void);
const char *(*_plGetDisplayTextModeName)(void);

void (*_displaystr)(uint16_t y, uint16_t x, uint8_t attr, const char *str, uint16_t len);
void (*_displaystrattr)(uint16_t y, uint16_t x, const uint16_t *buf, uint16_t len);
void (*_displayvoid)(uint16_t y, uint16_t x, uint16_t len);

void (*_displaystr_iso8859latin1)(uint16_t y, uint16_t x, uint8_t attr, const char *str, uint16_t len);
void (*_displaystrattr_iso8859latin1)(uint16_t y, uint16_t x, const uint16_t *buf, uint16_t len);

void (*_displaystr_utf8)(uint16_t y, uint16_t x, uint8_t attr, const char *str, uint16_t len);
int  (*_measurestr_utf8)(const char *src, int srclen);


int (*_plSetGraphMode)(int); /* -1 reset, 0 640x480 1 1024x768 */
void (*_gdrawchar)(uint16_t x, uint16_t y, uint8_t c, uint8_t f, uint8_t b);
void (*_gdrawchart)(uint16_t x, uint16_t y, uint8_t c, uint8_t f);
void (*_gdrawcharp)(uint16_t x, uint16_t y, uint8_t c, uint8_t f, void *picp);
void (*_gdrawchar8)(uint16_t x, uint16_t y, uint8_t c, uint8_t f, uint8_t b);
void (*_gdrawchar8t)(uint16_t x, uint16_t y, uint8_t c, uint8_t f);
void (*_gdrawchar8p)(uint16_t x, uint16_t y, uint8_t c, uint8_t f, void *picp);
void (*_gdrawstr)(uint16_t y, uint16_t x, const char *s, uint16_t len, uint8_t f, uint8_t b);
void (*_gupdatestr)(uint16_t y, uint16_t x, const uint16_t *str, uint16_t len, uint16_t *old);
void (*_gupdatepal)(uint8_t color, uint8_t red, uint8_t green, uint8_t blue);
void (*_gflushpal)(void);

int (*_ekbhit)(void);
int (*_egetch)(void);
int (*_validkey)(uint16_t);

void (*_drawbar)(uint16_t x, uint16_t yb, uint16_t yh, uint32_t hgt, uint32_t c);
void (*_idrawbar)(uint16_t x, uint16_t yb, uint16_t yh, uint32_t hgt, uint32_t c);


void (*_Screenshot)(void);
void (*_TextScreenshot)(int scrType);

void (*_setcur)(uint16_t y, uint16_t x) ;
void (*_setcurshape)(uint16_t shape);

int (*_conRestore)(void);
void (*_conSave)(void);

void (*_plDosShell)(void);

unsigned int plScrHeight;
unsigned int plScrWidth;
enum vidType plVidType;
unsigned char plScrType;
int plScrMode;
uint8_t *plVidMem;

int plScrTextGUIOverlay;
void *(*plScrTextGUIOverlayAddBGRA)(unsigned int x, unsigned int y, unsigned int width, unsigned int height, unsigned int pitch, uint8_t *data_bgra);
void (*plScrTextGUIOverlayRemove)(void *handle);
