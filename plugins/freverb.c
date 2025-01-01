// OpenCP Module Player
// copyright (c) 1994-'98 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
// copyright (c) 2022-'25 Stian Skjelstad <stian.skjelstad@gmail.com>

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
// -kb990601 Tammo Hinrichs <opencp@gmx.net>

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

static struct ocpvolstruct revvol[] =
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

static float  *leftl[6], *rightl[6]; // delay lines
static int32_t llen[6], lpos[6];     // dline length/pos left
static int32_t rlen[6], rpos[6];     // same right
static float   rlpf[6], llpf[6];     // left/right comb filter LPFs
static float   lpfval=0.5;           // LPF freq value
static float   lpconst,lpl,lpr;      // reverb out hpf (1-lpf)

static float  chrminspeed,chrmaxspeed; // chorus speed limits (0.1 - 10Hz)
static float  chrspeed;                // chorus speed
static float  chrpos;                  // chorus osc pos
static float  chrphase;                // chorus l/r phase shift
static float  chrdelay;                // chorus delay
static float  chrdepth;                // chorus depth
static float  chrfb;                   // chorus feedback
static float *lcline, *rcline;         // chorus delay lines
static int    cllen,clpos;             // dlines write length/pos

static float *co1dline;                // compr1 analyzer delay line
static int    co1dllen, co1dlpos;      // compr1 dl length/pos
static int    co1amode;                // compr1 analyzer mode (0:rms, 1:peak)
static float  co1mean,co1invlen;
static float  co1atten, /*co1attack, co1decay,*/ co1thres, co1cprv;

static       float gainsf[6]  = { 0.966384599,  0.958186359,  0.953783929,  0.950933178, 0.994260075, 0.998044717};
static const float ldelays[6] = {29.68253968,  37.07482993,  41.06575964,  43.67346939,  4.98866213,  1.67800453 };
static const float rdelays[6] = {21.18461635,  41.26753476,  14.15636606,  15.66663244,   3.21700938,   1.35656276};

static const float gainsc[6]  = { 0.966384599,  0.958186359,  0.953783929,  0.950933178, 0.994260075, 0.998044717};

static void updatevol (int n)
{
	int i;
	float v;
	switch(n)
	{
		case 0:  // rvb time
			v = pow(25.0/(revvol[0].val+1),2);
			for (i=0; i<6; i++)
			{
				gainsf[i] = pow (gainsc[i], v) * ((i&1)?-1:1);
			}
			break;
		case 1:  // rvb high cut
			v = ((revvol[1].val+20)/70.0)*(44100.0/srate);
			lpfval = v * v;
			break;
		case 2:  // chr delay
			chrdelay = (float)(cllen - 8) * (revvol[2].val / 100.0f); /* upto half the cllen */
		case 3:  // chr speed
			chrspeed = chrminspeed + pow (revvol[3].val / 50.0, 3) * (chrmaxspeed - chrminspeed);
			break;
		case 4:  // chr depth
			chrdepth = (float)(cllen - 8) * (revvol[4].val / 100.0f); /* upto half the cllen */
			break;
		case 5:  // chr phase shift
			chrphase = revvol[5].val / 50.0;
			break;
		case 6:  // chr feedback
			chrfb = revvol[6].val / 60.0;
			break;
	}
}

static void fReverb_close (void)
{
	int i;
	running=0;

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

static void fReverb_init (int rate)
{
	int i;
	initfail = 0;
	running = 0;

	srate = rate;

	chrminspeed = 0.2 / srate;  // 0.1hz
	chrmaxspeed = 20 / srate;   // 10hz
	cllen = (srate / 20) + 8;   // 50msec max depth

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

		llpf[i] = rlpf[i]=0;

		leftl[i]  = calloc(llen[i], sizeof (leftl [i][0]));
		rightl[i] = calloc(rlen[i], sizeof (rightl[i][0]));

		if ((!leftl[i]) || (!rightl[i]))
		{
			goto fail;
		}
	}

	lpconst = (150.0f / srate) * (150.0f / srate);
	lpl = lpr = 0;

	co1amode = 0;
	co1dllen = srate / 20;
	co1dline = calloc (sizeof (co1dline[0]), co1dllen);
	if (!co1dline)
	{
		goto fail;
	}

	co1invlen = 1.0/co1dllen;
	co1dlpos = 0;
	co1mean = 0;

	co1atten=0;
/*
	co1attack=0.0001;
	co1decay=0.0001;
*/
	co1thres=0;
	co1cprv=1;

	for (i=0; i<(sizeof(revvol)/sizeof(revvol[0])); i++)
	{
		updatevol(i);
	}

	running=1;
	return;

fail:
	initfail = 1;
	fReverb_close ();
}

static float doreverb(float inp, int32_t *pos, float *lines[], float lpf[])
{
	float asum = 0;
	float y1, y2, z;
	int i;

	inp *= 0.25;

	for (i=0; i<4; i++)
	{
		                   lpf[i] += lpfval * (inp + gainsf[i] * lines[i][pos[i]] - lpf[i]);
		lines[i][pos[i]] = lpf[i];
		asum            += lpf[i];
	}

	y1 = lines[4][pos[4]];
	z = gainsf[4] * y1 + asum;
	lines[4][pos[4]] = z;

	y2 = lines[5][pos[5]];
	z = gainsf[5] * y2 + y1 - gainsf[4] * z;
	lines[5][pos[5]] = z;

	asum = y2 - gainsf[5] * z;

	return asum;
}

static void fReverb_process (struct cpifaceSessionAPI_t *cpifaceSession, float *buf, int len, int rate)
{
	float outgainc;
	float outgainr;

	if (initfail)
	{
		return;
	}

	// THE CHORUS
	if (!cpifaceSession->mcpGet)
	{
		outgainc = 0;
	} else {
		outgainc = cpifaceSession->mcpGet (cpifaceSession, 0, mcpMasterChorus)/64.0;
	}

	if (outgainc > 0)
	{
		int i;

		for (i=0; i<len; i++)
		{
			float chrpos1, chrpos2;
			int readpos1, readpos2;
			int rpp1, rpp2;
			float lout, rout;

			float v1=buf[i*2  ];
			float v2=buf[i*2+1];

			// update LFO and get l/r delays (0-1)
			chrpos += chrspeed;
			if (chrpos >= 2) chrpos -= 2;
			chrpos1 = chrpos;
			if (chrpos1 > 1) chrpos1 = 2.0f - chrpos1;
			chrpos2 = chrpos + chrphase;
			if (chrpos2 >= 2) chrpos2 -= 2;
			if (chrpos2 > 1) chrpos2 = 2.0f - chrpos2;

			// get integer+fractional part of left delay
			chrpos1 = chrdelay + chrpos1 * chrdepth;
			readpos1=chrpos1+clpos;
			if (readpos1>=cllen) readpos1-=cllen;
			chrpos1-=(int)chrpos1; /* remove the integer part */
			rpp1=(readpos1<cllen-1)?readpos1+1:0;

			// get integer+fractional part of right delay
			chrpos2 = chrdelay + chrpos2 * chrdepth;
			readpos2=chrpos2+clpos;
			if (readpos2>=cllen) readpos2-=cllen;
			chrpos2-=(int)chrpos2; /* remove the integer part */
			rpp2=(readpos2<cllen-1)?readpos2+1:0;

			// now: readposx: integer pos,
			//      rppx:     integer pos+1,
			//      chrposx:  fractional pos

			// determine chorus output
			lout=lcline[readpos1]+chrpos1*(lcline[rpp1]-lcline[readpos1]);
			rout=rcline[readpos2]+chrpos2*(rcline[rpp2]-rcline[readpos2]);

			// mix chorus with original buffer
			buf[i*2  ]=v1+outgainc*(lout-v1);
			buf[i*2+1]=v2+outgainc*(rout-v2);

			// update delay lines and do feedback
			lcline[clpos]=v1-chrfb*lout;
			rcline[clpos]=v2-chrfb*rout;
			clpos=clpos?clpos-1:cllen-1;
		}
	}

/*
  const float invlog2=6/log(2);
  // THE COMPRESSOR I
  if (co1amode)
  { // peak mode
  }
  else
  { // rms mode
      for (int i=0; i<len; i++)
      {
        co1mean-=co1dline[co1dlpos];
        float v=(buf[2*i]+buf[2*i+1])/65536.0;
        co1dline[co1dlpos]=v*v;
        co1mean+=v*v;
        float co1out=sqrt(co1mean*co1invlen)*2.0;
        float co1db=log(co1out)*invlog2-co1thres;
        if (co1db<-40) co1db=-40;
        if (co1db>40) co1db=40;
        revvol[8].val=co1db+40;

        float dstatten=(co1db>0)?co1db*(1-co1cprv):0;

        co1atten+=((dstatten>co1atten)?co1attack:co1decay)*(dstatten-co1atten);

        //co1atten+=0.0002*(dstatten-co1atten);

        revvol[9].val=co1atten;

        // ok, now apply the gain
        float gain=pow(0.5,co1atten/6.0);
        buf[2*i]*=gain;
        buf[2*i+1]*=gain;

        co1dlpos++;
        if (co1dlpos==co1dllen) co1dlpos=0;
      }
  }
*/

	// THE REVERB
	if (!cpifaceSession->mcpGet)
	{
		outgainr = 0;
	} else {
		outgainr = cpifaceSession->mcpGet (cpifaceSession, 0, mcpMasterReverb) / 64.0;
	}

	if (outgainr > 0)
	{
		int i;
		for (i=0; i<len; i++)
		{
			int j;
			float v1, v2;

			for (j=0; j<6; j++)
			{
				if (++lpos[j]>=llen[j]) lpos[j]=0;
				if (++rpos[j]>=rlen[j]) rpos[j]=0;
			}

			v1 = buf[i*2  ];
			v2 = buf[i*2+1];
			lpl += lpconst*(v1-lpl);
			lpr += lpconst*(v2-lpr);

			// apply reverb
			buf[i*2  ]+=doreverb(v2-lpr, rpos, rightl, rlpf) * outgainr;
			buf[i*2+1]+=doreverb(v1-lpl, lpos,  leftl, llpf) * outgainr;
		}
	}
}

static int fReverb_processkey(uint16_t key)
{
	return 0;
}

static int fReverb_GetNumVolume()
{
	return sizeof(revvol) / sizeof(revvol[0]);
}

static int fReverb_GetVolume (struct ocpvolstruct *v, int n)
{
	if (running && n<(sizeof(revvol)/sizeof(revvol[0])))
	{
		*v = revvol[n];
		return !0;
	}
	return 0;
}

static int fReverb_SetVolume (struct ocpvolstruct *v, int n)
{
	if(n < (sizeof(revvol)/sizeof(revvol[0])))
	{
		revvol[n] = *v;
		updatevol(n);
		return !0;
	}
	return 0;
}

static const struct ocpvolregstruct volrev =
{
	fReverb_GetNumVolume,
	fReverb_GetVolume,
	fReverb_SetVolume
};

static struct PostProcFPRegStruct fReverb =
{
	"fReverb",
	fReverb_process,
	fReverb_init,
	fReverb_close,
	&volrev,
	fReverb_processkey
};

static int fReverbPluginInit (struct PluginInitAPI_t *API)
{
	API->mcpRegisterPostProcFP (&fReverb);

	return errOk;
}

static void fReverbPluginClose (struct PluginCloseAPI_t *API)
{
	API->mcpUnregisterPostProcFP (&fReverb);
}

DLLEXTINFO_DRIVER_PREFIX struct linkinfostruct dllextinfo = {.name = "freverb", .desc = "OpenCP floating point reverb (c) 1994-'25 Fabian Giesen, Tammo Hinrichs", .ver = DLLVERSION, .sortindex = 99, .PluginInit = fReverbPluginInit, .PluginClose = fReverbPluginClose};

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

