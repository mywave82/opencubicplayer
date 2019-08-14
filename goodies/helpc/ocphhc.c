/* OpenCP hypertext help compiler (OCPHHC) for use with new help system
 *
 * not copyrighted; written by Fabian Giesen (RYG/Chrome Design)
 * it's simple. it's freeware. it's badly coded. it (hardly) works.
 *
 * revision history: (please note changes here)
 *  -fg980811  Fabian Giesen <gfabian@jdcs.su.nw.schule.de>
 *    -first alpha
 *  -fg980812  Fabian Giesen <gfabian@jdcs.su.nw.schule.de>
 *    -bugfixing
 *  -fg980813  Fabian Giesen <gfabian@jdcs.su.nw.schule.de>
 *    -added some commands
 *  -fg980814  Fabian Giesen <gfabian@jdcs.su.nw.schule.de>
 *    -new helpfile format version which is compressed
 *    -compressed using "deflate" algorithm (via zlib), can be decompressed
 *     with OpenCP standard INFLATE.DLL
 *  -fg980815  Fabian Giesen <gfabian@jdcs.su.nw.schule.de>
 *    -added "statistics" screen
 *  -ryg_dunno Fabian Giesen <fabian@jdcs.su.nw.schule.de>
 *    -fixed some bugs (don't remember which ones)
 *    -changed email address (yeah, i've got a new one)
 *    -now i use "ryg" instead of "fg"; don't ask me why
 *  -ryg981205 Fabian Giesen <fabian@jdcs.su.nw.schule.de>
 *    -now uses some pseudo-zlib with just the deflation code in it (because
 *     including whole zlib would be just a waste of space)
 *    -this code is ABSOLUTELY NOT SUPPORTED. if it crashes, it's your own
 *     problem. also it will be obselete soon because i'll write some
 *     html->help compiler (which roxxs :)
 *    -there's no documentation for this. read the help definition file and
 *     try and understand it. good luck.
 *  -ryg981211 Fabian Giesen <fabian@jdcs.su.nw.schule.de>
 *    -fixed some stupid bug in the "parser" which killed almost all escape
 *     seqeunces (oops..)
 *  -fd990519 Felix Domke <tmbinc@gmx.net>
 *    -fixed the stupid "backslashed".
 *
 * READ THIS SOURCE FILE CAREFULLY. IT'S SURELY ONE OF THE BEST EXAMPLES OF
 * BAD C CODE YOU'LL EVER GET.
 *
 * some other thing: in this file the help pages are called "sections" (don't
 * ask me why).
 */

#include "config.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "zlib.h"

#pragma pack (1)                       /* this is essential */

#define MAXSECTSIZE (256*1024)         /* maximum section size 256k */

/* Help file control characters */

#define CMD_NORMAL           1         /* switch colour to normal */
#define CMD_BRIGHT           2         /* switch colour to bright */
#define CMD_HYPERLINK        3         /* hyperlink */
#define CMD_CENTERED         4         /* centered text */
#define CMD_COLOURCHOOSE     5         /* choose colour */
#define CMD_RAWCHAR          6         /* raw character (use for chars<32) */

typedef struct sect {
	char *name;
	char *desc;
	void *data,
	     *compdata;
	uint32_t   datasize,
	           compdatasize,
	           textlines;
	struct sect *next;
} section;

char     line[2048],
         sectionbuf[MAXSECTSIZE];
int      sectpos=0,
         quit=0,
         sectlines=0;
section *sections=NULL,
        *cursect=NULL;
FILE    *in,
        *out;
char     file_id[5]="OCPH";
uint32_t helpver=0x011000;
int      verbose=0,
         comment=0;

int      totdatasize,
         totcompdatasize;

void putfstr(char *what, FILE *to)
{
	if (what)
	{
		putc(strlen(what), to);
		if (strlen(what))
			if (fwrite(what, strlen(what), 1, to) != 1)
			{
				perror(__FILE__ ": fwrite failed #1: ");
				exit(1);
			}
	} else putc(0, to);
}

void flush_section(void)
{
	if (!cursect || comment)
		return;

	cursect->data=calloc(1, sectpos);
	memcpy(cursect->data, sectionbuf, sectpos);
	cursect->datasize=sectpos;
	cursect->textlines=sectlines;

	sectpos=0;
	sectlines=0;
}

void process_section_line(char *line)
{
	char *p1,
	     *p2;
	int   i;

	flush_section();

	comment=0;

	if (!strncasecmp(line, ".comment", 9)) { comment=1; return; };
	if (!strncasecmp(line, ".end", 4)) { quit=1; return; };

	if (cursect) {
		cursect->next=calloc(sizeof(section), 1); cursect=cursect->next;
	} else {
		sections=calloc(sizeof(section), 1); cursect=sections;
	};

	p1=line+1; p2=p1; i=0;

	while (*p2 && (*p2!=' '))
		p2++;
	if (*p2)
		i=1;
	*p2=0;

	cursect->name=strdup(p1);
	if (i)
		cursect->desc=strdup(p2+1);
	else
		cursect->desc=NULL;

	cursect->data=NULL; cursect->next=NULL;
}

int packhex(char *from)
{
	int val;
	char buf[3];

	strncpy(buf, from, 2);
	buf[2]=0;

	val=0;

	if ((*from>='0') && (*from<='9')) val|=*from-'0';
	if ((*from>='A') && (*from<='F')) val|=*from-'A'+10;
	if ((*from>='a') && (*from<='f')) val|=*from-'a'+10;

	val<<=4; from++;

	if ((*from>='0') && (*from<='9')) val|=*from-'0';
	if ((*from>='A') && (*from<='F')) val|=*from-'A'+10;
	if ((*from>='a') && (*from<='f')) val|=*from-'a'+10;

	return val;
}

void process_text_line(char *line)
{
	char buffer[2048],
	     buffer2[2048],
	     *p;
	int  len,
	     brightst,
	     i,
	     backslashed;

	     if (comment)
		     return;
	     strcpy(buffer, line);
	     brightst=0;

	     backslashed=0;

/* First, process all "bright"-switches */

	     p=&buffer[0]; i=0;

	     while (*p) {
		     backslashed=(*p=='\\')&&(!backslashed);     /* better? (fd) */

		     if (((unsigned)*p)<32) {
			     strcpy(buffer2, &buffer[i+1]);
			     strcpy(&buffer[i+2], buffer2);

			     buffer[i+1]=*p;
			     buffer[i]=CMD_RAWCHAR;

			     p++;
			     i++;
		     } else if (*p=='~')
		     {
			     if (backslashed)
			     {
				     strcpy(buffer2, p);
				     strcpy(p-1, buffer2);
				     backslashed=0;
			     } else {
				     brightst=1-brightst;

				     if (brightst)
					     *p=CMD_BRIGHT;
				     else
					     *p=CMD_NORMAL;
			     }
		     } else if (*p=='^')
		     {
			     if (backslashed)
			     {
				     strcpy(buffer2, p);
				     strcpy(p-1, buffer2);
				     backslashed=0;
			     } else {
				     *p=CMD_COLOURCHOOSE; p++; i++;
				     *p=packhex(p);

				     strcpy(buffer2, p+2);
				     strcpy(p+1, buffer2);
			     }
		     } else if (*p=='n') {
			     if (backslashed)
			     {
				     *(p-1)=CMD_RAWCHAR;
				     *p='\n';
				     backslashed=0;
			     }
		     } else if (*p=='r') {
			     if (backslashed)
			     {
				     *(p-1)=CMD_RAWCHAR;
				     *p='\r';
				     backslashed=0;
			     }
		     } else if (*p=='d') {
			     if (backslashed)
			     {
				     *(p-1)=CMD_RAWCHAR;
				     *p=26;
				     backslashed=0;
			     }
		     }

		     p++;
		     i++;
	     }

/* Then check for any special marks. If there are, process them, otherwise,
 * just copy the text.
 */

	     if (strchr(buffer, '[')) {
		     if (*(strchr(buffer, '[')-1)!='\\')
		     {
			     char *fptr;;

			     len=strlen(buffer);

			     fptr=&buffer[0];

			     while ((fptr=strchr(fptr, '\\')))
			     {
				     memmove (fptr, fptr+1, strlen (fptr));
				     fptr+=1;
			     }

			     *strchr(buffer, '[')=CMD_CENTERED;

			     if (strchr(buffer, ']')) *strchr(buffer, ']')=0;

			     memcpy(&sectionbuf[sectpos], buffer, len); sectpos+=len+1;
			     sectionbuf[sectpos-1]='\n';
		     } else {
			     char *fptr=&buffer[0];

			     while ((fptr=strchr(fptr, '\\')))
			     {
				     memmove (fptr, fptr+1, strlen (fptr));
				     fptr+=1;
			     }

			     len=strlen(buffer); memcpy(&sectionbuf[sectpos], buffer, len);
			     sectpos+=len+1; sectionbuf[sectpos-1]='\n';
		     }
	     } else if (strchr(buffer, '{')) {
		     if (*(strchr(buffer, '{')-1)!='\\')
		     {
			     char *fptr;

			     len=strlen(buffer);

			     fptr=&buffer[0];

			     while ((fptr=strchr(fptr, '\\')))
			     {
				     memmove (fptr, fptr+1, strlen (fptr));
				     fptr+=1;
			     };

			     *strchr(buffer, '{')=CMD_HYPERLINK;

			     if (strchr(buffer, '}')) *strchr(buffer, '}')=0;

			     memcpy(&sectionbuf[sectpos], buffer, len); sectpos+=len+1;
			     sectionbuf[sectpos-1]='\n';
		     } else {
			     char *fptr;

			     {
				     char *dst = strchr(buffer, '{')-1;
				     memmove (dst, dst+1, strlen (dst));
			     }

			     fptr=&buffer[0];

			     while ((fptr=strchr(fptr, '\\')))
			     {
				     memmove (fptr, fptr+1, strlen (fptr));
				     fptr+=1;
			     }

			     len=strlen(buffer); memcpy(&sectionbuf[sectpos], buffer, len);
			     sectpos+=len+1; sectionbuf[sectpos-1]='\n';
		     }
	     } else {
		     char *fptr=&buffer[0];

		     while ((fptr=strchr(fptr, '\\')))
		     {
			     memmove (fptr, fptr+1, strlen (fptr));
			     fptr+=1;
		     }

		     len=strlen(buffer); memcpy(&sectionbuf[sectpos], buffer, len);
		     sectpos+=len+1; sectionbuf[sectpos-1]='\n';
	     }

	     sectlines++;
}

void read_lines(void)
{
	while (!feof(in) && (!quit))
	{
		if (!fgets(line, 2048, in))
			break;
		if (line[strlen(line)-1]=='\n')
			line[strlen(line)-1]=0;
		if (line[strlen(line)-1]=='\r')
			line[strlen(line)-1]=0;

		if (line[0]=='.')
			process_section_line(line);
		else
			process_text_line(line);
	}
}

void compress_sections(void)
{
	uLong          insize,
	               outsize;
	unsigned char *inbuf,
	              *outbuf;
	section       *s;

	s=sections;

	while (s) {
		inbuf=(unsigned char *) s->data;
		insize=s->datasize;

		outsize=insize+(insize/1000)+2;
		outbuf=calloc(outsize, 1);

		compress2(outbuf, &outsize, inbuf, insize, 9);

#if 0
		s->compdata=(void *) (outbuf+2); s->compdatasize=outsize-2;

    /* this is because zlib puts 2 header bytes to the beginning of the data-
     * stream we don't need
     */

#else
		s->compdata=(void *) (outbuf); s->compdatasize=outsize;

#endif

		s=s->next;
	};
}

void write_section_directory(void)
{
	uint32_t numsects;
	section *s;

	s=sections;
	numsects=0;
	while (s)
	{
		s=s->next;
		numsects++;
	}

	numsects = uint32_little(numsects);
	if (fwrite(&numsects, 4, 1, out) != 1)
	{
		perror(__FILE__ ": fwrite failed #2: ");
		exit(1);
	}

	s=sections;

	while (s)
	{
		putfstr(s->name, out);
		putfstr(s->desc, out);
		s->datasize=uint32_little(s->datasize);
		s->textlines=uint32_little(s->textlines);
		s->compdatasize=uint32_little(s->compdatasize);
		if (fwrite(&s->datasize, 4, 1, out) != 1)
		{
			perror(__FILE__ ": fwrite failed #3: ");
			exit(1);
		}
		if (fwrite(&s->textlines, 4, 1, out) != 1)
		{
			perror(__FILE__ ": fwrite failed #4: ");
			exit(1);
		}
		if (fwrite(&s->compdatasize, 4, 1, out) != 1)
		{
			perror(__FILE__ ": fwrite failed #5: ");
			exit(1);
		}
		s->datasize=uint32_little(s->datasize);
		s->textlines=uint32_little(s->textlines);
		s->compdatasize=uint32_little(s->compdatasize);
		s=s->next;
	}
}

void write_sections(void)
{
	section *s;
	int     i;

	s=sections;
	i=0;

	while (s)
	{
		i++;
		if (fwrite(s->compdata, s->compdatasize, 1, out) != 1)
		{
			perror(__FILE__ ": fwrite failed #6: ");
			exit(1);
		}
		if (verbose)
			printf("Page: %ld, original size %ld bytes, compressed %ld bytes\n", (long)i,
					(long)s->datasize, (long)s->compdatasize);

		totdatasize+=s->datasize; totcompdatasize+=s->compdatasize;

		s=s->next;
	}
}

void write_file_header(void)
{
	uint32_t t = uint32_little(helpver);
	if (fwrite(file_id, 4, 1, out) != 1)
	{
		perror(__FILE__ ": fwrite failed #7: ");
		exit(1);
	}
	if (fwrite(&t, 4, 1, out) != 1)
	{
		perror(__FILE__ ": fwrite failed #8: ");
		exit(1);
	}
}

int main(int argc, char *argv[])
{
	printf("OCPHHC - OpenCP hypertext help compiler v1.2a - written by ryg\n\n");

	if (argc!=3)
	{
		printf("Use: OCPHHC <Infile.EXT> <Outfile.HLP>\n\n");
		return 0;
	}

	in=fopen(argv[1], "rb");
	if (!in)
	{
		fprintf (stderr, "fopen(\"%s\", \"rb\"): %s\n", argv[1], strerror (errno));
		return 1;
	}
	out=fopen(argv[2], "wb");
	if (!out)
	{
		fprintf (stderr, "fopen(\"%s\", \"wb\"): %s\n", argv[2], strerror (errno));
		fclose (in);
		return 1;
	}

	totdatasize=totcompdatasize=0;

	printf("Reading input...\n"); read_lines();
	printf("Compressing page data...\n"); compress_sections();
	printf("Writing output header...\n"); write_file_header();
	printf("Writing page directory...\n"); write_section_directory();
	printf("Writing pages...\n"); write_sections();
	printf("\nDone!\n\n");

	printf("Helpfile statistics:\n\n");
	printf("Complete data size  : %ld bytes (%ld KB)\n", (long)totdatasize,
			(long)(totdatasize>>10));
	printf("Compressed data size: %ld bytes (%ld KB)\n", (long)totcompdatasize,
			(long)(totcompdatasize>>10));
	printf("Ratio               : %2.1f:1\n",
			(float) totdatasize/(float) totcompdatasize);
	printf("\n");

	fclose(in);
	fclose(out);

	for(cursect=sections;cursect;cursect=sections)
	{
		sections=cursect->next;
		free(cursect->name);
		free(cursect->desc);
		free(cursect->data);
		free(cursect->compdata);
		free(cursect);
	}

	return 0;
}
