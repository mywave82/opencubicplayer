/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'25 Stian Skjelstad <stian.skjelstad@gmail.com>
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
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>
#include "types.h"
#include "boot/psetting.h"
#include "boot/plinkman.h"
#include "cpiface/cpiface.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "help/cphelper.h"
#include "stuff/compat.h"
#include "stuff/err.h"
#include "stuff/poutput.h"

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

static int doReadVersion100Helpfile (struct ocpfilehandle_t *file)
{
	unsigned int i;

	if (ocpfilehandle_read_uint32_le (file, &Helppages))
	{
		fprintf (stderr, __FILE__ ": fread failed #1\n");
		return hlpErrBadFile;
	}
	Page=calloc(Helppages, sizeof(Page[0]));

	for (i=0; i<Helppages; i++)
	{
		unsigned char len;

		if (ocpfilehandle_read_uint8 (file, &len))
		{
			fprintf (stderr, __FILE__ ": fread failed #2\n");
			return hlpErrBadFile;
		}
		if (len >= sizeof (Page[i].name))
		{
			fprintf (stderr, __FILE__ ": len >= sizeof (Page[%d].name)\n", i);
			return hlpErrBadFile;
		}
		if (file->read (file, Page[i].name, len) != len)
		{
			fprintf (stderr, __FILE__ ": fread failed #3\n");
			return hlpErrBadFile;
		}

		if (ocpfilehandle_read_uint8 (file, &len))
		{
			fprintf (stderr, __FILE__ ": fread failed #4\n");
			return hlpErrBadFile;
		}
		if (len >= sizeof (Page[i].desc))
		{
			fprintf (stderr, __FILE__ ": len >= sizeof (Page[%d].desc)\n", i);
			return hlpErrBadFile;
		}
		if (file->read (file, Page[i].desc, len) != len)
		{
			fprintf (stderr, __FILE__ ": fread failed #5\n");
			return hlpErrBadFile;
		}

		if (ocpfilehandle_read_uint32_le (file, &Page[i].size) ||
		    ocpfilehandle_read_uint32_le (file, &Page[i].lines))
		{
			fprintf (stderr, __FILE__ ": fread failed #6 / #7\n");
			return hlpErrBadFile;
		}
	};

	for (i=0; i<Helppages; i++)
	{
		Page[i].data=calloc(Page[i].size, 1);
		if (file->read (file, Page[i].data, Page[i].size) != Page[i].size)
		{
			fprintf (stderr, __FILE__ ": fread failed #8\n");
			return hlpErrBadFile;
		}
	};

	return hlpErrOk;
}

static int doReadVersion110Helpfile (struct ocpfilehandle_t *file)
{
	uint32_t *compdatasize;
	char *inbuf;
	unsigned int i;

	if (ocpfilehandle_read_uint32_le (file, &Helppages))
	{
		fprintf (stderr, __FILE__ ": fread failed #9\n");
		return hlpErrBadFile;
	}
	Page = calloc(Helppages, sizeof(Page[0]));

	compdatasize=calloc(Helppages, sizeof(int));

	for (i=0; i<Helppages; i++)
	{
		unsigned char len;

		if (ocpfilehandle_read_uint8 (file, &len))
		{
			fprintf (stderr, __FILE__ ": fread failed #10\n");
			free(compdatasize);
			return hlpErrBadFile;
		}
		if (len >= sizeof (Page[i].name))
		{
			fprintf (stderr, __FILE__ ": len >= sizeof (Page[%d].name)\n", i);
			return hlpErrBadFile;
		}
		if (file->read (file, Page[i].name, len) != len)
		{
			fprintf (stderr, __FILE__ ": fread failed #11\n");
			free(compdatasize);
			return hlpErrBadFile;
		}

		if (ocpfilehandle_read_uint8 (file, &len))
		{
			fprintf (stderr, __FILE__ ": fread failed #12\n");
			free(compdatasize);
			return hlpErrBadFile;
		}
		if (len >= sizeof (Page[i].desc))
		{
			fprintf (stderr, __FILE__ ": len >= sizeof (Page[%d].desc)\n", i);
			return hlpErrBadFile;
		}
		if (file->read (file, Page[i].desc, len) != len)
		{
			fprintf (stderr, __FILE__ ": fread failed #13\n");
			free(compdatasize);
			return hlpErrBadFile;
		}

		if (ocpfilehandle_read_uint32_le (file, &Page[i].size) ||
		    ocpfilehandle_read_uint32_le (file, &Page[i].lines) ||
		    ocpfilehandle_read_uint32_le (file, &compdatasize[i]))
		{
			fprintf (stderr, ": fread failed #14 / #15 / #16\n");
			free(compdatasize);
			return hlpErrBadFile;
		}
	};

	for (i=0; i<Helppages; i++)
	{
		uLongf temp=Page[i].size;
		Page[i].data=calloc(Page[i].size, 1);
		inbuf=calloc(compdatasize[i], 1);
		if (file->read (file, inbuf, compdatasize[i]) != compdatasize[i])
		{
			fprintf (stderr, ": fread failed #17\n");
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

static int doReadHelpFile (struct ocpfilehandle_t *file)
{
	uint32_t version;
	uint32_t temp;

	if (ocpfilehandle_read_uint32_le (file, &temp))
	{
		fprintf (stderr, __FILE__ ": fread failed #18\n");
		return hlpErrBadFile;
	}
	if (temp!=Helpfile_ID)
	{
		return hlpErrBadFile;
	}

	if (ocpfilehandle_read_uint32_le (file, &version))
	{
		fprintf (stderr, __FILE__ ": fread failed #19\n");
		return hlpErrBadFile;
	}

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

static char plReadHelpExternal (struct PluginInitAPI_t *API)
{
	struct ocpfile_t *f;
	struct ocpfilehandle_t *bf = 0;

	if (Page && (HelpfileErr==hlpErrOk))
		return 1;

	f = ocpdir_readdir_file (API->configAPI->DataDir, "ocp.hlp", API->dirdb);
	if (f) {
		bf = f->open (f);
		f->unref (f);
		f = 0;
	}
	if (!bf)
	{
		fprintf (stderr, "Failed to open(cfData/ocp.hlp)\n");
		HelpfileErr=hlpErrNoFile;
		return 0;
	};

	HelpfileErr=doReadHelpFile(bf);
	bf->unref (bf);

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
	char      *data;
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

	pg->rendered=calloc(80*MAX(pg->lines, 1), sizeof(uint16_t));;
	memset(pg->rendered, 0, 160*MAX(pg->lines, 1));
	memset(linebuf, 0, 160);

	data=pg->data;
	i=pg->size;

	while (i>0)
	{
		if ((uint8_t)*data<CMD_MAX)
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
				linebuf[x]=((uint8_t)*data)|(attr<<8);
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

	snprintf (strbuffer, sizeof (strbuffer), "%s-%3d%%", (HelpfileErr==hlpErrOk)?curpage->desc:"Error!", 100*plHelpScroll/MAX(plHelpHeight-plWinHeight, 1));
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
		int addx = (plScrWidth - 80) / 2;
		for (y=0; y<plWinHeight; y++)
		{
			int yp=(y+plHelpScroll)*80;
			if ((y+plHelpScroll)>=plHelpHeight)
			{
				displayvoid(y+plWinFirstLine, 0, plScrWidth);
				continue;
			}

			displayvoid (y+plWinFirstLine, 0, addx);
			if (y!=curlinky)
			{ /* display line as is, no links needs to be highlighted */
				displaystrattr(y+plWinFirstLine, 0+addx, &curpage->rendered[yp], 80);
			} else { /* there is a link that should be highlighted on the current line */
				int xp;
			        char dummystr[82];
				int i, off;

				if (curlink->posx!=0)
			        { /* print data before the link */
					displaystrattr(y+plWinFirstLine, addx, &curpage->rendered[yp], curlink->posx);
			        };

				/* highlight the link */
				for (i=0, off=yp+curlink->posx; curpage->rendered[off] & 0xff; i++, off++)
					dummystr[i]=curpage->rendered[off] & 0xff;
				dummystr[i]=0;
				displaystr(y+plWinFirstLine, curlink->posx+addx, 4, dummystr, curlink->len);

				/* print data after the link */
			        xp=curlink->posx+curlink->len;
			        displaystrattr(y+plWinFirstLine, xp+addx,
		                       &curpage->rendered[yp+xp],
		                       79-xp);

			        /* and all this just to prevent flickering. ARG! */
			}
			displayvoid (y+plWinFirstLine, 80+addx, plScrWidth-80-addx);
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

static int hlpGlobalInit (struct PluginInitAPI_t *API)
{
	helppage *pg;

	plHelpHeight=plHelpScroll=0;

	if (!plReadHelpExternal (API))
	{
		fprintf(stderr, "Warning. Failed to read help files\n");
		return errOk; /* this error is not fatal to rest of the player */
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

static void hlpGlobalClose (struct PluginCloseAPI_t *API)
{
	hlpFreePages();
}

DLLEXTINFO_CORE_PREFIX struct linkinfostruct dllextinfo = {.name = "cphelper", .desc = "OpenCP help browser (c) 1998-'25 Fabian Giesen", .ver = DLLVERSION, .sortindex = 20, .PluginInit = hlpGlobalInit, .PluginClose = hlpGlobalClose};
