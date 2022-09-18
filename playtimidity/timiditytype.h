#ifndef PLAYTIMIDITY_TIMIDITYTYPE_H
#define PLAYTIMIDITY_TIMIDITYTYPE_H 1

int __attribute__ ((visibility ("internal"))) timidity_type_init (void);

void __attribute__ ((visibility ("internal"))) timidity_type_done (void);

extern const struct cpifaceplayerstruct __attribute__ ((visibility ("internal"))) timidityPlayer;

#endif
