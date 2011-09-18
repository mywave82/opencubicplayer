#ifndef _MCHASM_H
#define _MCHASM_H

#ifdef I386_ASM
#define RP2 __attribute__ ((regparm (2)))
#else
#define RP2
#endif

typedef uint32_t RP2 (*mixAddAbsfn)(const void *ch, uint32_t len);

extern uint32_t RP2 mixAddAbs16M(const void *ch, uint32_t len);
extern uint32_t RP2 mixAddAbs16MS(const void *ch, uint32_t len);
extern uint32_t RP2 mixAddAbs16S(const void *ch, uint32_t len);
extern uint32_t RP2 mixAddAbs16SS(const void *ch, uint32_t len);

extern uint32_t RP2 mixAddAbs8M(const void *ch, uint32_t len);
extern uint32_t RP2 mixAddAbs8MS(const void *ch, uint32_t len);
extern uint32_t RP2 mixAddAbs8S(const void *ch, uint32_t len);
extern uint32_t RP2 mixAddAbs8SS(const void *ch, uint32_t len);


#if defined(I386_ASM) && !defined(__PIC__)
#define RP3 __attribute__ ((regparm (3)))
#else
#define RP3
#endif

typedef void RP3 (*mixGetMasterSamplefn)(int16_t *dst, const void *src, uint32_t len, uint32_t step);

extern void RP3 mixGetMasterSampleMS8M(int16_t *dst, const void *src, uint32_t len, uint32_t step);
extern void RP3 mixGetMasterSampleMU8M(int16_t *dst, const void *src, uint32_t len, uint32_t step);
extern void RP3 mixGetMasterSampleMS8S(int16_t *dst, const void *src, uint32_t len, uint32_t step);
extern void RP3 mixGetMasterSampleMU8S(int16_t *dst, const void *src, uint32_t len, uint32_t step);
extern void RP3 mixGetMasterSampleSS8M(int16_t *dst, const void *src, uint32_t len, uint32_t step);
extern void RP3 mixGetMasterSampleSU8M(int16_t *dst, const void *src, uint32_t len, uint32_t step);
extern void RP3 mixGetMasterSampleSS8S(int16_t *dst, const void *src, uint32_t len, uint32_t step);
extern void RP3 mixGetMasterSampleSU8S(int16_t *dst, const void *src, uint32_t len, uint32_t step);
extern void RP3 mixGetMasterSampleSS8SR(int16_t *dst, const void *src, uint32_t len, uint32_t step);
extern void RP3 mixGetMasterSampleSU8SR(int16_t *dst, const void *src, uint32_t len, uint32_t step);

extern void RP3 mixGetMasterSampleMS16M(int16_t *dst, const void *src, uint32_t len, uint32_t step);
extern void RP3 mixGetMasterSampleMU16M(int16_t *dst, const void *src, uint32_t len, uint32_t step);
extern void RP3 mixGetMasterSampleMS16S(int16_t *dst, const void *src, uint32_t len, uint32_t step);
extern void RP3 mixGetMasterSampleMU16S(int16_t *dst, const void *src, uint32_t len, uint32_t step);
extern void RP3 mixGetMasterSampleSS16M(int16_t *dst, const void *src, uint32_t len, uint32_t step);
extern void RP3 mixGetMasterSampleSU16M(int16_t *dst, const void *src, uint32_t len, uint32_t step);
extern void RP3 mixGetMasterSampleSS16S(int16_t *dst, const void *src, uint32_t len, uint32_t step);
extern void RP3 mixGetMasterSampleSU16S(int16_t *dst, const void *src, uint32_t len, uint32_t step);
extern void RP3 mixGetMasterSampleSS16SR(int16_t *dst, const void *src, uint32_t len, uint32_t step);
extern void RP3 mixGetMasterSampleSU16SR(int16_t *dst, const void *src, uint32_t len, uint32_t step);

#endif
