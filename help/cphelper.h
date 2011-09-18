/* OpenCP Module Player
 * copyright (c) '94-'98 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * CP hypertext help viewer
 *
 * revision history: (please note changes here)
 *  -fg980924  Fabian Giesen <gfabian@jdcs.su.nw.schule.de>
 *    -first version (mainly for wrappers)
 */

#ifndef _cphelper_h
#define _cphelper_h

typedef struct help_link {
	uint32_t   posx, posy, len;
	void *ref;
} help_link;

typedef struct llink {
	uint32_t   posx, posy, len;
	void *ref;
	struct llink *next;
} link_list;

typedef struct     helppage {
	char       name[128];
	char       desc[128];
	char      *data;
	uint16_t  *rendered;
	int        linkcount;
	help_link *links;
	uint32_t   size, lines;
} helppage;

#define hlpErrOk       0
#define hlpErrNoFile   1
#define hlpErrBadFile  2
#define hlpErrTooNew   3

extern helppage *brDecodeRef(char *name);
extern void brRenderPage(helppage *pg);
extern void brSetPage(helppage *pg);
extern void brDisplayHelp(void);
extern void brSetWinStart(int fl);
extern void brSetWinHeight(int h);
extern int brHelpKey(uint16_t key);

#endif
