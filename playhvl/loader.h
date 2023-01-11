#ifndef _PLAY_HVL_LOADER_H
#define _PLAY_HVL_LOADER_H 1

struct cpifaceSessionAPI_t;

struct hvl_tune __attribute__ ((visibility ("internal"))) *hvl_load_ahx (struct cpifaceSessionAPI_t *cpifaceSession, const uint8_t *buf, uint32_t buflen, uint32_t defstereo, uint32_t freq );
struct hvl_tune __attribute__ ((visibility ("internal"))) *hvl_load_hvl (struct cpifaceSessionAPI_t *cpifaceSession, const uint8_t *buf, uint32_t buflen, uint32_t defstereo, uint32_t freq );
struct hvl_tune __attribute__ ((visibility ("internal"))) *hvl_LoadTune_memory (struct cpifaceSessionAPI_t *cpifaceSession, const uint8_t *buf, uint32_t buflen, uint32_t defstereo, uint32_t freq);
void __attribute__ ((visibility ("internal"))) hvl_FreeTune (struct hvl_tune *ht);

#endif
