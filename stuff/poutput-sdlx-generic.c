static void displayvoid(uint16_t y, uint16_t x, uint16_t len)
{
	uint8_t *target;
	unsigned int length;
	unsigned int count;
	int i;
	switch (plCurrentFont)
	{
		default:
		case _8x16:
			target = virtual_framebuffer + y * 16 * plScrLineBytes + x * 8;
			length = len * 8;
			count = 16;
			break;
		case _8x8:
			target = virtual_framebuffer + y * 8 * plScrLineBytes + x * 8;
			length = len * 8;
			count = 8;
			break;
		case _4x4:
			target = virtual_framebuffer + y * 4 * plScrLineBytes + x * 4;
			length = len * 4;
			count = 4;
			break;
	}
	for (i=0; i < count; i++)
	{
		memset (target, 0, length);
		target += plScrLineBytes;
	}
}

static void displaystrattr_cp437(uint16_t y, uint16_t x, const uint16_t *buf, uint16_t len)
{
	switch (plCurrentFont)
	{
		case _8x16:
			displaystrattr_unifont_8x16 (y, x, buf, len, 0);
			break;
		case _8x8:
			displaystrattr_cpfont_8x8 (y, x, buf, len, 0);
			break;
		case _4x4:
			displaystrattr_cpfont_4x4 (y, x, buf, len, 0);
			break;
	}
}

static void displaystrattr_iso8859latin1(uint16_t y, uint16_t x, const uint16_t *buf, uint16_t len)
{
	switch (plCurrentFont)
	{
		case _8x16:
			displaystrattr_unifont_8x16 (y, x, buf, len, latin1_to_unicode);
			break;
		case _8x8:
			displaystrattr_cpfont_8x8 (y, x, buf, len, latin1_table);
			break;
		case _4x4:
			displaystrattr_cpfont_4x4 (y, x, buf, len, latin1_table);
			break;
	}
}


static void displaystr_cp437(uint16_t y, uint16_t x, uint8_t attr, const char *str, uint16_t len)
{
	switch (plCurrentFont)
	{
		case _8x16:
			displaystr_unifont_8x16 (y, x, attr, str, len, 0);
			break;
		case _8x8:
			displaystr_cpfont_8x8 (y, x, attr, str, len, 0);
			break;
		case _4x4:
			displaystr_cpfont_4x4 (y, x, attr, str, len, 0);
			break;
	}
}

static void displaystr_iso8859latin1(uint16_t y, uint16_t x, uint8_t attr, const char *str, uint16_t len)
{
	switch (plCurrentFont)
	{
		case _8x16:
			displaystr_unifont_8x16 (y, x, attr, str, len, latin1_to_unicode);
			break;
		case _8x8:
			displaystr_cpfont_8x8 (y, x, attr, str, len, latin1_table);
			break;
		case _4x4:
			displaystr_cpfont_4x4 (y, x, attr, str, len, latin1_table);
			break;
	}
}

static void drawbar(uint16_t x, uint16_t yb, uint16_t yh, uint32_t hgt, uint32_t c)
{
	int yh1, yh2, yh3;

	uint8_t *target, f, b;
	int i;
	int font_width;
	int font_height;

	if (hgt>((yh*(unsigned)16)-4))
		hgt=(yh*16)-4;

	yh1=(yh+2)/3;
	yh2=(yh+yh1+1)/2 - yh1;
	yh3=yh-yh1-yh2;

	switch (plCurrentFont)
	{
		default:
		case _8x16:
			font_width = 8;
			font_height = 16;
			break;
		case _8x8:
			font_width = 8;
			font_height = 8;
			hgt >>= 1;
			break;
		case _4x4:
			font_width = 4;
			font_height = 4;
			hgt >>= 2;
			break;
	}
	target = virtual_framebuffer + ((yb + 1) * font_height - 1) * plScrLineBytes + x * font_width;
	f = c & 0x0f;
	b = (c >> 4) & 0x0f;
	for (i = yh1 * font_height; i > 0; i--)
	{
		if (hgt > 0)
		{
			memset (target, f, font_width-1);
			target[font_width-1] = b;
			hgt--;
		} else {
			memset (target, b, font_width);
		}
		target -= plScrLineBytes;
	}
	c>>=8;
	f = c & 0x0f;
	b = (c >> 4) & 0x0f;
	for (i = yh2 * font_height; i > 0; i--)
	{
		if (hgt > 0)
		{
			memset (target, f, font_width-1);
			target[font_width-1] = b;
			hgt--;
		} else {
			memset (target, b, font_width);
		}
		target -= plScrLineBytes;
	}
	c>>=8;
	f = c & 0x0f;
	b = (c >> 4) & 0x0f;
	for (i = yh3 * font_height; i > 0; i--)
	{
		if (hgt > 0)
		{
			memset (target, f, font_width-1);
			target[font_width-1] = b;
			hgt--;
		} else {
			memset (target, b, font_width);
		}
		target -= plScrLineBytes;
	}
}

static void idrawbar(uint16_t x, uint16_t yb, uint16_t yh, uint32_t hgt, uint32_t c)
{
	int yh1, yh2, yh3;

	uint8_t *target, f, b;
	int i;
	int font_width;
	int font_height;

	if (hgt>((yh*(unsigned)16)-4))
		hgt=(yh*16)-4;

	yh1=(yh+2)/3;
	yh2=(yh+yh1+1)/2 - yh1;
	yh3=yh-yh1-yh2;

	switch (plCurrentFont)
	{
		default:
		case _8x16:
			font_width = 8;
			font_height = 16;
			break;
		case _8x8:
			font_width = 8;
			font_height = 8;
			hgt >>= 1;
			break;
		case _4x4:
			font_width = 4;
			font_height = 4;
			hgt >>= 2;
			break;
	}
	target = virtual_framebuffer + (yb - yh + 1) * font_height * plScrLineBytes + x * font_width;
	f = c & 0x0f;
	b = (c >> 4) & 0x0f;
	for (i = yh1 * font_height; i > 0; i--)
	{
		if (hgt > 0)
		{
			memset (target, f, font_width-1);
			target[font_width-1] = b;
			hgt--;
		} else {
			memset (target, b, font_width);
		}
		target += plScrLineBytes;
	}
	c>>=8;
	f = c & 0x0f;
	b = (c >> 4) & 0x0f;
	for (i = yh2 * font_height; i > 0; i--)
	{
		if (hgt > 0)
		{
			memset (target, f, font_width-1);
			target[font_width-1] = b;
			hgt--;
		} else {
			memset (target, b, font_width);
		}
		target += plScrLineBytes;
	}
	c>>=8;
	f = c & 0x0f;
	b = (c >> 4) & 0x0f;
	for (i = yh3 * font_height; i > 0; i--)
	{
		if (hgt > 0)
		{
			memset (target, f, font_width-1);
			target[font_width-1] = b;
			hgt--;
		} else {
			memset (target, b, font_width);
		}
		target += plScrLineBytes;
	}
}
