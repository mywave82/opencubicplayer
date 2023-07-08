#ifndef _STUFF__PIPERUN_H
#define _STUFF__PIPERUN_H

void *ocpPipeProcess_create (const char * const commandLine[]);
int ocpPipeProcess_destroy (void *process);
int ocpPipeProcess_terminate (void *process);

int ocpPipeProcess_read_stdout (void *process, char *const buffer, unsigned size);
int ocpPipeProcess_read_stderr (void *process, char *const buffer, unsigned size);

struct PipeProcessAPI_t
{
	void *(*Create) (const char * const commandLine[]);
	int   (*Destroy) (void *process);
	int   (*Terminate) (void *process);

	int   (*ReadStdOut) (void *process, char *const buffer, unsigned size);
	int   (*ReadStdErr) (void *process, char *const buffer, unsigned size);
};

extern const struct PipeProcessAPI_t PipeProcess;

#endif
