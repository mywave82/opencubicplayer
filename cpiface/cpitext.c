/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * CPIFace text modes master mode and window handler
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
 *  -ss040825   Stian Skjelstad <stian@nixia.no>
 *    -upgraded the IQ of cpiTextRecalc to make it try fill more of the screen
 */

#include "config.h"
#include <string.h>
#include "types.h"
#include "stuff/poutput.h"
#include "filesel/pfilesel.h"
#include "cpiface.h"
#include "boot/psetting.h"
#include "boot/plinkman.h"

static struct cpitextmoderegstruct *cpiTextActModes;
static struct cpitextmoderegstruct *cpiTextModes = 0;
static struct cpitextmoderegstruct *cpiTextDefModes = 0;
static struct cpitextmoderegstruct *cpiFocus;
static char cpiFocusHandle[9];
static int modeactive;

static unsigned int LastWidth, LastHeight;

void cpiTextRegisterMode(struct cpitextmoderegstruct *mode)
{
	if (mode->Event&&!mode->Event(cpievInit))
		return;
	mode->next=cpiTextModes;
	cpiTextModes=mode;
}

void cpiTextUnregisterMode(struct cpitextmoderegstruct *m)
{
	struct cpitextmoderegstruct **prev;
	for (prev = &cpiTextModes; *prev; *prev = (*prev)->next)
	{
		if (*prev == m)
		{
			*prev = m->next;
			return;
		}
	}
}

void cpiTextRegisterDefMode(struct cpitextmoderegstruct *mode)
{
	mode->nextdef=cpiTextDefModes;
	cpiTextDefModes=mode;
}

static void cpiTextVerifyDefModes(void)
{
	struct cpitextmoderegstruct *p;

	while (cpiTextDefModes)
	{
		if (cpiTextDefModes->Event&&!cpiTextDefModes->Event(cpievInitAll))
			cpiTextDefModes=cpiTextDefModes->nextdef;
		else
			break;
	}
	p = cpiTextDefModes;
	while (p)
	{
		if (p->nextdef)
		{
			if (p->nextdef->Event&&!p->nextdef->Event(cpievInitAll))
				p->nextdef=p->nextdef->nextdef;
			else
				p=p->nextdef;
		} else
			break;
	}
}

void cpiTextUnregisterDefMode(struct cpitextmoderegstruct *m)
{
	struct cpitextmoderegstruct **prev;
	for (prev = &cpiTextDefModes; *prev; *prev = (*prev)->nextdef)
	{
		if (*prev == m)
		{
			*prev = m->nextdef;
			return;
		}
	}
}

static void cpiSetFocus(const char *name)
{
	struct cpitextmoderegstruct *mode;

	if (cpiFocus&&cpiFocus->Event)
		cpiFocus->Event(cpievLoseFocus);
	cpiFocus=0;
	if (!name)
	{
		*cpiFocusHandle=0;
		return;
	}
	for (mode=cpiTextActModes; mode; mode=mode->nextact)
		if (!strcasecmp(name, mode->handle))
			break;
	*cpiFocusHandle=0;
	if (!mode||(mode->Event&&!mode->Event(cpievGetFocus)))
		return;
	cpiFocus=mode;
	mode->active=1;
	strcpy(cpiFocusHandle, cpiFocus->handle);
	cpiTextRecalc();
}

void cpiTextSetMode(const char *name)
{
	if (!name)
		name=cpiFocusHandle;
	if (!modeactive)
	{
		strcpy(cpiFocusHandle, name);
		cpiSetMode("text");
	} else
		cpiSetFocus(name);
}

void cpiTextRecalc(void)
{
	unsigned int i;
	int winfirst=5;
	int winheight=plScrHeight-winfirst;
	int sidefirst=5;
	int sideheight=plScrHeight-sidefirst;
	struct cpitextmodequerystruct win[10];
	unsigned int nwin=0;
	struct cpitextmoderegstruct *mode;

	int sidemin,sidemax,sidesize;
	int winmin,winmax,winsize;

	plChanChanged=1;

	LastWidth=plScrWidth;
	LastHeight=plScrHeight;

	for (mode=cpiTextActModes; mode; mode=mode->nextact)
	{
		mode->active=0;
		if (mode->GetWin(&win[nwin]))
			win[nwin++].owner=mode;
	}
#ifdef CPIFACE_DEBUG
	fprintf (stderr, "cpiTextRecalc\n");
	fprintf (stderr, "step 1, found all active modes\n");
	for (i=0; i<nwin; i++)
	{
		fprintf (stderr, "[%d] %-8s xmode=%d\n", i, win[i].owner->handle, win[i].xmode);
	};
#endif

	/* xmode bit0 = left column request
	 *       bit1 = right column request
	 */
	if (plScrWidth<132)
		for (i=0; i<nwin; i++)
			win[i].xmode&=1;

#ifdef CPIFACE_DEBUG
	fprintf (stderr, "step 2, masked away xmode bit1 if display can not fit 132 width\n");
	for (i=0; i<nwin; i++)
	{
		fprintf (stderr, "[%d] %-8s xmode=%d\n", i, win[i].owner->handle, win[i].xmode);
	}
#endif

	/* can we fit all the columned windows (not checking the header ones)? */
	while (1)
	{
		/* initialize all the probes back to zero */
		sidemin=sidemax=sidesize=0;
		winmin=winmax=winsize=0;
		/* sum all the left and right windows */
		for (i=0; i<nwin; i++)
		{
			if (win[i].xmode&1)
			{
				winmin+=win[i].hgtmin;
				winmax+=win[i].hgtmax;
				winsize+=win[i].size;
			}
			if (win[i].xmode&2)
			{
				sidemin+=win[i].hgtmin;
				sidemax+=win[i].hgtmax;
				sidesize+=win[i].size;
			}
		}
		if ((winmin<=winheight)&&(sidemin<=sideheight))
			break;
		/* if we were too heigh, hide windows by setting the xmode to 0 */
		if (sidemin>sideheight)
		{
			int worst=0;
			for (i=0; i<nwin; i++)
				if (win[i].xmode&2)
					if (win[i].killprio>win[worst].killprio)
						worst=i;
			win[i].xmode=0;
			continue;
		}
		if (winmin>winheight)
		{
			int worst=0;
			for (i=0; i<nwin; i++)
				if (win[i].xmode&1)
					if (win[i].killprio>win[worst].killprio)
						worst=i;
			win[i].xmode=0;
			continue;
		}
	}

	/* Disable all windows. We are about to actually pick out one and one window until the screen is full */
	for (i=0; i<nwin; i++)
		win[i].owner->active=0;

	/* first we want to fill the screen with all the windows that requested both left and right column, starting with the highest priority ones */
#ifdef CPIFACE_DEBUG
	fprintf (stderr, "step 3, place all xmode==3 windows\n");
#endif
	while (1)
	{
		int best=-1;
		int whgt,shgt,hgt;

		for (i=0; i<nwin; i++)
			if ((win[i].xmode==3)&&!win[i].owner->active)
				if ((best==-1)||(win[i].viewprio>win[best].viewprio))
					best=i;
		if (best==-1)
			break;
		if (!win[best].size)
			hgt=win[best].hgtmin;
		else {
			whgt=win[best].hgtmin+(winheight-winmin)*win[best].size/winsize;
			if ((winheight-whgt)>(winmax-win[best].hgtmax))
				whgt=winheight-(winmax-win[best].hgtmax);
			shgt=win[best].hgtmin+(sideheight-sidemin)*win[best].size/sidesize;
			if ((sideheight-shgt)>(sidemax-win[best].hgtmax))
				shgt=sideheight-(sidemax-win[best].hgtmax);
			hgt=(whgt<shgt)?whgt:shgt;
		}
		if (hgt>win[best].hgtmax)
			hgt=win[best].hgtmax;
		if (win[best].top)
		{
#ifdef CPIFACE_DEBUG
			fprintf (stderr, "Placing window %-8s top %d %d %d %d\n", win[best].owner->handle, 0, plScrWidth, winfirst, hgt);
#endif
			win[best].owner->SetWin(0, plScrWidth, winfirst, hgt);
			winfirst+=hgt;
			sidefirst+=hgt;
		} else {
#ifdef CPIFACE_DEBUG
			fprintf (stderr, "Placing window %-8s bot %d %d %d %d\n", win[best].owner->handle, 0, plScrWidth, winfirst+winheight-hgt, hgt);
#endif
			win[best].owner->SetWin(0, plScrWidth, winfirst+winheight-hgt, hgt);
		}
		win[best].owner->active=1;
		winheight-=hgt;
		sideheight-=hgt;
		winmin-=win[best].hgtmin;
		winsize-=win[best].size;
		sidemin-=win[best].hgtmin;
		sidesize-=win[best].size;

		winmax-=win[best].hgtmax;
		sidemax-=win[best].hgtmax;
	}
#ifdef CPIFACE_DEBUG
	fprintf (stderr, "step 4, place all xmode==2 windows (right column)\n");
#endif

	while (1)
	{
		int best=-1;
		int hgt;

		for (i=0; i<nwin; i++)
			if ((win[i].xmode==2)&&!win[i].owner->active)
				if ((best==-1)||(win[i].viewprio>win[best].viewprio)) /* can crash in theory, TODO */
					best=i;
		if (best==-1)
			break;
		hgt=win[best].hgtmin;
		if (win[best].size)
		{
			hgt+=(sideheight-sidemin)*win[best].size/sidesize;
			if ((sideheight-hgt)>(sidemax-win[best].hgtmax))
				hgt=sideheight-(sidemax-win[best].hgtmax);
		}
		if (hgt>win[best].hgtmax)
			hgt=win[best].hgtmax;
		if (win[best].top)
		{
#ifdef CPIFACE_DEBUG
			fprintf (stderr, "Placing window %-8s top %d %d %d %d\n", win[best].owner->handle, plScrWidth-52, 52, sidefirst, hgt);
#endif
			win[best].owner->SetWin(plScrWidth-52, 52, sidefirst, hgt);
			sidefirst+=hgt;
		} else {
#ifdef CPIFACE_DEBUG
			fprintf (stderr, "Placing window %-8s bot %d %d %d %d\n", win[best].owner->handle, plScrWidth-52, 52, sidefirst+sideheight-hgt, hgt);
#endif
			win[best].owner->SetWin(plScrWidth-52, 52, sidefirst+sideheight-hgt, hgt);
		}
		win[best].owner->active=1;
		sideheight-=hgt;
		sidemin-=win[best].hgtmin;
		sidesize-=win[best].size;
		sidemax-=win[best].hgtmax;
	}

#ifdef CPIFACE_DEBUG
	fprintf (stderr, "step 5, place all xmode==1 windows (left column)\n");
#endif

	while (1)
	{
		int best=-1;
		int hgt;
		int wid;

		for (i=0; i<nwin; i++)
			if ((win[i].xmode==1)&&!win[i].owner->active)
				if ((best==-1)||(win[i].viewprio>win[best].viewprio))
					best=i;
		if (best==-1)
			break;
		if (winmax<=winheight)
			hgt=win[best].hgtmax;
		else {
			hgt=win[best].hgtmin;
			if (win[best].size) /* if size were requested, we try to adjust up from minsize */
			{
/*
				        / free space left
				        |         / min space that has to be used
				        |         |                / size requested
				        |         |                |     / total size requested
*/
				hgt+=(winheight-winmin)*win[best].size/winsize;
				if ((winheight-hgt)>(winmax-win[best].hgtmax))
					hgt=winheight-(winmax-win[best].hgtmax);
			}
			if (hgt>win[best].hgtmax)
				hgt=win[best].hgtmax;
		}
		if (win[best].top)
		{

			if (plScrWidth < 132)
			{
				wid=plScrWidth;
			} else {
/*
				              /-------------- does our new window start below (the last used) sidewindow?
				              |                            /-- does our new window stop before the sidewindow have anything to display?
				              |                            |                             /-- then use the hole width
				              |                            |                             |           /-- or make room for the sidewindow
*/
				wid=((winfirst>=sidefirst)&&((winfirst+hgt)<=(sidefirst+sideheight)))?plScrWidth:plScrWidth-52;
			}
#ifdef CPIFACE_DEBUG
			fprintf (stderr, "Placing window %-8s top %d %d %d %d\n", win[best].owner->handle, 0, wid, winfirst, hgt);
#endif

			win[best].owner->SetWin(0, wid, winfirst, hgt);
			winfirst+=hgt;
		} else {
			if (plScrWidth < 132)
			{
				wid=plScrWidth;
			} else {
				wid=(((winfirst+winheight)<=(sidefirst+sideheight))&&((winfirst+winheight-hgt)>=sidefirst))?plScrWidth:plScrWidth-52;
			}
#ifdef CPIFACE_DEBUG
			fprintf (stderr, "Placing window %-8s bot %d %d %d %d\n", win[best].owner->handle, 0, wid, winfirst+winheight-hgt, hgt);
#endif

			win[best].owner->SetWin(0, wid, winfirst+winheight-hgt, hgt);
		}
		win[best].owner->active=1;
		winheight-=hgt;
		winmin-=win[best].hgtmin;
		winsize-=win[best].size;
		winmax-=win[best].hgtmax;

	}
#if 0
	for (i=0; i<winheight; i++)
		displayvoid(winfirst+i, 0, plScrWidth-52);
	for (i=0; i<sideheight; i++)
		displayvoid(sidefirst+i, plScrWidth-52, 52);
	if (!nwin)
		for (i=0;i<plScrHeight;i++)
			displayvoid(i, 0, plScrWidth);
#else
	for (i=0;i<plScrHeight;i++)
		displayvoid(i, 0, plScrWidth);
#endif
}

static void txtSetMode(void)
{
	struct cpitextmoderegstruct *mode;
	plSetTextMode(fsScrType);
	fsScrType=plScrType;
	for (mode=cpiTextActModes; mode; mode=mode->nextact)
		if (mode->Event)
			mode->Event(cpievSetMode);
	cpiTextRecalc();
}

static void txtDraw(void)
{
	struct cpitextmoderegstruct *mode;

	if ((LastWidth!=plScrWidth)||(LastHeight!=plScrHeight)) /* xterms as so fun */
		cpiTextRecalc();

	cpiDrawGStrings();
	for (mode=cpiTextActModes; mode; mode=mode->nextact)
		if (mode->active)
			mode->Draw(mode==cpiFocus);
	for (mode=cpiTextModes; mode; mode=mode->next)
		mode->Event(cpievKeepalive);
}

static int txtIProcessKey(uint16_t key)
{
	struct cpitextmoderegstruct *mode;
#ifdef KEYBOARDTEXT_DEBUG
	fprintf(stderr, "txtIProcessKey:START\n");
#endif
	for (mode=cpiTextModes; mode; mode=mode->next)
	{
#ifdef KEYBOARDTEXT_DEBUG
		fprintf(stderr, "Checking mode %s\n", mode->handle);
#endif
		if (mode->IProcessKey(key))
		{
#ifdef KEYBOARDTEXT_DEBUG
			fprintf(stderr, "Mode swallowed event\ntxtIProcessKey:STOP\n");
#endif
			return 1;
		}
	}
#ifdef KEYBOARDTEXT_DEBUG
	fprintf(stderr, "txtIProcessKey:STOP\n");
#endif
	switch (key)
	{
		case 'x': case 'X':
			fsScrType=7;
			cpiTextSetMode(cpiFocusHandle);
			return 1;
		case KEY_ALT_X:
			fsScrType=0;
			cpiTextSetMode(cpiFocusHandle);
			return 1;
		case 'z': case 'Z':
			cpiTextSetMode(cpiFocusHandle);
			break;
		default:
			return 0;
	}
	return 1;
}

static int txtAProcessKey(uint16_t key)
{
#ifdef KEYBOARDTEXT_DEBUG
	fprintf(stderr, "txtAProcessKey:START\ncpiFocus is %s\n", cpiFocus?"set":"unset");
#endif
	if (cpiFocus)
		if (cpiFocus->active)
			if (cpiFocus->AProcessKey(key))
			{
#ifdef KEYBOARDTEXT_DEBUG
				fprintf(stderr, "cpiFocus %s swallowed event\ntxtAProcessKey:STOP\n", cpiFocus->handle);
#endif
				return 1;
			}
#ifdef KEYBOARDTEXT_DEBUG
	fprintf(stderr, "nobody swallowed the event\ntxtAProcessKey:STOP\n");
#endif
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('x', "Set screen text mode (set mode 7)");
			cpiKeyHelp('X', "Set screen text mode (set mode 7)");
			cpiKeyHelp('z', "Set screen text mode (toggle bit 1)");
			cpiKeyHelp('Z', "Set screen text mode (toggle bit 1)");
			cpiKeyHelp(KEY_ALT_X, "Set screen text screen mode (set mode 0)");
			cpiKeyHelp(KEY_ALT_Z, "Set screen text screen mode (toggle bit 2)");
			cpiKeyHelp(KEY_CTRL_Z, "Set screen text screen mode (toggle bit 1)");
			return 0;
		case 'x': case 'X':
			fsScrType=7;
			cpiForwardIProcessKey (key);
			cpiResetScreen();
			return 1;
		case KEY_ALT_X:
			fsScrType=0;
			cpiForwardIProcessKey (key);
			cpiResetScreen();
			return 1;
		case 'z': case 'Z':
			fsScrType^=2;
			cpiForwardIProcessKey (key);
			cpiResetScreen();
			break;
		case KEY_ALT_Z:
			fsScrType^=4;
			cpiForwardIProcessKey (key);
			cpiResetScreen();
			break;
		case KEY_CTRL_Z:
			fsScrType^=1;
			cpiForwardIProcessKey (key);
			cpiResetScreen();
			break;
		default:
			return 0;
	}
	return 1;
}

static int txtInit(void)
{
	struct cpitextmoderegstruct *mode;
	for (mode=cpiTextDefModes; mode; mode=mode->nextdef)
		cpiTextRegisterMode(mode);
	cpiSetFocus(cpiFocusHandle);
	return 1;
}

static void txtClose(void)
{
	struct cpitextmoderegstruct *mode;
	for (mode=cpiTextModes; mode; mode=mode->next)
		if (mode->Event)
			 mode->Event(cpievDone);
	cpiTextModes=0;
}

static int txtInitAll(void)
{
	cpiTextVerifyDefModes();
	return 1;
}

static void txtCloseAll(void)
{
	struct cpitextmoderegstruct *mode;
	for (mode=cpiTextDefModes; mode; mode=mode->nextdef)
		if (mode->Event)
			mode->Event(cpievDoneAll);
	cpiTextDefModes=0;
}

static int txtOpenMode(void)
{
	struct cpitextmoderegstruct *mode;

	modeactive=1;
	cpiTextActModes=0;
	for (mode=cpiTextModes; mode; mode=mode->next)
	{
		if (mode->Event&&!mode->Event(cpievOpen))
			continue;
		mode->nextact=cpiTextActModes;
		cpiTextActModes=mode;
	}
	cpiSetFocus(cpiFocusHandle);

	return 1;
}

static void txtCloseMode(void)
{
	struct cpitextmoderegstruct *mode;

	cpiSetFocus(0);
	for (mode=cpiTextActModes; mode; mode=mode->nextact)
		if (mode->Event)
			mode->Event(cpievClose);
	cpiTextActModes=0;
	modeactive=0;
}

static int txtEvent(int ev)
{
	switch (ev)
	{
		case cpievOpen:
			return txtOpenMode();
		case cpievClose:
			txtCloseMode();
			return 1;
		case cpievInit:
			return txtInit();
		case cpievDone:
			txtClose();
			return 1;
		case cpievInitAll:
			return txtInitAll();
		case cpievDoneAll:
			txtCloseAll();
			return 1;
	}
	return 1;
}

struct cpimoderegstruct cpiModeText = {"text", txtSetMode, txtDraw, txtIProcessKey, txtAProcessKey, txtEvent CPIMODEREGSTRUCT_TAIL};
