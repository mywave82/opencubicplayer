#include "config.h"
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "stuff/poutput.h"
#include "cpiface/cpiface.h"

#include "sndhtype.h"
#include "sndhplay.h"

#define COLTITLE1 0x01
#define COLTITLE1H 0x09

#ifndef MIN
# define MIN(a,b) ((a)<(b))?(a):(b)
#endif

static int sndhTrackActive;
static int sndhTrackFirstLine;
static int sndhTrackHeight;
static int sndhTrackWidth;
static int sndhTrackDesiredHeight;
static int sndhTrackScroll;

static void sndhTrackDraw (struct cpifaceSessionAPI_t *cpifaceSession, int focus)
{
	int i;
	struct sndhMeta_t *meta = sndhGetMeta ();
	struct sndhStat_t stat;
	sndhStat (&stat);

	while (sndhTrackScroll && ((sndhTrackScroll + sndhTrackHeight) > sndhTrackDesiredHeight))
	{
		sndhTrackScroll--;
	}

	cpifaceSession->console->Driver->DisplayStr(sndhTrackFirstLine, 0, focus?COLTITLE1H:COLTITLE1, focus?"psgplay subtrack view - page up/dn to scroll":"psgplay subtrack view (press <t> to focus)", sndhTrackWidth);

	for (i=1; i < sndhTrackHeight; i++)
	{
		int subtrack = i + sndhTrackScroll; // 1-based

		const char *title;
		char trackbuffer[20];
		if (meta && meta->titles)
		{
			title = meta->titles[subtrack - 1];
		} else if (meta && meta->subtunes > 2)
		{
			snprintf (trackbuffer, sizeof (trackbuffer), "Title %d", subtrack);
			title = trackbuffer;
		} else if (meta && meta->title)
		{
			title = meta->title;
		} else {
			title = "Unknown title";
		}

		int minutes, seconds, frames;
		int timeunknown;
		if (meta && meta->frames)
		{
			minutes = meta->frames[i - 1] / (50 * 60);
			seconds = meta->frames[i - 1] % (50 * 60) / 50;
			frames  = meta->frames[i - 1] % 50;
		} else if (meta && meta->times)
		{
			minutes = meta->times[i - 1] / 60;
			seconds = meta->times[i - 1] % 60;
			frames = 0;
		} else {
			minutes = seconds = frames = 0;
		}
		timeunknown = ((minutes == 0) && (seconds == 0) && (frames == 0));

		cpifaceSession->console->DisplayPrintf (
			sndhTrackFirstLine + i,
			0,
			0x07,
			sndhTrackWidth,
			"%-2d:%c%.*o%*S %.*o%-3d:%02d.%02d",
			subtrack,
			(subtrack == stat.SubTune_active) ? '>' : (meta && meta->defaultsubtune == subtrack) ? '*' : ' ',
			(subtrack == stat.SubTune_active) ? 0x0f : 0x07,
			MIN(64, sndhTrackWidth - 2 - 1 - 1   - 1 - 3 - 1 - 2 - 1 - 2),
			title,
			timeunknown ? 8 : 7,
			minutes,
			seconds,
			frames);
	}
}

static int sndhTrackGetWin (struct cpifaceSessionAPI_t *cpifaceSession, struct cpitextmodequerystruct *q)
{
	struct sndhMeta_t *meta;

	if (!sndhTrackActive)
		return 0;

	meta = sndhGetMeta ();
	if (!meta)
	{
		sndhTrackActive = 0;
		return 0;
	}

	sndhTrackDesiredHeight = q->hgtmax = 1 + (meta->subtunes ? meta->subtunes : 1);
	q->hgtmin = 4;
	if (q->hgtmin > q->hgtmax)
	{
		q->hgtmax = q->hgtmin;
	}

	q->xmode=1;
	q->size=2;
	q->top=0;
	q->killprio=64;
	q->viewprio=160;
	return 1;
}

static void sndhTrackSetWin (struct cpifaceSessionAPI_t *cpifaceSession, int _ignore, int wid, int ypos, int hgt)
{
	sndhTrackFirstLine = ypos;
	sndhTrackHeight = hgt;
	sndhTrackWidth = wid;
}

static int sndhTrackIProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpifaceSession->KeyHelp ('t', "Enable SNDH subsongs viewer");
			cpifaceSession->KeyHelp ('T', "Enable SNDH subsongs viewer");
			break;
		case 't': case 'T':
			sndhTrackActive = 1;
			cpifaceSession->cpiTextSetMode (cpifaceSession, "sndhTrack");
			return 1;
		case 'x': case 'X':
			sndhTrackActive = 1;
			break;
		case KEY_ALT_X:
			sndhTrackActive = 0;
			break;
	}
	return 0;
}

static int sndhTrackAProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	switch (key)
	{
		case 't': case 'T':
			sndhTrackActive = !sndhTrackActive;
			cpifaceSession->cpiTextRecalc (cpifaceSession);
			break;

		case KEY_ALT_K:
			cpifaceSession->KeyHelp ('t',       "Disable SNDH subsongs viewer");
			cpifaceSession->KeyHelp ('T',       "Disable SNDH subsongs viewer");
			cpifaceSession->KeyHelp (KEY_PPAGE, "Scroll SNDH subsongs viewer up");
			cpifaceSession->KeyHelp (KEY_NPAGE, "Scroll SNDH subsongs viewer down");
			cpifaceSession->KeyHelp (KEY_HOME,  "Scroll SNDH subsongs viewer to the top");
			cpifaceSession->KeyHelp (KEY_END,   "Scroll SNDH subsongs viewer to the bottom");
			return 0;

		case KEY_PPAGE:
			if (sndhTrackScroll)
			{
				sndhTrackScroll--;
			}
			break;
		case KEY_NPAGE:
			sndhTrackScroll++;
			break;
		case KEY_HOME:
			sndhTrackScroll = 0;
		case KEY_END:
			sndhTrackScroll = sndhTrackDesiredHeight - sndhTrackHeight;
			break;
		default:
			return 0;
	}
	return 1;
}

static int sndhTrackEvent (struct cpifaceSessionAPI_t *cpifaceSession, int ev)
{
	switch (ev)
	{
		case cpievInitAll:
			return 1;
		case cpievInit:
			sndhTrackActive=1;
			// Here we can allocate memory, return 0 on error
			break;
		case cpievDone:
			// Here we can free memory
			break;
	}
	return 1;
}


static struct cpitextmoderegstruct cpiSndhTrack = {"sndhTrack", sndhTrackGetWin, sndhTrackSetWin, sndhTrackDraw, sndhTrackIProcessKey, sndhTrackAProcessKey, sndhTrackEvent CPITEXTMODEREGSTRUCT_TAIL};

OCP_INTERNAL void sndhTrackInit (struct cpifaceSessionAPI_t *cpifaceSession)
{
	cpifaceSession->cpiTextRegisterMode (cpifaceSession, &cpiSndhTrack);
}

OCP_INTERNAL void sndhTrackDone (struct cpifaceSessionAPI_t *cpifaceSession)
{
	cpifaceSession->cpiTextUnregisterMode (cpifaceSession, &cpiSndhTrack);
}
