#ifndef _IMSDEV_H
#define _IMSDEV_H

/* All drivers set these, but they are not used to anything */
#define SS_PLAYER 0
#define SS_WAVETABLE 2
#define SS_NEEDPLAYER 4

struct sounddevice;

#define DEVICE_NAME_MAX 63

struct deviceinfo
{
	struct sounddevice *devtype;
	uint32_t opt; /* device specific flags parsed by GetOpt() */
	int8_t subtype;
	char path[DEVICE_NAME_MAX+1]; /* can be like 127.0.0.1:32000, or stuff like /tmp/.esd .... or whatever you prefer or just empty for no force*/
	char mixer[DEVICE_NAME_MAX+1];
};

struct sounddevice
{
	char type;
	char keep;
	char name[32];
	int (*Detect)(struct deviceinfo *c);
	int (*Init)(const struct deviceinfo *c, const char *handle);
	void (*Close)(void);
	uint32_t (*GetOpt)(const char *devinfonode_handle);
};

#endif
