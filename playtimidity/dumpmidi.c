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

struct MIDI_Session
{
	int fd;
	unsigned char *data;
	size_t data_len;
	size_t data_mmaped_len;

	int level;
};

static void fprintf_level(FILE *f, struct MIDI_Session *s)
{
	int i;
	for (i=0; i < s->level; i++)
	{
		fprintf (f, " ");
	}
}

struct __attribute__((packed)) RIFFHEADER
{
	unsigned char signature[4];
	uint32_t size;
};

struct __attribute__((packed)) MthdHEADER
{
	uint16_t mtype;
	uint16_t trknum;
	uint16_t tempo;
};
static void MthdHEADER_endian (struct MthdHEADER *a)
{
	a->mtype = uint16_big (a->mtype);
	a->trknum = uint16_big (a->trknum);
	a->tempo = uint16_big (a->tempo);
}


static int riff_dechunk(struct MIDI_Session *s, unsigned char *buffer, uint32_t len, int (*callback)(struct MIDI_Session *s, unsigned char *signature, unsigned char *buffer, uint32_t len))
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

		/* Woha.. RIFF and MIDI have reversed endian in chunk headers... */
		if (memcmp (head->signature, "RIFF", 4) && memcmp (head->signature, "data", 4))
		{
			head->size = uint32_big(head->size);
		} else {
			head->size = uint32_little(head->size);
		}

		buffer += 8;
		len -= 8;

		if (head->size > len)
		{
			fprintf (stderr, "riff_dechunk: not enough data left to assemble chunk payload (%d > %d)\n", head->size, len);
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

static inline unsigned long readvlnum(unsigned char **ptr, unsigned char *endptr, int *eof)
{
	unsigned long num=0;
	while (1)
	{
		if(*ptr>=endptr)
		{
			*eof=1;
			break; /* END OF DATA */
		}
		num=(num<<7)|((**ptr)&0x7F);
		if (!(*((*ptr)++)&0x80))
			break;
	}
	return num;
}

static inline uint8_t readu8(unsigned char **ptr, unsigned char *endptr, int *eof)
{
	if(*ptr>=endptr)
	{
		*eof=1;
		return 0;
	}
	return *((*ptr)++);
}

static int parse_RIFF(struct MIDI_Session *s, unsigned char *head, unsigned char *buffer, uint32_t len)
{
	fprintf_level (stderr, s);
	fprintf (stderr, "%c%c%c%c %d\n", head[0], head[1], head[2], head[3], (int)len);

	if ((head[0] == 'R') &&
	    (head[1] == 'I') &&
	    (head[2] == 'F') &&
	    (head[3] == 'F'))
	{
		if (len <= 4)
		{
			fprintf (stderr, "Too little space to search for inner headers\n");
			return -1;
		}
		if (memcmp (buffer, "RMID", 4))
		{
			fprintf (stderr, "Not a RIFF MIDI file\n");
			return -1;
		}
		s->level++;
		if (riff_dechunk(s, buffer+4, len-4, parse_RIFF))
			return -1;
		s->level--;
		return 0;
	}

	if ((head[0] == 'd') &&
	    (head[1] == 'a') &&
	    (head[2] == 't') &&
	    (head[3] == 'a'))
	{
		s->level++;
		if (riff_dechunk(s, buffer, len, parse_RIFF))
			return -1;
		s->level--;
		return 0;
	}

	if ((head[0] == 'M') &&
	    (head[1] == 'T') &&
	    (head[2] == 'h') &&
	    (head[3] == 'd'))
	{
		struct MthdHEADER *Mthd = (struct MthdHEADER *)buffer;

		if (len < 6)
		{
			fprintf (stderr, "Header is too small\n");
			return -1;
		}
		if (len > 6)
		{
			fprintf (stderr, "(warning, header is longer than 6 bytes\n");
		}

		MthdHEADER_endian (Mthd);

		fprintf_level (stderr, s); fprintf (stderr, "mtype=%d %s\n", Mthd->mtype, Mthd->mtype==0?"Single track file format":Mthd->mtype==1?"multiple track file format":Mthd->mtype==2?"multiple song file format":"unknown");
		fprintf_level (stderr, s); fprintf (stderr, "trknum=%d\n", Mthd->trknum);
		fprintf_level (stderr, s); fprintf (stderr, "tempo=%d (%d %s)\n", Mthd->tempo, Mthd->tempo<0?-Mthd->tempo:Mthd->tempo, Mthd->tempo<0?"SMPTE units":"bpm");

		return 0;
	}

	if ((head[0] == 'M') &&
	    (head[1] == 'T') &&
	    (head[2] == 'r') &&
	    (head[3] == 'k'))
	{
		unsigned char *endptr = buffer + len;
		unsigned char *ptr = buffer;
		uint8_t prevevent = 0xff;

		while (ptr != endptr)
		{
			uint32_t time;
			uint8_t event;
			int eof = 0;

			time = readvlnum (&ptr, endptr, &eof);
			event = readu8 (&ptr, endptr, &eof);

			/* inline-MIDI file caching of registers */
			if (!(event & 0x80))
			{
				if ((prevevent & 0xf0) == 0xf0)
				{
					fprintf (stderr, "(error) MIDI file contains invalid cache registers usage (file-compression.. cachehit=0x%02x prevevent=0x%02x\n", event, prevevent);
					return -1;
				}
				event = prevevent;
				ptr--;
			};
			prevevent = event;

			if (eof) { fprintf (stderr, "premature eof encountered (1)\n"); return -1; }

			if ((event & 0xf0) == 0x80)
			{
				uint8_t ch = event & 0x0f;
				uint8_t key = readu8 (&ptr, endptr, &eof);
				uint8_t vel = readu8 (&ptr, endptr, &eof);

				if (eof) { fprintf (stderr, "premature eof encountered (2)\n"); return -1; }
				fprintf_level (stderr, s); fprintf (stderr, "time %d, event 0x%02x, channel %d, NOTE OFF, key %d, velocity %d\n", time, event, ch, key, vel);
			} else if ((event & 0xf0) == 0x90)
			{
				uint8_t ch = event & 0x0f;
				uint8_t key = readu8 (&ptr, endptr, &eof);
				uint8_t vel = readu8 (&ptr, endptr, &eof);

				if (eof) { fprintf (stderr, "premature eof encountered (3)\n"); return -1; }
				fprintf_level (stderr, s); fprintf (stderr, "time %d, event 0x%02x, channel %d, NOTE ON, key %d, velocity %d\n", time, event, ch, key, vel);
			} else if ((event & 0xf0) == 0xa0)
			{
				uint8_t ch = event & 0x0f;
				uint8_t key = readu8 (&ptr, endptr, &eof);
				uint8_t vel = readu8 (&ptr, endptr, &eof);

				if (eof) { fprintf (stderr, "premature eof encountered (4)\n"); return -1; }
				fprintf_level (stderr, s); fprintf (stderr, "time %d, event 0x%02x, channel %d, POLY, key %d, velocity %d\n", time, event, ch, key, vel);
			} else if ((event & 0xf0) == 0xb0)
			{
				uint8_t ch = event & 0x0f;
				uint8_t control = readu8 (&ptr, endptr, &eof);
				uint8_t value = readu8 (&ptr, endptr, &eof);

				if (eof) { fprintf (stderr, "premature eof encountered (5)\n"); return -1; }
				fprintf_level (stderr, s); fprintf (stderr, "time %d, event 0x%02x, channel %d, CONTROL, control %d, value %d", time, event, ch, control, value);
				if (control == 0x00)
				{
					fprintf (stderr, " BANK SELECT MSB\n");
				} else if (control == 0x01)
				{
					fprintf (stderr, " MODULATION WHEEL MSB\n");
				} else if (control == 0x02)
				{
					fprintf (stderr, " BREATH CONTROL MSB\n");
				} else if (control == 0x04)
				{
					fprintf (stderr, " FOOT CONTROLLER MSB\n");
				} else if (control == 0x05)
				{
					fprintf (stderr, " PORTAMENTO TIME MSB\n");
				} else if (control == 0x06)
				{
					fprintf (stderr, " DATA ENTRY MSB\n");
				} else if (control == 0x07)
				{
					fprintf (stderr, " CHANNEL VOLUME MSB\n");
				} else if (control == 0x08)
				{
					fprintf (stderr, " BALANCE MSB\n");
				} else if (control == 0x0a)
				{
					fprintf (stderr, " PAN MSB\n");
				} else if (control == 0x0b)
				{
					fprintf (stderr, " EXPRESSION CONTROLLER MSB\n");
				} else if (control == 0x0c)
				{
					fprintf (stderr, " EFFECT CONTROL #1 MSB\n");
				} else if (control == 0x0d)
				{
					fprintf (stderr, " EFFECT CONTROL #2 MSB\n");
				} else if (control == 0x10)
				{
					fprintf (stderr, " GENERAL PURPOSE CONTROLLER #1 MSB\n");
				} else if (control == 0x11)
				{
					fprintf (stderr, " GENERAL PURPOSE CONTROLLER #2 MSB \n");
				} else if (control == 0x12)
				{
					fprintf (stderr, " GENERAL PURPOSE CONTROLLER #3 MSB\n");
				} else if (control == 0x13)
				{
					fprintf (stderr, " GENERAL PURPOSE CONTROLLER #4 MSB\n");
				} else if (control == 0x20)
				{
					fprintf (stderr, " BANK SELECT LSB\n");
				} else if (control == 0x21)
				{
					fprintf (stderr, " MODULATION WHEEL LSB\n");
				} else if (control == 0x22)
				{
					fprintf (stderr, " BREATH CONTROL LSB\n");
				} else if (control == 0x24)
				{
					fprintf (stderr, " FOOT CONTROLLER LSB\n");
				} else if (control == 0x25)
				{
					fprintf (stderr, " PORTAMENTO TIME LSB\n");
				} else if (control == 0x26)
				{
					fprintf (stderr, " DATA ENTRY LSB\n");
				} else if (control == 0x27)
				{
					fprintf (stderr, " CHANNEL VOLUME LSB\n");
				} else if (control == 0x28)
				{
					fprintf (stderr, " BALANCE LSB\n");
				} else if (control == 0x2a)
				{
					fprintf (stderr, " PAN LSB\n");
				} else if (control == 0x2b)
				{
					fprintf (stderr, " EXPRESSION CONTROLLER LSB\n");
				} else if (control == 0x2c)
				{
					fprintf (stderr, " EFFECT CONTROL #1 LSB\n");
				} else if (control == 0x2d)
				{
					fprintf (stderr, " EFFECT CONTROL #2 LSB\n");
				} else if (control == 0x30)
				{
					fprintf (stderr, " GENERAL PURPOSE CONTROLLER #1 LSB\n");
				} else if (control == 0x31)
				{
					fprintf (stderr, " GENERAL PURPOSE CONTROLLER #2 LSB \n");
				} else if (control == 0x32)
				{
					fprintf (stderr, " GENERAL PURPOSE CONTROLLER #3 LSB\n");
				} else if (control == 0x33)
				{
					fprintf (stderr, " GENERAL PURPOSE CONTROLLER #4 LSB\n");
				} else if (control == 0x40)
				{
					fprintf (stderr, " DAMPER PEDAL (sustain)\n");
				} else if (control == 0x41)
				{
					fprintf (stderr, " PORTAMENTO (on/off)\n");
				} else if (control == 0x42)
				{
					fprintf (stderr, " SUSTENUTO PEDAL (on/off)\n");
				} else if (control == 0x43)
				{
					fprintf (stderr, " SOFT PEDAL (on/off)\n");
				} else if (control == 0x44)
				{
					fprintf (stderr, " LEGATO FOOTSWITCH (on/off)\n");
				} else if (control == 0x45)
				{
					fprintf (stderr, " HOLD 2 (on/off)\n");
				} else if (control == 0x46)
				{
					fprintf (stderr, " SOUND CONTROLLER #1 (Sound variation)\n");
				} else if (control == 0x47)
				{
					fprintf (stderr, " SOUND CONTROLLER #2 (Timbre)\n");
				} else if (control == 0x48)
				{
					fprintf (stderr, " SOUND CONTROLLER #3 (Release Time)\n");
				} else if (control == 0x49)
				{
					fprintf (stderr, " SOUND CONTROLLER #4 (Attack Time)\n");
				} else if (control == 0x4a)
				{
					fprintf (stderr, " SOUND CONTROLLER #5 (Brightness)\n");
				} else if (control == 0x4b)
				{
					fprintf (stderr, " SOUND CONTROLLER #6\n");
				} else if (control == 0x4c)
				{
					fprintf (stderr, " SOUND CONTROLLER #7\n");
				} else if (control == 0x4d)
				{
					fprintf (stderr, " SOUND CONTROLLER #8\n");
				} else if (control == 0x4e)
				{
					fprintf (stderr, " SOUND CONTROLLER #9\n");
				} else if (control == 0x4f)
				{
					fprintf (stderr, " SOUND CONTROLLER #10\n");
				} else if (control == 0x50)
				{
					fprintf (stderr, " GENERAL PURPOSE CONTROLLER #5\n");
				} else if (control == 0x51)
				{
					fprintf (stderr, " GENERAL PURPOSE CONTROLLER #6\n");
				} else if (control == 0x52)
				{
					fprintf (stderr, " GENERAL PURPOSE CONTROLLER #7\n");
				} else if (control == 0x53)
				{
					fprintf (stderr, " GENERAL PURPOSE CONTROLLER #8\n");
				} else if (control == 0x54)
				{
					fprintf (stderr, " PORTAMENTA CONTROL (set source note)\n");
				} else if (control == 0x5b)
				{
					fprintf (stderr, " EFFECTS #1 DEPTH\n");
				} else if (control == 0x5c)
				{
					fprintf (stderr, " EFFECTS #2 DEPTH\n");
				} else if (control == 0x5d)
				{
					fprintf (stderr, " EFFECTS #3 DEPTH\n");
				} else if (control == 0x5e)
				{
					fprintf (stderr, " EFFECTS #4 DEPTH\n");
				} else if (control == 0x5f)
				{
					fprintf (stderr, " EFFECTS #5 DEPTH\n");
				} else if (control == 0x60)
				{
					fprintf (stderr, " DATA ENTRY+=1\n");
				} else if (control == 0x61)
				{
					fprintf (stderr, " DATA ENTRY-=1\n");
				} else if (control == 0x62)
				{
					fprintf (stderr, " NON-REGISTERED PARAMETER NUMBER LSB\n");
				} else if (control == 0x63)
				{
					fprintf (stderr, " NON-REGISTERED PARAMETER NUMBER MSB\n");
				} else if (control == 0x64)
				{
					fprintf (stderr, " REGISTERED PARAMETER NUMBER LSB\n");
				} else if (control == 0x65)
				{
					fprintf (stderr, " REGISTERED PARAMETER NUMBER MSB\n");
				} else if ((control == 0x78) && (value == 0x00))
				{
					fprintf (stderr, " ALL SOUND OFF\n");
				} else if ((control == 0x79) && (value == 0x00))
				{
					fprintf (stderr, " reset all controllers\n");
				} else if (control == 0x7a)
				{
					fprintf (stderr, " LOCAL CONTROL (reconnect/disconnect)\n");
				} else if ((control == 0x7b) && (value == 0x00))
				{
					fprintf (stderr, " ALL NOTES OFF\n");
				} else if ((control == 0x7c) && (value == 0x00))
				{
					fprintf (stderr, " OMNI MODE OFF\n");
				} else if ((control == 0x7D) && (value == 0x00))
				{
					fprintf (stderr, " OMNI MODE OM\n");
				} else if ((control == 0x7e) && (value == 0x00))
				{
					fprintf (stderr, " MONO MODE OFF\n");
				} else if ((control == 0x7f) && (value == 0x00))
				{
					fprintf (stderr, " MONO MODE ON\n");
				} else {
					fprintf (stderr, " unknown\n");
				}
			} else if ((event & 0xf0) == 0xc0)
			{
				uint8_t ch = event & 0x0f;
				uint8_t program = readu8 (&ptr, endptr, &eof);

				if (eof) { fprintf (stderr, "premature eof encountered (6)\n"); return -1; }
				fprintf_level (stderr, s); fprintf (stderr, "time %d, event 0x%02x, channel %d, PROGRAM, program/preset %d\n", time, event, ch, program);
			} else if ((event & 0xf0) == 0xd0)
			{
				uint8_t ch = event & 0x0f;
				uint8_t p1 = readu8 (&ptr, endptr, &eof);

				if (eof) { fprintf (stderr, "premature eof encountered (7)\n"); return -1; }
				fprintf_level (stderr, s); fprintf (stderr, "time %d, event 0x%02x, channel %d, AFTERTOUCH, value %d\n", time, event, ch, p1 & 0x7f);
			} else if ((event & 0xf0) == 0xe0)
			{
				uint8_t ch = event & 0x0f;
				uint8_t p1 = readu8 (&ptr, endptr, &eof);
				uint8_t p2 = readu8 (&ptr, endptr, &eof);

				if (eof) { fprintf (stderr, "premature eof encountered (7)\n"); return -1; }
				fprintf_level (stderr, s); fprintf (stderr, "time %d, event 0x%02x, channel %d, PITCH WHEEL, value %d (0x%04x)\n", time, event, ch, (int)((p1 & 0x7f) | ((p2 & 0x7f)<<7)) - 0x2000, (p1 & 0x7f) | ((p2 & 0x7f)<<7));
			} else if (event == 0xf0)
			{
				uint32_t len = readvlnum (&ptr, endptr, &eof);
				int i;

				if (ptr + len >= endptr) eof = 1;
				if (eof) { fprintf (stderr, "premature eof encountered (8)\n"); return -1; }

				fprintf_level (stderr, s); fprintf (stderr, "time %d, event 0x%02x, F0 Sysex Event", time, event);
				for (i=0; i < len; i++)
				{
					fprintf (stderr, " %02x", *(ptr++));
				}
			} else if (event == 0xf7)
			{
				uint32_t len = readvlnum (&ptr, endptr, &eof);
				int i;

				if (ptr + len >= endptr) eof = 1;
				if (eof) { fprintf (stderr, "premature eof encountered (9)\n"); return -1; }

				fprintf_level (stderr, s); fprintf (stderr, "time %d, event 0x%02x, F7 Sysex Event (escape)", time, event);
				for (i=0; i < len; i++)
				{
					fprintf (stderr, " %02x", *(ptr++));
				}
			} else if (event == 0xff)
			{
				uint8_t type = readu8 (&ptr, endptr, &eof);
				uint32_t len = readvlnum (&ptr, endptr, &eof);
				int i;

				if ((ptr + len) > endptr) eof = 1;
				if (eof) { fprintf (stderr, "premature eof encountered (10) (ptr=%p endptr=%p len=%d)\n", ptr, endptr, len); return -1; }

				fprintf_level (stderr, s); fprintf (stderr, "time %d, event 0x%02x, FF META Event 0x%02x ", time, event, type);

				if (type == 0x00)
				{
					uint16_t data;
					if (len != 2)
					{
						fprintf (stderr, "(error) FF META event Sequence number is not 2 bytes long\n");
						return -1;
					}
					data = ptr[0] | (ptr[1]<<8);
					fprintf (stderr, "SequenceNumber %d\n", data);
				} else if (type == 0x01)
				{
					int i;
					char c;
					fprintf (stderr, "Text \"");
					for (i=0; i < len; i++)
					{
						c = *(ptr++);
						if ((c>=32) && (c<=127))
							fprintf (stderr, "%c", c);
						else
							fprintf (stderr, "\\x%02x", (unsigned char)c);
					}
					fprintf (stderr, "\"\n");
				} else if (type == 0x02)
				{
					int i;
					char c;
					fprintf (stderr, "Copyright \"");
					for (i=0; i < len; i++)
					{
						c = *(ptr++);
						if ((c>=32) && (c<=127))
							fprintf (stderr, "%c", c);
						else
							fprintf (stderr, "\\x%02x", (unsigned char)c);
					}
					fprintf (stderr, "\"\n");
				} else if (type == 0x03)
				{
					int i;
					char c;
					fprintf (stderr, "Sequence/Track Name \"");
					for (i=0; i < len; i++)
					{
						c = *(ptr++);
						if ((c>=32) && (c<=127))
							fprintf (stderr, "%c", c);
						else
							fprintf (stderr, "\\x%02x", (unsigned char)c);
					}
					fprintf (stderr, "\"\n");
				} else if (type == 0x04)
				{
					int i;
					char c;
					fprintf (stderr, "Instrument Name \"");
					for (i=0; i < len; i++)
					{
						c = *(ptr++);
						if ((c>=32) && (c<=127))
							fprintf (stderr, "%c", c);
						else
							fprintf (stderr, "\\x%02x", (unsigned char)c);
					}
					fprintf (stderr, "\"\n");
				} else if (type == 0x05)
				{
					int i;
					char c;
					fprintf (stderr, "Lyric \"");
					for (i=0; i < len; i++)
					{
						c = *(ptr++);
						if ((c>=32) && (c<=127))
							fprintf (stderr, "%c", c);
						else
							fprintf (stderr, "\\x%02x", (unsigned char)c);
					}
					fprintf (stderr, "\"\n");
				} else if (type == 0x06)
				{
					int i;
					char c;
					fprintf (stderr, "Marker \"");
					for (i=0; i < len; i++)
					{
						c = *(ptr++);
						if ((c>=32) && (c<=127))
							fprintf (stderr, "%c", c);
						else
							fprintf (stderr, "\\x%02x", (unsigned char)c);
					}
					fprintf (stderr, "\"\n");
				} else if (type == 0x02)
				{
					int i;
					char c;
					fprintf (stderr, "Cue Point \"");
					for (i=0; i < len; i++)
					{
						c = *(ptr++);
						if ((c>=32) && (c<=127))
							fprintf (stderr, "%c", c);
						else
							fprintf (stderr, "\\x%02x", (unsigned char)c);
					}
					fprintf (stderr, "\"\n");
				} else if (type == 0x20)
				{
					uint8_t data = *(ptr++);

					if (len != 1)
					{
						fprintf (stderr, "(error) FF META event MIDI Channel Prefix number that is not 1 byte long\n");
						return -1;
					}
					fprintf (stderr, "MIDI Channel Prefix %d\n", data);
				} else if (type == 0x21)
				{
					uint8_t data = *(ptr++);

					if (len != 1)
					{
						fprintf (stderr, "(error) FF META event MIDI Port number that is not 1 byte long\n");
						return -1;
					}
					fprintf (stderr, "MIDI Port %d\n", data);
				} else if (type == 0x2f)
				{
					if (len != 0)
					{
						fprintf (stderr, "(error) FF META event End Of Track that is not 0 bytes long\n");
						return -1;
					}
					fprintf (stderr, "End of Track\n");
				} else if (type == 0x51)
				{
					uint32_t data;
					if (len != 3)
					{
						fprintf (stderr, "(error) FF META event Sequence number that is not 3 bytes long\n");
						return -1;
					}
					data = ptr[0] | (ptr[1]<<8) | ptr[2]<<16;
					fprintf (stderr, "Set Tempo %d\n", data);

					ptr += 3;
				} else if (type == 0x54)
				{
					if (len != 5)
					{
						fprintf (stderr, "(error) FF META event SMPTE Offset that is not 5 bytes long\n");
						return -1;
					}
					fprintf (stderr, "SMPTE Offset hour %d, minute %d, second %d, frame %d, fraction %d\n", ptr[0], ptr[1], ptr[2], ptr[3], ptr[4]);

					ptr+=5;
				} else if (type == 0x58)
				{
					if (len != 4)
					{
						fprintf (stderr, "(error) FF META event Time Signature that is not 4 bytes long\n");
						return -1;
					}
					fprintf (stderr, "Time Signature numerator %d, denominator 2^%d, clocks %d, %d/32 notes per 24 MIDI clock\n", ptr[0], ptr[1], ptr[2], ptr[3]);

					ptr+=4;
				} else if (type == 0x59)
				{
					if (len != 2)
					{
						fprintf (stderr, "(error) FF META event Key Signature that is not 2 bytes long\n");
						return -1;
					}
					fprintf (stderr, "Key Signature %d half notes, key %d(%s)\n", (signed)ptr[0], ptr[1], ptr[1]==0?"major":ptr[1]==1?"minor":"unknown");

					ptr+=2;
				} else if (type == 0x7f)
				{
					fprintf (stderr, "Sequencer-Specific Meta-Event ");
					for (i=0; i < len; i++)
					{
						fprintf (stderr, " %02x", *(ptr++));
					}
					fprintf (stderr, "\n");
				} else {
					fprintf (stderr, "Unknown ");
					for (i=0; i < len; i++)
					{
						fprintf (stderr, " %02x", *(ptr++));
					}
					fprintf (stderr, "\n");
				}
			} else {
				fprintf (stderr, "(uknown event 0x%02x)\n", event);
			}
		}
	}
	return 0;
}

int main(int argc, char *argv[])
{
	struct MIDI_Session s;
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

	if (riff_dechunk (&s, s.data, s.data_len, parse_RIFF))
	{
		goto failed;
	}

failed:
	munmap (s.data, s.data_mmaped_len);
	close (s.fd);
	return 0;
}
