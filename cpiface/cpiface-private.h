#ifndef _CPIFACE_PRIVATE_H
#define _CPIFACE_PRIVATE_H 1

#include "stuff/sets.h"

struct cpiDebugPair_t
{
	uint16_t offset;
	uint16_t length;
	uint8_t  linebreak;
};

struct cpifaceSessionPrivate_t
{
	struct cpifaceSessionAPI_t Public;

	/* mcpedit */
	struct settings mcpset;
	enum mcpNormalizeType mcpType;
	int MasterPauseFadeParameter;

	/* instrument visualizer */
	struct insdisplaystruct Inst;
	int   InstScroll;
	int   InstFirstLine;
	int   InstStartCol;
	int   InstLength;
	int   InstHeight;
	int   InstWidth;

	int   openStatus; // err.h

	/* cpiDebug */
	char                  cpiDebug_bufbase[2048];
	unsigned int          cpiDebug_buffill;
	unsigned int          cpiDebugLastWidth;
	struct cpiDebugPair_t cpiDebug_line[100];
	unsigned int          cpiDebug_lines;
};

extern __attribute__ ((visibility ("internal"))) struct cpifaceSessionPrivate_t cpifaceSessionAPI;

void __attribute__ ((visibility ("internal"))) fftInit(void);

void __attribute__ ((visibility ("internal"))) cpiAnalInit (void);
void __attribute__ ((visibility ("internal"))) cpiAnalDone (void);
void __attribute__ ((visibility ("internal"))) cpiChanInit (void);
void __attribute__ ((visibility ("internal"))) cpiGraphInit (void);
void __attribute__ ((visibility ("internal"))) cpiGraphDone (void);
void __attribute__ ((visibility ("internal"))) cpiInstInit (void);
void __attribute__ ((visibility ("internal"))) cpiWurfel2Init (void);
void __attribute__ ((visibility ("internal"))) cpiWurfel2Done (void);
void __attribute__ ((visibility ("internal"))) cpiLinksInit (void);
void __attribute__ ((visibility ("internal"))) cpiLinksDone (void);
void __attribute__ ((visibility ("internal"))) cpiMVolInit (void);
void __attribute__ ((visibility ("internal"))) cpiMVolDone (void);
void __attribute__ ((visibility ("internal"))) cpiPhaseInit (void);
void __attribute__ ((visibility ("internal"))) cpiPhaseDone (void);
void __attribute__ ((visibility ("internal"))) cpiScopeInit (void);
void __attribute__ ((visibility ("internal"))) cpiScopeDone (void);
void __attribute__ ((visibility ("internal"))) cpiTrackInit (void);
void __attribute__ ((visibility ("internal"))) cpiVolCtrlInit (void);
void __attribute__ ((visibility ("internal"))) cpiVolCtrlDone (void);

int mcpSetProcessKey (struct cpifaceSessionPrivate_t *f, uint16_t key);

#endif
