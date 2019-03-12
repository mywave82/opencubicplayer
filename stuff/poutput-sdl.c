/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * SDL graphic driver
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
 */

#define _CONSOLE_DRIVER
#include "config.h"
#include <stdio.h>
#include <time.h>
#include <SDL.h>
#include "boot/psetting.h"
#include "types.h"
#include "cpiface/cpiface.h"
#include "poutput-sdl.h"
#include "boot/console.h"
#include "poutput.h"
#include "pfonts.h"
#include "stuff/framelock.h"

typedef enum
{
	MODE_320_200 = 0,
	MODE_640_400 =  1,
	MODE_640_480 =  2,
	MODE_1024_768 = 3,
	MODE_1280_1024 =  4,
	MODE_BIGGEST = 5
} mode_gui_t;

typedef struct
{
	const mode_t mode;
	int width;
	int height;
} mode_gui_data_t;

static const mode_gui_data_t mode_gui_data[] =
{
	{MODE_320_200, 320, 200},
	{MODE_640_400, 640, 400},
	{MODE_640_480, 640, 480},
	{MODE_1024_768, 1024, 768},
	{MODE_1280_1024, 1280, 1024},
	{-1, -1, -1}
};

struct FontSizeInfo_t
{
	int w, h;
};

typedef enum {
	_4x4 = 0,
	_8x8 = 1,
	_8x16 = 2,
	_FONT_MAX = 2
} FontSizeEnum;

const struct FontSizeInfo_t FontSizeInfo[] =
{
	{4, 4},
	{8, 8},
	{8, 16}
};

static FontSizeEnum plCurrentFont;

typedef struct
{
	int text_width;
	int text_height;
	mode_gui_t gui_mode;
	FontSizeEnum font;
} mode_tui_data_t;

static const mode_tui_data_t mode_tui_data[] =
{
	{  80,  25, MODE_640_400,   _8x16/*, MODE_640x400*/},
	{  80,  30, MODE_640_480,   _8x16/*, MODE_640x480*/},
	{  80,  50, MODE_640_400,   _8x8/*, MODE_640x400*/},
	{  80,  60, MODE_640_480,   _8x8/*, MODE_640x480*/},
	{ 128,  48, MODE_1024_768,  _8x16/*, MODE_1024x768*/},
	{ 160,  64, MODE_1280_1024, _8x16/*, MODE_1280x1024*/},
	{ 128,  96, MODE_1024_768,  _8x8/*, MODE_1024x768*/},
	{ 160, 128, MODE_1280_1024, _8x8/*, MODE_1280x1024*/},
	{ -1, -1, -1, -1}
};

typedef struct
{
	int is_possible;
	SDL_Rect resolution;
	Uint32 flags;
} fullscreen_info_t;

static fullscreen_info_t fullscreen_info[MODE_BIGGEST+1];

static const SDL_VideoInfo *info;

static SDL_Surface *current_surface = NULL;
static void (*set_state)(int fullscreen, int width, int height) = 0;
static int do_fullscreen = 0;
static uint8_t *vgatextram = 0;
static int plScrRowBytes = 0;
static int ekbhit(void);
static int ___valid_key(uint16_t key);
static void sdl_gflushpal(void);
static void sdl_gupdatepal(unsigned char color, unsigned char _red, unsigned char _green, unsigned char _blue);
static void displayvoid(uint16_t y, uint16_t x, uint16_t len);
static void displaystrattr(uint16_t y, uint16_t x, const uint16_t *buf, uint16_t len);
static void displaystr(uint16_t y, uint16_t x, uint8_t attr, const char *str, uint16_t len);
static void drawbar(uint16_t x, uint16_t yb, uint16_t yh, uint32_t hgt, uint32_t c);
static void idrawbar(uint16_t x, uint16_t yb, uint16_t yh, uint32_t hgt, uint32_t c);
static void setcur(uint8_t y, uint8_t x);
static void setcurshape(uint16_t shape);

#ifdef PFONT_IDRAWBAR
static uint8_t bartops[18]="\xB5\xB6\xB6\xB7\xB7\xB8\xBD\xBD\xBE\xC6\xC6\xC7\xC7\xCF\xCF\xD7\xD7";
static uint8_t ibartops[18]="\xB5\xD0\xD0\xD1\xD1\xD2\xD2\xD3\xD3\xD4\xD4\xD5\xD5\xD6\xD6\xD7\xD7";
#else
static uint8_t bartops[18]="\xB5\xB6\xB7\xB8\xBD\xBE\xC6\xC7\xCF\xD0\xD1\xD2\xD3\xD4\xD5\xD6\xD7";
#endif
static uint8_t red[256]=  {0x00, 0x00, 0x00, 0x00, 0xaa, 0xaa, 0xaa, 0xaa, 0x55, 0x55, 0x55, 0x55, 0xff, 0xff, 0xff, 0xff};
static uint8_t green[256]={0x00, 0x00, 0xaa, 0xaa, 0x00, 0x00, 0x55, 0xaa, 0x55, 0x55, 0xff, 0xff, 0x55, 0x55, 0xff, 0xff};
static uint8_t blue[256]= {0x00, 0xaa, 0x00, 0xaa, 0x00, 0xaa, 0x00, 0xaa, 0x55, 0xff, 0x55, 0xff, 0x55, 0xff, 0x55, 0xff};
static uint32_t sdl_palette[256];
static unsigned int curshape=0, curposx=0, curposy=0;
static char *virtual_framebuffer = 0;

static void set_state_textmode(int fullscreen, int width, int height)
{
#warning TODO, is this right?
	if (current_surface)
	{
		// SDL_FreeSurface(current_surface);
		current_surface = 0;
	}

	{
		static int oldwidth_fs = 0;
		static int oldheight_fs = 0;
		static int oldwidth = 0;
		static int oldheight = 0;

		if (fullscreen&&!do_fullscreen)
		{
			oldwidth = plScrLineBytes;
			oldheight = plScrLines;
			if (oldwidth_fs&&oldheight_fs)
			{
				width=oldwidth_fs;
				height=oldheight_fs;
			}
		} else if (do_fullscreen&&!fullscreen)
		{
			oldwidth_fs = plScrLineBytes;
			oldheight_fs = plScrLines;
			if (oldwidth&&oldheight)
			{
				width=oldwidth;
				height=oldheight;
			}
		}
	}
	do_fullscreen = fullscreen;

again:
	if (fullscreen && fullscreen_info[MODE_BIGGEST].is_possible)
	{
		width = fullscreen_info[MODE_BIGGEST].resolution.w;
		height = fullscreen_info[MODE_BIGGEST].resolution.h;
		current_surface = SDL_SetVideoMode(width, height, 0, fullscreen_info[MODE_BIGGEST].flags | SDL_ANYFORMAT);
	} else {
		current_surface = SDL_SetVideoMode(width, height, 0, SDL_ANYFORMAT|SDL_RESIZABLE|SDL_HWSURFACE);
		if (!current_surface)
			current_surface = SDL_SetVideoMode(width, height, 0, SDL_ANYFORMAT|SDL_RESIZABLE|SDL_SWSURFACE);
	}

	while ( ((width/FontSizeInfo[plCurrentFont].w) < 80) || ((height/FontSizeInfo[plCurrentFont].h) < 25))
	{
		if (plCurrentFont)
			plCurrentFont--;
		else {
			if (!fullscreen)
			{
				fprintf(stderr, "[SDL-video] unable to find a small enough font for %d x %d, increasing window size\n", width, height);
				width = FontSizeInfo[plCurrentFont].w * 80;
				height = FontSizeInfo[plCurrentFont].h * 25;
				goto again;
			} else {
				fprintf(stderr, "[SDL-video] unable to find a small enough font for %d x %d\n", width, height);
				exit(-1);
			}
		}
	}

	plScrWidth     = width/FontSizeInfo[plCurrentFont].w;
	plScrHeight    = height/FontSizeInfo[plCurrentFont].h;
	plScrLineBytes = width;
	plScrLines     = height;


	plScrRowBytes = plScrWidth*2;

	if (vgatextram)
	{
		free(vgatextram);
		vgatextram=0;
	}
	vgatextram = calloc (plScrHeight * 2, plScrWidth);
	if (!vgatextram)
	{
		fprintf(stderr, "[x11] calloc() failed\n");
		exit(-1);
	}

	sdl_gflushpal();

	___push_key(VIRT_KEY_RESIZE);
}

static void plSetTextMode(unsigned char x)
{
	set_state = set_state_textmode;

	___setup_key(ekbhit, ekbhit);
	_validkey=___valid_key;

	if (x==plScrMode)
	{
		memset(vgatextram, 0, plScrHeight * 2 * plScrWidth);
		return;
	}

	_plSetGraphMode(-1);

	if (x==255)
	{
		if (current_surface)
		{
#warning TODO, is this right?
			// SDL_FreeSurface(current_surface);
			current_surface = 0;
		}
		plScrMode=255;
		return; /* gdb helper */
	}

	/* if invalid mode, set it to zero */
	if (x>7)
		x=0;
#warning We have disabled textmode being able to change resolution of Screen

	plCurrentFont = mode_tui_data[x].font;

	set_state_textmode(
		do_fullscreen,
		mode_gui_data[mode_tui_data[x].gui_mode].width,
		mode_gui_data[mode_tui_data[x].gui_mode].height);

	plScrType = plScrMode = x;
}

static int cachemode = -1;

static void set_state_graphmode(int fullscreen, int width, int height)
{
	mode_gui_t mode;
	switch (cachemode)
	{
		case 13:
			plScrMode=13;
			mode = MODE_320_200;
			break;
		case 0:
			plScrMode=100;
			mode = MODE_640_480;
			break;
		case 1:
			plScrMode=101;
			mode = MODE_1024_768;
			break;
		default:
			fprintf(stderr, "[SDL-video] plSetGraphMode helper: invalid graphmode\n");
			exit(-1);
	}
	width = mode_gui_data[mode].width;
	height = mode_gui_data[mode].height;

	if (current_surface)
	{
#warning TODO, is this right?
		// SDL_FreeSurface(current_surface);
		current_surface = 0;
	}

	if (virtual_framebuffer)
	{
		free(virtual_framebuffer);
		virtual_framebuffer=0;
	}

	if ((do_fullscreen = fullscreen))
	{

		if (fullscreen_info[mode].is_possible)
			current_surface = SDL_SetVideoMode(fullscreen_info[mode].resolution.w, fullscreen_info[mode].resolution.h, 0, fullscreen_info[mode].flags | SDL_ANYFORMAT);
	}

	if (!current_surface)
	{
		do_fullscreen = 0;
		current_surface = SDL_SetVideoMode(width, height, 0, SDL_ANYFORMAT|SDL_HWSURFACE);
		if (!current_surface)
			current_surface = SDL_SetVideoMode(width, height, 0, SDL_ANYFORMAT|SDL_SWSURFACE);
	}

	plScrLineBytes=width;
	plScrLines=height;
	plScrWidth=plScrLineBytes/8;
	plScrHeight=plScrLines/16;

	plScrRowBytes=plScrWidth*2;
	if (vgatextram)
	{
		free(vgatextram);
		vgatextram=0;
	}
	vgatextram = calloc (plScrHeight * 2, plScrWidth);
	if (!vgatextram)
	{
		fprintf(stderr, "[x11] calloc() failed\n");
		exit(-1);
	}

	virtual_framebuffer=calloc(plScrLineBytes, plScrLines);
	plVidMem=virtual_framebuffer;

	if (virtual_framebuffer)
		memset(virtual_framebuffer, 0, plScrLineBytes * plScrLines);

	sdl_gflushpal();

	___push_key(VIRT_KEY_RESIZE);
	return;
}

static int __plSetGraphMode(int high)
{
	if (high>=0)
		set_state = set_state_graphmode;

	if ((high==cachemode)&&(high>=0))
		goto quick;
	cachemode=high;

	if (virtual_framebuffer)
	{
		free(virtual_framebuffer);
		virtual_framebuffer=0;
	}

	if (high<0)
		return 0;

	___setup_key(ekbhit, ekbhit);
	_validkey=___valid_key;

	set_state_graphmode(do_fullscreen, 0, 0);

quick:
	if (virtual_framebuffer)
		memset(virtual_framebuffer, 0, plScrLineBytes * plScrLines);

	sdl_gflushpal();
	return 0;
}

static void __vga13(void)
{
	_plSetGraphMode(13);
}

static void FindFullscreenModes_SDL(Uint32 flags)
{
	SDL_Rect** modes;
	int i, j;

	/* Get available fullscreen/hardware modes */
	modes = SDL_ListModes(NULL, flags);

	if (modes == (SDL_Rect**)0)
	{
		fprintf(stderr, "[SDL video] No modes available!\n");
		return;
	}

	if (modes == (SDL_Rect**)-1)
	{
		fprintf(stderr, "[SDL video] All resolutions available, wierd\n");
	} else{
		for (i=0; modes[i]; ++i)
		{
#ifdef SDL_DEBUG
			fprintf(stderr, "[SDL video] have %d x %d\n", modes[i]->w, modes[i]->h);
#endif
			for (j=0;j<MODE_BIGGEST;j++)
			{
				if ((modes[i]->w<mode_gui_data[j].width) || (modes[i]->h<mode_gui_data[j].height))
					continue;
				if (fullscreen_info[j].is_possible)
				{
					if ( (fullscreen_info[j].resolution.w < modes[i]->w) || (fullscreen_info[j].resolution.h < modes[i]->h) )
						continue;
					if ( (fullscreen_info[j].resolution.w == modes[i]->w) && (fullscreen_info[j].resolution.h == modes[i]->h) )
						continue;
				}
				fullscreen_info[j].is_possible = 1;
				fullscreen_info[j].resolution = modes[i][0];
				fullscreen_info[j].flags = flags;
			}

			if (fullscreen_info[MODE_BIGGEST].is_possible)
				if ( (fullscreen_info[MODE_BIGGEST].resolution.w >= modes[i]->w) || (fullscreen_info[MODE_BIGGEST].resolution.h >= modes[i]->h) )
					continue;
			fullscreen_info[MODE_BIGGEST].is_possible = 1;
			fullscreen_info[MODE_BIGGEST].resolution = modes[i][0];
			fullscreen_info[MODE_BIGGEST].flags = flags;
		}
	}

	plVidType=vidNorm;

	if ((fullscreen_info[MODE_BIGGEST].resolution.w>=1024) && (fullscreen_info[MODE_BIGGEST].resolution.h>=768))
		plVidType=vidVESA;
}

static int conRestore(void)
{
	return 0;
}
static void conSave(void)
{
}

static void plDisplaySetupTextMode(void)
{
	while (1)
	{
		uint16_t c;

		memset(vgatextram, 0, plScrHeight * 2 * plScrWidth);

		make_title("sdl-driver setup");
		displaystr(1, 0, 0x07, "1:  font-size:", 14);
		displaystr(1, 15, plCurrentFont == _4x4 ? 0x0f : 0x07 , "4x4", 3);
		displaystr(1, 19, plCurrentFont == _8x8 ? 0x0f : 0x07, "8x8", 3);
		displaystr(1, 23, plCurrentFont == _8x16 ? 0x0f : 0x07, "8x16", 4);
/*
		displaystr(2, 0, 0x07, "2:  fullscreen: ", 16);
		displaystr(3, 0, 0x07, "3:  resolution in fullscreen:", 29);*/

		displaystr(plScrHeight-1, 0, 0x17, "  press the number of the item you wish to change and ESC when done", plScrWidth);

		while (!_ekbhit())
				framelock();
		c=_egetch();

		switch (c)
		{
			case '1':
				/* we can assume that we are in text-mode if we are here */
				plCurrentFont = (plCurrentFont+1)%3;
				set_state_textmode(do_fullscreen, plScrLineBytes, plScrLines);
				break;
			case 27: return;
		}
	}
}

static const char *plGetDisplayTextModeName(void)
{
	static char mode[32];
	snprintf(mode, sizeof(mode), "res(%dx%d), font(%s)%s", plScrWidth, plScrHeight,
		plCurrentFont == _4x4 ? "4x4"
		: plCurrentFont == _8x8 ? "8x8" : "8x16", do_fullscreen?" fullscreen":"");
	return mode;
}

#ifdef SDL_DEBUG

static void Dump_SDL_VideoInfo(const SDL_VideoInfo *info)
{
	fprintf(stderr, "hw_available: %d\n", (int)info->hw_available);
	fprintf(stderr, "wm_available: %d\n", (int)info->wm_available);
	fprintf(stderr, "blit_hw: %d\n", (int)info->blit_hw);
	fprintf(stderr, "blit_hw_CC: %d\n", (int)info->blit_hw_CC);
	fprintf(stderr, "blit_hw_A: %d\n", (int)info->blit_hw_A);
	fprintf(stderr, "blit_sw: %d\n", (int)info->blit_sw);
	fprintf(stderr, "blit_sw_CC: %d\n", (int)info->blit_sw_CC);
	fprintf(stderr, "blit_sw_A: %d\n", (int)info->blit_sw_A);
	fprintf(stderr, "blit_fill: %d\n", (int)info->blit_fill);
	fprintf(stderr, "video_mem: %d\n", (int)info->video_mem);
	fprintf(stderr, "vfmt->palette: %p", info->vfmt->palette);
	fprintf(stderr, "vfmt->BitsPerPixel: %d\n", (int)info->vfmt->BitsPerPixel);
	fprintf(stderr, "vfmt->BytesPerPixel: %d\n", (int)info->vfmt->BytesPerPixel);
	fprintf(stderr, "vfmt->Rloss: %d\n", (int)info->vfmt->Rloss);
	fprintf(stderr, "vfmt->Gloss: %d\n", (int)info->vfmt->Gloss);
	fprintf(stderr, "vfmt->Bloss: %d\n", (int)info->vfmt->Bloss);
	fprintf(stderr, "vfmt->Aloss: %d\n", (int)info->vfmt->Aloss);
	fprintf(stderr, "vfmt->Rshift: %d\n", (int)info->vfmt->Rshift);
	fprintf(stderr, "vfmt->Gshift: %d\n", (int)info->vfmt->Gshift);
	fprintf(stderr, "vfmt->Bshift: %d\n", (int)info->vfmt->Bshift);
	fprintf(stderr, "vfmt->Ashift: %d\n", (int)info->vfmt->Ashift);
	fprintf(stderr, "vfmt->Rmask: %d\n", (int)info->vfmt->Rmask);
	fprintf(stderr, "vfmt->Gmask: %d\n", (int)info->vfmt->Gmask);
	fprintf(stderr, "vfmt->Bmask: %d\n", (int)info->vfmt->Bmask);
	fprintf(stderr, "vfmt->Amask: %d\n", (int)info->vfmt->Amask);
	fprintf(stderr, "vfmt->colorkey: %d\n", (int)info->vfmt->colorkey);
	fprintf(stderr, "vfmt->alpfa: %d\n", (int)info->vfmt->alpha);
	fprintf(stderr, "current_w: %d\n", (int)info->current_w);
	fprintf(stderr, "current_h: %d\n", (int)info->current_h);
}

#endif

static int need_quit = 0;

int sdl_init(void)
{
	if ( SDL_Init(/*SDL_INIT_AUDIO|*/SDL_INIT_VIDEO) < 0 )
	{
		fprintf(stderr, "[SDL video] Unable to init SDL: %s\n", SDL_GetError());
		return 1;
	}

	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

	plCurrentFont = cfGetProfileInt("x11", "font", _8x16, 10);
	if (plCurrentFont > _FONT_MAX)
		plCurrentFont = _8x16;

#ifdef SDL_DEBUG
	{
		char buffer[256];
		if (!SDL_VideoDriverName(buffer, sizeof(buffer)))
			fprintf(stderr, "[SDL video] Unable to retrieve the SDL video driver name: %s\n", SDL_GetError());
		else
			fprintf(stderr, "[SDL-video] Video driver name: %s\n", buffer);
	}
#endif

	info = SDL_GetVideoInfo();
	if (!info)
	{
		fprintf(stderr, "[SDL video] Unable to retrieve video info: %s\n", SDL_GetError());
		SDL_Quit();
		return 1;
	}
#ifdef SDL_DEBUG
	fprintf(stderr, "[SDL video] Video-info:\n");
	Dump_SDL_VideoInfo(info);
#endif


#ifdef SDL_DEBUG
	fprintf(stderr, "[SDL video] check SDL_FULLSCREEN|SDL_HWSURFACE:\n");
#endif
	FindFullscreenModes_SDL(SDL_FULLSCREEN|SDL_HWSURFACE);
#ifdef SDL_DEBUG
	fprintf(stderr, "[SDL video] check SDL_FULLSCREEN:\n");
#endif
	FindFullscreenModes_SDL(SDL_FULLSCREEN);

#ifdef SDL_DEBUG
	{
		int i;
		for (i=0;i<MODE_BIGGEST;i++)
		{
			fprintf(stderr, "[SDL video] has_%d_%d_fullscreen: %d\n", mode_gui_data[i].width, mode_gui_data[i].height, fullscreen_info[i].is_possible);
			if (fullscreen_info[i].is_possible)
				fprintf(stderr, "[SDL video] %d_%d_fullscreen: %d x %d (0x%x)\n", mode_gui_data[i].width, mode_gui_data[i].height, fullscreen_info[i].resolution.w, fullscreen_info[i].resolution.h, (int)fullscreen_info[i].flags);
		}
	}
	fprintf(stderr, "[SDL video] has_text_fullscreen: %d\n", fullscreen_info[MODE_BIGGEST].is_possible);
	if (fullscreen_info[MODE_BIGGEST].is_possible)
		fprintf(stderr, "[SDL video] text_fullscreen: %d x %d (0x%x)\n", fullscreen_info[MODE_BIGGEST].resolution.w, fullscreen_info[MODE_BIGGEST].resolution.h, (int)fullscreen_info[MODE_BIGGEST].flags);
#endif

	if (!fullscreen_info[MODE_BIGGEST].is_possible)
		fprintf(stderr, "[SDL video] Unable to find a fullscreen mode\n");

	need_quit = 1;

	_plSetTextMode=plSetTextMode;
	_plSetGraphMode=__plSetGraphMode;
	_gdrawstr=generic_gdrawstr;
	_gdrawchar8=generic_gdrawchar8;
	_gdrawchar8p=generic_gdrawchar8p;
	_gdrawchar8t=generic_gdrawchar8t;
	_gdrawcharp=generic_gdrawcharp;
	_gdrawchar=generic_gdrawchar;
	_gupdatestr=generic_gupdatestr;
	_gupdatepal=sdl_gupdatepal;
	_gflushpal=sdl_gflushpal;
	_vga13=__vga13;

	_displayvoid=displayvoid;
	_displaystrattr=displaystrattr;
	_displaystr=displaystr;
	_drawbar=drawbar;
#ifdef PFONT_IDRAWBAR
	_idrawbar=idrawbar;
#else
	_idrawbar=drawbar;
#endif
	_setcur=setcur;
	_setcurshape=setcurshape;
	_conRestore=conRestore;
	_conSave=conSave;

	_plGetDisplayTextModeName = plGetDisplayTextModeName;
	_plDisplaySetupTextMode = plDisplaySetupTextMode;

	return 0;
}

void sdl_done(void)
{
	if (!need_quit)
		return;

	SDL_Quit();

	if (vgatextram)
	{
		free(vgatextram);
		vgatextram=0;
	}

	need_quit = 0;
}

struct keytranslate_t
{
	SDLKey SDL;
	uint16_t OCP;
};

struct keytranslate_t translate[] =
{
	{SDLK_BACKSPACE,    KEY_BACKSPACE},
	{SDLK_TAB,          KEY_TAB},
	{SDLK_CLEAR,        KEY_DELETE}, /* ??? */
	{SDLK_RETURN,       _KEY_ENTER},
	/*SDLK_PAUSE*/
	{SDLK_ESCAPE,       KEY_ESC},
	{SDLK_SPACE,        ' '},
	{SDLK_EXCLAIM,      '!'},
	{SDLK_QUOTEDBL,     '\"'},
	{SDLK_HASH,         '#'},
	{SDLK_DOLLAR,       '$'},
	{SDLK_AMPERSAND,    '&'},
	{SDLK_QUOTE,        '\''},
	{SDLK_LEFTPAREN,    '('},
	{SDLK_RIGHTPAREN,   ')'},
	{SDLK_ASTERISK,     '*'},
	{SDLK_PLUS,         '+'},
	{SDLK_COMMA,        ','},
	{SDLK_MINUS,        '-'},
	{SDLK_PERIOD,       '.'},
	{SDLK_SLASH,        '/'},
	{SDLK_0,            '0'},
	{SDLK_1,            '1'},
	{SDLK_2,            '2'},
	{SDLK_3,            '3'},
	{SDLK_4,            '4'},
	{SDLK_5,            '5'},
	{SDLK_6,            '6'},
	{SDLK_7,            '7'},
	{SDLK_8,            '8'},
	{SDLK_9,            '9'},
	{SDLK_COLON,        ':'},
	{SDLK_SEMICOLON,    ';'},
	{SDLK_LESS,         '<'},
	{SDLK_EQUALS,       '='},
	{SDLK_GREATER,      '>'},
	{SDLK_QUESTION,     '?'},
	{SDLK_AT,           '@'},
	{65,                'A'},
	{66,                'B'},
	{67,                'C'},
	{68,                'D'},
	{69,                'E'},
	{70,                'F'},
	{71,                'G'},
	{72,                'H'},
	{73,                'I'},
	{74,                'J'},
	{75,                'K'},
	{76,                'L'},
	{77,                'M'},
	{78,                'N'},
	{79,                'O'},
	{80,                'P'},
	{81,                'Q'},
	{82,                'R'},
	{83,                'S'},
	{84,                'T'},
	{85,                'U'},
	{86,                'V'},
	{87,                'W'},
	{88,                'X'},
	{89,                'Y'},
	{90,                'Z'},
	{'|',               '|'},
	{SDLK_LEFTBRACKET,  '['},
	{SDLK_BACKSLASH,    '\\'},
	{SDLK_RIGHTBRACKET, ']'},
	{SDLK_CARET,        '^'},
	{SDLK_UNDERSCORE,   '_'},
	{SDLK_BACKQUOTE,    '`'},
	{SDLK_a,            'a'},
	{SDLK_b,            'b'},
	{SDLK_c,            'c'},
	{SDLK_d,            'd'},
	{SDLK_e,            'e'},
	{SDLK_f,            'f'},
	{SDLK_g,            'g'},
	{SDLK_h,            'h'},
	{SDLK_i,            'i'},
	{SDLK_j,            'j'},
	{SDLK_k,            'k'},
	{SDLK_l,            'l'},
	{SDLK_m,            'm'},
	{SDLK_n,            'n'},
	{SDLK_o,            'o'},
	{SDLK_p,            'p'},
	{SDLK_q,            'q'},
	{SDLK_r,            'r'},
	{SDLK_s,            's'},
	{SDLK_t,            't'},
	{SDLK_u,            'u'},
	{SDLK_v,            'v'},
	{SDLK_w,            'w'},
	{SDLK_x,            'x'},
	{SDLK_y,            'y'},
	{SDLK_z,            'z'},
	{SDLK_DELETE,       KEY_DELETE},
	{SDLK_KP0,          '0'},
	{SDLK_KP1,          '1'},
	{SDLK_KP2,          '2'},
	{SDLK_KP3,          '3'},
	{SDLK_KP4,          '4'},
	{SDLK_KP5,          '5'},
	{SDLK_KP6,          '6'},
	{SDLK_KP7,          '7'},
	{SDLK_KP8,          '8'},
	{SDLK_KP9,          '9'},
	{SDLK_KP_PERIOD,    ','},
	{SDLK_KP_DIVIDE,    '/'},
	{SDLK_KP_MULTIPLY,  '*'},
	{SDLK_KP_MINUS,     '-'},
	{SDLK_KP_PLUS,      '+'},
	{SDLK_KP_ENTER,     _KEY_ENTER},
	{SDLK_KP_EQUALS,    '='},
	{SDLK_UP,           KEY_UP},
	{SDLK_DOWN,         KEY_DOWN},
	{SDLK_RIGHT,        KEY_RIGHT},
	{SDLK_LEFT,         KEY_LEFT},
	{SDLK_INSERT,       KEY_INSERT},
	{SDLK_HOME,         KEY_HOME},
	{SDLK_END,          KEY_END},
	{SDLK_PAGEUP,       KEY_PPAGE},
	{SDLK_PAGEDOWN,     KEY_NPAGE},
	{SDLK_F1,           KEY_F(1)},
	{SDLK_F2,           KEY_F(2)},
	{SDLK_F3,           KEY_F(3)},
	{SDLK_F4,           KEY_F(4)},
	{SDLK_F5,           KEY_F(5)},
	{SDLK_F6,           KEY_F(6)},
	{SDLK_F7,           KEY_F(7)},
	{SDLK_F8,           KEY_F(8)},
	{SDLK_F9,           KEY_F(9)},
	{SDLK_F10,          KEY_F(10)},
	{SDLK_F11,          KEY_F(11)},
	{SDLK_F12,          KEY_F(12)},
	/*SDLK_F13*/
	/*SDLK_F14*/
	/*SDLK_F15*/
	{-1,                0xffff},
};

struct keytranslate_t translate_shift[] =
{
	{SDLK_TAB,          KEY_SHIFT_TAB},
	{SDLK_a,            'A'},
	{SDLK_b,            'B'},
	{SDLK_c,            'C'},
	{SDLK_d,            'D'},
	{SDLK_e,            'E'},
	{SDLK_f,            'F'},
	{SDLK_g,            'G'},
	{SDLK_h,            'H'},
	{SDLK_i,            'I'},
	{SDLK_j,            'J'},
	{SDLK_k,            'K'},
	{SDLK_l,            'L'},
	{SDLK_m,            'M'},
	{SDLK_n,            'N'},
	{SDLK_o,            'O'},
	{SDLK_p,            'P'},
	{SDLK_q,            'Q'},
	{SDLK_r,            'R'},
	{SDLK_s,            'S'},
	{SDLK_t,            'T'},
	{SDLK_u,            'U'},
	{SDLK_v,            'V'},
	{SDLK_w,            'W'},
	{SDLK_x,            'X'},
	{SDLK_y,            'Y'},
	{SDLK_z,            'Z'},
	{-1,                0xffff},
};

struct keytranslate_t translate_ctrl[] =
{
	{SDLK_BACKSPACE,    KEY_CTRL_BS},
	{SDLK_RETURN,       KEY_CTRL_ENTER},
	/*{65,                'A'},
	{66,                'B'},
	{67,                'C'},*/
	{68,                KEY_CTRL_D},
	/*{69,                'E'},
	{70,                'F'},
	{71,                'G'},*/
	{72,                KEY_CTRL_H},
	/*{73,                'I'},*/
	{74,                KEY_CTRL_J},
	{75,                KEY_CTRL_K},
	{76,                KEY_CTRL_L},
	/*{77,                'M'},
	{78,                'N'},
	{79,                'O'},*/
	{80,                KEY_CTRL_P},
	{81,                KEY_CTRL_Q},
	/*{82,                'R'},*/
	{83,                KEY_CTRL_S},
	/*{84,                'T'},
	{85,                'U'},
	{86,                'V'},
	{87,                'W'},
	{88,                'X'},
	{89,                'Y'},*/
	{90,                KEY_CTRL_Z},
	/*{SDLK_a,            'a'},
	{SDLK_b,            'b'},
	{SDLK_c,            'c'},*/
	{SDLK_d,            KEY_CTRL_D},
	/*{SDLK_e,            'e'},
	{SDLK_f,            'f'},
	{SDLK_g,            'g'},*/
	{SDLK_h,            KEY_CTRL_H},
	/*{SDLK_i,            'i'},*/
	{SDLK_j,            KEY_CTRL_J},
	{SDLK_k,            KEY_CTRL_K},
	{SDLK_l,            KEY_CTRL_L},
	/*{SDLK_m,            'm'},
	{SDLK_n,            'n'},
	{SDLK_o,            'o'},*/
	{SDLK_p,            KEY_CTRL_P},
	{SDLK_q,            KEY_CTRL_Q},
	/*{SDLK_r,            'r'},*/
	{SDLK_s,            KEY_CTRL_S},
	/*{SDLK_t,            't'},
	{SDLK_u,            'u'},
	{SDLK_v,            'v'},
	{SDLK_w,            'w'},
	{SDLK_x,            'x'},
	{SDLK_y,            'y'},*/
	{SDLK_z,            KEY_CTRL_Z},
	{SDLK_KP_ENTER,     KEY_CTRL_ENTER},
	{SDLK_UP,           KEY_CTRL_UP},
	{SDLK_DOWN,         KEY_CTRL_DOWN},
	{SDLK_RIGHT,        KEY_CTRL_RIGHT},
	{SDLK_LEFT,         KEY_CTRL_LEFT},
	{SDLK_PAGEUP,       KEY_CTRL_PGUP},
	{SDLK_PAGEDOWN,     KEY_CTRL_PGDN},
	{-1,                0xffff},
};

struct keytranslate_t translate_alt[] =
{
	{65,                KEY_ALT_A},
	{66,                KEY_ALT_B},
	{67,                KEY_ALT_C},
	/*{68,                'D'},*/
	{69,                KEY_ALT_E},
	/*{70,                'F'},*/
	{71,                KEY_ALT_G},
	/*{72,                'H'},*/
	{73,                KEY_ALT_I},
	/*{74,                'J'},*/
	{75,                KEY_ALT_K},
	{76,                KEY_ALT_L},
	{77,                KEY_ALT_M},
	/*{78,                'N'},*/
	{79,                KEY_ALT_O},
	{80,                KEY_ALT_P},
	/*{81,                'Q'},*/
	{82,                KEY_ALT_R},
	{83,                KEY_ALT_S},
	/*{84,                'T'},
	{85,                'U'},
	{86,                'V'},
	{87,                'W'},*/
	{88,                KEY_ALT_X},
	/*{89,                'Y'},*/
	{90,                KEY_ALT_Z},
	{SDLK_a,            KEY_ALT_A},
	{SDLK_b,            KEY_ALT_B},
	{SDLK_c,            KEY_ALT_C},
	/*{SDLK_d,            'd'},*/
	{SDLK_e,            KEY_ALT_E},
	/*{SDLK_f,            'f'},*/
	{SDLK_g,            KEY_ALT_G},
	/*{SDLK_h,            'h'},*/
	{SDLK_i,            KEY_ALT_I},
	/*{SDLK_j,            'j'},*/
	{SDLK_k,            KEY_ALT_K},
	{SDLK_l,            KEY_ALT_L},
	{SDLK_m,            KEY_ALT_M},
	/*{SDLK_n,            'n'},*/
	{SDLK_o,            KEY_ALT_O},
	{SDLK_p,            KEY_ALT_P},
	/*{SDLK_q,            'q'},*/
	{SDLK_r,            KEY_ALT_R},
	{SDLK_s,            KEY_ALT_S},
	/*{SDLK_t,            't'},
	{SDLK_u,            'u'},
	{SDLK_v,            'v'},
	{SDLK_w,            'w'},*/
	{SDLK_x,            KEY_ALT_X},
	/*{SDLK_y,            'y'},*/
	{SDLK_z,            KEY_ALT_Z},
	{-1,                0xffff},
};

static int ___valid_key(uint16_t key)
{
	int index;

	if (key==KEY_ALT_ENTER)
		return 0;

	for (index=0;translate[index].OCP!=0xffff;index++)
		if (translate[index].OCP==key)
			return 1;
	for (index=0;translate_shift[index].OCP!=0xffff;index++)
		if (translate_shift[index].OCP==key)
			return 1;
	for (index=0;translate_ctrl[index].OCP!=0xffff;index++)
		if (translate_ctrl[index].OCP==key)
			return 1;
	for (index=0;translate_alt[index].OCP!=0xffff;index++)
		if (translate_alt[index].OCP==key)
			return 1;

	fprintf(stderr, __FILE__ ": unknown key 0x%04x\n", (int)key);
	return 0;
}

static void RefreshScreenText(void)
{
	unsigned int x, y;
	uint8_t *mem=vgatextram;
	int doshape=0;
	uint8_t save=save;
	int precalc_linelength;

	if (!current_surface)
		return;

	if (curshape)
	if (time(NULL)&1)
		doshape=curshape;

	if (doshape==2)
	{
		save=vgatextram[curposy*plScrRowBytes+curposx*2];
		vgatextram[curposy*plScrRowBytes+curposx*2]=219;
	}

	SDL_LockSurface(current_surface);

	switch (current_surface->format->BytesPerPixel)
	{
		default:
			fprintf(stderr, "[SDL-video]: bytes-per-pixel: %d (TODO)\n", current_surface->format->BytesPerPixel);
			exit(-1);

#define BLOCK8x16(BASETYPE) \
				precalc_linelength = current_surface->pitch/sizeof(BASETYPE); \
				for (y=0;y<plScrHeight;y++) \
				{ \
					BASETYPE *scr_precalc = ((BASETYPE *)current_surface->pixels)+y*16*precalc_linelength; \
					for (x=0;x<plScrWidth;x++) \
					{ \
						uint8_t a, *cp; \
						BASETYPE f, b; \
						BASETYPE *scr=scr_precalc; \
						int i, j; \
						scr_precalc += 8; \
						cp=plFont816[*(mem++)]; \
						a=*(mem++); \
						f=sdl_palette[a&15]; \
						b=sdl_palette[a>>4]; \
						for (i=0; i<16; i++) \
						{ \
							uint8_t bitmap=*cp++; \
							for (j=0; j<8; j++) \
							{ \
								*scr++=(bitmap&128)?f:b; \
								bitmap<<=1; \
							} \
							scr+=precalc_linelength-8; \
						} \
						if ((doshape==1)&&(curposy==y)&&(curposx==x)) \
						{ \
							cp=plFont816['_']+15; \
							scr+=8; \
							for (i=0; i<16; i++) \
							{ \
								uint8_t bitmap=*cp--; \
								scr-=precalc_linelength+8; \
								for (j=0; j<8; j++) \
								{ \
									if (bitmap&1) \
										*scr=f; \
									bitmap>>=1; \
									scr++; \
								} \
							} \
						} \
					} \
				} \

#define BLOCK8x8(BASETYPE) \
				precalc_linelength = current_surface->pitch/sizeof(BASETYPE); \
				for (y=0;y<plScrHeight;y++) \
				{ \
					BASETYPE *scr_precalc = ((BASETYPE *)current_surface->pixels)+y*8*precalc_linelength; \
					for (x=0;x<plScrWidth;x++) \
					{ \
						uint8_t a, *cp; \
						BASETYPE f, b; \
						BASETYPE *scr=scr_precalc; \
						int i, j; \
						scr_precalc += 8; \
						cp=plFont88[*(mem++)]; \
						a=*(mem++); \
						f=sdl_palette[a&15]; \
						b=sdl_palette[a>>4]; \
						for (i=0; i<8; i++) \
						{ \
							uint8_t bitmap=*cp++; \
							for (j=0; j<8; j++) \
							{ \
								*scr++=(bitmap&128)?f:b; \
								bitmap<<=1; \
							} \
							scr+=precalc_linelength-8; \
						} \
						if ((doshape==1)&&(curposy==y)&&(curposx==x)) \
						{ \
							cp=plFont88['_']+7; \
							scr+=8; \
							for (i=0; i<8; i++) \
							{ \
								uint8_t bitmap=*cp--; \
								scr-=precalc_linelength+8; \
								for (j=0; j<8; j++) \
								{ \
									if (bitmap&1) \
										*scr=f; \
									bitmap>>=1; \
									scr++; \
								} \
							} \
						} \
					}\
				}

#define BLOCK4x4(BASETYPE) \
				precalc_linelength = current_surface->pitch/sizeof(BASETYPE); \
				for (y=0;y<plScrHeight;y++) \
				{ \
					BASETYPE *scr_precalc = ((BASETYPE *)current_surface->pixels)+y*4*precalc_linelength; \
					for (x=0;x<plScrWidth;x++) \
					{ \
						uint8_t a, *cp; \
						BASETYPE f, b; \
						BASETYPE *scr=scr_precalc; \
						int i, j; \
						scr_precalc += 4; \
						cp=plFont44[*(mem++)]; \
						a=*(mem++); \
						f=sdl_palette[a&15]; \
						b=sdl_palette[a>>4]; \
						for (i=0; i<2; i++) \
						{ \
							uint8_t bitmap=*cp++; \
							for (j=0; j<4; j++) \
							{ \
								*scr++=(bitmap&128)?f:b; \
								bitmap<<=1; \
							} \
							scr+=precalc_linelength-4; \
							for (j=0; j<4; j++) \
							{ \
								*scr++=(bitmap&128)?f:b; \
								bitmap<<=1; \
							} \
							scr+=precalc_linelength-4; \
						} \
						if ((doshape==1)&&(curposy==y)&&(curposx==x)) \
						{ \
							cp=plFont44['_']+1; \
							scr+=4; \
							for (i=0; i<2; i++) \
							{ \
								uint8_t bitmap=*cp--; \
								scr-=precalc_linelength+4; \
								for (j=0; j<4; j++) \
								{ \
									if (bitmap&1) \
										*scr=f; \
									bitmap>>=1; \
									scr++; \
								} \
								scr-=precalc_linelength+4; \
								for (j=0; j<4; j++) \
								{ \
									if (bitmap&1) \
										*scr=f; \
									bitmap>>=1; \
									scr++; \
								} \
							} \
						} \
					} \
				}

		case 1:
			switch (plCurrentFont)
			{
			case _8x16:
				BLOCK8x16(uint8_t)
				break;
			case _8x8:
				BLOCK8x8(uint8_t)
				break;
			case _4x4:
				BLOCK4x4(uint8_t)
				break;
			}
			break;
		case 2:
			switch (plCurrentFont)
			{
			case _8x16:
				BLOCK8x16(uint16_t)
				break;
			case _8x8:
				BLOCK8x8(uint16_t)
				break;
			case _4x4:
				BLOCK4x4(uint16_t)
				break;
			}
			break;
		case 4:
			switch (plCurrentFont)
			{
			case _8x16:
				BLOCK8x16(uint32_t)
				break;
			case _8x8:
				BLOCK8x8(uint32_t)
				break;
			case _4x4:
				BLOCK4x4(uint32_t)
				break;
			}
			break;

		case 3: /* case 3 is special */
			switch (plCurrentFont)
			{
			case _8x16:
				precalc_linelength = current_surface->pitch/sizeof(uint8_t);
				for (y=0;y<plScrHeight;y++)
				{
					uint8_t *scr_precalc = ((uint8_t *)current_surface->pixels)+y*16*precalc_linelength;
					for (x=0;x<plScrWidth;x++)
					{
						uint8_t a, *cp;
						uint8_t fr, fg, fb, br, bg, bb;
						uint8_t *scr=scr_precalc;
						int i, j;
						scr_precalc += 24;
						cp=plFont816[*(mem++)];
						a=*(mem++);
						fr=sdl_palette[a&15] & 266;
						fg=(sdl_palette[a&15]>>8) & 255;
						fb=(sdl_palette[a&15]>>16) & 255;
						br=sdl_palette[a>>4] & 255;
						bg=(sdl_palette[a>>4]>>8) & 255;
						bb=(sdl_palette[a>>4]>>16) & 255;
						for (i=0; i<16; i++)
						{
							uint8_t bitmap=*cp++;
							for (j=0; j<8; j++)
							{
								if (bitmap&128)
								{
									*scr++=fr;
									*scr++=fg;
									*scr++=fb;
								} else {
									*scr++=br;
									*scr++=bg;
									*scr++=bb;
								}
								bitmap<<=1;
							}
							scr+=precalc_linelength-24;
						}
						if ((doshape==1)&&(curposy==y)&&(curposx==x))
						{
							cp=plFont816['_']+15;
							scr+=24;
							for (i=0; i<16; i++)
							{
								uint8_t bitmap=*cp--;
								scr-=precalc_linelength+24;
								for (j=0; j<8; j++)
								{
									if (bitmap&1)
									{
										*scr++=fr;
										*scr++=fg;
										*scr++=fb;
									} else
										scr+=3;
									bitmap>>=1;
								}
							}
						}
					}
				}
				break;
			case _8x8:
				precalc_linelength = current_surface->pitch/sizeof(uint8_t);
				for (y=0;y<plScrHeight;y++)
				{
					uint8_t *scr_precalc = ((uint8_t *)current_surface->pixels)+y*8*precalc_linelength;
					for (x=0;x<plScrWidth;x++)
					{
						uint8_t a, *cp;
						uint8_t fr, fg, fb, br, bg, bb;
						uint8_t *scr=scr_precalc;
						int i, j;
						scr_precalc += 24;
						cp=plFont88[*(mem++)];
						a=*(mem++);
						fr=sdl_palette[a&15] & 266;
						fg=(sdl_palette[a&15]>>8) & 255;
						fb=(sdl_palette[a&15]>>16) & 255;
						br=sdl_palette[a>>4] & 255;
						bg=(sdl_palette[a>>4]>>8) & 255;
						bb=(sdl_palette[a>>4]>>16) & 255;
						for (i=0; i<8; i++)
						{
							uint8_t bitmap=*cp++;
							for (j=0; j<8; j++)
							{
								if (bitmap&128)
								{
									*scr++=fr;
									*scr++=fg;
									*scr++=fb;
								} else {
									*scr++=br;
									*scr++=bg;
									*scr++=bb;
								}
								bitmap<<=1;
							}
							scr+=precalc_linelength-24;
						}
						if ((doshape==1)&&(curposy==y)&&(curposx==x))
						{
							cp=plFont88['_']+7;
							scr+=24;
							for (i=0; i<8; i++)
							{
								uint8_t bitmap=*cp--;
								scr-=precalc_linelength+24;
								for (j=0; j<8; j++)
								{
									if (bitmap&1)
									{
										*scr++=fr;
										*scr++=fg;
										*scr++=fb;
									} else
										scr+=3;
									bitmap>>=1;
								}
							}
						}
					}
				}
				break;
			case _4x4:
				precalc_linelength = current_surface->pitch/sizeof(uint8_t);
				for (y=0;y<plScrHeight;y++)
				{
					uint8_t *scr_precalc = ((uint8_t *)current_surface->pixels)+y*4*precalc_linelength;
					for (x=0;x<plScrWidth;x++)
					{
						uint8_t a, *cp;
						uint8_t fr, fg, fb, br, bg, bb;
						uint8_t *scr=scr_precalc;
						int i, j;
						scr_precalc += 12;
						cp=plFont44[*(mem++)];
						a=*(mem++);
						fr=sdl_palette[a&15] & 266;
						fg=(sdl_palette[a&15]>>8) & 255;
						fb=(sdl_palette[a&15]>>16) & 255;
						br=sdl_palette[a>>4] & 255;
						bg=(sdl_palette[a>>4]>>8) & 255;
						bb=(sdl_palette[a>>4]>>16) & 255;
						for (i=0; i<2; i++)
						{
							uint8_t bitmap=*cp++;
							for (j=0; j<4; j++)
							{
								if (bitmap&128)
								{
									*scr++=fr;
									*scr++=fg;
									*scr++=fb;
								} else {
									*scr++=br;
									*scr++=bg;
									*scr++=bb;
								}
								bitmap<<=1;
							}
							scr+=precalc_linelength-12;
							for (j=0; j<4; j++)
							{
								if (bitmap&128)
								{
									*scr++=fr;
									*scr++=fg;
									*scr++=fb;
								} else {
									*scr++=br;
									*scr++=bg;
									*scr++=bb;
								}
								bitmap<<=1;
							}
							scr+=precalc_linelength-12;
						}
						if ((doshape==1)&&(curposy==y)&&(curposx==x))
						{
							cp=plFont44['_']+1;
							scr+=4;
							for (i=0; i<2; i++)
							{
								uint8_t bitmap=*cp--;
								scr-=precalc_linelength+12;
								for (j=0; j<4; j++)
								{
									if (bitmap&1)
									{
										*scr++=fr;
										*scr++=fg;
										*scr++=fb;
									} else
										scr+=3;
									bitmap>>=1;
								}
								scr-=precalc_linelength+12;
								for (j=0; j<4; j++)
								{
									if (bitmap&1)
									{
										*scr++=fr;
										*scr++=fg;
										*scr++=fb;
									} else
										scr+=3;
									bitmap>>=1;
								}
							}
						}
					}
				}
				break;
			}
			break;
	}

	if (doshape==2)
		vgatextram[curposy*plScrRowBytes+curposx*2]=save;

	SDL_UnlockSurface(current_surface);

	SDL_Flip(current_surface);
}

static void RefreshScreenGraph(void)
{

	if (!current_surface)
		return;

	if (!virtual_framebuffer)
		return;

	SDL_LockSurface(current_surface);

	{
		uint8_t *src=(uint8_t *)virtual_framebuffer;
		uint8_t *dst_line = (uint8_t *)current_surface->pixels;
		int Y=0, j;

		switch (current_surface->format->BytesPerPixel)
		{
			default:
				fprintf(stderr, "[SDL-video]: bytes-per-pixel: %d (TODO)\n", current_surface->format->BytesPerPixel);
				exit(-1);

			case 4:
			{
				uint32_t *dst;
				while (1)
				{
					dst = (uint32_t *)dst_line;
					for (j=0;j<plScrLineBytes;j++)
						*(dst++)=sdl_palette[*(src++)];
					if ((++Y)>=plScrLines)
						break;
					dst_line += current_surface->pitch;
				}
				break;
			}
			case 2:
			{
				uint16_t *dst;
				while (1)
				{
					dst = (uint16_t *)dst_line;
					for (j=0;j<plScrLineBytes;j++)
						*(dst++)=sdl_palette[*(src++)];
					if ((++Y)>=plScrLines)
						break;
					dst_line += current_surface->pitch;
				}
			}
			case 1:
			{
				uint8_t *dst;
				while (1)
				{
					dst = dst_line;
					for (j=0;j<plScrLineBytes;j++)
						*(dst++)=sdl_palette[*(src++)];
/*
					memcpy(dst, src, plScrLineBytes);
*/
					src+=plScrLineBytes;
					if ((++Y)>=plScrLines)
						break;
					dst_line += current_surface->pitch;
				}
			}
		}
	}

	SDL_UnlockSurface(current_surface);

	SDL_Flip(current_surface);
}

static int ekbhit(void)
{
	SDL_Event event;

	if (plScrMode<8)
	{
		RefreshScreenText();
	} else {
		RefreshScreenGraph();
	}

	while(SDL_PollEvent(&event))
	{
		switch (event.type)
		{
			case SDL_VIDEORESIZE:
			{
#ifdef SDL_DEBUG
				fprintf(stderr, "[SDL-video] RESIZE:\n");
				fprintf(stderr, "[SDL-video]  w:%d\n", event.resize.w);
				fprintf(stderr, "[SDL-video]  h:%d\n", event.resize.h);
				fprintf(stderr, "\n");
#endif
				set_state(do_fullscreen, event.resize.w, event.resize.h);
				break;
			}
			case SDL_SYSWMEVENT:
			{
#ifdef SDL_DEBUG
				fprintf(stderr, "[SDL-video] SYSWMEVENT\n");
				fprintf(stderr, "\n");
#endif
				break;
			}
			case SDL_MOUSEBUTTONDOWN:
			{
#ifdef SDL_DEBUG
				fprintf(stderr, "[SDL-video] MOUSEBUTTONDOWN\n");
				fprintf(stderr, "which: %d\n", (int)event.button.which);
				fprintf(stderr, "button: %d\n", (int)event.button.button);
				fprintf(stderr, "state: %d\n", (int)event.button.state);
				fprintf(stderr, "x,y: %d, %d\n", (int)event.button.x, (int)event.button.y);
				fprintf(stderr, "\n");
#endif
				switch (event.button.button)
				{
					case 1:
						___push_key(_KEY_ENTER);
						break;

					case 3:
						set_state(!do_fullscreen, plScrLineBytes, plScrLines);
						break;

					case 4:
						___push_key(KEY_UP);
						break;

					case 5:
						___push_key(KEY_DOWN);
						break;
				}
				break;
			}
			case SDL_KEYDOWN:
			{
				int index;

				if ((event.key.keysym.mod & KMOD_CTRL) && (!(event.key.keysym.mod & ~(KMOD_CTRL|KMOD_SHIFT|KMOD_ALT|KMOD_NUM))))
				{
					for (index=0;translate_ctrl[index].OCP!=0xffff;index++)
						if (translate_ctrl[index].SDL==event.key.keysym.sym)
						{
							___push_key(translate_ctrl[index].OCP);
							break;
						}
					break;
				}

				if ((event.key.keysym.mod & KMOD_SHIFT) && (!(event.key.keysym.mod & ~(KMOD_CTRL|KMOD_SHIFT|KMOD_ALT|KMOD_NUM))))
				{
					for (index=0;translate_shift[index].OCP!=0xffff;index++)
						if (translate_shift[index].SDL==event.key.keysym.sym)
						{
							___push_key(translate_shift[index].OCP);
							break;
						}
					/* no break here */
				}

				if ((event.key.keysym.mod & KMOD_ALT) && (!(event.key.keysym.mod & ~(KMOD_CTRL|KMOD_SHIFT|KMOD_ALT|KMOD_NUM))))
				{
					/* TODO, handle ALT-ENTER */
					if (event.key.keysym.sym==SDLK_RETURN)
					{
						set_state(!do_fullscreen, plScrLineBytes, plScrLines);
					} else {
						for (index=0;translate_alt[index].OCP!=0xffff;index++)
							if (translate_alt[index].SDL==event.key.keysym.sym)
							{
								___push_key(translate_alt[index].OCP);
								break;
							}
					}
					break;
				}

				if (event.key.keysym.mod & (KMOD_CTRL|KMOD_SHIFT|KMOD_ALT))
					break;

				for (index=0;translate[index].OCP!=0xffff;index++)
				{
					if (translate[index].SDL==event.key.keysym.sym)
					{
						___push_key(translate[index].OCP);
						break;
					}
				}
				break;
			}
		}
	}

	return 0;
}

static void sdl_gflushpal(void)
{
	int i;
	for (i=0;i<256;i++)
		sdl_palette[i]=SDL_MapRGB(current_surface->format, red[i], green[i], blue[i]);
}

static void sdl_gupdatepal(unsigned char color, unsigned char _red, unsigned char _green, unsigned char _blue)
{
	red[color]=_red<<2;
	green[color]=_green<<2;
	blue[color]=_blue<<2;
}

static void displayvoid(uint16_t y, uint16_t x, uint16_t len)
{
	uint8_t *addr=vgatextram+y*plScrRowBytes+x*2;
	while (len--)
	{
		*addr++=0;
		*addr++=plpalette[0];
	}
}

static void displaystrattr(uint16_t y, uint16_t x, const uint16_t *buf, uint16_t len)
{
	uint8_t *p=vgatextram+(y*plScrRowBytes+x*2);
	while (len)
	{
		*(p++)=(*buf)&0x0ff;
		*(p++)=plpalette[((*buf)>>8)];
		buf++;
		len--;
	}
}

static void displaystr(uint16_t y, uint16_t x, uint8_t attr, const char *str, uint16_t len)
{
	uint8_t *p=vgatextram+(y*plScrRowBytes+x*2);
	int i;
	attr=plpalette[attr];
	for (i=0; i<len; i++)
	{
		*p++=*str;
		if (*str)
			str++;
		*p++=attr;
	}
}

static void drawbar(uint16_t x, uint16_t yb, uint16_t yh, uint32_t hgt, uint32_t c)
{
	char buf[60];
	unsigned int i;
	uint8_t *scrptr;
	unsigned int yh1, yh2;

	if (hgt>((yh*(unsigned)16)-4))
		hgt=(yh*16)-4;
	for (i=0; i<yh; i++)
	{
		if (hgt>=16)
		{
			buf[i]=bartops[16];
			hgt-=16;
		} else {
			buf[i]=bartops[hgt];
			hgt=0;
		}
	}
	scrptr=vgatextram+(2*x+yb*plScrRowBytes);
	yh1=(yh+2)/3;
	yh2=(yh+yh1+1)/2;
	for (i=0; i<yh1; i++, scrptr-=plScrRowBytes)
	{
		scrptr[0]=buf[i];
		scrptr[1]=plpalette[c&0xFF];
	}
	c>>=8;
	for (i=yh1; i<yh2; i++, scrptr-=plScrRowBytes)
	{
		scrptr[0]=buf[i];
		scrptr[1]=plpalette[c&0xFF];
	}
	c>>=8;
	for (i=yh2; i<yh; i++, scrptr-=plScrRowBytes)
	{
		scrptr[0]=buf[i];
		scrptr[1]=plpalette[c&0xFF];
	}
}
#ifdef PFONT_IDRAWBAR
static void idrawbar(uint16_t x, uint16_t yb, uint16_t yh, uint32_t hgt, uint32_t c)
{
	unsigned char buf[60];
	unsigned int i;
	uint8_t *scrptr;
	unsigned int yh1=(yh+2)/3;
	unsigned int yh2=(yh+yh1+1)/2;

	if (hgt>((yh*(unsigned)16)-4))
	  hgt=(yh*16)-4;

	scrptr=vgatextram+(2*x+(yb-yh+1)*plScrRowBytes);

	for (i=0; i<yh; i++)
	{
		if (hgt>=16)
		{
			buf[i]=ibartops[16];
			hgt-=16;
		} else {
			buf[i]=ibartops[hgt];
			hgt=0;
		}
	}
	yh1=(yh+2)/3;
	yh2=(yh+yh1+1)/2;
	for (i=0; i<yh1; i++, scrptr+=plScrRowBytes)
	{
		scrptr[0]=buf[i];
		scrptr[1]=plpalette[c&0xFF];
	}
	c>>=8;
	for (i=yh1; i<yh2; i++, scrptr+=plScrRowBytes)
	{
		scrptr[0]=buf[i];
		scrptr[1]=plpalette[c&0xFF];
	}
	c>>=8;
	for (i=yh2; i<yh; i++, scrptr+=plScrRowBytes)
	{
		scrptr[0]=buf[i];
		scrptr[1]=plpalette[c&0xFF];
	}
}
#endif
static void setcur(uint8_t y, uint8_t x)
{
	curposx=x;
	curposy=y;
}
static void setcurshape(uint16_t shape)
{
	curshape=shape;
}
