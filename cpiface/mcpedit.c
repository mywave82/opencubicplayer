/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * CPIFace output routines / key handlers for the MCP system
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
 *  -fd981119   Felix Domke <tmbinc@gmx.net>
 *    -added the really important 'NO_CPIFACE_IMPORT'
 */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "dev/deviplay.h"
#include "dev/deviwave.h"
#include "dev/player.h"
#include "dev/mcp.h"
#include "boot/psetting.h"
#include "stuff/poutput.h"
#include "stuff/sets.h"
#include "cpiface.h"
#include "cpiface-private.h"

static int finespeed=8;

void mcpNormalize (struct cpifaceSessionAPI_t *cpifaceSession, enum mcpNormalizeType Type)
{
	struct cpifaceSessionPrivate_t *f = (struct cpifaceSessionPrivate_t *)cpifaceSession;

	f->mcpType = Type;
	f->mcpset = set;
	f->MasterPauseFadeParameter = 64;

	if (!(f->mcpType & mcpNormalizeCanSpeedPitchUnlock))
	{
		f->mcpset.speed = f->mcpset.pitch;
		f->mcpset.splock = 1;
	}
	if (!(f->mcpType & mcpNormalizeCanEcho))
	{
		f->mcpset.viewfx = 0;
		//f->mcpset.useecho = 0;
	}

	cpifaceSession->mcpSet(-1, mcpMasterAmplify,  256*f->mcpset.amp);
	cpifaceSession->mcpSet(-1, mcpMasterVolume,   f->mcpset.vol);
	cpifaceSession->mcpSet(-1, mcpMasterBalance,  f->mcpset.bal);
	cpifaceSession->mcpSet(-1, mcpMasterPanning,  f->mcpset.pan);
	cpifaceSession->mcpSet(-1, mcpMasterSurround, f->mcpset.srnd);
	cpifaceSession->mcpSet(-1, mcpMasterPitch,    f->mcpset.pitch);
	cpifaceSession->mcpSet(-1, mcpMasterSpeed,    f->mcpset.speed);
	cpifaceSession->mcpSet(-1, mcpMasterReverb,   f->mcpset.reverb);
	cpifaceSession->mcpSet(-1, mcpMasterChorus,   f->mcpset.chorus);
	if (f->mcpType & mcpNormalizeCanEcho)
	{
		cpifaceSession->mcpSet(-1, mcpMasterFilter, f->mcpset.filter);
	} else {
		cpifaceSession->mcpSet(-1, mcpMasterFilter, 0);
	}
}

int mcpSetProcessKey (struct cpifaceSessionPrivate_t *f, uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('-', "Decrease volume");
			cpiKeyHelp('+', "Increase volume");
			cpiKeyHelp('/', "Fade balance left");
			cpiKeyHelp('*', "Fade balance right");
			cpiKeyHelp(',', "Fade panning against normal");
			cpiKeyHelp('.', "Fade panning against reverse");
			cpiKeyHelp(KEY_F(2), "Decrease volume (faster)");
			cpiKeyHelp(KEY_F(3), "Increase volume (faster)");
			cpiKeyHelp(KEY_F(4), "Toggle surround on/off");
			cpiKeyHelp(KEY_F(5), "Fade balance left (faster)");
			cpiKeyHelp(KEY_F(6), "Fade balance right (faster)");
			cpiKeyHelp(KEY_F(7), "Fade panning against normal (faster)");
			cpiKeyHelp(KEY_F(8), "Fade panning against reverse (faster)");
			cpiKeyHelp(KEY_F(9), "Decrease speed (fine)");
			cpiKeyHelp(KEY_F(10), "Increase speed (fine)");
			cpiKeyHelp(KEY_F(11), "Decrease pitch (fine)");
			cpiKeyHelp(KEY_F(12), "Increase pitch (fine)");
			if (f->mcpType & mcpNormalizeCanSpeedPitchUnlock)
			{
				cpiKeyHelp(KEY_CTRL_F(12), "Toggle lock between pitch/speed");
				cpiKeyHelp('\\', "Toggle lock between pitch/speed");
			}
			cpiKeyHelp(KEY_CTRL_F(11), "Toggle between fine/course speed/pitch control");
			if (f->mcpType & mcpNormalizeCanAmplify)
			{
				cpiKeyHelp (KEY_SHIFT_F(2), "Decrease amplification");
				cpiKeyHelp (KEY_SHIFT_F(3), "Increase amplification");
			}
			if (f->mcpType & mcpNormalizeCanEcho)
			{
				cpiKeyHelp (KEY_SHIFT_F(4), "Toggle view volume vs echo");
				cpiKeyHelp (KEY_SHIFT_F(5), "Decrease reverb");
				cpiKeyHelp (KEY_SHIFT_F(6), "Increase reverb");
				cpiKeyHelp (KEY_SHIFT_F(7), "Decrease chorus");
				cpiKeyHelp (KEY_SHIFT_F(8), "Increase chorus");
			}
			cpiKeyHelp(KEY_CTRL_SHIFT_F(2), "`Save` the current configuration");
			cpiKeyHelp(KEY_CTRL_SHIFT_F(3), "`Load` configuration");
			cpiKeyHelp(KEY_CTRL_SHIFT_F(4), "`Reset` configuration");
			cpiKeyHelp(KEY_BACKSPACE, "Cycle mixer-filters");
			if (f->Public.plrActive && f->Public.plrDevAPI->ProcessKey)
			{
				f->Public.plrDevAPI->ProcessKey (key);
			}
			if (f->Public.mcpActive && f->Public.mcpDevAPI->ProcessKey)
			{
				f->Public.mcpDevAPI->ProcessKey (key);
			}
			return 0;
		case '-':
			if (f->mcpset.vol>=2)
				f->mcpset.vol-=2;
			f->Public.mcpSet(-1, mcpMasterVolume, f->mcpset.vol * f->MasterPauseFadeParameter / 64);
			break;
		case '+':
			if (f->mcpset.vol<=62)
				f->mcpset.vol+=2;
			f->Public.mcpSet(-1, mcpMasterVolume, f->mcpset.vol * f->MasterPauseFadeParameter / 64);
			break;
		case '/':
			if ((f->mcpset.bal-=4)<-64)
				f->mcpset.bal=-64;
			f->Public.mcpSet(-1, mcpMasterBalance, f->mcpset.bal);
			break;
		case '*':
			if ((f->mcpset.bal+=4)>64)
				f->mcpset.bal=64;
			f->Public.mcpSet(-1, mcpMasterBalance, f->mcpset.bal);
			break;
		case ',':
			if ((f->mcpset.pan-=4)<-64)
				f->mcpset.pan=-64;
			f->Public.mcpSet(-1, mcpMasterPanning, f->mcpset.pan);
			break;
		case '.':
			if ((f->mcpset.pan+=4)>64)
				f->mcpset.pan=64;
			f->Public.mcpSet(-1, mcpMasterPanning, f->mcpset.pan);
			break;
		/*case 0x3c00: //f2*/
		case KEY_F(2):
			if ((f->mcpset.vol-=8)<0)
				f->mcpset.vol=0;
			f->Public.mcpSet(-1, mcpMasterVolume, f->mcpset.vol * f->MasterPauseFadeParameter / 64);
			break;
		/*case 0x3d00: //f3*/
		case KEY_F(3):
			if ((f->mcpset.vol+=8)>64)
				f->mcpset.vol=64;
			f->Public.mcpSet(-1, mcpMasterVolume, f->mcpset.vol * f->MasterPauseFadeParameter / 64);
			break;
		/*case 0x3e00: //f4*/
		case KEY_F(4):
			f->Public.mcpSet(-1, mcpMasterSurround, f->mcpset.srnd=!f->mcpset.srnd);
			break;
		/*case 0x3f00: //f5*/
		case KEY_F(5):
			if ((f->mcpset.pan-=16)<-64)
				f->mcpset.pan=-64;
			f->Public.mcpSet(-1, mcpMasterPanning, f->mcpset.pan);
			break;
		/*case 0x4000: //f6*/
		case KEY_F(6):
			if ((f->mcpset.pan+=16)>64)
				f->mcpset.pan=64;
			f->Public.mcpSet(-1, mcpMasterPanning, f->mcpset.pan);
			break;
		/*case 0x4100: //f7*/
		case KEY_F(7):
			if ((f->mcpset.bal-=16)<-64)
				f->mcpset.bal=-64;
			f->Public.mcpSet(-1, mcpMasterBalance, f->mcpset.bal);
			break;
		/*case 0x4200: //f8*/
		case KEY_F(8):
			if ((f->mcpset.bal+=16)>64)
				f->mcpset.bal=64;
			f->Public.mcpSet(-1, mcpMasterBalance, f->mcpset.bal);
			break;
		/*case 0x4300: //f9*/
		case KEY_F(9):
			if ((f->mcpset.speed-=finespeed)<16)
				f->mcpset.speed=16;
			f->Public.mcpSet(-1, mcpMasterSpeed, f->mcpset.speed * f->MasterPauseFadeParameter / 64);
			if (f->mcpset.splock)
				f->Public.mcpSet(-1, mcpMasterPitch, (f->mcpset.pitch=f->mcpset.speed) * f->MasterPauseFadeParameter / 64);
			break;
		/*case 0x4400: //f10*/
		case KEY_F(10):
			if ((f->mcpset.speed+=finespeed)>2048)
				f->mcpset.speed=2048;
			f->Public.mcpSet(-1, mcpMasterSpeed, f->mcpset.speed * f->MasterPauseFadeParameter / 64);
			if (f->mcpset.splock)
				f->Public.mcpSet(-1, mcpMasterPitch, (f->mcpset.pitch=f->mcpset.speed) * f->MasterPauseFadeParameter / 64);
			break;
		/*case 0x8500: //f11*/
		case KEY_F(11):
			if ((f->mcpset.pitch-=finespeed)<16)
				f->mcpset.pitch=16;
			f->Public.mcpSet(-1, mcpMasterPitch, (f->mcpset.pitch) * f->MasterPauseFadeParameter / 64);
			if (f->mcpset.splock)
				f->Public.mcpSet(-1, mcpMasterSpeed, (f->mcpset.speed=f->mcpset.pitch) * f->MasterPauseFadeParameter  / 64);
			break;
		/*case 0x8600: //f12*/
		case KEY_F(12):
			if ((f->mcpset.pitch+=finespeed)>2048)
				f->mcpset.pitch=2048;
			f->Public.mcpSet(-1, mcpMasterPitch, f->mcpset.pitch * f->MasterPauseFadeParameter / 64);
			if (f->mcpset.splock)
				f->Public.mcpSet(-1, mcpMasterSpeed, (f->mcpset.speed=f->mcpset.pitch) * f->MasterPauseFadeParameter / 64);
			break;

		case KEY_SHIFT_F(2):
			if (f->mcpType & mcpNormalizeCanAmplify)
			{
				if ((f->mcpset.amp-=4)<4)
					f->mcpset.amp=4;
				f->Public.mcpSet(-1, mcpMasterAmplify, 256*f->mcpset.amp);
			}
			break;
		case KEY_SHIFT_F(3):
			if (f->mcpType & mcpNormalizeCanAmplify)
			{
				if ((f->mcpset.amp+=4)>508)
					f->mcpset.amp=508;
				f->Public.mcpSet(-1, mcpMasterAmplify, 256*f->mcpset.amp);
			}
			break;
		case KEY_SHIFT_F(4):
			if (f->mcpType & mcpNormalizeCanEcho)
			{
				f->mcpset.viewfx^=1;
			}
			break;
		case KEY_SHIFT_F(5):
			if (f->mcpType & mcpNormalizeCanEcho)
			{
				if ((f->mcpset.reverb-=2)<0)
					f->mcpset.reverb=0;
				f->Public.mcpSet(-1, mcpMasterReverb, f->mcpset.reverb);
			}
			break;
		case KEY_SHIFT_F(6):
			if (f->mcpType & mcpNormalizeCanEcho)
			{
				if ((f->mcpset.reverb+=2)>64)
					f->mcpset.reverb=64;
				f->Public.mcpSet(-1, mcpMasterReverb, f->mcpset.reverb);
			}
			break;
		case KEY_SHIFT_F(7):
			if (f->mcpType & mcpNormalizeCanEcho)
			{
				if ((f->mcpset.chorus-=2)<0)
					f->mcpset.chorus=0;
				f->Public.mcpSet(-1, mcpMasterChorus, f->mcpset.chorus);
			}
			break;
		case KEY_SHIFT_F(8):
			if (f->mcpType & mcpNormalizeCanEcho)
			{
				if ((f->mcpset.chorus+=2)>64)
					f->mcpset.chorus=64;
				f->Public.mcpSet(-1, mcpMasterChorus, f->mcpset.chorus);
			}
			break;
		case KEY_CTRL_F(11):
			finespeed=(finespeed==8)?1:8;
			break;

		case KEY_CTRL_F(12):
		case '\\':
			if (f->mcpType & mcpNormalizeCanSpeedPitchUnlock)
			{
				f->mcpset.splock^=1;
			}
			break;
		case KEY_BACKSPACE:
			if (f->mcpType & mcpNormalizeFilterAOIFOI)
			{
				set.filter = f->mcpset.filter = (f->mcpset.filter + 1) % 3;
				f->Public.mcpSet(-1, mcpMasterFilter, f->mcpset.filter);
			}
			break;

		case KEY_CTRL_SHIFT_F(2):
			set.pan=f->mcpset.pan;
			set.bal=f->mcpset.bal;
			set.vol=f->mcpset.vol;
			set.speed=f->mcpset.speed;
			set.pitch=f->mcpset.pitch;
			set.amp=f->mcpset.amp;
			set.reverb=f->mcpset.reverb;
			set.chorus=f->mcpset.chorus;
			set.srnd=f->mcpset.srnd;
			break;

		case KEY_CTRL_SHIFT_F(3):
			mcpNormalize (&f->Public, f->mcpType);
			break;

		case KEY_CTRL_SHIFT_F(4):
			f->mcpset.pan=64;
			f->mcpset.bal=0;
			f->mcpset.vol=64;
			f->mcpset.speed=256;
			f->mcpset.pitch=256;
			f->mcpset.chorus=0;
			f->mcpset.reverb=0;
			f->mcpset.amp=64;
			f->Public.mcpSet(-1, mcpMasterAmplify, 256*f->mcpset.amp);
			f->Public.mcpSet(-1, mcpMasterVolume, f->mcpset.vol * f->MasterPauseFadeParameter / 64);
			f->Public.mcpSet(-1, mcpMasterBalance, f->mcpset.bal);
			f->Public.mcpSet(-1, mcpMasterPanning, f->mcpset.pan);
			f->Public.mcpSet(-1, mcpMasterSurround, f->mcpset.srnd);
			f->Public.mcpSet(-1, mcpMasterPitch, f->mcpset.pitch);
			f->Public.mcpSet(-1, mcpMasterSpeed, f->mcpset.speed);
			f->Public.mcpSet(-1, mcpMasterReverb, f->mcpset.reverb);
			f->Public.mcpSet(-1, mcpMasterChorus, f->mcpset.chorus);
			break;

		default:
			if (f->Public.plrActive && f->Public.plrDevAPI->ProcessKey)
			{
				int ret = f->Public.plrDevAPI->ProcessKey (key);
				if (ret == 2)
				{
					cpiResetScreen();
				}
				if (ret)
				{
					return 1;
				}
			}
			if (f->Public.mcpActive && f->Public.mcpDevAPI->ProcessKey)
			{
				int ret = f->Public.mcpDevAPI->ProcessKey (key);
				if (ret==2)
				{
					cpiResetScreen();
				}
				if (ret)
				{
					return 1;
				}
			}

			return 0;
	}
	return 1;
}

void mcpSetMasterPauseFadeParameters (struct cpifaceSessionAPI_t *cpifaceSession, int i)
{
	struct cpifaceSessionPrivate_t *f = (struct cpifaceSessionPrivate_t *)cpifaceSession;

	f->MasterPauseFadeParameter = i;
	cpifaceSession->mcpSet(-1, mcpMasterPitch, f->mcpset.pitch*i/64);
	cpifaceSession->mcpSet(-1, mcpMasterSpeed, f->mcpset.speed*i/64);
	cpifaceSession->mcpSet(-1, mcpMasterVolume, f->mcpset.vol*i/64);
}
