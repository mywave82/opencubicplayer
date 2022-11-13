#ifndef __PLAYSID_LIBSIDPLAYFP_API_H
#define __PLAYSID_LIBSIDPLAYFP_API_H

extern "C"
{
struct dmDrive;
struct dirdbAPI_t;
struct configAPI_t;
}

#include <sidplayfp/SidTune.h>
#include <c64/c64.h>
#include <player.h>
#include <sidplayfp/SidConfig.h>
#include <sidplayfp/SidTuneInfo.h>

namespace libsidplayfp
{
	const char* VICIImodel_ToString(MOS656X::model_t model);
	const char* sidModel_ToString(SidConfig::sid_model_t model);
	const char* tuneInfo_sidModel_toString (const SidTuneInfo::model_t model);
	const char* tuneInfo_compatibility_toString (const SidTuneInfo::compatibility_t compatibility);
	const char* tuneInfo_clockSpeed_toString (const SidTuneInfo::clock_t clock);

	class ConsolePlayer
	{
	public:
		typedef enum
		{
			playerError,
			playerStopped,
			playerRunning,
		} player_state_t;

		ConsolePlayer (const unsigned int rate, const struct configAPI_t *configAPI, const struct dirdbAPI_t *dirdbAPI, struct dmDrive *dmFile);
		virtual ~ConsolePlayer (void);

		bool load (const uint_least8_t* sourceBuffer, uint_least32_t bufferLen);

		const SidTuneInfo* getInfo() const;

		unsigned int currenttrack (void) { return selected_track; }

		bool nexttrack (void) { return selecttrack (selected_track + 1); }
		bool selecttrack (unsigned int track); // void sidpStartSong(char sng);
		bool prevtrack (void) { return selecttrack (selected_track - 1); }

		/* targetBuffer:
			Mixer left, right (interleaved)
		   rawSamples:
			index 0: SID chip 1, master, chan1, chan2, chan3 (interleaved)
			index 1: SID chip 2, master, chan1, chan2, chan3 (interleaved, if chip 2 present)
			index 2: SID chip 3, master, chan1, chan2, chan3 (interleaved, if chip 3 present)
		*/
		bool iterateaudio (int16_t *targetBuffer, uint_least32_t count, std::vector<int16_t *> *rawSamples);
		bool getSidStatus(unsigned int sidNum, uint8_t registers[32], uint8_t &volume_a, uint8_t &volume_b, uint8_t &volume_c) { return sidplayer.getSidStatus (sidNum, registers, volume_a, volume_b, volume_c); }

		int getSidCount (void) { return sidplayer.getSidCount(); }

		const char *kernalDesc(void);
		const char *basicDesc(void);
		const char *chargenDesc(void);

		const float getMainCpuSpeed(void) { return sidplayer.getMainCpuSpeed(); }
		const MOS656X::model_t getVICIImodel(void);
		const char *getCIAmodel(void);
		const SidConfig::sid_model_t getSIDmodel(int i) { return sidplayer.getSidModel(i); }
		const uint16_t getSIDaddr(int i) { return sidplayer.getSidAddress(i); }
		const char *getTuneStatusString(void) { return m_tune.statusString(); }
		const SidTuneInfo::clock_t getTuneInfoClockSpeed(void);
		void close (void);
		c64::model_t c64Model (void) { return sidplayer.getModel(); }
		void mute(int chan, bool mute);

	private:
		SidConfig          m_engCfg;
		SidTune            m_tune;
		player_state_t     m_state;
		uint_least16_t selected_track;
		libsidplayfp::Player &sidplayer;

		struct m_filter_t
		{
			// Filter parameter for reSID
			double         bias;
			// Filter parameters for reSIDfp
			double         filterCurve6581;
			double         filterCurve8580;

			bool           enabled;
		} m_filter;

		void clearSidEmu (void);
		bool createSidEmu (const struct configAPI_t *configAPI);

		bool open (void);

		uint8_t* loadRom(uint32_t dirdb_ref, const int size, const struct dirdbAPI_t *dirdbAPI);
	};
}

#endif
