/* Based on SDL_ttf: Copyright (C) 2001-2020 Sam Lantinga <slouken@libsdl.org> */

#ifndef _STUFF_TTF_H_
#define _STUFF_TTF_H_

typedef struct
{
	int w, h, pitch;
	uint8_t pixels[];
} TTF_Surface;

typedef struct _TTF_Font TTF_Font;

int TTF_Init(void);
void TTF_Quit(void);


TTF_Font * TTF_OpenFontFilename(const char *filename, int ptsize, long index, unsigned int hdpi, unsigned int vdpi);
TTF_Font * TTF_OpenFontFILE(FILE *src, int ptsize, long index, unsigned int hdpi, unsigned int vdpi);
void TTF_CloseFont(TTF_Font *font);

int TTF_SetFontSizeDPI(TTF_Font *font, int ptsize, unsigned int hdpi, unsigned int vdpi);

TTF_Surface * TTF_RenderGlyph32_Shaded(TTF_Font *font, uint32_t ch);

const char *TTF_GetError (void);
void TTF_ClearError (void);

#endif
