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
#include "ringbuffer.h"

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
};

void ringbuffer_reset (struct ringbuffer_t *self)
{
	self->head = 0;
	self->processing = 0;
	self->tail = 0;

	self->cache_write_available = self->buffersize - 1;
	self->cache_read_available = 0;
	self->cache_processing_available = 0;
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

	ringbuffer_reset (self);
}


struct ringbuffer_t *ringbuffer_new_samples(int flags, int buffersize_samples)
{
	struct ringbuffer_t *self = malloc (sizeof (*self));

	ringbuffer_static_initialize (self, flags, buffersize_samples);

	return self;
}

void ringbuffer_free(struct ringbuffer_t *self)
{
	free (self);
}

void ringbuffer_tail_consume_samples(struct ringbuffer_t *self, int samples)
{
	assert (samples <= self->cache_read_available);
	self->tail = (self->tail + samples) % self->buffersize;

	self->cache_read_available -= samples;

	self->cache_write_available += samples;

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

/* UNIT TEST */
#if 0
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

int main(int argc, char *argv[])
{
	int retval = 0;
	struct ringbuffer_t *instance;

	int testval;

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

	return retval;
}
#endif
