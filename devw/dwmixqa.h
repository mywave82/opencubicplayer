#ifndef _DWMIXQ_H
#define _DWMIXQ_H

extern void mixqPlayChannel(int16_t *buf, uint32_t len, struct channel *ch, int quiet);
extern void mixqSetupAddresses(int16_t (*voltab)[2][256], int16_t (*interpoltabq)[32][256][2], int16_t (*interpoltabq2)[16][256][4]);
extern void mixqAmplifyChannel(int32_t *buf, const int16_t *src, uint32_t len, const int32_t vol);
extern void mixqAmplifyChannelUp(int32_t *buf, const int16_t *src, uint32_t len, int32_t vol);
extern void mixqAmplifyChannelDown(int32_t *buf, const int16_t *src, uint32_t len, int32_t vol);

#endif
