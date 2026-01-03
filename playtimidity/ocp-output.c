/* OpenCP Module Player
 * copyright (c) 2005-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Timidity glue logic
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
/*
    Based on timidity/output.c
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#ifdef STDC_HEADERS
#include <string.h>
#include <ctype.h>
#elif HAVE_STRINGS_H
#include <strings.h>
#endif
#include "timidity.h"
#include "common.h"
#include "output.h"
#include "tables.h"
#include "controls.h"
#include "audio_cnv.h"

/*****************************************************************/
/* Some functions to convert signed 32-bit data to other formats */

void s32tos8(int32 *lp, int32 c)
{
    int8 *cp=(int8 *)(lp);
    int32 l, i;

    for(i = 0; i < c; i++)
    {
	l=(lp[i])>>(32-8-GUARD_BITS);
	if (l>127) l=127;
	else if (l<-128) l=-128;
	cp[i] = (int8)(l);
    }
}

void s32tou8(int32 *lp, int32 c)
{
    uint8 *cp=(uint8 *)(lp);
    int32 l, i;

    for(i = 0; i < c; i++)
    {
	l=(lp[i])>>(32-8-GUARD_BITS);
	if (l>127) l=127;
	else if (l<-128) l=-128;
	cp[i] = 0x80 ^ ((uint8) l);
    }
}

void s32tos16(int32 *lp, int32 c)
{
  int16 *sp=(int16 *)(lp);
  int32 l, i;

  for(i = 0; i < c; i++)
    {
      l=(lp[i])>>(32-16-GUARD_BITS);
      if (l > 32767) l=32767;
      else if (l<-32768) l=-32768;
      sp[i] = (int16)(l);
    }
}

void s32tou16(int32 *lp, int32 c)
{
  uint16 *sp=(uint16 *)(lp);
  int32 l, i;

  for(i = 0; i < c; i++)
    {
      l=(lp[i])>>(32-16-GUARD_BITS);
      if (l > 32767) l=32767;
      else if (l<-32768) l=-32768;
      sp[i] = 0x8000 ^ (uint16)(l);
    }
}

void s32tos16x(int32 *lp, int32 c)
{
  int16 *sp=(int16 *)(lp);
  int32 l, i;

  for(i = 0; i < c; i++)
    {
      l=(lp[i])>>(32-16-GUARD_BITS);
      if (l > 32767) l=32767;
      else if (l<-32768) l=-32768;
      sp[i] = XCHG_SHORT((int16)(l));
    }
}

void s32tou16x(int32 *lp, int32 c)
{
  uint16 *sp=(uint16 *)(lp);
  int32 l, i;

  for(i = 0; i < c; i++)
    {
      l=(lp[i])>>(32-16-GUARD_BITS);
      if (l > 32767) l=32767;
      else if (l<-32768) l=-32768;
      sp[i] = XCHG_SHORT(0x8000 ^ (uint16)(l));
    }
}

#define MAX_24BIT_SIGNED (8388607)
#define MIN_24BIT_SIGNED (-8388608)

#define STORE_S24_LE(cp, l) *cp++ = l & 0xFF, *cp++ = l >> 8 & 0xFF, *cp++ = l >> 16
#define STORE_S24_BE(cp, l) *cp++ = l >> 16, *cp++ = l >> 8 & 0xFF, *cp++ = l & 0xFF
#define STORE_U24_LE(cp, l) *cp++ = l & 0xFF, *cp++ = l >> 8 & 0xFF, *cp++ = l >> 16 ^ 0x80
#define STORE_U24_BE(cp, l) *cp++ = l >> 16 ^ 0x80, *cp++ = l >> 8 & 0xFF, *cp++ = l & 0xFF

#ifdef LITTLE_ENDIAN
  #define STORE_S24  STORE_S24_LE
  #define STORE_S24X STORE_S24_BE
  #define STORE_U24  STORE_U24_LE
  #define STORE_U24X STORE_U24_BE
#else
  #define STORE_S24  STORE_S24_BE
  #define STORE_S24X STORE_S24_LE
  #define STORE_U24  STORE_U24_BE
  #define STORE_U24X STORE_U24_LE
#endif

void s32tos24(int32 *lp, int32 c)
{
	uint8 *cp = (uint8 *)(lp);
	int32 l, i;

	for(i = 0; i < c; i++)
	{
		l = (lp[i]) >> (32 - 24 - GUARD_BITS);
		l = (l > MAX_24BIT_SIGNED) ? MAX_24BIT_SIGNED
				: (l < MIN_24BIT_SIGNED) ? MIN_24BIT_SIGNED : l;
		STORE_S24(cp, l);
	}
}

void s32tou24(int32 *lp, int32 c)
{
	uint8 *cp = (uint8 *)(lp);
	int32 l, i;

	for(i = 0; i < c; i++)
	{
		l = (lp[i]) >> (32 - 24 - GUARD_BITS);
		l = (l > MAX_24BIT_SIGNED) ? MAX_24BIT_SIGNED
				: (l < MIN_24BIT_SIGNED) ? MIN_24BIT_SIGNED : l;
		STORE_U24(cp, l);
	}
}

void s32tos24x(int32 *lp, int32 c)
{
	uint8 *cp = (uint8 *)(lp);
	int32 l, i;

	for(i = 0; i < c; i++)
	{
		l = (lp[i]) >> (32 - 24 - GUARD_BITS);
		l = (l > MAX_24BIT_SIGNED) ? MAX_24BIT_SIGNED
				: (l < MIN_24BIT_SIGNED) ? MIN_24BIT_SIGNED : l;
		STORE_S24X(cp, l);
	}
}

void s32tou24x(int32 *lp, int32 c)
{
	uint8 *cp = (uint8 *)(lp);
	int32 l, i;

	for(i = 0; i < c; i++)
	{
		l = (lp[i]) >> (32 - 24 - GUARD_BITS);
		l = (l > MAX_24BIT_SIGNED) ? MAX_24BIT_SIGNED
				: (l < MIN_24BIT_SIGNED) ? MIN_24BIT_SIGNED : l;
		STORE_U24X(cp, l);
	}
}

void s32toulaw(int32 *lp, int32 c)
{
    int8 *up=(int8 *)(lp);
    int32 l, i;

    for(i = 0; i < c; i++)
    {
	l=(lp[i])>>(32-16-GUARD_BITS);
	if (l > 32767) l=32767;
	else if (l<-32768) l=-32768;
	up[i] = AUDIO_S2U(l);
    }
}

void s32toalaw(int32 *lp, int32 c)
{
    int8 *up=(int8 *)(lp);
    int32 l, i;

    for(i = 0; i < c; i++)
    {
	l=(lp[i])>>(32-16-GUARD_BITS);
	if (l > 32767) l=32767;
	else if (l<-32768) l=-32768;
	up[i] = AUDIO_S2A(l);
    }
}

/* return: number of bytes */
int32 general_output_convert(int32 *buf, int32 count)
{
    int32 bytes;

    if(!(play_mode->encoding & PE_MONO))
	count *= 2; /* Stereo samples */
    bytes = count;
    if(play_mode->encoding & PE_16BIT)
    {
	bytes *= 2;
	if(play_mode->encoding & PE_BYTESWAP)
	{
	    if(play_mode->encoding & PE_SIGNED)
		s32tos16x(buf, count);
	    else
		s32tou16x(buf, count);
	}
	else if(play_mode->encoding & PE_SIGNED)
	    s32tos16(buf, count);
	else
	    s32tou16(buf, count);
    }
	else if(play_mode->encoding & PE_24BIT) {
		bytes *= 3;
		if(play_mode->encoding & PE_BYTESWAP)
		{
			if(play_mode->encoding & PE_SIGNED)
			s32tos24x(buf, count);
			else
			s32tou24x(buf, count);
		} else if(play_mode->encoding & PE_SIGNED)
			s32tos24(buf, count);
		else
			s32tou24(buf, count);
    }
	else if(play_mode->encoding & PE_ULAW)
	s32toulaw(buf, count);
    else if(play_mode->encoding & PE_ALAW)
	s32toalaw(buf, count);
    else if(play_mode->encoding & PE_SIGNED)
	s32tos8(buf, count);
    else
	s32tou8(buf, count);
    return bytes;
}

int validate_encoding(int enc, int include_enc, int exclude_enc)
{
    const char *orig_enc_name, *enc_name;

    orig_enc_name = output_encoding_string(enc);
    enc |= include_enc;
    enc &= ~exclude_enc;
    if(enc & (PE_ULAW|PE_ALAW))
	enc &= ~(PE_24BIT|PE_16BIT|PE_SIGNED|PE_BYTESWAP);
    if(!(enc & PE_16BIT || enc & PE_24BIT))
	enc &= ~PE_BYTESWAP;
    if(enc & PE_24BIT)
	enc &= ~PE_16BIT;	/* 24bit overrides 16bit */
    enc_name = output_encoding_string(enc);
    if(strcmp(orig_enc_name, enc_name) != 0)
	ctl->cmsg(CMSG_WARNING, VERB_NOISY,
		  "Notice: Audio encoding is changed `%s' to `%s'",
		  orig_enc_name, enc_name);
    return enc;
}

int32 apply_encoding(int32 old_enc, int32 new_enc)
{
	const int32 mutex_flags[] = {
		PE_16BIT | PE_24BIT | PE_ULAW | PE_ALAW,
		PE_BYTESWAP | PE_ULAW | PE_ALAW,
		PE_SIGNED | PE_ULAW | PE_ALAW,
	};
	int i;

	for (i = 0; i < sizeof mutex_flags / sizeof mutex_flags[0]; i++) {
		if (new_enc & mutex_flags[i])
			old_enc &= ~mutex_flags[i];
	}
	return old_enc | new_enc;
}

const char *output_encoding_string(int enc)
{
    if(enc & PE_MONO)
    {
	if(enc & PE_16BIT)
	{
	    if(enc & PE_SIGNED)
		return "16bit (mono)";
	    else
		return "unsigned 16bit (mono)";
	}
	else if(enc & PE_24BIT)
	{
	    if(enc & PE_SIGNED)
		return "24bit (mono)";
	    else
		return "unsigned 24bit (mono)";
	}
	else
	{
	    if(enc & PE_ULAW)
		return "U-law (mono)";
	    else if(enc & PE_ALAW)
		return "A-law (mono)";
	    else if(enc & PE_SIGNED)
		return "8bit (mono)";
	    else
		return "unsigned 8bit (mono)";
	}
    }
    else if(enc & PE_16BIT)
    {
	if(enc & PE_BYTESWAP)
	{
	    if(enc & PE_SIGNED)
		return "16bit (swap)";
	    else
		return "unsigned 16bit (swap)";
	}
	else if(enc & PE_SIGNED)
	    return "16bit";
	else
	    return "unsigned 16bit";
    }
    else if(enc & PE_24BIT)
    {
	if(enc & PE_SIGNED)
	    return "24bit";
	else
	    return "unsigned 24bit";
    }
    else
	if(enc & PE_ULAW)
	    return "U-law";
	else if(enc & PE_ALAW)
	    return "A-law";
	else if(enc & PE_SIGNED)
	    return "8bit";
	else
	    return "unsigned 8bit";
    /*NOTREACHED*/
}

int get_encoding_sample_size(int32 enc)
{
	int size = (enc & PE_MONO) ? 1 : 2;

	if (enc & PE_24BIT)
		size *= 3;
	else if (enc & PE_16BIT)
		size *= 2;
	return size;
}
