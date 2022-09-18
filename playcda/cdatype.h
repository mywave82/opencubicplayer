#ifndef PLAYCDA_CDATYPE_H
#define PLAYCDA_CDATYPE_H 1

int __attribute__ ((visibility ("internal"))) cda_type_init (void);

void __attribute__ ((visibility ("internal"))) cda_type_done (void);

extern const struct cpifaceplayerstruct __attribute__ ((visibility ("internal"))) cdaPlayer;

#endif
