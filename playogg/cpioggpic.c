/* OpenCP Module Player
 * copyright (c) 2020 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Ogg TAG Picture viewer
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
#include "oggplay.h"

static int OggPicActive;  /* requested mode from the user */
static int OggPicVisible; /* are we actually visible? */

static int OggPicFirstColumn;
static int OggPicFirstLine;
static int OggPicHeight;
static int OggPicWidth;
static int OggPicMaxHeight;
static int OggPicMaxWidth;
static int OggPicFontSizeX;
static int OggPicFontSizeY;

static void *OggPicHandle;
static int OggPicCurrentIndex;

static void OggPicture_ScaleUp(struct ogg_picture_t *srcdst, int factor)
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

static void OggPicture_ScaleDown(struct ogg_picture_t *srcdst, int factor)
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

static void OggPicture_Scale(struct ogg_picture_t *srcdst, int width, int height)
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
		OggPicture_ScaleUp(srcdst, i);
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
		OggPicture_ScaleDown(srcdst, i);
		return;
	}

	free (srcdst->scaled_data_bgra); srcdst->scaled_data_bgra = 0;
	srcdst->scaled_width = 0;
	srcdst->scaled_height = 0;
}

static int Refresh_OggPictures (void)
{
	int i;

	OggPicMaxHeight = 0;
	OggPicMaxWidth = 0;

	for (i=0; i < ogg_pictures_count; i++)
	{
		if (ogg_pictures[i].width  > OggPicMaxWidth ) OggPicMaxWidth  = ogg_pictures[i].width;
		if (ogg_pictures[i].height > OggPicMaxHeight) OggPicMaxHeight = ogg_pictures[i].height;
	}

	if (OggPicCurrentIndex >= ogg_pictures_count)
	{
		OggPicCurrentIndex=0;
	}

	return 1;
}

static void OggPicSetWin(int xpos, int wid, int ypos, int hgt)
{
	int i;
	OggPicVisible = 1;

	if (OggPicHandle)
	{
		plScrTextGUIOverlayRemove (OggPicHandle);
		OggPicHandle = 0;
	}
	OggPicFirstLine=ypos;
	OggPicFirstColumn=xpos;
	OggPicHeight=hgt;
	OggPicWidth=wid;

	for (i=0; i < ogg_pictures_count; i++)
	{
		OggPicture_Scale(ogg_pictures + i, OggPicFontSizeX * OggPicWidth, OggPicFontSizeY * (OggPicHeight - 1));
	}

	if (ogg_pictures[OggPicCurrentIndex].scaled_data_bgra)
	{
		OggPicHandle = plScrTextGUIOverlayAddBGRA
		(
			OggPicFontSizeX * OggPicFirstColumn,
			OggPicFontSizeY * (OggPicFirstLine + 1),
			ogg_pictures[OggPicCurrentIndex].scaled_width,
			ogg_pictures[OggPicCurrentIndex].scaled_height,
			ogg_pictures[OggPicCurrentIndex].scaled_width,
			ogg_pictures[OggPicCurrentIndex].scaled_data_bgra
		);
	} else {
		OggPicHandle = plScrTextGUIOverlayAddBGRA
		(
			OggPicFontSizeX * OggPicFirstColumn,
			OggPicFontSizeY * (OggPicFirstLine + 1),
			ogg_pictures[OggPicCurrentIndex].width,
			ogg_pictures[OggPicCurrentIndex].height,
			ogg_pictures[OggPicCurrentIndex].width,
			ogg_pictures[OggPicCurrentIndex].data_bgra
		);
	}
}

static int OggPicGetWin(struct cpitextmodequerystruct *q)
{
	OggPicVisible = 0;
	if (OggPicHandle)
	{
		plScrTextGUIOverlayRemove (OggPicHandle);
		OggPicHandle = 0;
	}

	if ((OggPicActive==3)&&(plScrWidth<132))
		OggPicActive=2;

	if ((OggPicMaxHeight == 0) || (OggPicMaxWidth == 0))
	{
		return 0;
	}

	switch (plCurrentFont)
	{
		case _4x4:
			q->hgtmax = 1 + (OggPicMaxHeight +  3) /  4;
			OggPicFontSizeX = OggPicFontSizeY = 4;
			break;
		case _8x8:
			q->hgtmax = 1 + (OggPicMaxHeight +  7) /  8;
			OggPicFontSizeX = OggPicFontSizeY = 8;
			break;
		case _8x16:
			q->hgtmax = 1 + (OggPicMaxHeight + 15) / 16;
			OggPicFontSizeX =  8;
			OggPicFontSizeY = 16;
			break;
	}

	switch (OggPicActive)
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

static void OggPicDraw(int focus)
{
	const char *picture_type = PictureType (ogg_pictures[OggPicCurrentIndex].picture_type);
	int left = OggPicWidth;

	if (left)
	{
		displaystr      (OggPicFirstLine, OggPicFirstColumn,                                 focus?0x09:0x01, "Ogg PIC: ", MIN(9, left));
		left -= 9;
	}

	if (left)
	{
		displaystr      (OggPicFirstLine, OggPicFirstColumn + 9,                             focus?0x0a:0x02, picture_type, MIN(strlen (picture_type), left));
		left -= strlen (picture_type);
	}

	if (left)
	{
		displaystr      (OggPicFirstLine, OggPicFirstColumn + 9 + strlen (picture_type),     focus?0x09:0x01, ", ", MIN(2, left));
		left -= 2;
	}

	if (left)
	{
		displaystr_utf8 (OggPicFirstLine, OggPicFirstColumn + 9 + strlen (picture_type) + 2, focus?0x0a:0x02, ogg_pictures[OggPicCurrentIndex].description, left);
	}
}

static int OggPicIProcessKey(uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('c', "Enable Ogg picture viewer");
			cpiKeyHelp('C', "Enable Ogg picture viewer");
			break;
		case 'c': case 'C':
			if (!OggPicActive)
			{
				OggPicActive=1;
			}
			cpiTextSetMode("oggpic");
			return 1;
		case 'x': case 'X':
			OggPicActive=3;
			break;
		case KEY_ALT_X:
			OggPicActive=2;
			break;
	}
	return 0;
}

static int OggPicAProcessKey(uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('c', "Change Ogg picture view mode");
			cpiKeyHelp('C', "Change Ogg picture view mode");
			cpiKeyHelp(KEY_TAB, "Rotate Ogg pictures");
			return 0;
		case KEY_TAB:
			OggPicCurrentIndex++;
			if (OggPicCurrentIndex >= ogg_pictures_count)
			{
				OggPicCurrentIndex = 0;
			}

			if (OggPicHandle)
			{
				plScrTextGUIOverlayRemove (OggPicHandle);
				OggPicHandle = 0;
			}

			if (ogg_pictures[OggPicCurrentIndex].scaled_data_bgra)
			{
				OggPicHandle = plScrTextGUIOverlayAddBGRA
				(
					OggPicFontSizeX * OggPicFirstColumn,
					OggPicFontSizeY * (OggPicFirstLine + 1),
					ogg_pictures[OggPicCurrentIndex].scaled_width,
					ogg_pictures[OggPicCurrentIndex].scaled_height,
					ogg_pictures[OggPicCurrentIndex].scaled_width,
					ogg_pictures[OggPicCurrentIndex].scaled_data_bgra
				);
			} else {
				OggPicHandle = plScrTextGUIOverlayAddBGRA
				(
					OggPicFontSizeX * OggPicFirstColumn,
					OggPicFontSizeY * (OggPicFirstLine + 1),
					ogg_pictures[OggPicCurrentIndex].width,
					ogg_pictures[OggPicCurrentIndex].height,
					ogg_pictures[OggPicCurrentIndex].width,
					ogg_pictures[OggPicCurrentIndex].data_bgra
				);
			}

			break;
		case 'c': case 'C':
			OggPicActive=(OggPicActive+1)%4;
			if ((OggPicActive==3)&&(plScrWidth<132))
			{
				OggPicActive=0;
			}
			cpiTextRecalc();
			break;
		default:
			return 0;
	}
	return 1;
}

static int OggPicEvent(int ev)
{
	switch (ev)
	{
		case cpievInit:
			Refresh_OggPictures();
			OggPicActive=3;
			break;
		case cpievClose:
			if (OggPicHandle)
			{
				plScrTextGUIOverlayRemove (OggPicHandle);
				OggPicHandle = 0;
			}
			break;
		case cpievOpen:
			if (OggPicVisible && (!OggPicHandle))
			{
				if (ogg_pictures[OggPicCurrentIndex].scaled_data_bgra)
				{
					OggPicHandle = plScrTextGUIOverlayAddBGRA
					(
						OggPicFontSizeX * OggPicFirstColumn,
						OggPicFontSizeY * (OggPicFirstLine + 1),
						ogg_pictures[OggPicCurrentIndex].scaled_width,
						ogg_pictures[OggPicCurrentIndex].scaled_height,
						ogg_pictures[OggPicCurrentIndex].scaled_width,
						ogg_pictures[OggPicCurrentIndex].scaled_data_bgra
					);
				} else {
					OggPicHandle = plScrTextGUIOverlayAddBGRA
					(
						OggPicFontSizeX * OggPicFirstColumn,
						OggPicFontSizeY * (OggPicFirstLine + 1),
						ogg_pictures[OggPicCurrentIndex].width,
						ogg_pictures[OggPicCurrentIndex].height,
						ogg_pictures[OggPicCurrentIndex].width,
						ogg_pictures[OggPicCurrentIndex].data_bgra
					);
				}
			}
			break;
		case cpievDone:
			if (OggPicHandle)
			{
				plScrTextGUIOverlayRemove (OggPicHandle);
				OggPicHandle = 0;
			}
			break;
	}
	return 1;
}

static struct cpitextmoderegstruct cpiOggPic = {"oggpic", OggPicGetWin, OggPicSetWin, OggPicDraw, OggPicIProcessKey, OggPicAProcessKey, OggPicEvent CPITEXTMODEREGSTRUCT_TAIL};

void __attribute__ ((visibility ("internal"))) OggPicInit (void)
{

	//cpiTextRegisterDefMode(&cpiOggPic);
	cpiTextRegisterMode(&cpiOggPic);
}


void __attribute__ ((visibility ("internal"))) OggPicDone (void)
{
	//cpiTextUnregisterDefMode(&cpiOggPic);
}
