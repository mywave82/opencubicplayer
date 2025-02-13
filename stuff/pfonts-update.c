#include <stdio.h>
#include <string.h>
int fixed = 0;
int matched = 0;

static unsigned char hex(unsigned char i)
{
	if (i>15) return ' ';
	return "0123456789abcdef"[i];
}

static void patch(char *line)
{
	int f = 0;
	unsigned char h1=0, h2=0;
	if ((strlen(line) != 37) && (strlen(line) != 38)) return;
	if (line[0] != '\t') return;
	if ((line[1] != ' ') && (line[1]!='{')) return;
	if (line[2] != '/') return;
	if (line[3] != '*') return;
	if (line[4] != '"') return;
	if (line[5] == ' ') {} else if (line[5] == '#') {h1 |= 0x80;} else return;
	if (line[6] == ' ') {} else if (line[6] == '#') {h1 |= 0x40;} else return;
	if (line[7] == ' ') {} else if (line[7] == '#') {h1 |= 0x20;} else return;
	if (line[8] == ' ') {} else if (line[8] == '#') {h1 |= 0x10;} else return;
	if (line[9] == ' ') {} else if (line[9] == '#') {h1 |= 0x08;} else return;
	if (line[10] == ' ') {} else if (line[10] == '#') {h1 |= 0x04;} else return;
	if (line[11] == ' ') {} else if (line[11] == '#') {h1 |= 0x02;} else return;
	if (line[12] == ' ') {} else if (line[12] == '#') {h1 |= 0x01;} else return;
	if (line[13] == ' ') {} else if (line[13] == '#') {h2 |= 0x80;} else return;
	if (line[14] == ' ') {} else if (line[14] == '#') {h2 |= 0x40;} else return;
	if (line[15] == ' ') {} else if (line[15] == '#') {h2 |= 0x20;} else return;
	if (line[16] == ' ') {} else if (line[16] == '#') {h2 |= 0x10;} else return;
	if (line[17] == ' ') {} else if (line[17] == '#') {h2 |= 0x08;} else return;
	if (line[18] == ' ') {} else if (line[18] == '#') {h2 |= 0x04;} else return;
	if (line[19] == ' ') {} else if (line[19] == '#') {h2 |= 0x02;} else return;
	if (line[20] == ' ') {} else if (line[20] == '#') {h2 |= 0x01;} else return;
	if (line[21] != '"') return;
	if (line[22] != '*') return;
	if (line[23] != '/') return;
	if (line[24] != ' ') return;
	if (line[25] != '0') return;
	if (line[26] != 'x') return;
	if (line[29] != ',') return;
	if (line[30] != ' ') return;
	if (line[31] != '0') return;
	if (line[32] != 'x') return;

	if (line[27] != hex(h1 >> 4)) { line[27] = hex(h1 >> 4); f=1; }
	if (line[28] != hex(h1 & 15)) { line[28] = hex(h1 & 15); f=1; }
	if (line[33] != hex(h2 >> 4)) { line[33] = hex(h2 >> 4); f=1; }
	if (line[34] != hex(h2 & 15)) { line[34] = hex(h2 & 15); f=1; }

	fixed += f;
	matched++;
}

int main (int argc, char *argv[])
{
	char line[256];
	while (fgets(line, sizeof (line), stdin))
	{
		patch (line);
		fputs (line, stdout);
	}
	fflush (stdout);
	fprintf (stderr, "Updated %d of %d lines\n", fixed, matched);
	return 0;
}
