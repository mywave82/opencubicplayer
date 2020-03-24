#if 0
//not needed until we add UTF-8 support
static void displaycharattr_double8x16(uint16_t y, uint16_t x, uint8_t *cp, uint8_t attr)
{
	uint8_t *target;
	int i, j;
	uint8_t f, b;

	target = virtual_framebuffer + y * 16 * plScrLineBytes + x * 8;

	f = attr & 0x0f;
	b = attr >> 4;

	for (i=0; i < 16; i++)
	{
		uint8_t bitmap=*cp++;
		for (j=0; j < 8; j++)
		{
			*target++=(bitmap&128)?f:b;
			bitmap<<=1;
		}
		bitmap=*cp++;
		for (j=0; j < 8; j++)
		{
			*target++=(bitmap&128)?f:b;
			bitmap<<=1;
		}
		target -= 16;
		target += plScrLineBytes;
	}
}

static void displaycharattr_doublefirsthalf8x16(uint16_t y, uint16_t x, uint8_t *cp, uint8_t attr)
{
	uint8_t *target;
	int i, j;
	uint8_t f, b;

	target = virtual_framebuffer + y * 16 * plScrLineBytes + x * 8;

	f = attr & 0x0f;
	b = attr >> 4;

	for (i=0; i < 16; i++)
	{
		uint8_t bitmap=*cp++;
		cp++; /* skip one byte of the source font bitmap */
		for (j=0; j < 8; j++)
		{
			*target++=(bitmap&128)?f:b;
			bitmap<<=1;
		}
		target -= 8;
		target += plScrLineBytes;
	}
}
#endif

static void displaycharattr_single8x16(uint16_t y, uint16_t x, uint8_t *cp, uint8_t attr)
{
	uint8_t *target;
	int i, j;
	uint8_t f, b;

	target = virtual_framebuffer + y * 16 * plScrLineBytes + x * 8;

	f = attr & 0x0f;
	b = attr >> 4;

	for (i=0; i < 16; i++)
	{
		uint8_t bitmap=*cp++;
		for (j=0; j < 8; j++)
		{
			*target++=(bitmap&128)?f:b;
			bitmap<<=1;
		}
		target -= 8;
		target += plScrLineBytes;
	}
}

#if 0
//not needed until we add UTF-8 support
static int displaycharattr_unifont_8x16(uint16_t y, uint16_t x, const uint32_t codepoint, uint8_t attr, int width_left)
{
	uint8_t *cp;
	int fontwidth;

	cp = fontengine_8x16 (codepoint, &fontwidth);

	if (fontwidth == 16)
	{
		if (width_left >= 2)
		{
			displaycharattr_double8x16 (y, x, cp, attr);
			return 2;
		} else { /* we can only fit the first half */
			displaycharattr_doublefirsthalf8x16 (y, x, cp, attr);
			return 1;
		}
	} else if (fontwidth == 8)
	{
		displaycharattr_single8x16 (y, x, cp, attr);
		return 1;
	}
	return 0;
}
#endif

static void displaystrattr_unifont_8x16(uint16_t y, uint16_t x, const uint16_t *buf, uint16_t len, const uint16_t *codepage)
{
	if (codepage)
	{
		while (len)
		{
			uint8_t *cp;
			int fontwidth;

			if (x >= plScrWidth) return;

			cp = fontengine_8x16 (codepage[(*buf)&0x0ff], &fontwidth);
			/* all these codepoints should always use only one CELL */
			displaycharattr_single8x16 (y, x, cp, plpalette[((*buf)>>8)]);
			x += 1;
			len -= 1;
			buf++;
		}
	} else { /* codepage == NULL => optimization, ocp-cp437 is in the start of the unifont cache */
		while (len)
		{
			if (x >= plScrWidth) return;
			/* all these codepoints should always use only one CELL */
			displaycharattr_single8x16 (y, x, font_entries[(*buf)&0x0ff]->data, plpalette[((*buf)>>8)]);
			x += 1;
			len -= 1;
			buf++;
		}
	}
}

static void displaystr_unifont_8x16(uint16_t y, uint16_t x, uint8_t attr, const char *str, uint16_t len, const uint16_t *codepage)
{
	if (codepage)
	{
		while (len)
		{
			uint8_t *cp;
			int fontwidth;

			if (x >= plScrWidth) return;

			/* all these codepoints should always use only one CELL */
			cp = fontengine_8x16 (codepage[*(uint8_t *)str], &fontwidth);
			displaycharattr_single8x16 (y, x, cp, attr);
			x += 1;
			len -= 1;
			if (*str) str++;
		}
	} else { /* codepage == NULL => optimization, ocp-cp437 is in the start of the unifont cache */
		while (len)
		{

			if (x >= plScrWidth) return;

			/* all these codepoints should always use only one CELL */
			displaycharattr_single8x16 (y, x, font_entries[*(uint8_t *)str]->data, attr);
			x += 1;
			len -= 1;
			if (*str) str++;
		}
	}
}
