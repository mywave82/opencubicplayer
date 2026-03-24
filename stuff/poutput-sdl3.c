/* opencp module Player
 * copyright (c) 2018-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * SDL3 graphic driver
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
#include <math.h>
#include <time.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_version.h>
#include <SDL3/SDL_video.h>
#include "types.h"
#include "boot/console.h"
#include "boot/psetting.h"
#include "cpiface/cpiface.h"
#include "stuff/framelock.h"
#include "stuff/imsrtns.h"
#include "stuff/poll.h"
#include "stuff/poutput.h"
#include "stuff/poutput-fontengine.h"
#include "stuff/poutput-keyboard.h"
#include "stuff/poutput-sdl3.h"
#include "stuff/poutput-swtext.h"

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

static FontSizeEnum sdl3_CurrentFontWanted;

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

static SDL_Window *current_window = NULL;
static SDL_Renderer *current_renderer = NULL;
static SDL_Texture *current_texture = NULL;

static int last_text_height;
static int last_text_width;

static int cachemode = -1;

static void (*set_state)(const int fullscreen, int width, int height, const int window_resized) = 0;
static int current_fullsceen = 0;
static int plScrRowBytes = 0;
static int ekbhit_sdl3dummy(void);
static int sdl3_SetGraphMode (int high);
static int sdl3_HasKey (uint16_t key);
static void sdl3_gFlushPal (void);
static void sdl3_gUpdatePal (uint8_t color, uint8_t _red, uint8_t _green, uint8_t _blue);

static void *sdl3_TextOverlayAddBGRA (unsigned int x, unsigned int y, unsigned int width, unsigned int height, unsigned int pitch, uint8_t *data_bgra);
static void sdl3_TextOverlayRemove (void *handle);

static const struct consoleDriver_t sdl3ConsoleDriver;

static uint32_t sdl3_palette[256] = {
	0xff000000,
	0xff0000aa,
	0xff00aa00,
	0xff00aaaa,
	0xffaa0000,
	0xffaa00aa,
	0xffaa5500,
	0xffaaaaaa,
	0xff555555,
	0xff5555ff,
	0xff55ff55,
	0xff55ffff,
	0xffff5555,
	0xffff55ff,
	0xffffff55,
	0xffffffff,
};

static uint8_t *virtual_framebuffer = 0;

static void sdl3_close_window(void)
{
#ifdef SDL3_DEBUG
	fprintf (stderr, "[SDL3-video] sdl3_close_window()");
#endif
	if (current_texture)
	{
		SDL_DestroyTexture (current_texture);
		current_texture = 0;
	}
	if (current_renderer)
	{
		SDL_DestroyRenderer (current_renderer);
		current_renderer = 0;
	}
	if (current_window)
	{
		SDL_StopTextInput (current_window);
		SDL_DestroyWindow (current_window);
		current_window = 0;
	}
}

#ifdef SDL3_DEBUG
static void dump_prop(void *userdata, SDL_PropertiesID props, const char *name)
{
	switch (SDL_GetPropertyType(props, name))
	{
		case SDL_PROPERTY_TYPE_POINTER:
			fprintf (stderr, "   %s = pointer %p\n", name, SDL_GetPointerProperty (props, name, 0));
			break;
		case SDL_PROPERTY_TYPE_STRING:
			fprintf (stderr, "   %s = string \"%s\"\n", name, SDL_GetStringProperty(props, name, ""));
			break;
		case SDL_PROPERTY_TYPE_NUMBER:
			fprintf (stderr, "   %s = number %"SDL_PRIs64"\n", name, SDL_GetNumberProperty(props, name, 0));
			break;
		case SDL_PROPERTY_TYPE_FLOAT:
			fprintf (stderr, "   %s = float %f\n", name, SDL_GetFloatProperty(props, name, 0.0f));
			break;
		case SDL_PROPERTY_TYPE_BOOLEAN:
			fprintf (stderr, "   %s = boolean %d\n", name, SDL_GetBooleanProperty(props, name, false));
			break;
		case SDL_PROPERTY_TYPE_INVALID:
		default:
			fprintf (stderr, "   %s = invalid property\n", name);
			break;
	}
}
#endif

static void sdl3_dump_renderer (void)
{
#ifdef SDL3_DEBUG
	SDL_PropertiesID rprops = SDL_GetRendererProperties(current_renderer);
	fprintf (stderr, "[SDL3-video]\n");
	fprintf (stderr, "  renderer.name = \"%s\"\n", SDL_GetRendererName (current_renderer));
	SDL_EnumerateProperties (rprops, dump_prop, 0);
#endif
}

static void set_state_textmode (const int fullscreen, int width, int height, const int window_resized)
{
#ifdef SDL3_DEBUG
	fprintf (stderr, "[SDL3-video] set_state_textmode (fullscreen=%d, width=%d, height=%d, window_resized=%d)\n", fullscreen, width, height, window_resized);
#endif
	/* texture WILL for sure resize, so just get rid of it! */
	if (current_texture)
	{
		SDL_DestroyTexture (current_texture);
		current_texture = 0;
	}

	if (virtual_framebuffer)
	{
		free (virtual_framebuffer);
		Console.VidMem = virtual_framebuffer = 0;
	}

	if (fullscreen != current_fullsceen)
	{
#ifdef SDL3_DEBUG
		fprintf (stderr, "             fullscreen =! current_fullsceen\n");
#endif
		if (fullscreen)
		{
			last_text_width  = Console.GraphBytesPerLine;
			last_text_height = Console.GraphLines;
#ifdef SDL3_DEBUG
			fprintf (stderr, "             store previous resolution into last_text_width and last_text_height\n");
#endif
		} else {
			width = last_text_width;
			height = last_text_height;
#ifdef SDL3_DEBUG
			fprintf (stderr, "             restore and override width and height\n");
#endif
		}
	}

	if (!width)
	{
#ifdef SDL3_DEBUG
		fprintf (stderr, "             width==0 ???\n");
#endif
		width = 640;
	}

	if (!height)
	{
#ifdef SDL3_DEBUG
		fprintf (stderr, "             height==0 ???\n");
#endif
		height = 480;
	}

	/* if the call is due to window resized, do not do any changes to the window */
	if (!window_resized)
	{
		current_fullsceen = fullscreen;

		if (fullscreen)
		{
			if (!current_window)
			{
				current_window = SDL_CreateWindow ("Open Cubic Player",
				                                   0, 0,
				                                   SDL_WINDOW_FULLSCREEN);
				SDL_StartTextInput (current_window);
			} else {
				SDL_SetWindowFullscreen (current_window, SDL_WINDOW_FULLSCREEN);
			}
		} else {
			if (!current_window)
			{
				current_window = SDL_CreateWindow ("Open Cubic Player",
				                                   width, height,
				                                   SDL_WINDOW_RESIZABLE);
				SDL_StartTextInput (current_window);
			} else {
				SDL_SetWindowFullscreen (current_window, 0);
				SDL_SetWindowResizable (current_window, true);
				SDL_SetWindowSize (current_window, width, height);
			}
		}
	}

	if (!current_window)
	{
		fprintf (stderr, "[SDL3-video] SDL_CreateWindow: %s (fullscreen=%d %dx%d)\n", SDL_GetError(), fullscreen, width, height);
		SDL_ClearError();
		exit(1);
	}

	SDL_GetWindowSize (current_window, &width, &height);

	while ( ((width/FontSizeInfo[Console.CurrentFont].w) < CONSOLE_MIN_X) || ((height/FontSizeInfo[Console.CurrentFont].h) < CONSOLE_MIN_Y))
	{
#ifdef SDL3_DEBUG
		fprintf (stderr, "             find a smaller font, since (%d/%d)=%d < %d   or   (%d/%d)=%d < %d\n",
				width,  FontSizeInfo[Console.CurrentFont].w, width/FontSizeInfo[Console.CurrentFont].w, CONSOLE_MIN_X,
				height, FontSizeInfo[Console.CurrentFont].h, width/FontSizeInfo[Console.CurrentFont].h, CONSOLE_MIN_Y);
#endif
		switch (Console.CurrentFont)
		{
			case _16x32:
				Console.CurrentFont = _8x16;
				break;
			case _8x16:
				Console.CurrentFont = _8x8;
				break;
			case _8x8:
			default:
				if (!fullscreen)
				{
#ifdef SDL3_DEBUG
					fprintf(stderr, "             unable to find a small enough font for %d x %d, increasing window size\n", width, height);
#endif
					width  = FontSizeInfo[Console.CurrentFont].w * CONSOLE_MIN_X;
					height = FontSizeInfo[Console.CurrentFont].h * CONSOLE_MIN_Y;
					SDL_SetWindowSize (current_window, width, height);
				} else {
					fprintf(stderr, "             unable to find a small enough font for %d x %d\n", width, height);
					exit(-1);
				}
				break;
		}
	}

	Console.TextWidth         = width /FontSizeInfo[Console.CurrentFont].w;
	Console.TextHeight        = height/FontSizeInfo[Console.CurrentFont].h;
	Console.GraphBytesPerLine = width;
	Console.GraphLines        = height;

	plScrRowBytes = Console.TextWidth * 2;

	if (!current_renderer)
	{
		current_renderer = SDL_CreateRenderer (current_window, 0);

		if (!current_renderer)
		{
			fprintf (stderr, "[SD2-video] SDL_CreateRenderer: %s\n", SDL_GetError());
			SDL_ClearError();
			exit(-1);
		} else {
			sdl3_dump_renderer();
		}

		/* This call does nothing until we have a renderer, so that is why we waited until now */
		SDL_SetWindowMinimumSize (current_window, FontSizeInfo[0].w * CONSOLE_MIN_X, FontSizeInfo[0].h * CONSOLE_MIN_Y);
	}

	if (!current_texture)
	{
		current_texture = SDL_CreateTexture (current_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
		if (!current_texture)
		{
#ifdef SDL3_DEBUG
			fprintf (stderr, "[SDL3-video] SDL_CreateTexture: %s\n", SDL_GetError());
#endif
			SDL_ClearError();
			current_texture = SDL_CreateTexture (current_renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
			if (!current_texture)
			{
				fprintf (stderr, "[SDL3-video] SDL_CreateTexture: %s\n", SDL_GetError());
				SDL_ClearError();
				exit(-1);
			}
		}
	}

	Console.VidMem = virtual_framebuffer = calloc (Console.GraphBytesPerLine, Console.GraphLines);

	sdl3_gFlushPal ();

	do { /* resolve Console.CurrentMode */
		int i;
		for (i=0; i < 8; i++)
		{
			if ((Console.TextWidth   == mode_tui_data[i].text_width) &&
			    (Console.TextHeight  == mode_tui_data[i].text_height) &&
			    (Console.CurrentFont == mode_tui_data[i].font))
			{
				break;
			}
		}
		Console.LastTextMode = Console.CurrentMode = i;
	} while (0);

	___push_key(VIRT_KEY_RESIZE);
}

static void sdl3_SetTextMode(unsigned char x)
{
#ifdef SDL3_DEBUG
	fprintf (stderr, "[SDL3-video] sdl3_SetTextMode(%d) # Console.CurrentMode=%d\n", x, Console.CurrentMode);
#endif
	set_state = set_state_textmode;

	if ((x == Console.CurrentMode) && (current_window))
	{
#ifdef SDL3_DEBUG
		fprintf (stderr, "             quickreturn, we already have the correct mode\n");
#endif
		memset (virtual_framebuffer, 0, Console.GraphBytesPerLine * Console.GraphLines);
		return;
	}

	if (cachemode >= 0)
	{
#ifdef SDL3_DEBUG
		fprintf (stderr, "             cachemode >= 0, last active mode was GUI, please close it\n");
#endif

		sdl3_SetGraphMode (-1); /* closes the window */
#ifdef SDL3_DEBUG
	fprintf (stderr, "[SDL3-video] sdl3_SetTextMode....\n");
#endif
	}

	Console.CurrentFont = sdl3_CurrentFontWanted;

	/* if invalid mode, set it to custom */
	if (x>7)
	{
#ifdef SDL3_DEBUG
		fprintf (stderr, "             custom request\n");
#endif
		set_state_textmode (
			current_fullsceen,
			last_text_width,
			last_text_height,
		        0);
	} else {
#ifdef SDL3_DEBUG
		fprintf (stderr, "             old vga mode request:\n");
		fprintf (stderr, "               mode_tui_data[%d].gui_mode=%d\n", x, mode_tui_data[x].gui_mode);
		fprintf (stderr, "               mode_tui_data[%d].font=%d", x, mode_tui_data[x].font);
		fprintf (stderr, "               mode_gui_data[%d].width=%d\n", mode_tui_data[x].gui_mode, mode_gui_data[mode_tui_data[x].gui_mode].width);
		fprintf (stderr, "               mode_gui_data[%d].height=%d", mode_tui_data[x].gui_mode, mode_gui_data[mode_tui_data[x].gui_mode].height);
#endif
		Console.CurrentFont = mode_tui_data[x].font;

		set_state_textmode (
			current_fullsceen,
			mode_gui_data[mode_tui_data[x].gui_mode].width,
			mode_gui_data[mode_tui_data[x].gui_mode].height,
		        0);
	}

#ifdef SDL3_DEBUG
	fprintf (stderr, "[SDL3-video] sdl3_SetTextMode....\n");
	fprintf (stderr, "             sdl3_CurrentFontWanted=%d Console.CurrentFont=%d\n", sdl3_CurrentFontWanted, Console.CurrentFont);
	fprintf (stderr, "             plScrType = plScrMode = %d\n", Console.CurrentMode);
#endif

	Console.LastTextMode = Console.CurrentMode;
}

static void set_state_graphmode (const int fullscreen, int width, int height, const int window_resized)
{
	mode_gui_t mode;

#ifdef SDL3_DEBUG
	fprintf (stderr, "[SDL3-video] set_state_graphmode(fullscreen=%d, width=%d, height=%d, window_resized=%d)\n", fullscreen, width, height, window_resized);
#endif

	/* texture WILL for sure resize, so just get rid of it! */
	if (current_texture)
	{
		SDL_DestroyTexture (current_texture);
		current_texture = 0;
	}

	switch (cachemode)
	{
		case 13:
			/* würfel mode often gets a window the horizontal resolution doubled */
			if (window_resized && (width == 640) && (height == 200))
			{
				SDL_SetWindowSize (current_window, 640, 400);
			}
			Console.CurrentMode = 13;
			mode = MODE_320_200;
			break;
		case 0:
			Console.CurrentMode = 100;
			mode = MODE_640_480;
			break;
		case 1:
			Console.CurrentMode = 101;
			mode = MODE_1024_768;
			break;
		default:
			fprintf(stderr, "             invalid graphmode\n");
			exit(-1);
	}
	width = mode_gui_data[mode].width;
	height = mode_gui_data[mode].height;

	/* if the call is due to window resized, do not do any changes to the window */
	if (!window_resized)
	{
		current_fullsceen = fullscreen;

		if (fullscreen)
		{
			if (!current_window)
			{
				current_window = SDL_CreateWindow ("Open Cubic Player",
			                                           0, 0,
			                                           SDL_WINDOW_FULLSCREEN);
				SDL_StartTextInput (current_window);
			} else {
				SDL_SetWindowFullscreen (current_window, SDL_WINDOW_FULLSCREEN);
			}
		} else {
			if (!current_window)
			{
				current_window = SDL_CreateWindow ("Open Cubic Player",
			                                           width, height,
			                                           0);
				SDL_StartTextInput (current_window);
			} else {
				SDL_SetWindowFullscreen (current_window, 0);
				SDL_SetWindowResizable (current_window, false);
				SDL_SetWindowSize (current_window, width, height);
			}
		}
	}

	if (!current_window)
	{
		fprintf (stderr, "[SDL3-video]: SDL_CreateWindow: %s (fullscreen=%d %dx%d)\n", SDL_GetError(), fullscreen, width, height);
		SDL_ClearError();
		exit(1);
	}

	if (!current_renderer)
	{
		current_renderer = SDL_CreateRenderer (current_window, 0);

		if (!current_renderer)
		{
			fprintf (stderr, "[SD2-video]: SDL_CreateRenderer: %s\n", SDL_GetError());
			SDL_ClearError();
			exit(-1);
		} else {
			sdl3_dump_renderer();
		}
	}

	if (!current_texture)
	{
		current_texture = SDL_CreateTexture (current_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
		if (!current_texture)
		{
#ifdef SDL3_DEBUG
			fprintf (stderr, "[SDL3-video]: SDL_CreateTexture: %s\n", SDL_GetError());
#endif
			SDL_ClearError();
			current_texture = SDL_CreateTexture (current_renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
			if (!current_texture)
			{
				fprintf (stderr, "[SDL3-video]: SDL_CreateTexture: %s\n", SDL_GetError());
				SDL_ClearError();
				exit(-1);
			}
		}
	}

	Console.GraphBytesPerLine = width;
	Console.GraphLines        = height;
	Console.TextWidth         = Console.GraphBytesPerLine/8;
	Console.TextHeight        = Console.GraphLines/16;
	Console.CurrentFont       = _8x16;

	plScrRowBytes = Console.TextWidth * 2;

	sdl3_gFlushPal ();

	___push_key(VIRT_KEY_RESIZE);
	return;
}

static int sdl3_SetGraphMode (int high)
{
#ifdef SDL3_DEBUG
	fprintf (stderr, "[SDL3-video] sdl3_SetGraphMode (high=%d)\n", high);
#endif

	if (high>=0)
		set_state = set_state_graphmode;

	if ((high==cachemode)&&(high>=0))
		goto quick;
	cachemode=high;

	if (virtual_framebuffer)
	{
		free (virtual_framebuffer);
		Console.VidMem = virtual_framebuffer = 0;
	}

	if (high<0)
	{
		return 0;
	}

	set_state_graphmode (current_fullsceen, 0, 0, 0);

	Console.VidMem = virtual_framebuffer = malloc (Console.GraphBytesPerLine * Console.GraphLines);

quick:
	if (virtual_framebuffer)
	{
		memset (virtual_framebuffer, 0, Console.GraphBytesPerLine * Console.GraphLines);
	}

	sdl3_gFlushPal ();
	return 0;
}

static int sdl3_vga13 (void)
{
	return sdl3_SetGraphMode (13);
}

static int sdl3_consoleRestore(void)
{
	return 0;
}

static void sdl3_consoleSave(void)
{
}

static void sdl3_DisplaySetupTextMode(void)
{
	while (1)
	{
		uint16_t c;
		memset (virtual_framebuffer, 0, Console.GraphBytesPerLine * Console.GraphLines);
		make_title("sdl3-driver setup", 0);
		swtext_displaystr_cp437(1, 0, 0x07, "1:  font-size:", 14);
		swtext_displaystr_cp437(1, 15, Console.CurrentFont == _8x8   ? 0x0f : sdl3_CurrentFontWanted == _8x8   ? 0x02 : 0x07, "8x8", 3);
		swtext_displaystr_cp437(1, 19, Console.CurrentFont == _8x16  ? 0x0f : sdl3_CurrentFontWanted == _8x16  ? 0x02 : 0x07, "8x16", 4);
		swtext_displaystr_cp437(1, 24, Console.CurrentFont == _16x32 ? 0x0f : sdl3_CurrentFontWanted == _16x32 ? 0x02 : 0x07, "16x32", 5);
/*
		swtext_displaystr_cp437(2, 0, 0x07, "2:  fullscreen: ", 16);
		swtext_displaystr_cp437(3, 0, 0x07, "3:  resolution in fullscreen:", 29);
*/
		swtext_displaystr_cp437(Console.TextHeight - 1, 0, 0x17, "  press the number of the item you wish to change and ESC when done", Console.TextWidth);

		while (!ekbhit())
		{
				framelock();
		}
		c = egetch();

		switch (c)
		{
			case '1':
				/* we can assume that we are in text-mode if we are here */
				sdl3_CurrentFontWanted = Console.CurrentFont = (sdl3_CurrentFontWanted == _8x8) ? _8x16 : (sdl3_CurrentFontWanted == _8x16) ? _16x32 : _8x8;
				set_state_textmode (current_fullsceen, Console.GraphBytesPerLine, Console.GraphLines, 0);
				cfSetProfileInt(cfScreenSec, "fontsize", Console.CurrentFont, 10);
				break;
			case KEY_EXIT:
			case KEY_ESC: return;
		}
	}
}

static const char *sdl3_GetDisplayTextModeName(void)
{
	static char mode[48];
	snprintf(mode, sizeof(mode), "res(%dx%d), font(%s)%s", Console.TextWidth, Console.TextHeight,
		Console.CurrentFont == _8x8 ? "8x8" : Console.CurrentFont == _8x16 ? "8x16" : "16x32", current_fullsceen?" fullscreen":"");
	return mode;
}

struct keytranslate_t
{
	SDL_Keycode SDL;
	uint16_t OCP;
};

struct keytranslate_t translate[] =
{
	{SDLK_BACKSPACE,     KEY_BACKSPACE},
	{SDLK_TAB,           KEY_TAB},
	{SDLK_DELETE,        KEY_DELETE},
	{SDLK_RETURN,        _KEY_ENTER},
	{SDLK_RETURN2,       _KEY_ENTER},
	/*SDLK_PAUSE*/
	{SDLK_ESCAPE,        KEY_ESC},
	{SDLK_SPACE,         ' '},
	{SDLK_EXCLAIM,       '!'},
	{SDLK_DBLAPOSTROPHE, '\"'},
	{SDLK_HASH,          '#'},
	{SDLK_DOLLAR,        '$'},
	{SDLK_AMPERSAND,     '&'},
	{SDLK_APOSTROPHE,    '\''},
	{SDLK_LEFTPAREN,     '('},
	{SDLK_RIGHTPAREN,    ')'},
	{SDLK_ASTERISK,      '*'},
	{SDLK_PLUS,          '+'},
	{SDLK_COMMA,         ','},
	{SDLK_MINUS,         '-'},
	{SDLK_PERIOD,        '.'},
	{SDLK_SLASH,         '/'},
	{SDLK_0,             '0'},
	{SDLK_1,             '1'},
	{SDLK_2,             '2'},
	{SDLK_3,             '3'},
	{SDLK_4,             '4'},
	{SDLK_5,             '5'},
	{SDLK_6,             '6'},
	{SDLK_7,             '7'},
	{SDLK_8,             '8'},
	{SDLK_9,             '9'},
	{SDLK_COLON,         ':'},
	{SDLK_SEMICOLON,     ';'},
	{SDLK_LESS,          '<'},
	{SDLK_EQUALS,        '='},
	{SDLK_GREATER,       '>'},
	{SDLK_QUESTION,      '?'},
	{SDLK_AT,            '@'},
	{65,                 'A'},
	{66,                 'B'},
	{67,                 'C'},
	{68,                 'D'},
	{69,                 'E'},
	{70,                 'F'},
	{71,                 'G'},
	{72,                 'H'},
	{73,                 'I'},
	{74,                 'J'},
	{75,                 'K'},
	{76,                 'L'},
	{77,                 'M'},
	{78,                 'N'},
	{79,                 'O'},
	{80,                 'P'},
	{81,                 'Q'},
	{82,                 'R'},
	{83,                 'S'},
	{84,                 'T'},
	{85,                 'U'},
	{86,                 'V'},
	{87,                 'W'},
	{88,                 'X'},
	{89,                 'Y'},
	{90,                 'Z'},
	{'|',                '|'},
	{SDLK_LEFTBRACKET,   '['},
	{SDLK_BACKSLASH,     '\\'},
	{SDLK_RIGHTBRACKET,  ']'},
	{SDLK_CARET,         '^'},
	{SDLK_UNDERSCORE,    '_'},
	{SDLK_GRAVE,         '`'},
	{SDLK_A,             'a'},
	{SDLK_B,             'b'},
	{SDLK_C,             'c'},
	{SDLK_D,             'd'},
	{SDLK_E,             'e'},
	{SDLK_F,             'f'},
	{SDLK_G,             'g'},
	{SDLK_H,             'h'},
	{SDLK_I,             'i'},
	{SDLK_J,             'j'},
	{SDLK_K,             'k'},
	{SDLK_L,             'l'},
	{SDLK_M,             'm'},
	{SDLK_N,             'n'},
	{SDLK_O,             'o'},
	{SDLK_P,             'p'},
	{SDLK_Q,             'q'},
	{SDLK_R,             'r'},
	{SDLK_S,             's'},
	{SDLK_T,             't'},
	{SDLK_U,             'u'},
	{SDLK_V,             'v'},
	{SDLK_W,             'w'},
	{SDLK_X,             'x'},
	{SDLK_Y,             'y'},
	{SDLK_Z,             'z'},
	{SDLK_KP_0,          '0'},
	{SDLK_KP_1,          '1'},
	{SDLK_KP_2,          '2'},
	{SDLK_KP_3,          '3'},
	{SDLK_KP_4,          '4'},
	{SDLK_KP_5,          '5'},
	{SDLK_KP_6,          '6'},
	{SDLK_KP_7,          '7'},
	{SDLK_KP_8,          '8'},
	{SDLK_KP_9,          '9'},
	{SDLK_KP_PERIOD,     ','},
	{SDLK_KP_DIVIDE,     '/'},
	{SDLK_KP_MULTIPLY,   '*'},
	{SDLK_KP_MINUS,      '-'},
	{SDLK_KP_PLUS,       '+'},
	{SDLK_KP_ENTER,      _KEY_ENTER},
	{SDLK_KP_EQUALS,     '='},
	{SDLK_UP,            KEY_UP},
	{SDLK_DOWN,          KEY_DOWN},
	{SDLK_RIGHT,         KEY_RIGHT},
	{SDLK_LEFT,          KEY_LEFT},
	{SDLK_INSERT,        KEY_INSERT},
	{SDLK_HOME,          KEY_HOME},
	{SDLK_END,           KEY_END},
	{SDLK_PAGEUP,        KEY_PPAGE},
	{SDLK_PAGEDOWN,      KEY_NPAGE},
	{SDLK_F1,            KEY_F(1)},
	{SDLK_F2,            KEY_F(2)},
	{SDLK_F3,            KEY_F(3)},
	{SDLK_F4,            KEY_F(4)},
	{SDLK_F5,            KEY_F(5)},
	{SDLK_F6,            KEY_F(6)},
	{SDLK_F7,            KEY_F(7)},
	{SDLK_F8,            KEY_F(8)},
	{SDLK_F9,            KEY_F(9)},
	{SDLK_F10,           KEY_F(10)},
	{SDLK_F11,           KEY_F(11)},
	{SDLK_F12,           KEY_F(12)},
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
	{SDLK_APOSTROPHE,   '\"'},
	{SDLK_LEFTBRACKET,  '{'},
	{SDLK_RIGHTBRACKET, '}'},
	{SDLK_MINUS,        '_'},
	{SDLK_EQUALS,       '+'},
	{SDLK_A,            'A'},
	{SDLK_B,            'B'},
	{SDLK_C,            'C'},
	{SDLK_D,            'D'},
	{SDLK_E,            'E'},
	{SDLK_F,            'F'},
	{SDLK_G,            'G'},
	{SDLK_H,            'H'},
	{SDLK_I,            'I'},
	{SDLK_J,            'J'},
	{SDLK_K,            'K'},
	{SDLK_L,            'L'},
	{SDLK_M,            'M'},
	{SDLK_N,            'N'},
	{SDLK_O,            'O'},
	{SDLK_P,            'P'},
	{SDLK_Q,            'Q'},
	{SDLK_R,            'R'},
	{SDLK_S,            'S'},
	{SDLK_T,            'T'},
	{SDLK_U,            'U'},
	{SDLK_V,            'V'},
	{SDLK_W,            'W'},
	{SDLK_X,            'X'},
	{SDLK_Y,            'Y'},
	{SDLK_Z,            'Z'},
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
	/*{SDLK_A,            'a'},
	{SDLK_B,            'b'},
	{SDLK_C,            'c'},*/
	{SDLK_D,            KEY_CTRL_D},
	/*{SDLK_E,            'e'},
	{SDLK_F,            'f'},
	{SDLK_G,            'g'},*/
	{SDLK_H,            KEY_CTRL_H},
	/*{SDLK_I,            'i'},*/
	{SDLK_J,            KEY_CTRL_J},
	{SDLK_K,            KEY_CTRL_K},
	{SDLK_L,            KEY_CTRL_L},
	/*{SDLK_M,            'm'},
	{SDLK_N,            'n'},
	{SDLK_O,            'o'},*/
	{SDLK_P,            KEY_CTRL_P},
	{SDLK_Q,            KEY_CTRL_Q},
	/*{SDLK_R,            'r'},*/
	{SDLK_S,            KEY_CTRL_S},
	/*{SDLK_T,            't'},
	{SDLK_U,            'u'},
	{SDLK_V,            'v'},
	{SDLK_W,            'w'},
	{SDLK_X,            'x'},
	{SDLK_Y,            'y'},*/
	{SDLK_Z,            KEY_CTRL_Z},
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
	{SDLK_A,            KEY_ALT_A},
	{SDLK_B,            KEY_ALT_B},
	{SDLK_C,            KEY_ALT_C},
	/*{SDLK_D,            'd'},*/
	{SDLK_E,            KEY_ALT_E},
	/*{SDLK_F,            'f'},*/
	{SDLK_G,            KEY_ALT_G},
	/*{SDLK_H,            'h'},*/
	{SDLK_I,            KEY_ALT_I},
	/*{SDLK_J,            'j'},*/
	{SDLK_K,            KEY_ALT_K},
	{SDLK_L,            KEY_ALT_L},
	{SDLK_M,            KEY_ALT_M},
	/*{SDLK_N,            'n'},*/
	{SDLK_O,            KEY_ALT_O},
	{SDLK_P,            KEY_ALT_P},
	/*{SDLK_Q,            'q'},*/
	{SDLK_R,            KEY_ALT_R},
	{SDLK_S,            KEY_ALT_S},
	/*{SDLK_T,            't'},
	{SDLK_U,            'u'},
	{SDLK_V,            'v'},
	{SDLK_W,            'w'},*/
	{SDLK_X,            KEY_ALT_X},
	/*{SDLK_Y,            'y'},*/
	{SDLK_Z,            KEY_ALT_Z},
	{-1,                0xffff},
};

static int sdl3_HasKey (uint16_t key)
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

struct SDL3ScrTextGUIOverlay_t
{
	unsigned int x;
	unsigned int y;
	unsigned int width;
	unsigned int height;
	unsigned int pitch;
	uint8_t     *data_bgra;

};

static struct SDL3ScrTextGUIOverlay_t **SDL3ScrTextGUIOverlays;
static int                              SDL3ScrTextGUIOverlays_count;
static int                              SDL3ScrTextGUIOverlays_size;

static void *sdl3_TextOverlayAddBGRA (unsigned int x, unsigned int y, unsigned int width, unsigned int height, unsigned int pitch, uint8_t *data_bgra)
{
	struct SDL3ScrTextGUIOverlay_t *e = malloc (sizeof (*e));
	e->x = x;
	e->y = y;
	e->width = width;
	e->height = height;
	e->pitch = pitch;
	e->data_bgra = data_bgra;

	if (SDL3ScrTextGUIOverlays_count == SDL3ScrTextGUIOverlays_size)
	{
		SDL3ScrTextGUIOverlays_size += 10;
		SDL3ScrTextGUIOverlays = realloc (SDL3ScrTextGUIOverlays, sizeof (SDL3ScrTextGUIOverlays[0]) * SDL3ScrTextGUIOverlays_size);
	}
	SDL3ScrTextGUIOverlays[SDL3ScrTextGUIOverlays_count++] = e;

	return e;
}

static void sdl3_TextOverlayRemove (void *handle)
{
	int i;
	for (i=0; i < SDL3ScrTextGUIOverlays_count; i++)
	{
		if (SDL3ScrTextGUIOverlays[i] == handle)
		{
			memmove (SDL3ScrTextGUIOverlays + i, SDL3ScrTextGUIOverlays + i + 1, sizeof (SDL3ScrTextGUIOverlays[0]) * (SDL3ScrTextGUIOverlays_count - i - 1));
			SDL3ScrTextGUIOverlays_count--;
			free (handle);
			return;
		}
	}
	fprintf (stderr, "[SDL3] Warning: sdl3_TextOverlayRemove, handle %p not found\n", handle);
}

static void RefreshScreenGraph(void)
{
	void *pixels;
	int pitch;

	if (!current_texture)
		return;

	if (!virtual_framebuffer)
		return;

	SDL_LockTexture (current_texture, NULL, &pixels, &pitch);

	{
		uint8_t *src=virtual_framebuffer;
		uint8_t *dst_line = (uint8_t *)/*current_surface->*/pixels;
		int Y=0;
		int i;

		uint32_t *dst;

		while (1)
		{
			int j;

			dst = (uint32_t *)dst_line;
			for (j = 0; j < Console.GraphBytesPerLine; j++)
			{
				*(dst++)=sdl3_palette[*(src++)];
			}
			if ((++Y) >= Console.GraphLines)
			{
				break;
			}
			dst_line += /*current_surface->*/pitch;
		}

		for (i=0; i < SDL3ScrTextGUIOverlays_count; i++)
		{
			int ty, y;
			ty = SDL3ScrTextGUIOverlays[i]->y + SDL3ScrTextGUIOverlays[i]->height;
			y = (SDL3ScrTextGUIOverlays[i]->y < 0) ? 0 : SDL3ScrTextGUIOverlays[i]->y;
			for (; y < ty; y++)
			{
				int tx, x;
				uint8_t *src, *dst;

				if (y >= Console.GraphLines) { break; }

				tx = SDL3ScrTextGUIOverlays[i]->x + SDL3ScrTextGUIOverlays[i]->width;
				x = (SDL3ScrTextGUIOverlays[i]->x < 0) ? 0 : SDL3ScrTextGUIOverlays[i]->x;

				src = SDL3ScrTextGUIOverlays[i]->data_bgra + (((y - SDL3ScrTextGUIOverlays[i]->y) * SDL3ScrTextGUIOverlays[i]->pitch + (x - SDL3ScrTextGUIOverlays[i]->x)) << 2);
				dst = (uint8_t *)pixels + (y * pitch) + (x<<2);

				for (; x < tx; x++)
				{
					if (x >= Console.GraphBytesPerLine) { break; }

					if (src[3] == 0)
					{
						src+=4;
						dst+=4;
					} else if (src[3] == 255)
					{
						*(dst++) = *(src++);
						*(dst++) = *(src++);
						*(dst++) = *(src++);
						src++;
						dst++;
					} else{
						//uint8_t b = src[0];
						//uint8_t g = src[1];
						//uint8_t r = src[2];
						uint8_t a = src[3];
						uint8_t na = a ^ 0xff;
						*dst = ((*dst * na) >> 8) + ((*src * a) >> 8); dst++; src++; // b
						*dst = ((*dst * na) >> 8) + ((*src * a) >> 8); dst++; src++; // g
						*dst = ((*dst * na) >> 8) + ((*src * a) >> 8); dst++; src++; // r
						dst++;
						src++;
					}
				}
			}
		}
	}

	SDL_UnlockTexture (current_texture);

	SDL_RenderTexture (current_renderer, current_texture, NULL, NULL);
	SDL_RenderPresent (current_renderer);

	if (Console.CurrentFont == _8x8)
	{
		fontengine_8x8_iterate ();
	} else if (Console.CurrentFont == _8x16)
	{
		fontengine_8x16_iterate ();
	} else if (Console.CurrentFont == _16x32)
	{
		fontengine_16x32_iterate ();
	}
}

static void RefreshScreenText(void)
{
	if (!current_texture)
		return;

	if (!virtual_framebuffer)
		return;

	swtext_cursor_inject();

	RefreshScreenGraph();

	swtext_cursor_eject();
}

static void sdl3VideoTimer(void)
{
	if (Console.CurrentMode <= 8)
	{
		RefreshScreenText();
	} else {
		RefreshScreenGraph();
	}
}

static int ekbhit_sdl3dummy(void)
{
	SDL_Event event;

	while(SDL_PollEvent(&event))
	{
		static int skipone = 0;
		switch (event.type)
		{
			case SDL_EVENT_TERMINATING:
#ifdef SDL3_DEBUG
				fprintf(stderr, "[SDL3-video] SDL_EVENT_WINDOW_CLOSE_REQUESTED\n");
#endif
				___push_key(KEY_EXIT);
				break;

			case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
			{
#ifdef SDL3_DEBUG
				fprintf(stderr, "[SDL3-video] SDL_EVENT_WINDOW_CLOSE_REQUESTED\n");
#endif
				___push_key(KEY_EXIT);
				break;
			}

			case SDL_EVENT_WINDOW_RESIZED:
			{
#ifdef SDL3_DEBUG
				fprintf(stderr, "[SDL3-video] SDL_EVENT_WINDOW_RESIZED\n");
				fprintf(stderr, "              w: %d\n", event.window.data1);
				fprintf(stderr, "              h: %d\n", event.window.data2);
				fprintf(stderr, "              windowID: %d\n", event.window.windowID);
#endif
				break;
			}

			case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
			{
#ifdef SDL3_DEBUG
				fprintf(stderr, "[SDL3-video] SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED\n");
				fprintf(stderr, "              w: %d\n", event.window.data1);
				fprintf(stderr, "              h: %d\n", event.window.data2);
				fprintf(stderr, "              windowID: %d\n", event.window.windowID);
#endif
				if (current_window && (SDL_GetWindowID(current_window) == event.window.windowID))
				{
					Console.CurrentFont = sdl3_CurrentFontWanted;

					if (!current_fullsceen && (Console.CurrentMode == Console.LastTextMode ))
					{
						last_text_height = event.window.data2;
						last_text_width = event.window.data1;
					}
					set_state (current_fullsceen, event.window.data1, event.window.data2, 1);
				} else {
#ifdef SDL3_DEBUG
					fprintf (stderr, "[SDL3-video] we ignored that event, it does not belong to our window...\n");
#endif
				}
				break;
			}

			case SDL_EVENT_MOUSE_BUTTON_DOWN:
			{
#ifdef SDL3_DEBUG
				fprintf(stderr, "[SDL3-video] SDL_EVENT_MOUSE_BUTTON_DOWN\n");
				fprintf(stderr, "              which: %d\n", (int)event.button.which);
				fprintf(stderr, "              button: %d\n", (int)event.button.button);
				fprintf(stderr, "              down: %d\n", (int)event.button.down);
				fprintf(stderr, "              x,y: %d, %d\n", (int)event.button.x, (int)event.button.y);
#endif
				switch (event.button.button)
				{
					case 1:
						___push_key(_KEY_ENTER);
						break;

					case 3:
						set_state (!current_fullsceen, Console.GraphBytesPerLine, Console.GraphLines, 0);
						break;
				}
				break;
			}

			case SDL_EVENT_MOUSE_WHEEL:
			{
				int i;
#ifdef SDL3_DEBUG
				fprintf(stderr, "[SDL3-video] SDL_EVENT_MOUSE_WHEEL\n");
				fprintf(stderr, "              x: %f\n", event.wheel.x);
				fprintf(stderr, "              y: %f\n", event.wheel.y);
#if SDL_VERSION_ATLEAST(3,2,12)
				fprintf(stderr, "              integer_x: %d\n", event.wheel.integer_x);
				fprintf(stderr, "              integer_y: %d\n", event.wheel.integer_y);
#endif
				fprintf(stderr, "              mouse x,y: %f, %f\n", event.wheel.mouse_x, event.wheel.mouse_y);
#endif

#if SDL_VERSION_ATLEAST(3,2,12)
				if (event.wheel.integer_y > 0)
				{
					for (i = 0; i < event.wheel.integer_y; i++)
					{
						___push_key (KEY_UP);
					}
				} else if (event.wheel.integer_y < 0)
				{
					for (i = 0; i > event.wheel.integer_y; i--)
					{
						___push_key (KEY_DOWN);
					}
				}
#else
				{
					int integer_y = (int)round(event.wheel.y);
					if (integer_y > 0)
					{
						for (i = 0; i < integer_y; i++)
						{
							___push_key (KEY_UP);
						}
					} else if (integer_y < 0)
					{
						for (i = 0; i > integer_y; i--)
						{
							___push_key (KEY_DOWN);
						}
					}
				}
#endif
				break;
			}

			case SDL_EVENT_TEXT_EDITING:
#ifdef SDL3_DEBUG
				fprintf(stderr, "[SDL3-video] SDL_EVENT_TEXT_EDITING\n");
				fprintf(stderr, "              edit.text=\"");
				{
					int i;
					for (i=0;i<event.edit.start;i++)
					{
						fprintf (stderr, "(%c)", event.edit.text[i]);
					}
					for (i=0;i<event.edit.length;i++)
					{
						fprintf (stderr, "%c", event.edit.text[event.edit.start+i]);
					}
				}
				fprintf (stderr, "\"\n");
#endif
				break;
			case SDL_EVENT_TEXT_INPUT:
#ifdef SDL3_DEBUG
				fprintf(stderr, "[SDL3-video] SDL_EVENT_TEXT_INPUT\n");
				fprintf(stderr, "              text=\"%s\"\n", event.text.text);
#endif
				if (skipone)
				{
#ifdef SDL3_DEBIG
					fprintf (stderr, "              skipone hit..\n");
#endif
					skipone=0;
				} else {
					int i;
					for (i=0; event.text.text[i]; i++)
					{
						___push_key((uint8_t)event.text.text[i]);
					}
				}
				break;
			case SDL_EVENT_KEY_UP:
			{
#ifdef SDL3_DEBUG
				fprintf(stderr, "[SDL3-video] SDL_EVENT_KEY_UP\n");
				fprintf(stderr, "              key.scancode=0x%08x\n", (int)event.key.scancode);
				fprintf(stderr, "              mod: 0x%04x%s%s%s%s%s%s%s%s%s%s%s\n", (int)event.key.mod,
						(event.key.mod & SDL_KMOD_LSHIFT) ? " SDL_KMOD_LSHIFT":"",
						(event.key.mod & SDL_KMOD_RSHIFT) ? " SDL_KMOD_RSHIFT":"",
						(event.key.mod & SDL_KMOD_LCTRL) ? " SDL_KMOD_LCTRL":"",
						(event.key.mod & SDL_KMOD_RCTRL) ? " SDL_KMOD_RCTRL":"",
						(event.key.mod & SDL_KMOD_LALT) ? " SDL_KMOD_LALT":"",
						(event.key.mod & SDL_KMOD_RALT) ? " SDL_KMOD_RALT":"",
						(event.key.mod & SDL_KMOD_LGUI) ? " SDL_KMOD_LGUI":"",
						(event.key.mod & SDL_KMOD_RGUI) ? " SDL_KMOD_RGUI":"",
						(event.key.mod & SDL_KMOD_NUM) ? " SDL_KMOD_NUM":"",
						(event.key.mod & SDL_KMOD_CAPS) ? " SDL_KMOD_CAPS":"",
						(event.key.mod & SDL_KMOD_MODE) ? " SDL_KMOD_MODE":"");
				fprintf(stderr, "              sym: 0x%08x ->%s<-\n", (int)event.key.key, SDL_GetKeyName(event.key.key));
#endif
				break;
			}
			case SDL_EVENT_KEY_DOWN:
			{
				int index;

#ifdef SDL3_DEBUG
				fprintf(stderr, "[SDL3-video] SDL_EVENT_KEY_DOWN\n");
				fprintf(stderr, "              key.scancode=0x%08x\n", (int)event.key.scancode);
				fprintf(stderr, "              mod: 0x%04x%s%s%s%s%s%s%s%s%s%s%s\n", (int)event.key.mod,
						(event.key.mod & SDL_KMOD_LSHIFT) ? " SDL_KMOD_LSHIFT":"",
						(event.key.mod & SDL_KMOD_RSHIFT) ? " SDL_KMOD_RSHIFT":"",
						(event.key.mod & SDL_KMOD_LCTRL) ? " SDL_KMOD_LCTRL":"",
						(event.key.mod & SDL_KMOD_RCTRL) ? " SDL_KMOD_RCTRL":"",
						(event.key.mod & SDL_KMOD_LALT) ? " SDL_KMOD_LALT":"",
						(event.key.mod & SDL_KMOD_RALT) ? " SDL_KMOD_RALT":"",
						(event.key.mod & SDL_KMOD_LGUI) ? " SDL_KMOD_LGUI":"",
						(event.key.mod & SDL_KMOD_RGUI) ? " SDL_KMOD_RGUI":"",
						(event.key.mod & SDL_KMOD_NUM) ? " SDL_KMOD_NUM":"",
						(event.key.mod & SDL_KMOD_CAPS) ? " SDL_KMOD_CAPS":"",
						(event.key.mod & SDL_KMOD_MODE) ? " SDL_KMOD_MODE":"");
				fprintf(stderr, "              sym: 0x%08x ->%s<-\n", (int)event.key.key, SDL_GetKeyName(event.key.key));
#endif
				skipone = 0;

				if ((!(event.key.mod & (SDL_KMOD_CTRL|SDL_KMOD_ALT))) && // ignore shift
				      (event.key.key >= 32) && (event.key.key < 127))
				{ // We use SDL_TEXTINPUT event to fetch all these now...
					break;
				}

				if ( (event.key.mod &  SDL_KMOD_CTRL) &&
				     (!(event.key.mod & (SDL_KMOD_SHIFT|SDL_KMOD_ALT))) )
				{
					for (index=0;translate_ctrl[index].OCP!=0xffff;index++)
						if (translate_ctrl[index].SDL==event.key.key)
						{
							___push_key(translate_ctrl[index].OCP);
							skipone = 1;
							break;
						}
					break;
				}

				if ( (event.key.mod & SDL_KMOD_CTRL) &&
				     (event.key.mod & SDL_KMOD_SHIFT) &&
				    (!((event.key.mod & SDL_KMOD_ALT))) )
				{
					for (index=0;translate_ctrl_shift[index].OCP!=0xffff;index++)
						if (translate_ctrl_shift[index].SDL==event.key.key)
						{
							___push_key(translate_ctrl_shift[index].OCP);
							skipone = 1;
							break;
						}
					break;
				}

				if ( (event.key.mod & SDL_KMOD_SHIFT) &&
				     (!(event.key.mod & (SDL_KMOD_CTRL|SDL_KMOD_ALT))) )
				{
					for (index=0;translate_shift[index].OCP!=0xffff;index++)
						if (translate_shift[index].SDL==event.key.key)
						{
							___push_key(translate_shift[index].OCP);
							skipone = 1;
							break;
						}
					/* no break here */
				}

				if ( (event.key.mod & SDL_KMOD_ALT) &&
                                    (!(event.key.mod & (SDL_KMOD_CTRL|SDL_KMOD_SHIFT))) )
				{
					/* TODO, handle ALT-ENTER */
					if (event.key.key==SDLK_RETURN)
					{
						set_state (!current_fullsceen, Console.GraphBytesPerLine, Console.GraphLines, 0);
					} else {
						for (index=0;translate_alt[index].OCP!=0xffff;index++)
							if (translate_alt[index].SDL==event.key.key)
							{
								___push_key(translate_alt[index].OCP);
								skipone = 1;
								break;
							}
					}
					break;
				}

				if (event.key.mod & (SDL_KMOD_CTRL|SDL_KMOD_SHIFT|SDL_KMOD_ALT))
				{
					break;
				}

#ifdef SDL3_DEBUG
				fprintf (stderr, "KEY: %d 0x%x\n", event.key.key, event.key.key);
#endif
				for (index=0;translate[index].OCP!=0xffff;index++)
				{
					if (translate[index].SDL==event.key.key)
					{
						___push_key(translate[index].OCP);
						skipone = 1;
						break;
					}
				}
				break;
			}
		}
	}

	return 0;
}

static void sdl3_gFlushPal(void)
{
}

static void sdl3_gUpdatePal (uint8_t index, uint8_t _red, uint8_t _green, uint8_t _blue)
{
	uint8_t *pal = (uint8_t *)sdl3_palette;
	pal[(index<<2)+3] = 0xff;
	pal[(index<<2)+2] = _red<<2;
	pal[(index<<2)+1] = _green<<2;
	pal[(index<<2)+0] = _blue<<2;
}

static int need_quit = 0;

int sdl3_init(void)
{
	if (!SDL_Init(/*SDL_INIT_AUDIO|*/SDL_INIT_VIDEO))
	{
		fprintf(stderr, "[SDL3 video] Unable to init SDL: %s\n", SDL_GetError());
		SDL_ClearError();
		return 1;
	}

	if (fontengine_init())
	{
		SDL_Quit();
		return 1;
	}

	/* we now test-spawn one window, and so we can fallback to other drivers */
	current_window = SDL_CreateWindow ("Open Cubic Player detection",
                                           320, 200,
                                           0);

	if (!current_window)
	{
		fprintf(stderr, "[SDL3 video] Unable to create window: %s\n", SDL_GetError());
		goto error_out;
	}

	current_renderer = SDL_CreateRenderer (current_window, 0);
	if (!current_renderer)
	{
		fprintf (stderr, "[SD2-video]: Unable to create renderer: %s\n", SDL_GetError());
		goto error_out;
	}

	current_texture = SDL_CreateTexture (current_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 320, 200);
	if (!current_texture)
	{
		fprintf (stderr, "[SDL3-video]: Unable to create texture (will do one more attempt): %s\n", SDL_GetError());
		SDL_ClearError();
		current_texture = SDL_CreateTexture (current_renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, 320, 200);
		if (!current_texture)
		{
			fprintf (stderr, "[SDL3-video]: Unable to create texture: %s\n", SDL_GetError());
			goto error_out;
		}
	}

	sdl3_close_window ();

	SDL_SetEventEnabled (SDL_EVENT_TERMINATING, true);
	SDL_SetEventEnabled (SDL_EVENT_WINDOW_CLOSE_REQUESTED, true);
	SDL_SetEventEnabled (SDL_EVENT_WINDOW_RESIZED, true);
	SDL_SetEventEnabled (SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED, true);
	SDL_SetEventEnabled (SDL_EVENT_MOUSE_BUTTON_DOWN, true);
	SDL_SetEventEnabled (SDL_EVENT_MOUSE_WHEEL, true);
	SDL_SetEventEnabled (SDL_EVENT_TEXT_EDITING, true);
	SDL_SetEventEnabled (SDL_EVENT_TEXT_INPUT, true);
	SDL_SetEventEnabled (SDL_EVENT_KEY_UP, true);
	SDL_SetEventEnabled (SDL_EVENT_KEY_DOWN, true);

	/* Fill some default font and window sizes */
	sdl3_CurrentFontWanted = Console.CurrentFont = cfGetProfileInt(cfScreenSec, "fontsize", _8x16, 10);

	if (Console.CurrentFont > _FONT_MAX)
	{
		Console.CurrentFont = _16x32;
	}

	last_text_width  = Console.GraphBytesPerLine = saturate(cfGetProfileInt(cfScreenSec, "winwidth",  1280, 10), 640, 16384);
	last_text_height = Console.GraphLines        = saturate(cfGetProfileInt(cfScreenSec, "winheight", 1024, 10), 400, 16384);
	Console.LastTextMode = Console.CurrentMode = cfGetProfileInt(cfScreenSec, "screentype", 8, 10);

	need_quit = 1;

	Console.Driver = &sdl3ConsoleDriver;
	___setup_key(ekbhit_sdl3dummy, ekbhit_sdl3dummy);
	pollInit (sdl3VideoTimer, pollTypeVideo);

	Console.TextGUIOverlay = 1;

	Console.VidType = vidModern;

	return 0;
error_out:
	SDL_ClearError();
	sdl3_close_window ();
	fontengine_done ();
	SDL_Quit();
	return 1;
}

void sdl3_done(void)
{
	pollClose (pollTypeVideo);

	sdl3_close_window ();

	if (!need_quit)
		return;

	fontengine_done ();

	SDL_Quit();

	if (virtual_framebuffer)
	{
		free (virtual_framebuffer);
		Console.VidMem = virtual_framebuffer = 0;
	}

	need_quit = 0;

	free (SDL3ScrTextGUIOverlays);
	SDL3ScrTextGUIOverlays = 0;
	SDL3ScrTextGUIOverlays_size = 0;
	SDL3ScrTextGUIOverlays_count = 0;
}

static void sdl3_DosShell (void)
{
	/* dummy */
}

static const struct consoleDriver_t sdl3ConsoleDriver =
{
	sdl3_vga13,
	sdl3_SetTextMode,
	sdl3_DisplaySetupTextMode,
	sdl3_GetDisplayTextModeName,
	swtext_measurestr_utf8,
	swtext_displaystr_utf8,
	swtext_displaychr_cp437,
	swtext_displaystr_cp437,
	swtext_displaystrattr_cp437,
	swtext_displayvoid,
	swtext_drawbar,
	swtext_idrawbar,
	sdl3_TextOverlayAddBGRA,
	sdl3_TextOverlayRemove,
	sdl3_SetGraphMode,
	generic_gdrawchar,
	generic_gdrawcharp,
	generic_gdrawchar8,
	generic_gdrawchar8p,
	generic_gdrawstr,
	generic_gupdatestr,
	sdl3_gUpdatePal,
	sdl3_gFlushPal,
	sdl3_HasKey,
	swtext_setcur,
	swtext_setcurshape,
	sdl3_consoleRestore,
	sdl3_consoleSave,
	sdl3_DosShell
};
