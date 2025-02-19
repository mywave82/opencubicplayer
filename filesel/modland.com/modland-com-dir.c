/* OpenCP Module Player
 * copyright (c) 2024-'25 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Support for accessing https://modland.com from the filebrowser
 *  - Directories
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



static const char *known_adlib_directories[] =
{
	"ADL",
	"AMusic",
	"AMusic XMS",
	"AdLib Tracker",
	"AdLib Tracker 2",
	"Apogee",
	"Beni Tracker",
	"Bob's AdLib Music",
	"Boom Tracker",
	"Creative Music File",
	/* "DOSBox", should have worked, 2 of 2 fails to load */
	"DeFy AdLib Tracker",
	"Digital FM",
	"EdLib D00",
	/* "Edlib D01", includes PCM audio */
	/* "EdLib Packed", includes PCM audio */
	"Exotic AdLib",
	"Extra Simple Music",
	// "FM Tracker",
	// "FM-Kingtracker",
	"Faust Music Creator"
	"HSC AdLib Composer",
	"Herad Music System",
	"Johannes Bjerregard Module",
	"Ken's AdLib Music",
	"Loudness Sound System",
	"LucasArts",
	"MK-Jamz",
	"MPU-401 Trakker",
	// "MUS", detected as IMS, fails to load
	"Martin Fernandez", /* These are actually Apogee IMF files */
	"Master Tracker",
	"Mlat Adlib Tracker",
	"Palladix",
	// "Pixel Painters FMF",
	// "Pixel Painters FTF",
	"Raw OPL Capture",
	"Reality AdLib Tracker",
	"SNG Player",
	"Screamtracker 3 AdLib",
	// "Shadowlands",
	"Sierra",
	// "Sound Images Generation 2",
	// "Sound Interface System",
	"Surprise! AdLib Tracker",
	"Surprise! AdLib Tracker 2.0",
	"Twin TrackPlayer",
	"Ultima 6",
	// "Vibrants",
	"Visual Composer",
};

static const char *known_root_directories[] =
{
	"AHX",                                /* HVL */
	// "AM Composer",
	// "AND XSynth",
	// "AProSys",
	// "AXS",
	// "AY Amadeus",
	"AY Emul",                            /* AY */
        // "AY STRC",
	// "Ace Tracker",
	// "Actionamics",
	// "Activision Pro",
	"Ad Lib",
	// "Aero Studio",
	// "All Sound Tracker",
	// "Anders Oland",
	// "ArkosTracker",
	// "Arpeggiator",
	// "Art And Magic",
	// "Art Of Noise",
	// "Astroidea XMF",
	// "Asylum",
	"Atari Digi-Mix",                     /* YM*/
	// "Athtune",
	// "Audio Sculpture",
	// "BP SoundMon 2",
	// "BP SoundMon 3",
	// "BeRoTracker",
	// "Beathoven Synthesizer",
	// "Beaver Sweeper",
	// "Beepola",
	// "Ben Daglish",
	// "Ben Daglish SID",
	// "BoyScout",
	// "Buzz",
	// "Buzzic",
	// "Buzzic 2",
	// "CBA",
	// "Capcom Q-Sound Format",
	// "Composer 667",
	"Composer 669",                       /* 669 */
	// "Composer 670 (CDFM)",
	// "Compoz",
	// "Core Design",
	"Cubic Tiny XM",                      /* MXM     1 of 2 files are a broken, perhaps version 1.0 had different sample headers than laters versions? */
	// "CustomMade",
	// "Cybertracker",
	// "Cybertracker C64",
	// "DSMI Compact",
	// "Darius Zendeh",
	// "DarkWave Studio",
	// "Dave Lowe",
	// "Dave Lowe New",
	// "David Hanney",
	// "David Whittaker",
	// "Deflemask",
	// "Delitracker Custom",
	// "Delta Music",
	// "Delta Music 2",
	// "Delta Packer",
	// "Desire",
	// "Digibooster",
	// "Digibooster Pro",
	// "Digital Mugician",
	// "Digital Mugician 2",
	// "Digital Sonix And Chrome",
	// "Digital Sound And Music Interface",
	// "Digital Sound Interface Kit",
	// "Digital Sound Interface Kit RIFF",
	// "Digital Sound Studio",
	// "Digital Symphony",
	// "Digital Tracker DTM",
	"Digital Tracker MOD",                /* MOD */
	"Digitrakker",                        /* MDL */
	// "Digitrekker",
	// "Dirk Bialluch",
	// "Disorder Tracker 2",
	// "DreamStation",
	// "Dreamcast Sound Format",
	// "Dynamic Studio Professional",
	// "Dynamic Synthesizer",
	// "EarAche",
	// "Electronic Music System",
	// "Electronic Music System v6",
	// "Epic Megagames MASI",
	// "Euphony",
	"Extreme Tracker",                   /* AMS */
	// "FAC SoundTracker",
	// "FM Tracker",
	// "Face The Music",
	// "FamiTracker",
	// "Farandole Composer",
	// "Fashion Tracker",
	"Fasttracker",                       /* MOD */
	"Fasttracker 2",                     /* XM */
	// "Follin Player II",
	// "Forgotten Worlds",
	// "Fred Gray",
	// "FredMon",
	// "FuchsTracker",
	// "Funktracker",
	// "Future Composer 1.3",
	// "Future Composer 1.4",
	// "Future Composer BSI",
	// "Future Player",
	// "GT Game Systems",
	// "Game Music Creator",
	// "Gameboy Sound Format",           /* <-- Should have been blaarg ? */
	"Gameboy Sound System",              /* GBS (Blaarg's game music emulator) */
	// "Gameboy Sound System GBR",
	// "Gameboy Tracker",
	// "General DigiMusic",
	// "GlueMon",
	// "GoatTracker",
	// "GoatTracker 2",
	// "Graoumf Tracker",
	// "Graoumf Tracker 2",
	"HES",                               /* HES (Blaarg's game music emulator) */
	"HVSC",                              /* SID, mirror of "The High Voltage SID Collection" */
	// "Hippel",
	// "Hippel 7V",
	// "Hippel COSO",
	// "Hippel ST",
	// "Hippel ST COSO",
	"His Master's Noise",                /* MOD/MODt */
	"HivelyTracker",                     /* HVL */
	// "Howie Davies",
	// "IFF-SMUS",
	// "Images Music System",
	// "Imago Orpheus",
	"Impulsetracker",                    /* IT */
	// "InStereo!",
	// "InStereo! 2.0",
	// "Infogrames",
	// "Ixalance",
	// "JamCracker",
	// "Janko Mrsic-Flogel",
	// "Jason Brooke",
	// "Jason Page",
	// "Jason Page Old",
	// "JayTrax",
	// "Jeroen Tel",
	// "Jesper Olsen",
	// "KSS",                            /* KSS (Blaarg's game music emulator) <- only supports SEGA, and MSX that uses the internal AY chip. No support for ram-module, stereo, FM-PAC, MSX-AUDIO, etc. Only some few tunes inside "- unknown" works */
	// "Ken's Digital Music";
	// "Klystrack",
	// "Kris Hatlelid",
	// "Leggless Music Editor",
	// "Lionheart",
	// "Liquid Tracker",
	// "MCMD",
	// "MDX",
	// "MO3",
	// "MVS Tracker",
	// "MVX Module",
	// "Mad Tracker 2",
	// "Magnetic Fields Packer",
	// "Maniacs Of Noise",
	// "Mark Cooksey",
	// "Mark Cooksey Old",
	// "Mark II",
	// "MaxTrax",
	// "Medley",
	// "MegaStation",
	// "MegaStation MIDI",
	// "Megadrive CYM",
	"Megadrive GYM",                     /* GYM (Blaarg's game music emulator) */
	"Multitracker",                      /* MTM */
	// "MikMod UNITRK",
	// "Mike Davies",
	// "Monotone",
	// "MoonBlaster",
	// "MoonBlaster (edit mode)",
	// "MultiMedia Sound",
	// "Multitracker",
	// "Music Assembler",
	// "Music Editor",
	// "MusicMaker V8",
	// "MusicMaker V8 Old",
	// "Musicline Editor",
	// "NerdTracker 2",
	// "Nintendo DS Sound Format",
	"Nintendo SPC",                      /* SPC (Blaarg's game music emulator) */
	"Nintendo Sound Format",             /* NSF (Blaarg's game music emulator) */
	// "NoiseTrekker",
	// "NoiseTrekker 2",
	// "NovoTrade Packer",
	// "OctaMED MMD0",
	// "OctaMED MMD1",
	// "OctaMED MMD2",
	// "OctaMED MMD3",
	// "OctaMED MMDC",
	"Octalyser",                         /* MOD */
	"Oktalyzer",                         /* OKT */
	// "Onyx Music File",
	// "OpenMPT MPTM",
	// "Organya",
	// "Organya 2",
	// "PMD",
	// "Paul Robotham",
	// "Paul Shields",
	// "Paul Summers",
	// "Peter Verswyvelen",
	// "Picatune",
	// "Picatune2",
	// "Pierre Adane Packer",
	// "Piston Collage",
	// "Piston Collage Protected",
	"PlaySID",                           /* SID */
	// "PlayerPro",
	// "Playstation 2 Sound Format",
	// "Playstation Sound Format",
	// "PokeyNoise",
	// "Pollytracker",
	"Polytracker",                       /* PTM */
	// "Powertracker",
	// "Pretracker",
	// "ProTrekkr",
	// "ProTrekkr 2.0",
	// "Professional Sound Artists",
	"Protracker",                        /* MOD */
	// "Protracker IFF",
	// "Psycle",
	// "Pumatracker",
	// "Quadra Composer",
	// "Quartet PSG",
	// "Quartet ST",
	// "RamTracker",
	// "Real Tracker",
	"RealSID",                           /* SID */
	// "Renoise",
	// "Renoise Old",
	// "Richard Joseph",
	// "Riff Raff",
	// "Rob Hubbard",
	// "Rob Hubbard ST",
	// "Ron Klaren",
	// "S98",
	// "SBStudio",
	// "SC68",                           /* <-- https://github.com/Zeinok/sc68 */
	// "SCC-Musixx",
	// "SCUMM",
	// "SNDH",
	// "SPU",
	// "SVAr Tracker",
	// "Sam Coupe COP";
	// "Sam Coupe SNG",
	// "Saturn Sound Format",
	"Screamtracker 2",                   /* STM */
	"Screamtracker 3",                   /* S3M */
	// "Sean Connolly",
	// "Sean Conran",
	// "Shroom",
	// "SidMon 1",                       /* <-- should have been SID */
	// "SidMon 2",                       /* <-- should have been SID? */
	"Sidplayer",                         /* SID */
	// "Silmarils",
	// "Skale Tracker",
	"Slight Atari Player",               /* SAP (Blaarg's game music emulator) */
	// "Sonic Arranger",
	// "Sound Club",
	// "Sound Club 2",
	// "Sound Images",
	// "Sound Master",
	// "Sound Master II v1",
	// "Sound Master II v3",
	// "Sound Programming Language",
	// "SoundControl",
	// "SoundFX",
	// "SoundFactory",
	// "SoundPlayer",
	"Soundtracker",                      /* M15 */
	// "Soundtracker 2.6",               /* <- different file format, just looks similiar to .MOD */
	// "Soundtracker Pro II",            /* <- different file format, just looks similiar to .MOD */
	// "Special FX",
	// "Special FX ST",
	// "Spectrum",                       /* This is bucket, just like Adlib */
	// "Speedy A1 System",
	// "Speedy System",
	// "Starkos",
	// "Startrekker AM",                 /* <- Contains (potential) AM (OPL3) instruments via external mod.nt file. Has normal FLT4 signature */
	// "Startrekker FLT8",               /* <- Could be loaded as regular MOD files, but contains a hack: two and two 4-channels patterns should be merged into a 8-channel pattern */
	"Stereo Sidplayer",                  /* SID, unsure if they actually contain stereo files */
	// "Steve Barrett",
	// "Stonetracker",
	// "SunTronic",
	// "SunVox",
	// "Super Nintendo Sound Format",    /* <-- not in blaargh ? */
	// "Symphonie",
	// "SynTracker",
	// "Synder SNG-Player",
	// "Synder SNG-Player Stereo",
	// "Synder Tracker",
	// "Synth Dream",
	// "Synth Pack",
	// "Synthesis",
	// "TCB Tracker",
	// "TFM Music Maker",
	// "TFMX",
	// "TFMX ST",
	// "TSS",
	// "The 0ok Amazing Synth Tracker",
	// "The Musical Enlightenment",
	// "Thomas Hermann",
	// "Tomy Tracker",
	// "Tunefish",
	// "Ultra64 Sound Format",
	"Ultratracker",                      /* ULT */
	"Unis 669",                          /* 669 */
	// "V2",
	// "VGM Music Maker",
	"Velvet Studio",                     /* AMS */
	// "Vic-Tracker",
	"Video Game Music",                  /* VGM (can be OPL too) */
	// "Voodoo Supreme Synthesizer",
	// "Wally Beben",
	// "Westwood SND",
	// "WonderSwan",
	"X-Tracker",                         /* DMF */
	"YM",                                /* YM */
	// "YMST",                           /* <- Furher development of YM fileformat, unable to find documentation. UADE can play these via YM-2149 / MYST */
	// "Zoundmonitor",
};

struct modland_com_ocpdir_t
{
	struct ocpdir_t head;
	char *dirname; /* survives main database changes */
};

static void modland_com_ocpdir_ref (struct ocpdir_t *d)
{
	struct modland_com_ocpdir_t *self = (struct modland_com_ocpdir_t *)d;
	self->head.refcount++;
}

static void modland_com_ocpdir_unref (struct ocpdir_t *d)
{
	struct modland_com_ocpdir_t *self = (struct modland_com_ocpdir_t *)d;
	if (!--self->head.refcount)
	{
		if (self->head.parent)
		{
			self->head.parent->unref (self->head.parent);
			self->head.parent = 0;
		}
		dirdbUnref (self->head.dirdb_ref, dirdb_use_dir);
		free (self->dirname);
		free (self);
	}
}

struct modland_com_readdir_t
{
	struct modland_com_ocpdir_t *dir;
	int isroot;
	int isadlib;
	unsigned int fileoffset;
	unsigned int diroffset;
	unsigned int dirmax; // used by flatdir
	unsigned int direxact;
	unsigned int dirnamelength; /* cache for strncmp */
	int flatdir;
	int sentdevs;
	void(*callback_file)(void *token, struct ocpfile_t *);
	void(*callback_dir )(void *token, struct ocpdir_t *);
	void *token;
};

static ocpdirhandle_pt modland_com_ocpdir_readdir_start_common (struct ocpdir_t *d,
                                                                void(*callback_file)(void *token, struct ocpfile_t *),
                                                                void(*callback_dir )(void *token, struct ocpdir_t *),
                                                                void *token,
                                                                int flatdir)
{
	struct modland_com_ocpdir_t *self = (struct modland_com_ocpdir_t *)d;
	struct modland_com_readdir_t *iter = calloc (sizeof (*iter), 1);

	if (!iter)
	{
		return 0;
	}
	iter->dir = self;
	iter->dirnamelength = strlen (self->dirname);
	iter->callback_file = callback_file;
	iter->callback_dir = callback_dir;
	iter->token = token;
	iter->flatdir = flatdir;

	/* perform binary search for the exact directory entry */
	if (modland_com.database.direntries_n)
	{
		unsigned long start = 0;
		unsigned long stop = modland_com.database.direntries_n;

		iter->isroot = !strcasecmp (self->dirname, "");
		iter->isadlib = !strcasecmp (self->dirname, "Ad Lib");

		if (!strcmp (modland_com.database.direntries[start], self->dirname))
		{ /* we only need to strcmp the start entry once, after this start can only be transfered from "half", which is strcompared */
			iter->direxact = start;
		} else {
			while (1)
			{
				unsigned long distance = stop - start;
				unsigned long half;
				int res;

				if (distance <= 1) /* we can't have a direct hit, since entry does not match start which is the only possible entry in the list */
				{
					iter->direxact = UINT_MAX;
					break;
				}

				half = (stop - start) / 2 + start;

				res = modland_com_dir_strcmp (modland_com.database.direntries[half], self->dirname);
				if (!res)
				{
					iter->direxact = half;
					break;
				} else if (res > 0)
				{
					stop = half;
				} else {
					start = half;
				}
			}
		}
	} else {
		iter->direxact = UINT_MAX;
	}

	iter->diroffset = iter->direxact;
	if (iter->diroffset != UINT_MAX)
	{
		if (flatdir)
		{ /* need to find dirlimit */
			iter->dirmax = iter->diroffset + 1;
			while ((iter->dirmax < modland_com.database.direntries_n) &&
			       (!strncmp (modland_com.database.direntries[iter->dirmax], self->dirname, iter->dirnamelength)) &&
			       (modland_com.database.direntries[iter->dirmax][iter->dirnamelength]=='/'))
			{
				iter->dirmax++;
			}
		} else { /* need to skip first directory, it is "." */
			iter->diroffset++;
			if ((iter->diroffset >= modland_com.database.direntries_n) ||
			    strncmp (modland_com.database.direntries[iter->diroffset], self->dirname, iter->dirnamelength))
			{
				iter->diroffset = UINT_MAX;
			}
		}
	}

	/* perform binary search for the first file with the correct dirindex */
	if (iter->direxact != UINT_MAX)
	{
		unsigned long start = 0;
		unsigned long stop = modland_com.database.fileentries_n;

		while (1)
		{
			unsigned long distance = stop - start;
			unsigned long half = distance / 2 + start;

			if (distance <= 1)
			{
				iter->fileoffset = start;
				break;
			}

			if (modland_com.database.fileentries[half].dirindex == iter->direxact)
			{
				if (modland_com.database.fileentries[half-1].dirindex >= iter->direxact)
				{
					stop = half;
				} else {
					start = half;
				}
			} else {
				if (modland_com.database.fileentries[half].dirindex >= iter->direxact)
				{
					stop = half;
				} else {
					start = half;
				}
			}
		}
		/* if fileoffset did not find exact match, it will point to one or more directories too early */
		while ((iter->fileoffset < modland_com.database.fileentries_n) && (modland_com.database.fileentries[iter->fileoffset].dirindex < iter->direxact))
		{
			iter->fileoffset++;
		}
	} else {
		iter->fileoffset=UINT_MAX;
	}

	self->head.ref (&self->head);
	return iter;
}

static ocpdirhandle_pt modland_com_ocpdir_readdir_start (struct ocpdir_t *d,
                                                         void(*callback_file)(void *token, struct ocpfile_t *),
                                                         void(*callback_dir )(void *token, struct ocpdir_t *),
                                                         void *token)
{
	return modland_com_ocpdir_readdir_start_common (d, callback_file, callback_dir, token, 0);
}

static ocpdirhandle_pt modland_com_ocpdir_readflatdir_start (struct ocpdir_t *d,
                                                             void(*callback_file)(void *token, struct ocpfile_t *),
                                                             void *token)
{
	return modland_com_ocpdir_readdir_start_common (d, callback_file, 0, token, 1);
}

static void modland_com_ocpdir_readdir_cancel (ocpdirhandle_pt v)
{
	struct modland_com_readdir_t *iter = (struct modland_com_readdir_t *)v;
	iter->dir->head.unref (&iter->dir->head);
	free (iter);
}

static int modland_com_ocpdir_readdir_iterate (ocpdirhandle_pt v)
{
	struct modland_com_readdir_t *iter = (struct modland_com_readdir_t *)v;

	if ((!iter->sentdevs) &&
	    (!iter->dirnamelength))
	{
		iter->callback_file (iter->token, modland_com.modland_com_setup);
		iter->sentdevs = 1;
	}

	if (iter->flatdir)
	{
		int n = 0;

		while (n < 1000)
		{
			struct ocpfile_t *f;

			if ((iter->fileoffset >= modland_com.database.fileentries_n) ||
			    (modland_com.database.fileentries[iter->fileoffset].dirindex >= iter->dirmax))
			{
				iter->fileoffset = UINT_MAX;
				return 0;
			}

			f = modland_com_file_spawn (&iter->dir->head, iter->fileoffset);
			if (f)
			{
				iter->callback_file (iter->token, f);
				f->unref (f);
			}
			iter->fileoffset++;
			n++;
		}
		return 1;
	}

	if (iter->diroffset != UINT_MAX)
	{
		if ((iter->diroffset >= modland_com.database.direntries_n) ||
		    (strncmp (modland_com.database.direntries[iter->diroffset], iter->dir->dirname, iter->dirnamelength)) ||
		    (iter->dirnamelength && (modland_com.database.direntries[iter->diroffset][iter->dirnamelength] != '/')))
		{
			iter->diroffset = UINT_MAX;
		} else {
			char *ptr = strchr (modland_com.database.direntries[iter->diroffset] + iter->dirnamelength + (iter->dirnamelength?1:0), '/');
			unsigned long len;
			unsigned long next;
			if (ptr)
			{ /* this should not be reachable */
				iter->diroffset++;
				return 1;
			}
			len = strlen (modland_com.database.direntries[iter->diroffset]);
			if (iter->isroot)
			{
				const int max = sizeof (known_root_directories) / sizeof (known_root_directories[0]);
				int i;
				for (i=0; i < max; i++)
				{
					if (!strcasecmp (known_root_directories[i], modland_com.database.direntries[iter->diroffset]))
					{
						break;
					}
				}
				if (i == max)
				{
					goto skipdir;
				}
			} else if (iter->isadlib)
			{
				const int max = sizeof (known_adlib_directories) / sizeof (known_adlib_directories[0]);
				int i;
				for (i=0; i < max; i++)
				{
					if (!strcasecmp (known_adlib_directories[i], modland_com.database.direntries[iter->diroffset] + 7 /* strlen("Ad Lib/") */))
					{
						break;
					}
				}
				if (i == max)
				{
					goto skipdir;
				}
			}
			{
				struct modland_com_ocpdir_t *d = calloc (sizeof (*d), 1);
				if (d)
				{
					iter->dir->head.ref (&iter->dir->head);
					ocpdir_t_fill (
						&d->head,
						modland_com_ocpdir_ref,
						modland_com_ocpdir_unref,
						&iter->dir->head, /* parent */
						modland_com_ocpdir_readdir_start,
						modland_com_ocpdir_readflatdir_start,
						modland_com_ocpdir_readdir_cancel,
						modland_com_ocpdir_readdir_iterate,
						0, /* readdir_dir */
						0, /* readdir_file */
						0, /* charset_override_API */
						dirdbFindAndRef (iter->dir->head.dirdb_ref, modland_com.database.direntries[iter->diroffset] + iter->dirnamelength + (iter->dirnamelength?1:0), dirdb_use_dir),
						1, /* refcount */
						0, /* is_archive */
						0, /* is_playlist */
						0  /* compression */
					);
					d->dirname = strdup (modland_com.database.direntries[iter->diroffset]);
					if (d->dirname)
					{
						iter->callback_dir (iter->token, &d->head);
					}
					modland_com_ocpdir_unref (&d->head);
				}
			}

skipdir:
			next = iter->diroffset;
			while (1)
			{
				next++;
				if (next >= modland_com.database.direntries_n)
				{
					next = UINT_MAX;
					break;
				}
				if ((strncmp (modland_com.database.direntries[iter->diroffset], modland_com.database.direntries[next], len)) || (modland_com.database.direntries[next][len] != '/'))
				{
					break;
				}
			}
			iter->diroffset = next;
			return 1;
		}
	}

	if (iter->fileoffset != UINT_MAX)
	{
		struct ocpfile_t *f;

		if ((iter->fileoffset >= modland_com.database.fileentries_n) ||
		    (modland_com.database.fileentries[iter->fileoffset].dirindex != iter->direxact))
		{ /* no more files */
			iter->fileoffset = UINT_MAX;
			return 1;
		}

		f = modland_com_file_spawn (&iter->dir->head, iter->fileoffset);
		if (f)
		{
			iter->callback_file (iter->token, f);
			f->unref (f);
		}

		iter->fileoffset++;
		return 1;
	}

	return 0;
}

static struct ocpdir_t *modland_com_init_root(void)
{
	struct modland_com_ocpdir_t *retval = calloc (sizeof (*retval), 1);
	if (!retval)
	{
		return 0;
	}
	ocpdir_t_fill (
		&retval->head,
		modland_com_ocpdir_ref,
		modland_com_ocpdir_unref,
		0, /* parent */
		modland_com_ocpdir_readdir_start,
		modland_com_ocpdir_readflatdir_start,
		modland_com_ocpdir_readdir_cancel,
		modland_com_ocpdir_readdir_iterate,
		0, /* readdir_dir */
		0, /* readdir_file */
		0, /* charset_override_API */
		dirdbFindAndRef (DIRDB_NOPARENT, "modland.com:", dirdb_use_dir),
		1, /* refcount */
		0, /* is_archive */
		0, /* is_playlist */
		0  /* compression */
	);
	retval->dirname = strdup ("");
	if (!retval->dirname)
	{
		modland_com_ocpdir_unref (&retval->head);
		return 0;
	}
	return &retval->head;
}
