#ifndef __POSTPROC_H
#define __POSTPROC_H 1

struct cpifaceSessionAPI_t;

struct PostProcFPRegStruct
{
	const char *name;
	void (*Process) (struct cpifaceSessionAPI_t *cpifaceSession, float *buffer, int len, int rate);
	void (*Init) (int rate);
	void (*Close) (void);
	const struct ocpvolregstruct *VolRegs;
	int (*ProcessKey) (uint16_t key);
};

int mcpRegisterPostProcFP (const struct PostProcFPRegStruct *plugin);
void mcpUnregisterPostProcFP (const struct PostProcFPRegStruct *plugin);
const struct PostProcFPRegStruct *mcpFindPostProcFP (const char *name);
void mcpListAllPostProcFP (const struct PostProcFPRegStruct ***postproc, int *count);

struct PostProcIntegerRegStruct
{
	const char *name;
	void (*Process) (struct cpifaceSessionAPI_t *cpifaceSession, int32_t *buffer, int len, int rate);
	void (*Init) (int rate);
	void (*Close) (void);
	const struct ocpvolregstruct *VolRegs;
	int (*ProcessKey) (uint16_t key);
};

int mcpRegisterPostProcInteger (const struct PostProcIntegerRegStruct *plugin);
void mcpUnregisterPostProcInteger (const struct PostProcIntegerRegStruct *plugin);
const struct PostProcIntegerRegStruct *mcpFindPostProcInteger (const char *name);
void mcpListAllPostProcInteger (const struct PostProcIntegerRegStruct ***postproc, int *count);

#endif
