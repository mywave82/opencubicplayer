/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2005-'25 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * C version of dwmixa assembler routines
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
 */

/*static uint32_t (*mixrFadeChannelvoltab)[256];*/
static int32_t ramping[2][2];
static int32_t (*mixrFadeChannelvoltab)[256];
static uint8_t (*mixrFadeChannelintrtab)[256][2];

void mixrSetupAddresses(int32_t (*vol)[256], uint8_t (*intr)[256][2])
{
	mixrFadeChannelvoltab=vol;
	mixrFadeChannelintrtab=intr;
}

void mixrFadeChannel(int32_t *fade, struct channel *chan)
{
	if (chan->status&MIXRQ_PLAYSTEREO)
	{
		if (chan->status&MIXRQ_PLAY16BIT)
		{
			fade[0]+=mixrFadeChannelvoltab[chan->curvols[0][0]][((uint16_t)chan->realsamp.bit16[(chan->pos<<1)    ])>>8];
			fade[0]+=mixrFadeChannelvoltab[chan->curvols[0][1]][((uint16_t)chan->realsamp.bit16[(chan->pos<<1) + 1])>>8];
			fade[1]+=mixrFadeChannelvoltab[chan->curvols[1][0]][((uint16_t)chan->realsamp.bit16[(chan->pos<<1)    ])>>8];
			fade[1]+=mixrFadeChannelvoltab[chan->curvols[1][1]][((uint16_t)chan->realsamp.bit16[(chan->pos<<1) + 1])>>8];
		} else {
			fade[0]+=mixrFadeChannelvoltab[chan->curvols[0][0]][(uint8_t)chan->realsamp.bit8[(chan->pos<<1)    ]];
			fade[0]+=mixrFadeChannelvoltab[chan->curvols[0][1]][(uint8_t)chan->realsamp.bit8[(chan->pos<<1) + 1]];
			fade[1]+=mixrFadeChannelvoltab[chan->curvols[1][0]][(uint8_t)chan->realsamp.bit8[(chan->pos<<1)    ]];
			fade[1]+=mixrFadeChannelvoltab[chan->curvols[1][1]][(uint8_t)chan->realsamp.bit8[(chan->pos<<1) + 1]];
		}
	} else {
		if (chan->status&MIXRQ_PLAY16BIT)
		{
			fade[0]+=mixrFadeChannelvoltab[chan->curvols[0][0]][((uint16_t)chan->realsamp.bit16[chan->pos])>>8];
			fade[0]+=mixrFadeChannelvoltab[chan->curvols[0][1]][((uint16_t)chan->realsamp.bit16[chan->pos])>>8];
			fade[1]+=mixrFadeChannelvoltab[chan->curvols[1][0]][((uint16_t)chan->realsamp.bit16[chan->pos])>>8];
			fade[1]+=mixrFadeChannelvoltab[chan->curvols[1][1]][((uint16_t)chan->realsamp.bit16[chan->pos])>>8];
		} else {
			fade[0]+=mixrFadeChannelvoltab[chan->curvols[0][0]][(uint8_t)chan->realsamp.bit8[chan->pos]];
			fade[0]+=mixrFadeChannelvoltab[chan->curvols[0][1]][(uint8_t)chan->realsamp.bit8[chan->pos]];
			fade[1]+=mixrFadeChannelvoltab[chan->curvols[1][0]][(uint8_t)chan->realsamp.bit8[chan->pos]];
			fade[1]+=mixrFadeChannelvoltab[chan->curvols[1][1]][(uint8_t)chan->realsamp.bit8[chan->pos]];
		}
	}
	chan->curvols[0][0]=0;
	chan->curvols[0][1]=0;
	chan->curvols[1][0]=0;
	chan->curvols[1][1]=0;
}

static inline int32_t interp_none8(const struct channel *chan, const int volindex, const uint32_t pos, uint32_t fpos)
{
	return mixrFadeChannelvoltab[volindex][(uint8_t)(chan->realsamp.bit8[pos])];
}

static inline int32_t interp_none16(const struct channel *chan, const int volindex, const uint32_t pos, uint32_t fpos)
{
	return mixrFadeChannelvoltab[volindex][((uint16_t)(chan->realsamp.bit16[pos]))>>8];
}

static inline int32_t interp_i8(const struct channel *chan, const int volindex, const uint32_t pos, uint32_t fpos)
{
	uint8_t cache =
		mixrFadeChannelintrtab[fpos>>12][((uint8_t)(chan->realsamp.bit8[pos  ]))][0]+
		mixrFadeChannelintrtab[fpos>>12][((uint8_t)(chan->realsamp.bit8[pos+1]))][1];
	return mixrFadeChannelvoltab[volindex][cache];
}

static inline int32_t interp_i16(const struct channel *chan, const int volindex, const uint32_t pos, uint32_t fpos)
{
	uint8_t cache =
		mixrFadeChannelintrtab[fpos>>12][((uint16_t)(chan->realsamp.bit16[pos  ]))>>8][0]+
		mixrFadeChannelintrtab[fpos>>12][((uint16_t)(chan->realsamp.bit16[pos+1]))>>8][1];
	return mixrFadeChannelvoltab[volindex][cache];
}

#define MIX_TEMPLATE_M(NAME, INTERP)                                    \
static void                                                             \
NAME(int32_t *buf,                                                      \
     uint32_t len,                                                      \
     struct channel *chan)                                              \
{                                                                       \
    int32_t vol00    = chan->curvols[0][0];                             \
    int32_t vol00add = ramping[0][0];                                   \
    int32_t vol01    = chan->curvols[0][1];                             \
    int32_t vol01add = ramping[0][1];                                   \
    int32_t vol10    = chan->curvols[1][0];                             \
    int32_t vol10add = ramping[1][0];                                   \
    int32_t vol11    = chan->curvols[1][1];                             \
    int32_t vol11add = ramping[1][1];                                   \
    uint32_t pos=chan->pos;                                             \
    uint32_t fpos=chan->fpos;                                           \
                                                                        \
    while (len)                                                         \
        {                                                               \
            *(buf++)+=interp_##INTERP(chan, vol00, pos, fpos) +         \
                      interp_##INTERP(chan, vol01, pos, fpos);          \
            *(buf++)+=interp_##INTERP(chan, vol10, pos, fpos) +         \
                      interp_##INTERP(chan, vol11, pos, fpos);          \
            fpos+=chan->step&0x0000ffff;                                \
            if (fpos&0xffff0000)                                        \
            {                                                           \
                pos++;                                                  \
                fpos&=0xffff;                                           \
            }                                                           \
            pos   += chan->step>>16;                                    \
            vol00 += vol00add;                                          \
            vol01 += vol01add;                                          \
            vol10 += vol10add;                                          \
            vol11 += vol11add;                                          \
            len--;                                                      \
        }                                                               \
}

MIX_TEMPLATE_M(playstereo,    none8)
MIX_TEMPLATE_M(playstereoi,   i8)
MIX_TEMPLATE_M(playstereo16,  none16)
MIX_TEMPLATE_M(playstereoi16, i16)

static inline void interp_none8_s(const struct channel *chan, const int volindexLL, const int volindexLR, const int volindexRL, const int volindexRR, const uint32_t pos, uint32_t fpos, int32_t * const outL, int32_t * const outR)
{
	*outL = mixrFadeChannelvoltab[volindexLL][(uint8_t)(chan->realsamp.bit8[(pos<<1)  ])]
	      + mixrFadeChannelvoltab[volindexLR][(uint8_t)(chan->realsamp.bit8[(pos<<1)+1])];
	*outR = mixrFadeChannelvoltab[volindexRL][(uint8_t)(chan->realsamp.bit8[(pos<<1)  ])]
	      + mixrFadeChannelvoltab[volindexRR][(uint8_t)(chan->realsamp.bit8[(pos<<1)+1])];
}

static inline void interp_none16_s(const struct channel *chan, const int volindexLL, const int volindexLR, const int volindexRL, const int volindexRR, const uint32_t pos, uint32_t fpos, int32_t * const outL, int32_t * const outR)
{
	*outL = mixrFadeChannelvoltab[ volindexLL ][ ((uint16_t)(chan->realsamp.bit16[(pos<<1)  ]))>>8 ]
	      + mixrFadeChannelvoltab[ volindexLR ][ ((uint16_t)(chan->realsamp.bit16[(pos<<1)+1]))>>8 ];
	*outR = mixrFadeChannelvoltab[ volindexRL ][ ((uint16_t)(chan->realsamp.bit16[(pos<<1)  ]))>>8 ]
	      + mixrFadeChannelvoltab[ volindexRR ][ ((uint16_t)(chan->realsamp.bit16[(pos<<1)+1]))>>8 ];
}

static inline void interp_i8_s(const struct channel *chan, const int volindexLL, const int volindexLR, const int volindexRL, const int volindexRR, const uint32_t pos, uint32_t fpos, int32_t * const outL, int32_t * const outR)
{
	uint8_t cacheL =
		mixrFadeChannelintrtab[ fpos>>12 ][ ((uint8_t)(chan->realsamp.bit8[(pos<<1)  ])) ][ 0 ]+
		mixrFadeChannelintrtab[ fpos>>12 ][ ((uint8_t)(chan->realsamp.bit8[(pos<<1)+2])) ][ 1 ];
	uint8_t cacheR =
		mixrFadeChannelintrtab[ fpos>>12 ][ ((uint8_t)(chan->realsamp.bit8[(pos<<1)+1])) ][ 0 ]+
		mixrFadeChannelintrtab[ fpos>>12 ][ ((uint8_t)(chan->realsamp.bit8[(pos<<1)+3])) ][ 1 ];

	*outL = mixrFadeChannelvoltab[volindexLL][cacheL]
	      + mixrFadeChannelvoltab[volindexLR][cacheR];
	*outR = mixrFadeChannelvoltab[volindexRL][cacheL]
	      + mixrFadeChannelvoltab[volindexRR][cacheR];
}

static inline void interp_i16_s(const struct channel *chan, const int volindexLL, const int volindexLR, const int volindexRL, const int volindexRR, const uint32_t pos, uint32_t fpos, int32_t * const outL, int32_t * const outR)
{
	uint8_t cacheL =
		mixrFadeChannelintrtab[ fpos>>12 ][ ((uint16_t)(chan->realsamp.bit16[(pos<<1)  ]))>>8 ][ 0 ]+
		mixrFadeChannelintrtab[ fpos>>12 ][ ((uint16_t)(chan->realsamp.bit16[(pos<<1)+2]))>>8 ][ 1 ];
	uint8_t cacheR =
		mixrFadeChannelintrtab[ fpos>>12 ][ ((uint16_t)(chan->realsamp.bit16[(pos<<1)+1]))>>8 ][ 0 ]+
		mixrFadeChannelintrtab[ fpos>>12 ][ ((uint16_t)(chan->realsamp.bit16[(pos<<1)+3]))>>8 ][ 1 ];

	*outL = mixrFadeChannelvoltab[volindexLL][cacheL]
	      + mixrFadeChannelvoltab[volindexLR][cacheR];
	*outR = mixrFadeChannelvoltab[volindexRL][cacheL]
	      + mixrFadeChannelvoltab[volindexRR][cacheR];
}

#define MIX_TEMPLATE_S(NAME, INTERP)                                    \
static void                                                             \
NAME(int32_t *buf,                                                      \
     uint32_t len,                                                      \
     struct channel *chan)                                              \
{                                                                       \
    int32_t vol00    = chan->curvols[0][0];                             \
    int32_t vol00add = ramping[0][0];                                   \
    int32_t vol01    = chan->curvols[0][1];                             \
    int32_t vol01add = ramping[0][1];                                   \
    int32_t vol10    = chan->curvols[1][0];                             \
    int32_t vol10add = ramping[1][0];                                   \
    int32_t vol11    = chan->curvols[1][1];                             \
    int32_t vol11add = ramping[1][1];                                   \
    uint32_t pos=chan->pos;                                             \
    uint32_t fpos=chan->fpos;                                           \
                                                                        \
    while (len)                                                         \
        {                                                               \
            int32_t l, r;                                               \
            interp_##INTERP##_s(chan, vol00, vol01, vol10, vol11, pos, fpos, &l, &r); \
            *(buf++)+=l;                                                \
            *(buf++)+=r;                                                \
            fpos+=chan->step&0x0000ffff;                                \
            if (fpos&0xffff0000)                                        \
            {                                                           \
                pos++;                                                  \
                fpos&=0xffff;                                           \
            }                                                           \
            pos+=(chan->step>>16);                                      \
            vol00 += vol00add;                                          \
            vol01 += vol01add;                                          \
            vol10 += vol10add;                                          \
            vol11 += vol11add;                                          \
            len--;                                                      \
        }                                                               \
}

MIX_TEMPLATE_S(playstereo_s,    none8)
MIX_TEMPLATE_S(playstereoi_s,   i8)
MIX_TEMPLATE_S(playstereo16_s,  none16)
MIX_TEMPLATE_S(playstereoi16_s, i16)

static void routequiet(int32_t *buf, uint32_t len, struct channel *chan)
{
}

typedef void (*route_func)(int32_t *buf, uint32_t len, struct channel *chan);
static const route_func routeptrs[8]=
{
	playstereo,
	playstereo16,
	playstereoi,
	playstereoi16,
	playstereo_s,
	playstereo16_s,
	playstereoi_s,
	playstereoi16_s,
};

void mixrPlayChannel(int32_t *buf, int32_t *fadebuf, uint32_t len, struct channel *chan)
{
	uint32_t fillen=0;
	int inloop;

	int dofade=0;
	int route=0;
	route_func routeptr;
	uint32_t mixlen; /* ecx */

	if (!(chan->status&MIXRQ_PLAYING))
		return;

	if (chan->status&MIXRQ_PLAYSTEREO)
		route+=4;

	if (chan->status&MIXRQ_INTERPOLATE)
		route+=2;

	if (chan->status&MIXRQ_PLAY16BIT)
		route++;

mixrPlayChannelbigloop:
	inloop=0;
	mixlen=len;

	if (chan->step)
	{
		uint32_t abs_step;       /* abs of chan->step */
		uint32_t data_left;
		uint16_t data_left_fraction;

		/* length = eax */
		if (chan->step<0)
		{ /* mixrPlayChannelbackward: */
			abs_step = -chan->step;

			data_left          = chan->pos;
			data_left_fraction = chan->fpos;
			if (chan->status&MIXRQ_LOOPED)
			{
				if (chan->pos >= chan->loopstart)
				{
					data_left -= chan->loopstart;
					inloop = 1;
				}
			}
		} else { /* mixrPlayChannelforward */
			abs_step = chan->step;
			data_left          = chan->length - chan->pos - (!!chan->fpos);
			data_left_fraction = -chan->fpos;
			if (chan->status&MIXRQ_LOOPED)
			{
				if (chan->pos < chan->loopend)
				{
					data_left -= chan->length - chan->loopend;
					inloop = 1;
				}
			}
		}
		/* here we analyze how much we can sample */
		{
			/* fprintf(stderr, "Samples available to use: %d/0x%08x\n", data_left, mypos);*/
			uint64_t tmppos=((((uint64_t)data_left)<<16)|data_left_fraction)+((uint32_t)abs_step)-1;
			/* fprintf(stderr, "Samples available to use hidef: %lld/0x%012llx\n", tmppos, tmppos);*/
			if ((tmppos>>32)<abs_step)
			{/* this is the safe check to avoid overflow in div */
				uint32_t tmplen;
				tmplen = tmppos / abs_step; /* eax */
				/* fprintf(stderr, "Samples at current playrate would be output into %d samples\n", tmplen);*/
				if (mixlen>=tmplen)
				{
					/* fprintf(stderr, "world wants more data than we can provide, limit output\n");*/
					mixlen=tmplen;
					if (!inloop)
					{ /* add padding */
						/* fprintf(stderr, "We are not in loop, configure fillen \n");*/
						dofade=1;
						chan->status&=~MIXRQ_PLAYING;
						fillen=(len-mixlen); /* the gap that is left */
						len=mixlen;
					}
				}
			}
		}
	}
	ramping[0][0] = 0;
	ramping[0][1] = 0;
	ramping[1][0] = 0;
	ramping[1][1] = 0;
	if (mixlen) /* ecx */
	{
		int32_t diff; /* edx */
		int ramploop = 0;
		if ((diff=chan->dstvols[0][0]-chan->curvols[0][0]))
		{
			if (diff>0)
			{
				ramping[0][0]=1;
				if (mixlen>diff)
				{
					ramploop+=mixlen-diff;
					mixlen=diff;
				}
			} else {
				ramping[0][0]=-1;
				diff=-diff;
				if (mixlen>diff)
				{
					ramploop+=mixlen-diff;
					mixlen=diff;
				}
			}
		}
		if ((diff=chan->dstvols[0][1]-chan->curvols[0][1]))
		{
			if (diff>0)
			{
				ramping[0][1]=1;
				if (mixlen>diff)
				{
					ramploop+=mixlen-diff;
					mixlen=diff;
				}
			} else {
				ramping[0][1]=-1;
				diff=-diff;
				if (mixlen>diff)
				{
					ramploop+=mixlen-diff;
					mixlen=diff;
				}
			}
		}
		if ((diff=chan->dstvols[1][0]-chan->curvols[1][0]))
		{
			if (diff>0)
			{
				ramping[1][0]=1;
				if (mixlen>diff)
				{
					ramploop+=mixlen-diff;
					mixlen=diff;
				}
			} else {
				ramping[1][0]=-1;
				diff=-diff;
				if (mixlen>diff)
				{
					ramploop+=mixlen-diff;
					mixlen=diff;
				}
			}
		}
		if ((diff=chan->dstvols[1][1]-chan->curvols[1][1]))
		{
			if (diff>0)
			{
				ramping[1][1]=1;
				if (mixlen>diff)
				{
					ramploop+=mixlen-diff;
					mixlen=diff;
				}
			} else {
				ramping[1][1]=-1;
				diff=-diff;
				if (mixlen>diff)
				{
					ramploop+=mixlen-diff;
					mixlen=diff;
				}
			}
		}
		routeptr=routeptrs[route];
		if (!ramping[0][0] &&
		    !ramping[0][1] &&
		    !ramping[1][0] &&
		    !ramping[1][1] &&
		    !chan->curvols[0][0] &&
		    !chan->curvols[0][1] &&
		    !chan->curvols[1][0] &&
		    !chan->curvols[1][1])
			routeptr=routequiet;
		routeptr(buf, mixlen, chan);
		buf+=mixlen << 1 /* stereo */;
		len-=mixlen;
		chan->curvols[0][0] += mixlen*ramping[0][0];
		chan->curvols[0][1] += mixlen*ramping[0][1];
		chan->curvols[1][0] += mixlen*ramping[1][0];
		chan->curvols[1][1] += mixlen*ramping[1][1];
		{
			int64_t tmp64=((int64_t)chan->step)*mixlen + ((uint16_t)chan->fpos);
			chan->fpos=tmp64&0xffff;
			chan->pos+=(tmp64>>16);
		}

		if (ramploop)
		{
			goto mixrPlayChannelbigloop;
		}
	}

	/* if we were in a loop, check if we hit the boundary, if so loop and mix more data if needed*/
	if (inloop)
	{
		int32_t mypos = chan->pos;
		if (chan->step<0)
		{
			if (mypos>=(int32_t)chan->loopstart)
				return;
			if (!(chan->status&MIXRQ_PINGPONGLOOP))
			{
				mypos+=chan->replen;
			} else {
				chan->step=-chan->step;
				mypos+=!!(chan->fpos);
				chan->fpos=-chan->fpos;
				mypos=-mypos+chan->loopstart+chan->loopstart;
			}
		} else {
			if (mypos<chan->loopend)
				return;
			if (!(chan->status&MIXRQ_PINGPONGLOOP))
			{
				mypos-=chan->replen;
			} else {
				chan->step=-chan->step;
				mypos+=!!(chan->fpos);
				chan->fpos=-chan->fpos;
				mypos=-mypos+chan->loopend+chan->loopend;
			}
		}
		chan->pos=mypos;
		if (len)
			goto mixrPlayChannelbigloop;
		return;
	}

	if (fillen)
	{
		uint32_t curvols[2][2];
		chan->pos=chan->length;
		if (chan->status&MIXRQ_PLAYSTEREO)
		{
			if (chan->status&MIXRQ_PLAY16BIT)
			{
				curvols[0][0]=mixrFadeChannelvoltab[chan->curvols[0][0]][((uint16_t)(chan->realsamp.bit16[(chan->pos<<1)    ]))>>8];
				curvols[0][1]=mixrFadeChannelvoltab[chan->curvols[0][1]][((uint16_t)(chan->realsamp.bit16[(chan->pos<<1) + 1]))>>8];
				curvols[1][0]=mixrFadeChannelvoltab[chan->curvols[1][0]][((uint16_t)(chan->realsamp.bit16[(chan->pos<<1)    ]))>>8];
				curvols[1][1]=mixrFadeChannelvoltab[chan->curvols[1][1]][((uint16_t)(chan->realsamp.bit16[(chan->pos<<1) + 1]))>>8];
			} else {
				curvols[0][0]=mixrFadeChannelvoltab[chan->curvols[0][0]][(uint8_t)(chan->realsamp.bit8[(chan->pos<<1)    ])];
				curvols[0][1]=mixrFadeChannelvoltab[chan->curvols[0][1]][(uint8_t)(chan->realsamp.bit8[(chan->pos<<1) + 1])];
				curvols[1][0]=mixrFadeChannelvoltab[chan->curvols[1][0]][(uint8_t)(chan->realsamp.bit8[(chan->pos<<1)    ])];
				curvols[1][1]=mixrFadeChannelvoltab[chan->curvols[1][1]][(uint8_t)(chan->realsamp.bit8[(chan->pos<<1) + 1])];
			}
		} else {
			if (chan->status&MIXRQ_PLAY16BIT)
			{
				curvols[0][0]=mixrFadeChannelvoltab[chan->curvols[0][0]][((uint16_t)(chan->realsamp.bit16[chan->pos]))>>8];
				curvols[0][1]=mixrFadeChannelvoltab[chan->curvols[0][1]][((uint16_t)(chan->realsamp.bit16[chan->pos]))>>8];
				curvols[1][0]=mixrFadeChannelvoltab[chan->curvols[1][0]][((uint16_t)(chan->realsamp.bit16[chan->pos]))>>8];
				curvols[1][1]=mixrFadeChannelvoltab[chan->curvols[1][1]][((uint16_t)(chan->realsamp.bit16[chan->pos]))>>8];
			} else {
				curvols[0][0]=mixrFadeChannelvoltab[chan->curvols[0][0]][(uint8_t)(chan->realsamp.bit8[chan->pos])];
				curvols[0][1]=mixrFadeChannelvoltab[chan->curvols[0][1]][(uint8_t)(chan->realsamp.bit8[chan->pos])];
				curvols[1][0]=mixrFadeChannelvoltab[chan->curvols[1][0]][(uint8_t)(chan->realsamp.bit8[chan->pos])];
				curvols[1][1]=mixrFadeChannelvoltab[chan->curvols[1][1]][(uint8_t)(chan->realsamp.bit8[chan->pos])];
			}
		}
		while (fillen)
		{
			*(buf++) += curvols[0][0] + curvols[0][1];
			*(buf++) += curvols[1][0] + curvols[1][1];
			fillen--;
		}
	} else {
		if (!dofade)
			return;
	}
	mixrFadeChannel(fadebuf, chan);
}

void mixrFade(int32_t *buf, int32_t *fade, int len)
{
	int32_t samp0 = fade[0];
	int32_t samp1 = fade[1];

	do
	{
		if (samp0 || samp1)
		{
		}
		*(buf++)=samp0;
		*(buf++)=samp1;
		samp0 = ((samp0<<7) - samp0)>>7;
		samp1 = ((samp1<<7) - samp1)>>7;
	} while (--len);

	fade[0]=samp0;
	fade[1]=samp1;
}

void mixrClip(void *dst, int32_t *src, int len, void *tab, int32_t max)
{
	const uint16_t (*amptab)[256] = tab;
	const uint16_t *mixrClipamp1 = amptab[0];
	const uint16_t *mixrClipamp2 = amptab[1];
	const uint16_t *mixrClipamp3 = amptab[2];
	const int32_t mixrClipmax=max;
	const int32_t mixrClipmin=-max;
	const uint16_t mixrClipminv =
		mixrClipamp1[mixrClipmin&0xff]+
		mixrClipamp2[(mixrClipmin&0xff00)>>8]+
		mixrClipamp3[(mixrClipmin&0xff0000)>>16];
	const uint16_t mixrClipmaxv =
		mixrClipamp1[mixrClipmax&0xff]+
		mixrClipamp2[(mixrClipmax&0xff00)>>8]+
		mixrClipamp3[(mixrClipmax&0xff0000)>>16];

	uint16_t *_dst=dst;
	while (len)
	{
		if (*src<mixrClipmin)
		{
			*_dst=mixrClipminv;
		} else if (*src>mixrClipmax)
		{
			*_dst=mixrClipmaxv;
		} else {
			*_dst=
				mixrClipamp1[*src&0xff]+
				mixrClipamp2[(*src&0xff00)>>8]+
				mixrClipamp3[(*src&0xff0000)>>16];
		}
		src++;
		_dst++;
		len--;
	}
}
