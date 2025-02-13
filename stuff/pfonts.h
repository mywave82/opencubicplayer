#ifndef _PFONTS_H
#define _PFONTS_H

/* cp437_to_unicode gives lookup for these - with some few exceptions TODO*/
extern unsigned char plFont88[256][8];
extern unsigned char plFont816[256][16];
extern unsigned char plFont1632[256][64];

struct FontData_8x8_t
{
	uint16_t codepoint;
	uint8_t data[8];
};

struct FontData_8x16_t
{
	uint16_t codepoint;
	uint8_t data[16];
};

struct FontData_16x32_t
{
	uint16_t codepoint;
	uint8_t data[64];
};

extern const struct FontData_8x8_t   plFont_8x8_latin1_addons  [41];
extern const struct FontData_8x16_t  plFont_8x16_latin1_addons [41];
extern const struct FontData_16x32_t plFont_16x32_latin1_addons[41];

#endif
