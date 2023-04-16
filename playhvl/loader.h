#ifndef _PLAY_HVL_LOADER_H
#define _PLAY_HVL_LOADER_H 1

struct cpifaceSessionAPI_t;

OCP_INTERNAL struct hvl_tune *hvl_load_ahx (struct cpifaceSessionAPI_t *cpifaceSession, const uint8_t *buf, uint32_t buflen, uint32_t defstereo, uint32_t freq );
OCP_INTERNAL struct hvl_tune *hvl_load_hvl (struct cpifaceSessionAPI_t *cpifaceSession, const uint8_t *buf, uint32_t buflen, uint32_t defstereo, uint32_t freq );
OCP_INTERNAL struct hvl_tune *hvl_LoadTune_memory (struct cpifaceSessionAPI_t *cpifaceSession, const uint8_t *buf, uint32_t buflen, uint32_t defstereo, uint32_t freq);
OCP_INTERNAL void hvl_FreeTune (struct hvl_tune *ht);

#endif
