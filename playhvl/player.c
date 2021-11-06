/* OpenCP Module Player
 * copyright (c) 2019 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Renderer routine. Heavily based on https://github.com/pete-gordon/hivelytracker/tree/master/hvl2wav
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
#include <math.h>
#include <string.h>
#include "types.h"
#include "player.h"

int32_t __attribute__ ((visibility ("internal"))) stereopan_left[5]  = { 128,  96,  64,  32,   0 };
int32_t __attribute__ ((visibility ("internal"))) stereopan_right[5] = { 128, 160, 193, 225, 255 };

/*
** Waves
*/
int8_t __attribute__ ((visibility ("internal"))) waves[WAVES_SIZE];
//static int16 waves2[WAVES_SIZE];

static const int16_t vib_tab[] =
{
	   0,   24,   49,   74,   97,  120,  141,  161,  180,  197,  212,  224,  235,  244,  250,  253,
	 255,  253,  250,  244,  235,  224,  212,  197,  180,  161,  141,  120,   97,   74,   49,   24,
	   0,  -24,  -49,  -74,  -97, -120, -141, -161, -180, -197, -212, -224, -235, -244, -250, -253,
	-255, -253, -250, -244, -235, -224, -212, -197, -180, -161, -141, -120,  -97,  -74,  -49,  -24
};

static const uint16_t period_tab[] =
{
	0x0000, 0x0D60, 0x0CA0, 0x0BE8, 0x0B40, 0x0A98, 0x0A00, 0x0970,
	0x08E8, 0x0868, 0x07F0, 0x0780, 0x0714, 0x06B0, 0x0650, 0x05F4,
	0x05A0, 0x054C, 0x0500, 0x04B8, 0x0474, 0x0434, 0x03F8, 0x03C0,
	0x038A, 0x0358, 0x0328, 0x02FA, 0x02D0, 0x02A6, 0x0280, 0x025C,
	0x023A, 0x021A, 0x01FC, 0x01E0, 0x01C5, 0x01AC, 0x0194, 0x017D,
	0x0168, 0x0153, 0x0140, 0x012E, 0x011D, 0x010D, 0x00FE, 0x00F0,
	0x00E2, 0x00D6, 0x00CA, 0x00BE, 0x00B4, 0x00AA, 0x00A0, 0x0097,
	0x008F, 0x0087, 0x007F, 0x0078, 0x0071
};

static uint32_t panning_left[256], panning_right[256];

static void hvl_GenPanningTables (void)
{
	uint32_t i;
	double aa, ab;

	// Sine based panning table
	aa = M_PI_2;                    // Quarter of the way through the sinewave == top peak
	ab = 0.0f;                      // Start of the climb from zero

	for( i=0; i<256; i++ )
	{
		panning_left[i]  = (uint32_t)(sin(aa)*255.0f);
		panning_right[i] = (uint32_t)(sin(ab)*255.0f);

		aa += (M_PI_2)/256.0f;
		ab += (M_PI_2)/256.0f;
	}
	panning_left[255] = 0;
	panning_right[0] = 0;
}

#include "hvl_genwaves.inc.c"

static void hvl_reset_some_stuff( struct hvl_tune *ht )
{
	uint32_t i;

	for ( i=0; i<MAX_CHANNELS; i++ )
	{
		ht->ht_Voices[i].vc_Instrument = 0;

		ht->ht_Voices[i].vc_SamplePos = 0;
		ht->ht_Voices[i].vc_Track = 0;
		ht->ht_Voices[i].vc_Transpose = 0;
		ht->ht_Voices[i].vc_NextTrack = 0;
		ht->ht_Voices[i].vc_NextTranspose = 0;
		ht->ht_Voices[i].vc_ADSRVolume = 0;
		ht->ht_Voices[i].vc_InstrPeriod = 0;
		ht->ht_Voices[i].vc_TrackPeriod = 0;
		ht->ht_Voices[i].vc_VibratoPeriod = 0;
		ht->ht_Voices[i].vc_NoteMaxVolume = 0;
		ht->ht_Voices[i].vc_PerfSubVolume = 0;
		ht->ht_Voices[i].vc_TrackMasterVolume = 0;
		ht->ht_Voices[i].vc_NewWaveform = 0;
		ht->ht_Voices[i].vc_Waveform = 0;
		ht->ht_Voices[i].vc_PlantSquare = 0;
		ht->ht_Voices[i].vc_PlantPeriod = 0;
		ht->ht_Voices[i].vc_IgnoreSquare = 0;
		ht->ht_Voices[i].vc_TrackOn = 0;
		ht->ht_Voices[i].vc_FixedNote = 0;
		ht->ht_Voices[i].vc_VolumeSlideUp = 0;
		ht->ht_Voices[i].vc_VolumeSlideDown = 0;
		ht->ht_Voices[i].vc_HardCut = 0;
		ht->ht_Voices[i].vc_HardCutRelease = 0;
		ht->ht_Voices[i].vc_HardCutReleaseF = 0;
		ht->ht_Voices[i].vc_PeriodSlideSpeed = 0;
		ht->ht_Voices[i].vc_PeriodSlidePeriod = 0;
		ht->ht_Voices[i].vc_PeriodSlideLimit = 0;
		ht->ht_Voices[i].vc_PeriodSlideOn = 0;
		ht->ht_Voices[i].vc_PeriodSlideWithLimit = 0;
		ht->ht_Voices[i].vc_PeriodPerfSlideSpeed = 0;
		ht->ht_Voices[i].vc_PeriodPerfSlidePeriod = 0;
		ht->ht_Voices[i].vc_PeriodPerfSlideOn = 0;
		ht->ht_Voices[i].vc_VibratoDelay = 0;
		ht->ht_Voices[i].vc_VibratoCurrent = 0;
		ht->ht_Voices[i].vc_VibratoDepth = 0;
		ht->ht_Voices[i].vc_VibratoSpeed = 0;
		ht->ht_Voices[i].vc_SquareOn = 0;
		ht->ht_Voices[i].vc_SquareInit = 0;
		ht->ht_Voices[i].vc_SquareLowerLimit = 0;
		ht->ht_Voices[i].vc_SquareUpperLimit = 0;
		ht->ht_Voices[i].vc_SquarePos = 0;
		ht->ht_Voices[i].vc_SquareSign = 0;
		ht->ht_Voices[i].vc_SquareSlidingIn = 0;
		ht->ht_Voices[i].vc_SquareReverse = 0;
		ht->ht_Voices[i].vc_FilterOn = 0;
		ht->ht_Voices[i].vc_FilterInit = 0;
		ht->ht_Voices[i].vc_FilterLowerLimit = 0;
		ht->ht_Voices[i].vc_FilterUpperLimit = 0;
		ht->ht_Voices[i].vc_FilterPos = 0;
		ht->ht_Voices[i].vc_FilterSign = 0;
		ht->ht_Voices[i].vc_FilterSpeed = 0;
		ht->ht_Voices[i].vc_FilterSlidingIn = 0;
		ht->ht_Voices[i].vc_IgnoreFilter = 0;
		ht->ht_Voices[i].vc_PerfCurrent = 0;
		ht->ht_Voices[i].vc_PerfSpeed = 0;
		ht->ht_Voices[i].vc_WaveLength = 0;
		ht->ht_Voices[i].vc_NoteDelayOn = 0;
		ht->ht_Voices[i].vc_NoteCutOn = 0;
		ht->ht_Voices[i].vc_AudioPeriod = 0;
		ht->ht_Voices[i].vc_AudioVolume = 0;
		ht->ht_Voices[i].vc_VoiceVolume = 0;
		ht->ht_Voices[i].vc_VoicePeriod = 0;
		ht->ht_Voices[i].vc_VoiceNum = 0;
		ht->ht_Voices[i].vc_WNRandom = 0;
		ht->ht_Voices[i].vc_SquareWait = 0;
		ht->ht_Voices[i].vc_FilterWait = 0;
		ht->ht_Voices[i].vc_PerfWait = 0;
		ht->ht_Voices[i].vc_NoteDelayWait = 0;
		ht->ht_Voices[i].vc_NoteCutWait = 0;
		ht->ht_Voices[i].vc_PerfList = 0;
		ht->ht_Voices[i].vc_RingSamplePos = 0;
		ht->ht_Voices[i].vc_RingDelta = 0;
		ht->ht_Voices[i].vc_RingPlantPeriod = 0;
		ht->ht_Voices[i].vc_RingAudioPeriod = 0;
		ht->ht_Voices[i].vc_RingNewWaveform = 0;
		ht->ht_Voices[i].vc_RingWaveform = 0;
		ht->ht_Voices[i].vc_RingFixedPeriod = 0;
		ht->ht_Voices[i].vc_RingBasePeriod = 0;

		ht->ht_Voices[i].vc_RingMixSource = NULL;
		ht->ht_Voices[i].vc_RingAudioSource = NULL;

		memset(&ht->ht_Voices[i].vc_SquareTempBuffer, 0, 0x80);
		memset(&ht->ht_Voices[i].vc_ADSR, 0, sizeof (struct hvl_envelope));
		memset(&ht->ht_Voices[i].vc_VoiceBuffer, 0 ,0x281);
		memset(&ht->ht_Voices[i].vc_RingVoiceBuffer, 0 ,0x281);

		ht->ht_Voices[i].vc_Delta = 1;
		ht->ht_Voices[i].vc_OverrideTranspose = 1000;  // 1.5
		ht->ht_Voices[i].vc_WNRandom          = 0x280;
		ht->ht_Voices[i].vc_VoiceNum          = i;
		ht->ht_Voices[i].vc_TrackMasterVolume = 0x40;
		ht->ht_Voices[i].vc_TrackOn           = 1;
		ht->ht_Voices[i].vc_MixSource         = ht->ht_Voices[i].vc_VoiceBuffer;
	}

	ht->ht_defpanleft  = stereopan_left[ht->ht_defstereo];
	ht->ht_defpanright = stereopan_right[ht->ht_defstereo];
}

/* half-public */
void __attribute__ ((visibility ("internal"))) hvl_InitReplayer( void )
{
	hvl_GenPanningTables ();
	hvl_GenSawtooth ( &waves[WO_SAWTOOTH_04], 0x04 );
	hvl_GenSawtooth ( &waves[WO_SAWTOOTH_08], 0x08 );
	hvl_GenSawtooth ( &waves[WO_SAWTOOTH_10], 0x10 );
	hvl_GenSawtooth ( &waves[WO_SAWTOOTH_20], 0x20 );
	hvl_GenSawtooth ( &waves[WO_SAWTOOTH_40], 0x40 );
	hvl_GenSawtooth ( &waves[WO_SAWTOOTH_80], 0x80 );
	hvl_GenTriangle ( &waves[WO_TRIANGLE_04], 0x04 );
	hvl_GenTriangle ( &waves[WO_TRIANGLE_08], 0x08 );
	hvl_GenTriangle ( &waves[WO_TRIANGLE_10], 0x10 );
	hvl_GenTriangle ( &waves[WO_TRIANGLE_20], 0x20 );
	hvl_GenTriangle ( &waves[WO_TRIANGLE_40], 0x40 );
	hvl_GenTriangle ( &waves[WO_TRIANGLE_80], 0x80 );
	hvl_GenSquare ( &waves[WO_SQUARES] );
	hvl_GenWhiteNoise ( &waves[WO_WHITENOISE], WHITENOISELEN );
	hvl_GenFilterWaves ( &waves[WO_TRIANGLE_04], &waves[WO_LOWPASSES], &waves[WO_HIGHPASSES] );
}

/* half-public */
int __attribute__ ((visibility ("internal"))) hvl_InitSubsong ( struct hvl_tune *ht, uint32_t nr )
{
	uint32_t PosNr, i;

	if ( nr > ht->ht_SubsongNr )
	{
		return FALSE;
	}

	ht->ht_SongNum = nr;

	PosNr = 0;
	if ( nr )
	{
		PosNr = ht->ht_Subsongs[nr-1];
	}

	ht->ht_PosNr          = PosNr;
	ht->ht_PosJump        = 0;
	ht->ht_PatternBreak   = 0;
	ht->ht_NoteNr         = 0;
	ht->ht_PosJumpNote    = 0;
	ht->ht_Tempo          = 6;
	ht->ht_StepWaitFrames = 0;
	ht->ht_GetNewPosition = 1;
	ht->ht_SongEndReached = 0;
	ht->ht_PlayingTime    = 0;

	for ( i=0; i<MAX_CHANNELS; i+=4 )
	{
		ht->ht_Voices[i+0].vc_Pan          = ht->ht_defpanleft;
		ht->ht_Voices[i+0].vc_SetPan       = ht->ht_defpanleft; // 1.4
		ht->ht_Voices[i+0].vc_PanMultLeft  = panning_left[ht->ht_defpanleft];
		ht->ht_Voices[i+0].vc_PanMultRight = panning_right[ht->ht_defpanleft];
		ht->ht_Voices[i+1].vc_Pan          = ht->ht_defpanright;
		ht->ht_Voices[i+1].vc_SetPan       = ht->ht_defpanright; // 1.4
		ht->ht_Voices[i+1].vc_PanMultLeft  = panning_left[ht->ht_defpanright];
		ht->ht_Voices[i+1].vc_PanMultRight = panning_right[ht->ht_defpanright];
		ht->ht_Voices[i+2].vc_Pan          = ht->ht_defpanright;
		ht->ht_Voices[i+2].vc_SetPan       = ht->ht_defpanright; // 1.4
		ht->ht_Voices[i+2].vc_PanMultLeft  = panning_left[ht->ht_defpanright];
		ht->ht_Voices[i+2].vc_PanMultRight = panning_right[ht->ht_defpanright];
		ht->ht_Voices[i+3].vc_Pan          = ht->ht_defpanleft;
		ht->ht_Voices[i+3].vc_SetPan       = ht->ht_defpanleft;  // 1.4
		ht->ht_Voices[i+3].vc_PanMultLeft  = panning_left[ht->ht_defpanleft];
		ht->ht_Voices[i+3].vc_PanMultRight = panning_right[ht->ht_defpanleft];
	}

	hvl_reset_some_stuff ( ht );

	return TRUE;
}

static void hvl_process_stepfx_1 ( struct hvl_tune *ht, struct hvl_voice *voice, int32_t FX, int32_t FXParam )
{
	switch ( FX )
	{
		case 0x0:  // Position Jump HI
			if ( ((FXParam&0x0f) > 0) && ((FXParam&0x0f) <= 9) )
			{
				ht->ht_PosJump = FXParam & 0xf;
			}
			break;

		case 0x5:  // Volume Slide + Tone Portamento
		case 0xa:  // Volume Slide
			voice->vc_VolumeSlideDown = FXParam & 0x0f;
			voice->vc_VolumeSlideUp   = FXParam >> 4;
			break;

		case 0x7:  // Panning
			if ( FXParam > 127 )
			{
				FXParam -= 256;
			}
			voice->vc_Pan          = (FXParam+128);
			voice->vc_SetPan       = (FXParam+128); // 1.4
			voice->vc_PanMultLeft  = panning_left[voice->vc_Pan];
			voice->vc_PanMultRight = panning_right[voice->vc_Pan];
			break;

		case 0xb: // Position jump
			ht->ht_PosJump      = ht->ht_PosJump*100 + (FXParam & 0x0f) + (FXParam >> 4)*10;
			ht->ht_PatternBreak = 1;
			if ( ht->ht_PosJump <= ht->ht_PosNr )
			{
			        ht->ht_SongEndReached = 1;
			}
			break;

		case 0xd: // Pattern break
			ht->ht_PosJump      = ht->ht_PosNr+1;
			ht->ht_PosJumpNote  = (FXParam & 0x0f) + (FXParam>>4)*10;
			ht->ht_PatternBreak = 1;
			if ( ht->ht_PosJumpNote >  ht->ht_TrackLength )
			{
				ht->ht_PosJumpNote = 0;
			}
			break;

		case 0xe: // Extended commands
			switch ( FXParam >> 4 )
			{
				case 0xc: // Note cut
					if ( (FXParam & 0x0f) < ht->ht_Tempo )
					{
						voice->vc_NoteCutWait = FXParam & 0x0f;
						if ( voice->vc_NoteCutWait )
						{
							voice->vc_NoteCutOn      = 1;
							voice->vc_HardCutRelease = 0;
						}
					}
					break;

				// 1.6: 0xd case removed
			}
			break;

		case 0xf: // Speed
			ht->ht_Tempo = FXParam;
			if ( FXParam == 0 )
			{
				ht->ht_SongEndReached = 1;
			}
			break;
	}
}

static void hvl_process_stepfx_2 ( struct hvl_tune *ht, struct hvl_voice *voice, int32_t FX, int32_t FXParam, int32_t *Note )
{
	switch ( FX )
	{
		case 0x9: // Set squarewave offset
			voice->vc_SquarePos    = FXParam >> (5 - voice->vc_WaveLength);
			voice->vc_PlantSquare  = 1;
			voice->vc_IgnoreSquare = 1;
			break;

		case 0x3: // Tone portamento
			if ( FXParam != 0 )
			{
				voice->vc_PeriodSlideSpeed = FXParam;
			}
		case 0x5: // Tone portamento + volume slide
			if ( *Note )
			{
				int32_t new, diff;

				new   = period_tab[*Note];
				diff  = period_tab[voice->vc_TrackPeriod];
				diff -= new;
				new   = diff + voice->vc_PeriodSlidePeriod;

				if( new )
				{
					voice->vc_PeriodSlideLimit = -diff;
				}
			}
			voice->vc_PeriodSlideOn        = 1;
			voice->vc_PeriodSlideWithLimit = 1;
			*Note = 0;
			break;
	}
}

static void hvl_process_stepfx_3 ( struct hvl_tune *ht, struct hvl_voice *voice, int32_t FX, int32_t FXParam )
{
	int32_t i;

	switch ( FX )
	{
		case 0x01: // Portamento up (period slide down)
			voice->vc_PeriodSlideSpeed     = -FXParam;
			voice->vc_PeriodSlideOn        = 1;
			voice->vc_PeriodSlideWithLimit = 0;
			break;
		case 0x02: // Portamento down
			voice->vc_PeriodSlideSpeed     = FXParam;
			voice->vc_PeriodSlideOn        = 1;
			voice->vc_PeriodSlideWithLimit = 0;
			break;
		case 0x04: // Filter override
			if ( ( FXParam == 0 ) || ( FXParam == 0x40 ) )
			{
				break;
			}
			if ( FXParam < 0x40 )
			{
				voice->vc_IgnoreFilter = FXParam;
				break;
			}
			if ( FXParam > 0x7f )
			{
				break;
			}
			voice->vc_FilterPos = FXParam - 0x40;
			break;
		case 0x0c: // Volume
			FXParam &= 0xff;
			if ( FXParam <= 0x40 )
			{
				voice->vc_NoteMaxVolume = FXParam;
				break;
			}

			if ( (FXParam -= 0x50) < 0 )
			{
				break;  // 1.6
			}

			if ( FXParam <= 0x40 )
			{
				for( i=0; i<ht->ht_Channels; i++ )
				{
					ht->ht_Voices[i].vc_TrackMasterVolume = FXParam;
				}
				break;
			}

			if ( (FXParam -= 0xa0-0x50) < 0 )
			{
				break; // 1.6
			}

			if ( FXParam <= 0x40 )
			{
				voice->vc_TrackMasterVolume = FXParam;
			}
			break;

		case 0xe: // Extended commands;
			switch ( FXParam >> 4 )
			{
				case 0x1: // Fineslide up
					voice->vc_PeriodSlidePeriod -= (FXParam & 0x0f); // 1.8
					voice->vc_PlantPeriod = 1;
					break;

				case 0x2: // Fineslide down
					voice->vc_PeriodSlidePeriod += (FXParam & 0x0f); // 1.8
					voice->vc_PlantPeriod = 1;
					break;

				case 0x4: // Vibrato control
					voice->vc_VibratoDepth = FXParam & 0x0f;
					break;

				case 0x0a: // Fine volume up
					voice->vc_NoteMaxVolume += FXParam & 0x0f;

					if ( voice->vc_NoteMaxVolume > 0x40 )
					{
						voice->vc_NoteMaxVolume = 0x40;
					}
					break;

				case 0x0b: // Fine volume down
					voice->vc_NoteMaxVolume -= FXParam & 0x0f;

					if ( voice->vc_NoteMaxVolume < 0 )
					{
						voice->vc_NoteMaxVolume = 0;
					}
					break;

				case 0x0f: // Misc flags (1.5)
					if( ht->ht_Version < 1 )
					{
						break;
					}
					switch ( FXParam & 0xf )
					{
						case 1:
							voice->vc_OverrideTranspose = voice->vc_Transpose;
							break;
					}
					break;
			}
			break;
	}
}

static void hvl_process_step ( struct hvl_tune *ht, struct hvl_voice *voice )
{
	int32_t  Note, Instr, donenotedel;
	struct   hvl_step *Step;

	if ( voice->vc_TrackOn == 0 )
	{
		return;
	}

	voice->vc_VolumeSlideUp = 0;
	voice->vc_VolumeSlideDown = 0;

	Step = &ht->ht_Tracks
		[
			ht->ht_Positions
			[
				ht->ht_PosNr
			].pos_Track
			[
				voice->vc_VoiceNum
			]
		][
			ht->ht_NoteNr
		];

	Note    = Step->stp_Note;
	Instr   = Step->stp_Instrument;

	// --------- 1.6: from here --------------

	donenotedel = 0;

	// Do notedelay here
	if ( ((Step->stp_FX&0xf)==0xe) && ((Step->stp_FXParam&0xf0)==0xd0) )
	{
		if ( voice->vc_NoteDelayOn )
		{
			voice->vc_NoteDelayOn = 0;
			donenotedel = 1;
		} else {
			if ( (Step->stp_FXParam&0x0f) < ht->ht_Tempo )
			{
				voice->vc_NoteDelayWait = Step->stp_FXParam & 0x0f;
				if( voice->vc_NoteDelayWait )
				{
					voice->vc_NoteDelayOn = 1;
					return;
				}
			}
		}
	}

	if ( (donenotedel==0) && ((Step->stp_FXb&0xf)==0xe) && ((Step->stp_FXbParam&0xf0)==0xd0) )
	{
		if ( voice->vc_NoteDelayOn )
		{
			voice->vc_NoteDelayOn = 0;
		} else {
			if ( (Step->stp_FXbParam&0x0f) < ht->ht_Tempo )
			{
				voice->vc_NoteDelayWait = Step->stp_FXbParam & 0x0f;
				if ( voice->vc_NoteDelayWait )
				{
					voice->vc_NoteDelayOn = 1;
					return;
				}
			}
		}
	}

	// --------- 1.6: to here --------------

	if ( Note )
	{
		voice->vc_OverrideTranspose = 1000; // 1.5
	}

	hvl_process_stepfx_1( ht, voice, Step->stp_FX &0xf, Step->stp_FXParam );
	hvl_process_stepfx_1( ht, voice, Step->stp_FXb&0xf, Step->stp_FXbParam );

	if ( ( Instr ) && ( Instr <= ht->ht_InstrumentNr ) )
	{
		struct hvl_instrument *Ins;
		int16_t SquareLower, SquareUpper, d6, d3, d4;

		/* 1.4: Reset panning to last set position */
		voice->vc_Pan          = voice->vc_SetPan;
		voice->vc_PanMultLeft  = panning_left[voice->vc_Pan];
		voice->vc_PanMultRight = panning_right[voice->vc_Pan];

		voice->vc_PeriodSlideSpeed  = 0;
		voice->vc_PeriodSlidePeriod = 0;
		voice->vc_PeriodSlideLimit  = 0;

		voice->vc_PerfSubVolume    = 0x40;
		voice->vc_ADSRVolume       = 0;
		voice->vc_Instrument       = Ins = &ht->ht_Instruments[Instr];
		voice->vc_SamplePos        = 0;

		voice->vc_ADSR.aFrames     = Ins->ins_Envelope.aFrames;
		voice->vc_ADSR.aVolume     = Ins->ins_Envelope.aFrames ? Ins->ins_Envelope.aVolume * 256 / voice->vc_ADSR.aFrames : Ins->ins_Envelope.aVolume * 256;
		voice->vc_ADSR.dFrames     = Ins->ins_Envelope.dFrames;
		voice->vc_ADSR.dVolume     = Ins->ins_Envelope.dFrames ? (Ins->ins_Envelope.dVolume-Ins->ins_Envelope.aVolume) * 256 / voice->vc_ADSR.dFrames : Ins->ins_Envelope.dVolume * 256; // XXX
		voice->vc_ADSR.sFrames     = Ins->ins_Envelope.sFrames;
		voice->vc_ADSR.rFrames     = Ins->ins_Envelope.rFrames;
		voice->vc_ADSR.rVolume     = Ins->ins_Envelope.rFrames ? (Ins->ins_Envelope.rVolume-Ins->ins_Envelope.dVolume) * 256 / voice->vc_ADSR.rFrames : Ins->ins_Envelope.rVolume * 256; // XXX

		voice->vc_WaveLength       = Ins->ins_WaveLength;
		voice->vc_NoteMaxVolume    = Ins->ins_Volume;

		voice->vc_VibratoCurrent   = 0;
		voice->vc_VibratoDelay     = Ins->ins_VibratoDelay;
		voice->vc_VibratoDepth     = Ins->ins_VibratoDepth;
		voice->vc_VibratoSpeed     = Ins->ins_VibratoSpeed;
		voice->vc_VibratoPeriod    = 0;

		voice->vc_HardCutRelease   = Ins->ins_HardCutRelease;
		voice->vc_HardCut          = Ins->ins_HardCutReleaseFrames;

		voice->vc_IgnoreSquare    = 0;
		voice->vc_SquareSlidingIn = 0;
		voice->vc_SquareWait      = 0;
		voice->vc_SquareOn        = 0;

		SquareLower = Ins->ins_SquareLowerLimit >> (5 - voice->vc_WaveLength);
		SquareUpper = Ins->ins_SquareUpperLimit >> (5 - voice->vc_WaveLength);

		if ( SquareUpper < SquareLower )
		{
			int16_t t = SquareUpper;
			SquareUpper = SquareLower;
			SquareLower = t;
		}

		voice->vc_SquareUpperLimit = SquareUpper;
		voice->vc_SquareLowerLimit = SquareLower;

		voice->vc_IgnoreFilter    = 0;
		voice->vc_FilterWait      = 0;
		voice->vc_FilterOn        = 0;
		voice->vc_FilterSlidingIn = 0;

		d6 = Ins->ins_FilterSpeed;
		d3 = Ins->ins_FilterLowerLimit;
		d4 = Ins->ins_FilterUpperLimit;

		if ( d3 & 0x80 )
		{
			d6 |= 0x20;
		}
		if ( d4 & 0x80 )
		{
			d6 |= 0x40;
		}

		voice->vc_FilterSpeed = d6;
		d3 &= ~0x80;
		d4 &= ~0x80;

		if ( d3 > d4 )
		{
			int16_t t = d3;
			d3 = d4;
			d4 = t;
		}

		voice->vc_FilterUpperLimit = d4;
		voice->vc_FilterLowerLimit = d3;
		voice->vc_FilterPos        = 32;

		voice->vc_PerfWait  = voice->vc_PerfCurrent = 0;
		voice->vc_PerfSpeed = Ins->ins_PList.pls_Speed;
		voice->vc_PerfList  = &voice->vc_Instrument->ins_PList;

		voice->vc_RingMixSource   = NULL;   // No ring modulation
		voice->vc_RingSamplePos   = 0;
		voice->vc_RingPlantPeriod = 0;
		voice->vc_RingNewWaveform = 0;
	}

	voice->vc_PeriodSlideOn = 0;

	hvl_process_stepfx_2 ( ht, voice, Step->stp_FX&0xf,  Step->stp_FXParam,  &Note );
	hvl_process_stepfx_2 ( ht, voice, Step->stp_FXb&0xf, Step->stp_FXbParam, &Note );

	if ( Note )
	{
		voice->vc_TrackPeriod = Note;
		voice->vc_PlantPeriod = 1;
	}

	hvl_process_stepfx_3 ( ht, voice, Step->stp_FX&0xf,  Step->stp_FXParam );
	hvl_process_stepfx_3 ( ht, voice, Step->stp_FXb&0xf, Step->stp_FXbParam );
}

static void hvl_plist_command_parse ( struct hvl_tune *ht, struct hvl_voice *voice, int32_t FX, int32_t FXParam )
{
	switch ( FX )
	{
		case 0:
			if ( ( FXParam > 0 ) && ( FXParam < 0x40 ) )
			{
				if ( voice->vc_IgnoreFilter )
				{
					voice->vc_FilterPos    = voice->vc_IgnoreFilter;
					voice->vc_IgnoreFilter = 0;
				} else {
					voice->vc_FilterPos    = FXParam;
				}
				voice->vc_NewWaveform = 1;
			}
			break;

		case 1:
			voice->vc_PeriodPerfSlideSpeed = FXParam;
			voice->vc_PeriodPerfSlideOn    = 1;
			break;

		case 2:
			voice->vc_PeriodPerfSlideSpeed = -FXParam;
			voice->vc_PeriodPerfSlideOn    = 1;
			break;

		case 3:
			if ( voice->vc_IgnoreSquare == 0 )
			{
				voice->vc_SquarePos = FXParam >> (5-voice->vc_WaveLength);
			} else {
				voice->vc_IgnoreSquare = 0;
			}
			break;

		case 4:
			if ( FXParam == 0 )
			{
				voice->vc_SquareInit = (voice->vc_SquareOn ^= 1);
				voice->vc_SquareSign = 1;
			} else {
				if ( FXParam & 0x0f )
				{
					voice->vc_SquareInit = (voice->vc_SquareOn ^= 1);
					voice->vc_SquareSign = 1;
					if (( FXParam & 0x0f ) == 0x0f )
					{
						voice->vc_SquareSign = -1;
					}
				}
				if ( FXParam & 0xf0 )
				{
					voice->vc_FilterInit = (voice->vc_FilterOn ^= 1);
					voice->vc_FilterSign = 1;
					if (( FXParam & 0xf0 ) == 0xf0 )
					{
						voice->vc_FilterSign = -1;
					}
				}
			}
			break;

		case 5:
			voice->vc_PerfCurrent = FXParam;
			break;

		case 7:
			// Ring modulate with triangle
			if (( FXParam >= 1 ) && ( FXParam <= 0x3C ))
			{
				voice->vc_RingBasePeriod  = FXParam;
				voice->vc_RingFixedPeriod = 1;
			} else if (( FXParam >= 0x81 ) && ( FXParam <= 0xBC ))
			{
				voice->vc_RingBasePeriod  = FXParam-0x80;
				voice->vc_RingFixedPeriod = 0;
			} else {
				voice->vc_RingBasePeriod  = 0;
				voice->vc_RingFixedPeriod = 0;
				voice->vc_RingNewWaveform = 0;
				voice->vc_RingAudioSource = NULL; // turn it off
				voice->vc_RingMixSource   = NULL;
				break;
			}
			voice->vc_RingWaveform    = 0;
			voice->vc_RingNewWaveform = 1;
			voice->vc_RingPlantPeriod = 1;
			break;

		case 8:  // Ring modulate with sawtooth
			if (( FXParam >= 1 ) && ( FXParam <= 0x3C ))
			{
				voice->vc_RingBasePeriod  = FXParam;
				voice->vc_RingFixedPeriod = 1;
			} else if (( FXParam >= 0x81 ) && ( FXParam <= 0xBC ))
			{
				voice->vc_RingBasePeriod  = FXParam-0x80;
				voice->vc_RingFixedPeriod = 0;
			} else {
				voice->vc_RingBasePeriod  = 0;
				voice->vc_RingFixedPeriod = 0;
				voice->vc_RingNewWaveform = 0;
				voice->vc_RingAudioSource = NULL;
				voice->vc_RingMixSource   = NULL;
				break;
			}

			voice->vc_RingWaveform    = 1;
			voice->vc_RingNewWaveform = 1;
			voice->vc_RingPlantPeriod = 1;
			break;

			/* New in HivelyTracker 1.4 */
		case 9:
			if ( FXParam > 127 )
			{
				FXParam -= 256;
			}
			voice->vc_Pan          = (FXParam+128);
			voice->vc_PanMultLeft  = panning_left[voice->vc_Pan];
			voice->vc_PanMultRight = panning_right[voice->vc_Pan];
			break;

		case 12:
			if ( FXParam <= 0x40 )
			{
				voice->vc_NoteMaxVolume = FXParam;
				break;
			}

			if ( (FXParam -= 0x50) < 0 )
			{
				break;
			}

			if ( FXParam <= 0x40 )
			{
				voice->vc_PerfSubVolume = FXParam;
				break;
			}

			if ( (FXParam -= 0xa0-0x50) < 0 )
			{
				break;
			}

			if ( FXParam <= 0x40 )
			{
				voice->vc_TrackMasterVolume = FXParam;
			}
			break;

		case 15:
			voice->vc_PerfSpeed = voice->vc_PerfWait = FXParam;
			break;
	}
}

static void hvl_process_frame ( struct hvl_tune *ht, struct hvl_voice *voice )
{
	static const uint8_t Offsets[] = {
		0x00,
		0x04,
		0x04 + 0x08,
		0x04 + 0x08 + 0x10,
		0x04 + 0x08 + 0x10 + 0x20,
		0x04 + 0x08 + 0x10 + 0x20 + 0x40
	};

	if ( voice->vc_TrackOn == 0 )
	{
		return;
	}

	if ( voice->vc_NoteDelayOn )
	{
		if ( voice->vc_NoteDelayWait <= 0 )
		{
			hvl_process_step ( ht, voice );
		} else {
			voice->vc_NoteDelayWait--;
		}
	}

	if ( voice->vc_HardCut )
	{
		int32_t nextinst;

		if ( ht->ht_NoteNr+1 < ht->ht_TrackLength )
		{
			nextinst = ht->ht_Tracks[voice->vc_Track][ht->ht_NoteNr+1].stp_Instrument;
		} else {
			nextinst = ht->ht_Tracks[voice->vc_NextTrack][0].stp_Instrument;
		}

		if ( nextinst )
		{
			int32_t d1;

			d1 = ht->ht_Tempo - voice->vc_HardCut;

			if ( d1 < 0 )
			{
				d1 = 0;
			}

			if ( !voice->vc_NoteCutOn )
			{
				voice->vc_NoteCutOn       = 1;
				voice->vc_NoteCutWait     = d1;
				voice->vc_HardCutReleaseF = -(d1-ht->ht_Tempo);
			} else {
				voice->vc_HardCut = 0;
			}
		}
	}

	if ( voice->vc_NoteCutOn )
	{
		if ( voice->vc_NoteCutWait <= 0 )
		{
			voice->vc_NoteCutOn = 0;

			if ( voice->vc_HardCutRelease )
			{
				voice->vc_ADSR.rVolume = -(voice->vc_ADSRVolume - (voice->vc_Instrument->ins_Envelope.rVolume << 8)) / voice->vc_HardCutReleaseF;
				voice->vc_ADSR.rFrames = voice->vc_HardCutReleaseF;
				voice->vc_ADSR.aFrames = voice->vc_ADSR.dFrames = voice->vc_ADSR.sFrames = 0;
			} else {
				voice->vc_NoteMaxVolume = 0;
			}
		} else {
			voice->vc_NoteCutWait--;
		}
	}

	// ADSR envelope
	if ( voice->vc_ADSR.aFrames )
	{
		voice->vc_ADSRVolume += voice->vc_ADSR.aVolume;

		if ( --voice->vc_ADSR.aFrames <= 0 )
		{
			voice->vc_ADSRVolume = voice->vc_Instrument->ins_Envelope.aVolume << 8;
		}
	} else if ( voice->vc_ADSR.dFrames )
	{
		voice->vc_ADSRVolume += voice->vc_ADSR.dVolume;

		if ( --voice->vc_ADSR.dFrames <= 0 )
		{
			voice->vc_ADSRVolume = voice->vc_Instrument->ins_Envelope.dVolume << 8;
		}
	} else if ( voice->vc_ADSR.sFrames )
	{
		voice->vc_ADSR.sFrames--;
	} else if ( voice->vc_ADSR.rFrames )
	{
		voice->vc_ADSRVolume += voice->vc_ADSR.rVolume;

		if ( --voice->vc_ADSR.rFrames <= 0 )
		{
			voice->vc_ADSRVolume = voice->vc_Instrument->ins_Envelope.rVolume << 8;
		}
	}

	// VolumeSlide
	voice->vc_NoteMaxVolume = voice->vc_NoteMaxVolume + voice->vc_VolumeSlideUp - voice->vc_VolumeSlideDown;

	if ( voice->vc_NoteMaxVolume < 0 )
	{
		voice->vc_NoteMaxVolume = 0;
	} else if ( voice->vc_NoteMaxVolume > 0x40 )
	{
		voice->vc_NoteMaxVolume = 0x40;
	}

	// Portamento
	if ( voice->vc_PeriodSlideOn )
	{
		if ( voice->vc_PeriodSlideWithLimit )
		{
			int32_t  d0, d2;

			d0 = voice->vc_PeriodSlidePeriod - voice->vc_PeriodSlideLimit;
			d2 = voice->vc_PeriodSlideSpeed;

			if ( d0 > 0 )
			{
				d2 = -d2;
			}

			if ( d0 )
			{
				int32_t d3;

				d3 = (d0 + d2) ^ d0;

				if ( d3 >= 0 )
				{
					d0 = voice->vc_PeriodSlidePeriod + d2;
				} else {
					d0 = voice->vc_PeriodSlideLimit;
				}

				voice->vc_PeriodSlidePeriod = d0;
				voice->vc_PlantPeriod = 1;
			}
		} else {
			voice->vc_PeriodSlidePeriod += voice->vc_PeriodSlideSpeed;
			voice->vc_PlantPeriod = 1;
		}
	}

	// Vibrato
	if ( voice->vc_VibratoDepth )
	{
		if ( voice->vc_VibratoDelay <= 0 )
		{
			voice->vc_VibratoPeriod = (vib_tab[voice->vc_VibratoCurrent] * voice->vc_VibratoDepth) >> 7;
			voice->vc_PlantPeriod = 1;
			voice->vc_VibratoCurrent = (voice->vc_VibratoCurrent + voice->vc_VibratoSpeed) & 0x3f;
		} else {
			voice->vc_VibratoDelay--;
		}
	}

	// PList
	if ( voice->vc_PerfList != 0 )
	{
		if ( voice->vc_Instrument && voice->vc_PerfCurrent < voice->vc_Instrument->ins_PList.pls_Length )
		{
			if ( --voice->vc_PerfWait <= 0 )
			{
				uint32_t i;
				int32_t cur;

				cur = voice->vc_PerfCurrent++;
				voice->vc_PerfWait = voice->vc_PerfSpeed;

				if ( voice->vc_PerfList->pls_Entries[cur].ple_Waveform )
				{
					voice->vc_Waveform             = voice->vc_PerfList->pls_Entries[cur].ple_Waveform-1;
					voice->vc_NewWaveform          = 1;
					voice->vc_PeriodPerfSlideSpeed = voice->vc_PeriodPerfSlidePeriod = 0;
				}

				// Holdwave
				voice->vc_PeriodPerfSlideOn = 0;

				for( i=0; i<2; i++ )
				{
					hvl_plist_command_parse ( ht, voice, voice->vc_PerfList->pls_Entries[cur].ple_FX[i]&0xff, voice->vc_PerfList->pls_Entries[cur].ple_FXParam[i]&0xff );
				}

				// GetNote
				if ( voice->vc_PerfList->pls_Entries[cur].ple_Note )
				{
					voice->vc_InstrPeriod = voice->vc_PerfList->pls_Entries[cur].ple_Note;
					voice->vc_PlantPeriod = 1;
					voice->vc_FixedNote   = voice->vc_PerfList->pls_Entries[cur].ple_Fixed;
				}
			}
		} else {
			if ( voice->vc_PerfWait )
			{
				voice->vc_PerfWait--;
			} else {
				voice->vc_PeriodPerfSlideSpeed = 0;
			}
		}
	}

	// PerfPortamento
	if ( voice->vc_PeriodPerfSlideOn )
	{
		voice->vc_PeriodPerfSlidePeriod -= voice->vc_PeriodPerfSlideSpeed;

		if ( voice->vc_PeriodPerfSlidePeriod )
		{
			voice->vc_PlantPeriod = 1;
		}
	}

	if ( voice->vc_Waveform == 3-1 && voice->vc_SquareOn ) /* SQUARE */
	{
		if ( --voice->vc_SquareWait <= 0 )
		{
			int32_t d1, d2, d3;

			d1 = voice->vc_SquareLowerLimit;
			d2 = voice->vc_SquareUpperLimit;
			d3 = voice->vc_SquarePos;

			if ( voice->vc_SquareInit )
			{
				voice->vc_SquareInit = 0;

				if ( d3 <= d1 )
				{
					voice->vc_SquareSlidingIn = 1;
					voice->vc_SquareSign = 1;
				} else if ( d3 >= d2 )
				{
					voice->vc_SquareSlidingIn = 1;
					voice->vc_SquareSign = -1;
				}
			}

			// NoSquareInit
			if ( d1 == d3 || d2 == d3 )
			{
				if ( voice->vc_SquareSlidingIn )
				{
					voice->vc_SquareSlidingIn = 0;
				} else {
					voice->vc_SquareSign = -voice->vc_SquareSign;
				}
			}

			d3 += voice->vc_SquareSign;
			voice->vc_SquarePos   = d3;
			voice->vc_PlantSquare = 1;
			voice->vc_SquareWait  = voice->vc_Instrument->ins_SquareSpeed;
		}
	}

	if ( voice->vc_FilterOn && --voice->vc_FilterWait <= 0 )
	{
		uint32_t i, FMax;
		int32_t d1, d2, d3;

		d1 = voice->vc_FilterLowerLimit;
		d2 = voice->vc_FilterUpperLimit;
		d3 = voice->vc_FilterPos;

		if ( voice->vc_FilterInit )
		{
			voice->vc_FilterInit = 0;
			if ( d3 <= d1 )
			{
				voice->vc_FilterSlidingIn = 1;
				voice->vc_FilterSign      = 1;
			} else if ( d3 >= d2 )
			{
				voice->vc_FilterSlidingIn = 1;
				voice->vc_FilterSign      = -1;
			}
		}

		// NoFilterInit
		FMax = (voice->vc_FilterSpeed < 3) ? (5-voice->vc_FilterSpeed) : 1;

		for ( i=0; i<FMax; i++ )
		{
			if ( ( d1 == d3 ) || ( d2 == d3 ) )
			{
				if ( voice->vc_FilterSlidingIn )
				{
					voice->vc_FilterSlidingIn = 0;
				} else {
					voice->vc_FilterSign = -voice->vc_FilterSign;
				}
			}
			d3 += voice->vc_FilterSign;
		}

		if( d3 < 1 )
		{
			d3 = 1;
		}
		if( d3 > 63 )
		{
			d3 = 63;
		}
		voice->vc_FilterPos   = d3;
		voice->vc_NewWaveform = 1;
		voice->vc_FilterWait  = voice->vc_FilterSpeed - 3;

		if ( voice->vc_FilterWait < 1 )
		{
			voice->vc_FilterWait = 1;
		}
	}

	if ( voice->vc_Waveform == 3-1 || voice->vc_PlantSquare ) /* SQUARE */
	{
		// CalcSquare
		uint32_t  i;
		int32_t   Delta;
		int8_t   *SquarePtr;
		int32_t  X;

		SquarePtr = &waves[WO_SQUARES+(voice->vc_FilterPos-0x20)*(0xfc+0xfc+0x80*0x1f+0x80+0x280*3)];
		X = voice->vc_SquarePos << (5 - voice->vc_WaveLength);

		if ( X > 0x20 )
		{
			X = 0x40 - X;
			voice->vc_SquareReverse = 1;
		}

		// OkDownSquare
		if( X > 0 )
		{
			SquarePtr += (X-1) << 7;
		}

		Delta = 32 >> voice->vc_WaveLength;
		ht->ht_WaveformTab[2] = voice->vc_SquareTempBuffer;

		for ( i=0; i<(1<<voice->vc_WaveLength)*4; i++ )
		{
			voice->vc_SquareTempBuffer[i] = *SquarePtr;
			SquarePtr += Delta;
		}

		voice->vc_NewWaveform = 1;
		voice->vc_Waveform    = 3-1; /* SQUARE */
		voice->vc_PlantSquare = 0;
	}

	if ( voice->vc_Waveform == 4-1 ) /* WHITENOISE */
	{
		voice->vc_NewWaveform = 1;
	}

	if ( voice->vc_RingNewWaveform )
	{
		int8_t *rasrc;

		if ( voice->vc_RingWaveform > 1 )
		{
			voice->vc_RingWaveform = 1;
		}

		rasrc = ht->ht_WaveformTab[voice->vc_RingWaveform]; /* RingWaveform is either TRIANGLE or SAWTOOTH */
		rasrc += Offsets[voice->vc_WaveLength];

		voice->vc_RingAudioSource = rasrc;
	}

	if ( voice->vc_NewWaveform )
	{
		int8_t *AudioSource;

		AudioSource = ht->ht_WaveformTab[voice->vc_Waveform];

		if ( voice->vc_Waveform != 3-1 ) /* SQUARE_WAVEFORM */
		{
			AudioSource += (voice->vc_FilterPos-0x20)*(0xfc+0xfc+0x80*0x1f+0x80+0x280*3);
		}

		if ( voice->vc_Waveform < 3-1) /* TRIANGLE or SAWTOOTH */
		{
			// GetWLWaveformlor2
			AudioSource += Offsets[voice->vc_WaveLength];
		}

		if ( voice->vc_Waveform == 4-1 ) /* WHITENOISE */
		{
			// AddRandomMoving
			AudioSource += ( voice->vc_WNRandom & (2*0x280-1) ) & ~1;
			// GoOnRandom
			voice->vc_WNRandom += 2239384;
			voice->vc_WNRandom  = ((((voice->vc_WNRandom >> 8) | (voice->vc_WNRandom << 24)) + 782323) ^ 75) - 6735;
		}

		voice->vc_AudioSource = AudioSource;
	}

	// Ring modulation period calculation
	if ( voice->vc_RingAudioSource )
	{
		voice->vc_RingAudioPeriod = voice->vc_RingBasePeriod;

		if ( !(voice->vc_RingFixedPeriod) )
		{
			if ( voice->vc_OverrideTranspose != 1000 )  // 1.5
			{
				voice->vc_RingAudioPeriod += voice->vc_OverrideTranspose + voice->vc_TrackPeriod - 1;
			} else {
				voice->vc_RingAudioPeriod += voice->vc_Transpose + voice->vc_TrackPeriod - 1;
			}
		}

		if ( voice->vc_RingAudioPeriod > 5*12 )
		{
			voice->vc_RingAudioPeriod = 5*12;
		}

		if ( voice->vc_RingAudioPeriod < 0 )
		{
			voice->vc_RingAudioPeriod = 0;
		}

		voice->vc_RingAudioPeriod = period_tab[voice->vc_RingAudioPeriod];

		if ( !(voice->vc_RingFixedPeriod) )
		{
			voice->vc_RingAudioPeriod += voice->vc_PeriodSlidePeriod;
		}
		voice->vc_RingAudioPeriod += voice->vc_PeriodPerfSlidePeriod + voice->vc_VibratoPeriod;

		if ( voice->vc_RingAudioPeriod > 0x0d60 )
		{
			voice->vc_RingAudioPeriod = 0x0d60;
		}

		if ( voice->vc_RingAudioPeriod < 0x0071 )
		{
			voice->vc_RingAudioPeriod = 0x0071;
		}
	}

	// Normal period calculation
	voice->vc_AudioPeriod = voice->vc_InstrPeriod;

	if ( !(voice->vc_FixedNote) )
	{
		if ( voice->vc_OverrideTranspose != 1000 ) // 1.5
		{
			voice->vc_AudioPeriod += voice->vc_OverrideTranspose + voice->vc_TrackPeriod - 1;
		} else {
			voice->vc_AudioPeriod += voice->vc_Transpose + voice->vc_TrackPeriod - 1;
		}
	}

	if ( voice->vc_AudioPeriod > 5*12 )
	{
		voice->vc_AudioPeriod = 5*12;
	}

	if ( voice->vc_AudioPeriod < 0 )
	{
		voice->vc_AudioPeriod = 0;
	}

	voice->vc_AudioPeriod = period_tab[voice->vc_AudioPeriod];

	if ( !(voice->vc_FixedNote) )
	{
		voice->vc_AudioPeriod += voice->vc_PeriodSlidePeriod;
	}

	voice->vc_AudioPeriod += voice->vc_PeriodPerfSlidePeriod + voice->vc_VibratoPeriod;

	if ( voice->vc_AudioPeriod > 0x0d60 )
	{
		voice->vc_AudioPeriod = 0x0d60;
	}

	if ( voice->vc_AudioPeriod < 0x0071 )
	{
		voice->vc_AudioPeriod = 0x0071;
	}

	voice->vc_AudioVolume = (((((((voice->vc_ADSRVolume >> 8) * voice->vc_NoteMaxVolume) >> 6) * voice->vc_PerfSubVolume) >> 6) * voice->vc_TrackMasterVolume) >> 6);
}

static void hvl_set_audio ( struct hvl_voice *voice, double freqf )
{
	if ( voice->vc_TrackOn == 0 )
	{
		voice->vc_VoiceVolume = 0;
		return;
	}

	voice->vc_VoiceVolume = voice->vc_AudioVolume;

	if ( voice->vc_PlantPeriod )
	{
		double freq2;
		uint32_t  delta;

		voice->vc_PlantPeriod = 0;
		voice->vc_VoicePeriod = voice->vc_AudioPeriod;

		freq2 = Period2Freq( voice->vc_AudioPeriod );
		delta = (uint32_t)(freq2 / freqf);

		if ( delta > (0x280<<16) )
		{
			delta -= (0x280<<16);
		}
		if ( delta == 0 )
		{
			delta = 1;
		}
		voice->vc_Delta = delta;
	}

	if ( voice->vc_NewWaveform )
	{
		int8_t *src;

		src = voice->vc_AudioSource;

		if ( voice->vc_Waveform == 4-1 ) /* WHITENOISE */
		{
			memcpy ( &voice->vc_VoiceBuffer[0], src, 0x280 );
		} else {
			uint32_t i, WaveLoops;

			WaveLoops = (1 << (5 - voice->vc_WaveLength)) * 5;

			for ( i=0; i<WaveLoops; i++ )
			{
				memcpy( &voice->vc_VoiceBuffer[i*4*(1<<voice->vc_WaveLength)], src, 4*(1<<voice->vc_WaveLength) );
			}
		}

		voice->vc_VoiceBuffer[0x280] = voice->vc_VoiceBuffer[0];
		voice->vc_MixSource          = voice->vc_VoiceBuffer;
	}

	/* Ring Modulation */
	if ( voice->vc_RingPlantPeriod )
	{
		double   freq2;
		uint32_t delta;

		voice->vc_RingPlantPeriod = 0;
		freq2 = Period2Freq( voice->vc_RingAudioPeriod );
		delta = (uint32_t)(freq2 / freqf);

		if ( delta > (0x280<<16) )
		{
			delta -= (0x280<<16);
		}
		if ( delta == 0 )
		{
			delta = 1;
		}
		voice->vc_RingDelta = delta;
	}

	if ( voice->vc_RingNewWaveform )
	{
		int8_t  *src;
		uint32_t i, WaveLoops;

		src = voice->vc_RingAudioSource;

		WaveLoops = (1 << (5 - voice->vc_WaveLength)) * 5;

		for ( i=0; i<WaveLoops; i++ )
		{
			memcpy( &voice->vc_RingVoiceBuffer[i*4*(1<<voice->vc_WaveLength)], src, 4*(1<<voice->vc_WaveLength) );
		}

		voice->vc_RingVoiceBuffer[0x280] = voice->vc_RingVoiceBuffer[0];
		voice->vc_RingMixSource          = voice->vc_RingVoiceBuffer;
	}
}

static void hvl_play_irq ( struct hvl_tune *ht )
{
	uint32_t i;

	if ( ht->ht_StepWaitFrames <= 0 )
	{
		if ( ht->ht_GetNewPosition )
		{
			int32_t nextpos = (ht->ht_PosNr+1==ht->ht_PositionNr)?0:(ht->ht_PosNr+1);

			for ( i=0; i<ht->ht_Channels; i++ )
			{
				ht->ht_Voices[i].vc_Track         = ht->ht_Positions[ht->ht_PosNr].pos_Track[i];
				ht->ht_Voices[i].vc_Transpose     = ht->ht_Positions[ht->ht_PosNr].pos_Transpose[i];
				ht->ht_Voices[i].vc_NextTrack     = ht->ht_Positions[nextpos].pos_Track[i];
				ht->ht_Voices[i].vc_NextTranspose = ht->ht_Positions[nextpos].pos_Transpose[i];
			}
			ht->ht_GetNewPosition = 0;
		}

		for ( i=0; i<ht->ht_Channels; i++ )
		{
			hvl_process_step ( ht, &ht->ht_Voices[i] );
		}

		ht->ht_StepWaitFrames = ht->ht_Tempo;
	}

	for ( i=0; i<ht->ht_Channels; i++ )
	{
		hvl_process_frame ( ht, &ht->ht_Voices[i] );
	}

	ht->ht_PlayingTime++;
	if ( ht->ht_Tempo > 0 && --ht->ht_StepWaitFrames <= 0 )
	{
		if ( !ht->ht_PatternBreak )
		{
			ht->ht_NoteNr++;
			if ( ht->ht_NoteNr >= ht->ht_TrackLength )
			{
				ht->ht_PosJump      = ht->ht_PosNr+1;
				ht->ht_PosJumpNote  = 0;
				ht->ht_PatternBreak = 1;
			}
		}

		if ( ht->ht_PatternBreak )
		{
			ht->ht_PatternBreak = 0;
			ht->ht_PosNr        = ht->ht_PosJump;
			ht->ht_NoteNr       = ht->ht_PosJumpNote;
			if ( ht->ht_PosNr == ht->ht_PositionNr )
			{
				ht->ht_SongEndReached = 1;
				ht->ht_PosNr          = ht->ht_Restart;
			}
			ht->ht_PosJumpNote  = 0;
			ht->ht_PosJump      = 0;

			ht->ht_GetNewPosition = 1;
		}
	}

	for ( i=0; i<ht->ht_Channels; i++ )
	{
		hvl_set_audio ( &ht->ht_Voices[i], ht->ht_Frequency );
	}
}

static void
hvl_mixchunk (struct hvl_tune *ht, int16_t *buf, size_t samples)
{
	int8_t   *src[MAX_CHANNELS];
	int8_t   *rsrc[MAX_CHANNELS];
	uint32_t  delta[MAX_CHANNELS];
	uint32_t  rdelta[MAX_CHANNELS];
	int32_t   vol[MAX_CHANNELS];
	uint32_t  pos[MAX_CHANNELS];
	uint32_t  rpos[MAX_CHANNELS];
	uint32_t  cnt;
	int32_t   panl[MAX_CHANNELS];
	int32_t   panr[MAX_CHANNELS];
//	uint32_t  vu[MAX_CHANNELS];
	int32_t   j;
	uint32_t  i, chans, loops;

	chans = ht->ht_Channels;
	for ( i=0; i<chans; i++ )
	{
		delta[i] = ht->ht_Voices[i].vc_Delta;
		vol[i]   = ht->ht_Voices[i].vc_VoiceVolume;
		pos[i]   = ht->ht_Voices[i].vc_SamplePos;
		src[i]   = ht->ht_Voices[i].vc_MixSource;
		panl[i]  = ht->ht_Voices[i].vc_PanMultLeft;
		panr[i]  = ht->ht_Voices[i].vc_PanMultRight;

	/* Ring Modulation */
		rdelta[i]= ht->ht_Voices[i].vc_RingDelta;
		rpos[i]  = ht->ht_Voices[i].vc_RingSamplePos;
		rsrc[i]  = ht->ht_Voices[i].vc_RingMixSource;

//		vu[i] = 0;
	}

	do
	{
		loops = samples;
		for ( i=0; i<chans; i++ )
		{
			if ( pos[i] >= (0x280 << 16))
			{
				pos[i] -= 0x280<<16;
			}
			cnt = ((0x280<<16) - pos[i] - 1) / delta[i] + 1;
			if ( cnt < loops )
			{
				loops = cnt;
			}

			if ( rsrc[i] )
			{
				if ( rpos[i] >= (0x280<<16))
				{
					rpos[i] -= 0x280<<16;
				}
				cnt = ((0x280<<16) - rpos[i] - 1) / rdelta[i] + 1;
				if ( cnt < loops )
				{
					loops = cnt;
				}
			}
		}

		samples -= loops;

		// Inner loop
		do
		{
			for ( i=0; i<chans; i++ )
			{
				if ( rsrc[i] )
				{
					/* Ring Modulation */
					j = ((src[i][pos[i]>>16]*rsrc[i][rpos[i]>>16])>>7)*vol[i];
					rpos[i] += rdelta[i];
				} else {
					j = src[i][pos[i]>>16]*vol[i];
				}
				/*
				if ( abs( j ) > vu[i] )
				{
					vu[i] = abs( j );
				}
				*/

				*(buf++) = (j * panl[i]) >> 7;
				*(buf++) = (j * panr[i]) >> 7;
				pos[i] += delta[i];
			}
#if 1
			/* clear non-used channels, just to be nice to the caller */
			for ( ; i < MAX_CHANNELS; i++)
			{
				*(buf++) = 0;
				*(buf++) = 0;
			}
#else
/* Possible micro-optimalization */
			buf += 2 * (MAX_CHANNELS - chans);
#endif

			loops--;
		} while ( loops > 0 );
	} while ( samples > 0 );

	for ( i=0; i<chans; i++ )
	{
		ht->ht_Voices[i].vc_SamplePos = pos[i];
		ht->ht_Voices[i].vc_RingSamplePos = rpos[i];
//		ht->ht_Voices[i].vc_VUMeter = vu[i];
	}
}

void __attribute__ ((visibility ("internal"))) hvl_DecodeFrame (struct hvl_tune *ht, int16_t *buf, size_t buflen)
{
	uint32_t pos = 0;
	uint32_t newpos;
	int i;

	for(i=1; pos < buflen; i++)
	{
		hvl_play_irq ( ht );

		newpos = buflen * i / ht->ht_SpeedMultiplier;

		if (newpos == pos)
		{
			continue;
		}
		hvl_mixchunk (ht, buf, newpos - pos);
		buf += (newpos - pos) * 2 * MAX_CHANNELS; // stereo
		pos = newpos;
	}
}

#if 0
int32_t __attribute__ ((visibility ("internal"))) hvl_mix_findloudest ( struct hvl_tune *ht, uint32_ samples )
{
	int8_t   *src[MAX_CHANNELS];
	int8_t   *rsrc[MAX_CHANNELS];
	uint32_t  delta[MAX_CHANNELS];
	uint32_t  rdelta[MAX_CHANNELS];
	int32_t   vol[MAX_CHANNELS];
	uint32_t  pos[MAX_CHANNELS];
	uint32_t  rpos[MAX_CHANNELS];
	uint32_t  cnt;
	int32_t   panl[MAX_CHANNELS];
	int32_t   panr[MAX_CHANNELS];
	int32_t   a=0, b=0, j;
	uint32_t  loud;
	uint32_t  i, chans, loops;

	loud = 0;

	chans = ht->ht_Channels;
	for ( i=0; i<chans; i++ )
	{
		delta[i] = ht->ht_Voices[i].vc_Delta;
		vol[i]   = ht->ht_Voices[i].vc_VoiceVolume;
		pos[i]   = ht->ht_Voices[i].vc_SamplePos;
		src[i]   = ht->ht_Voices[i].vc_MixSource;
		panl[i]  = ht->ht_Voices[i].vc_PanMultLeft;
		panr[i]  = ht->ht_Voices[i].vc_PanMultRight;
		/* Ring Modulation */
		rdelta[i]= ht->ht_Voices[i].vc_RingDelta;
		rpos[i]  = ht->ht_Voices[i].vc_RingSamplePos;
		rsrc[i]  = ht->ht_Voices[i].vc_RingMixSource;
	}

	do
	{
		loops = samples;
		for ( i=0; i<chans; i++ )
		{
			if ( pos[i] >= (0x280 << 16))
			{
				pos[i] -= 0x280<<16;
			}
			cnt = ((0x280<<16) - pos[i] - 1) / delta[i] + 1;
			if ( cnt < loops )
			{
				loops = cnt;
			}

			if ( rsrc[i] )
			{
				if ( rpos[i] >= (0x280<<16))
				{
					rpos[i] -= 0x280<<16;
				}
				cnt = ((0x280<<16) - rpos[i] - 1) / rdelta[i] + 1;
				if ( cnt < loops )
				{
					loops = cnt;
				}
			}
		}

		samples -= loops;

		// Inner loop
		do
		{
			a=0;
			b=0;
			for ( i=0; i<chans; i++ )
			{
				if ( rsrc[i] )
				{
					/* Ring Modulation */
					j = ((src[i][pos[i]>>16]*rsrc[i][rpos[i]>>16])>>7)*vol[i];
					rpos[i] += rdelta[i];
				} else {
					j = src[i][pos[i]>>16]*vol[i];
				}
				a += (j * panl[i]) >> 7;
				b += (j * panr[i]) >> 7;
				pos[i] += delta[i];
			}

//			a = (a*ht->ht_mixgain)>>8;
//			b = (b*ht->ht_mixgain)>>8;
			a = abs ( a );
			b = abs ( b );

			if ( a > loud )
			{
				loud = a;
			}
			if ( b > loud )
			{
				loud = b;
			}

			loops--;
		} while ( loops > 0 );
	} while ( samples > 0 );

	for ( i=0; i<chans; i++ )
	{
		ht->ht_Voices[i].vc_SamplePos = pos[i];
		ht->ht_Voices[i].vc_RingSamplePos = rpos[i];
	}

	return loud;
}

int32_t __attribute__ ((visibility ("internal"))) hvl_FindLoudest ( struct hvl_tune *ht, int32_t maxframes, BOOL usesongend )
{
	uint32_t rsamp, rloop;
	uint32_t samples, loops, loud, n;
	int32_t frm;

	rsamp = ht->ht_Frequency/50/ht->ht_SpeedMultiplier;
	rloop = ht->ht_SpeedMultiplier;

	loud = 0;

	ht->ht_SongEndReached = 0;

	frm = 0;
	while ( frm < maxframes )
	{
		if ( ( usesongend ) && ( ht->ht_SongEndReached ) )
		{
			break;
		}

		samples = rsamp;
		loops   = rloop;

		do
		{
			hvl_play_irq ( ht );
			n = hvl_mix_findloudest ( ht, samples );
			if ( n > loud )
			{
				loud = n;
			}
			loops--;
		} while ( loops );

		frm++;
	}

	return loud;
}
#endif
