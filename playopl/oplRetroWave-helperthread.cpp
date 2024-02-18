/* OpenCP Module Player
 * copyright (c) 2005-'24 Stian Skjelstad <stian.skjelstad@gmail.com>
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

#define SLOTS 8192

#if defined (__linux)
#define RETROWAVE_PLAYER_TIME_REF       CLOCK_MONOTONIC_RAW
#else
#define RETROWAVE_PLAYER_TIME_REF       CLOCK_MONOTONIC
#endif

enum oplRetroWaveCommands
{
	CMD_RESET = 1,
	CMD_WRITE = 2,
	CMD_SLEEP = 3,
	CMD_CLOSE = 4,
};

struct oplRetroWaveCommandWrite
{
	uint8_t chip;
	uint8_t reg;
	uint8_t value;
};

struct oplRetroWaveCommandSleep
{
	uint32_t us;
};

struct oplRetroWaveCommand
{
	enum oplRetroWaveCommands Action;
	union
	{
		struct oplRetroWaveCommandWrite Write;
		struct oplRetroWaveCommandSleep Sleep;
	};
};

static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
static pthread_t t;
static int UseCount;

static struct timespec nexttick;

static struct oplRetroWaveCommand Commands[SLOTS];
static int CommandTail = 0;
static int CommandHead = 0;

static void *oplRetroWave_ThreadHelper (void *context)
{
#ifdef __linux
	pthread_setname_np (t, "RetroWave OPL3");
#endif

	clock_gettime(RETROWAVE_PLAYER_TIME_REF, &nexttick);

	if (pthread_mutex_lock (&m))
	{
		fprintf (stderr, "[Adplug OPL, RetroWave OPL3] pthread_mutex_lock() failed #1\n");
		_exit(0);
	}

	while (1)
	{
		if (CommandTail == CommandHead) // queue is empty.....
		{
			pthread_mutex_unlock (&m);
			flush ();
			usleep (1000); // 1ms
			pthread_mutex_lock (&m);
			continue;
		}

		switch (Commands[CommandTail].Action)
		{
			case CMD_RESET:
				reset ();
				CommandTail = (CommandTail + 1) & (SLOTS - 1);

				pthread_mutex_unlock (&m);
				flush ();
				pthread_mutex_lock (&m);
				break;

			case CMD_WRITE:
				if (Commands[CommandTail].Write.chip == 0)
				{
					queue_port0(Commands[CommandTail].Write.reg, Commands[CommandTail].Write.value);
				} else if (Commands[CommandTail].Write.chip == 0)
				{
					queue_port1(Commands[CommandTail].Write.reg, Commands[CommandTail].Write.value);
				}
				CommandTail = (CommandTail + 1) & (SLOTS - 1);
				break;

			case CMD_SLEEP:
				{
					int tosleep = Commands[CommandTail].Sleep.us;
					if (tosleep > 10000)
					{
						tosleep = 10000;
						Commands[CommandTail].Sleep.us -= tosleep;
					} else {
						CommandTail = (CommandTail + 1) & (SLOTS - 1);
					}

					pthread_mutex_unlock (&m);
					flush ();
					pthread_mutex_lock (&m);

					struct timespec now;
					clock_gettime(RETROWAVE_PLAYER_TIME_REF, &now);
					nexttick.tv_nsec += tosleep * 1000;
					while (nexttick.tv_nsec > 1000000000l)
					{
						nexttick.tv_sec++;
						nexttick.tv_nsec-= 1000000000l;
					}

					if (nexttick.tv_sec < now.tv_sec)
					{
						break;
					}
					if ((nexttick.tv_sec == now.tv_sec) &&
					    (nexttick.tv_nsec < now.tv_nsec))
					{
						break;
					}

					pthread_mutex_unlock (&m);
					usleep( (nexttick.tv_sec  - now.tv_sec ) * 1000000 +
					        (nexttick.tv_nsec - now.tv_nsec) / 1000);
					pthread_mutex_lock (&m);
				}
				break;

			case CMD_CLOSE: /* imply a RESET */
				reset();
				CommandTail = (CommandTail + 1) & (SLOTS - 1);

				pthread_mutex_unlock (&m);
				flush ();
				pthread_mutex_lock (&m);
				goto quit;

			default:
				write (2, "[Adplug OPL, RetroWave OPL3] Invalid command in RetroWave Queue\n", 64);
				goto quit;
		}
	}

quit:
#ifdef _WIN32
	CloseHandle(fd);
	fd = INVALID_HANDLE_VALUE;
#else
	close (fd);
	fd = -1;
#endif
	pthread_mutex_unlock (&m);
	return NULL;
}

static int oplRetroWave_Open (void(*cpiDebug)(struct cpifaceSessionAPI_t *cpifaceSession, const char *fmt, ...), struct cpifaceSessionAPI_t *cpifaceSession, const char *filename)
{
#ifdef _WIN32
	pthread_mutex_lock (&m);
	while (fd != INVALID_HANDLE_VALUE) /* should not happen ATM */
	{
		pthread_mutex_unlock (&m);
		usleep (1000); // 1ms
		pthread_mutex_lock (&m);
		return -1;
	}

	fd = CreateFile (filename,                     /* lpFileName */
	                 GENERIC_READ | GENERIC_WRITE, /* dwDesiredAccess */
	                 0,                            /* dwShareMode (exclusive access) */
	                 0,                            /* lpSecurityAttributes */
	                 OPEN_EXISTING,                /* dwCreationDisposition */
	                 FILE_ATTRIBUTE_NORMAL,        /* dwFlagsAndAttributes */
	                 0);                           /* hTemplateFile */
	if (fd == INVALID_HANDLE_VALUE)
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
			cpiDebug (cpifaceSession, "[Adplug OPL, RetroWave OPL3] CreateFile(%s): %s\n", filename, lpMsgBuf);
			LocalFree (lpMsgBuf);
		}
		pthread_mutex_unlock (&m);
		return -1;
	}
#else
	struct termios tio;
# if 0 /* We do not need to set baud-rate, since the serial-port is not real */
#  ifdef __APPLE__
	int speed = 2000000;
#  endif
# endif

	pthread_mutex_lock (&m);
	while (fd >= 0) /* should not happen ATM */
	{
		pthread_mutex_unlock (&m);
		usleep (1000); // 1ms
		pthread_mutex_lock (&m);
		return -1;
	}

	fd = open(filename, O_RDWR);

	if (fd < 0)
	{
		cpiDebug (cpifaceSession, "[Adplug OPL, RetroWave OPL3] Failed to open tty/serial device %s: %s\n", filename, strerror(errno));
		pthread_mutex_unlock (&m);
		return -1;
	}

	if (flock (fd, LOCK_EX))
	{
		cpiDebug (cpifaceSession, "[Adplug OPL, RetroWave OPL3] Failed to lock tty/serial device: %s: %s\n", filename, strerror(errno));
		goto error_out;
	}

	if (tcgetattr (fd, &tio))
	{
		cpiDebug (cpifaceSession, "[Adplug OPL, RetroWave OPL3] Failed to perform tcgetattr() on device %s, not a tty/serial device?: %s\n", filename, strerror (errno));
		goto error_out;
	}
	cfmakeraw (&tio);

# if 0 /* We do not need to set baud-rate, since the serial-port is not real */
#  ifndef __APPLE__
	cfsetispeed (&tio, 2000000);
	cfsetospeed (&tio, 2000000);
#  endif
# endif
	if (tcgetattr (fd, &tio))
	{
		cpiDebug (cpifaceSession, "[Adplug OPL, RetroWave OPL3] Failed to perform tcsetattr() on device %s, not a tty/serial device?: %s\n", filename, strerror(errno));
		goto error_out;
	}

# if 0 /* We do not need to set baud-rate, since the serial-port is not real */
#  ifdef __APPLE__
	if (ioctl (fd, IOSSIOSPEED, &speed) == -1)
	{
		cpiDebug (cpifaceSession, "[Adplug OPL, RetroWave OPL3] Failed to set baudrate on device %s: %s", filename, strerror(errno));
		goto error_out;
	}
#  endif
# endif
#endif

	/* sync communication */
	cmd_buffer[cmd_buffer_used++] = 0x00;
	flush ();

	/* configure GPIO bridge ICs */
	for (uint8_t i=0x20; i<0x28; i++)
	{
		cmd_prepare ((uint8_t)(i<<1), 0x0a, 1); // IOCON register
		cmd_buffer[cmd_buffer_used++] = 0x28;  // Enable: HAEN, SEQOP
		flush();

		cmd_prepare ((uint8_t)(i<<1), 0x00, 2); // IODIRA register
		cmd_buffer[cmd_buffer_used++] = 0x00;  // Set output
		cmd_buffer[cmd_buffer_used++] = 0x00;  // Set output
		flush();

		cmd_prepare ((uint8_t)(i<<1), 0x12, 2); // GPIOA register
		cmd_buffer[cmd_buffer_used++] = 0xff;  // Set all HIGH
		cmd_buffer[cmd_buffer_used++] = 0xff;  // Set all HIGH
		cmd_buffer_used = 4;
		flush();
	}

	// Create an initial sleep, so that we have time to add something to the queue
#warning we should delay creating the tread until we queue a sleep or queue is full instead of this initial delay..
	Commands[CommandHead].Action = CMD_SLEEP;
	Commands[CommandHead].Sleep.us = 1000;
	CommandHead = (CommandHead + 1) & (SLOTS - 1);

	UseCount++;

	if (pthread_create (&t, NULL, oplRetroWave_ThreadHelper, NULL))
	{
		cpiDebug (cpifaceSession, "[Adplug OPL, RetroWave OPL3] Failed to spawn thread: %s\n", strerror(errno));
		goto error_out;
	}

	pthread_mutex_unlock (&m);

	cpiDebug (cpifaceSession, "[Adplug OPL, RetroWave OPL3] Successfull opened tty/serial device %s\n", filename);

	return 0;

error_out:
#ifdef _WIN32
	CloseHandle (fd);
	fd = INVALID_HANDLE_VALUE;
	pthread_mutex_unlock (&m);
#else
	close (fd);
	fd = -1;
	pthread_mutex_unlock (&m);
#endif
	return -1;
}

static void oplRetroWave_EnsureQueue(void)
{
#ifdef _WIN32
	if (fd == INVALID_HANDLE_VALUE)
#else
	if (fd < 0)
#endif
	{
		fprintf (stderr, "[Adplug OPL, RetroWave OPL3] warning fd < 0\n");
		return;
	}
	while (((CommandHead + 1) & (SLOTS - 1)) == CommandTail)
	{
		pthread_mutex_unlock (&m);
		usleep (1000); // 1ms
		pthread_mutex_lock (&m);
	}
}

static void oplRetroWave_Sleep(uint32_t value)
{
	pthread_mutex_lock (&m);
	oplRetroWave_EnsureQueue ();
	Commands[CommandHead].Action = CMD_SLEEP;
	Commands[CommandHead].Sleep.us = value;
	CommandHead = (CommandHead + 1) & (SLOTS - 1);
	pthread_mutex_unlock (&m);
}

static void oplRetroWave_Reset()
{
	pthread_mutex_lock (&m);
	oplRetroWave_EnsureQueue ();
	Commands[CommandHead].Action = CMD_RESET;
	CommandHead = (CommandHead + 1) & (SLOTS - 1);
	pthread_mutex_unlock (&m);
}

static void oplRetroWave_Write(uint8_t chip, uint8_t reg, uint8_t value)
{
	pthread_mutex_lock (&m);
	oplRetroWave_EnsureQueue ();
	Commands[CommandHead].Action = CMD_WRITE;
	Commands[CommandHead].Write.chip = chip;
	Commands[CommandHead].Write.reg = reg;
	Commands[CommandHead].Write.value = value;
	CommandHead = (CommandHead + 1) & (SLOTS - 1);
	pthread_mutex_unlock (&m);
}

static void oplRetroWave_Close(void)
{
	void *retval;
	pthread_mutex_lock (&m);

#ifdef _WIN32
	if (fd != INVALID_HANDLE_VALUE)
#else
	if (fd >= 0)
#endif
	{
		oplRetroWave_EnsureQueue();
		Commands[CommandHead].Action = CMD_CLOSE;
		CommandHead = (CommandHead + 1) & (SLOTS - 1);
		pthread_mutex_unlock (&m);
		usleep (1000); // 1ms
		pthread_mutex_lock (&m);
	}

#ifdef _WIN32
	while (fd != INVALID_HANDLE_VALUE)
#else
	while (fd >= 0)
#endif
	{
		pthread_mutex_unlock (&m);
		usleep (1000); // 1ms
		pthread_mutex_lock (&m);
	}

	if (UseCount)
	{
		pthread_join (t, &retval);
		UseCount--;
	}

	CommandTail = 0;
	CommandHead = 0;

	pthread_mutex_unlock (&m);
}
