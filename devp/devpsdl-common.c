extern struct sounddevice plrSDL;

static void *devpSDLBuffer;
static struct ringbuffer_t *devpSDLRingBuffer;
static uint32_t devpSDLRate;
static int devpSDLPauseSamples;
static int devpSDLInPause;

#if SDL_VERSION_ATLEAST(2,0,18)
volatile static uint64_t lastCallbackTime;
#else
volatile static uint32_t lastCallbackTime;
#endif
static volatile unsigned int lastLength;

void theRenderProc(void *userdata, Uint8 *stream, int len)
{
	int pos1, length1, pos2, length2;

	PRINT("%s(,,%d)\n", __FUNCTION__, len);

	SDL_LockAudio();

#if SDL_VERSION_ATLEAST(2,0,18)
	lastCallbackTime = SDL_GetTicks64();
#else
	lastCallbackTime = SDL_GetTicks();
#endif

	ringbuffer_get_tail_samples (devpSDLRingBuffer, &pos1, &length1, &pos2, &length2);
	ringbuffer_tail_consume_samples (devpSDLRingBuffer, length1 + length2);

	if (devpSDLPauseSamples)
	{
		if ((length1 + length2) > devpSDLPauseSamples)
		{
			devpSDLPauseSamples = 0;
		} else {
			devpSDLPauseSamples -= (length1 + length2);
		}
	}

	ringbuffer_get_processing_bytes (devpSDLRingBuffer, &pos1, &length1, &pos2, &length2);

	if (length1 > len)
	{
		length1 = len;
	}
	memcpy (stream, (uint8_t *)devpSDLBuffer + pos1, length1);
	ringbuffer_processing_consume_bytes (devpSDLRingBuffer, length1);
	len -= length1;
	stream += length1;
	lastLength = length1 >> 2 /* stereo + bit16 */;
	
	if (len && length2)
	{
		if (length2 > len)
		{
			length2 = len;
		}
		memcpy (stream, (uint8_t *)devpSDLBuffer + pos2, length2);
		ringbuffer_processing_consume_bytes (devpSDLRingBuffer, length2);
		len -= length2;
		stream += length2;
		lastLength += length2 >> 2 /* stereo + bit16 */;
	}

	SDL_UnlockAudio();

	if (len)
	{
		bzero (stream, len);
		PRINT("%s: buffer overrun - %d left\n", __FUNCTION__, len);
	}
}

static unsigned int devpSDLIdle (void)
{
	int pos1, length1, pos2, length2;
	unsigned int RetVal;

	PRINT("%s()\n", __FUNCTION__);

	SDL_LockAudio();

/* START: this magic updates the tail by time, causing events in the ringbuffer to be fired if needed and audio-visuals to be more responsive */
	{
#if SDL_VERSION_ATLEAST(2,0,18)
		uint64_t curTime;
#else
		uint32_t curTime;
#endif
		signed int expect_consume;
		signed int expect_left;
		signed int consume;

		ringbuffer_get_tail_samples (devpSDLRingBuffer, &pos1, &length1, &pos2, &length2);

#if SDL_VERSION_ATLEAST(2,0,18)
		curTime = SDL_GetTicks64();
#else
		curTime = SDL_GetTicks();
#endif
		expect_consume = devpSDLRate * (curTime - lastCallbackTime) / 1000;
		expect_left = (signed int)lastLength - expect_consume;
		if (expect_left < 0)
		{
			expect_left = 0;
		}
		consume = (signed int)(length1 + length2) - expect_left;
		if (consume > 0)
		{
			ringbuffer_tail_consume_samples (devpSDLRingBuffer, consume);
		}
	}
/* STOP */

	ringbuffer_get_tailandprocessing_samples (devpSDLRingBuffer, &pos1, &length1, &pos2, &length2);

	/* do we need to insert pause-samples? */
	if (devpSDLInPause)
	{
		int pos1, length1, pos2, length2;
		ringbuffer_get_head_bytes (devpSDLRingBuffer, &pos1, &length1, &pos2, &length2);
		bzero ((char *)devpSDLBuffer+pos1, length1);
		if (length2)
		{
			bzero ((char *)devpSDLBuffer+pos2, length2);
		}
		ringbuffer_head_add_bytes (devpSDLRingBuffer, length1 + length2);
		devpSDLPauseSamples += (length1 + length2) >> 2; /* stereo + 16bit */
	}

	SDL_UnlockAudio();

	RetVal = length1 + length2;

	if (devpSDLPauseSamples >= RetVal)
	{
		return 0;
	}
	return RetVal - devpSDLPauseSamples;
}

static void devpSDLPeekBuffer (void **buf1, unsigned int *buf1length, void **buf2, unsigned int *buf2length)
{
	int pos1, length1, pos2, length2;

	devpSDLIdle (); /* update the tail */

	SDL_LockAudio();
	ringbuffer_get_tailandprocessing_samples (devpSDLRingBuffer, &pos1, &length1, &pos2, &length2);
	SDL_UnlockAudio();

	if (length1)
	{
		*buf1 = (char *)devpSDLBuffer + (pos1 << 2); /* stereo + 16bit */
		*buf1length = length1;
		if (length2)
		{
			*buf2 = (char *)devpSDLBuffer + (pos2 << 2); /* stereo + 16bit */
			*buf2length = length2;
		} else {
			*buf2 = 0;
			*buf2length = 0;
		}
	} else {
		*buf1 = 0;
		*buf1length = 0;
		*buf2 = 0;
		*buf2length = 0;
	}
}

static int devpSDLPlay (uint32_t *rate, enum plrRequestFormat *format, struct ocpfilehandle_t *source_file, struct cpifaceSessionAPI_t *cpifaceSession)
{
	SDL_AudioSpec desired, obtained;
	int status;
	int plrbufsize; /* given in ms */
	int buflength;

	PRINT("%s(*%d,*%d)\n", __FUNCTION__, *rate, *format);

	devpSDLInPause = 0;
	devpSDLPauseSamples = 0;

	*format = PLR_STEREO_16BIT_SIGNED; /* fixed fixed fixed */

	if (!*rate)
	{
		*rate = 44100;
	}
	if (*rate < 22050)
	{
		*rate = 22050;
	}
	if (*rate > 96000)
	{
		*rate = 96000;
	}

	SDL_memset (&desired, 0, sizeof (desired));
	desired.freq = *rate;
	desired.format = AUDIO_S16SYS;
	desired.channels = 2;
	desired.samples = *rate / 8; /* 125 ms */
	desired.callback = theRenderProc;
	desired.userdata = NULL;

#if SDL_VERSION_ATLEAST(2,0,18)
	lastCallbackTime = SDL_GetTicks64();
#else
	lastCallbackTime = SDL_GetTicks();
#endif
	lastLength = 0;

	status=SDL_OpenAudio(&desired, &obtained);
	if (status < 0)
	{
		fprintf (stderr, "[SDL] SDL_OpenAudio returned %d (%s)\n", (int)status, SDL_GetError());
		free (devpSDLBuffer); devpSDLBuffer = 0;
		ringbuffer_free (devpSDLRingBuffer); devpSDLRingBuffer = 0;
		return 0;
	}
	devpSDLRate = *rate = obtained.freq;

	plrbufsize = cfGetProfileInt2(cfSoundSec, "sound", "plrbufsize", 200, 10);
	/* clamp the plrbufsize to be atleast 150ms and below 1000 ms */
	if (plrbufsize < 150)
	{
		plrbufsize = 150;
	}
	if (plrbufsize > 1000)
	{
		plrbufsize = 1000;
	}
	buflength = devpSDLRate * plrbufsize / 1000;

	if (buflength < obtained.samples * 2)
	{
		buflength = obtained.samples * 2;
	}
	if (!(devpSDLBuffer=calloc (buflength, 4)))
	{
		SDL_CloseAudio();
		return 0;
	}

	if (!(devpSDLRingBuffer = ringbuffer_new_samples (RINGBUFFER_FLAGS_STEREO | RINGBUFFER_FLAGS_16BIT | RINGBUFFER_FLAGS_SIGNED | RINGBUFFER_FLAGS_PROCESS, buflength)))
	{
		SDL_CloseAudio();
		free (devpSDLBuffer); devpSDLBuffer = 0;
		return 0;
	}

	cpifaceSession->GetMasterSample = plrGetMasterSample;
	cpifaceSession->GetRealMasterVolume = plrGetRealMasterVolume;
	cpifaceSession->plrActive = 1;

#warning This needs to delay until we have received the first commit
	SDL_PauseAudio(0);
	return 1;
}

static void devpSDLGetBuffer (void **buf, unsigned int *samples)
{
	int pos1, length1;
	assert (devpSDLRingBuffer);

	PRINT("%s()\n", __FUNCTION__);

	SDL_LockAudio();

	ringbuffer_get_head_samples (devpSDLRingBuffer, &pos1, &length1, 0, 0);

	SDL_UnlockAudio();

	*samples = length1;
	*buf = devpSDLBuffer + (pos1<<2); /* stereo + bit16 */
}

static uint32_t devpSDLGetRate (void)
{
	return devpSDLRate;
}

static void devpSDLOnBufferCallback (int samplesuntil, void (*callback)(void *arg, int samples_ago), void *arg)
{
	assert (devpSDLRingBuffer);
	ringbuffer_add_tail_callback_samples (devpSDLRingBuffer, samplesuntil, callback, arg);
}

static void devpSDLCommitBuffer (unsigned int samples)
{
	PRINT("%s(%u)\n", __FUNCTION__, samples);
	SDL_LockAudio();

	ringbuffer_head_add_samples (devpSDLRingBuffer, samples);

	SDL_UnlockAudio();
}

static void devpSDLPause (int pause)
{
	assert (devpSDLBuffer);
	devpSDLInPause = pause;
}

static void devpSDLStop (struct cpifaceSessionAPI_t *cpifaceSession)
{
	PRINT("%s()\n", __FUNCTION__);
	/* TODO, forceflush */

	SDL_PauseAudio(1);

	SDL_CloseAudio();

	free(devpSDLBuffer); devpSDLBuffer=0;
	if (devpSDLRingBuffer)
	{
		ringbuffer_reset (devpSDLRingBuffer);
		ringbuffer_free (devpSDLRingBuffer);
		devpSDLRingBuffer = 0;
	}

	cpifaceSession->plrActive = 0;
}

static const struct plrDevAPI_t devpSDL = {
	devpSDLIdle,
	devpSDLPeekBuffer,
	devpSDLPlay,
	devpSDLGetBuffer,
	devpSDLGetRate,
	devpSDLOnBufferCallback,
	devpSDLCommitBuffer,
	devpSDLPause,
	devpSDLStop,
	0, /* VolRegs */
	0 /* ProcessKey */
};

static void sdlClose(void)
{
	PRINT("%s()\n", __FUNCTION__);
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
	if (plrDevAPI  == &devpSDL)
	{
		plrDevAPI = 0;
	}
}

static int sdlDetect(struct deviceinfo *card)
{
	PRINT("%s()\n", __FUNCTION__);

	card->devtype=&plrSDL;
	card->port=0;
	card->port2=0;
	card->subtype=-1;
	card->mem=0;
	card->chan=2;

	return 1;
}
