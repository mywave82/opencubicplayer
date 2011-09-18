#ifndef _PAT_H
#define _PAT_H

struct __attribute__((packed)) PATCHHEADER
{
	char header[12]; /* "GF1PATCH110" */
	char gravis_id[10];   /* "ID#000002" */
	char description[60];
	uint8_t instruments; /* 0 is bogus for 1 */
	uint8_t voices; /* should be 14*/
	uint8_t channels;
	uint16_t wave_forms;
	uint16_t master_volume;
	uint32_t data_size;
	char reserved[36];
};
static inline void PATCHHEADER_endian(struct PATCHHEADER *a) {
	a->wave_forms    = uint16_little (a->wave_forms);
	a->master_volume = uint16_little (a->master_volume);
	a->data_size     = uint32_little (a->data_size);
}

struct __attribute__((packed)) INSTRUMENTDATA
{
	uint16_t instrument;
	char instrument_name[16];
	uint32_t instrument_size;
	uint8_t layers;
	char reserved[40];
};
static inline void INSTRUMENTDATA_endian(struct INSTRUMENTDATA *a) {
	a->instrument      = uint16_little (a->instrument);
	a->instrument_size = uint32_little (a->instrument_size);
}

struct __attribute__((packed)) LAYERDATA
{
	uint8_t layer_duplicate;
	uint8_t layer;
	uint32_t layer_size;
	uint8_t samples;
	char reserved[40];
};
static inline void LAYERDATA_endian(struct LAYERDATA *a) {
	a->layer_size = uint32_little (a->layer_size);
}

struct __attribute__((packed)) PATCHDATA
{
	char wave_name[7];

	uint8_t fractions;
	uint32_t wave_size;
	uint32_t start_loop;
	uint32_t end_loop;

	uint16_t sample_rate;
	uint32_t low_frequency;
	uint32_t high_frequency;
	uint32_t root_frequency;
	uint16_t tune;

	uint8_t balance;

	uint8_t envelope_rate[6];
	uint8_t envelope_offset[6];

	uint8_t tremolo_sweep;
	uint8_t tremolo_rate;
	uint8_t tremolo_depth;

	uint8_t vibrato_sweep;
	uint8_t vibrato_rate;
	uint8_t vibrato_depth;

	/* bit 5 = Turn sustaining on. (Env. pts. 3)*/
	/* bit 6 = Enable envelopes - 1 */
	/* bit 7 = fast release */
	uint8_t modes;

	uint16_t scale_frequency;
	uint16_t scale_factor;    /* from 0 to 2048 or 0 to 2 */

	char reserved[36];
};
static inline void PATCHDATA_endian(struct PATCHDATA *a) {
	a->wave_size       = uint32_little (a->wave_size);
	a->start_loop      = uint32_little (a->start_loop);
	a->end_loop        = uint32_little (a->end_loop);
	a->sample_rate     = uint16_little (a->sample_rate);
	a->low_frequency   = uint32_little (a->low_frequency);
	a->high_frequency  = uint32_little (a->high_frequency);
	a->root_frequency  = uint32_little (a->root_frequency);
	a->tune            = uint16_little (a->tune);
	a->scale_frequency = uint16_little (a->scale_frequency);
	a->scale_factor    = uint16_little (a->scale_factor);
}

struct minstrument;
struct sampleinfo;

extern int __attribute__ ((visibility ("internal")))
loadpatchPAT( FILE               *file,
              struct minstrument *ins,
              uint8_t             program,
              uint8_t            *sampused,
              struct sampleinfo **smps,
              uint16_t           *samplenum);

extern int __attribute__ ((visibility ("internal")))
addpatchPAT ( FILE               *file,
              struct minstrument *ins,
              uint8_t             program,
              uint8_t             sn,
              uint8_t             sampnum,
              struct sampleinfo  *sip,
              uint16_t           *samplenum);

extern __attribute__ ((visibility ("internal"))) uint32_t pocttab[16];
extern __attribute__ ((visibility ("internal"))) uint16_t pnotetab[12];
extern __attribute__ ((visibility ("internal"))) uint16_t pfinetab[16];
extern __attribute__ ((visibility ("internal"))) uint16_t pxfinetab[16];

#endif
