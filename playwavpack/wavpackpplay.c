/* OpenCP Module Player
 * copyright (c) 2026 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * WavPack interface routines
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

#include "config.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "boot/plinkman.h"
#include "cpiface/cpiface.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "wavpackplay.h"
#include "wavpacktype.h"
#include "stuff/err.h"


static int wavpackIsLooped (struct cpifaceSessionAPI_t *cpifaceSession, int LoopMod)
{
	wavpackSetLoop (LoopMod);
	wavpackIdle (cpifaceSession);
	return (!LoopMod) && wavpackLooped();
}


static void wavpackCloseFile (struct cpifaceSessionAPI_t *cpifaceSession)
{
	wavpackClosePlayer (cpifaceSession);
	wavpackInfoDone (cpifaceSession);
	wavpackPicDone (cpifaceSession);
}

struct wavpackSearchC
{
	struct cpifaceSessionAPI_t *cpifaceSession;
	const char *filename;
	struct ocpfile_t *indirect_hit;
	struct ocpfile_t *direct_hit;
};

static void file_hit (void *token, struct ocpfile_t *f)
{
	const char *filename;
	struct wavpackSearchC *t = (struct wavpackSearchC *)token;
	if (t->direct_hit)
	{
		return;
	}
	t->cpifaceSession->dirdb->GetName_internalstr (f->dirdb_ref, &filename);
	if (!strcmp (filename, t->filename))
	{
		t->direct_hit = f;
		f->ref (f);
		return;
	}
	if (!t->indirect_hit)
	{
		if (!strcasecmp (filename, t->filename))
		{
			t->indirect_hit = f;
			f->ref (f);
			return;
		}
	}
}

static void dir_hit (void *token, struct ocpdir_t *f)
{
	return;
}


static void wavpackDrawGStrings (struct cpifaceSessionAPI_t *cpifaceSession)
{
	struct wavpackinfo inf;
	wavpackGetInfo (cpifaceSession, &inf);

	cpifaceSession->drawHelperAPI->GStringsFixedLengthStream
	(
		cpifaceSession,
		inf.pos,
		inf.len,
		1, /* KB */
		inf.opt25,
		inf.opt50,
		inf.bitrate / 1000
	);
}

static int wavpackProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	struct wavpackinfo inf;
	wavpackGetInfo (cpifaceSession, &inf);

	switch (key)
	{
		case KEY_ALT_K:
			cpifaceSession->KeyHelp ('p', "Start/stop pause with fade");
			cpifaceSession->KeyHelp ('P', "Start/stop pause with fade");
			cpifaceSession->KeyHelp (KEY_CTRL_P, "Start/stop pause");
			cpifaceSession->KeyHelp ('<', "Jump back (big)");
			cpifaceSession->KeyHelp (KEY_CTRL_LEFT, "Jump back (big)");
			cpifaceSession->KeyHelp ('>', "Jump forward (big)");
			cpifaceSession->KeyHelp (KEY_CTRL_RIGHT, "Jump forward (big)");
			cpifaceSession->KeyHelp (KEY_CTRL_UP, "Jump back (small)");
			cpifaceSession->KeyHelp (KEY_CTRL_DOWN, "Jump forward (small)");
			cpifaceSession->KeyHelp (KEY_CTRL_HOME, "Jump to start of track");
			return 0;
		case 'p': case 'P':
			cpifaceSession->TogglePauseFade (cpifaceSession);
			break;
		case KEY_CTRL_P:
			cpifaceSession->TogglePause (cpifaceSession);
			break;
		case KEY_CTRL_UP:
			wavpackSetPos (cpifaceSession, inf.pos - inf.rate);
			break;
		case KEY_CTRL_DOWN:
			wavpackSetPos (cpifaceSession, inf.pos + inf.rate);
			break;
		case '<':
		case KEY_CTRL_LEFT:
			{
				uint64_t newpos = inf.pos -(inf.len>>5);
				if (newpos > inf.pos)
				{
					newpos = 0;
				}
				wavpackSetPos (cpifaceSession, newpos);
			}
			break;
		case '>':
		case KEY_CTRL_RIGHT:
			{
				uint64_t newpos = inf.pos + (inf.len>>5);
				if ((newpos < inf.pos) || (newpos > inf.len)) /* catch both wrap around (not likely), and overshots */
				{
					newpos = inf.len - 4;
				}
				wavpackSetPos (cpifaceSession, newpos);
			}
			break;
		case KEY_CTRL_HOME:
			wavpackSetPos (cpifaceSession, 0);
			cpifaceSession->ResetSongTimer (cpifaceSession);
			break;
		default:
			return 0;
	}
	return 1;
}

static int wavpackOpenFile (struct cpifaceSessionAPI_t *cpifaceSession, struct moduleinfostruct *info, struct ocpfilehandle_t *file)
{
	int retval;
	const char *filename;
	struct ocpfile_t *file_wvc = 0;
	struct ocpfilehandle_t *filehandle_wvc = 0;

	if (!file)
	{
		return errFormStruc;
	}

	cpifaceSession->dirdb->GetName_internalstr (file->dirdb_ref, &filename);
	cpifaceSession->cpiDebug (cpifaceSession, "[WAVPACK] preloading %s...\n", filename);

	{
		size_t filename_len;
		char *filename_wvc_ideal;

		filename_len = strlen (filename);
		filename_wvc_ideal = malloc (filename_len+2);
		if (filename_wvc_ideal)
		{
			strcpy (filename_wvc_ideal, filename);
			filename_wvc_ideal[filename_len] = 'c';
			filename_wvc_ideal[filename_len+1] = 0;
			if ((filename_len >= 2) &&
			     ((filename[filename_len-2] == 'W') || (filename[filename_len-1] == 'V')) )
			{
				filename_wvc_ideal[filename_len] = 'C';
			}

			struct wavpackSearchC token;
			token.cpifaceSession = cpifaceSession;
			token.filename = filename_wvc_ideal;
			token.indirect_hit = 0;
			token.direct_hit = 0;
			ocpdirhandle_pt readdir = file->origin->parent->readdir_start(file->origin->parent, file_hit, dir_hit, &token);
			if (readdir)
			{
				while (file->origin->parent->readdir_iterate(readdir) && (!token.direct_hit))
				{
				}
				file->origin->parent->readdir_cancel (readdir);
			}
			if (token.direct_hit)
			{
				file_wvc = token.direct_hit;
				if (token.indirect_hit)
				{
					token.indirect_hit->unref (token.indirect_hit);
				}
			} else if (token.indirect_hit)
			{
				file_wvc = token.indirect_hit;
			}
		}

		free (filename_wvc_ideal);
	} while (0);

	if (file_wvc)
	{
		filehandle_wvc = file_wvc->open (file_wvc);
		file_wvc->unref (file_wvc);
		file_wvc = 0;
	}

	cpifaceSession->IsEnd = wavpackIsLooped;
	cpifaceSession->ProcessKey = wavpackProcessKey;
	cpifaceSession->DrawGStrings = wavpackDrawGStrings;
	cpifaceSession->InPause = 0;

	retval = wavpackOpenPlayer(file, filehandle_wvc, cpifaceSession);
	if (filehandle_wvc)
	{
		filehandle_wvc->unref (filehandle_wvc);
		filehandle_wvc = 0;
	}

	if (retval)
	{
		return retval;
	}


	wavpackInfoInit (cpifaceSession);
	wavpackPicInit (cpifaceSession);

	return errOk;
}


static int wavpackPluginInit (struct PluginInitAPI_t *API)
{
	return wavpack_type_init (API);
}

static void wavpackPluginClose (struct PluginCloseAPI_t *API)
{
	wavpack_type_done (API);
}

OCP_INTERNAL const struct cpifaceplayerstruct wavpackPlayer = {"[WavPack plugin]", wavpackOpenFile, wavpackCloseFile};
DLLEXTINFO_PLAYBACK_PREFIX struct linkinfostruct dllextinfo = {.name = "playwavpack", .desc = "OpenCP WavPack Player (c) 2026 Stian Skjelstad", .ver = DLLVERSION, .sortindex = 95, .PluginInit = wavpackPluginInit, .PluginClose = wavpackPluginClose};
