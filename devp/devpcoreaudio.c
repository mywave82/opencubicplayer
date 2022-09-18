/** OpenCP Module Player
 * copyright (c) 2006-'21 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * CoreAudio (Darwin/Mac OS/OSX) Player device
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

#include "config.h"
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <CoreAudio/CoreAudio.h>
#include <CoreServices/CoreServices.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>
#include "types.h"
#include "boot/plinkman.h"
#include "boot/psetting.h"
#include "cpiface/cpiface.h"
#include "dev/imsdev.h"
#include "dev/player.h"
#include "dev/ringbuffer.h"
#include "stuff/imsrtns.h"

#ifdef COREAUDIO_DEBUG
# define debug_printf(...) fprintf(stderr, __VA_ARGS__)
#else
# define debug_printf(...) do {} while(0)
#endif

static AudioUnit theOutputUnit;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutexattr_t mta;
struct sounddevice plrCoreAudio;
static int needfini=0;

static void *devpCoreAudioBuffer;
static struct ringbuffer_t *devpCoreAudioRingBuffer;
static uint32_t devpCoreAudioRate;
static int devpCoreAudioPauseSamples;
static int devpCoreAudioInPause;

volatile static uint32_t lastCallbackTime;
static volatile unsigned int lastLength;


static const char *OSStatus_to_string(OSStatus status)
{
	switch (status)
	{
		case kAudioHardwareNoError:
			return "kAudioHardwareNoError";
		case kAudioHardwareNotRunningError:
			return "kAudioHardwareNotRunningError";
		case kAudioHardwareUnspecifiedError:
			return "kAudioHardwareUnspecifiedError";
		case kAudioHardwareUnknownPropertyError:
			return "kAudioHardwareUnknownPropertyError";
		case kAudioHardwareBadPropertySizeError:
			return "kAudioHardwareBadPropertySizeError";
		case kAudioHardwareIllegalOperationError:
			return "kAudioHardwareIllegalOperationError";
		case kAudioHardwareBadDeviceError:
			return "kAudioHardwareBadDeviceError";
		case kAudioHardwareBadStreamError:
			return "kAudioHardwareBadStreamError";
		case kAudioHardwareUnsupportedOperationError:
			return "kAudioHardwareUnsupportedOperationError";
		case kAudioDeviceUnsupportedFormatError:
			return "kAudioDeviceUnsupportedFormatError";
		case kAudioDevicePermissionsError:
			return "kAudioDevicePermissionsError";
		case kAudioHardwareBadObjectError:
			return "kAudioHardwareBadObjectError";
		case (OSErr)badComponentInstance:
			return "badComponentInstance";
		case (OSErr)badComponentSelector:
			return "badComponentSelector";
		default:
			return "unknown";
	}
}

#ifdef COREAUDIO_DEBUG

static void print_format(const char* str,AudioStreamBasicDescription *f){
    uint32_t flags=(uint32_t) f->mFormatFlags;
    fprintf(stderr, "%s %7.1fHz %dbit [%c%c%c%c] %s %s %s%s%s%s\n",
            str, f->mSampleRate, (int)f->mBitsPerChannel,
            (int)(f->mFormatID & 0xff000000) >> 24,
            (int)(f->mFormatID & 0x00ff0000) >> 16,
            (int)(f->mFormatID & 0x0000ff00) >>  8,
            (int)(f->mFormatID & 0x000000ff) >>  0,
            (flags&kAudioFormatFlagIsFloat) ? "float" : "int",
            (flags&kAudioFormatFlagIsBigEndian) ? "BE" : "LE",
            (flags&kAudioFormatFlagIsSignedInteger) ? "S" : "U",
            (flags&kAudioFormatFlagIsPacked) ? " packed" : " unpacked",
            (flags&kAudioFormatFlagIsAlignedHigh) ? " aligned" : " unaligned",
            (flags&kAudioFormatFlagIsNonInterleaved) ? " non-interleaved" : " interleaved" );

    fprintf(stderr, "%5d mBytesPerPacket\n",
            (int)f->mBytesPerPacket);
    fprintf(stderr, "%5d mFramesPerPacket\n",
            (int)f->mFramesPerPacket);
    fprintf(stderr, "%5d mBytesPerFrame\n",
            (int)f->mBytesPerFrame);
    fprintf(stderr, "%5d mChannelsPerFrame\n",
            (int)f->mChannelsPerFrame);

}
#endif

#if 0
static void ostype2string (OSType inType, char *outString)
{
   unsigned char   theChars[4];
   unsigned char *theString = (unsigned char *)outString;
   unsigned char *theCharPtr = theChars;
   int                       i;

   // extract four characters in big-endian order
   theChars[0] = 0xff & (inType >> 24);
   theChars[1] = 0xff & (inType >> 16);
   theChars[2] = 0xff & (inType >> 8);
   theChars[3] = 0xff & (inType);

   for (i = 0; i < 4; i++) {
      if((*theCharPtr >= ' ') && (*theCharPtr <= 126)) {
         *theString++ = *theCharPtr;
      } else {
         *theString++ = ' ';
      }

      theCharPtr++;
   }

   *theString = 0;
}
#endif

static OSStatus theRenderProc(void *inRefCon, AudioUnitRenderActionFlags *inActionFlags, const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber, UInt32 inNumFrames, AudioBufferList *ioData)
{
	int pos1, length1, pos2, length2;
	int len = ioData->mBuffers[0].mDataByteSize;
	uint8_t *stream = ioData->mBuffers[0].mData;

	debug_printf ("theRenderProc: ENTER\n");

	pthread_mutex_lock(&mutex);

	{
		struct timespec tp;
		clock_gettime (CLOCK_MONOTONIC, &tp);
		lastCallbackTime = tp.tv_sec * 1000000;
		lastCallbackTime += tp.tv_nsec / 1000;
	}

	ringbuffer_get_tail_samples (devpCoreAudioRingBuffer, &pos1, &length1, &pos2, &length2);
	ringbuffer_tail_consume_samples (devpCoreAudioRingBuffer, length1 + length2);

	debug_printf ("theRenderProc: consumed %d + %d (paused=%d)\n", length1, length2, devpCoreAudioPauseSamples);

	if (devpCoreAudioPauseSamples)
	{
		if ((length1 + length2) > devpCoreAudioPauseSamples)
		{
			devpCoreAudioPauseSamples = 0;
		} else {
			devpCoreAudioPauseSamples -= (length1 + length2);
		}
	}

	ringbuffer_get_processing_bytes (devpCoreAudioRingBuffer, &pos1, &length1, &pos2, &length2);

	debug_printf ("theRenderProc: processing available %d + %d, len %d => %d\n", length1, length2, len, (length1 > len) ? len : length1);

	if (length1 > len)
	{
		length1 = len;
	}

	memcpy(stream, (uint8_t *)devpCoreAudioBuffer + pos1, length1);
	ringbuffer_processing_consume_bytes (devpCoreAudioRingBuffer, length1);
	len -= length1;
	stream += length1;
	lastLength = length1 >> 2 /* stereo + bit16 */;

	if (len && length2)
	{
		if (length2 > len)
		{
			length2 = len;
		}
		memcpy (stream, (uint8_t *)devpCoreAudioBuffer + pos2, length2);
		ringbuffer_processing_consume_bytes (devpCoreAudioRingBuffer, length2);
		len -= length2;
		stream += length2;
		lastLength += length2 >> 2 /* stereo + bit16 */;
	}

	pthread_mutex_unlock(&mutex);

	debug_printf ("theRenderProc: EXIT\n");

	if (len)
	{
		bzero (stream, len);
		debug_printf ("theRenderProc: buffer overrun - %d left\n", len);
	}

	return noErr;
}

static void devpCoreAudioGetBuffer (void **buf, unsigned int *samples)
{
	int pos1, length1, pos2, length2;
	assert (devpCoreAudioRingBuffer);

	debug_printf ("devpCoreAudioGetBuffer: ENTER\n");

	pthread_mutex_lock(&mutex);

	ringbuffer_get_head_samples (devpCoreAudioRingBuffer, &pos1, &length1, &pos2, &length2);

	debug_printf ("devpCoreAudioGetBuffer: %d + %d\n", length1, length2);

	pthread_mutex_unlock(&mutex);

	debug_printf ("devpCoreAudioGetBuffer: EXIT\n");

	*samples = length1;
	*buf = devpCoreAudioBuffer + (pos1<<2); /* stereo + bit16 */
}

static unsigned int devpCoreAudioIdle(void)
{
	int pos1, length1, pos2, length2;
	unsigned int RetVal;

	debug_printf ("devpCoreAudioIdle: ENTER\n");

	pthread_mutex_lock(&mutex);

/* START: this magic updates the tail by time, causing events in the ringbuffer to be fired if needed and audio-visuals to be more responsive */
	{
		struct timespec tp;
		uint32_t curTime;
		signed int expect_consume;
		signed int expect_left;
		signed int consume;

		ringbuffer_get_tail_samples (devpCoreAudioRingBuffer, &pos1, &length1, &pos2, &length2);

		clock_gettime (CLOCK_MONOTONIC, &tp);
		curTime = tp.tv_sec * 1000000;
		curTime += tp.tv_nsec / 1000;

		expect_consume = devpCoreAudioRate * (curTime - lastCallbackTime) / 1000;
		expect_left = (signed int)lastLength - expect_consume;
		if (expect_left < 0)
		{
			expect_left = 0;
		}
		consume = (signed int)(length1 + length2) - expect_left;
		if (consume > 0)
		{
			ringbuffer_tail_consume_samples (devpCoreAudioRingBuffer, consume);
			debug_printf ("devpCoreAudioIdle: time %" PRIu32" - %" PRIu32" => preconsume %d\n", lastCallbackTime, curTime, consume);
		}
	}
/* STOP */

	ringbuffer_get_tailandprocessing_samples (devpCoreAudioRingBuffer, &pos1, &length1, &pos2, &length2);

	/* do we need to insert pause-samples? */
	if (devpCoreAudioInPause)
	{
		int pos1, length1, pos2, length2;
		ringbuffer_get_head_bytes (devpCoreAudioRingBuffer, &pos1, &length1, &pos2, &length2);
		bzero ((char *)devpCoreAudioBuffer+pos1, length1);
		if (length2)
		{
			bzero ((char *)devpCoreAudioBuffer+pos2, length2);
		}
		ringbuffer_head_add_bytes (devpCoreAudioRingBuffer, length1 + length2);
		devpCoreAudioPauseSamples += (length1 + length2) >> 2; /* stereo + 16bit */
	}


	debug_printf ("devpCoreAudioIdle: current delay: %d + %d\n", length1, length2);

	pthread_mutex_unlock(&mutex);

	RetVal = length1 + length2;

	debug_printf ("devpCoreAudioIdle: EXIT\n");

	if (devpCoreAudioPauseSamples >= RetVal)
	{
		return 0;
	}
	return RetVal - devpCoreAudioPauseSamples;

}

static uint32_t devpCoreAudioGetRate (void)
{
	return devpCoreAudioRate;
}

static void devpCoreAudioOnBufferCallback (int samplesuntil, void (*callback)(void *arg, int samples_ago), void *arg)
{
	assert (devpCoreAudioRingBuffer);

	debug_printf ("devpCoreAudioOnBufferCallback: ENTER\n");

	pthread_mutex_lock(&mutex);

	ringbuffer_add_tail_callback_samples (devpCoreAudioRingBuffer, samplesuntil, callback, arg);

	pthread_mutex_unlock(&mutex);

	debug_printf ("devpCoreAudioOnBufferCallback: EXIT\n");
}


static void devpCoreAudioCommitBuffer (unsigned int samples)
{
	debug_printf ("devpCoreAudioCommitBuffer: ENTER\n");

	pthread_mutex_lock(&mutex);

	debug_printf ("devpCoreAudioCommitBuffer: %u\n", samples);

	ringbuffer_head_add_samples (devpCoreAudioRingBuffer, samples);

	pthread_mutex_unlock(&mutex);

	debug_printf ("devpCoreAudioCommitBuffer: EXIT\n");
}

static void devpCoreAudioPause (int pause)
{
	assert (devpCoreAudioBuffer);
	devpCoreAudioInPause = pause;
}

static void devpCoreAudioStop(void)
{
	OSErr status;

	/* TODO, forceflush */

	status=AudioOutputUnitStop(theOutputUnit);
	if (status)
	{
		fprintf(stderr, "[CoreAudio] AudioOutputUnitStop returned %d (%s)\nn", (int)status, OSStatus_to_string(status));
	}

	free (devpCoreAudioBuffer);
	devpCoreAudioBuffer = 0;
	ringbuffer_free (devpCoreAudioRingBuffer);
	devpCoreAudioRingBuffer = 0;
}

static void devpCoreAudioPeekBuffer (void **buf1, unsigned int *buf1length, void **buf2, unsigned int *buf2length)
{
	int pos1, length1, pos2, length2;

	devpCoreAudioIdle (); /* update the tail */

	debug_printf ("devpCoreAudioPeekBuffer: ENTER\n");

	pthread_mutex_lock(&mutex);
	ringbuffer_get_tailandprocessing_samples (devpCoreAudioRingBuffer, &pos1, &length1, &pos2, &length2);
	pthread_mutex_unlock(&mutex);

	debug_printf ("devpCoreAudioPeekBuffer: EXIT\n");

	if (length1)
	{
		*buf1 = (char *)devpCoreAudioBuffer + (pos1 << 2); /* stereo + 16bit */
		*buf1length = length1;
		if (length2)
		{
			*buf2 = (char *)devpCoreAudioBuffer + (pos2 << 2); /* stereo + 16bit */
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


static int devpCoreAudioPlay (uint32_t *rate, enum plrRequestFormat *format, struct ocpfilehandle_t *source_file, struct cpifaceSessionAPI_t *cpifaceSession)
{
	OSErr status;
	int plrbufsize; /* given in ms */
	int buflength;

	devpCoreAudioInPause = 0;
	devpCoreAudioPauseSamples = 0;
	{
		struct timespec tp;
		clock_gettime (CLOCK_MONOTONIC, &tp);
		lastCallbackTime = tp.tv_sec * 1000000;
		lastCallbackTime += tp.tv_nsec / 1000;
	}

	*rate = devpCoreAudioRate; /* fixed */
	*format = PLR_STEREO_16BIT_SIGNED; /* fixed */

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
	buflength = devpCoreAudioRate * plrbufsize / 1000;

	if (!(devpCoreAudioBuffer=calloc (buflength, 4)))
	{
		return 0;
	}

	if (!(devpCoreAudioRingBuffer = ringbuffer_new_samples (RINGBUFFER_FLAGS_STEREO | RINGBUFFER_FLAGS_16BIT | RINGBUFFER_FLAGS_SIGNED | RINGBUFFER_FLAGS_PROCESS, buflength)))
	{
		free (devpCoreAudioBuffer); devpCoreAudioBuffer = 0;
		return 0;
	}

	status=AudioOutputUnitStart(theOutputUnit);
	debug_printf ("[CoreAudio] AudioOutputUnitStart(AudioUnit ci = theOutputUnit) = %d %s\n", (int)status, OSStatus_to_string(status));
	if (status)
	{
		fprintf(stderr, "[CoreAudio] AudioOutputUnitStart returned %d (%s)\n", (int)status, OSStatus_to_string(status));
		free (devpCoreAudioBuffer); devpCoreAudioBuffer = 0;
		ringbuffer_free (devpCoreAudioRingBuffer); devpCoreAudioRingBuffer = 0;
		return 0;
	}

	cpifaceSession->GetMasterSample = plrGetMasterSample;
	cpifaceSession->GetRealMasterVolume = plrGetRealMasterVolume;

	return 1;
}

static const struct plrAPI_t devpCoreAudio = {
	devpCoreAudioIdle,
	devpCoreAudioPeekBuffer,
	devpCoreAudioPlay,
	devpCoreAudioGetBuffer,
	devpCoreAudioGetRate,
	devpCoreAudioOnBufferCallback,
	devpCoreAudioCommitBuffer,
	devpCoreAudioPause,
	devpCoreAudioStop,
	0
};

static int CoreAudioInit(const struct deviceinfo *c)
{
	plrAPI = &devpCoreAudio;
	return 1;
}

static void CoreAudioClose(void)
{
	if (plrAPI == &devpCoreAudio)
	{
		plrAPI = 0;
	}
}

static int CoreAudioDetect(struct deviceinfo *card)
{
	AudioStreamBasicDescription inDesc;
	const int channels=2;
	OSStatus status;

	AudioComponentDescription desc;
	AudioComponent comp;

	UInt32 /*maxFrames,*/ size;

	pthread_mutexattr_init (&mta);
	pthread_mutexattr_settype (&mta, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init (&mutex, &mta);

	AURenderCallbackStruct renderCallback;

	comp = 0;

	desc.componentType = kAudioUnitType_Output;
	desc.componentSubType = kAudioUnitSubType_DefaultOutput;
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;

	comp = AudioComponentFindNext (0, &desc);
	debug_printf ("[CoreAudio] AudioComponentFindNext (Component aComponent = 0, ComponentDescription *looking = {componentType=kAudioUnitType_Output, componentSubType=kAudioUnitSubType_DefaultOutput, componentManufacturer = kAudioUnitManufacturer_Apple, componentFlags = 0, componentFlagsMask = 0}) = %d\n", (int)comp);
	if ( !comp )
	{
		fprintf(stderr, "[CoreAudio] Unable to find the Output Unit component\n");
		goto errorout;
	}

	status = AudioComponentInstanceNew (comp, &theOutputUnit);
	debug_printf ("[CoreAudio] AudioComponentInstanceNew(Component aComponent = %d, ComponentInstance *ci = &theOutputUnit) = %d %s\n", (int)comp, (int)status, OSStatus_to_string(status));
	if (status)
	{
		fprintf(stderr, "[CoreAudio] Unable to open Output Unit component (status = %d (%s))\n", (int)status, OSStatus_to_string(status));
		goto errorout;
	}

#if 0
	do {
		UInt32 i_param_size;
		char *psz_name;
		UInt32 propertySize;
		static AudioDeviceID gOutputDeviceID; /* TODO */

		propertySize = sizeof(gOutputDeviceID);
		status = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice,
		                                  &propertySize,
		                                  &gOutputDeviceID);
		//fprintf(stderr, "[CoreAudio] AudioHardwareGetProperty(AudioHardwarePropertyID inPropertyID = kAudioHardwarePropertyDefaultOutputDevice, &UInt32*ioPropertyDataSize = &4, void*outPropertyData = %p) = %d %p\n *%p=%d\n", &gOutputDeviceID, (int)status, OSStatus_to_string(status), &gOutputDeviceID, (int)gOutputDeviceID);
		if (status) {
			fprintf(stderr, "[CoreAudio] Could not get default output Device (status = %d (%s))\n", (int)status, OSStatus_to_string(status));
			break;
		}

		if (gOutputDeviceID == kAudioDeviceUnknown)
		{
			fprintf(stderr, "[CoreAudio] Unable to find a default output device\n");
			break;
		}

		/* Retrieve the length of the device name. */
		i_param_size = 0;
		status = AudioDeviceGetPropertyInfo(gOutputDeviceID, 0, 0,
		                                    kAudioDevicePropertyDeviceName,
		                                    &i_param_size, NULL);
		if (status != noErr)
		{
			fprintf(stderr, "[CoreAudio] Unable to retrieve default audio device name length (status = %d (%s))\n", (int)status, OSStatus_to_string(status));
			break;
		}

		/* Retrieve the name of the device. */
		psz_name = malloc(i_param_size);
		if (!psz_name)
			break;

		status = AudioDeviceGetProperty(gOutputDeviceID, 0, 0,
		                                kAudioDevicePropertyDeviceName,
		                                &i_param_size, psz_name);
		if (status != noErr)
		{
			fprintf(stderr, "[CoreAudio] Unable to retrieve default audio device name (status = %d (%s))\n", (int)status, OSStatus_to_string(status));
			free( psz_name);
			break;
		}

		fprintf(stderr, "[CoreAudio] Default audio output device ID: %#lx Name: %s\n", (long)gOutputDeviceID, psz_name );

		free (psz_name);

	} while(0);
#endif

	status = AudioUnitInitialize (theOutputUnit);
	debug_printf ("[CoreAudio] AudioUnitInitialize(AudioUnit inUnit = theOutputUnit) = %d %s\n", (int)status, OSStatus_to_string(status));
	if (status) {
		fprintf(stderr, "[CoreAudio] Unable to initialize Output Unit component (status = %d (%s))\n", (int)status, OSStatus_to_string(status));
		goto errorout_component;
	}

	size = sizeof(inDesc);
	status = AudioUnitGetProperty (theOutputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &inDesc, &size);
	debug_printf ("[CoreAudio] AudioUnitGetProperty(AudioUnit inUnit = theOutputUnit, AudioUnitPropertyID inID = kAudioUnitProperty_StreamFormat, AudioUnitScope inScope = kAudioUnitScope_Input, AudioUnitElement inElement = 0, void *outData = &inDesc, UInt32 *ioDataSize = &sizeof(inDesc)) = %d %s\n", (int)status, OSStatus_to_string(status));
	if (status) {
		fprintf(stderr, "[CoreAudio] Unable to get the input (default) format (status = %d (%s))\n", (int)status, OSStatus_to_string(status));
		goto errorout_uninit;
	}

#ifdef COREAUDIO_DEBUG
	print_format("default inDesc: ", &inDesc);
#endif

	devpCoreAudioRate = inDesc.mSampleRate;
	/* inDesc.mSampleRate = 44100; Use default here */
	inDesc.mFormatID=kAudioFormatLinearPCM;
	inDesc.mChannelsPerFrame=channels;
	inDesc.mBitsPerChannel=16;
	inDesc.mFormatFlags = kAudioFormatFlagIsSignedInteger|kAudioFormatFlagIsPacked; /* float is possible */
#ifdef WORDS_BIGENDIAN
	inDesc.mFormatFlags |= kAudioFormatFlagIsBigEndian;
#endif
	inDesc.mFramesPerPacket=1;
	inDesc.mBytesPerPacket = inDesc.mBytesPerFrame = inDesc.mFramesPerPacket*channels*(inDesc.mBitsPerChannel/8);

#ifdef COREAUDIO_DEBUG
	print_format("wanted inDesc: ", &inDesc);
#endif

	status = AudioUnitSetProperty (theOutputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &inDesc, sizeof(inDesc));
	debug_printf ("[CoreAudio] AudioUnitSetProperty(AudioUnit inUnit = theOutputUnit, AudioUnitPropertyID inID = kAudioUnitProperty_StreamFormat, AudioUnitScope inScope = kAudioUnitScope_Input, AudioUnitElement inElement = 0, void *outData = &inDesc, UInt32 *ioDataSize = &sizeof(inDesc)) = %d %s\n", (int)status, OSStatus_to_string(status));
	if (status) {
		fprintf(stderr, "[CoreAudio] Unable to set the input format (status = %d (%s))\n", (int)status, OSStatus_to_string(status));
		goto errorout_uninit;
	}

#if 0
	size = sizeof(maxFrames);
	status = AudioUnitGetProperty (theOutputUnit, kAudioDevicePropertyBufferSize, kAudioUnitScope_Input, 0, &maxFrames, &size);
	if (status) {
		fprintf(stderr, "[CoreAudio] AudioUnitGetProperty returned %d (%s) when getting kAudioDevicePropertyBufferSize\n", (int)status, OSStatus_to_string(status));
		goto errorout_uninit;
	}

	fprintf(stderr, "[CoreAudio] maxFrames=%d\n", (int)maxFrames);
#endif

	renderCallback.inputProc = theRenderProc;
	renderCallback.inputProcRefCon = 0;
	status = AudioUnitSetProperty(theOutputUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &renderCallback, sizeof(AURenderCallbackStruct));
	debug_printf ("[CoreAudio] AudioUnitSetProperty(AudioUnit inUnit = theOutputUnit, AudioUnitPropertyID inID = kAudioUnitProperty_SetRenderCallback, AudioUnitScope inScope = kAudioUnitScope_Input, AudioUnitElement inElement = 0, void *outData = AURenderCallbackStruct {inputProc = theRenderProc, inputProcRefCon = 0}, UInt32 *ioDataSize = &sizeof(AURenderCallbackStruct)) = %d %s\n", (int)status, OSStatus_to_string(status));
        if (status) {
		fprintf(stderr, "[CoreAudio] Unable to set the render callback (status = %d (%s))\n", (int)status, OSStatus_to_string(status));
		goto errorout_uninit;
	}

	/* ao is now created, the above is needed only ONCE */
	card->devtype=&plrCoreAudio;
	card->port=0;
	card->port2=0;
	card->subtype=-1;
	card->mem=0;
	card->chan=2;

	needfini=1;

	return 1;

errorout_uninit:
	AudioUnitUninitialize (theOutputUnit);
errorout_component:
	AudioComponentInstanceDispose (theOutputUnit);
errorout:
	return 0;
}

static void __attribute__((destructor))fini(void)
{
	if (needfini)
	{
		debug_printf ("[CoreAudio] AudioUnitUninitialize (theOutputUnit)\n");
		debug_printf ("[CoreAudio] AudioComponentInstanceDispose (theOutputUnit)\n");
		AudioUnitUninitialize (theOutputUnit);
		AudioComponentInstanceDispose (theOutputUnit);
	}
}

struct sounddevice plrCoreAudio={SS_PLAYER, 1, "CoreAudio player", CoreAudioDetect,  CoreAudioInit,  CoreAudioClose};

const char *dllinfo="driver plrCoreAudio";
const struct linkinfostruct dllextinfo = {.name = "devpcoreaudio", .desc = "OpenCP Player Device: CoreAudio (c) 2006-'22 Stian Skjelstad", .ver = DLLVERSION};
