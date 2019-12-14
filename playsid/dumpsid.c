#include "config.h"
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// Specification https://www.hvsc.c64.org/download/C64Music/DOCUMENTS/SID_file_format.txt

// typical run method
// dumpsid mysidfile.sid --color|iconv -f cp1252 -t utf8|less -r

#include "dumpsid_6502_dis.c"

//#define DEBUG_TODO 1

int usecolor = 0;

static char *FONT_RESET = "";
static char *FONT_BRIGHT_BLACK = "";
static char *FONT_BRIGHT_RED = "";
static char *FONT_BRIGHT_GREEN = "";
static char *FONT_BRIGHT_YELLOW = "";
static char *FONT_BRIGHT_BLUE = "";
static char *FONT_BRIGHT_PURPLE = "";
static char *FONT_BRIGHT_CYAN = "";

static unsigned char MOS65xx_memory[65536];
static unsigned char MOS65xx_flags[65536];
#define FLAG_CODE_DIRECT0    1
#define FLAG_CODE_DIRECT1    2
#define FLAG_CODE_DIRECT2    3
#define FLAG_CODE_DIRECT     7

#define FLAG_CODE_INDIRECT0   8
#define FLAG_CODE_INDIRECT1  16
#define FLAG_CODE_INDIRECT2  32
#define FLAG_CODE_INDIRECT   56


static void breakme (void)
{

}

static void DumpPrefix (unsigned char *mem, int len, int base, int baselen)
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

static int load_siddata (uint16_t *load, int ntsc, int PSID, unsigned char *buffer, int base, int length)
{
	int targetlength;

	MOS65xx_memory[0x02a6] = !ntsc;

	printf ("[%sDATA%s]\n", FONT_BRIGHT_CYAN, FONT_RESET);

	if (PSID)
	{
		// VIC = any number lower than 0x100 if speed flag is 0
		if (ntsc)
		{
			//CIA1_timer_A = 0x4025;
			// timer tune = 0x3ffb;
		} else {
			//CIA1_timer_A = 0x4295;
			// timer tune = 0x5021;
		}
		/*
		if (init and play) is smaller than              0xa000  bank = 0x37;
		else if (init and play) is smaller than         0xd000  bank = 0x36;
		else if (init and play) is larger or equal than 0xe000  bank = 0x35;
		else                                                    bank = 0x34;
		*/
	} else { /* if (RSID) */
		//VIC = 0x137; but irq disabled
		if (ntsc)
		{
			//CIA1_timer_A = 0x4025;
		} else {
			//CIA1_timer_A = 0x4295;
		}
		//CIA1_timer_A IRQ enabled
		// init/play => bank = 0x37;
		// if C64_BASIC is true, target song is written to  65xx_memory[0x30c] = song - 1;
	}
	if (*load == 0)
	{
		if ((base + 2) > length)
		{
			fprintf (stderr, "%sERROR: datalen > 2 - needed (to read out data target address)  (%d + 2 > %d)%s\n", FONT_BRIGHT_RED, base, length, FONT_RESET);
			return -1;
		}
		*load = (buffer[base+1]<<8) | buffer[base];
		printf ("load ptr: 0x%04x   # where in the memory space should this be loaded into\n",(int)*load);

		base += 2;
	}
	if (base >= length)
	{
		fprintf (stderr, "%sERROR: no data to load%s\n", FONT_BRIGHT_RED, FONT_RESET);
		return -1;
	}

	targetlength = length - base;
	if ((targetlength + *load) >= 0x10000)
	{
		fprintf (stderr, "%sWARNING: data exceeds memory-space (data 0x%04x + length 0x%x > 0x10000)%s\n", FONT_BRIGHT_YELLOW, length-base, *load, FONT_RESET);
		targetlength = 0x10000 - *load;
	}
	memcpy (MOS65xx_memory + *load, buffer + base, targetlength);

	return 0;
}

static void pre_disassemble(uint16_t load, uint16_t init, uint16_t play)
{

	uint16_t *todo_ptrs = malloc (sizeof (uint16_t *)*16);
	int      todo_n = 0;
	int      todo_size = 16;

	//bzero (MOS65xx_flags, sizeof (MOS65xx_flags));

	if (!load)
	{
		load = init;
	}
	if (!play)
	{
		play = init;
	}

	if (load)
	{
		todo_ptrs[todo_n++] = load;
	}

	if (play && (play != load))
	{
		todo_ptrs[todo_n++] = play;
	}

	while (todo_n)
	{
		char opcode[16];
		char param1[16];
		char param2[16];
		char comment[32];
		int length;
		uint16_t ptr = todo_ptrs[0];
		uint16_t alt_ptr;
		int retval;

		opcode[0] = 0;
		param1[0] = 0;
		param2[0] = 0;
		comment[0] = 0;

		if (MOS65xx_flags[ptr] & FLAG_CODE_DIRECT)
		{
#ifdef DEBUG_TODO
			fprintf (stderr, "pre-emptive remove, already scanned\n");
#endif
			memmove (todo_ptrs, todo_ptrs+1, sizeof (todo_ptrs[0]) * (todo_n - 1));
			todo_n--;
			continue;
		}

		retval = disassemble (MOS65xx_memory, ptr, opcode, param1, param2, comment, &length, &alt_ptr);
		if (retval != -2)
		{
			MOS65xx_flags[ptr] |= length; /* matches up with FLAG_CODE_DIRECT */
			if (MOS65xx_flags[ptr] & FLAG_CODE_INDIRECT)
			{
				breakme();
			}
			if (length>1)
			{
				if (MOS65xx_flags[ptr+1] & FLAG_CODE_DIRECT)
				{
					breakme();
				}
				MOS65xx_flags[ptr+1] |= FLAG_CODE_INDIRECT0;
			}
			if (length>2)
			{
				if (MOS65xx_flags[ptr+2] & FLAG_CODE_DIRECT)
				{
					breakme();
				}
				MOS65xx_flags[ptr+2] |= FLAG_CODE_INDIRECT1;
			}
			if (length>3)
			{
				if (MOS65xx_flags[ptr+3] & FLAG_CODE_DIRECT)
				{
					breakme();
				}
				MOS65xx_flags[ptr+3] |= FLAG_CODE_INDIRECT2;
			}
			todo_ptrs[0] += length;

			if ((retval == -1)||(retval == 2)||((todo_n > 1) && (todo_ptrs[0] == todo_ptrs[1])))
			{
#ifdef DEBUG_TODO
				fprintf (stderr, "removing todo[0], due to jump or duplication\n");
#endif
				memmove (todo_ptrs, todo_ptrs+1, sizeof (todo_ptrs[0]) * (todo_n - 1));
				todo_n--;
			}

			if (todo_n > 1)
			{
				/* did we overtake the queue? */
				if (todo_ptrs[0] > todo_ptrs[1])
				{
					uint16_t temp = todo_ptrs[0];
					todo_ptrs[0] = todo_ptrs[1];
					todo_ptrs[1] = temp;

					fprintf (stderr, "swapping todo[0] and todo[1]\n");
				}
			}
		} else {
			/* failed, remove from list */
#ifdef DEBUG_TODO
			fprintf (stderr, "removing todo[0], due to failure of decoding 0x%02x\n", MOS65xx_memory[todo_ptrs[0]]);
#endif
			memmove (todo_ptrs, todo_ptrs+1, sizeof (todo_ptrs[0]) * (todo_n - 1));
			todo_n--;
		}
		if ((retval == 1) || (retval == 2))
		{
			if (MOS65xx_flags[alt_ptr] & FLAG_CODE_DIRECT)
			{
#ifdef DEBUG_TODO
				fprintf (stderr, "Not adding %d, already decoded\n", alt_ptr);
#endif
			} else {
				int i;
				int skip = 0;
				for (i=0;i<todo_n;i++)
				{
					if (todo_ptrs[i] == alt_ptr)
					{
#ifdef DEBUG_TODO
						fprintf (stderr, "Skipping injecting, already on list %04x\n", alt_ptr);
#endif
						skip = 1;
						break;
					}
					if (todo_ptrs[i] > alt_ptr)
					{
						break;
					}
				}
				if (!skip)
				{
					if (todo_n == todo_size)
					{
						todo_size += 16;
						todo_ptrs = realloc (todo_ptrs, sizeof (todo_ptrs[0]) * todo_size);
					}
					memmove (todo_ptrs + i + 1, todo_ptrs + i, (todo_n - i) * sizeof (todo_ptrs[0]));
					todo_ptrs[i] = alt_ptr;
					todo_n++;
#ifdef DEBUG_TODO
					fprintf (stderr, "Injected todo[%d]=%04x\n", i, alt_ptr);
#endif
				}
			}
		}

#ifdef DEBUG_TODO
		{
			int i;
			for (i=0;i<todo_n;i++)
			{
				fprintf (stderr, "%s%04x", i?", ":"", todo_ptrs[i]);
			}
			fprintf (stderr, "\n");
		}
#endif
	}
	free (todo_ptrs);
}

static void print_disassemble (void)
{
	int prev = -1;
	int prevprev = -1;
	uint32_t ptr;

	for (ptr=0; ptr < 0x10000;)
	{
		char opcode[16];
		char param1[16];
		char param2[16];
		char comment[32];
		int length;
		uint16_t alt_ptr;

		if ((length=(MOS65xx_flags[ptr] & FLAG_CODE_DIRECT)))
		{
			int j;

			opcode[0] = 0;
			param1[0] = 0;
			param2[0] = 0;
			comment[0] = 0;

			disassemble (MOS65xx_memory, ptr, opcode, param1, param2, comment, &length, &alt_ptr);
			switch (length)
			{
				case 1: printf ("%s%04x %s%02x __ __%s    %s %s%s%s%s%s\n",
						FONT_BRIGHT_BLUE,
						ptr,
						FONT_BRIGHT_PURPLE,
						MOS65xx_memory[ptr],
						FONT_RESET,
						opcode, param1, param2[0]?", ":"", param2, comment[0]?" # ":"", comment); break;
				case 2: printf ("%s%04x %s%02x %02x __%s    %s %s%s%s%s%s\n",
						FONT_BRIGHT_BLUE,
						ptr,
						FONT_BRIGHT_PURPLE,
						MOS65xx_memory[ptr],
						MOS65xx_memory[ptr+1],
						FONT_RESET,
						opcode, param1, param2[0]?", ":"", param2, comment[0]?" # ":"", comment); break;
				case 3: printf ("%s%04x %s%02x %02x %02x%s    %s %s%s%s%s%s\n",
						FONT_BRIGHT_BLUE,
						ptr,
						FONT_BRIGHT_PURPLE,
						MOS65xx_memory[ptr],
						MOS65xx_memory[ptr+1],
						MOS65xx_memory[ptr+2],
						FONT_RESET,
						opcode, param1, param2[0]?", ":"", param2, comment[0]?" # ":"", comment); break;
			}

			for (j=1; j < length; j++)
			{
				if (MOS65xx_flags[ptr+j] & FLAG_CODE_DIRECT)
				{
					printf ("%sANTI DISASSEMBLER DETECTED at PTR %s%04x%s\n", FONT_BRIGHT_RED, FONT_BRIGHT_BLUE, j + ptr, FONT_RESET);
				}
			}
			ptr++;
			prev = -1;
			prevprev = -1;
		} else if (MOS65xx_flags[ptr] & FLAG_CODE_INDIRECT)
		{
			ptr++;
		} else {
			if (!((MOS65xx_memory[ptr] == prev) && ((prev == 0x00) || (prev == 0xff))))
			{
				printf ("%s%04x %s%02x%s\n",
					FONT_BRIGHT_BLUE,
					ptr,
					FONT_BRIGHT_PURPLE,
					MOS65xx_memory[ptr],
					FONT_RESET);
				prev = MOS65xx_memory[ptr];
				prevprev = -1;
			} else {
				if (prevprev < 0)
				{
					printf ("...\n");
					prevprev = prev;
				}
			}

			ptr++;
		}
	}

}

static int parse_sidfile (unsigned char *buffer, int length)
{
	int i;
	int PSID, RSID;
	uint16_t version, data, load, init, play, songs;
	uint32_t speeds;

	int flags_C64BASIC = 0;
	int flags_PlaySIDSpecific = 1;
	int isNTSC = 1;

	if (length < 0x76)
	{
		fprintf (stderr, "%sERROR: len < sizeof(FileHeader)%s\n", FONT_BRIGHT_RED, FONT_RESET);
		return -1;
	}

	printf ("[%sHEADER%s]\n", FONT_BRIGHT_CYAN, FONT_RESET);

	PSID = !memcmp (buffer, "PSID", 4);
	RSID = !memcmp (buffer, "RSID", 4);
	if (RSID || PSID)
	{
		DumpPrefix (buffer, length, 0, 4);
		printf ("id: \"%c%c%c%c\" %sOK%s\n",
		buffer[0], buffer[1], buffer[2], buffer[3], FONT_BRIGHT_GREEN, FONT_RESET);

		DumpPrefix (buffer, length, 4, 2);
		version = buffer[5] | (buffer[4]<<8);
		if ((PSID && (version>=1) && (version<=4)) ||
		    (RSID && (version>=2) && (version<=4)))
		{
			printf("version: %d - %s OK%s\n", version, FONT_BRIGHT_GREEN, FONT_RESET);
		} else {
			printf("version: %s%d - Out of range%s\n", FONT_BRIGHT_RED, version, FONT_RESET);
		}

		if ((version >= 2) && (length >= 0x007c))
		{
			flags_C64BASIC = RSID && (buffer[0x76] & 0x02);
			flags_PlaySIDSpecific = PSID && (buffer[0x76] & 0x02);
		}

		DumpPrefix (buffer, length, 6, 2);
		data = buffer[7] | (buffer[8]<<8);
		if ((version == 1) && (data != 0x0076))
		{
			printf ("data ptr: %s0x%04x - expected 0x0076%s\n", FONT_BRIGHT_RED, (int)data, FONT_RESET);
		} else if ((version >= 2) && (version <= 4) && (data != 0x007c))
		{
			printf ("data ptr: %s0x%04x - expected 0x007c%s\n", FONT_BRIGHT_RED, (int)data, FONT_RESET);
		} else {
			printf ("data ptr: 0x%04x   # where in this SID file is memory-dump located\n", (int)data);
		}

		DumpPrefix (buffer, length, 8, 2);
		load = buffer[9] | (buffer[8]<<8);
		if (RSID && load)
		{
			printf ("load ptr: %s0x%04x - expected 0x0000 (address is in the two first bytes of the memory-dump)%s\n", FONT_BRIGHT_RED, (int)load, FONT_RESET);
		} else {
			printf ("load ptr: 0x%04x   # where in the memory space should this be loaded into (if value is 0x0000, ptr is stored as the two first bytes in the data-dump)\n", (int)load);
		}

		DumpPrefix (buffer, length, 10, 2);
		init = buffer[11] | (buffer[10]<<8);
		if (init && flags_C64BASIC)
		{
			printf ("init ptr: %s0x%04x - expected 0x0000 due to C64BASIC flag   # where in the memory space is the init subroutine%s\n", FONT_BRIGHT_RED, (int)init, FONT_RESET);
		} else if (!init)
		{
			printf ("init ptr: 0x%04x - same as load ptr   # where in the memory space is the init subroutine\n", (int)init);
		} else if (RSID && ((init < 0x07e8) || ((init >= 0xa000) && (init < 0xc000)) || (init >= 0xd000)))
		{
			printf ("init ptr: %s0x%04x - Out of range   # where in the memory space is the init subroutine%s\n", FONT_BRIGHT_RED, (int)init, FONT_RESET);
		} else {
			printf ("init ptr: 0x%04x   # where in the memory space is the init subroutine\n", (int)init);
		}

		DumpPrefix (buffer, length, 12, 2);
		play = buffer[13] | (buffer[12]<<8);
		if (RSID && play)
		{
			printf ("play ptr: %s0x%04x   - RSID expects init function to install interrupt vectors   # where in the memory space is the play subroutine%s\n", FONT_BRIGHT_RED, (int)play, FONT_RESET);
		} else {
			printf ("play ptr: 0x%04x   # where in the memory space is the play subroutine (0x0000 means that init function installs interrupt vectors)\n", (int)play);
		}

		DumpPrefix (buffer, length, 14, 2);
		songs = buffer[15] | (buffer[14]<<8);
		if ((songs == 0) || (songs > 256))
		{
			printf ("songs: %s0x%04x   - Out of range%s\n", FONT_BRIGHT_RED, (int)songs, FONT_RESET);
		} else {
			printf ("songs: 0x%04x\n", (int)songs);
		}

		DumpPrefix (buffer, length, 16, 2);
		i = buffer[17] | (buffer[16]<<8);
		if ((i < 1) || (i > songs))
		{
			printf ("default song: %s0x%04x   - Out of range%s\n", FONT_BRIGHT_RED, i, FONT_RESET);
		} else {
			printf ("default song: %04x\n", i);
		}

		DumpPrefix (buffer, length, 18, 4);
		speeds = buffer[21] | (buffer[20]<<8) | (buffer[19]<<16) | (buffer[18] << 24);
		if (PSID)
		{
			printf ("speeds: 0x%08x\n", speeds);
			for (i=0; i < songs; i++)
			{
				printf ("Song %d - %s\n", i+1,
					(flags_PlaySIDSpecific?
						(speeds & (i << (i%31)))
						:
						(i < 32)?speeds & (1 << i):(speeds & 0x8000000)
					) ? "Use VBLANK - 50Hz PAL / 60Hz NTSC" : "Use CIA 1 timer interrupt (default 60Hz)");
			}
			if (i<32)
			{
				for (;i<32;i++)
				{
					if (speeds & (1 << i))
					{
						printf ("%sSong %d specified???%s\n", FONT_BRIGHT_RED, i+1, FONT_RESET);
					}
				}
			}
		} else { /* if (RSID) */
			if (speeds)
			{
				printf ("speeds: %s0x%08x - Expected 0x00000000 for RSID%s\n", FONT_BRIGHT_RED, speeds, FONT_RESET);
			} else {
				printf ("speeds: 0x%08x\n", speeds);
			}
		}

		DumpPrefix (buffer, length, 0x16, 0x20);
		printf ("Name: \"");
		for (i=0; i < 0x20; i++)
		{
			if (!buffer[0x16+i])
				break;
			printf ("%c", (char)buffer[0x16+i]);
		}
		printf ("\"\n");

		DumpPrefix (buffer, length, 0x36, 0x20);
		printf ("Author: \"");
		for (i=0; i < 0x20; i++)
		{
			if (!buffer[0x36+i])
				break;
			printf ("%c", (char)buffer[0x36+i]);
		}
		printf ("\"\n");

		DumpPrefix (buffer, length, 0x56, 0x20);
		printf ("Copyright/Released: \"");
		for (i=0; i < 0x20; i++)
		{
			if (!buffer[0x56+i])
				break;
			printf ("%c", (char)buffer[0x56+i]);
		}
		printf ("\"\n");

		if (version >= 2)
		{
			uint16_t flags;

			if (length < 0x007c)
			{
				fprintf (stderr, "%sERROR: len < sizeof(FileHeaderExtended)%s\n", FONT_BRIGHT_RED, FONT_RESET);
				return -1;
			}

			DumpPrefix (buffer, length, 0x76, 2);
			flags = buffer[0x77] | (buffer[0x76]<<8);
			printf ("flags: 0x%04x\n", (int)flags);
			printf ("  bit 0: %d - %s\n", !!(flags & 0x0001), (flags & 0x0001) ? "Compute!'s Sidplayer MUS data, music player must be merged" : "built-in music player");
			if (!(flags & 0x0002))
			{
				printf ("  bit 1: 0 - C64 compatible\n");
			} else {
				if (PSID)
				{
					printf ("  bit 1: 1 - PlaySID specific file / playback on C64 will have samples played wrong\n");
				} else { /* if (RSID) */
					printf ("  bit 1: 1 - C64 BASIC (must be available)\n");
				}
			}
			switch (flags & 0x000c) /* only PSID ?? */
			{
				case 0x0000: printf ("  bit 2-3: 00 - Video Standard: Unknown\n"); break;
				case 0x0004: printf ("  bit 2-3: 01 - Video Standard: PAL\n"); isNTSC = 0; break;
				case 0x0008: printf ("  bit 2-3: 10 - Video Standard: NTSC only\n"); break;
				case 0x000c: printf ("  bit 2-3: 11 - Video Standard: PAL and NTSC\n"); break;
			}
			switch (flags & 0x0030)
			{
				case 0x0000: printf ("  bit 4-5: 00 - sidModel position 1: Unknown\n"); break;
				case 0x0010: printf ("  bit 4-5: 01 - sidModel position 1: MOS6581\n"); break;
				case 0x0020: printf ("  bit 4-5: 10 - sidModel position 1: MOS8580\n"); break;
				case 0x0030: printf ("  bit 4-5: 11 - sidModel position 1: MOS6581 and MOS8580\n"); break;
			}

			if (version >= 3)
			{
				switch (flags & 0x00c0)
				{
					case 0x0000: printf ("  bit 6-7: 00 - sidModel position 2: Same as position 1\n"); break;
					case 0x0040: printf ("  bit 6-7: 01 - sidModel position 2: MOS6581\n"); break;
					case 0x0080: printf ("  bit 6-7: 10 - sidModel position 2: MOS8580\n"); break;
					case 0x00c0: printf ("  bit 6-7: 11 - sidModel position 2: MOS6581 and MOS8580\n"); break;
				}
			}
			if (version >= 4)
			{
				switch (flags & 0x0300)
				{
					case 0x0000: printf ("  bit 8-9: 00 - sidModel position 3: Same as position 2\n");
					case 0x0100: printf ("  bit 8-9: 01 - sidModel position 3: MOS6581\n"); break;
					case 0x0200: printf ("  bit 8-9: 10 - sidModel position 3: MOS8580\n"); break;
					case 0x0300: printf ("  bit 8-9: 11 - sidModel position 3: MOS6581 and MOS8580\n"); break;
				}
			}

			/* PSID only ??? */
			DumpPrefix (buffer, length, 0x78, 1);
			printf ("startPage: 0x%02x\n", buffer[0x78]);
			DumpPrefix (buffer, length, 0x79, 1);
			printf ("pageOnly: 0x%02x\n", buffer[0x79]);
			if (buffer[0x78] == 0x00)
			{
				printf ("  SID file is clean, only occupies loaded area\n");
			} else if (buffer[0x78] == 0xff)
			{
				printf ("  SID file uses the entire memory-map, driver-relocation not possible\n");
			} else {
				printf ("  free memory for driver-relocation at 0x%02x00-0x%02xff\n", buffer[0x78], buffer[0x78]+buffer[0x79]-1);
			}

			if (version >= 3) /* only RSID ?? */
			{
				DumpPrefix(buffer, length, 0x7a, 1);
				printf ("secondSIDAddress: 0x%02x\n", buffer[0x7a]);
				if (buffer[0x7a] == 0x00)
				{
					printf ("  No second SID chip need to be installed ??\n");
				} else if ((buffer[0x7a] & 0x01) && (!(((buffer[0x7a] >= 0x42) && (buffer[0x7a] <= 0x7f)) || ((buffer[0x7a] >= 0xe0) && (buffer[0x7a] <= 0xfe)))))
				{
					printf ("  %sSecond SID is in invalid range: 0xD%02xx%s\n", FONT_BRIGHT_RED, buffer[0x7a], FONT_RESET);
				} else {
					printf ("  Second SID should be in range: 0xD%02xx\n", buffer[0x7a]);
				}
			} else {
				#warning we can add a zero-test here
			}

			if (version >= 4) /* only RSID ?? */
			{
				DumpPrefix(buffer, length, 0x7b, 1);
				printf ("thirdSIDAddress: 0x%02x\n", buffer[0x7b]);
				if (buffer[0x7b] == 0x00)
				{
					printf ("  No third SID chip need to be installed ?? \n");
				} else if ((buffer[0x7b] & 0x01) && (!(((buffer[0x7b] >= 0x42) && (buffer[0x7b] <= 0x7f)) || ((buffer[0x7b] >= 0xe0) && (buffer[0x7b] <= 0xfe)))))
				{
					printf ("  %sThird SID is in invalid range: 0xD%02xx%s\n", FONT_BRIGHT_RED, buffer[0x7b], FONT_RESET);
				} else {
					printf ("  Third SID should be in range: 0xD%02xx\n", buffer[0x7b]);
				}
			} else {
				#warning we can add a zero-test here
			}
		}
		if (load_siddata (&load, isNTSC, PSID, buffer, data, length))
		{
			return -1;
		}
		pre_disassemble (load, init, play);
		print_disassemble ();
		return 0;
	} else {
#warning TODO
		return -1;
	}
}

int main (int argc, char *argv[])
{
	int fd;
	unsigned char *buffer = malloc (1024*1024); // way to big buffer
	int length;
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
		fprintf (stderr, "Usage:\n%s [--color=auto/never/on] [--help] file.sid  (%d)\n", argv[0], help);
		return 1;
	}

	fd = open (argv[optind], O_RDONLY);
	if (fd < 0)
	{
		fprintf (stderr, "open(%s) failed: %s\n", argv[1], strerror (errno));
		return -1;
	}

	length = read (fd, buffer, 1024*1024);
	if (length < 0)
	{
		fprintf (stderr, "read() failed: %s\n", strerror (errno));
		free (buffer);
		close (fd);
		return -1;
	}

	close (fd);

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

	parse_sidfile (buffer, length);

	free (buffer);

	return 0;
}
