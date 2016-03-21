/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
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
#include "dev/mcp.h"
#include "boot/psetting.h"
#include "stuff/poutput.h"
#include "stuff/sets.h"
#include "cpiface.h"

static int vol;
static int bal;
static int pan;
static int srnd;
static int amp;
uint16_t globalmcpspeed;
uint16_t globalmcppitch;
static int reverb;
static int chorus;
static int splock=1;
static int viewfx=0;
static int finespeed=8;

void mcpNormalize(int hasfilter)
{
	globalmcpspeed=set.speed;
	globalmcppitch=set.pitch;
	pan=set.pan;
	bal=set.bal;
	vol=set.vol;
	amp=set.amp;
	srnd=set.srnd;
	reverb=set.reverb;
	chorus=set.chorus;
	mcpSet(-1, mcpMasterAmplify, 256*amp);
	mcpSet(-1, mcpMasterVolume, vol);
	mcpSet(-1, mcpMasterBalance, bal);
	mcpSet(-1, mcpMasterPanning, pan);
	mcpSet(-1, mcpMasterSurround, srnd);
	mcpSet(-1, mcpMasterPitch, globalmcppitch);
	mcpSet(-1, mcpMasterSpeed, globalmcpspeed);
	mcpSet(-1, mcpMasterReverb, reverb);
	mcpSet(-1, mcpMasterChorus, chorus);
	if (hasfilter)
		mcpSet(-1, mcpMasterFilter, set.filter);
	else
		mcpSet(-1, mcpMasterFilter, 0);
}

void mcpDrawGStrings(uint16_t (*buf)[CONSOLE_MAX_X])
{
	memset(buf[0], 0, sizeof(buf[0]));
	memset(buf[1], 0, sizeof(buf[1]));
	if (plScrWidth<128)
	{
		writestring(buf[0], 0, 0x09, " vol: \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa ", 15);
		if (viewfx)
			writestring(buf[0], 15, 0x09, " echo: \xfa  rev: -\xfa\xfa\xfan\xfa\xfa\xfa+  chr: -\xfa\xfa\xfan\xfa\xfa\xfa+ ", 41);
		else
			writestring(buf[0], 15, 0x09, " srnd: \xfa  pan: l\xfa\xfa\xfam\xfa\xfa\xfar  bal: l\xfa\xfa\xfam\xfa\xfa\xfar ", 41);
		writestring(buf[0], 56, 0x09, " spd: ---%  pitch: ---% ", 24);
		if (splock)
			writestring(buf[0], 67, 0x09, "\x1D p", 3);
		writestring(buf[0], 6, 0x0F, "\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe", (vol+4)>>3);
		if (viewfx)
		{
			writestring(buf[0], 22, 0x0F, 0?"x":"o", 1);
			writestring(buf[0], 30+((reverb+70)>>4), 0x0F, "I", 1);
			writestring(buf[0], 46+((chorus+70)>>4), 0x0F, "I", 1);
		} else {
			writestring(buf[0], 22, 0x0F, srnd?"x":"o", 1);
			if (((pan+70)>>4)==4)
				writestring(buf[0], 34, 0x0F, "m", 1);
			else {
				writestring(buf[0], 30+((pan+70)>>4), 0x0F, "r", 1);
				writestring(buf[0], 38-((pan+70)>>4), 0x0F, "l", 1);
			}
			writestring(buf[0], 46+((bal+70)>>4), 0x0F, "I", 1);
		}
		_writenum(buf[0], 62, 0x0F, globalmcpspeed*100/256, 10, 3);
		_writenum(buf[0], 75, 0x0F, globalmcppitch*100/256, 10, 3);

		writestring(buf[1], 58, 0x09, "amp: ...% filter: ... ", 22);
		_writenum(buf[1], 63, 0x0F, amp*100/64, 10, 3);
		writestring(buf[1], 76, 0x0F, (set.filter==1)?"AOI":(set.filter==2)?"FOI":"off", 3);
	} else {
		writestring(buf[0], 0, 0x09, "    volume: \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa  ", 30);
		if (viewfx)
			writestring(buf[0], 30, 0x09, " echoactive: \xfa   reverb: -\xfa\xfa\xfa\xfa\xfa\xfa\xfam\xfa\xfa\xfa\xfa\xfa\xfa\xfa+   chorus: -\xfa\xfa\xfa\xfa\xfa\xfa\xfam\xfa\xfa\xfa\xfa\xfa\xfa\xfa+  ", 72);
		else
			writestring(buf[0], 30, 0x09, " surround: \xfa   panning: l\xfa\xfa\xfa\xfa\xfa\xfa\xfam\xfa\xfa\xfa\xfa\xfa\xfa\xfar   balance: l\xfa\xfa\xfa\xfa\xfa\xfa\xfam\xfa\xfa\xfa\xfa\xfa\xfa\xfar  ", 72);
		writestring(buf[0], 102, 0x09,  " speed: ---%   pitch: ---%    ", 30);
		writestring(buf[0], 12, 0x0F, "\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe", (vol+2)>>2);
		if (viewfx)
		{
			writestring(buf[0], 43, 0x0F, 0?"x":"o", 1);
			writestring(buf[0], 55+((reverb+68)>>3), 0x0F, "I", 1);
			writestring(buf[0], 83+((chorus+68)>>3), 0x0F, "I", 1);
		} else {
			writestring(buf[0], 41, 0x0F, srnd?"x":"o", 1);
			if (((pan+68)>>3)==8)
				writestring(buf[0], 62, 0x0F, "m", 1);
			else {
				writestring(buf[0], 54+((pan+68)>>3), 0x0F, "r", 1);
				writestring(buf[0], 70-((pan+68)>>3), 0x0F, "l", 1);
			}
			writestring(buf[0], 83+((bal+68)>>3), 0x0F, "I", 1);
		}
		_writenum(buf[0], 110, 0x0F, globalmcpspeed*100/256, 10, 3);
		if (splock)
			writestring(buf[0], 115, 0x09, "\x1D", 1);
		_writenum(buf[0], 124, 0x0F, globalmcppitch*100/256, 10, 3);

		writestring(buf[1], 81, 0x09, "              amplification: ...%  filter: ...     ", 52);
		_writenum(buf[1], 110, 0x0F, amp*100/64, 10, 3);
		writestring(buf[1], 124, 0x0F, (set.filter==1)?"AOI":(set.filter==2)?"FOI":"off", 3);
	}
}

int mcpSetProcessKey(uint16_t key)
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
			cpiKeyHelp('\\', "Toggle lock between pitch/speed");
			cpiKeyHelp(KEY_BACKSPACE, "Cycle mixer-filters");
			return 0;
		case '-':
			if (vol>=2)
				vol-=2;
			mcpSet(-1, mcpMasterVolume, vol);
			break;
		case '+':
			if (vol<=62)
				vol+=2;
			mcpSet(-1, mcpMasterVolume, vol);
			break;
		case '/':
			if ((bal-=4)<-64)
				bal=-64;
			mcpSet(-1, mcpMasterBalance, bal);
			break;
		case '*':
			if ((bal+=4)>64)
				bal=64;
			mcpSet(-1, mcpMasterBalance, bal);
			break;
		case ',':
			if ((pan-=4)<-64)
				pan=-64;
			mcpSet(-1, mcpMasterPanning, pan);
			break;
		case '.':
			if ((pan+=4)>64)
				pan=64;
			mcpSet(-1, mcpMasterPanning, pan);
			break;
		/*case 0x3c00: //f2*/
		case KEY_F(2):
			if ((vol-=8)<0)
				vol=0;
			mcpSet(-1, mcpMasterVolume, vol);
			break;
		/*case 0x3d00: //f3*/
		case KEY_F(3):
			if ((vol+=8)>64)
				vol=64;
			mcpSet(-1, mcpMasterVolume, vol);
			break;
		/*case 0x3e00: //f4*/
		case KEY_F(4):
			mcpSet(-1, mcpMasterSurround, srnd=!srnd);
			break;
		/*case 0x3f00: //f5*/
		case KEY_F(5):
			if ((pan-=16)<-64)
				pan=-64;
			mcpSet(-1, mcpMasterPanning, pan);
			break;
		/*case 0x4000: //f6*/
		case KEY_F(6):
			if ((pan+=16)>64)
				pan=64;
			mcpSet(-1, mcpMasterPanning, pan);
			break;
		/*case 0x4100: //f7*/
		case KEY_F(7):
			if ((bal-=16)<-64)
				bal=-64;
			mcpSet(-1, mcpMasterBalance, bal);
			break;
		/*case 0x4200: //f8*/
		case KEY_F(8):
			if ((bal+=16)>64)
				bal=64;
			mcpSet(-1, mcpMasterBalance, bal);
			break;
		/*case 0x4300: //f9*/
		case KEY_F(9):
			if ((globalmcpspeed-=finespeed)<16)
				globalmcpspeed=16;
			mcpSet(-1, mcpMasterSpeed, globalmcpspeed);
			if (splock)
				mcpSet(-1, mcpMasterPitch, globalmcppitch=globalmcpspeed);
			break;
		/*case 0x4400: //f10*/
		case KEY_F(10):
			if ((globalmcpspeed+=finespeed)>2048)
				globalmcpspeed=2048;
			mcpSet(-1, mcpMasterSpeed, globalmcpspeed);
			if (splock)
				mcpSet(-1, mcpMasterPitch, globalmcppitch=globalmcpspeed);
			break;
		/*case 0x8500: //f11*/
		case KEY_F(11):
			if ((globalmcppitch-=finespeed)<16)
				globalmcppitch=16;
			mcpSet(-1, mcpMasterPitch, globalmcppitch);
			if (splock)
				mcpSet(-1, mcpMasterSpeed, globalmcpspeed=globalmcppitch);
			break;
		/*case 0x8600: //f12*/
		case KEY_F(12):
			if ((globalmcppitch+=finespeed)>2048)
				globalmcppitch=2048;
			mcpSet(-1, mcpMasterPitch, globalmcppitch);
			if (splock)
				mcpSet(-1, mcpMasterSpeed, globalmcpspeed=globalmcppitch);
			break;
/* TODO-keys
		case 0x5f00: // ctrl f2
			if ((amp-=4)<4)
				amp=4;
			mcpSet(-1, mcpMasterAmplify, 256*amp);
			break;
		case 0x6000: // ctrl f3
			if ((amp+=4)>508)
				amp=508;
			mcpSet(-1, mcpMasterAmplify, 256*amp);
			break;
		case 0x6100: // ctrl f4
			viewfx^=1;
			break;
		case 0x6200: // ctrl f5
			if ((reverb-=8)<-64)
				reverb=-64;
			mcpSet(-1, mcpMasterReverb, reverb);
			break;
		case 0x6300: // ctrl f6
			if ((reverb+=8)>64)
				reverb=64;
			mcpSet(-1, mcpMasterReverb, reverb);
			break;
		case 0x6400: // ctrl f7
			if ((chorus-=8)<-64)
				chorus=-64;
			mcpSet(-1, mcpMasterChorus, chorus);
			break;
		case 0x6500: // ctrl f8
			if ((chorus+=8)>64)
				chorus=64;
			mcpSet(-1, mcpMasterChorus, chorus);
			break;
		case 0x8900: // ctrl f11
			finespeed=(finespeed==8)?1:8;
			break;
		case 0x8a00: // ctrl f12
			splock^=1;
			break;*/
		case '\\':
			splock^=1;
			break;
		case KEY_BACKSPACE:
			mcpSet(-1, mcpMasterFilter, set.filter=(set.filter==1)?2:(set.filter==2)?0:1);
			break;
/* TODO-keys
		case 0x6a00:
			mcpNormalize();
			break;
		case 0x6900:
			set.pan=pan;
			set.bal=bal;
			set.vol=vol;
			set.speed=globalmcpspeed;
			set.pitch=globalmcppitch;
			set.amp=amp;
			set.reverb=reverb;
			set.chorus=chorus;
			set.srnd=srnd;
			break;
		case 0x6b00:
			pan=64;
			bal=0;
			vol=64;
			globalmcpspeed=256;
			globalmcppitch=256;
			chorus=0;
			reverb=0;
			amp=64;
			mcpSet(-1, mcpMasterVolume, vol);
			mcpSet(-1, mcpMasterBalance, bal);
			mcpSet(-1, mcpMasterPanning, pan);
			mcpSet(-1, mcpMasterSurround, srnd);
			mcpSet(-1, mcpMasterPitch, globalmcppitch);
			mcpSet(-1, mcpMasterSpeed, globalmcpspeed);
			mcpSet(-1, mcpMasterReverb, reverb);
			mcpSet(-1, mcpMasterChorus, chorus);
			mcpSet(-1, mcpMasterAmplify, 256*amp);
			break;*/
		default:
			return 0;
	}
	return 1;
}

void mcpSetFadePars(int i)
{
	mcpSet(-1, mcpMasterPitch, globalmcppitch*i/64);
	mcpSet(-1, mcpMasterSpeed, globalmcpspeed*i/64);
	mcpSet(-1, mcpMasterVolume, vol*i/64);
}
