/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * GIF picture loader
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
 * -doj980930  Dirk Jagdmann <doj@cubic.org>
 *   -initial release of this file
 *    the lzw decompression is Copyright (C) 1987, by Steven A. Bennett
 *    the header parsing was written according to the GIF87a specs from
 *    compuserve
 * -fd990408   Felix Domke <tmbinc@gmx.net>
 *   -included hack to load GIF89a-pics
 */

#include "config.h"
#include <stdlib.h>
#include "types.h"
#include "gif.h"

static uint8_t *filedata=0; /* because we are lazy */
static uint8_t *filedataEnd=0;
static uint8_t *image=0; /* because we are very lazy */
static uint16_t GIFimageHeight=0;
static int GIFimageInterlace=0; /* oh I am so lazy */
static int *interlaceTable=0; /* I'm so lame it hurts */
static int currentLine=0; /* my brain hurts */

static /*inline*/ int get_byte(void)
{
	if(filedata<filedataEnd)
		return *filedata++;

	return -1;
}

static /*inline*/ int out_line(uint8_t *line, int len)
{
	int i;

	if(GIFimageInterlace)
	{
		if(currentLine<GIFimageHeight)
		{
			int offset=interlaceTable[currentLine++];
			for(i=0; i<len; i++)
				image[offset+i]=line[i];
			return 0;
		}
	} else if(currentLine<GIFimageHeight)
	{
		currentLine++;
		for(i=0; i<len; i++)
			*image++=*line++;
		return 0;
	}

	return -1;
}

static int bad_code_count=0;

/***************************************************************************
 * DECODE.C - An LZW decoder for GIF
 * Copyright (C) 1987, by Steven A. Bennett
 *
 * Permission is given by the author to freely redistribute and include
 * this code in any program as long as this credit is given where due.
 *
 * In accordance with the above, I want to credit Steve Wilhite who wrote
 * the code which this is heavily inspired by...
 *
 * GIF and 'Graphics Interchange Format' are trademarks (tm) of
 * Compuserve, Incorporated, an H&R Block Company.
 *
 * Release Notes: This file contains a decoder routine for GIF images
 * which is similar, structurally, to the original routine by Steve Wilhite.
 * It is, however, somewhat noticably faster in most cases.
 *
 */
#define MAX_CODES   4095

/* Static variables */
static int16_t curr_size;                     /* The current code size */
static int16_t clear;                         /* Value for a clear code */
static int16_t ending;                        /* Value for a ending code */
static int16_t newcodes;                      /* First available code */
static int16_t top_slot;                      /* Highest code for current size */
static int16_t slot;                          /* Last read code */

/* The following static variables are used
 * for seperating out codes
 */
static int16_t navail_bytes = 0;              /* # bytes left in block */
static int16_t nbits_left = 0;                /* # bits left in current byte */
static uint8_t b1;                           /* Current byte */
static uint8_t byte_buff[257];               /* Current block */
static uint8_t *pbytes;                      /* Pointer to next byte in block */

static int32_t code_mask[13] =
{
	0,
	0x0001, 0x0003,
	0x0007, 0x000F,
	0x001F, 0x003F,
	0x007F, 0x00FF,
	0x01FF, 0x03FF,
	0x07FF, 0x0FFF
};


/* This function initializes the decoder for reading a new image.
 */
static int16_t init_exp(int16_t size)
{
	curr_size = size + 1;
	top_slot = 1 << curr_size;
	clear = 1 << size;
	ending = clear + 1;
	slot = newcodes = ending + 1;
	navail_bytes = nbits_left = 0;
	return 0;
}

/* get_next_code()
 * - gets the next code from the GIF file.  Returns the code, or else
 * a negative number in case of file errors...
 */
static int16_t get_next_code(void)
{
	int i, x;
	uint32_t ret;

	if (nbits_left == 0)
	{
		if (navail_bytes <= 0)
		{

		/* Out of bytes in current block, so read next block
		 */
		pbytes = byte_buff;
		if ((navail_bytes = get_byte()) < 0)
			return(navail_bytes);
		else if (navail_bytes)
		{
			for (i = 0; i < navail_bytes; ++i)
			{
				if ((x = get_byte()) < 0)
					return(x);
				byte_buff[i] = x;
			}
		}
	}
	b1 = *pbytes++;
	nbits_left = 8;
	--navail_bytes;
	}

	ret = b1 >> (8 - nbits_left);
	while (curr_size > nbits_left)
	{
		if (navail_bytes <= 0)
		{

			/* Out of bytes in current block, so read next block
			 */
			pbytes = byte_buff;
			if ((navail_bytes = get_byte()) < 0)
				return(navail_bytes);
			else if (navail_bytes)
			{
				for (i = 0; i < navail_bytes; ++i)
				{
					if ((x = get_byte()) < 0)
						return x;
					byte_buff[i] = x;
				}
			}
		}
		b1 = *pbytes++;
		ret |= b1 << nbits_left;
		nbits_left += 8;
		--navail_bytes;
	}
	nbits_left -= curr_size;
	ret &= code_mask[curr_size];
	return (int16_t)ret;
}


/* The reason we have these seperated like this instead of using
 * a structure like the original Wilhite code did, is because this
 * stuff generally produces significantly faster code when compiled...
 * This code is full of similar speedups...  (For a good book on writing
 * C for speed or for space optomisation, see Efficient C by Tom Plum,
 * published by Plum-Hall Associates...)
 */
static uint8_t stack[MAX_CODES + 1];            /* Stack for storing pixels */
static uint8_t suffix[MAX_CODES + 1];           /* Suffix table */
static uint16_t prefix[MAX_CODES + 1];           /* Prefix linked list */

/* WORD decoder(linewidth)
 *    WORD linewidth;               * Pixels per line of image *
 *
 * - This function decodes an LZW image, according to the method used
 * in the GIF spec.  Every *linewidth* "characters" (ie. pixels) decoded
 * will generate a call to out_line(), which is a user specific function
 * to display a line of pixels.  The function gets it's codes from
 * get_next_code() which is responsible for reading blocks of data and
 * seperating them into the proper size codes.  Finally, get_byte() is
 * the global routine to read the next byte from the GIF file.
 *
 * It is generally a good idea to have linewidth correspond to the actual
 * width of a line (as specified in the Image header) to make your own
 * code a bit simpler, but it isn't absolutely necessary.
 *
 * Returns: 0 if successful, else negative.  (See ERRS.H)
 *
 */

static int decoder(int linewidth)
{
	unsigned char *sp, *bufptr;
	unsigned char *buf;
	uint16_t code, fc, oc, bufcnt;
	int c, size, ret;

	/* Initialize for decoding a new image...
	 */
	if ((size = get_byte()) < 0)
		return(size);
	if (size < 2 || 9 < size)
		return(-1);
	init_exp(size);

	/* Initialize in case they forgot to put in a clear code.
	 * (This shouldn't happen, but we'll try and decode it anyway...)
	 */
	oc = fc = 0;

	/* Allocate space for the decode buffer
	 */
	if ((buf = calloc(sizeof(unsigned char),linewidth + 1)) == 0)
		return(-1);

	/* Set up the stack pointer and decode buffer pointer
	 */
	sp = stack;
	bufptr = buf;
	bufcnt = linewidth;

	/* This is the main loop.  For each code we get we pass through the
	 * linked list of prefix codes, pushing the corresponding "character" for
	 * each code onto the stack.  When the list reaches a single "character"
	 * we push that on the stack too, and then start unstacking each
	 * character for output in the correct order.  Special handling is
	 * included for the clear code, and the whole thing ends when we get
	 * an ending code.
	 */
	while ((c = get_next_code()) != ending)
	{

		/* If we had a file error, return without completing the decode
		 */
		if (c < 0)
		{
			free(buf);
			return(0);
		}

		/* If the code is a clear code, reinitialize all necessary items.
		 */
		if (c == clear)
		{
			curr_size = size + 1;
			slot = newcodes;
			top_slot = 1 << curr_size;

			/* Continue reading codes until we get a non-clear code
			 * (Another unlikely, but possible case...)
			 */
			while ((c = get_next_code()) == clear);

			/* If we get an ending code immediately after a clear code
			 * (Yet another unlikely case), then break out of the loop.
			 */
			if (c == ending)
				break;

			/* Finally, if the code is beyond the range of already set codes,
			 * (This one had better NOT happen...  I have no idea what will
			 * result from this, but I doubt it will look good...) then set it
			 * to color zero.
			 */
			if (c >= slot)
				c = 0;

			oc = fc = c;

			/* And let us not forget to put the char into the buffer... And
			 * if, on the off chance, we were exactly one pixel from the end
			 * of the line, we have to send the buffer to the out_line()
			 * routine...
			 */
			*bufptr++ = c;
			if (--bufcnt == 0)
			{
				if ((ret = out_line(buf, linewidth)) < 0)
				{
					free(buf);
					return(ret);
				}
				bufptr = buf;
				bufcnt = linewidth;
			}
		} else {

			/* In this case, it's not a clear code or an ending code, so
			 * it must be a code code...  So we can now decode the code into
			 * a stack of character codes. (Clear as mud, right?)
			 */
			code = c;

			/* Here we go again with one of those off chances...  If, on the
			 * off chance, the code we got is beyond the range of those already
			 * set up (Another thing which had better NOT happen...) we trick
			 * the decoder into thinking it actually got the last code read.
			 * (Hmmn... I'm not sure why this works...  But it does...)
			 */
			if (code >= slot)
			{
				if (code > slot)
					++bad_code_count;
				code = oc;
				*sp++ = fc;
			}

			/* Here we scan back along the linked list of prefixes, pushing
			 * helpless characters (ie. suffixes) onto the stack as we do so.
			 */
			while (code >= newcodes)
			{
				*sp++ = suffix[code];
				code = prefix[code];
			}

			/* Push the last character on the stack, and set up the new
			 * prefix and suffix, and if the required slot number is greater
			 * than that allowed by the current bit size, increase the bit
			 * size.  (NOTE - If we are all full, we *don't* save the new
			 * suffix and prefix...  I'm not certain if this is correct...
			 * it might be more proper to overwrite the last code...
			 */
			*sp++ = code;
			if (slot < top_slot)
			{
				suffix[slot] = fc = code;
				prefix[slot++] = oc;
				oc = c;
			}
			if (slot >= top_slot)
				if (curr_size < 12)
				{
					top_slot <<= 1;
					++curr_size;
				}

			/* Now that we've pushed the decoded string (in reverse order)
			 * onto the stack, lets pop it off and put it into our decode
			 * buffer...  And when the decode buffer is full, write another
			 * line...
			 */
			while (sp > stack)
			{
				*bufptr++ = *(--sp);
				if (--bufcnt == 0)
				{
					if ((ret = out_line(buf, linewidth)) < 0)
					{
						free(buf);
						return(ret);
					}
					bufptr = buf;
					bufcnt = linewidth;
				}
			}
		}
	}
	ret = 0;
	if (bufcnt != linewidth)
		ret = out_line(buf, (linewidth - bufcnt));
	free(buf);
	return(ret);
}
/*************************************************************************/

int GIF87read(unsigned char *fd, int filesize, unsigned char *pic, unsigned char *pal, const int picWidth, const int picHeight)
{
	int i;
	uint8_t *GIFsignature;
	uint16_t GIFscreenWidth;
	uint16_t GIFscreenHeight;
	uint8_t byte;
	int GIFglobalColorMap;
	/* int GIFcolorResolution; */
	int GIFbitPerPixel;
	/* uint8_t GIFbackground; */
	uint16_t GIFimageLeft;
	uint16_t GIFimageTop;
	uint16_t GIFimageWidth;
	int GIFimageColorMap;
	int GIFimageBitPerPixel;

	filedata=fd;
	filedataEnd=filedata+filesize;

	/************************************************************************
	 * process the header
	 ***********************************************************************/

	/* check if file format is GIF87 */
	GIFsignature=(unsigned char *)"GIF87a";
	for(i=0; i<6; i++)
		if(GIFsignature[i]!=*filedata++)
		{
			if (i==4)
				continue;
			return -1;
		}

	/* read the screen descriptor */
	GIFscreenWidth=*filedata++;
	GIFscreenWidth+=(*filedata++)<<8;

	GIFscreenHeight=*filedata++;
	GIFscreenHeight+=(*filedata++)<<8;

	byte=*filedata++;
	GIFglobalColorMap=byte&128;
#if 0
	GIFcolorResolution=((byte&(64+32+16))>>4)+1;
#endif
	GIFbitPerPixel=(byte&7)+1;

#if 0
	GIFbackground=*filedata++;
#else
	filedata++;
#endif

	if(*filedata++!=0)
		return -1;

	/* read the global color map */
	if(GIFglobalColorMap)
		for(i=0; i<(1<<GIFbitPerPixel)*3; i++)
			pal[i]=*filedata++;

	/* read the image descriptor */
	if(*filedata++!=',')
		return -1;

	GIFimageLeft=*filedata++;
	GIFimageLeft+=(*filedata++)<<8;

	GIFimageTop=*filedata++;
	GIFimageTop+=(*filedata++)<<8;

	GIFimageWidth=*filedata++;
	GIFimageWidth+=(*filedata++)<<8;
	if(GIFimageWidth!=picWidth)
		return -1;

	GIFimageHeight=*filedata++;
	GIFimageHeight+=(*filedata++)<<8;
	if(GIFimageHeight>picHeight)
		GIFimageHeight=picHeight;

	byte=*filedata++;
	GIFimageColorMap=byte&128;
	GIFimageInterlace=byte&64;
	GIFimageBitPerPixel=(byte&7)+1;

	if(GIFimageInterlace)
	{
		int z;
		interlaceTable=calloc(sizeof(int), GIFimageHeight);
		if(interlaceTable==0)
			return -1;

		z=0;
		for(i=0; i<GIFimageHeight; i+=8)
			interlaceTable[z++]=i*GIFimageWidth;
		for(i=4; i<GIFimageHeight; i+=8)
			interlaceTable[z++]=i*GIFimageWidth;
		for(i=2; i<GIFimageHeight; i+=4)
			interlaceTable[z++]=i*GIFimageWidth;
		for(i=1; i<GIFimageHeight; i+=2)
			interlaceTable[z++]=i*GIFimageWidth;
	}

	/* check for an GIF extension block */
	if(*filedata=='!')
		while(*filedata++!=0) ; /* skip this block */


	/* read the local color map */
	if(GIFimageColorMap)
		for(i=0; i<(1<<GIFimageBitPerPixel)*3; i++)
			pal[i]=*filedata++;

	#if 0
	fprintf("GIFscreenWidth: %i\n",GIFscreenWidth);
	fprintf("GIFscreenHeight: %i\n",GIFscreenHeight);
	fprintf("GIFglobalColorMap: %i\n",GIFglobalColorMap);
	fprintf("GIFcolorResolution: %i\n",GIFcolorResolution);
	fprintf("GIFbitPerPixel: %i\n",GIFbitPerPixel);
	fprintf("GIFbackground: %i\n",GIFbackground);
	fprintf("GIFimageLeft: %i\n",GIFimageLeft);
	fprintf("GIFimageTop: %i\n",GIFimageTop);
	fprintf("GIFimageWidth: %i\n",GIFimageWidth);
	fprintf("GIFimageHeight: %i\n",GIFimageHeight);
	fprintf("GIFimageColorMap: %i\n",GIFimageColorMap);
	fprintf("GIFimageInterlace: %i\n",GIFimageInterlace);
	fprintf("GIFimageBitPerPixel: %i\n",GIFimageBitPerPixel);
	#endif

	/* read the lzw compressed data */
	currentLine=0;
	image=pic;
	if(decoder(GIFimageWidth)<0)
		bad_code_count=-1;

	if(GIFimageInterlace)
		free(interlaceTable);
	return bad_code_count;
}
