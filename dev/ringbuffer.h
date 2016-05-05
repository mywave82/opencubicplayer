#ifndef _RINGBUFFER_H
#define _RINGBUFFER_H 1

#define RINGBUFFER_FLAGS_STEREO 1

/* these are mutual exclusive */
#define RINGBUFFER_FLAGS_8BIT   2 
#define RINGBUFFER_FLAGS_16BIT  4
#define RINGBUFFER_FLAGS_FLOAT  8

#define RINGBUFFER_FLAGS_SIGNED 16 /* valid for 8BIT and 16BIT */

#define RINGBUFFER_FLAGS_PROCESS 32 /* if present, processing and cache_process will be maintained */

struct ringbuffer_t;

void ringbuffer_reset (struct ringbuffer_t *self);

void ringbuffer_tail_consume_bytes(struct ringbuffer_t *self, int bytes);
void ringbuffer_tail_set_bytes(struct ringbuffer_t *self, int pos);

void ringbuffer_processing_consume_bytes(struct ringbuffer_t *self, int bytes);
void ringbuffer_processing_set_bytes(struct ringbuffer_t *self, int pos);

void ringbuffer_head_add_bytes(struct ringbuffer_t *self, int bytes);
void ringbuffer_head_set_bytes(struct ringbuffer_t *self, int pos);

void ringbuffer_tail_consume_samples(struct ringbuffer_t *self, int samples);
void ringbuffer_tail_set_samples(struct ringbuffer_t *self, int pos);

void ringbuffer_processing_consume_samples(struct ringbuffer_t *self, int samples);
void ringbuffer_processing_set_samples(struct ringbuffer_t *self, int pos);

void ringbuffer_head_add_samples(struct ringbuffer_t *self, int samples);
void ringbuffer_head_set_samples(struct ringbuffer_t *self, int pos);

void ringbuffer_get_tail_bytes (struct ringbuffer_t *self, int *pos1, int *length1, int *pos2, int *length2);
void ringbuffer_get_processing_bytes (struct ringbuffer_t *self, int *pos1, int *length1, int *pos2, int *length2);
void ringbuffer_get_head_bytes (struct ringbuffer_t *self, int *pos1, int *length1, int *pos2, int *length2);

void ringbuffer_get_tail_samples (struct ringbuffer_t *self, int *pos1, int *length1, int *pos2, int *length2);
void ringbuffer_get_processing_samples (struct ringbuffer_t *self, int *pos1, int *length1, int *pos2, int *length2);
void ringbuffer_get_head_samples (struct ringbuffer_t *self, int *pos1, int *length1, int *pos2, int *length2);

int ringbuffer_get_tail_available_bytes (struct ringbuffer_t *self);
int ringbuffer_get_processing_available_bytes (struct ringbuffer_t *self);
int ringbuffer_get_head_available_bytes (struct ringbuffer_t *self);

int ringbuffer_get_tail_available_samples (struct ringbuffer_t *self);
int ringbuffer_get_processing_available_samples (struct ringbuffer_t *self);
int ringbuffer_get_head_available_samples (struct ringbuffer_t *self);

struct ringbuffer_t *ringbuffer_new_samples(int flags, int buffersize_samples); /* some users might non-pow(2,x) size buffers */
void ringbuffer_free(struct ringbuffer_t *self);

void ringbuffer_static_initialize (struct ringbuffer_t *self, int flags, int ringbuffer_shift_samples);

#endif
