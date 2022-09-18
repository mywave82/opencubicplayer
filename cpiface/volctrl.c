/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * CPIface text mode volume control interface code
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
 *  -fd9807??   Felix Domke    <tmbinc@gmx.net>
 *    -first release
 *  -kb990531   Tammo Hinrichs <opencp@gmx.net>
 *    -put enumeration of volregs into the Init event handler
 *     (was formerly in InitAll, thus preventing detection of
 *      volregs only available AFTER the interface startup)
 *  -ryg990609  Fabian Giesen  <fabian@jdcs.su.nw.schule.de>
 *    -togglebutton support (and how i did it... god, forgive me :)
 *  -fd991113   Felix Domke  <tmbinc@gmx.net>
 *    -added support for scrolling
 *  -doj20020901 Dirk Jagdmann <doj@cubic.org>
 *    -window position is not reseted anymore when switching from graphicsmode
 *     to textmode

 * ocp.ini:
 * [screen]
 *   volctrl80=off         ; volctrl active by default only in 132column mode,
 *   volctrl132=on         ; not in 80.
 * and maybe:
 * [sound]
 *   volregs=...           ; (same as in dllinfo's volregs)
 */

#include "config.h"
#include <string.h>
#include "types.h"
#include "boot/plinkman.h"
#include "boot/psetting.h"
#include "cpiface.h"
#include "cpiface-private.h"
#include "stuff/imsrtns.h"
#include "stuff/poutput.h"
#include "vol.h"

static enum modeenum { modeNone=0, mode80=1, mode52=2 } mode;
static int focus, active;
static int x0, y0, x1, y1, yoff;       /* origin x, origin y, width, height */
static int AddVolsByName(char *n);
static int vols;                       /* number of registered vols */
static struct regvolstruct             /* the registered vols */
{
	struct ocpvolregstruct *volreg;
	int id;
} vol[100];                            /* TODO: dynamically? on the other hand: which display has more than 100 lines? :) */

static int AddVolsByName(char *n)
{
	struct ocpvolregstruct *x=(struct ocpvolregstruct *)_lnkGetSymbol(n);
	int num;
	int i;
	if(!x)
		return(0);
	num=x->GetVolumes();
	for(i=0; i<num; i++)
	{
		struct ocpvolstruct y;
		if(vols>=100)
			return 0;
		if(x->GetVolume(&y, i))
		{
			vol[vols].volreg=x;
			vol[vols].id=i;
			vols++;
		}
	}
	return 1;
}

static int GetVols(void)
{
	char const *dllinfo=_lnkReadInfoReg("volregs");
	vols=0;

	if(dllinfo)
	{
		int num=cfCountSpaceList(dllinfo, 100);
		int i;
		for(i=0; i<num; i++)
		{
			char buf[100];
			cfGetSpaceListEntry(buf, &dllinfo, 100);
			AddVolsByName(buf);
		}
	}
	return 1;
}

static int volctrlGetWin (struct cpifaceSessionAPI_t *cpifaceSession, struct cpitextmodequerystruct *q)
{
	switch(mode)
	{
		case(modeNone):
			return 0;
		case(mode80):
			q->top=0;
			q->xmode=1;
			break;
		case(mode52):
			q->top=0;
			q->xmode=2;
			break;
		default:
			break;
	}
	q->killprio=128;                /* don't know in which range they are.. */
	q->viewprio=20;
	q->size=1;                      /* ?!? */
	q->hgtmin=3;
	q->hgtmax=/*(!vols)?1:(vols+1);*/vols+1;
	return 1;
}

static void volctrlSetWin (struct cpifaceSessionAPI_t *cpifaceSession, int xmin, int xwid, int ymin, int ywid)
{
	x0=xmin;
	y0=ymin;
	x1=xwid;
	y1=ywid;
}

static void volctrlDraw (struct cpifaceSessionAPI_t *cpifaceSession, int focus)
{
	uint16_t buf[CONSOLE_MAX_X];
	size_t ll;
	int i;
	unsigned int barlen;
	int su, sd;

	memset(buf, 0, sizeof(buf));
	if(vols)
		writestring(buf, 3, focus?0x9:0x1, "volume control", x1);
	else
		writestring(buf, 3, focus?0x9:0x1, "volume control: no volume regs", x1);
	displaystrattr(y0, x0, buf, x1);

	if (!vols)
		return;

	ll=0;
	for(i=0; i<vols; i++)
	{
		char name[256];
		struct ocpvolstruct x;
		vol[i].volreg->GetVolume(&x, vol[i].id);

		strcpy(name, x.name);

		if (strchr(name, '\t'))
			*strchr(name, '\t')=0;                   /* don't forget this one :) */

		if (strlen(name)>ll) ll=strlen(name);
	}
	barlen=x1-ll-5;
	if(barlen<4) { barlen=4; ll=x1-9; }

	if ((active-yoff)<0)
		yoff=active;
	if ((active-yoff)>=(y1-1))
		yoff=active-y1+2;

	if ((yoff+(y1-1))>vols)
		yoff=(y1-1)-vols;
	if (yoff<0) /* shouldn't happen, unless we get a bigger window than we need */
		yoff=0;

	su=0; sd=0;

	if (vols>(y1-1))
		su=sd=1;

	if (yoff<=(vols-y1-1))
		sd++;
	if (yoff)
		su++;

	for(i=yoff; i<(yoff+(y1-1)); i++)
	{
		struct ocpvolstruct x;
		int hc=focus?((i==active)?7:8):8;
		char  name[256];     /* you won't do labels with >255 chars, WILL YOU? :) */

		vol[i].volreg->GetVolume(&x, vol[i].id);

		strncpy(name, x.name, ll);
		name[ll]=0;
		if (strchr(name, '\t'))
			*strchr(name, '\t')=0;                              /* nice hack. */

		/*  if(strlen(x.name)>ll) x.name[ll]=0;*/                 /* ugly hack, but who cares? :) */
		*buf=' ';
		if (!(i-yoff))
			if (su--)
				writestring(buf, 0, su?0x07:0x08, "\x18", 1);
		if (i==(yoff+y1-2))
			if (sd--)
				writestring(buf, 0, sd?0x07:0x08, "\x19", 1);

		writestring(buf, 1, hc, name, ll);                    /* ('len' seems to be ingnored by writestring) */
		writestring(buf, ll+1, hc, " [", ll);
		writestring(buf, ll+barlen+3, hc, "] ", ll);

		if (!x.min && x.max<0)          /* this hack enables togglebuttons (ouch) */
		{
			/* --- how to do toglebuttons ---
			 * set volreg.min to 0, volreg.max to -<choices>, and use \t as
			 * delimiter in the name-field (for the values).
			 * hurts, but it works :)
			 */

			char   sbuf[CONSOLE_MAX_X], *ptr=&sbuf[0];
			int    i;
			unsigned int of;
			unsigned int po;

			strcpy(sbuf, x.name);
			for (i=x.val+1, ptr=&sbuf[0]; i && *ptr; ptr++)
				if (*ptr=='\t') i--;

			memsetw(&buf[ll+3], (hc<<8)+0x20, barlen);

			if (!*ptr || i)
			{
				strcpy(sbuf, "(NULL)");
				ptr=&sbuf[0];
			}

			if (strchr(ptr, '\t'))
				*strchr(ptr, '\t')=0;

			if (strlen(ptr)>=barlen)
				ptr[barlen]=0;

			of=(barlen-strlen(ptr))>>1;

			for (po=of; po<strlen(ptr)+of; po++)
				buf[po+ll+3]=ptr[po-of]/*|((hc-7)?0x800:0x900)*/;
		} else {
			int p=((x.val-x.min)*(barlen))/(x.max-x.min);       /* range: 0..barlen */
			int po;

			p=((unsigned)p>barlen)?barlen:(p<0)?0:p; /* p never get this big */

			for (po=0; (unsigned)po<barlen; po++) /* barlen and po should never be this big */
			{
				int p4=(po<<2)/barlen;                    /* range: 0..4 */
				if(po<p)
					buf[po+ll+3]=((unsigned char)'\xfe')| ( (i==active && focus)? (("\x01\x09\x0b\x0f"[p4>3?3:p4])<<8):0x0800);
				else
					buf[po+ll+3]=((unsigned char)'\xfa')|(hc<<8);
			}
		}

		displaystrattr(y0+i+1-yoff, x0, buf, x1);
	}
}

static int volctrlIProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('m', "Toggle volume control interface mode");
			cpiKeyHelp('M', "Toggle volume control interface mode");
			break;
		case 'm': case 'M':
			if((focus)||(mode==modeNone))
			{
				mode=(enum modeenum)(((int)mode+1)%3);
				if((mode==mode52)&&(plScrWidth<132))
					mode=modeNone;
				if(mode!=modeNone)
					cpiTextSetMode (cpifaceSession, "volctrl");
				cpiTextRecalc (cpifaceSession);
			} else {
				cpiTextSetMode (cpifaceSession, "volctrl");
			}
			return 1;
		case 'x': case 'X':
			if(mode)
			{
				mode=mode52;
				if(plScrWidth<132)
					mode=mode80;
			}
			break;
		case KEY_ALT_X:
			if(mode)
			{
				mode=mode80;
			}
			break;
	}
	return 0;
}

static int volctrlAProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('m', "Toggle volume control interface mode");
			cpiKeyHelp('M', "Toggle volume control interface mode");
			cpiKeyHelp(KEY_UP, "Select previous volume interface");
			cpiKeyHelp(KEY_DOWN, "Select next volume interface");
			cpiKeyHelp(KEY_LEFT, "Decrease selected volume interface");
			cpiKeyHelp(KEY_RIGHT, "Increase selected volume interface");
			return 0;
		case KEY_UP:
			if(focus&&vols)
			{
				active--;
				if(active<0)
					active=vols-1;
				volctrlDraw (cpifaceSession, focus);
				return 1;
			}
			break;
		case KEY_DOWN:
			if(focus&&vols)
			{
				active++;
				if (active>(vols-1))
					active=0;
				volctrlDraw (cpifaceSession, focus);
				return 1;
			}
			break;
		case KEY_RIGHT:
			if(focus&&vols)
			{
				struct ocpvolstruct x;
				vol[active].volreg->GetVolume(&x, vol[active].id);
				if (!x.min && x.max<0)
				{
					x.val++;
					if (x.val>=-x.max) x.val=0;
					if (x.val<0) x.val=-x.max-1;
				} else {
					x.val+=x.step;
					if(x.val>x.max) x.val=x.max;
					if(x.val<x.min) x.val=x.min;
				}

				vol[active].volreg->SetVolume(&x, vol[active].id);
				return 1;
			}
			break;
		case KEY_LEFT:
			if(focus&&vols)
			{
				struct ocpvolstruct x;
				vol[active].volreg->GetVolume(&x, vol[active].id);

				if (!x.min && x.max<0)
				{
					x.val--;
					if (x.val>=-x.max) x.val=0;
					if (x.val<0) x.val=-x.max-1;
				} else {
					x.val-=x.step;
					if(x.val>x.max) x.val=x.max;
					if(x.val<x.min) x.val=x.min;
				}

				vol[active].volreg->SetVolume(&x, vol[active].id);
				return 1;
			}
			break;
		default:
			return 0;
	}
	return 1;
}

static int volctrlEvent (struct cpifaceSessionAPI_t *cpifaceSession, int ev)
{
	switch(ev)
	{
		case cpievInit:
			GetVols();
			mode=modeNone;
			return(!!vols);
		case cpievInitAll:
			return(1);
		case cpievOpen:
			return(1);
		case cpievGetFocus:
			focus=!0;
			return(1);
		case cpievLoseFocus:
			focus=0;
			return(1);
		case cpievSetMode:
			if(cfGetProfileBool("screen", plScrWidth<132?"volctrl80":"volctrl132", plScrWidth<132?0:!0, plScrWidth<132?0:!0))
			{
				if(plScrWidth<132) mode=mode80;
				cpiTextRecalc(&cpifaceSessionAPI.Public);
			}
			return(1);
	}
	return 0;
}

static struct cpitextmoderegstruct cpiVolCtrl={"volctrl", volctrlGetWin, volctrlSetWin, volctrlDraw, volctrlIProcessKey, volctrlAProcessKey, volctrlEvent CPITEXTMODEREGSTRUCT_TAIL};

static void __attribute__((constructor))init(void)
{
	cpiTextRegisterDefMode(&cpiVolCtrl);
}

static void __attribute__((destructor))done(void)
{
	cpiTextUnregisterDefMode(&cpiVolCtrl);
}
