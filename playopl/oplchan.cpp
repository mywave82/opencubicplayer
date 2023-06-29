#include "config.h"
#include <math.h>
#include <string.h>
#include "types.h"
#include "boot/psetting.h"
#include "cpiface/cpiface.h"
#include "playopl/ocpemu.h"
#include "playopl/oplplay.h"
#include "stuff/poutput.h"

#warning move this static into the session
static unsigned int OPLChannelType     = 3;
static unsigned int OPLChanFirstLine   = 0;
static unsigned int OPLChanStartCol    = 0;
static unsigned int OPLChanHeight      = 0;
static unsigned int OPLChanWidth       = 0;
#define OPLSelectedChannel cpifaceSession->SelectedChannel
//static unsigned int OPLSelectedChannel = 0;

#if 0
123456789012345678901234567890123456 36
Nte Shpe ADSR
C#2 SINE ADSR + C#2 HALF ADSR ##|##     <- 2 ch mode
C#2    +  C#2 + C#2 +  C#2    ##|##     <- 4 ch mode
C#2 Bass-Drum - C#2 Bass-Drum ##|##
C#2 Hi-Hat    + C#2 Snare-Drum##|##     // we combine the volumes
C#2 Tom-Tom   + C#2 Cymbal    ##|##     // we combine the volumes

12345678901234567890123456789012345678901234 44
C#2 SINE ADSR Vi+ C#2 HALF ADSR Vi####|####     <- 2 ch mode
C#2     +  C#2  + C#2  +  C#2     ####|####     <- 4 ch mode
C#2 Bass-Drum   - C#2 Bass-Drum   ####|####
C#2 Hi-Hat      + C#2 Snare-Drum  ####|####
C#2 Tom-Tom     + C#2 Cymbal      ####|####

12345678901234567890123456789012345678901234567890123456789012 62
C#2 SINE ADSR ViTr E K + C#2 HALF ADSR ViTr E K ######|######     <- 2 ch mode
C#2 SINE    + C#2 SINE + C#2 SINE + C#2 SINE    ######|######     <- 4 ch mode
C#2 Bass-Drum ADR DUAL - C#2 Bass-Drum  ADR DUAL######|######
C#2 Hi-Hat    ADR DUAL + C#2 Snare-Drum ADR DUAL######|######
C#2 Tom-Tom   ADR DUAL + C#2 Cymbal     ADR DUAL######|######

1234567890123456789012345678901234567890123456789012345678901234567890123456 76
C#2 SINE ADSR Vi Tr E K     + C#2 HALF ADSR Vi Tr E K     ########|########     <- 2 ch mode
C#2 SIN ADSR + C#2 SIN ADSR + C#2 SIN ADSR + C#2 SIN ADSR ########|########     <- 4 ch mode
C#2 Bass-Drum ADR DUAL      - C#2 Bass-Drum  ADR DUAL     ########|########
C#2 Hi-Hat    ADR DUAL      + C#2 Snare-Drum ADR DUAL     ########|########
C#2 Tom-Tom   ADR DUAL      + C#2 Cymbal     ADR DUAL     ########|########

12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678 128
C#2 SINE ADSR Vi Tr E K                           + C#2 HALF ADSR Vi Tr E K                           #########|######### AM
C#2 SINE ADSR Vi Tr E K + C#2 SINE ADSR Vi Tr E K + C#2 SINE ADSR Vi Tr E K + C#2 SINE ADSR Vi Tr E K #########|######### AM-AM
C#2 Bass-Drum ADR DUAL  - C#2 Bass-Drum ADR DUAL                                                      #########|#########
C#2 Hi-Hat    ADR DUAL                            +  C#2 Snare-Drum ADR DUAL                          #########|#########
C#2 Tom-Tom   ADR DUAL                            +  C#2 Cymbal     ADR DUAL                          #########|#########

If a channel is in OPL3 multiuse mode, we borrow two operators, and disable a channel
02   1    4    7   10
..
05   -    -    -    -

If we enable purcussion mode, we replace the view slightly, channel 7-9 (index 6-8), only leaves the freqency, rest of the parameters are gone
BD  12    15
HH  13 SD 16
TT  14 CY 17

channel mode:
00  disabled due to OPL3 4-op mode
01  OPL2 2-op mode AM+AM
02  OPL2 2-op mode AM*AM        (FM mode)
03  OPL3 4-op mode AM*AM*AM*AM  (FM-FM mode)
04  OPL3 4-op mode AM+AM*AM*AM  (AM-FM mode)
05  OPL3 4-op mode AM*AM+AM*AM  (FM-AM mode)
06  OPL3 4-op mode AM+AM*AM+AM  (AM-AM mode)
07  percussion mode (DUAL_OPL2 will allow dual mapping of these)
#endif

static void OPLChanDisplay36Header (struct cpifaceSessionAPI_t *cpifaceSession, const int line, const int left)
{
	cpifaceSession->console->Driver->DisplayStr (line, left, 0x07, "nte shap ADSR   nte shap ADSR   \xb3   ", 36);
}
static void OPLChanDisplay44Header (struct cpifaceSessionAPI_t *cpifaceSession, const int line, const int left)
{
	cpifaceSession->console->Driver->DisplayStr (line, left, 0x07, "nte shap ADSR Vi  nte shap ADSR Vi    \xb3     ", 44);
}
static void OPLChanDisplay62Header (struct cpifaceSessionAPI_t *cpifaceSession, const int line, const int left)
{
	cpifaceSession->console->Driver->DisplayStr (line, left, 0x07, "nte shap ADSR ViTr E K   nte shap ADSR ViTr E K       \xb3       ", 62);
}
static void OPLChanDisplay76Header (struct cpifaceSessionAPI_t *cpifaceSession, const int line, const int left)
{
	cpifaceSession->console->Driver->DisplayStr (line, left, 0x07, "nte shap ADSR Vi Tr E K       nte shap ADSR Vi Tr E K             \xb3         ", 76);
}
static void OPLChanDisplay128Header (struct cpifaceSessionAPI_t *cpifaceSession, const int line, const int left)
{
	cpifaceSession->console->Driver->DisplayStr (line, left, 0x07, "nte shap ADSR Vi Tr E K   ntw shap ADSR Vi Tr E K   nte shap ADSR Vi Tr E K   nte shap ADSR Vi Tr E K          \xb3          mode  ", 128);
}

static const char *getnote (struct cpifaceSessionAPI_t *cpifaceSession, int freq)
{
	// Relative to A-4
	int n = (int)round(12.0f*log2((float)freq/440.0f));
	n += 12*7 + 9;
	return cpifaceSession->plNoteStr(n);
}

static void getvolsub (struct oplStatus *s, const int ch, const int o, unsigned int *left, unsigned int *right)
{
	uint32_t t = s->channel[ch].op[o].EnvelopePosition * (64-s->channel[ch].op[o].output_level);
	t >>= 20;
	*left = s->channel[ch].left ? t : 0;
	*right = s->channel[ch].right ? t : 0;
}

static void getvol (struct oplStatus *s, const int ch, unsigned int *left, unsigned int *right)
{
	*left = 0;
	*right = 0;

	switch (s->channel[ch].CM)
	{
		unsigned int l0, l1, l2, r0, r1, r2;
		default:
			break;
		case CM_PERCUSSION:
		case CM_2OP_AM:
			getvolsub (s, ch, 0, &l0, &r0);
			getvolsub (s, ch, 1, &l1, &r1);
			*left = l0 + l1;
			*right = r0 + r1;
			break;
		case CM_2OP_FM:
			getvolsub (s, ch, 1, left, right);
			break;
		case CM_4OP_FM_FM:
			if ((ch < 3) || ((ch >= 9) && (ch < 12)))
			{
				getvolsub (s, ch+3, 1, left, right);
			}
			break;
		case CM_4OP_AM_FM:
			if ((ch < 3) || ((ch >= 9) && (ch < 12)))
			{
				getvolsub (s, ch  , 0, &l0, &r0);
				getvolsub (s, ch+3, 1, &l1, &r1);
				*left = l0 + l1;
				*right = r0 + r1;
			}
		case CM_4OP_FM_AM:
			if ((ch < 3) || ((ch >= 9) && (ch < 12)))
			{
				getvolsub (s, ch  , 1, &l0, &r0);
				getvolsub (s, ch+3, 1, &l1, &r1);
				*left = l0 + l1;
				*right = r0 + r1;
			}
			break;
		case CM_4OP_AM_AM:
			if ((ch < 3) || ((ch >= 9) && (ch < 12)))
			{
				getvolsub (s, ch  , 0, &l0, &r0);
				getvolsub (s, ch+3, 0, &l1, &r1);
				getvolsub (s, ch+3, 1, &l2, &r2);
				*left = l0 + l1 + l2;
				*right = r0 + r1 + r2;
			}
			break;
	}
	if (*left  > 256) *left  = 256;
	if (*right > 256) *right = 256;
}

static const char *shap3[16] =
{
	"sin", "Hsn", "Asn", "saw",
	"odd", "Aod", "sqr", "drv"
};

static const char *shap4[16] =
{
	"sine", "Hsin", "Asin", "saw ",
	"odd ", "Aodd", "sqre", "deri"
};

const char *drum0(int ch)
{
	switch (ch % 3)
	{
		default:
		case 0: return "Bass-Drum";
		case 1: return "Hi-Hat";
		case 2: return "Tom-Tom";
	}
}
const char *drum1(int ch)
{
	switch (ch % 3)
	{
		default:
		case 0: return "Bass-Drum";
		case 1: return "Snare-Drum";
		case 2: return "Cymbal";
	}
}

static void PrepareNte (struct cpifaceSessionAPI_t *cpifaceSession, const char **nte, struct oplStatus *s, int ch)
{
	uint_fast32_t frq[4];
	const uint8_t mt[] = {1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 20, 24, 24, 30, 30};

	frq[0] = (uint_fast32_t)s->channel[ch].frequency_number * 49716;
	frq[1] = frq[0];
	frq[2] = frq[0];
	frq[3] = frq[0];

	if (s->channel[ch].op[0].EnvelopeState == 0)
	{
		nte[0] = "---";
	} else {
		frq[0] = (frq[0] * mt[s->channel[ch].op[0].frequency_multiplication_factor]) >> 1;
		frq[0] >>= (20-s->channel[ch].block_number);
		nte[0] = getnote (cpifaceSession, frq[0]);
	}

	if (s->channel[ch].op[1].EnvelopeState == 0)
	{
		nte[1] = "---";
	} else {
		frq[1] = (frq[1] * mt[s->channel[ch].op[1].frequency_multiplication_factor]) >> 1;
		frq[1] >>= (20-s->channel[ch].block_number);
		nte[1] = getnote (cpifaceSession, frq[1]);
	}

	if ((s->channel[ch].CM >= CM_4OP_FM_FM) &&
	    (s->channel[ch].CM <= CM_4OP_AM_AM))
	{
		if (s->channel[ch+3].op[0].EnvelopeState == 0)
		{
			nte[2] = "---";
		} else {
			frq[2] = (frq[2] * mt[s->channel[ch+3].op[0].frequency_multiplication_factor]) >> 1;
			frq[2] >>= (20-s->channel[ch].block_number);
			nte[2] = getnote (cpifaceSession, frq[2]);
		}

		if (s->channel[ch+3].op[1].EnvelopeState == 0)
		{
			nte[3] = "---";
		} else {
			frq[3] = (frq[3] * mt[s->channel[ch+3].op[1].frequency_multiplication_factor]) >> 1;
			frq[3] >>= (20-s->channel[ch].block_number);
			nte[3] = getnote (cpifaceSession, frq[3]);
		}
	}
}

#define ADSR_fmt "%.*o%1X%.*o%1X%.*o%1X%.*o%1X%.7o"
#define ADSR_data(c,o) \
	s->channel[c].op[o].EnvelopeState==STATE_ATTACK ?9:7, s->channel[c].op[o].attack_rate,   \
	s->channel[c].op[o].EnvelopeState==STATE_DELAY  ?9:7, s->channel[c].op[o].decay_rate,    \
	s->channel[c].op[o].EnvelopeState==STATE_SUSTAIN?9:7, s->channel[c].op[o].sustain_level, \
	s->channel[c].op[o].EnvelopeState==STATE_RELEASE?9:7, s->channel[c].op[o].release_rate

#define ADR_fmt "%.*o%1X%.*o%1X%.*o%1X%.7o"
#define ADR_data(c,o) \
	s->channel[c].op[o].EnvelopeState==STATE_ATTACK ?9:7, s->channel[c].op[o].attack_rate,   \
	s->channel[c].op[o].EnvelopeState==STATE_DELAY  ?9:7, s->channel[c].op[o].decay_rate,    \
	s->channel[c].op[o].EnvelopeState==STATE_RELEASE?9:7, s->channel[c].op[o].release_rate

#define Vi_fmt "%.2o%s"
#define Vi_data(c,o) \
	s->channel[c].op[o].vibrato_enabled?(s->Vibrato[c/9]?"~7":"~D"):"  "

#define Tr_fmt "%.9o%s"
#define Tr_data(c,o) \
	s->channel[c].op[o].tremolo_enabled?(s->Tremolo[c/9]?"\xa9""1":"\xa9""5"):"  "

static void OPLChanDisplay36 (struct cpifaceSessionAPI_t *cpifaceSession, const int line, const int left, struct oplStatus *s, int ch)
{
	if (s->channel[ch].CM == CM_disabled)
	{
		cpifaceSession->console->Driver->DisplayStr (line, left, 0x08, " --- (disabled)                 \xb3", 36);
		return;
	}

	const char *nte[4];
	PrepareNte (cpifaceSession, nte, s, ch);

	unsigned int vleft = 0;
	unsigned int vright = 0;
	getvol (s, ch, &vleft, &vright);
	vleft = (vleft + 127) / 128;
	vright = (vright + 127) / 128;

	if ((s->channel[ch].CM == CM_2OP_AM) ||
	    (s->channel[ch].CM == CM_2OP_FM))
	{
		cpifaceSession->console->DisplayPrintf (line, left, 0x07, 36,
			"%3s %4s " ADSR_fmt " %c "
			"%3s %4s " ADSR_fmt " "
			"%.*o\xfe%.*o\xfe%.7o\xb3%.*o\xfe%.*o\xfe"
			" ",

			nte[0], shap4[s->channel[ch].op[0].waveform_select],
			ADSR_data (ch, 0),
			(s->channel[ch].CM == CM_2OP_AM)?'+':'*',

			nte[1], shap4[s->channel[ch].op[1].waveform_select],
			ADSR_data (ch, 1),

			vleft >=2?0xb:0x0,
			vleft >=1?0x1:0x0,

			vright>=1?0x1:0x0,
			vright>=2?0xb:0x0
		);
	} else if ((s->channel[ch].CM >= CM_4OP_FM_FM) &&
	           (s->channel[ch].CM <= CM_4OP_AM_AM))
	{
		cpifaceSession->console->DisplayPrintf (line, left, 0x07, 36,
			"%3s    %c  %3s %c %3s %c  %3s    "
			"%.*o\xfe%.*o\xfe%.7o\xb3%.*o\xfe%.*o\xfe"
			" ",

			nte[0],
			((s->channel[ch].CM == CM_4OP_AM_FM) || (s->channel[ch].CM == CM_4OP_AM_AM))?'+':'*',
			nte[1],
			(s->channel[ch].CM == CM_4OP_FM_AM)?'+':'*',
			nte[2],
			(s->channel[ch].CM == CM_4OP_AM_AM)?'+':'*',
			nte[3],

			vleft >=2?0xb:0x0,
			vleft >=1?0x1:0x0,

			vright>=1?0x1:0x0,
			vright>=2?0xb:0x0
		);
	} else {
		cpifaceSession->console->DisplayPrintf (line, left, 0x07, 36,
			"%3s %9s - %3s %10s"
			"%.*o\xfe%.*o\xfe%.7o\xb3%.*o\xfe%.*o\xfe"
			" ",

			nte[0],
			drum0(ch),
			nte[1],
			drum1(ch),

			vleft >=2?0xb:0x0,
			vleft >=1?0x1:0x0,

			vright>=1?0x1:0x0,
			vright>=2?0xb:0x0
		);
	}
}

static void OPLChanDisplay44 (struct cpifaceSessionAPI_t *cpifaceSession, const int line, const int left, struct oplStatus *s, int ch)
{
	if (s->channel[ch].CM == CM_disabled)
	{
		cpifaceSession->console->Driver->DisplayStr (line, left, 0x08, "--- (disabled)                        \xb3", 44);
		return;
	}

	const char *nte[4];
	PrepareNte (cpifaceSession, nte, s, ch);

	unsigned int vleft;
	unsigned int vright;
	getvol (s, ch, &vleft, &vright);
	vleft = (vleft + 63) / 64;
	vright = (vright + 63) / 64;

	if ((s->channel[ch].CM == CM_2OP_AM) ||
	    (s->channel[ch].CM == CM_2OP_FM))
	{
		cpifaceSession->console->DisplayPrintf (line, left, 0x07, 44,
			"%3s %4s " ADSR_fmt " " Vi_fmt "%.7o%c "
			"%3s %4s " ADSR_fmt " " Vi_fmt
			"%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.7o\xb3%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe"
			" ",

			nte[0], shap4[s->channel[ch].op[0].waveform_select],
			ADSR_data (ch, 0),
			Vi_data (ch, 0),

			(s->channel[ch].CM == CM_2OP_AM)?'+':'*',

			nte[1], shap4[s->channel[ch].op[1].waveform_select],
			ADSR_data (ch, 1),
			Vi_data (ch, 1),

			vleft >=4?0xf:0x0,
			vleft >=3?0xb:0x0,
			vleft >=2?0x9:0x0,
			vleft >=1?0x1:0x0,

			vright>=1?0x1:0x0,
			vright>=2?0x9:0x0,
			vright>=3?0xb:0x0,
			vright>=4?0xf:0x0
		);
	} else if ((s->channel[ch].CM >= CM_4OP_FM_FM) &&
	           (s->channel[ch].CM <= CM_4OP_AM_AM))
	{
		cpifaceSession->console->DisplayPrintf (line, left, 0x07, 44,
			"%3s     %c  %3s  %c "
			"%3s     %c  %3s  "
			"%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.7o\xb3%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe"
			" ",

			nte[0],
			((s->channel[ch].CM == CM_4OP_AM_FM) || (s->channel[ch].CM == CM_4OP_AM_AM))?'+':'*',
			nte[1],
			(s->channel[ch].CM == CM_4OP_FM_AM)?'+':'*',
			nte[2],
			(s->channel[ch].CM == CM_4OP_AM_AM)?'+':'*',
			nte[3],

			vleft >=4?0xf:0x0,
			vleft >=3?0xb:0x0,
			vleft >=2?0x9:0x0,
			vleft >=1?0x1:0x0,

			vright>=1?0x1:0x0,
			vright>=2?0x9:0x0,
			vright>=3?0xb:0x0,
			vright>=4?0xf:0x0
		);
	} else {
		cpifaceSession->console->DisplayPrintf (line, left, 0x07, 44,
			"%3s %9s   - "
			"%3s %10s  "
			"%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.7o\xb3%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe"
			" ",

			nte[0],
			drum0(ch),
			nte[1],
			drum1(ch),

			vleft >=4?0xf:0x0,
			vleft >=3?0xb:0x0,
			vleft >=2?0x9:0x0,
			vleft >=1?0x1:0x0,

			vright>=1?0x1:0x0,
			vright>=2?0x9:0x0,
			vright>=3?0xb:0x0,
			vright>=4?0xf:0x0
		);
	}
}

static void OPLChanDisplay62 (struct cpifaceSessionAPI_t *cpifaceSession, const int line, const int left, struct oplStatus *s, int ch)
{
	if (s->channel[ch].CM == CM_disabled)
	{
		cpifaceSession->console->Driver->DisplayStr (line, left, 0x08, "--- (disabled)                                        \xb3", 62);
		return;
	}

	const char *nte[4];
	PrepareNte (cpifaceSession, nte, s, ch);

	unsigned int vleft;
	unsigned int vright;
	getvol (s, ch, &vleft, &vright);
	vleft = (vleft + 37) / 42;
	vright = (vright + 37) / 42;

	if ((s->channel[ch].CM == CM_2OP_AM) ||
	    (s->channel[ch].CM == CM_2OP_FM))
	{
		cpifaceSession->console->DisplayPrintf (line, left, 0x07, 62,
			"%3s %4s " ADSR_fmt " " Vi_fmt Tr_fmt " %.7o%c %c %c "
			"%3s %4s " ADSR_fmt " " Vi_fmt Tr_fmt " %.7o%c %c "
			"%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.7o\xb3%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe"
			" ",

			nte[0], shap4[s->channel[ch].op[0].waveform_select],
			ADSR_data (ch, 0),
			Vi_data (ch, 0),
			Tr_data (ch, 0),
			s->channel[ch].op[0].ksr_enabled?'E':' ',
			s->channel[ch].op[0].key_scale_level?s->channel[ch].op[0].key_scale_level+'0':' ',
			(s->channel[ch].CM == CM_2OP_AM)?'+':'*',

			nte[1], shap4[s->channel[ch].op[1].waveform_select],
			ADSR_data (ch, 1),
			Vi_data (ch, 1),
			Tr_data (ch, 1),
			s->channel[ch].op[1].ksr_enabled?'E':' ',
			s->channel[ch].op[1].key_scale_level?s->channel[ch].op[1].key_scale_level+'0':' ',

			vleft >=6?0xf:0x0,
			vleft >=5?0xb:0x0,
			vleft >=4?0x9:0x0,
			vleft >=3?0x9:0x0,
			vleft >=2?0x1:0x0,
			vleft >=1?0x1:0x0,

			vright>=1?0x1:0x0,
			vright>=2?0x1:0x0,
			vright>=3?0x9:0x0,
			vright>=4?0x9:0x0,
			vright>=5?0xb:0x0,
			vright>=6?0xf:0x0
		);
	} else if ((s->channel[ch].CM >= CM_4OP_FM_FM) &&
	           (s->channel[ch].CM <= CM_4OP_AM_AM))
	{
		cpifaceSession->console->DisplayPrintf (line, left, 0x07, 62,
			"%3s %4s    %c %3s %4s %c "
			"%3s %4s %c  %3s %4s   "
			"%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.7o\xb3%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe"
			" ",

			nte[0], shap4[s->channel[ch].op[0].waveform_select],
			((s->channel[ch].CM == CM_4OP_AM_FM) || (s->channel[ch].CM == CM_4OP_AM_AM))?'+':'*',
			nte[1], shap4[s->channel[ch].op[1].waveform_select],
			(s->channel[ch].CM == CM_4OP_FM_AM)?'+':'*',
			nte[2], shap4[s->channel[ch+3].op[0].waveform_select],
			(s->channel[ch].CM == CM_4OP_AM_AM)?'+':'*',
			nte[3], shap4[s->channel[ch+3].op[1].waveform_select],

			vleft >=6?0xf:0x0,
			vleft >=5?0xb:0x0,
			vleft >=4?0x9:0x0,
			vleft >=3?0x9:0x0,
			vleft >=2?0x1:0x0,
			vleft >=1?0x1:0x0,

			vright>=1?0x1:0x0,
			vright>=2?0x1:0x0,
			vright>=3?0x9:0x0,
			vright>=4?0x9:0x0,
			vright>=5?0xb:0x0,
			vright>=6?0xf:0x0
		);
	} else {
		cpifaceSession->console->DisplayPrintf (line, left, 0x07, 62,
			"%3s %9s " ADR_fmt " %4s - "
			"%3s %10s " ADR_fmt "     "
			"%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.7o\xb3%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe"
			" ",

			nte[0],
			drum0(ch),
			ADR_data (ch, 0),
			ch > 9 ? "DUAL":"",
			nte[1],
			drum1(ch),
			ADR_data (ch, 1),
			ch > 9 ? "DUAL":"",

			vleft >=6?0xf:0x0,
			vleft >=5?0xb:0x0,
			vleft >=4?0x9:0x0,
			vleft >=3?0x9:0x0,
			vleft >=2?0x1:0x0,
			vleft >=1?0x1:0x0,

			vright>=1?0x1:0x0,
			vright>=2?0x1:0x0,
			vright>=3?0x9:0x0,
			vright>=4?0x9:0x0,
			vright>=5?0xb:0x0,
			vright>=6?0xf:0x0
		);
	}
}

static void OPLChanDisplay76 (struct cpifaceSessionAPI_t *cpifaceSession, const int line, const int left, struct oplStatus *s, int ch)
{
	if (s->channel[ch].CM == CM_disabled)
	{
		cpifaceSession->console->Driver->DisplayStr (line, left, 0x08, "--- (disabled)                                                    \xb3", 76);
		return;
	}

	const char *nte[4];
	PrepareNte (cpifaceSession, nte, s, ch);

	unsigned int vleft;
	unsigned int vright;
	getvol (s, ch, &vleft, &vright);
	vleft = (vleft + 31) / 32;
	vright = (vright + 31) / 32;

	if ((s->channel[ch].CM == CM_2OP_AM) ||
	    (s->channel[ch].CM == CM_2OP_FM))
	{
		cpifaceSession->console->DisplayPrintf (line, left, 0x07, 76,
			"%3s %4s " ADSR_fmt " " Vi_fmt " " Tr_fmt " %.7o%c %c     %c "
			"%3s %4s " ADSR_fmt " " Vi_fmt " " Tr_fmt " %.7o%c %c     "
			"%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.7o\xb3%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe"
			" ",
			nte[0], shap4[s->channel[ch].op[0].waveform_select],
			ADSR_data (ch, 0),
			Vi_data (ch, 0),
			Tr_data (ch, 0),
			s->channel[ch].op[0].ksr_enabled?'E':' ',
			s->channel[ch].op[0].key_scale_level?s->channel[ch].op[0].key_scale_level+'0':' ',

			(s->channel[ch].CM == CM_2OP_AM)?'+':'*',

			nte[1], shap4[s->channel[ch].op[1].waveform_select],
			ADSR_data (ch, 1),
			Vi_data (ch, 1),
			Tr_data (ch, 1),
			s->channel[ch].op[1].ksr_enabled?'E':' ',
			s->channel[ch].op[1].key_scale_level?s->channel[ch].op[1].key_scale_level+'0':' ',

			vleft >=8?0xf:0x0,
			vleft >=7?0xb:0x0,
			vleft >=6?0xb:0x0,
			vleft >=5?0x9:0x0,
			vleft >=4?0x9:0x0,
			vleft >=3?0x1:0x0,
			vleft >=2?0x1:0x0,
			vleft >=1?0x1:0x0,

			vright>=1?0x1:0x0,
			vright>=2?0x1:0x0,
			vright>=3?0x1:0x0,
			vright>=4?0x9:0x0,
			vright>=5?0x9:0x0,
			vright>=6?0xb:0x0,
			vright>=7?0xb:0x0,
			vright>=8?0xf:0x0
		);
	} else if ((s->channel[ch].CM >= CM_4OP_FM_FM) &&
	           (s->channel[ch].CM <= CM_4OP_AM_AM))
	{
		cpifaceSession->console->DisplayPrintf (line, left, 0x07, 76,
			"%3s %3s " ADSR_fmt " %c %3s %s " ADSR_fmt " %c "
			"%3s %3s " ADSR_fmt " %c %3s %s " ADSR_fmt " "
			"%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.7o\xb3%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe"
			" ",

			nte[0], shap3[s->channel[ch].op[0].waveform_select],
			ADSR_data (ch, 0),
			((s->channel[ch].CM == CM_4OP_AM_FM) || (s->channel[ch].CM == CM_4OP_AM_AM))?'+':'*',
			nte[1], shap3[s->channel[ch].op[1].waveform_select],
			ADSR_data (ch, 1),

			(s->channel[ch].CM == CM_4OP_FM_AM)?'+':'*',

			nte[2], shap3[s->channel[ch+3].op[0].waveform_select],
			ADSR_data (ch+3, 0),
			(s->channel[ch].CM == CM_4OP_AM_AM)?'+':'*',
			nte[3], shap3[s->channel[ch+3].op[1].waveform_select],
			ADSR_data (ch+3, 1),

			vleft >=8?0xf:0x0,
			vleft >=7?0xb:0x0,
			vleft >=6?0xb:0x0,
			vleft >=5?0x9:0x0,
			vleft >=4?0x9:0x0,
			vleft >=3?0x1:0x0,
			vleft >=2?0x1:0x0,
			vleft >=1?0x1:0x0,

			vright>=1?0x1:0x0,
			vright>=2?0x1:0x0,
			vright>=3?0x1:0x0,
			vright>=4?0x9:0x0,
			vright>=5?0x9:0x0,
			vright>=6?0xb:0x0,
			vright>=7?0xb:0x0,
			vright>=8?0xf:0x0
		);
	} else {
		cpifaceSession->console->DisplayPrintf (line, left, 0x07, 76,
			"%3s %9s " ADR_fmt " %4s      - "
			"%3s %10s " ADR_fmt " %4s     "
			"%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.7o\xb3%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe"
			" ",

			nte[0],
			drum0(ch),
			ADR_data (ch, 0),
			ch > 9 ? "DUAL":"",
			nte[1],
			drum1(ch),
			ADR_data (ch, 1),
			ch > 9 ? "DUAL":"",

			vleft >=8?0xf:0x0,
			vleft >=7?0xb:0x0,
			vleft >=6?0xb:0x0,
			vleft >=5?0x9:0x0,
			vleft >=4?0x9:0x0,
			vleft >=3?0x1:0x0,
			vleft >=2?0x1:0x0,
			vleft >=1?0x1:0x0,

			vright>=1?0x1:0x0,
			vright>=2?0x1:0x0,
			vright>=3?0x1:0x0,
			vright>=4?0x9:0x0,
			vright>=5?0x9:0x0,
			vright>=6?0xb:0x0,
			vright>=7?0xb:0x0,
			vright>=8?0xf:0x0
		);
	}
}

static void OPLChanDisplay128 (struct cpifaceSessionAPI_t *cpifaceSession, const int line, const int left, struct oplStatus *s, int ch)
{
	if (s->channel[ch].CM == CM_disabled)
	{
		cpifaceSession->console->Driver->DisplayStr (line, left, 0x08, "--- (disabled)                                                                                                 \xb3", 128);
		return;
	}

	const char *nte[4];
	PrepareNte (cpifaceSession, nte, s, ch);

	unsigned int vleft;
	unsigned int vright;
	getvol (s, ch, &vleft, &vright);
	vleft = (vleft + 23) / 28;
	vright = (vright + 23) / 28;

	if ((s->channel[ch].CM == CM_2OP_AM) ||
	    (s->channel[ch].CM == CM_2OP_FM))
	{
		cpifaceSession->console->DisplayPrintf (line, left, 0x07, 128,
			"%3s %4s " ADSR_fmt " " Vi_fmt " " Tr_fmt " %.7o%c %c                           %c "
			"%3s %4s " ADSR_fmt " " Vi_fmt " " Tr_fmt " %.7o%c %c                           "
			"%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.7o\xb3%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe"
			" %.7o%6s",

			nte[0], shap4[s->channel[ch].op[0].waveform_select],
			ADSR_data (ch, 0),
			Vi_data (ch, 0),
			Tr_data (ch, 0),
			s->channel[ch].op[0].ksr_enabled?'E':' ',
			s->channel[ch].op[0].key_scale_level?s->channel[ch].op[0].key_scale_level+'0':' ',

			(s->channel[ch].CM == CM_2OP_AM)?'+':'*',

			nte[1], shap4[s->channel[ch].op[1].waveform_select],
			ADSR_data (ch, 1),
			Vi_data (ch, 1),
			Tr_data (ch, 1),
			s->channel[ch].op[1].ksr_enabled?'E':' ',
			s->channel[ch].op[1].key_scale_level?s->channel[ch].op[1].key_scale_level+'0':' ',

			vleft >=9?0xf:0x0,
			vleft >=8?0xf:0x0,
			vleft >=7?0xb:0x0,
			vleft >=6?0xb:0x0,
			vleft >=5?0x9:0x0,
			vleft >=4?0x9:0x0,
			vleft >=3?0x1:0x0,
			vleft >=2?0x1:0x0,
			vleft >=1?0x1:0x0,

			vright>=1?0x1:0x0,
			vright>=2?0x1:0x0,
			vright>=3?0x1:0x0,
			vright>=4?0x9:0x0,
			vright>=5?0x9:0x0,
			vright>=6?0xb:0x0,
			vright>=7?0xb:0x0,
			vright>=8?0xf:0x0,
			vright>=9?0xf:0x0,

			(s->channel[ch].CM == CM_2OP_AM)?"AM":"FM"
		);
	} else if ((s->channel[ch].CM >= CM_4OP_FM_FM) &&
	           (s->channel[ch].CM <= CM_4OP_AM_AM))
	{
		cpifaceSession->console->DisplayPrintf (line, left, 0x07, 128,
			"%3s %4s " ADSR_fmt " " Vi_fmt " " Tr_fmt " %.7o%c %c %c "
			"%3s %4s " ADSR_fmt " " Vi_fmt " " Tr_fmt " %.7o%c %c %c "
			"%3s %4s " ADSR_fmt " " Vi_fmt " " Tr_fmt " %.7o%c %c %c "
			"%3s %4s " ADSR_fmt " " Vi_fmt " " Tr_fmt " %.7o%c %c "
			"%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.7o\xb3%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe"
			" %.7o%2s-%2s",

			nte[0], shap4[s->channel[ch].op[0].waveform_select],
			ADSR_data (ch, 0),
			Vi_data (ch, 0),
			Tr_data (ch, 0),
			s->channel[ch].op[0].ksr_enabled?'E':' ',
			s->channel[ch].op[0].key_scale_level?s->channel[ch].op[0].key_scale_level+'0':' ',

			((s->channel[ch].CM == CM_4OP_AM_FM) || (s->channel[ch].CM == CM_4OP_AM_AM))?'+':'*',

			nte[1], shap4[s->channel[ch].op[1].waveform_select],
			ADSR_data (ch, 1),
			Vi_data (ch, 1),
			Tr_data (ch, 1),
			s->channel[ch].op[1].ksr_enabled?'E':' ',
			s->channel[ch].op[1].key_scale_level?s->channel[ch].op[1].key_scale_level+'0':' ',

			(s->channel[ch].CM == CM_4OP_FM_AM)?'+':'*',

			nte[2], shap4[s->channel[ch+3].op[0].waveform_select],
			ADSR_data (ch+3, 0),
			Vi_data (ch+3, 0),
			Tr_data (ch+3, 0),
			s->channel[ch+3].op[0].ksr_enabled?'E':' ',
			s->channel[ch+3].op[0].key_scale_level?s->channel[ch+3].op[0].key_scale_level+'0':' ',

			(s->channel[ch].CM == CM_4OP_AM_AM)?'+':'*',

			nte[3], shap4[s->channel[ch+3].op[1].waveform_select],
			ADSR_data (ch+3, 1),
			Vi_data (ch+3, 1),
			Tr_data (ch+3, 1),
			s->channel[ch+3].op[1].ksr_enabled?'E':' ',
			s->channel[ch+3].op[1].key_scale_level?s->channel[ch+3].op[1].key_scale_level+'0':' ',

			vleft >=9?0xf:0x0,
			vleft >=8?0xf:0x0,
			vleft >=7?0xb:0x0,
			vleft >=6?0xb:0x0,
			vleft >=5?0x9:0x0,
			vleft >=4?0x9:0x0,
			vleft >=3?0x1:0x0,
			vleft >=2?0x1:0x0,
			vleft >=1?0x1:0x0,

			vright>=1?0x1:0x0,
			vright>=2?0x1:0x0,
			vright>=3?0x1:0x0,
			vright>=4?0x9:0x0,
			vright>=5?0x9:0x0,
			vright>=6?0xb:0x0,
			vright>=7?0xb:0x0,
			vright>=8?0xf:0x0,
			vright>=9?0xf:0x0,

			((s->channel[ch].CM == CM_4OP_FM_AM) || (s->channel[ch].CM == CM_4OP_FM_FM)) ? "FM" : "AM",
			((s->channel[ch].CM == CM_4OP_FM_FM) || (s->channel[ch].CM == CM_4OP_AM_FM)) ? "FM" : "AM"
		);
	} else {
		cpifaceSession->console->DisplayPrintf (line, left, 0x07, 128,
			"%3s %9s " ADR_fmt " %4s                            - "
			"%3s %10s " ADR_fmt " %4s                           "
			"%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.7o\xb3%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe%.*o\xfe"
			" %.7oPERC",

			nte[0],
			drum0(ch),
			ADR_data (ch, 0),
			ch > 9 ? "DUAL":"",
			nte[1],
			drum1(ch),
			ADR_data (ch, 1),
			ch > 9 ? "DUAL":"",

			vleft >=9?0xf:0x0,
			vleft >=8?0xf:0x0,
			vleft >=7?0xb:0x0,
			vleft >=6?0xb:0x0,
			vleft >=5?0x9:0x0,
			vleft >=4?0x9:0x0,
			vleft >=3?0x1:0x0,
			vleft >=2?0x1:0x0,
			vleft >=1?0x1:0x0,

			vright>=1?0x1:0x0,
			vright>=2?0x1:0x0,
			vright>=3?0x1:0x0,
			vright>=4?0x9:0x0,
			vright>=5?0x9:0x0,
			vright>=6?0xb:0x0,
			vright>=7?0xb:0x0,
			vright>=8?0xf:0x0,
			vright>=9?0xf:0x0
		);
	}
}

static int OPLChanGetWin(struct cpifaceSessionAPI_t *cpifaceSession, struct cpitextmodequerystruct *q)
{
	if ( (OPLChannelType == 3) && (cpifaceSession->console->TextWidth < 132) )
	{
		OPLChannelType = 0;
	}

	switch (OPLChannelType)
	{
		case 0:
			return 0;
		case 1:
			q->hgtmax = 11;
			q->xmode  =  3;
			break;
		case 2:
			q->hgtmax = 20;
			q->xmode  =  1;
			break;
		case 3:
			q->hgtmax = 20;
			q->xmode  =  2;
			break;
	}

	q->size     =   1;
	q->top      =   1;
	q->killprio = 128;
	q->viewprio = 160;
	q->hgtmin   =   3;

	return 1;
}

static void OPLChanDraw (struct cpifaceSessionAPI_t *cpifaceSession, int sel)
{
	unsigned int x,y;
	unsigned int h =(OPLChannelType==1) ? 9 : 18;
	unsigned int sh=(OPLChannelType==1) ? (OPLSelectedChannel / 2) : OPLSelectedChannel;
	int first = 0;
	struct oplStatus *s;

	s = &oplLastStatus;

	switch (OPLChannelType)
	{
		case 1:
			cpifaceSession->console->Driver->DisplayStr (OPLChanFirstLine, OPLChanStartCol, sel?0x09:0x01, "   channels (compact):", OPLChanWidth);
			if (OPLChanWidth<132)
			{
				for (x=0; x < 2; x++)
				{
					cpifaceSession->console->Driver->DisplayStr (OPLChanFirstLine + 1, OPLChanStartCol + x*40, 0x07, " ##:", 4);
					OPLChanDisplay36Header (cpifaceSession, OPLChanFirstLine + 1, OPLChanStartCol + x*40 + 4);
				}
				cpifaceSession->console->Driver->DisplayVoid (OPLChanFirstLine + 1, OPLChanStartCol + x*40, OPLChanWidth - x*40);
			} else {
				for (x=0; x < 2; x++)
				{
					cpifaceSession->console->Driver->DisplayStr (OPLChanFirstLine + 1, OPLChanStartCol + x*66, 0x07, " ##:", 4);
					OPLChanDisplay62Header (cpifaceSession, OPLChanFirstLine + 1, OPLChanStartCol + x*66 + 4);
				}
				cpifaceSession->console->Driver->DisplayVoid (OPLChanFirstLine + 1, OPLChanStartCol + x*66, OPLChanWidth - x*66);
			}
			break;
		case 2:
			cpifaceSession->console->Driver->DisplayStr (OPLChanFirstLine, OPLChanStartCol, sel?0x09:0x01, "   channels (long):", OPLChanWidth);
			cpifaceSession->console->Driver->DisplayStr (OPLChanFirstLine + 1, OPLChanStartCol, 0x07, " ##:", 4);
			if (OPLChanWidth<132)
			{
				OPLChanDisplay76Header (cpifaceSession, OPLChanFirstLine + 1, OPLChanStartCol + 4);
				cpifaceSession->console->Driver->DisplayVoid (OPLChanFirstLine + 1, OPLChanStartCol + 4 + 76, OPLChanWidth - 4 - 76);
			} else {
				OPLChanDisplay128Header (cpifaceSession, OPLChanFirstLine + 1, OPLChanStartCol + 4);
				cpifaceSession->console->Driver->DisplayVoid (OPLChanFirstLine + 1, OPLChanStartCol + 4 + 128, OPLChanWidth - 4 - 128);
			}
			break;
		case 3:
			cpifaceSession->console->Driver->DisplayStr (OPLChanFirstLine, OPLChanStartCol, sel?0x09:0x01, "   channels (side):", OPLChanWidth);
			cpifaceSession->console->Driver->DisplayStr (OPLChanFirstLine + 1, OPLChanStartCol, 0x07, "     ##:", 8);
			OPLChanDisplay44Header (cpifaceSession, OPLChanFirstLine + 1, OPLChanStartCol + 8);
			break;
	}

	if (((h+2) > OPLChanHeight) && ((sh+2) >= (OPLChanHeight/2)))
	{
		if (sh >= (h - (OPLChanHeight - 2)/2) )
		{
			first = h - OPLChanHeight - 2;
		} else {
			first = sh - (OPLChanHeight-2) / 2;
		}
	}

	for (y=0; y<(OPLChanHeight - 2); y++)
	{
		char sign=' ';
		if (!y&&first)
			sign=0x18;
		if (((y+1)==(OPLChanHeight-2))&&((y+first+1)!=h))
			sign=0x19;
		if (OPLChannelType==1)
		{
			for (x=0; x<2; x++)
			{
				unsigned int i=2*first+y*2+x;
				if (i < 18)
				{
					if (OPLChanWidth<132)
					{
						cpifaceSession->console->DisplayPrintf (OPLChanFirstLine + y + 2, OPLChanStartCol + x*40, 0x0f, 4, "%c%0.*o%02d:",
								(i==OPLSelectedChannel)?sign:' ', s->mute[i]?0x08:0x07, i + 1);
						OPLChanDisplay36 (cpifaceSession, OPLChanFirstLine + y + 2, OPLChanStartCol+x*40+4, s, i);
					} else {
						cpifaceSession->console->DisplayPrintf (OPLChanFirstLine + y + 2, OPLChanStartCol + x*66, 0x0f, 4, "%c%0.*o%02d:",
								(i==OPLSelectedChannel)?sign:' ', s->mute[i]?0x08:0x07, i + 1);
						OPLChanDisplay62 (cpifaceSession, OPLChanFirstLine + y + 2, OPLChanStartCol+x*66+4, s, i);
					}
				} else {
					if (OPLChanWidth<132)
					{
						cpifaceSession->console->Driver->DisplayVoid (OPLChanFirstLine + y + 2, OPLChanStartCol + x*40, 40);
					} else {
						cpifaceSession->console->Driver->DisplayVoid (OPLChanFirstLine + y + 2, OPLChanStartCol + x*66, 66);
					}
				}
			}
			if (OPLChanWidth<132)
			{
				cpifaceSession->console->Driver->DisplayVoid (OPLChanFirstLine + y + 2, OPLChanStartCol + x*40, OPLChanWidth - x*40);
			} else {
				cpifaceSession->console->Driver->DisplayVoid (OPLChanFirstLine + y + 2, OPLChanStartCol + x*66, OPLChanWidth - x*66);
			}
		} else {
			unsigned int i=y+first;
			if ((y + first) == OPLSelectedChannel)
			{
				sign='>';
			}
			if (OPLChannelType==2)
			{
				cpifaceSession->console->DisplayPrintf (OPLChanFirstLine + y + 2, OPLChanStartCol, 0x0f, 4, "%c%0.*o%02d:",
						(i==OPLSelectedChannel)?sign:' ', s->mute[i]?0x08:0x07, i + 1);
				if (OPLChanWidth<132)
				{
					OPLChanDisplay76 (cpifaceSession, OPLChanFirstLine + y + 2, OPLChanStartCol+4, s, i);
					cpifaceSession->console->Driver->DisplayVoid (OPLChanFirstLine + y + 2, OPLChanStartCol + 80, OPLChanWidth - 80);
				} else {
					OPLChanDisplay128 (cpifaceSession, OPLChanFirstLine + y + 2, OPLChanStartCol+4, s, i);
					cpifaceSession->console->Driver->DisplayVoid (OPLChanFirstLine + y + 2, OPLChanStartCol + 132, OPLChanWidth - 132);
				}
			} else {
				cpifaceSession->console->DisplayPrintf (OPLChanFirstLine + y + 2, OPLChanStartCol, 0x0f, 8, "    %c%0.*o%02d:",
						(i==OPLSelectedChannel)?sign:' ', s->mute[i]?0x08:0x07, i + 1);
				OPLChanDisplay44 (cpifaceSession, OPLChanFirstLine + y + 2, OPLChanStartCol+8, s, i);
				cpifaceSession->console->Driver->DisplayVoid (OPLChanFirstLine + y + 2, OPLChanStartCol + 52, OPLChanWidth - 52);
			}
		}
	}
}

static void OPLChanSetWin(struct cpifaceSessionAPI_t *cpifaceSession, int xpos, int wid, int ypos, int hgt)
{
	OPLChanFirstLine = ypos;
	OPLChanStartCol  = xpos;
	OPLChanHeight    = hgt;
	OPLChanWidth    = wid;
}

static int OPLChanIProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpifaceSession->KeyHelp('c', "Enable channel viewer");
			cpifaceSession->KeyHelp('C', "Enable channel viewer");
			break;
		case 'c': case 'C':
			if (!OPLChannelType)
				OPLChannelType=(OPLChannelType+1)%4;
			cpifaceSession->cpiTextSetMode (cpifaceSession, "oplchan");
			return 1;
		case 'x': case 'X':
			OPLChannelType=3;
			break;
		case KEY_ALT_X:
			OPLChannelType=2;
			break;
	}
	return 0;
}

static int OPLChanAProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpifaceSession->KeyHelp('c', "Change channel view mode");
			cpifaceSession->KeyHelp('C', "Change channel view mode");
			return 0;
		case 'c': case 'C':
			OPLChannelType=(OPLChannelType+1)%4;
			cpifaceSession->cpiTextRecalc (cpifaceSession);
			break;
#if 0
		case KEY_LEFT:
			if (OPLSelectedChannel)
			{
				OPLSelectedChannel--;
			}
			break;
		case KEY_UP:
			OPLSelectedChannel = (OPLSelectedChannel - 1 + 18) % 18;
			break;
		case KEY_RIGHT:
			if ((OPLSelectedChannel + 1) < 18)
			{
				OPLSelectedChannel++;
			}
			break;
		case KEY_DOWN:
			OPLSelectedChannel = (OPLSelectedChannel + 1) % 18;
			break;
#endif
		default:
			return 0;
	}
	return 1;
}

static int OPLChanEvent(struct cpifaceSessionAPI_t *cpifaceSession, int ev)
{
	return 1;
}

static struct cpitextmoderegstruct oplchan = {"oplchan", OPLChanGetWin, OPLChanSetWin, OPLChanDraw, OPLChanIProcessKey, OPLChanAProcessKey, OPLChanEvent CPITEXTMODEREGSTRUCT_TAIL};

void OPLChanInit (struct cpifaceSessionAPI_t *cpifaceSession)
{
	OPLChannelType = cpifaceSession->configAPI->GetProfileInt2(cpifaceSession->configAPI->ScreenSec, "screen", "channeltype", 3, 10)&3;
	cpifaceSession->cpiTextRegisterMode(cpifaceSession, &oplchan);
}
