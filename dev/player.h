#ifndef __PLAYER_H
#define __PLAYER_H

/* in the future we might add optional 5.1, 7.1, float etc - All devp drivers MUST atleast support PLR_STEREO_16BIT_SIGNED */
enum plrRequestFormat
{
	PLR_STEREO_16BIT_SIGNED=1
};

struct ocpfilehandle_t;

struct cpifaceSessionAPI_t; /* cpiface.h */

struct ocpvolregstruct; /* vol.h */

struct plrDevAPI_t
{
	unsigned int (*Idle)(void); /* returns the current BufferDelay - should be called periodically, usually at FPS rate - inserts pause-samples if needed */
	void (*PeekBuffer)(void **buf1, unsigned int *length1, void **buf2, unsigned int *length2); /* used by analyzer, graphs etc - length given in samples */
	int (*Play)(uint32_t *rate, enum plrRequestFormat *format, struct ocpfilehandle_t *source_file, struct cpifaceSessionAPI_t *cpifaceSession); // returns 0 on error - DiskWriter plugin uses source_file to name its output. Caller can suggest values in rate and format */
	void (*GetBuffer)(void **buf, unsigned int *samples);
	uint32_t (*GetRate)(void); /* this call will probably disappear in the future */

	/* positive numbers = samples in the past
         * 0 on the last sample commited
         * negative numbers = samples into the future
	 */
	void (*OnBufferCallback) (int samplesuntil, void (*callback)(void *arg, int samples_ago), void *arg);
	void (*CommitBuffer)(unsigned int samples);
	void (*Pause)(int pause); // driver will insert dummy samples as needed
	void (*Stop) (struct cpifaceSessionAPI_t *cpifaceSession);
	struct ocpvolregstruct *VolRegs; /* null if feature is not present */
	int (*ProcessKey)(uint16_t);

	void (*GetStats)(uint64_t *committed, uint64_t *processed);
};

extern const struct plrDevAPI_t *plrDevAPI;

extern void plrGetRealMasterVolume(int *l, int *r);
extern void plrGetMasterSample(int16_t *s, uint32_t len, uint32_t rate, int opt);

#endif
