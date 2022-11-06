/* OpenCP Module Player
 * copyright (c) 2020-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Charset editor for the the File selector ][ (used by .ZIP file handler and others)
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

#include <iconv.h>

/* also free iconv_handle and input_testtext at the right times */
static void LoadIconv(iconv_t *iconv_handle, const char *src, char ***input_testtext, char ***output_testtext)
{
	char *temp;
	int i;
	if (*output_testtext)
	{
		for (i=0; output_testtext[0][i]; i++)
		{
			free (output_testtext[0][i]);
		}
		free (*output_testtext);
		*output_testtext = 0;
	}
	if ((*iconv_handle) != (iconv_t)(-1))
	{
		iconv_close (*iconv_handle);
		(*iconv_handle) = (iconv_t)(-1);
	}
	if (!src)
	{
		if (*input_testtext)
		{
			for (i=0; input_testtext[0][i]; i++)
			{
				free (input_testtext[0][i]);
			}
			free (*input_testtext);
			*input_testtext = 0;
		}
		return;
	}

	temp = malloc (strlen (src) + 11);
	if (temp)
	{
		sprintf (temp, "%s//TRANSLIT", src);
		(*iconv_handle) = iconv_open ("UTF-8", temp);
		free (temp);
		temp = 0;
		if ((*iconv_handle) != (iconv_t)-1)
		{
			goto refill;
		}
	}
	(*iconv_handle) = iconv_open ("UTF-8", src);
	if ((*iconv_handle) == (iconv_t)-1)
	{
		return;
	}
refill:
	i = 0;
	if (*input_testtext)
	{
		for (i=0; input_testtext[0][i]; i++)
		{
		}
	}
	*output_testtext = calloc (i + 1, sizeof (char **));
	if (!*output_testtext)
	{
		return;
	}
	i=0;
	if (*input_testtext)
	{
		for (i=0; input_testtext[0][i]; i++)
		{
			size_t inputbuflen = strlen (input_testtext[0][i]);
			size_t outputbuflen = inputbuflen * 4;
			const char *inputbuf;
			char *outputbuf;

			output_testtext[0][i] = malloc (outputbuflen + 1);

			inputbuf = input_testtext[0][i];
			outputbuf = output_testtext[0][i];

			while (inputbuflen)
			{
				iconv (*iconv_handle, (char **)&inputbuf, &inputbuflen, &outputbuf, &outputbuflen); /* iconv inputbuf is not const.... */
				if (inputbuflen)
				{
					if (errno == E2BIG)
					{
						break;
					}

					outputbuf[0] = '\xff';
					outputbuf++;
					outputbuflen--;
					inputbuf++;
					inputbuflen--;
				}
			}
			outputbuf[0] = 0;
		}
	}
	output_testtext[0][i] = 0;
}

static int fsEditCharset (struct ocpdir_t *dir)
{
	static int state = 0;
	/* 0 - new / idle
	 * 1 - in editGroup
         * 2 - in editItem
	 * 3 - in keyboard help1
	 * 4 - in keyboard help2
	 */
	static signed int currentGroup, oldGroup;
	static signed int currentItem;

	static int widestgroup = 0;
	static int widestentry = 0;

	static int groupcount = 0;
	static int itemcount = 0; /* count in the current selected group */
	static int testtextcount;

	static const char *active_key;
	static const char *active_description;

	static char **displaytext = 0;
	static char **testtext = 0;

	unsigned int top;
	unsigned int height;
	unsigned int heighttop;
	unsigned int heightbottom;
	unsigned int left;
	unsigned int width;

	unsigned int vpos1;
	unsigned int vpos2;

	int i, j;
	unsigned int x, y;

	static iconv_t iconv_handle = (iconv_t)(-1);

	const char *default_label = "";
	const char *default_key = "";

	if (dir && dir->charset_override_API)
	{
		dir->charset_override_API->get_default_string (dir, &default_label, &default_key);
	}

	if (state == 0)
	{
		/* default values if search below fails */
		currentGroup = 0; oldGroup = -1;
		currentItem = 0;
		state = 1;
		active_description = 0;

		if (dir && dir->charset_override_API)
		{
			active_key = dir->charset_override_API->get_byuser_string (dir);
			testtext = dir->charset_override_API->get_test_strings (dir);
		} else {
			active_key = 0;
			testtext = 0;
		}

		/* try to locate active_key */
		for (i=0; charset_collections[i].label; i++)
		{
			int len = strlen (charset_collections[i].label);
			if (len > widestgroup)
			{
				widestgroup = len;
			}
			for (j=0; charset_collections[i].entries[j].label; j++)
			{
				if ((oldGroup == -1) && active_key && (!strcmp (active_key, charset_collections[i].entries[j].key)))
				{
					oldGroup = currentGroup = i;
					currentItem = j;
					active_description = charset_collections[i].entries[j].description;
					state = 2;
				}
				len = strlen (charset_collections[i].entries[j].label);
				if (len > widestentry)
				{
					widestentry = len;
				}
			}
		}
		groupcount = i + 1;

		if (!active_key)
		{
			oldGroup = currentGroup = groupcount - 1;
			currentItem = 0;
		}

		if (currentGroup + 1 == groupcount)
		{
			itemcount = 1;
		} else {
			for (j=0; charset_collections[currentGroup].entries[j].label; j++)
			{
			}
			itemcount = j;
		}

		testtextcount = 0;
		if (testtext)
		{
			for (i=0; testtext[i]; i++)
			{
				testtextcount++;
			}
		}

		LoadIconv (&iconv_handle, active_key ? active_key : default_key, &testtext, &displaytext);
	}

	if ((state == 3) || (state == 4))
	{
		if (cpiKeyHelpDisplay())
		{
			framelock();
			return 1;
		}
		state -= 2;
	}

	/* Calculate size */
	{
#define heighttopmax groupcount
#define heightbottommax (testtextcount + 2)
		uint8_t add_to_cycle = 0;
		height = plScrHeight-6;
		//heighttop = (height - 2) / 2;
		heighttop = 3;
		heightbottom = 3;
		height = 1 + heighttop + 1 + heightbottom + 1;

		while (1)
		{
			if (((heighttop >= heighttopmax) && (heightbottom >= heightbottommax)) || height >= (plScrHeight - 6))
			{
				break;
			} else if ((heighttop < heighttopmax) && (add_to_cycle != 1))
			{
				heighttop++;
				height++;
			} else if (heightbottom < heightbottommax)
			{
				heightbottom++;
				height++;
			}
			add_to_cycle = (add_to_cycle + 1) % 3;
		}

		width = plScrWidth - 6;

		top = (plScrHeight - height) / 2;
		left = (plScrWidth - width) / 2;
		#warning need to find ideal top and left to match where we pop out from
#undef heighttopmax
#undef heightbottommax
	}

	/* Draw */
	{
		unsigned int LeftRightSplitBar = left + widestgroup + 5;

		/* calculate the scrollbar for the group and entry lists */
		if (heighttop < groupcount)
		{
			vpos1 = currentGroup * heighttop / groupcount;
		} else {
			vpos1 = heighttop;
		}

		if (heighttop < itemcount)
		{
			vpos2 = currentItem * heighttop / itemcount;
			if (vpos2 == heighttop)
			{
				vpos2--;
			}
		} else {
			vpos2 = heighttop;
		}

		/* Top line, consists of two corners, top T of the LeftRight top-window split and a title
                 *   +-------+--- Title ----------------+
		 */
		displaystr(top, left, 0x04, "\xda", 1);
		for (x = left + 1; x < ( left + width - 1); x++)
		{
			displaystr(top, x, 0x04, x == LeftRightSplitBar ? "\xc2" : "\xc4", 1);
		}
		displaystr(top, left + width - 1, 0x04, "\xbf", 1);
		displaystr(top, left + width/2 - 10, 0x04, " Charset selector ", 18);

		/* LeftRight top-window that displays the groups and items.... The group and items text will be filled in later
		 * | Group    |  Item               |
		 * | Group    |  Item               |
		 * | Group    |  Item               |
		 */
		for (y = 1; y < (heighttop + 1); y++)
		{
			displaystr (y + top, left            , 0x04,                      "\xb3",          1);
			displaystr (y + top, LeftRightSplitBar  , 0x04, (y != (vpos1 + 1)) ? "\xb3" : "\xdd", 1);
			displaystr (y + top, left + width - 1, 0x04, (y != (vpos2 + 1)) ? "\xb3" : "\xdd", 1);
		}

		/* The middle bar that contains the bottom T for the top LeftRight window.
		 * *------------*------------------*
		 */
		displaystr(top + y, left, 0x04, "\xc3", 1);
		for (x = left + 1; x < ( left + width - 1); x++)
		{
			displaystr(top + y, x, 0x04, x == LeftRightSplitBar ? "\xc1" : "\xc4", 1);
		}
		displaystr(top + y, left + width - 1, 0x04, "\xb4", 1);
		y++;

		/* The bottom window, that should contain some info and preview text
		 * |                                       |
		 * |                                       |
		 * |                                       |
		 */
		for (; ( y + 1 ) < height; y++)
		{
			displaystr (y + top, left            , 0x04,               "\xb3",        1);
			displaystr (y + top, left + width - 1, 0x04, /*(i!=vpos)?*/"\xb3"/*:"\xdd"*/, 1);
		}

		/* The bottom bar
		 * *-------------------------------*
		 */
		displaystr(top + y, left, 0x04, "\xc0", 1);
		for (x = left + 1; x < ( left + width - 1); x++)
		{
			displaystr(top + y, x, 0x04, "\xc4", 1);
		}
		displaystr(top + y, left + width - 1, 0x04, "\xd9", 1);
		y++;
	}

	{
		int firstgroup;
		int firstitem;

		firstgroup = currentGroup - heighttop / 2;
		if ((unsigned)(firstgroup + heighttop) > groupcount)
		{
			firstgroup = groupcount - heighttop;
		}
		if (firstgroup < 0)
		{
			firstgroup = 0;
		}

		if ((state == 1) && (oldGroup != currentGroup))
		{
			firstitem = 0;
		} else {
			firstitem = currentItem - heighttop / 2;
			if ((unsigned)(firstitem + heighttop) > itemcount)
			{
				firstitem = itemcount - heighttop;
			}
			if (firstitem < 0)
			{
				firstitem = 0;
			}
		}

		for (y = 0; y < heighttop; y++)
		{
			signed group = y + firstgroup;
			signed item = y + firstitem;

			/* draw the major groups */
			if (group < groupcount)
			{
				uint8_t attr = (group + 1 == groupcount) ? 0x09 : 0x0f;
				if (state == 1)
				{
					attr |= (group == currentGroup) ? 0x80 : 0x00;
					displaystr(top + y + 1, left               + 1, attr, "  ", 2);
					displaystr(top + y + 1, left + widestgroup + 3, attr, "  ", 2);
				} else {
					displaystr(top + y + 1, left               + 1, attr, (group == currentGroup) ? "->" : "  ", 2);
					displaystr(top + y + 1, left + widestgroup + 3, attr, (group == currentGroup) ? "<-" : "  ", 2);
				}
				if ((group + 1) == groupcount)
				{
					displaystr(top + y + 1, left + 3, attr, "default", widestgroup);
				} else {
					displaystr(top + y + 1, left + 3, attr, charset_collections[group].label, widestgroup);
				}
			} else {
				displaystr(top + y + 1, left + 1, 0x00, " ", widestgroup + 4);
			}

			if (item < itemcount)
			{
				uint8_t attr;
				if (state == 2)
				{
					attr = (item == currentItem) ? 0x8f : 0x0f;
					displaystr(top + y + 1, left + widestgroup + 6, attr, "  ", 2);
					displaystr(top + y + 1, left + width       - 3, attr, "  ", 2);
				} else {
					attr = 0x0f;
					displaystr(top + y + 1, left + widestgroup + 6, attr, ((oldGroup == currentGroup) && (item == currentItem)) ? "->" : "  ", 2);
					displaystr(top + y + 1, left + width       - 3, attr, ((oldGroup == currentGroup) && (item == currentItem)) ? "<-" : "  ", 2);
				}

				if ((currentGroup + 1) == groupcount)
				{
					displaystr(top + y + 1, left + widestgroup + 8, attr, default_label, width - widestgroup - 11);
				} else {
					displaystr(top + y + 1, left + widestgroup + 8, attr, charset_collections[currentGroup].entries[item].label, width - widestgroup - 11);
				}
			} else {
				displaystr (y + top + 1, left + widestgroup + 6, 0x00, " ", width - widestgroup - 7);
			}
		}
	}
	{
		int l;
		displaystr (top + heighttop + 2, left + 1, 0x00, " ", 3);
		displaystr (top + heighttop + 2, left + 3, 0x09, active_key ? active_key : default_key, width - 4);
		displaystr (top + heighttop + 3, left + 1, 0x00, " ", 3);
		displaystr (top + heighttop + 3, left + 3, 0x09, active_description ? active_description : "", width - 4);
		for (y = 2, l = 0; y < heightbottom; y++)
		{
			if (!displaytext)
			{
				if (!l)
				{
					displaystr (top + heighttop + 2 + y, left + 1, 0x04, " failed to generate converter - encoding not supported", width - 2);
					l++;
				} else {
					displaystr (top + heighttop + 2 + y, left + 1, 0x00, " ", width - 2);
				}
			} else {
				if (displaytext[l])
				{
					char *data = displaytext[l];
					int pos = left + 1;
					int left = width - 2;
					while (left)
					{
						char *epos = strchr (data, 0xff);
						if (!epos)
						{
							displaystr_utf8 (top + heighttop + 2 + y, pos, 0x0f, data, left);
							left = 0;
						} else {
							int need = measurestr_utf8 (data, epos - data);
							if (need > left)
							{
								need = left;
							}
							displaystr_utf8 (top + heighttop + 2 + y, pos, 0x0f, data, need);
							data = epos +  1;
							left -= need;
							pos += need;
							if (left)
							{
								displaystr_utf8 (top + heighttop + 2 + y, pos, 0x0c, "?", 1);
								left--;
								pos++;
							}
						}
					}
					l++;
				} else {
					/* this code should not be reachable */
					displaystr_utf8 (top + heighttop + 2 + y, left + 1, 0x00, " ", width - 2);
				}
			}
		}
	}

	while (Console.KeyboardHit())
	{
		uint16_t c = Console.KeyboardGetChar();
		switch (c)
		{
			case KEY_UP:
				if (state == 1)
				{
					if (currentGroup)
					{
						currentGroup--;

						for (j=0; charset_collections[currentGroup].entries[j].label; j++)
						{
						}
						itemcount = j;
					}
				} else if (state == 2)
				{
					if (currentItem)
					{
						currentItem--;
						active_key = charset_collections[currentGroup].entries[currentItem].key;
						active_description = charset_collections[currentGroup].entries[currentItem].description;
						LoadIconv (&iconv_handle, active_key ? active_key : default_key, &testtext, &displaytext);
					}
				}
				break;
			case KEY_DOWN:
				if (state == 1)
				{
					if ((currentGroup + 1) < groupcount)
					{
						currentGroup++;
						if (currentGroup + 1 == groupcount)
						{
							itemcount = 1;
						} else {
							for (j=0; charset_collections[currentGroup].entries[j].label; j++)
							{
							}
							itemcount = j;
						}
					}
				} else if (state == 2)
				{
					if ((currentItem + 1) < itemcount)
					{
						currentItem++;
						active_key = charset_collections[currentGroup].entries[currentItem].key;
						active_description = charset_collections[currentGroup].entries[currentItem].description;
						LoadIconv (&iconv_handle, active_key ? active_key : default_key, &testtext, &displaytext);
					}
				}
				break;
			case KEY_LEFT:
				if (state == 2)
				{
					state = 1;
				}
				break;
			case KEY_RIGHT:
				if (state == 1)
				{
					state = 2;
					if (oldGroup != currentGroup)
					{
						currentItem = 0;
						oldGroup = currentGroup;
						if (currentGroup == (groupcount - 1))
						{
							active_key = 0;
							active_description = 0;
						} else {
							active_key = charset_collections[currentGroup].entries[currentItem].key;
							active_description = charset_collections[currentGroup].entries[currentItem].description;
						}
						LoadIconv (&iconv_handle, active_key ? active_key : default_key, &testtext, &displaytext);
					}
				}
				break;
			case KEY_HOME:
				if (state == 1)
				{
					currentGroup = 0;
				}
				break;
			case KEY_END:
				if (state == 1)
				{
					currentGroup = groupcount + 1;
				}
				break;
			case KEY_PPAGE:
			case KEY_NPAGE:
				break;

			case KEY_EXIT:
			case KEY_ESC:
				LoadIconv (&iconv_handle, NULL, &testtext, &displaytext);
				state = 0;
				return 0;

			case _KEY_ENTER:
				LoadIconv (&iconv_handle, NULL, &testtext, &displaytext);
				if (dir && dir->charset_override_API)
				{
					dir->charset_override_API->set_byuser_string (dir, active_key);
				}
				state = 0;
				return 0;

			case KEY_ALT_K:
				cpiKeyHelpClear();
				cpiKeyHelp(KEY_RIGHT, "Move cursor right");
				cpiKeyHelp(KEY_LEFT,  "Move cursor left");
				cpiKeyHelp(KEY_UP,    "Move cursor up");
				cpiKeyHelp(KEY_DOWN,  "Move cursor down");
				cpiKeyHelp(KEY_HOME,  "Move cursor to the top");
				cpiKeyHelp(KEY_END,   "Move cursor to the bottom");
				cpiKeyHelp(KEY_PPAGE, "Move cursor a page up");
				cpiKeyHelp(KEY_NPAGE, "Move cursor a page down");
				cpiKeyHelp(KEY_ESC, "Cancel changes");
				cpiKeyHelp(_KEY_ENTER, "Submit changes");
				state += 2;
				return 1;
		}
	}

	return 1;
}
