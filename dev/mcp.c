/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
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
int (*mcpOpenPlayer)(int, void (*p)(void), struct ocpfilehandle_t *source_file, struct cpifaceSessionAPI_t *cpifaceSession);
void (*mcpClosePlayer)(void);
void (*mcpSet)(int ch, int opt, int val);
int (*mcpGet)(int ch, int opt);
void (*mcpGetRealVolume)(int ch, int *l, int *r);
void (*mcpGetMasterSample)(int16_t *s, unsigned int len, uint32_t rate, int opt);
int (*mcpGetChanSample) (struct cpifaceSessionAPI_t *cpifaceSession, unsigned int ch, int16_t *s, unsigned int len, uint32_t rate, int opt);
int (*mcpMixChanSamples) (struct cpifaceSessionAPI_t *cpifaceSession, unsigned int *ch, unsigned int n, int16_t *s, unsigned int len, uint32_t rate, int opt);

unsigned int mcpMixMaxRate;
unsigned int mcpMixProcRate;
