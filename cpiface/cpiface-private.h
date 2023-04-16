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

extern OCP_INTERNAL struct cpifaceSessionPrivate_t cpifaceSessionAPI;

OCP_INTERNAL void fftInit(void);

OCP_INTERNAL void cpiAnalInit (void);
OCP_INTERNAL void cpiAnalDone (void);
OCP_INTERNAL void cpiChanInit (void);
OCP_INTERNAL void cpiGraphInit (void);
OCP_INTERNAL void cpiGraphDone (void);
OCP_INTERNAL void cpiInstInit (void);
OCP_INTERNAL void cpiWurfel2Init (void);
OCP_INTERNAL void cpiWurfel2Done (void);
OCP_INTERNAL void cpiLinksInit (void);
OCP_INTERNAL void cpiLinksDone (void);
OCP_INTERNAL void cpiMVolInit (void);
OCP_INTERNAL void cpiMVolDone (void);
OCP_INTERNAL void cpiPhaseInit (void);
OCP_INTERNAL void cpiPhaseDone (void);
OCP_INTERNAL void cpiScopeInit (void);
OCP_INTERNAL void cpiScopeDone (void);
OCP_INTERNAL void cpiTrackInit (void);
OCP_INTERNAL void cpiVolCtrlInit (void);
OCP_INTERNAL void cpiVolCtrlDone (void);

OCP_INTERNAL int mcpSetProcessKey (struct cpifaceSessionPrivate_t *f, uint16_t key);

#endif
