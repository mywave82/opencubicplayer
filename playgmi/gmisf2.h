#ifndef _GMISF2_H
#define _GMISF2_H

struct SF2_phdr
{
	uint8_t PresetName[20];
	uint16_t Preset;
	uint16_t Bank;
	uint16_t PresetBagIndex;
	uint32_t Library;
	uint32_t Genre;
	uint32_t Morphology;
};

struct SF2_pbag
{

	uint16_t GenIndex;
	uint16_t ModIndex;
};

struct SF2_pmod
{
	uint16_t SrcOper;
	uint16_t DestOper;
	int16_t  Amount;
	uint16_t AmountSrcOper;
	uint16_t TransOper;
};

struct SF2_pgen
{
	uint16_t GenOper;
	uint16_t Amount;
};

struct SF2_inst
{
	char InstName[21];
	uint16_t InstBagIndex;
};

struct SF2_ibag
{
	uint16_t InstGenIndex;
	uint16_t InstModIndex;
};

struct SF2_imod
{
	uint16_t SrcOper;
	uint16_t DestOper;
	int16_t  Amount;
	uint16_t AmountSrcOper;
	uint16_t TransOper;
};

struct SF2_igen
{
	uint16_t GenOper;
	uint16_t Amount;
};

struct SF2_shdr
{
	char SampleName[21];
	uint32_t Start;
	uint32_t End;
	uint32_t StartLoop;
	uint32_t EndLoop;
	uint32_t SampleRate;
	uint8_t  RootNote;
	int8_t   PitchCorrection; /* cents 100 = 1 half note */
	uint16_t SampleLink; /* ID */
	uint16_t SampleType;
};

#define DETECTED_sfbk 0x00000001
#define DETECTED_INFO 0x00000002
#define DETECTED_ifil 0x00000004
#define DETECTED_isng 0x00000008
#define DETECTED_INAM 0x00000010
#define DETECTED_irom 0x00000020
#define DETECTED_iver 0x00000040
#define DETECTED_ICRD 0x00000080
#define DETECTED_IENG 0x00000100
#define DETECTED_IPRD 0x00000200
#define DETECTED_ICOP 0x00000400
#define DETECTED_ICMT 0x00000800
#define DETECTED_ISFT 0x00001000
#define DETECTED_sdta 0x00002000
#define DETECTED_smpl 0x00004000
#define DETECTED_sm24 0x00008000
#define DETECTED_pdta 0x00010000
#define DETECTED_phdr 0x00020000
#define DETECTED_pbag 0x00040000
#define DETECTED_pmod 0x00080000
#define DETECTED_pgen 0x00100000
#define DETECTED_inst 0x00200000
#define DETECTED_ibag 0x00400000
#define DETECTED_imod 0x00800000
#define DETECTED_igen 0x01000000
#define DETECTED_shdr 0x02000000

struct SF2_Session
{
	unsigned char *data;
	size_t data_len;
	size_t data_mmap_len;   /* will cause munmap */
	size_t data_malloc_len; /* will cause free */

	int detected;

	struct SF2_phdr *phdr;
	int phdr_n;

	struct SF2_pbag *pbag;
	int pbag_n;

	struct SF2_pmod *pmod;
	int pmod_n;

	struct SF2_pgen *pgen;
	int pgen_n;

	struct SF2_inst *inst;
	int inst_n;

	struct SF2_ibag *ibag;
	int ibag_n;

	struct SF2_imod *imod;
	int imod_n;

	struct SF2_igen *igen;
	int igen_n;

	struct SF2_shdr *shdr;
	int shdr_n;

	/* file-format version */
	uint16_t ifil_major;
	uint16_t ifil_minor;

	uint16_t *samples;
	uint8_t *samples24; /* optional last 24bit of presicion if available */
	uint32_t samples_n;
};

__attribute__ ((visibility ("internal")))
void
SF2_Free(struct SF2_Session *s);

__attribute__ ((visibility ("internal")))
struct SF2_Session *
SF2_Load_FILE(FILE *file);

__attribute__ ((visibility ("internal")))
struct SF2_Session *
SF2_Load_fd(int fd);

#endif
