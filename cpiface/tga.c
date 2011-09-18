/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * TGA picture loader
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
 *
 * revision history: (please note changes here)
 * -doj980928  Dirk Jagdmann <doj@cubic.org>
 *   -this file was written from scratch according to the official file
 *    description by AT&T
 *   -this routine is able to read uncompressed/rle compressed color mapped
 *    TGA files. I have tested this routine with TGA procuced by the programs:
 *     -Graphics Workshop (uncompressed)
 *     -Photoshop 5 (uncompressed)
 *     -GIMP (uncompressed and rle)
 *   -however there are many programs writing TGA files which only comply
 *    loosely to the standard. If you have problems loading your TGA files,
 *    I suggest that you convert them with any of the above programs,
 *    preferably GIMP, as it is Free Software.
 *   -please look at TGA.H on how to use this routine in your own programs
 */

#include "config.h"
#include "types.h"
#include "tga.h"

static int TGApal24(unsigned char *data, unsigned char *pal, int len)
{
	int i;
	for(i=0; i<len*3; i++)
		pal[i]=data[i];
	return len*3;
}

static int TGApal32(unsigned char *data, unsigned char *pal, int len)
{
	int i;
	for(i=0; i<len; i++)
	{
		pal[i*3+0]=data[i*4+0];
		pal[i*3+1]=data[i*4+1];
		pal[i*3+2]=data[i*4+2];
	}
	return len*4;
}

static int TGApal16(unsigned char *data, unsigned char *pal, int len)
{
	int i;
	for(i=0; i<len; i++)
	{
		pal[i*3+2]=data[i*2]&31; /* blue */
		pal[i*3+1]=data[i*2]>>5; /* green */
		pal[i*3+1]+=(data[i*2+1]&3)<<3; /* green */
		pal[i*3+0]=(data[i*2+1]&0x7c)>>2; /* red */
	}
	return len*2;
}

static int TGArle(unsigned char *data, unsigned char *pic, int width, int height)
{
	unsigned char *length=pic+width*height;
	while(pic<length)
	{
		unsigned char byte=*data++;
		if(byte&128)
		{
			/* run length packet */
			unsigned char color=*data++;
			unsigned char len=(byte&127)+1;
			int i;
			for(i=0; i<len; i++)
				if(pic<length)
					*pic++=color;
		} else  {
			/* raw packet */
			unsigned char len=(byte&127)+1;
			int i;
			if(pic+len>length)
				return -1;

			for(i=0; i<len; i++)
				*pic++=*data++;
		}
	}
	return 0;
}

int TGAread(unsigned char *filedata, int _ignore, unsigned char *pic, unsigned char *pal, const int picWidth, const int picHeight)
{
	int i;

	/************************************************************************
	 * process the header
	 ***********************************************************************/

	/* the size of the comment field */
	uint8_t identField=*filedata++;

	uint8_t imageTypeCode;
	uint16_t colorMapOrigin;
	uint16_t colorMapLength;
	uint8_t colorMapEntrySize;
	uint16_t xOrigin;
	uint16_t yOrigin;
	uint16_t width;
	uint16_t height;
	uint8_t imagePixelSize;
	uint8_t imageDescriptor;

	/* check if the image is color mapped. If not return */
	unsigned char colorMapType=*filedata++;
	if(colorMapType!=1)
		return -1;

	/* this byte contains information about the compression used */
	imageTypeCode=*filedata++;

	/* the first index of the color map (should be 0) */
	colorMapOrigin=*filedata++;
	colorMapOrigin+=(*filedata++)<<8;

	/* the length of the color map (should by <=256) */
	colorMapLength=*filedata++;
	colorMapLength+=(*filedata++)<<8;

	/* check for more than 256 colors */
	if(colorMapLength > 256)
		return -1;

	/* number of bits for each color map entry (should be 16, 24 or 32) */
	colorMapEntrySize=*filedata++;

	/* the left position of the image on the screen (ignored) */
	xOrigin=*filedata++;
	xOrigin+=(*filedata++)<<8;

	/* the top position of the image on the screen (ignored) */
	yOrigin=*filedata++;
	yOrigin+=(*filedata++)<<8;

	/* the width of the image. if differing from the parameter picWidth return
	 * with an error
	 */
	width=*filedata++;
	width+=(*filedata++)<<8;
	if(width!=picWidth)
		return -1;

	/* the height of the image. Not more than picHeight lines are processed */
	height=*filedata++;
	height+=(*filedata++)<<8;
	if(height>picHeight)
		height=picHeight;

	/* number of bits in a stored pixel (ignored) */
	imagePixelSize=*filedata++;

	/* image descriptor. Only information about the position of the origin is
	 * used
	 */
	imageDescriptor=*filedata++;

	/***********************************************************************
	 * now we skip the "Image Identification Field"
	 **********************************************************************/
	filedata+=identField;

	/***********************************************************************
	 * process Color Map
	 **********************************************************************/

	/* now we read the palette */
	switch(colorMapEntrySize)
	{
		case 16:
			i=TGApal16(filedata, pal, colorMapLength);
			filedata+=i;
			break;
		case 32:
			i=TGApal32(filedata, pal, colorMapLength);
			filedata+=i;
			break;
		default:
			i=TGApal24(filedata, pal, colorMapLength);
			filedata+=i;
			break;
	}
	/* convert the palette from BGR to RGB */
	for(i=0; i<colorMapLength; i++)
	{
		unsigned char t=pal[i*3];
		pal[i*3]=pal[i*3+2];
		pal[i*3+2]=t;
	}

	/***********************************************************************
	 * process Image
	 **********************************************************************/

	/* and read the image */
	switch(imageTypeCode)
	{
		case 1:
			for(i=0; i<width*height; i++)
				pic[i]=*filedata++;
			break;
		case 9:
			if(TGArle(filedata, pic, width, height))
			return -1;
			break;
		default:
			for(i=0; i<picWidth*picHeight; i++)
			pic[i]=0;
			break;
	}
	/* if the color map origin is set we have to move all entries
	 * I am not sure if this is correct, but I have not found any images where
	 * color map origin was set.
	 */
	if(colorMapOrigin!=0)
		for(i=0; i<width*height; i++)
			pic[i]-=colorMapOrigin;

	/* check if the image is on top */
	if(!(imageDescriptor&32))
		for(i=0; i<height/2; i++)
		{
			int j;
			for(j=0; j<width; j++)
			{
				unsigned char tmp=pic[j+i*width];
				pic[j+i*width]=pic[j+(height-i-1)*width];
				pic[j+(height-i-1)*width]=tmp;
			}
		}
	 return 0;
}
