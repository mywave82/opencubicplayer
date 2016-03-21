/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * Routines for parsing escaped keycodes mapping and extending ncurses
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
 *  -ss040613   Stian Skjelstad <stian@nixia.no>
 *    -first release
 */

#define _CONSOLE_DRIVER 1
#include "config.h"
#ifdef KEYBOARD_DEBUG
#include <stdio.h>
#endif
#include "types.h"
#include "poutput.h"

/* #define KEYB_DEBUG */

#define BUFFER_LEN 128
static uint16_t ring_buffer[BUFFER_LEN];
static int ring_head, ring_tail;

static int(*kbhit)(void);
static int(*getnextchar)(void);
static int ___valid_key(uint16_t key);

void ___setup_key(int(*_kbhit)(void), int(*_getch)(void))
{
	ring_head=0;
	ring_tail=0;
	kbhit=_kbhit;
	getnextchar=_getch;
	_ekbhit=___peek_key;
	_egetch=___pop_key;
	_validkey=___valid_key;
}

void ___push_key(uint16_t key)
{
#ifdef KEYBOARD_DEBUG
	fprintf(stderr, "___push_key %d/%c\n", key, (unsigned char)key);
#endif
	if (!key)
		return;
	if (((ring_head+1)%BUFFER_LEN)==ring_tail)
		return; /* buffer full */
	ring_buffer[ring_head]=key;
	ring_head=(ring_head+1)%BUFFER_LEN;
}
int ___peek_key(void)
{
	if (ring_head!=ring_tail)
		return 1;
	return kbhit();
}
/*uint16_t*/int ___pop_key(void)
{
	int retval=0;
	int escapelevel=0; /* number of [ */
	int upcode=0;
	int key_len, i;

	while (((ring_head+1)%BUFFER_LEN)!=ring_tail)
	{
		int temp=getnextchar();
		if (!temp)
			break;
		___push_key(temp);
	}
	if (ring_head==ring_tail)
		return retval;

	/* this code now expects escape code not to enter this part of the code
	 * broken up into more than one single buffer read
	 */
#ifdef KEYBOARD_DEBUG
	fprintf(stderr, "ring_head=%d ring_tail=%d\n", ring_head, ring_tail);
#endif
	key_len=(ring_head-ring_tail+BUFFER_LEN)%BUFFER_LEN;
#ifdef KEYBOARD_DEBUG
	fprintf(stderr, "SEQ: %d", ring_buffer[ring_tail]);
#endif
	for (i=1;i<key_len;i++)
	{
		if (ring_buffer[(ring_tail+i)%BUFFER_LEN]==27)
		{
			key_len=i;
			break;
		}
#ifdef KEYBOARD_DEBUG
		else
			fprintf(stderr, " %d", ring_buffer[(ring_tail+i)%BUFFER_LEN]);
#endif
	}
#ifdef KEYBOARD_DEBUG
	fprintf(stderr, "\n");
#endif

	retval=ring_buffer[ring_tail];
	ring_tail=(ring_tail+1)%BUFFER_LEN;
	key_len--;
	if (retval>255)
		return retval; /* pre-parsed upcode from extern filter */

	if ((retval==27)&&key_len) /* okey as long as we read fast */
		escapelevel++;

	if ((retval==27)&&key_len&&(ring_buffer[ring_tail]=='O')) /* aterm on gentoo -> ssh -> redhat -> ncurses gives wierd results */
	{
		escapelevel+=10;
/*
		ring_tail=(ring_tail+1)%BUFFER_LEN;
		key_len--;*/
	}
	while ((retval==27)&&key_len&&(ring_buffer[ring_tail]=='['))
	{
		escapelevel++;
		ring_tail=(ring_tail+1)%BUFFER_LEN;
		key_len--;
	}
#ifdef KEYBOARD_DEBUG
	fprintf(stderr, "escapelevel=%d key_len=%d\n", escapelevel, key_len);
#endif
	if (escapelevel&&key_len)
	{
		if ((((ring_buffer[ring_tail]>='a') && (ring_buffer[ring_tail]<='z')) || (ring_buffer[ring_tail] == 10) || (ring_buffer[ring_tail] == 13)) && (key_len==1) )
		{
			upcode=ring_buffer[ring_tail]+512;
			ring_tail=(ring_tail+1)%BUFFER_LEN;
			key_len--;
		} else if ((ring_buffer[ring_tail]>='A') && (ring_buffer[ring_tail]<='Z') && (key_len==1) )
		{
			upcode=ring_buffer[ring_tail]+256;
			ring_tail=(ring_tail+1)%BUFFER_LEN;
			key_len--;
		} else if ( (ring_buffer[ring_tail]>='0') && (ring_buffer[ring_tail]<='9') )
		{
			while ( (ring_buffer[ring_tail]>='0') && (ring_buffer[ring_tail]<='9') && key_len)
			{
				upcode*=10;
				upcode+=ring_buffer[ring_tail]-'0';
				ring_tail=(ring_tail+1)%BUFFER_LEN;
				key_len--;
			}
			if (key_len&&(ring_buffer[ring_tail]=='~'))
			{
				ring_tail=(ring_tail+1)%BUFFER_LEN;
				key_len--;
			}
		} else if ( ( ring_buffer[ring_tail] == 'O') && key_len>=3)
		{
			ring_tail=(ring_tail+1)%BUFFER_LEN;
			key_len--;
			if ( (ring_buffer[ring_tail]>='0') && (ring_buffer[ring_tail]<='9') )
				upcode = (ring_buffer[ring_tail] - '0') * 16;
			else
				upcode = (ring_buffer[ring_tail] - 'A' + 10) * 16;
			ring_tail=(ring_tail+1)%BUFFER_LEN;
			key_len--;
			if ( (ring_buffer[ring_tail]>='0') && (ring_buffer[ring_tail]<='9') )
				upcode |= ring_buffer[ring_tail] - '0';
			else
				upcode |= ring_buffer[ring_tail] - 'A' + 10;
			ring_tail=(ring_tail+1)%BUFFER_LEN;
			key_len--;
		} else if ( ( ring_buffer[ring_tail] == 'O') && key_len==1)
		{
			if ((ring_buffer[ring_tail]>='A') && (ring_buffer[ring_tail]<='Z'))
				upcode=ring_buffer[ring_tail]+256;
			ring_tail=(ring_tail+1)%BUFFER_LEN;
			key_len--;
		}
	}
#ifdef KEYBOARD_DEBUG
	fprintf(stderr, "poutput-keyboard.c: testing upcode=%d escapelevel=%d (key_len=%d n=%d)\n", upcode, escapelevel, key_len, ring_buffer[ring_tail]);
#endif
	if (escapelevel==11)
	{ /* redhat and ncurses is one big pain */
		switch (upcode)
		{
			default:
				retval=KEY_BACKSPACE;
				break;
			case 0x5A:
				retval=KEY_CTRL_UP;
				break;
			case 0x5B:
				retval=KEY_CTRL_DOWN;
				break;
			case 0x5C:
				retval=KEY_CTRL_RIGHT;
				break;
			case 0x5D:
				retval=KEY_CTRL_LEFT;
				break;
			case 'H'+256:
				retval=KEY_HOME;
				break;
			case 'F'+256:
				retval=KEY_END;
				break;
		}
	} else if (escapelevel==3)
	{
		switch (upcode)
		{
			case 'A'+256:
				retval=KEY_F(1);
				break;
			case 'B'+256:
				retval=KEY_F(2);
				break;
			case 'C'+256:
				retval=KEY_F(3);
				break;
			case 'D'+256:
				retval=KEY_F(4);
				break;
			case 'E'+256:
				retval=KEY_F(5);
				break;
			default:
				retval=0;
				break;
		}

	} else if (escapelevel==2)
	{
		switch (upcode)
		{
			case 17:
				retval=KEY_F(6);
				break;
			case 18:
				retval=KEY_F(7);
				break;
			case 19:
				retval=KEY_F(8);
				break;
			case 20:
				retval=KEY_F(9);
				break;
			case 21:
				retval=KEY_F(10);
				break;
			case 23:
				retval=KEY_F(11);
				break;
			case 24:
				retval=KEY_F(12);
				break;

			case 'A'+256:
				retval=KEY_UP;
				break;
			case 'B'+256:
				retval=KEY_DOWN;
				break;
			case 'C'+256:
				retval=KEY_RIGHT;
				break;
			case 'D'+256:
				retval=KEY_LEFT;
				break;
	/*
			case 'P'+256:
				retval=KEY_BREAK;
				break;*/
			case 1:
				retval=KEY_HOME;
				break;
			case 2:
				retval=KEY_INSERT;
				break;
			case 3:
				retval=KEY_DELETE;
				break;
			case 4:
				retval=KEY_END;
				break;
			case 5:
				retval=KEY_PPAGE;
				break;
			case 6:
				retval=KEY_NPAGE;
				break;

/*
			case 25:
				retval=KEY_SHIFT_F1;
				break;
			case 26:
				retval=KEY_SHIFT_F2;
				break;
			case 28:
				retval=KEY_SHIFT_F3;
				break;
			case 29:
				retval=KEY_SHIFT_F4;
				break;
			case 31:
				retval=KEY_SHIFT_F5;
				break;
			case 32:
				retval=KEY_SHIFT_F6;
				break;
			case 33:
				retval=KEY_SHIFT_F7;
				break;
			case 34:
				retval=KEY_SHIFT_F8;
				break;*/

			default:
				retval=0;
				break;

		}
	} else if (escapelevel==1)
	{
		switch (upcode)
		{
			case 'a'+512:
				retval=KEY_ALT_A;
				break;
			case 'b'+512:
				retval=KEY_ALT_B;
				break;
			case 'c'+512:
				retval=KEY_ALT_C;
				break;
/*
			case 'd'+512:
				retval=KEY_ALT_D;
				break;
*/
			case 'e'+512:
				retval=KEY_ALT_E;
				break;
/*
			case 'f'+512:
				retval=KEY_ALT_F;
				break;
*/
			case 'g'+512:
				retval=KEY_ALT_G;
				break;
/*
			case 'h'+512:
				retval=KEY_ALT_H;
				break;
*/
			case 'i'+512:
				retval=KEY_ALT_I;
				break;
/*
			case 'j'+512:
				retval=KEY_ALT_J;
				break;
*/
			case 'k'+512:
				retval=KEY_ALT_K;
				break;
			case 'l'+512:
				retval=KEY_ALT_L;
				break;
			case 'm'+512:
				retval=KEY_ALT_M;
				break;
/*
			case 'n'+512:
				retval=KEY_ALT_N;
				break;
*/
			case 'o'+512:
				retval=KEY_ALT_O;
				break;
			case 'p'+512:
				retval=KEY_ALT_P;
				break;
/*
			case 'q'+512:
				retval=KEY_ALT_Q;
				break;
*/
			case 'r'+512:
				retval=KEY_ALT_R;
				break;
			case 's'+512:
				retval=KEY_ALT_S;
				break;
/*
			case 't'+512:
				retval=KEY_ALT_T;
				break;
			case 'u'+512:
				retval=KEY_ALT_U;
				break;
			case 'v'+512:
				retval=KEY_ALT_V;
				break;
			case 'w'+512:
				retval=KEY_ALT_W;
				break;
*/
			case 'x'+512:
				retval=KEY_ALT_X;
				break;
/*
			case 'y'+512:
				retval=KEY_ALT_Y;
				break;
*/
			case 'z'+512:
				retval=KEY_ALT_Z;
				break;
			case 10+512:
			case 13+512:
				retval=KEY_ALT_ENTER;
				break;
			default:
				retval=0;

		}
	} else {
		switch (retval)
		{
			case 242:
				retval=KEY_ALT_R;
				break;
			case 240:
				retval=KEY_ALT_P;
				break;
			case 239:
				retval=KEY_ALT_O;
				break;
			case 233:
				retval=KEY_ALT_I;
				break;
			case 231:
				retval=KEY_ALT_G;
				break;
			case 229:
				retval=KEY_ALT_E;
				break;
			case 227:
				retval=KEY_ALT_C;
				break;
			case 141:
				retval=KEY_ALT_ENTER;
				break;
			case 127:
			case 8:
				retval=KEY_BACKSPACE;
				break;
		}
	}
#ifdef KEYBOARD_DEBUG
	fprintf(stderr, "ring_head=%d ring_tail=%d\n", ring_head, ring_tail);
#endif

	if (!retval)
	{
#ifdef KEYBOARD_DEBUG
		fprintf(stderr, "poutput-keyboard.c: upcode %d, escapelevel %d gave no result\n", upcode, escapelevel);
#endif
		return ___pop_key();
	}
#ifdef KEYBOARD_DEBUG
	fprintf(stderr, "gave result %04x\n", retval);
#endif
	return retval;
}

int ___valid_key(uint16_t key)
{
	switch (key)
	{
		case KEY_ESC:
		case KEY_NPAGE:
		case KEY_PPAGE:
		case KEY_HOME:
		case KEY_END:
		case KEY_TAB:
		case _KEY_ENTER:
		case KEY_DOWN:
		case KEY_UP:
		case KEY_LEFT:
		case KEY_RIGHT:
		case KEY_ALT_A:
		case KEY_ALT_B:
		case KEY_ALT_C:
		case KEY_ALT_E:
		case KEY_ALT_G:
		case KEY_ALT_I:
		case KEY_ALT_K:
		case KEY_ALT_L:
		case KEY_ALT_M:
		case KEY_ALT_O:
		case KEY_ALT_P:
		case KEY_ALT_R:
		case KEY_ALT_S:
		case KEY_ALT_X:
		case KEY_ALT_Z:
		case KEY_BACKSPACE:
		case KEY_F(1):
		case KEY_F(2):
		case KEY_F(3):
		case KEY_F(4):
		case KEY_F(5):
		case KEY_F(6):
		case KEY_F(7):
		case KEY_F(8):
		case KEY_F(9):
		case KEY_F(10):
		case KEY_F(11):
		case KEY_F(12):
		case KEY_DELETE:
		case KEY_INSERT:
		case 'a':
		case 'b':
		case 'c':
		case 'd':
		case 'e':
		case 'f':
		case 'g':
		case 'h':
		case 'i':
		case 'j':
		case 'k':
		case 'l':
		case 'm':
		case 'n':
		case 'o':
		case 'p':
		case 'q':
		case 'r':
		case 's':
		case 't':
		case 'u':
		case 'v':
		case 'w':
		case 'x':
		case 'y':
		case 'z':
		case 'A':
		case 'B':
		case 'C':
		case 'D':
		case 'E':
		case 'F':
		case 'G':
		case 'H':
		case 'I':
		case 'J':
		case 'K':
		case 'L':
		case 'M':
		case 'N':
		case 'O':
		case 'P':
		case 'Q':
		case 'R':
		case 'S':
		case 'T':
		case 'U':
		case 'V':
		case 'W':
		case 'X':
		case 'Y':
		case 'Z':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		case '0':
		case '/':
		case '*':
		case '-':
		case '+':
		case '\\':
		case '\'':
		case ',':
		case '.':
		case '?':
		case '!':
		case '>':
		case '<':
		case KEY_ALT_ENTER:
			return 1;

		default:
			fprintf(stderr, __FILE__ ": unknown key 0x%04x\n", (int)key);
		case KEY_SHIFT_TAB:
		case KEY_CTRL_P:
		case KEY_CTRL_D:
		case KEY_CTRL_H:
		case KEY_CTRL_J:
		case KEY_CTRL_L:
		case KEY_CTRL_Q:
		case KEY_CTRL_S:
		case KEY_CTRL_Z:
		case KEY_CTRL_BS:
		case KEY_CTRL_UP:
		case KEY_CTRL_DOWN:
		case KEY_CTRL_LEFT:
		case KEY_CTRL_RIGHT:
		case KEY_CTRL_PGUP:
		case KEY_CTRL_PGDN:
		case KEY_CTRL_ENTER:
			return 0;
	}
}
