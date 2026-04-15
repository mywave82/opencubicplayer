#include "config.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "stuff/compat.h"
#include "stuff/imsrtns.h"
#include "stuff/poutput.h"
#include "cpiface/cpiface.h"

#include "sndhplay.h"

#define COLTITLE1 0x01
#define COLTITLE1H 0x09

#ifndef MIN
# define MIN(a,b) ((a)<(b))?(a):(b)
#endif

static int sndhConfigActive;
static int sndhConfigFirstLine;
static int sndhConfigHeight;
static int sndhConfigWidth;
static int sndhConfigSelected; // 0 = StereoModel, 1 = Empiric Low-Pass Filter Frequency, 2 = Empiric Low-Pass Filter Q-Factor

static void DrawItems (struct cpifaceSessionAPI_t *cpifaceSession, const int focus, const int lineno, const int xpos, const int width, const char **list, const int listlength, const int selected, const int active, const int disabled)
{
	int i;
	int used = 0;

	if (disabled)
	{
		cpifaceSession->console->Driver->DisplayStr (lineno, xpos, 0x08, "  ----", width);
		return;
	}

#warning THIS CODE ASSUMES ALL ITEMS WILL FIT!!!!

	for (i=0; i < listlength; i++)
	{
		int l = strlen (list[i]);
		if (selected == i)
		{
			cpifaceSession->console->DisplayPrintf (lineno, xpos + used, (focus&&active)?0x09:0x01, l + 2, "[%.*o%s%.*o]", (focus&&active)?0x0f:0x07, list[i], (focus&&active)?0x09:0x01);
		} else {
			cpifaceSession->console->DisplayPrintf (lineno, xpos + used, 0x00, l + 2, " %.*o%s%.0o ", (focus&&active)?0x07:0x08, list[i]);
		}
		used += l + 2;
	}
	cpifaceSession->console->Driver->DisplayStr (lineno, xpos + used, 0x00, " ", width - used);
}

static void DrawBar (struct cpifaceSessionAPI_t *cpifaceSession, const int focus, const int lineno, const int xpos, const int width, int scale, const char *suffix, int minlevel, int maxlevel, int level, const int active, const int disabled)
{
	char prefix[11];
	char min[8];
	char max[7];

	unsigned int abslevel;
	unsigned int absmin;

	int pos, p1, p2;

	level    = saturate (level,    -99999, 99999);
	minlevel = saturate (minlevel, -99999, 99999);
	maxlevel = saturate (maxlevel,      0, 99999);
	abslevel = abs(level);
	absmin   = abs(minlevel);

	assert ((scale == 1) || (scale == 10) || (scale == 100));

	if (disabled)
	{
		cpifaceSession->console->Driver->DisplayStr (lineno, xpos, 0x08, "  ----", width);
		return;
	}

	if (scale == 100)
	{
		snprintf (prefix, sizeof (prefix), "%3d.%02d%s",
			level / scale,
			abslevel % scale,
			suffix);

		snprintf (min, sizeof (min), "%3d.%02d",
			minlevel / scale,
			absmin % scale);

		snprintf (max, sizeof (max), "%3d.%02d",
			maxlevel / scale,
			maxlevel % scale);
	} else if (scale == 10)
	{
		snprintf (prefix, sizeof (prefix), "%4d.%01d%s",
			level / scale,
			abslevel % scale,
			suffix);

		snprintf (min, sizeof (min), "%4d.%01d",
			minlevel / scale,
			absmin % scale);

		snprintf (max, sizeof (max), "%4d.%01d",
			maxlevel / scale,
			maxlevel % scale);
	} else {
		snprintf (prefix, sizeof (prefix), "%6d%s",
			level,
			suffix);

		snprintf (min, sizeof (min), "%6d",
			minlevel);

		snprintf (max, sizeof (max), "%6d",
			maxlevel);
	}

#define _22 (width - 28)
	pos = (/*(maxlevel - minlevel / 46) - 1 +*/ (level - minlevel) * _22) / (maxlevel - minlevel);

	p1 = pos;
        p2 = _22 - pos;

	cpifaceSession->console->DisplayPrintf (lineno, xpos, (active)?0x07:0x08, width, "%10s%-7s [%*C.#%*C.] %-6s", prefix, min, p1, p2, max);
#undef _22
}


static void sndhConfigDraw (struct cpifaceSessionAPI_t *cpifaceSession, int focus)
{
	enum sndhStereoModels StereoModel;
	int FIR_length;
	int StereoBalanceA;
	int StereoBalanceB;
	int StereoBalanceC;
	int StereoEmpiricLPF_cutoff;
	int StereoEmpiricLPF_Qpct;
	int line = 0;
	int skip;

	if (sndhConfigWidth >= 83)
	{
		skip = 2;
	} else if (sndhConfigWidth >= 81)
	{
		skip = 1;
	} else {
		skip = 0;
	}

	sndhGetStereoModel (&StereoModel, &FIR_length, &StereoBalanceA, &StereoBalanceB, &StereoBalanceC, &StereoEmpiricLPF_cutoff, &StereoEmpiricLPF_Qpct);

	if (StereoModel == STEREO_BALANCE)
	{
		if (sndhConfigSelected >= 5)
		{
			sndhConfigSelected = 0;
		}
	} else if (StereoModel == STEREO_EMPIRIC_LPF)
	{
		if (sndhConfigSelected >= 4)
		{
			sndhConfigSelected = 0;
		}
	}
	else
	{
		if (sndhConfigSelected > 1)
		{
			sndhConfigSelected = 0;
		}
	}

	cpifaceSession->console->Driver->DisplayStr(sndhConfigFirstLine + line, 0, focus?COLTITLE1H:COLTITLE1, focus?"psgplay stereo model":"psgplay stereo model (press <i> to focus)", sndhConfigWidth);
	line = 1;

	if ((sndhConfigHeight >= 6) || (sndhConfigSelected == 0))
	{
		const char *models[] = {"Balance", "Linear", "Empiric", "Empiric+LPF"};
		cpifaceSession->console->Driver->DisplayStr (sndhConfigFirstLine + line, 0, (focus && (sndhConfigSelected == 0)) ? 0x07 : 0x08, &"  StereoModel:"[2 - skip], 27 + skip);
		DrawItems (cpifaceSession, focus, sndhConfigFirstLine + line, 0 + 27 + skip, sndhConfigWidth - 27 - skip, models, 4, StereoModel, sndhConfigSelected==0, 0);
		line++;
		if (line >= sndhConfigHeight)
		{
			return;
		}
	}
	if ((sndhConfigHeight >= 5) || (sndhConfigSelected == 1))
	{
		cpifaceSession->console->Driver->DisplayStr (sndhConfigFirstLine + line, 0, (focus && (sndhConfigSelected == 1)) ? 0x07 : 0x08, &"  FIR length:"[2 - skip], 27 + skip);
		DrawBar (cpifaceSession, focus, sndhConfigFirstLine + line, 0 + 27 + skip, sndhConfigWidth - 27 - skip, 1, "", 1, 16, FIR_length, sndhConfigSelected == 1, 0);
		line++;
		if (line >= sndhConfigHeight)
		{
			return;
		}
	}

	if (StereoModel == STEREO_BALANCE)
	{
		if ((sndhConfigHeight >= 4) || (sndhConfigSelected == 2))
		{
			cpifaceSession->console->Driver->DisplayStr (sndhConfigFirstLine + line, 0, (focus && (sndhConfigSelected == 2)) ? 0x07 : 0x08, &"  Balance PSG.A:"[2 - skip], 27 + skip);
			DrawBar (cpifaceSession, focus, sndhConfigFirstLine + line, 0 + 27 + skip, sndhConfigWidth - 27 - skip, 1, "", BALANCE_PCT_MIN, BALANCE_PCT_MAX, StereoBalanceA, sndhConfigSelected == 2, StereoModel != STEREO_BALANCE);
			line++;
			if (line >= sndhConfigHeight)
			{
				return;
			}
		}
		if ((sndhConfigHeight >= 3) || (sndhConfigSelected == 3))
		{
			cpifaceSession->console->Driver->DisplayStr (sndhConfigFirstLine + line, 0, (focus && (sndhConfigSelected == 2)) ? 0x07 : 0x08, &"  Balance PSG.B:"[2 - skip], 27 + skip);
			DrawBar (cpifaceSession, focus, sndhConfigFirstLine + line, 0 + 27 + skip, sndhConfigWidth - 27 - skip, 1, "", BALANCE_PCT_MIN, BALANCE_PCT_MAX, StereoBalanceB, sndhConfigSelected == 3, StereoModel != STEREO_BALANCE);
			line++;
			if (line >= sndhConfigHeight)
			{
				return;
			}
		}
		if ((sndhConfigHeight >= 2) || (sndhConfigSelected == 4))
		{
			cpifaceSession->console->Driver->DisplayStr (sndhConfigFirstLine + line, 0, (focus && (sndhConfigSelected == 2)) ? 0x07 : 0x08, &"  Balance PSG.C:"[2 - skip], 27 + skip);
			DrawBar (cpifaceSession, focus, sndhConfigFirstLine + line, 0 + 27 + skip, sndhConfigWidth - 27 - skip, 1, "", BALANCE_PCT_MIN, BALANCE_PCT_MAX, StereoBalanceC, sndhConfigSelected == 4, StereoModel != STEREO_BALANCE);
			line++;
			if (line >= sndhConfigHeight)
			{
				return;
			}
		}
	} else if (StereoModel == STEREO_EMPIRIC_LPF)
	{
		if ((sndhConfigHeight >= 3) || (sndhConfigSelected == 2))
		{
			cpifaceSession->console->Driver->DisplayStr (sndhConfigFirstLine + line, 0, (focus && (sndhConfigSelected == 2)) ? 0x07 : 0x08, &"  LPF Frequency:"[2 - skip], 27 + skip);
			DrawBar (cpifaceSession, focus, sndhConfigFirstLine + line, 0 + 27 + skip, sndhConfigWidth - 27 - skip, 1, "Hz", LPF_FREQ_MIN, LPF_FREQ_MAX, StereoEmpiricLPF_cutoff, sndhConfigSelected == 2, StereoModel != STEREO_EMPIRIC_LPF);
			line++;
			if (line >= sndhConfigHeight)
			{
				return;
			}
		}
		if ((sndhConfigHeight >= 2) || (sndhConfigSelected == 3))
		{
			cpifaceSession->console->Driver->DisplayStr (sndhConfigFirstLine + line, 0, (focus&&(sndhConfigSelected == 3)) ? 0x07 : 0x08, &"  LPF Q-factor:"[2 - skip], 27 + skip);	
			DrawBar (cpifaceSession, focus, sndhConfigFirstLine + line, 0 + 27 + skip, sndhConfigWidth - 27 - skip, 100, "", LPF_QPCT_MIN, LPF_QPCT_MAX, StereoEmpiricLPF_Qpct, sndhConfigSelected == 3, StereoModel != STEREO_EMPIRIC_LPF);
			line++;
			if (line >= sndhConfigHeight)
			{
				return;
			}
		}
	}
	while (line < sndhConfigHeight)
	{
		cpifaceSession->console->Driver->DisplayVoid (sndhConfigFirstLine + line, 0, sndhConfigWidth);
		line++;
	}
}

static int sndhConfigGetWin (struct cpifaceSessionAPI_t *cpifaceSession, struct cpitextmodequerystruct *q)
{
	struct sndhMeta_t *meta;

	if (!sndhConfigActive)
		return 0;

	meta = sndhGetMeta ();
	if (!meta)
	{
		sndhConfigActive = 0;
		return 0;
	}

	q->hgtmin = 2;
	q->hgtmax = 6;
	if (q->hgtmin > q->hgtmax)
	{
		q->hgtmin = q->hgtmax;
	}

	q->xmode=1;
	q->size=2;
	q->top=0;
	q->killprio=64;
	q->viewprio=200;
	return 1;
}

static void sndhConfigSetWin (struct cpifaceSessionAPI_t *cpifaceSession, int _ignore, int wid, int ypos, int hgt)
{
	sndhConfigFirstLine = ypos;
	sndhConfigHeight = hgt;
	sndhConfigWidth = wid;
}

static int sndhConfigIProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpifaceSession->KeyHelp ('t', "Enable stereo model configurator");
			cpifaceSession->KeyHelp ('T', "Enable stereo model configurator");
			break;
		case 'i': case 'I':
			sndhConfigActive = 1;
			cpifaceSession->cpiTextSetMode (cpifaceSession, "sndhCfg");
			return 1;
		case 'x': case 'X':
			sndhConfigActive = 1;
			break;
		case KEY_ALT_X:
			sndhConfigActive = 0;
			break;
	}
	return 0;
}

static int sndhConfigAProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	enum sndhStereoModels StereoModel;
	int FIR_length;
	int StereoBalanceA;
	int StereoBalanceC;
	int StereoBalanceB;
	int StereoEmpiricLPF_cutoff;
	int StereoEmpiricLPF_Qpct;

	sndhGetStereoModel (&StereoModel, &FIR_length, &StereoBalanceA, &StereoBalanceB, &StereoBalanceC, &StereoEmpiricLPF_cutoff, &StereoEmpiricLPF_Qpct);

	static uint16_t lastkey = 0;
	static uint32_t lastpress = 0;
	static int repeat = 0, warmup = 0;

	if ((key != KEY_LEFT) && (key != KEY_RIGHT) && (key != lastkey))
	{
		lastkey = key;
		lastpress = 0;
		repeat = 1;
		warmup = 0;
	} else {
		uint32_t newpress = clock_ms();
		if ((newpress-lastpress) > 250) /* 125 ms */
		{
			repeat = 1;
			warmup = 0;
		} else {
			if (warmup < 5)
			{
				warmup++;
			} else if ((sndhConfigSelected == 2) && (repeat < 100))
			{
				repeat += 2;
			} else if ((sndhConfigSelected == 3) && (repeat < 5))
			{
				repeat++;
				warmup = 3;
			}
		}
		lastpress = newpress;
	}

	switch (key)
	{
		case 'i': case 'I':
			sndhConfigActive = !sndhConfigActive;
			cpifaceSession->cpiTextRecalc (cpifaceSession);
			break;

		case KEY_ALT_K:
			cpifaceSession->KeyHelp ('i',       "Disable stereo model configurator");
			cpifaceSession->KeyHelp ('I',       "Disable stereo model configurator");
			cpifaceSession->KeyHelp (KEY_LEFT,  "Adjust selected item in stereo model configurator");
			cpifaceSession->KeyHelp (KEY_RIGHT, "Adjust selected item in stereo model configurator");
			cpifaceSession->KeyHelp (KEY_UP,    "Change selected item in stereo model configurator");
			cpifaceSession->KeyHelp (KEY_DOWN,  "Change selected item in stereo model configurator");
			return 0;

		case KEY_UP:
			if (sndhConfigSelected)
			{
				sndhConfigSelected--;
			}
			break;

		case KEY_DOWN:
			if (StereoModel == STEREO_BALANCE)
			{
				if (sndhConfigSelected < 4)
				{
					sndhConfigSelected++;
				}
			} else if (StereoModel == STEREO_EMPIRIC_LPF)
			{
				if (sndhConfigSelected < 3)
				{
					sndhConfigSelected++;
				}
			} else {
				if (sndhConfigSelected < 1)
				{
					sndhConfigSelected++;
				}
			}
			break;

		case KEY_LEFT:
			switch (sndhConfigSelected)
			{
				case 0:
					if (StereoModel > 0)
					{
						StereoModel--;
					}
					break;
				case 1:
					if (FIR_length > 1)
					{
						FIR_length--;
					}
					break;
				case 2:
					if (StereoModel == STEREO_BALANCE)
					{
						StereoBalanceA -= repeat;
						if (StereoBalanceA < BALANCE_PCT_MIN)
						{
							StereoBalanceA = BALANCE_PCT_MIN;
						}
					} else if (StereoModel == STEREO_EMPIRIC_LPF)
					{
						StereoEmpiricLPF_cutoff -= repeat;
						if (StereoEmpiricLPF_cutoff < LPF_FREQ_MIN)
						{
							StereoEmpiricLPF_cutoff = LPF_FREQ_MIN;
						}
					}
					break;
				case 3:
					if (StereoModel == STEREO_BALANCE)
					{
						StereoBalanceB -= repeat;
						if (StereoBalanceB < BALANCE_PCT_MIN)
						{
							StereoBalanceB = BALANCE_PCT_MIN;
						}
					} else if (StereoModel == STEREO_EMPIRIC_LPF)
					{
						StereoEmpiricLPF_Qpct -= repeat;
						if (StereoEmpiricLPF_Qpct < LPF_QPCT_MIN)
						{
							StereoEmpiricLPF_Qpct = LPF_QPCT_MIN;
						}
					}
					break;
				case 4:
					if (StereoModel == STEREO_BALANCE)
					{
						StereoBalanceC -= repeat;
						if (StereoBalanceC < BALANCE_PCT_MIN)
						{
							StereoBalanceC = BALANCE_PCT_MIN;
						}
					}
					break;
			}
			sndhSetStereoModel (cpifaceSession, StereoModel, FIR_length, StereoBalanceA, StereoBalanceB, StereoBalanceC, StereoEmpiricLPF_cutoff, StereoEmpiricLPF_Qpct);
			break;

		case KEY_RIGHT:
			switch (sndhConfigSelected)
			{
				case 0:
					if (StereoModel < 3)
					{
						StereoModel++;
					}
					break;
				case 1:
					if (FIR_length < 16)
					{
						FIR_length++;
					}
					break;
				case 2:
					if (StereoModel == STEREO_BALANCE)
					{
						StereoBalanceA += repeat;
						if (StereoBalanceA > BALANCE_PCT_MAX)
						{
							StereoBalanceA = BALANCE_PCT_MAX;
						}
					} else if (StereoModel == STEREO_EMPIRIC_LPF)
					{
						StereoEmpiricLPF_cutoff += repeat;
						if (StereoEmpiricLPF_cutoff > LPF_FREQ_MAX)
						{
							StereoEmpiricLPF_cutoff = LPF_FREQ_MAX;
						}
					}
					break;
				case 3:
					if (StereoModel == STEREO_BALANCE)
					{
						StereoBalanceB += repeat;
						if (StereoBalanceB > BALANCE_PCT_MAX)
						{
							StereoBalanceB = BALANCE_PCT_MAX;
						}
					} else if (StereoModel == STEREO_EMPIRIC_LPF)
					{
						StereoEmpiricLPF_Qpct += repeat;
						if (StereoEmpiricLPF_Qpct > LPF_QPCT_MAX)
						{
							StereoEmpiricLPF_Qpct = LPF_QPCT_MAX;
						}
					}
					break;
				case 4:
					if (StereoModel == STEREO_BALANCE)
					{
						StereoBalanceC += repeat;
						if (StereoBalanceC > BALANCE_PCT_MAX)
						{
							StereoBalanceC = BALANCE_PCT_MAX;
						}
					}
					break;
			}
			sndhSetStereoModel (cpifaceSession, StereoModel, FIR_length, StereoBalanceA, StereoBalanceB, StereoBalanceC, StereoEmpiricLPF_cutoff, StereoEmpiricLPF_Qpct);
			break;

		default:
			return 0;
	}

	return 1;
}

static int sndhConfigEvent (struct cpifaceSessionAPI_t *cpifaceSession, int ev)
{
	switch (ev)
	{
		case cpievInitAll:
			return 1;
		case cpievInit:
			sndhConfigActive=1;
			// Here we can allocate memory, return 0 on error
			break;
		case cpievDone:
			// Here we can free memory
			break;
	}
	return 1;
}


static struct cpitextmoderegstruct cpiSndhConfig = {"sndhCfg", sndhConfigGetWin, sndhConfigSetWin, sndhConfigDraw, sndhConfigIProcessKey, sndhConfigAProcessKey, sndhConfigEvent CPITEXTMODEREGSTRUCT_TAIL};

OCP_INTERNAL void sndhConfigInit (struct cpifaceSessionAPI_t *cpifaceSession)
{
	cpifaceSession->cpiTextRegisterMode (cpifaceSession, &cpiSndhConfig);
}

OCP_INTERNAL void sndhConfigDone (struct cpifaceSessionAPI_t *cpifaceSession)
{
	cpifaceSession->cpiTextUnregisterMode (cpifaceSession, &cpiSndhConfig);
}
