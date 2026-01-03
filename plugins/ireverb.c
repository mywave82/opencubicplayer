// OpenCP Module Player
// copyright (c) 1994-'98 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
// copyright (c) 2022-'26 Stian Skjelstad <stian.skjelstad@gmail.com>

// this shall somewhen become a cool reverb/compressor/EQ audio plug-in
// but i didnt have any time for it yet ;)
// (KB)

// ok, time for the changelog:
// -rygwhenever Fabian Giesen <ripped@purge.com>
//   -first release
// -kb990531 Tammo Hinrichs <opencp@gmx.net>
//   -added high pass filter to reverb output to remove bass and
//    dc offsets
//   -added low pass filters to the comb filter delays (treble cut, simulates
//    echo damping at the walls)
//   -implemented a simple two-delay stereo chorus
//   -implemented volreg structure to make reverb and chorus parameters
//    user adjustable
//   -fixed some minor things here and there
// -ryg990610 Fabian Giesen <fabian@jdcs.su.nw.schule.de>
//   -converted this back to int for normal/quality mixer

#include "config.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include "types.h"
#include "boot/plinkman.h"
#include "cpiface/cpiface.h"
#include "cpiface/vol.h"
#include "dev/mcp.h"
#include "dev/postproc.h"
#include "stuff/err.h"
#include "stuff/imsrtns.h"

static struct ocpvolstruct irevvol[] = 
{
	{16, 0, 50, 1, 0, "reverb time"},
	{37, 0, 50, 1, 0, "reverb high cut"},
	{24, 0, 50, 1, 0, "chorus delay"},
	{12, 0, 50, 1, 0, "chorus speed"},
	{10, 0, 50, 1, 0, "chorus depth"},
	{10, 0, 50, 1, 0, "chorus phase"},
	{ 5, 0, 50, 1, 0, "chorus feedback"},
#if 0 /* not implemented at all */
	{ 0, 0, -2, 1, 0, "chorus mode select\tchorus\tflanger"},
	{30, 0, 80, 1, 0, "level detector"},
	{ 0, 0, 40, 1, 0, "db reduction"},
#endif
};

static float srate;

static int initfail;
static int running;

static int32_t *leftl[6], *rightl[6];     // delay lines
static int32_t  llen[6], lpos[6];         // dline length/pos left
static int32_t  rlen[6], rpos[6];         // same right
static int32_t  rlpf[6], llpf[6];         // left/right comb filter LPFs
static uint32_t lpfval = 8388608;         // LPF freq value                        8.24
static uint32_t lpconst;                  //                                        .32
static int32_t  lpl, lpr;                 // reverb out hpf (1-lpf)

static uint32_t chrminspeed, chrmaxspeed; // chorus speed limits (0.1-10Hz)       24.8
static uint32_t chrspeed;                 // chorus speed                         24.8
static uint32_t chrpos;                   // chorus osc pos                       24.8
static int32_t  chrphase;                 // chorus l/r phase shift
static int32_t  chrdelay;                 // chorus delay
static int32_t  chrdepth;                 // chorus depth
static int32_t  chrfb;                    // chorus feedback
static int32_t *lcline, *rcline;          // chorus delay lines
static int      cllen, clpos;             // dlines write length/pos

static int32_t *co1dline;                 // compr1 analyzer delay line
static int      co1dllen, co1dlpos;       // compr1 dl length/pos
static int      co1amode;                 // compr1 analyzer mode (0:rms, 1:peak)
static int32_t  co1mean, co1invlen;
static int32_t  co1atten, /*co1attack, co1decay,*/ co1thres, co1cprv;

static       int32_t  gainsf[6] = {63333, 62796, 62507, 62320, 65160, 65408}; /* s15.16 */
static const float   ldelays[6] = {29.68253968,  37.07482993,  41.06575964,  43.67346939,   4.98866213,   1.67800453};
static const float   rdelays[6] = {21.18461635,  41.26753476,  14.15636606,  15.66663244,   3.21700938,   1.35656276};

static const float    gainsc[6] = { 0.966384599,  0.958186359,  0.953783929,  0.950933178,  0.994260075,  0.998044717};

static void updatevol (int n)
{
	int i;
	float v;
	switch (n)
	{
		case 0:  // rvb time
			v = pow(25.0f / (irevvol[0].val + 1), 2);
			for (i=0; i<6; i++)
			{
				gainsf[i] = pow (gainsc[i], v) * 65536.0f * ((i&1)?-1.0:1.0);
			}
			break;
		case 1:  // rvb high cut
			v = ((irevvol[1].val+20) / 70.0f) * (44100.0f / srate);
			lpfval = v * v * 16777216.0f;
			break;
		case 2:  // chr delay
			chrdelay = (cllen - 8) * irevvol[2].val * 655.36; /* upto half the cllen */
			break;
		case 3:  // chr speed
			chrspeed = chrminspeed + pow (irevvol[3].val / 50.0f, 3) * ( chrmaxspeed - chrminspeed );
			break;
		case 4:  // chr depth
			chrdepth = (cllen - 8) * (irevvol[4].val * 655.36); /* upto half the cllen  */
			break;
		case 5:  // chr phase shift
			chrphase = irevvol[5].val * 1310.72f;
			break;
		case 6:  // chr feedback
			chrfb = irevvol[6].val * 1092.2666f;
			break;
	}
}

static void iReverb_close (void)
{
	int i;
	running = 0;

	for (i=0; i<6; i++)
	{
		free (leftl[i]);
		free (rightl[i]);

		leftl[i]  = 0;
		rightl[i] = 0;
	}

	free (lcline);
	free (rcline);
	free (co1dline);
	lcline = 0;
	rcline = 0;
	co1dline = 0;
}

static void iReverb_init (int rate)
{
	int i;

	initfail = 0;
	running = 0;

	srate = rate;

	chrminspeed = 3355443 / srate;   // 0.1hz
	chrmaxspeed = 335544320 / srate; // 10hz
	cllen = (srate / 20) + 8;        // 50msec max depth

	lcline = calloc(sizeof (lcline[0]), cllen);
	if (!lcline)
	{
		goto fail;
	}
	rcline = calloc(sizeof (rcline[0]), cllen);
	if (!rcline)
	{
		goto fail;
	}

	chrpos = 0;
	clpos = 0;

	// init reverb
	for (i=0; i<6; i++)
	{
		llen[i] = (int32_t) (ldelays[i] * rate / 1000.0);
		lpos[i] = 0;

		rlen[i] = (int32_t) (rdelays[i] * rate / 1000.0);
		rpos[i] = 0;

		llpf[i] = rlpf[i] = 0;

		leftl[i]  = calloc(llen[i], sizeof (leftl [i][0]));
		rightl[i] = calloc(rlen[i], sizeof (rightl[i][0]));

		if ((!leftl[i]) || (!rightl[i]))
		{
			goto fail;
		}
	}

	// most of the compressor values aren't right, i'll update them when
	// kebby gets them running in the FLOAT version *mg*

	lpconst = (150.0f / (float)srate) * (150.0f / (float)srate) * 4294967296.0;
	lpl = lpr = 0;

	co1amode = 0;
	co1dllen = srate / 20;
	co1dline = calloc (sizeof (co1dline[0]), co1dllen);
	if (!co1dline)
	{
		goto fail;
	}

	co1invlen = 65536 / co1dllen;
	co1dlpos = 0;
	co1mean = 0;

	co1atten = 0;
/*
	co1attack = 0.0001;
	co1decay = 0.0001;
*/
	co1thres = 0;
	co1cprv = 1;

	for (i=0; i < (sizeof(irevvol) / sizeof(irevvol[0])); i++)
	{
		updatevol (i);
	}

	running = 1;
	return;

fail:
	initfail = 1;
	iReverb_close ();
}

static int doreverb (int32_t inp, int32_t *pos, int32_t *lines[], int32_t lpf[])
{
	int32_t asum=0,
	        y1, y2, z;
	int i;

	inp >>= 2;

	for (i=0; i<4; i++)
	{
		                   lpf[i] += imulshr24(lpfval, (inp + imulshr16 (gainsf[i], lines[i][pos[i]]) - lpf[i]));
		lines[i][pos[i]] = lpf[i];
		asum            += lpf[i];
	}

	y1 = lines[4][pos[4]];
	z  = imulshr16(gainsf[4], y1) + asum;
	lines[4][pos[4]] = z;

	y2 = lines[5][pos[5]];
	z = imulshr16(gainsf[5], y2) + y1 - imulshr16(gainsf[4], z);
	lines[5][pos[5]] = z;

	asum = y2 - imulshr16(gainsf[5], z);

	return asum;
}

static void iReverb_process (struct cpifaceSessionAPI_t *cpifaceSession, int32_t *buf, int len, int rate)
{
	uint32_t outgainchorus;
	uint32_t outgainreverb;

	if (initfail)
	{
		return;
	}

	// THE CHORUS
	if (!cpifaceSession->mcpGet)
	{
		outgainchorus = 0;
	} else {
		outgainchorus = cpifaceSession->mcpGet (cpifaceSession, 0, mcpMasterChorus)<<10;
	}

	if (outgainchorus > 0)
	{
		int i;

		for (i=0; i<len; i++)
		{
			uint32_t chrpos1, chrpos2;
			uint32_t readpos1, readpos2;
			int rpp1, rpp2;
			int32_t lout, rout;

			int32_t v1=buf[i*2  ];
			int32_t v2=buf[i*2+1];

			// update LFO and get l/r delays (0-1)
			chrpos += chrspeed;
			if (chrpos  >= 0x02000000) chrpos  -= 0x02000000;
			chrpos1 = chrpos;
			if (chrpos1 >  0x01000000) chrpos1  = 0x02000000 - chrpos1;
			chrpos2 = chrpos + chrphase;
			if (chrpos2 >= 0x02000000) chrpos2 -= 0x02000000;
			if (chrpos2 >  0x01000000) chrpos2  = 0x02000000 - chrpos2;

			// get integer+fractional part of left delay
			chrpos1 = chrdelay + imulshr24(chrpos1, chrdepth);
			readpos1 = (chrpos1 >> 16) + clpos;
			if (readpos1 >= cllen) readpos1 -= cllen;
			chrpos1 &= 0xffff; /* removes the integer part */
			rpp1 = (readpos1 < cllen - 1) ? readpos1 + 1 : 0;

			// get integer+fractional part of right delay
			chrpos2 = chrdelay + imulshr24(chrpos2, chrdepth);
			readpos2 = (chrpos2 >> 16) + clpos;
			if (readpos2 >= cllen) readpos2 -= cllen;
			chrpos2 &= 0xffff; /* removes the integer part */
			rpp2 = (readpos2 < cllen - 1) ? readpos2 + 1 : 0;

			// now: readposx: integer pos,
			//      rppx:     integer pos+1,
			//      chrposx:  fractional pos

			// determine chorus output
			lout = lcline[readpos1] + imulshr16 (chrpos1, ( lcline[rpp1] - lcline[readpos1] ));
			rout = rcline[readpos2] + imulshr16 (chrpos2, ( rcline[rpp2] - rcline[readpos2] ));

			// mix chorus with original buffer
			buf[i*2  ] += imulshr16 ( outgainchorus, ( lout - v1 ) );
			buf[i*2+1] += imulshr16 ( outgainchorus, ( rout - v2 ) );

			// update delay lines and do negative feedback
			lcline[clpos] = v1 - imulshr16 ( chrfb, lout );
			rcline[clpos] = v2 - imulshr16 ( chrfb, rout );
			clpos = clpos ? clpos - 1 : cllen - 1;
		}
	}

	// THE REVERB
	if (!cpifaceSession->mcpGet)
	{
		outgainreverb = 0;
	} else {
		outgainreverb = cpifaceSession->mcpGet (cpifaceSession, 0, mcpMasterReverb)<<10;
	}
	if (outgainreverb > 0)
	{
		int i;
		for (i=0; i<len; i++)
		{
			int j;
			int32_t v1, v2;
			for (j=0; j<6; j++)
			{
				if (++lpos[j] >= llen[j]) lpos[j]=0;
				if (++rpos[j] >= rlen[j]) rpos[j]=0;
			}

			v1 = buf[i*2  ];
			v2 = buf[i*2+1];
			lpl += imulshr24 (lpconst, (v1 - (lpl>>8)));
			lpr += imulshr24 (lpconst, (v2 - (lpr>>8)));

			// lpl and lpr is a small "infection" of sound from left to right and back again

			// apply reverb
			buf[i*2  ] += imulshr16 (doreverb (v2 - (lpr>>8), rpos, rightl, rlpf), outgainreverb);
			buf[i*2+1] += imulshr16 (doreverb (v1 - (lpl>>8), lpos,  leftl, llpf), outgainreverb);
		}
	}
}

static int iReverb_processkey(uint16_t key)
{
	return 0;
}

static int iReverb_GetNumVolume (void)
{
	return sizeof(irevvol) / sizeof(irevvol[0]);
}

static int iReverb_GetVolume(struct ocpvolstruct *v, int n)
{
	if (running && n < (sizeof(irevvol)/sizeof(irevvol[0])))
	{
		*v = irevvol[n];
		return(!0);
	}
	return(0);
}

static int iReverb_SetVolume(struct ocpvolstruct *v, int n)
{
	if (n < (sizeof(irevvol)/sizeof(irevvol[0])))
	{
		irevvol[n] = *v;
		updatevol (n);
		return !0;
	}
	return 0;
}

static const struct ocpvolregstruct ivolrev =
{
	iReverb_GetNumVolume,
	iReverb_GetVolume,
	iReverb_SetVolume
};

static struct PostProcIntegerRegStruct iReverb =
{
	"iReverb",
	iReverb_process,
	iReverb_init,
	iReverb_close,
	&ivolrev,
	iReverb_processkey
};

static int iReverbPluginInit (struct PluginInitAPI_t *API)
{
	API->mcpRegisterPostProcInteger (&iReverb);

	return errOk;
}

static void iReverbPluginClose (struct PluginCloseAPI_t *API)
{
	API->mcpUnregisterPostProcInteger (&iReverb);
}

DLLEXTINFO_DRIVER_PREFIX struct linkinfostruct dllextinfo = {.name = "ireverb", .desc = "OpenCP integer reverb (c) 1994-'26 Fabian Giesen, Tammo Hinrichs, Stian Skjelstad", .ver = DLLVERSION, .sortindex = 99, .PluginInit = iReverbPluginInit, .PluginClose = iReverbPluginClose};

/*
  notizen dazu (von ryg):
    1. der reverbeffekt besteht aus 4 comb- und 2 allpassfiltern
       mit folgenden parametern:

       1. comb   gain: 0.966384599   delay: 29.68253968,21.18461635 ms
       2. comb   gain: 0.958186359   delay: 37.07482993,41.26753476 ms
       3. comb   gain: 0.953783929   delay: 41.06575964,14.15636606 ms
       4. comb   gain: 0.950933178   delay: 43.67346939,15.66663244 ms

       1. apass  gain: 0.994260075   delay:  4.98866213,3.21700938 ms
       2. apass  gain: 0.998044717   delay:  1.67800453,1.35656276 ms

    2. gains in fixedpoint (16.16) sind dabei:

       1. comb   63333
       2. comb   62796
       3. comb   62507
       4. comb   62320

       1. apass  65160
       2. apass  65473

    3. originalmodul von totraum kriegt inputwerte vom tb 303-synthesizer,
       sprich im bereich -8000..8000 (d.h. inputsignal durch 4 teilen fÅr
       richtigen sound)

    4. in totraum sind weiterhin tb303 und reverberator getrennt, d.h. im
       reverbmodul wird tb-output nicht nochmal geaddet. das sollte man hier
       natuerlich tun :)
*/
