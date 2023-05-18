/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * CPIFace output routines / key handlers for the MCP system
 */

#ifndef __MCPEDIT_H
#define __MCPEDIT_H

struct cpifaceSessionAPI_t;

/* For the sliding pause effect, range 64 = normal speed, 1 = almost complete stop.
 * For complete stop with wavetable use mcpSet (-1, mcpMasterPause, 1) and for stream playback the stream has to send zero-data
 */
OCP_INTERNAL void mcpSetMasterPauseFadeParameters (struct cpifaceSessionAPI_t *cpifaceSession, int i);

/* manipulates SetMasterPauseFadeParameters() and InPause */
OCP_INTERNAL void mcpTogglePauseFade (struct cpifaceSessionAPI_t *cpifaceSession);

/* manipulates SetMasterPauseFadeParameters() and InPause */
OCP_INTERNAL void mcpTogglePause (struct cpifaceSessionAPI_t *cpifaceSession);

enum mcpNormalizeType; /* cpiface.h */
OCP_INTERNAL void mcpNormalize (struct cpifaceSessionAPI_t *cpifaceSession, enum mcpNormalizeType Type);

OCP_INTERNAL void mcpDoPauseFade (struct cpifaceSessionAPI_t *cpifaceSession);

#endif
