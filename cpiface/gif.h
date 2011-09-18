/* OpenCP Module Player
 * copyright (c) '94-'05 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * GIF picture loader
 *
 * revision history: (please note changes here)
 * -doj980930  Dirk Jagdmann <doj@cubic.org>
 *   -initialial release of this file
 */

#ifndef GIF__H
#define GIF__H

/*************************************************************************
 * routine GIF87read(...)
 * ---------------------
 * this routine will load a gif file which is either
 * uncompressed or rle compressed. The routine will try to read as much
 * data out of the file as possible without any error. If this routine
 * returns with an error value, the array *pic and *pal could already
 * contain some data, which could be read until the error occured.
 *
 * call parameters:
 * -----------------
 * char *filename : a null terminated string, which contains the file to
 *                  load. This string is directly passed to the standard
 *                  open(...) routine of your compiler environment, so you
 *                  can supply every feature open(...) supports.
 * unsigned char *pic: the array which will contain the uncompressed color
 *                     mapped picture. Note that this array already has to
 *                     be allocated when calling GIF87read(...) with at least
 *                     picWidth*picHeight bytes !!!
 * unsigned char *pal: an array which will contain the RGB palette of the
 *                     picture.  This array must be allocated before calling
 *                     GIF87read(). GIF87read(...) will not read any palettes
 *                     containing more than 256 colors, so a size of 768
 *                     bytes should be safe.
 * int picWidth : the desired width of the image. If the image has a
 *                different width GIF87read(...) will return an error value.
 * int picHeight: the desired height of the image. If the image contains less
 *                scanlines, the rest of the array *pic will remain unchanged.
 *                If the image contains more scanlines, not more than
 *                picHeight scanlines will be read.
 *
 * return values:
 * --------------
 *  0 upon success
 * -1 if an error occured
 *  a positive value indicating the number of bad code blocks from the lzw
 *  decompression. This means that an error occurred that was not fatal, but
 *  the picture could not be uncompressed completely.
 *
 ************************************************************************/
int GIF87read(unsigned char *filedata,
              int filesize,
              unsigned char *pic,
              unsigned char *pal,
              const int picWidth,
              const int picHeight);

#endif
