#include "config.h"
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "types.h"

#include "gmifff-internals.h"

#define roundup(x,y) (((x) + (y) - 1) & ~((y) - 1))

struct FFF_Session
{
	int fd;
	unsigned char *data;
	size_t data_len;
	size_t data_mmaped_len;
	
	int level; /* used for nice debug print-outs */
};

static void fprintf_level(FILE *f, struct FFF_Session *s)
{
	int i;
	for (i=0; i < s->level; i++)
	{
		fprintf (f, " ");
	}
}

struct __attribute__((packed)) RIFFHEADER
{
	unsigned char       signature[4];
	uint32_t            size;
};

static int riff_dechunk(struct FFF_Session *s, unsigned char *buffer, uint32_t len, int (*callback)(struct FFF_Session *s, unsigned char *signature, unsigned char *buffer, uint32_t len))
{	
	while (len)
	{
		struct RIFFHEADER *head;

		if (len < 8)
		{
			fprintf (stderr, "riff_dechunk: %d bytes of data left. Not enough to assemble a new chunk header\n", (int)len);
			return -1;
		}

		head=(struct RIFFHEADER *)buffer;

		head->size = uint32_little(head->size);

		buffer += 8;
		len -= 8;

		if (head->size > len)
		{
			fprintf (stderr, "riff_dechunk: not enough data left to assemble chunk payload (0x%0x %d > %d)\n", head->size, head->size, len);
			return -1;
		}

		if (callback(s, head->signature, buffer, head->size))
		{
			return -1;
		}

		buffer += head->size;
		len -= head->size;
	}
	return 0;
}

static int parse_program(struct FFF_Session *s, unsigned char *head, unsigned char *data, uint32_t len)
{
	fprintf_level (stdout, s);
	printf ("%c%c%c%c %d\n", head[0], head[1], head[2], head[3], (int)len);

	if ((head[0] == 'P') &&
	    (head[1] == 'T') &&
	    (head[2] == 'C') &&
	    (head[3] == 'H'))
	{
		struct FFF_PTCH_HEADER *PTCH = (struct FFF_PTCH_HEADER *)data;

		s->level++;
		if (len < sizeof (*PTCH))
		{
			fprintf (stderr, "PTCH is too small\n");
			return -1;
		}

		FFF_PTCH_HEADER_endian (PTCH);

		fprintf_level (stdout, s); printf ("ID 0x%04x:0x%04x\n", PTCH->id.major, PTCH->id.minor);
		fprintf_level (stdout, s); printf ("nlayers: %d\n", PTCH->nlayers);
		fprintf_level (stdout, s); printf ("layer_mode: %d\n", PTCH->layer_mode);
		fprintf_level (stdout, s); printf ("excl_mode: %d\n", PTCH->excl_mode);
		fprintf_level (stdout, s); printf ("effect1: %d\n", PTCH->effect1);
		fprintf_level (stdout, s); printf ("effect1_depth: %d\n", PTCH->effect1_depth);
		fprintf_level (stdout, s); printf ("effect2: %d\n", PTCH->effect2);
		fprintf_level (stdout, s); printf ("effect2_depth: %d\n", PTCH->effect2_depth);
		fprintf_level (stdout, s); printf ("bank: %d\n", PTCH->bank);
		fprintf_level (stdout, s); printf ("program: %d\n", PTCH->program);

		s->level--;

		return 0;
	}

	if ((head[0] == 'L') &&
	    (head[1] == 'A') &&
	    (head[2] == 'Y') &&
	    (head[3] == 'R'))
	{
		struct FFF_LAYR_HEADER *LAYR = (struct FFF_LAYR_HEADER *)data;

		s->level++;
		if (len < sizeof (*LAYR))
		{
			fprintf (stderr, "LAYR is too small\n");
			return -1;
		}

		FFF_LAYR_HEADER_endian (LAYR);

		fprintf_level (stdout, s); printf ("ID 0x%04x:0x%04x\n", LAYR->id.major, LAYR->id.minor);
		fprintf_level (stdout, s); printf ("penv 0x%04x:0x%04x\n", LAYR->penv.major, LAYR->penv.minor); /* pitch envelope */
		fprintf_level (stdout, s); printf ("venv 0x%04x:0x%04x\n", LAYR->venv.major, LAYR->venv.minor); /* volume envelope */

		fprintf_level (stdout, s); printf ("nwaves: %d\n", LAYR->nwaves);
		fprintf_level (stdout, s); printf ("flags: %d\n", LAYR->flags); /* not used? */
		fprintf_level (stdout, s); printf ("range: %d -> %d\n", LAYR->low_range, LAYR->high_range);
		fprintf_level (stdout, s); printf ("pan: %d\n", LAYR->pan);
		fprintf_level (stdout, s); printf ("pan_freq_scale: %d\n", LAYR->pan_freq_scale);
		fprintf_level (stdout, s); printf ("tremolo: freq %d\n", LAYR->tremolo.freq);
		fprintf_level (stdout, s); printf ("         depth %d\n", LAYR->tremolo.depth);
		fprintf_level (stdout, s); printf ("         sweep %d\n", LAYR->tremolo.sweep);
		fprintf_level (stdout, s); printf ("         shape %d\n", LAYR->tremolo.shape);
		fprintf_level (stdout, s); printf ("         delay %d\n", LAYR->tremolo.delay);
		fprintf_level (stdout, s); printf ("vibrato: freq %d\n", LAYR->vibrato.freq);
		fprintf_level (stdout, s); printf ("         depth %d\n", LAYR->vibrato.depth);
		fprintf_level (stdout, s); printf ("         sweep %d\n", LAYR->vibrato.sweep);
		fprintf_level (stdout, s); printf ("         shape %d\n", LAYR->vibrato.shape);
		fprintf_level (stdout, s); printf ("         delay %d\n", LAYR->vibrato.delay);
		fprintf_level (stdout, s); printf ("volocity_mode: %d\n", LAYR->velocity_mode);
		fprintf_level (stdout, s); printf ("attenuation: %d\n", LAYR->attenuation);
		fprintf_level (stdout, s); printf ("freq_scale: %d\n", LAYR->freq_scale);
		fprintf_level (stdout, s); printf ("freq_center: %d (base note)\n", LAYR->freq_center); /* base_note */
		fprintf_level (stdout, s); printf ("layer_event: %d\n", LAYR->layer_event);

		s->level--;

		return 0;
	}

	if ((head[0] == 'W') &&
	    (head[1] == 'A') &&
	    (head[2] == 'V') &&
	    (head[3] == 'E'))
	{
		struct FFF_WAVE_HEADER *WAVE = (struct FFF_WAVE_HEADER *)data;

		s->level++;
		if (len < sizeof (*WAVE))
		{
			fprintf (stderr, "WAVE is too small\n");
			return -1;
		}

		FFF_WAVE_HEADER_endian (WAVE);

		fprintf_level (stdout, s); printf ("ID 0x%04x:0x%04x\n", WAVE->id.major, WAVE->id.minor);
		fprintf_level (stdout, s); printf ("data ID 0x%04x:0x%04x\n", WAVE->data.major, WAVE->data.minor);
		fprintf_level (stdout, s); printf ("size: %d\n", WAVE->size);
		fprintf_level (stdout, s); printf ("start: %d\n", WAVE->start); /* offset? */
		fprintf_level (stdout, s); printf ("loopstart: %d (%d + %d/16)\n", WAVE->loopstart, WAVE->loopstart>>4, WAVE->loopstart&15);
		fprintf_level (stdout, s); printf ("loopend: %d (%d + %d/16)\n", WAVE->loopend, WAVE->loopend>>4, WAVE->loopend&15);
		fprintf_level (stdout, s); printf ("m_start: %d\n", WAVE->m_start); /* ? */
		fprintf_level (stdout, s); printf ("sample_ratio: %d\n", WAVE->sample_ratio);
		fprintf_level (stdout, s); printf ("attenuation: %d\n", WAVE->attenuation);
		fprintf_level (stdout, s); printf ("note: %d -> %d\n", WAVE->low_note, WAVE->high_note);
		fprintf_level (stdout, s); printf ("format: 0x%02x %s%s%s%s%s%s\n", WAVE->format,
		                                   (WAVE->format&0x01)?"8bit":"16bit",
		                                   (WAVE->format&0x02)?" signed":" unsigned",
		                                   (WAVE->format&0x20)?" uLaw":"",
		                                   (WAVE->format&0x04)?" forward-loop":"",
		                                   (WAVE->format&0x10)?" bidi-loop":"",
		                                   (WAVE->format&0x08)?" enable-loop":" disable-loop");
		fprintf_level (stdout, s); printf ("m_format: 0x%02x\n", WAVE->m_format);

		s->level--;
	}

	return 0;
}

static int parse_content(struct FFF_Session *s, unsigned char *head, unsigned char *data, uint32_t len)
{
	int i, j;

	fprintf_level (stdout, s);
	printf ("%c%c%c%c %d\n", head[0], head[1], head[2], head[3], (int)len);

	if ((head[0] == 'C') &&
	    (head[1] == 'P') &&
	    (head[2] == 'R') &&
	    (head[3] == 'T'))
	{
		s->level++;
		if (len > 128)
		{
			fprintf (stderr, "Warning, CPRT length over 128 bytes\n");
		}
		fprintf_level (stdout, s); printf ("Copyright: \"");
		for (i=0; i < len; i++)
		{
			if ((data[i] >= 32) && (data[i]<128))
			{
				printf ("%c", data[i]);
			} else {
				printf ("\\x%02x", data[i]);
			}
		}
		printf ("\"\n");
		s->level--;
		return 0;
	}

	if ((head[0] == 'E') &&
	    (head[1] == 'N') &&
	    (head[2] == 'V') &&
	    (head[3] == 'P'))
	{
		int i;
		struct FFF_ENVP_HEADER *ENVP = (struct FFF_ENVP_HEADER *)data;

		s->level++;
		if (len < sizeof (*ENVP))
		{
			fprintf (stderr, "ENVP is too small to contain header\n");
			return -1;
		}

		FFF_ENVP_HEADER_endian (ENVP);

		data+=sizeof (*ENVP);
		len-=sizeof (*ENVP);

		fprintf_level (stdout, s); printf ("ID 0x%04x:0x%04x\n", ENVP->id.major, ENVP->id.minor);
		fprintf_level (stdout, s); printf ("flags 0x%04x %s\n", ENVP->flags, (ENVP->flags&0x01)?"RETRIGGER":"");
		fprintf_level (stdout, s); printf ("mode %d %s\n", ENVP->mode, (ENVP->mode==1)?"drum?":(ENVP->mode==2)?"melodic?":"?");
		fprintf_level (stdout, s); printf ("index_type %d\n", ENVP->index_type);

		for (i=0; i<ENVP->num_envelopes; i++)
		{
			struct FFF_ENVP_ENTRY *e = (struct FFF_ENVP_ENTRY *)data;

			if (len < sizeof (*e))
			{
				fprintf (stderr, "ENVP is too small to contain all envelopes (%d/%d len=%d<%d)\n", i+1, ENVP->num_envelopes, len, (int)sizeof (*e));
				return -1;
			}

			FFF_ENVP_ENTRY_endian (e);

			data+=sizeof (*e);
			len -= sizeof (*e);

			fprintf_level (stdout, s); printf ("e[%d] hirange=%d nattack=%d nrelease=%d sustain_offset=%d sustain_rate=%d release_rate=%d\n", i, e->hirange, e->nattack, e->nrelease, e->sustain_offset, e->sustain_rate, e->release_rate);

			for (j=0; j<e->nattack; j++)
			{
				struct FFF_ENVP_POINT *p = (struct FFF_ENVP_POINT *)data;
				if (len < sizeof (*p))
				{
					fprintf (stderr, "ENVP is too small to contain all attack points\n");
				}

				FFF_ENVP_POINT_endian (p);

				fprintf_level (stdout, s); printf (" attack[%d] point=%d rate/time=%d\n", j, p->next, p->data.time);
				data+=sizeof (*p);
				len-=sizeof (*p);
			}
			for (j=0; j<e->nrelease; j++)
			{
				struct FFF_ENVP_POINT *p = (struct FFF_ENVP_POINT *)data;
				if (len < sizeof (*p))
				{
					fprintf (stderr, "ENVP is too small to contain all release points\n");
				}

				FFF_ENVP_POINT_endian (p);

				fprintf_level (stdout, s); printf (" release[%d] point=%d rate/time=%d\n", j, p->next, p->data.time);
				data+=sizeof (*p);
				len-=sizeof (*p);
			}
		}

		if (len)
		{
			fprintf_level (stdout, s); printf ("(we got padded data? len=%d\n", len);
		}

		s->level--;
		return 0;
	}

	if ((head[0] == 'D') &&
	    (head[1] == 'A') &&
	    (head[2] == 'T') &&
	    (head[3] == 'A'))
	{
		struct FFF_DATA_HEADER *DATA = (struct FFF_DATA_HEADER *)data;

		s->level++;
		if (len < 5)
		{
			fprintf (stderr, "DATA is too small to contain header\n");
			return -1;
		}

		if (len > (128+4))
		{
			fprintf (stderr, "Warning, DATA length over 128 bytes\n");
		}

		FFF_DATA_HEADER_endian (DATA);

		fprintf_level (stdout, s); printf ("ID 0x%04x:0x%04x\n", DATA->id.major, DATA->id.minor);
		fprintf_level (stdout, s); printf ("Datafile: \"");
		for (i=0; i < len-4; i++)
		{
			if (i == (len-5))
			{
				if (DATA->filename[i]!=0)
				{
					fprintf (stderr, "Warning, DATA filename is not zero-terminated\n");
				} else {
					break;
				}
			}
			if ((DATA->filename[i] >= 32) && (DATA->filename[i]<128))
			{
				printf ("%c", DATA->filename[i]);
			} else {
				printf ("\\x%02x", DATA->filename[i]);
			}
		}
		printf ("\"\n");

		s->level--;
		return 0;
	}

	if ((head[0] == 'P') &&
	    (head[1] == 'R') &&
	    (head[2] == 'O') &&
	    (head[3] == 'G'))
	{
		struct FFF_PROG_HEADER *PROG = (struct FFF_PROG_HEADER *)data;

		s->level++;
		if (len < sizeof (*PROG))
		{
			fprintf (stderr, "PROG is too small to contain header\n");
			return -1;
		}

		FFF_PROG_HEADER_endian (PROG);

		fprintf_level (stdout, s); printf ("ID 0x%04x:0x%04x\n", PROG->id.major, PROG->id.minor);
		fprintf_level (stdout, s); printf ("Version 0x%04x:0x%04x\n", PROG->version.major, PROG->version.minor);

		if (riff_dechunk(s, data + sizeof (*PROG), len - sizeof (*PROG), parse_program))
			return -1;

		s->level--;
		return 0;
	}

	return 0;
}

static int parse_FFFF(struct FFF_Session *s, unsigned char *head, unsigned char *buffer, uint32_t len)
{
	fprintf_level (stdout, s);
	printf ("%c%c%c%c %d\n", head[0], head[1], head[2], head[3], (int)len);

	if ((head[0] == 'F') &&
	    (head[1] == 'F') &&
	    (head[2] == 'F') &&
	    (head[3] == 'F'))
	{
		s->level++;
		if (riff_dechunk(s, buffer, len, parse_content))
			return -1;
		s->level--;
		return 0;
	}

	fprintf (stderr, "Not e FFF formated file\n");

	return 1;
}

int main(int argc, char *argv[])
{
	struct FFF_Session s;
	struct stat st;
	size_t ps = sysconf(_SC_PAGE_SIZE);

	bzero (&s, sizeof (s));

	if (argc != 2)
	{
		fprintf (stderr, "No file given\n");
		return 0;
	}

	s.fd = open (argv[1], O_RDONLY);
	if (s.fd < 0)
	{
		perror ("open()");
		return 0;
	}
	if (fstat(s.fd, &st))
	{
		perror("fstat()");
		close (s.fd);
		return 0;
	}
	if (!st.st_size)
	{
		fprintf (stderr, "Zero-size file\n");
		close (s.fd);
		return 0;
	}

	s.data_len = st.st_size;
	s.data_mmaped_len = roundup (s.data_len, ps);
	s.data = mmap (0, s.data_mmaped_len, PROT_READ|PROT_WRITE, MAP_FILE|MAP_PRIVATE, s.fd, 0);

	if (s.data == MAP_FAILED)
	{
		perror ("mmap() failed");
		close (s.fd);
		return 0;
	}

	if (riff_dechunk (&s, s.data, s.data_len, parse_FFFF))
	{
		goto failed;
	}

failed:
	munmap (s.data, s.data_mmaped_len);
	close (s.fd);
	return 0;
}
