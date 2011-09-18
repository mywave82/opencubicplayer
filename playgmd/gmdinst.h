#ifndef __GMDINST_H
#define __GMDINST_H

enum
{
	mpEnvLoop=1, mpEnvBiDi=2, mpEnvSLoop=4, mpEnvSBiDi=8
};

struct gmdenvelope
{
	uint8_t *env;
	uint16_t len;
	uint16_t loops, loope;
	uint16_t sloops, sloope;
	uint8_t type;
	uint8_t speed;
};

struct gmdsample
{
	char name[32];
	uint16_t handle;
	int16_t normnote;
	int16_t stdvol;
	int16_t stdpan;
	uint16_t opt;
#define MP_OFFSETDIV2 1
	uint16_t volfade;
	uint8_t pchint;
	uint16_t volenv;
	uint16_t panenv;
	uint16_t pchenv;
	uint8_t vibspeed;
	uint8_t vibtype;
	uint16_t vibrate;
	uint16_t vibdepth;
	uint16_t vibsweep;
};

struct gmdinstrument
{
	char name[32];
	unsigned short samples[128];
};

struct sampleinfo;

extern void __attribute__ ((visibility ("internal"))) gmdInstSetup(const struct gmdinstrument *ins, int nins, const struct gmdsample *smp, int nsmp, const struct sampleinfo *smpi, int nsmpi, int type, void (*MarkyBoy)(uint8_t *, uint8_t *));
extern void __attribute__ ((visibility ("internal"))) gmdInstClear(void);

#endif
