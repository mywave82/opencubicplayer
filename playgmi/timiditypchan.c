
#include "config.h"
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "dev/mcp.h"
#include "timidityplay.h"
#include "stuff/poutput.h"
#include "cpiface/cpiface.h"

/* static const struct minstrument *plChanMInstr; */

#warning TODO midGetMute
#define midGetMute(i) 0

static void drawchannel36(uint16_t *buf, int i)
{
	struct mchaninfo ci;
	unsigned char mute=midGetMute(i);

	unsigned char tcol=mute?0x08:0x0F;
	unsigned char tcold=mute?0x08:0x07;
/*
	unsigned char tcolr=mute?0x08:0x0B;*/

	int j;

	timidityGetChanInfo(i, &ci);

	writestring(buf, 0, tcold, " -- -- -. \xfa\xfa\xfa \xfa\xfa\xfa \xfa\xfa\xfa \xfa\xfa\xfa \xfa\xfa\xfa \xfa\xfa\xfa   ", 36);

	if (!ci.notenum)
		return;
	writenum(buf,  1, tcol, ci.program, 16, 2, 0);
	writenum(buf, 4, tcol, ci.gvol, 16, 2, 0);
	writestring(buf,  7, tcol, &"L123456MM9ABCDER"[ci.pan>>3], 1);
	writestring(buf,  8, tcol, &" P"[ci.pedal], 1);
	if (ci.notenum>6)
		ci.notenum=6;
	for (j=0; j<ci.notenum; j++)
		writestring(buf, 10+4*j, (ci.opt[j]&1)?tcol:0x08, plNoteStr[ci.note[j]+12], 3);
}

static void drawchannel62(uint16_t *buf, int i)
{
	struct mchaninfo ci;
	unsigned char mute=midGetMute(i);

	unsigned char tcol=mute?0x08:0x0F;
	unsigned char tcold=mute?0x08:0x07;
/*
	unsigned char tcolr=mute?0x08:0x0B;*/

	int j;

	timidityGetChanInfo(i, &ci);

	writestring(buf, 0, tcold, "                  -- -. \xfa\xfa\xfa \xfa\xfa\xfa \xfa\xfa\xfa \xfa\xfa\xfa \xfa\xfa\xfa \xfa\xfa\xfa \xfa\xfa\xfa \xfa\xfa\xfa \xfa\xfa\xfa ", 62);

	if (!ci.notenum)
		return;
	writestring(buf,  1, tcol, ci.instrument, 16);
	writenum(buf, 18, tcol, ci.gvol, 16, 2, 0);
	writestring(buf,  21, tcol, &"L123456MM9ABCDER"[ci.pan>>3], 1);
	writestring(buf,  22, tcol, &" P"[ci.pedal], 1);
	if (ci.notenum>9)
		ci.notenum=9;
	for (j=0; j<ci.notenum; j++)
		writestring(buf, 24+4*j, (ci.opt[j]&1)?tcol:0x08, plNoteStr[ci.note[j]+12], 3);
}

static void drawchannel76(uint16_t *buf, int i)
{
	struct mchaninfo ci;
	unsigned char mute=midGetMute(i);

	unsigned char tcol=mute?0x08:0x0F;
	unsigned char tcold=mute?0x08:0x07;
/*
	unsigned char tcolr=mute?0x08:0x0B;*/

	int j;

	timidityGetChanInfo(i, &ci);

	writestring(buf, 0, tcold, "               \xfa  \xfa \xfa \xfa\xfa\xfa \xfa\xfa  \xfa\xfa\xfa \xfa\xfa  \xfa\xfa\xfa \xfa\xfa  \xfa\xfa\xfa \xfa\xfa  \xfa\xfa\xfa \xfa\xfa  \xfa\xfa\xfa \xfa\xfa  \xfa\xfa\xfa \xfa\xfa", 76);

	if (!ci.notenum)
		return;
	writestring(buf,  1, tcol, ci.instrument, 14);
	writenum(buf, 16, tcol, ci.gvol, 16, 2, 0);
	writestring(buf, 19, tcol, &"L123456MM9ABCDER"[ci.pan>>3], 1);
	if (ci.notenum>7)
		ci.notenum=7;
	for (j=0; j<ci.notenum; j++)
	{
		writestring(buf, 22+8*j, (ci.opt[j]&1)?tcol:0x08, plNoteStr[ci.note[j]+12], 3);
		writenum(buf, 26+8*j, (ci.opt[j]&1)?tcold:0x08, ci.vol[j], 16, 2, 0);
	}
}

static void drawchannel128(uint16_t *buf, int i)
{
	struct mchaninfo ci;
	unsigned char mute=midGetMute(i);

	unsigned char tcol=mute?0x08:0x0F;
	unsigned char tcold=mute?0x08:0x07;
/*
	unsigned char tcolr=mute?0x08:0x0B;*/

	int j;

	timidityGetChanInfo(i, &ci);

	writestring(buf, 0, tcold,
			"                  \xfa  \xfa \xfa     \xfa  \xfa  \xfa  \xfa\xfa\xfa \xfa\xfa  \xfa\xfa\xfa \xfa\xfa"
			"  \xfa\xfa\xfa \xfa\xfa  \xfa\xfa\xfa \xfa\xfa  \xfa\xfa\xfa \xfa\xfa  \xfa\xfa\xfa \xfa\xfa  "
			"\xfa\xfa\xfa \xfa\xfa  \xfa\xfa\xfa \xfa\xfa  \xfa\xfa\xfa \xfa\xfa  \xfa\xfa\xfa \xfa\xfa  \xfa\xfa\xfa \xfa\xfa  ", 128);

	if (!ci.notenum)
		return;

	writestring(buf,  1, tcol, ci.instrument, 16);
	writenum(buf, 19, tcol, ci.gvol, 16, 2, 0);
	writestring(buf, 22, tcol, &"L123456MM9ABCDER"[ci.pan>>3], 1);
	writestring(buf, 24, tcol, (ci.pitch<0)?"-":(ci.pitch>0)?"+":" ", 1);
	writenum(buf, 25, tcol, abs(ci.pitch), 16, 4, 0);
	writenum(buf, 30, tcol, ci.reverb, 16, 2, 0);
	writenum(buf, 33, tcol, ci.chorus, 16, 2, 0);

	if (ci.notenum>11)
		ci.notenum=11;
	for (j=0; j<ci.notenum; j++)
	{
		writestring(buf, 38+8*j, (ci.opt[j]&1)?tcol:0x08, plNoteStr[ci.note[j]+12], 3);
		writenum(buf, 42+8*j, (ci.opt[j]&1)?tcold:0x08, ci.vol[j], 16, 2, 0);
	}
}

static void drawchannel44(uint16_t *buf, int i)
{
	struct mchaninfo ci;
	unsigned char mute=midGetMute(i);

	unsigned char tcol=mute?0x08:0x0F;
	unsigned char tcold=mute?0x08:0x07;
/*
	unsigned char tcolr=mute?0x08:0x0B;*/

	int j;

	timidityGetChanInfo(i, &ci);

	writestring(buf, 0, tcold, " -- -- -. \xfa\xfa\xfa \xfa\xfa\xfa \xfa\xfa\xfa \xfa\xfa\xfa \xfa\xfa\xfa \xfa\xfa\xfa \xfa\xfa\xfa \xfa\xfa\xfa   ", 44);

	if (!ci.notenum)
		return;

	writenum(buf,  1, tcol, ci.program, 16, 2, 0);
	writenum(buf, 4, tcol, ci.gvol, 16, 2, 0);
	writestring(buf,  7, tcol, &"L123456MM9ABCDER"[ci.pan>>3], 1);
	writestring(buf,  8, tcol, &" P"[ci.pedal], 1);

	if (ci.notenum>8)
		ci.notenum=8;
	for (j=0; j<ci.notenum; j++)
		writestring(buf, 10+4*j, (ci.opt[j]&1)?tcol:0x08, plNoteStr[ci.note[j]+12], 3);
}

static void drawchannel(uint16_t *buf, int len, int i)
{
	switch (len)
	{
		case 36:
			drawchannel36(buf, i);
			break;
		case 44:
			drawchannel44(buf, i);
			break;
		case 62:
			drawchannel62(buf, i);
			break;
		case 76:
			drawchannel76(buf, i);
			break;
		case 128:
			drawchannel128(buf, i);
			break;
	}
}

void __attribute__ ((visibility ("internal"))) timidityChanSetup(/*const struct midifile *mid*/)
{
	/*plChanMInstr=mid->instruments; TODO ?*/
	plUseChannels(drawchannel);
}
