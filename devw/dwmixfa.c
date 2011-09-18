/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * assembler routines for FPU mixer
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
 *  -kbwhenever Tammo Hinrichs <opencp@gmx.net>
 *    -first release
 *  -ryg990426  Fabian Giesen  <fabian@jdcs.su.nw.schule.de>
 *    -extreeeeem kbchangesapplying+sklavenarbeitverrichting
 *     (was mir angst macht, ich finds nichmal schlimm)
 *  -ryg990504  Fabian Giesen  <fabian@jdcs.su.nw.schule.de>
 *    -added float postprocs, the key to player realtimeruling
 *  -kb990531   Tammo Hinrichs <opengp@gmx.net>
 *    -fixed mono playback
 *    -cubic spline interpolation now works
 *  -ss04????   Stian Skjelstad <stian@nixia.no>
 *    -ported to gcc
 *  -ss040908   Stian Skjelstad <stian@nixia.no>
 *    -made it optimizesafe
 *
 * dominators und doc rooles geiler floating point mixer mit volume ramps
 * (die man gar nicht benutzen kann (kb sagt man kann). und mit
 * ultra-rauschabstand und viel geil interpolation.
 * wir sind besser als ihr...
 */

#include "config.h"
#include "types.h"
#include "devwmixf.h"
#include "dwmixfa.h"

#if defined(I386_ASM)
#  include "dwmixfa_8087.c"
# elif defined(I386_ASM_EMU)
#  include "dwmixfa_8087_emu.c"
# else
#   include "dwmixfa_c.c"
# endif
