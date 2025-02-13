#ifndef _STUFF_POUTOUT_FONTENGINE_H
#define _STUFF_POUTOUT_FONTENGINE_H 1

struct font_entry_8x8_t
{
	uint32_t codepoint;
	//char code[6+1];
	unsigned char width;
	/* for 8 lines font, we have have 1 bit per pixel */
	unsigned char data[16]; /* we fit up to 16 by 8 pixels */
	uint8_t score;
};

struct font_entry_8x16_t
{
	uint32_t codepoint;
	//char code[6+1];
	unsigned char width; /* 8 or 16 */
	/* for 16 lines font, we have have 1 bit per pixel */
	unsigned char data[32]; /* we fit up to 16 by 16 pixels */
	uint8_t score;
};

struct font_entry_16x32_t
{
	uint32_t codepoint;
	//char code[6+1];
	unsigned char width; /* 8 or 16 */
	/* for 16 lines font, we have have 1 bit per pixel */
	unsigned char data[128]; /* we fit up to 32 by 32 pixels */
	uint8_t score;
};

extern struct font_entry_8x8_t   cp437_8x8  [256];
extern struct font_entry_8x16_t  cp437_8x16 [256];
extern struct font_entry_16x32_t cp437_16x32[256];

/* age the cache */
void fontengine_8x8_iterate (void);
void fontengine_8x16_iterate (void);
void fontengine_16x32_iterate (void);

uint8_t *fontengine_8x8(uint32_t codepoint, int *width);
uint8_t *fontengine_8x16(uint32_t codepoint, int *width);
uint8_t *fontengine_16x32(uint32_t codepoint, int *width);

/* used by fontdebug only */
int fontengine_8x8_forceunifont (uint32_t codepoint, int *width, uint8_t buffer[16]);
int fontengine_8x16_forceunifont (uint32_t codepoint, int *width, uint8_t buffer[32]);
int fontengine_16x32_forceunifont (uint32_t codepoint, int *width, uint8_t buffer[128]);

int fontengine_init (void);

void fontengine_done (void);

#endif
