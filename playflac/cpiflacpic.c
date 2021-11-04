/* OpenCP Module Player
 * copyright (c) 2020 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Flac TAG Picture viewer
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
#include "flacplay.h"

static int FlacPicActive;  /* requested mode from the user */
static int FlacPicVisible; /* are we actually visible? */

static int FlacPicFirstColumn;
static int FlacPicFirstLine;
static int FlacPicHeight;
static int FlacPicWidth;
static int FlacPicMaxHeight;
static int FlacPicMaxWidth;
static int FlacPicFontSizeX;
static int FlacPicFontSizeY;

static void *FlacPicHandle;
static int FlacPicCurrentIndex;

static void FlacPicture_ScaleUp(struct flac_picture_t *srcdst, int factor)
{
	int x, y;
	int sx, sy;
	uint32_t *src, *dst;
	srcdst->scaled_width = srcdst->width * factor;
	srcdst->scaled_height = srcdst->height * factor;
	srcdst->scaled_data_bgra = malloc (srcdst->scaled_width * srcdst->scaled_height * 4);
	src = (uint32_t *)srcdst->data_bgra;
	dst = (uint32_t *)srcdst->scaled_data_bgra;
	for (y = 0; y < srcdst->height; y++)
	{
		uint8_t *origdst = (uint8_t *)dst;
		int len;
		for (x = 0; x < srcdst->width; x++)
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

static void FlacPicture_ScaleDown(struct flac_picture_t *srcdst, int factor)
{
	int x, y;
	int sx, sy;
	uint8_t *src, *dst;
	srcdst->scaled_width = (srcdst->width + factor - 1) / factor;
	srcdst->scaled_height = (srcdst->height + factor - 1) / factor;
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

				if (tempy >= srcdst->height)
				{
					break;
				}

				src = srcdst->data_bgra + (tempy * srcdst->width + tempx) * 4;

				for (sx = 0; sx < factor; sx++, tempx++)
				{
					if (tempx >= srcdst->width)
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

static void FlacPicture_Scale(struct flac_picture_t *srcdst, int width, int height)
{
	int i;

	for (i = 1; ; i++)
	{
		if ((srcdst->width * i) > width)
		{
			/* scaled too high up */
			break;
		}
		if ((srcdst->height * i) > height)
		{
			/* scaled too high up */
			break;
		}
	}
	i--; /* revert the failed step */

	if (i > 1)
	{
		if ((srcdst->width  * i == srcdst->scaled_width ) &&
		    (srcdst->height * i == srcdst->scaled_height))
		{
			return;
		}
		free (srcdst->scaled_data_bgra); srcdst->scaled_data_bgra = 0;
		srcdst->scaled_width = 0;
		srcdst->scaled_height = 0;
		FlacPicture_ScaleUp(srcdst, i);
		return;
	}

	for (i = 1; ; i++)
	{
		/* Are we still too wide */
		if (((srcdst->width + i - 1) / i) > width)
		{
			continue;
		}
		/* Are we still too high? */
		if (((srcdst->height + i - 1) /  i) > height)
		{
			continue;
		}
		break;
	}

	if (i > 1)
	{
		if (((srcdst->width  + i - 1) / i == srcdst->scaled_width ) &&
		    ((srcdst->height + i - 1) / i == srcdst->scaled_height))
		{
			return;
		}
		free (srcdst->scaled_data_bgra); srcdst->scaled_data_bgra = 0;
		srcdst->scaled_width = 0;
		srcdst->scaled_height = 0;
		FlacPicture_ScaleDown(srcdst, i);
		return;
	}

	free (srcdst->scaled_data_bgra); srcdst->scaled_data_bgra = 0;
	srcdst->scaled_width = 0;
	srcdst->scaled_height = 0;
}

static int Refresh_FlacPictures (void)
{
	int i;

	FlacPicMaxHeight = 0;
	FlacPicMaxWidth = 0;

	for (i=0; i < flac_pictures_count; i++)
	{
		if (flac_pictures[i].width  > FlacPicMaxWidth ) FlacPicMaxWidth  = flac_pictures[i].width;
		if (flac_pictures[i].height > FlacPicMaxHeight) FlacPicMaxHeight = flac_pictures[i].height;
	}

	if (FlacPicCurrentIndex >= flac_pictures_count)
	{
		FlacPicCurrentIndex=0;
	}

	return 1;
}

static void FlacPicSetWin(int xpos, int wid, int ypos, int hgt)
{
	int i;
	FlacPicVisible = 1;

	if (FlacPicHandle)
	{
		plScrTextGUIOverlayRemove (FlacPicHandle);
		FlacPicHandle = 0;
	}
	FlacPicFirstLine=ypos;
	FlacPicFirstColumn=xpos;
	FlacPicHeight=hgt;
	FlacPicWidth=wid;

	flacMetaDataLock();

	for (i=0; i < flac_pictures_count; i++)
	{
		FlacPicture_Scale(flac_pictures + i, FlacPicFontSizeX * FlacPicWidth, FlacPicFontSizeY * (FlacPicHeight - 1));
	}

	if (flac_pictures[FlacPicCurrentIndex].scaled_data_bgra)
	{
		FlacPicHandle = plScrTextGUIOverlayAddBGRA
		(
			FlacPicFontSizeX * FlacPicFirstColumn,
			FlacPicFontSizeY * (FlacPicFirstLine + 1),
			flac_pictures[FlacPicCurrentIndex].scaled_width,
			flac_pictures[FlacPicCurrentIndex].scaled_height,
			flac_pictures[FlacPicCurrentIndex].scaled_width,
			flac_pictures[FlacPicCurrentIndex].scaled_data_bgra
		);
	} else {
		FlacPicHandle = plScrTextGUIOverlayAddBGRA
		(
			FlacPicFontSizeX * FlacPicFirstColumn,
			FlacPicFontSizeY * (FlacPicFirstLine + 1),
			flac_pictures[FlacPicCurrentIndex].width,
			flac_pictures[FlacPicCurrentIndex].height,
			flac_pictures[FlacPicCurrentIndex].width,
			flac_pictures[FlacPicCurrentIndex].data_bgra
		);
	}

	flacMetaDataUnlock();
}

static int FlacPicGetWin(struct cpitextmodequerystruct *q)
{
	FlacPicVisible = 0;
	if (FlacPicHandle)
	{
		plScrTextGUIOverlayRemove (FlacPicHandle);
		FlacPicHandle = 0;
	}

	if ((FlacPicActive==3)&&(plScrWidth<132))
		FlacPicActive=2;

	if ((FlacPicMaxHeight == 0) || (FlacPicMaxWidth == 0))
	{
		return 0;
	}

	switch (plCurrentFont)
	{
		case _8x8:
			q->hgtmax = 1 + (FlacPicMaxHeight +  7) /  8;
			FlacPicFontSizeX = FlacPicFontSizeY = 8;
			break;
		case _8x16:
			q->hgtmax = 1 + (FlacPicMaxHeight + 15) / 16;
			FlacPicFontSizeX =  8;
			FlacPicFontSizeY = 16;
			break;
	}

	switch (FlacPicActive)
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
	q->top=2;
	q->killprio=128;
	q->viewprio=160;
	q->hgtmin=4;
	if (q->hgtmin>q->hgtmax)
		q->hgtmin=q->hgtmax;
	return 1;
}

static int MIN(const int a, const int b)
{
	if (a < b)
	{
		return a;
	} else {
		return b;
	}
}

static const char *PictureType (const int pt)
{
	switch (pt)
	{
		case 0x00: return "Other";
		case 0x01: return "Icon";
		case 0x02: return "Other file icon";
		case 0x03: return "Cover (front)";
		case 0x04: return "Cover (back)";
		case 0x05: return "Leaflet page";
		case 0x06: return "Media (e.g. label side of CD)";
		case 0x07: return "Lead artist/lead performer/soloist";
		case 0x08: return "Artist/performer";
		case 0x09: return "Conductor";
		case 0x0A: return "Band/Orchestra";
		case 0x0B: return "Composer";
		case 0x0C: return "Lyricist/text writer";
		case 0x0D: return "Recording Location";
		case 0x0E: return "During recording";
		case 0x0F: return "During performance";
		case 0x10: return "Movie/video screen capture";
		case 0x11: return "A bright coloured fish";
		case 0x12: return "Illustration";
		case 0x13: return "Band/artist logotype";
		case 0x14: return "Publisher/Studio logotype";
		default: return "Unknown";
	}
}

static void FlacPicDraw(int focus)
{
	const char *picture_type;
	int left = FlacPicWidth;

	flacMetaDataLock();

	picture_type = PictureType (flac_pictures[FlacPicCurrentIndex].picture_type);

	if (left)
	{
		displaystr      (FlacPicFirstLine, FlacPicFirstColumn,                                 focus?0x09:0x01, "Flac PIC: ", MIN(9, left));
		left -= 9;
	}

	if (left)
	{
		displaystr      (FlacPicFirstLine, FlacPicFirstColumn + 9,                             focus?0x0a:0x02, picture_type, MIN(strlen (picture_type), left));
		left -= strlen (picture_type);
	}

	if (left)
	{
		displaystr      (FlacPicFirstLine, FlacPicFirstColumn + 9 + strlen (picture_type),     focus?0x09:0x01, ", ", MIN(2, left));
		left -= 2;
	}

	if (left)
	{
		displaystr_utf8 (FlacPicFirstLine, FlacPicFirstColumn + 9 + strlen (picture_type) + 2, focus?0x0a:0x02, flac_pictures[FlacPicCurrentIndex].description, left);
	}

	flacMetaDataUnlock();
}

static int FlacPicIProcessKey(uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('c', "Enable Flac picture viewer");
			cpiKeyHelp('C', "Enable Flac picture viewer");
			break;
		case 'c': case 'C':
			if (!FlacPicActive)
			{
				FlacPicActive=1;
			}
			cpiTextSetMode("flacpic");
			return 1;
		case 'x': case 'X':
			FlacPicActive=3;
			break;
		case KEY_ALT_X:
			FlacPicActive=2;
			break;
	}
	return 0;
}

static int FlacPicAProcessKey(uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('c', "Change Flac picture view mode");
			cpiKeyHelp('C', "Change Flac picture view mode");
			cpiKeyHelp(KEY_TAB, "Rotate Flac pictures");
			return 0;
		case KEY_TAB:
			FlacPicCurrentIndex++;
			flacMetaDataLock();
			if (FlacPicCurrentIndex >= flac_pictures_count)
			{
				FlacPicCurrentIndex = 0;
			}

			if (FlacPicHandle)
			{
				plScrTextGUIOverlayRemove (FlacPicHandle);
				FlacPicHandle = 0;
			}

			if (flac_pictures[FlacPicCurrentIndex].scaled_data_bgra)
			{
				FlacPicHandle = plScrTextGUIOverlayAddBGRA
				(
					FlacPicFontSizeX * FlacPicFirstColumn,
					FlacPicFontSizeY * (FlacPicFirstLine + 1),
					flac_pictures[FlacPicCurrentIndex].scaled_width,
					flac_pictures[FlacPicCurrentIndex].scaled_height,
					flac_pictures[FlacPicCurrentIndex].scaled_width,
					flac_pictures[FlacPicCurrentIndex].scaled_data_bgra
				);
			} else {
				FlacPicHandle = plScrTextGUIOverlayAddBGRA
				(
					FlacPicFontSizeX * FlacPicFirstColumn,
					FlacPicFontSizeY * (FlacPicFirstLine + 1),
					flac_pictures[FlacPicCurrentIndex].width,
					flac_pictures[FlacPicCurrentIndex].height,
					flac_pictures[FlacPicCurrentIndex].width,
					flac_pictures[FlacPicCurrentIndex].data_bgra
				);
			}
			flacMetaDataUnlock();

			break;
		case 'c': case 'C':
			FlacPicActive=(FlacPicActive+1)%4;
			if ((FlacPicActive==3)&&(plScrWidth<132))
			{
				FlacPicActive=0;
			}
			cpiTextRecalc();
			break;
		default:
			return 0;
	}
	return 1;
}

static int FlacPicEvent(int ev)
{
	switch (ev)
	{
		case cpievInit:
			flacMetaDataLock();
			Refresh_FlacPictures();
			FlacPicActive=3;
			flacMetaDataUnlock();
			break;
		case cpievClose:
			if (FlacPicHandle)
			{
				plScrTextGUIOverlayRemove (FlacPicHandle);
				FlacPicHandle = 0;
			}
			break;
		case cpievOpen:
			if (FlacPicVisible && (!FlacPicHandle))
			{
				flacMetaDataLock();
				if (flac_pictures[FlacPicCurrentIndex].scaled_data_bgra)
				{
					FlacPicHandle = plScrTextGUIOverlayAddBGRA
					(
						FlacPicFontSizeX * FlacPicFirstColumn,
						FlacPicFontSizeY * (FlacPicFirstLine + 1),
						flac_pictures[FlacPicCurrentIndex].scaled_width,
						flac_pictures[FlacPicCurrentIndex].scaled_height,
						flac_pictures[FlacPicCurrentIndex].scaled_width,
						flac_pictures[FlacPicCurrentIndex].scaled_data_bgra
					);
				} else {
					FlacPicHandle = plScrTextGUIOverlayAddBGRA
					(
						FlacPicFontSizeX * FlacPicFirstColumn,
						FlacPicFontSizeY * (FlacPicFirstLine + 1),
						flac_pictures[FlacPicCurrentIndex].width,
						flac_pictures[FlacPicCurrentIndex].height,
						flac_pictures[FlacPicCurrentIndex].width,
						flac_pictures[FlacPicCurrentIndex].data_bgra
					);
				}
				flacMetaDataUnlock();
			}
			break;
		case cpievDone:
			if (FlacPicHandle)
			{
				plScrTextGUIOverlayRemove (FlacPicHandle);
				FlacPicHandle = 0;
			}
			break;
	}
	return 1;
}

static struct cpitextmoderegstruct cpiFlacPic = {"flacpic", FlacPicGetWin, FlacPicSetWin, FlacPicDraw, FlacPicIProcessKey, FlacPicAProcessKey, FlacPicEvent CPITEXTMODEREGSTRUCT_TAIL};

void __attribute__ ((visibility ("internal"))) FlacPicInit (void)
{

	//cpiTextRegisterDefMode(&cpiFlacPic);
	cpiTextRegisterMode(&cpiFlacPic);
}


void __attribute__ ((visibility ("internal"))) FlacPicDone (void)
{
	//cpiTextUnregisterDefMode(&cpiFlacPic);
}
