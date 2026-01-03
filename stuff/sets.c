/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Sound settings module
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
#include <stdio.h>
#include "types.h"
#include "boot/plinkman.h"
#include "boot/psetting.h"
#include "stuff/sets.h"
#include "stuff/err.h"

struct settings set;

static int ssInit (const struct configAPI_t *configAPI)
{
  int per;

  per        = configAPI->GetProfileInt2  (configAPI->SoundSec, "sound",         "amplify",  100, 10);
  per        = configAPI->GetProfileInt   (                     "commandline_v", "a",        per, 10);
  set.amp    = (per>=800) ? 511 : (per*64/100);

  per        = configAPI->GetProfileInt2  (configAPI->SoundSec, "sound",         "volume",   100, 10);
  per        = configAPI->GetProfileInt   (                     "commandline_v", "v",        per, 10);
  set.vol    = (per>=100) ? 64 : (per*64/100);

  per        = configAPI->GetProfileInt2  (configAPI->SoundSec, "sound",         "balance",  0, 10);
  per        = configAPI->GetProfileInt   (                     "commandline_v", "b",        per, 10);
  set.bal    = (per>=100) ? 64 : (per<=-100) ? -64 : (per*64/100);

  per        = configAPI->GetProfileInt2  (configAPI->SoundSec, "sound",         "panning",  100, 10);
  per        = configAPI->GetProfileInt   (                     "commandline_v", "p",        per, 10);
  set.pan    = (per>=100) ? 64 : (per<=-100) ? -64 : (per*64/100);

  set.srnd   = configAPI->GetProfileBool2 (configAPI->SoundSec, "sound",         "surround", 0, 0);
  set.srnd   = configAPI->GetProfileBool  (                     "commandline_v", "s",        set.srnd, 1);

  set.filter = configAPI->GetProfileInt2  (configAPI->SoundSec, "sound",         "filter",   1, 10)%3;
  set.filter = configAPI->GetProfileInt   (                     "commandline_v", "f",        set.filter, 10)%3;

  per        = configAPI->GetProfileInt2  (configAPI->SoundSec, "sound",         "reverb",   0, 10);
  per        = configAPI->GetProfileInt   (                     "commandline_v", "r",        per, 10);
  set.reverb = (per>=100) ? 64 : (per<=-100) ? -64 : (per*64/100);

  per        = configAPI->GetProfileInt2  (configAPI->SoundSec, "sound",         "chorus",   0, 10);
  per        = configAPI->GetProfileInt   (                     "commandline_v", "c",        per, 10);
  set.chorus = (per>=100) ? 64 : (per<=-100) ? -64 : (per*64/100);

  set.speed = 256;
  set.pitch = 256;

  return errOk;
}

DLLEXTINFO_CORE_PREFIX struct linkinfostruct dllextinfo = {.name = "sets", .desc = "OpenCP Sound Settings Auxiliary Routines (c) 1994-'26 Niklas Beisert, Tammo Hinrichs", .ver = DLLVERSION, .sortindex = 10, .Init = ssInit};
