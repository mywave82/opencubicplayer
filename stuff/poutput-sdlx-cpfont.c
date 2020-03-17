static void displaycharattr_cpfont_8x8(uint16_t y, uint16_t x, const uint8_t ch, uint8_t attr)
{
	uint8_t *target;
	int i, j;
	uint8_t *cp;
	uint8_t f, b;

	target = virtual_framebuffer + y * 8 * plScrLineBytes + x * 8;

	cp = plFont88[ch];
	f = attr & 0x0f;
	b = attr >> 4;

	for (i=0; i < 8; i++)
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

/* codepage is optional, to translate...*/
static void displaystrattr_cpfont_8x8(uint16_t y, uint16_t x, const uint16_t *buf, uint16_t len, const uint8_t *codepage)
{
	while (len)
	{
		uint8_t ch;

		if (x >= plScrWidth) return;

		ch = (*buf)&0x0ff;
		if (codepage)
		{
			ch = codepage[ch];
		}
		displaycharattr_cpfont_8x8 (y, x, ch, plpalette[((*buf)>>8)]);
		buf++;
		len--;
		x++;
	}
}

static void displaystr_cpfont_8x8(uint16_t y, uint16_t x, uint8_t attr, const char *str, uint16_t len, const uint8_t *codepage)
{
	while (len)
	{
		uint8_t ch;

		if (x >= plScrWidth) return;

		ch = *str;
		if (codepage)
		{
			ch = codepage[ch];
		}

		displaycharattr_cpfont_8x8 (y, x, ch, attr);

		if (*str) str++;
		len--;
		x++;
	}
}

static void displaycharattr_cpfont_4x4(uint16_t y, uint16_t x, const uint8_t ch, uint8_t attr)
{
	uint8_t *target;
	int i, j;
	uint8_t *cp;
	uint8_t f, b;

	target = virtual_framebuffer + y * 4 * plScrLineBytes + x * 4;

	cp = plFont44[ch];
	f = attr & 0x0f;
	b = attr >> 4;

	for (i=0; i < 2; i++)
	{ /* we get two lines of data per byte in the font */
		uint8_t bitmap=*cp++;
		for (j=0; j < 4; j++)
		{
			*target++=(bitmap&128)?f:b;
			bitmap<<=1;
		}
		target -= 4;
		target += plScrLineBytes;
		for (j=0; j < 4; j++)
		{
			*target++=(bitmap&128)?f:b;
			bitmap<<=1;
		}
		target -= 4;
		target += plScrLineBytes;
	}
}

static void displaystrattr_cpfont_4x4(uint16_t y, uint16_t x, const uint16_t *buf, uint16_t len, const uint8_t *codepage)
{
	while (len)
	{
		uint8_t ch;

		if (x >= plScrWidth) return;

		ch = (*buf)&0x0ff;
		if (codepage)
		{
			ch = codepage[ch];
		}
		displaycharattr_cpfont_4x4 (y, x, ch, plpalette[((*buf)>>8)]);
		buf++;
		len--;
		x++;
	}
}

static void displaystr_cpfont_4x4(uint16_t y, uint16_t x, uint8_t attr, const char *str, uint16_t len, const uint8_t *codepage)
{
	while (len)
	{
		uint8_t ch;

		if (x >= plScrWidth) return;

		ch = *str;
		if (codepage)
		{
			ch = codepage[ch];
		}

		displaycharattr_cpfont_4x4 (y, x, ch, attr);

		if (*str) str++;
		len--;
		x++;
	}
}
