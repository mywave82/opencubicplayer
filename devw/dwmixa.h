#ifndef _DWMIXA_H
#define _DWMIXA_H

extern void mixrPlayChannel(int32_t *buf, int32_t *fade, uint32_t len, struct channel *ch, int st);
extern void mixrFadeChannel(int32_t *fade, struct channel *ch);
extern void mixrFade(int32_t *buf, int32_t *fade, int len, int stereo);
extern void mixrClip(void *dst, int32_t *src, int len, void *, int32_t max, int b16);
extern void mixrSetupAddresses(int32_t (*vol)[256], uint8_t (*intr)[256][2]);

#ifdef I386_ASM

extern void remap_range1_start(void);
extern void remap_range1_stop(void);

#endif

#endif
