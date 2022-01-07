/* OpenCP Module Player
 * copyright (c) 2019-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Utility: Dumping the content of a AY file
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

#define NO_CURSES

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

/* file-format spec source: http://vgmrips.net/wiki/AY_File_Format */

struct z80_session_t
{
	unsigned char z80_memory[65536];
	unsigned char z80_flags[65536];
	uint32_t ptr; /* used for printing */
	int prev;     /* used for printing */
	int prevprev; /* used for printing */
};

struct z80_session_t *active_session;

struct z80_session_t session_a; // vanilla
struct z80_session_t session_b; // after init
struct z80_session_t session_c; // after some iterations

#define FLAG_CODE_DIRECT0    1
#define FLAG_CODE_DIRECT1    2
#define FLAG_CODE_DIRECT2    3
#define FLAG_CODE_DIRECT     7

#define FLAG_CODE_INDIRECT0   8
#define FLAG_CODE_INDIRECT1  16
#define FLAG_CODE_INDIRECT2  32
#define FLAG_CODE_INDIRECT   56











#define _Z80_H 1

#define Z80_quit  1
#define Z80_NMI   2
#define Z80_reset 3
#define Z80_load  4
#define Z80_save  5
#define Z80_log   6

static inline uint8_t fetch(uint16_t x)
{
	return active_session->z80_memory[x];
}

static inline uint16_t fetch2(uint16_t x)
{
	uint16_t retval;
	retval  = active_session->z80_memory[x];
	x++;
	retval |= (active_session->z80_memory[x]<<8);
	return retval;
}

static void store (uint16_t ad, uint8_t b)
{
	active_session->z80_memory[ad] = b;
}

static uint16_t pc;

void store2b (uint16_t ad, uint8_t hi, uint8_t lo)
{
	active_session->z80_memory[ad] = lo;
	ad++;
	active_session->z80_memory[ad] = hi;
}

void store2(uint16_t ad, uint16_t w)
{
	store2b(ad,(w)>>8,(w)&255);
}

#define bc ((b<<8)|c)
#define de ((d<<8)|e)
#define hl ((h<<8)|l)


unsigned int ay_in(int h,int l)
{
	return 0;
}

unsigned int ay_out(int h,int l,int a)
{
	return 0;
}
int ay_do_interrupt(void)
{
	return 0;
}

unsigned long ay_tstates,ay_tsmax;

#define Z80_DISABLE_INTERRUPT

#include "z80.c"

















#include "dumpay_z80_dis.c"

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

static char safeprintchar (const char input)
{
	if ((input >= 32) && (input < 127))
	{
		return input;
	}
	return ' ';
}

static void tryprint_string (const unsigned char *buffer, int length, uint16_t ptr)
{
	int i;
	if (ptr >= length)
	{
		printf ("points to outside the file");
		return;
	}
	for (i=ptr; ;i++)
	{
		if (i >= length)
		{
			printf ("string not null-terminated");
			return;
		}
		if (buffer[i] == 0)
		{
			break;
		}
		if ((buffer[i] < 32) || (buffer[i] > 127))
		{
			printf ("string contains non-ascii characters");
			return;
		}
	}
	printf ("\"%s\"", buffer + ptr);
}

static void tryprint_points (const unsigned char *buffer, int length, uint16_t i_ptr, uint16_t *init, uint16_t *interrupt, uint16_t *sp)
{
	if (i_ptr+6 > length)
	{
		printf ("   (points to outside the file)");
		return;
	}

	printf ("   Stack/SP:  0x%04x\n", *sp = ((buffer[i_ptr+0] << 8) | buffer[i_ptr+1]));
	printf ("   Init:      0x%04x\n", *init = ((buffer[i_ptr+2] << 8) | buffer[i_ptr+3]));
	printf ("   Interrupt: 0x%04x\n", *interrupt = ((buffer[i_ptr+4] << 8) | buffer[i_ptr+5]));
}

static void tryprint_addresses (const unsigned char *buffer, int length, uint16_t i_ptr, uint16_t init, uint16_t interrupt)
{
	uint16_t mem_ptr;
	uint16_t mem_length;
	uint16_t rel_ptr;
	uint16_t ptr;
	int overflow_source;
	int overflow_target;
	int setup = 0;

	while (1)
	{
		if (i_ptr+2 > length)
		{
			printf ("   (points to outside the file)");
			return;
		}
		if ((buffer[i_ptr] == 0) && (buffer[i_ptr + 1] == 0))
		{
			printf ("   0x0000 ENDWORD\n");
			return;
		}
		if (i_ptr+6 > length)
		{
			printf ("   (points to outside the file)");
			return;
		}

		mem_ptr         = (buffer[i_ptr+0] << 8) | buffer[i_ptr+1];
		mem_length      = (buffer[i_ptr+2] << 8) | buffer[i_ptr+3];
		rel_ptr         = (buffer[i_ptr+4] << 8) | buffer[i_ptr+5]; ptr = rel_ptr + i_ptr + 4;
		overflow_source = ((int)ptr+mem_length) > length;
		overflow_target = ((int)mem_ptr+mem_length) > 0x10000;
		printf ("   Target[0x%04x] Length %d: REL PTR 0x%04x => 0x%04x%s%s\n",
			mem_ptr,
			mem_length,
			rel_ptr, ptr,
			overflow_source?" (reads past end of file, will be truncated)":"",
			overflow_target?" (writes past end of memorymap, will be truncated)":"");

		if (!setup)
		{
			if (!init)
			{
				init = mem_ptr;
			}
			memset (&session_a.z80_memory[0x0000], 0xc9, 0x0100);
			memset (&session_a.z80_memory[0x0100], 0xff, 0x3f00);
			memset (&session_a.z80_memory[0x4000], 0x00, 0xc000);

			session_a.z80_memory[0x0038] = 0xfb;

			if (!interrupt)
			{
				static const unsigned char intz[] =
				{
					0xf3,         /* di */
					0xcd,0,0,     /* call init */
					0xed,0x5e,    /* loop: im 2 */
					0xfb,         /* ei */
					0x76,         /* halt */
					0x18,0xfa     /* jr loop */
				};
				memcpy (session_a.z80_memory, intz, sizeof (intz));
			} else {
				static const unsigned char intnz[] =
				{
					0xf3,         /* di */
					0xcd,0,0,     /* call init */
					0xed,0x56,    /* loop: im 1 */
					0xfb,         /* ei */
					0x76,         /* halt */
					0xcd,0,0,     /* call interrupt */
					0x18,0xf7     /* jr loop */
				};
				memcpy (session_a.z80_memory, intnz, sizeof (intnz));
				session_a.z80_memory[0x0009] = interrupt;
				session_a.z80_memory[0x000a] = interrupt >> 8;
			}
			session_a.z80_memory[0x0002] = init;
			session_a.z80_memory[0x0003] = init >> 8;
			setup = 1;
		}

		if (((int)mem_ptr+mem_length) > 0x10000)
		{
			mem_length = 0x10000 - mem_ptr;
		}
		if (((int)ptr+mem_length) > length)
		{
			int from_source = length - mem_ptr;
			int fill = mem_length - from_source;
			memcpy (session_a.z80_memory + mem_ptr, buffer + ptr, length - mem_ptr);
			bzero (session_a.z80_memory + mem_ptr + from_source, fill);
		} else {
			memcpy (session_a.z80_memory + mem_ptr, buffer + ptr, mem_length);
		}

		i_ptr += 6;
	}
}

static void tryprint_songdata (const unsigned char *buffer, int length, uint16_t i_ptr, uint16_t *sp)
{
	uint16_t rel_ptr;
	uint16_t ptr;
	int i;
	int perm[4] = {0,0,0,0};
	uint16_t init = 0, interrupt = 0;

	if (i_ptr+14 > length)
	{
		printf ("  (points to outside the file)");
		return;
	}

	printf ("  AChan: %d (Amiga channel mapping) - %s\n", buffer[i_ptr+0], (buffer[i_ptr+0]<4) ? "valid" : "invalid");
	printf ("  BChan: %d (Amiga channel mapping) - %s\n", buffer[i_ptr+1], (buffer[i_ptr+1]<4) ? "valid" : "invalid");
	printf ("  CChan: %d (Amiga channel mapping) - %s\n", buffer[i_ptr+2], (buffer[i_ptr+2]<4) ? "valid" : "invalid");
	printf ("  noise: %d (Amiga channel mapping) - %s\n", buffer[i_ptr+3], (buffer[i_ptr+3]<4) ? "valid" : "invalid");
	for (i=0; i<4; i++)
	{
		if (buffer[i_ptr+i] < 4)
		{
			perm[buffer[i_ptr+i]]++;
		}
	}
	if ((perm[0] == 1) && (perm[1] == 1) && (perm[2] == 1) && (perm[3] == 1))
	{
		printf ("  (channel permutation is valid)\n");
	} else {
		printf ("  (channel permutation is INVALID)\n");
	}

	i = (buffer[i_ptr+4]<<8) | buffer[i_ptr+5];
	printf ("  SongLength: %d (%d.%02d s)\n", i, i/50, (i%50)*2);

	i = (buffer[i_ptr+6]<<8) | buffer[i_ptr+7];
	printf ("  FadeLength: %d (%d.%02d s)\n", i, i/50, (i%50)*2);

	printf ("  HiReg: 0x%02x\n", buffer[i_ptr+8]);
	printf ("  LoReg: 0x%02x\n", buffer[i_ptr+9]);

	rel_ptr = (buffer[i_ptr+10]<<8) | buffer[i_ptr+11]; ptr = rel_ptr + i_ptr + 10;
	printf ("  Points: REL PTR 0x%04x => 0x%04x\n", rel_ptr, ptr);
	tryprint_points (buffer, length, ptr, &init, &interrupt, sp);

	rel_ptr = (buffer[i_ptr+12]<<8) | buffer[i_ptr+13]; ptr = rel_ptr + i_ptr + 12;
	printf ("  Addresses: REL PTR 0x%04x => 0x%04x\n", rel_ptr, ptr);
	tryprint_addresses (buffer, length, ptr, init, interrupt);
}

uint16_t *todo_ptrs = 0;
int      todo_n = 0;
int      todo_size = 0;

static void predisassemble_session_start(struct z80_session_t *s)
{
	bzero (s->z80_flags, sizeof (s->z80_flags));
}

static void predisassemble_session_add(struct z80_session_t *s, uint16_t alt_ptr)
{
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

static void predisassemble_session_stop(struct z80_session_t *s, int recursive)
{
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

		if (s->z80_flags[ptr] & FLAG_CODE_DIRECT)
		{
#ifdef DEBUG_TODO
			fprintf (stderr, "pre-emptive remove, already scanned\n");
#endif
			memmove (todo_ptrs, todo_ptrs+1, sizeof (todo_ptrs[0]) * (todo_n - 1));
			todo_n--;
			continue;
		}

		retval = disassemble (s->z80_memory, ptr, opcode, param1, param2, comment, &length, &alt_ptr);
		if (retval != -2)
		{
			s->z80_flags[ptr] |= length; /* matches up with FLAG_CODE_DIRECT */
			if (length>1)
			{
				s->z80_flags[ptr+1] |= FLAG_CODE_INDIRECT0;
			}
			if (length>2)
			{
				s->z80_flags[ptr+2] |= FLAG_CODE_INDIRECT1;
			}
			if (length>3)
			{
				s->z80_flags[ptr+3] |= FLAG_CODE_INDIRECT2;
			}
			todo_ptrs[0] += length;

			if ((!recursive)||(retval == -1)||(retval == 2)||((todo_n > 1) && (todo_ptrs[0] == todo_ptrs[1])))
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
#ifdef DEBUG_TODO
					fprintf (stderr, "swapping todo[0] and todo[1]\n");
#endif
				}
			}
		} else {
			/* failed, remove from list */
#ifdef DEBUG_TODO
			fprintf (stderr, "removing todo[0], due to failure of decoding\n");
#endif
			memmove (todo_ptrs, todo_ptrs+1, sizeof (todo_ptrs[0]) * (todo_n - 1));
			todo_n--;
		}
		if (recursive && ((retval == 1) || (retval == 2)))
		{
			if (s->z80_flags[alt_ptr] & FLAG_CODE_DIRECT)
			{
#ifdef DEBUG_TODO
				fprintf (stderr, "Not adding %d, already decoded\n", alt_ptr);
#endif
			} else {
				predisassemble_session_add (s, alt_ptr);
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
}

static void predisassemble_session(struct z80_session_t *s)
{
	predisassemble_session_start (s);
	predisassemble_session_add (s, 0x0000);
	predisassemble_session_add (s, 0x0008);
	predisassemble_session_add (s, 0x0038);
	predisassemble_session_stop (s, 1);
}

static int disassemble_session(struct z80_session_t *s, char *b, int blen)
{
	char opcode[16];
	char param1[16];
	char param2[16];
	char comment[32];
	int length;
	uint16_t alt_ptr;
	int retval = 1;

	if ((length=(s->z80_flags[s->ptr] & FLAG_CODE_DIRECT)))
	{
		char combined[21];
		opcode[0] = 0;
		param1[0] = 0;
		param2[0] = 0;
		comment[0] = 0;

		disassemble (s->z80_memory, s->ptr, opcode, param1, param2, comment, &length, &alt_ptr);

		snprintf (combined, sizeof (combined), "%s %s%s%s%s%s", opcode, param1, param2[0]?", ":"", param2, comment[0]?" # ":"", comment);

		switch (length)
		{
			case 1: snprintf (b, blen, "%s%04x %s%02x __ __ __%s %-20s",
					FONT_BRIGHT_BLUE,
					s->ptr,
					s->z80_flags[s->ptr] & FLAG_CODE_INDIRECT ? FONT_BRIGHT_RED : FONT_BRIGHT_PURPLE,
					s->z80_memory[s->ptr],
					FONT_RESET,
					combined); break;
			case 2: snprintf (b, blen, "%s%04x %s%02x %02x __ __%s %-20s",
					FONT_BRIGHT_BLUE,
					s->ptr,
					s->z80_flags[s->ptr] & FLAG_CODE_INDIRECT ? FONT_BRIGHT_RED : FONT_BRIGHT_PURPLE,
					s->z80_memory[s->ptr], s->z80_memory[s->ptr+1],
					FONT_RESET,
					combined); break;
			case 3: snprintf (b, blen, "%s%04x %s%02x %02x %02x __%s %-20s",
					FONT_BRIGHT_BLUE,
					s->ptr,
					s->z80_flags[s->ptr] & FLAG_CODE_INDIRECT ? FONT_BRIGHT_RED : FONT_BRIGHT_PURPLE,
					s->z80_memory[s->ptr], s->z80_memory[s->ptr+1], s->z80_memory[s->ptr+2],
					FONT_RESET,
					combined); break;
			case 4: snprintf (b, blen, "%s%04x %s%02x %02x %02x %02x%s %-20s",
					FONT_BRIGHT_BLUE,
					s->ptr,
					s->z80_flags[s->ptr] & FLAG_CODE_INDIRECT ? FONT_BRIGHT_RED : FONT_BRIGHT_PURPLE,
					s->z80_memory[s->ptr], s->z80_memory[s->ptr+1], s->z80_memory[s->ptr+2], s->z80_memory[s->ptr+3],
					FONT_RESET,
					combined); break;
		}
#if 0
		for (j=1; j < length; j++)
		{
			if (s->z80_flags[s->ptr+j] & FLAG_CODE_DIRECT)
			{
				snprintf (b, blen, "%sANTI DISASSEMBLER DETECTED at PTR %s%04x%s\n", FONT_BRIGHT_RED, FONT_BRIGHT_BLUE, j + s->ptr, FONT_RESET);
			}
		}
#endif
		s->ptr++;
		s->prev = -1;
		s->prevprev = -1;
	} else if (s->z80_flags[s->ptr] & FLAG_CODE_INDIRECT)
	{
		s->ptr++;
		retval = 0;
	} else {
		if (!((s->z80_memory[s->ptr] == s->prev) && ((s->prev == 0x00) || (s->prev == 0xff) || (s->prev == 0xc9))))
		{
			snprintf (b, blen, "%s%04x %s%02x%s          " "                    ",
				FONT_BRIGHT_BLUE,
				s->ptr,
				FONT_BRIGHT_PURPLE,
				s->z80_memory[s->ptr],
				FONT_RESET);
			s->prev = s->z80_memory[s->ptr];
			s->prevprev = -1;
		} else if (s->prevprev < 0)
		{
			snprintf (b, blen, "...              " "                    ");
			s->prevprev = s->prev;
		} else {
			retval = 0;
		}
		s->ptr++;
	}

	return retval;
}


static void tryprint_songs (const unsigned char *buffer, int length, uint16_t i_ptr, int songs)
{
	int i, j, k;

	for (i=0; i < songs; i++)
	{
		uint16_t sp = 0;
		uint16_t ptr;
		uint16_t rel_ptr;

		printf ("SONG %d\n", i);
		if (((int)i_ptr+4) > length)
		{
			printf (" (points to outside the file)");
			continue;
		}

		rel_ptr = (buffer[i_ptr] << 8) | buffer[i_ptr+1]; ptr = rel_ptr + i_ptr;
		printf (" SongName: REL_PTR 0x%04x => 0x%04x ", rel_ptr, ptr);
		tryprint_string (buffer, length, ptr);

		rel_ptr = (buffer[i_ptr+2] << 8) | buffer[i_ptr+3]; ptr = rel_ptr + i_ptr + 2;
		printf ("\n SongData: REL_PTR 0x%04x => 0x%04x\n", rel_ptr, ptr);
		tryprint_songdata (buffer, length, ptr, &sp);
		i_ptr += 4;

		predisassemble_session (&session_a);

		memcpy (session_b.z80_memory, session_a.z80_memory, sizeof (session_b.z80_memory));
		active_session = &session_b;
		predisassemble_session_start (&session_c);
		{
			uint8_t stack[2];
			stack[0] = sp << 8;
			stack[1] = sp;
			ay_z80_init (buffer + ptr, stack);
		}
		for (j=0; j < 100000; j++)
		{
			ay_tstates = 0;
			ay_tsmax = 1;
			ay_z80loop();
			if (intsample || (op == 0xf3) || (op == 0xfb))
			{
				predisassemble_session_add (&session_c, pc);
				if ((op==0x76) && j)
				{
					break;
				}
			}
		}
		predisassemble_session_stop (&session_b, 0);

		memcpy (session_c.z80_memory, session_b.z80_memory, sizeof (session_c.z80_memory));
		active_session = &session_c;
		predisassemble_session_start (&session_c);
		for (k=0; k < 120*50; k++) /* 120 seconds * 50 frames */
		{
			interrupted = 1;
			for (j=0; j < 100000; j++)
			{
				ay_tstates = 0;
				ay_tsmax = 1;
				ay_z80loop();
				if (intsample || (op == 0xf3) || (op == 0xfb))
				{
					predisassemble_session_add (&session_c, pc);
					if ((op==0x76) && j)
					{
						break;
					}
				}
			}
		}
		predisassemble_session_stop (&session_c, 0);

		session_a.ptr = 0x0000;
		session_a.prev = -1;
		session_a.prevprev = -1;

		session_b.ptr = 0x0000;
		session_b.prev = -1;
		session_b.prevprev = -1;

		session_c.ptr = 0x0000;
		session_c.prev = -1;
		session_c.prevprev = -1;

		printf ("         STATIC CODE ANALYZER         ||         RUNNING INIT                  ||         RUNNING MUSIC                \n");

		while ((session_a.ptr < 0x10000) && (session_b.ptr < 0x10000) && (session_c.ptr < 0x10000))
		{
			char buffer_a[256];
			char buffer_b[256];
			char buffer_c[256];
			int reta;
			int retb;
			int retc;
			reta = disassemble_session (&session_a, buffer_a, sizeof (buffer_a));
			retb = disassemble_session (&session_b, buffer_b, sizeof (buffer_b));
			retc = disassemble_session (&session_c, buffer_c, sizeof (buffer_c));
			if ((!reta) && (!retb) && (!retc))
			{
				continue;
			}
			if (!reta)
			{
				snprintf (buffer_a, sizeof (buffer_a), "                                     ");
			}
			if (!retb)
			{
				snprintf (buffer_b, sizeof (buffer_b), "                                     ");
			}
			if (!retc)
			{
				snprintf (buffer_c, sizeof (buffer_c), "                                     ");
			}

			printf ("%s || %s || %s\n", buffer_a, buffer_b, buffer_c);
		}
	}
}

static void parse_ayfile (const unsigned char *buffer, int length)
{
	uint16_t rel_ptr;
	uint16_t ptr;

	if (length < 8)
	{
		printf ("File too short to contain atom\n");
		return;
	}

	if (memcmp (buffer, "ZXAYEMUL", 8))
	{
		printf ("Invalid atom, expected \"ZXAYEMUL\", but got \"%c%c%c%c%c%c%c%c\"\n",
			safeprintchar (buffer[0]),
			safeprintchar (buffer[1]),
			safeprintchar (buffer[2]),
			safeprintchar (buffer[3]),
			safeprintchar (buffer[4]),
			safeprintchar (buffer[5]),
			safeprintchar (buffer[6]),
			safeprintchar (buffer[7]));
		return;
	}
	printf ("\"ZXAYEMUL\" atom found\n");

	buffer += 8; length -= 8;

	if (length < 12)
	{
		printf ("Not enough data to contain header\n");
		return;
	}

	printf ("FileVersion: %d\n", buffer[0]);

	printf ("PlayerVersion: %d ", buffer[1]);
	switch (buffer[1])
	{
		case 0: printf ("(no special requirement)\n"); break;
		case 1: printf ("(Initial player version)\n"); break;
		case 2: printf ("(first 256 bytes are filled with 0xc9 (ret))\n"); break;
		case 3: printf ("(PC rewrite version, new emulator, support for 48K tunes)\n"); break;
		default: printf ("(unknown version)\n"); break;
	}

	rel_ptr = (buffer[2] << 8) | buffer[3]; ptr = rel_ptr + 2;
	printf ("SpecialPlayer: REL PTR 0x%04x => 0x%04x (only used by one custom song, M68K assembler)\n", rel_ptr, ptr);

	rel_ptr = (buffer[4] << 8) | buffer[5]; ptr = rel_ptr + 4;
	printf ("Author: REL PTR 0x%04x => 0x%04x ", rel_ptr, ptr);
	tryprint_string (buffer, length, ptr);

	rel_ptr = (buffer[6] << 8) | buffer[7]; ptr = rel_ptr + 6;
	printf ("\nMisc: REL PTR 0x%04x => 0x%04x ", rel_ptr, ptr);
	tryprint_string (buffer, length, ptr);

	printf ("\nNumSongs: %d\n", buffer[8]+1); /* both numbers are stored -1 in the file */
	printf ("FirstSong: %d\n", buffer[9]+1);

	rel_ptr = (buffer[10] << 8) | buffer[11]; ptr = rel_ptr + 10;
	printf ("SongStructure: REL PTR 0x%04x => 0x%04x\n", rel_ptr, ptr);

	tryprint_songs (buffer, length, ptr, buffer[8]+1);
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
		fprintf (stderr, "Usage:\n%s [--color=auto/never/on] [--help] file.ay  (%d)\n", argv[0], help);
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

	parse_ayfile (buffer, length);

	free (buffer);

	return 0;
}
