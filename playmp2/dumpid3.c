/* OpenCP Module Player
 * copyright (c) 2020-'2x Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Utility: Dumping the ID3 tags from MP3 files
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

#define _GNU_SOURCE
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>

/**** Most common file-layuts:
 *
 * [----------MPEG DATA-------------][ID3v1.0]          # 128 bytes at the end
 *
 * [----------MPEG DATA-------------][ID3v1.1]          # 128 bytes at the end
 *
 *
 * [ID3v2.2][----------MPEG DATA-------------]          # ID3V2.2 at the start
 * [ID3v2.3][----------MPEG DATA-------------]          # ID3V2.3 at the start
 * [ID3v2.4][----------MPEG DATA-------------]          # ID3V2.4 at the start
 *
 * [ID3v2.2][------MPEG DATA--------][ID3v1.0]          # ID3V1.x tag at the end of legacy software, ID3v2.x at the start
 *
 * [----------MPEG DATA----------][ID3v2.4][F]          # ID3V2.4 with footer at the end
 *
 * [MPEG DATA][ID3v2.x][MPEG DATA][ID3v2.x]..           # Streaming service, music changes while playing
 *
 *
 * This never got popular:
 *
 * [------MPEG DATA--------][ID3v1.2][ID3v1.1]          # 256 bytes at the end - The two tags combined gives longer strings
 *
 *
 * "Broken" software generates these:
 *
 *[ID3v2.2][ID3v2.3][-------MPEG DATA--------]          # not common, but perfectly OK. Tag with unknown versions should be ignored, so old software reads v2.2, new read v2.3 and replaces the information
 */

static int indent = 0;
static int _newline = 1;
static void print(const char *format, ...)
{
	va_list ap;

	if (_newline)
	{
		int i;
		_newline = 0;
		for (i=0; i < indent; i++)
		{
			fputc(' ', stdout);
		}
	}

	va_start(ap, format);
	vprintf(format, ap);
	va_end(ap);
}
enum stack_message_type
{
	STACK_INFO = 0,
	STACK_WARNING = 1,
	STACK_ERROR = 2
};
static int stacked_count;
static char *stacked_messages[1024];
static int stacked_indent[1024];
static enum stack_message_type stacked_smt[1024];
static void stack_message(enum stack_message_type smt, int extraindent, const char *format, ...)
{
	va_list ap;

	char *buffer;
	buffer = malloc(1024);
	va_start(ap, format);
	vsnprintf(buffer, 1024, format, ap);
	va_end(ap);
	stacked_indent[stacked_count] = extraindent + indent;
	stacked_messages[stacked_count] = buffer;
	stacked_smt[stacked_count++] = smt;
}
static void flush()
{
	int i;
	int oldindent = indent;
	if (!_newline)
	{
		fputc('\n', stdout);
		_newline=1;
	}
	for (i=0; i < stacked_count; i++)
	{
		indent = stacked_indent[i];
		switch (stacked_smt[i])
		{
			case STACK_WARNING: print ("*-* WARNING: "); break;
			case STACK_ERROR: print ("*-* ERROR: "); break;
			case STACK_INFO: print ("*-* INFO: "); break;
		}
		print(stacked_messages[i]);
		free(stacked_messages[i]);
		switch (stacked_smt[i])
		{
			case STACK_WARNING:
			case STACK_ERROR:
			case STACK_INFO: print (" *-*"); break;
		}
		fputc('\n', stdout);
		_newline=1;
	}
	indent = oldindent;
	stacked_count = 0;
}
static void newline()
{
	if (stacked_count)
	{
		flush();
	} else {
		fputc('\n', stdout);
		_newline=1;
	}
}
static void unsync(uint8_t *data, uint32_t *len)
{
	uint32_t i;
	/* Unescape all 0xff 0x00 combinations with 0xff */
	for (i = 0; (i+1) < (*len); i++)
	{
		if ((data[i]==0xff) && (data[i+1]==0x00))
		{
			memmove (data + i + 1, data + i + 2, (*len) - i - 1);
			(*len)--;
		}
	}
}

static int decode_print_UTF8(uint8_t **data, uint32_t *len, const int must_include_null_term)
{
	int ilen = 1;
#warning TODO, sanitize UTF-8
	print("\"");
	while (*len)
	{
		if ((*data)[0] == 0x00)
		{
			(*data)++;
			(*len)--;
			print("\"");
			return 0;
		} else if ((*data[0]) == 0x0a)
		{
			ilen += 6;
			print("\" NL \"");
		} else if ((*data)[0] == 0x0d)
		{
			ilen += 6;
			print("\" CR \""); /* not always present */
		} else {
			if ((*data)[0] & 0x80)
			{
				if (((*data)[0] & 0xC0) != 0x80)
				{
					ilen++;
				}
			} else {
				ilen++;
			}
			print("%c", (*data)[0]);
		}
		(*data)++;
		(*len)--;
	}
	print("\"");
	if (must_include_null_term)
	{
		stack_message(ilen, STACK_ERROR, "no NULL termination");
		return -1;
	}
	return 0;
}

static int decode_print_iso8859_1(uint8_t **data, uint32_t *len, const int must_include_null_term)
{
	int ilen = 1;
	int valid_utf8_chars = 0;
	int invalid_utf8_chars = 0;
	/* first we prescan to detect if UTF-8 has been dumped into latin1.. this happens often in ID3.
	 *(we only probe for (2 and 3 characters wide)
	 */
	{
		uint8_t *d2 = *data;
		uint32_t l2 = *len;
		while (l2)
		{
			if (!*d2)
			{
				break;
			}
			if (d2[0] & 0x80)
			{
				if ((l2 >= 2) && ((d2[0] & 0xe0) == 0xc0) && ((d2[1] & 0xc0) == 0x80))
				{
					valid_utf8_chars++;
					d2+=2;
					l2-=2;
				} else if ((l2 >= 3) && ((d2[0] & 0xf0) == 0xe0) && ((d2[1] & 0xc0) == 0x80) && ((d2[2] & 0xc0) == 0x80))
				{
					valid_utf8_chars++;
					d2+=3;
					l2-=3;
				} else {
					invalid_utf8_chars++;
					break;
				}
			} else {
				d2++;
				l2--;
			}
		}
	}

	if ((!invalid_utf8_chars) && valid_utf8_chars)
	{
		print("=> UTF8_detected ");
		return decode_print_UTF8(data, len, must_include_null_term);
	}
	print("\"");
	while (*len)
	{
		if ((*data)[0] == 0x00)
		{
			(*data)++;
			(*len)--;
			print("\"");
			return 0;
		} else if ((*data)[0] == 0x0a)
		{
			ilen += 6;
			print("\" NL \"");
		} else if ((*data)[0] == 0x0d)
		{
			ilen += 6;
			print("\" CR \""); /* not always present */
		} else if (((*data)[0] <= 0x1f) || (((*data)[0] >= 0x8f) && ((*data)[0] <= 0x9f)))
		{
			stack_message(STACK_WARNING, ilen, "byte with invalid value: 0x%02"PRIx8, (*data)[0]);
		} else {
			ilen++;
			if ((*data)[0] < 0x80)
			{
				print("%c", (*data)[0]);
			} else {
				print("%c%c", ((*data)[0] >> 6) | 0xc0, ((*data)[0] & 0x3f) | 0x80);
			}
		}
		(*data)++;
		(*len)--;
	}
	print("\"");
	if (must_include_null_term)
	{
		stack_message(STACK_ERROR, ilen, "no NULL termination");
		return -1;
	}
	return 0;
}

static int decode_print_UCS2(uint8_t **data, uint32_t *len, const int must_include_null_term, int firststring)
{
	int ilen = 1;
	static int be = 1;

	if (*len < 2)
	{
		stack_message(STACK_WARNING, ilen, "BOM can not fit");
		return -1;
	}
	if (((*data)[0] == 0xfe) && ((*data)[1] == 0xff))
	{ // big endian
		be = 1;
		(*data)+=2;
		(*len)-=2;
	} else if (((*data)[0] == 0xff) && ((*data)[1] == 0xfe))
	{ // little endian
		be = 0;
		(*data)+=2;
		(*len)-=2;
	} else {
		if (firststring)
		{
			stack_message(STACK_WARNING, ilen, "Invalid BOM: 0x%02x 0x%02x", (*data)[0], (*data)[1]);
			return -1;
		}
	}

	print("\"");
	while (*len)
	{
		uint16_t codepoint;
		if (*len == 1)
		{
			stack_message(STACK_WARNING, ilen, "len==1");
			print("\"");
			return -1;
		}
		if (be)
		{
			codepoint = ((*data)[0]<<8) | (*data)[1];
		} else {
			codepoint = ((*data)[1]<<8) | (*data)[0];
		}
		(*data)+=2;
		(*len)-=2;

		if (codepoint == 0x0000)
		{
			(*data)+=2;
			(*len)-=2;
			print("\"");
			return 0;
		} else if (codepoint == 0x000a)
		{
			ilen += 6;
			print("\" NL \"");
		} else if (codepoint == 0x000d)
		{
			ilen += 6;
			print("\" CR \""); /* not always present */
		} else if (codepoint < 127)
		{
			ilen++;
			print("%c", codepoint);
		} else if (codepoint < 0x800)
		{
			ilen++;
			print("%c%c", (codepoint >> 6) | 0xc0, (codepoint & 0x3f) | 0x80);
		} else {
			ilen++;
			print("%c%c%c", (codepoint >> 12) | 0xe0, ((codepoint >> 6) & 0x3f) | 0x80, (codepoint & 0x3f) | 0x80);
		}
	}
	print("\"");
	if (must_include_null_term)
	{
		stack_message(STACK_ERROR, ilen, "no NULL termination");
		return -1;
	}
	return 0;
}

static int decode_print_UTF16(uint8_t **data, uint32_t *len, const int must_include_null_term)
{
	int ilen = 1;
	int be = 1;

	print("\"");
	while (*len)
	{
		uint32_t codepoint;
		if ((*len) == 1)
		{
			if (must_include_null_term)
			{
				(*data)+=2;
				(*len)-=2;
			}
			stack_message(STACK_ERROR, ilen, "len==1");
			return -1;
		}
		if (be)
		{
			codepoint = ((*data)[0]<<8) | (*data)[1];
		} else {
			codepoint = ((*data)[1]<<8) | (*data)[0];
		}

		(*data)+=2;
		(*len)-=2;

		if ((codepoint >= 0xd800) && (codepoint <= 0xdbff))
		{
			uint16_t second;
			if ((*len) < 2)
			{
				stack_message(STACK_ERROR, ilen, "Second surrogate does not fit");
				print("\"");
				return 1;
			}
			if (be)
			{
				second = ((*data)[0]<<8) | (*data)[1];
			} else {
				second = ((*data)[1]<<8) | (*data)[0];
			}
			(*data)+=2;
			(*len)-=2;

			if ((second < 0xdc00) || (second > 0xdfff))
			{
				stack_message(STACK_ERROR, ilen, "Second surrogate out og range");
				print("\"");
				return -1;
			}

			codepoint = ((codepoint & 0x03ff) << 10) | (second & 0x03ff);
			codepoint += 0x10000;

		} else if ((codepoint >= 0xdc00) && (codepoint <= 0xdfff))
		{
			stack_message(STACK_ERROR, ilen, "Surrogates in the wrong order");
			print("\"");
			return -1;
		}
		if (codepoint == 0xfeff)
		{
			/* NO-OP */
		} if (codepoint == 0xfffe)
		{
			be = !be;
		} else if (codepoint == 0x0000)
		{
			(*data)+=2;
			(*len)-=2;
			print("\"");
			return 0;
		} else if (codepoint == 0x000a)
		{
			ilen += 6;
			print("\" NL \"");
		} else if (codepoint == 0x000d)
		{
			ilen += 6;
			print("\" CR \""); /* not always present */
		} else if (codepoint < 127)
		{
			ilen++;
			print("%c", codepoint);
		} else if (codepoint < 0x800)
		{
			ilen++;
			print("%c%c", (codepoint >> 6) | 0xc0, (codepoint & 0x3f) | 0x80);
		} else if (codepoint < 0x10000)
		{
			ilen++;
			print("%c%c%c", (codepoint >> 12) | 0xe0, ((codepoint >> 6) & 0x3f) | 0x80, (codepoint & 0x3f) | 0x80);
		} else {
			ilen++;
			print("%c%c%c%c", (codepoint >> 18) | 0xf0, ((codepoint >> 12) & 0x3f) | 0x80, ((codepoint >> 6) & 0x3f) | 0x80, (codepoint & 0x3f) | 0x80);
		}
	}
	print("\"");
	if (must_include_null_term)
	{
		stack_message(STACK_ERROR, ilen, "no NULL termination");
		return -1;
	}
	return 0;
}

static void decode_UFID(uint8_t *data, uint32_t len)
{ /* ID3v220 used UFI only */
	uint8_t *nameeof = memchr (data, len, 0x00);

	print("Unique file identifier");

	if (!nameeof)
	{
		stack_message(STACK_WARNING, 0, "no DB");
	} else {
		print(" \"%s\"", (char *)data);
		len -= nameeof-data;
		data=nameeof;

		len--;
		data++;
	}

	if (len > 64)
	{
		stack_message(STACK_WARNING, 0, "More than 64 bytes in ID");
	}

	print(" ID=0x");
	while (len)
	{
		print("%02" PRIx8, *data);
		data++;
		len--;
	}
	newline();
}

static void decode_MCDI(uint8_t *data, uint32_t len)
{ /* ID3v220 used MCI only */
	print("Music CD identifier");

	if (len > 804)
	{
		stack_message(STACK_WARNING, 0, "More than 804 bytes in ID");
	}

	print(" - CD-TOC=0x");
	while (len)
	{
		print("%02" PRIx8, *data);
		data++;
		len--;
	}
	newline();
}

static void decode_ETCO(uint8_t *data, uint32_t len, uint8_t version)
{ /* ID3v220 used ETC only */
	uint8_t format;

	print("Event timing codes");

	if (len < 1)
	{
		stack_message(STACK_WARNING, 0, "Frame contains no format");
		newline();
		return;
	}
	format = *data;
	data++;
	len--;

	if ((format == 0) || (format > 2))
	{
		stack_message(STACK_WARNING, 0, "Invalid format");
		newline();
		return;
	}

	newline();
	while (len >= 5)
	{
		if (data[0] != 0xff)
		{
			uint32_t timecode;
			timecode = (data[1]<<24)|(data[2]<<16)|(data[3]<<8)|data[4];

			if (timecode == 0)
			{
				print("            %s", (format==1)?"  ":"           ");
			} else {
				print("%10" PRId32 " %s ", timecode, (format==1)?"ms":"mpeg-frames");
			}

			     if (data[0] == 0x00) print("padding (has no meaning)");
			else if (data[0] == 0x01) print("end of initial silence");
			else if (data[0] == 0x02) print("intro start");
			else if (data[0] == 0x03) print("main part start");
			else if (data[0] == 0x04) print("outro start");
			else if (data[0] == 0x05) print("outro end");
			else if (data[0] == 0x06) print("verse start");
			else if (data[0] == 0x07) print("refrain start");
			else if (data[0] == 0x08) print("interlude start");
			else if (data[0] == 0x09) print("theme start");
			else if (data[0] == 0x0a) print("variation start");
			else if (data[0] == 0x0b) print("key change");
			else if (data[0] == 0x0c) print("time change");
			else if (data[0] == 0x0d) print("momentary unwanted noise (Snap, Crackle & Pop)");
			else if ((data[0] == 0x0e) && (version >= 3)) print("sustained noise");     /* new in ID3v230 */
			else if ((data[0] == 0x0f) && (version >= 3)) print("sustained noise end"); /* new in ID3v230 */
			else if ((data[0] == 0x10) && (version >= 3)) print("intro end");           /* new in ID3v230 */
			else if ((data[0] == 0x11) && (version >= 3)) print("main part end");       /* new in ID3v230 */
			else if ((data[0] == 0x12) && (version >= 3)) print("verse end");           /* new in ID3v230 */
			else if ((data[0] == 0x13) && (version >= 3)) print("refrain end");         /* new in ID3v230 */
			else if ((data[0] == 0x14) && (version >= 3)) print("theme end");           /* new in ID3v230 */
			else if ((data[0] == 0x15) && (version >= 4)) print("profanity");           /* new in ID3v240 */
			else if ((data[0] == 0x16) && (version >= 4)) print("profanity end");       /* new in ID3v240 */
			else if (data[0] == 0xfd) print("audio end (start of silence)");
			else if (data[0] == 0xfe) print("audio file ends");
			else if ((data[0] & 0xf0) == 0xe0) print("not predefined sync: %c", "0123456789ABCDEF"[data[0]&0x0f]);
			else print("(%02x) - reserved for future use", data[0]);

			newline();

			data+=5;
			len-=5;
		} else {
			int i, j;
			uint32_t timecode;

			for (i=0; data[i]==0xff; i++)
			{
				if (len < (i*2+4))
				{
					stack_message(STACK_ERROR, 0, "Ran out of data while reading ETCO user event");
					return;
				}
			}

			timecode = (data[i*2+0]<<24)|(data[i*2+1]<<16)|(data[i*2+2]<<8)|data[i*2+3];
			indent += 2;
			for (j=0; j < i; j++)
			{
				if ((timecode == 0) || j)
				{
					print("            %s", (format==1)?"  ":"           ");
				} else {
					print("%10" PRId32 " %s ", timecode, (format==1)?"ms":"mpeg-frames");
				}
				print ("User event %d", data[i+j]);
				newline();
			}
			indent -= 2;

			data += i*2+4;
			len -= i*2+4;
		}
	}

	if (len)
	{
		stack_message(STACK_WARNING, 0, "Some extra padding in the frame");
	}
}

static void decode_POSS(uint8_t *data, uint32_t len)
{
	uint32_t timecode = 0;
	uint8_t format;

	print("Position synchronisation frame");

	if (len < 1)
	{
		stack_message(STACK_ERROR, 0, "Frame contains no format");
		newline();
		return;
	}
	format = *data;
	data++;
	len--;

	if ((format == 0) || (format > 2))
	{
		stack_message(STACK_ERROR, 0, "Invalid format");
		newline();
		return;
	}

	while (len)
	{
		timecode <<= 8;
		timecode |= data[0];
	}
	print(" - Position: %" PRId32 " %s", timecode, (format==1)?"ms":"mpeg-frames");
	newline();
}

static void decode_GEOB(uint8_t *data, uint32_t len, int version)
{ /* ID3v220 used GEO only */
	uint8_t text_encoding;
	int error;

	print("General encapsulated object");

	if (len < 1)
	{
		stack_message(STACK_ERROR, 0, "No space for text_encoding");
		newline();
		return;
	}
	text_encoding = *data;
	data++;
	len--;

	if (((text_encoding >= 2) && (version < 4)) || (text_encoding >= 4))
	{
		stack_message(STACK_ERROR, 0, "Text_encoding out of range");
		newline();
		return;
	}

	if (!len)
	{
		stack_message(STACK_ERROR, 0, "No space for MIME-Type");
		newline();
		return;
	}
	newline();
	print("MIME-Type: ");
	error=decode_print_iso8859_1(&data, &len, 1);
	newline();

	if (error)
	{
		return;
	}

	if (!len)
	{
		stack_message(STACK_ERROR, 0, "No space for Filename");
		return;
	}

	print("Filename: ");
	switch (text_encoding)
	{
		case 0x00:
			print("LATIN1 ");
			error = decode_print_iso8859_1(&data, &len, 1);
			break;
		case 0x01:
			print("UCS-2 ");
			error = decode_print_UCS2(&data, &len, 1, 1);
			break;
		case 0x02:
			print("UTF-16 ");
			error = decode_print_UTF16(&data, &len, 1);
			break;
		case 0x03:
			print("UTF-8 ");
			error = decode_print_UTF8(&data, &len, 1);
			break;
	}
	newline();
	if (error)
	{
		return;
	}

	if (!len)
	{
		stack_message(STACK_ERROR, 0, "Ran out of data");
		return;
	}

	print("Content description: ");
	switch (text_encoding)
	{
		case 0x00:
			print("LATIN1 ");
			error = decode_print_iso8859_1(&data, &len, 1);
			break;
		case 0x01:
			print("UCS-2 ");
			error = decode_print_UCS2(&data, &len, 1, 0);
			break;
		case 0x02:
			print("UTF-16 ");
			error = decode_print_UTF16(&data, &len, 1);
			break;
		case 0x03:
			print("UTF-8 ");
			error = decode_print_UTF8(&data, &len, 1);
			break;
	}
	newline();
	if (error)
	{
		return;
	}

	print("Encapsulated object: 0x");
	while (len)
	{
		print("%02" PRIx8, *data);
		data++;
		len--;
	}
	newline();
}

static void decode_EQUA(uint8_t *data, uint32_t len)
{
	uint8_t bits;
	print("Equalisation"); newline();

	if (len < 1)
	{
		stack_message(STACK_ERROR, 0, "No space for Adjustment bits");
		return;
	}
	bits = data[0];
	if (bits==0||bits>16)
	{
		stack_message(STACK_ERROR, 0, "Adjustment bits out of range");
		return;
	}
	data++;
	len--;

	while (len > 3)
	{
		uint16_t frequency  = (data[0]<<8) | data[1];
		uint16_t adjustment = (data[2]);

		data+=3;
		len-=3;
		if (bits>8)
		{
			if (!len)
			{
				stack_message(STACK_ERROR, 0, "Ran out of data");
				return;
			}
			adjustment<<=8;
			adjustment|=data[0];
			data++;
			len--;
		}

		print("Frequency: %"PRId16" Hz, Volume adjustment: %c%"PRId16, frequency & 0x7fff, (frequency & 0x8000)?'+':'-', adjustment); newline();
	}
	if (len)
	{
		stack_message(STACK_WARNING, 0, "Some extra padding in the frame");
	}
}

static void decode_EQU2(uint8_t *data, uint32_t len)
{
	int error;
	print("Equalisation (2)"); newline();

	if (len < 1)
	{
		stack_message(STACK_ERROR, 0, "No space for Interpolation method");
		return;
	}
	switch (data[0])
	{
		case 0x00: print("Interpolation method: Band"); newline(); break;
		case 0x01: print("Interpolation method: Linear"); newline(); break;
		default: stack_message(STACK_ERROR, 0, "Unknown interpolation method"); return;
	}
	data++;
	len--;

	print("Identification: ");
	error = decode_print_iso8859_1(&data, &len, 1);
	newline();
	if (error)
	{
		return;
	}

	while (len > 4)
	{
		uint16_t frequency  = (data[0]<<8) | data[1];
		uint16_t adjustment = (data[2]<<8) | data[3];

		print("Frequency: %"PRId16" Hz, Volume adjustment: %"PRId16, frequency, adjustment); newline();
		data+=4;
		len-=4;
	}
	if (len)
	{
		stack_message(STACK_WARNING, 0, "Some extra padding in the frame");
	}
}

static void decode_RBUF(uint8_t *data, uint32_t len)
{ /* ID3v220 used BUF*/
	uint32_t buffersize;
	print("Recommended buffer size"); newline();

	if (len < 4)
	{
		stack_message(STACK_ERROR, 0, "Frame too small");
		return;
	}
	buffersize = (data[0]<<16) | (data[1]<<8) | data[2];
	data+=3;
	len-=3;

	print("Buffer size: %" PRId32, buffersize); newline();
	if (data[0] & 0x01)
	{
		print("ID3 tag larger than Recommended buffer size can appear!"); newline();
	} else {
		print("Oversized ID3 tag shall not appear!"); newline();
	}
	data++;
	len--;

	if (len < 4)
	{
		print("(Offset to next tag omitted)"); newline();
	} else {
		buffersize = (data[0] << 24) | (data[1]<<16) | (data[2]<<8) | data[3];
		print("Offset to next tag: %" PRId32, buffersize); newline();
		data+=4;
		len-=4;
	}

	if (len)
	{
		stack_message(STACK_WARNING, 0, "Some extra padding in the frame");
	}
}

static void decode_MLLT(uint8_t *data, uint32_t len)
{ /* ID3v220 used MLL */
	print("MPEG location lookup table"); newline();

	if (len < 10)
	{
		stack_message(STACK_ERROR, 0, "Frame too small");
		return;
	}

	print("MPEG frames between reference:   %d",                 (data[0]<<8) | data[1]); newline();
	print("Bytes between reference:         %d", (data[2]<<24) | (data[3]<<8) | data[4]); newline();
	print("Milliseconds between reference:  %d", (data[5]<<24) | (data[6]<<8) | data[7]); newline();
	print("Bits for bytes deviation:        %d",                                data[8]); newline();
	print("Bits for milliseconds deviation: %d",                                data[9]); newline();
	data+=10;
	len-=10;
	print("DATA: 0x");
#warning TODO, decode this raw data...
	while (len)
	{
		print("%02x", *data);
		data++;
		len--;
	}
	newline();
}

static void decode_SEEK(uint8_t *data, uint32_t len)
{
	uint32_t min;
	print("Seek"); newline();

	if (len < 4)
	{
		stack_message(STACK_ERROR, 0, "Frame too small");
		return;
	}
	min = (data[0]<<24)|(data[1]<<16)|(data[2]<<8)|data[3];
	print("Minimum offset to next tag: %" PRId32, min); newline();
	data+=4;
	len-=4;

	if (len)
	{
		stack_message(STACK_WARNING, 0, "Some extra padding in the frame");
	}
}

static void decode_ASPI(uint8_t *data, uint32_t len)
{
	uint32_t S, L;
	uint16_t N;
	uint8_t  b;
	print("Audio seek point index"); newline();

	if (len < 11)
	{
		stack_message(STACK_ERROR, 0, "Frame too small");
		return;
	}

	S = (data[0]<<24)|(data[1]<<16)|(data[2]<<8)|data[3];
	L = (data[4]<<24)|(data[5]<<16)|(data[6]<<8)|data[7];
	N =                             (data[8]<<8)|data[9];
	b =                                         data[10];
	print("Indexed data start (S):     %" PRId32, S); newline();
	print("Indexed data length (L):    %" PRId32, L); newline();
	print("Number of index points (N): %" PRId16, N); newline();
	print("Bits per index point (b):   %" PRId8, b); newline();
	data+=11;
	len-=11;
	print("DATA: 0x");
#warning TODO, decode this raw data...
	while (len)
	{
		print("%02x", *data);
		data++;
		len--;
	}
	newline();
}

static void decode_SYTC(uint8_t *data, uint32_t len)
{ /* ID3v220 used STC */
	uint8_t format;
	print("Synchronised tempo codes"); newline();

	if (len < 1)
	{
		stack_message(STACK_ERROR, 0, "Frame too small");
		return;
	}
	format = *data;
	data++;
	len--;
	if ((format < 1) || (format > 2))
	{
		stack_message(STACK_ERROR, 0, "Format out of range");
		return;
	}

	while (len)
	{
		uint16_t value = *data;
		uint32_t timestamp;
		data++;
		len--;
		if (value == 255)
		{
			if (len < 1)
			{
				stack_message(STACK_ERROR, 0, "Out of data reading second byte of value");
				return;
			}
			value += *data;
			data++;
			len--;
		}
		if (len < 4)
		{
			stack_message(STACK_ERROR, 0, "Out of data reading timestamp");
			break;
		}
		timestamp = (data[0]<<24) | (data[1]<<16) | (data[2]<<8) | data[3];
		data+=4;
		len-=4;
		print("%" PRId32 "%s -- ", timestamp, (format==1)?"mpeg frames":"ms");
		if (value == 0)
		{
			print("beat-free");
		} else if (value == 1)
		{
			print("single beat");
		} else {
			print("%d BPM", (int)value);
		}
		newline();
	}

	if (len)
	{
		stack_message(STACK_WARNING, 0, "Some extra padding in the frame");
	}
}

static void decode_SYLT(uint8_t *data, uint32_t len, int version)
{ /* ID3v220 used SLT */
	int error;
	uint8_t text_encoding, format;
	int first = 1;
	print("Synchronised lyrics/text");
	newline();

	if (len < 6)
	{
		stack_message(STACK_ERROR, 0, "Frame too small");
		return;
	}

	text_encoding = data[0];
	if (((text_encoding >= 2) && (version < 4)) || (text_encoding >= 4))
	{
		stack_message(STACK_ERROR, 0, "Text_encoding out of range");
		return;
	}

	if ((data[1]<0x20)||(data[1]>=0x80)||
	    (data[2]<0x20)||(data[2]>=0x80)||
	    (data[3]<0x20)||(data[3]>=0x80))
	{
		print("language is invalid 0x%02x 0x%02x 0x%02x", data[1], data[2], data[3]);
	} else {
		print("language: \"%c%c%c\"", data[1], data[2], data[3]);
	}
	newline();

	format = data[4];
	if ((format < 0) || (format > 2))
	{
		stack_message(STACK_ERROR, 0, "Time stamp format out of range");
		return;
	}
	switch (data[5])
	{
		case 0x00: print("Content type: other"); newline(); break;
		case 0x01: print("Content type: lyrics"); newline(); break;
		case 0x02: print("Content type: text transcription"); newline(); break;
		case 0x03: print("Content type: movement/part name (e.g. \"Adagio\")"); newline(); break;
		case 0x04: print("Content type: events (e.g. \"Don Quijote enters the stage\")"); newline(); break;
		case 0x05: print("Content type: chord (e.g. \"Bb F Fsus\")"); newline(); break;
		case 0x06: print("Content type: trivia/'pop up' information"); newline(); break;
		case 0x07: print("Content type: URLs to webpages"); newline(); break;
		case 0x08: print("Content type: URLs to images"); newline(); break;
		default: stack_message(STACK_WARNING, 0, "Content type out of range"); break;
	}
	data+= 6;
	len-=6;

	while (len)
	{
		uint32_t timecode;
		switch (text_encoding)
		{
			case 0x00:
				print("LATIN1 ");
				error = decode_print_iso8859_1 (&data, &len, 1);
				break;
			case 0x01:
				print("UCS2 ");
				error = decode_print_UCS2 (&data, &len, 1, first);
				first = 0;
				break;
			case 0x02:
				print("UTF-16 ");
				error=decode_print_UTF16 (&data, &len, 1);
				break;
			case 0x03:
				print("UTF-8 ");
				error=decode_print_UTF8 (&data, &len, 1);
				break;
		}
		if (error)
		{
			newline();
			return;
		}
		if (len < 4)
		{
			newline();
			stack_message(STACK_ERROR, 0, "Ran out of data");
			break;
		}
		timecode = (data[0]<<24)|(data[1]<<16)|(data[2]<<8)|data[1];
		data+=4;
		len-=4;
		print(" %" PRId32 " %s", timecode, (format==1)?"mpeg frames":"ms"); newline();
	}

	if (len)
	{
		stack_message(STACK_WARNING, 0, "Some extra padding in the frame");
	}
}

static void decode_USLT(uint8_t *data, uint32_t len, int version)
{ /* ID3v220 used ULT */
	int error;
	uint8_t text_encoding;
	print("Unsynchronised lyrics/text transcription"); newline();

	if (len < 4)
	{
		stack_message(STACK_ERROR, 0, "Frame too small");
		return;
	}

	text_encoding = data[0];
	if (((text_encoding >= 2) && (version < 4)) || (text_encoding >= 4))
	{
		stack_message(STACK_ERROR, 0, "Text_encoding out of range");
		return;
	}

	if ((data[1]<0x20)||(data[1]>=0x80)||
	    (data[2]<0x20)||(data[2]>=0x80)||
	    (data[3]<0x20)||(data[3]>=0x80))
	{
		print("language is invalid 0x%02x 0x%02x 0x%02x", data[1], data[2], data[3]);
	} else {
		print("language: \"%c%c%c\"", data[1], data[2], data[3]);
	}
	newline();

	print("Content-Descriptor: ");
	switch (text_encoding)
	{
		case 0x00:
			print("LATIN1 ");
			error = decode_print_iso8859_1 (&data, &len, 1);
			break;
		case 0x01:
			print("UCS2 ");
			error = decode_print_UCS2 (&data, &len, 1, 1);
			break;
		case 0x02:
			print("UTF-16 ");
			error = decode_print_UTF16 (&data, &len, 1);
			break;
		case 0x03:
			print("UTF-8 ");
			error = decode_print_UTF8 (&data, &len, 1);
			break;
	}
	newline();
	if (error)
	{
		return;
	}
	print("Lyrics/text: ");
	switch (text_encoding)
	{
		case 0x00:
			print("LATIN1 "); /* we are laxed on require NULL here */
			error = decode_print_iso8859_1 (&data, &len, 0);
			break;
		case 0x01:
			print("UCS2 ");
			error = decode_print_UCS2 (&data, &len, 0, 0);
			break;
		case 0x02:
			print("UTF-16 ");
			error = decode_print_UTF16 (&data, &len, 0);
			break;
		case 0x03:
			print("UTF-8 ");
			error = decode_print_UTF8 (&data, &len, 0);
			break;
	}
	newline();
	if (error)
	{
		return;
	}
	if (len)
	{
		stack_message(STACK_WARNING, 0, "Some extra padding in the frame");
	}
}

static void decode_RVAD(uint8_t *data, uint32_t len, int version)
{ /* ID3v220 used RVA, replaced with RVA2 in ID3v240 */
	uint8_t flags;
	uint8_t bits;
	uint8_t bytesperchannel;
	int i;
	print("Relative volume adjustment"); newline();

	if (len < 2)
	{
		stack_message(STACK_ERROR, 0, "Frame too small");
		return;
	}

	flags = data[0];
	bits = data[1];
	data+=2;
	len-=2;

	if ((bits==0) || (bits > 64))
	{
		stack_message(STACK_ERROR, 0, "\"Bits used for volume descr.\" is out of range");
		return;
	}
	bytesperchannel = (bits + 7)>>3;

	if (flags & 0x03)
	{
		if (len < (bytesperchannel * 2 * ( (!!(flags & 0x01)) + (!!(flags & 0x02)) ) ) )
		{
			stack_message(STACK_ERROR, 0, "Ran out of data");
			return;
		}
		if (flags & 0x01)
		{
			print(" Relative volume change, right: 0x");
			for (i=0; i < bytesperchannel; i++)
			{
				print("%02x", data[0]);
				data++;
				len--;
			}
			newline();
		}
		if (flags & 0x02)
		{
			print("Relative volume change, left: 0x");
			for (i=0; i < bytesperchannel; i++)
			{
				print("%02x", data[0]);
				data++;
				len--;
			}
			newline();
		}
		if (flags & 0x01)
		{
			print("Peak volume right:             0x");
			for (i=0; i < bytesperchannel; i++)
			{
				print("%02x", data[0]);
				data++;
				len--;
			}
			newline();
		}
		if (flags & 0x02)
		{
			print("Peak volume left:             0x");
			for (i=0; i < bytesperchannel; i++)
			{
				print("%02x", data[0]);
				data++;
				len--;
			}
			newline();
		}
	}

	if (version < 3)
	{
		if (len)
		{
			stack_message(STACK_WARNING, 0, "Some extra padding in the frame");
		}
		return;
	}

	if (flags & 0x0c)
	{
		if (len < (bytesperchannel * 2 * ( (!!(flags & 0x08)) + (!!(flags & 0x04)) ) ) )
		{
			stack_message(STACK_ERROR, 0, "Ran out of data");
			return;
		}
		if (flags & 0x04)
		{
			print("Relative volume change, right back: 0x");
			for (i=0; i < bytesperchannel; i++)
			{
				print("%02x", data[0]);
				data++;
				len--;
			}
			newline();
		}
		if (flags & 0x08)
		{
			print("Relative volume change, left back: 0x");
			for (i=0; i < bytesperchannel; i++)
			{
				print("%02x", data[0]);
				data++;
				len--;
			}
			newline();
		}
		if (flags & 0x04)
		{
			print("Peak volume right back:             0x");
			for (i=0; i < bytesperchannel; i++)
			{
				print("%02x", data[0]);
				data++;
				len--;
			}
			newline();
		}
		if (flags & 0x08)
		{
			print("Peak volume left back:             0x");
			for (i=0; i < bytesperchannel; i++)
			{
				print("%02x", data[0]);
				data++;
				len--;
			}
			newline();
		}
	}

	if (flags & 0x10)
	{
		if (len < (bytesperchannel * 2) )
		{
			stack_message(STACK_ERROR, 0, "Ran out of data");
			return;
		}
		print("Relative volume change, center: 0x");
		for (i=0; i < bytesperchannel; i++)
		{
			print("%02x", data[0]);
			data++;
			len--;
		}
		newline();
		print("Peak volume center:             0x");
		for (i=0; i < bytesperchannel; i++)
		{
			print("%02x", data[0]);
			data++;
			len--;
		}
		newline();
	}

	if (flags & 0x20)
	{
		if (len < (bytesperchannel * 2) )
		{
			stack_message(STACK_ERROR, 0, "Ran out of data");
			return;
		}
		print("Relative volume change, bass: 0x");
		for (i=0; i < bytesperchannel; i++)
		{
			print("%02x", data[0]);
			data++;
			len--;
		}
		newline();
		print("Peak volume bass:             0x");
		for (i=0; i < bytesperchannel; i++)
		{
			print("%02x", data[0]);
			data++;
			len--;
		}
		newline();
	}

	if (len)
	{
		stack_message(STACK_WARNING, 0, "Some extra padding in the frame");
	}
}

static void decode_RVRB(uint8_t *data, uint32_t len)
{ /* ID3v220 used REV */
	print("Reverb"); newline();

	if (len < 12)
	{
		stack_message(STACK_ERROR, 0, "Frame too small");
		return;
	}

	print("Reverb left (ms)                 %d ms", (data[0]<<8)|data[1]); newline();
	print("Reverb right (ms)                %d ms", (data[2]<<8)|data[3]); newline();
	print("Reverb bounces, left             %d/255", data[4]); newline();
	print("Reverb bounces, right            %d/255", data[5]); newline();
	print("Reverb feedback, left to left    %d/255", data[6]); newline();
	print("Reverb feedback, left to right   %d/255", data[7]); newline();
	print("Reverb feedback, right to right  %d/255", data[8]); newline();
	print("Reverb feedback, right to left   %d/255", data[9]); newline();
	print("Premix left to right             %d/255", data[10]); newline();
	print("Premix right to left             %d/255", data[11]); newline();

	data+=12;
	len-=12;

	if (len)
	{
		stack_message(STACK_WARNING, 0, "Some extra padding in the frame");
	}
}

static void decode_RVA2(uint8_t *data, uint32_t len)
{
	int error;
	print("Relative volume adjustment (2)"); newline();

	print("Identification: ");
	error=decode_print_iso8859_1(&data, &len, 1);
	newline();
	if (error)
	{
		return;
	}

	while (len > 4)
	{
		uint16_t peakvolume = 0;
		switch (data[0])
		{
			case 0x00: print("Type of channel:        Other"); break;
			case 0x01: print("Type of channel:        Master volume"); break;
			case 0x02: print("Type of channel:        Front right"); break;
			case 0x03: print("Type of channel:        Front left"); break;
			case 0x04: print("Type of channel:        Back right"); break;
			case 0x05: print("Type of channel:        Back left"); break;
			case 0x06: print("Type of channel:        Front centre"); break;
			case 0x07: print("Type of channel:        Back centre"); break;
			case 0x08: print("Type of channel:        Subwooder"); break;
			default:   print("Type of channel:        (%d) Unknown", data[0]); break;
		}
		newline();

		print("Volume adjustment:      %d", (data[1]<<8)|data[2]); newline();
		print("Bits representing peak: %d", data[3]); newline();
		data+=4;
		len-=4;
		while (data[0] == 0xff)
		{
			if (!len)
			{
				stack_message(STACK_ERROR, 0, "Ran out of data");
				return;
			}
			peakvolume+=data[0];
			data++;
			len--;
		}
		if (!len)
		{
			stack_message(STACK_ERROR, 0, "Ran out of data");
			return;
		}
		peakvolume+=data[0];
		data++;
		len--;
		print("Peak volume:            %" PRId16, peakvolume); newline();
	}

	if (len)
	{
		stack_message(STACK_WARNING, 0, "Some extra padding in the frame");
	}
}

static void decode_POPM(uint8_t *data, uint32_t len)
{ /* ID3v220 used POP */
	int error;
	print("Popularimeter");

	if (!len)
	{
		stack_message(STACK_ERROR, 0, "Frame too small");
		newline();
		return;
	}

	print("Email to user: ");
	error=decode_print_iso8859_1 (&data, &len, 1);
	newline();
	if (error)
	{
		return;
	}

	if (!len)
	{
		stack_message(STACK_ERROR, 0, "No space for rating");
		return;
	}
	if (*data)
	{
		print("Rating: %d", *data);
	} else {
		print("Rating: unknown");
	}
	newline();
	data++;
	len--;

	if (!len)
	{
		print("Counter: omitted");
	} else {
		uint64_t counter = 0;
		while (len)
		{
			counter<<=8;
			counter |= *data;
			data++;
			len--;
		}
		print("Counter: %"PRId64, counter);
	}
	newline();
}

static void decode_LINK(uint8_t *data, uint32_t len)
{ /* ID3v220 used LEN */
	int error;
	print("Link"); newline();

	if (!len)
	{
		stack_message(STACK_ERROR, 0, "Frame too small");
		return;
	}

	print("URL: ");
	error = decode_print_iso8859_1 (&data, &len, 1);
	newline();
	if (error)
	{
		return;
	}

	print(" ID and additional data: "); /* we are laxed on the last zero here */
	error=decode_print_iso8859_1 (&data, &len, 0);
	newline();
	if (error)
	{
		return;
	}

	if (len)
	{
		stack_message(STACK_WARNING, 0, "Some extra padding in the frame");
	}
}

static void decode_PCNT(uint8_t *data, uint32_t len)
{ /* ID3v220 used CNT */
	uint64_t counter = 0;
	print("Play Counter");

	while (len)
	{
		counter<<=8;
		counter |= *data;
		data++;
		len--;
	}
	print(" - Counter: %"PRId64, counter);
	newline();
}

static void decode_AENC(uint8_t *data, uint32_t len)
{ /* ID3v220 used CRA */
	int error;
	print("Audio encryption"); newline();

	print("Owner identifier: ");
	error = decode_print_iso8859_1 (&data, &len, 1);
	newline();
	if (error)
	{
		return;
	}
	if (len < 4)
	{
		stack_message(STACK_ERROR, 0, "Ran out of data");
		return;
	}
	print("Preview start: %d MPEG frames", (data[0]<<8)|data[1]); newline();
	print("Preview length: %d MPEG frames", (data[2]<<8)|data[3]); newline();
	print("Encryption info: 0x");
	data+=4;
	len-=4;
	while (len)
	{
		print("%02x", data[0]);
		data++;
		len--;
	}
	newline();
}

static void decode_COMM(uint8_t *data, uint32_t len, int version)
{ /* ID3v220 used COM */
	uint8_t text_encoding;
	int error;
	print("Comments"); newline();

	if (len < 4)
	{
		stack_message(STACK_ERROR, 0, "Frame too small");
		return;
	}

	text_encoding = data[0];
	if (((text_encoding >= 2) && (version < 4)) || (text_encoding >= 4))
	{
		stack_message(STACK_ERROR, 0, "Text_encoding out of range");
		return;
	}
	if ((data[1]<0x20)||(data[1]>=0x80)||
	    (data[2]<0x20)||(data[2]>=0x80)||
	    (data[3]<0x20)||(data[3]>=0x80))
	{
		print("language is invalid 0x%02x 0x%02x 0x%02x", data[1], data[2], data[3]);
	} else {
		print("language: \"%c%c%c\"", data[1], data[2], data[3]);
	}
	newline();

	data+=4;
	len-=4;

	print("Content-Descriptor: ");
	switch (text_encoding)
	{
		case 0x00:
			print("LATIN1 ");
			error=decode_print_iso8859_1 (&data, &len, 1);
			break;
		case 0x01:
			print("UCS2 ");
			error=decode_print_UCS2 (&data, &len, 1, 1);
			break;
		case 0x02:
			print("UTF-16 ");
			error=decode_print_UTF16 (&data, &len, 1);
			break;
		case 0x03:
			print("UTF-8 ");
			error=decode_print_UTF8 (&data, &len, 1);
			break;
	}
	newline();
	if (error)
	{
		return;
	}
	print("Comment/text: ");
	switch (text_encoding)
	{
		case 0x00:
			print("LATIN1 "); /* we are laxed on require NL here */
			error=decode_print_iso8859_1 (&data, &len, 0);
			break;
		case 0x01:
			print("UCS2 ");
			error=decode_print_UCS2 (&data, &len, 0, 0);
			break;
		case 0x02:
			print("UTF-16 ");
			error=decode_print_UTF16 (&data, &len, 0);
			break;
		case 0x03:
			print("UTF-8 ");
			error=decode_print_UTF8 (&data, &len, 0);
			break;
	}
	newline();
	if (error)
	{
		return;
	}

	if (len)
	{
		stack_message(STACK_WARNING, 0, "Some extra padding in the frame");
	}
}

static void decode_PRIV(uint8_t *data, uint32_t len)
{ /* ID3v220 used COM */
	int error;
	print("Private"); newline();

	print("Owner identifier:");
	error=decode_print_iso8859_1 (&data, &len, 1);
	newline();
	if (error)
	{
		return;
	}

	print("The private data: 0x");
	while (len)
	{
		print("%02x", data[0]);
		data++;
		len--;
	}
	newline();
}


static void decode_USER(uint8_t *data, uint32_t len, int version)
{
	int error;
	uint8_t text_encoding;
	print("Terms of use"); newline();

	if (len < 4)
	{
		stack_message(STACK_ERROR, 0, "Frame too small");
		return;
	}

	text_encoding = data[0];
	if (((text_encoding >= 2) && (version < 4)) || (text_encoding >= 4))
	{
		stack_message(STACK_ERROR, 0, "Text_encoding out of range");
		return;
	}
	if ((data[1]<0x20)||(data[1]>=0x80)||
	    (data[2]<0x20)||(data[2]>=0x80)||
	    (data[3]<0x20)||(data[3]>=0x80))
	{
		print("language is invalid 0x%02x 0x%02x 0x%02x", data[1], data[2], data[3]);
	} else {
		print("language: \"%c%c%c\"", data[1], data[2], data[3]);
	}
	newline();
	data+=4;
	len-=4;

	print("text: ");
	switch (text_encoding)
	{
		case 0x00:
			print("LATIN1 "); /* we are laxed on require NL here */
			error = decode_print_iso8859_1 (&data, &len, 0);
			break;
		case 0x01:
			print("UCS2 ");
			error = decode_print_UCS2 (&data, &len, 0, 1);
			break;
		case 0x02:
			print("UTF-16 ");
			error=decode_print_UTF16 (&data, &len, 0);
			break;
		case 0x03:
			print("UTF-8 ");
			error=decode_print_UTF8 (&data, &len, 0);
			break;
	}
	newline();
	if (error)
	{
		return;
	}

	if (len)
	{
		stack_message(STACK_WARNING, 0, "Some extra padding in the frame");
	}
}

static void decode_COMR(uint8_t *data, uint32_t len, int version)
{
	int error;
	uint8_t text_encoding;
	print("Commercial frame"); newline();

	if (len < 4)
	{
		stack_message(STACK_ERROR, 0, "Frame too small");
		return;
	}

	text_encoding = data[0];
	if (((text_encoding >= 2) && (version < 4)) || (text_encoding >= 4))
	{
		stack_message(STACK_ERROR, 0, "Text_encoding out of range");
		return;
	}

	print("Price string: ");
	error = decode_print_iso8859_1 (&data, &len, 1);
	newline();
	if (error)
	{
		return;
	}

	if (len < 8)
	{
		stack_message(STACK_ERROR, 0, "Frame too small to contain field \"Valid until\"");
		return;
	}
	print("Valid Until: \"%c%c%c%c%c%c%c%c\"", data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]); newline();
	data+=8;
	len-=8;

	print("Contact URL: ");
	error = decode_print_iso8859_1 (&data, &len, 1);
	newline();
	if (error)
	{
		return;
	}

	if (len < 1)
	{
		stack_message(STACK_ERROR, 0, "Frame too small to contain field \"Received as\"");
		return;
	}
	switch (data[0])
	{
		case 0x00: print("Received as: Other"); break;
		case 0x01: print("Received as: Standard CD album with other songs"); break;
		case 0x02: print("Received as: Compressed audio on CD"); break;
		case 0x03: print("Received as: File over the Internet"); break;
		case 0x04: print("Received as: Stream over the Internet"); break;
		case 0x05: print("Received as: As note sheets"); break;
		case 0x06: print("Received as: As note sheets in a book with other sheets"); break;
		case 0x07: print("Received as: Music on other media"); break;
		case 0x08: print("Received as: Non-musical merchandise"); break;
		default:   print("Received as: (%d)Unknown", data[0]); break;
	}
	newline();
	data++;
	len--;


	print("name of seller: ");
	switch (text_encoding)
	{
		case 0x00:
			print("LATIN1 ");
			error = decode_print_iso8859_1 (&data, &len, 1);
			break;
		case 0x01:
			print("UCS2 ");
			error = decode_print_UCS2 (&data, &len, 1, 1);
			break;
		case 0x02:
			print("UTF-16 ");
			error = decode_print_UTF16 (&data, &len, 1);
			break;
		case 0x03:
			print("UTF-8 ");
			error = decode_print_UTF8 (&data, &len, 1);
			break;
	}
	newline();
	if (error)
	{
		return;
	}

	print("Description: ");
	switch (text_encoding)
	{
		case 0x00:
			print("LATIN1 "); /* we are laxed on require NL here */
			error = decode_print_iso8859_1 (&data, &len, 0);
			break;
		case 0x01:
			print("UCS2 ");
			error = decode_print_UCS2 (&data, &len, 0, 0);
			break;
		case 0x02:
			print("UTF-16 ");
			error = decode_print_UTF16 (&data, &len, 0);
			break;
		case 0x03:
			print("UTF-8 ");
			error = decode_print_UTF8 (&data, &len, 0);
			break;
	}
	newline();
	if (error)
	{
		return;
	}

	if (!len)
	{
		print("(Picture data omitted)"); newline();
		return;
	}

	print("Picture MIME type: ");
	error = decode_print_iso8859_1 (&data, &len, 1);
	newline();
	if (error)
	{
		return;
	}
	print("Seller logo: 0x");
	while (len)
	{
		print("%02x", data[0]);
		data++;
		len--;
	}
	newline();
}

static void decode_OWNE(uint8_t *data, uint32_t len, int version)
{
	int error;
	uint8_t text_encoding;
	print("Ownership"); newline();

	if (len < 4)
	{
		stack_message(STACK_ERROR, 0, "Frame too small");
		return;
	}

	text_encoding = data[0];
	if (((text_encoding >= 2) && (version < 4)) || (text_encoding >= 4))
	{
		stack_message(STACK_ERROR, 0, "Text_encoding out of range");
		return;
	}

	print("Price Payed: ");
	error = decode_print_iso8859_1 (&data, &len, 1);
	newline();
	if (error)
	{
		return;
	}

	if (len < 8)
	{
		stack_message(STACK_ERROR, 0, "Frame too small to contain field \"Date of purchase\"");
		return;
	}
	print("Date of purchase: \"%c%c%c%c%c%c%c%c\"", data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]); newline();
	data+=8;
	len-=8;

	print("Seller: ");
	switch (text_encoding)
	{
		case 0x00:
			print(" LATIN1 "); /* we are laxed on require NL here */
			error = decode_print_iso8859_1 (&data, &len, 0);
			break;
		case 0x01:
			print(" UCS2 ");
			error = decode_print_UCS2 (&data, &len, 0, 1);
			break;
		case 0x02:
			print(" UTF-16 ");
			error = decode_print_UTF16 (&data, &len, 0);
			break;
		case 0x03:
			print(" UTF-8 ");
			error = decode_print_UTF8 (&data, &len, 0);
			break;
	}
	newline();
	if (error)
	{
		return;
	}

	if (len)
	{
		stack_message(STACK_WARNING, 0, "Some extra padding in the frame");
	}
}

static void decode_GRID(uint8_t *data, uint32_t len)
{
	int error;

	print("Group ID registration"); newline();

	print("Owner identifier: ");
	error = decode_print_iso8859_1 (&data, &len, 1);
	newline();
	if (error)
	{
		return;
	}

	if (!len)
	{
		stack_message(STACK_ERROR, 0, "Frame too small to contain field \"Group symbol\"");
		return;
	}
	print("Group symbol: 0x%02x", data[0]); newline();
	data++;
	len--;
	print("Group dependent data: 0x");
	while (len)
	{
		print("%02x", data[0]);
		data++;
		len--;
	}
	newline();
}

static void decode_ENCR(uint8_t *data, uint32_t len)
{
	int error;
	print("Encryption method registration"); newline();

	print("Owner identifier: ");
	error = decode_print_iso8859_1 (&data, &len, 1);
	newline();
	if (error)
	{
		return;
	}

	if (!len)
	{
		stack_message(STACK_ERROR, 0, "Frame too small to contain field \"Group symbol\"");
		return;
	}
	print("Method symbol: 0x%02x", data[0]); newline();
	data++;
	len--;

	print("Encryption data: 0x");
	while (len)
	{
		print("%02x", data[0]);
		data++;
		len--;
	}
	newline();
}

static void decode_SIGN(uint8_t *data, uint32_t len)
{
	print("Signature"); newline();
	if (!len)
	{
		stack_message(STACK_ERROR, 0, "Frame too small to contain field \"Group symbol\"");
		return;
	}

	print("Group symbol: 0x%02x", data[0]); newline();
	data++;
	len--;

	print("Signature data: 0x");
	while (len)
	{
		print("%02x", data[0]);
		data++;
		len--;
	}
	newline();
}

static void decode_CRM(uint8_t *data, uint32_t len)
{ /* ID3v220 only */
	int error;
	print("Encrypted meta frame"); newline();

	print("   Owner identifier: ");
	error = decode_print_iso8859_1 (&data, &len, 1);
	newline();

	if (error)
	{
		return;
	}

	print("Content/explanation: ");
	error = decode_print_iso8859_1 (&data, &len, 1);
	newline();

	if (error)
	{
		return;
	}

	print("Content: 0x");
	while (len)
	{
		print("%02x", data[0]);
		data++;
		len--;
	}
	newline();
}

static void decode_APIC(uint8_t *data, uint32_t len, int version)
{ /* ID3v220 used PIC */
	uint8_t text_encoding;
	int error;

	print("Attached picture"); newline();

	if (len < 1)
	{
		stack_message(STACK_ERROR, 0, "No space for \"text_encoding\"");
		return;
	}

	text_encoding = data[0];
	data++;
	len--;

	if ((text_encoding >= 2 && version < 4) || (text_encoding >= 4))
	{
		stack_message(STACK_ERROR, 0, "Text_encoding out of range");
		return;
	}

	if (version <= 2)
	{
		if (len<3)
		{
			stack_message(STACK_ERROR, 0, "No space for \"Image format\"");
			return;
		}
		print("Image format: \"%c%c%c\"", data[0], data[1], data[2]);newline();
		len+=3;
	} else {
		int error;
		print("MIME/type: ");
		error = decode_print_iso8859_1(&data, &len, 1); newline();
		if (error)
		{
			return;
		}
	}

	if (len < 1)
	{
		stack_message(STACK_ERROR, 0, "No space for \"Picture Type\"");
		return;
	}
	switch (data[0])
	{
		case 0x00: print("Picture type: Other"); break;
		case 0x01: print("Picture type: 32x32 pixels 'file icon' (PNG only)"); break;
		case 0x02: print("Picture type: Other file icon"); break;
		case 0x03: print("Picture type: Cover (front)"); break;
		case 0x04: print("Picture type: Cover (back)"); break;
		case 0x05: print("Picture type: Leaflet page"); break;
		case 0x06: print("Picture type: Media (e.g. lable side of CD)"); break;
		case 0x07: print("Picture type: Lead artist/lead performer/soloist"); break;
		case 0x08: print("Picture type: Artist/performer"); break;
		case 0x09: print("Picture type: Conductor"); break;
		case 0x0A: print("Picture type: Band/Orchestra"); break;
		case 0x0B: print("Picture type: Composer"); break;
		case 0x0C: print("Picture type: Lyricist/text writer"); break;
		case 0x0D: print("Picture type: Recording Location"); break;
		case 0x0E: print("Picture type: During recording"); break;
		case 0x0F: print("Picture type: During performance"); break;
		case 0x10: print("Picture type: Movie/video screen capture"); break;
		case 0x11: print("Picture type: A bright coloured fish"); break;
		case 0x12: print("Picture type: Illustration"); break;
		case 0x13: print("Picture type: Band/artist logotype"); break;
		case 0x14: print("Picture type: Publisher/Studio logotype"); break;
		default:   print("Picture type: (%d)unknown", data[0]); break;
	}
	newline();
	data++;
	len--;

	print("Description: ");
	switch (text_encoding)
	{
		case 0x00:
			print("LATIN1 ");
			error = decode_print_iso8859_1(&data, &len, 1);
			break;
		case 0x01:
			print("UCS-2 ");
			error = decode_print_UCS2(&data, &len, 1, 1);
			break;
		case 0x02:
			print("UTF-16 ");
			error = decode_print_UTF16(&data, &len, 1);
			break;
		case 0x03:
			print("UTF-8 ");
			error = decode_print_UTF8(&data, &len, 1);
			break;
	}
	newline();
	if (error)
	{
		return;
	}
	print("Picture data: 0x");
	while (len)
	{
		print("%02x", data[0]);
		data++;
		len--;
	}
	newline();
}

static void decode_T(const char *description, uint8_t *data, uint32_t len, int version)
{
	int added = 0;
	int first = 1;
	uint8_t text_encoding;

	print("\"%-60s\" ", description);

	if (len < 1)
	{
		stack_message(STACK_ERROR, 0, "No space for \"Text encoding\"");
		return;
	}

	text_encoding = data[0];
	data++;
	len--;

	if ((text_encoding >= 2 && version < 4) || (text_encoding >= 4))
	{
		stack_message(STACK_ERROR, 0, "Text_encoding out of range");
		newline();
	}

	while (1)
	{
		int error = 1;
		switch (text_encoding)
		{
			case 0x00:
				print("LATIN1 ");
				error = decode_print_iso8859_1(&data, &len, 0);
				break;
			case 0x01:
				print("UCS-2 ");
				error = decode_print_UCS2(&data, &len, 0, first);
				break;
			case 0x02:
				print("UTF-16 ");
				error = decode_print_UTF16(&data, &len, 0);
				break;
			case 0x03:
				print("UTF-8 ");
				error = decode_print_UTF8(&data, &len, 0);
				break;
		}
		newline();
		if (error || (!len))
		{
			indent -= added;
			return;
		}
		if (version < 4)
		{
			if (len)
			{
				stack_message(STACK_WARNING, 0, "Some extra padding in the frame");
			}
			indent -= added;
			return;
		}
		if (first)
		{
			first=0;
			added=10;
			indent+=added;
		}
	}
}

static void decode_TXXX(uint8_t *data, uint32_t len, int version)
{
	int error;
	uint8_t text_encoding;
	print("Private text");

	if (len < 1)
	{
		stack_message(STACK_ERROR, 0 , "No space for \"Text encoding\"");
		return;
	}

	text_encoding = data[0];
	data++;
	len--;

	if ((text_encoding >= 2 && version < 4) || (text_encoding >= 4))
	{
		stack_message(STACK_ERROR, 0, "Text_encoding out of range");
		return;
	}

	switch (text_encoding)
	{
		case 0x00:
			print("LATIN1 ");
			error=decode_print_iso8859_1(&data, &len, 1);
			break;
		case 0x01:
			print("UCS-2 ");
			error=decode_print_UCS2(&data, &len, 1, 1);
			break;
		case 0x02:
			print("UTF-16 ");
			error=decode_print_UTF16(&data, &len, 1);
			break;
		case 0x03:
			print("UTF-8 ");
			error=decode_print_UTF8(&data, &len, 1);
			break;
	}
	if (error)
	{
		newline();
		return;
	}
	print(" = ");
	switch (text_encoding)
	{
		case 0x00:
			print("LATIN1 ");
			error = decode_print_iso8859_1(&data, &len, 0);
			break;
		case 0x01:
			print("UCS-2 ");
			error = decode_print_UCS2(&data, &len, 0, 0);
			break;
		case 0x02:
			print("UTF-16 ");
			error = decode_print_UTF16(&data, &len, 0);
			break;
		case 0x03:
			print("UTF-8 ");
			error = decode_print_UTF8(&data, &len, 0);
			break;
	}
	newline();
	if (error)
	{
		return;
	}
	if (len)
	{
		stack_message(STACK_WARNING, 0, "Some extra padding in the frame");
	}
}

static void decode_W(const char *description, uint8_t *data, uint32_t len)
{
	print("\"%s\" ", description);

	decode_print_iso8859_1(&data, &len, 0);
	newline();

	if (len)
	{
		stack_message(STACK_WARNING, 0, "Some extra padding in the frame");
	}
}

static void decode_WXXX(uint8_t *data, uint32_t len, int version)
{
	int error;
	uint8_t text_encoding;
	print("Private URL");
	newline();

	if (len < 1)
	{
		stack_message(STACK_ERROR, 0, "No space for \"Text encoding\"");
		return;
	}

	text_encoding = data[0];
	data++;
	len--;

	if ((text_encoding >= 2 && version < 4) || (text_encoding >= 4))
	{
		stack_message(STACK_ERROR, 0, "Text_encoding out of range");
		return;
	}

	switch (text_encoding)
	{
		case 0x00:
			print("LATIN1 ");
			error=decode_print_iso8859_1(&data, &len, 1);
			break;
		case 0x01:
			print("UCS-2 ");
			error=decode_print_UCS2(&data, &len, 1, 1);
			return;
		case 0x02:
			print("UTF-16 ");
			error=decode_print_UTF16(&data, &len, 1);
			break;
		case 0x03:
			print("UTF-8 ");
			error=decode_print_UTF8(&data, &len, 1);
			break;
	}
	if (error)
	{
		newline();
		return;
	}
	print(" = ");
	error = decode_print_iso8859_1(&data, &len, 0);
	newline();
	if (error)
	{
		return;
	}

	if (len)
	{
		stack_message(STACK_WARNING, 0, "Some extra padding in the frame");
	}
}

static void decode_Tn(const char *description, uint8_t *data, uint32_t len, int version)
{
	uint8_t text_encoding;
	int first = 1;
	print("%s", description);

	if (len < 1)
	{
		stack_message(STACK_ERROR, 0, "No space for \"Text encoding\"");
		newline();
		return;
	}

	text_encoding = data[0];
	data++;
	len--;

	if ((text_encoding >= 2 && version < 4) || (text_encoding >= 4))
	{
		stack_message(STACK_ERROR, 0, "Text_encoding out of range");
		newline();
	}

	switch (text_encoding)
	{
		case 0x00:
			print(" LATIN1");
			newline();
			while (len)
			{
				int error = decode_print_iso8859_1(&data, &len, 0);
				newline();
				if (error)
				{
					return;
				}
			}
			return;
		case 0x01:
			print(" UCS-2");
			newline();
			while (len)
			{
				int error=decode_print_UCS2(&data, &len, 0, first);
				newline();
				if (error)
				{
					return;
				}
				first=0;
			}
			return;
		case 0x02:
			print(" UTF-16");
			newline();
			while (len)
			{
				int error = decode_print_UTF16(&data, &len, 0);
				newline();
				if (error)
				{
					return;
				}
			}
			return;
		case 0x03:
			print(" UTF-8");
			newline();
			while (len)
			{
				int error = decode_print_UTF8(&data, &len, 0);
				newline();
				if (error)
				{
					return;
				}
			}
			return;
	}
}

static void *zalloc(void *q, unsigned int n, unsigned m)
{
	(void)q;
	return calloc(n, m);
}

static void zfree(void *q, void *p)
{
	(void)q;
	free(p);
}

static int32_t decode_id3v240_frame(uint8_t *ptr, uint32_t len)
{
	uint8_t FrameId[4];
	uint32_t inputsize;
	uint8_t *frameptr, *zlibptr=0;
	uint32_t framelen;
	uint32_t zlib_new_framelen=0xffffff;
	uint16_t flags;

	if (len < 11)
	{
		print("*-*-* ERROR, Minimum frame length is 11 bytes"); newline();
		return -1;
	}

	FrameId[0] = ptr[0];
	FrameId[1] = ptr[1];
	FrameId[2] = ptr[2];
	FrameId[3] = ptr[3];

	if ((FrameId[0] < 0x20) || (FrameId[0] >= 0x80) ||
	    (FrameId[1] < 0x20) || (FrameId[1] >= 0x80) ||
	    (FrameId[2] < 0x20) || (FrameId[2] >= 0x80) ||
	    (FrameId[3] < 0x20) || (FrameId[3] >= 0x80))
	{
		print("Invalid frame ID %02x %02x %02x %02x", FrameId[0], FrameId[1], FrameId[2], FrameId[3]); newline();
		return -1;
	}

	flags = (ptr[8]<<8)|(ptr[9]);

	if ((ptr[4] & 0x80) |
	    (ptr[5] & 0x80) |
	    (ptr[6] & 0x80) |
	    (ptr[7] & 0x80))
	{
		stack_message(STACK_WARNING, 0, "Framelength not valid not valid %02x %02x %02x %02x, trying to use 2.3.0 syntax", ptr[4], ptr[5], ptr[6], ptr[7]);
		framelen = inputsize = (ptr[4]<<24)|(ptr[5]<<16)|(ptr[6]<<8)|ptr[7]; /* attempt to ressurect broken FRAME, iTunes of some version*/
	} else {
		framelen = inputsize = (ptr[4]<<21)|(ptr[5]<<14)|(ptr[6]<<7)|ptr[7];
	}
	frameptr = ptr + 10;
	len-=10;

	print("Frame %c%c%c%c (%5"PRId32" bytes) - ", FrameId[0], FrameId[1], FrameId[2], FrameId[3], inputsize);
	indent++;

	if (inputsize > len)
	{
		stack_message(STACK_ERROR, 0, "We are missing %d of data in this frame", inputsize - len);
		newline();
		indent--;
		return -1;
	}

	print("FLAGS=0x%04"PRIx16" ", flags);

	/* part-1, parse header */
#if 0
	/* these are very noisy to see */
	if (flags & 0x4000) printf (" Tag  alter preservation: Frame should be discarded.\n"); else printf (" Tag  alter preservation: Frame should be preserved.\n");
	if (flags & 0x2000) printf (" File alter preservation: Frame should be discarded.\n"); else printf (" File alter preservation: Frame should be preserved.\n");
	if (flags & 0x1000) printf (" Frame is Read only\n"); else printf (" Frame is Read/Write\n");
#endif

	if (flags & 0x0008)
	{
		if (!(flags & 0x0001))
		{
			stack_message(STACK_ERROR, 0, "Compression flag set, without data length indicicator");
			newline();
			goto ignoreframe;
		}
	}
	if (flags & 0x0004)
	{
		uint8_t method;
		if (framelen < 1)
		{
			stack_message(STACK_ERROR, 0, "*-*-* ERROR, Encryption set, but no space for method field");
		} else {
			method = frameptr[0];
			frameptr++;
			framelen--;
			stack_message(STACK_INFO, 0, "Frame is encrypted (method %d), ignoring it", method);
		}
		newline();
		goto ignoreframe;
	}
	if (flags & 0x0040)
	{
		if (framelen < 1)
		{
			stack_message(STACK_WARNING, 0, "Grouping set, but no space for Frame grouping information");
		} else {
			stack_message(STACK_INFO, 0, "Frame is grouped with ID=%d", frameptr[0]);
			frameptr++;
			framelen--;
		}
	}
	if (flags & 0x0002)
	{
		stack_message(STACK_INFO, 0, "Frame is unsynced");
		unsync (frameptr, &framelen);
	}

	if (framelen == 0)
	{
		stack_message(STACK_ERROR, 0, "Frame with zero-length");
		flush();
		indent--;
		return -1;
	}

	if (flags & 0x0001)
	{
		uint32_t temp;

		if (framelen < 4)
		{
			stack_message(STACK_ERROR, 0, "Data-Length-Indicator set, but no space for outputsize field (skipping to next frame)");
			newline();
			goto ignoreframe;
		}

		temp = (frameptr[0]<<21)|(frameptr[1]<<14)|(frameptr[2]<<7)|frameptr[3];
		frameptr+=4;
		framelen-=4;

		if (flags & 0x0008)
		{
			if (temp > 32*1024*1024)
			{
				stack_message(STACK_WARNING, 0, "Decompression bigger than 32MB, blocking it");
				newline();
				goto ignoreframe;
			}
			zlib_new_framelen = temp;
			if (!zlib_new_framelen)
			{
				stack_message(STACK_ERROR, 0, "Decompression size of zero bytes");
				newline();
				goto ignoreframe;
			}
		} else {
			if (temp > framelen)
			{
				stack_message(STACK_WARNING, 0, "Data-Length-Indicator overshots the original framelen");
			} else {
				stack_message(STACK_INFO, 0, "framelen forcefully set from % " PRId32 " to %" PRId32 " bytes", framelen, temp);
				framelen = temp;
			}
		}
	}
	/* part-2 fix the data*/
	if (flags & 0x0008)
	{
		int result;

		z_stream strm;
		zlibptr = malloc (zlib_new_framelen);

		strm.zalloc = zalloc;
		strm.zfree = zfree;
		strm.opaque = (voidpf)0;
		strm.avail_in = framelen;
		strm.next_in = frameptr;
		strm.avail_out = zlib_new_framelen;
		strm.next_out = zlibptr;
		stack_message(STACK_INFO, 0, "Frame is compressed (inputsize=%" PRId32 ", outputsize=%" PRId32 ")", framelen, zlib_new_framelen);

		if (inflateInit(&strm))
		{
			stack_message(STACK_ERROR, 0, "Zlib failed to init");
			newline();
			goto ignoreframe;
		}
		result = inflate(&strm, Z_FINISH);
		if (result == Z_STREAM_ERROR ||
		    result == Z_NEED_DICT ||
		    result == Z_DATA_ERROR ||
		    result == Z_MEM_ERROR)
		{
			stack_message(STACK_ERROR, 0, "Zlib failed to decompress");
			if (result != Z_STREAM_ERROR)
			{
				inflateEnd (&strm);
			}
			newline();
			goto ignoreframe;
		}
		if (strm.avail_in != 0)
		{
			stack_message(STACK_WARNING, 0, "Zlib did not consume the entire input, %d bytes left", (int)strm.avail_in);
		}
		if (strm.avail_out != 0)
		{
			stack_message(STACK_WARNING, 0, "Zlib did not fill the entire output, %d bytes left", (int)strm.avail_out);
			memset (strm.next_out, 0, strm.avail_out);
		}
		inflateEnd (&strm);
		frameptr = zlibptr;
		framelen = zlib_new_framelen;
	}

	indent++;

	     if (!memcmp(FrameId, "AENC", 4)) decode_AENC(                                                                frameptr, framelen);
	else if (!memcmp(FrameId, "APIC", 4)) decode_APIC(                                                                frameptr, framelen, 4);
	else if (!memcmp(FrameId, "ASPI", 4)) decode_ASPI(                                                                frameptr, framelen);
	else if (!memcmp(FrameId, "COMM", 4)) decode_COMM(                                                                frameptr, framelen, 4);
	else if (!memcmp(FrameId, "COMR", 4)) decode_COMR(                                                                frameptr, framelen, 4);
	else if (!memcmp(FrameId, "ENCR", 4)) decode_ENCR(                                                                frameptr, framelen);
	else if (!memcmp(FrameId, "EQU2", 4)) decode_EQU2(                                                                frameptr, framelen);
	/* NOT SUPPORTED ANY MORE
	else if (!memcmp(FrameId, "EQUA", 4)) decode_EQUA(                                                                frameptr, framelen);
	*/
	else if (!memcmp(FrameId, "ETCO", 4)) decode_ETCO(                                                                frameptr, framelen, 4);
	else if (!memcmp(FrameId, "GEOB", 4)) decode_GEOB(                                                                frameptr, framelen, 4);
	else if (!memcmp(FrameId, "GRID", 4)) decode_GRID(                                                                frameptr, framelen);
	/* NOT SUPPORTED ANY MORE
	else if (!memcmp(FrameId, "IPLS", 4)) decode_Tn  ("Involved people list",                                         frameptr, framelen, 4);
	*/
	else if (!memcmp(FrameId, "LINK", 4)) decode_LINK(                                                                frameptr, framelen);
	else if (!memcmp(FrameId, "MCDI", 4)) decode_MCDI(                                                                frameptr, framelen);
	else if (!memcmp(FrameId, "MLLT", 4)) decode_MLLT(                                                                frameptr, framelen);
	else if (!memcmp(FrameId, "OWNE", 4)) decode_OWNE(                                                                frameptr, framelen, 4);
	else if (!memcmp(FrameId, "PRIV", 4)) decode_PRIV(                                                                frameptr, framelen);
	else if (!memcmp(FrameId, "PCNT", 4)) decode_PCNT(                                                                frameptr, framelen);
	else if (!memcmp(FrameId, "POPM", 4)) decode_POPM(                                                                frameptr, framelen);
	else if (!memcmp(FrameId, "POSS", 4)) decode_POSS(                                                                frameptr, framelen);
	else if (!memcmp(FrameId, "RBUF", 4)) decode_RBUF(                                                                frameptr, framelen);
	else if (!memcmp(FrameId, "RVA2", 4)) decode_RVA2(                                                                frameptr, framelen);
	/* NOT SUPPORTED ANY MORE
	else if (!memcmp(FrameId, "RVAD", 4)) decode_RVAD(                                                                frameptr, framelen, 4);
	*/
	else if (!memcmp(FrameId, "RVRB", 4)) decode_RVRB(                                                                frameptr, framelen);
	else if (!memcmp(FrameId, "SEEK", 4)) decode_SEEK(                                                                frameptr, framelen);
	else if (!memcmp(FrameId, "SIGN", 4)) decode_SIGN(                                                                frameptr, framelen);
	else if (!memcmp(FrameId, "SYLT", 4)) decode_SYLT(                                                                frameptr, framelen, 4);
	else if (!memcmp(FrameId, "SYTC", 3)) decode_SYTC(                                                                frameptr, framelen);
	else if (!memcmp(FrameId, "TALB", 4)) decode_T   ("Album/Movie/Show title",                                       frameptr, framelen, 4);
	else if (!memcmp(FrameId, "TBPM", 4)) decode_T   ("BPM",                                                          frameptr, framelen, 4);
	else if (!memcmp(FrameId, "TCOM", 4)) decode_T   ("Composer(s)",                                                  frameptr, framelen, 4);
	else if (!memcmp(FrameId, "TCON", 4)) decode_T   ("Content (ID3v1 (n)String encoded)",                            frameptr, framelen, 4);
	else if (!memcmp(FrameId, "TCOP", 4)) decode_T   ("Copyright message",                                            frameptr, framelen, 4);
	/* NOT SUPPORTED ANY MORE
	else if (!memcmp(FrameId, "TDAT", 4)) decode_T   ("Date (DDMM encoded)",                                          frameptr, framelen, 4);
	*/
	else if (!memcmp(FrameId, "TDEN", 4)) decode_T   ("Encoding time",                                                frameptr, framelen, 4);
	else if (!memcmp(FrameId, "TDLY", 4)) decode_T   ("Playlist delay",                                               frameptr, framelen, 4);
	else if (!memcmp(FrameId, "TDOR", 4)) decode_T   ("Original release time",                                        frameptr, framelen, 4);
	else if (!memcmp(FrameId, "TDRC", 4)) decode_T   ("Recording time",                                               frameptr, framelen, 4);
	else if (!memcmp(FrameId, "TDRL", 4)) decode_T   ("Release time",                                                 frameptr, framelen, 4);
	else if (!memcmp(FrameId, "TDTG", 4)) decode_T   ("Tagging time",                                                 frameptr, framelen, 4);
	else if (!memcmp(FrameId, "TENC", 4)) decode_T   ("Encoded by",                                                   frameptr, framelen, 4);
	else if (!memcmp(FrameId, "TEXT", 4)) decode_T   ("Lyricist(s)/text writer(s)",                                   frameptr, framelen, 4);
	else if (!memcmp(FrameId, "TFLT", 4)) decode_T   ("File type",                                                    frameptr, framelen, 4);
	/* NOT SUPPORTED ANY MORE
	else if (!memcmp(FrameId, "TIME", 4)) decode_T   ("Time (HHMM encoded)",                                          frameptr, framelen, 4);
	*/
	else if (!memcmp(FrameId, "TIPL", 4)) decode_T   ("Involved people list",                                         frameptr, framelen, 4);
	else if (!memcmp(FrameId, "TIT1", 4)) decode_T   ("Content group description",                                    frameptr, framelen, 4);
	else if (!memcmp(FrameId, "TIT2", 4)) decode_T   ("Title/Songname/Content description",                           frameptr, framelen, 4);
	else if (!memcmp(FrameId, "TIT3", 4)) decode_T   ("Subtitle/Description refinement",                              frameptr, framelen, 4);
	else if (!memcmp(FrameId, "TKEY", 4)) decode_T   ("Initial key",                                                  frameptr, framelen, 4);
	else if (!memcmp(FrameId, "TLAN", 4)) decode_T   ("Language(s) (ISO-639-2)",                                      frameptr, framelen, 4);
	else if (!memcmp(FrameId, "TLEN", 4)) decode_T   ("Length (in milliseconds)",                                     frameptr, framelen, 4);
	else if (!memcmp(FrameId, "TMCL", 4)) decode_T   ("Musician credits list",/*New*/                                 frameptr, framelen, 4);
	else if (!memcmp(FrameId, "TMED", 4)) decode_T   ("Media type",                                                   frameptr, framelen, 4);
	else if (!memcmp(FrameId, "TMOO", 4)) decode_T   ("Mood",                                                         frameptr, framelen, 4);
	else if (!memcmp(FrameId, "TOAL", 4)) decode_T   ("Original album/Movie/Show title",                              frameptr, framelen, 4);
	else if (!memcmp(FrameId, "TOFN", 4)) decode_T   ("Original filename",                                            frameptr, framelen, 4);
	else if (!memcmp(FrameId, "TOLY", 4)) decode_T   ("Original Lyricist(s)/text writer(s)",                          frameptr, framelen, 4);
	else if (!memcmp(FrameId, "TOPE", 4)) decode_T   ("Original artist(s)/performer(s)",                              frameptr, framelen, 4);
	/* NOT SUPPORTED ANY MORE
	else if (!memcmp(FrameId, "TORY", 4)) decode_T   ("Original release year",                                        frameptr, framelen, 4);
	*/
	else if (!memcmp(FrameId, "TOWN", 4)) decode_T   ("File Owner/License",/*New*/                                    frameptr, framelen, 4);
	else if (!memcmp(FrameId, "TPE1", 4)) decode_T   ("Lead artist(s)/Lead performer(s)/Soloist(s)/Performing group", frameptr, framelen, 4);
	else if (!memcmp(FrameId, "TPE2", 4)) decode_T   ("Band/Orchestra/Accompaniment",                                 frameptr, framelen, 4);
	else if (!memcmp(FrameId, "TPE3", 4)) decode_T   ("Conductor",                                                    frameptr, framelen, 4);
	else if (!memcmp(FrameId, "TPE4", 4)) decode_T   ("Interpreted, remixed, or otherwise modified by",               frameptr, framelen, 4);
	else if (!memcmp(FrameId, "TPOS", 4)) decode_T   ("Part of a set",                                                frameptr, framelen, 4);
	else if (!memcmp(FrameId, "TPRO", 4)) decode_T   ("Produced notice",                                              frameptr, framelen, 4);
	else if (!memcmp(FrameId, "TPUB", 4)) decode_T   ("Publisher",                                                    frameptr, framelen, 4);
	else if (!memcmp(FrameId, "TRCK", 4)) decode_T   ("Track number/Position in set (#/total encoded)",               frameptr, framelen, 4);
	/* NOT SUPPORTED ANY MORE
	else if (!memcmp(FrameId, "TRDA", 4)) decode_T   ("Recording dates",                                              frameptr, framelen, 4);
	*/
	else if (!memcmp(FrameId, "TRSN", 4)) decode_T   ("Internet radio station name",/*New*/                           frameptr, framelen, 4);
	else if (!memcmp(FrameId, "TRSO", 4)) decode_T   ("Internet radio station Owner",/*New*/                          frameptr, framelen, 4);
	/* NOT SUPPORTED ANY MORE
	else if (!memcmp(FrameId, "TSIZ", 4)) decode_T   ("Size (file in bytes)",                                         frameptr, framelen, 4);
	*/
	else if (!memcmp(FrameId, "TSOA", 4)) decode_T   ("Album sort order",                                             frameptr, framelen, 4);
	else if (!memcmp(FrameId, "TSOP", 4)) decode_T   ("Performer sort order",                                         frameptr, framelen, 4);
	else if (!memcmp(FrameId, "TSOT", 4)) decode_T   ("Title sort order",                                             frameptr, framelen, 4);
	else if (!memcmp(FrameId, "TSRC", 4)) decode_T   ("International Standard Recording Code (ISO 3901:1986)",        frameptr, framelen, 4);
	else if (!memcmp(FrameId, "TSSE", 4)) decode_T   ("Software/hardware and settings used for encoding",             frameptr, framelen, 4);
	else if (!memcmp(FrameId, "TSST", 4)) decode_T   ("Set subtitle"                                    ,             frameptr, framelen, 4);
	/* NOT SUPPORTED ANY MORE
	else if (!memcmp(FrameId, "TYER", 4)) decode_T   ("Year (YYYY encoded)",                                          frameptr, framelen, 4);
	*/
	else if (!memcmp(FrameId, "TXXX", 4)) decode_TXXX(                                                                frameptr, framelen, 4);
	else if (!memcmp(FrameId, "UFID", 4)) decode_UFID(                                                                frameptr, framelen);
	else if (!memcmp(FrameId, "USER", 4)) decode_USER(                                                                frameptr, framelen, 4);
	else if (!memcmp(FrameId, "USLT", 3)) decode_USLT(                                                                frameptr, framelen, 4);
	else if (!memcmp(FrameId, "WCOM", 4)) decode_W   ("Commercial information",                                       frameptr, framelen);
	else if (!memcmp(FrameId, "WCOP", 4)) decode_W   ("Copyright/Legal information",                                  frameptr, framelen);
	else if (!memcmp(FrameId, "WOAF", 4)) decode_W   ("Official audio file webpage",                                  frameptr, framelen);
	else if (!memcmp(FrameId, "WOAR", 4)) decode_W   ("Official artist/performer webpage",                            frameptr, framelen);
	else if (!memcmp(FrameId, "WOAS", 4)) decode_W   ("Official audio source webpage",                                frameptr, framelen);
	else if (!memcmp(FrameId, "WORS", 4)) decode_W   ("Official internet radio station homepage",/*New*/              frameptr, framelen);
	else if (!memcmp(FrameId, "WPAY", 4)) decode_W   ("Payment",/*New*/                                               frameptr, framelen);
	else if (!memcmp(FrameId, "WPUB", 4)) decode_W   ("Publishers official webpage",                                  frameptr, framelen);
	else if (!memcmp(FrameId, "WXXX", 4)) decode_WXXX(                                                                frameptr, framelen,4);
	else if ((FrameId[0]=='T')&&
		(((FrameId[1]>='0')&&(FrameId[1]<='9'))||((FrameId[1]>='a')&&(FrameId[1]<='z'))||((FrameId[1]>='A')&&(FrameId[1]<='Z')))&&
		(((FrameId[2]>='0')&&(FrameId[2]<='9'))||((FrameId[2]>='a')&&(FrameId[2]<='z'))||((FrameId[2]>='A')&&(FrameId[2]<='Z')))&&
		(((FrameId[3]>='0')&&(FrameId[3]<='9'))||((FrameId[3]>='a')&&(FrameId[3]<='z'))||((FrameId[3]>='A')&&(FrameId[3]<='Z'))))
	{
		decode_T("Unknown Text", frameptr, framelen, 4);
	}
	else if ((FrameId[0]=='W')&&
		(((FrameId[1]>='0')&&(FrameId[1]<='9'))||((FrameId[1]>='a')&&(FrameId[1]<='z'))||((FrameId[1]>='A')&&(FrameId[1]<='Z')))&&
		(((FrameId[2]>='0')&&(FrameId[2]<='9'))||((FrameId[2]>='a')&&(FrameId[2]<='z'))||((FrameId[2]>='A')&&(FrameId[2]<='Z')))&&
		(((FrameId[3]>='0')&&(FrameId[3]<='9'))||((FrameId[3]>='a')&&(FrameId[3]<='z'))||((FrameId[3]>='A')&&(FrameId[3]<='Z'))))
	{
		decode_W("Unknown URL", frameptr, framelen);
	} else {
		print("Unknown frame"); newline();
	}
	flush();
	indent--;
ignoreframe:
	indent--;
	if (zlibptr)
	{
		free (zlibptr);
		zlibptr = 0;
	}
	return (int32_t)(inputsize + 10);
}

static void decode_id3v240(uint8_t *data, uint32_t _length, int fd)
{
	uint8_t *ptr = data;
	uint32_t len = _length;
	uint8_t flags = data[5];

	ptr = data;
	len = _length;

	if (flags & 0x80)
	{
		print(" UNSYNC");
		unsync(ptr, &len);
	}
	if (flags & 0x20)
	{
		print(" EXPERIMENTAL");
	}
	if (flags & 0x10)
	{
		print(" FOOTER");
	}

	ptr+=10;
	len-=10;

	indent++;

	if (flags & 0x40)
	{ /* extended header */
		uint32_t _elength, elen;
		uint8_t *eptr = ptr;
		uint8_t eflags;

		print(" EXTENDED HEADER");newline();

		if ((eptr[0] & 0x80) ||
		    (eptr[1] & 0x80) ||
		    (eptr[2] & 0x80) ||
		    (eptr[3] & 0x80))
		{
			stack_message(STACK_ERROR, 0, "Extended header length has a MSB set 0x%02x 0x%02x 0x%02x 0x%02x", eptr[0], eptr[1], eptr[2], eptr[3]);
			flush();
			indent--;
			return;
		}
		_elength = (eptr[0] << 21) |
		           (eptr[1] << 14) |
		           (eptr[2] << 7) |
		           (eptr[3]);
		if (_elength < 6)
		{
			stack_message(STACK_ERROR, 0, "Extended header length too small: %"PRId32, _elength);
			flush();
			indent--;
			return;
		}
		if (_elength >= len)
		{
			stack_message(STACK_ERROR, 0, "Extended header length too big: %"PRId32" > %"PRId32, _elength, len);
			flush();
			indent--;
			return;
		}
		elen = _elength;
		if (eptr[4] != 0x01)
		{
			stack_message(STACK_WARNING, 0, "Non-standard number of flag bytes: %"PRId32" != 0", eptr[4]);
			flush();
			goto skip_eheader_v240;
		}
		eflags = eptr[5];

		eptr += 6;
		elen -= 6;

		if (eflags & 0x40)
		{
			if (elen < 1)
			{
				stack_message(STACK_WARNING, 0, "Extended header ran out of space");
				flush();
				goto skip_eheader_v240;
			}
			if (eptr[0] != 0x00)
			{
				stack_message(STACK_WARNING, 0, "\"update_flags\" should not have data");
				flush();
				goto skip_eheader_v240;
			}
			print("ID3 TAG IS AN UPDATE OF EARLIER DATA"); newline();
		} else {
			print("ID3 TAG IS A REPLACEMENT OF EARLIER DATA"); newline();
		}

		if (eflags & 0x20)
		{
			if (elen < 6)
			{
				stack_message(STACK_WARNING, 0, "Extended header ran out of space");
				flush();
				goto skip_eheader_v240;
			}
			if (eptr[0] != 0x05)
			{
				stack_message(STACK_WARNING, 0, "CRC should have 5 bytes of data: %d", eptr[0]);
				flush();
				goto skip_eheader_v240;
			}
			if ((eptr[1] & 0xf0) ||
			    (eptr[2] & 0x80) ||
			    (eptr[3] & 0x80) ||
			    (eptr[4] & 0x80) ||
			    (eptr[5] & 0x80))
			{
				stack_message(STACK_WARNING, 0, "CRC data stream not valid, MSB set: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x", eptr[1], eptr[2], eptr[3], eptr[4], eptr[5]);
				flush();
				goto skip_eheader_v240;
			}
			uint32_t crc = (eptr[1] << 28) |
			               (eptr[2] << 21) |
			               (eptr[3] << 14) |
			               (eptr[4] << 7) |
			               (eptr[5]);
			print("CRC = 0x%08" PRIx32, crc); newline();
			eptr += 6;
			elen -= 6;
		}
		if (eflags & 0x10)
		{
			if (elen < 2)
			{
				stack_message(STACK_WARNING, 0, "Extended header ran out of space");
				flush();
				goto skip_eheader_v240;
			}
			if (eptr[0] != 0x01)
			{
				stack_message(STACK_WARNING, 0, "\"Restrictions\" should have 1 bytes of data: %d", eptr[0]);
				flush();
				goto skip_eheader_v240;
			}
			print("RESTRICTIONS IN TAG"); newline();
			switch (eptr[1] & 0xc0)
			{
				case 0x00: print("No more than 128 frames and 1 MB total tag size."); break;
				case 0x40: print("No more than 64 frames and 128 KB total tag size."); break;
				case 0x80: print("No more than 32 frames and 40 KB total tag size."); break;
				case 0xc0: print("No more than 32 frames and 4 KB total tag size."); break;
			}
			newline();
			switch (eptr[1] & 0x40)
			{
				case 0x00: print("Text encoding not restricted."); break;
				case 0x40: print("Strings are only encoded with ISO-8859-1 [ISO-8859-1] or UTF-8 [UTF-8]."); break;
			}
			newline();
			switch (eptr[1] & 0x0c)
			{
				case 0x00: print("Text lengths not restricted."); break;
				case 0x04: print("No string is longer than 1024 characters."); break;
				case 0x08: print("No string is longer than 128 characters."); break;
				case 0x0c: print("No string is longer than 30 characters."); break;
			}
			newline();
			switch (eptr[1] & 0x04)
			{
				case 0x00: print("Image encoding not restricted."); break;
				case 0x04: print("Images are encoded only with PNG [PNG] or JPEG [JFIF]."); break;
			}
			newline();
			switch (eptr[1] & 0x03)
			{
				case 0x00: print("Image resolution not restricted."); break;
				case 0x01: print("All images are 256x256 pixels or smaller."); break;
				case 0x02: print("All images are 64x64 pixels or smaller."); break;
				case 0x03: print("All images are exactly 64x64 pixels, unless required otherwise."); break;
			}
			newline();

			eptr += 2;
			elen -= 2;
		}
		if (elen != 0)
		{
			stack_message(STACK_WARNING, 0, "Unused data in extended header: %" PRId32 "bytes left", elen);
			flush();
		}
skip_eheader_v240:
		ptr += _elength;
		len -= _elength;
	}

	while (len > 10)
	{
		int32_t framelen;

		if (ptr[0] == 0)
		{
			break;
		}

		indent++;
		framelen = decode_id3v240_frame (ptr, len);
		indent--;
		if (framelen < 0)
		{
			indent--;
			return;
		}

		ptr += framelen;
		len -= framelen;
	}

	if (len)
	{
		if (flags & 0x10)
		{
			stack_message(STACK_WARNING, 0, "PADDING + FOOTER %d bytes", (int) len);
		} else {
			stack_message(STACK_INFO, 0, "%d bytes of PADDING to inspect", (int)len);
		}
	}
	while (len)
	{
		if (ptr[0])
		{
			stack_message(STACK_WARNING, 0, "Non-zero data in padding: 0x%02x", ptr[0]);
		}
		ptr++;
		len--;
	}

	{
		uint8_t fdata[10];
		if (read (fd, fdata, 10) != 10)
		{
			if (flags & 0x10)
			{
				stack_message(STACK_ERROR, 0, "Unable to read FOOTER"); flush();
			}
			indent--;
			return;
		}
		if ((fdata[0] == '3') &&
		    (fdata[1] == 'D') &&
		    (fdata[2] == 'I'))
		{
			print("%08lx FOOTER v2.%d.%d 10 bytes", (long) lseek (fd, 0, SEEK_CUR)-10, fdata[3], fdata[4]); newline();
			if (fdata[5] != data[5]) {stack_message(STACK_WARNING, 0, "Flags does not match the copy from the header"); flush(); }
			if ((fdata[6] != data[6]) ||
			    (fdata[7] != data[7]) ||
			    (fdata[8] != data[8]) ||
			    (fdata[9] != data[9])) {stack_message(STACK_WARNING, 0, "Size does not match the copy from the header"); flush(); }
		}
	}
	indent--;
}

static int32_t decode_id3v230_frame(uint8_t *ptr, uint32_t len)
{
	uint8_t FrameId[4];
	uint32_t inputsize;
	uint8_t *frameptr, *zlibptr=0;
	uint32_t framelen;
	uint32_t zlib_new_framelen = 0xffffff;
	uint16_t flags;

	if (len < 11)
	{
		stack_message(STACK_ERROR, 0, "Minimum frame length is 11 bytes");
		newline();
		return -1;
	}

	FrameId[0] = ptr[0];
	FrameId[1] = ptr[1];
	FrameId[2] = ptr[2];
	FrameId[3] = ptr[3];

	if ((FrameId[0] < 0x20) || (FrameId[0] >= 0x80) ||
	    (FrameId[1] < 0x20) || (FrameId[1] >= 0x80) ||
	    (FrameId[2] < 0x20) || (FrameId[2] >= 0x80) ||
	    (FrameId[3] < 0x20) || (FrameId[3] >= 0x80))
	{
		stack_message(STACK_ERROR, 0, "Invalid frame ID %02x %02x %02x %02x", FrameId[0], FrameId[1], FrameId[2], FrameId[3]);
		newline();
		return -1;
	}

	flags = (ptr[8]<<8)|(ptr[9]);

	framelen = inputsize = (ptr[4]<<24)|(ptr[5]<<16)|(ptr[6]<<8)|ptr[7];

	frameptr = ptr + 10;
	len-=10;

	print("Frame %c%c%c%c (%5"PRId32" bytes) - ", FrameId[0], FrameId[1], FrameId[2], FrameId[3], inputsize);
	indent++;

	if (inputsize > len)
	{
		stack_message(STACK_ERROR, 0, "We are missing %d of data in this frame", inputsize - len);
		newline();
		indent--;
		return -1;
	}

	print("FLAGS=0x%04"PRIx16" ", flags);

	/* part-1, parse header */
#if 0
	print("flags=0x%04", flags);
#endif
#if 0
	/* these are very noisy to see */
	if (flags & 0x8000) stack_message("Tag  alter preservation: Frame should be discarded."); else stack_message("Tag  alter preservation: Frame should be preserved.");
	if (flags & 0x4000) stack_message("File alter preservation: Frame should be discarded."); else stack_message("File alter preservation: Frame should be preserved.");
	if (flags & 0x2000) stack_message("Frame is Read only"); else stack_message(" Frame is Read/Write");
#endif

	if (flags & 0x0080)
	{
		if (framelen < 4)
		{
			stack_message(STACK_ERROR, 0, "Compression set, but no space for outputsize field");
			newline();
			goto ignoreframe;
		}
		zlib_new_framelen  = (frameptr[0]<<24)|(frameptr[1]<<16)|(frameptr[2]<<8)|frameptr[3];
		if (zlib_new_framelen > 32*1024*1024)
		{
			stack_message(STACK_WARNING, 0, "Decompression bigger than 32MB, blocking it");
			newline();
			goto ignoreframe;
		}
		if (!zlib_new_framelen)
		{
			stack_message(STACK_WARNING, 0, "Decompression size of zero bytes");
			newline();
			goto ignoreframe;
		}
		frameptr+=4;
		framelen-=4;
	}
	if (flags & 0x0040)
	{
		uint8_t method;
		if (framelen < 1)
		{
			stack_message(STACK_WARNING, 0, "Encryption set, but not space for method field");
		}
		method = frameptr[0];
		frameptr++;
		framelen--;
		stack_message(STACK_INFO, 0, "Frame is encrypted (method %d), ignoring it", method);
		newline();
		goto ignoreframe;
	}
	if (flags & 0x0020)
	{
		if (framelen < 1)
		{
			stack_message(STACK_WARNING, 0, "Grouping set, but no space for Frame grouping information");
		}
		stack_message(STACK_INFO, 0, "Frame is grouped with ID=%d", frameptr[0]);
		frameptr++;
		framelen--;
	}
	/* part-2 fix the data*/

	if (framelen == 0)
	{
		stack_message(STACK_ERROR, 0, "Frame with zero-length");
		indent--;
		flush();
		return -1;
	}

	if (flags & 0x0080)
	{
		int result;

		z_stream strm;
		zlibptr = malloc (zlib_new_framelen);

		strm.zalloc = zalloc;
		strm.zfree = zfree;
		strm.opaque = (voidpf)0;
		strm.avail_in = framelen;
		strm.next_in = frameptr;
		strm.avail_out = zlib_new_framelen;
		strm.next_out = zlibptr;

		stack_message(STACK_INFO, 0, "Frame is compressed (inputsize=%" PRId32 ", outputsize=%" PRId32 ")", framelen, zlib_new_framelen);

		if (inflateInit(&strm))
		{
			stack_message(STACK_ERROR, 0, "Zlib failed to init");
			newline();
			goto ignoreframe;
		}
		result = inflate(&strm, Z_FINISH);
		if (result == Z_STREAM_ERROR ||
		    result == Z_NEED_DICT ||
		    result == Z_DATA_ERROR ||
		    result == Z_MEM_ERROR)
		{
			stack_message(STACK_ERROR, 0, "Zlib failed to decompress");
			if (result != Z_STREAM_ERROR)
			{
				inflateEnd (&strm);
			}
			newline();
			goto ignoreframe;
		}
		if (strm.avail_in != 0)
		{
			stack_message(STACK_WARNING, 0, "Zlib did not consume the entire input, %d bytes left", (int)strm.avail_in);
		}
		if (strm.avail_out != 0)
		{
			stack_message(STACK_WARNING, 0, "Zlib did not fill the entire output, %d bytes left", (int)strm.avail_out);
			memset (strm.next_out, 0, strm.avail_out);
		}
		inflateEnd (&strm);
		frameptr = zlibptr;
		framelen = zlib_new_framelen;
	}

	indent++;

	     if (!memcmp(FrameId, "AENC", 4)) decode_AENC(                                                                frameptr, framelen);
	else if (!memcmp(FrameId, "APIC", 4)) decode_APIC(                                                                frameptr, framelen, 3);
	else if (!memcmp(FrameId, "COMM", 4)) decode_COMM(                                                                frameptr, framelen, 3);
	else if (!memcmp(FrameId, "COMR", 4)) decode_COMR(                                                                frameptr, framelen, 3);
	else if (!memcmp(FrameId, "ENCR", 4)) decode_ENCR(                                                                frameptr, framelen);
	else if (!memcmp(FrameId, "EQUA", 4)) decode_EQUA(                                                                frameptr, framelen);
	else if (!memcmp(FrameId, "ETCO", 4)) decode_ETCO(                                                                frameptr, framelen, 3);
	else if (!memcmp(FrameId, "GEOB", 4)) decode_GEOB(                                                                frameptr, framelen, 3);
	else if (!memcmp(FrameId, "GRID", 4)) decode_GRID(                                                                frameptr, framelen);
	else if (!memcmp(FrameId, "IPLS", 4)) decode_Tn  ("Involved people list",                                         frameptr, framelen, 3);
	else if (!memcmp(FrameId, "LINK", 4)) decode_LINK(                                                                frameptr, framelen);
	else if (!memcmp(FrameId, "MCDI", 4)) decode_MCDI(                                                                frameptr, framelen);
	else if (!memcmp(FrameId, "MLLT", 4)) decode_MLLT(                                                                frameptr, framelen);
	else if (!memcmp(FrameId, "OWNE", 4)) decode_OWNE(                                                                frameptr, framelen, 3);
	else if (!memcmp(FrameId, "PRIV", 4)) decode_PRIV(                                                                frameptr, framelen);
	else if (!memcmp(FrameId, "PCNT", 4)) decode_PCNT(                                                                frameptr, framelen);
	else if (!memcmp(FrameId, "POPM", 4)) decode_POPM(                                                                frameptr, framelen);
	else if (!memcmp(FrameId, "POSS", 4)) decode_POSS(                                                                frameptr, framelen);
	else if (!memcmp(FrameId, "RBUF", 4)) decode_RBUF(                                                                frameptr, framelen);
	else if (!memcmp(FrameId, "RVAD", 4)) decode_RVAD(                                                                frameptr, framelen, 3);
	else if (!memcmp(FrameId, "RVRB", 4)) decode_RVRB(                                                                frameptr, framelen);
	else if (!memcmp(FrameId, "SYLT", 4)) decode_SYLT(                                                                frameptr, framelen, 3);
	else if (!memcmp(FrameId, "SYTC", 3)) decode_SYTC(                                                                frameptr, framelen);
	else if (!memcmp(FrameId, "TALB", 4)) decode_T   ("Album/Movie/Show title",                                       frameptr, framelen, 3);
	else if (!memcmp(FrameId, "TBPM", 4)) decode_T   ("BPM",                                                          frameptr, framelen, 3);
	else if (!memcmp(FrameId, "TCOM", 4)) decode_T   ("Composer(s)",                                                  frameptr, framelen, 3);
	else if (!memcmp(FrameId, "TCON", 4)) decode_T   ("Content (ID3v1 (n)String encoded)",                            frameptr, framelen, 3);
	else if (!memcmp(FrameId, "TCOP", 4)) decode_T   ("Copyright message",                                            frameptr, framelen, 3);
	else if (!memcmp(FrameId, "TDAT", 4)) decode_T   ("Date (DDMM encoded)",                                          frameptr, framelen, 3);
	else if (!memcmp(FrameId, "TDLY", 4)) decode_T   ("Playlist delay",                                               frameptr, framelen, 3);
	else if (!memcmp(FrameId, "TENC", 4)) decode_T   ("Encoded by",                                                   frameptr, framelen, 3);
	else if (!memcmp(FrameId, "TEXT", 4)) decode_T   ("Lyricist(s)/text writer(s)",                                   frameptr, framelen, 3);
	else if (!memcmp(FrameId, "TFLT", 4)) decode_T   ("File type",                                                    frameptr, framelen, 3);
	else if (!memcmp(FrameId, "TIME", 4)) decode_T   ("Time (HHMM encoded)",                                          frameptr, framelen, 2);
	else if (!memcmp(FrameId, "TIT1", 4)) decode_T   ("Content group description",                                    frameptr, framelen, 3);
	else if (!memcmp(FrameId, "TIT2", 4)) decode_T   ("Title/Songname/Content description",                           frameptr, framelen, 3);
	else if (!memcmp(FrameId, "TIT3", 4)) decode_T   ("Subtitle/Description refinement",                              frameptr, framelen, 3);
	else if (!memcmp(FrameId, "TKEY", 4)) decode_T   ("Initial key",                                                  frameptr, framelen, 3);
	else if (!memcmp(FrameId, "TLAN", 4)) decode_T   ("Language(s) (ISO-639-2)",                                      frameptr, framelen, 3);
	else if (!memcmp(FrameId, "TLEN", 4)) decode_T   ("Length (in milliseconds)",                                     frameptr, framelen, 3);
	else if (!memcmp(FrameId, "TMED", 4)) decode_T   ("Media type",                                                   frameptr, framelen, 3);
	else if (!memcmp(FrameId, "TOAL", 4)) decode_T   ("Original album/Movie/Show title",                              frameptr, framelen, 3);
	else if (!memcmp(FrameId, "TOFN", 4)) decode_T   ("Original filename",                                            frameptr, framelen, 3);
	else if (!memcmp(FrameId, "TOLY", 4)) decode_T   ("Original Lyricist(s)/text writer(s)",                          frameptr, framelen, 3);
	else if (!memcmp(FrameId, "TOPE", 4)) decode_T   ("Original artist(s)/performer(s)",                              frameptr, framelen, 3);
	else if (!memcmp(FrameId, "TORY", 4)) decode_T   ("Original release year",                                        frameptr, framelen, 3);
	else if (!memcmp(FrameId, "TOWN", 4)) decode_T   ("File Owner/License",/*New*/                                    frameptr, framelen, 3);
	else if (!memcmp(FrameId, "TPE1", 4)) decode_T   ("Lead artist(s)/Lead performer(s)/Soloist(s)/Performing group", frameptr, framelen, 3);
	else if (!memcmp(FrameId, "TPE2", 4)) decode_T   ("Band/Orchestra/Accompaniment",                                 frameptr, framelen, 3);
	else if (!memcmp(FrameId, "TPE3", 4)) decode_T   ("Conductor",                                                    frameptr, framelen, 3);
	else if (!memcmp(FrameId, "TPE4", 4)) decode_T   ("Interpreted, remixed, or otherwise modified by",               frameptr, framelen, 3);
	else if (!memcmp(FrameId, "TPOS", 4)) decode_T   ("Part of a set",                                                frameptr, framelen, 3);
	else if (!memcmp(FrameId, "TPUB", 4)) decode_T   ("Publisher",                                                    frameptr, framelen, 3);
	else if (!memcmp(FrameId, "TRCK", 4)) decode_T   ("Track number/Position in set (#/total encoded)",               frameptr, framelen, 3);
	else if (!memcmp(FrameId, "TRDA", 4)) decode_T   ("Recording dates",                                              frameptr, framelen, 3);
	else if (!memcmp(FrameId, "TRSN", 4)) decode_T   ("Internet radio station name",/*New*/                           frameptr, framelen, 3);
	else if (!memcmp(FrameId, "TRSO", 4)) decode_T   ("Internet radio station Owner",/*New*/                          frameptr, framelen, 3);
	else if (!memcmp(FrameId, "TSIZ", 4)) decode_T   ("Size (file in bytes)",                                         frameptr, framelen, 3);
	else if (!memcmp(FrameId, "TSRC", 4)) decode_T   ("International Standard Recording Code (ISO 3901:1986)",        frameptr, framelen, 3);
	else if (!memcmp(FrameId, "TSSE", 4)) decode_T   ("Software/hardware and settings used for encoding",             frameptr, framelen, 3);
	else if (!memcmp(FrameId, "TYER", 4)) decode_T   ("Year (YYYY encoded)",                                          frameptr, framelen, 3);
	else if (!memcmp(FrameId, "TXXX", 4)) decode_TXXX(                                                                frameptr, framelen, 3);
	else if (!memcmp(FrameId, "UFID", 4)) decode_UFID(                                                                frameptr, framelen);
	else if (!memcmp(FrameId, "USER", 4)) decode_USER(                                                                frameptr, framelen, 3);
	else if (!memcmp(FrameId, "USLT", 3)) decode_USLT(                                                                frameptr, framelen, 3);
	else if (!memcmp(FrameId, "WCOM", 4)) decode_W   ("Commercial information",                                       frameptr, framelen);
	else if (!memcmp(FrameId, "WCOP", 4)) decode_W   ("Copyright/Legal information",                                  frameptr, framelen);
	else if (!memcmp(FrameId, "WOAF", 4)) decode_W   ("Official audio file webpage",                                  frameptr, framelen);
	else if (!memcmp(FrameId, "WOAR", 4)) decode_W   ("Official artist/performer webpage",                            frameptr, framelen);
	else if (!memcmp(FrameId, "WOAS", 4)) decode_W   ("Official audio source webpage",                                frameptr, framelen);
	else if (!memcmp(FrameId, "WORS", 4)) decode_W   ("Official internet radio station homepage",/*New*/              frameptr, framelen);
	else if (!memcmp(FrameId, "WPAY", 4)) decode_W   ("Payment",/*New*/                                               frameptr, framelen);
	else if (!memcmp(FrameId, "WPUB", 4)) decode_W   ("Publishers official webpage",                                  frameptr, framelen);
	else if (!memcmp(FrameId, "WXXX", 4)) decode_WXXX(                                                                frameptr, framelen,3);
	else if ((FrameId[0]=='T')&&
		(((FrameId[1]>='0')&&(FrameId[1]<='9'))||((FrameId[1]>='a')&&(FrameId[1]<='z'))||((FrameId[1]>='A')&&(FrameId[1]<='Z')))&&
		(((FrameId[2]>='0')&&(FrameId[2]<='9'))||((FrameId[2]>='a')&&(FrameId[2]<='z'))||((FrameId[2]>='A')&&(FrameId[2]<='Z')))&&
		(((FrameId[3]>='0')&&(FrameId[3]<='9'))||((FrameId[3]>='a')&&(FrameId[3]<='z'))||((FrameId[3]>='A')&&(FrameId[3]<='Z'))))
	{
		decode_T("Unknown Text", frameptr, framelen, 3);
	}
	else if ((FrameId[0]=='W')&&
		(((FrameId[1]>='0')&&(FrameId[1]<='9'))||((FrameId[1]>='a')&&(FrameId[1]<='z'))||((FrameId[1]>='A')&&(FrameId[1]<='Z')))&&
		(((FrameId[2]>='0')&&(FrameId[2]<='9'))||((FrameId[2]>='a')&&(FrameId[2]<='z'))||((FrameId[2]>='A')&&(FrameId[2]<='Z')))&&
		(((FrameId[3]>='0')&&(FrameId[3]<='9'))||((FrameId[3]>='a')&&(FrameId[3]<='z'))||((FrameId[3]>='A')&&(FrameId[3]<='Z'))))
	{
		decode_W("Unknown URL", frameptr, framelen);
	} else {
		print("Unknown frame"); newline();
	}
	flush();
	indent--;
ignoreframe:
	indent--;
	if (zlibptr)
	{
		free (zlibptr);
		zlibptr = 0;
	}
	ptr += inputsize;
	len -= inputsize;

	return (int32_t)(inputsize + 10);
}

static void decode_id3v230(uint8_t *data, uint32_t _length)
{
	uint8_t *ptr;
	uint32_t len;
	uint8_t flags = data[5];
	uint32_t v3_paddinglength=0;

	ptr = data;
	len = _length;

	if (flags & 0x80)
	{
		print(" UNSYNC");
		unsync(ptr, &len);
	}
	if (flags & 0x20)
	{
		print(" EXPERIMENTAL");
	}
	if (flags & 0x1f)
	{
		print(" UNKNOWN_FLAGS(This will probably fail to parse)");
	}
	newline();

	ptr+=10;
	len-=10;

	indent++;
	if (flags & 0x40)
	{
		uint32_t _elength;
		uint8_t *eptr = ptr;

		print(" EXTENDED HEADER");newline();

		_elength = (eptr[0] << 24) |
		           (eptr[1] << 16) |
		           (eptr[2] << 8) |
		           (eptr[3]);

		if ((_elength != 6) && (_elength != 10))
		{
			stack_message(STACK_ERROR, 0, "Extended header length should be 6 or 10");
			indent--;
			return;
		}

		if ((_elength + 4) > len)
		{
			stack_message(STACK_ERROR, 0, "Extended header length too big");
			indent--;
			return;
		}
		if (eptr[4] == 0x00)
		{
			if (_elength != 6)
			{
				stack_message(STACK_WARNING, 0, "CRC disabled and length = 6");
				goto skip_eheader_v230;
			}
			v3_paddinglength = (eptr[6] << 24) |
			                   (eptr[7] << 16) |
			                   (eptr[8] << 8) |
			                    eptr[9] ;
			len -= v3_paddinglength;
			print("PADDING=%" PRId32 " BYTES", v3_paddinglength);
		} else if (eptr[4] == 0x80)
		{
			uint32_t crc;
			if (_elength != 10)
			{
				stack_message(STACK_WARNING, 0, "CRC enabled and length != 10");
				goto skip_eheader_v230;
			}
			v3_paddinglength = (eptr[6] << 24) |
			                   (eptr[7] << 16) |
			                   (eptr[8] << 8) |
			                   (eptr[9]);
			crc = (eptr[10]<<24) |
			      (eptr[11]<<16) |
			      (eptr[12]<<8) |
			       eptr[13];

			print("CRC=0x%08"PRIx32, crc);
		} else {
			stack_message(STACK_WARNING, 0, "FLAGS[0] is out of range");
			goto skip_eheader_v230;
		}
		if (eptr[5] != 0x00)
		{
			stack_message(STACK_WARNING, 0, "FLAGS[0] is out of range");
			goto skip_eheader_v230;
		}
skip_eheader_v230:
		ptr += _elength+4;
		len -= _elength+4;
	}

	while (len > 10)
	{
		int32_t framelen;

		if (ptr[0] == 0)
		{
			stack_message(STACK_WARNING, 0, "Hit zero data, should have been marked as padding");
			break;
		}

		indent++;
		framelen = decode_id3v230_frame (ptr, len);
		indent--;
		if (framelen < 0)
		{
			indent--;
			return;
		}

		ptr += framelen;
		len -= framelen;
	}

	len += v3_paddinglength;
	if (len)
	{
		stack_message(STACK_INFO, 0, "%d bytes of PADDING to inspect", (int)len);
	}
	while (len)
	{
		if (ptr[0])
		{
			stack_message(STACK_WARNING, 0, "Non-zero data in padding: 0x%02x", ptr[0]);
		}
		ptr++;
		len--;
	}
}

static int32_t decode_id3v220_frame(uint8_t *ptr, uint32_t len)
{
	uint8_t FrameId[3];
	uint32_t framelen;

	if (len < 7)
	{
		print("*-*-* ERROR, Minimum frame length is 7 bytes"); newline();
		return -1;
	}

	FrameId[0] = ptr[0];
	FrameId[1] = ptr[1];
	FrameId[2] = ptr[2];

	if ((FrameId[0] < 0x20) || (FrameId[0] >= 0x80) ||
	    (FrameId[1] < 0x20) || (FrameId[1] >= 0x80) ||
	    (FrameId[2] < 0x20) || (FrameId[2] >= 0x80))
	{
		stack_message(STACK_ERROR, 0, "Invalid frame ID %02x %02x %02x", FrameId[0], FrameId[1], FrameId[2]);
		newline();
		return -1;
	}

	framelen = (ptr[3]<<16) | (ptr[4]<<8) | ptr[5];

	if (framelen == 0)
	{
		stack_message(STACK_ERROR, 1, "Frame with zero-length");
		newline();
		return -1;
	}

	ptr += 6;
	len -= 6;

	print("Frame %c%c%c (%5"PRId32" bytes) - ", FrameId[0], FrameId[1], FrameId[2], framelen);
	indent++;

	if (framelen > len)
	{
		stack_message(STACK_ERROR, 0, "We are missing %d of data in this frame", framelen - len);
		newline();
		indent--;
		return -1;
	}
	indent++;

	     if (!memcmp(FrameId, "UFI", 3)) decode_UFID(                                                                ptr, framelen);
	else if (!memcmp(FrameId, "TT1", 3)) decode_T   ("Content group description",                                    ptr, framelen, 2);
	else if (!memcmp(FrameId, "TT2", 3)) decode_T   ("Title/Songname/Content description",                           ptr, framelen, 2);
	else if (!memcmp(FrameId, "TT3", 3)) decode_T   ("Subtitle/Description refinement",                              ptr, framelen, 2);
	else if (!memcmp(FrameId, "TP1", 3)) decode_T   ("Lead artist(s)/Lead performer(s)/Soloist(s)/Performing group", ptr, framelen, 2);
	else if (!memcmp(FrameId, "TP2", 3)) decode_T   ("Band/Orchestra/Accompaniment",                                 ptr, framelen, 2);
	else if (!memcmp(FrameId, "TP3", 3)) decode_T   ("Conductor",                                                    ptr, framelen, 2);
	else if (!memcmp(FrameId, "TP4", 3)) decode_T   ("Interpreted, remixed, or otherwise modified by",               ptr, framelen, 2);
	else if (!memcmp(FrameId, "TCM", 3)) decode_T   ("Composer(s)",                                                  ptr, framelen, 2);
	else if (!memcmp(FrameId, "TXT", 3)) decode_T   ("Lyricist(s)/text writer(s)",                                   ptr, framelen, 2);
	else if (!memcmp(FrameId, "TLA", 3)) decode_T   ("Language(s) (ISO-639-2)",                                      ptr, framelen, 2);
	else if (!memcmp(FrameId, "TCO", 3)) decode_T   ("Content (ID3v1 (n)String encoded)",                            ptr, framelen, 2);
	else if (!memcmp(FrameId, "TAL", 3)) decode_T   ("Album/Movie/Show title",                                       ptr, framelen, 2);
	else if (!memcmp(FrameId, "TPA", 3)) decode_T   ("Part of a set",                                                ptr, framelen, 2);
	else if (!memcmp(FrameId, "TRK", 3)) decode_T   ("Track number/Position in set (#/total encoded)",               ptr, framelen, 2);
	else if (!memcmp(FrameId, "TRC", 3)) decode_T   ("International Standard Recording Code (ISO 3901:1986)",        ptr, framelen, 2);
	else if (!memcmp(FrameId, "TYE", 3)) decode_T   ("Year (YYYY encoded)",                                          ptr, framelen, 2);
	else if (!memcmp(FrameId, "TDA", 3)) decode_T   ("Date (DDMM encoded)",                                          ptr, framelen, 2);
	else if (!memcmp(FrameId, "TIM", 3)) decode_T   ("Time (HHMM encoded)",                                          ptr, framelen, 2);
	else if (!memcmp(FrameId, "TRD", 3)) decode_T   ("Recording dates",                                              ptr, framelen, 2);
	else if (!memcmp(FrameId, "TMT", 3)) decode_T   ("Media type",                                                   ptr, framelen, 2);
	else if (!memcmp(FrameId, "TFT", 3)) decode_T   ("File type",                                                    ptr, framelen, 2);
	else if (!memcmp(FrameId, "TBP", 3)) decode_T   ("BPM",                                                          ptr, framelen, 2);
	else if (!memcmp(FrameId, "TCR", 3)) decode_T   ("Copyright message",                                            ptr, framelen, 2);
	else if (!memcmp(FrameId, "TPB", 3)) decode_T   ("Publisher",                                                    ptr, framelen, 2);
	else if (!memcmp(FrameId, "TEN", 3)) decode_T   ("Encoded by",                                                   ptr, framelen, 2);
	else if (!memcmp(FrameId, "TSS", 3)) decode_T   ("Software/hardware and settings used for encoding",             ptr, framelen, 2);
	else if (!memcmp(FrameId, "TOF", 3)) decode_T   ("Original filename",                                            ptr, framelen, 2);
	else if (!memcmp(FrameId, "TLE", 3)) decode_T   ("Length (in milliseconds)",                                     ptr, framelen, 2);
	else if (!memcmp(FrameId, "TSI", 3)) decode_T   ("Size (file in bytes)",                                         ptr, framelen, 2);
	else if (!memcmp(FrameId, "TDY", 3)) decode_T   ("Playlist delay",                                               ptr, framelen, 2);
	else if (!memcmp(FrameId, "TKE", 3)) decode_T   ("Initial key",                                                  ptr, framelen, 2);
	else if (!memcmp(FrameId, "TOT", 3)) decode_T   ("Original album/Movie/Show title",                              ptr, framelen, 2);
	else if (!memcmp(FrameId, "TOA", 3)) decode_T   ("Original artist(s)/performer(s)",                              ptr, framelen, 2);
	else if (!memcmp(FrameId, "TOL", 3)) decode_T   ("Original Lyricist(s)/text writer(s)",                          ptr, framelen, 2);
	else if (!memcmp(FrameId, "TOR", 3)) decode_T   ("Original release year",                                        ptr, framelen, 2);
	else if (!memcmp(FrameId, "TXX", 3)) decode_TXXX(                                                                ptr, framelen, 2);
	else if (!memcmp(FrameId, "WAF", 3)) decode_W   ("Official audio file webpage",                                  ptr, framelen);
	else if (!memcmp(FrameId, "WAR", 3)) decode_W   ("Official artist/performer webpage",                            ptr, framelen);
	else if (!memcmp(FrameId, "WAS", 3)) decode_W   ("Official audio source webpage",                                ptr, framelen);
	else if (!memcmp(FrameId, "WCM", 3)) decode_W   ("Commercial information",                                       ptr, framelen);
	else if (!memcmp(FrameId, "WCP", 3)) decode_W   ("Copyright/Legal information",                                  ptr, framelen);
	else if (!memcmp(FrameId, "WPB", 3)) decode_W   ("Publishers official webpage",                                  ptr, framelen);
	else if (!memcmp(FrameId, "WXX", 3)) decode_WXXX(                                                                ptr, framelen, 2);
	else if (!memcmp(FrameId, "IPL", 3)) decode_Tn  ("Involved people list",                                         ptr, framelen, 2);
	else if (!memcmp(FrameId, "MCI", 3)) decode_MCDI(                                                                ptr, framelen);
	else if (!memcmp(FrameId, "ETC", 3)) decode_ETCO(                                                                ptr, framelen, 2);
	else if (!memcmp(FrameId, "MLL", 3)) decode_MLLT(                                                                ptr, framelen);
	else if (!memcmp(FrameId, "STC", 3)) decode_SYTC(                                                                ptr, framelen);
	else if (!memcmp(FrameId, "ULT", 3)) decode_USLT(                                                                ptr, framelen, 2);
	else if (!memcmp(FrameId, "SLT", 3)) decode_SYLT(                                                                ptr, framelen, 2);
	else if (!memcmp(FrameId, "COM", 3)) decode_COMM(                                                                ptr, framelen, 2);
	else if (!memcmp(FrameId, "RVA", 3)) decode_RVAD(                                                                ptr, framelen, 2);
	else if (!memcmp(FrameId, "EQU", 3)) decode_EQUA(                                                                ptr, framelen);
	else if (!memcmp(FrameId, "REV", 3)) decode_RVRB(                                                                ptr, framelen);
	else if (!memcmp(FrameId, "PIC", 3)) decode_APIC(                                                                ptr, framelen, 2);
	else if (!memcmp(FrameId, "GEO", 3)) decode_GEOB(                                                                ptr, framelen, 2);
	else if (!memcmp(FrameId, "CNT", 3)) decode_PCNT(                                                                ptr, framelen);
	else if (!memcmp(FrameId, "POP", 3)) decode_POPM(                                                                ptr, framelen);
	else if (!memcmp(FrameId, "BUF", 3)) decode_RBUF(                                                                ptr, framelen);
	else if (!memcmp(FrameId, "CRM", 3)) decode_CRM (                                                                ptr, framelen); /* ID3v2.3.0 changed layout */
	else if (!memcmp(FrameId, "CRA", 3)) decode_AENC(                                                                ptr, framelen);
	else if (!memcmp(FrameId, "LNK", 3)) decode_LINK(                                                                ptr, framelen);
	else if ((ptr[0]=='T')&&
		(((ptr[1]>='0')&&(ptr[1]<='9'))||((ptr[1]>='a')&&(ptr[1]<='z'))||((ptr[1]>='A')&&(ptr[1]<='Z')))&&
		(((ptr[2]>='0')&&(ptr[2]<='9'))||((ptr[2]>='a')&&(ptr[2]<='z'))||((ptr[2]>='A')&&(ptr[1]<='Z'))) )
	{
		decode_T("Unknown Text", ptr+6, framelen, 2);
	}
	else if ((ptr[0]=='W')&&
		(((ptr[1]>='0')&&(ptr[1]<='9'))||((ptr[1]>='a')&&(ptr[1]<='z'))||((ptr[1]>='A')&&(ptr[1]<='Z')))&&
		(((ptr[2]>='0')&&(ptr[2]<='9'))||((ptr[2]>='a')&&(ptr[2]<='z'))||((ptr[2]>='A')&&(ptr[1]<='Z'))))
	{
		decode_W("Unknown URL", ptr+6, framelen);
	} else {
		print("Unknown frame"); newline();
	}
	flush();
	indent-=2;

	return 6 + framelen;
}

static void decode_id3v220(uint8_t *data, uint32_t _length)
{
	uint8_t *ptr;
	uint32_t len;
	uint8_t flags = data[5];

	ptr = data;
	len = _length;

	if (flags & 0x80)
	{
		print(" UNSYNC");
		unsync(ptr, &len);
	}
	if (flags & 0x40)
	{
		print(" COMPRESSION?(This is not defined yet)");
	}
	if (flags & 0x3f)
	{
		print(" UNKNOWN_FLAGS(This will probably fail to parse)");
	}
	newline();

	ptr+=10;
	len-=10;

	while (len > 6)
	{
		int32_t framelen;

		if (ptr[0] == 0)
		{
			break;
		}

		indent++;
		framelen = decode_id3v220_frame (ptr, len);
		indent--;
		if (framelen < 0)
		{
			indent--;
			return;
		}

		ptr += framelen;
		len -= framelen;
	}

	if (len)
	{
		stack_message(STACK_INFO, 0, "(%d bytes of PADDING to inspect)", (int)len);
	}
	while (len)
	{
		if (ptr[0])
		{
			stack_message(STACK_WARNING,0, "Non-zero data in padding: 0x%02x", ptr[0]);
		}
		ptr++;
		len--;
	}
}

static off_t decode_id3v2x(int fd, uint8_t data[10], off_t offset)
{
	unsigned char *ndata;
	uint32_t   length;
	uint32_t  hlength = 10;
	off_t p;

	if (!(((data[3] == 2) && (data[4]==0)) ||
	      ((data[3] == 3) && (data[4]==0)) ||
	      ((data[3] == 4) && (data[4]==0)) ) )
	{ /* False positive or unknown version */
		return 0;
	}

	if (data[5] & 0x0f)
	{
		/* The lower nibble will never be set - also why FLAGS byte is never unsyncronized */
		return 0;
	}

	if ((data[6] & 0x80) ||
	    (data[7] & 0x80) ||
	    (data[8] & 0x80) ||
	    (data[9] & 0x80))
	{
		/* size is stored as 'syncsafe integer'. MSB will never be set */
		return 0;
	}

	length = (((unsigned char *)data)[6]<<21) |
	         (((unsigned char *)data)[7]<<14) |
	         (((unsigned char *)data)[8]<<7) |
	         (((unsigned char *)data)[9]);

	if (length < (10 + ((data[5] & 0x40)?6:0)))
	{
		/* must fit atleast the header */
		return 0;
	}

	print("0x%08lx ID3v2.%d.%d %ld bytes", (long) offset, ((unsigned char *)data)[3], ((unsigned char *)data)[4], (long)length);

	ndata = malloc (length + hlength);
	p = lseek(fd, 0, SEEK_CUR);
	lseek(fd, offset, SEEK_SET);
	{
		int result = read (fd, ndata, (length+hlength));
		if (result != (length + hlength))
		{
			stack_message(STACK_WARNING, 0, "Unable to read entire tag from disk");
			if (result < 0) result = 0;
			memset (ndata + result, 0, (length+hlength)-result);
		}

		/* Different versions have different rules for extended header, and sync-safe integers */
		if (data[3] == 2)
		{
			indent++;
			decode_id3v220(ndata, length+hlength);
			indent--;
		} else if(data[3] == 3)
		{
			indent++;
			decode_id3v230(ndata, length+hlength);
			indent--;
		} else if (data[3] == 4)
		{
			indent++;
			decode_id3v240(ndata, length+hlength, fd); /* FD, to probe for footer */
			indent--;
		}
		flush();
	}

	free (ndata);
	lseek (fd, p, SEEK_SET);
	return length + hlength;
}

static void decode_id3v12(uint8_t data[128], off_t offset)
{
	uint8_t  *d;
	uint32_t l;

	print("%08lx ID3v1.2 (extends more text onto ID3v1.0/ID3v1.1 128 bytes", (long) offset); newline();
	indent++;
	print("..SongName: "); d=data+  3; l=30; decode_print_iso8859_1(&d, &l, 0); newline();
	print("..Artist:   "); d=data+ 33; l=30; decode_print_iso8859_1(&d, &l, 0); newline();
	print("..Album:    "); d=data+ 63; l=30; decode_print_iso8859_1(&d, &l, 0); newline();
	print("..Comment:  "); d=data+ 93; l=15; decode_print_iso8859_1(&d, &l, 0); newline();
	print("SubGenre:   "); d=data+108; l=20; decode_print_iso8859_1(&d, &l, 0); newline();
	indent--;
}

static void decode_id3v1x(uint8_t data[128], off_t offset)
{
	uint8_t  *d;
	uint32_t l;
	const char *genre[] =
	{
		"Blues",
		"Classic Rock",
		"Country",
		"Dance",
		"Disco",
		"Funk",
		"Grunge",
		"Hip-Hop",
		"Jazz",
		"Metal",
		"New Age",
		"Oldies",
		"Other",
		"Pop",
		"R&B",
		"Rap",
		"Reggae",
		"Rock",
		"Techno",
		"Industrial",
		"Alternative",
		"Ska",
		"Death Metal",
		"Pranks",
		"Soundtrack",
		"Euro-Techno",
		"Ambient",
		"Trip-Hop",
		"Vocal",
		"Jazz+Funk",
		"Fusion",
		"Trance",
		"Classical",
		"Instrumental",
		"Acid",
		"House",
		"Game",
		"Sound Clip",
		"Gospel",
		"Noise",
		"AlternRock",
		"Bass",
		"Soul",
		"Punk",
		"Space",
		"Meditative",
		"Instrumental Pop",
		"Instrumental Rock",
		"Ethnic",
		"Gothic",
		"Darkwave",
		"Techno-Industrial",
		"Electronic",
		"Pop-Folk",
		"Eurodance",
		"Dream",
		"Southern Rock",
		"Comedy",
		"Cult",
		"Gangsta",
		"Top 40",
		"Christian Rap",
		"Pop/Funk",
		"Jungle",
		"Native American",
		"Cabaret",
		"New Wave",
		"Psychadelic",
		"Rave",
		"Showtunes",
		"Trailer",
		"Lo-Fi",
		"Tribal",
		"Acid Punk",
		"Acid Jazz",
		"Polka",
		"Retro",
		"Musical",
		"Rock & Roll",
		"Hard Rock",
		"Folk (Winamp extension)",
		"Folk-Rock (Winamp extension)",
		"National Folk (Winamp extension)",
		"Swing (Winamp extension)",
		"Fast Fusion (Winamp extension)",
		"Bebob (Winamp extension)",
		"Latin (Winamp extension)",
		"Revival (Winamp extension)",
		"Celtic (Winamp extension)",
		"Bluegrass (Winamp extension)",
		"Avantgarde (Winamp extension)",
		"Gothic Rock (Winamp extension)",
		"Progressive Rock (Winamp extension)",
		"Psychedelic Rock (Winamp extension)",
		"Symphonic Rock (Winamp extension)",
		"Slow Rock (Winamp extension)",
		"Big Band (Winamp extension)",
		"Chorus (Winamp extension)",
		"Easy Listening (Winamp extension)",
		"Acoustic (Winamp extension)",
		"Humour (Winamp extension)",
		"Speech (Winamp extension)",
		"Chanson (Winamp extension)",
		"Opera (Winamp extension)",
		"Chamber Music (Winamp extension)",
		"Sonata (Winamp extension)",
		"Symphony (Winamp extension)",
		"Booty Bass (Winamp extension)",
		"Primus (Winamp extension)",
		"Porn Groove (Winamp extension)",
		"Satire (Winamp extension)",
		"Slow Jam (Winamp extension)",
		"Club (Winamp extension)",
		"Tango (Winamp extension)",
		"Samba (Winamp extension)",
		"Folklore (Winamp extension)",
		"Ballad (Winamp extension)",
		"Power Ballad (Winamp extension)",
		"Rhythmic Soul (Winamp extension)",
		"Freestyle (Winamp extension)",
		"Duet (Winamp extension)",
		"Punk Rock (Winamp extension)",
		"Drum Solo (Winamp extension)",
		"A capella (Winamp extension)",
		"Euro-House (Winamp extension)",
		"Dance Hall (Winamp extension"
	};

	if ((!data[125]) && data[126])
	{
		print("%08lx ID3v1.1 128 bytes", (long) offset); newline();
	} else {
		print("%08lx ID3v1 128 bytes", (long) offset); newline();
	}
	indent++;

	print("SongName:     "); d=data+  3; l=30; decode_print_iso8859_1(&d, &l, 0); newline();
	print("Artist:       "); d=data+ 33; l=30; decode_print_iso8859_1(&d, &l, 0); newline();
	print("Album:        "); d=data+ 63; l=30; decode_print_iso8859_1(&d, &l, 0); newline();
	print("Year:         "); d=data+ 93; l= 4; decode_print_iso8859_1(&d, &l, 0); newline();

	if ((!data[125]) && data[126])
	{
		print("Comment:      "); d=data+ 97; l=27; decode_print_iso8859_1(&d, &l, 0); newline();
		print("Track number: %d", data[126]); newline();
	} else {

		print("Comment:      "); d=data+ 97; l=30; decode_print_iso8859_1(&d, &l, 0); newline();
	}
	print("Genre:        %d \"%s\"", data[127], (data[127]>=(sizeof(genre)/sizeof(genre[0])))?"Unknown":genre[data[127]]); newline();

	indent--;
}

static void decode_etag(uint8_t data[227], off_t offset)
{
	uint8_t  *d;
	uint32_t l;

	print("%08lx ETAG 227 bytes", (long) offset);newline();
	indent++;

	print("Title:      "); d=data+  4; l=60; decode_print_iso8859_1(&d, &l, 0); newline();
	print("Artist:     "); d=data+ 64; l=60; decode_print_iso8859_1(&d, &l, 0); newline();
	print("Album:      "); d=data+124; l=60; decode_print_iso8859_1(&d, &l, 0); newline();
	switch (data[184])
	{
		case 0x00: print("Speed:      (unset)"); newline(); break;
		case 0x01: print("Speed:      slow"); newline();  break;
		case 0x02: print("Speed:      medium"); newline(); break;
		case 0x03: print("Speed:      fast"); newline(); break;
		case 0x04: print("Speed:      hardcore"); newline(); break;
		default:   print("Speed:      (%d) Unknown", data[184]); newline(); break;
	}
	print("Genre:      "); d=data+185; l=30; decode_print_iso8859_1(&d, &l, 0); newline();
	print("Start-Time: "); d=data+215; l=6; decode_print_iso8859_1(&d, &l, 0); newline();
	print("Stop-Time:  "); d=data+221; l=6; decode_print_iso8859_1(&d, &l, 0); newline();

	indent--;
}

static void scanfile(const char *filename)
{
	int fd;
	uint8_t prehead[65536];
	int fill;
	int need_sync = 1;

	printf ("FILE: %s\n", filename);
	fd = open (filename, O_RDONLY);
	if (fd < 0)
	{
		fprintf (stderr, "Failed to open %s\n", filename);
		return;
	}

	if (read (fd, prehead, 3) != 3)
	{
		fprintf (stderr, "Failed to read offset 0, count 3\n");
		close (fd);
		return;
	}

	if (!memcmp (prehead, "TAG", 3))
	{
		if (read (fd, prehead + 3, 128-3) != (128-3))
		{
			fprintf (stderr, "Failed to read offset 3, count 125\n");
			close (fd);
			return;
		}
		fprintf (stderr, "NON STANDARD ID3v1 tag at the start of the file!!!\n");
		decode_id3v1x (prehead, 0);
	}

	if (lseek (fd, -128, SEEK_END) != (off_t)-1)
	{
		read (fd, prehead, 3);
		if (!memcmp (prehead, "TAG", 3))
		{
			if (read (fd, prehead + 3, 128-3) != (128-3))
			{
				fprintf (stderr, "Failed to read offset -125, count 125\n");
				close (fd);
				return;
			}
			decode_id3v1x (prehead, lseek(fd, 0, SEEK_CUR)-128);

			lseek (fd, -256, SEEK_END);
			read (fd, prehead, 3);
			if (!memcmp (prehead, "EXT", 3))
			{
				if (read (fd, prehead + 3, 125) != (125))
				{
					fprintf (stderr, "Failed to read offset -351, count 223\n");
					close (fd);
					return;
				}
				decode_id3v12 (prehead, lseek(fd, 0, SEEK_CUR)-256);
			}

			lseek (fd, -355, SEEK_END);
			read (fd, prehead, 4);
			if (!memcmp (prehead, "TAG+", 4))
			{
				if (read (fd, prehead + 4, 227-4) != (227-4))
				{
					fprintf (stderr, "Failed to read offset -351, count 223\n");
					close (fd);
					return;
				}
				decode_etag (prehead, lseek(fd, 0, SEEK_CUR)-355);
			}
		}
	}
	lseek (fd, 0, SEEK_SET);
	fill=0;
	while (1)
	{
read_more:
		fill += read (fd, prehead + fill, sizeof(prehead) - fill);
		if (fill < 10)
		{
			break;
		}
		while (fill >= 10)
		{
			uint8_t *loc;

			if (need_sync)
			{
				if (memcmp(prehead, "ID3", 3))
				{
					if (prehead[0] != 0xff)
					{
						printf ("offset=0x%08lx not sync yet: %02x\n", lseek(fd, 0, SEEK_CUR)-fill, prehead[0]);
						fill -= 1;
						memmove (prehead, prehead+1, fill);
						continue;
					} else {
						need_sync = 0;
					}
				}
			}


			if ((loc = memmem (prehead, fill - 6, "ID3", 3)))
			{
				off_t decode_length;
				//printf ("fill=%d offset=0x%08lx loc-prehead=%d realoffset=0x%08lx\n", fill, (long int) lseek(fd, 0, SEEK_CUR), loc-prehead, lseek(fd, 0, SEEK_CUR)-fill+(loc-prehead));
				decode_length = decode_id3v2x(fd, loc, lseek(fd, 0, SEEK_CUR)-fill+(loc-prehead));
				if (decode_length)
				{
					lseek (fd, - fill + (loc-prehead) + decode_length, SEEK_CUR);
					fill = 0;
					need_sync = 1;
					goto read_more;
				} else {
					fill -= (loc-prehead)+10;
					memmove (prehead, loc+10, fill);
				}
			} else {
				break;
			}
		}
		memmove (prehead, prehead + fill - 9, 9);
		fill = 9;
	}

	close (fd);
	fflush(stdout);
}

int main(int argc, char *argv[])
{
	int i;

	if (argc < 2)
	{
		fprintf (stderr, "Usage: %s file\n", argv[0]);
		return -1;
	}
	if (!strcmp (argv[1], "-2"))
	{
		uint8_t buffer[65536];
		uint32_t fill;
		int32_t result;
		int fd;
		for (i=2; i < argc; i++)
		{
			fd = open (argv[i], O_RDONLY);
			if (fd < 0)
			{
				continue;
			}
			fill = read (fd, buffer, sizeof (buffer));
			close (fd);
			print("ID3v2.2.0 frame file: %s", argv[i]);newline();
			result = decode_id3v220_frame (buffer, fill);
			if (result < 0)
			{
			} else if (result != fill)
			{
				print("(EXTRA DATA AFTER FRAME: %d bytes", fill - result); newline();
			}
			flush();
		}
	} else if (!strcmp (argv[1], "-3"))
	{
		uint8_t buffer[65536];
		uint32_t fill;
		int32_t result;
		int fd;
		for (i=2; i < argc; i++)
		{
			fd = open (argv[i], O_RDONLY);
			if (fd < 0)
			{
				continue;
			}
			fill = read (fd, buffer, sizeof (buffer));
			close (fd);
			print("ID3v2.3.0 frame file: %s", argv[i]);newline();
			result = decode_id3v230_frame (buffer, fill);
			if (result < 0)
			{
			} else if (result != fill)
			{
				print("(EXTRA DATA AFTER FRAME: %d bytes", fill - result); newline();
			}
			flush();
		}
	} else if (!strcmp (argv[1], "-4"))
	{
		uint8_t buffer[65536];
		uint32_t fill;
		int32_t result;
		int fd;
		for (i=2; i < argc; i++)
		{
			fd = open (argv[i], O_RDONLY);
			if (fd < 0)
			{
				continue;
			}
			fill = read (fd, buffer, sizeof (buffer));
			close (fd);
			print("ID3v2.4.0 frame file: %s", argv[i]);newline();
			result = decode_id3v240_frame (buffer, fill);
			if (result < 0)
			{
			} else if (result != fill)
			{
				print("(EXTRA DATA AFTER FRAME: %d bytes", fill - result); newline();
			}
			flush();
		}
	} else {
		for (i=1; i < argc; i++)
		{
			scanfile(argv[i]);
			flush();
		}
	}
	return 0;
}
