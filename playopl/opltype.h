#ifndef PLAYOPL_OPLTYPE_H
#define PLAYOPL_OPLTYPE_H 1

int __attribute__ ((visibility ("internal"))) opl_type_init (void);

void __attribute__ ((visibility ("internal"))) opl_type_done (void);

extern "C"
{
	extern const struct cpifaceplayerstruct __attribute__ ((visibility ("internal"))) oplPlayer;
}

#endif
