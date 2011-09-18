/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * Variables for wavetable system
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
 *  -nb980510   Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *    -first release
 */

#include "config.h"
#include "types.h"
#include "mcp.h"

int mcpNChan;

void (*mcpIdle)(void);

int (*mcpLoadSamples)(struct sampleinfo* si, int n);
int (*mcpOpenPlayer)(int, void (*p)(void));
void (*mcpClosePlayer)(void);
void (*mcpSet)(int ch, int opt, int val);
int (*mcpGet)(int ch, int opt);
void (*mcpGetRealVolume)(int ch, int *l, int *r);
void (*mcpGetRealMasterVolume)(int *l, int *r);
void (*mcpGetMasterSample)(int16_t *s, unsigned int len, uint32_t rate, int opt);
int (*mcpGetChanSample)(unsigned int ch, int16_t *s, unsigned int len, uint32_t rate, int opt);
int (*mcpMixChanSamples)(unsigned int *ch, unsigned int n, int16_t *s, unsigned int len, uint32_t rate, int opt);

unsigned int mcpMixMaxRate;
unsigned int mcpMixProcRate;
int mcpMixOpt;
unsigned int mcpMixBufSize;
unsigned int mcpMixMax;
int mcpMixPoll;
