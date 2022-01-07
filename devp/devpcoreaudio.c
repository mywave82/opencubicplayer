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
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>
#include <CoreAudio/CoreAudio.h>
#include <CoreServices/CoreServices.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>
#include "types.h"
#include "boot/plinkman.h"
#include "dev/imsdev.h"
#include "dev/player.h"
#include "stuff/imsrtns.h"

static AudioUnit theOutputUnit;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
struct sounddevice plrCoreAudio;
static int needfini=0;

static unsigned int CoreAudioRate;
static void *playbuf=0;
static int buflen;
volatile static int kernpos, cachepos, bufpos; /* in bytes */
/* kernpos = kernel write header
 * bufpos = the write header given out of this driver */

/*  playbuf     kernpos  cachepos   bufpos      buflen
 *    |           | kernlen | cachelen |          |
 *
 *  on flush, we update all variables> *  on getbufpos we return kernpos-(1 sample) as safe point to buffer up to
 *  on getplaypos we use last known kernpos if locked, else update kernpos
 */


volatile static int cachelen, kernlen; /* to make life easier */

volatile static uint64_t playpos; /* how many samples have we done totally */

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
	int i, i2;

#ifdef COREAUDIO_DEBUG
	fprintf(stderr, "renderproc ENTER\n");
#endif
	pthread_mutex_lock(&mutex);

#ifdef COREAUDIO_DEBUG
	fprintf(stderr, "renderproc BEGIN\n");
	fprintf (stderr, "  INPUT  cachelen=%d\n", (int)cachelen);
	fprintf (stderr, "         ioData->mBuffers[0].mDataByteSize=%d\n", (int)ioData->mBuffers[0].mDataByteSize);
#endif

	i = cachelen;/* >>2  *stereo + 16it */
	if (i > ioData->mBuffers[0].mDataByteSize)
		i = ioData->mBuffers[0].mDataByteSize;

	kernlen = ioData->mBuffers[0].mDataByteSize = i;
	cachelen -= i;
	cachepos = kernpos;
	playpos += i<<(2/*stereo+bit16*/);

#ifdef COREAUDIO_DEBUG
	fprintf (stderr, "  OUTPUT kernlen=%d\n", i);
	fprintf (stderr, "         ioData->mBuffers[0].mDataByteSize=%d\n", i);
	fprintf (stderr, "         cachepos=%d\n", (int)cachepos);
	fprintf (stderr, "         cachelen=%d\n", (int)cachelen);
	fprintf (stderr, "         playpos=%d\n", (int)playpos);
#endif

	if ((i+kernpos)>buflen)
	{
		i2 = ( i + kernpos ) % buflen;
		i = i - i2;
	} else {
		i2 = 0;
	}

#ifdef COREAUDIO_DEBUG
	fprintf (stderr, "  PROC   i=%d i2=%d\n", i, i2);
#endif

	memcpy(ioData->mBuffers[0].mData, playbuf+kernpos, i);
	if (i2)
		memcpy(ioData->mBuffers[0].mData+i, playbuf, i2);

	kernpos = (kernpos+i+i2)%buflen;

#ifdef COREAUDIO_DEBUG
	fprintf (stderr, "         kernpos=%d\n", (int)kernpos);




/*
playbuf      kernpos      cachepos     bufpos       buflen
|            12345678     12345678     12345678     12345678
|            |  kernlen   |  cachelen  |            |
|            |  12345678  |  12345678  |            |
*/
	fprintf (stderr, "playbuf      kernpos      cachepos     bufpos       buflen\n");
	fprintf (stderr, "|            %-8d     %-8d     %-8d     %-8d\n", (int)kernpos, (int)cachepos, (int)bufpos, (int)buflen);
	fprintf (stderr, "|            |  %-8d  |  %-8d  |            |\n", (int)kernlen, (int)cachelen);


	fprintf(stderr, "renderproc END\n");
#endif

	pthread_mutex_unlock(&mutex);

	return noErr;
}

/* stolen from devposs */
static int CoreAudioGetBufPos(void)
{
	int retval;

	/* this thing is utterly broken */

	pthread_mutex_lock(&mutex);
	if (kernpos==bufpos)
	{
		if (cachelen|kernlen)
		{
			retval=kernpos;
			pthread_mutex_unlock(&mutex);
			return retval;
		}
	}
	retval=(kernpos+buflen-4 /* 1 sample = 4 bytes*/)%buflen;
	pthread_mutex_unlock(&mutex);
	return retval;
}

static int CoreAudioGetPlayPos(void)
{
	int retval;

	pthread_mutex_lock(&mutex);
	retval=kernpos;
	pthread_mutex_unlock(&mutex);
        return retval;
}

static void CoreAudioIdle(void)
{
}

static void CoreAudioAdvanceTo(unsigned int pos)
{
	pthread_mutex_lock(&mutex);

	cachelen+=(pos-bufpos+buflen)%buflen;
	bufpos=pos;

	pthread_mutex_unlock(&mutex);
}

static uint32_t CoreAudioGetTimer(void)
{
	long retval;

	pthread_mutex_lock(&mutex);

	retval=playpos-(kernlen + cachelen);

	pthread_mutex_unlock(&mutex);

	return imuldiv(retval, 65536>>(2/*stereo+bit16*/), plrRate);
}

static void CoreAudioStop(void)
{
	OSErr status;

	/* TODO, forceflush */

	if (playbuf)
	{
		free(playbuf);
		playbuf=0;
	}

	plrGetBufPos=0;
	plrGetPlayPos=0;
	plrIdle=0;
	plrAdvanceTo=0;
	plrGetTimer=0;

	status=AudioOutputUnitStop(theOutputUnit);
	if (status)
		fprintf(stderr, "[CoreAudio] AudioOutputUnitStop returned %d (%s)\nn", (int)status, OSStatus_to_string(status));
}

static int CoreAudioPlay(void **buf, unsigned int *len, struct ocpfilehandle_t *source_file)
{
	OSErr status;

	if ((*len)<(plrRate&~3))
		*len=plrRate&~3;
	if ((*len)>(plrRate*4))
		*len=plrRate*4;
	playbuf=*buf=malloc(*len);

	memset(*buf, 0x80008000, (*len)>>2);
	buflen = *len;

	cachepos=0;
	kernpos=0;
	bufpos=0;
	cachelen=0;
	kernlen=0;

	playpos=0;

	plrGetBufPos=CoreAudioGetBufPos;
	plrGetPlayPos=CoreAudioGetPlayPos;
	plrIdle=CoreAudioIdle;
	plrAdvanceTo=CoreAudioAdvanceTo;
	plrGetTimer=CoreAudioGetTimer;

	status=AudioOutputUnitStart(theOutputUnit);
#ifdef COREAUDIO_DEBUG
	fprintf(stderr, "[CoreAudio] AudioOutputUnitStart(AudioUnit ci = theOutputUnit) = %d %s\n", (int)status, OSStatus_to_string(status));
#endif
	if (status)
	{
		fprintf(stderr, "[CoreAudio] AudioOutputUnitStart returned %d (%s)\n", (int)status, OSStatus_to_string(status));
		free(*buf);
		*buf = playbuf = 0;
		plrGetBufPos = 0;
		plrGetPlayPos = 0;
		plrIdle = 0;
		plrAdvanceTo = 0;
		plrGetTimer = 0;
		return 0;
	}
	return 1;
}

static void CoreAudioSetOptions(unsigned int rate, int opt)
{
	plrRate=CoreAudioRate; /* fixed */
	plrOpt=PLR_STEREO|PLR_16BIT|PLR_SIGNEDOUT; /* fixed fixed fixed */
}

static int CoreAudioInit(const struct deviceinfo *c)
{
	plrSetOptions=CoreAudioSetOptions;
	plrPlay=CoreAudioPlay;
	plrStop=CoreAudioStop;
	return 1;
}

static void CoreAudioClose(void)
{
	plrSetOptions=0;
	plrPlay=0;
	plrStop=0;
}

static int CoreAudioDetect(struct deviceinfo *card)
{
	AudioStreamBasicDescription inDesc;
	const int channels=2;
	OSStatus status;

	AudioComponentDescription desc;
	AudioComponent comp;

	UInt32 /*maxFrames,*/ size;

	AURenderCallbackStruct renderCallback;

	comp = 0;

	desc.componentType = kAudioUnitType_Output;
	desc.componentSubType = kAudioUnitSubType_DefaultOutput;
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;

	comp = AudioComponentFindNext (0, &desc);
#ifdef COREAUDIO_DEBUG
	fprintf(stderr, "[CoreAudio] AudioComponentFindNext (Component aComponent = 0, ComponentDescription *looking = {componentType=kAudioUnitType_Output, componentSubType=kAudioUnitSubType_DefaultOutput, componentManufacturer = kAudioUnitManufacturer_Apple, componentFlags = 0, componentFlagsMask = 0}) = %d\n", (int)comp);
#endif
	if ( !comp )
	{
		fprintf(stderr, "[CoreAudio] Unable to find the Output Unit component\n");
		goto errorout;
	}

	status = AudioComponentInstanceNew (comp, &theOutputUnit);
#ifdef COREAUDIO_DEBUG
	fprintf(stderr, "[CoreAudio] AudioComponentInstanceNew(Component aComponent = %d, ComponentInstance *ci = &theOutputUnit) = %d %s\n", (int)comp, (int)status, OSStatus_to_string(status));
#endif
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
#ifdef COREAUDIO_DEBUG
	fprintf(stderr, "[CoreAudio] AudioUnitInitialize(AudioUnit inUnit = theOutputUnit) = %d %s\n", (int)status, OSStatus_to_string(status));
#endif
	if (status) {
		fprintf(stderr, "[CoreAudio] Unable to initialize Output Unit component (status = %d (%s))\n", (int)status, OSStatus_to_string(status));
		goto errorout_component;
	}

	size = sizeof(inDesc);
	status = AudioUnitGetProperty (theOutputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &inDesc, &size);
#ifdef COREAUDIO_DEBUG
	fprintf(stderr, "AudioUnitGetProperty(AudioUnit inUnit = theOutputUnit, AudioUnitPropertyID inID = kAudioUnitProperty_StreamFormat, AudioUnitScope inScope = kAudioUnitScope_Input, AudioUnitElement inElement = 0, void *outData = &inDesc, UInt32 *ioDataSize = &sizeof(inDesc)) = %d %s\n", (int)status, OSStatus_to_string(status));
#endif
	if (status) {
		fprintf(stderr, "[CoreAudio] Unable to get the input (default) format (status = %d (%s))\n", (int)status, OSStatus_to_string(status));
		goto errorout_uninit;
	}

#ifdef COREAUDIO_DEBUG
	print_format("default inDesc: ", &inDesc);
#endif

	CoreAudioRate = inDesc.mSampleRate;
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
#ifdef COREAUDIO_DEBUG
	fprintf(stderr, "AudioUnitSetProperty(AudioUnit inUnit = theOutputUnit, AudioUnitPropertyID inID = kAudioUnitProperty_StreamFormat, AudioUnitScope inScope = kAudioUnitScope_Input, AudioUnitElement inElement = 0, void *outData = &inDesc, UInt32 *ioDataSize = &sizeof(inDesc)) = %d %s\n", (int)status, OSStatus_to_string(status));
#endif
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
#ifdef COREAUDIO_DEBUG
	fprintf(stderr, "AudioUnitSetProperty(AudioUnit inUnit = theOutputUnit, AudioUnitPropertyID inID = kAudioUnitProperty_SetRenderCallback, AudioUnitScope inScope = kAudioUnitScope_Input, AudioUnitElement inElement = 0, void *outData = AURenderCallbackStruct {inputProc = theRenderProc, inputProcRefCon = 0}, UInt32 *ioDataSize = &sizeof(AURenderCallbackStruct)) = %d %s\n", (int)status, OSStatus_to_string(status));
#endif
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
#ifdef COREAUDIO_DEBUG
		fprintf(stderr, "[CoreAudio] AudioUnitUninitialize (theOutputUnit)\n");
		fprintf(stderr, "[CoreAudio] AudioComponentInstanceDispose (theOutputUnit)\n");
#endif
		AudioUnitUninitialize (theOutputUnit);
		AudioComponentInstanceDispose (theOutputUnit);
	}
}

struct sounddevice plrCoreAudio={SS_PLAYER, 1, "CoreAudio player", CoreAudioDetect,  CoreAudioInit,  CoreAudioClose};

char *dllinfo="driver plrCoreAudio";
struct linkinfostruct dllextinfo = {.name = "devpcoreaudio", .desc = "OpenCP Player Device: CoreAudio (c) 2006-'22 Stian Skjelstad", .ver = DLLVERSION, .size = 0};
