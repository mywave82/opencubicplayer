#ifndef _PLAY_HVL_LOADER_H
#define _PLAY_HVL_LOADER_H 1

struct hvl_tune __attribute__ ((visibility ("internal"))) *hvl_load_ahx (const uint8_t *buf, uint32_t buflen, uint32_t defstereo, uint32_t freq );
struct hvl_tune __attribute__ ((visibility ("internal"))) *hvl_load_hvl (const uint8_t *buf, uint32_t buflen, uint32_t defstereo, uint32_t freq );
struct hvl_tune __attribute__ ((visibility ("internal"))) *hvl_LoadTune_memory (const uint8_t *buf, uint32_t buflen, uint32_t defstereo, uint32_t freq);
#if 0
struct hvl_tune __attribute__ ((visibility ("internal"))) *hvl_LoadTune_file (char *name, uint32_t freq, uint32_t defstereo);
#endif
void __attribute__ ((visibility ("internal"))) hvl_FreeTune (struct hvl_tune *ht);

#endif
