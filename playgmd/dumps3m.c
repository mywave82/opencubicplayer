#include "config.h"
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
char *FONT_BRIGHT_BLUE = "";
char *FONT_BRIGHT_PURPLE = "";
char *FONT_BRIGHT_CYAN = "";

typedef struct __attribute__((packed))
{
	char name[28];
	uint8_t sig,type;
	uint16_t d1;
	uint16_t orders,ins,pats,flags,cwt,ffv;
	char magic[4];
	uint8_t gv,is,it,mv,uc,dp;
	uint8_t d2[8];
	uint16_t special;
	uint8_t channels[32];
} FileHeader;
FileHeader hdr;

void DumpPrefix (unsigned char *mem, int len, int base, int baselen)
{
	int i;
	printf ("[%s%08x%s]%s", FONT_BRIGHT_BLUE, base, FONT_RESET, FONT_BRIGHT_PURPLE);
	for (i=0; i < baselen; i++)
	{
		if (base+i >= len)
		{
			printf (" \?\?");
		} else {
			printf (" %02x", mem[base+i]);
		}
	}
	switch (baselen)
	{
		case 2:  printf (                  "%s ", FONT_RESET); break;
		case 1:  printf (               "%s    ", FONT_RESET); break;
		case 0:  printf (            "%s       ", FONT_RESET); break;
		default: printf ("%s\n                 ", FONT_RESET); break;
	}
}

int DumpInstrumentEmpty (unsigned char *mem, int len, int base, int instrument)
{
	int i;
	printf ("[%sINSTRUMENT %d%s]\n", FONT_BRIGHT_CYAN, instrument + 1, FONT_RESET);

	DumpPrefix (mem, len, base + 0x00, 1);
	printf ("Type: empty instrument (message only)\n");

	DumpPrefix (mem, len, base + 0x01, 12);
	printf ("FileName: \"");
	for (i=0; i < 12; i++)
	{
		if (!mem[base + 0x01 + i])
		{
			break;
		}
		putchar (mem[base + 0x01 + i]);
	}
	printf("\"\n");

	DumpPrefix (mem, len, base + 0x0d, 3+16+16);
	printf ("Reserved / not used\n");

	DumpPrefix (mem, len, base + 0x30, 28);
	printf ("Message: \"");
	for (i=0; i < 28; i++)
	{
		if (!mem[base + 0x30 + i])
		{
			break;
		}
		putchar (mem[base + 0x30 + i]);
	}
	if (i == 28)
	{
		printf("\" %sWarning, no NUL terminator%s\n", FONT_BRIGHT_RED, FONT_RESET);
	} else {
		printf("\"\n");
	}

	DumpPrefix (mem, len, base + 0x4c, 4);
	printf ("Magic: \"%c%c%c%c\" %s%s%s\n",
		(char)mem[base + 0x4c + 0],
		(char)mem[base + 0x4c + 1],
		(char)mem[base + 0x4c + 2],
		(char)mem[base + 0x4c + 3],
		(memcmp (mem + base + 0x4c, "SCRI", 4) && memcmp (mem + base + 0x4c, "SCRS", 4)) ? FONT_BRIGHT_RED : FONT_BRIGHT_GREEN,
		(memcmp (mem + base + 0x4c, "SCRI", 4) && memcmp (mem + base + 0x4c, "SCRS", 4)) ? "Failed" : "OK",
		FONT_RESET);

	return 0;
}

int DumpInstrumentPCM (unsigned char *mem, int len, int base, int instrument)
{
	uint32_t Length    = uint32_little (((uint32_t *)(mem + base + 0x10))[0]);
	uint32_t LoopStart = uint32_little (((uint32_t *)(mem + base + 0x14))[0]);
	uint32_t LoopEnd   = uint32_little (((uint32_t *)(mem + base + 0x18))[0]);
	uint32_t C2Spd     = uint32_little (((uint32_t *)(mem + base + 0x20))[0]);
	int i;

	printf ("[%sINSTRUMENT %d  / PCM Sample%s]\n", FONT_BRIGHT_CYAN, instrument + 1, FONT_RESET);

	DumpPrefix (mem, len, base + 0x00, 1);
	printf ("Type: PCM Sample\n");

	DumpPrefix (mem, len, base + 0x01, 12);
	printf ("FileName: \"");
	for (i=0; i < 12; i++)
	{
		if (!mem[base + 0x01 + i])
		{
			break;
		}
		putchar (mem[base + 0x01 + i]);
	}
	printf("\"\n");

	DumpPrefix (mem, len, base + 0x0d, 3);
	printf ("MemSeg: 0x%08x\n", mem[base+0x0d]<<16 | (mem[base+0x0e]<<8) | mem[base+0x0f]);

	DumpPrefix (mem, len, base + 0x10, 4);
	printf ("Length:     %d %s%s%s\n", (int)Length, Length>64000 ? FONT_BRIGHT_RED : FONT_BRIGHT_GREEN, (Length>64000) ? " Sample too long, will be cropped at 64000" : "", FONT_RESET);

	DumpPrefix (mem, len, base + 0x14, 4);
	printf ("Loop-Start: %d %s%s%s\n", (int)LoopStart, (LoopStart>Length) ? FONT_BRIGHT_RED : FONT_BRIGHT_GREEN, (LoopStart>Length) ? " LoopStart > Length" : "", FONT_RESET);

	DumpPrefix (mem, len, base + 0x18, 4);
	printf ("Loop-End: %d %s%s%s\n", (int)LoopEnd, (LoopEnd>(Length+1)) ? FONT_BRIGHT_RED : FONT_BRIGHT_GREEN, (LoopEnd>(Length+1)) ? " LoopEnd > Length + 1" : "", FONT_RESET);

	DumpPrefix (mem, len, base + 0x1c, 1);
	printf ("Volume: %d %s%s%s\n", mem[base+0x1c], (mem[base+0x1c] > 64) ? FONT_BRIGHT_RED : FONT_BRIGHT_GREEN, (mem[base+0x1c] > 64) ? " This is greater than 64" : "", FONT_RESET);

	DumpPrefix (mem, len, base + 0x1d, 1);
	printf ("Reserved/unused\n");

	DumpPrefix (mem, len, base + 0x1e, 1);
	switch (mem[base + 0x1e])
	{
		case 0: printf ("unpacked)\n"); break;
		case 1: printf ("DP30ADPCM packing (deprecated after Scream Tracker 3.00)\n"); break;
		default: printf ("%sUnknown%s\n", FONT_BRIGHT_RED, FONT_RESET);
	}

	DumpPrefix (mem, len, base + 0x1f, 1);
	printf ("Flags:\n");
	printf ("                   1: [%c] Loop On\n", (mem[base + 0x1f] & 1) ? 'x' : ' ');
	printf ("                   2: [%c] Stereo (Not support by ScreamTracker 3.01)\n",  (mem[base + 0x1f] & 2) ? 'x' : ' ');
	printf ("                   4: [%c] 16-bit (Not support by ScreamTracker 3.01)\n", (mem[base + 0x1f] & 4) ? 'x' : ' ');

	DumpPrefix (mem, len, base + 0x20, 4);
	printf ("C2 Sample Rate: %d%s%s%s\n", (int)C2Spd, (C2Spd>0xffff)?FONT_BRIGHT_RED : FONT_BRIGHT_GREEN, (C2Spd>0xffff) ? " Scream Tracker 3 only supports upto 65535" : "", FONT_RESET);

	DumpPrefix (mem, len, base + 0x24, 12);
	printf ("Reserved / Internal use / leave at 0\n");

	DumpPrefix (mem, len, base + 0x30, 28);
	printf ("Sample Name: \"");
	for (i=0; i < 28; i++)
	{
		if (!mem[base + 0x30 + i])
		{
			break;
		}
		putchar (mem[base + 0x30 + i]);
	}
	if (i == 28)
	{
		printf("\" %sWarning, no NUL terminator%s\n", FONT_BRIGHT_RED, FONT_RESET);
	} else {
		printf("\"\n");
	}

	DumpPrefix (mem, len, base + 0x3c, 4);
	printf ("Magic: \"%c%c%c%c\" %s%s%s\n",
		(char)mem[base + 0x4c + 0],
		(char)mem[base + 0x4c + 1],
		(char)mem[base + 0x4c + 2],
		(char)mem[base + 0x4c + 3],
		(memcmp(mem + base + 0x4c, "SCRS", 4)) ? FONT_BRIGHT_RED : FONT_BRIGHT_GREEN,
		(memcmp(mem + base + 0x4c, "SCRS", 4)) ? "Failed" : "OK",
		FONT_RESET);

	return 0;
}

int DumpInstrumentAdlibMelody (unsigned char *mem, int len, int base, int instrument)
{
	uint32_t C2Spd     = uint32_little (((uint32_t *)(mem + base + 0x20))[0]);
	int i;

	printf ("[%sINSTRUMENT %d  / Adlib Melody%s]\n", FONT_BRIGHT_CYAN, instrument + 1, FONT_RESET);

	DumpPrefix (mem, len, base + 0x00, 1);
	printf ("Type: Adlib Melody\n");

	DumpPrefix (mem, len, base + 0x01, 12);
	printf ("FileName: \"");
	for (i=0; i < 12; i++)
	{
		if (!mem[base + 0x01 + i])
		{
			break;
		}
		putchar (mem[base + 0x01 + i]);
	}
	printf("\"\n");

	DumpPrefix (mem, len, base + 0x0d, 3);
	printf ("Reserved\n");

	DumpPrefix (mem, len, base + 0x10, 1);
	printf ("D00 (adlib reg 0x23): Modulator -- Tremolo:%d Vibrator:%d Sustain:%d EnvelopeScaling:%d  FrequencyMultiplicationFactor:%d (0=half)\n",
		!!(mem[base+0x10]&0x80),
		!!(mem[base+0x10]&0x40),
		!!(mem[base+0x10]&0x20),
		!!(mem[base+0x10]&0x10),
		mem[base+0x10]&0x0f);

	DumpPrefix (mem, len, base + 0x11, 1);
	printf ("D01 (adlib reg 0x23):  Carrier  -- Tremolo:%d Vibrator:%d Sustain:%d EnvelopeScaling:%d  FrequencyMultiplicationFactor:%d (0=half)\n",
		!!(mem[base+0x11]&0x80),
		!!(mem[base+0x11]&0x40),
		!!(mem[base+0x11]&0x20),
		!!(mem[base+0x11]&0x10),
		mem[base+0x11]&0x0f);

	DumpPrefix (mem, len, base + 0x12, 1);
	printf ("D02 (adlib reg 0x43): Modulator -- KeyScaleLevel:%d OutputLevel:%d (inverse)\n",
		mem[base+0x12] >> 6,
		mem[base+0x12] & 0x3f);

	DumpPrefix (mem, len, base + 0x13, 1);
	printf ("D03 (adlib reg 0x43):  Carrier  -- KeyScaleLevel:%d OutputLevel:%d (inverse)\n",
		mem[base+0x13] >> 6,
		mem[base+0x13] & 0x3f);

	DumpPrefix (mem, len, base + 0x14, 1);
	printf ("D04 (adlib reg 0x60): Modulator -- AttackRate:%d DecayRate:%d\n",
		mem[base+0x14] >> 4,
		mem[base+0x14] & 0x0f);

	DumpPrefix (mem, len, base + 0x15, 1);
	printf ("D05 (adlib reg 0x63):  Carrier  -- AttackRate:%d DecayRate:%d\n",
		mem[base+0x15] >> 4,
		mem[base+0x15] & 0x0f);

	DumpPrefix (mem, len, base + 0x16, 1);
	printf ("D06 (adlib reg 0x80): Modulator -- SustainLevel:%d ReleaseRate:%d\n",
		mem[base+0x16] >> 4,
		mem[base+0x16] & 0x0f);

	DumpPrefix (mem, len, base + 0x17, 1);
	printf ("D07 (adlib reg 0x83):  Carrier  -- SustainRate:%d ReleaseRate:%d\n",
		mem[base+0x17] >> 4,
		mem[base+0x17] & 0x0f);

	DumpPrefix (mem, len, base + 0x18, 1);
	printf ("D08 (adlib reg 0xe0): Modulator -- WaveForm:");
	switch (mem[base+0x18] & 7)
	{
		case 0: printf ("Sine\n");
		case 1: printf ("Half-Sine\n");
		case 2: printf ("Abs-Sine\n");
		case 3: printf ("Pulse-Sine\n");
		case 4: printf ("Sine - even periods only (OPL3 only)\n");
		case 5: printf ("Abs-Sine - even periods only (OPL3 only)\n");
		case 6: printf ("Square (OPL3 only)\n");
		case 7: printf ("Derived Square (OPL3 only)\n");
	}

	DumpPrefix (mem, len, base + 0x19, 1);
	printf ("D09 (adlib reg 0xe3):  Carrier  -- WaveForm:");
	switch (mem[base+0x19] & 7)
	{
		case 0: printf ("Sine\n");
		case 1: printf ("Half-Sine\n");
		case 2: printf ("Abs-Sine\n");
		case 3: printf ("Pulse-Sine\n");
		case 4: printf ("Sine - even periods only (OPL3 only)\n");
		case 5: printf ("Abs-Sine - even periods only (OPL3 only)\n");
		case 6: printf ("Square (OPL3 only)\n");
		case 7: printf ("Derived Square (OPL3 only)\n");
	}

	DumpPrefix (mem, len, base + 0x1a, 1);
	printf ("D0A (adlib reg 0xc0): Modulator -- ModulatationFeedback:%d SynthesisType:%s\n",
		(mem[base + 0x1a] >> 1 ) & 7,
		(mem[base + 0x1a] & 0x01) ? "Additive synthesis":"Frequency Modulation");

	DumpPrefix (mem, len, base + 0x1b, 1);
	printf ("D0B Unused\n");

	DumpPrefix (mem, len, base + 0x1c, 1);
	printf ("Volume: %d %s%s%s\n", mem[base+0x1c], (mem[base+0x1c] > 64) ? FONT_BRIGHT_RED : FONT_BRIGHT_GREEN, (mem[base+0x1c] > 64) ? " This is greater than 64" : "", FONT_RESET);

	DumpPrefix (mem, len, base + 0x1e, 2);
	printf ("Reserved/unused\n");

	DumpPrefix (mem, len, base + 0x20, 4);
	printf ("C2 Sample Rate: %d%s%s%s\n", (int)C2Spd, (C2Spd>0xffff)?FONT_BRIGHT_RED : FONT_BRIGHT_GREEN, (C2Spd>0xffff) ? " Scream Tracker 3 only supports upto 65535" : "", FONT_RESET);

	DumpPrefix (mem, len, base + 0x24, 12);
	printf ("Reserved / Internal use / leave at 0\n");

	DumpPrefix (mem, len, base + 0x30, 28);
	printf ("Sample Name: \"");
	for (i=0; i < 28; i++)
	{
		if (!mem[base + 0x30 + i])
		{
			break;
		}
		putchar (mem[base + 0x30 + i]);
	}
	if (i == 28)
	{
		printf("\" %sWarning, no NUL terminator%s\n", FONT_BRIGHT_RED, FONT_RESET);
	} else {
		printf("\"\n");
	}

	DumpPrefix (mem, len, base + 0x3c, 4);
	printf ("Magic: \"%c%c%c%c\" %s%s%s\n",
		(char)mem[base + 0x4c + 0],
		(char)mem[base + 0x4c + 1],
		(char)mem[base + 0x4c + 2],
		(char)mem[base + 0x4c + 3],
		(memcmp(mem + base + 0x4c, "SCRI", 4)) ? FONT_BRIGHT_RED: FONT_BRIGHT_GREEN,
		(memcmp(mem + base + 0x4c, "SCRI", 4)) ? "Failed" : "OK",
		FONT_RESET);

	return 0;
}

int DumpInstrumentAdlibDrum (unsigned char *mem, int len, int base, int instrument)
{
	uint32_t C2Spd     = uint32_little (((uint32_t *)(mem + base + 0x20))[0]);
	int i;

	printf ("[%sINSTRUMENT %d  / Adlib Percussive%s]\n", FONT_BRIGHT_CYAN, instrument + 1, FONT_RESET);

	DumpPrefix (mem, len, base + 0x00, 1);
	printf ("Type: Adlib percussive:");
	switch (mem[base+0x00])
	{
		case 3: printf ("bass drum\n"); break;
		case 4: printf ("snare\n"); break;
		case 5:	printf ("tom tom\n"); break;
		case 6: printf ("top cymbal\n"); break;
		case 7: printf ("hi-hat\n"); break;
	}

	DumpPrefix (mem, len, base + 0x01, 12);
	printf ("FileName: \"");
	for (i=0; i < 12; i++)
	{
		if (!mem[base + 0x01 + i])
		{
			break;
		}
		putchar (mem[base + 0x01 + i]);
	}
	printf("\"\n");

	DumpPrefix (mem, len, base + 0x0d, 3 + 12);
	printf ("Reserved\n");

	DumpPrefix (mem, len, base + 0x1c, 1);
	printf ("Volume: %d %s%s%s\n", mem[base+0x1c], (mem[base+0x1c] > 64) ? FONT_BRIGHT_RED : FONT_BRIGHT_GREEN, (mem[base+0x1c] > 64) ? " This is greater than 64" : "", FONT_RESET);

	DumpPrefix (mem, len, base + 0x1e, 2);
	printf ("Reserved/unused\n");

	DumpPrefix (mem, len, base + 0x20, 4);
	printf ("C2 Sample Rate: %d%s%s%s\n", (int)C2Spd, (C2Spd>0xffff)?FONT_BRIGHT_RED : FONT_BRIGHT_GREEN, (C2Spd>0xffff) ? " Scream Tracker 3 only supports upto 65535" : "", FONT_RESET);

	DumpPrefix (mem, len, base + 0x24, 12);
	printf ("Reserved / Internal use / leave at 0\n");

	DumpPrefix (mem, len, base + 0x30, 28);
	printf ("Sample Name: \"");
	for (i=0; i < 28; i++)
	{
		if (!mem[base + 0x30 + i])
		{
			break;
		}
		putchar (mem[base + 0x30 + i]);
	}
	if (i == 28)

	{
		printf("\" %sWarning, no NUL terminator%s\n", FONT_BRIGHT_RED, FONT_RESET);
	} else {
		printf("\"\n");
	}

	DumpPrefix (mem, len, base + 0x4c, 4);
	printf ("Magic: \"%c%c%c%c\" %s%s%s\n",
		(char)mem[base + 0x4c + 0],
		(char)mem[base + 0x4c + 1],
		(char)mem[base + 0x4c + 2],
		(char)mem[base + 0x4c + 3],
		(memcmp(mem + base + 0x4c, "SCRI", 4)) ? FONT_BRIGHT_RED : FONT_BRIGHT_GREEN,
		(memcmp(mem + base + 0x4c, "SCRI", 4)) ? "Failed" : "OK",
		FONT_RESET);

	return 0;
}

int DumpInstrumentUnknown (unsigned char *mem, int len, int base, int instrument)
{
	int i;
	printf ("[%sINSTRUMENT %d / Unknown%s]\n", FONT_BRIGHT_CYAN, instrument + 1, FONT_RESET);

	DumpPrefix (mem, len, base + 0x00, 1);
	printf ("Type: %sUnknown%s\n", FONT_BRIGHT_RED, FONT_RESET);

	DumpPrefix (mem, len, base + 0x01, 12);
	printf ("FileName: \"");
	for (i=0; i < 12; i++)
	{
		if (!mem[base + 0x01 + i])
		{
			break;
		}
		putchar (mem[base + 0x01 + i]);
	}
	printf("\n");

	DumpPrefix (mem, len, base + 0x0d, 3+16+16);
	printf ("Reserved / not used\n");

	DumpPrefix (mem, len, base + 0x30, 28);
	printf ("Message: \"");
	for (i=0; i < 28; i++)
	{
		if (!mem[base + 0x30 + i])
		{
			break;
		}
		putchar (mem[base + 0x30 + i]);
	}
	if (i == 28)
	{
		printf(" %sWarning, no NUL terminator%s", FONT_BRIGHT_RED, FONT_RESET);
	}
	printf("\n");

	DumpPrefix (mem, len, base + 0x4c, 4);
	printf ("Magic: \"%c%c%c%c\" ???\n",
		(char)mem[base + 0x4c + 0],
		(char)mem[base + 0x4c + 1],
		(char)mem[base + 0x4c + 2],
		(char)mem[base + 0x4c + 3]);

	return 0;
}

int DumpInstrument (unsigned char *mem, int len, int base, int instrument)
{
	if (base+0x50 > len)
	{
		fprintf (stderr, "0x%08x + 0x50 > filelen, unable to print instrument %d\n", base, instrument + 1);
		return -1;
	}
	switch (mem[base])
	{
		case 0: return DumpInstrumentEmpty (mem, len, base, instrument); break;
		case 1: return DumpInstrumentPCM (mem, len, base, instrument); break;
		case 2: return DumpInstrumentAdlibMelody (mem, len, base, instrument); break;
		case 3:
		case 4:
		case 5:
		case 6:
		case 7: return DumpInstrumentAdlibDrum (mem, len, base, instrument); break;
		default: return DumpInstrumentUnknown (mem, len, base, instrument); break;
	}
}

int ParseS3M (unsigned char *mem, int len)
{
	int i;
	int offset = 0;
	if (len < sizeof (FileHeader))
	{
		fprintf (stderr, "*** len < sizeof(FileHeader) ***\n");
		return 1;
	}

	printf ("[%sHEADER%s]\n", FONT_BRIGHT_CYAN, FONT_RESET);

	memcpy (&hdr, mem, sizeof (FileHeader));
	hdr.d1      = uint16_little (hdr.d1);
	hdr.orders  = uint16_little (hdr.orders);
	hdr.ins     = uint16_little (hdr.ins);
	hdr.pats    = uint16_little (hdr.pats);
	hdr.flags   = uint16_little (hdr.flags);
	hdr.cwt     = uint16_little (hdr.cwt);
	hdr.ffv     = uint16_little (hdr.ffv);
	hdr.special = uint16_little (hdr.special);

	offset += sizeof (FileHeader);

	/* hdr.name */
	DumpPrefix (mem, len, offsetof (FileHeader, name), sizeof (hdr.name));
	printf ("Name: \"");
	for (i=0; i < sizeof (hdr.name); i++)
	{
		if (!hdr.name[i])
			break;
		printf ("%c", hdr.name[i]);
	}
	printf ("\"\n");

	DumpPrefix (mem, len, offsetof (FileHeader, sig), sizeof (hdr.sig));
	printf ("Signature: %s%s%s\n",
		(hdr.sig == 0x1A) ? FONT_BRIGHT_GREEN : FONT_BRIGHT_RED,
		(hdr.sig == 0x1A) ? "OK" : "Failed!!!!",
		FONT_RESET);

	DumpPrefix (mem, len, offsetof (FileHeader, type), sizeof (hdr.type));
	printf ("Type: ");
	switch (hdr.type)
	{
		case  1: printf ("STM song w/o samples, FAILED !!!!!!\n"); return 1;
		case  2: printf ("STM module, FAILED !!!!!!\n"); return 1;
		case 16: printf ("S3M module\n"); break;
		default: printf ("Unknown, FAILED !!!!!\n"); return 1;
	}

	DumpPrefix (mem, len, offsetof (FileHeader, d1), sizeof (hdr.d1));
	printf ("Reserved (%s%s%s)\n",
		(hdr.d1 == 0) ? FONT_BRIGHT_GREEN : FONT_BRIGHT_RED,
		(hdr.d1 == 0) ? "OK" : "Failed!!!!",
		FONT_RESET);

	DumpPrefix (mem, len, offsetof (FileHeader, orders), sizeof (hdr.orders));
	printf ("Orders: %d%s\n",
		hdr.orders,
		hdr.orders & 1 ? " Warning, not an even number":"");

	DumpPrefix (mem, len, offsetof (FileHeader, ins), sizeof (hdr.ins));
	printf ("Instruments: %d\n", hdr.ins);

	DumpPrefix (mem, len, offsetof (FileHeader, pats), sizeof (hdr.pats));
	printf ("Patterns: %d\n", hdr.pats);

	DumpPrefix (mem, len, offsetof (FileHeader, flags), sizeof (hdr.flags));
	printf ("Flags:\n");
	// deprecated flag
	printf ("%s                   1: [%c] st2vibrato%s\n",
		(hdr.ffv == 1) ? FONT_RESET : FONT_BRIGHT_BLACK,
		(hdr.flags & 1) ? 'x' : ' ',
		FONT_RESET);
	// deprecated flag
	printf ("%s                   2: [%c] st2tempo%s\n",
		(hdr.ffv == 1) ? FONT_RESET : FONT_BRIGHT_BLACK,
		(hdr.flags & 2) ? 'x' : ' ',
		FONT_RESET);
	// deprecated flag
	printf ("%s                   4: [%c] amigaslides%s\n",
		(hdr.ffv == 1) ? FONT_RESET : FONT_BRIGHT_BLACK,
		(hdr.flags & 4) ? 'x' : ' ',
		FONT_RESET);
	printf ("                   8: [%c] 0vol optimizations" /* " (Automatically turn off looping notes whose volume is zero for >2 note rows)" */ "\n", (hdr.flags & 8) ? 'x' : ' ');
	printf ("                  16: [%c] amiga limits" /*" (Disallow any notes that go beond the amiga hardware limits (like amiga does). This means that sliding up stops at B#5 etc. Also affects some minor amiga compatibility issues)" */ "\n", (hdr.flags & 16) ? 'x' : ' ');
	// deprecated flag
	printf ("%s                  32: [%c] enable filter/sfx with SoundBlaster%s\n",
		(hdr.ffv == 1) ? FONT_RESET : FONT_BRIGHT_BLACK,
		(hdr.flags & 32) ? 'x' : ' ',
		FONT_RESET);
	printf ("                  64: [%s] ST3.00 volume slides\n", (hdr.cwt == 0x1300) ? "implicit" : ((hdr.flags & 64) ? "x" : " "));
	printf ("                 128: [%c] special custom data in file\n", (hdr.flags & 128) ? 'x' : ' ');

	DumpPrefix (mem, len, offsetof (FileHeader, cwt), sizeof (hdr.cwt));
	printf ("Created With Tracker/Version: ");
	if ((hdr.cwt & 0xff00) == 0x1300)
	{
		printf ("ScreamTracker 3.%02x\n", hdr.cwt&0x00ff);
	} else if ((hdr.cwt & 0xf000) == 0x2000)
	{
		printf ("Imago Orpheus %d.%d\n", (hdr.cwt >> 8) & 0x000f, hdr.cwt&0x00ff);
	} else if ((hdr.cwt & 0xf000) == 0x3000)
	{
		printf ("Impulse Tracker %d.%d\n", (hdr.cwt >> 8) & 0x000f, hdr.cwt&0x00ff);
	} else if (hdr.cwt == 0x4100)
	{
		printf ("old BeRoTracker version from between 2004 and 2012\n");
	} else if ((hdr.cwt & 0xf000) == 0x4000)
	{
		printf ("Schism Tracker %d.%d\n", (hdr.cwt >> 8) & 0x000f, hdr.cwt&0x00ff);
	} else if ((hdr.cwt & 0xf000) == 0x5000)
	{
		printf ("OpenMPT %d.%d\n", (hdr.cwt >> 8) & 0x000f, hdr.cwt&0x00ff);
	} else if ((hdr.cwt & 0xf000) == 0x6000)
	{
		printf ("BeRoTracker %d.%d\n", (hdr.cwt >> 8) & 0x000f, hdr.cwt&0x00ff);
	} else if ((hdr.cwt & 0xf000) == 0x7000)
	{
		printf ("CreamTracker %d.%d\n", (hdr.cwt >> 8) & 0x000f, hdr.cwt&0x00ff);
	} else if (hdr.cwt == 0xca00)
	{
		printf ("Camoto/libgamemusic\n");
	} else {
		printf ("Unknown\n");
	}

	DumpPrefix (mem, len, offsetof (FileHeader, ffv), sizeof (hdr.ffv));
	printf ("FileFormatVersion: ");
	if (hdr.ffv == 0x0001)
	{
		printf("old version used long ago (samples signed)\n");
	} else if (hdr.ffv == 0x0002)
	{
		printf("standard (samples unsigned)\n");
	} else {
		printf("unknown!!!!!!!!\n");
	}

	DumpPrefix (mem, len, offsetof (FileHeader, magic), sizeof (hdr.magic));
	printf ("Signature: \"%c%c%c%c\" %s%s%s\n",
		hdr.magic[0],
		hdr.magic[1],
		hdr.magic[2],
		hdr.magic[3],
		memcmp (hdr.magic, "SCRM", 4) ? FONT_BRIGHT_RED : FONT_BRIGHT_GREEN,
		memcmp (hdr.magic, "SCRM", 4) ? "Failed!!!!" : "OK",
		FONT_RESET);

	DumpPrefix (mem, len, offsetof (FileHeader, gv), sizeof (hdr.gv));
	printf ("Global Volume: %d\n", hdr.gv);

	DumpPrefix (mem, len, offsetof (FileHeader, is), sizeof (hdr.is));
	printf ("Initial Speed: %d\n", hdr.is);

	DumpPrefix (mem, len, offsetof (FileHeader, it), sizeof (hdr.it));
	printf ("Initial Tempo: %d\n", hdr.it);

	DumpPrefix (mem, len, offsetof (FileHeader, mv), sizeof (hdr.mv));
	printf ("Master Volume: %d\n", hdr.mv);

	DumpPrefix (mem, len, offsetof (FileHeader, uc), sizeof (hdr.uc));
	printf ("GUS hardware, click removal for %d channels\n", hdr.uc);

	DumpPrefix (mem, len, offsetof (FileHeader, dp), sizeof (hdr.dp));
	printf ("Default Pan: %s\n", (hdr.dp == 252) ? "Use data from header" : "Ignore data in header, use system default");

	DumpPrefix (mem, len, offsetof (FileHeader, d2), sizeof (hdr.d2));
	printf ("(Reserved/not used)\n");

	DumpPrefix (mem, len, offsetof (FileHeader, special), sizeof (hdr.special));
	printf ("SpecialData: ");
	if (hdr.flags & 128)
	{
		printf ("ParaPointer 0x%04x => ptr 0x%08x\n", hdr.special, 16*hdr.special);
	} else {
		printf ("not enabled in flags\n");
	}

	for (i=0; i < 32; i++)
	{
		DumpPrefix (mem, len, offsetof (FileHeader, channels) + i, 1);
		printf ("ChannelSetting %2d: ", i + 1);
		if (hdr.channels[i] < 8)
		{
			printf ("Left  PCM Channel %d\n", hdr.channels[i]);
		} else if (hdr.channels[i] < 16)
		{
			printf ("Right PCM Channel %d\n", hdr.channels[i] - 8);
		} else if (hdr.channels[i] < 25)
		{
			printf ("Adlib melody channel %d\n", hdr.channels[i] - 16);
		} else if (hdr.channels[i] == 25)
		{
			printf ("Adlib percussion channel: bass drum\n");
		} else if (hdr.channels[i] == 26)
		{
			printf ("Adlib percussion channel: snare drum\n");
		} else if (hdr.channels[i] == 27)
		{
			printf ("Adlib percussion channel: tom tom\n");
		} else if (hdr.channels[i] == 28)
		{
			printf ("Adlib percussion channel: top cymbal\n");
		} else if (hdr.channels[i] == 29)
		{
			printf ("Adlib percussion channel: hi-hat\n");
		} else if (hdr.channels[i] < 128)
		{
			printf ("Invalid!!!!!\n");
		} else if (hdr.channels[i] < 255)
		{
			printf ("Disabled (invalid\?\?\?)\n");
		} else {
			printf ("Channel unused\n");
		}
	}

	DumpPrefix(mem, len, sizeof(hdr), hdr.orders);
	if ((sizeof (hdr) + hdr.orders) >= len)
	{
		printf ("Orders: Not enough data in the file\n");
		return -1;
	}
	printf ("Orders: [");
	for (i=0; i < hdr.orders; i++)
	{
		printf ("%s%d%s%s", i?", ":"", mem[sizeof(hdr)+i],
		mem[sizeof(hdr)+i]==254?" marker/ignore this":"",
		mem[sizeof(hdr)+i]==255?" end of song":""
		);
	}
	printf ("]\n");

	for (i=0; i < hdr.ins; i++)
	{
		DumpPrefix (mem, len, sizeof(hdr) + hdr.orders + 2*i, 2);

		if ((sizeof (hdr) + hdr.orders + (i+1)*2) >= len)
		{
			printf ("Instrument %2d: Not enough data in the file\n", i + 1);
			return -1;
		} else {
			uint16_t temp = uint16_little (((uint16_t *)(mem + sizeof(hdr) + hdr.orders))[i]);
			printf ("Instrument %2d: ParaPointer 0x%04x => ptr 0x%08x\n", i + 1, temp, 16*temp);
		}
	}

	for (i=0; i < hdr.pats; i++)
	{
		DumpPrefix (mem, len, sizeof(hdr) + hdr.orders + hdr.ins*2 + 2*i, 2);

		if ((sizeof (hdr) + hdr.orders + (i+1)*2) >= len)
		{
			printf ("Pattern %2d: Not enough data in the file\n", i + 1);
			return -1;
		} else {
			uint16_t temp = uint16_little (((uint16_t *)(mem + sizeof(hdr) + hdr.orders + hdr.ins*2))[i]);
			printf ("Pattern %2d: ParaPointer 0x%04x => ptr 0x%08x\n", i + 1, temp, 16*temp);
		}
	}

	for (i=0; i < hdr.ins; i++)
	{
		uint16_t temp = uint16_little (((uint16_t *)(mem + sizeof(hdr) + hdr.orders))[i]);
		DumpInstrument (mem, len, 16*temp, i);
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
			{"color", required_argument, 0, 0},
			{"help",  no_argument,       0, 'h'},
			{0,       0,                 0, 0}
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

	if (!strcmp (color, "auto"))
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
		fprintf (stderr, "Usage:\n%s [--color=auto/never/on] [--help] file.s3m  (%d)\n", argv[0], help);
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
		FONT_BRIGHT_BLUE   = "\033[34;1m";
		FONT_BRIGHT_PURPLE = "\033[35;1m";
		FONT_BRIGHT_CYAN   = "\033[36;1m";
	}

	if (ParseS3M (data, data_mmaped_len))
	{
		goto failed;
	}

failed:
	munmap (data, data_mmaped_len);
	close (fd);
	return 0;
}
