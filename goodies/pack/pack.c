/* OpenCP Module Player
 * copyright (c) '94-'09 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * PACK.EXE - small utility for creating Quake .PAK files
 *
 * revision history: (please note changes here)
 *  -nb980510   Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *    -first release
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "types.h"

struct __attribute__((packed)) direntry
{
	char name[0x38];
	uint32_t off;
	uint32_t len;
};

int main(int argc, char *argv[])
{
	FILE *output;
	int nfiles;
	struct direntry *dir;
	int i;
	int off=12;

	if (argc<=2)
	{
		fprintf(stderr, "Small utility for creating Quake .PAK files\n");
		fprintf(stderr, "%s target.pak file1 [file2] [...]\n", argv[0]);
		return -1;
	}
	if (!(output=fopen(argv[1], "w+")))
	{
		perror("fopen(argv[1], \"w\")");
		return -1;
	}
	nfiles=argc-2;
	dir=calloc(nfiles, sizeof(dir[0]));
	for (i=0;i<nfiles;i++)
	{
		struct stat st;
		strncpy(dir[i].name, argv[i+2], 0x37);
		if (stat(argv[i+2], &st))
		{
			perror("stat()");
			return -1;
		}
		dir[i].len = st.st_size;
		dir[i].off = off;
		off += dir[i].len;
	}

	if (fwrite("PACK", 4, 1, output)!=1) { perror("fwrite()"); fclose(output); return -1;}
	off = uint32_little(off);
	if (fwrite(&off, sizeof(uint32_t), 1, output)!=1) { perror("fwrite()"); fclose(output); return -1;}
	off = uint32_little(off);
	off=nfiles*sizeof(dir[0]);
	off = uint32_little(off);
	if (fwrite(&off, sizeof(uint32_t), 1, output)!=1) { perror("fwrite()"); fclose(output); return -1;}

	for (i=0; i<nfiles; i++)
	{
		char *buffer;
		FILE *file;
		if (!(file=fopen(dir[i].name, "r"))) { perror("fopen()"); fclose(output); return -1; }
		buffer=calloc(dir[i].len, 1);
		if (fread(buffer, dir[i].len, 1, file)!=1) { perror("fwrite()"); fclose(output); return -1;}
		fclose(file);
		if (fwrite(buffer, dir[i].len, 1, output)!=1) { perror("fwrite)"); fclose(output); return -1;}
		free(buffer);
	}

	for (i=0; i<nfiles; i++)
	{
		dir[i].len = uint32_little(dir[i].len);
		dir[i].off = uint32_little(dir[i].off);
	}
	if (fwrite(dir, sizeof(dir[0]), nfiles, output)!=nfiles) { perror("fwrite()"); fclose(output); return -1;}
	fclose(output);
	free(dir);
	return 0;
}
