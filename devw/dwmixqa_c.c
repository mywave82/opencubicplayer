static int16_t (*myvoltab)[2][256];
static int16_t (*myinterpoltabq)[32][256][2];
static int16_t (*myinterpoltabq2)[16][256][4];

void mixqSetupAddresses(int16_t (*voltab)[2][256], int16_t (*interpoltabq)[32][256][2], int16_t (*interpoltabq2)[16][256][4])
{
	myvoltab=voltab;
	myinterpoltabq=interpoltabq;
	myinterpoltabq2=interpoltabq2;
}

static void playquiet(int16_t *buf, uint32_t len, struct channel *chan)
{
}

static inline int32_t interp_none8(const struct channel *chan, const uint32_t pos, uint32_t fpos)
{
	return chan->realsamp.bit8[pos]<<8;
}

static inline int32_t interp_none16(const struct channel *chan, const uint32_t pos, uint32_t fpos)
{
	return chan->realsamp.bit16[pos];
}

static inline int32_t interp_i8(const struct channel *chan, const uint32_t pos, uint32_t fpos)
{
	uint8_t cache = fpos>>11;
	return myinterpoltabq[0][cache][(uint8_t)chan->realsamp.bit8[pos]][0]
	        +
	       myinterpoltabq[0][cache][(uint8_t)chan->realsamp.bit8[pos+1]][1];
}

static inline int32_t interp_i16(const struct channel *chan, const uint32_t pos, uint32_t fpos)
{
	uint8_t cache = fpos>>11;

	return myinterpoltabq[0][cache][(uint8_t)(chan->realsamp.bit16[pos]>>8)][0]
	        +
	       myinterpoltabq[0][cache][(uint8_t)(chan->realsamp.bit16[pos+1]>>8)][1]
	        +
	       myinterpoltabq[1][cache][(uint8_t)(chan->realsamp.bit16[pos]&0xff)][0]
	        +
	       myinterpoltabq[1][cache][(uint8_t)(chan->realsamp.bit16[pos+1]&0xff)][1];
}

static inline int32_t interp_i28(const struct channel *chan, const uint32_t pos, uint32_t fpos)
{
	uint8_t cache = fpos>>12;

	return myinterpoltabq2[0][cache][(uint8_t)(chan->realsamp.bit8[pos])][0]
	        +
	        myinterpoltabq2[0][cache][(uint8_t)(chan->realsamp.bit8[pos+1])][1]
	        +
	        myinterpoltabq2[0][cache][(uint8_t)(chan->realsamp.bit8[pos+2])][2];
}

static inline int32_t interp_i216(const struct channel *chan, const uint32_t pos, uint32_t fpos)
{
	uint8_t cache = fpos>>12;

	return myinterpoltabq2[0][cache][(uint8_t)(chan->realsamp.bit16[pos]>>8)][0]
	        +
	       myinterpoltabq2[0][cache][(uint8_t)(chan->realsamp.bit16[pos+1]>>8)][1]
	        +
	       myinterpoltabq2[0][cache][(uint8_t)(chan->realsamp.bit16[pos+2]>>8)][2]
	        +
	       myinterpoltabq2[1][cache][(uint8_t)(chan->realsamp.bit16[pos]&0xff)][0]
	        +
	       myinterpoltabq2[1][cache][(uint8_t)(chan->realsamp.bit16[pos+1]&0xff)][1]
	       +
	       myinterpoltabq2[1][cache][(uint8_t)(chan->realsamp.bit16[pos+2]&0xff)][2];
}

#define MIX_TEMPLATE(NAME, INTERP)                                      \
static void                                                             \
NAME(int16_t *buf,                                                      \
     uint32_t len,                                                      \
     struct channel *chan)                                              \
{                                                                       \
    uint32_t pos=chan->pos;                                             \
    uint32_t fpos=chan->fpos;                                           \
    uint32_t fadd=chan->step&0xffff;                                    \
    uint32_t posadd=(int16_t)(chan->step>>16);                          \
                                                                        \
    while (len)                                                         \
        {                                                               \
            *(buf++) = interp_##INTERP(chan, pos, fpos);                \
            fpos+=fadd;                                                 \
            if (fpos&0xffff0000)                                        \
            {                                                           \
                pos++;                                                  \
                fpos&=0xffff;                                           \
            }                                                           \
            pos+=posadd;                                                \
            len--;                                                      \
        }                                                               \
}

//#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused"
MIX_TEMPLATE(playmono, none8)
MIX_TEMPLATE(playmono16, none16)
MIX_TEMPLATE(playmonoi, i8)
MIX_TEMPLATE(playmonoi16, i16)
MIX_TEMPLATE(playmonoi2, i28)
MIX_TEMPLATE(playmonoi216, i216)
//#pragma GCC diagnostic pop

void mixqPlayChannel(int16_t *buf, uint32_t len, struct channel *chan, int quiet)
{
	uint32_t fillen=0;
	uint32_t mixlen;
	int inloop;

	void (*playrout)(int16_t *buf, uint32_t len, struct channel *chan);

	if (quiet)
	{
		playrout=playquiet;
	} else {
		if (chan->status&MIXQ_INTERPOLATE)
		{
			if (chan->status&MIXQ_INTERPOLATEMAX)
			{
				if (chan->status&MIXQ_PLAY16BIT)
				{
					playrout=playmonoi216;
				} else {
					playrout=playmonoi2;
				}
			} else {
				if (chan->status&MIXQ_PLAY16BIT)
				{
					playrout=playmonoi16;
				} else {
					playrout=playmonoi;
				}
			}
		} else {
			if (chan->status&MIXQ_PLAY16BIT)
			{
				playrout=playmono16;
			} else {
				playrout=playmono;
			}
		}
	}

mixqPlayChannel_bigloop:
	inloop=0;
	mixlen=len;

	if (chan->step)
	{
		uint32_t abs_step;       /* abs of chan->step */
		uint32_t data_left;
		uint16_t data_left_fraction;

		if (chan->step<0)
		{
			abs_step=-chan->step;
			data_left          = chan->pos;
			data_left_fraction = chan->fpos;
			if (chan->status&MIXQ_LOOPED)
			{
				if (chan->pos >= chan->loopstart)
				{
					data_left -= chan->loopstart;
					inloop = 1;
				}
			}
		} else {
			abs_step=chan->step;
			data_left = chan->length - chan->pos - (!chan->fpos);
			data_left_fraction = -chan->fpos;
			if (chan->status&MIXQ_LOOPED)
			{
				if (chan->pos < chan->loopend)
				{
					data_left -= chan->length - chan->loopend;
					inloop = 1;
				}
			}
		}
		/* data_left should now be the end of the loop envelope */
		{
			/* fprintf(stderr, "Samples available to use: %d/0x%08x\n", data_left, data_left_fraction); */
			uint64_t tmppos=((((uint64_t)data_left)<<16)|data_left_fraction)+((uint32_t)abs_step)-1;
			/* fprintf(stderr, "Samples available to use hidef: %lld/0x%012llx\n", tmppos, tmppos);*/
			if ((tmppos>>32)<abs_step)
			{/* this is the safe check to avoid overflow in div */
				uint32_t tmplen;
				tmplen=tmppos/abs_step;
				/* fprintf(stderr, "Samples at current playrate would be output into %d samples\n", tmplen); */
				if (mixlen>=tmplen)
				{
					/* fprintf(stderr, "world wants more data than we can provide, limit output\n");*/
					mixlen=tmplen;
					if (!inloop)
					{
						/* fprintf(stderr, "We are not in loop, configure fillen\n");*/
						chan->status&=~MIXQ_PLAYING;
						fillen=(len-tmplen); /* the gap that is left */
						len=mixlen;
					}
				}
			}
		}
	}
	playrout(buf, mixlen, chan);
	buf+=mixlen;
	len-=mixlen;
	{
		int64_t tmp64=((int64_t)chan->step)*mixlen + (uint16_t)chan->fpos;
		chan->fpos=tmp64&0xffff;
		chan->pos+=(tmp64>>16);
	}

	if (inloop)
	{
		int32_t mypos = chan->pos; /* eax */
		if (chan->step<0)
		{
			if (mypos>=(int32_t)chan->loopstart)
				return;
			if (!(chan->status&MIXQ_PINGPONGLOOP))
			{
				mypos+=chan->replen;
			} else {
				chan->step=-chan->step;
				if ((chan->fpos=-chan->fpos))
					mypos++;
				mypos=chan->loopstart+chan->loopstart-mypos;
			}
		} else {
			if (mypos<chan->loopend)
				return;
			if (!(chan->status&MIXQ_PINGPONGLOOP))
			{
				mypos-=chan->replen;
			} else {
				chan->step=-chan->step;
				if ((chan->fpos=-chan->fpos))
					mypos++;
				mypos=chan->loopend+chan->loopend-mypos;
			}
		}
		chan->pos=mypos;
		if (len)
			goto mixqPlayChannel_bigloop;
	}
	if (fillen)
	{
		int16_t sample;
		int count;
		chan->pos=chan->length;
		if (!(chan->status&MIXQ_PLAY16BIT))
		{
			sample=chan->realsamp.bit8[chan->pos]<<8;
		} else {
			sample=chan->realsamp.bit16[chan->pos];
		}
		for (count=0;count<fillen;count++)
			*(buf++)=sample;
	}
}

void mixqAmplifyChannel(int32_t *buf, int16_t *src, uint32_t len, int32_t vol, uint32_t step)
{
	while (len)
	{
		(*buf) += myvoltab[vol][0][(uint8_t)((*src)>>8)] + myvoltab[vol][1][(uint8_t)((*src)&0xff)];
		src++;
		len--;
		buf+=step/sizeof(uint32_t);
	}
}

void mixqAmplifyChannelUp(int32_t *buf, int16_t *src, uint32_t len, int32_t vol, uint32_t step)
{
	while (len)
	{
		(*buf) += myvoltab[vol][0][(uint8_t)((*src)>>8)] + myvoltab[vol][1][(uint8_t)((*src)&0xff)];
		src++;
		vol++;
		len--;
		buf+=step/sizeof(uint32_t);
	}
}

void mixqAmplifyChannelDown(int32_t *buf, int16_t *src, uint32_t len, int32_t vol, uint32_t step)
{
	while (len)
	{
		(*buf) += myvoltab[vol][0][(uint8_t)((*src)>>8)] + myvoltab[vol][1][(uint8_t)((*src)&0xff)];
		src++;
		vol--;
		len--;
		buf+=step/sizeof(uint32_t);
	}
}

