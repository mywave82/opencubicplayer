/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'24 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Freqency calculation routines
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
 */

/* included from deviwave.c */

static uint32_t hnotetab6848[16]={11131415,4417505,1753088,695713,276094,109568,43482,17256,6848,2718,1078,428,170,67,27,11};
static uint32_t hnotetab8363[16]={13594045,5394801,2140928,849628,337175,133808,53102,21073,8363,3319,1317,523,207,82,33,13};
static uint16_t notetab[16]={32768,30929,29193,27554,26008,24548,23170,21870,20643,19484,18390,17358,16384,15464,14596,13777};
static uint16_t finetab[16]={32768,32650,32532,32415,32298,32182,32066,31950,31835,31720,31606,31492,31379,31266,31153,31041};
static uint16_t xfinetab[16]={32768,32761,32753,32746,32738,32731,32724,32716,32709,32702,32694,32687,32679,32672,32665,32657};

/* Some Amiga hardware theory:
 *
 * DMA can fetch up to two audio samples per horizontal line and the audio output must be contiously feed every sample of the given audio
 * sample data. What actually changes is the feed rate into the DAC register, since you adjust how many hardware ticks DMA / Audio
 * hardware waits before the next feed cycle. High value = low sample rate.
 *
 * Amiga NTSC:
 *  3579545 hardware ticks per second
 *  minimum ticks per sample: 124 in order to guarantee a DMA fetch will be performed before the data is feed into the DAC.
 *  maximum ticks per sample: 65535 (upper limit of the counter)
 *
 *  maximum sample rate:
 *  3579545 Hz
 *  ---------- = 28867.30 samples per second
 *     124
 *
 *  minimum sample rate:
 *  3579545 Hz
 *  ---------- = 54.62 samples per second
 *    65535
 *
 * Amiga PAL:
 *  3546895 hardware ticks per second
 *  minimum ticks per sample: 123
 *  maximum ticks per sample: 65535
 *
 *  maximum sample rate:
 *  3546895 Hz
 *  ---------- = 28836.54 samples per second
 *     123
 *
 *  minimum sample rate:
 *  3546895 Hz
 *  ---------- = 54.12 samples per second
 *    65535
 */

/* Amiga module-file standard C-2 period is 428 giving us sample rate for
 * NTSC: 3579545 / 428 = 8363.42 samples per second
 * PAL:  3546895 / 428 = 8287.14 samples per second
 */

/* The these are the paths currently in use for handling sampling rates in Open Cubic Player:
 *
 * (period value is inverse of sample rate, it tells a DAC how many clock pulses to delay between each
 * sample output for conversion)
 *
 * "LinearFreq", which actally works in note-space
 *    Take note data as-is
 *
 *    Manipulate all effects using the provided numbers
 *
 *    Transfer the given negative value to devw using mcpCPitch
 *
 *    Negate the value and use mcpGetFreq8363()
 *
 *    Scale the actual sample rate using the provided data
 *
 *
 *
 * "Not LinearFreq", which actually works in period space (inverse sample rate)
 *    Convert note data when needed to period value using mcpGetFreq6848() // negate note, offset the value to place the most bass note on the mcpGetFreq6848() center point.
 *
 *    Manipulate all effects using the provided numbers
 *
 *    Transfer the given value to devw using mcpCPitch6848
 *
 *    Scale the actual sample rate using the provided data
 *
 *
 *
 * 8363 is probably choosen, since it is the default sample rate for classic Amiga modules.
 * 6848 is probably choosen, since it is the period for the lowest possible note in Fast Tracker II exported modules.
 */

/* mcpGetFreq8363() Retrieve the relative sampling freqency for a given note, given that the middle C is sampled at 8363 Herz
 *
 * input:
 *  signed integer note input - given 256 cents per note, centered on middle a C
 */

static int mcpGetFreq8363(int note)
{
  note=-note;
  return umulshr16(umulshr16(umulshr16(hnotetab8363[((note+0x8000)>>12)&0xF],notetab[(note>>8)&0xF]*2),finetab[(note>>4)&0xF]*2),xfinetab[note&0xF]*2);
}

/* mcpGetFreq6848() Retrieve the relative sampling freqency for a given note, given that the middle C is sampled at 6848 Herz
 *
 * input:
 *  signed integer note input - given 256 cents per note, centered on middle a C
 */
static int mcpGetFreq6848(int note)
{
  note=-note;
  return umulshr16(umulshr16(umulshr16(hnotetab6848[((note+0x8000)>>12)&0xF],notetab[(note>>8)&0xF]*2),finetab[(note>>4)&0xF]*2),xfinetab[note&0xF]*2);
}

static int mcpGetNote8363(unsigned int frq)
{
  int16_t x;
  unsigned int i;
  for (i=0; i<15; i++)
    if (hnotetab8363[i+1]<frq)
      break;
  x=(i-8)*16*256;
  frq=umuldiv(frq, 32768, hnotetab8363[i]);
  for (i=0; i<15; i++)
    if (notetab[i+1]<frq)
      break;
  x+=i*256;
  frq=umuldiv(frq, 32768, notetab[i]);
  for (i=0; i<15; i++)
    if (finetab[i+1]<frq)
      break;
  x+=i*16;
  frq=umuldiv(frq, 32768, finetab[i]);
  for (i=0; i<15; i++)
    if (xfinetab[i+1]<frq)
      break;
  return -x-i;
}

static int mcpGetNote6848(unsigned int frq)
{
  int16_t x;
  unsigned int i;
  for (i=0; i<15; i++)
    if (hnotetab6848[i+1]<frq)
      break;
  x=(i-8)*16*256;
  frq=umuldiv(frq, 32768, hnotetab6848[i]);
  for (i=0; i<15; i++)
    if (notetab[i+1]<frq)
      break;
  x+=i*256;
  frq=umuldiv(frq, 32768, notetab[i]);
  for (i=0; i<15; i++)
    if (finetab[i+1]<frq)
      break;
  x+=i*16;
  frq=umuldiv(frq, 32768, finetab[i]);
  for (i=0; i<15; i++)
    if (xfinetab[i+1]<frq)
      break;
  return -x-i;
}
