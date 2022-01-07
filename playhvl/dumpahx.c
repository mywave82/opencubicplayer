/* OpenCP Module Player
 * copyright (c) 2019-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Utility: Dumping the raw contents of a AHX file
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

char *FONT_RESET = "";
char *FONT_BRIGHT_BLACK = "";
char *FONT_BRIGHT_RED = "";
char *FONT_BRIGHT_GREEN = "";
char *FONT_BRIGHT_YELLOW = "";
char *FONT_BRIGHT_BLUE = "";
char *FONT_BRIGHT_PURPLE = "";
char *FONT_BRIGHT_CYAN = "";

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
	return retval;
}

uint8_t SMP;	// Number of samples

uint8_t SS;     // SubSongs
uint16_t LEN;   // Number of orders (each order has 4 track references with transpose tagged to it)
uint8_t TRK;    // Number of tracks
uint8_t TRL;    // Length of each track / rows

int HaveTrack0; // True if Track 0 is stored in the file

int DumpHeader (unsigned char *mem, int len)
{
	int i;
	int isAHX1 = 0;
	uint16_t RES;

	printf ("[%sHEADER%s]\n", FONT_BRIGHT_CYAN, FONT_RESET);

	if (len < 14)
	{
		fprintf (stderr, "%sERROR: len < sizeof(FileHeader)%s\n", FONT_BRIGHT_RED, FONT_RESET);
		return 1;
	}

	/* hdr.name */
	DumpPrefix (mem, len, 0x00, 4);
	printf ("Signature: \"");
	for (i=0; i < 4; i++)
	{
		if ((mem[0x00+i] & 0xc0) == 0x40)
		{
			printf ("%c", (char)mem[0x00+i]);
		} else if (mem[0x00+i] < 9)
		{
			printf ("\\%d", mem[0x00+i]);
		} else {
			printf ("%s\\x%02x%s", FONT_BRIGHT_RED, mem[0x00+i], FONT_RESET);
		}
	}
	printf ("\" - ");
	if (!memcmp(mem + 0x00, "THX\0", 4))
	{
		printf ("%sAHX version 0.00 - 1.27%s\n", FONT_BRIGHT_GREEN, FONT_RESET);
	} else if (!memcmp (mem + 0x00, "THX\1", 4))
	{
		printf ("%sAHX version >= 2.00%s\n", FONT_BRIGHT_GREEN, FONT_RESET);
		isAHX1 = 1;
	} else {
		printf ("%sUnknown%s\n", FONT_BRIGHT_RED, FONT_RESET);
	}

	DumpPrefix (mem, len, 0x04, 2);
	printf ("Titles and Sample names is stored at ptr [%s0xXXXX%04x%s]\n", FONT_BRIGHT_BLUE, (mem[0x04]<<8)|mem[0x05], FONT_RESET);

	DumpPrefix (mem, len, 0x06, 1);
	HaveTrack0 = !(mem[0x06] & 0x80);
	printf ("Track 0 present: %d\n", HaveTrack0);
	if (isAHX1)
	{
		printf ("                    Speed: ");
		switch (mem[0x06] & 0x70)
		{
			case 0x00: printf ("50Hz\n"); break;
			case 0x10: printf ("100Hz\n"); break;
			case 0x20: printf ("150Hz\n"); break;
			case 0x30: printf ("200Hz\n"); break;
			default: printf ("%sInvalid value%s\n", FONT_BRIGHT_RED, FONT_RESET); break;
		}
	}

	DumpPrefix (mem, len, 0x07, 1);
	LEN = ((mem[0x06] << 8) | mem[0x07]) & 0xfff;
	if ((LEN >= 1) && (LEN <= 999))
	{
		printf ("LEN (number of positions): %d    # Orders\n", LEN);
	} else {
		printf ("LEN (number of positions): %s%d OUT OF RANGE%s    # Orders\n", FONT_BRIGHT_RED, LEN, FONT_RESET);
	}

	DumpPrefix (mem, len, 0x08, 2);
	RES = ((mem[0x08] << 8) | mem[0x09]);
	if (RES < LEN)
	{
		printf ("RES (loop position): %d\n", LEN);
	} else {
		printf ("RES (loop position): %s%d OUT OF RANGE%s\n", FONT_BRIGHT_RED, LEN, FONT_RESET);
	}

	DumpPrefix (mem, len, 0x0a, 1);
	TRL = mem[0x0a];
	if ((TRL >= 1) && (TRL <= 64))
	{
		printf ("TRL (track length): %d    # Pattern Length\n", TRL);
	} else {
		printf ("TRL (track length): %s%d OUT OF RANGE    # Pattern Length%s\n", FONT_BRIGHT_RED, TRL, FONT_RESET);
	}

	DumpPrefix (mem, len, 0x0b, 1);
	TRK = mem[0x0b];
	printf ("TRK (number of tracks saved): %d    # Patterns\n", (int)TRK);

	DumpPrefix (mem, len, 0x0c, 1);
	SMP = mem[0x0c];
	if (SMP <= 63)
	{
		printf ("SMP (samples stored): %d\n", SMP);
	} else {
		printf ("SMP (samples stored): %s%d OUT OF RANGE%s\n", FONT_BRIGHT_RED, SMP, FONT_RESET);
	}

	DumpPrefix (mem, len, 0x0d, 1);
	SS = mem[0x0d];
	printf ("SS (number of subsongs): %d\n", SS);

	return 0;
}

int DumpSubSongs (unsigned char *mem, int offset, int len)
{
	int i;
	printf ("[%sSUBSONGS%s]\n", FONT_BRIGHT_CYAN, FONT_RESET);
	for (i=0; i < SS; i++)
	{
		uint16_t SL;
		DumpPrefix (mem, len, offset + (i<<1), 2);
		if (len < offset + (i<<1) + 2)
		{
			fprintf (stderr, "\n%sERROR: ran out of data%s\n", FONT_BRIGHT_RED, FONT_RESET);
			return 1;
		}
		SL = ((mem[offset+(i<<1)] << 8) | mem[offset+(i<<1)+1]);

		if (SL >= LEN)
		{
			printf ("SubSong %d: Starts at position %s0x%04x  # OUT OF RANGE, must be <LEN%s\n", i, FONT_BRIGHT_RED, SL, FONT_RESET);
		} else {
			printf ("SubSong %d: Starts at position 0x%04x\n", i, SL);
		}

	}
	return 0;
}

int DumpPositions (unsigned char *mem, int offset, int len)
{
	int i, j;
	printf ("[%sPOSITIONS (Orders)%s]\n", FONT_BRIGHT_CYAN, FONT_RESET);
	for (i=0; i < LEN; i++)
	{
		int error = 0;

		if (DumpPrefix (mem, len, offset + (i<<3), 8))
		{
			fprintf (stderr, "\n%sERROR: ran out of data%s\n", FONT_BRIGHT_RED, FONT_RESET);
			return 1;
		}

		printf ("Position %3d: ", i);
		for (j=0; j < 4; j++)
		{
			uint8_t Track     = mem[offset + (i<<3) + (j<<1)];
			int8_t  Transpose = mem[offset + (i<<3) + (j<<1) + 1];
			if (Track > TRK)
			{
				error = 1;
				printf (" | %s%3d%s transpose %-4d", FONT_BRIGHT_RED, Track, FONT_RESET, Transpose);
			} else {
				printf (" | %3d transpose %-4d", Track, Transpose);
			}
		}
		if (error)
		{
			printf (" | %s# TRACK REFERENCE OUT OF RANGE%s\n", FONT_BRIGHT_RED, FONT_RESET);
		} else {
			printf (" |\n");
		}
	}
	return 0;
}

void print_note (uint8_t note)
{
	char *N[12] = {"C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"};
	if (!note)
	{
		printf("...");
		return;
	}
	if (note >= 61)
	{
		printf("%s???%s", FONT_BRIGHT_RED, FONT_RESET);
		return;
	}
	note--;
	printf ("%s%d", N[note%12], (note / 12)+1);
}

void print_sample (uint8_t sample)
{
	if (!sample)
	{
		printf ("..");
		return;
	}
	printf ("%02d", sample);
}

/* Commands
    0 00 no-op
    0 .x Position jump hi
    1 xx Portamento up (period slide down)
    2 xx Portamento down
    3 xx Tone portamento
    4 xx Filter override
    5 xx Volume Slide + Tone Portamento
    6 ????????????
    7 xx Panning
    8 ????????????
    9 xx Set squarewave offset
    A xx Volume Slide
    B xx Position jump
    C xx Volume
    D xx Pattern Break
    E 0x ?????????????
    E 1x Fineslide up
    E 2x Fineslide down
    E 3x ?????????????
    E 4x Vibrato control
    E 5x ?????????????
    E 6x ?????????????
    E 7x ?????????????
    E 8x ?????????????
    E 9x ?????????????
    E Ax Fine volume up
    E Bx Fine volume down
    E Cx Note cut
    E Dx Note delay
    E Ex ?????????????
    E Fx Misc flags
    F xx Speed
*/

void print_command (uint8_t cmd, uint8_t info)
{
	if ((cmd == 0) && (info == 0))
	{
		printf ("...");
	} else {
		printf ("%X%02X", cmd, info);
	}
}

int DumpTracks (unsigned char *mem, int offset, int len)
{
	int i, j;
	printf ("[%sTRACKS (Patterns)%s]\n", FONT_BRIGHT_CYAN, FONT_RESET);
	for (i=(HaveTrack0?0:1); i <= TRK; i++)
	{
		printf ("Track: %d\n", i);
		for (j=0; j < TRL; j++)
		{
			uint8_t note;
			uint8_t sample;
			uint8_t command;
			uint8_t data;

			if (DumpPrefix (mem, len, offset, 3))
			{
				fprintf (stderr, "\n%sERROR: ran out of data%s\n", FONT_BRIGHT_RED, FONT_RESET);
				return 1;
			}
			note    =   mem[offset    ] >> 2;
			sample  = ((mem[offset    ] << 4) & 0x30) |
			           (mem[offset + 1] >> 4);
			command =   mem[offset + 1] & 0x0f;
			data    =   mem[offset + 2];

			offset += 3;

			printf ("%02x: ", j);
			print_note (note);
			printf (" ");
			print_sample (sample);
			printf (" ");
			print_command (command, data);
			printf ("\n");
		}
	}
	return 0;
}

int DumpSamples (unsigned char *mem, int *offset, int len)
{
	int i;

	printf ("[%sSAMPLES (Instruments)%s]\n", FONT_BRIGHT_CYAN, FONT_RESET);

	for (i=0; i < SMP; i++)
	{
		uint8_t square_modulation_lower_limit_min = 1;
		uint8_t PLEN;
		int j;

		printf ("Sample: %d\n", i+1);

		if (DumpPrefix (mem, len, *offset, 1)) { fprintf (stderr, "\n%sERROR: ran out of data%s\n", FONT_BRIGHT_RED, FONT_RESET); return 1; }
		if (mem[*offset+0] > 64)
		{
			printf ("Master volume: %s%d # OUT OF RANGE%s\n", FONT_BRIGHT_RED, mem[*offset+0], FONT_RESET);
		} else {
			printf ("Master volume: %d\n", mem[*offset+0]);
		}

		if (DumpPrefix (mem, len, *offset+1, 1)) { fprintf (stderr, "\n%sERROR: ran out of data%s\n", FONT_BRIGHT_RED, FONT_RESET); return 1; }
		if ((mem[*offset+1] & 7) > 5)
		{
			printf ("WaveLen: %s[%d]?? # OUT OF RANGE%s\n", FONT_BRIGHT_RED, 4<<(mem[*offset+1] & 7), FONT_RESET);
		} else {
			printf ("WaveLen: %d\n", 4<<(mem[*offset+1] & 7));
			square_modulation_lower_limit_min = 32 >> (mem[*offset+1] & 7);
		}

		if (DumpPrefix (mem, len, *offset+2, 1)) { fprintf (stderr, "\n%sERROR: ran out of data%s\n", FONT_BRIGHT_RED, FONT_RESET); return 1; }
		if (mem[*offset+2] == 0)
		{
			printf ("Attack Length: %s%d # OUT OF RANGE%s\n", FONT_BRIGHT_RED, mem[*offset+2], FONT_RESET);
		} else {
			printf ("Attack Length: %d\n", mem[*offset+2]);
		}

		if (DumpPrefix (mem, len, *offset+3, 1)) { fprintf (stderr, "\n%sERROR: ran out of data%s\n", FONT_BRIGHT_RED, FONT_RESET); return 1; }
		if (mem[*offset+3] > 64)
		{
			printf ("Attack volume: %s%d # OUT OF RANGE%s\n", FONT_BRIGHT_RED, mem[*offset+3], FONT_RESET);
		} else {
			printf ("Attack volume: %d\n", mem[*offset+3]);
		}

		if (DumpPrefix (mem, len, *offset+4, 1)) { fprintf (stderr, "\n%sERROR: ran out of data%s\n", FONT_BRIGHT_RED, FONT_RESET); return 1; }
		if (mem[*offset+4] == 0)
		{
			printf ("Decay Length: %s%d # OUT OF RANGE%s\n", FONT_BRIGHT_RED, mem[*offset+4], FONT_RESET);
		} else {
			printf ("Decay Length: %d\n", mem[*offset+4]);
		}

		if (DumpPrefix (mem, len, *offset+5, 1)) { fprintf (stderr, "\n%sERROR: ran out of data%s\n", FONT_BRIGHT_RED, FONT_RESET); return 1; }
		if (mem[*offset+5] > 64)
		{
			printf ("Decay volume: %s%d # OUT OF RANGE%s\n", FONT_BRIGHT_RED, mem[*offset+5], FONT_RESET);
		} else {
			printf ("Decay volume: %d\n", mem[*offset+5]);
		}

		if (DumpPrefix (mem, len, *offset+6, 1)) { fprintf (stderr, "\n%sERROR: ran out of data%s\n", FONT_BRIGHT_RED, FONT_RESET); return 1; }
		if (mem[*offset+6] == 0)
		{
			printf ("Sustain Length: %s%d # OUT OF RANGE%s\n", FONT_BRIGHT_RED, mem[*offset+6], FONT_RESET);
		} else {
			printf ("Sustain Length: %d\n", mem[*offset+6]);
		}

		if (DumpPrefix (mem, len, *offset+7, 1)) { fprintf (stderr, "\n%sERROR: ran out of data%s\n", FONT_BRIGHT_RED, FONT_RESET); return 1; }
		if (mem[*offset+7] == 0)
		{
			printf ("Release Length: %s%d # OUT OF RANGE%s\n", FONT_BRIGHT_RED, mem[*offset+7], FONT_RESET);
		} else {
			printf ("Release Length: %d\n", mem[*offset+7]);
		}

		if (DumpPrefix (mem, len, *offset+8, 1)) { fprintf (stderr, "\n%sERROR: ran out of data%s\n", FONT_BRIGHT_RED, FONT_RESET); return 1; }
		if (mem[*offset+8] > 64)
		{
			printf ("Release volume: %s%d # OUT OF RANGE%s\n", FONT_BRIGHT_RED, mem[*offset+8], FONT_RESET);
		} else {
			printf ("Release volume: %d\n", mem[*offset+8]);
		}

		if (DumpPrefix (mem, len, *offset+9, 3)) { fprintf (stderr, "\n%sERROR: ran out of data%s\n", FONT_BRIGHT_RED, FONT_RESET); return 1; }
		printf ("Reserved\n");

		if (DumpPrefix (mem, len, *offset+12, 1)) { fprintf (stderr, "\n%sERROR: ran out of data%s\n", FONT_BRIGHT_RED, FONT_RESET); return 1; }
		/* must be all zero for AHX0 */
		if ((!(mem[*offset+12] & 0x7f)) || ((mem[*offset+12] & 0x7f) > 63))
		{
			printf ("Filter modulation lower limit: %s %d # OUT OF RANGE%s\n", FONT_BRIGHT_RED, mem[*offset+12] & 0x7f, FONT_RESET);
		} else {
			printf ("Filter modulation lower limit: %d\n", mem[*offset+12] & 0x7f);
		}

		if (DumpPrefix (mem, len, *offset+13, 1)) { fprintf (stderr, "\n%sERROR: ran out of data%s\n", FONT_BRIGHT_RED, FONT_RESET); return 1; }
		printf ("Vibrato Delay: %d\n", mem[*offset+13]);

		if (DumpPrefix (mem, len, *offset+14, 1)) { fprintf (stderr, "\n%sERROR: ran out of data%s\n", FONT_BRIGHT_RED, FONT_RESET); return 1; }
		if (mem[*offset+14] & 0x80) /* must be all zero for AHX0 */
		{
			printf ("Hardcut: enabled %d\n", (mem[*offset+14] >> 4) & 7);
		} else {
			printf ("Hardcut: disabled (%d)\n", (mem[*offset+14] >> 4) & 7);
		}
		printf ("                    Vibrato: %d\n", mem[*offset+14] & 15);

		if (DumpPrefix (mem, len, *offset+15, 1)) { fprintf (stderr, "\n%sERROR: ran out of data%s\n", FONT_BRIGHT_RED, FONT_RESET); return 1; }
		if (mem[*offset+15] > 63)
		{
			printf ("Vibrato: %s%d # OUT OF RANGE%s\n", FONT_BRIGHT_RED, mem[*offset+15], FONT_RESET);
		} else {
			printf ("Vibrato: %d\n", mem[*offset+15]);
		}

		if (DumpPrefix (mem, len, *offset+16, 1)) { fprintf (stderr, "\n%sERROR: ran out of data%s\n", FONT_BRIGHT_RED, FONT_RESET); return 1; }
		if ((mem[*offset+16] > 63) || (mem[*offset+16] < square_modulation_lower_limit_min))
		{
			printf ("Square modulation lower limit: %s%d # OUT OF RANGE%s\n", FONT_BRIGHT_RED, mem[*offset+16], FONT_RESET);
		} else {
			printf ("Square modulation lower limit: %d\n", mem[*offset+16]);
		}

		if (DumpPrefix (mem, len, *offset+17, 1)) { fprintf (stderr, "\n%sERROR: ran out of data%s\n", FONT_BRIGHT_RED, FONT_RESET); return 1; }
		if ((mem[*offset+17] > 63) || (mem[*offset+17] < mem[*offset+16]))
		{
			printf ("Square modulation upper limit: %s%d # OUT OF RANGE%s\n", FONT_BRIGHT_RED, mem[*offset+17], FONT_RESET);
		} else {
			printf ("Square modulation upper limit: %d\n", mem[*offset+17]);
		}

		if (DumpPrefix (mem, len, *offset+18, 1)) { fprintf (stderr, "\n%sERROR: ran out of data%s\n", FONT_BRIGHT_RED, FONT_RESET); return 1; }
		printf ("Square modulation speed: %d\n", mem[*offset+18]);

		if (DumpPrefix (mem, len, *offset+19, 1)) { fprintf (stderr, "\n%sERROR: ran out of data%s\n", FONT_BRIGHT_RED, FONT_RESET); return 1; }
		/* must be all zero for AHX0 */
		if ((!(mem[*offset+19] & 0x7f)) || ((mem[*offset+19] & 0x7f) > 63))
		{
			printf ("Filter modulation upper limit: %s%d # OUT OF RANGE%s\n", FONT_BRIGHT_RED, mem[*offset+19], FONT_RESET);
		} else {
			printf ("Filter modulation upper limit: %d\n", mem[*offset+19]);
		}

		/* Must be all zero for AHX0 */
		printf ("                    Filter modulation speed: %d\n", 
			((mem[*offset+ 1] & 0xf8) >> 3) | 
			((mem[*offset+12] & 0x80) >> 2) |
			((mem[*offset+19] & 0x80) >> 1));

		if (DumpPrefix (mem, len, *offset+20, 1)) { fprintf (stderr, "\n%sERROR: ran out of data%s\n", FONT_BRIGHT_RED, FONT_RESET); return 1; }
		/* must be all zero for AHX0 */
		if (!mem[*offset+20])
		{
			printf ("Playlist default speed: %s%d # OUT OF RANGE%s\n", FONT_BRIGHT_RED, mem[*offset+20], FONT_RESET);
		} else {
			printf ("Playlist default speed: %d\n", mem[*offset+20]);
		}

		if (DumpPrefix (mem, len, *offset+21, 1)) { fprintf (stderr, "\n%sERROR: ran out of data%s\n", FONT_BRIGHT_RED, FONT_RESET); return 1; }
		if (!mem[*offset+21])
		{
			printf ("Playlist length: 0 # EMPTY SAMPLE, IGNORE\n");
			*offset += 22;
			continue;
		} else {
			PLEN = mem[*offset+21];
			printf ("Playlist length: %d\n", mem[*offset+21]);
			*offset += 22;
		}
		for (j=0; j < PLEN; j++)
		{
			int noteon, note, waveform, fx1, fx2, info1, info2;

			if (DumpPrefix (mem, len, *offset, 4)) { fprintf (stderr, "\n%sERROR: ran out of data%s\n", FONT_BRIGHT_RED, FONT_RESET); return 1; }

			fx2 =      ((mem[*offset + 0] & 0xe0) >> 5);
			fx1 =      ((mem[*offset + 0] & 0x1c) >> 2);
			waveform = ((mem[*offset + 0] & 0x03) << 1) |
			           ((mem[*offset + 1] & 0x80) >> 7);
			noteon =     mem[*offset + 1] & 0x40;
			note   =     mem[*offset + 1] & 0x3f;
			info1  =     mem[*offset + 2];
			info2  =     mem[*offset + 3];

			*offset += 4;

			printf ("%03d | %c", j, noteon ? '*' : '.');
			print_note (note);
			if (waveform > 4)
			{
				printf (" %s%d%s", FONT_BRIGHT_RED, waveform, FONT_RESET);
			} else {
				printf (" %d", waveform);
			}

			if ((!fx1) && (!info1))
			{
				printf (" ...");
			} else {
				printf (" %d%02X", fx1, info1);
			}

			if ((!fx2) && (!info2))
			{
				printf (" ...");
			} else {
				printf (" %d%02X", fx2, info2);
			}

			printf (" |\n");
		}
	}

	return 0;
}

int DumpStrings (unsigned char *mem, int *offset, int len)
{
	int ptr, i;

	printf ("[%sSTRINGS%s]\n", FONT_BRIGHT_CYAN, FONT_RESET);

	for (ptr=*offset; ;ptr++)
	{
		if (ptr > len)
		{
			{ fprintf (stderr, "\n%sERROR: ran out of data%s\n", FONT_BRIGHT_RED, FONT_RESET); return 1; }
		}
		if (!mem[ptr])
		{
			break;
		}
	}
	DumpPrefix (mem, len, *offset, ptr-(*offset)+1);
	printf ("Title: \"");
	for (ptr=*offset; mem[ptr] ;ptr++)
	{
		if ((mem[ptr] < 0x20) || (mem[ptr] == 127))
		{
			printf ("%s\\x%02x%s", FONT_BRIGHT_RED, mem[ptr], FONT_RESET);
		} else if (mem[ptr] >= 128)
		{
			printf ("\\x%02x", mem[ptr]);
		} else {
			printf ("%c", mem[ptr]);
		}
	}
	printf ("\"\n");
	*offset = ptr + 1;

	for (i=0; i < SMP; i++)
	{
		for (ptr=*offset; ;ptr++)
		{
			if (ptr > len)
			{
				{ fprintf (stderr, "\n%sERROR: ran out of data%s\n", FONT_BRIGHT_RED, FONT_RESET); return 1; }
			}
			if (!mem[ptr])
			{
				break;
			}
		}
		DumpPrefix (mem, len, *offset, ptr-(*offset)+1);
		printf ("Sample %2d: \"", i + 1);
		for (ptr=*offset; mem[ptr] ;ptr++)
		{
			if ((mem[ptr] < 0x20) || (mem[ptr] == 127))
			{
				printf ("%s\\x%02x%s", FONT_BRIGHT_RED, mem[ptr], FONT_RESET);
			} else if (mem[ptr] >= 128)
			{
				printf ("\\x%02x", mem[ptr]);
			} else {
				printf ("%c", mem[ptr]);
			}
		}
		printf ("\"\n");
		*offset = ptr + 1;
	}

	return 0;
}

int ParseAHX (unsigned char *mem, int len)
{
	int offset;

	if (DumpHeader (mem, len))
	{
		return -1;
	}
	if (DumpSubSongs (mem, 14, len))
	{
		return -1;
	}
	if (DumpPositions (mem, 14+SS*2, len))
	{
		return -1;
	}
	if (DumpTracks (mem, 14+SS*2 + LEN*8, len))
	{
		return -1;
	}
	offset = 14+SS*2 + LEN*8 + (TRK+HaveTrack0)*TRL*3;
	if (DumpSamples (mem, &offset, len))
	{
		return -1;
	}
	if (DumpStrings (mem, &offset, len))
	{
		return -1;
	}

	return 0;
}


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

	while (1)
	{
		int option_index = 0;
		static struct option long_options[] =
		{
			{"color",        optional_argument, 0, 0},
			{"help",         no_argument,       0, 'h'},
			{0,              0,                 0, 0}
		};

		c = getopt_long(argc, argv, "h", long_options, &option_index);
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
		fprintf (stderr, "Usage:\n%s [--color=auto/never/on] [--savesamples -s] [--savepatterns -p] [--help] file.stm  (%d)\n", argv[0], help);
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

	if (ParseAHX (data, st.st_size))
	{
		goto failed;
	}

failed:
	munmap (data, data_mmaped_len);
	close (fd);
	return 0;
}
