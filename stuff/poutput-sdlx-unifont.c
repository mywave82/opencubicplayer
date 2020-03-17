static int displaycharattr_unifont_8x16(uint16_t y, uint16_t x, const uint32_t codepoint, uint8_t attr, int width_left)
{
	uint8_t *target;
	int i, j;
	uint8_t *cp;
	uint8_t f, b;
	int fontwidth;

	target = virtual_framebuffer + y * 16 * plScrLineBytes + x * 8;

	cp = fontengine_8x16 (codepoint, &fontwidth);

	if (fontwidth == 16)
	{
		if (width_left >= 2)
		{
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
			return 2;
		} else { /* we can only fit the first half */
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
			return 1;
		}
	} else if (fontwidth == 8)
	{
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
		return 1;
	}
	return 0;
}

static void displaystrattr_unifont_8x16(uint16_t y, uint16_t x, const uint16_t *buf, uint16_t len, const uint16_t *codepage)
{
	while (len)
	{
		if (x >= plScrWidth) return;
		/* all these codepoints should always use only one CELL */
		/*used =*/ displaycharattr_unifont_8x16 (y, x, codepage[(*buf)&0x0ff], plpalette[((*buf)>>8)], len);
		x += 1;
		len -= 1;
		buf++;
	}
}

static void displaystr_unifont_8x16(uint16_t y, uint16_t x, uint8_t attr, const char *str, uint16_t len, const uint16_t *codepage)
{
	while (len)
	{
		if (x >= plScrWidth) return;
		/* all these codepoints should always use only one CELL */
		/*used =*/ displaycharattr_unifont_8x16 (y, x, codepage[*(uint8_t *)str], attr, len);
		x += 1;
		len -= 1;
		if (*str) str++;
	}
}

