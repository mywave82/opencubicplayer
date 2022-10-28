/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * CPIface main interface code
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
 *  -cp1.7   Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *    -first release
 *  -kb_in_between   Tammo Hinrichs <opencp@gmx.net>
 *    -reintegrated it into OpenCP, 'coz it r00ls ;)
 *  -fd980717  Felix Domke <tmbinc@gmx.net>
 *    -added Wuerfel Mode ][ (320x200)
 */

#include "config.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "types.h"
#include "stuff/poutput.h"
#include "cpiface.h"
#include "stuff/compat.h"
#include "boot/psetting.h"

static unsigned char *plWuerfel=NULL;
static int plWuerfelDirect;
static uint8_t wuerfelpal[720];
static uint16_t wuerfelpos;
static uint32_t wuerfelscroll;
static uint16_t wuerfelstframes;
static uint16_t wuerfelframes;
static uint16_t wuerfelrle;
static uint16_t wuerfeldlt;
static uint8_t wuerfellowmem;
static uint8_t *wuerfelloadedframes;
static uint16_t *wuerfelframelens;
static uint32_t *wuerfelframepos;
static uint16_t *wuerfelcodelens;
static uint32_t wuerfelframe0pos;
static uint32_t wuerfelframesize;
static uint32_t wuerfelscanlines, wuerfellinelength, wuerfelversion;
static FILE *wuerfelfile = 0;
static uint8_t *wuerfelframebuf;
static uint32_t cfUseAnis=0xFFFFFFFF;
static uint32_t wuerfelFilesCount=0;
static char **wuerfelFiles=0;

static struct timespec wurfelTicker;
static uint32_t wurfelTicks;


static void memcpyintr(uint8_t *d, const uint8_t *s, unsigned long l)
{
	do {
		*d=*s;
		d++;
		*d=*s;
		d+=3;
		s++;
		*d=*s;
		d++;
		*d=*s;
		d+=3;
		s+=3;
		l-=2;
	} while(l);
}

static int plCloseWuerfel(void)
{
	if (plWuerfel)
	{
		free(plWuerfel);
		plWuerfel=NULL;
		if (wuerfelcodelens)
			free(wuerfelcodelens);
		if (wuerfelframelens)
			free(wuerfelframelens);
		if (wuerfelframepos)
			free(wuerfelframepos);
		if (wuerfelframebuf)
			free(wuerfelframebuf);
		if (wuerfelloadedframes)
			free(wuerfelloadedframes);
		wuerfelcodelens=NULL;
		wuerfelframelens=NULL;
		wuerfelframepos=NULL;
		wuerfelframebuf=NULL;
		wuerfelloadedframes=NULL;
		if (wuerfelfile)
		{
			fclose(wuerfelfile);
			wuerfelfile=0;
		}
		return 1;
	}
	return 0;
}

static char plLoadWuerfel(void)
{
	uint8_t sig[8];
	uint16_t opt, pallen, codelenslen;
	int i;
	uint16_t maxframe;
	uint32_t framemem;

	if (plWuerfel)
		plCloseWuerfel();

	if (wuerfelFilesCount<=0)
	{
		fprintf(stderr, __FILE__ ": no wuerfel animations found\n");
		return 0;
	}

	cfUseAnis = (uint32_t)(((double)rand()/((double)RAND_MAX + (double)1))*(double)(wuerfelFilesCount-1));
	if (cfUseAnis >= wuerfelFilesCount)
		cfUseAnis = wuerfelFilesCount-1;

	wuerfelfile=fopen(wuerfelFiles[cfUseAnis], "r");
	if (!wuerfelfile)
	{
		perror(__FILE__" fopen:");
		return 0;
	}

	if (fread(sig, 8, 1, wuerfelfile)!=1)
	{
		perror(__FILE__ " fread #1:");
		plCloseWuerfel();
		return 0;
	}
	if (memcmp(sig, "CPANI\x1A\x00\x00", 8))
	{
		fprintf(stderr, __FILE__ ": invalid signature\n");
		plCloseWuerfel();
		return 0;
	}
	if (fseek(wuerfelfile, 32, SEEK_CUR))
	{
		perror(__FILE__ " fseek #1:");
		plCloseWuerfel();
		return 0;
	}

	if (fread(&wuerfelframes, 2, 1, wuerfelfile)!=1)
	{
		perror(__FILE__ " fread #2:");
		plCloseWuerfel();
		return 0;
	}
	wuerfelframes=uint16_little(wuerfelframes);
	if (fread(&wuerfelstframes, 2, 1, wuerfelfile)!=1)
	{
		perror(__FILE__ " fread #3:");
		plCloseWuerfel();
		return 0;
	}
	wuerfelstframes=uint16_little(wuerfelstframes);

	if (fread(&opt, 2, 1, wuerfelfile)!=1)
	{
		perror(__FILE__ " fread #4:");
		plCloseWuerfel();
		return 0;
	}
	opt=uint16_little(opt);
	wuerfelrle=opt&1;
	wuerfeldlt=!!(opt&2);
	wuerfelframesize=(opt&4)?64000:16000;
	wuerfelscanlines=(opt&4)?200:100;
	wuerfellinelength=(opt&4)?320:160;
	wuerfelversion=!!(opt&4);

	wuerfelframelens=calloc(sizeof(uint16_t), wuerfelframes+wuerfelstframes);
	wuerfelframepos=calloc(sizeof(uint32_t), wuerfelframes+wuerfelstframes);
	wuerfelframebuf=calloc(sizeof(uint8_t), wuerfelframesize);
	wuerfelloadedframes=calloc(sizeof(uint8_t), wuerfelframes+wuerfelstframes);

	if (!wuerfelframelens||!wuerfelframepos||!wuerfelframebuf||!wuerfelloadedframes)
	{
		fprintf(stderr, __FILE__ " calloc() failed\n");
		plCloseWuerfel();
		return 0;
	}

	if (fseek(wuerfelfile, 2, SEEK_CUR))
	{
		perror(__FILE__ " fseek #2:");
		plCloseWuerfel();
		return 0;
	}
	if (fread(&codelenslen, 2, 1, wuerfelfile)!=1)
	{
		perror(__FILE__ " fread #5:");
		plCloseWuerfel();
		return 0;
	}
	codelenslen=uint16_little(codelenslen);
	wuerfelcodelens=calloc(sizeof(uint16_t), codelenslen);
	if (!wuerfelcodelens)
	{
		fprintf(stderr, __FILE__ ": Invalid file\n");
		plCloseWuerfel();
		return 0;
	}
	if (fread(&pallen, 2, 1, wuerfelfile)!=1)
	{
		perror(__FILE__ " fread #6:");
		plCloseWuerfel();
		return 0;
	}
	pallen=uint16_little(pallen);
	if (fread(wuerfelframelens, 2*(wuerfelframes+wuerfelstframes), 1, wuerfelfile)!=1)
	{
		perror(__FILE__ " fread #7:");
		plCloseWuerfel();
		return 0;
	}

	if(wuerfelversion)
	{
		if (fread(wuerfelcodelens, codelenslen, 1, wuerfelfile)!=1)
		{
			perror(__FILE__ " fread #8:");
			plCloseWuerfel();
			return 0;
		}
	} else {
		if (fseek(wuerfelfile, codelenslen, SEEK_CUR))
		{
			perror(__FILE__ " fseek #3");
			plCloseWuerfel();
			return 0;
		}
	}

	if (fread(wuerfelpal, pallen, 1, wuerfelfile)!=1)
	{
		perror(__FILE__ " fread #9:");
		plCloseWuerfel();
		return 0;
	}

	memset(wuerfelloadedframes, 0, wuerfelframes+wuerfelstframes);
	wuerfelframepos[0]=0;
	maxframe=0;
	for (i=1; i<(wuerfelframes+wuerfelstframes); i++)
	{
		if (maxframe<wuerfelframelens[i-1])
			maxframe=wuerfelframelens[i-1];
		wuerfelframepos[i]=wuerfelframepos[i-1]+wuerfelframelens[i-1];
	}

	if (maxframe<wuerfelframelens[i-1])
		maxframe=wuerfelframelens[i-1];

	framemem=wuerfelframepos[i-1]+wuerfelframelens[i-1];

	plWuerfel=calloc(sizeof(uint8_t),  framemem);

	wuerfelframe0pos=ftell(wuerfelfile);

	if (plWuerfel)
	{
		wuerfellowmem=0;
		/* do preload, if desired! */
	} else {
		for (i=0; i<wuerfelstframes; i++)
			framemem-=wuerfelframelens[i];
		plWuerfel=calloc(sizeof(uint8_t), framemem);
		if (plWuerfel)
			wuerfellowmem=1;
		else {
			free(wuerfelloadedframes);
			wuerfelloadedframes=NULL;
			wuerfellowmem=2;
			plWuerfel=calloc(sizeof(uint8_t), maxframe);
			if (!plWuerfel)
			{
				fprintf(stderr, "calloc() failed\n");
				plCloseWuerfel();
				return 0;
			}
		}
	}

	return 1;
}

static void plPrepareWuerfel(void)
{
	int i;
	vga13();
/*
	if(!wuerfelversion)
	{
		outp(0x3c4, 4);
		outp(0x3c5, (inp(0x3c5)&~8)|4);
		outp(0x3d4, 0x14);
		outp(0x3d5, inp(0x3d5)&~0x40);
		outp(0x3d4, 0x17);
		outp(0x3d5, inp(0x3d5)|0x40);
		outp(0x3d4, 0x09);
		outp(0x3d5, inp(0x3d5)|2);
		 outpw(0x3c4, 0x0F02);
	}
	memset((char*)0xA0000, 0, 65536);
	*/
	for (i=16; i<256; i++)
		gupdatepal(i, wuerfelpal[i*3-48], wuerfelpal[i*3+1-48], wuerfelpal[i*3+2-48]);
	gflushpal();
	wuerfelpos=0;
	wuerfelscroll=0;
/* This was commented out
    outpw(0x3c4, 0x0302);
    memcpyintr((void*)(0xA0000+80*24), plWuerfel[wuerfelpos], 160*76/2);
    outpw(0x3c4, 0x0C02);
    memcpyintr((void*)(0xA0000+80*24), plWuerfel[wuerfelpos]+1, 160*76/2);
*/
}

static void decodrle(uint8_t *rp, uint16_t rbuflen)
{
	uint8_t *op=wuerfelframebuf;
	uint8_t *re=rp+rbuflen;
	while (rp<re)
	{
		uint8_t c=*rp++;
		if (c<=0x0F)
		{
			memset(op, *rp++, c+3);
			op+=c+3;
		}  else
			*op++=c;
	}
}

static void decodrledlt(uint8_t *rp, uint16_t rbuflen)
{
	uint8_t *op=wuerfelframebuf;
	uint8_t *re=rp+rbuflen;
	while (rp<re)
	{
		uint8_t c=*rp++;
		if (c<=0x0E)
		{
			uint8_t c2=*rp++;
			if (c2!=0x0F)
				memset(op, c2, c+3);
			op+=c+3;
		} else {
			if (c!=0x0F)
				*op=c;
			op++;
		}
	}
}

static void wuerfelDraw (struct cpifaceSessionAPI_t *cpifaceSession)
{
	unsigned int i;
	uint8_t *curframe;
	uint16_t framelen;
	struct timespec now;

	if (!plWuerfel)
		return;

	if (wuerfelversion && (!wuerfelcodelens))
		return;

	clock_gettime (CLOCK_MONOTONIC, &now);
	now.tv_nsec /= 10000;
	if (now.tv_sec > wurfelTicker.tv_sec)
	{
		wurfelTicks += (now.tv_sec - wurfelTicker.tv_sec) * 100000 + now.tv_nsec - wurfelTicker.tv_nsec;
	} else {
		wurfelTicks += now.tv_nsec - wurfelTicker.tv_nsec;
	}
	wurfelTicker = now;

	if (!wuerfelversion)
	{
		if (wurfelTicks < 4000)
			return;

		wurfelTicks -= 4000;
	} else {
		if (wurfelTicks < wuerfelcodelens[wuerfelpos])
			return;

		wurfelTicks -= wuerfelcodelens[wuerfelpos];
	}

	if (wuerfeldlt)
		plWuerfelDirect=0;

	if (wuerfelpos<wuerfelstframes)
	{
		plWuerfelDirect=0;
		wuerfelscroll=wuerfelscanlines;
	}

	framelen=wuerfelframelens[wuerfelpos];
	if (wuerfellowmem==2)
	{
		if (fseek(wuerfelfile, wuerfelframe0pos+wuerfelframepos[wuerfelpos], SEEK_SET))
			fprintf(stderr, __FILE__ ": warning, fseek failed() #1\n");
		if (fread(plWuerfel, framelen, 1, wuerfelfile)!=1)
			fprintf(stderr, __FILE__ ": warning, fseek failed() #1\n");
		curframe=plWuerfel;
	} else if (wuerfellowmem==1)
	{
		if (wuerfelpos<wuerfelstframes)
		{
			if (fseek(wuerfelfile, wuerfelframe0pos+wuerfelframepos[wuerfelpos], SEEK_SET))
				fprintf(stderr, __FILE__ ": warning, fseek failed() #2\n");
			if (fread(plWuerfel, framelen, 1, wuerfelfile)!=1)
				fprintf(stderr, __FILE__ ": warning, fseek failed() #2\n");
			curframe=plWuerfel;
		} else {
			curframe=plWuerfel+wuerfelframepos[wuerfelpos];
			if (!wuerfelloadedframes[wuerfelpos])
			{
				if (fseek(wuerfelfile, wuerfelframe0pos+wuerfelframepos[wuerfelpos], SEEK_SET))
					fprintf(stderr, __FILE__ ": warning, fseek failed() #3\n");
			        if (fread(curframe, framelen, 1, wuerfelfile)!=1)
					fprintf(stderr, __FILE__ ": warning, fseek failed() #3\n");
			        wuerfelloadedframes[wuerfelpos]=1;
			}
		}
	} else {
		curframe=plWuerfel+wuerfelframepos[wuerfelpos];
		if (!wuerfelloadedframes[wuerfelpos])
		{
			if (fseek(wuerfelfile, wuerfelframe0pos+wuerfelframepos[wuerfelpos], SEEK_SET))
				fprintf(stderr, __FILE__ ": warning, fseek failed() #4\n");
			if (fread(curframe, framelen, 1, wuerfelfile)!=1)
				fprintf(stderr, __FILE__ ": warning, fseek failed() #4\n");
			wuerfelloadedframes[wuerfelpos]=1;
		}
	}

	if (wuerfeldlt)
		decodrledlt(curframe, framelen);
	else if (wuerfelrle)
		decodrle(curframe, framelen);
	else
		memcpy(wuerfelframebuf, curframe, framelen);

	for (i=0; i<wuerfelscroll; i++)
	{
		if(!wuerfelversion)
		{
			memcpyintr(plVidMem+320* (100+i-wuerfelscroll)*2,       wuerfelframebuf+i*160, 80);
			memcpyintr(plVidMem+320*((100+i-wuerfelscroll)*2+1),    wuerfelframebuf+i*160, 80);
			memcpyintr(plVidMem+320* (100+i-wuerfelscroll)*2+2,     wuerfelframebuf+i*160+1, 80);
			memcpyintr(plVidMem+320*((100+i-wuerfelscroll)*2+1)+2,  wuerfelframebuf+i*160+1, 80);
		} else {
			memcpy(plVidMem+320*(wuerfelscanlines+i-wuerfelscroll), wuerfelframebuf+i*320, 320);
		}
	}

	if (wuerfelscroll<wuerfelscanlines)
		wuerfelscroll+=(wuerfelversion?2:1);
	if (wuerfelpos<wuerfelstframes)
		wuerfelpos++;
	else
		wuerfelpos=wuerfelstframes+(wuerfelpos-wuerfelstframes+(plWuerfelDirect?(wuerfelframes-1):1))%wuerfelframes;
}

static int wuerfelKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	switch (key)
	{
#if 0
  case 0x9700: /*alt-home*/
  case 0x4700: /*home */
    break;
#endif
		case KEY_TAB: /* tab */
		/*case 0x0F00:  shift-tab
		case 0xA500:*/
			plWuerfelDirect=!plWuerfelDirect;
			return 1;
		case 'w': case 'W':
			plLoadWuerfel();
			plPrepareWuerfel();
			return 1;
	}
	return 0;
}

static void wuerfelSetMode(void)
{
	plLoadWuerfel();
	plPrepareWuerfel();
	clock_gettime (CLOCK_MONOTONIC, &wurfelTicker);
	wurfelTicker.tv_nsec /= 10000;
}

static int wuerfelEvent (struct cpifaceSessionAPI_t *cpifaceSession, int ev)
{
	switch (ev)
	{
		case cpievInitAll:
			return 1;
		case cpievInit:
			if (wuerfelFilesCount<=0)
			{
				/* fprintf(stderr, __FILE__ ": no wuerfel animations found\n"); */
				return 0;
			}
			return 1;
		case cpievDoneAll:
			plCloseWuerfel();
	}
	return 1;
}

static int wuerfelIProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('w', "Enable wurfel mode");
			cpiKeyHelp('W', "Enable wurfel mode");
			return 0;
		case 'w': case 'W':
			if (vga13)
				cpiSetMode("wuerfel2");
		return 1;
	}
	return 0;
}

static struct cpimoderegstruct cpiModeWuerfel = {"wuerfel2", wuerfelSetMode, wuerfelDraw, wuerfelIProcessKey, wuerfelKey, wuerfelEvent CPIMODEREGSTRUCT_TAIL};

static void parse_wurfel_directory (const char *src, DIR *d)
{
	struct dirent *de;

	while ( ( de = readdir(d) ) )
	{
		size_t len;
		char **new;
		if ( strncasecmp ("CPANI", de->d_name, 5) )
			continue;
		len = strlen(de->d_name);
		/* Don't bother to check for len > 4 here, since above strncasecmp() will catch that */
		if ( strcasecmp ( de->d_name + len - 4, ".DAT") )
			continue;
		fprintf(stderr, "wuerfel mode: discovered %s%s\n", src, de->d_name);
		new = realloc (wuerfelFiles, (wuerfelFilesCount + 1) * sizeof (char *) );
		if (!new)
		{
			perror(__FILE__ ", realloc() of filelist\n");
			break;
		} else {
			wuerfelFiles = new;
			if (!(wuerfelFiles[wuerfelFilesCount] = malloc (strlen (src) + strlen (de->d_name) + 1)))
			{
				perror(__FILE__ ", strdup() failed\n");
				break;
			}
			sprintf (wuerfelFiles[wuerfelFilesCount], "%s%s", src, de->d_name);
			wuerfelFilesCount++;
		}
	}
}

void __attribute__ ((visibility ("internal"))) cpiWurfel2Init (void)
{
	DIR *d;
	cpiRegisterDefMode(&cpiModeWuerfel);

	d = opendir (cfDataDir);
	if (d)
	{
		parse_wurfel_directory (cfDataDir, d);
		closedir (d);
	}

	d = opendir (cfConfigDir);
	if (d)
	{
		parse_wurfel_directory (cfConfigDir, d);
		closedir (d);
	}
}

void __attribute__ ((visibility ("internal"))) cpiWurfel2Done (void)
{
	unsigned int i;
	for (i=0;i<wuerfelFilesCount;i++)
		free(wuerfelFiles[i]);
	if (wuerfelFiles)
		free(wuerfelFiles);
	cpiUnregisterDefMode(&cpiModeWuerfel);
}
