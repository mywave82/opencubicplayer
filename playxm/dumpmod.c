/* OpenCP Module Player
 * copyright (c) 2020-'24 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Utility for dumping the content of a MOD file
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
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "types.h"

#define roundup(x,y) (((x) + (y) - 1) & ~((y) - 1))

int usecolor = 0;
int savesamples = 0;
FILE *savepatterns = 0;

char *FONT_RESET = "";
char *FONT_BRIGHT_BLACK = "";
char *FONT_BRIGHT_RED = "";
char *FONT_BRIGHT_GREEN = "";
char *FONT_BRIGHT_YELLOW = "";
char *FONT_BRIGHT_BLUE = "";
char *FONT_BRIGHT_PURPLE = "";
char *FONT_BRIGHT_CYAN = "";

#if 0

struct
{
	uint32_t offset;
	uint16_t length;
} Instruments[31];

#if 0
typedef struct __attribute__((packed))
{
	char name[20];
	char tracker[8];
	uint8_t sig,type;
	uint8_t maj, min;
	uint8_t it, pats, gv;
	char reserved[13];
} FileHeader;
#endif

#endif

int DumpPrefix (unsigned char *mem, int len, int base, int baselen)
{
	int retval = 0;
	int i;
	printf ("[%s%08x%s]%s", FONT_BRIGHT_BLUE, base, FONT_RESET, FONT_BRIGHT_PURPLE);
	for (i=0; i < baselen; i++)
	{
		if (base+i >= len)
		{
			printf (" \?\?");
			retval = -1;
		} else {
			printf (" %02x", mem[base+i]);
		}
	}
	switch (baselen)
	{
		case 3:  printf (                     "%s ", FONT_RESET); break;
		case 2:  printf (                  "%s    ", FONT_RESET); break;
		case 1:  printf (               "%s       ", FONT_RESET); break;
		case 0:  printf (            "%s          ", FONT_RESET); break;
		default: printf ("%s\n                    ", FONT_RESET); break;
	}
	if (retval) { printf ("%sRAN OUT OF DATA%s\n", FONT_BRIGHT_RED, FONT_RESET); }
	return retval;
}

int DumpInstrument (unsigned char *mem, int len, int base, int instrument)
{
	uint16_t samplelength;
	uint16_t loopstart;
	uint16_t looplength;
	int i;

	printf ("[%sINSTRUMENT %02d / PCM Sample%s]\n", FONT_BRIGHT_CYAN, instrument + 1, FONT_RESET);

	if (DumpPrefix (mem, len, base + 0x00, 22)) return -1;
	printf ("Name: \"");
	for (i=0; i < 22; i++)
	{
		if (!mem[base + 0x00 + i])
		{
			break;
		}
		putchar (mem[base + 0x00 + i]);
	}
	printf("\"\n");

	if (DumpPrefix (mem, len, base + 22, 2)) return -1;
	samplelength = (mem[base+22] << 8) | mem[base+23];
	printf ("SampleLength: %d\n", (int)samplelength);

	if (DumpPrefix (mem, len, base + 24, 1)) return -1;
	printf ("FineTuning: %d", (int)(int8_t)((mem[base + 24] & 0x0f)|((mem[base+24]&0x08)?0xf0:0x00)));
	if (mem[base+24]&0xf0) printf ("%s (WARNING, the upper nibble is not zero)%s", FONT_BRIGHT_YELLOW, FONT_RESET);
	printf("\n");

	if (DumpPrefix (mem, len, base + 25, 1)) return -1;
	printf ("Volume: %d", mem[base+25]);
	if (mem[base+25] > 64)
	{
		printf ("%s (WARNING, value out of range)%s", FONT_BRIGHT_YELLOW, FONT_RESET);
	}
	printf("\n");

	if (DumpPrefix (mem, len, base + 26, 2)) return -1;
	loopstart = (mem[base+26] << 8) | mem[base+27];
	printf ("LoopStart: %d", (int)loopstart);
	if ((samplelength && (loopstart > samplelength)) && ((!samplelength) && (loopstart > 1)))
	{
		printf ("%s (WARNING, value out of range)%s", FONT_BRIGHT_YELLOW, FONT_RESET);
	}
	printf ("\n");

	if (DumpPrefix (mem, len, base + 26, 2)) return -1;
	looplength = (mem[base+26] << 8) | mem[base+27];
	printf ("LoopLength: %d", (int)looplength);
	if ((samplelength && ((int)loopstart + looplength > samplelength)) && ((!samplelength) && (looplength > 0)))
	{
		printf ("%s (WARNING, value out of range)%s", FONT_BRIGHT_YELLOW, FONT_RESET);
	}
	printf ("\n");

	return 0;
}

#if 0
void print_note (unsigned char note)
{
	char *text[16] = {"C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-", "?c", "?d", "?e", "?f"};

	if (note == 0xfe)
	{
		printf ("-0-");
		if (savepatterns)
		{
			fprintf (savepatterns, "-0-");
		}
		return;
	}
	if (note >= 0x60)
	{
		printf ("...");
		if (savepatterns)
		{
			fprintf (savepatterns, "...");
		}
		return;
	}

	if ((note & 0x0f) >= 12) printf ("%s", FONT_BRIGHT_RED);
	printf ("%s", text[note & 0x0f]);
	if (savepatterns)
	{
		fprintf (savepatterns, "%s", text[note & 0x0f]);
	}
	if ((note & 0x0f) >= 12) printf ("%s", FONT_RESET);

	printf ("%d", note >> 4);
	if (savepatterns)
	{
		fprintf (savepatterns, "%d", note>>4);
	}
}
#endif

int DumpPattern (unsigned char *mem, int len, int base, int channels)
{
	int i;

	for (i=0; i < 64; i++)
	{
		int j;

		if (DumpPrefix (mem, len, base, channels * 4)) return -1;

		printf("%02X |", i);
		if (savepatterns)
		{
			fprintf (savepatterns, "%02X | ", i);
		}

		for (j=0; j < channels; j++)
		{
			uint8_t instrument = (mem[base] & 0xf0) | (mem[base+2]>>4);
			uint16_t period = ((mem[base] & 0x0f) << 8) | mem[base+1];
			uint8_t effect = mem[base+2] & 0x0f;
			uint8_t param = mem[base+3];
			base += 4;

			if (period)
			{
				printf (" %03X ", period);
			} else {
				printf (" ... ");
			}

			if (instrument)
			{
				printf ("%02X ", instrument);
			} else {
				printf (".. ");
			}

			printf ("%01X%02X |", effect, param);

			if (savepatterns)
			{
				if (period)
				{
					fprintf (savepatterns, " %03X ", period);
				} else {
					fprintf (savepatterns, " ... ");
				}

				if (instrument)
				{
					fprintf (savepatterns, "%02X ", instrument);
				} else {
					fprintf (savepatterns, ".. ");
				}

				fprintf (savepatterns, "%01X%02X |", effect, param);
			}
		}
		printf("\n");
		if (savepatterns)
		{
			fprintf (savepatterns, "\n");
		}
	}

	return 0;
}

int DumpPatterns (unsigned char *mem, int len, int ofs, int channels, int patterns)
{
	int i;

	for (i=0; i < patterns; i++)
	{
		printf ("[%sPATTERN %d%s]\n", FONT_BRIGHT_CYAN, i, FONT_RESET);
		if (savepatterns)
		{
			fprintf (savepatterns, "PATTERN %d\n", i);
		}
		if (DumpPattern(mem, len, ofs + i * channels * 256, channels)) return -1;
	}

	return 0;
}

#if 0
int DumpHeader (unsigned char *mem, int len)
{
	int i, j;

	if (len < 0x30)
	{
		fprintf (stderr, "%sERROR: len < sizeof(FileHeader)%s\n", FONT_BRIGHT_RED, FONT_RESET);
		return 1;
	}

	printf ("[%sHEADER%s]\n", FONT_BRIGHT_CYAN, FONT_RESET);

	/* hdr.name */
	DumpPrefix (mem, len, 0x00, 20);
	printf ("Name: \"");
	for (i=0; i < 20; i++)
	{
		if (!mem[0x00+i])
			break;
		printf ("%c", (char)mem[0x00+i]);
	}
	printf ("\"\n");

	DumpPrefix (mem, len, 0x14, 8);
	printf ("Tracker: \"");
	j=0;
	for (i=0; i < 8; i++)
	{
		if ((mem[0x14+i] < 0x20) || (mem[0x14+i] >= 0x80))
		{
			j = 1;
		}
	}
	for (i=0; i < 8; i++)
	{
		if (!mem[0x14+i])
			break;
		printf ("%c", (char)mem[0x14+i]);
	}
	printf ("\" %s%s%s\n",
		j ? FONT_BRIGHT_RED : FONT_BRIGHT_GREEN,
		j ? "Non-ASCII" : "OK",
		FONT_RESET);

	DumpPrefix (mem, len, 0x1c, 1);
	printf ("Signature: %s%s%s\n",
		(mem[0x1c] == 0x1A) ? FONT_BRIGHT_GREEN : (mem[0x1c] == 0x02) ? FONT_BRIGHT_YELLOW : FONT_BRIGHT_RED,
		(mem[0x1c] == 0x1A) ? "OK" : (mem[0x1c] == 0x02) ? "Known invalid value" : "Failed!!!!",
		FONT_RESET);

	DumpPrefix (mem, len, 0x1d, 1);
	printf ("Type: ");
	switch (mem[0x1d])
	{
		case  1: printf ("STM song w/o samples\n"); break;
		case  2: printf ("STM module\n"); break;
		case 16: printf ("S3M module, FAILED !!!!!\n"); return 1;
		default: printf ("Unknown, FAILED !!!!!\n"); return 1;
	}

	DumpPrefix (mem, len, 0x1e, 2);
	printf ("Version: %d.%d\n", mem[0x1e], mem[0x1f]);

	DumpPrefix (mem, len, 0x20, 1);
	printf ("Initial Tempo: %d\n", mem[0x20]);

	DumpPrefix (mem, len, 0x21, 1);
	printf ("Patterns: %d\n", mem[0x21]);

	DumpPrefix (mem, len, 0x22, 1);
	printf ("Global Volume: %d\n", mem[0x22]);

	DumpPrefix (mem, len, 0x23, 13);
	printf ("(Reserved/not used)\n");

	return 0;
}

#endif

int DumpOrders (unsigned char *mem, int len, int ofs, int count)
{
	int MaxOrder = 0;
	int i;

	printf ("[%sORDERS%s]\n", FONT_BRIGHT_CYAN, FONT_RESET);

	if (DumpPrefix (mem, len, ofs, count)) return -1;

	for (i=0; i < count; i++)
	{
		printf ("%d ", mem[ofs+i]);
		if (mem[ofs+i] > MaxOrder)
		{
			MaxOrder = mem[ofs+i];
		}
	}
	printf ("\n");
	if (count < 128)
	{
		if (DumpPrefix (mem, len, ofs + count, 128-count)) return -1;
		printf("(");
		for (i=count; i < 128; i++)
		{
			printf ("%d%s", mem[ofs+i], (i!=127)?" ":"");
			if (mem[ofs+i] > MaxOrder)
			{
				MaxOrder = mem[ofs+i];
			}
		}
		printf (")\n");
	}

	return MaxOrder;
}

#if 0

int ParseSTM (unsigned char *mem, int len)
{
	int i;
	int patternlen = 0;

	if (DumpHeader (mem, len))
	{
		return -1;
	}

	for (i=0; i < 31; i++)
	{
		if (DumpInstrument (mem, len, 0x30 + i*0x20, i))
		{
			return -1;
		}
	}

	if (DumpOrders (mem, len, mem[0x1f]?128:64, mem[0x21]))
	{
		return -1;
	}

	for (i=0; i < mem[0x21]; i++)
	{
		int retval = DumpPattern (mem, len, 0x30 + 0x20*31 + (mem[0x1f]?128:64) + patternlen, i);
		if (retval < 0)
		{
			return -1;
		}
		patternlen += DumpPattern (mem, len, 0x30 + 0x20*31 + (mem[0x1f]?128:64) + patternlen, i);
	}
	if (savepatterns)
	{
		fclose (savepatterns);
	}

	for (i=0; i < 31; i++)
	{
		if (Instruments[i].length > 0)
		{
			printf ("[%sINSTRUMENT %02x SAMPLE DATA%s]\n", FONT_BRIGHT_CYAN, i + 1, FONT_RESET);

			printf ("[%s%08x-%08x%s]%s\n", FONT_BRIGHT_BLUE, Instruments[i].offset, Instruments[i].offset + Instruments[i].length - 1, FONT_RESET, FONT_RESET);
			if (savesamples)
			{
				if ((Instruments[i].offset + Instruments[i].length) > len)
				{
					printf ("%sWARNING: Unable to store instrument %d, missing data\n%s", FONT_BRIGHT_YELLOW, i + 1, FONT_RESET);
				} else {
					char filename[33];
					int fd;
					snprintf (filename, sizeof (filename), "Instrument %02d.signed 8bit.sample", i+1);
					fd = open (filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
					if (fd < 0)
					{
						printf ("%sWARNING: Unable to open %s%s\n", FONT_BRIGHT_YELLOW, filename, FONT_RESET);
					} else {
						write (fd, mem + Instruments[i].offset, Instruments[i].length);
						close (fd);
						printf ("Saved %s\n", filename);
					}
				}
			}
		}
	}

	return 0;
}

#endif

int ParseMOD (unsigned char *mem, int len, int instruments, int channels, int signature)
{
	int i, MaxOrder;
	uint8_t NoOrders;
	int ofs = 0;

	if (DumpPrefix (mem, len, 0, 20)) return -1;
	printf ("Name: \"");
	for (i=0; i < 20; i++)
	{
		if (!mem[0x00+i])
			break;
		printf ("%c", (char)mem[0x00+i]);
	}
	ofs = 20;
	printf ("\"\n");

	for (i=0; i < instruments; i++)
	{
		if (DumpInstrument(mem, len, ofs, i)) return -1;
		ofs += 30;
	}

	if (DumpPrefix (mem, len, ofs, 1)) return -1;
	NoOrders = mem[ofs++];
	if ((NoOrders < 1) || (NoOrders > 128))
	{
		printf ("Number of orders: %d %s(OUT OF RANGE)%s\n", (int)NoOrders, FONT_BRIGHT_RED, FONT_RESET);
	} else {
		printf ("Number of orders: %d\n", (int)NoOrders);
	}

	if (DumpPrefix (mem, len, ofs, 1)) return -1;
	printf ("(Restart position: %d)\n", (int)mem[ofs++]);

	MaxOrder = DumpOrders(mem, len, ofs, NoOrders);
	if (MaxOrder < 0) return -1;
	ofs += 128;

	if (signature)
	{
		if (DumpPrefix (mem, len, ofs, 4)) return -1;
		printf ("Signtaure: \"%c%c%c%c\"\n", mem[ofs+0], mem[ofs+1], mem[ofs+2], mem[ofs+3]);
		ofs+=signature;
	}

	if (DumpPatterns(mem, len, ofs, channels, MaxOrder + 1)) return -1;

	ofs += channels * 256;

#if 0
	if (DumpSamples(mem, len, ofs, instruments)) return -1;
#endif

	return 0;
}

int preParseMOD15 (unsigned char *mem, int len, int *channels15instruments)
{
	int i;
	uint32_t samplelengths15 = 0;
	int canbe15instruments = 1;
	uint8_t highestorder = 0;

	if (len < (20 + 15*30))
	{
		printf ("%sERROR: no space for even 15 instruments, the minimum%s\n", FONT_BRIGHT_RED, FONT_RESET);
		return -2;
	}

	for (i=0; i < 15; i++)
	{
		uint16_t samplelength = (mem[20+22+i*30] << 8) | mem[20+23+i*30];
		uint8_t  finetune = mem[20+24+i*30];
		uint8_t  volume = mem[20+25+i*30];
		uint16_t repeatstart = (mem[20+26+i*30] << 8) | mem[20+27+i*30];
		uint16_t repeatlen = (mem[20+28+i*30] << 8) | mem[20+29+i*30];

		if (finetune & 0xf0)
		{
			printf ("%sWARNING: instrument %d have high nibble in finetuning set%s\n", FONT_BRIGHT_YELLOW, i + 1, FONT_RESET);
		}
		if (volume > 64)
		{
			printf ("%sERROR: instrument %d volume is out of range%s\n", FONT_BRIGHT_RED, i + 1, FONT_RESET);
			return -2;
		}
		if (samplelength <= 1)
		{
			if (repeatlen > 1)
			{
				printf ("%sERROR: instrument %d looplen is out of range for empty instrument%s\n", FONT_BRIGHT_RED, i + 1, FONT_RESET);
				return -2;
			}
		} else {
			if (repeatstart >= samplelength)
			{
				printf ("%sERROR: instrument %d loopstart is out of range%s\n", FONT_BRIGHT_RED, i + 1, FONT_RESET);
				return -2;
			}
			if (repeatlen > samplelength)
			{
				printf ("%sERROR: instrument %d looplen is out of range%s\n", FONT_BRIGHT_RED, i + 1, FONT_RESET);
				return -2;
			}
			if (((uint32_t)repeatstart + repeatlen) > samplelength)
			{
				printf ("%sERROR: instrument %d loopstart + looplen is out of range%s\n", FONT_BRIGHT_RED, i + 1, FONT_RESET);
				return -2;
			}
			samplelengths15 += samplelength;
		}
		//printf ("15.Instrument: len=%04x loopstart=%04x looplen=%04x volume=%02x finetune=%d\n", samplelength, repeatstart, repeatlen, volume, finetune);
	}


	if (len < (20 + 15*30 + 130))
	{
		printf ("%sERROR: No room for order data - can not be 15 instrument format%s\n", FONT_BRIGHT_RED, FONT_RESET);
		return -2;
	}

	if ((mem[20+15*30] < 1) || (mem[20+15*30] > 128))
	{
		printf ("%sWARNING: Order length (%d) is out of range - can not be 15 instrument format%s\n", FONT_BRIGHT_YELLOW, mem[20+22+15*30], FONT_RESET);
		return -1;
	}

	for (i=0; i < 128; i++)
	{
		if (mem[20+15*30+2 + i] > highestorder)
		{
			highestorder = mem[20+15*30+2 + i];
		}
	}
	if (highestorder >= 64)
	{
		printf ("%sWARNING: Order > 63 not possible in 15 instrument format - can not be 15 instrument file%s\n", FONT_BRIGHT_YELLOW, FONT_RESET);
		return -1;
	}

	if ((20+15*30+2+128+4 + (highestorder+1)*1024) > len)
	{
		printf ("%sERROR: Not enough space for pattern data - can not be a 15 instrument file%s\n", FONT_BRIGHT_RED, FONT_RESET);
		return -1;
	}

	if ((20+15*30+2+128+4 + (highestorder+1)*1024 + (samplelengths15<<1)) > len)
	{
		printf ("%sERROR: Not enough space for sample data - can not be a 15 instrument file%s\n", FONT_BRIGHT_RED, FONT_RESET);
		return -1;
	}

	if ((20+15*30+2+128+4 + (highestorder+1)*1024 + (samplelengths15<<1)) != len)
	{
		printf ("%sWARNING: Got %d bytes extra data for 15 instrument file%s\n", FONT_BRIGHT_YELLOW, len - (20+15*30+2+128+4 + (highestorder+1)*1024 + (samplelengths15<<1)), FONT_RESET);
	}

	return canbe15instruments;
}

int preParseMOD31 (unsigned char *mem, int len, int *channels31instruments)
{
	int i;
	uint32_t samplelengths31 = 0;
	int canbe31instruments = 0;
	uint8_t highestorder = 0;

	if (len < (20 + 31*30 + 130 + 4))
	{
		printf ("%sERROR: no space for even 31 instruments, the minimum%s\n", FONT_BRIGHT_RED, FONT_RESET);
		return -1;
	}

	for (i=0; i < 31; i++)
	{
		uint16_t samplelength = (mem[20+22+i*30] << 8) | mem[20+23+i*30];
		uint8_t  finetune = mem[20+24+i*30];
		uint8_t  volume = mem[20+25+i*30];
		uint16_t repeatstart = (mem[20+26+i*30] << 8) | mem[20+27+i*30];
		uint16_t repeatlen = (mem[20+28+i*30] << 8) | mem[20+29+i*30];

		if (finetune & 0xf0)
		{
			printf ("%sWARNING: instrument %d have high nibble in finetuning set%s\n", FONT_BRIGHT_YELLOW, i + 1, FONT_RESET);
		}
		if (volume > 64)
		{
			printf ("%sWARNING: instrument %d volume is out of range - can not be 31 instrument file%s\n", FONT_BRIGHT_YELLOW, i + 1, FONT_RESET);
			return -1;
		}
		if (samplelength <= 1)
		{
			if (repeatlen > 1)
			{
				printf ("%sWARNING: instrument %d looplen is out of range for empty instrument - can not be 31 instrument file%s\n", FONT_BRIGHT_YELLOW, i + 1, FONT_RESET);
				return -1;
			}
		} else {
			if (repeatstart >= samplelength)
			{
				printf ("%sWARNING: instrument %d loopstart is out of range - can not be 31 instrument file%s\n", FONT_BRIGHT_YELLOW, i + 1, FONT_RESET);
				return -1;
			}
			if (repeatlen > samplelength)
			{
				printf ("%sWARNING: instrument %d looplen is out of range - can not be 31 instrument file%s\n", FONT_BRIGHT_YELLOW, i + 1, FONT_RESET);
				return -1;
			}
			if (((uint32_t)repeatstart + repeatlen) > samplelength)
			{
				printf ("%sWARNING: instrument %d loopstart + looplen is out of range - can not be 31 instrument file%s\n", FONT_BRIGHT_YELLOW, i + 1, FONT_RESET);
				return -1;
			}
			samplelengths31 += samplelength;
		}
		//printf ("31.Instrument: len=%04x loopstart=%04x looplen=%04x volume=%02x finetune=%d\n", samplelength, repeatstart, repeatlen, volume, finetune);
	}

	if ((mem[20+31*30] < 1) || (mem[20+31*30] > 128))
	{
		printf ("%sWARNING: Order length (%d) is out of range - can not be 31 instrument format%s\n", FONT_BRIGHT_YELLOW, mem[20+31*30], FONT_RESET);

		return -1;
	}
	for (i=0; i < 128; i++)
	{
		if (mem[20+31*30+2 + i] > highestorder)
		{
			highestorder = mem[20+31*30+2 + i];
		}
	}

	if (!memcmp(&mem[20+31*30+2+128], "M.K.", 4))
	{
		printf ("%sNOTE: got M.K. signature in 31 instrument format%s\n", FONT_BRIGHT_GREEN, FONT_RESET);
		canbe31instruments = 4;
		if (highestorder >= 64)
		{
			printf ("%sWARNING: Order > 63 not possible in M.K. format - can not be 31 instrument file%s\n", FONT_BRIGHT_YELLOW, FONT_RESET);
			return -1;
		}
	} else if (!memcmp(&mem[20+31*30+2+128], "M!K!", 4))
	{
		printf ("%sNOTE: got M!K! signature in 31 instrument format%s\n", FONT_BRIGHT_GREEN, FONT_RESET);
		canbe31instruments = 4;
		if (highestorder < 64)
		{
			printf ("%sWARNING: Order < 64 not possible in M!K! format - can not be 31 instrument file%s\n", FONT_BRIGHT_YELLOW, FONT_RESET);
			return -1;
		}
	} else if (!memcmp(&mem[20+31*30+2+128], "M&K!", 4))
	{
		printf ("%sNOTE: got M&K! signature in 31 instrument format (His Master's Noise)%s\n", FONT_BRIGHT_GREEN, FONT_RESET);
		canbe31instruments = 4;
		if (highestorder >= 64)
		{
			printf ("%sWARNING: Order > 63 not possible in M&K! format - can not be 31 instrument file%s\n", FONT_BRIGHT_YELLOW, FONT_RESET);
			return -1;
		}
	} else if (!memcmp(&mem[20+31*30+2+128], "FLT4", 4)) { printf ("%sNOTE: got FLT4 - probably StarTrekker 4-channel MOD%s\n",   FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 4;
	} else if (!memcmp(&mem[20+31*30+2+128], "FLT8", 4)) { printf ("%sNOTE: got FLT8 - probably StarTrekker 8-channel MOD%s\n",   FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 8;
	} else if (!memcmp(&mem[20+31*30+2+128], "CD81", 4)) { printf ("%sNOTE: got CD81 - probably Oktalyzer for Atari ST?%s\n",     FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 8;
	} else if (!memcmp(&mem[20+31*30+2+128], "OKTA", 4)) { printf ("%sNOTE: got OKTA - probably Oktalyzer for Atari ST?%s\n",     FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 8;
	} else if (!memcmp(&mem[20+31*30+2+128], "OCTA", 4)) { printf ("%sNOTE: got OKTA - probably OctaMED%s\n",                     FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 8;
	} else if (!memcmp(&mem[20+31*30+2+128], "FA08", 4)) { printf ("%sNOTE: got FA08 - probably Digital Tracker (MOD)%s\n",       FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 8 : 0; *channels31instruments = 8;
	} else if (!memcmp(&mem[20+31*30+2+128], "FA06", 4)) { printf ("%sNOTE: got FA06 - probably Digital Tracker (MOD)%s\n",       FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 8 : 0; *channels31instruments = 6;
	} else if (!memcmp(&mem[20+31*30+2+128], "FA04", 4)) { printf ("%sNOTE: got FA04 - probably Digital Tracker (MOD)%s\n",       FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 8 : 0; *channels31instruments = 4;

	} else if (!memcmp(&mem[20+31*30+2+128], "1TDZ", 4)) { printf ("%sNOTE: got 1TDZ - TakeTracker extension for 1 channel%s\n",  FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 1;
	} else if (!memcmp(&mem[20+31*30+2+128], "2TDZ", 4)) { printf ("%sNOTE: got 2TDZ - TakeTracker extension for 3 channel%s\n",  FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 2;
	} else if (!memcmp(&mem[20+31*30+2+128], "3TDZ", 4)) { printf ("%sNOTE: got 3TDZ - TakeTracker extension for 3 channel%s\n",  FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 3;
	} else if (!memcmp(&mem[20+31*30+2+128], "5CHN", 4)) { printf ("%sNOTE: got 5CHN - TakeTracker extension for 5 channel%s\n",  FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 5;
	} else if (!memcmp(&mem[20+31*30+2+128], "7CHN", 4)) { printf ("%sNOTE: got 7CHN - TakeTracker extension for 7 channel%s\n",  FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 7;
	} else if (!memcmp(&mem[20+31*30+2+128], "9CHN", 4)) { printf ("%sNOTE: got 9CHN - TakeTracker extension for 9 channel%s\n",  FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 9;
	} else if (!memcmp(&mem[20+31*30+2+128], "10CN", 4)) { printf ("%sNOTE: got 10CN - TakeTracker extension for 10 channel%s\n", FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 10;
	} else if (!memcmp(&mem[20+31*30+2+128], "11CN", 4)) { printf ("%sNOTE: got 11CN - TakeTracker extension for 11 channel%s\n", FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 11;
	} else if (!memcmp(&mem[20+31*30+2+128], "12CN", 4)) { printf ("%sNOTE: got 12CN - TakeTracker extension for 12 channel%s\n", FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 12;
	} else if (!memcmp(&mem[20+31*30+2+128], "13CN", 4)) { printf ("%sNOTE: got 13CN - TakeTracker extension for 13 channel%s\n", FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 13;
	} else if (!memcmp(&mem[20+31*30+2+128], "14CN", 4)) { printf ("%sNOTE: got 14CN - TakeTracker extension for 14 channel%s\n", FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 14;
	} else if (!memcmp(&mem[20+31*30+2+128], "15CN", 4)) { printf ("%sNOTE: got 15CN - TakeTracker extension for 15 channel%s\n", FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 15;
	} else if (!memcmp(&mem[20+31*30+2+128], "16CN", 4)) { printf ("%sNOTE: got 16CN - TakeTracker extension for 16 channel%s\n", FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 16;
	} else if (!memcmp(&mem[20+31*30+2+128], "17CN", 4)) { printf ("%sNOTE: got 17CN - TakeTracker extension for 17 channel%s\n", FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 17;
	} else if (!memcmp(&mem[20+31*30+2+128], "18CN", 4)) { printf ("%sNOTE: got 18CN - TakeTracker extension for 18 channel%s\n", FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 18;
	} else if (!memcmp(&mem[20+31*30+2+128], "19CN", 4)) { printf ("%sNOTE: got 19CN - TakeTracker extension for 19 channel%s\n", FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 19;
	} else if (!memcmp(&mem[20+31*30+2+128], "20CN", 4)) { printf ("%sNOTE: got 20CN - TakeTracker extension for 20 channel%s\n", FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 20;
	} else if (!memcmp(&mem[20+31*30+2+128], "21CN", 4)) { printf ("%sNOTE: got 21CN - TakeTracker extension for 21 channel%s\n", FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 21;
	} else if (!memcmp(&mem[20+31*30+2+128], "22CN", 4)) { printf ("%sNOTE: got 22CN - TakeTracker extension for 22 channel%s\n", FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 22;
	} else if (!memcmp(&mem[20+31*30+2+128], "23CN", 4)) { printf ("%sNOTE: got 23CN - TakeTracker extension for 23 channel%s\n", FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 23;
	} else if (!memcmp(&mem[20+31*30+2+128], "24CN", 4)) { printf ("%sNOTE: got 24CN - TakeTracker extension for 24 channel%s\n", FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 24;
	} else if (!memcmp(&mem[20+31*30+2+128], "25CN", 4)) { printf ("%sNOTE: got 25CN - TakeTracker extension for 25 channel%s\n", FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 25;
	} else if (!memcmp(&mem[20+31*30+2+128], "26CN", 4)) { printf ("%sNOTE: got 26CN - TakeTracker extension for 26 channel%s\n", FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 26;
	} else if (!memcmp(&mem[20+31*30+2+128], "27CN", 4)) { printf ("%sNOTE: got 27CN - TakeTracker extension for 27 channel%s\n", FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 27;
	} else if (!memcmp(&mem[20+31*30+2+128], "28CN", 4)) { printf ("%sNOTE: got 28CN - TakeTracker extension for 28 channel%s\n", FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 28;
	} else if (!memcmp(&mem[20+31*30+2+128], "29CN", 4)) { printf ("%sNOTE: got 29CN - TakeTracker extension for 29 channel%s\n", FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 29;
	} else if (!memcmp(&mem[20+31*30+2+128], "30CN", 4)) { printf ("%sNOTE: got 30CN - TakeTracker extension for 30 channel%s\n", FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 30;
	} else if (!memcmp(&mem[20+31*30+2+128], "31CN", 4)) { printf ("%sNOTE: got 31CN - TakeTracker extension for 31 channel%s\n", FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 31;
	} else if (!memcmp(&mem[20+31*30+2+128], "33CN", 4)) { printf ("%sNOTE: got 32CN - TakeTracker extension for 32 channel%s\n", FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 32;

	} else if (!memcmp(&mem[20+31*30+2+128], "2CHN", 4)) { printf ("%sNOTE: got 2CHN - uncommon 2 channel format%s\n",            FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 2;
	} else if (!memcmp(&mem[20+31*30+2+128], "6CHN", 4)) { printf ("%sNOTE: got 6CHN - common 6 channel format%s\n",              FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 6;
	} else if (!memcmp(&mem[20+31*30+2+128], "8CHN", 4)) { printf ("%sNOTE: got 8CHN - common 8 channel format%s\n",              FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 8;
	} else if (!memcmp(&mem[20+31*30+2+128], "10CH", 4)) { printf ("%sNOTE: got 10CH - common 10 channel format%s\n",             FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 10;
	} else if (!memcmp(&mem[20+31*30+2+128], "12CH", 4)) { printf ("%sNOTE: got 12CH - common 10 channel format%s\n",             FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 12;
	} else if (!memcmp(&mem[20+31*30+2+128], "14CH", 4)) { printf ("%sNOTE: got 14CH - common 10 channel format%s\n",             FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 14;
	} else if (!memcmp(&mem[20+31*30+2+128], "16CH", 4)) { printf ("%sNOTE: got 16CH - common 10 channel format%s\n",             FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 16;
	} else if (!memcmp(&mem[20+31*30+2+128], "18CH", 4)) { printf ("%sNOTE: got 18CH - common 10 channel format%s\n",             FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 18;
	} else if (!memcmp(&mem[20+31*30+2+128], "20CH", 4)) { printf ("%sNOTE: got 20CH - common 10 channel format%s\n",             FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 20;
	} else if (!memcmp(&mem[20+31*30+2+128], "22CH", 4)) { printf ("%sNOTE: got 22CH - common 10 channel format%s\n",             FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 22;
	} else if (!memcmp(&mem[20+31*30+2+128], "24CH", 4)) { printf ("%sNOTE: got 24CH - common 10 channel format%s\n",             FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 24;
	} else if (!memcmp(&mem[20+31*30+2+128], "26CH", 4)) { printf ("%sNOTE: got 26CH - common 10 channel format%s\n",             FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 26;
	} else if (!memcmp(&mem[20+31*30+2+128], "28CH", 4)) { printf ("%sNOTE: got 28CH - common 10 channel format%s\n",             FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 28;
	} else if (!memcmp(&mem[20+31*30+2+128], "30CH", 4)) { printf ("%sNOTE: got 30CH - common 10 channel format%s\n",             FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 30;
	} else if (!memcmp(&mem[20+31*30+2+128], "32CH", 4)) { printf ("%sNOTE: got 32CH - common 10 channel format%s\n",             FONT_BRIGHT_GREEN, FONT_RESET); canbe31instruments = (highestorder < 64) ? 4 : 0; *channels31instruments = 32;
	} else {
		if (
                    (((mem[20+31*30+2+128] >= 'A') && (mem[20+31*30+2+128] <= 'Z')) ||
		     ((mem[20+31*30+2+128] >= '0') && (mem[20+31*30+2+128] <= '9')) ||
		      (mem[20+31*30+2+128] == '.')                                     ||
		      (mem[20+31*30+2+128] == '!')                                     ||
		      (mem[20+31*30+2+128] == '#')) &&
                    (((mem[20+31*30+2+129] >= 'A') && (mem[20+31*30+2+129] <= 'Z')) ||
		     ((mem[20+31*30+2+129] >= '0') && (mem[20+31*30+2+129] <= '9')) ||
		      (mem[20+31*30+2+129] == '.')                                     ||
		      (mem[20+31*30+2+129] == '!')                                     ||
		      (mem[20+31*30+2+129] == '#')) &&
                    (((mem[20+31*30+2+130] >= 'A') && (mem[20+31*30+2+130] <= 'Z')) ||
		     ((mem[20+31*30+2+130] >= '0') && (mem[20+31*30+2+130] <= '9')) ||
		      (mem[20+31*30+2+130] == '.')                                     ||
		      (mem[20+31*30+2+130] == '!')                                     ||
		      (mem[20+31*30+2+130] == '#')) &&
                    (((mem[20+31*30+2+131] >= 'A') && (mem[20+31*30+2+131] <= 'Z')) ||
		     ((mem[20+31*30+2+131] >= '0') && (mem[20+31*30+2+131] <= '9')) ||
		      (mem[20+31*30+2+131] == '.')                                     ||
		      (mem[20+31*30+2+131] == '!')                                     ||
		      (mem[20+31*30+2+131] == '#')))
		{
			printf ("%sWARNING: Unknown TAG: \"%c%c%c%c\"%s\n", FONT_BRIGHT_YELLOW, mem[20+31*30+2+128], mem[20+31*30+2+129], mem[20+31*30+2+130], mem[20+31*30+2+131], FONT_RESET);
			*channels31instruments = 4;
			canbe31instruments = 0;
		} else {
			printf ("%sERROR: Illegal TAG - can not be a 31 instrument file%s\n", FONT_BRIGHT_RED, FONT_RESET);
		}
	}

	if ((20+31*30+2+128+4+ (highestorder+1)*1024) > len)
	{
		printf ("%sERROR: Not enough space for pattern data - can not be a 31 instrument file%s\n", FONT_BRIGHT_RED, FONT_RESET);
		return -1;
	}

	if ((20+31*30+2+128+4 + (highestorder+1)*1024 + (samplelengths31<<1)) > len)
	{
		printf ("%sERROR: Not enough space for sample data - can not be a 31 instrument file%s\n", FONT_BRIGHT_RED, FONT_RESET);
		return -1;
	}

	if ((20+31*30+2+128+4 + (highestorder+1)*1024 + (samplelengths31<<1)) != len)
	{
		printf ("%sWARNING: Got %d bytes extra data for 31 instrument file%s\n", FONT_BRIGHT_YELLOW, len - (20+31*30+2+128+4 + (highestorder+1)*1024 + (samplelengths31<<1)), FONT_RESET);
	}

	return canbe31instruments;
}


int preParseMOD (unsigned char *mem, int len, int *instruments, int *channels, int *signature)
{
	int i;

	int channels15instruments = 4;
	int channels31instruments = 4;
	int canbe15instruments;
	int canbe31instruments;

	if (len < 20)
	{
		printf ("%sERROR: no space for title%s\n", FONT_BRIGHT_RED, FONT_RESET);
		return -1;
	}
	do
	{
		int invalid = 0;
		int upper = 0;
		int lower = 0;
		int other = 0;
		int hi = 0;
		for (i=0; i < 20; i++)
		{
			if (mem[i] == 0)
			{
				break;
			}
			if ((mem[i] < 0x20) || ((mem[i] >= 0x7f) && (mem[i] <= 0x9f)))
			{
				invalid++;
			} else if ((mem[i] >= 'A') && (mem[i] <= 'Z'))
			{
				upper++;
			} else if ((mem[i] >= 'a') && (mem[i] <= 'z'))
			{
				lower++;
			} else if (!(mem[i] & 0x80))
			{
				other++;
			} else {
				hi++;
			}
		}
		if (invalid)
		{
			printf ("Title: contains invalid characters\n");
		} else if (hi || lower)
		{
			printf ("Title: Can not be \"Original Protracker\"\n");
		} else if (lower)
		{
			printf ("Title: Might be \"Original Protracker\"\n");
		}
	} while (0);

	canbe15instruments = preParseMOD15 (mem, len, &channels15instruments);
	if (canbe15instruments < -1)
	{
		return -1;
	}
	canbe31instruments = preParseMOD31 (mem, len, &channels31instruments);

	if (canbe31instruments>0)
	{
		printf ("%sINFO: File is 31 instruments, %d channels%s\n", FONT_BRIGHT_GREEN, channels31instruments, FONT_RESET);
		*instruments = 31; *channels = channels31instruments; *signature = canbe31instruments;
		return 0;
	}
	if (canbe15instruments>0)
	{
		printf ("%sINFO: File is 15 instruments, %d channels%s\n", FONT_BRIGHT_GREEN, channels15instruments, FONT_RESET);
		*instruments = 15; *channels = channels15instruments; *signature = 0;
		return 0;
	}
	if (canbe31instruments==0)
	{
		printf ("%sINFO: File is probably 31 instruments, %d channels%s\n", FONT_BRIGHT_GREEN, channels31instruments, FONT_RESET);
		*instruments = 31; *channels = channels31instruments; *signature = 4;
		return 0;
	}
	if (canbe15instruments==0)
	{
		printf ("%sINFO: File is probably 15 instruments, %d channels%s\n", FONT_BRIGHT_GREEN, channels15instruments, FONT_RESET);
		*instruments = 15; *channels = channels15instruments; *signature = 0;
		return 0;
	}

	return -1;
}

#warning TODO N.T. files!

int main(int argc, char *argv[])
{
	struct stat st;
	size_t ps = sysconf(_SC_PAGE_SIZE);
	int fd;
	size_t data_mmaped_len;
	unsigned char *data;
	int c;
	char *color = "auto";
	int help = 0;

	int instruments = 31;
	int channels = 4;
	int signature = 1;

	while (1)
	{
		int option_index = 0;
		static struct option long_options[] =
		{
			{"color",        optional_argument, 0, 0},
			{"help",         no_argument,       0, 'h'},
			{"savepatterns", no_argument,       0, 'p'},
			{"savesamples",  no_argument,       0, 's'},
			{0,              0,                 0, 0}
		};

		c = getopt_long(argc, argv, "hsp", long_options, &option_index);
		if (c == -1)
			break;

		switch (c)
		{
			case 0:
				if (option_index == 0)
				{
					color = optarg;
				}
				break;
			case 'p':
				if (!savepatterns)
				{
					savepatterns = fopen ("patterns.txt", "w");
					if (!savepatterns)
					{
						fprintf (stderr, "Unable to open patterns.txt for writing: %s\n", strerror (errno));
						return 1;
					}
				}
				break;
			case 's':
				savesamples = 1;
				break;
			case 'h':
				help = 1;
				break;
			case '?':
				help = 3;
				break;
			default:
				printf("?? getopt returned character code 0%o ??\n", c);
		}
	}

	if (optind != (argc-1))
	{
		help = 4;
	}

	if (!color)
	{
		usecolor = 1;
	} else if (!strcmp (color, "auto"))
	{
		usecolor = isatty ( 1 );
	} else if ((strcmp (color, "never")) && (strcmp (color, "no")))
	{
		usecolor = 1;
	} else {
		usecolor = 0;
	}

	if (help)
	{
		fprintf (stderr, "Usage:\n%s [--color=auto/never/on] [--savesamples -s] [--savepatterns -p] [--help] file.mod  (%d)\n", argv[0], help);
		return 1;
	}

	fd = open (argv[optind], O_RDONLY);
	if (fd < 0)
	{
		perror ("open()");
		return 0;
	}
	if (fstat(fd, &st))
	{
		perror("fstat()");
		close (fd);
		return 0;
	}
	if (!st.st_size)
	{
		fprintf (stderr, "Zero-size file\n");
		close (fd);
		return 0;
	}

//	s.data_len = st.st_size;
	data_mmaped_len = roundup (st.st_size, ps);
	data = mmap (0, data_mmaped_len, PROT_READ|PROT_WRITE, MAP_FILE|MAP_PRIVATE, fd, 0);

	if (data == MAP_FAILED)
	{
		perror ("mmap() failed");
		close (fd);
		return 0;
	}

	if (usecolor)
	{
		FONT_RESET         = "\033[0m";
		FONT_BRIGHT_BLACK  = "\033[30;1m";
		FONT_BRIGHT_RED    = "\033[31;1m";
		FONT_BRIGHT_GREEN  = "\033[32;1m";
		FONT_BRIGHT_YELLOW = "\033[33;1m";
		FONT_BRIGHT_BLUE   = "\033[34;1m";
		FONT_BRIGHT_PURPLE = "\033[35;1m";
		FONT_BRIGHT_CYAN   = "\033[36;1m";
	}

	if (preParseMOD (data, st.st_size, &instruments, &channels, &signature))
	{
		goto failed;
	}
	ParseMOD (data, st.st_size, instruments, channels, signature);

failed:
	munmap (data, data_mmaped_len);
	close (fd);
	return 0;
}
