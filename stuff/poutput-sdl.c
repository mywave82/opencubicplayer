/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2009-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
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
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <SDL.h>
#include "types.h"
#include "boot/console.h"
#include "boot/psetting.h"
#include "cpiface/cpiface.h"
#include "framelock.h"
#include "poutput.h"
#include "poutput-sdl.h"
#include "poutput-fontengine.h"
#include "poutput-swtext.h"

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

static FontSizeEnum sdl_CurrentFontWanted;

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

static int last_text_height;
static int last_text_width;

static void (*set_state)(int fullscreen, int width, int height) = 0;
static int do_fullscreen = 0;
static int plScrRowBytes = 0;
static int ekbhit_sdldummy(void);
static int sdl_SetGraphMode (int high);
static int sdl_HasKey(uint16_t key);
static void sdl_gFlushPal (void);
static void sdl_gUpdatePal (uint8_t color, uint8_t _red, uint8_t _green, uint8_t _blue);

static uint8_t red[256]=  {0x00, 0x00, 0x00, 0x00, 0xaa, 0xaa, 0xaa, 0xaa, 0x55, 0x55, 0x55, 0x55, 0xff, 0xff, 0xff, 0xff};
static uint8_t green[256]={0x00, 0x00, 0xaa, 0xaa, 0x00, 0x00, 0x55, 0xaa, 0x55, 0x55, 0xff, 0xff, 0x55, 0x55, 0xff, 0xff};
static uint8_t blue[256]= {0x00, 0xaa, 0x00, 0xaa, 0x00, 0xaa, 0x00, 0xaa, 0x55, 0xff, 0x55, 0xff, 0x55, 0xff, 0x55, 0xff};
static uint32_t sdl_palette[256];

static uint8_t *virtual_framebuffer = 0;

static void *sdl_TextOverlayAddBGRA (unsigned int x, unsigned int y, unsigned int width, unsigned int height, unsigned int pitch, uint8_t *data_bgra);
static void sdl_TextOverlayRemove (void *handle);

static const struct consoleDriver_t sdlConsoleDriver;

static void set_state_textmode(int fullscreen, int width, int height)
{
	if (current_surface)
	{
		/* This surface belongs to SDL internals */
		/* SDL_FreeSurface(current_surface); */
		current_surface = 0;
	}

	if (virtual_framebuffer)
	{
		free (virtual_framebuffer);
		conStatus.VidMem = virtual_framebuffer = 0;
	}

	if (fullscreen != do_fullscreen)
	{
		if (fullscreen)
		{
			last_text_width  = conStatus.GraphBytesPerLine;
			last_text_height = conStatus.GraphLines;
		} else {
			width = last_text_width;
			height = last_text_height;
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
	conStatus.TextGUIOverlay = current_surface->format->BytesPerPixel != 1;

	while ( ((width/FontSizeInfo[conStatus.CurrentFont].w) < 80) || ((height/FontSizeInfo[conStatus.CurrentFont].h) < 25))
	{
		switch (conStatus.CurrentFont)
		{
			case _8x16:
				conStatus.CurrentFont = _8x8;
				break;
			case _8x8:
			default:
				if (!fullscreen)
				{
					fprintf(stderr, "[SDL-video] unable to find a small enough font for %d x %d, increasing window size\n", width, height);
					width  = FontSizeInfo[conStatus.CurrentFont].w * 80;
					height = FontSizeInfo[conStatus.CurrentFont].h * 25;
					goto again;
				} else {
					fprintf(stderr, "[SDL-video] unable to find a small enough font for %d x %d\n", width, height);
					exit(-1);
				}
				break;
		}
	}

	conStatus.TextWidth         = width /FontSizeInfo[conStatus.CurrentFont].w;
	conStatus.TextHeight        = height/FontSizeInfo[conStatus.CurrentFont].h;
	conStatus.GraphBytesPerLine = width;
	conStatus.GraphLines        = height;

	plScrRowBytes = conStatus.TextWidth * 2;

	conStatus.VidMem = virtual_framebuffer = calloc (conStatus.GraphBytesPerLine, conStatus.GraphLines);
	if (!virtual_framebuffer)
	{
		fprintf(stderr, "[SDL-video] calloc() failed\n");
		exit(-1);
	}

	sdl_gFlushPal ();

	___push_key(VIRT_KEY_RESIZE);
}

static void sdl_SetTextMode (unsigned char x)
{
	set_state = set_state_textmode;

	if (x == conStatus.CurrentMode)
	{
		bzero (virtual_framebuffer, conStatus.GraphBytesPerLine * conStatus.GraphLines);
		return;
	}

	sdl_SetGraphMode (-1);

	if (x==255)
	{
		if (current_surface)
		{
			/* This surface belongs to SDL internals */
			/* SDL_FreeSurface(current_surface); */
			current_surface = 0;
		}
		conStatus.CurrentMode = 255;
		return; /* gdb helper */
	}

	/* if invalid mode, set it to custom */
	if (x>7)
	{
		x=8;

		set_state_textmode(
			do_fullscreen,
			last_text_width,
			last_text_height);
	} else {
		conStatus.CurrentFont = mode_tui_data[x].font;

		set_state_textmode(
			do_fullscreen,
			mode_gui_data[mode_tui_data[x].gui_mode].width,
			mode_gui_data[mode_tui_data[x].gui_mode].height);
	}

	conStatus.LastTextMode = conStatus.CurrentMode = x;
}

static int cachemode = -1;

static void set_state_graphmode(int fullscreen, int width, int height)
{
	mode_gui_t mode;
	switch (cachemode)
	{
		case 13:
			conStatus.CurrentMode = 13;
			mode = MODE_320_200;
			break;
		case 0:
			conStatus.CurrentMode = 100;
			mode = MODE_640_480;
			break;
		case 1:
			conStatus.CurrentMode = 101;
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
		/* This surface belongs to SDL internals */
		/* SDL_FreeSurface(current_surface); */
		current_surface = 0;
	}

	if (virtual_framebuffer)
	{
		free(virtual_framebuffer);
		conStatus.VidMem = virtual_framebuffer = 0;
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
	conStatus.TextGUIOverlay = current_surface->format->BytesPerPixel != 1;

	conStatus.TextWidth         = width/8;
	conStatus.TextHeight        = height/16;
	conStatus.GraphBytesPerLine = width;
	conStatus.GraphLines        = height;

	plScrRowBytes = conStatus.TextWidth * 2;

	conStatus.VidMem = virtual_framebuffer = calloc (conStatus.GraphBytesPerLine, conStatus.GraphLines);
	if (!virtual_framebuffer)
	{
		fprintf(stderr, "[SDL-video] calloc() failed\n");
		exit(-1);
	}

	sdl_gFlushPal ();

	___push_key(VIRT_KEY_RESIZE);
}

static int sdl_SetGraphMode (int high)
{
	if (high>=0)
		set_state = set_state_graphmode;

	if ((high==cachemode)&&(high>=0))
		goto quick;
	cachemode=high;

	if (virtual_framebuffer)
	{
		free (virtual_framebuffer);
		conStatus.VidMem = virtual_framebuffer = 0;
	}

	if (high<0)
		return 0;

	set_state_graphmode(do_fullscreen, 0, 0);

quick:
	if (virtual_framebuffer)
	{
		bzero (virtual_framebuffer, conStatus.GraphBytesPerLine * conStatus.GraphLines);
	}

	sdl_gFlushPal ();
	return 0;
}

static void sdl_vga13 (void)
{
	sdl_SetGraphMode (13);
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

	conStatus.VidType = vidNorm;

	if ((fullscreen_info[MODE_BIGGEST].resolution.w>=1024) && (fullscreen_info[MODE_BIGGEST].resolution.h>=768))
	{
		conStatus.VidType = vidVESA;
	}
}

static int sdl_consoleRestore(void)
{
	return 0;
}

static void sdl_consoleSave(void)
{
}

static void sdl_DisplaySetupTextMode(void)
{
	while (1)
	{
		uint16_t c;
		bzero (virtual_framebuffer, conStatus.GraphBytesPerLine * conStatus.GraphLines);
		make_title("sdl-driver setup", 0);
		swtext_displaystr_cp437(1, 0, 0x07, "1:  font-size:", 14);
		swtext_displaystr_cp437(1, 15, conStatus.CurrentFont == _8x8 ? 0x0f : 0x07, "8x8", 3);
		swtext_displaystr_cp437(1, 19, conStatus.CurrentFont == _8x16 ? 0x0f : 0x07, "8x16", 4);
/*
		swtext_displaystr_cp437(2, 0, 0x07, "2:  fullscreen: ", 16);
		swtext_displaystr_cp437(3, 0, 0x07, "3:  resolution in fullscreen:", 29);*/

		swtext_displaystr_cp437(conStatus.TextHeight - 1, 0, 0x17, "  press the number of the item you wish to change and ESC when done", conStatus.TextWidth);

		while (!ekbhit())
		{
				framelock();
		}
		c = egetch();

		switch (c)
		{
			case '1':
				/* we can assume that we are in text-mode if we are here */
				sdl_CurrentFontWanted = conStatus.CurrentFont = (conStatus.CurrentFont == _8x8)?_8x16:_8x8;
				set_state_textmode(do_fullscreen, conStatus.GraphBytesPerLine, conStatus.GraphLines);
				cfSetProfileInt("x11", "font", conStatus.CurrentFont, 10);
				break;
			case KEY_EXIT:
			case KEY_ESC: return;
		}
	}
}

static const char *sdl_GetDisplayTextModeName(void)
{
	static char mode[48];
	snprintf(mode, sizeof(mode), "res(%dx%d), font(%s)%s", conStatus.TextWidth, conStatus.TextHeight,
		conStatus.CurrentFont == _8x8 ? "8x8" : "8x16", do_fullscreen?" fullscreen":"");
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

struct keytranslate_t
{
	SDLKey SDL;
	uint16_t OCP;
};

struct keytranslate_t translate[] =
{
	{SDLK_BACKSPACE,    KEY_BACKSPACE},
	{SDLK_TAB,          KEY_TAB},
	{SDLK_DELETE,       KEY_DELETE},
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
	{SDLK_LESS,         '>'},
	{SDLK_COMMA,        '<'},
	{SDLK_MINUS,        '?'},
	{SDLK_PERIOD,       '>'},
	{SDLK_SEMICOLON,    ':'},
	{SDLK_QUOTE,        '\"'},
	{SDLK_LEFTBRACKET,  '{'},
	{SDLK_RIGHTBRACKET, '}'},
	{SDLK_MINUS,        '_'},
	{SDLK_EQUALS,       '+'},
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
	{SDLK_F1,           KEY_SHIFT_F(1)},
	{SDLK_F2,           KEY_SHIFT_F(2)},
	{SDLK_F3,           KEY_SHIFT_F(3)},
	{SDLK_F4,           KEY_SHIFT_F(4)},
	{SDLK_F5,           KEY_SHIFT_F(5)},
	{SDLK_F6,           KEY_SHIFT_F(6)},
	{SDLK_F7,           KEY_SHIFT_F(7)},
	{SDLK_F8,           KEY_SHIFT_F(8)},
	{SDLK_F9,           KEY_SHIFT_F(9)},
	{SDLK_F10,          KEY_SHIFT_F(10)},
	{SDLK_F11,          KEY_SHIFT_F(11)},
	{SDLK_F12,          KEY_SHIFT_F(12)},
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
	{SDLK_HOME,         KEY_CTRL_HOME},
	{SDLK_F1,           KEY_CTRL_F(1)},
	{SDLK_F2,           KEY_CTRL_F(2)},
	{SDLK_F3,           KEY_CTRL_F(3)},
	{SDLK_F4,           KEY_CTRL_F(4)},
	{SDLK_F5,           KEY_CTRL_F(5)},
	{SDLK_F6,           KEY_CTRL_F(6)},
	{SDLK_F7,           KEY_CTRL_F(7)},
	{SDLK_F8,           KEY_CTRL_F(8)},
	{SDLK_F9,           KEY_CTRL_F(9)},
	{SDLK_F10,          KEY_CTRL_F(10)},
	{SDLK_F11,          KEY_CTRL_F(11)},
	{SDLK_F12,          KEY_CTRL_F(12)},
	{-1,                0xffff},
};

struct keytranslate_t translate_ctrl_shift[] =
{
	{SDLK_F1,           KEY_CTRL_SHIFT_F(1)},
	{SDLK_F2,           KEY_CTRL_SHIFT_F(2)},
	{SDLK_F3,           KEY_CTRL_SHIFT_F(3)},
	{SDLK_F4,           KEY_CTRL_SHIFT_F(4)},
	{SDLK_F5,           KEY_CTRL_SHIFT_F(5)},
	{SDLK_F6,           KEY_CTRL_SHIFT_F(6)},
	{SDLK_F7,           KEY_CTRL_SHIFT_F(7)},
	{SDLK_F8,           KEY_CTRL_SHIFT_F(8)},
	{SDLK_F9,           KEY_CTRL_SHIFT_F(9)},
	{SDLK_F10,          KEY_CTRL_SHIFT_F(10)},
	{SDLK_F11,          KEY_CTRL_SHIFT_F(11)},
	{SDLK_F12,          KEY_CTRL_SHIFT_F(12)},
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

static int sdl_HasKey (uint16_t key)
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
	for (index=0;translate_ctrl_shift[index].OCP!=0xffff;index++)
		if (translate_ctrl_shift[index].OCP==key)
			return 1;
	for (index=0;translate_alt[index].OCP!=0xffff;index++)
		if (translate_alt[index].OCP==key)
			return 1;

	fprintf(stderr, __FILE__ ": unknown key 0x%04x\n", (int)key);
	return 0;
}

struct SDLScrTextGUIOverlay_t
{
	unsigned int x;
	unsigned int y;
	unsigned int width;
	unsigned int height;
	uint8_t     *data_bgra;
	SDL_Surface *surface;

};

static struct SDLScrTextGUIOverlay_t **SDLScrTextGUIOverlays;
static int                             SDLScrTextGUIOverlays_count;
static int                             SDLScrTextGUIOverlays_size;

static void *sdl_TextOverlayAddBGRA (unsigned int x, unsigned int y, unsigned int width, unsigned int height, unsigned int pitch, uint8_t *data_bgra)
{
	struct SDLScrTextGUIOverlay_t *e = malloc (sizeof (*e));
	e->x = x;
	e->y = y;
	e->width = width;
	e->height = height;
	e->data_bgra = data_bgra;

	if (SDLScrTextGUIOverlays_count == SDLScrTextGUIOverlays_size)
	{
		SDLScrTextGUIOverlays_size += 10;
		SDLScrTextGUIOverlays = realloc (SDLScrTextGUIOverlays, sizeof (SDLScrTextGUIOverlays[0]) * SDLScrTextGUIOverlays_size);
	}
	e->surface = SDL_CreateRGBSurfaceFrom (data_bgra, width, height, 32, pitch * 4, uint32_little(0x00ff0000), uint32_little(0x0000ff00), uint32_little(0x000000ff), uint32_little(0xff000000));
	SDLScrTextGUIOverlays[SDLScrTextGUIOverlays_count++] = e;

	return e;
}

static void sdl_TextOverlayRemove (void *handle)
{
	int i;
	for (i=0; i < SDLScrTextGUIOverlays_count; i++)
	{
		if (SDLScrTextGUIOverlays[i] == handle)
		{
			SDL_FreeSurface(SDLScrTextGUIOverlays[i]->surface);
			memmove (SDLScrTextGUIOverlays + i, SDLScrTextGUIOverlays + i + 1, sizeof (SDLScrTextGUIOverlays[0]) * (SDLScrTextGUIOverlays_count - i - 1));
			SDLScrTextGUIOverlays_count--;
			free (handle);
			return;
		}
	}
	fprintf (stderr, "[SDL] Warning: sdl_TextOverlayRemove, handle %p not found\n", handle);
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
					for (j = 0; j < conStatus.GraphBytesPerLine; j++)
					{
						*(dst++)=sdl_palette[*(src++)];
					}
					if ((++Y) >= conStatus.GraphLines)
					{
						break;
					}
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
					for (j = 0; j < conStatus.GraphBytesPerLine; j++)
					{
						*(dst++)=sdl_palette[*(src++)];
					}
					if ((++Y) >= conStatus.GraphLines)
					{
						break;
					}
					dst_line += current_surface->pitch;
				}
			}
			case 1:
			{
				uint8_t *dst;
				while (1)
				{
					dst = dst_line;
					for (j = 0; j < conStatus.GraphBytesPerLine; j++)
					{
						*(dst++)=sdl_palette[*(src++)];
					}
/*
					memcpy(dst, src, conStatus.GraphBytesPerLine);
*/
					src += conStatus.GraphBytesPerLine;
					if ((++Y) >= conStatus.GraphLines)
					{
						break;
					}
					dst_line += current_surface->pitch;
				}
			}
		}
	}

	SDL_UnlockSurface(current_surface);

	if ((current_surface->format->BytesPerPixel != 1) && SDLScrTextGUIOverlays_count)
	{
		int i;
		for (i=0; i < SDLScrTextGUIOverlays_count; i++)
		{
			SDL_Rect src;
			SDL_Rect dst;

			src.x = 0;
			src.y = 0;
			src.w = SDLScrTextGUIOverlays[i]->width;
			src.h = SDLScrTextGUIOverlays[i]->height;

			dst.x = SDLScrTextGUIOverlays[i]->x;
			dst.y = SDLScrTextGUIOverlays[i]->y;
			dst.w = SDLScrTextGUIOverlays[i]->width;
			dst.h = SDLScrTextGUIOverlays[i]->height;

			SDL_BlitSurface (SDLScrTextGUIOverlays[i]->surface, &src, current_surface, &dst);
		}
	}

	SDL_Flip(current_surface);

	if (conStatus.CurrentFont == _8x8)
	{
		fontengine_8x8_iterate ();
	} else if (conStatus.CurrentFont == _8x16)
	{
		fontengine_8x16_iterate ();
	}
}

static void RefreshScreenText(void)
{
	if (!current_surface)
		return;

	if (!virtual_framebuffer)
		return;

	swtext_cursor_inject();

	RefreshScreenGraph();

	swtext_cursor_eject();
}

static uint32_t utf16_try_decode (uint16_t *data_he, int *length)
{
	if (!*length) return 0;

	/* normal single cell character */
	if ((data_he[0] < 0xd800) ||
            (data_he[0] > 0xdfff))
	{
		(*length) = 0;
		return data_he[0];
	}
	if ((data_he[0] & 0xfc00) != 0xd800)
	{
		(*length) = 0;
		return 0;
	}
	if ((*length) < 2)
	{
		return 0;
	}
	if ((data_he[1] & 0xfc00) != 0xdc00)
	{
		(*length) = 0;
		return 0;
	}
	(*length) = 0;
	return ((uint_fast32_t)(data_he[0] & 0x03ff) << 10) | (data_he[1] & 0x03ff);
}

/* duplicate from utf8.c: poutput.so is not available yet, and SDL 1.x is only for legacy use */
static int utf8_encode (char *dst, uint32_t codepoint)
{
	if (codepoint == 0)
	{
		dst[0] = 0;
		return 0;
	}

	if (codepoint <= 0x7f)
	{
		dst[0] = codepoint;
		dst[1] = 0;
		return 1;
	}

	if (codepoint <= 0x7ff)
	{
		dst[0] = 0xC0 | (codepoint >> 6);
		dst[1] = 0x80 | (codepoint & 0x3f);
		dst[2] = 0;
		return 2;
	}

	if (codepoint <= 0xffff)
	{
		dst[0] = 0xe0 |  (codepoint >> 12);
		dst[1] = 0x80 | ((codepoint >>  6) & 0x3f);
		dst[2] = 0x80 |  (codepoint        & 0x3f);
		dst[3] = 0;
		return 3;
	}

	if (codepoint <= 0x1fffff)
	{
		dst[0] = 0xf0 |  (codepoint >> 18);
		dst[1] = 0x80 | ((codepoint >> 12) & 0x3f);
		dst[2] = 0x80 | ((codepoint >>  6) & 0x3f);
		dst[3] = 0x80 |  (codepoint        & 0x3f);
		dst[4] = 0;
		return 4;
	}

	if (codepoint <= 0x3ffffff)
	{ /* non-standard */
		dst[0] = 0xf8 |  (codepoint >> 24);
		dst[1] = 0x80 | ((codepoint >> 18) & 0x3f);
		dst[2] = 0x80 | ((codepoint >> 12) & 0x3f);
		dst[3] = 0x80 | ((codepoint >>  6) & 0x3f);
		dst[4] = 0x80 |  (codepoint        & 0x3f);
		dst[5] = 0;
		return 5;
	}

	if (codepoint <= 0x7fffffff)
	{ /* non-standard */
		dst[0] = 0xfc |  (codepoint >> 30);
		dst[1] = 0x80 | ((codepoint >> 24) & 0x3f);
		dst[2] = 0x80 | ((codepoint >> 18) & 0x3f);
		dst[3] = 0x80 | ((codepoint >> 12) & 0x3f);
		dst[4] = 0x80 | ((codepoint >>  6) & 0x3f);
		dst[5] = 0x80 |  (codepoint        & 0x3f);
		dst[6] = 0;
		return 6;
	}

	/* 7 bytes has never been used */
	dst[0] = 0;
	return 0;
}

static int ekbhit_sdldummy(void)
{
	SDL_Event event;

	if (conStatus.CurrentMode < 8)
	{
		RefreshScreenText();
	} else {
		RefreshScreenGraph();
	}

	while(SDL_PollEvent(&event))
	{
		switch (event.type)
		{
			case SDL_QUIT:
			{
#ifdef SDL_DEBUG
				fprintf(stderr, "[SDL-video] QUIT\n");
#endif
				___push_key(KEY_EXIT);
				break;
			}
			case SDL_VIDEORESIZE:
			{
#ifdef SDL_DEBUG
				fprintf(stderr, "[SDL-video] RESIZE:\n");
				fprintf(stderr, "[SDL-video]  w:%d\n", event.resize.w);
				fprintf(stderr, "[SDL-video]  h:%d\n", event.resize.h);
				fprintf(stderr, "\n");
#endif
				conStatus.CurrentFont = sdl_CurrentFontWanted;
				if (!do_fullscreen && (conStatus.CurrentMode == conStatus.LastTextMode))
				{
					last_text_height = event.resize.h;
					last_text_width = event.resize.w;
				}
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
						set_state(!do_fullscreen, conStatus.GraphBytesPerLine, conStatus.GraphLines);
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

				if ( (event.key.keysym.mod & KMOD_CTRL) &&
				     (event.key.keysym.mod & KMOD_ALT) &&
				     (event.key.keysym.unicode))
				{
					int count = 0;
					uint16_t data[2];
					uint32_t code;
					data[count] = event.key.keysym.unicode;
					count++;
					code = utf16_try_decode (data, &count);
					if ((code == 0) && (count == 2))
					{
						count = 0;
					}
					if (code)
					{
						char dst[8];
						utf8_encode (dst, code);
						for (index =0; dst[index]; index++)
						{
							___push_key((uint8_t)dst[index]);
						}
					}
					break;
				}

				if ( (event.key.keysym.mod &  KMOD_CTRL) &&
				     (!(event.key.keysym.mod & (KMOD_SHIFT|KMOD_ALT))) )
				{
					for (index=0;translate_ctrl[index].OCP!=0xffff;index++)
						if (translate_ctrl[index].SDL==event.key.keysym.sym)
						{
							___push_key(translate_ctrl[index].OCP);
							break;
						}
					break;
				}

				if ( (event.key.keysym.mod & KMOD_CTRL) &&
				     (event.key.keysym.mod & KMOD_SHIFT) &&
				    (!((event.key.keysym.mod & KMOD_ALT))) )
				{
					for (index=0;translate_ctrl_shift[index].OCP!=0xffff;index++)
						if (translate_ctrl_shift[index].SDL==event.key.keysym.sym)
						{
							___push_key(translate_ctrl_shift[index].OCP);
							break;
						}
					break;
				}

				if ( (event.key.keysym.mod & KMOD_SHIFT) &&

				     (!(event.key.keysym.mod & (KMOD_CTRL|KMOD_ALT))) )
				{
					for (index=0;translate_shift[index].OCP!=0xffff;index++)
						if (translate_shift[index].SDL==event.key.keysym.sym)
						{
							___push_key(translate_shift[index].OCP);
							break;
						}
					/* no break here */
				}

				if ( (event.key.keysym.mod & KMOD_ALT) &&
                                    (!(event.key.keysym.mod & (KMOD_CTRL|KMOD_SHIFT))) )
				{
					/* TODO, handle ALT-ENTER */
					if (event.key.keysym.sym==SDLK_RETURN)
					{
						set_state (!do_fullscreen, conStatus.GraphBytesPerLine, conStatus.GraphLines);
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

static void sdl_gFlushPal (void)
{
	int i;
	for (i = 0; i < 256; i++)
	{
		sdl_palette[i] = SDL_MapRGB (current_surface->format, red[i], green[i], blue[i]);
	}
}

static void sdl_gUpdatePal (uint8_t color, uint8_t _red, uint8_t _green, uint8_t _blue)
{
	red[color]=_red<<2;
	green[color]=_green<<2;
	blue[color]=_blue<<2;
}

static int need_quit = 0;

int sdl_init(void)
{
	if ( SDL_Init(/*SDL_INIT_AUDIO|*/SDL_INIT_VIDEO) < 0 )
	{
		fprintf(stderr, "[SDL video] Unable to init SDL: %s\n", SDL_GetError());
		return 1;
	}

	if (fontengine_init())
	{
		SDL_Quit();
		return 1;
	}

	SDL_EnableUNICODE (1);

	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

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
		goto error_out;
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

	sdl_CurrentFontWanted = conStatus.CurrentFont = cfGetProfileInt("x11", "font", _8x16, 10);
	if (conStatus.CurrentFont > _FONT_MAX)
	{
		conStatus.CurrentFont = _8x16;
	}
	last_text_width  = conStatus.GraphBytesPerLine = 640;
	last_text_height = conStatus.GraphLines        = 480;

	conStatus.LastTextMode = conStatus.CurrentMode = 8;

	need_quit = 1;

	conDriver = &sdlConsoleDriver;
	___setup_key(ekbhit_sdldummy, ekbhit_sdldummy);

	conStatus.TextGUIOverlay = 0;

	return 0;

error_out:
	SDL_ClearError();
	fontengine_done ();
	SDL_Quit();
	return 1;
}

void sdl_done(void)
{
	if (!need_quit)
		return;

	fontengine_done ();

	SDL_Quit();

	if (virtual_framebuffer)
	{
		free(virtual_framebuffer);
		conStatus.VidMem = virtual_framebuffer = 0;
	}

	need_quit = 0;

	free (SDLScrTextGUIOverlays);
	SDLScrTextGUIOverlays = 0;
	SDLScrTextGUIOverlays_size = 0;
	SDLScrTextGUIOverlays_count = 0;
}

static void sdl_DosShell (void)
{
	/* dummy */
}

static const struct consoleDriver_t sdlConsoleDriver =
{
	sdl_vga13,
	sdl_SetTextMode,
	sdl_DisplaySetupTextMode,
	sdl_GetDisplayTextModeName,
	swtext_measurestr_utf8,
	swtext_displaystr_utf8,
	swtext_displaychr_cp437,
	swtext_displaystr_cp437,
	swtext_displaystrattr_cp437,
	swtext_displayvoid,
	swtext_drawbar,
	swtext_idrawbar,
	sdl_TextOverlayAddBGRA,
	sdl_TextOverlayRemove,
	sdl_SetGraphMode,
	generic_gdrawchar,
	generic_gdrawcharp,
	generic_gdrawchar8,
	generic_gdrawchar8p,
	generic_gdrawstr,
	generic_gupdatestr,
	sdl_gUpdatePal,
	sdl_gFlushPal,
	sdl_HasKey,
	swtext_setcur,
	swtext_setcurshape,
	sdl_consoleRestore,
	sdl_consoleSave,
	sdl_DosShell
};
