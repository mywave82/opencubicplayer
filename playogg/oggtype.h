#ifndef PLAYOGG_OGGTYPE_H
#define PLAYOGG_OGGTYPE_H 1

int __attribute__ ((visibility ("internal"))) ogg_type_init (void);

void __attribute__ ((visibility ("internal"))) ogg_type_done (void);

extern const struct cpifaceplayerstruct __attribute__ ((visibility ("internal"))) oggPlayer;

#endif
