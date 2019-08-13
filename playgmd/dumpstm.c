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
char *FONT_BRIGHT_YELLOW = "";
char *FONT_BRIGHT_BLUE = "";
char *FONT_BRIGHT_PURPLE = "";
char *FONT_BRIGHT_CYAN = "";

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

int DumpInstrument (unsigned char *mem, int len, int base, int instrument)
{
	uint16_t ParaPtr    = uint16_little (((uint16_t *)(mem + base + 0x0e))[0]);
	uint16_t Length     = uint16_little (((uint16_t *)(mem + base + 0x10))[0]);
	uint16_t LoopStart  = uint16_little (((uint16_t *)(mem + base + 0x12))[0]);
	uint16_t LoopEnd    = uint16_little (((uint16_t *)(mem + base + 0x14))[0]);
	uint16_t C3Spd      = uint32_little (((uint16_t *)(mem + base + 0x18))[0]);
	uint16_t LengthPara = uint16_little (((uint16_t *)(mem + base + 0x1e))[0]);
	int i;

	printf ("[%sINSTRUMENT %02x / PCM Sample%s]\n", FONT_BRIGHT_CYAN, instrument, FONT_RESET);

	DumpPrefix (mem, len, base + 0x01, 12);
	printf ("FileName: \"");
	for (i=0; i < 12; i++)
	{
		if (!mem[base + 0x00 + i])
		{
			break;
		}
		putchar (mem[base + 0x00 + i]);
	}
	printf("\"\n");

	DumpPrefix (mem, len, base + 0x0c, 1);
	printf ("Type: %s%s%s\n",
		(mem[base+0x0c] == 0x00) ? FONT_BRIGHT_GREEN : FONT_BRIGHT_RED,
		(mem[base+0x0c] == 0x00) ? "PCM Sample" : "Unknown",
		FONT_RESET);

	DumpPrefix (mem, len, base + 0x0d, 1);
	printf ("Disk: %d\n", mem[base+0x0d]);

	DumpPrefix (mem, len, base + 0x0e, 3);
	printf ("MemSeg: paraptr 0x%04x => 0x%08x\n", ParaPtr, ParaPtr << 4);

	DumpPrefix (mem, len, base + 0x10, 2);
	printf ("Length:     %d %s%s%s\n", (int)Length, Length>64000 ? FONT_BRIGHT_RED : FONT_BRIGHT_GREEN, (Length>64000) ? " Sample too long, will be cropped at 64000" : "", FONT_RESET);

	DumpPrefix (mem, len, base + 0x12, 2);
	printf ("Loop-Start: %d %s%s%s\n", (int)LoopStart, (LoopStart>Length) ? FONT_BRIGHT_RED : FONT_BRIGHT_GREEN, (LoopStart>Length) ? " LoopStart > Length" : "", FONT_RESET);

	DumpPrefix (mem, len, base + 0x14, 2);
	printf ("Loop-End: %d %s%s%s\n", (int)LoopEnd,
		((LoopEnd>(Length+1)) && (LoopEnd != 0xffff)) ? FONT_BRIGHT_RED : FONT_BRIGHT_GREEN,
		((LoopEnd>(Length+1)) && (LoopEnd != 0xffff)) ? " LoopEnd > Length + 1" : (LoopEnd == 0xffff) ? "No loop" : "", FONT_RESET);

	DumpPrefix (mem, len, base + 0x16, 1);
	printf ("Volume: %d %s%s%s\n", mem[base+0x16], (mem[base+0x16] > 64) ? FONT_BRIGHT_RED : FONT_BRIGHT_GREEN, (mem[base+0x16] > 64) ? " This is greater than 64" : "", FONT_RESET);

	DumpPrefix (mem, len, base + 0x17, 1);
	printf ("Reserved/unused\n");

	DumpPrefix (mem, len, base + 0x18, 2);
	printf ("C3 Sample Rate: %d\n", C3Spd);
	
	DumpPrefix (mem, len, base + 0x1a, 4);
	printf ("Reserved / Internal use / leave at 0\n");

	DumpPrefix (mem, len, base + 0x1e, 2);
	printf ("Length-Para: %d (modules only)\n", LengthPara);

	return 0;
}

void print_note (unsigned char note)
{
	char *text[16] = {"C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-", "?c", "?d", "?e", "?f"};

	if (note == 0xfe)
	{
		printf ("-0-");
		return;
	}
	if ((note >= 0x60) || (note == 0x00))
	{
		printf ("...");
		return;
	}

	if ((note & 0x0f) >= 12) printf ("%s", FONT_BRIGHT_RED);
	printf ("%s", text[note & 0x0f]);
	if ((note & 0x0f) >= 12) printf ("%s", FONT_RESET);

	printf ("%d", note >> 4);
}

int DumpPattern (unsigned char *mem, int len, int base, int pattern)
{
	int offset = 0;
	int i;

	printf ("[%sPATTERN %d%s]\n", FONT_BRIGHT_CYAN, pattern, FONT_RESET);

	for (i=0; i < 64; i++)
	{
		int j;

		{
			int preoffset = offset; /* so we can rewind */

			for (j=0; j < 4; j++)
			{
				switch (mem[base + preoffset])
				{
					case 0xfb:
					case 0xfc:
					case 0xfd: preoffset += 1; break;
					default:   preoffset += 4; break;
				}
			}

			DumpPrefix (mem, len, base + offset, preoffset - offset);


		}

		printf ("%02X |", i);

		for (j=0; j < 4; j++)
		{
			uint8_t note, insvol, volcmd, cmdinf;
			switch (mem[base+offset])
			{
				case 0xfb: offset += 1; note = insvol = volcmd = cmdinf = 0x00; break;
				case 0xfc: offset += 1; printf (" ... .. .. .00 |"); goto next;
				case 0xfd: offset += 1; printf(" -0- .. .. .00 |"); goto next;
				default:   note=mem[base + offset++]; insvol=mem[base + offset++]; volcmd=mem[base + offset++]; cmdinf=mem[base + offset++]; break;
			}

			printf (" ");

			print_note(note);

			printf (" ");

			if ((insvol >> 3) > 31)
			{
				printf ("..");
			} else {
				printf ("%02X", insvol>>3);
			}

			printf (" ");

			if ((note >= 0x60) /* || (((insvol & 0x07) | ((voldcmd & 0xf0) >> 1)) > 64) */)
			{
				printf ("..");
			} else {
				printf ("%02X", (insvol & 0x07) | ((volcmd & 0x70) >> 1));
			}

			printf (" ");

			printf ("%c", ".ABCDEFGHIJKLMNO"[volcmd & 0x0f]);
			printf ("%02X |", cmdinf);

			next: {};
		}
		printf ("\n");
	}

	return offset;
}

int ParseSTM (unsigned char *mem, int len)
{
	int i, j;
	int patternlen = 0;
	if (len < sizeof (0x30))
	{
		fprintf (stderr, "*** len < sizeof(FileHeader) ***\n");
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

	for (i=0; i < 31; i++)
	{
		DumpInstrument (mem, len, 0x30 + i*0x20, i);
	}	

	printf ("[%sOrders%s]\n", FONT_BRIGHT_CYAN, FONT_RESET);
	for (i=0; i < (mem[0x1f]?128:64); i++)
	{
		if ((i&15) != 0)
		{
			putchar (' ');
		} else {
			DumpPrefix (mem, len, 0x410 + i, 16);
		};
		printf ("%d%s%s", mem[0x410+i],
		mem[0x410+i]==99?" (ignore) ":"",
		mem[0x410+i]==255?" (EOS) ":""
		);
		if ((i&15) == 15)
		{
			putchar ('\n');
		}
	}
	printf ("]\n");

	for (i=0; i < mem[0x21]; i++)
	{
		patternlen += DumpPattern (mem, len, 0x30 + 0x20*31 + (mem[0x1f]?128:64) + patternlen, i);
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
		FONT_BRIGHT_YELLOW = "\033[33;1m";
		FONT_BRIGHT_BLUE   = "\033[34;1m";
		FONT_BRIGHT_PURPLE = "\033[35;1m";
		FONT_BRIGHT_CYAN   = "\033[36;1m";
	}

	if (ParseSTM (data, data_mmaped_len))
	{
		goto failed;
	}

failed:
	munmap (data, data_mmaped_len);
	close (fd);
	return 0;
}
