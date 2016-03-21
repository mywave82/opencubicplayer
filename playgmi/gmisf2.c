#include "config.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "gmiplay.h"
#include "gmisf2.h"

#define roundup(x,y) (((x) + (y) - 1) & ~((y) - 1))

static const char *SFGeneratorToText(uint16_t i)
{
	switch (i)
	{
		case 0: return "startAddrsOffset";
		case 1: return "endAddrsOffset";
		case 2: return "startloopAddrsOffset";
		case 3: return "endloopAddrsOffset";
		case 4: return "startAddrsCoarseOffset";
		case 5: return "modLfoToPitch";
		case 6: return "vibLfoToPitch";
		case 7: return "modEnvToPitch";
		case 8: return "initialFilterFc";
		case 9: return "initialFilterQ";
		case 10: return "modLfoToFilterFc";
		case 11: return "modEnvToFilterFc";
		case 12: return "endAddrsCoarseOffset";
		case 13: return "modLfoToVolume";
		case 15: return "chorusEffectsSend";
		case 16: return "reverbEffectsSend";
		case 17: return "pan";
		case 21: return "delayModLFO";
		case 22: return "freqModLFO";
		case 23: return "delayVibLFO";
		case 24: return "freqVibLFO";
		case 25: return "delayModEnv";
		case 26: return "attackModEnv";
		case 27: return "holdModEnv";
		case 28: return "decayModEnv";
		case 29: return "sustainModEnv";
		case 30: return "releaseModEnv";
		case 31: return "keynumToModEnvHold";
		case 32: return "keynumToModEnvDecay";
		case 33: return "delayVolEnv";
		case 34: return "attackVolEnv";
		case 35: return "holdVolEnv";
		case 36: return "decayVolEnv";
		case 37: return "sustainVolEnv";
		case 38: return "releaseVolEnv";
		case 39: return "keynumToVolEnvHold";
		case 40: return "keynumToVolEnvDecay";
		case 41: return "instrument";
		case 43: return "keyRange";
		case 44: return "velRange";
		case 45: return "startloopAddrsCoarseOffset";
		case 46: return "keynum";
		case 47: return "velocity";
		case 48: return "initialAttenuation";
		case 50: return "endloopAddrsCoarseOffset";
		case 51: return "coarseTune";
		case 52: return "fineTune";
		case 53: return "sampleID";
		case 54: return "sampleModes";
		case 56: return "scaleTuning";
		case 57: return "exclusiveClass";
		case 58: return "overridingRootKey";
		default: return "";
	}
}

static void SFModulatorSourceToText(uint16_t i, char *buffer, int buffer_len)
{
	uint8_t T = i >> 10;
	uint8_t P = !!(i & 0x0200);
	uint8_t D = !!(i & 0x0100);
	uint8_t CC = !!(i & 0x0080);
	uint8_t Index = i & 0x7f;

	const char *source;
	const char *type;

	if (!CC)
	{
		switch (Index)
		{
			case  0: source = "No Controller";            break;
			case  2: source = "Note-On Velocity";         break;
			case  3: source = "Note-On Key Number";       break;
			case  10: source = "Poly Pressure";           break;
			case  13: source = "Channel Pressure";        break;
			case  14: source = "Pitch Wheel";             break;
			case  16: source = "Pitch Wheel Sensitivity"; break;
			case 127: source = "Link";                    break;
			default: source = "Unknown";                  break;
		}
	} else {
		switch (Index)
		{ /* from the MIDI spec */
			case   0: case 32: source = "Bank Select"; /* illegal */                       break;
			case   1: case 33: source = "Modulation wheel or lever";                       break;
			case   2: case 34: source = "Breath Controller";                               break;
			case   4: case 36: source = "Foot Controller";                                 break;
			case   5: case 37: source = "Portamento Time";                                 break;
			case   6: case 38: source = "Data Entry MSB"; /* illegal */                    break;
			case   7: case 39: source = "Channel Volume";                                  break;
			case   8: case 40: source = "Balance";                                         break;
			case  10: case 42: source = "Pan";                                             break;
			case  11: case 43: source = "Expression Controller";                           break;
			case  12: case 44: source = "Effect Control 1";                                break;
			case  13: case 45: source = "Effect Control 2";                                break;
			case  16: case 48: source = "General Purpose Controller #1";                   break;
			case  17: case 49: source = "General Purpose Controller #2";                   break;
			case  18: case 50: source = "General Purpose Controller #3";                   break;
			case  19: case 51: source = "General Purpose Controller #4";                   break;
			case  64: source = "Damper pedal (sustain)";                                   break;
			case  65: source = "Portamento on/off";                                        break;
			case  66: source = "Sostenuto";                                                break;
			case  67: source = "Soft pedal";                                               break;
			case  68: source = "Legato footswitch";                                        break;
			case  69: source = "Hold 2";                                                   break;
			case  70: source = "Sound Controller #1 (default: Sound Variation)";           break;
			case  71: source = "Sound Controller #2 (default: Timbre/Harmonic Intensity)"; break;
			case  72: source = "Sound Controller #3 (default: Release Time)";              break;
			case  73: source = "Sound Controller #4 (default: Attack Time)";               break;
			case  74: source = "Sound Controller #5 (default: Brightness)";                break;
			case  75: source = "Sound Controller #6";                                      break;
			case  76: source = "Sound Controller #7";                                      break;
			case  77: source = "Sound Controller #8";                                      break;
			case  78: source = "Sound Controller #9";                                      break;
			case  79: source = "Sound Controller #10";                                     break;
			case  80: source = "General Purpose Controller #5";                            break;
			case  81: source = "General Purpose Controller #6";                            break;
			case  82: source = "General Purpose Controller #7";                            break;
			case  83: source = "General Purpose Controller #8";                            break;
			case  84: source = "Portamento Control";                                       break;
			case  91: source = "Effects #1 Depth (formerly External Effects Depth)";       break;
			case  92: source = "Effects #2 Depth (formerly Tremolo Depth)";                break;
			case  93: source = "Effects #3 Depth (formerly Chorus Depth)";                 break;
			case  94: source = "Effects #4 Depth (formerly Celeste (Deptune) Depth)";      break;
			case  95: source = "Effects #5 Depth (formerly Phaser Depth)";                 break;
			case  96: source = "Data increment";                                           break;
			case  97: source = "Data decrement";                                           break;
			case  98: source = "Non-Registered Parameter Number LSB"; /* illegal */        break;
			case  99: source = "Non-Registered Parameter Number MSB"; /* illegal */        break;
			case 100: source = "Registered Parameter Number LSB"; /* illegal */            break;
			case 101: source = "Registered Parameter Number MSB"; /* illegal */            break;
			default: source = "Unknown";                                                   break;
		}
	}

	switch (T)
	{
		case 0: type = "Linear";   break;
		case 1: type = "Concave";  break;
		case 2: type = "Convex";   break;
		case 3: type = "Switch";   break;
		default: type = "Unknown"; break;
	}

	snprintf (buffer, buffer_len, "%s/%s/%s/%s", source, D?"descending":"ascending", P?"unipolar":"bipolar", type);
}

static const char *SFTransformToText(uint16_t i)
{
	switch (i)
	{
		case 0: return "Linear";
		case 1: return "Absolute Value";
		default: return "Unknown";
	}
}

static const char *SFSampleTypeToText(uint16_t i)
{
	switch (i)
	{
		case 1: return "mono";
		case 2: return "right";
		case 4: return "left";
		case 8: return "linked";
		case 32769: return "ROM mono";
		case 32770: return "ROM right";
		case 32772: return "ROM left";
		case 32776: return "ROM linked";
		default: return "Unknown";
	}
}

static int riff_dechunk(struct SF2_Session *s, unsigned char *buffer, uint32_t len, int (*callback)(struct SF2_Session *s, unsigned char *head, unsigned char *buffer, uint32_t len))
{
	while (len)
	{
		unsigned char *head;
		uint32_t datasize;

		if (len < 8)
		{
			fprintf (stderr, "riff_dechunk: %d bytes of data left. Not enough to assemble a new chunk header\n", (int)len);
			return -1;
		}

		head=buffer;
		datasize = (buffer[7] << 24) |
		           (buffer[6] << 16) |
		           (buffer[5] << 8) |
		            buffer[4];

		buffer += 8;
		len -= 8;

		if (datasize > len)
		{
			fprintf (stderr, "riff_dechunk: not enough data left to assemble chunk payload\n");
			return -1;
		}

		if (callback(s, head, buffer, datasize))
		{
			return -1;
		}

		buffer += datasize;
		len -= datasize;
	}
	return 0;
}

static int parse_level2_INFO(struct SF2_Session *s, unsigned char *head, unsigned char *buffer, uint32_t len)
{
	fprintf (stderr, "    %c%c%c%c %d\n", head[0], head[1], head[2], head[3], (int)len);
	if ((head[0] == 'i') &&
	    (head[1] == 'f') &&
	    (head[2] == 'i') &&
	    (head[3] == 'l'))
	{
		if (len != 4)
		{
			fprintf (stderr, "ifil chunk expected to be 4 bytes long\n");
			return -1;
		}

		if (s->detected & DETECTED_ifil)
		{
			fprintf (stderr, "Too many level 3 ifil signatures\n");
			return -1;
		}
		s->detected |= DETECTED_ifil;

		s->ifil_major = (buffer[1] << 8) | buffer[0];
		s->ifil_minor = (buffer[3] << 8) | buffer[2];

		fprintf (stderr, "         file format %d.%d\n", s->ifil_major, s->ifil_minor);

		if (s->ifil_major < 2)
		{
			fprintf (stderr, "Invalid version\n");
			return -1;
		}

		return 0;
	}
	if ((head[0] == 'i') &&
	    (head[1] == 's') &&
	    (head[2] == 'n') &&
	    (head[3] == 'g'))
	{
		int i;

		if (len > 256)
		{
			fprintf (stderr, "warning, engine name should be limited to 256 bytes\n");
		}
		if (s->detected & DETECTED_isng)
		{
			fprintf (stderr, "Too many level 3 isng signatures\n");
			return -1;
		}
		s->detected |= DETECTED_isng;

		fprintf (stderr, "         Sound engine: \"");
		for (i=0; i < len; i++)
		{
			if (buffer[i] == 0)
			{
				break;
			}
			if (buffer[i]>=32 && (buffer[i]<=127))
			{
				fprintf (stderr, "%c", buffer[i]);
			} else {
				fprintf (stderr, "\\x%02x", buffer[i]);
			}
		}
		fprintf(stderr, "\"\n");
		return 0;
	}
	if ((head[0] == 'I') &&
	    (head[1] == 'N') &&
	    (head[2] == 'A') &&
	    (head[3] == 'M'))
	{
		int i;

		if (len > 256)
		{
			fprintf (stderr, "warning, soundfont name should be limited to 256 bytes\n");
		}
		if (s->detected & DETECTED_INAM)
		{
			fprintf (stderr, "Too many level 3 INAM signatures\n");
			return -1;
		}
		s->detected |= DETECTED_INAM;

		fprintf (stderr, "         SoundFont Name: \"");
		for (i=0; i < len; i++)
		{
			if (buffer[i] == 0)
			{
				break;
			}
			if (buffer[i]>=32 && (buffer[i]<=127))
			{
				fprintf (stderr, "%c", buffer[i]);
			} else {
				fprintf (stderr, "\\x%02x", buffer[i]);
			}
		}
		fprintf(stderr, "\"\n");
		return 0;
	}
	if ((head[0] == 'i') &&
	    (head[1] == 'r') &&
	    (head[2] == 'o') &&
	    (head[3] == 'm'))
	{
		int i;

		if (len > 256)
		{
			fprintf (stderr, "warning, wavetable rom name should be limited to 256 bytes\n");
		}
		if (s->detected & DETECTED_irom)
		{
			fprintf (stderr, "Too many level 3 IROM signatures\n");
			return -1;
		}
		s->detected |= DETECTED_irom;

		fprintf (stderr, "         WaveTable ROM Name: \"");
		for (i=0; i < len; i++)
		{
			if (buffer[i] == 0)
			{
				break;
			}
			if (buffer[i]>=32 && (buffer[i]<=127))
			{
				fprintf (stderr, "%c", buffer[i]);
			} else {
				fprintf (stderr, "\\x%02x", buffer[i]);
			}
		}
		fprintf(stderr, "\"\n");
		return 0;
	}
	if ((head[0] == 'i') &&
	    (head[1] == 'v') &&
	    (head[2] == 'e') &&
	    (head[3] == 'r'))
	{
		uint16_t major, minor;

		if (len != 4)
		{
			fprintf (stderr, "warning, wavetable rom revision should be 4 bytes long\n");
		}
		if (s->detected & DETECTED_iver)
		{
			fprintf (stderr, "Too many level 3 iver signatures\n");
			return -1;
		}
		s->detected |= DETECTED_iver;

		major = (buffer[1] << 8) | buffer[0];
		minor = (buffer[3] << 8) | buffer[2];

		fprintf (stderr, "         wavetable rom revision: %d.%d\n", major, minor);
		return 0;
	}
	if ((head[0] == 'I') &&
	    (head[1] == 'C') &&
	    (head[2] == 'R') &&
	    (head[3] == 'D'))
	{
		int i;

		if (len > 256)
		{
			fprintf (stderr, "warning, wavetable creation date should be limited to 256 bytes\n");
		}
		if (s->detected & DETECTED_ICRD)
		{
			fprintf (stderr, "Too many level 3 ICRD signatures\n");
			return -1;
		}
		s->detected |= DETECTED_ICRD;

		fprintf (stderr, "         Creation Date: \"");
		for (i=0; i < len; i++)
		{
			if (buffer[i] == 0)
			{
				break;
			}
			if (buffer[i]>=32 && (buffer[i]<=127))
			{
				fprintf (stderr, "%c", buffer[i]);
			} else {
				fprintf (stderr, "\\x%02x", buffer[i]);
			}
		}
		fprintf(stderr, "\"\n");
		return 0;
	}
	if ((head[0] == 'I') &&
	    (head[1] == 'E') &&
	    (head[2] == 'N') &&
	    (head[3] == 'G'))
	{
		int i;

		if (len > 256)
		{
			fprintf (stderr, "warning, engineer should be limited to 256 bytes\n");
		}
		if (s->detected & DETECTED_IENG)
		{
			fprintf (stderr, "Too many level 3 IENG signatures\n");
			return -1;
		}
		s->detected |= DETECTED_IENG;

		fprintf (stderr, "         Engineer: \"");
		for (i=0; i < len; i++)
		{
			if (buffer[i] == 0)
			{
				break;
			}
			if (buffer[i]>=32 && (buffer[i]<=127))
			{
				fprintf (stderr, "%c", buffer[i]);
			} else {
				fprintf (stderr, "\\x%02x", buffer[i]);
			}
		}
		fprintf(stderr, "\"\n");
		return 0;
	}
	if ((head[0] == 'I') &&
	    (head[1] == 'P') &&
	    (head[2] == 'R') &&
	    (head[3] == 'D'))
	{
		int i;

		if (len > 256)
		{
			fprintf (stderr, "warning, product name should be limited to 256 bytes\n");
		}
		if (s->detected & DETECTED_IPRD)
		{
			fprintf (stderr, "Too many level 3 IPRD signatures\n");
			return -1;
		}
		s->detected |= DETECTED_IPRD;

		fprintf (stderr, "         Target Product Name: \"");
		for (i=0; i < len; i++)
		{
			if (buffer[i] == 0)
			{
				break;
			}
			if (buffer[i]>=32 && (buffer[i]<=127))
			{
				fprintf (stderr, "%c", buffer[i]);
			} else {
				fprintf (stderr, "\\x%02x", buffer[i]);
			}
		}
		fprintf(stderr, "\"\n");
		return 0;
	}
	if ((head[0] == 'I') &&
	    (head[1] == 'C') &&
	    (head[2] == 'O') &&
	    (head[3] == 'P'))
	{
		int i;

		if (len > 256)
		{
			fprintf (stderr, "warning, product name should be limited to 256 bytes\n");
		}
		if (s->detected & DETECTED_ICOP)
		{
			fprintf (stderr, "Too many level 3 ICOP signatures\n");
			return -1;
		}
		s->detected |= DETECTED_ICOP;

		fprintf (stderr, "         Copyright: \"");
		for (i=0; i < len; i++)
		{
			if (buffer[i] == 0)
			{
				break;
			}
			if (buffer[i]>=32 && (buffer[i]<=127))
			{
				fprintf (stderr, "%c", buffer[i]);
			} else {
				fprintf (stderr, "\\x%02x", buffer[i]);
			}
		}
		fprintf(stderr, "\"\n");
		return 0;
	}
	if ((head[0] == 'I') &&
	    (head[1] == 'C') &&
	    (head[2] == 'M') &&
	    (head[3] == 'T'))
	{
		int i;

		if (len > 65536)
		{
			fprintf (stderr, "warning, comment should be limited to 65536 bytes\n");
		}
		if (s->detected & DETECTED_ICMT)
		{
			fprintf (stderr, "Too many level 3 ICMT signatures\n");
			return -1;
		}
		s->detected |= DETECTED_ICMT;

		fprintf (stderr, "         Comment: \"");
		for (i=0; i < len; i++)
		{
			if (buffer[i] == 0)
			{
				break;
			}
			if (buffer[i]>=32 && (buffer[i]<=127))
			{
				fprintf (stderr, "%c", buffer[i]);
			} else {
				fprintf (stderr, "\\x%02x", buffer[i]);
			}
		}
		fprintf(stderr, "\"\n");
		return 0;
	}
	if ((head[0] == 'I') &&
	    (head[1] == 'S') &&
	    (head[2] == 'F') &&
	    (head[3] == 'T'))
	{
		int i;

		if (len > 256)
		{
			fprintf (stderr, "warning, software should be limited to 256 bytes\n");
		}
		if (s->detected & DETECTED_ISFT)
		{
			fprintf (stderr, "Too many level 3 ISFT signatures\n");
			return -1;
		}
		s->detected |= DETECTED_ISFT;

		fprintf (stderr, "         Software: \"");
		for (i=0; i < len; i++)
		{
			if (buffer[i] == 0)
			{
				break;
			}
			if (buffer[i]>=32 && (buffer[i]<=127))
			{
				fprintf (stderr, "%c", buffer[i]);
			} else {
				fprintf (stderr, "\\x%02x", buffer[i]);
			}
		}
		fprintf(stderr, "\"\n");
		return 0;
	}

	return 0;
}

static int parse_level2_sdta(struct SF2_Session *s, unsigned char *head, unsigned char *buffer, uint32_t len)
{
	fprintf (stderr, "    %c%c%c%c %d\n", head[0], head[1], head[2], head[3], (int)len);

	if ((head[0] == 's') &&
	    (head[1] == 'm') &&
	    (head[2] == 'p') &&
	    (head[3] == 'l'))
	{
		if (len & 1)
		{
			fprintf (stderr, "warning, sample has odd number of bytes, which is impossible for collecting 16bit audio samples\n");
		}
		if (s->detected & DETECTED_smpl)
		{ /* for us, this is mandatory, since software implementation does not support ROM samples */
			fprintf (stderr, "Too many level 3 smpl signatures\n");
			return -1;
		}
		s->detected |= DETECTED_smpl;

		fprintf (stderr, "         %d samples of audio\n", len >> 1);

		s->samples = (uint16_t *)buffer;
		s->samples_n = len >> 1;

		return 0;
	}

	if ((head[0] == 's') &&
	    (head[1] == 'm') &&
	    (head[2] == '2') &&
	    (head[3] == '4'))
	{
		if ((s->ifil_major == 2) && (s->ifil_minor <= 4))
		{
			fprintf (stderr, "Ignoring this data, since specifications tells us to.\n");
			return 0;
		}

		if (s->detected |= DETECTED_sm24)
		{
			fprintf (stderr, "Too many level 3 sm24 signatures\n");
			return -1;
		}
		s->detected |= DETECTED_sm24;
		if (!(s->detected & DETECTED_smpl))
		{
			fprintf (stderr, "sm24 chunk without smpl chunk\n");
			return -1;
		}
		if ((len != s->samples_n) && (len != (s->samples_n + 1)))
		{ /* standard allows for this chunk to be one byte too long in order to meet the RIFF 16bit alignment requirements */
			fprintf (stderr, "sm24 has incorrect size to match with smpl chunk\n");
			return -1;
		}

		fprintf (stderr, "         extended 24bit quality on audio\n");

		s->samples24 = (uint8_t *)buffer;

		return 0;
	}

	return 0;
}

static int parse_level2_pdta_helper(struct SF2_Session *s, int len, int recsize, int D, const char *name, void *ptr, int targetsize, int *count)
{
	if (len % recsize)
	{
		fprintf (stderr, "Unaligned number of entries\n");
		return -1;
	}

	if (s->detected & D)
	{
		fprintf (stderr, "Too many level 3 %s signatures\n", name);
		return -1;
	}
	s->detected |= D;

	*count = len / recsize;
	if (!*count)
	{
		fprintf (stderr, "No %s entries\n", name);
		return -1;
	}

	*(void **)ptr = calloc (*count, targetsize);

	return 0;
}

static int parse_level2_pdta(struct SF2_Session *s, unsigned char *head, unsigned char *buffer, uint32_t len)
{
	fprintf (stderr, "    %c%c%c%c %d\n", head[0], head[1], head[2], head[3], (int)len);

	if ((head[0] == 'p') &&
	    (head[1] == 'h') &&
	    (head[2] == 'd') &&
	    (head[3] == 'r'))
	{
		int i;

		if (parse_level2_pdta_helper (s, len, 38, DETECTED_phdr, "phdr", &s->phdr, sizeof (s->phdr[0]), &s->phdr_n))
			return -1;

		for (i=0; i < s->phdr_n; i++)
		{
			/* The last entry is EOP, and is only used to calculate the number of presetbags the actual last data entry has */

			memcpy (s->phdr[i].PresetName, buffer + i*38, 20);
			s->phdr[i].Preset         = buffer[i*38+20] | (buffer[i*38+21]<<8);
			s->phdr[i].Bank           = buffer[i*38+22] | (buffer[i*38+23]<<8);
			s->phdr[i].PresetBagIndex = buffer[i*38+24] | (buffer[i*38+25]<<8);
			s->phdr[i].Library        = buffer[i*38+26] | (buffer[i*38+27]<<8) | (buffer[i*38+28]<<16) | (buffer[i*38+29]<<24);
			s->phdr[i].Genre          = buffer[i*38+30] | (buffer[i*38+31]<<8) | (buffer[i*38+32]<<16) | (buffer[i*38+33]<<24);
			s->phdr[i].Morphology     = buffer[i*38+34] | (buffer[i*38+35]<<8) | (buffer[i*38+36]<<16) | (buffer[i*38+37]<<24);

			fprintf (stderr, "         preset %d bank %d presetbagindex %d name \"%s\"\n", s->phdr[i].Preset, s->phdr[i].Bank, s->phdr[i].PresetBagIndex, s->phdr[i].PresetName);
		}

		return 0;
	}

	if ((head[0] == 'p') &&
	    (head[1] == 'b') &&
	    (head[2] == 'a') &&
	    (head[3] == 'g'))
	{
		int i;

		if (parse_level2_pdta_helper (s, len, 4, DETECTED_pbag, "pbag", &s->pbag, sizeof (s->pbag[0]), &s->pbag_n))
			return -1;

		for (i=0; i < s->pbag_n; i++)
		{
			/* The last entry is EOP, and is only used to calculate the number of presetbags the actual last data entry has */

			s->pbag[i].GenIndex = buffer[i*4+0] | (buffer[i*4+1]<<8);
			s->pbag[i].ModIndex = buffer[i*4+2] | (buffer[i*4+3]<<8);

			fprintf (stderr, "         presetbag[%d] GenIndex %d ModIndex %d\n", i, s->pbag[i].GenIndex, s->pbag[i].ModIndex);
		}

		return 0;
	}

	if ((head[0] == 'p') &&
	    (head[1] == 'm') &&
	    (head[2] == 'o') &&
	    (head[3] == 'd'))
	{
		int i;
		/* modulators are not defined yet in version 2.00 */

		if (parse_level2_pdta_helper (s, len, 10, DETECTED_pmod, "pmod", &s->pmod, sizeof (s->pmod[0]), &s->pmod_n))
			return -1;

		for (i=0; i < s->pmod_n; i++)
		{
			char a[128], b[128];

			/* The last entry is EOP, and is only used to calculate the number of presetbags the actual last data entry has */

			s->pmod[i].SrcOper  =         buffer[i*10+0] | (buffer[i*10+1]<<8);
			s->pmod[i].DestOper =         buffer[i*10+2] | (buffer[i*10+3]<<8);
			s->pmod[i].Amount = (int16_t)(buffer[i*10+4] | (buffer[i*10+5]<<8));
			s->pmod[i].AmountSrcOper =    buffer[i*10+6] | (buffer[i*10+7]<<8);
			s->pmod[i].TransOper =        buffer[i*10+8] | (buffer[i*10+9]<<8);

			SFModulatorSourceToText (s->pmod[i].SrcOper, a, sizeof(a));
			SFModulatorSourceToText (s->pmod[i].AmountSrcOper, b, sizeof(b));
			fprintf (stderr, "         modulator[%d] SrcOper %d(%s) DestOper %d(%s) Amount %d AmountSrcOper %d(%s) TransOper %d(%s)\n", i, s->pmod[i].SrcOper, a, s->pmod[i].DestOper, SFGeneratorToText(s->pmod[i].DestOper), s->pmod[i].Amount, s->pmod[i].AmountSrcOper, b, s->pmod[i].TransOper, SFTransformToText(s->pmod[i].TransOper));
		}

		return 0;
	}

	if ((head[0] == 'p') &&
	    (head[1] == 'g') &&
	    (head[2] == 'e') &&
	    (head[3] == 'n'))
	{
		int i;
		/* modulators are not defined yet in version 2.00 */

		if (parse_level2_pdta_helper (s, len, 4, DETECTED_pgen, "pgen", &s->pgen, sizeof (s->pgen[0]), &s->pgen_n))
			return -1;

		for (i=0; i < s->pgen_n; i++)
		{
			/* The last entry is EOP, and is only used to calculate the number of presetbags the actual last data entry has */

			s->pgen[i].GenOper = buffer[i*4+0] | (buffer[i*4+1]<<8);
			s->pgen[i].Amount  = buffer[i*4+2] | (buffer[i*4+3]<<8);

			fprintf (stderr, "         generator[%d] GenOper %d(%s) Amount 0x%04x\n", i, s->pgen[i].GenOper, SFGeneratorToText(s->pgen[i].GenOper), s->pgen[i].Amount);
		}

		return 0;
	}
	if ((head[0] == 'i') &&
	    (head[1] == 'n') &&
	    (head[2] == 's') &&
	    (head[3] == 't'))
	{
		int i;

		if (parse_level2_pdta_helper (s, len, 22, DETECTED_inst, "inst", &s->inst, sizeof (s->inst[0]), &s->inst_n))
			return -1;

		for (i=0; i < s->inst_n; i++)
		{
			/* The last entry is EOP, and is only used to calculate the number of presetbags the actual last data entry has */

			memcpy (s->inst[i].InstName, buffer + i*22, 20);;
			s->inst[i].InstBagIndex = buffer[i*22+20] | (buffer[i*22+21]<<8);

			fprintf (stderr, "         instrument[%d] InstrumentBagIndex %d Name \"%s\"\n", i, s->inst[i].InstBagIndex, s->inst[i].InstName);
		}

		return 0;
	}
	if ((head[0] == 'i') &&
	    (head[1] == 'b') &&
	    (head[2] == 'a') &&
	    (head[3] == 'g'))
	{
		int i;

		if (parse_level2_pdta_helper (s, len, 4, DETECTED_ibag, "ibag", &s->ibag, sizeof (s->ibag[0]), &s->ibag_n))
			return -1;

		for (i=0; i < s->ibag_n; i++)
		{
			/* The last entry is EOP, and is only used to calculate the number of presetbags the actual last data entry has */

			s->ibag[i].InstGenIndex = buffer[i*4+0] | (buffer[i*4+1]<<8);
			s->ibag[i].InstModIndex = buffer[i*4+2] | (buffer[i*4+3]<<8);

			fprintf (stderr, "         instrumentbag[%d] InstGenIndex %d InstModIndex %d\n", i, s->ibag[i].InstGenIndex, s->ibag[i].InstModIndex);
		}

		return 0;
	}
	if ((head[0] == 'i') &&
	    (head[1] == 'm') &&
	    (head[2] == 'o') &&
	    (head[3] == 'd'))
	{
		int i;

		if (parse_level2_pdta_helper (s, len, 10, DETECTED_imod, "imod", &s->imod, sizeof (s->imod[0]), &s->imod_n))
			return -1;

		for (i=0; i < s->imod_n; i++)
		{
			char a[128], b[128];

			/* The last entry is EOP, and is only used to calculate the number of presetbags the actual last data entry has */

			s->imod[i].SrcOper  =         buffer[i*10+0] | (buffer[i*10+1]<<8);
			s->imod[i].DestOper =         buffer[i*10+2] | (buffer[i*10+3]<<8);
			s->imod[i].Amount = (int16_t)(buffer[i*10+4] | (buffer[i*10+5]<<8));
			s->imod[i].AmountSrcOper =    buffer[i*10+6] | (buffer[i*10+7]<<8);
			s->imod[i].TransOper =        buffer[i*10+8] | (buffer[i*10+9]<<8);

			SFModulatorSourceToText (s->imod[i].SrcOper, a, sizeof(a));
			SFModulatorSourceToText (s->imod[i].AmountSrcOper, b, sizeof(b));
			fprintf (stderr, "         instrumentmodulator[%d] SrcOper %d(%s) DestOper %d(%s) Amount %d AmountSrcOper %d(%s) TransOper %d(%s)\n", i, s->imod[i].SrcOper, a, s->imod[i].DestOper, SFGeneratorToText(s->imod[i].DestOper), s->imod[i].Amount, s->imod[i].AmountSrcOper, b, s->imod[i].TransOper, SFTransformToText(s->imod[i].TransOper));
		}

		return 0;
	}
	if ((head[0] == 'i') &&
	    (head[1] == 'g') &&
	    (head[2] == 'e') &&
	    (head[3] == 'n'))
	{
		int i;

		if (parse_level2_pdta_helper (s, len, 4, DETECTED_igen, "igen", &s->igen, sizeof (s->igen[0]), &s->igen_n))
			return -1;

		for (i=0; i < s->igen_n; i++)
		{
			/* The last entry is EOP, and is only used to calculate the number of presetbags the actual last data entry has */

			s->igen[i].GenOper = buffer[i*4+0] | (buffer[i*4+1]<<8);
			s->igen[i].Amount  = buffer[i*4+2] | (buffer[i*4+3]<<8);

			fprintf (stderr, "         instrumentgenerator[%d] GenOper %d(%s), Amount 0x%04x\n", i, s->igen[i].GenOper, SFGeneratorToText(s->igen[i].GenOper), s->igen[i].Amount);
		}

		return 0;
	}
	if ((head[0] == 's') &&
	    (head[1] == 'h') &&
	    (head[2] == 'd') &&
	    (head[3] == 'r'))
	{
		int i;
		if (parse_level2_pdta_helper (s, len, 46, DETECTED_shdr, "shdr", &s->shdr, sizeof (s->shdr[0]), &s->shdr_n))
			return -1;

		for (i=0; i < s->shdr_n; i++)
		{
			/* The last entry is EOP, and is only used to calculate the number of presetbags the actual last data entry has */
			memcpy (s->shdr[i].SampleName, buffer + i*46, 20);
			s->shdr[i].Start      = buffer[i*46+20] | (buffer[i*46+21]<<8) | (buffer[i*46+22]<<16) | (buffer[i*46+23]<<24);
			s->shdr[i].End        = buffer[i*46+24] | (buffer[i*46+25]<<8) | (buffer[i*46+26]<<16) | (buffer[i*46+27]<<24);
			s->shdr[i].StartLoop  = buffer[i*46+28] | (buffer[i*46+29]<<8) | (buffer[i*46+30]<<16) | (buffer[i*46+31]<<24);
			s->shdr[i].EndLoop    = buffer[i*46+32] | (buffer[i*46+33]<<8) | (buffer[i*46+34]<<16) | (buffer[i*46+35]<<24);
			s->shdr[i].SampleRate = buffer[i*46+36] | (buffer[i*46+37]<<8) | (buffer[i*46+38]<<16) | (buffer[i*46+39]<<24);
			s->shdr[i].RootNote   = buffer[i*46+40]; /* 60 = C */
			s->shdr[i].PitchCorrection = buffer[i*46+41];
			s->shdr[i].SampleLink = buffer[i*46+42] | (buffer[i*46+43]<<8);
			s->shdr[i].SampleType = buffer[i*46+44] | (buffer[i*46+45]<<8);	

			fprintf (stderr, "         sample[%d] Start %u, End %u, LoopStart %u, LoopEnd %u, SampleRate %u, RootNode %u, PitchCorrection %d, SampleLink %d, SampleType %u(%s), Name \"%s\"\n", i, (unsigned)s->shdr[i].Start, (unsigned)s->shdr[i].End, (unsigned)s->shdr[i].StartLoop, (unsigned)s->shdr[i].EndLoop, (unsigned)s->shdr[i].SampleRate, s->shdr[i].RootNote, s->shdr[i].PitchCorrection, (unsigned)s->shdr[i].SampleLink, (unsigned)s->shdr[i].SampleType, SFSampleTypeToText(s->shdr[i].SampleType), s->shdr[i].SampleName);
		}

		return 0;
	}

	return 0;
}

static int parse_level1_LIST(struct SF2_Session *s, unsigned char *head, unsigned char *buffer, uint32_t len)
{
	fprintf (stderr, "  %c%c%c%c %d\n", head[0], head[1], head[2], head[3], (int)len);

	if ((head[0] != 'L') ||
	    (head[1] != 'I') ||
	    (head[2] != 'S') ||
	    (head[3] != 'T'))
	{ /* unknown chunk, ignore */
		return 0;
	}

	if (len < 4)
	{
		fprintf (stderr, "Too little space to search for level 2 signature\n");
		return -1;
	}

	fprintf (stderr, "       %c%c%c%c\n", buffer[0], buffer[1], buffer[2], buffer[3]);

	if ((buffer[0] == 'I') &&
	    (buffer[1] == 'N') &&
	    (buffer[2] == 'F') &&
	    (buffer[3] == 'O'))
	{
		if (s->detected & DETECTED_INFO)
		{
			fprintf (stderr, "Too many level 2 INFO signatures\n");
			return -1;
		}
		s->detected |= DETECTED_INFO;

		buffer+=4;
		len-=4;

		if (riff_dechunk (s, buffer, len, parse_level2_INFO))
		{
			return -1;
		}
		if ((s->detected & (DETECTED_ifil | DETECTED_isng | DETECTED_INAM)) != (DETECTED_ifil | DETECTED_isng | DETECTED_INAM))
		{
			fprintf (stderr, "Mandatory chunks missing\n");
			return -1;
		}
		return 0;
	}

	if ((buffer[0] == 's') &&
	    (buffer[1] == 'd') &&
	    (buffer[2] == 't') &&
	    (buffer[3] == 'a'))
	{
		if (s->detected & DETECTED_sdta)
		{
			fprintf (stderr, "Too many level 2 sdta signatures\n");
			return -1;
		}
		if (!(s->detected & DETECTED_INFO))
		{
			fprintf (stderr, "level 1 sdta without level 2 info signature\n");
			return -1;
		}
		s->detected |= DETECTED_sdta;

		buffer+=4;
		len-=4;

		if (riff_dechunk (s, buffer, len, parse_level2_sdta))
		{
			return -1;
		}

		if ((s->detected & DETECTED_smpl) != DETECTED_smpl)
		{
			fprintf (stderr, "Mandatory chunks missing\n");
			return -1;
		}

		return 0;
	}

	if ((buffer[0] == 'p') &&
	    (buffer[1] == 'd') &&
	    (buffer[2] == 't') &&
	    (buffer[3] == 'a'))
	{
		if (s->detected & DETECTED_pdta)
		{
			fprintf (stderr, "Too many level 2 pdta signatures\n");
			return -1;
		}
		if (!(s->detected & DETECTED_sdta))
		{
			fprintf (stderr, "level 1 pdta without level 2 sdta signature\n");
			return -1;
		}
		s->detected |= DETECTED_pdta;

		buffer+=4;
		len-=4;

		if (riff_dechunk (s, buffer, len, parse_level2_pdta))
		{
			return -1;
		}

		if ((s->detected & (DETECTED_phdr | DETECTED_pbag | DETECTED_pmod | DETECTED_pgen | DETECTED_inst | DETECTED_ibag | DETECTED_imod | DETECTED_igen | DETECTED_shdr)) != (DETECTED_phdr | DETECTED_pbag | DETECTED_pmod | DETECTED_pgen | DETECTED_inst | DETECTED_ibag | DETECTED_imod | DETECTED_igen | DETECTED_shdr))
		{
			fprintf (stderr, "Mandatory chunks missing\n");
			return -1;
		}
	}

	return 0;
}

static int parse_RIFF(struct SF2_Session *s, unsigned char *head, unsigned char *buffer, uint32_t len)
{
	fprintf (stderr, "%c%c%c%c %d\n", head[0], head[1], head[2], head[3], (int)len);
	if ((head[0] != 'R') ||
	    (head[1] != 'I') ||
	    (head[2] != 'F') ||
	    (head[3] != 'F'))
	{
		fprintf (stderr, "RIFF header not found\n");
		return -1;
	}
	if (len < 4)
	{
		fprintf (stderr, "Too little space to search for SF2 signature\n");
		return -1;
	}

	fprintf (stderr, "     %c%c%c%c\n", buffer[0], buffer[1], buffer[2], buffer[3]);
	if ((buffer[0] != 's') ||
	    (buffer[1] != 'f') ||
	    (buffer[2] != 'b') ||
	    (buffer[3] != 'k'))
	{
		fprintf (stderr, "SF2 signature not detected\n");
		return -1;
	}

	if (s->detected & DETECTED_sfbk)
	{
		fprintf (stderr, "Too many signatures\n");
		return -1;
	}
	s->detected |= DETECTED_sfbk;

	buffer+=4;
	len-=4;

	if (riff_dechunk (s, buffer, len, parse_level1_LIST))
	{
		return -1;
	}

	if ((s->detected & (DETECTED_INFO | DETECTED_sdta | DETECTED_pdta)) != (DETECTED_INFO | DETECTED_sdta | DETECTED_pdta))
	{
		fprintf (stderr, "Missing one or more level 1 chunks\n");
		return -1;
	}

	return 0;
}

void
__attribute__ ((visibility ("internal")))
SF2_Free(struct SF2_Session *s)
{
	if (s->data_mmap_len)
	{
		munmap (s->data, s->data_mmap_len);
	}
	if (s->data_malloc_len)
	{
		free (s->data);
	}
	free (s->phdr);
	free (s->pbag);
	free (s->pmod);
	free (s->pgen);
	free (s->inst);
	free (s->ibag);
	free (s->imod);
	free (s->igen);
	free (s->shdr);
	free (s);
}

__attribute__ ((visibility ("internal")))
struct SF2_Session *
SF2_Load_FILE(FILE *file)
{
	struct SF2_Session *s = calloc (1, sizeof (*s));
	unsigned char buffer[8];
	uint32_t datasize;

	fseek (file, 0, SEEK_SET);

	if (fread (buffer, 8, 1, file) != 1)
	{
		fprintf (stderr, "[SF2]: SF2_Load_FILE failed to read initial header\n");
		free (s);
		return 0;
	}

	datasize = (buffer[7] << 24) |
	           (buffer[6] << 16) |
	           (buffer[5] << 8) |
	            buffer[4];

	s->data_malloc_len = datasize + 8;
	s->data = malloc (s->data_malloc_len);

	if (!s->data)
	{
		fprintf (stderr, "[SF2]: SF2_Load_FILE: malloc failed\n");
		free (s);
		return 0;
	}

	memcpy (s->data, buffer, 8);
	if (fread (s->data + 8, datasize, 1, file) != 1)
	{
		fprintf (stderr, "[SF2]: SF2_Load_File failed to read file body\n");
		free (s);
		return 0;
	}

	if (riff_dechunk (s, s->data, s->data_len, parse_RIFF))
	{
		goto failed;
	}
	if (!(s->detected & DETECTED_sfbk))
	{
		fprintf (stderr, "[SF2]: Failed to detect SF2 file\n");
		goto failed;
	}

	return s;

failed:
	SF2_Free (s);
	return 0;
}

__attribute__ ((visibility ("internal")))
struct SF2_Session *
SF2_Load_fd(int fd)
{
	struct SF2_Session *s = calloc (1, sizeof (*s));
	struct stat st;
	size_t ps = sysconf(_SC_PAGE_SIZE);

	if (fstat(fd, &st))
	{
		perror("[SF2]: fstat()");
		free (s);
		return 0;
	}
	if (!st.st_size)
	{
		fprintf (stderr, "[SF2]: Zero-size file\n");
		free (s);
		return 0;
	}

	s->data_len = st.st_size;
	s->data_mmap_len = roundup (s->data_len, ps);
	s->data = mmap (0, s->data_mmap_len, PROT_READ, MAP_FILE|MAP_SHARED, fd, 0);

	if (s->data == MAP_FAILED)
	{
		perror ("[SF2]: mmap() failed");
		free (s);
		return 0;
	}

	if (riff_dechunk (s, s->data, s->data_len, parse_RIFF))
	{
		goto failed;
	}
	if (!(s->detected & DETECTED_sfbk))
	{
		fprintf (stderr, "[SF2]: Failed to detect SF2 file\n");
		goto failed;
	}

	return s;

failed:
	SF2_Free (s);
	return 0;
}

struct SF2_Session *global_session = 0;

static void
closeSF2(void)
{
	if (global_session)
	{
		SF2_Free (global_session);
		global_session = 0;
	}
}

static int
loadpatchSF2 (struct minstrument *ins,
              uint8_t             program,
              uint8_t            *sampused,
              struct sampleinfo **smps,
              uint16_t           *samplenum)
{
#error implement me
}

int
addpatchSF2 (struct minstrument *ins,
             uint8_t             program,
             uint8_t             sn,
             uint8_t             sampnum,
             struct sampleinfo  *sip,
             uint16_t           *samplenum)
{
#error implement me
}

int __attribute__ ((visibility ("internal"))) midInitSF2(void)
{
	int fd;

#warning FIXME (hardcoded SF2 file for the time-beeing)
	fd = open ("/usr/share/sounds/sf2/FluidR3_GM.sf2", O_RDONLY);

	if (fd < 0)
	{
		fprintf (stderr, "[SF2]: failed to open %s: %s\n", "/usr/share/sounds/sf2/FluidR3_GM.sf2", strerror (errno));
		return 0;
	}

	global_session = SF2_Load_fd(fd);
	close (fd);
	if (!global_session)
	{
		return 0;
	}

	_midClose = closeSF2;

	loadpatch = loadpatchSF2;
	addpatch = addpatchSF2;
	return 1;
}
