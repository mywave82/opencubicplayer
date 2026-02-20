/* OpenCP Module Player
 * copyright (c) '94-'98 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * CPIFace background picture loader
 *
 * revision history: (please note changes here)
 * -doj980928  Dirk Jagdmann <doj@cubic.org>
 *   -initial release of this file
 */

#ifndef CPIPIC__H
#define CPIPIC__H

extern unsigned char *plOpenCPPict; /* an array containing the raw picture */
extern unsigned char plOpenCPPal[]; /* the palette for the picture */

struct configAPI_t;
struct dirdbAPI_t;

void plReadOpenCPPic (const struct configAPI_t *configAPI, const struct dirdbAPI_t *dirdb); /* load a new background picture into *plOpenCPPict and *plOpenCPPal */

void plOpenCPPicInit (const struct configAPI_t *configAPI, const struct dirdbAPI_t *dirdb);
void plOpenCPPicDone (void);

#endif
