#include "config.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include "filesel/gendir.h"

#define roundup(x,y) (((x) + (y) - 1) & ~((y) - 1))
#define MAXWORDS 130
#define MAXSOURCELEVEL 50


#if 0
struct TimidityVariable
{
	char *name;
	char *value;
};
#endif

/* Roland manual (SC55 / SC88): (basically swapped LSB and MSB around.. "great")
	BANK MSB (CC00) = variation
	BANK LSB (CC20) = map       (00h = user default, 01h = SC-55, 02h = SC-88, 03h = native SC-88pro)

   XG manual:
	BANK MSB (CC00) = map    00h = melody, 40h = SFX, 7eh = SFX kit, 7fh = Rhythm
	BANK LSB (CC20) = variation

   GM/GM2
	BANK MSB (CC00) = map        (121 = melody, 120 = drum)
	BANK LSB (CC20) = variation
*/

typedef struct {
  int type;
  int samples;
  Sample *sample;
  char *instname;
} TimidityInstrument;


typedef struct
{
	char *name;
	char *comment;

	TimidityInstrument *instrument;

	uint8_t instype; /* 0: Normal
	                    1: %font
	                    2: %sample
	                    3-255: reserved
	                 */
	/* used if instype==1, to locate the sample */
	int8_t font_preset, font_keynote;
	uint8_t font_bank;

	int8_t loop_timeout, legato, tva_level, play_note, damper_mode;


	int16_t amp; /* Amplifies the instrument’s volume by amplification percent if positive. */

	int8_t note; /* if not -1, specifies a fixed MIDI note to use when playing the instrument. config is scaled into this range */

	int8_t pan; /* -1 = default, 0 = left, 64 = center, 127 = right.. config is scaled into this range */

	int    tunenum;
	float *tune; /* Adjust the instrument’s root frequency. */

	int   envratenum;
	int **envrate; /* Sets the instrument’s ADSR rate. */

	int   envofsnum;
	int **envofs; /* Sets the instrument’s ADSR offset. */

	int8_t strip_loop, strip_envelope, strip_tail; /* controlled by keep= and strip= */

	int                tremnum;
	struct Quantity_ **trem; /* Sets the instrument’s tremolo */


	int16_t rnddelay;

	int      sclnotenum;
	int16_t *sclnote;

	int      scltunenum;
	int16_t *scltune;

	int      fcnum;
	int16_t *fc;

	int      resonum;
	int16_t *reso;

	int      trempitchnum;
	int16_t *trempitch;

	int      tremfcnum;
	int16_t *tremfc;

	int      modpitchnum;
	int16_t *modpitch;

	int      modfcnum;
	int16_t *modfc;

	int   modenvratenum;
	int **modenvrate;

	int   modenvofsnum;
	int **modenvofs;

	int   envvelfnum;
	int **envvelf;

	int   envkeyfnum;
	int **envkeyf;


	int   modenvvelfnum;
	int **modenvvelf;
	int   modenvkeyfnum;
	int **modenvkeyf;

	int                vibnum;
	struct Quantity_ **vib;

	int16_t vel_to_fc;
	int16_t key_to_fc;
	int16_t vel_to_resonance;

	int8_t reverb_send;
	int8_t chorus_send;
	int8_t delay_send;
} TimidityPatch;


struct TimidityBank
{
	struct TimidityPatch patches[128];

	int bank_msb;
	int bank_lsb;
	int program;  /* used for drums */
};

struct TimidityConfig
{
/* at the moment, only one variable is supported, and it is read-only.. $basedir
	struct TimidityVariable *variables;
	int                      variables_n;
*/
	char *basedir;
	int sourcelevel;

	char **directories;
	int directories_n;

	int bank_gs; /* if GS=true the bank change MSB. if GS=false (GM), the bank change LSB */
	int bank_msb; /* current bank. Default is gm2 */
	int bank_lsb; /* current bank */

	int drumset_msb; /* current bank, Default is gm2drum */
	int drumset_lsb; /* current bank */
	int drumset_program; /* current program */
	/* drumset program will always be 0 ? */

	int progbase; /* program/preset offset */

	struct TimidityPatch *patches;
	int patches_n;
};

static int ParseTimidityConfigMemory (struct TimidityConfig *config, const char *string, int stringlength);

static struct TimidityPatch *TimidityAddPatch (struct TimidityConfig *config, const char *filename /* will be duplicated */, int bank_msb, int bank_lsb, int patch);

static void TimidityDefaultPatch (struct TimidityPatch *patch)
{
	patch->format = 0;
	patch->format_sf2_bank = 0;
	patch->format_sf2_program = 0;
}

static struct TimidityPatch *TimdityAddPatch (struct TimidityConfig *config, const char *filename /* will be duplicated */, int bank_msb, int bank_lsb, int patch)
{
	int i;
	struct TimidityPatch *temp;
	char *newfilename = strdup (filename);

	if (!newfilename)
	{
		fprintf (stderr, "[Timidity] TimidityAddPatch failed to duplicate filename. Out of memory?\n");
		return 0;
	}

	for (i = 0; i < config->patches_n; i++)
	{
		if ((config->patches[i].bank_msb == bank_msb) &&
		    (config->patches[i].bank_lsb == bank_lsb) &&
		    (config->patches[i].patch == patch))
		{
			free (config->patches[i].filename);
			config->patches[i].filename = newfilename;
			TimidityDefaultPatch (config->patches + i);
			return config->patches + i;
		}
	}

	temp = realloc (config->patches, sizeof (config->patches) * (config->patches_n + 1));
	if (!temp)
	{
		fprintf (stderr, "[Timidity] TimidityAddPatch failed to realloc array. Out of memory?\n");
		return 0;
	}

	config->patches[config->patches_n].filename = newfilename;
	config->patches[config->patches_n].bank_msb = bank_msb;
	config->patches[config->patches_n].bank_lsb = bank_lsb;
	config->patches[config->patches_n].patch = patch;

	TimidityDefaultPatch (config->patches + config->patches_n);

	return config->patches + (config->patches_n++);		
}


static const char *get_variable (struct TimidityConfig *config, const char *src, int src_len)
{
	int i;

	if ((src_len == 7) && (!strncmp(src, "basedir", 7)))
	{
		return config->basedir;
	}

	fprintf (stderr, "[timidity] found an unknown variable $");
	for (i = 0; i < src_len; i++)
	{
		fputc (src[i], stderr);
	}
	fprintf (stderr, " when parsing config\n");

	return 0;
}

static int ParseTimdityConfig_dir (struct TimidityConfig *config, const char *dir, const int archive)
{
	int i;
	void *temp;
	char *dircopy;

	if (archive)
	{
		fprintf (stderr, "[timidity] archive dir statement not supported yet\n");
		return 0;
	}

	if (!strlen (dir))
	{
		return 0;
	}

	/* is directory already on the list??, if so, skip it */
	for (i=0; i < config->directories_n; i++)
	{
		if (!strcmp (config->directories[i], dir))
		{
			return 0;
		}
	}

	dircopy = strdup (dir);
	if (!dircopy)
	{
		fprintf (stderr, "[timidity] ParseTimdityConfig_dir: running out of memory\n");
		return -1;
	}

	temp = realloc (config->directories, (config->directories_n+1) * sizeof (char *));
	if (!temp)
	{
		fprintf (stderr, "[timidity] ParseTimdityConfig_dir: running out of memory\n");
		free (dircopy);
		return -1;
	}

	config->directories = temp;
	config->directories[config->directories_n++] = dircopy;

	fprintf (stderr, "[timidity] Adding dir %s to searchlist\n", dir);
	return 0;
}

static int ParseTimdityConfig_source (struct TimidityConfig *config, const char *filename)
{
	char temp[PATH_MAX+1], *temp2;
	struct stat st;
	size_t ps = sysconf(_SC_PAGE_SIZE);
	int fd;
	size_t mmap_len;
	void *data;
	int retval;

	if (config->sourcelevel >= MAXSOURCELEVEL)
	{
		fprintf (stderr, "[timidity] nesting level of config files too high (%d)\n", config->sourcelevel);
		return -1;
	}

	if (!strlen (filename))
	{
		fprintf (stderr, "[timidity] Source statement without parameter\n");
		return -1;
	}

	gendir (config->basedir, filename, temp);
	temp[strlen(temp)] = 0; /* gendir() will append a '/' at the end of out new path */

	/* we use mmap for this magic */
	fd = open (temp, O_RDONLY);
	if (fd < 0)
	{
		fprintf (stderr, "[timidity] Failed to open %s: %s\n", temp, strerror (errno));
		return -1;
	}
	if (fstat(fd, &st))
	{
		fprintf (stderr, "[timidity] Failed to stat %s: %s\n", temp, strerror (errno));
		close (fd);
		return -1;
	}
	if (!st.st_size)
	{
		close (fd);
		return 0; /* we ignore empty files .. */
	}

	mmap_len = roundup (st.st_size, ps);
	data = mmap (0, mmap_len, PROT_READ, MAP_FILE|MAP_SHARED, fd, 0);
	close (fd);

	if (data == MAP_FAILED)
	{
		fprintf (stderr, "[timidity] Failed to mmap %s: %s\n", temp, strerror (errno));
		return -1;
	}

	temp2 = strrchr (temp, '/');
	if (temp2 == temp)
	{ /* if we have no directory, add the termination after root instead */
		temp[1] = 0;
	} else {
		*temp2 = 0;
	}

	temp2 = config->basedir; /* store old basedir */
	config->basedir = temp; /* set basedir */

	fprintf (stderr, "[timidity] parseing %s with basedir %s\n", filename, temp);
	config->sourcelevel++;
	retval = ParseTimidityConfigMemory (config, data, st.st_size);
	config->sourcelevel--;

	config->basedir = temp2; /* restore basedir */

	munmap (data, mmap_len);

	return retval;;
}

static int ParseTimidityConfig_bank (struct TimidityConfig *config, const char *map, int variation)
{
	if (!strcmp (map, "gm2"))
	{
		config->bank_gs = 0;
		config->bank_msb = 121;
		config->bank_lsb = variation;
	} else if (!strcmp (map, "sc55"))
	{
		config->bank_gs = 1;
		config->bank_lsb = 1;
		config->bank_msb = variation;
	} else if (!strcmp (map, "sc88"))
	{
		config->bank_gs = 1;
		config->bank_lsb = 2;
		config->bank_msb = variation;
	} else if (!strcmp (map, "sc88pro"))
	{
		config->bank_gs = 1;
		config->bank_lsb = 3;
		config->bank_msb = variation;
	} else if (!strcmp (map, "sc8850"))
	{
		config->bank_gs = 1;
		config->bank_lsb = 4;
		config->bank_msb = variation;
	} else if (!strcmp (map, "xg"))
	{
		config->bank_gs = 0;
		config->bank_msb = 0;
		config->bank_lsb = variation;
	} else if (!strcmp (map, "xgsfx64"))
	{
		config->bank_gs = 0;
		config->bank_msb = 0x40;
		config->bank_lsb = variation;
	} else if (isdigit (*map))
	{
		config->bank_gs = 0;
		config->bank_msb = atoi (map);
		config->bank_lsb = variation;
	} else {
		fprintf (stderr, "[timidity] bank command as unknown MapID1\n");
		return -1;
	}
	return 0;
}

static int ParseTimidityConfig_bank_variant (struct TimidityConfig *config, int variation)
{
	if (config->bank_gs)
	{
		config->bank_msb = variation;
	} else {
		config->bank_lsb = variation;
	}
	return 0;
}

static int ParseTimidityConfig_drumset (struct TimidityConfig *config, const char *map, int variation)
{
	if (!strcmp (map, "gm2drum"))
	{
		config->drumset_msb = 120;
		config->drumset_lsb = 0;
		config->drumset_program = variation;
	} else if (!strcmp (map, "sc55drum"))
	{
		config->drumset_lsb = 1+128; /* use sys-ex to enable drum-mode */
		config->drumset_msb = 0;
		config->drumset_program = variation; /* ignored, only supports program */
	} else if (!strcmp (map, "sc88drum"))
	{
		config->drumset_lsb = 2+128; /* use sys-ex to enable drum-mode */
		config->drumset_msb = 0;
		config->drumset_program = variation;
	} else if (!strcmp (map, "sc88prodrum"))
	{
		config->drumset_lsb = 3+128; /* use sys-ex to enable drum-mode */
		config->drumset_msb = 0;
		config->drumset_program = variation;
	} else if (!strcmp (map, "sc8850drum"))
	{
		config->drumset_lsb = 4+128; /* use sys-ex to enable drum-mode */
		config->drumset_msb = 0;
		config->drumset_program = variation;
	} else if (!strcmp (map, "xgdrum"))
	{
		config->drumset_msb = 0x7f;
		config->drumset_lsb = 0;
		config->drumset_program = variation;
	} else if (!strcmp (map, "xgsfx126"))
	{
		config->drumset_msb = 0x7e;
		config->drumset_lsb = 0;
		config->drumset_program  = variation;
	} else if (isdigit (*map))
	{
		config->drumset_msb = atoi (map);
		config->drumset_lsb = 0;
		config->drumset_program = variation;
	} else {
		fprintf (stderr, "[timidity] bank command as unknown MapID1\n");
		return -1;
	}
	return 0;
}

static int ParseTimidityConfig_drumset_variant (struct TimidityConfig *config, int variation)
{
	config->drumset_program = variation; 
	return 0;
}

static char *trim_and_expand (struct TimidityConfig *config, const char *src, int src_len, int *comment, int *extension)
{
	char *retval = 0;
	int retval_fill = 0;
	int retval_size = 0;

	*comment = 0;
	*extension = 0;

	if ((src_len > 10) && (!strncmp (src, "#extension", 10))) /* has to be at the start of the string according to the timidity.c parser */
	{
		src += 10;
		src_len -= 10;
		*extension = 1;
	}

	while (src_len && isblank(*src))
	{
		src++;
		src_len--;	
	}

	while (src_len)
	{
		const char *temp1;

		if (*src == '#')
		{
			*comment = 1;
			break;
		}

		if ((src_len >= 2) && (src[0] == '$') && (isalnum(src[1]) || (src[1] == '_')))
		{ /* variable direct */
			int i, len;

			for (i = 2; i < src_len; i++)
			{
				if ((!isalnum(src[i])) && (src[i] != '_'))
					break;
			}
			temp1 = get_variable (config, src + 1, i - 1);
			if (temp1)
			{
				len = strlen (temp1);
				if ((retval_fill + len + 1) >= retval_size)
				{
					char *temp2;
					retval_size+=len + 16;
					temp2 = realloc (retval, retval_size);
					if (!temp2)
					{
						free (retval);
						fprintf (stderr, "[timidity] trim_and_expand, running out of memory\n");
						return 0;
					}
					retval = temp2;
				}
				strcpy (retval + retval_fill, temp1);
				retval_fill += len;
			}

			src_len -= i;
			src += i;
		} else if ((src_len >= 4) && (src[0] == '$') && (src[1] == '{') && (temp1 = memchr(src+2, '}', src_len-2)))
		{ /* {} enclosed varible */
			int len;
			const char *temp3;

			temp3 = get_variable (config, src + 2, temp1 - src - 2);
			if (temp3)
			{
				len = strlen (temp3);
				if ((retval_fill + len + 1) >= retval_size)
				{
					char *temp2;
					retval_size+=len + 16;
					temp2 = realloc (retval, retval_size);
					if (!temp2)
					{
						free (retval);
						fprintf (stderr, "[timidity] trim_and_expand, running out of memory\n");
						return 0;
					}
					retval = temp2;
				}
				strcpy (retval + retval_fill, temp3);
				retval_fill += len;
			}

			src_len -= temp1 - src + 1;
			src = temp1 + 1;
		} else {
			if ((retval_fill + 2) >= retval_size)
			{
				char *temp2;
				retval_size+=16;
				temp2 = realloc (retval, retval_size);
				if (!temp2)
				{
					free (retval);
					fprintf (stderr, "[timidity] trim_and_expand, running out of memory\n");
					return 0;
				}
				retval = temp2;
			}
			retval[retval_fill++] = *src;
			retval[retval_fill] = 0;
			src++;
			src_len--;
		}
	}

	return retval;
}

static int ParseTimidityConfigMemory (struct TimidityConfig *config, const char *string, int stringlength)
{
	const char *iter = string;
	const char *eof = string + stringlength;


	while (iter < eof)
	{
		const char *eol = memchr (iter, '\n', eof-iter);
		int words = 0;
		char *w[MAXWORDS + 1];
		char *line;
		int comment;
		int extension;
		int i = 0, j;

		if (!eol)
		{
			eol = eof;
		}

		line = trim_and_expand (config, iter, eol - iter, &comment, &extension);

		iter = eol;
		while ((iter < eof) && ((*iter == '\n' || *iter == '\r')))
		{
			iter++;
		}

		if (!line)
		{
			continue;
		}

		fprintf (stderr, "Parse +%s+\n", line);

		/* j = token-length */

		j = strcspn(line + i, " \t\r\n\240");
		if (j == 0)
		{
			j = strlen(line + i);
		}

		line[i + j] = '\0';	/* terminate the first token */
		w[0] = line + i;
		i += j + 1;
		while(words < MAXWORDS - 1)             /* -1 : next arg */
		{
			char *terminator;

			while (isspace(line[i]))
			{
				i++;
			}
			if (line[i] == '#')
			{
				comment = 1;
				break;
			}
			if (line[i] == '\0')
			{
				break;
			}
			if (((line[i] == '"') || (line[i] == '\'')) && (terminator = strchr(line + i + 1, line[i])) != NULL)
			{
				if (!isspace(terminator[1]) && terminator[1] != '\0')
				{
					fprintf (stderr, "[timidity] There must be at least one whitespeed between string terminator (%c) and the next parameter\n", line[i]);
					goto error_out;
				}
				w[++words] = line + i + 1;
				i = terminator - line + 1;
				*terminator = '\0';
			} else { /* not terminated */
				j = strcspn(line + i, " \t\r\n\240");
				w[++words] = line + i;
				i += j;
				if (line[i] != '\0')             /* unless at the end-of-string (i.e. EOF) */
				{
					line[i++] = '\0';            /* terminate the token */
				}
			}
		}
		w[++words] = NULL;

		if ((words >= 2) && (!strcmp (w[0], "dir")))
		{
			if (ParseTimdityConfig_dir (config, w[1], comment))
			{
				goto error_out;
			}
		} else if ((words >= 2) && (!strcmp (w[0], "source")))
		{
			if (ParseTimdityConfig_source (config, w[1]))
			{
				goto error_out;
			}
		} else if ((words >= 2) && (!strcmp (w[0], "progbase")))
		{
			config->progbase = atoi (w[1]); /* not much error-checking, but neither does timidity */
		} else if ((words >= 2) && (!strcmp (w[0], "bank")))
		{
			if (words == 2)
			{
				if (ParseTimidityConfig_bank_variant (config, atoi (w[1]))) /* sets LSB, except if on Roland/SC-xx */
				{
					goto error_out;
				}
			} else {
				if (ParseTimidityConfig_bank (config, w[1], atoi (w[2])))
				{
					goto error_out;
				}
			}
		} else if ((words >= 2) && (!strcmp (w[0], "bank")))
		{
			if (words == 2)
			{
				if (ParseTimidityConfig_drumset_variant (config, atoi (w[1]))) /* sets LSB, except if on Roland/SC-xx */
				{
					goto error_out;
				}
			} else {
				if (ParseTimidityConfig_drumset (config, w[1], atoi (w[2])))
				{
					goto error_out;
				}
			}
		} else if ((words >= 2) && (isdigit(w[0][0])))
		{
			if (config->melodic)
			{
				int program = atoi (w[0]);
				struct TimidityPatch *patch = 0;

				if (!strcmp (w[1], "%font"))
				{
					if (words <= 5)
					{
						fprintf (stderr, "[timidity] Too few parameters to %%font entry\n");
						goto error_out;
					}
					patch = TimidityAddPatch (config, w[2], config->bank_msb, config->bank_lsb, program);
					if (patch)
					{
						patch->format = 2;
						patch->format_sf2_bank = atoi (w[3]);
						patch->format_sf2_program = atoi (w[4];
asdf
				} else if (!strcmp (!w[1], "%sample"))
				{
					if (words <= 2)
					{
						fprintf (stderr, "[timidity] Too few parameters to %%sample entry\n");
						goto error_out;
					}
					patch = TimidityAddPatch (config, w[2], config->bank_msb, config->bank_lsb, program);

sdaf
				} else {
					patch = TimidityAddPatch (config, w[1], config->bank_msb, config->bank_lsb, program);
asdf
				}

				if (!patch)
				{
					goto error_out;
				}

			}
		}


#warning do_magic_do_line

		free (line);
		continue;

error_out:
		free (line);
		return -1;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	struct TimidityConfig *config = calloc (sizeof (*config), 1);

	FILE *fd = fopen ("test.cfg", "r");

	char *buffer = malloc(1024*1024);
	int fill;

	config->basedir = strdup ("/home/oem");
	ParseTimidityConfig_bank (config, "gm2", 0); /* set default values */
	ParseTimidityConfig_drumset (config, "gm2drum", 0); /* set default values */

	fill = fread (buffer, 1, 1024*1024, fd);

	fclose (fd);

	ParseTimidityConfigMemory (config, buffer, fill);

	free (buffer);

	free (config->basedir);

	free (config);

	return 0;
}


