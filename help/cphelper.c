/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * CP hypertext help viewer
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * revision history: (please note changes here)
 *  -fg980812  Fabian Giesen <gfabian@jdcs.su.nw.schule.de>
 *    -first alpha (hey, and it was written on my BIRTHDAY!)
 *    -some pages of html documentation converted
 *  -fg980813  Fabian Giesen <gfabian@jdcs.su.nw.schule.de>
 *    -added pgup/pgdown support
 *    -changed drawing method. stopped all flickering this way. but the code
 *     did not get better during this bugfix... :)
 *    -changed helpfile loader to load in "boot" phase, not in first use
 *     I did this because the helpfile was sometimes not found when you
 *     changed the directory via DOS shell.
 *    -some speedups
 *    -the "description" field is now displayed at top/right of the window
 *    -again converted some documentation
 *  -fg980814  Fabian Giesen <gfabian@jdcs.su.nw.schule.de>
 *    -added support for compressed helpfiles
 *    -added percentage display at right side of description
 *    -added jumping to Contents page via Alt-C
 *    -added jumping to Index page via Alt-I
 *    -added jumping to License page via Alt-L
 *    -html conversion again
 *  -fg980820  Fabian Giesen <gfabian@jdcs.su.nw.schule.de>
 *    -now searches CP.HLP in data path
 *    -decided that code could be much cleaner but too lazy to change it :)
 *    -maybe i'll add context sensitivity (if there is interest)
 *    -well, uhm, html documentation is still not fully converted (shame
 *     over me... :))
 *  -fg_dunno  Fabian Giesen <gfabian@jdcs.su.nw.schule.de>
 *    -added support for CP.HLP in CP.PAK because kb said CPHELPER would be
 *     part of the next release
 *  -fg980924  Fabian Giesen <gfabian@jdcs.su.nw.schule.de>
 *    -changed keyboard use/handling a lot based on advices (commands? :)
 *     from kb.
 *    -finally the help compiler supports colour codes (i don't know why I
 *     write this here).
 *    -added fileselector support (yes!)
 *    -made this possible by splitting this up in one "host" and two
 *     "wrappers" for fileselector/player interface
 *    -and, you won't believe, it still works :)
 *  -kbwhenever Tammo Hinrichs <opencp@gmx.net>
 *    -some minor cosmetical changes
 *  -fg981118  Fabian Giesen <fabian@jdcs.su.nw.schule.de>
 *    -note: please use this email now (the old one still works, but I don't
 *     give promises about how long this will be)
 *    -changed keyboard control again according to suggestions kb made
 *     (1. kannst du mir das nicht einfach mailen?  2. warum sagst du mir
 *      nicht sofort, das du lynx-keyboard-handling willst  3. sag mal, bin
 *      ich eigentlich dein codesklave? :)
 *    -detailed changes: up/down now also selects links
 *                       this ugly tab/shift-tab handling removed
 *                       pgup/pgdown updates current link
 *                       keyboard handling much more lynx-like
 *    -this code even got worse during change of the keyboard-handling
 *     (but using it got even nicer)
 *    -hope kb likes this as it is (nich wahr, meister, sei ma zufrieden! :)
 *    -fixed this nasty bug which crashed opencp (seemed to be some un-
 *     initialized pointer, or something else...)
 *  -ryg981121  Fabian Giesen <fabian@jdcs.su.nw.schule.de>
 *    -now i'm using my handle instead of my name for changelog (big change,
 *     eh?)
 *    -fixed some stupid bugs with pgup/pgdown handling
 *    -also fixed normal scrolling (hope it works correctly now...)
 *    -no comment from kb about my "lynx style key handling" yet...
 *  -fd981205  Felix Domke <tmbinc@gmx.net>
 *    -included the stdlib.h AGAIN AND AGAIN. hopefully this version will now
 *     finally reach the repository ;)
 *     (still using my realname... ;)
 *  -fd981206   Felix Domke    <tmbinc@gmx.net>
 *    -edited for new binfile
 *  -ss040911   Stian Skjelstad <stian@nixia.no>
 *    -stupid misstake prevented cp.hlp to be imported from cp.pak
 */

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <zlib.h>
#include "types.h"
#include "boot/psetting.h"
#include "stuff/poutput.h"
#include "boot/plinkman.h"
#include "stuff/err.h"
#include "help/cphelper.h"
#include "cpiface/cpiface.h"

static unsigned int plWinFirstLine, plWinHeight, plHelpHeight, plHelpScroll;

static const uint32_t  Helpfile_ID=1213219663;
static const uint32_t  Helpfile_Ver=0x011000;
static uint32_t   Helppages;
static int        HelpfileErr=hlpErrNoFile;
static helppage  *Page, *curpage;
static help_link *curlink;
static int        link_ind;

/* Helpfile Commands */

#define   CMD_NORMAL     1
#define   CMD_BRIGHT     2
#define   CMD_HYPERLINK  3
#define   CMD_CENTERED   4
#define   CMD_CHCOLOUR   5
#define   CMD_RAWCHAR    6
#define   CMD_LINEFEED  10             /* this is a pseudo-command... */
#define   CMD_MAX       31

/* Useful macros... */

#define   MIN(a,b)      ((a)<(b)?(a):(b))
#define   MAX(a,b)      ((a)>(b)?(a):(b))
#define   ABS(a)        ((a)<0?-(a):(a))

/* ---------------------------- here starts the viewer */

static int doReadVersion100Helpfile(FILE *file)
{
	unsigned int i;
	if (fread(&Helppages, sizeof(Helppages), 1, file) != 1)
	{
		perror(__FILE__ ": fread failed #1: ");
		return hlpErrBadFile;
	}
	Page=calloc(Helppages, sizeof(Page[0]));

	for (i=0; i<Helppages; i++)
	{
		unsigned char len;

		memset(Page[i].name, 0, 128);
		if (fread(&len, sizeof(len), 1, file) != 1)
		{
			perror(__FILE__ ": fread failed #2: ");
			return hlpErrBadFile;
		}
		if (fread(Page[i].name, len, 1, file) != 1)
		{
			perror(__FILE__ ": fread failed #3: ");
			return hlpErrBadFile;
		}

		memset(Page[i].desc, 0, 128);
		if (fread(&len, sizeof(len), 1, file) != 1)
		{
			perror(__FILE__ ": fread failed #4: ");
			return hlpErrBadFile;
		}

		if (fread(Page[i].desc, len, 1, file) != 1)
		{
			perror(__FILE__ ": fread failed #5: ");
			return hlpErrBadFile;
		}

		if (fread(&Page[i].size, sizeof(Page[i].size), 1, file) != 1)
		{
			perror(__FILE__ ": fread failed #6: ");
			return hlpErrBadFile;
		}

		Page[i].size = uint32_little (Page[i].size);
		if (fread(&Page[i].lines, sizeof(Page[i].lines), 1, file) != 1)
		{
			perror(__FILE__ ": fread failed #7: ");
			return hlpErrBadFile;
		}

		Page[i].lines = uint32_little (Page[i].lines);

		Page[i].links=NULL;
		Page[i].rendered=NULL;
	};

	for (i=0; i<Helppages; i++)
	{
		Page[i].data=calloc(Page[i].size, 1);
		if (fread(Page[i].data, Page[i].size, 1, file) != 1)
		{
			perror(__FILE__ ": fread failed #8: ");
			return hlpErrBadFile;
		}
	};

	return hlpErrOk;
}

static int doReadVersion110Helpfile(FILE *file)
{
	int  *compdatasize;
	char *inbuf;
	unsigned int i;

	if (fread(&Helppages, sizeof(Helppages), 1, file) != 1)
	{
		perror(__FILE__ ": fread failed #9: ");
		return hlpErrBadFile;
	}
	Helppages = uint32_little (Helppages);
	Page = calloc(Helppages, sizeof(Page[0]));

	compdatasize=calloc(Helppages, sizeof(int));

	for (i=0; i<Helppages; i++)
	{
		unsigned char len;

		memset(Page[i].name, 0, 128);
		if (fread(&len, sizeof(len), 1, file) != 1)
		{
			perror(__FILE__ ": fread failed #10: ");
			free(compdatasize);
			return hlpErrBadFile;
		}
		if (fread(Page[i].name, len, 1, file) != 1)
		{
			perror(__FILE__ ": fread failed #11: ");
			free(compdatasize);
			return hlpErrBadFile;
		}

		memset(Page[i].desc, 0, 128);
		if (fread(&len, sizeof(len), 1, file) != 1)
		{
			perror(__FILE__ ": fread failed #12: ");
			free(compdatasize);
			return hlpErrBadFile;
		}
		if (fread(Page[i].desc, len, 1, file) != 1)
		{
			perror(__FILE__ ": fread failed #13: ");
			free(compdatasize);
			return hlpErrBadFile;
		}

		if (fread(&Page[i].size, sizeof(Page[i].size), 1, file) != 1)
		{
			perror(__FILE__ ": fread failed #14: ");
			free(compdatasize);
			return hlpErrBadFile;
		}
		Page[i].size = uint32_little (Page[i].size);
		if (fread(&Page[i].lines, sizeof(Page[i].lines), 1, file) != 1)
		{
			perror(__FILE__ ": fread failed #15: ");
			free(compdatasize);
			return hlpErrBadFile;
		}
		Page[i].lines = uint32_little (Page[i].lines);

		if (fread(&compdatasize[i], sizeof(compdatasize[i]), 1, file) != 1)
		{
			perror(__FILE__ ": fread failed #16: ");
			free(compdatasize);
			return hlpErrBadFile;
		}
		compdatasize[i]=uint32_little(compdatasize[i]);
		Page[i].links=NULL;
		Page[i].rendered=NULL;
	};

	for (i=0; i<Helppages; i++)
	{
		uLongf temp=Page[i].size;
		Page[i].data=calloc(Page[i].size, 1);
		inbuf=calloc(compdatasize[i], 1);
		if (fread(inbuf, compdatasize[i], 1, file) != 1)
		{
			perror(__FILE__ ": fread failed #17: ");
			free(compdatasize);
			free(inbuf);
			return hlpErrBadFile;
		}
		uncompress((Bytef *)Page[i].data, &temp, (Bytef *)inbuf, compdatasize[i]);
		Page[i].size=temp;
		free(inbuf);
	}
	free(compdatasize);

	return hlpErrOk;
}

static int doReadHelpFile(FILE *file)
{
	uint32_t version;
	uint32_t temp;

	if (fread(&temp, sizeof(temp), 1, file) != 1)
	{
		perror(__FILE__ ": fread failed #18: ");
		return hlpErrBadFile;
	}
	temp = uint32_little (temp);
	if (temp!=Helpfile_ID)
		return hlpErrBadFile;

	if (fread(&version, sizeof(version), 1, file) != 1)
	{
		perror(__FILE__ ": fread failed #19: ");
		return hlpErrBadFile;
	}
	version = uint32_little (version);

	if (version>Helpfile_Ver)
		return hlpErrTooNew;
	if (version<0x10000)
		return hlpErrBadFile;

	switch (version >> 8)
	{
		case 0x100:
			return doReadVersion100Helpfile(file);
		case 0x110:
			return doReadVersion110Helpfile(file);
		default:
			return hlpErrBadFile;
	};
}

static char plReadHelpExternal(void)
{
	char    helpname[PATH_MAX+1];
	FILE   *bf;

	if (Page && (HelpfileErr==hlpErrOk))
		return 1;

	strcpy(helpname, cfDataDir);
	strcat(helpname, "ocp.hlp");

	if ((bf=fopen(helpname, "r")))
	{
		HelpfileErr=doReadHelpFile(bf);
		fclose(bf);
	} else {
		HelpfileErr=hlpErrNoFile;
		return 0;
	};

	return (HelpfileErr==hlpErrOk);
}

static char plReadHelpPack(void)
{
	char path[PATH_MAX];
	FILE *ref;

	if (Page && (HelpfileErr==hlpErrOk))
		return 1;

	snprintf(path, sizeof(path), "%s%s", cfDataDir, "ocp.hlp");
	if ((ref=fopen(path, "r")))
	{
		HelpfileErr=doReadHelpFile(ref);
		fclose(ref);
	} else {
		HelpfileErr=hlpErrNoFile;
		return 0;
	};

	return (HelpfileErr==hlpErrOk);
}

helppage *brDecodeRef(char *name)
{
	unsigned int i;
	for (i=0; i<Helppages; i++)
		if (!strcasecmp(Page[i].name, name))
			return &Page[i];
	return NULL;
}

static help_link *firstLinkOnPage(helppage *pg)
{
	if (!pg->linkcount)
		return NULL;
	return &pg->links[0];
}

static int linkOnCurrentPage(help_link *lnk)
{
	unsigned int y;

	if (!lnk)
		return 0;

	y=lnk->posy;
	if ((y>=plHelpScroll) && (y<plHelpScroll+plWinHeight))
		return 1;

	return 0;
}

void brRenderPage(helppage *pg)
{
	link_list *lst, *endlst;
	int        lcount;
	uint16_t   linebuf[80];
	char       *data;
	char       attr;
	int        x, y, i;

	if (pg->rendered)
	{
		free(pg->rendered);
		pg->rendered=NULL;
	};

	if (pg->links)
	{
		free(pg->links);
		pg->links=NULL;
	};

	lst=endlst=NULL;
	lcount=0;
	x=y=0;
	attr=0x07;

	pg->rendered=calloc(80*MAX(pg->lines, plWinHeight), sizeof(uint16_t));;
	memset(pg->rendered, 0, 160*MAX(pg->lines, plWinHeight));
	memset(linebuf, 0, 160);

	data=pg->data;
	i=pg->size;

	while (i>0)
	{
		if (*data<CMD_MAX)
		{
			switch (*data)
			{
				case CMD_NORMAL:
					attr=0x07;
					break;
				case CMD_BRIGHT:
					attr=0x0f;
					break;
				case CMD_HYPERLINK:
					{
						char linkbuf[256];
						int  llen;

						data++;
						i--;
						strcpy(linkbuf, data);

						if (!endlst)
						{
							lst=calloc(sizeof(link_list), 1);
							endlst=lst;
						} else {
							 endlst->next=calloc(sizeof(link_list), 1);
							 endlst=endlst->next;
						};

						*strchr(linkbuf, ',')=0;
						endlst->ref=(void *) brDecodeRef(linkbuf);

						i-=strchr(data, ',')-data+1;
						data+=strchr(data, ',')-data+1;

						llen=0;

						endlst->posx=x;
						endlst->posy=y;

						while (*data)
						{
							if ((x<80)&&(*data!=CMD_RAWCHAR))
							{
								linebuf[x]=(*data)|0x0300;
								x++;
								llen++;
							};

							data++; i--;
						};

						endlst->len=llen;

						lcount++;

						break;
					}
				case CMD_CENTERED:
					data++;
					i--;

					x=40-(strlen(data) >> 1);
					if (x<0)
						x=0;

					while (*data)
					{
						if (x<80)
						{
							linebuf[x]=(*data)|(attr<<8);
							x++;
						};

						data++;
						i--;
					};

					break;
				case CMD_CHCOLOUR:
					data++;
					i--;
					attr=*data;
					break;
				case CMD_RAWCHAR:
					data++;
					i--;

					if (x<80)
					{
						linebuf[x]=(*data)|(attr<<8);
						x++;
					};

					break;
				case CMD_LINEFEED:
					memcpy(&pg->rendered[y*80], linebuf, 160);
					x=0;
					y++;
					memset(linebuf, 0, 160);
					break;
			};

			data++; i--;
		} else {
			if (x<80)
			{
				linebuf[x]=(*data)|(attr<<8);
				x++;
			};

			data++;
			i--;
		};
	};

	pg->links=calloc(sizeof(help_link), lcount);
	pg->linkcount=lcount;

	for (i=0; i<lcount; i++)
	{
		pg->links[i].posx=lst->posx;
		pg->links[i].posy=lst->posy;
		pg->links[i].len=lst->len;
		pg->links[i].ref=lst->ref;

		endlst=lst;
		lst=lst->next;
		free(endlst);
	}
}

void brSetPage(helppage *page)
{
	if (!page)
		return;

	if (curpage)
	{
		if (curpage->rendered)
		{
			free(curpage->rendered);
			curpage->rendered=NULL;
		};

		if (curpage->links)
		{
			free(curpage->links);
			curpage->links=NULL;
		};
	};

	curpage=page;
	brRenderPage(curpage);

	plHelpHeight=curpage->lines;
	plHelpScroll=0;

	curlink=firstLinkOnPage(curpage);
	if (!curlink)
		link_ind=-1;
	else
		link_ind=0;
}

void brDisplayHelp(void)
{
	unsigned int curlinky;
	char destbuffer[60];
	char strbuffer[256];
	char numbuffer[4];
	int descxp;
	unsigned int y;

	if ((plHelpScroll+plWinHeight)>plHelpHeight)
		plHelpScroll=plHelpHeight-plWinHeight;

	if ((signed)plHelpScroll<0)
		plHelpScroll=0;

	if (curlink)
		curlinky=(curlink->posy)-plHelpScroll;
	else
		curlinky=-1;

	displaystr(plWinFirstLine-1, 0, 0x09, "   OpenCP help ][   ", 20);


	if (HelpfileErr==hlpErrOk)
		strcpy(strbuffer, curpage->desc);
	else
		strcpy(strbuffer, "Error!");

	_convnum(100*plHelpScroll/MAX(plHelpHeight-plWinHeight, 1), numbuffer, 10, 3);

	strcat(strbuffer, "-");
	strcat(strbuffer, numbuffer);
	strcat(strbuffer, "%");

	memset(destbuffer, 0x20, 60);
	descxp=MAX(0, 59-(signed)strlen(strbuffer));

	strncpy(&destbuffer[descxp], strbuffer, 59-descxp);
	displaystr(plWinFirstLine-1, 20, 0x08, destbuffer, 59);


	if (HelpfileErr!=hlpErrOk)
	{
		char errormsg[80];

		strcpy(errormsg, "Error: ");

		switch (HelpfileErr)
		{
			case hlpErrNoFile:
				strcat(errormsg, "Helpfile \"OCP.HLP\" is not present");
				break;
			case hlpErrBadFile:
				strcat(errormsg, "Helpfile \"OCP.HLP\" is corrupted");
				break;
			case hlpErrTooNew:
				strcat(errormsg, "Helpfile version is too new. Please update.");
				break;
			default:
				strcat(errormsg, "Currently undefined help error");
		};

		displayvoid(plWinFirstLine, 0, CONSOLE_MAX_X);

		displaystr(plWinFirstLine+1, 4, 0x04, errormsg, 74);

		for (y=2; y<plWinHeight; y++)
			displayvoid(y+plWinFirstLine, 0, CONSOLE_MAX_X);
	} else {
		for (y=0; y<plWinHeight; y++)
		{
			if ((y+plHelpScroll)>=plHelpHeight)
			{
				displayvoid(y+plWinFirstLine, 0, plScrWidth);
				continue;
			}
			if (y!=curlinky)
			{
				displaystrattr(y+plWinFirstLine, 0, &curpage->rendered[(y+plHelpScroll)*80], 80);
				displayvoid(y+plWinFirstLine, 80, plScrWidth-80);
			} else {
				int yp=(y+plHelpScroll)*80;
				int xp;
			        char dummystr[82];
				int i, off;

				if (curlink->posx!=0)
			        {
					displaystrattr(y+plWinFirstLine, 0, &curpage->rendered[yp], curlink->posx);
			        };

			        xp=curlink->posx+curlink->len;

			        displaystrattr(y+plWinFirstLine, xp,
		                       &curpage->rendered[yp+xp],
		                       79-xp);


				for (i=0, off=yp+curlink->posx; curpage->rendered[off] & 0xff; i++, off++)
					dummystr[i]=curpage->rendered[off] & 0xff;

				dummystr[i]=0;

				displaystr(y+plWinFirstLine, curlink->posx, 4, dummystr, curlink->len);

				displayvoid(y+plWinFirstLine, 80, plScrWidth-80);

			        /* and all this just to prevent flickering. ARG! */
			}
		}
	}
}

static void processActiveHyperlink(void)
{
	if (curlink)
		brSetPage((helppage *) curlink->ref);
}

void brSetWinStart(int fl)
{
	plWinFirstLine=fl;
}

void brSetWinHeight(int h)
{
	plWinHeight=h;
}

int brHelpKey(uint16_t key)
{
	help_link *link2;

	if(!curpage)
		return 1;

	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp(KEY_UP, "Scroll help page up");
			cpiKeyHelp(KEY_DOWN, "Scroll help page down");
			cpiKeyHelp(KEY_PPAGE, "Scroll help page, a page up");
			cpiKeyHelp(KEY_NPAGE, "Scroll help page, a page down");
			cpiKeyHelp(KEY_HOME, "Scroll help page, to the start");
			cpiKeyHelp(KEY_END, "Scroll help page, to the bottom");
			cpiKeyHelp(KEY_ALT_C, "Goto contents help page");
			cpiKeyHelp(KEY_ALT_I, "Goto index help page");
			cpiKeyHelp(KEY_ALT_L, "Goto licence help page");
			cpiKeyHelp(KEY_TAB, "Goto next link");
			cpiKeyHelp(KEY_SHIFT_TAB, "Goto previous link");
			return 0;
		/*case 0x4800: //up*/
		case KEY_UP:
			if (curpage->linkcount)
			{
				link2=&curpage->links[MAX(link_ind-1, 0)];

				if (link2!=curlink)
				{
					if ((signed)(plHelpScroll-link2->posy)>1)
						plHelpScroll--;
					else {
						link_ind=MAX(link_ind-1, 0);
						curlink=link2;

						if (link2->posy<plHelpScroll)
							plHelpScroll=link2->posy;
					}
				} else
					if (plHelpScroll>0)
						plHelpScroll--;
			} else
				if (plHelpScroll>0)
					plHelpScroll--;
			break;
		/*case 0x5000: //down*/
		case KEY_DOWN:
			if (curpage->linkcount)
			{
				link2=&curpage->links[MIN(link_ind+1, curpage->linkcount-1)];

				if (link2->posy-plHelpScroll>plWinHeight)
					plHelpScroll++;
				else {
					link_ind=MIN(link_ind+1, curpage->linkcount-1);
					curlink=link2;

					if (link2->posy>(plHelpScroll+plWinHeight))
						plHelpScroll=link2->posy;
					else
						if (link2->posy==(plHelpScroll+plWinHeight))
							plHelpScroll++;
				}
			} else
				if (plHelpScroll<plHelpHeight-1)
					plHelpScroll++;

			break;
		/*case 0x4900: //pgup*/
		case KEY_PPAGE:
			plHelpScroll-=plWinHeight;
			if ((signed)plHelpScroll<0)
				plHelpScroll=0;

			if (curpage->linkcount)
			{
				if (!linkOnCurrentPage(curlink))
				{
					int bestmatch, bestd;
					int i;

					bestd=2000000;
					bestmatch=-1;

					for (i=curpage->linkcount-1; i>=0; i--)
					{
						int d=ABS((signed)plHelpScroll+(signed)plWinHeight-(signed)curpage->links[i].posy-1);
						if (d<bestd)
						{
							bestd=d;
							bestmatch=i;
						};
					};
					link_ind=bestmatch;
					curlink=&curpage->links[link_ind];
				};
			};
			break;
		/*case 0x5100: //pgdn*/
		case KEY_NPAGE:
			plHelpScroll+=plWinHeight;

			if (plHelpScroll>(plHelpHeight-plWinHeight))
				plHelpScroll=plHelpHeight-plWinHeight;

			if (curpage->linkcount)
			{
				if (!linkOnCurrentPage(curlink))
				{
					int bestmatch, bestd;
					int i;

					bestd=2000000;
					bestmatch=-1;

				        for (i=0; i<curpage->linkcount; i++)
				        {
						int d=ABS((signed)plHelpScroll-(signed)curpage->links[i].posy);
						if (d<bestd)
						{
							bestd=d;
							bestmatch=i;
						};
					};

					link_ind=bestmatch;
					curlink=&curpage->links[link_ind];
				};
			};
			break;
		/*case 0x4700: //home*/
		case KEY_HOME:
			plHelpScroll=0;
			break;
		/*case 0x4F00: //end*/
		case KEY_END:
			plHelpScroll=plHelpHeight-plWinHeight;
			break;
		case KEY_ALT_C:
			brSetPage(brDecodeRef("Contents"));
			break;
		case KEY_ALT_I:
			brSetPage(brDecodeRef("Index"));
			break;
		case KEY_ALT_L:
			brSetPage(brDecodeRef("License"));
			break;
		case KEY_TAB:
			if (curpage->linkcount)
			{
				if (linkOnCurrentPage(curlink))
				{
					link_ind=(link_ind+1)%curpage->linkcount;
					curlink=&curpage->links[link_ind];
				}
				if (!linkOnCurrentPage(curlink))
					plHelpScroll=curlink->posy;
			}
			break;
		case KEY_SHIFT_TAB: /* 0x0f00: //shift-tab*/
			if (curpage->linkcount)
			{
				if (linkOnCurrentPage(curlink))
				{
					link_ind--;
					if (link_ind<0)
						link_ind=curpage->linkcount-1;
					curlink=&curpage->links[link_ind];
				}
				if (!linkOnCurrentPage(curlink))
					plHelpScroll=curlink->posy;
			}
			break;
/*  case 0x3920: case 0x3900: case 0x0020: case 0x000D: // space/enter*/
		case KEY_CTRL_J: case _KEY_ENTER: case ' ':
			processActiveHyperlink();
			break;
		default:
			return 0;
	}
	if ((plHelpScroll+plWinHeight)>plHelpHeight)
		plHelpScroll=plHelpHeight-plWinHeight;
	if ((signed)plHelpScroll<0)
		plHelpScroll=0;
	return 1;
}

static int hlpGlobalInit(void)
{
	helppage *pg;

	plHelpHeight=plHelpScroll=0;

	if (!plReadHelpExternal())
	{
		if (!plReadHelpPack())
		{
			fprintf(stderr, "Warning. Failed to read help files\n");
			return errOk; /* this error is not fatal to rest of the player */
		}
	};

	curpage=NULL;


	pg=brDecodeRef("Contents");
	if (!pg)
		HelpfileErr=hlpErrBadFile;
	else
		brSetPage(pg);

	return errOk;
}

void hlpFreePages(void)
{
	unsigned int i;
	for (i=0; i<Helppages; i++)
	{
		if (Page[i].data)
		{
			free(Page[i].data);
			Page[i].data=NULL;
		};

		if (Page[i].rendered)
		{
			free(Page[i].rendered);
			Page[i].rendered=NULL;
		};

		if (Page[i].links)
		{
			free(Page[i].links);
			Page[i].links=NULL;
		};
	};

	free(Page);
	Page=NULL;

	curpage=NULL;
	curlink=NULL;

	Helppages=link_ind=0;
	HelpfileErr=hlpErrNoFile;
}

static void hlpGlobalClose(void)
{
	hlpFreePages();
}

#ifndef SUPPORT_STATIC_PLUGINS
char *dllinfo = "";
#endif
DLLEXTINFO_PREFIX struct linkinfostruct dllextinfo = {.name = "cphelper", .desc = "OpenCP help browser (c) 1998-09 Fabian Giesen", .ver = DLLVERSION, .size = 0, .Init = hlpGlobalInit, .Close = hlpGlobalClose};
