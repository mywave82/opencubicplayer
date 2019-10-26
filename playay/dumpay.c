#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* file-format spec source: http://vgmrips.net/wiki/AY_File_Format */

#include "dumpay_z80_dis.c"

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

static void tryprint_points (const unsigned char *buffer, int length, uint16_t i_ptr, uint16_t *init, uint16_t *interrupt)
{
	if (i_ptr+6 > length)
	{
		printf ("   (points to outside the file)");
		return;
	}

	printf ("   Stack/SP:  0x%04x\n", (buffer[i_ptr+0] << 8) | buffer[i_ptr+1]);
	printf ("   Init:      0x%04x\n", *init = ((buffer[i_ptr+2] << 8) | buffer[i_ptr+3]));
	printf ("   Interrupt: 0x%04x\n", *interrupt = ((buffer[i_ptr+4] << 8) | buffer[i_ptr+5]));
}

static unsigned char z80_memory[65536];
static unsigned char z80_flags[65536];
#define FLAG_CODE_DIRECT0    1
#define FLAG_CODE_DIRECT1    2
#define FLAG_CODE_DIRECT2    3
#define FLAG_CODE_DIRECT     7

#define FLAG_CODE_INDIRECT0   8
#define FLAG_CODE_INDIRECT1  16
#define FLAG_CODE_INDIRECT2  32
#define FLAG_CODE_INDIRECT   56


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
		rel_ptr         = (buffer[i_ptr+4] << 8) | buffer[i_ptr+5]; ptr = rel_ptr + i_ptr + 2;
		overflow_source = ((int)ptr+mem_length) > length;
		overflow_target = ((int)mem_ptr+mem_length) > 0x10000;
		printf ("   Target[0x%04x] Length %d: REL PTR 0x%04x => 0x%04x%s%s\n",
			mem_ptr,
			mem_length,
			rel_ptr, ptr,
			overflow_source?" (reads past end of file, will be truncated)":"",
			overflow_target?" (writes past end of memorymap, will be truncated)":"");
#warning dump_memory

		if (!setup)
		{
			if (!init)
			{
				init = mem_ptr;
			}
			memset (z80_memory +      0, 0xc9, 0x0100);
			memset (z80_memory + 0x0100, 0xff, 0x3f00);
			memset (z80_memory + 0x4000, 0x00, 0xc000);

			z80_memory[0x38] = 0xfb;

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
				memcpy (z80_memory, intz, sizeof (intz));
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
				memcpy (z80_memory, intnz, sizeof (intnz));
				z80_memory[0x0009] = interrupt;
				z80_memory[0x000a] = interrupt >> 8;
			}
			z80_memory[0x0002] = init;
			z80_memory[0x0003] = init >> 8;
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
			memcpy (z80_memory + mem_ptr, buffer + ptr, length - mem_ptr);
			bzero (z80_memory + mem_ptr + from_source, fill);
		} else {
			memcpy (z80_memory + mem_ptr, buffer + ptr, mem_length);
		}

		i_ptr += 6;
	}
}

static void tryprint_songdata (const unsigned char *buffer, int length, uint16_t i_ptr)
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
	tryprint_points (buffer, length, ptr, &init, &interrupt);

	rel_ptr = (buffer[i_ptr+12]<<8) | buffer[i_ptr+13]; ptr = rel_ptr + i_ptr + 12;
	printf ("  Addresses: REL PTR 0x%04x => 0x%04x\n", rel_ptr, ptr);
	tryprint_addresses (buffer, length, ptr, init, interrupt);
}

void breakme()
{

}

static void tryprint_songs (const unsigned char *buffer, int length, uint16_t i_ptr, int songs)
{
	uint16_t rel_ptr;
	uint16_t ptr;
	int i;

	for (i=0; i < songs; i++)
	{
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
		tryprint_songdata (buffer, length, ptr);
		i_ptr += 4;

		{
			uint16_t *todo_ptrs = malloc (sizeof (uint16_t *)*16);
			int      todo_n = 0;
			int      todo_size = 16;

			bzero (z80_flags, sizeof (z80_flags));

			todo_ptrs[0] = 0x0000;
			todo_ptrs[1] = 0x0008;
			todo_ptrs[2] = 0x0038;
			todo_n = 3;

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

				if (z80_flags[ptr] & FLAG_CODE_DIRECT)
				{
					fprintf (stderr, "pre-emptive remove, already scanned\n");
					memmove (todo_ptrs, todo_ptrs+1, sizeof (todo_ptrs[0]) * (todo_n - 1));
					todo_n--;
					continue;
				}

				retval = disassemble (z80_memory, ptr, opcode, param1, param2, comment, &length, &alt_ptr);
				if (retval != -2)
				{
					z80_flags[ptr] |= length; /* matches up with FLAG_CODE_DIRECT */
					if (z80_flags[ptr] & FLAG_CODE_INDIRECT)
					{
						breakme();
					}
					if (length>1)
					{
						if (z80_flags[ptr+1] & FLAG_CODE_DIRECT)
						{
							breakme();
						}
						z80_flags[ptr+1] |= FLAG_CODE_INDIRECT0;
					}
					if (length>2)
					{
						if (z80_flags[ptr+2] & FLAG_CODE_DIRECT)
						{
							breakme();
						}
						z80_flags[ptr+2] |= FLAG_CODE_INDIRECT1;
					}
					if (length>3)
					{
						if (z80_flags[ptr+3] & FLAG_CODE_DIRECT)
						{
							breakme();
						}
						z80_flags[ptr+3] |= FLAG_CODE_INDIRECT2;
					}
					todo_ptrs[0] += length;

					if ((retval == -1)||(retval == 2)||((todo_n > 1) && (todo_ptrs[0] == todo_ptrs[1])))
					{
						fprintf (stderr, "removing todo[0], due to jump or duplication\n");
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
					fprintf (stderr, "removing todo[0], due to failure of decoding\n");
					memmove (todo_ptrs, todo_ptrs+1, sizeof (todo_ptrs[0]) * (todo_n - 1));
					todo_n--;
				}
				if ((retval == 1) || (retval == 2))
				{
					if (z80_flags[alt_ptr] & FLAG_CODE_DIRECT)
					{
						fprintf (stderr, "Not adding %d, already decoded\n", alt_ptr);
					} else {
						int i;
						int skip = 0;
						for (i=0;i<todo_n;i++)
						{
							if (todo_ptrs[i] == alt_ptr)
							{
								fprintf (stderr, "Skipping injecting, already on list %04x\n", alt_ptr);
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
							fprintf (stderr, "Injected todo[%d]=%04x\n", i, alt_ptr);
						}
					}
				}

				{
					int i;
					for (i=0;i<todo_n;i++)
					{
						fprintf (stderr, "%s%04x", i?", ":"", todo_ptrs[i]);
					}
					fprintf (stderr, "\n");
				}
			}
			free (todo_ptrs);
		}

		{
			int prev = -1;
			uint32_t ptr;

			for (ptr=0; ptr < 0x10000;)
			{
				char opcode[16];
				char param1[16];
				char param2[16];
				char comment[32];
				int length;
				uint16_t alt_ptr;

				if ((length=(z80_flags[ptr] & FLAG_CODE_DIRECT)))
				{
					int j;

					opcode[0] = 0;
					param1[0] = 0;
					param2[0] = 0;
					comment[0] = 0;

					disassemble (z80_memory, ptr, opcode, param1, param2, comment, &length, &alt_ptr);
					switch (length)
					{
						case 1: printf ("%04x %02x __ __ __ %s %s%s%s%s%s\n", ptr, z80_memory[ptr], opcode, param1, param2[0]?", ":"", param2, comment[0]?" # ":"", comment); break;
						case 2: printf ("%04x %02x %02x __ __ %s %s%s%s%s%s\n", ptr, z80_memory[ptr], z80_memory[ptr+1], opcode, param1, param2[0]?", ":"", param2, comment[0]?" # ":"", comment); break;
						case 3: printf ("%04x %02x %02x %02x __ %s %s%s%s%s%s\n", ptr, z80_memory[ptr], z80_memory[ptr+1], z80_memory[ptr+2], opcode, param1, param2[0]?", ":"", param2, comment[0]?" # ":"", comment); break;
						case 4: printf ("%04x %02x %02x %02x %02x %s %s%s%s%s%s\n", ptr, z80_memory[ptr], z80_memory[ptr+1], z80_memory[ptr+2], z80_memory[ptr+3], opcode, param1, param2[0]?", ":"", param2, comment[0]?" # ":"", comment); break;
					}

					for (j=1; j < length; j++)
					{
						if (z80_flags[ptr+j] & FLAG_CODE_DIRECT)
						{
							printf ("ANTI DISASSEMBLER DETECTED at PTR %04x\n", j + ptr);
						}
					}
					ptr++;
					prev = -1;
				} else if (z80_flags[ptr] & FLAG_CODE_INDIRECT)
				{
					ptr++;
				} else {
					if (!((z80_memory[ptr] == prev) && ((prev == 0x00) || (prev == 0xff))))
					{
						printf ("%04x %02x\n", ptr, z80_memory[ptr]);
						prev = z80_memory[ptr];
					}

					ptr++;
				}
			}
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

	if (argc != 2)
	{
		fprintf (stderr, "Usage:\n %s file.ay\n", argv[0]);
		return -1;
	}

	fd = open (argv[1], O_RDONLY);
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

	parse_ayfile (buffer, length);

	free (buffer);

	return 0;
}
