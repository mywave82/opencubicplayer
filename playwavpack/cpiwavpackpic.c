/* OpenCP Module Player
 * copyright (c) 2026 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * WavPack TAG Picture viewer
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
#include "wavpackplay.h"

static int WavPackPicActive;  /* requested mode from the user */
static int WavPackPicVisible; /* are we actually visible? */

static int WavPackPicFirstColumn;
static int WavPackPicFirstLine;
static int WavPackPicHeight;
static int WavPackPicWidth;
static int WavPackPicMaxHeight;
static int WavPackPicMaxWidth;
static int WavPackPicFontSizeX;
static int WavPackPicFontSizeY;

static void *WavPackPicHandle;
static int WavPackPicCurrentIndex;

static void WavPackPicture_ScaleUp(struct wavpack_picture_t *srcdst, int factor)
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

static void WavPackPicture_ScaleDown(struct wavpack_picture_t *srcdst, int factor)
{
	int x, y;
	int sx, sy;
	const uint8_t *src;
	uint8_t *dst;
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

static void WavPackPicture_Scale(struct wavpack_picture_t *srcdst, int width, int height)
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
		WavPackPicture_ScaleUp(srcdst, i);
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
		WavPackPicture_ScaleDown(srcdst, i);
		return;
	}

	free (srcdst->scaled_data_bgra); srcdst->scaled_data_bgra = 0;
	srcdst->scaled_width = 0;
	srcdst->scaled_height = 0;
}

static int Refresh_WavPackPictures (void)
{
	int i;

	WavPackPicMaxHeight = 0;
	WavPackPicMaxWidth = 0;

	for (i=0; i < wavpack_pictures_count; i++)
	{
		if (wavpack_pictures[i].width  > WavPackPicMaxWidth ) WavPackPicMaxWidth  = wavpack_pictures[i].width;
		if (wavpack_pictures[i].height > WavPackPicMaxHeight) WavPackPicMaxHeight = wavpack_pictures[i].height;
	}

	if (WavPackPicCurrentIndex >= wavpack_pictures_count)
	{
		WavPackPicCurrentIndex=0;
	}

	return 1;
}

static void WavPackPicSetWin (struct cpifaceSessionAPI_t *cpifaceSession, int xpos, int wid, int ypos, int hgt)
{
	int i;
	WavPackPicVisible = 1;

	if (WavPackPicHandle)
	{
		cpifaceSession->console->Driver->TextOverlayRemove (WavPackPicHandle);
		WavPackPicHandle = 0;
	}
	WavPackPicFirstLine=ypos;
	WavPackPicFirstColumn=xpos;
	WavPackPicHeight=hgt;
	WavPackPicWidth=wid;

	for (i=0; i < wavpack_pictures_count; i++)
	{
		WavPackPicture_Scale(wavpack_pictures + i, WavPackPicFontSizeX * WavPackPicWidth, WavPackPicFontSizeY * (WavPackPicHeight - 1));
	}

	if (wavpack_pictures[WavPackPicCurrentIndex].scaled_data_bgra)
	{
		WavPackPicHandle = cpifaceSession->console->Driver->TextOverlayAddBGRA
		(
			WavPackPicFontSizeX * WavPackPicFirstColumn,
			WavPackPicFontSizeY * (WavPackPicFirstLine + 1),
			wavpack_pictures[WavPackPicCurrentIndex].scaled_width,
			wavpack_pictures[WavPackPicCurrentIndex].scaled_height,
			wavpack_pictures[WavPackPicCurrentIndex].scaled_width,
			wavpack_pictures[WavPackPicCurrentIndex].scaled_data_bgra
		);
	} else {
		WavPackPicHandle = cpifaceSession->console->Driver->TextOverlayAddBGRA
		(
			WavPackPicFontSizeX * WavPackPicFirstColumn,
			WavPackPicFontSizeY * (WavPackPicFirstLine + 1),
			wavpack_pictures[WavPackPicCurrentIndex].width,
			wavpack_pictures[WavPackPicCurrentIndex].height,
			wavpack_pictures[WavPackPicCurrentIndex].width,
			wavpack_pictures[WavPackPicCurrentIndex].data_bgra
		);
	}
}

static int WavPackPicGetWin (struct cpifaceSessionAPI_t *cpifaceSession, struct cpitextmodequerystruct *q)
{
	WavPackPicVisible = 0;
	if (WavPackPicHandle)
	{
		cpifaceSession->console->Driver->TextOverlayRemove (WavPackPicHandle);
		WavPackPicHandle = 0;
	}

	if ((WavPackPicActive==3) && (cpifaceSession->console->TextWidth < 132))
		WavPackPicActive=2;

	if ((WavPackPicMaxHeight == 0) || (WavPackPicMaxWidth == 0))
	{
		return 0;
	}

	switch (cpifaceSession->console->CurrentFont)
	{
		case _8x8:
			q->hgtmax = 1 + (WavPackPicMaxHeight +  7) /  8;
			WavPackPicFontSizeX = WavPackPicFontSizeY = 8;
			break;
		case _8x16:
			q->hgtmax = 1 + (WavPackPicMaxHeight + 15) / 16;
			WavPackPicFontSizeX =  8;
			WavPackPicFontSizeY = 16;
			break;
		case _16x32:
			q->hgtmax = 1 + (WavPackPicMaxHeight + 31) / 32;
			WavPackPicFontSizeX = 16;
			WavPackPicFontSizeY = 32;
			break;
	}

	switch (WavPackPicActive)
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

static void WavPackPicDraw (struct cpifaceSessionAPI_t *cpifaceSession, int focus)
{
	cpifaceSession->console->DisplayPrintf (WavPackPicFirstLine, WavPackPicFirstColumn, focus?0x09:0x01, WavPackPicWidth, "WavPack PIC: %.*o%S%.*o, %.*o%S", focus?0x0a:0x02, wavpack_pictures[WavPackPicCurrentIndex].title, focus?0x09:0x01, focus?0x0a:0x02, wavpack_pictures[WavPackPicCurrentIndex].filename);
}

static int WavPackPicIProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)

{	if (!cpifaceSession->console->TextGUIOverlay)
	{
		return 0;
	}

	switch (key)
	{
		case KEY_ALT_K:
			cpifaceSession->KeyHelp ('c', "Enable WavPack picture viewer");
			cpifaceSession->KeyHelp ('C', "Enable WavPack picture viewer");
			break;
		case 'c': case 'C':
			if (!WavPackPicActive)
			{
				WavPackPicActive=1;
			}
			cpifaceSession->cpiTextSetMode (cpifaceSession, "wvpic");
			return 1;
		case 'x': case 'X':
			WavPackPicActive=3;
			break;
		case KEY_ALT_X:
			WavPackPicActive=2;
			break;
	}
	return 0;
}

static int WavPackPicAProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	if (!cpifaceSession->console->TextGUIOverlay)
	{
		return 0;
	}

	switch (key)
	{
		case KEY_ALT_K:
			cpifaceSession->KeyHelp ('c', "Change WavPack picture view mode");
			cpifaceSession->KeyHelp ('C', "Change WavPack picture view mode");
			cpifaceSession->KeyHelp (KEY_TAB, "Rotate WavPack pictures");
			return 0;
		case KEY_TAB:
			WavPackPicCurrentIndex++;
			if (WavPackPicCurrentIndex >= wavpack_pictures_count)
			{
				WavPackPicCurrentIndex = 0;
			}

			if (WavPackPicHandle)
			{
				cpifaceSession->console->Driver->TextOverlayRemove (WavPackPicHandle);
				WavPackPicHandle = 0;
			}

			if (wavpack_pictures[WavPackPicCurrentIndex].scaled_data_bgra)
			{
				WavPackPicHandle = cpifaceSession->console->Driver->TextOverlayAddBGRA
				(
					WavPackPicFontSizeX * WavPackPicFirstColumn,
					WavPackPicFontSizeY * (WavPackPicFirstLine + 1),
					wavpack_pictures[WavPackPicCurrentIndex].scaled_width,
					wavpack_pictures[WavPackPicCurrentIndex].scaled_height,
					wavpack_pictures[WavPackPicCurrentIndex].scaled_width,
					wavpack_pictures[WavPackPicCurrentIndex].scaled_data_bgra
				);
			} else {
				WavPackPicHandle = cpifaceSession->console->Driver->TextOverlayAddBGRA
				(
					WavPackPicFontSizeX * WavPackPicFirstColumn,
					WavPackPicFontSizeY * (WavPackPicFirstLine + 1),
					wavpack_pictures[WavPackPicCurrentIndex].width,
					wavpack_pictures[WavPackPicCurrentIndex].height,
					wavpack_pictures[WavPackPicCurrentIndex].width,
					wavpack_pictures[WavPackPicCurrentIndex].data_bgra
				);
			}

			break;
		case 'c': case 'C':
			WavPackPicActive=(WavPackPicActive+1)%4;
			if ((WavPackPicActive==3) && (cpifaceSession->console->TextWidth < 132))
			{
				WavPackPicActive=0;
			}
			cpifaceSession->cpiTextRecalc (cpifaceSession);
			break;
		default:
			return 0;
	}
	return 1;
}

static int WavPackPicEvent (struct cpifaceSessionAPI_t *cpifaceSession, int ev)
{
	switch (ev)
	{
		case cpievInit:
			if (cpifaceSession->console->TextGUIOverlay)
			{
				Refresh_WavPackPictures();
				WavPackPicActive=3;
			}
			break;
		case cpievClose:
			if (WavPackPicHandle)
			{
				cpifaceSession->console->Driver->TextOverlayRemove (WavPackPicHandle);
				WavPackPicHandle = 0;
			}
			break;
		case cpievOpen:
			if (WavPackPicVisible && (!WavPackPicHandle) && cpifaceSession->console->TextGUIOverlay)
			{
				if (wavpack_pictures[WavPackPicCurrentIndex].scaled_data_bgra)
				{
					WavPackPicHandle = cpifaceSession->console->Driver->TextOverlayAddBGRA
					(
						WavPackPicFontSizeX * WavPackPicFirstColumn,
						WavPackPicFontSizeY * (WavPackPicFirstLine + 1),
						wavpack_pictures[WavPackPicCurrentIndex].scaled_width,
						wavpack_pictures[WavPackPicCurrentIndex].scaled_height,
						wavpack_pictures[WavPackPicCurrentIndex].scaled_width,
						wavpack_pictures[WavPackPicCurrentIndex].scaled_data_bgra
					);
				} else {
					WavPackPicHandle = cpifaceSession->console->Driver->TextOverlayAddBGRA
					(
						WavPackPicFontSizeX * WavPackPicFirstColumn,
						WavPackPicFontSizeY * (WavPackPicFirstLine + 1),
						wavpack_pictures[WavPackPicCurrentIndex].width,
						wavpack_pictures[WavPackPicCurrentIndex].height,
						wavpack_pictures[WavPackPicCurrentIndex].width,
						wavpack_pictures[WavPackPicCurrentIndex].data_bgra
					);
				}
			}
			break;
		case cpievDone:
			if (WavPackPicHandle)
			{
				cpifaceSession->console->Driver->TextOverlayRemove (WavPackPicHandle);
				WavPackPicHandle = 0;
			}
			break;
	}
	return 1;
}

static struct cpitextmoderegstruct cpiWavPackPic = {"wvpic", WavPackPicGetWin, WavPackPicSetWin, WavPackPicDraw, WavPackPicIProcessKey, WavPackPicAProcessKey, WavPackPicEvent CPITEXTMODEREGSTRUCT_TAIL};

OCP_INTERNAL void wavpackPicInit (struct cpifaceSessionAPI_t *cpifaceSession)
{
	cpifaceSession->cpiTextRegisterMode (cpifaceSession, &cpiWavPackPic);
}

OCP_INTERNAL void wavpackPicDone (struct cpifaceSessionAPI_t *cpifaceSession)
{
	WavPackPicVisible = 0;
	cpifaceSession->cpiTextUnregisterMode (cpifaceSession, &cpiWavPackPic);
}
