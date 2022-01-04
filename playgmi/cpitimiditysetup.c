#include "config.h"
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "cpiface/cpiface.h"
#include "stuff/compat.h"
#include "stuff/poutput.h"

#include "timidity.h"
#include "instrum.h"
#include "reverb.h"
#include "playmidi.h"

#define COLTITLE1 0x01
#define COLTITLE1H 0x09

static int TimiditySetupActive;

static int TimiditySetupFirstLine;
static int TimiditySetupHeight;
static int TimiditySetupWidth;
//static int TimiditySetupHeight;

static int TimiditySetupEditPos = 0;

static int TimiditySetupSelected = 3; /* ranges from 0 to 4 */
static int TimiditySetupLevel = DEFAULT_REVERB_SEND_LEVEL; /* ranges from 1 to 127 */

static int TimiditySetupScaleRoom = 28;
static int TimiditySetupOffsetRoom = 70;
static int TimiditySetupPreDelayFactor = 100;

static void TimiditySetupSetWin(int _ignore, int wid, int ypos, int hgt)
{
	TimiditySetupFirstLine=ypos;
	TimiditySetupHeight=hgt;
	TimiditySetupWidth=wid;
}

static int TimiditySetupGetWin(struct cpitextmodequerystruct *q)
{
	if (!TimiditySetupActive)
	{
		return 0;
	}
	q->hgtmin = 9;
	q->hgtmax = 9;
	q->xmode = 1;
	q->size = 2;
	q->top = 0;
	q->killprio = 64;
	q->viewprio = 160;
	return 1;
}

static void TimiditySetupDrawItems (const int focus, const int lineno, const int skip, const char **list, const int listlength, const int selected, const int active)
{
	int xpos = 16 + skip;
	int i;
	for (i=0; i < listlength; i++)
	{
		if (selected == i)
		{
			displaystr (TimiditySetupFirstLine + lineno, xpos, (focus&&active)?0x09:0x01, "[",     1);                xpos += 1;
			displaystr (TimiditySetupFirstLine + lineno, xpos, (focus&&active)?0x0f:0x07, list[i], strlen (list[i])); xpos += strlen (list[i]);
			displaystr (TimiditySetupFirstLine + lineno, xpos, (focus&&active)?0x09:0x01, "]",     1);                xpos += 1;
		} else {
			displaystr (TimiditySetupFirstLine + lineno, xpos, 0x00,                      " ",     1);                xpos += 1;
			displaystr (TimiditySetupFirstLine + lineno, xpos, (focus&&active)?0x07:0x08, list[i], strlen (list[i])); xpos += strlen (list[i]);
			displaystr (TimiditySetupFirstLine + lineno, xpos, 0x00,                      " ",     1);                xpos += 1;
		}
	}
	displaystr (TimiditySetupFirstLine + lineno, xpos, 0x00, " ", TimiditySetupWidth - xpos);
}

static void TimiditySetupDrawBar (const int focus, const int lineno, const int skip, int level, int maxlevel, const int active)
{
	if (level >= 0)
	{
		char temp[7];
		int tw = TimiditySetupWidth - 5 - 16 - skip - skip - 2;
		int pw = tw * level / maxlevel;
		int p1, p2, p3, p4;
	
		p1 = tw * 1 / 4;
		p2 = tw * 2 / 4;
		p3 = tw * 3 / 4;
		p4 = tw;
		if (p1 > pw)
		{
			p1 = pw;
			p2 = 0;
			p3 = 0;
			p4 = 0;
		} else if (p2 > pw)
		{
			p2 = pw - p1;
			p3 = 0;
			p4 = 0;
		} else if (p3 > pw)
		{
			p3 = pw - p2;
			p2 -= p1;
			p4 = 0;
		} else {
			p4 = pw - p3;
			p3 -= p2;
			p2 -= p1;
		}
		displaystr (TimiditySetupFirstLine + lineno, 16 + skip,                         (focus && active)?0x07:0x08, "[", 1);
		displaychr (TimiditySetupFirstLine + lineno, 16 + 1 + skip,                     (focus && active)?0x01:0x08, '\xfe', p1);
		displaychr (TimiditySetupFirstLine + lineno, 16 + 1 + skip + p1,                (focus && active)?0x09:0x08, '\xfe', p2);
		displaychr (TimiditySetupFirstLine + lineno, 16 + 1 + skip + p1 + p2,           (focus && active)?0x0b:0x08, '\xfe', p3);
		displaychr (TimiditySetupFirstLine + lineno, 16 + 1 + skip + p1 + p2 + p3,      (focus && active)?0x0f:0x08, '\xfe', p4);
		displaychr (TimiditySetupFirstLine + lineno, 16 + 1 + skip + p1 + p2 + p3 + p4, (focus && active)?0x07:0x08, '\xfa', tw - p1 - p2 - p3 - p4);

		snprintf (temp, sizeof (temp), "]%5d", level);
		displaystr (TimiditySetupFirstLine + lineno, TimiditySetupWidth - 6 - skip, (focus && active)?0x07:0x08, temp, 6 + skip);
	} else {
		displaystr (TimiditySetupFirstLine + lineno, 16 + skip, 0x08, "----", TimiditySetupWidth - 16 - skip);
	}

}

static void TimiditySetupDraw(int focus)
{
	const char *reverbs[] = {"disable", "original", "global-original", "freeverb", "global-freeverb"};
	const char *effect_lr_modes[] = {"disable", "left", "right", "both"};
	const char *disable_enable[] = {"disable", "enable"};
	int skip;

	if (TimiditySetupWidth >= 83)
	{
		skip = 2;
	} else if (TimiditySetupWidth >= 81)
	{
		skip = 1;
	} else {
		skip = 0;
	}

	displaystr (TimiditySetupFirstLine, 0, focus?COLTITLE1H:COLTITLE1, focus?"   Timidity Setup":"   Timidity Setup (press t to focus)", TimiditySetupWidth);

	displaystr (TimiditySetupFirstLine + 1, 0, (focus&&(TimiditySetupEditPos==1))?0x07:0x08, "  Reverb:" + 2 - skip, 16 + skip);
	TimiditySetupDrawItems (focus, 1, skip, reverbs, 5, TimiditySetupSelected, TimiditySetupEditPos==0);

	displaystr (TimiditySetupFirstLine + 2, 0, (focus&&(TimiditySetupEditPos==1))?0x07:0x08, "  Level:" + 2 - skip, 16 + skip);
	TimiditySetupDrawBar (focus, 2, skip, (TimiditySetupSelected != 0) ? TimiditySetupLevel : -1, 127, TimiditySetupEditPos == 1);
#if 0
	if (TimiditySetupSelected != 0)
	{
		int tw = TimiditySetupWidth - 5 - 16 - skip - skip - 2;
		int pw = tw * TimiditySetupLevel / 127;
		int p1, p2, p3, p4;

		displaystr (TimiditySetupFirstLine + 2, 16 + skip, (focus&&(TimiditySetupEditPos==1))?0x07:0x08, "[", 1);
	
		p1 = tw * 1 / 4;
		p2 = tw * 2 / 4;
		p3 = tw * 3 / 4;
		p4 = tw;
		if (p1 > pw)
		{
			p1 = pw;
			p2 = 0;
			p3 = 0;
			p4 = 0;
		} else if (p2 > pw)
		{
			p2 = pw - p1;
			p3 = 0;
			p4 = 0;
		} else if (p3 > pw)
		{
			p3 = pw - p2;
			p2 -= p1;
			p4 = 0;
		} else {
			p4 = pw - p3;
			p3 -= p2;
			p2 -= p1;
		}
		displaychr (TimiditySetupFirstLine + 2, 16 + 1 + skip,                     (focus && (TimiditySetupEditPos==1)) ?0x01:0x08, '\xfe', p1);
		displaychr (TimiditySetupFirstLine + 2, 16 + 1 + skip + p1,                (focus && (TimiditySetupEditPos==1)) ?0x09:0x08, '\xfe', p2);
		displaychr (TimiditySetupFirstLine + 2, 16 + 1 + skip + p1 + p2,           (focus && (TimiditySetupEditPos==1)) ?0x0b:0x08, '\xfe', p3);
		displaychr (TimiditySetupFirstLine + 2, 16 + 1 + skip + p1 + p2 + p3,      (focus && (TimiditySetupEditPos==1)) ?0x0f:0x08, '\xfe', p4);
		displaychr (TimiditySetupFirstLine + 2, 16 + 1 + skip + p1 + p2 + p3 + p4, (focus && (TimiditySetupEditPos==1)) ?0x07:0x08, '\xfa', tw - p1 - p2 - p3 - p4);

		snprintf (temp, sizeof (temp), "]%5d", TimiditySetupLevel);
		displaystr (TimiditySetupFirstLine + 2, TimiditySetupWidth - 6 - skip, (focus&&(TimiditySetupEditPos==1))?0x07:0x08, temp, 6 + skip);
	} else {
		displaystr (TimiditySetupFirstLine + 2, 16 + skip, 0x08, "----", TimiditySetupWidth - 16 - skip);
	}
#endif

	displaystr (TimiditySetupFirstLine + 3, 0, (focus&&(TimiditySetupEditPos==2))?0x07:0x08, "  ScaleRoom:" + 2 - skip, 16 + skip);
	TimiditySetupDrawBar (focus, 3, skip, (TimiditySetupSelected >= 3) ? TimiditySetupScaleRoom : -1, 1000, TimiditySetupEditPos == 2);
#if 0
	if (TimiditySetupSelected >= 3)
	{
		snprintf (temp, sizeof (temp), "%4d", TimiditySetupScaleRoom);
		displaystr (TimiditySetupFirstLine + 3, 16 + skip, (focus&&(TimiditySetupEditPos==2))?0x07:0x08, temp, TimiditySetupWidth - 16 - skip);
	} else {
		displaystr (TimiditySetupFirstLine + 3, 16 + skip, 0x08, "----", TimiditySetupWidth - 16 - skip);
	}
#endif

	displaystr (TimiditySetupFirstLine + 4, 0, (focus&&(TimiditySetupEditPos==3))?0x07:0x08, "  OffsetRoom:" + 2 - skip, 16 + skip);
	TimiditySetupDrawBar (focus, 4, skip, (TimiditySetupSelected >= 3) ? TimiditySetupOffsetRoom : -1, 1000, TimiditySetupEditPos == 3);
#if 0
	if (TimiditySetupSelected >= 3)
	{
		snprintf (temp, sizeof (temp), "%4d", TimiditySetupOffsetRoom);
		displaystr (TimiditySetupFirstLine + 4, 16 + skip, (focus&&(TimiditySetupEditPos==3))?0x07:0x08, temp, TimiditySetupWidth - 16 - skip);
	} else {
		displaystr (TimiditySetupFirstLine + 4, 16 + skip, 0x08, "----", TimiditySetupWidth - 16 - skip);
	}
#endif

	displaystr (TimiditySetupFirstLine + 5, 0, (focus&&(TimiditySetupEditPos==4))?0x07:0x08, "  PreDelayFactor:" + 2 - skip, 16 + skip);
	TimiditySetupDrawBar (focus, 5, skip, (TimiditySetupSelected >= 3) ? TimiditySetupPreDelayFactor : -1, 1000, TimiditySetupEditPos == 4);
#if 0
	if (TimiditySetupSelected >= 3)
	{
		snprintf (temp, sizeof (temp), "%4d", TimiditySetupPreDelayFactor);
		displaystr (TimiditySetupFirstLine + 5, 16 + skip, (focus&&(TimiditySetupEditPos==4))?0x07:0x08, temp, TimiditySetupWidth - 16 - skip);
	} else {
		displaystr (TimiditySetupFirstLine + 5, 16 + skip, 0x08, "----", TimiditySetupWidth - 16 - skip);
	}
#endif

	displaystr (TimiditySetupFirstLine + 6, 0, (focus&&(TimiditySetupEditPos==5))?0x07:0x08, "  Delay:" + 2 - skip, 16 + skip);
	TimiditySetupDrawItems (focus, 6, skip, effect_lr_modes, 4, effect_lr_mode + 1, TimiditySetupEditPos==5);

	displaystr (TimiditySetupFirstLine + 7, 0, (focus&&(TimiditySetupEditPos==6))?0x07:0x08, "  Delay ms:" + 2 - skip, 16 + skip);
	TimiditySetupDrawBar (focus, 7, skip, (effect_lr_mode >= 0) ? effect_lr_delay_msec : -1, 1000, TimiditySetupEditPos == 6);
#if 0
	if (effect_lr_mode >= 0)
	{
		snprintf (temp, sizeof (temp), "%4d", effect_lr_delay_msec);
		displaystr (TimiditySetupFirstLine + 7, 16 + skip, (focus&&(TimiditySetupEditPos==6))?0x07:0x08, temp, TimiditySetupWidth - 16 - skip);
	} else {
		displaystr (TimiditySetupFirstLine + 7, 16 + skip, 0x08, "----", TimiditySetupWidth - 16 - skip);
	}
#endif

	displaystr (TimiditySetupFirstLine + 8, 0, (focus&&(TimiditySetupEditPos==7))?0x07:0x08, "  Chorus:" + 2 - skip, 16 + skip);
	TimiditySetupDrawItems (focus, 8, skip, disable_enable, 2, opt_chorus_control, TimiditySetupEditPos==7);
}

static int TimiditySetupIProcessKey(uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('t', "Enable Timidity Setup Viewer");
			cpiKeyHelp('T', "Enable Timidity Setup Viewer");
			break;
		case 't': case 'T':
			TimiditySetupActive=1;
			cpiTextSetMode("TimSetup");
			return 1;
		case 'x': case 'X':
			TimiditySetupActive=1;
			break;
		case KEY_ALT_X:
			TimiditySetupActive=0;
			break;
	}
	return 0;
}

static int TimiditySetupAProcessKey(uint16_t key)
{
	static uint32_t lastpress = 0;
	static int repeat;
	if ((key != KEY_LEFT) && (key != KEY_RIGHT))
	{
		lastpress = 0;
		repeat = 1;
	} else {
		uint32_t newpress = dos_clock();
		if ((newpress-lastpress) > 0x4000) /* 250 ms */
		{
			repeat = 1;
		} else {
			if (repeat < 20)
			{
				repeat += 1;
			}
		}
		lastpress = newpress;
	}
	switch (key)
	{
		case 't': case 'T':
			TimiditySetupActive=!TimiditySetupActive;
			cpiTextRecalc();
			break;

		case KEY_ALT_K:
			cpiKeyHelp('t',       "Disable Timidity Setup Viewer");
			cpiKeyHelp('T',       "Disable Timidity Setup Viewer");
			cpiKeyHelp(KEY_UP,    "Move cursor up");
			cpiKeyHelp(KEY_DOWN,  "Move cursor down");
			return 0;
#if 0
			cpiKeyHelp(KEY_PPAGE, "Scroll SID info viewer up");
			cpiKeyHelp(KEY_NPAGE, "Scroll SID info viewer down");
			cpiKeyHelp(KEY_HOME,  "Scroll SID info viewer to the top");
			cpiKeyHelp(KEY_END,   "Scroll SID info viewer to the bottom");
			return 0;

		case KEY_PPAGE:
			if (SidInfoScroll)
			{
				SidInfoScroll--;
			}
			break;
		case KEY_NPAGE:
			SidInfoScroll++;
			break;
		case KEY_HOME:
			SidInfoScroll=0;
		case KEY_END:
			SidInfoScroll=SidInfoDesiredHeight - SidInfoHeight;
			break;
#endif
		case KEY_LEFT:
			if (TimiditySetupEditPos==0)
			{
				if (TimiditySetupSelected)
				{
					TimiditySetupSelected--;
					if (TimiditySetupSelected)
					{
						opt_reverb_control = -TimiditySetupLevel - TimiditySetupSelected * 128 + 128;
					} else {
						opt_reverb_control = 0;
					}
					init_reverb ();
				}
			} else if (TimiditySetupEditPos==1)
			{
				if ((TimiditySetupSelected != 0) && (TimiditySetupLevel > 1))
				{
					if (repeat > TimiditySetupLevel)
					{
						TimiditySetupLevel = 0;
					} else {
						TimiditySetupLevel -= repeat;
					}
					opt_reverb_control = -TimiditySetupLevel - TimiditySetupSelected * 128 + 128;
					init_reverb ();
				}
			} else if (TimiditySetupEditPos==2)
			{
				if ((TimiditySetupSelected >= 3) && (TimiditySetupScaleRoom > 0))
				{
					if (repeat > TimiditySetupScaleRoom)
					{
						TimiditySetupScaleRoom = 0;
					} else {
						TimiditySetupScaleRoom -= repeat;
					}
					freeverb_scaleroom = (float)TimiditySetupScaleRoom / 100.0f;
					init_reverb ();
				}
			} else if (TimiditySetupEditPos==3)
			{
				if ((TimiditySetupSelected >= 3) && (TimiditySetupOffsetRoom > 0))
				{
					if (repeat > TimiditySetupOffsetRoom)
					{
						TimiditySetupOffsetRoom = 0;
					} else {
						TimiditySetupOffsetRoom -= repeat;
					}
					freeverb_offsetroom = (float)TimiditySetupOffsetRoom / 100.0f;
					init_reverb ();
				}
			} else if (TimiditySetupEditPos==4)
			{
				if ((TimiditySetupSelected >= 3) && (TimiditySetupPreDelayFactor > 0))
				{
					if (repeat > TimiditySetupPreDelayFactor)
					{
						TimiditySetupPreDelayFactor = 0;
					} else {
						TimiditySetupPreDelayFactor -= repeat;
					}
					TimiditySetupPreDelayFactor--;
					reverb_predelay_factor= (float)TimiditySetupPreDelayFactor / 100.0f;
					init_reverb ();
				}
			} else if (TimiditySetupEditPos==5)
			{
				if (effect_lr_mode > -1)
				{
					effect_lr_mode--;
				}
			} else if (TimiditySetupEditPos==6)
			{
				if ((effect_lr_mode >= 0) && (effect_lr_delay_msec > 1))
				{
					if (repeat >= effect_lr_delay_msec)
					{
						effect_lr_delay_msec = 1;
					} else {
						effect_lr_delay_msec -= repeat;
					}
				}
			} else if (TimiditySetupEditPos==7)
			{
				if (opt_chorus_control > 0)
				{
					opt_chorus_control--;
				}
			}
			break;
		case KEY_RIGHT:
			if (TimiditySetupEditPos==0)
			{
				if (TimiditySetupSelected < 4)
				{
					TimiditySetupSelected++;
					if (TimiditySetupSelected)
					{
						opt_reverb_control = -TimiditySetupLevel - TimiditySetupSelected * 128 + 128;
					} else {
						opt_reverb_control = 0;
					}
					init_reverb ();
				}
			} else if (TimiditySetupEditPos==1)
			{
				if ((TimiditySetupSelected != 0) && (TimiditySetupLevel < 127))
				{
					if ((TimiditySetupLevel + repeat) > 127)
					{
						TimiditySetupLevel = 127;
					} else {
						TimiditySetupLevel += repeat;
					}
					opt_reverb_control = -TimiditySetupLevel - TimiditySetupSelected * 128 + 128;
					init_reverb ();
				}
			} else if (TimiditySetupEditPos==2)
			{
				if ((TimiditySetupSelected >= 3) && (TimiditySetupScaleRoom < 1000))
				{
					if ((TimiditySetupScaleRoom + repeat) > 1000)
					{
						TimiditySetupScaleRoom = 1000;
					} else {
						TimiditySetupScaleRoom += repeat;
					}
					freeverb_scaleroom = (float)TimiditySetupScaleRoom / 100.0f;
					init_reverb ();
				}
			} else if (TimiditySetupEditPos==3)
			{
				if ((TimiditySetupSelected >= 3) && (TimiditySetupOffsetRoom < 1000))
				{
					if ((TimiditySetupOffsetRoom + repeat) > 1000)
					{
						TimiditySetupOffsetRoom = 1000;
					} else {
						TimiditySetupOffsetRoom += repeat;
					}
					freeverb_offsetroom = (float)TimiditySetupOffsetRoom / 100.0f;
					init_reverb ();
				}
			} else if (TimiditySetupEditPos==4)
			{
				if ((TimiditySetupSelected >= 3) && (TimiditySetupPreDelayFactor < 1000))
				{
					if ((TimiditySetupPreDelayFactor + repeat) > 1000)
					{
						TimiditySetupPreDelayFactor = 1000;
					} else {
						TimiditySetupPreDelayFactor += repeat;
					}
					reverb_predelay_factor= (float)TimiditySetupPreDelayFactor / 100.0f;
					init_reverb ();
				}
			} else if (TimiditySetupEditPos==5)
			{
				if (effect_lr_mode < 2)
				{
					effect_lr_mode++;
				}
			} else if (TimiditySetupEditPos==6)
			{
				if ((effect_lr_mode >= 0) && (effect_lr_delay_msec < 1000))
				{
					if ((effect_lr_delay_msec + repeat) > 1000)
					{
						effect_lr_delay_msec = 1000;
					} else {
						effect_lr_delay_msec += repeat;
					}
				}
			} else if (TimiditySetupEditPos==7)
			{
				if (opt_chorus_control < 1)
				{
					opt_chorus_control++;
				}
			}
			break;
		case KEY_UP:
			if (TimiditySetupEditPos)
			{
				TimiditySetupEditPos--;
			}
			break;
		case KEY_DOWN:
			if (TimiditySetupEditPos < 7)
			{
				TimiditySetupEditPos++;
			}
			break;
		default:
			return 0;
	}
	return 1;
}

static int TimiditySetupEvent(int ev)
{
	switch (ev)
	{
		case cpievInitAll:
			return 1;
		case cpievInit:
			TimiditySetupActive=1;
			// Here we can allocate memory, return 0 on error
			break;
		case cpievDone:
			// Here we can free memory
			break;
	}
	return 1;
}


static struct cpitextmoderegstruct cpiTimiditySetup = {"TimSetup", TimiditySetupGetWin, TimiditySetupSetWin, TimiditySetupDraw, TimiditySetupIProcessKey, TimiditySetupAProcessKey, TimiditySetupEvent CPITEXTMODEREGSTRUCT_TAIL};

void __attribute__ ((visibility ("internal"))) cpiTimiditySetupInit (void)
{
	if (opt_reverb_control >= 0)
	{
		TimiditySetupSelected = opt_reverb_control;
		TimiditySetupLevel = DEFAULT_REVERB_SEND_LEVEL;
	} else {
		TimiditySetupSelected = (-opt_reverb_control + 128) / 128;
		TimiditySetupLevel = (-opt_reverb_control) & 0x7f;
	}

	cpiTextRegisterMode (&cpiTimiditySetup);
}

void __attribute__ ((visibility ("internal"))) cpiTimiditySetupDone (void)
{
	//cpiTextUnregisterDefMode(&cpiSidInfo);
}