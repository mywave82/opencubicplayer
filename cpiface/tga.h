/* OpenCP Module Player
 * copyright (c) '94-'05 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * TGA picture loader
 *
 * revision history: (please note changes here)
 * -doj980928  Dirk Jagdmann <doj@cubic.org>
 *   -initialial release of this file
 */

#ifndef TGA__H
#define TGA__H

/*************************************************************************
 * routine TGAread(...)
 * ---------------------
 * this routine will load a color mapped TGA file which is either
 * uncompressed or rle compressed. The routine will try to read as much
 * data out of the file as possible without any error. If this routine
 * returns with an error value, the array *pic and *pal could already
 * contain some data, which could be read until the error occurred.
 *
 * call parameters:
 * -----------------
 * char *filename : a null terminated string, which contains the file to
 *                  load. This string is directly passed to the standard
 *                  open(...) routine of your compiler environment, so you
 *                  can supply every feature open(...) supports.
 * unsigned char *pic: the array which will contain the uncompressed color
 *                     mapped picture. Note that this array already has to
 *                     be allocated when calling TGAread(...) with at least
 *                     picWidth*picHeight bytes !!!
 * unsigned char *pal: an array which will contain the RGB palette of the
 *                     picture.  This array must be allocated before calling
 *                     TGAread(). TGAread(...) will not read any palettes
 *                     containing more than 256 colors, so a size of 768
 *                     bytes should be safe.
 * int picWidth : the desired width of the image. If the image has a
 *                different width TGAread(...) will return an error value.
 * int picHeight: the desired height of the image. If the image contains less
 *                scanlines, the rest of the array *pic will remain unchanged.
 *                If the image contains more scanlines, not more than
 *                picHeight scanlines will be read.
 *
 * return values:
 * --------------
 *  0 upon success
 * -1 if an error occurred
 *
 ************************************************************************/
int TGAread(unsigned char *filedata,
            int filesize,
            unsigned char *pic,
            unsigned char *pal,
            const int picWidth,
            const int picHeight);

#endif
