/* OpenCP Module Player
 * copyright (c) 2020-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Glue logic for using libsidplayfp playback engine
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

#include "libsidplayfp-api.h"
#include <iostream>
#include <fstream>
#include <vector>
#include "sidplayfp/sidbuilder.h"
#include "sidplayfp/SidInfo.h"
#include "sidplayfp/SidTuneInfo.h"
#include "builders/residfp-builder/residfp.h"
#include "builders/residfpII-builder/residfpII.h"

extern "C"
{
#ifdef DPACKAGE_NAME
#undef DPACKAGE_NAME
#endif

#ifdef VERSION
#undef VERSION
#endif

#ifdef PACKAGE_VERSION
#undef PACKAGE_VERSION
#endif

#include "../config.h"
#include <stdio.h>
#include <unistd.h>
#include "types.h"
#include "boot/psetting.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem-drive.h"
#include "filesel/filesystem.h"
}

namespace libsidplayfp
{
#if 0
	const char* c64Model_ToString(c64::model_t model)
	{
		switch (model)
		{
			default:
			case c64::model_t::PAL_B:      ///< PAL C64
				return "PAL-B";
			case c64::model_t::NTSC_M:     ///< NTSC C64
				return "NTSC-M";
			case c64::model_t::OLD_NTSC_M: ///< Old NTSC C64
				return "(old)NTSC-M";
			case c64::model_t::PAL_N:      ///< C64 Drean
				return "PAL-N";
			case c64::model_t::PAL_M:      ///< C64 Brasil
				return "PAL-M";
		}
	}
#endif

	const char* VICIImodel_ToString(MOS656X::model_t model)
	{
		switch (model)
		{
			case MOS656X::model_t::MOS6567R56A:
				return "MOS6567R56A NTSC-M (old)";
			case MOS656X::model_t::MOS6567R8:
				return "MOS6567R8 NTSC-M";
			case MOS656X::model_t::MOS6569:
				return "MOS6569 PAL-B";
			case MOS656X::model_t::MOS6572:
				return "MOS6572 PAL-N";
			case MOS656X::model_t::MOS6573:
				return "MOS6573 PAL-M";
			default:
				return "MOS65xx ??";
		}
	}

	const char* sidModel_ToString(SidConfig::sid_model_t model)
	{
		switch (model)
		{
			default:
			case SidConfig::sid_model_t::MOS6581: return "MOS6581";
			case SidConfig::sid_model_t::MOS8580: return "MOS8580";
		}
	}

	const char* tuneInfo_sidModel_toString (const SidTuneInfo::model_t model)
	{
		switch (model)
		{
			default:
			case SidTuneInfo::model_t::SIDMODEL_UNKNOWN: return "unknown";
			case SidTuneInfo::model_t::SIDMODEL_6581: return "MOS6581";
			case SidTuneInfo::model_t::SIDMODEL_8580: return "MOS8580";
			case SidTuneInfo::model_t::SIDMODEL_ANY: return "any";
		}
	}

	const char* tuneInfo_compatibility_toString (const SidTuneInfo::compatibility_t compatibility)
	{
		switch (compatibility)
		{
			default: return "unknown";
			case SidTuneInfo::compatibility_t::COMPATIBILITY_C64: return "C64";
			case SidTuneInfo::compatibility_t::COMPATIBILITY_PSID: return "PSID specific";
			case SidTuneInfo::compatibility_t::COMPATIBILITY_R64: return "Real C64 only";
			case SidTuneInfo::compatibility_t::COMPATIBILITY_BASIC: return "C64 Basic ROM";
		}
	}

	const char* tuneInfo_clockSpeed_toString (const SidTuneInfo::clock_t clock)
	{
		switch (clock)
		{
			default:
			case SidTuneInfo::clock_t::CLOCK_UNKNOWN: return "unknown";
			case SidTuneInfo::clock_t::CLOCK_PAL: return "PAL (50Hz)";
			case SidTuneInfo::clock_t::CLOCK_NTSC: return "NTSC (60Hz)";
			case SidTuneInfo::clock_t::CLOCK_ANY: return "ANY";
		}
	}

	ConsolePlayer::ConsolePlayer (const unsigned int rate, const struct configAPI_t *configAPI, const struct dirdbAPI_t *dirdbAPI, struct dmDrive *dmFile) :
		m_tune(nullptr),
		m_state(playerStopped),
		selected_track(0),
		sidplayer(*(new libsidplayfp::Player))
	{
		char *endptr;

		m_engCfg = sidplayer.config();
		m_engCfg.powerOnDelay = 10000;

		const char *defaultC64model = configAPI->GetProfileString("libsidplayfp", "defaultC64", "PAL");
		if (!strcasecmp(defaultC64model, "PAL"))
		{
			//fprintf (stderr, "defaultC64=PAL\n");
			m_engCfg.defaultC64Model = SidConfig::PAL;
		} else if (!strcasecmp(defaultC64model, "NTSC"))
		{
			//fprintf (stderr, "defaultC64=NTSC\n");
			m_engCfg.defaultC64Model = SidConfig::NTSC;
		} else if ((!strcasecmp(defaultC64model, "OLD-NTSC")) ||
		           (!strcasecmp(defaultC64model, "OLD_NTSC")) ||
		           (!strcasecmp(defaultC64model, "OLDNTSC")))
		{
			//fprintf (stderr, "defaultC64=OLD-NTSC\n");
			m_engCfg.defaultC64Model = SidConfig::OLD_NTSC;
		} else if (!strcasecmp(defaultC64model, "DREAN"))
		{
			//fprintf (stderr, "defaultC64=DREAN\n");
			m_engCfg.defaultC64Model = SidConfig::DREAN;
		} else if ((!strcasecmp(defaultC64model, "PAL-M")) ||
		           (!strcasecmp(defaultC64model, "PAL_M")) ||
		           (!strcasecmp(defaultC64model, "PALM")))
		{
			//fprintf (stderr, "defaultC64=PAL-M\n");
			m_engCfg.defaultC64Model = SidConfig::PAL_M;
		} else {
			fprintf (stderr, "[libsidplayfp]\n  defaultC64=invalid.... defaulting to PAL\n");
			m_engCfg.defaultC64Model = SidConfig::PAL;
		}

		m_engCfg.forceC64Model = configAPI->GetProfileBool("libsidplayfp", "forceC64", 0, 0);
		//fprintf (stderr, "forceC64Model=%d\n", m_engCfg.forceC64Model);

		const char *defaultSIDmodel = configAPI->GetProfileString("libsidplayfp", "defaultSID", "MOS6581");
		if (!strcasecmp(defaultSIDmodel, "MOS6581"))
		{
			//fprintf (stderr, "defaultSID=MOS6581\n");
			m_engCfg.defaultSidModel = SidConfig::MOS6581;
		} else if (!strcasecmp(defaultSIDmodel, "MOS8580"))
		{
			//fprintf (stderr, "defaultSID=MOS8580\n");
			m_engCfg.defaultSidModel = SidConfig::MOS8580;
		} else {
			fprintf (stderr, "[libsidplayfp]\n  defaultSID=invalid.. defaulting to MOS6581\n");
			m_engCfg.defaultSidModel = SidConfig::MOS6581;
		}

		m_engCfg.forceSidModel = configAPI->GetProfileBool("libsidplayfp", "forceSID", 0, 0);
		//fprintf (stderr, "forceSIDModel=%d\n", m_engCfg.forceSidModel);

		const char *CIAmodel = configAPI->GetProfileString("libsidplayfp", "CIA", "MOS6526");
		if (!strcasecmp(CIAmodel, "MOS6526"))
		{
			//fprintf (stderr, "CIA=MOS6526\n");
			m_engCfg.ciaModel = SidConfig::MOS6526;
		} else if (!strcasecmp(CIAmodel, "MOS6526W4485"))
		{
			//fprintf (stderr, "CIA=MOS6526W4485\n");
			m_engCfg.ciaModel = SidConfig::MOS6526W4485;
		} else if (!strcasecmp(CIAmodel, "MOS8521"))
		{
			//fprintf (stderr, "CIA=MOS8521\n");
			m_engCfg.ciaModel = SidConfig::MOS8521;
		} else {
			fprintf (stderr, "[libsidplayfp]\n  CIA=invalid... defaulting to MOS6525\n");
			m_engCfg.ciaModel = SidConfig::MOS6526;
		}

		m_engCfg.frequency = rate;

		m_engCfg.playback = SidConfig::STEREO;

		m_filter.enabled = configAPI->GetProfileBool("libsidplayfp", "filter", 1, 0);
		//fprintf (stderr, "filter=%d\n", m_filter.enabled);

		const char *bias = configAPI->GetProfileString("libsidplayfp", "filterbias", "0.0");
		m_filter.bias = strtod(bias, &endptr);
		if ((*endptr != 0) || (bias == endptr))
		{
			fprintf (stderr, "[libsidplayfp]\n  filterbias=invalid... defaulting to 0.0\n");
			m_filter.bias = 0.5;
		} else {
			//fprintf (stderr, "filterbias=%lf\n", m_filter.bias);
		}

		const char *curve6581 = configAPI->GetProfileString("libsidplayfp", "filtercurve6581", "0.5");
		m_filter.filterCurve6581 = strtod(curve6581, &endptr);
		if ((*endptr != 0) || (curve6581 == endptr))
		{
			fprintf (stderr, "[libsidplayfp]\n  filtercurve6581=invalid... defaulting to 0.5\n");
			m_filter.filterCurve6581=0.5;
		} else {
			//fprintf (stderr, "filtercurve6581=%lf\n", m_filter.filterCurve6581);
		}

		const char *range6581 = configAPI->GetProfileString("libsidplayfp", "filterrange6581", "0.5");
		m_filter.filterRange6581 = strtod(range6581, &endptr);
		if ((*endptr != 0) || (range6581 == endptr))
		{
			fprintf (stderr, "[libsidplayfp]\n  filterrange6581=invalid... defaulting to 0.5\n");
			m_filter.filterRange6581=0.5;
		} else {
			//fprintf (stderr, "filterrange6581=%lf\n", m_filter.filterRange6581);
		}

		const char *curve8580 = configAPI->GetProfileString("libsidplayfp", "filtercurve8580", "0.5");
		m_filter.filterCurve8580 = strtod(curve8580, &endptr);
		if ((*endptr != 0) || (curve8580 == endptr))
		{
			fprintf (stderr, "[libsidplayfp]\n  filtercurve8580=invalid... defaulting to 0.5\n");
			m_filter.filterCurve8580=0.5;
		} else {
			//fprintf (stderr, "filtercurve8580=%lf\n", m_filter.filterCurve8580);
		}

		const char *CWS = configAPI->GetProfileString("libsidplayfp", "combinedwaveforms", "Average");
		if (!strcasecmp(CWS, "Weak"))
		{
			m_filter.combinedWaveforms = SidConfig::WEAK;
		} else if (!strcasecmp(CWS, "Strong"))
		{
			m_filter.combinedWaveforms = SidConfig::STRONG;
		} else if (!strcasecmp(CWS, "Average"))
		{
			m_filter.combinedWaveforms = SidConfig::AVERAGE;
		} else {
			fprintf (stderr, "[libsidplayfp]\n  combinedwaveforms=invalid... defaulting to Average\n");
			m_filter.combinedWaveforms = SidConfig::AVERAGE;
		}

		m_engCfg.digiBoost = configAPI->GetProfileBool("libsidplayfp", "digiboost", 0, 0);
		//fprintf (stderr, "digiboost=%d\n", m_engCfg.digiBoost);

		if (!createSidEmu (configAPI))
		{
			#warning handle this failure
		}

		const char *kernal_string  = configAPI->GetProfileString("libsidplayfp", "kernal",  "KERNAL.ROM");
		const char *basic_string   = configAPI->GetProfileString("libsidplayfp", "basic",   "BASIC.ROM");
		const char *chargen_string = configAPI->GetProfileString("libsidplayfp", "chargen", "CHARGEN.ROM");

		uint32_t dirdb_base = configAPI->DataHomeDir->dirdb_ref; /* should be the parent_dir of the file you want to load */
		uint32_t kernal_ref;
		uint32_t basic_ref;
		uint32_t chargen_ref;

#ifdef _WIN32
		kernal_ref  = dirdbAPI->ResolvePathWithBaseAndRef (dirdb_base, kernal_string,  DIRDB_RESOLVE_DRIVE | DIRDB_RESOLVE_TILDE_HOME | DIRDB_RESOLVE_WINDOWS_SLASH, dirdb_use_file);
		basic_ref   = dirdbAPI->ResolvePathWithBaseAndRef (dirdb_base, basic_string,   DIRDB_RESOLVE_DRIVE | DIRDB_RESOLVE_TILDE_HOME | DIRDB_RESOLVE_WINDOWS_SLASH, dirdb_use_file);
		chargen_ref = dirdbAPI->ResolvePathWithBaseAndRef (dirdb_base, chargen_string, DIRDB_RESOLVE_DRIVE | DIRDB_RESOLVE_TILDE_HOME | DIRDB_RESOLVE_WINDOWS_SLASH, dirdb_use_file);
#else
		kernal_ref  = dirdbAPI->ResolvePathWithBaseAndRef (dirdb_base, kernal_string,  DIRDB_RESOLVE_DRIVE | DIRDB_RESOLVE_TILDE_HOME, dirdb_use_file);
		basic_ref   = dirdbAPI->ResolvePathWithBaseAndRef (dirdb_base, basic_string,   DIRDB_RESOLVE_DRIVE | DIRDB_RESOLVE_TILDE_HOME, dirdb_use_file);
		chargen_ref = dirdbAPI->ResolvePathWithBaseAndRef (dirdb_base, chargen_string, DIRDB_RESOLVE_DRIVE | DIRDB_RESOLVE_TILDE_HOME, dirdb_use_file);
#endif
		uint8_t *kernalRom  = loadRom ( kernal_ref, 8192, dirdbAPI);
		uint8_t *basicRom   = loadRom (  basic_ref, 8192, dirdbAPI);
		uint8_t *chargenRom = loadRom (chargen_ref, 4096, dirdbAPI);

		dirdbAPI->Unref ( kernal_ref, dirdb_use_file);
		dirdbAPI->Unref (  basic_ref, dirdb_use_file);
		dirdbAPI->Unref (chargen_ref, dirdb_use_file);

		sidplayer.setKernal ( kernalRom);
		sidplayer.setBasic  (  basicRom);
		sidplayer.setChargen(chargenRom);

		delete [] kernalRom;
		delete [] basicRom;
		delete [] chargenRom;
	}

	ConsolePlayer::~ConsolePlayer (void)
	{
		close ();
		delete &sidplayer;
	}

	uint8_t* ConsolePlayer::loadRom(uint32_t dirdb_ref, const int size, const struct dirdbAPI_t *dirdbAPI)
	{
		char *romPath = 0;
#ifdef _WIN32
		dirdbAPI->GetFullname_malloc (dirdb_ref, &romPath, DIRDB_FULLNAME_DRIVE | DIRDB_FULLNAME_BACKSLASH);
#else
		dirdbAPI->GetFullname_malloc (dirdb_ref, &romPath, DIRDB_FULLNAME_NODRIVE);
#endif
		std::ifstream is(romPath, std::ios::binary);

		if (is.is_open())
		{
			try
			{
				uint8_t *buffer = new uint8_t[size];

				is.read((char*)buffer, size);
				if (!is.fail())
				{
					is.close();
					return buffer;
				}
				delete [] buffer;
			}
			catch (std::bad_alloc const &ba) {}
		}

		free (romPath);

		return nullptr;
	}

	void ConsolePlayer::clearSidEmu (void)
	{
		if (m_engCfg.sidEmulation)
		{
			sidbuilder *builder   = m_engCfg.sidEmulation;
			m_engCfg.sidEmulation = nullptr;
			sidplayer.config(m_engCfg);
			delete builder;
		}
	}


	// Create the sid emulation
	bool ConsolePlayer::createSidEmu (const struct configAPI_t *configAPI /*SIDEMUS emu*/)
	{
		// Remove old driver and emulation - should be no-op
		clearSidEmu ();

		int use_residfp = !strcmp(configAPI->GetProfileString("libsidplayfp", "emulator", "residfp"), "residfp"); // true for residfp, false for residfpII

		//fprintf (stderr, "use_residfp=%d\n", use_residfp);

		try
		{
			if (!use_residfp)
			{
				m_residfp = 2;
				ReSIDfpIIBuilder *rs = new ReSIDfpIIBuilder( "ReSIDFPII" );
				m_engCfg.sidEmulation = rs;
				rs->filter6581Curve(m_filter.filterCurve6581);
				rs->filter6581Range(m_filter.filterRange6581);
				rs->filter8580Curve(m_filter.filterCurve8580);
				rs->combinedWaveformsStrength(m_filter.combinedWaveforms);
			} else {
				m_residfp = 1;
				ReSIDfpBuilder *rs = new ReSIDfpBuilder( "ReSIDFP" );
				m_engCfg.sidEmulation = rs;
				rs->filter6581Curve(m_filter.filterCurve6581);
				rs->filter6581Range(m_filter.filterRange6581);
				rs->filter8580Curve(m_filter.filterCurve8580);
				rs->combinedWaveformsStrength(m_filter.combinedWaveforms);

			}
		}
		catch (std::bad_alloc const &ba) {}

		if (!m_engCfg.sidEmulation)
		{
			fprintf (stderr, "sidplayfp: not enough memory for creating virtual SID chips?\n");
			return false;
		} else {
			/* set up SID filter. HardSID just ignores call with def. */
			sidplayer.filter(0, m_filter.enabled);
			sidplayer.filter(1, m_filter.enabled);
			sidplayer.filter(2, m_filter.enabled);
		}

		return true;

	createSidEmu_error:
		fprintf (stderr, "sidplayfp: creating SIDs failed: %s\n", m_engCfg.sidEmulation->error ());
		delete m_engCfg.sidEmulation;
		m_engCfg.sidEmulation = nullptr;
		return false;
	}

	bool ConsolePlayer::load (const uint_least8_t* sourceBuffer, uint_least32_t bufferLen)
	{
		m_tune.read (sourceBuffer, bufferLen);

		if (!m_tune.getStatus())
		{
			fprintf (stderr, "sidplayfp: Failed to load SID file: %s\n", m_tune.statusString());
			return false;
		}

		if (!sidplayer.config (m_engCfg))
		{
			fprintf (stderr, "sidplayfp: Failed to configure engine (1): %s\n", sidplayer.error ());
			return false;
		}

		bool retval = open();

		return retval;
	}


	bool ConsolePlayer::open (void)
	{
		selected_track = m_tune.selectSong(selected_track);

		if (!sidplayer.load (&m_tune))
		{
			fprintf (stderr, "sidplayfp: Failed to load tune into engine: %s\n", sidplayer.error());
			return false;
		}

		if (!sidplayer.config (m_engCfg))
		{
			fprintf (stderr, "sidplayfp: Failed to configure engine (2): %s\n", sidplayer.error ());
			return false;
		}

		// First now we know number of SID chips. Since sidplayer.load() resets the buffers, so need to perform initMixer here, and not in load()
		sidplayer.initMixer(true); // false = mono, true = stereo

		m_state = playerRunning;

		return true;
	}

	bool ConsolePlayer::selecttrack (unsigned int track)
	{
		if (m_state == playerRunning)
		{
			selected_track = track;

			if ((selected_track < 1) || (selected_track > m_tune.getInfo()->songs()))
			{
				selected_track = 1;
			}
//			sidplayer.stop ();
			return open ();
		}
		return false;
	}


	uint_least32_t ConsolePlayer::iterateaudio (int16_t *targetBuffer, uint_least32_t count, uint_least32_t cycles, std::vector<int16_t *> *rawSamples)
	{
		if (m_state == playerRunning)
		{
			uint_least32_t samples_available = sidplayer.play (cycles);
			if (samples_available > count)
			{
				fprintf (stderr, "[sidplayer] Warning: sample count %u > samples available %u\n", (unsigned int)samples_available, (unsigned int)count);
				samples_available = count;
			}
			return sidplayer.mix(targetBuffer, samples_available, rawSamples);
		}
		return 0;
	}

	void ConsolePlayer::close (void)
	{
		if (m_state != playerStopped)
		{
			//sidplayer.stop ();
		}
		m_state = playerStopped;
		clearSidEmu ();
		sidplayer.load (nullptr);
		sidplayer.config (m_engCfg);
	}

	const SidTuneInfo* ConsolePlayer::getInfo() const
	{
		return m_tune.getInfo();
	}


	void ConsolePlayer::mute(int chan, bool mute)
	{
		sidplayer.mute (chan / 3, chan % 3, mute);
	}

	void ConsolePlayer::SetFilter(bool enable)
	{
		const SidConfig &config = sidplayer.config ();
		sidplayer.filter(0, enable);
		sidplayer.filter(1, enable);
		sidplayer.filter(2, enable);
	}

	void ConsolePlayer::SetFilterCurve6581 (double v)
	{
		if (v > 1.0) v = 1.0;
		if (v < 0.0) v = 0.0;

		if (m_residfp == 1)
		{
			ReSIDfpBuilder *rs = dynamic_cast<ReSIDfpBuilder *>(m_engCfg.sidEmulation);
			if (rs)
			{
				rs->filter6581Curve (v);
			}
		} else if (m_residfp == 2)
		{
			ReSIDfpIIBuilder *rs = dynamic_cast<ReSIDfpIIBuilder *>(m_engCfg.sidEmulation);
			if (rs)
			{
				rs->filter6581Curve (v);
			}
		}
	}

	void ConsolePlayer::SetFilterRange6581 (double v)
	{
		if (v > 1.0) v = 1.0;
		if (v < 0.0) v = 0.0;

		if (m_residfp == 1)
		{
			ReSIDfpBuilder *rs = dynamic_cast<ReSIDfpBuilder *>(m_engCfg.sidEmulation);
			if (rs)
			{
				rs->filter6581Range (v);
			}
		} else if (m_residfp == 2)
		{
			ReSIDfpIIBuilder *rs = dynamic_cast<ReSIDfpIIBuilder *>(m_engCfg.sidEmulation);
			if (rs)
			{
				rs->filter6581Range (v);
			}
		}
	}

	void ConsolePlayer::SetFilterCurve8580 (double v)
	{
		if (v > 1.0) v = 1.0;
		if (v < 0.0) v = 0.0;

		if (m_residfp == 1)
		{
			ReSIDfpBuilder *rs = dynamic_cast<ReSIDfpBuilder *>(m_engCfg.sidEmulation);
			if (rs)
			{
				rs->filter8580Curve (v);
			}
		} else if (m_residfp == 2)
		{
			ReSIDfpIIBuilder *rs = dynamic_cast<ReSIDfpIIBuilder *>(m_engCfg.sidEmulation);
			if (rs)
			{
				rs->filter8580Curve (v);
			}
		}
	}

	void ConsolePlayer::SetCombinedWaveformsStrength (int CWS)
	{
		if (CWS < 0) CWS = 0;
		if (CWS > 2) CWS = 2;

		if (m_residfp == 1)
		{
			ReSIDfpBuilder *rs = dynamic_cast<ReSIDfpBuilder *>(m_engCfg.sidEmulation);
			if (rs)
			{
				if (CWS == 0) rs->combinedWaveformsStrength (SidConfig::AVERAGE);
				if (CWS == 1) rs->combinedWaveformsStrength (SidConfig::WEAK);
				if (CWS == 2) rs->combinedWaveformsStrength (SidConfig::STRONG);
			}
		} else if (m_residfp == 2)
		{
			ReSIDfpIIBuilder *rs = dynamic_cast<ReSIDfpIIBuilder *>(m_engCfg.sidEmulation);
			if (rs)
			{
				if (CWS == 0) rs->combinedWaveformsStrength (SidConfig::AVERAGE);
				if (CWS == 1) rs->combinedWaveformsStrength (SidConfig::WEAK);
				if (CWS == 2) rs->combinedWaveformsStrength (SidConfig::STRONG);
			}
		}
	}

	const SidTuneInfo::clock_t ConsolePlayer::getTuneInfoClockSpeed(void)
	{
		const SidTuneInfo *tuneInfo = m_tune.getInfo();
		return tuneInfo->clockSpeed();
	}

	const char *ConsolePlayer::kernalDesc(void)
	{
		const SidInfo &info = sidplayer.info ();
		return info.kernalDesc();
	}

	const char *ConsolePlayer::basicDesc(void)
	{
		const SidInfo &info = sidplayer.info ();
		return info.basicDesc();
	}

	const char *ConsolePlayer::chargenDesc(void)
	{
		const SidInfo &info = sidplayer.info ();
		return info.chargenDesc();
	}

	const MOS656X::model_t ConsolePlayer::getVICIImodel(void)
	{
		return modelData[sidplayer.getModel()].vicModel;
	}

	const uint32_t ConsolePlayer::GetVICIICyclesPerFrame(void)
	{
		c64vic vic = sidplayer.getVIC();
		return vic.maxRasters *
		       vic.cyclesPerLine;
	}

	const char *ConsolePlayer::getCIAmodel(void)
	{
		const SidConfig &config = sidplayer.config ();
		return config.ciaModel ? "MOS8521" : "MOS6526";
	}
}
