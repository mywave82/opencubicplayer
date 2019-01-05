/* OpenCP Module Player
 * copyright (c) 2016 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Generic ringbuffer pointer handler
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "config.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "ringbuffer.h"

struct ringbuffer_callback_hook_t
{
	void (*callback)(void *arg, int samples_ago);
	void *arg;
	int samples;
};

struct ringbuffer_t
{
	int flags;
	int cache_sample_shift; /* how many bits to shift left to convert from samples to bytes */

	int buffersize;

	int cache_write_available;
	int cache_read_available;
	int cache_processing_available;

	int tail;
	int processing;
	int head;

	struct ringbuffer_callback_hook_t *tail_callbacks;
	int tail_callbacks_size;
	int tail_callbacks_fill;

	struct ringbuffer_callback_hook_t *processing_callbacks;
	int processing_callbacks_size;
	int processing_callbacks_fill;
};

void ringbuffer_reset (struct ringbuffer_t *self)
{
	int i;

	self->head = 0;
	self->processing = 0;
	self->tail = 0;

	self->cache_write_available = self->buffersize - 1;
	self->cache_read_available = 0;
	self->cache_processing_available = 0;

	for (i=0; i < self->processing_callbacks_fill; i++)
	{
		self->processing_callbacks[i].callback (self->processing_callbacks[i].arg, 1 - self->processing_callbacks[i].samples);
	}
	self->processing_callbacks_fill = 0;

	for (i=0; i < self->tail_callbacks_fill; i++)
	{
		self->tail_callbacks[i].callback (self->tail_callbacks[i].arg, 1 - self->tail_callbacks[i].samples);
	}
	self->tail_callbacks_fill = 0;
}

void ringbuffer_static_initialize (struct ringbuffer_t *self, int flags, int buffersize_samples)
{
	self->flags = flags;
	self->cache_sample_shift = 0;

	/* we can only have one bitdepth */
	assert  ( ((!!(self->flags & RINGBUFFER_FLAGS_8BIT)) + (!!(self->flags & RINGBUFFER_FLAGS_16BIT)) + (!!(self->flags & RINGBUFFER_FLAGS_FLOAT))) == 1);

	if (self->flags & RINGBUFFER_FLAGS_STEREO)
	{
		self->cache_sample_shift++;
	}

	if (self->flags & RINGBUFFER_FLAGS_16BIT)
	{
		self->cache_sample_shift++;
	}

	if (self->flags & RINGBUFFER_FLAGS_FLOAT)
	{
		self->cache_sample_shift+=2;
	}

	self->buffersize = buffersize_samples;

	self->processing_callbacks_fill = 0;

	self->tail_callbacks_fill = 0;

	ringbuffer_reset (self);
}


struct ringbuffer_t *ringbuffer_new_samples(int flags, int buffersize_samples)
{
	struct ringbuffer_t *self = calloc (sizeof (*self), 1);

	ringbuffer_static_initialize (self, flags, buffersize_samples);

	return self;
}

void ringbuffer_free(struct ringbuffer_t *self)
{
	free (self->processing_callbacks);
	self->processing_callbacks = 0;
	self->processing_callbacks_size = 0;

	free (self->tail_callbacks);
	self->tail_callbacks = 0;
	self->tail_callbacks_size = 0;

	free (self);
}

void ringbuffer_tail_consume_samples(struct ringbuffer_t *self, int samples)
{
	assert (samples <= self->cache_read_available);
	self->tail = (self->tail + samples) % self->buffersize;

	self->cache_read_available -= samples;

	self->cache_write_available += samples;

	if (self->tail_callbacks_fill)
	{
		int i;
		for (i=0; i < self->tail_callbacks_fill; i++)
		{
			self->tail_callbacks[i].samples -= samples;
		}
		while (self->tail_callbacks_fill)
		{
			if (self->tail_callbacks[0].samples >= 0)
			{
				break;
			}
			self->tail_callbacks[0].callback (self->tail_callbacks[0].arg, 1 - self->tail_callbacks[0].samples);
			memmove (self->tail_callbacks, self->tail_callbacks + 1, (self->tail_callbacks_fill - 1) * sizeof (self->tail_callbacks[0]));
			self->tail_callbacks_fill--;
		}
	}

	assert ((self->cache_read_available + self->cache_write_available + self->cache_processing_available + 1) == self->buffersize);
}

void ringbuffer_tail_set_samples(struct ringbuffer_t *self, int pos)
{
	int samples = (self->buffersize + pos - self->tail) % self->buffersize;

	ringbuffer_tail_consume_samples (self, samples);
}


void ringbuffer_processing_consume_samples(struct ringbuffer_t *self, int samples)
{
	assert (self->flags & RINGBUFFER_FLAGS_PROCESS);

	assert (samples <= self->cache_processing_available);

	self->processing = (self->processing + samples) % self->buffersize;

	self->cache_processing_available -= samples;

	self->cache_read_available += samples;

	if (self->processing_callbacks_fill)
	{
		int i;
		for (i=0; i < self->processing_callbacks_fill; i++)
		{
			self->processing_callbacks[i].samples -= samples;
		}
		while (self->processing_callbacks_fill)
		{
			if (self->processing_callbacks[0].samples >= 0)
			{
				break;
			}
			self->processing_callbacks[0].callback (self->processing_callbacks[0].arg, 1 - self->processing_callbacks[0].samples);
			memmove (self->processing_callbacks, self->processing_callbacks + 1, (self->processing_callbacks_fill - 1) * sizeof (self->processing_callbacks[0]));
			self->processing_callbacks_fill--;
		}
	}
	assert ((self->cache_read_available + self->cache_write_available + self->cache_processing_available + 1) == self->buffersize);
}

void ringbuffer_processing_set_samples(struct ringbuffer_t *self, int pos)
{
	int samples = (self->buffersize + pos - self->processing) % self->buffersize;

	ringbuffer_processing_consume_samples (self, samples);
}

void ringbuffer_head_add_samples(struct ringbuffer_t *self, int samples)
{
	assert (samples <= self->cache_write_available);

	self->head = (self->head + samples) % self->buffersize;

	self->cache_write_available -= samples;

	if (self->flags & RINGBUFFER_FLAGS_PROCESS)
	{
		self->cache_processing_available += samples;
	} else {
		self->cache_read_available += samples;
	}

	assert ((self->cache_read_available + self->cache_write_available + self->cache_processing_available + 1) == self->buffersize);
}

void ringbuffer_head_set_samples(struct ringbuffer_t *self, int pos)
{
	int samples = (self->buffersize + pos - self->head) % self->buffersize;

	ringbuffer_head_add_samples (self, samples);
}

int ringbuffer_get_tail_available_samples (struct ringbuffer_t *self)
{
	return self->cache_read_available;
}

int ringbuffer_get_processing_available_samples (struct ringbuffer_t *self)
{
	return self->cache_processing_available;
}

int ringbuffer_get_head_available_samples (struct ringbuffer_t *self)
{
	return self->cache_write_available;
}

void ringbuffer_get_tail_samples (struct ringbuffer_t *self, int *pos1, int *length1, int *pos2, int *length2)
{
	if (!self->cache_read_available)
	{
		goto clear1;
	}

	*pos1 = self->tail;
	if ((self->tail + self->cache_read_available) <= self->buffersize)
	{
		*length1 = self->cache_read_available;
		goto clear2;
	}

	*length1 = self->buffersize - self->tail;

	*pos2 = 0;
	*length2 = self->cache_read_available - *length1;

	return;

clear1:
	*pos1 = -1;
	*length1 = 0;
clear2:
	*pos2 = -1;
	*length2 = 0;
}

void ringbuffer_get_processing_samples (struct ringbuffer_t *self, int *pos1, int *length1, int *pos2, int *length2)
{
	assert (self->flags & RINGBUFFER_FLAGS_PROCESS);

	if (!self->cache_processing_available)
	{
		goto clear1;
	}

	*pos1 = self->processing;
	if ((self->processing + self->cache_processing_available) <= self->buffersize)
	{
		*length1 = self->cache_processing_available;
		goto clear2;
	}

	*length1 = self->buffersize - self->processing;

	*pos2 = 0;
	*length2 = self->cache_processing_available - *length1;

	return;

clear1:
	*pos1 = -1;
	*length1 = 0;
clear2:
	*pos2 = -1;
	*length2 = 0;
}

void ringbuffer_get_head_samples (struct ringbuffer_t *self, int *pos1, int *length1, int *pos2, int *length2)
{
	if (!self->cache_write_available)
	{
		goto clear1;
	}

	*pos1 = self->head;
	if ((self->head + self->cache_write_available) <= self->buffersize)
	{
		*length1 = self->cache_write_available;
		goto clear2;
	}

	*length1 = self->buffersize - self->head;

	*pos2 = 0;
	*length2 = self->cache_write_available - *length1;

	return;

clear1:
	*pos1 = -1;
	*length1 = 0;
clear2:
	*pos2 = -1;
	*length2 = 0;
}

void ringbuffer_tail_consume_bytes(struct ringbuffer_t *self, int bytes)
{
	ringbuffer_tail_consume_samples (self, bytes >> self->cache_sample_shift);
}

void ringbuffer_tail_set_bytes(struct ringbuffer_t *self, int pos)
{
	ringbuffer_tail_set_samples (self, pos >> self->cache_sample_shift);
}

void ringbuffer_processing_consume_bytes(struct ringbuffer_t *self, int bytes)
{
	ringbuffer_processing_consume_samples (self, bytes >> self->cache_sample_shift);
}

void ringbuffer_processing_set_bytes(struct ringbuffer_t *self, int pos)
{
	ringbuffer_processing_set_samples (self, pos >> self->cache_sample_shift);
}

void ringbuffer_head_add_bytes(struct ringbuffer_t *self, int bytes)
{
	ringbuffer_head_add_samples (self, bytes >> self->cache_sample_shift);
}

void ringbuffer_head_set_bytes(struct ringbuffer_t *self, int pos)
{
	ringbuffer_head_set_samples (self, pos>>self->cache_sample_shift);
}

void ringbuffer_get_tail_bytes (struct ringbuffer_t *self, int *pos1, int *length1, int *pos2, int *length2)
{
	ringbuffer_get_tail_samples (self, pos1, length1, pos2, length2);
	if ((*length1 <<= self->cache_sample_shift))
	{
		*pos1 <<= self->cache_sample_shift;
	}

	if ((*length2 <<= self->cache_sample_shift))
	{
		*pos2 <<= self->cache_sample_shift;
	}
}

void ringbuffer_get_processing_bytes (struct ringbuffer_t *self, int *pos1, int *length1, int *pos2, int *length2)
{
	ringbuffer_get_processing_samples (self, pos1, length1, pos2, length2);
	if ((*length1 <<= self->cache_sample_shift))
	{
		*pos1 <<= self->cache_sample_shift;
	}

	if ((*length2 <<= self->cache_sample_shift))
	{
		*pos2 <<= self->cache_sample_shift;
	}
}

void ringbuffer_get_head_bytes (struct ringbuffer_t *self, int *pos1, int *length1, int *pos2, int *length2)
{
	ringbuffer_get_head_samples (self, pos1, length1, pos2, length2);
	if ((*length1 <<= self->cache_sample_shift))
	{
		*pos1 <<= self->cache_sample_shift;
	}

	if ((*length2 <<= self->cache_sample_shift))
	{
		*pos2 <<= self->cache_sample_shift;
	}
}

int ringbuffer_get_tail_available_bytes (struct ringbuffer_t *self)
{
	return ringbuffer_get_tail_available_samples (self) << self->cache_sample_shift;
}

int ringbuffer_get_processing_available_bytes (struct ringbuffer_t *self)
{
	return ringbuffer_get_processing_available_samples (self) << self->cache_sample_shift;
}

int ringbuffer_get_head_available_bytes (struct ringbuffer_t *self)
{
	return ringbuffer_get_head_available_samples (self) << self->cache_sample_shift;
}

/* samples = 0, the callback should happen when the next added samples passes tail
 * samples = 1, the callback should happen when the last added samples passes tail
 * samples = 10, the callback should happen when the 10th last added samples passes tail
 */
void ringbuffer_add_tail_callback_samples (struct ringbuffer_t *self, int samples, void (*callback)(void *arg, int samples_ago), const void *arg)
{
	int insertat, i;
	if (samples < 0)
	{
		samples = 0;
	} else if (samples > (self->cache_read_available + self->cache_processing_available))
	{
		samples = self->cache_read_available + self->cache_processing_available;
	}
	samples = self->cache_read_available + self->cache_processing_available - samples;;
	if (self->tail_callbacks_size == self->tail_callbacks_fill)
	{
		self->tail_callbacks = realloc (self->tail_callbacks, (self->tail_callbacks_size+=10) * sizeof (self->tail_callbacks[0]));
	}

	insertat = self->tail_callbacks_fill;
	for (i=0; i < self->tail_callbacks_fill; i++)
	{
		if (self->tail_callbacks[i].samples >= samples)
		{
			insertat = i;
			break;
		}
	}

	memmove (self->tail_callbacks+insertat+1, self->tail_callbacks+insertat, (self->tail_callbacks_fill - insertat) * sizeof (self->tail_callbacks[0]));
	self->tail_callbacks[insertat].callback = callback;
	self->tail_callbacks[insertat].arg = (void *)arg;
	self->tail_callbacks[insertat].samples = samples;
	self->tail_callbacks_fill++;
}

void ringbuffer_add_processing_callback_samples (struct ringbuffer_t *self, int samples, void (*callback)(void *arg, int samples_ago), const void *arg)
{
	int insertat, i;

	if (!(self->flags & RINGBUFFER_FLAGS_PROCESS))
	{
		fprintf (stderr, "ringbuffer_add_processing_callback_samples() called for a buffer that does not have RINGBUFFER_FLAGS_PROCESS\n");
		return;
	}

	if (samples < 0)
	{
		samples = 0;
	} else if (samples > (self->cache_read_available))
	{
		samples = self->cache_read_available;
	}
	samples = self->cache_read_available - samples;
	if (self->processing_callbacks_size == self->processing_callbacks_fill)
	{
		self->processing_callbacks = realloc (self->processing_callbacks, (self->processing_callbacks_size+=10) * sizeof (self->processing_callbacks[0]));
	}

	insertat = self->processing_callbacks_fill;
	for (i=0; i < self->processing_callbacks_fill; i++)
	{
		if (self->processing_callbacks[i].samples >= samples)
		{
			insertat = i;
			break;
		}
	}

	memmove (self->processing_callbacks+insertat+1, self->processing_callbacks+insertat, (self->processing_callbacks_fill - insertat) * sizeof (self->processing_callbacks[0]));
	self->processing_callbacks[insertat].callback = callback;
	self->processing_callbacks[insertat].arg = (void *)arg;
	self->processing_callbacks[insertat].samples = samples;
	self->processing_callbacks_fill++;
}


/* UNIT TEST */
#ifdef UNIT_TEST
int ringbuffer_result_nonproc_samples(struct ringbuffer_t *instance, int tail1, int readsize1, int tail2, int readsize2, int head1, int writesize1, int head2, int writesize2)
{
	int i;
	int retval = 0;
	int temp1, temp2, temp3;
	int l1, l2;

	putchar('[');
	for (i=0; i < instance->buffersize; i++)
	{
		putchar('+');
	}
	putchar(']');
	putchar('\n');

	putchar(' ');
	for (i=0; i < instance->head; i++)
	{
		putchar(' ');
	}
	putchar('H');
	putchar('\n');

	putchar(' ');
	for (i=0; i < instance->tail; i++)
	{
		putchar(' ');
	}
	putchar('T');
	putchar('\n');

/*
	if (head != instance->head)
	{
		printf ("expected tail: %d, got %d\n", tail, instance->tail);
		retval++;
	}

	if (tail != instance->tail)
	{
		printf ("expected tail: %d, got %d\n", tail, instance->tail);
		retval++;
	}
*/
	temp1 = ringbuffer_get_tail_available_samples (instance);
	temp3 = ringbuffer_get_head_available_samples (instance);

	if ((readsize1+readsize2) != temp1)
	{
		printf ("expected tail read size: %d, got %d\n", readsize1 + readsize2, temp1);
		retval++;
	}

	if ((writesize1+writesize2) != temp3)
	{
		printf ("expected head write size: %d, got %d\n", writesize1 + writesize2, temp3);
		retval++;
	}

	ringbuffer_get_tail_samples (instance, &temp1, &l1, &temp2, &l2);
	if (temp1 != tail1)
	{
		printf ("expected tail buffer1 pos %d, got %d\n", tail1, temp1);
		retval++;
	}
	if (l1 != readsize1)
	{
		printf ("expected tail buffer1 length %d, got %d\n", readsize1, l1);
		retval++;
	}
	if (temp2 != tail2)
	{
		printf ("expected tail buffer2 pos %d, got %d\n", tail2, temp2);
		retval++;
	}
	if (l2 != readsize2)
	{
		printf ("expected tail buffer2 length %d, got %d\n", readsize2, l2);
		retval++;
	}

	ringbuffer_get_head_samples (instance, &temp1, &l1, &temp2, &l2);
	if (temp1 != head1)
	{
		printf ("expected head buffer1 pos %d, got %d\n", head1, temp1);
		retval++;
	}
	if (l1 != writesize1)
	{
		printf ("expected head buffer1 length %d, got %d\n", writesize1, l1);
		retval++;
	}
	if (temp2 != head2)
	{
		printf ("expected head buffer2 pos %d, got %d\n", head2, temp2);
		retval++;
	}
	if (l2 != writesize2)
	{
		printf ("expected head buffer2 length %d, got %d\n", writesize2, l2);
		retval++;
	}
	return retval;
}

int ringbuffer_result_proc_samples(struct ringbuffer_t *instance, int tail1, int readsize1, int tail2, int readsize2, int proc1, int procsize1, int proc2, int procsize2, int head1, int writesize1, int head2, int writesize2)
{
	int i;
	int retval = 0;
	int temp1, temp2, temp3;
	int l1, l2;

	putchar('[');
	for (i=0; i < instance->buffersize; i++)
	{
		putchar('+');
	}
	putchar(']');
	putchar('\n');

	putchar(' ');
	for (i=0; i < instance->head; i++)
	{
		putchar(' ');
	}
	putchar('H');
	putchar('\n');

	putchar(' ');
	for (i=0; i < instance->processing; i++)
	{
		putchar(' ');
	}
	putchar('P');
	putchar('\n');

	putchar(' ');
	for (i=0; i < instance->tail; i++)
	{
		putchar(' ');
	}
	putchar('T');
	putchar('\n');

/*
	if (head != instance->head)
	{
		printf ("expected tail: %d, got %d\n", tail, instance->tail);
		retval++;
	}

	if (tail != instance->tail)
	{
		printf ("expected tail: %d, got %d\n", tail, instance->tail);
		retval++;
	}
*/
	temp1 = ringbuffer_get_tail_available_samples (instance);
	temp2 = ringbuffer_get_processing_available_samples (instance);
	temp3 = ringbuffer_get_head_available_samples (instance);

	if ((readsize1+readsize2) != temp1)
	{
		printf ("expected tail read size: %d, got %d\n", readsize1 + readsize2, temp1);
		retval++;
	}

	if ((procsize1+procsize2) != temp2)
	{
		printf ("expected processing size: %d, got %d\n", procsize1 + procsize2, temp2);
		retval++;
	}

	if ((writesize1+writesize2) != temp3)
	{
		printf ("expected head write size: %d, got %d\n", writesize1 + writesize2, temp3);
		retval++;
	}

	ringbuffer_get_tail_samples (instance, &temp1, &l1, &temp2, &l2);
	if (temp1 != tail1)
	{
		printf ("expected tail buffer1 pos %d, got %d\n", tail1, temp1);
		retval++;
	}
	if (l1 != readsize1)
	{
		printf ("expected tail buffer1 length %d, got %d\n", readsize1, l1);
		retval++;
	}
	if (temp2 != tail2)
	{
		printf ("expected tail buffer2 pos %d, got %d\n", tail2, temp2);
		retval++;
	}
	if (l2 != readsize2)
	{
		printf ("expected tail buffer2 length %d, got %d\n", readsize2, l2);
		retval++;
	}

	ringbuffer_get_processing_samples (instance, &temp1, &l1, &temp2, &l2);
	if (temp1 != proc1)
	{
		printf ("expected proc buffer1 pos %d, got %d\n", proc1, temp1);
		retval++;
	}
	if (l1 != procsize1)
	{
		printf ("expected proc buffer1 length %d, got %d\n", procsize1, l1);
		retval++;
	}
	if (temp2 != proc2)
	{
		printf ("expected proc buffer2 pos %d, got %d\n", proc2, temp2);
		retval++;
	}
	if (l2 != procsize2)
	{
		printf ("expected proc buffer2 length %d, got %d\n", procsize2, l2);
		retval++;
	}

	ringbuffer_get_head_samples (instance, &temp1, &l1, &temp2, &l2);
	if (temp1 != head1)
	{
		printf ("expected head buffer1 pos %d, got %d\n", head1, temp1);
		retval++;
	}
	if (l1 != writesize1)
	{
		printf ("expected head buffer1 length %d, got %d\n", writesize1, l1);
		retval++;
	}
	if (temp2 != head2)
	{
		printf ("expected head buffer2 pos %d, got %d\n", head2, temp2);
		retval++;
	}
	if (l2 != writesize2)
	{
		printf ("expected head buffer2 length %d, got %d\n", writesize2, l2);
		retval++;
	}
	return retval;
}

static int ringbuffer_processing_expect_callback = 0;
static int ringbuffer_processing_expect_id = 0;
static int callback_processing_errors = 0;
static void processing_callback(void *arg, int samples_ago)
{
	printf (" processing_callback %d\n", *(int *)arg);
	if (ringbuffer_processing_expect_callback < 0)
	{
		printf ("proc_callback, too many...\n");
		callback_processing_errors++;
	}
	ringbuffer_processing_expect_callback--;
	if (ringbuffer_processing_expect_id != *(int *)arg)
	{
		printf ("proc_callback, wrong id. Expected %d, got %d\n", ringbuffer_processing_expect_id, *(int *)arg);
		callback_processing_errors++;
	}
}

static int ringbuffer_tail_expect_callback = 0;
static int ringbuffer_tail_expect_id = 0;
static int callback_tail_errors = 0;
static void tail_callback(void *arg, int samples_ago)
{
	printf (" tail_callback %d\n", *(int *)arg);
	if (ringbuffer_tail_expect_callback < 0)
	{
		printf ("proc_callback, too many...\n");
		callback_tail_errors++;
	}
	ringbuffer_tail_expect_callback--;
	if (ringbuffer_tail_expect_id != *(int *)arg)
	{
		printf ("proc_callback, wrong id. Expected %d, got %d\n", ringbuffer_tail_expect_id, *(int *)arg);
		callback_tail_errors++;
	}
}

const int int_0 = 0;
const int int_1 = 1;
const int int_2 = 2;
const int int_3 = 3;
const int int_4 = 4;
const int int_5 = 5;
const int int_6 = 6;
const int int_7 = 7;
const int int_8 = 8;
const int int_9 = 9;
const int int_10 = 10;
const int int_11 = 11;
const int int_12 = 12;
const int int_13 = 13;
const int int_14 = 14;
const int int_15 = 15;

int main(int argc, char *argv[])
{
	int retval = 0;
	struct ringbuffer_t *instance;

	int testval;
	int i;

	printf ("BUFFERSIZE 8BIT MONO\n");
	testval = 0;
	instance = ringbuffer_new_samples(RINGBUFFER_FLAGS_8BIT, 16);
	if (instance->buffersize != 16)
	{
		printf ("buffersize %d, expected 16\n", instance->buffersize); testval = 1;
	}
	if (instance->cache_sample_shift != 0)
	{
		printf ("cache_sample_shift %d, expected 0\n", instance->cache_sample_shift);
	}
	ringbuffer_free (instance);
	retval |= testval;
	printf ("\n");

	printf ("BUFFERSIZE 16BIT MONO\n");
	testval = 0;
	instance = ringbuffer_new_samples(RINGBUFFER_FLAGS_16BIT, 16);
	if (instance->buffersize != 16)
	{
		printf ("buffersize %d, expected 16\n", instance->buffersize); testval = 1;
	}
	if (instance->cache_sample_shift != 1)
	{
		printf ("cache_sample_shift %d, expected 1\n", instance->cache_sample_shift);
	}
	ringbuffer_free (instance);
	retval |= testval;
	printf ("\n");

	printf ("BUFFERSIZE FLOAT MONO\n");
	testval = 0;
	instance = ringbuffer_new_samples(RINGBUFFER_FLAGS_FLOAT, 16);
	if (instance->buffersize != 16)
	{
		printf ("buffersize %d, expected 16\n", instance->buffersize); testval = 1;
	}
	if (instance->cache_sample_shift != 2)
	{
		printf ("cache_sample_shift %d, expected 2\n", instance->cache_sample_shift);
	}
	ringbuffer_free (instance);
	retval |= testval;
	printf ("\n");

	printf ("BUFFERSIZE 8BIT STEREO\n");
	testval = 0;
	instance = ringbuffer_new_samples(RINGBUFFER_FLAGS_8BIT | RINGBUFFER_FLAGS_STEREO, 16);
	if (instance->buffersize != 16)
	{
		printf ("buffersize %d, expected 16\n", instance->buffersize); testval = 1;
	}
	if (instance->cache_sample_shift != 1)
	{
		printf ("cache_sample_shift %d, expected 1\n", instance->cache_sample_shift);
	}
	ringbuffer_free (instance);
	retval |= testval;
	printf ("\n");

	printf ("BUFFERSIZE 16BIT STEREO\n");
	testval = 0;
	instance = ringbuffer_new_samples(RINGBUFFER_FLAGS_16BIT | RINGBUFFER_FLAGS_STEREO, 16);
	if (instance->buffersize != 16)
	{
		printf ("buffersize %d, expected 16\n", instance->buffersize); testval = 1;
	}
	if (instance->cache_sample_shift != 2)
	{
		printf ("cache_sample_shift %d, expected 2\n", instance->cache_sample_shift);
	}
	ringbuffer_free (instance);
	retval |= testval;
	printf ("\n");

	printf ("BUFFERSIZE FLOAT STEREO\n");
	testval = 0;
	instance = ringbuffer_new_samples(RINGBUFFER_FLAGS_FLOAT | RINGBUFFER_FLAGS_STEREO, 16);
	if (instance->buffersize != 16)
	{
		printf ("buffersize %d, expected 16\n", instance->buffersize); testval = 1;
	}
	if (instance->cache_sample_shift != 3)
	{
		printf ("cache_sample_shift %d, expected 3\n", instance->cache_sample_shift);
	}
	ringbuffer_free (instance);
	retval |= testval;
	printf ("\n");

	printf ("NO PROCESSING, SAMPLES (progressive)\n");
	testval = 0;
	instance = ringbuffer_new_samples(RINGBUFFER_FLAGS_FLOAT | RINGBUFFER_FLAGS_STEREO, 16);
	testval += ringbuffer_result_nonproc_samples (instance, -1,  0, -1,  0,  /* tail */
	                                                         0, 15, -1,  0); /* head */
	/* add 1 sample, into buffer */
	printf ("Adding 1 sample\n");
	ringbuffer_head_add_samples(instance, 1);
	testval += ringbuffer_result_nonproc_samples (instance,  0,  1, -1,  0,  /* tail */
	                                                         1, 14, -1,  0); /* head */
	/* add 14 sample, into buffer */
	printf ("Adding 14 samples\n");
	ringbuffer_head_add_samples(instance, 14);
	testval += ringbuffer_result_nonproc_samples (instance,  0, 15, -1,  0,  /* tail */
	                                                        -1,  0, -1,  0); /* head */
	/* Remove 10 samples */
	printf ("Consuming 10 samples\n");
	ringbuffer_tail_consume_samples(instance, 10);
	testval += ringbuffer_result_nonproc_samples (instance, 10,  5, -1,  0,  /* tail */
	                                                        15,  1,  0,  9); /* head */
	/* add 10 sample, into buffer */
	printf ("Adding 10 samples (head mid-buffer wraps)\n");
	ringbuffer_head_add_samples(instance, 10);
	testval += ringbuffer_result_nonproc_samples (instance, 10,  6,  0,  9,  /* tail */
	                                                        -1,  0, -1,  0); /* head */
	/* Remove 15 samples */
	printf ("Consuming 15 samples (remove all, with mid-wrap)\n");
	ringbuffer_tail_consume_samples(instance, 15);
	testval += ringbuffer_result_nonproc_samples (instance, -1,  0, -1,  0,  /* tail */
	                                                         9,  7,  0,  8); /* head */
	/* add 7 samples, into buffer */
	printf ("Adding 7 samples (head lands on just after wrap)\n");
	ringbuffer_head_add_samples(instance, 7);
	testval += ringbuffer_result_nonproc_samples (instance,  9,  7, -1,  0,  /* tail */
	                                                         0,  8, -1,  0); /* head */
	/* Remove 7 samples */
	printf ("Consuming 7 samples (remove all, tail lands just after wrap)\n");
	ringbuffer_tail_consume_samples(instance, 7);
	testval += ringbuffer_result_nonproc_samples (instance, -1,  0, -1,  0,  /* tail */
	                                                         0, 15, -1,  0); /* head */
	ringbuffer_free (instance);
	retval += testval;

	printf ("NO PROCESSING, SAMPLES (set)\n");
	testval = 0;
	instance = ringbuffer_new_samples(RINGBUFFER_FLAGS_FLOAT | RINGBUFFER_FLAGS_STEREO, 16);
	testval += ringbuffer_result_nonproc_samples (instance, -1,  0, -1,  0,  /* tail */
	                                                         0, 15, -1,  0); /* head */
	/* add 1 sample, into buffer */
	printf ("Adding 1 sample\n");
	ringbuffer_head_set_samples(instance, 1);
	testval += ringbuffer_result_nonproc_samples (instance,  0,  1, -1,  0,  /* tail */
	                                                         1, 14, -1,  0); /* head */
	/* add 14 sample, into buffer */
	printf ("Adding 14 samples\n");
	ringbuffer_head_set_samples(instance, 15);
	testval += ringbuffer_result_nonproc_samples (instance,  0, 15, -1,  0,  /* tail */
	                                                        -1,  0, -1,  0); /* head */
	/* Remove 10 samples */
	printf ("Consuming 10 samples\n");
	ringbuffer_tail_set_samples(instance, 10);
	testval += ringbuffer_result_nonproc_samples (instance, 10,  5, -1,  0,  /* tail */
	                                                        15,  1,  0,  9); /* head */
	/* add 10 sample, into buffer */
	printf ("Adding 10 samples (head mid-buffer wraps)\n");
	ringbuffer_head_set_samples(instance, 9);
	testval += ringbuffer_result_nonproc_samples (instance, 10,  6,  0,  9,  /* tail */
	                                                        -1,  0, -1,  0); /* head */
	/* Remove 15 samples */
	printf ("Consuming 15 samples (remove all, with mid-wrap)\n");
	ringbuffer_tail_set_samples(instance, 9);
	testval += ringbuffer_result_nonproc_samples (instance, -1,  0, -1,  0,  /* tail */
	                                                         9,  7,  0,  8); /* head */
	/* add 7 samples, into buffer */
	printf ("Adding 7 samples (head lands on just after wrap)\n");
	ringbuffer_head_set_samples(instance, 0);
	testval += ringbuffer_result_nonproc_samples (instance,  9,  7, -1,  0,  /* tail */
	                                                         0,  8, -1,  0); /* head */
	/* Remove 7 samples */
	printf ("Consuming 7 samples (remove all, tail lands just after wrap)\n");
	ringbuffer_tail_set_samples(instance, 0);
	testval += ringbuffer_result_nonproc_samples (instance, -1,  0, -1,  0,  /* tail */
	                                                         0, 15, -1,  0); /* head */
	ringbuffer_free (instance);
	retval += testval;

	printf ("PROCESSING, SAMPLES (progressive)\n");
	testval = 0;
	instance = ringbuffer_new_samples(RINGBUFFER_FLAGS_FLOAT | RINGBUFFER_FLAGS_STEREO | RINGBUFFER_FLAGS_PROCESS, 16);
	testval += ringbuffer_result_proc_samples (instance, -1,  0, -1,  0,  /* tail */
	                                                     -1,  0, -1,  0,  /* proc */
	                                                      0, 15, -1,  0); /* head */
	/* add 1 sample, into buffer */
	printf ("Adding 1 sample\n");
	ringbuffer_head_add_samples(instance, 1);
	testval += ringbuffer_result_proc_samples (instance, -1,  0, -1,  0,  /* tail */
                                                              0,  1, -1,  0,  /* proc */
	                                                      1, 14, -1,  0); /* head */
	/* add 14 sample, into buffer */
	printf ("Adding 14 samples\n");
	ringbuffer_head_add_samples(instance, 14);
	testval += ringbuffer_result_proc_samples (instance, -1,  0, -1,  0,  /* tail */
                                                              0, 15, -1,  0,  /* proc */
	                                                     -1,  0, -1,  0); /* head */
	/* Process 14 samples */
	printf ("Process 14 samples\n");
	ringbuffer_processing_consume_samples(instance, 14);
	testval += ringbuffer_result_proc_samples (instance,  0, 14, -1,  0,  /* tail */
	                                                     14,  1, -1,  0,  /* proc */
	                                                     -1,  0, -1,  0); /* head */
	/* Remove 10 samples */
	printf ("Consuming 10 samples\n");
	ringbuffer_tail_consume_samples(instance, 10);
	testval += ringbuffer_result_proc_samples (instance, 10,  4, -1,  0,  /* tail */
	                                                     14,  1, -1,  0,  /* proc */
	                                                     15,  1,  0,  9); /* head */
	/* add 10 sample, into buffer */
	printf ("Adding 10 samples (head mid-buffer wraps)\n");
	ringbuffer_head_add_samples(instance, 10);
	testval += ringbuffer_result_proc_samples (instance, 10,  4, -1,  0,  /* tail */
	                                                     14,  2,  0,  9,  /* proc */
	                                                     -1,  0, -1,  0); /* head */
	/* Process 11 samples */
	printf ("Process 11 samples (head mid-buffer wraps)\n");
	ringbuffer_processing_consume_samples(instance, 11);
	testval += ringbuffer_result_proc_samples (instance, 10,  6,  0,  9,  /* tail */
	                                                     -1,  0, -1,  0,  /* proc */
	                                                     -1,  0, -1,  0); /* head */
	/* Remove 15 samples */
	printf ("Consuming 15 samples (remove all, with mid-wrap)\n");
	ringbuffer_tail_consume_samples(instance, 15);
	testval += ringbuffer_result_proc_samples (instance, -1,  0, -1,  0,  /* tail */
	                                                     -1,  0, -1,  0,  /* proc */
	                                                      9,  7,  0,  8); /* head */
	/* add 7 samples, into buffer */
	printf ("Adding 7 samples (head lands on just after wrap)\n");
	ringbuffer_head_add_samples(instance, 7);
	testval += ringbuffer_result_proc_samples (instance, -1,  0, -1,  0, /* tail */
	                                                      9,  7, -1,  0,  /* tail */
	                                                      0,  8, -1,  0); /* head */
	/* Process 7 samples */
	printf ("Process 7 samples (head lands on just after wrap)\n");
	ringbuffer_processing_consume_samples(instance, 7);
	testval += ringbuffer_result_proc_samples (instance,  9,  7, -1,  0,  /* tail */
	                                                     -1,  0, -1,  0,  /* proc */
	                                                      0,  8, -1,  0); /* head */
	/* Remove 7 samples */
	printf ("Consuming 7 samples (remove all, tail lands just after wrap)\n");
	ringbuffer_tail_consume_samples(instance, 7);
	testval += ringbuffer_result_proc_samples (instance, -1,  0, -1,  0,  /* tail */
	                                                     -1,  0, -1,  0,  /* proc */
	                                                      0, 15, -1,  0); /* head */
	ringbuffer_free (instance);
	retval += testval;

	printf ("PROCESSING, SAMPLES (set)\n");
	testval = 0;
	instance = ringbuffer_new_samples(RINGBUFFER_FLAGS_FLOAT | RINGBUFFER_FLAGS_STEREO | RINGBUFFER_FLAGS_PROCESS, 16);
	testval += ringbuffer_result_proc_samples (instance, -1,  0, -1,  0,  /* tail */
	                                                     -1,  0, -1,  0,  /* proc */
	                                                      0, 15, -1,  0); /* head */
	/* add 1 sample, into buffer */
	printf ("Adding 1 sample\n");
	ringbuffer_head_set_samples(instance, 1);
	testval += ringbuffer_result_proc_samples (instance, -1,  0, -1,  0,  /* tail */
                                                              0,  1, -1,  0,  /* proc */
	                                                      1, 14, -1,  0); /* head */
	/* add 14 sample, into buffer */
	printf ("Adding 14 samples\n");
	ringbuffer_head_set_samples(instance, 15);
	testval += ringbuffer_result_proc_samples (instance, -1,  0, -1,  0,  /* tail */
                                                              0, 15, -1,  0,  /* proc */
	                                                     -1,  0, -1,  0); /* head */
	/* Process 14 samples */
	printf ("Process 14 samples\n");
	ringbuffer_processing_set_samples(instance, 14);
	testval += ringbuffer_result_proc_samples (instance,  0, 14, -1,  0,  /* tail */
	                                                     14,  1, -1,  0,  /* proc */
	                                                     -1,  0, -1,  0); /* head */
	/* Remove 10 samples */
	printf ("Consuming 10 samples\n");
	ringbuffer_tail_set_samples(instance, 10);
	testval += ringbuffer_result_proc_samples (instance, 10,  4, -1,  0,  /* tail */
	                                                     14,  1, -1,  0,  /* proc */
	                                                     15,  1,  0,  9); /* head */
	/* add 10 sample, into buffer */
	printf ("Adding 10 samples (head mid-buffer wraps)\n");
	ringbuffer_head_set_samples(instance, 9);
	testval += ringbuffer_result_proc_samples (instance, 10,  4, -1,  0,  /* tail */
	                                                     14,  2,  0,  9,  /* proc */
	                                                     -1,  0, -1,  0); /* head */
	/* Process 11 samples */
	printf ("Process 11 samples (head mid-buffer wraps)\n");
	ringbuffer_processing_set_samples(instance, 9);
	testval += ringbuffer_result_proc_samples (instance, 10,  6,  0,  9,  /* tail */
	                                                     -1,  0, -1,  0,  /* proc */
	                                                     -1,  0, -1,  0); /* head */
	/* Remove 15 samples */
	printf ("Consuming 15 samples (remove all, with mid-wrap)\n");
	ringbuffer_tail_set_samples(instance, 9);
	testval += ringbuffer_result_proc_samples (instance, -1,  0, -1,  0,  /* tail */
	                                                     -1,  0, -1,  0,  /* proc */
	                                                      9,  7,  0,  8); /* head */
	/* add 7 samples, into buffer */
	printf ("Adding 7 samples (head lands on just after wrap)\n");
	ringbuffer_head_set_samples(instance, 0);
	testval += ringbuffer_result_proc_samples (instance, -1,  0, -1,  0, /* tail */
	                                                      9,  7, -1,  0,  /* tail */
	                                                      0,  8, -1,  0); /* head */
	/* Process 7 samples */
	printf ("Process 7 samples (head lands on just after wrap)\n");
	ringbuffer_processing_set_samples(instance, 0);
	testval += ringbuffer_result_proc_samples (instance,  9,  7, -1,  0,  /* tail */
	                                                     -1,  0, -1,  0,  /* proc */
	                                                      0,  8, -1,  0); /* head */
	/* Remove 7 samples */
	printf ("Consuming 7 samples (remove all, tail lands just after wrap)\n");
	ringbuffer_tail_set_samples(instance, 0);
	testval += ringbuffer_result_proc_samples (instance, -1,  0, -1,  0,  /* tail */
	                                                     -1,  0, -1,  0,  /* proc */
	                                                      0, 15, -1,  0); /* head */
	ringbuffer_free (instance);
	retval += testval;

	printf ("Going to test callbacks\n");
	instance = ringbuffer_new_samples(RINGBUFFER_FLAGS_FLOAT | RINGBUFFER_FLAGS_STEREO | RINGBUFFER_FLAGS_PROCESS, 16);
	ringbuffer_head_add_samples (instance, 12);
	ringbuffer_processing_consume_samples (instance, 7);
	ringbuffer_tail_consume_samples(instance, 2);

	ringbuffer_add_processing_callback_samples (instance, 2, processing_callback, &int_10);
	ringbuffer_add_processing_callback_samples (instance, 1, processing_callback, &int_11);
	/* this will be one sample into the future, we allow zero, but not negative numbers per today */
	ringbuffer_add_processing_callback_samples (instance, 0, processing_callback, &int_12); /* should not be called yet */
	ringbuffer_add_processing_callback_samples (instance, 3, processing_callback, &int_9);
	ringbuffer_add_processing_callback_samples (instance, 4, processing_callback, &int_8);
	ringbuffer_add_processing_callback_samples (instance, 5, processing_callback, &int_7);

	ringbuffer_add_tail_callback_samples (instance, 5, tail_callback, &int_7);
	ringbuffer_add_tail_callback_samples (instance, 6, tail_callback, &int_6);
	ringbuffer_add_tail_callback_samples (instance, 7, tail_callback, &int_5);
	ringbuffer_add_tail_callback_samples (instance, 8, tail_callback, &int_4);
	ringbuffer_add_tail_callback_samples (instance, 9, tail_callback, &int_3);
	ringbuffer_add_tail_callback_samples (instance, 10, tail_callback, &int_2);
	ringbuffer_add_tail_callback_samples (instance, 0, tail_callback, &int_12); /* should not be called yet */
	ringbuffer_add_tail_callback_samples (instance, 1, tail_callback, &int_11);
	ringbuffer_add_tail_callback_samples (instance, 2, tail_callback, &int_10);
	ringbuffer_add_tail_callback_samples (instance, 3, tail_callback, &int_9);
	ringbuffer_add_tail_callback_samples (instance, 4, tail_callback, &int_8);

	for (i=7; i<12; i++)
	{
		printf (" process sample %d\n", i);
		ringbuffer_processing_expect_callback = 1;
		ringbuffer_processing_expect_id = i;
		ringbuffer_processing_consume_samples (instance, 1);
		if (ringbuffer_processing_expect_callback)
		{
			printf ("Wrong amount callbacks for `processing`, sample id %d  of 12\n", i);
			retval++;
		}
	}

	for (i=2; i<12; i++)
	{
		printf(" tail sample %d\n", i);
		ringbuffer_tail_expect_callback = 1;
		ringbuffer_tail_expect_id = i;
		ringbuffer_tail_consume_samples(instance, 1);
		if (ringbuffer_processing_expect_callback)
		{
			printf ("Wrong amount callbacks for `tail`, sample id %d of 12\n", i);
			retval++;
		}
	}

	printf ("adding 5 more callbacks + one should linger from previous round\n");
	ringbuffer_head_add_samples (instance, 6);
	ringbuffer_add_tail_callback_samples (instance, 5, tail_callback, &int_13);
	ringbuffer_add_tail_callback_samples (instance, 4, tail_callback, &int_14);
	ringbuffer_add_tail_callback_samples (instance, 3, tail_callback, &int_15);
	ringbuffer_add_tail_callback_samples (instance, 2, tail_callback, &int_0);
	ringbuffer_add_tail_callback_samples (instance, 1, tail_callback, &int_1);
	{
		ringbuffer_processing_expect_callback = 1;
		ringbuffer_processing_expect_id = 12;
		ringbuffer_processing_consume_samples (instance, 6);
	}

	for (i=12; i<18; i++)
	{
		printf(" tail sample %d\n", i%16);
		ringbuffer_tail_expect_callback = 1;
		ringbuffer_tail_expect_id = i%16;
		ringbuffer_tail_consume_samples(instance, 1);
		if (ringbuffer_processing_expect_callback)
		{
			printf ("Wrong amount callbacks for `tail`, sample id %d of 12\n", i);
			retval++;
		}
	}

	ringbuffer_free (instance);
	retval += callback_processing_errors;
	retval += callback_tail_errors;

	printf ("\nFinal result: %d errors\n", retval);
	return retval;
}
#endif
