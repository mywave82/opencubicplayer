#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct buffer
{
	char *data;
	int len;
	int size;
};

struct buffer *buffer_new()
{
	return calloc (sizeof (struct buffer), 1);
}

void buffer_append_byte (struct buffer *self, char data)
{
	if (self->len == self->size)
	{
		self->size += 1024;
		self->data = realloc (self->data, self->size);
	}
	self->data[self->len++] = data;
}

void buffer_append_word (struct buffer *self, int bigendian, uint_fast16_t data)
{
	if ((self->len+1) >= self->size)
	{
		self->size += 1024;
		self->data = realloc (self->data, self->size);
	}

	if (bigendian)
	{
		self->data[self->len++] = data >> 8;
		self->data[self->len++] = data;
	} else {
		self->data[self->len++] = data;
		self->data[self->len++] = data >> 8;
	}
}

void buffer_append_buffer (struct buffer *self, struct buffer *src)
{
	if (self->size < (self->len + src->len))
	{
		self->size = self->len + src->len;
		self->data = realloc (self->data, self->size);
	}
	memcpy (self->data + self->len, src->data, src->len);
	self->len += src->len;
}

void buffer_free (struct buffer *self)
{
	free (self->data);
	free (self);
}

void buffer_append_vlnum (struct buffer *self, unsigned long long input)
{
	int hit;

	if (input & 0x3f800000000)
	{
		buffer_append_byte (self, ((input >> 35) & 0x7f) | 0x80);
		hit = 1;
	}

	if (hit || (input & 0x7f0000000))
	{
		buffer_append_byte (self, ((input >> 28) & 0x7f) | 0x80);
		hit = 1;
	}

	if (hit || (input & 0xfe00000))
	{
		buffer_append_byte (self, ((input >> 21) & 0x7f) | 0x80);
		hit = 1;
	}

	if (hit || (input & 0x1fc000))
	{
		buffer_append_byte (self, ((input >> 14) & 0x7f) | 0x80);
		hit = 1;
	}

	if (hit || (input & 0x3f80))
	{
		buffer_append_byte (self, ((input >> 7) & 0x7f) | 0x80);
		hit = 1;
	}

	buffer_append_byte (self, input & 0x7f);
}

struct buffer *generate_all_notes(int channel)
{
	int i;
	struct buffer *retval = buffer_new ();

	buffer_append_vlnum (retval, 1);
	buffer_append_byte (retval, 0xc0 | channel); /* PROGRAM */
	buffer_append_byte (retval, 56); /* Trumpet */

	for (i=0; i <= 127; i++)
	{
		buffer_append_vlnum(retval, 100);
		buffer_append_byte (retval, 0x90 | channel); /* NOTE-ON */
		buffer_append_byte (retval, i); /* note */
		buffer_append_byte (retval, 100); /* velocity */

		buffer_append_vlnum(retval, 100);
		buffer_append_byte (retval, 0x90 | channel); /* NOTE-OFF */
		buffer_append_byte (retval, i); /* note */
		buffer_append_byte (retval, 0); /* velocity */
	}

	return retval;
}

struct buffer *generate_header ()
{
	struct buffer *retval = buffer_new ();

	buffer_append_word (retval, 1, 0); /* single track file format */
	buffer_append_word (retval, 1, 1); /* Numer of tracks */
	buffer_append_word (retval, 1, 120); /* 120 BPM */

	return retval;
}

struct buffer *generate_chunk (const char *name, int bigendian, struct buffer *src)
{
	struct buffer *retval = buffer_new();

	buffer_append_byte (retval, name[0]);
	buffer_append_byte (retval, name[1]);
	buffer_append_byte (retval, name[2]);
	buffer_append_byte (retval, name[3]);

	if (bigendian)
	{
		buffer_append_byte (retval, src->len >> 24);
		buffer_append_byte (retval, src->len >> 16);
		buffer_append_byte (retval, src->len >> 8);
		buffer_append_byte (retval, src->len);
	} else {
		buffer_append_byte (retval, src->len);
		buffer_append_byte (retval, src->len >> 8);
		buffer_append_byte (retval, src->len >> 16);
		buffer_append_byte (retval, src->len >> 24);
	}

	buffer_append_buffer (retval, src);

	return retval;
}

int main (int argc, char *argv[])
{
	struct buffer *head = generate_header ();
	struct buffer *sheet = generate_all_notes (0);

	struct buffer *MThd = generate_chunk ("MThd", 1, head);
	struct buffer *MTrk = generate_chunk ("MTrk", 1, sheet);

	buffer_free (head);
	buffer_free (sheet);

	write (1, MThd->data, MThd->len);
	write (1, MTrk->data, MTrk->len);

	buffer_free (MThd);
	buffer_free (MTrk);

	return 0;
}
