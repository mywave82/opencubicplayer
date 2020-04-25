/* OpenCP Module Player
 * copyright (c) 2020 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * ID3 TAG Picture viewer
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

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "boot/psetting.h"
#include "cpiface/cpiface.h"
#include "stuff/poutput.h"
#include "id3.h"
#include "id3jpeg.h"
#include "id3png.h"
#include "mpplay.h"

static int ID3PicActive;  /* requested mode from the user */
static int ID3PicVisible; /* are we actually visible? */

static int ID3PicFirstColumn;
static int ID3PicFirstLine;
static int ID3PicHeight;
static int ID3PicWidth;
static int ID3PicMaxHeight;
static int ID3PicMaxWidth;
static int ID3PicFontSizeX;
static int ID3PicFontSizeY;

static void *ID3PicHandle;
static int ID3PicLastSerial;
static int ID3PicCurrentIndex;

struct ID3_pic_raw_t
{
	uint16_t  real_width;
	uint16_t  real_height;
	uint8_t  *real_data_bgra;

	uint16_t  scaled_width;
	uint16_t  scaled_height;
	uint8_t  *scaled_data_bgra;
};

static struct ID3_pic_raw_t ID3Pictures[sizeof(((struct ID3_t *)0)->APIC) / sizeof(((struct ID3_t *)0)->APIC[0])];

static void Free_ID3Pictures(void)
{
	int i;
	for (i=0; i < (sizeof (ID3Pictures) / sizeof (ID3Pictures[0])); i++)
	{
		free (ID3Pictures[i].real_data_bgra);
		free (ID3Pictures[i].scaled_data_bgra);
	}
	memset (ID3Pictures, 0, sizeof (ID3Pictures));
}

static void ID3Picture_ScaleUp(struct ID3_pic_raw_t *srcdst, int factor)
{
	int x, y;
	int sx, sy;
	uint32_t *src, *dst;
	srcdst->scaled_width = srcdst->real_width * factor;
	srcdst->scaled_height = srcdst->real_height * factor;
	srcdst->scaled_data_bgra = malloc (srcdst->scaled_width * srcdst->scaled_height * 4);
	src = (uint32_t *)srcdst->real_data_bgra;
	dst = (uint32_t *)srcdst->scaled_data_bgra;
	for (y = 0; y < srcdst->real_height; y++)
	{
		uint8_t *origdst = (uint8_t *)dst;
		int len;
		for (x = 0; x < srcdst->real_width; x++)
		{
			for (sx = 0; sx < factor; sx++)
			{
				*(dst++) = *src;
			}
			src++;
		}
		len = ((uint8_t *)dst) - origdst;
		for (sy = 1; sy < factor; sy++)
		{
			memcpy (dst, origdst, len);
			dst = (uint32_t *)((uint8_t *)dst + len);
		}
	}
}

static void ID3Picture_ScaleDown(struct ID3_pic_raw_t *srcdst, int factor)
{
	int x, y;
	int sx, sy;
	uint8_t *src, *dst;
	srcdst->scaled_width = (srcdst->real_width + factor - 1) / factor;
	srcdst->scaled_height = (srcdst->real_height + factor - 1) / factor;
	srcdst->scaled_data_bgra = malloc (srcdst->scaled_width * srcdst->scaled_height * 4);
	dst = srcdst->scaled_data_bgra;

	for (y = 0; y < srcdst->scaled_height; y++)
	{
		int _tempy = y * factor;
		for (x = 0; x < srcdst->scaled_width; x++)
		{
			uint16_t s1 = 0, s2 = 0, s3 = 0, s4 = 0;
			int count = 0;
			int tempy = _tempy;
			int _tempx = x * factor;

			for (sy = 0; sy < factor; sy++, tempy++)
			{
				int tempx = _tempx;

				if (tempy >= srcdst->real_height)
				{
					break;
				}

				src = srcdst->real_data_bgra + (tempy * srcdst->real_width + tempx) * 4;

				for (sx = 0; sx < factor; sx++, tempx++)
				{
					if (tempx >= srcdst->real_width)
					{
						break;
					}
					s1 += *(src++);
					s2 += *(src++);
					s3 += *(src++);
					s4 += *(src++);
					count++;
				}
			}

			s1 /= count;
			s2 /= count;
			s3 /= count;
			s4 /= count;
			*(dst++)=s1;
			*(dst++)=s2;
			*(dst++)=s3;
			*(dst++)=s4;
		}
	}
}

static void ID3Picture_Scale(struct ID3_pic_raw_t *srcdst, int width, int height)
{
	int i;

	for (i = 1; ; i++)
	{
		if ((srcdst->real_width * i) > width)
		{
			/* scaled too high up */
			break;
		}
		if ((srcdst->real_height * i) > height)
		{
			/* scaled too high up */
			break;
		}
	}
	i--; /* revert the failed step */

	if (i > 1)
	{
		if ((srcdst->real_width  * i == srcdst->scaled_width ) &&
		    (srcdst->real_height * i == srcdst->scaled_height))
		{
			return;
		}
		free (srcdst->scaled_data_bgra); srcdst->scaled_data_bgra = 0;
		srcdst->scaled_width = 0;
		srcdst->scaled_height = 0;
		ID3Picture_ScaleUp(srcdst, i);
		return;
	}

	for (i = 1; ; i++)
	{
		/* Are we still too wide */
		if (((srcdst->real_width + i - 1) / i) > width)
		{
			continue;
		}
		/* Are we still too high? */
		if (((srcdst->real_height + i - 1) /  i) > height)
		{
			continue;
		}
		break;
	}

	if (i > 1)
	{
		if (((srcdst->real_width  + i - 1) / i == srcdst->scaled_width ) &&
		    ((srcdst->real_height + i - 1) / i == srcdst->scaled_height))
		{
			return;
		}
		free (srcdst->scaled_data_bgra); srcdst->scaled_data_bgra = 0;
		srcdst->scaled_width = 0;
		srcdst->scaled_height = 0;
		ID3Picture_ScaleDown(srcdst, i);
		return;
	}

	free (srcdst->scaled_data_bgra); srcdst->scaled_data_bgra = 0;
	srcdst->scaled_width = 0;
	srcdst->scaled_height = 0;
}

static int Refresh_ID3Pictures (struct ID3_t *ID3)
{
	int i;

	if (ID3->serial == ID3PicLastSerial)
	{
		return 0;
	}
	Free_ID3Pictures();

	ID3PicLastSerial = ID3->serial;

	ID3PicMaxHeight = 0;
	ID3PicMaxWidth = 0;

	for (i=0; i < (sizeof(ID3->APIC) / sizeof (ID3->APIC[0])); i++)
	{
		if (ID3->APIC[i].data && ID3->APIC[i].is_jpeg)
		{
			try_open_jpeg (&ID3Pictures[i].real_width, &ID3Pictures[i].real_height, &ID3Pictures[i].real_data_bgra, ID3->APIC[i].data, ID3->APIC[i].size);
		} else if (ID3->APIC[i].data && ID3->APIC[i].is_png)
		{
			try_open_png (&ID3Pictures[i].real_width, &ID3Pictures[i].real_height, &ID3Pictures[i].real_data_bgra, ID3->APIC[i].data, ID3->APIC[i].size);
		}
		if (ID3Pictures[i].real_width && ID3Pictures[i].real_height && ID3Pictures[i].real_data_bgra)
		{
			if (ID3Pictures[i].real_width  > ID3PicMaxWidth ) ID3PicMaxWidth  = ID3Pictures[i].real_width;
			if (ID3Pictures[i].real_height > ID3PicMaxHeight) ID3PicMaxHeight = ID3Pictures[i].real_height;
		}
	}

	for (i=0; i < (sizeof(ID3->APIC) / sizeof (ID3->APIC[0])); i++)
	{
		if (ID3Pictures[ID3PicCurrentIndex].real_width && ID3Pictures[ID3PicCurrentIndex].real_height && ID3Pictures[ID3PicCurrentIndex].real_data_bgra)
			break;
		ID3PicCurrentIndex++;
		if (ID3PicCurrentIndex >= (sizeof(ID3->APIC) / sizeof (ID3->APIC[0])))
			ID3PicCurrentIndex=0;
	}

	return 1;
}

static void ID3PicSetWin(int xpos, int wid, int ypos, int hgt)
{
	int i;
	ID3PicVisible = 1;

	if (ID3PicHandle)
	{
		plScrTextGUIOverlayRemove (ID3PicHandle);
		ID3PicHandle = 0;
	}
	ID3PicFirstLine=ypos;
	ID3PicFirstColumn=xpos;
	ID3PicHeight=hgt;
	ID3PicWidth=wid;

	for (i=0; i < (sizeof(ID3Pictures) / sizeof (ID3Pictures[i])); i++)
	{
		if (ID3Pictures[i].real_data_bgra)
		{
			ID3Picture_Scale(ID3Pictures + i, ID3PicFontSizeX * ID3PicWidth, ID3PicFontSizeY * (ID3PicHeight - 1));
		}
	}

	if (ID3Pictures[ID3PicCurrentIndex].scaled_data_bgra)
	{
		ID3PicHandle = plScrTextGUIOverlayAddBGRA
		(
			ID3PicFontSizeX * ID3PicFirstColumn,
			ID3PicFontSizeY * (ID3PicFirstLine + 1),
			ID3Pictures[ID3PicCurrentIndex].scaled_width,
			ID3Pictures[ID3PicCurrentIndex].scaled_height,
			ID3Pictures[ID3PicCurrentIndex].scaled_width,
			ID3Pictures[ID3PicCurrentIndex].scaled_data_bgra
		);
	} else {
		ID3PicHandle = plScrTextGUIOverlayAddBGRA
		(
			ID3PicFontSizeX * ID3PicFirstColumn,
			ID3PicFontSizeY * (ID3PicFirstLine + 1),
			ID3Pictures[ID3PicCurrentIndex].real_width,
			ID3Pictures[ID3PicCurrentIndex].real_height,
			ID3Pictures[ID3PicCurrentIndex].real_width,
			ID3Pictures[ID3PicCurrentIndex].real_data_bgra
		);
	}
}

static int ID3PicGetWin(struct cpitextmodequerystruct *q)
{
	ID3PicVisible = 0;
	if (ID3PicHandle)
	{
		plScrTextGUIOverlayRemove (ID3PicHandle);
		ID3PicHandle = 0;
	}

	if ((ID3PicActive==3)&&(plScrWidth<132))
		ID3PicActive=2;

	if ((ID3PicMaxHeight == 0) || (ID3PicMaxWidth == 0))
	{
		return 0;
	}

	switch (plCurrentFont)
	{
		case _4x4:
			q->hgtmax = 1 + (ID3PicMaxHeight +  3) /  4;
			ID3PicFontSizeX = ID3PicFontSizeY = 4;
			break;
		case _8x8:
			q->hgtmax = 1 + (ID3PicMaxHeight +  7) /  8;
			ID3PicFontSizeX = ID3PicFontSizeY = 8;
			break;
		case _8x16:
			q->hgtmax = 1 + (ID3PicMaxHeight + 15) / 16;
			ID3PicFontSizeX =  8;
			ID3PicFontSizeY = 16;
			break;
	}

	switch (ID3PicActive)
	{
		case 0:
			return 0;
		case 1:
			q->xmode=3;
			break;
		case 2:
			q->xmode=1;
			break;
		case 3:
			q->xmode=2;
			break;
	}
	q->size=1;
	q->top=1;
	q->killprio=128;
	q->viewprio=160;
	q->hgtmin=4;
	if (q->hgtmin>q->hgtmax)
		q->hgtmin=q->hgtmax;
	return 1;
}

static void ID3PicDraw(int focus)
{
	int len = strlen (ID3_APIC_Titles[ID3PicCurrentIndex]);
	if ((len + 9) > ID3PicWidth)
	{
		len = ID3PicWidth - 9;
	}
	displaystr (ID3PicFirstLine, ID3PicFirstColumn, focus?0x09:0x01, "ID3 PIC: ", 9);
	displaystr (ID3PicFirstLine, ID3PicFirstColumn + 9, focus?0x0a:0x02, ID3_APIC_Titles[ID3PicCurrentIndex], len);
	displaystr (ID3PicFirstLine, ID3PicFirstColumn + 9 + len, focus?0x09:0x00, " (tab to cycle)", ID3PicWidth - len - 9);
}

static int ID3PicIProcessKey(uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('c', "Enable ID3 picture viewer");
			cpiKeyHelp('C', "Enable ID3 picture viewer");
			break;
		case 'c': case 'C':
			if (!ID3PicActive)
			{
				ID3PicActive=(ID3PicActive+1)%4;
				if ((ID3PicActive==3)&&(plScrWidth<132))
				{
					ID3PicActive=2;
				}
			}
			cpiTextSetMode("id3pic");
			return 1;
		case 'x': case 'X':
			ID3PicActive=3;
			break;
		case KEY_ALT_X:
			ID3PicActive=2;
			break;
	}
	return 0;
}

static int ID3PicAProcessKey(uint16_t key)
{
	int i;
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('c', "Change ID3 picture view mode");
			cpiKeyHelp('C', "Change ID3 picture view mode");
			cpiKeyHelp(KEY_TAB, "Rotate ID3 pictures");
			return 0;
		case KEY_TAB:
			for (i=0; i < (sizeof(((struct ID3_t *)0)->APIC) / sizeof (((struct ID3_t *)0)->APIC[0])); i++)
			{
				ID3PicCurrentIndex++;
				if (ID3PicCurrentIndex >= (sizeof(((struct ID3_t *)0)->APIC) / sizeof (((struct ID3_t *)0)->APIC[0])))
					ID3PicCurrentIndex=0;
				if (ID3Pictures[ID3PicCurrentIndex].real_width && ID3Pictures[ID3PicCurrentIndex].real_height && ID3Pictures[ID3PicCurrentIndex].real_data_bgra)
					break;
			}

			if (ID3PicHandle)
			{
				plScrTextGUIOverlayRemove (ID3PicHandle);
				ID3PicHandle = 0;
			}

			if (ID3Pictures[ID3PicCurrentIndex].scaled_data_bgra)
			{
				ID3PicHandle = plScrTextGUIOverlayAddBGRA
				(
					ID3PicFontSizeX * ID3PicFirstColumn,
					ID3PicFontSizeY * (ID3PicFirstLine + 1),
					ID3Pictures[ID3PicCurrentIndex].scaled_width,
					ID3Pictures[ID3PicCurrentIndex].scaled_height,
					ID3Pictures[ID3PicCurrentIndex].scaled_width,
					ID3Pictures[ID3PicCurrentIndex].scaled_data_bgra
				);
			} else {
				ID3PicHandle = plScrTextGUIOverlayAddBGRA
				(
					ID3PicFontSizeX * ID3PicFirstColumn,
					ID3PicFontSizeY * (ID3PicFirstLine + 1),
					ID3Pictures[ID3PicCurrentIndex].real_width,
					ID3Pictures[ID3PicCurrentIndex].real_height,
					ID3Pictures[ID3PicCurrentIndex].real_width,
					ID3Pictures[ID3PicCurrentIndex].real_data_bgra
				);
			}

			break;
		case 'c': case 'C':
			ID3PicActive=(ID3PicActive+1)%4;
			if ((ID3PicActive==3)&&(plScrWidth<132))
			{
				ID3PicActive=0;
			}
			cpiTextRecalc();
			break;
		default:
			return 0;
	}
	return 1;
}

static int ID3PicEvent(int ev)
{
	struct ID3_t *ID3;

	switch (ev)
	{
		case cpievKeepalive:
			mpegGetID3(&ID3);
			if (Refresh_ID3Pictures(ID3))
			{
				cpiTextRecalc();
			}
			break;
		case cpievInit:
			mpegGetID3(&ID3);
			Refresh_ID3Pictures(ID3);
			ID3PicActive=3;
			break;
		case cpievClose:
			if (ID3PicHandle)
			{
				plScrTextGUIOverlayRemove (ID3PicHandle);
				ID3PicHandle = 0;
			}
			break;
		case cpievOpen:
			if (ID3PicVisible && (!ID3PicHandle))
			{
				if (ID3Pictures[ID3PicCurrentIndex].scaled_data_bgra)
				{
					ID3PicHandle = plScrTextGUIOverlayAddBGRA
					(
						ID3PicFontSizeX * ID3PicFirstColumn,
						ID3PicFontSizeY * (ID3PicFirstLine + 1),
						ID3Pictures[ID3PicCurrentIndex].scaled_width,
						ID3Pictures[ID3PicCurrentIndex].scaled_height,
						ID3Pictures[ID3PicCurrentIndex].scaled_width,
						ID3Pictures[ID3PicCurrentIndex].scaled_data_bgra
					);
				} else {
					ID3PicHandle = plScrTextGUIOverlayAddBGRA
					(
						ID3PicFontSizeX * ID3PicFirstColumn,
						ID3PicFontSizeY * (ID3PicFirstLine + 1),
						ID3Pictures[ID3PicCurrentIndex].real_width,
						ID3Pictures[ID3PicCurrentIndex].real_height,
						ID3Pictures[ID3PicCurrentIndex].real_width,
						ID3Pictures[ID3PicCurrentIndex].real_data_bgra
					);
				}
			}
			break;
		case cpievDone:
			if (ID3PicHandle)
			{
				plScrTextGUIOverlayRemove (ID3PicHandle);
				ID3PicHandle = 0;
			}
			Free_ID3Pictures ();
			break;
	}
	return 1;
}

static struct cpitextmoderegstruct cpiID3Pic = {"id3pic", ID3PicGetWin, ID3PicSetWin, ID3PicDraw, ID3PicIProcessKey, ID3PicAProcessKey, ID3PicEvent CPITEXTMODEREGSTRUCT_TAIL};

static void __attribute__((constructor))init(void)
{

	//cpiTextRegisterDefMode(&cpiID3Pic);
	cpiTextRegisterMode(&cpiID3Pic);

}

static void __attribute__((destructor))done(void)
{
	//cpiTextUnregisterDefMode(&cpiID3Pic);
}
