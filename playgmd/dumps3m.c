#include "config.h"
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "types.h"

#define roundup(x,y) (((x) + (y) - 1) & ~((y) - 1))

typedef struct __attribute__((packed))
{
	char name[28];
	uint8_t sig,type;
	uint16_t d1;
	uint16_t orders,ins,pats,flags,cwt,ffv;
	char magic[4];
	uint8_t gv,is,it,mv,uc,dp;
	uint32_t d2;
	uint32_t d3;
	uint16_t special;
	uint8_t channels[32];
} FileHeader;
FileHeader hdr;

int ParseS3M (unsigned char *mem, int len)
{
	int i;
	int offset = 0;
	if (len < sizeof (FileHeader))
	{
		fprintf (stderr, "*** len < sizeof(FileHeader) ***\n");
		return 1;
	}

	memcpy (&hdr, mem, sizeof (FileHeader));
	hdr.d1      = uint16_little (hdr.d1);
	hdr.orders  = uint16_little (hdr.orders);
	hdr.ins     = uint16_little (hdr.ins);
	hdr.pats    = uint16_little (hdr.pats);
	hdr.flags   = uint16_little (hdr.flags);
	hdr.cwt     = uint16_little (hdr.cwt);
	hdr.ffv     = uint16_little (hdr.ffv);
	hdr.d2      = uint32_little (hdr.d2);
	hdr.d3      = uint32_little (hdr.d3);
	hdr.special = uint16_little (hdr.special);

	offset += sizeof (FileHeader);

	/* hdr.name */
	printf ("0x%04lx", offsetof (FileHeader, name));
	for (i=0; i < sizeof (hdr.name); i++)
	{
		printf (" %02x", (unsigned char)hdr.name[i]);
	}
	printf ("\n                   Name: \"");
	for (i=0; i < sizeof (hdr.name); i++)
	{
		if (!hdr.name[i])
			break;
		printf ("%c", hdr.name[i]);
	}
	printf ("\"\n");

	printf ("0x%04lx %02x          Signature: %s\n", offsetof (FileHeader, sig), hdr.sig, (hdr.sig == 0x1A) ? "OK" : "Failed!!!!");

	printf ("0x%04lx %02x          Type: ", offsetof (FileHeader, type), (hdr.type == 0x10));
	switch (hdr.type)
	{
		case  1: printf ("STM song w/o samples, FAILED !!!!!!\n"); return 1;
		case  2: printf ("STM module, FAILED !!!!!!\n"); return 1;
		case 16: printf ("S3M module\n"); break;
		default: printf ("Unknown, FAILED !!!!!\n"); return 1;
	}

	printf ("0x%04lx %02x %02x       Reserved (%s)\n",
		offsetof (FileHeader, d1),
		mem[offsetof (FileHeader, d1)],
		mem[offsetof (FileHeader, d1)+1],
		(hdr.d1 == 0) ? "OK" : "Failed!!!!");

	printf ("0x%04lx %02x %02x       Orders: %d%s\n",
		offsetof (FileHeader, orders),
		mem[offsetof (FileHeader, orders)],
		mem[offsetof (FileHeader, orders)+1],
		hdr.orders,
		hdr.orders & 1 ? " Warning, not an even number":"");

	printf ("0x%04lx %02x %02x       Instruments: %d\n",
		offsetof (FileHeader, ins),
		mem[offsetof (FileHeader, ins)],
		mem[offsetof (FileHeader, ins)+1],
		hdr.ins);

	printf ("0x%04lx %02x %02x       Patterns: %d\n",
		offsetof (FileHeader, pats),
		mem[offsetof (FileHeader, pats)],
		mem[offsetof (FileHeader, pats)+1],
		hdr.pats);

	printf ("0x%04lx %02x %02x       Flags:\n",
		offsetof (FileHeader, flags),
		mem[offsetof (FileHeader, flags)],
		mem[offsetof (FileHeader, flags)+1]);
	if (hdr.ffv == 1) /* All of these are deprecated */
	{
		printf ("                   1: [%c] st2vibrato\n", (hdr.flags & 1) ? 'x' : ' ');
		printf ("                   2: [%c] st2tempo\n", (hdr.flags & 2) ? 'x' : ' ');
		printf ("                   4: [%c] amigaslides\n", (hdr.flags & 4) ? 'x' : ' ');
		printf ("                  32: [%c] enable filter/sfx with SoundBlaster\n", (hdr.flags & 32) ? 'x' : ' ');
	}
	printf ("                   8: [%c] 0vol optimizations" /* " (Automatically turn off looping notes whose volume is zero for >2 note rows)" */ "\n", (hdr.flags & 8) ? 'x' : ' ');
	printf ("                  16: [%c] amiga limits" /*" (Disallow any notes that go beond the amiga hardware limits (like amiga does). This means that sliding up stops at B#5 etc. Also affects some minor amiga compatibility issues)" */ "\n", (hdr.flags & 16) ? 'x' : ' ');
	printf ("                  64: [%s] ST3.00 volume slides\n", (hdr.cwt == 0x1300) ? "implicit" : ((hdr.flags & 64) ? "x" : " "));
	printf ("                 128: [%c] special custom data in file\n", (hdr.flags & 128) ? 'x' : ' ');

	printf ("0x%04lx %02x %02x       Created With Tracker/Version: ",
		offsetof (FileHeader, cwt),
		mem[offsetof (FileHeader, cwt)],
		mem[offsetof (FileHeader, cwt)+1]);
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

	printf ("0x%04lx %02x %02x       FileFormatVersion: ",
		offsetof (FileHeader, ffv),
		mem[offsetof (FileHeader, ffv)],
		mem[offsetof (FileHeader, ffv)+1]);
	if (hdr.ffv == 0x0001)
	{
		printf("old version used long ago (samples signed)\n");
	} else if (hdr.ffv == 0x0002)
	{
		printf("standard (samples unsigned)\n");
	} else {
		printf("unknown!!!!!!!!\n");
	}

	printf ("0x%04lx %02x %02x %02x %02x Signature: \"%c%c%c%c\" %s\n",
		offsetof (FileHeader, magic),
		mem[offsetof (FileHeader, magic)],
		mem[offsetof (FileHeader, magic)+1],
		mem[offsetof (FileHeader, magic)+2],
		mem[offsetof (FileHeader, magic)+3],
		hdr.magic[0],
		hdr.magic[1],
		hdr.magic[2],
		hdr.magic[3],
		memcmp (hdr.magic, "SCRM", 4) ? "Failed!!!!" : "OK");

	printf ("0x%04lx %02x          Global Volume: %d\n",
		offsetof (FileHeader, gv),
		mem[offsetof (FileHeader, gv)],
		hdr.gv);
	
	printf ("0x%04lx %02x          Initial Speed: %d\n",
		offsetof (FileHeader, is),
		mem[offsetof (FileHeader, is)],
		hdr.is);

	printf ("0x%04lx %02x          Initial Tempo: %d\n",
		offsetof (FileHeader, it),
		mem[offsetof (FileHeader, it)],
		hdr.it);
	
	printf ("0x%04lx %02x          Master Volume: %d\n",
		offsetof (FileHeader, mv),
		mem[offsetof (FileHeader, mv)],
		hdr.mv);
	
	printf ("0x%04lx %02x          GUS hardware, click removal for %d channels\n",
		offsetof (FileHeader, uc),
		mem[offsetof (FileHeader, uc)],
		hdr.uc);

	printf ("0x%04lx %02x          Default Pan: %s\n",
		offsetof (FileHeader, dp),
		mem[offsetof (FileHeader, dp)],
		(hdr.dp == 252) ? "Use data from header" : "Ignore data in header, use system default");

	printf ("0x%04lx %02x %02x %02x %02x %02x %02x %02x %02x   (Reserved/not used)\n",
		offsetof (FileHeader, d2),
		mem[offsetof (FileHeader, d2)],
		mem[offsetof (FileHeader, d2)+1],
		mem[offsetof (FileHeader, d2)+2],
		mem[offsetof (FileHeader, d2)+3],
		mem[offsetof (FileHeader, d2)+4],
		mem[offsetof (FileHeader, d2)+5],
		mem[offsetof (FileHeader, d2)+6],
		mem[offsetof (FileHeader, d2)+7]);

	printf ("0x%04lx %02x %02x       SpecialData: ",
		offsetof (FileHeader, special),
		mem[offsetof (FileHeader, special)],
		mem[offsetof (FileHeader, special)+1]);
	if (hdr.flags & 128)
	{
		printf ("ParaPointer 0x%04x => ptr 0x%08lx\n", hdr.special, 16*hdr.special + sizeof (hdr));
	} else {
		printf ("not enabled in flags\n");
	}

	for (i=0; i < 32; i++)
	{
		printf ("0x%04lx %02x          ChannelSetting %2d: ",
			offsetof (FileHeader, channels) + i,
			mem[offsetof (FileHeader, channels) + i], i);
		if (hdr.channels[i] < 8)
		{
			printf ("Left PCM Channel %d\n", hdr.channels[i]);
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
	return 0;
}

int main(int argc, char *argv[])
{
	struct stat st;
	size_t ps = sysconf(_SC_PAGE_SIZE);
	int fd;
	size_t data_mmaped_len;
	unsigned char *data;

	if (argc != 2)
	{
		fprintf (stderr, "No file given\n");
		return 0;
	}

	fd = open (argv[1], O_RDONLY);
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

	if (ParseS3M (data, data_mmaped_len))
	{
		goto failed;
	}

failed:
	munmap (data, data_mmaped_len);
	close (fd);
	return 0;
}
