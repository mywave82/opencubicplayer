/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Sample processing routines (compression, mixer preparation etc)
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
 *  -kb980717   Tammo Hinrichs <opencp@gmx.net>
 *    -fixed some memory allocation bugs
 *  -kbwhenever   Tammo Hinrichs <opencp@gmx.net>
 *    -added sample-to-float conversion for FPU mixer
 *  -ryg990426  Fabian Giesen <fabian@jdcs.su.nw.schule.de>
 *    -applied some changes kebby supplied to me (du fauler sack :)
 *     (whatsthefuckindifference?!? :)
 *  -ss040918   Stian Skjelstad <stian@nixia.no>
 *    -added float to samptomono. Believe it should be there in order to be
 *     perfect (It is currently not hit, but I like it to be there still)
 */

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include "types.h"
#include "mcp.h"

#define SAMPEND 8

static unsigned short abstab[0x200];

static int sampsizefac(int type)
{
	return ((type&mcpSampFloat)?2:((type&mcpSamp16Bit)?1:0))+((type&mcpSampStereo)?1:0);
}

static int stereosizefac(int type)
{
	return (type&mcpSampStereo)?1:0;
}

static void sampto8(struct sampleinfo *s)
{
	int i, l;
	void *newptr;
	s->type&=~mcpSamp16Bit;
	s->type|=mcpSampRedBits;
	l=(s->length+SAMPEND)<<sampsizefac(s->type);
	for (i=0; i<l; i++)
		((int8_t *)s->ptr)[i]=((int8_t *)s->ptr)[2*i+1];
	newptr=realloc(s->ptr,(s->length+SAMPEND)<<sampsizefac(s->type));
	if (!newptr)
	{
		fprintf(stderr, __FILE__ " (sampto8): warning, realloc() failed\n");
	} else
		s->ptr=newptr;
}

static void samptomono(struct sampleinfo *s)
{
	int i;
	int l = s->length+SAMPEND;
	void *newptr;
#ifdef MCP_DEBUG
	fprintf(stderr, __FILE__ ": (samptomono)\n");
	fprintf(stderr, __FILE__ ": s->ptr=%p\n", s->ptr);
#endif
	s->type&=~mcpSampStereo;
	s->type|=mcpSampRedStereo;
	if (s->type&mcpSampFloat)
		for (i=0; i<l; i++)
			((float *)s->ptr)[i]=(((float *)s->ptr)[2*i]+((float *)s->ptr)[2*i+1])/2;
	else if (s->type&mcpSamp16Bit)
		for (i=0; i<l; i++)
			((int16_t *)s->ptr)[i]=(((int16_t *)s->ptr)[2*i]+((int16_t *)s->ptr)[2*i+1])>>1;
	else
		for (i=0; i<l; i++)
			((int8_t *)s->ptr)[i]=(((int8_t *)s->ptr)[2*i]+((int8_t *)s->ptr)[2*i+1])>>1;
	newptr=realloc(s->ptr,l<<sampsizefac(s->type));
#ifdef MCP_DEBUG
	fprintf(stderr, __FILE__ ": realloced buffer=%p\n", newptr);
#endif
	if (!newptr)
	{
		fprintf(stderr, __FILE__ " samptomono(): warning, realloc() failed\n"); /* safe to continue when buffer-shrink failes */
	} else
		s->ptr=newptr;
}


static int samptofloat(struct sampleinfo *s)
{
	int i;
	int l=s->length<<sampsizefac(s->type&mcpSampStereo);
	int l2;
	void *tmpnew;
	float *newptr;
	int extra;
#ifdef MCP_DEBUG
	fprintf(stderr, __FILE__ ": (samptofloat)\n");
	fprintf(stderr, __FILE__ ": s->ptr=%p\n", s->ptr);
#endif
	s->type|=mcpSampFloat;
	l2=s->length<<sampsizefac(s->type);
	tmpnew=realloc(s->ptr,l2);
#ifdef MCP_DEBUG
	fprintf(stderr, __FILE__ ": tmpptr=%p\n", tmpnew);
#endif
	if (!tmpnew)
	{
		fprintf(stderr, __FILE__ " samptofloat(): error, realloc() failed\n");
		return 0;
	}
	s->ptr=tmpnew;
	if (s->type&mcpSampStereo)
		extra=SAMPEND*2;
	else
		extra=SAMPEND;
	newptr = malloc(sizeof(float)*(l+extra));
	if (!newptr)
	{
		fprintf(stderr, __FILE__ " samptofloat(): error, malloc() failed\n");
		return 0;
	}
	if (s->type&mcpSamp16Bit)
	{
		for (i=0;i<l;i++)
			newptr[i]=((int16_t *)s->ptr)[i];
	} else {
		for (i=0;i<l;i++)
			newptr[i]=257.0*((int8_t *)s->ptr)[i];
	}
	for (i=0;i<extra;i++)
		newptr[l+i]=newptr[l-1];
	free(s->ptr);
	s->ptr=newptr;
#ifdef MCP_DEBUG
	fprintf(stderr, __FILE__ ": realloced buffer=%p\n", newptr);
#endif
	return 1;
}

static void repairloop(struct sampleinfo *s)
{
#ifdef MCP_DEBUG
	fprintf(stderr, __FILE__ ": (repairloop)\n");
#endif

	if (s->type&mcpSampLoop)
	{
		if (s->loopend<=s->loopstart)
			s->type&=~mcpSampLoop;
		if (s->loopstart<0)
			s->loopstart=0;
		if (s->loopend<0)
			s->loopend=0;
		if (s->loopstart>s->length)
			s->loopstart=s->length;
		if (s->loopend>s->length)
			s->loopend=s->length;
		if (s->loopend==s->loopstart)
			s->type&=~mcpSampLoop;
	}
	if (s->type&mcpSampSLoop)
	{
		if (s->sloopend<=s->sloopstart)
			s->type&=~mcpSampSLoop;
		if (s->sloopstart<0)
			s->sloopstart=0;
		if (s->sloopend<0)
			s->sloopend=0;
		if (s->sloopstart>s->length)
			s->sloopstart=s->length;
		if (s->sloopend>s->length)
			s->sloopend=s->length;
		if (s->sloopend==s->sloopstart)
			s->type&=~mcpSampSLoop;
	}
	if ((s->type&mcpSampLoop)&&(s->type&mcpSampSLoop)&&((!(s->type&mcpSampBiDi))==(!(s->type&mcpSampSBiDi)))&&(s->loopstart==s->sloopstart)&&(s->loopend==s->sloopend))
		s->type&=~mcpSampSLoop;
}


static int expandsmp(struct sampleinfo *s, int nopingpongloops)
{
	unsigned int newlen=s->length;
	signed int replen=s->loopend-s->loopstart;
	signed int sreplen=s->sloopend-s->sloopstart;
	int restricted=0; /* boolean */
	int toforward=0; /* boolean */
	int stoforward=0; /* boolean */
	unsigned int expandloop=0;
	unsigned int sexpandloop=0;
	signed int c;
	void *newptr;

#ifdef MCP_DEBUG
	fprintf(stderr, __FILE__ ": (expandsmp), nopingpongloops=%d\n", nopingpongloops);
#endif

	if ((s->type&mcpSampLoop)&&(s->type&mcpSampSLoop)&&(s->loopend>s->sloopstart)&&(s->sloopend>s->loopstart))
		restricted=1;

	if ((s->type&mcpSampLoop)&&(s->type&mcpSampBiDi)&&nopingpongloops&&!restricted)
	{
		toforward=1;
		expandloop=replen*toforward;
		replen+=replen*toforward;
	}

	if ((s->type&mcpSampLoop)&&(replen<256)&&!restricted)
	{
		int ln=255/replen;
		if ((s->type&mcpSampBiDi)&&!toforward)
		ln=(ln+1)&~1;
		expandloop+=ln*replen;
	}

	if ((s->type&mcpSampSLoop)&&(s->type&mcpSampSBiDi)&&nopingpongloops&&!restricted)
	{
		stoforward=1;
		sexpandloop=sreplen*stoforward;
		sreplen+=sreplen*stoforward;
	}
	if ((s->type&mcpSampSLoop)&&(sreplen<256)&&!restricted)
	{
		int ln=sexpandloop=255/sreplen;
		if ((s->type&mcpSampSBiDi)&&!stoforward)
			ln=(ln+1)&~1;
		sexpandloop+=ln*sreplen;
	}

	replen=s->loopend-s->loopstart;
	sreplen=s->sloopend-s->sloopstart;

	newlen+=expandloop+sexpandloop;
	if (newlen<2)
		newlen=2;

	newptr=realloc(s->ptr, (newlen+SAMPEND)<<sampsizefac(s->type));
#ifdef MCP_DEBUG
	fprintf(stderr, __FILE__ ": realloced buffer=%p (size=%d)\n", newptr, (newlen+SAMPEND)<<sampsizefac(s->type));
#endif
	if (!newptr)
	{
		fprintf(stderr, __FILE__ " expandsmp(): error, realloc() failed\n");
		return 0;
	}
	s->ptr=newptr;
	if (expandloop)
	{
#ifdef MCP_DEBUG
		fprintf(stderr, __FILE__ ": doing expandloop\n");
		fprintf(stderr, __FILE__ ": s->length=%d\n", s->length);
		fprintf(stderr, __FILE__ ": s->loopend=%d\n", s->loopend);
		fprintf(stderr, __FILE__ ": expandloop=%d\n", expandloop);
		fprintf(stderr, __FILE__ ": replen=%d\n", replen);
		fprintf(stderr, __FILE__ ": mcpSampBiDi set? %d\n", s->type&mcpSampBiDi);
#endif

		if (sampsizefac(s->type)==2)
		{
			float *p=(float *)s->ptr+s->loopend;
			for (c=s->length-s->loopend-1; c>=0; c--)
				p[c+expandloop]=p[c];
			if (!(s->type&mcpSampBiDi))
				for (c=0; c<replen; c++)
					p[c]=p[c-replen];
			else
				for (c=0; c<replen; c++)
					p[c]=p[-1-c];
			for (c=replen; c<expandloop; c++)
				p[c]=p[c-(replen<<1)];
		} else if (sampsizefac(s->type)==1)
		{
			int16_t *p=(int16_t *)s->ptr+s->loopend;
			for (c=s->length-s->loopend-1; c>=0; c--)
				p[c+expandloop]=p[c];
			if (!(s->type&mcpSampBiDi))
				for (c=0; c<replen; c++)
					p[c]=p[c-replen];
			else
				for (c=0; c<replen; c++)
					p[c]=p[-1-c];
			for (c=replen; c<expandloop; c++)
				p[c]=p[c-(replen<<1)];
		} else {
			int8_t *p=(int8_t *)s->ptr+s->loopend;
			for (c=s->length-s->loopend-1; c>=0; c--)
				p[c+expandloop]=p[c];
			if (!(s->type&mcpSampBiDi))
				for (c=0; c<replen; c++)
					p[c]=p[c-replen];
			else
				for (c=0; c<replen; c++)
					p[c]=p[-1-c];
			for (c=replen; c<expandloop; c++)
				p[c]=p[c-(replen<<1)];
		}

		if (s->sloopstart>=s->loopend)
			s->sloopstart+=expandloop;
		if (s->sloopend>=s->loopend)
			s->sloopend+=expandloop;
		s->length+=expandloop;
		s->loopend+=expandloop;
		if (toforward)
			s->type&=~mcpSampBiDi;
		if (toforward==2)
			s->loopstart+=replen;
	}

	if (sexpandloop)
	{
#ifdef MCP_DEBUG
		fprintf(stderr, __FILE__ ": doing sexpandloop\n");
		fprintf(stderr, __FILE__ ": s->length=%d\n", s->length);
		fprintf(stderr, __FILE__ ": s->sloopend=%d\n", s->sloopend);
		fprintf(stderr, __FILE__ ": sexpandloop=%d\n", sexpandloop);
		fprintf(stderr, __FILE__ ": sreplen=%d\n", sreplen);
		fprintf(stderr, __FILE__ ": mcpSampBiDi set? %d\n", s->type&mcpSampBiDi);
#endif
		if (sampsizefac(s->type)==2)
		{
			int l;
			float *p=(float *)s->ptr+s->sloopend;
			for (c=0; c<(s->length-s->sloopend); c++)
				p[c+sexpandloop]=p[c];
			if (!(s->type&mcpSampSBiDi))
				for (c=0; c<sreplen; c++)
					p[c]=p[c-sreplen];
			else
				for (c=0; c<sreplen; c++)
					p[c]=p[-1-c];
			l=sreplen<<1;
			for (c=sreplen; c<sexpandloop; c++)
				p[c]=p[c-l];
		} else
			if (sampsizefac(s->type)==1)
			{
				int16_t *p=(int16_t *)s->ptr+s->sloopend;
				for (c=0; c<(s->length-s->sloopend); c++)
					p[c+sexpandloop]=p[c];
				if (!(s->type&mcpSampSBiDi))
					for (c=0; c<sreplen; c++)
						p[c]=p[c-sreplen];
				else
					for (c=0; c<sreplen; c++)
						p[c]=p[-1-c];
				for (c=sreplen; c<sexpandloop; c++)
					p[c]=p[c-(sreplen<<1)];
			} else {
				int8_t *p=(int8_t *)s->ptr+s->sloopend;
				for (c=0; c<(s->length-s->sloopend); c++)
					p[c+sexpandloop]=p[c];
				if (!(s->type&mcpSampSBiDi))
					for (c=0; c<sreplen; c++)
						p[c]=p[c-sreplen];
				else
					for (c=0; c<sreplen; c++)
						p[c]=p[-1-c];
				for (c=sreplen; c<sexpandloop; c++)
					p[c]=p[c-(sreplen<<1)];
			}

		if (s->loopstart>=s->sloopend)
			s->loopstart+=sexpandloop;
		if (s->loopend>=s->sloopend)
			s->loopend+=sexpandloop;
		s->length+=sexpandloop;
		s->sloopend+=sexpandloop;
		if (stoforward)
			s->type&=~mcpSampSBiDi;
		if (stoforward==2)
			s->sloopstart+=sreplen;
	}

	if (s->length<2)
	{
		if (!s->length)
		{
			if (sampsizefac(s->type)==2)
				*(float *)s->ptr=0.0;
			else if (sampsizefac(s->type)==1)
				*(int16_t *)s->ptr=0;
			else
				*(int8_t *)s->ptr=0;
		}
		if (sampsizefac(s->type)==2)
			((float *)s->ptr)[1]=*(float *)s->ptr;
		else if (sampsizefac(s->type)==1)
			((int16_t *)s->ptr)[1]=*(int16_t *)s->ptr;
		else
			((int8_t *)s->ptr)[1]=*(int8_t *)s->ptr;
		s->length=2;
	}

	return 1;
}

static int repairsmp(struct sampleinfo *s)
{
	int i;
	void *newptr;

#ifdef MCP_DEBUG
	fprintf(stderr, __FILE__ ": (repairsmp)\n");
#endif

	newptr=realloc(s->ptr, (s->length+SAMPEND)<<sampsizefac(s->type));
	if (!newptr)
	{
		fprintf(stderr, __FILE__ ": (repairsmp) error, realloc() failed\n");
		return 0;
	}
	s->ptr=newptr;

	repairloop(s);

	if (sampsizefac(s->type)==2)
	{
		float *p=(float *)s->ptr;

		for (i=0; i<SAMPEND; i++)
			p[s->length+i]=p[s->length-1];
		if ((s->type&mcpSampSLoop)&&!(s->type&mcpSampSBiDi))
		{
			p[s->sloopend]=p[s->sloopstart];
			p[s->sloopend+1]=p[s->sloopstart+1];
		}
		if ((s->type&mcpSampLoop)&&!(s->type&mcpSampBiDi))
		{
			p[s->loopend]=p[s->loopstart];
			p[s->loopend+1]=p[s->loopstart+1];
		}
	} else if (sampsizefac(s->type)==1)
	{
		int16_t *p=(int16_t *)s->ptr;
		for (i=0; i<SAMPEND; i++)
			p[s->length+i]=p[s->length-1];
		if ((s->type&mcpSampSLoop)&&!(s->type&mcpSampSBiDi))
		{
			p[s->sloopend]=p[s->sloopstart];
			p[s->sloopend+1]=p[s->sloopstart+1];
		}
		if ((s->type&mcpSampLoop)&&!(s->type&mcpSampBiDi))
		{
			p[s->loopend]=p[s->loopstart];
			p[s->loopend+1]=p[s->loopstart+1];
		}
	} else {
		int8_t *p=(int8_t *)s->ptr;
		for (i=0; i<SAMPEND; i++)
			p[s->length+i]=p[s->length-1];
		if ((s->type&mcpSampSLoop)&&!(s->type&mcpSampSBiDi))
		{
			p[s->sloopend]=p[s->sloopstart];
			p[s->sloopend+1]=p[s->sloopstart+1];
		}
		if ((s->type&mcpSampLoop)&&!(s->type&mcpSampBiDi))
		{
			p[s->loopend]=p[s->loopstart];
			p[s->loopend+1]=p[s->loopstart+1];
		}
	}
	return 1;
}

#include "smpman_asminc.c"

static void dividefrq(struct sampleinfo *s)
{
	int i;
	int l=s->length>>1;
	void *newptr;

#ifdef MCP_DEBUG
	fprintf(stderr, __FILE__ ": (dividefrq)\n");
#endif

	if (sampsizefac(s->type)==2)
		for (i=0; i<l; i++)
			((float *)s->ptr)[i]=((float *)s->ptr)[2*i];
	else if (sampsizefac(s->type)==1)
		for (i=0; i<l; i++)
			((int16_t *)s->ptr)[i]=((int16_t *)s->ptr)[2*i];
	else for (i=0; i<l; i++)
		((int8_t *)s->ptr)[i]=((int8_t *)s->ptr)[2*i];

	s->length>>=1;
	s->loopstart>>=1;
	s->loopend>>=1;
	s->sloopstart>>=1;
	s->sloopend>>=1;
	s->samprate>>=1;
	s->type|=(s->type&mcpSampRedRate2)?mcpSampRedRate4:mcpSampRedRate2;

	newptr=realloc(s->ptr, (s->length+SAMPEND)<<sampsizefac(s->type));
	if (!newptr)
	{
		fprintf(stderr, __FILE__ ": (repairsmp) warning, realloc() failed\n");
	} else
		s->ptr=newptr;
}

static int totalsmpsize(struct sampleinfo *samples, int nsamp, int always16bit)
{
	int i;
	int curdif=0;
	if (always16bit)
		for (i=0; i<nsamp; i++)
			curdif+=(samples[i].length+SAMPEND)<<stereosizefac(samples[i].type);
	else for (i=0; i<nsamp; i++)
		curdif+=(samples[i].length+SAMPEND)<<sampsizefac(samples[i].type);
	return curdif;
}

static int reduce16(struct sampleinfo *samples, int samplenum, uint32_t *redpars, int memmax)
{
	int i;
	int32_t curdif=-memmax;
	int32_t totdif=0;
	for (i=0; i<samplenum; i++)
	{
		struct sampleinfo *s=&samples[i];
		if (s->type&mcpSamp16Bit)
			redpars[i]=(s->length+SAMPEND)<<stereosizefac(s->type);
		else
			redpars[i]=0;
		totdif+=redpars[i];
		curdif+=(s->length+SAMPEND)<<sampsizefac(s->type);
	}

	if (curdif>totdif)
	{
		for (i=0; i<samplenum; i++)
			if (samples[i].type&mcpSamp16Bit)
				sampto8(&samples[i]);
		return 0;
	}

	while (curdif>0)
	{
		int fit=0;
		int best=0; /* init to 0 removes warning */
		uint32_t bestdif=0;
		for (i=0; i<samplenum; i++)
			if ((unsigned)curdif<=redpars[i])
			{
				if (!fit||(bestdif>redpars[i]))
				{
					fit=1;
					bestdif=redpars[i];
					best=i;
				}
			} else {
				if (!fit&&(bestdif<redpars[i]))
				{
					bestdif=redpars[i];
					best=i;
				}
			}
		sampto8(&samples[best]);
		curdif-=redpars[best];
		redpars[best]=0;
	}
	return 1;
}

static int reducestereo(struct sampleinfo *samples, int samplenum, uint32_t *redpars, int memmax)
{
	int i;
	int32_t curdif=-memmax;
	int32_t totdif=0;
	for (i=0; i<samplenum; i++)
	{
		struct sampleinfo *s=&samples[i];
		if (s->type&mcpSampStereo)
			redpars[i]=s->length+SAMPEND;
		else
			redpars[i]=0;
		totdif+=redpars[i];
		curdif+=(s->length+SAMPEND)<<stereosizefac(s->type);
	}

	if (curdif>totdif)
	{
		for (i=0; i<samplenum; i++)
			if (samples[i].type&mcpSampStereo)
				samptomono(&samples[i]);
		return 0;
	}

	while (curdif>0)
	{
		int fit=0;
		int best=0; /* init to 0 removes warning */
		uint32_t bestdif=0;
		for (i=0; i<samplenum; i++)
			if ((unsigned)curdif<=redpars[i])
			{
				if (!fit||(bestdif>redpars[i]))
				{
					fit=1;
					bestdif=redpars[i];
					best=i;
				}
			} else {
				if (!fit&&(bestdif<redpars[i]))
				{
					bestdif=redpars[i];
					best=i;
				}
			}
		samptomono(&samples[best]);
		curdif-=redpars[best];
		redpars[best]=0;
	}
	return 1;
}

static int reducefrq(struct sampleinfo *samples, int samplenum, uint32_t *redpars, int memmax)
{
	int i;
	int32_t curdif;

	for (i=-0x100; i<0x100; i++)
		abstab[i+0x100]=i*i/16;

	curdif=-memmax;
	for (i=0; i<samplenum; i++)
	{
		struct sampleinfo *s=&samples[i];
		curdif+=s->length+SAMPEND;
		if (s->length<1024)
			redpars[i]=0xFFFFFFFF;
		else
			if (s->type&mcpSamp16Bit)
				redpars[i]=getpitch16(s->ptr, s->length)/s->length;
			else
				redpars[i]=getpitch(s->ptr, s->length)/s->length;
	}

	while (curdif>0)
	{
		int best=-1;
		uint32_t bestpitch=0xFFFFFFFF;
		struct sampleinfo *s;
		for (i=0; i<samplenum; i++)
			if (redpars[i]<bestpitch)
			{
				bestpitch=redpars[i];
				best=i;
			}
		if (best==-1)
			return 0;

		s=&samples[best];
		curdif-=s->length+SAMPEND;
		dividefrq(s);
		curdif+=s->length+SAMPEND;

		if ((s->length<1024)||(s->type&mcpSampRedRate4))
			redpars[best]=0xFFFFFFFF;
		else
			if (s->type&mcpSamp16Bit)
				redpars[best]=(getpitch16(s->ptr, s->length)/s->length)<<1;
			else
				redpars[best]=(getpitch(s->ptr, s->length)/s->length)<<1;
	}

	return 1;
}

static int convertsample(struct sampleinfo *s)
{
	unsigned int i;

#ifdef MCP_DEBUG
	fprintf(stderr, __FILE__ ": (convertsample)\n");
	if (s->loopstart>=s->loopend)
		fprintf(stderr, __FILE__ ": s->loopstart>=s->loopend, masking away mcpSampLoop flag if set\n");
#endif

	if (s->loopstart>=s->loopend)
		s->type&=~mcpSampLoop;

#ifndef WORDS_BIGENDIAN
	if ((s->type&(mcpSampBigEndian|mcpSamp16Bit))==(mcpSampBigEndian|mcpSamp16Bit))
#else
	if ((s->type&(mcpSampBigEndian|mcpSamp16Bit))==(mcpSamp16Bit))
#endif
	{
		unsigned int l=s->length<<sampsizefac(s->type);
#ifdef MCP_DEBUG
		fprintf(stderr, __FILE__ ": sampledata is 16bit and endian does not match host system, converting\n");
#endif
		for (i=0; i<l; i+=2)
		{
			char tmp=((char *)s->ptr)[i];
			((char*)s->ptr)[i]=((char*)s->ptr)[i+1];
			((char*)s->ptr)[i+1]=tmp;
		}
#ifndef WORDS_BIGENDIAN
		s->type&=~mcpSampBigEndian;
#else
		s->type|=mcpSampBigEndian;
#endif
	}

	if (s->type&mcpSampDelta)
	{
#ifdef MCP_DEBUG
		fprintf(stderr, __FILE__ ": sampledata is stored as deltas, making linear\n");
#endif
		if (s->type&mcpSampStereo)
		{
			if (s->type&mcpSamp16Bit)
			{
				int16_t oldl=0;
				int16_t oldr=0;
				for (i=0; i<s->length; i++)
				{
					oldl=(((int16_t *)s->ptr)[2*i]+=oldl);
					oldr=(((int16_t *)s->ptr)[2*i+1]+=oldr);
				}
			} else {
				int8_t oldl=0;
				int8_t oldr=0;
				for (i=0; i<s->length; i++)
				{
					oldl=(((int8_t *)s->ptr)[2*i]+=oldl);
					oldr=(((int8_t *)s->ptr)[2*i+1]+=oldr);
				}
			}
		} else {
			if (s->type&mcpSamp16Bit)
			{
				int16_t old=0;
				for (i=0; i<s->length; i++)
					old=(((int16_t *)s->ptr)[i]+=old);
			} else {
				int8_t old=0;
				for (i=0; i<s->length; i++)
					old=(((int8_t *)s->ptr)[i]+=old);
			}
		}
		s->type&=~(mcpSampDelta|mcpSampUnsigned);
	}

	if (s->type&mcpSampUnsigned)
	{
		unsigned int l=s->length<<stereosizefac(s->type);
#ifdef MCP_DEBUG
		fprintf(stderr, __FILE__ ": sampledata is stored unsigned, making it signed\n");
#endif
		if (s->type&mcpSamp16Bit)
		{
			for (i=0; i<l; i++)
				((int16_t *)s->ptr)[i]^=0x8000;
		} else {
			for (i=0; i<l; i++)
				((int8_t *)s->ptr)[i]^=0x80;
		}
		s->type&=~mcpSampUnsigned;
	}

	return 1;
}

int mcpReduceSamples(struct sampleinfo *si, int n, long mem, int opt)
{
	struct sampleinfo *samples=si;
	int32_t memmax=mem;
	int samplenum=n;

	int i;

#ifdef MCP_DEBUG
	fprintf(stderr, __FILE__ ": mcpReduceSamples STARTED\n");
	for (i=0; i<samplenum; i++)
	{
		fprintf(stderr, "type[%d]", i);
		if (si[i].type & mcpSampUnsigned)
			fprintf(stderr, " unsigned");
		else
			fprintf(stderr, " signed");
		if (si[i].type & mcpSampDelta)
			fprintf(stderr, " delta");
		if (si[i].type & mcpSampFloat)
			fprintf(stderr, " float");
		else if (si[i].type & mcpSamp16Bit)
			fprintf(stderr, " 16bit");
		else
			fprintf(stderr, " 8bit");
		if (si[i].type & mcpSampBigEndian)
			fprintf(stderr, " bigendian");
		else
			fprintf(stderr, " littleendian");
		if (si[i].type & mcpSampBiDi)
			fprintf(stderr, " bidi-loop");
		else if (si[i].type & mcpSampLoop)
			fprintf(stderr, " loop");
		if (si[i].type & mcpSampSBiDi)
			fprintf(stderr, " sbidi-loop");
		else if (si[i].type & mcpSampSLoop)
			fprintf(stderr, " sloop");
		if (si[i].type & mcpSampStereo)
			fprintf(stderr, " stereo");
		else
			fprintf(stderr, " mono");
		fprintf(stderr, "\n");
		fprintf(stderr, "ptr=%p\n", si[i].ptr);
		fprintf(stderr, "length=%d\n", (int)si[i].length);
		fprintf(stderr, "samprate=%d\n", (int)si[i].samprate);
		fprintf(stderr, "loopstart=%d\n", (int)si[i].loopstart);
		fprintf(stderr, "loopend=%d\n", (int)si[i].loopend);
		fprintf(stderr, "sloopstart=%d\n", (int)si[i].sloopstart);
		fprintf(stderr, "sloopend=%d\n", (int)si[i].sloopend);
		fprintf(stderr, "\n\n");
	}
#endif
	for (i=0; i<samplenum; i++)
	{
#ifdef MCP_DEBUG
		fprintf(stderr, __FILE__ ": [%d]\n", i);
#endif
		if (!convertsample(&samples[i]))
		{
#ifdef MCP_DEBUG
			fprintf(stderr, __FILE__ ": mcpReduceSamples FAILED\n");
#endif
			return 0;
		}
		repairloop(&samples[i]);
		if (!expandsmp(&samples[i], opt&mcpRedNoPingPong))
		{
#ifdef MCP_DEBUG
			fprintf(stderr, __FILE__ ": mcpReduceSamples FAILED\n");
#endif
			return 0;
		}

	}

	if (opt&mcpRedToMono)
		for (i=0; i<samplenum; i++)
			if (samples[i].type&mcpSampStereo)
				samptomono(&samples[i]);

	if (opt&(mcpRedGUS|mcpRedTo8Bit))
		for (i=0; i<samplenum; i++)
			if ((samples[i].type&mcpSamp16Bit)&&((opt&mcpRedTo8Bit)||((samples[i].length+SAMPEND)>(128*1024))))
				sampto8(&samples[i]);

	if (totalsmpsize(samples, samplenum, opt&mcpRedAlways16Bit)>memmax)
	{
		uint32_t *redpars=malloc(sizeof(uint32_t)*samplenum);

		fprintf (stderr, "[smpman] Sample bank size %d is bigger than maximum configured in ocp.ini %d. Reducing quality\n", totalsmpsize (samples, samplenum, opt&mcpRedAlways16Bit), (int)memmax);

		if (!redpars)
			return 0;
		if ((opt&mcpRedAlways16Bit)||!reduce16(samples, samplenum, redpars, memmax))
			if (!reducestereo(samples, samplenum, redpars, memmax))
				if (!reducefrq(samples, samplenum, redpars, memmax))
				{
					free(redpars);
#ifdef MCP_DEBUG
					fprintf(stderr, __FILE__ ": mcpReduceSamples FAILED\n");
#endif
					return 0;
				}
		free(redpars);
	}

	for (i=0; i<samplenum; i++)
		if (!repairsmp(&samples[i]))
		{
#ifdef MCP_DEBUG
			fprintf(stderr, __FILE__ ": mcpReduceSamples FAILED\n");
#endif
			return 0;
		}

	if (opt&mcpRedToFloat)
		for (i=0; i<samplenum; i++)
			if (!samptofloat(&samples[i]))
			{
#ifdef MCP_DEBUG
				fprintf(stderr, __FILE__ ": mcpReduceSamples FAILED\n");
#endif
				return 0;
			}
#ifdef MCP_DEBUG
	fprintf(stderr, __FILE__ ": mcpReduceSamples DONE\n");
#endif

	return 1;
}
