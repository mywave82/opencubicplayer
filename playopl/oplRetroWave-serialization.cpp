/* OpenCP Module Player
 * copyright (c) 2005-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
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

typedef enum {
	RetroWave_Board_Unknown = 0,
	RetroWave_Board_OPL3 = 0x21 << 1,
	RetroWave_Board_MiniBlaster = 0x20 << 1,
	RetroWave_Board_MasterGear = 0x24 << 1
} RetroWaveBoardType;

#define MIN(a,b) ((a)<(b)?(a):(b))
#define BUFFERSIZE 8192
static uint8_t cmd_buffer[BUFFERSIZE];
static uint_fast16_t cmd_buffer_used = 0;
static uint8_t io_buffer[BUFFERSIZE * 8 / 7 + 3];
static uint_fast16_t io_buffer_used;

static void flush(void)
{
	uint_fast16_t data = 0;
	uint8_t fill = 0;
	io_buffer_used = 0;

	io_buffer[io_buffer_used++] = 0x00;

	if (!cmd_buffer_used)
	{
		return;
	}

	for (uint_fast16_t i=0; i < cmd_buffer_used;)
	{
		if (fill < 7)
		{
			data <<= 8;
			data |= cmd_buffer[i++];
			fill += 8;
		}
		io_buffer[io_buffer_used++] = ((data >> (fill - 7)) << 1) | 0x01;
		fill -= 7;
	}
	if (fill)
	{
		io_buffer[io_buffer_used++] = 0x01 | (data << 1);
	}

	io_buffer[io_buffer_used++] = 0x02;

	cmd_buffer_used = 0;

#ifdef _WIN32
	DWORD NumberOfBytesWritten = 0;
	BOOL Result;
	Result = WriteFile (fd, io_buffer, io_buffer_used, &NumberOfBytesWritten, 0);
	if ((!Result) || (NumberOfBytesWritten != io_buffer_used))
	{
		char *lpMsgBuf = NULL;
		if (FormatMessage (
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM     |
			FORMAT_MESSAGE_IGNORE_INSERTS,             /* dwFlags */
			NULL,                                      /* lpSource */
			GetLastError(),                            /* dwMessageId */
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), /* dwLanguageId */
			(LPSTR) &lpMsgBuf,                         /* lpBuffer */
			0,                                         /* nSize */
			NULL                                       /* Arguments */
		))
		{
			fprintf(stderr, "[RetroWave] WriteFile(): %s\n", lpMsgBuf);
			LocalFree (lpMsgBuf);
		}
	}
#elif defined(__APPLE__)
	int pos = 0;
	while (pos < io_buffer_used)
	{
		int target = MIN (io_buffer_used - pos, 128);
		int res;
		if ((res = write(fd, io_buffer + pos, target)) != (ssize_t)target)
		{
			fprintf (stderr, "warning, write %d of %d bytes", res, target);
		}
		pos += target;
		usleep (250);
	}
#else
	int res;
	if ((res = write(fd, io_buffer, io_buffer_used)) != (ssize_t)io_buffer_used)
	{
		fprintf (stderr, "warning, write %d of %d bytes", res, (int)io_buffer_used);
	}
#endif
	io_buffer_used = 0;
}

static void cmd_prepare(uint8_t io_addr, uint8_t io_reg, const int len)
{
	if ((cmd_buffer_used > (uint_fast16_t)(BUFFERSIZE-len))||
	    (cmd_buffer_used && (cmd_buffer[0] != io_addr)) ||
	    (cmd_buffer_used && (cmd_buffer[1] != io_reg)) )
	{
		fprintf (stderr, "warning, buffer ran out, doing a flush\n");
		flush();
	}

	if (!cmd_buffer_used)
	{
		cmd_buffer[cmd_buffer_used++] = io_addr;
		cmd_buffer[cmd_buffer_used++] = io_reg;
	}
}

static void queue_port0(uint8_t reg, uint8_t val)
{
	cmd_prepare(RetroWave_Board_OPL3, 0x12, 6);
	cmd_buffer[cmd_buffer_used++] = 0xe1;
	cmd_buffer[cmd_buffer_used++] = reg;
	cmd_buffer[cmd_buffer_used++] = 0xe3;
	cmd_buffer[cmd_buffer_used++] = val;
	cmd_buffer[cmd_buffer_used++] = 0xfb;
	cmd_buffer[cmd_buffer_used++] = val;
}

static void queue_port1(uint8_t reg, uint8_t val)
{
	cmd_prepare(RetroWave_Board_OPL3, 0x12, 6);
	cmd_buffer[cmd_buffer_used++] = 0xe5;
	cmd_buffer[cmd_buffer_used++] = reg;
	cmd_buffer[cmd_buffer_used++] = 0xe7;
	cmd_buffer[cmd_buffer_used++] = val;
	cmd_buffer[cmd_buffer_used++] = 0xfb;
	cmd_buffer[cmd_buffer_used++] = val;
}

static void reset(void)
{
	if (cmd_buffer_used)
	{
		flush();
	}

#if 0 // reset by /CI, makes click sound
	cmd_prepare(RetroWave_Board_OPL3, 0x12, 1);
	cmd_buffer[cmd_buffer_used++] = 0xfe;
	cmd_buffer[cmd_buffer_used++] = 0x00;
	flush();

	usleep (1700); /* > 1.6ms */

	cmd_prepare(RetroWave_Board_OPL3, 0x12, 1);
	cmd_buffer[cmd_buffer_used++] = 0xff;
	cmd_buffer[cmd_buffer_used++] = 0x00;
	flush();
#else // reset by registers
	queue_port1 (5, 1); // Enable OPL3 mode
	queue_port1 (4, 0); // Disable all 4-OP connections

	for (int i=0x20; i <= 0x35; i++)
	{
		queue_port0 (i, 0x01);
		queue_port1 (i, 0x01);
	}
	for (int i=0x40; i <= 0x55; i++)
	{
		queue_port0 (i, 0x3f);
		queue_port1 (i, 0x3f);
	}
	for (int i=0x60; i <= 0x75; i++)
	{
		queue_port0 (i, 0xee);
		queue_port1 (i, 0xee);
	}
	for (int i=0x80; i <= 0x95; i++)
	{
		queue_port0 (i, 0x0e);
		queue_port1 (i, 0x0e);
	}
	for (int i=0xa0; i <= 0xa8; i++)
	{
		queue_port0 (i, 0x80);
		queue_port1 (i, 0x80);
	}
	for (int i=0xb0; i <= 0xb8; i++)
	{
		queue_port0 (i, 0x04);
		queue_port1 (i, 0x04);
	}
	for (int i=0xbd; i <= 0xbd; i++)
	{
		queue_port0 (i, 0);
		queue_port1 (i, 0);
	}
	for (int i=0xc0; i <= 0xc8; i++)
	{
		queue_port0 (i, 0x30); // Enable Left and Right
		queue_port1 (i, 0x30); // Enable Left and Right
	}
	for (int i=0xe0; i <= 0xf5; i++)
	{
		queue_port0 (i, 0);
		queue_port1 (i, 0);
	}
	for (int i=0x08; i <= 0x08; i++)
	{
		queue_port0 (i, 0);
		queue_port1 (i, 0);
	}
	for (int i=0x01; i <= 0x01; i++)
	{
		queue_port0 (i, 0);
		queue_port1 (i, 0);
	}
	queue_port1 (5, 0); // OPL2 mode
	flush();
#endif
}

