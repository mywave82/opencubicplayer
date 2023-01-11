/* OpenCP Module Player
 * copyright (c) 2019-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Loading routine. Heavily based on https://github.com/pete-gordon/hivelytracker/tree/master/hvl2wav
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "cpiface/cpiface.h"
#include "loader.h"
#include "player.h"

struct hvl_tune __attribute__ ((visibility ("internal"))) *hvl_load_ahx (struct cpifaceSessionAPI_t *cpifaceSession, const uint8_t *buf, uint32_t buflen, uint32_t defstereo, uint32_t freq )
{
	const uint8_t       *bptr;
	const char          *nptr, *tptr;
	uint32_t             i, j, k, l, posn, insn, ssn, hs, trkn, trkl;
	struct hvl_tune     *ht;
	struct hvl_plsentry *ple;
	const int32_t        defgain[] = { 71, 72, 76, 85, 100 };

	if (buflen < 14)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[AHX] file truncated, header incomplete\n");
		return NULL;
	}
	posn = ((buf[6]&0x0f)<<8)|buf[7];
	insn = buf[12];
	ssn  = buf[13];
	trkl = buf[10];
	trkn = buf[11];

	if (buflen < (14 + ssn*2))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[HVL/AHX] file truncated, sub-songs incomplete\n");
		return NULL;
	}

	if (buflen < (14 + ssn*2 + posn*4*2))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[AHX] file truncated, positions incomplete\n");
		return NULL;
	}

	if (buflen < (14 + ssn*2 + posn*4*2 + (trkn+!!(buf[6]&0x80))*trkl*3))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[AHX] file truncated, tracks incomplete\n");
		return NULL;
	}

	hs  = sizeof( struct hvl_tune );
	hs += sizeof( struct hvl_position ) * posn;
	hs += sizeof( struct hvl_instrument ) * (insn + 1);
	hs += sizeof( uint16_t ) * ssn;

	// Calculate the size of all instrument PList buffers
	bptr = &buf[14];
	bptr += ssn*2;    // Skip past the subsong list
	bptr += posn*4*2; // Skip past the positions
	bptr += trkn*trkl*3;
	if ((buf[6]&0x80)==0)
	{ /* Track0 stored on disk, or not */
		bptr += trkl*3;
	}

	// *NOW* we can finally calculate PList space
	for ( i=1; i<=insn; i++ )
	{
		if ((bptr+22-buf) > buflen)
		{
			cpifaceSession->cpiDebug (cpifaceSession, "[AHX] file incomplete, instrument incomplete\n");
			return NULL;
		}
		hs += bptr[21] * sizeof( struct hvl_plsentry );
		bptr += 22 + bptr[21]*4;
		if ((bptr-buf) > buflen)
		{
			cpifaceSession->cpiDebug (cpifaceSession, "[AHX] file incomplete, instrument playlist incomplete\n");
			return NULL;
		}
	}

	ht = malloc(hs); //AllocVec( hs, MEMF_ANY );
	if ( !ht )
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[AHX] Out of memory!\n");
		return NULL;
	}

	ht->ht_Version         = 0;
	ht->ht_Frequency       = freq;
	ht->ht_FreqF           = (double)freq;

	ht->ht_Positions       = (struct hvl_position *)(&ht[1]);
	ht->ht_Instruments     = (struct hvl_instrument *)(&ht->ht_Positions[posn]);
	ht->ht_Subsongs        = (uint16_t *)(&ht->ht_Instruments[(insn+1)]);
	ple                    = (struct hvl_plsentry *)(&ht->ht_Subsongs[ssn]);

	ht->ht_WaveformTab[0]  = &waves[WO_TRIANGLE_04];
	ht->ht_WaveformTab[1]  = &waves[WO_SAWTOOTH_04];
	ht->ht_WaveformTab[3]  = &waves[WO_WHITENOISE];

	ht->ht_Channels        = 4;
	ht->ht_PositionNr      = posn;
	ht->ht_Restart         = (buf[8]<<8)|buf[9];
	ht->ht_SpeedMultiplier = ((buf[6]>>5)&3)+1;
	ht->ht_TrackLength     = trkl;
	ht->ht_TrackNr         = trkn;
	ht->ht_InstrumentNr    = insn;
	ht->ht_SubsongNr       = ssn;
	ht->ht_defstereo       = defstereo;
	ht->ht_defpanleft      = stereopan_left[ht->ht_defstereo];
	ht->ht_defpanright     = stereopan_right[ht->ht_defstereo];
	ht->ht_mixgain         = (defgain[ht->ht_defstereo]*256)/100;

	if ( ht->ht_Restart >= ht->ht_PositionNr )
	{
		ht->ht_Restart = ht->ht_PositionNr-1;
	}

	// Do some validation
	if ( ( ht->ht_PositionNr > 1000 ) ||
	     ( ht->ht_TrackLength > 64 ) ||
	     ( ht->ht_InstrumentNr > 64 ) )
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[AHX] headers out of range (%d,%d,%d)\n",
			ht->ht_PositionNr,
			ht->ht_TrackLength,
			ht->ht_InstrumentNr );
		free ( ht );
		return NULL;
	}

	bptr = &buf[14];

	// Subsongs
	for ( i=0; i<ht->ht_SubsongNr; i++ )
	{
		ht->ht_Subsongs[i] = (bptr[0]<<8)|bptr[1];
		if ( ht->ht_Subsongs[i] >= ht->ht_PositionNr )
		{
			ht->ht_Subsongs[i] = 0;
		}
		bptr += 2;
	}

	// Position list
	for ( i=0; i<ht->ht_PositionNr; i++ )
	{
		for ( j=0; j<4; j++ )
		{
			ht->ht_Positions[i].pos_Track[j]     = *bptr++;
			ht->ht_Positions[i].pos_Transpose[j] = *(int8_t *)bptr++;
		}
	}

	// Tracks
	for ( i=0; i<=ht->ht_TrackNr; i++ )
	{
		/* Is track 0 stored on disk or not? */
		if ( ( ( buf[6]&0x80 ) == 0x80 ) && ( i == 0 ) )
		{
			for ( j=0; j<ht->ht_TrackLength; j++ )
			{
				ht->ht_Tracks[i][j].stp_Note       = 0;
				ht->ht_Tracks[i][j].stp_Instrument = 0;
				ht->ht_Tracks[i][j].stp_FX         = 0;
				ht->ht_Tracks[i][j].stp_FXParam    = 0;
				ht->ht_Tracks[i][j].stp_FXb        = 0;
				ht->ht_Tracks[i][j].stp_FXbParam   = 0;
			}
			continue;
		}

		/* AHX does not have FXb, only HVL */
		for( j=0; j<ht->ht_TrackLength; j++ )
		{
			ht->ht_Tracks[i][j].stp_Note       = (bptr[0]>>2)&0x3f;
			ht->ht_Tracks[i][j].stp_Instrument = ((bptr[0]&0x3)<<4) | (bptr[1]>>4);
			ht->ht_Tracks[i][j].stp_FX         = bptr[1]&0xf;
			ht->ht_Tracks[i][j].stp_FXParam    = bptr[2];
			ht->ht_Tracks[i][j].stp_FXb        = 0;
			ht->ht_Tracks[i][j].stp_FXbParam   = 0;
			bptr += 3;
		}
	}

	// Instruments
	for ( i=1; i<=ht->ht_InstrumentNr; i++ )
	{
		ht->ht_Instruments[i].ins_Volume      = bptr[0];
		ht->ht_Instruments[i].ins_FilterSpeed = ((bptr[1]>>3)&0x1f)|((bptr[12]>>2)&0x20);
		ht->ht_Instruments[i].ins_WaveLength  = bptr[1]&0x07;

		ht->ht_Instruments[i].ins_Envelope.aFrames = bptr[2];
		ht->ht_Instruments[i].ins_Envelope.aVolume = bptr[3];
		ht->ht_Instruments[i].ins_Envelope.dFrames = bptr[4];
		ht->ht_Instruments[i].ins_Envelope.dVolume = bptr[5];
		ht->ht_Instruments[i].ins_Envelope.sFrames = bptr[6];
		ht->ht_Instruments[i].ins_Envelope.rFrames = bptr[7];
		ht->ht_Instruments[i].ins_Envelope.rVolume = bptr[8];

		ht->ht_Instruments[i].ins_FilterLowerLimit     = bptr[12]&0x7f;
		ht->ht_Instruments[i].ins_VibratoDelay         = bptr[13];
		ht->ht_Instruments[i].ins_HardCutReleaseFrames = (bptr[14]>>4)&0x07;
		ht->ht_Instruments[i].ins_HardCutRelease       = bptr[14]&0x80?1:0;
		ht->ht_Instruments[i].ins_VibratoDepth         = bptr[14]&0x0f;
		ht->ht_Instruments[i].ins_VibratoSpeed         = bptr[15];
		ht->ht_Instruments[i].ins_SquareLowerLimit     = bptr[16];
		ht->ht_Instruments[i].ins_SquareUpperLimit     = bptr[17];
		ht->ht_Instruments[i].ins_SquareSpeed          = bptr[18];
		ht->ht_Instruments[i].ins_FilterUpperLimit     = bptr[19]&0x3f;
		ht->ht_Instruments[i].ins_PList.pls_Speed      = bptr[20];
		ht->ht_Instruments[i].ins_PList.pls_Length     = bptr[21];

		ht->ht_Instruments[i].ins_PList.pls_Entries    = ple;
		ple += bptr[21];

		bptr += 22;
		for ( j=0; j<ht->ht_Instruments[i].ins_PList.pls_Length; j++ )
		{
			k = (bptr[0]>>5)&7;
			if ( k == 6 ) k = 12;
			if ( k == 7 ) k = 15;
			l = (bptr[0]>>2)&7;
			if ( l == 6 ) l = 12;
			if ( l == 7 ) l = 15;
			ht->ht_Instruments[i].ins_PList.pls_Entries[j].ple_FX[1]      = k;
			ht->ht_Instruments[i].ins_PList.pls_Entries[j].ple_FX[0]      = l;
			ht->ht_Instruments[i].ins_PList.pls_Entries[j].ple_Waveform   = ((bptr[0]<<1)&6) | (bptr[1]>>7);
			ht->ht_Instruments[i].ins_PList.pls_Entries[j].ple_Fixed      = (bptr[1]>>6)&1;
			ht->ht_Instruments[i].ins_PList.pls_Entries[j].ple_Note       = bptr[1]&0x3f;
			ht->ht_Instruments[i].ins_PList.pls_Entries[j].ple_FXParam[0] = bptr[2];
			ht->ht_Instruments[i].ins_PList.pls_Entries[j].ple_FXParam[1] = bptr[3];

			// 1.6: Strip "toggle filter" commands if the module is
			//      version 0 (pre-filters). This is what AHX also does.
			if ( ( buf[3] == 0 ) && ( l == 4 ) && ( (bptr[2]&0xf0) != 0 ) )
			{
				ht->ht_Instruments[i].ins_PList.pls_Entries[j].ple_FXParam[0] &= 0x0f;
			}
			if ( ( buf[3] == 0 ) && ( k == 4 ) && ( (bptr[3]&0xf0) != 0 ) )
			{
				ht->ht_Instruments[i].ins_PList.pls_Entries[j].ple_FXParam[1] &= 0x0f; // 1.8
			}

			bptr += 4;
		}
	}

	// nptr = (chat *)&buf[((buf[4]<<8)|buf[5]); can not be trusted, since it only stores the last 16bits (legacy)
	nptr = (const char *)bptr;
	tptr = nptr;
	while (1)
	{
		if ((tptr-(char *)buf) > buflen)
		{
			cpifaceSession->cpiDebug (cpifaceSession, "[AHX] strings incomplete, no title\n");
			ht->ht_Name[0] = 0;
			nptr = 0;
			break;
		}
		if (!*(tptr++))
		{
			snprintf (ht->ht_Name, sizeof (ht->ht_Name), "%s", nptr);
			nptr = tptr;
			break;
		}
	}

	for ( i=1; i<=ht->ht_InstrumentNr; i++ )
	{
		if (!nptr)
		{
			ht->ht_Instruments[i].ins_Name[0] = 0;
			continue;
		}
		tptr = nptr;
		while (1)
		{
			if ((tptr-(char *)buf) > buflen)
			{
				cpifaceSession->cpiDebug (cpifaceSession, "[AHX] strings incomplete, instruments\n");
				ht->ht_Instruments[i].ins_Name[0] = 0;
				nptr = 0;
				break;
			}
			if (!*(tptr++))
			{
				snprintf (ht->ht_Instruments[i].ins_Name, sizeof (ht->ht_Instruments[i].ins_Name), "%s", nptr);
				nptr = tptr;
				break;
			}
		}
	}

	hvl_InitSubsong ( ht, 0 );
	return ht;
}

struct hvl_tune __attribute__ ((visibility ("internal"))) *hvl_load_hvl (struct cpifaceSessionAPI_t *cpifaceSession, const uint8_t *buf, uint32_t buflen, uint32_t defstereo, uint32_t freq )
{
	struct hvl_tune *ht;
	const char      *nptr, *tptr;
	const uint8_t   *bptr;
	uint32_t         i, j, posn, insn, ssn, chnn, hs, trkl, trkn;
	struct           hvl_plsentry *ple;

	if (buflen < 16)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[HVL] file truncated, header incomplete\n");
		return NULL;
	}

	posn = ((buf[6]&0x0f)<<8)|buf[7];
	insn = buf[12];
	ssn  = buf[13];
	chnn = (buf[8]>>2)+4;
	trkl = buf[10];
	trkn = buf[11];

	if (buflen < (16 + ssn*2))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[HVL] file truncated, sub-songs incomplete\n");
		return NULL;
	}

	if (buflen < (16 + ssn*2 + posn*chnn*2))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[HVL] file truncated, positions incomplete\n");
		return NULL;
	}

	hs  = sizeof ( struct hvl_tune );
	hs += sizeof ( struct hvl_position ) * posn;
	hs += sizeof ( struct hvl_instrument ) * (insn + 1);
	hs += sizeof ( uint16_t ) * ssn;

	// Calculate the size of all instrument PList buffers
	bptr = &buf[16];
	bptr += ssn*2;       // Skip past the subsong list
	bptr += posn*chnn*2; // Skip past the positions

	// Skip past the tracks
	// 1.4: Fixed two really stupid bugs that cancelled each other
	//      out if the module had a blank first track (which is how
	//      come they were missed.
	for ( i=((buf[6]&0x80)==0x80)?1:0; i<=trkn; i++ )
	{
		for( j=0; j<trkl; j++ )
		{
			if ((bptr-buf) >= buflen)
			{
				cpifaceSession->cpiDebug (cpifaceSession, "[HVL] file truncated, tracks incomplete #1\n");
				return NULL;
			}
			if( bptr[0] == 0x3f )
			{
				bptr++;
			} else {
				bptr += 5;
			}
			if ((bptr-buf) > buflen)
			{
				cpifaceSession->cpiDebug (cpifaceSession, "[HVL] file truncated, tracks incomplete #2\n");
				return NULL;
			}
		}
	}

	// *NOW* we can finally calculate PList space
	for ( i=1; i<=insn; i++ )
	{
		if ((bptr+22-buf) > buflen)
		{
			cpifaceSession->cpiDebug (cpifaceSession, "[HVL] file truncated, instrument incomplete\n");
			return NULL;
		}

		hs += bptr[21] * sizeof( struct hvl_plsentry );
		bptr += 22 + bptr[21]*5;

		if ((bptr-buf) > buflen)
		{
			cpifaceSession->cpiDebug (cpifaceSession, "[HVL] file incomplete, instrument playlist incomplete\n");
			return NULL;
		}
	}

	ht = malloc(hs); // AllocVec( hs, MEMF_ANY );
	if ( !ht )
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[HVL] Out of memory!\n" );
		return NULL;
	}

	ht->ht_Version         = buf[3]; // 1.5
	ht->ht_Frequency       = freq;
	ht->ht_FreqF           = (double)freq;

	ht->ht_Positions       = (struct hvl_position *)(&ht[1]);
	ht->ht_Instruments     = (struct hvl_instrument *)(&ht->ht_Positions[posn]);
	ht->ht_Subsongs        = (uint16_t *)(&ht->ht_Instruments[(insn+1)]);
	ple                    = (struct hvl_plsentry *)(&ht->ht_Subsongs[ssn]);

	ht->ht_WaveformTab[0]  = &waves[WO_TRIANGLE_04];
	ht->ht_WaveformTab[1]  = &waves[WO_SAWTOOTH_04];
	ht->ht_WaveformTab[3]  = &waves[WO_WHITENOISE];

	ht->ht_PositionNr      = posn;
	ht->ht_Channels        = (buf[8]>>2)+4;
	ht->ht_Restart         = ((buf[8]&3)<<8)|buf[9];
	ht->ht_SpeedMultiplier = ((buf[6]>>5)&3)+1;
	ht->ht_TrackLength     = buf[10];
	ht->ht_TrackNr         = buf[11];
	ht->ht_InstrumentNr    = insn;
	ht->ht_SubsongNr       = ssn;
	ht->ht_mixgain         = (buf[14]<<8)/100;
	ht->ht_defstereo       = buf[15];
	ht->ht_defpanleft      = stereopan_left[ht->ht_defstereo];
	ht->ht_defpanright     = stereopan_right[ht->ht_defstereo];

	if ( ht->ht_Restart >= ht->ht_PositionNr )
	{
		ht->ht_Restart = ht->ht_PositionNr-1;
	}

	// Do some validation
	if( ( ht->ht_PositionNr > 1000 ) ||
	    ( ht->ht_TrackLength > 64 ) ||
	    ( ht->ht_InstrumentNr > 64 ) )
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[HVL] headers out of range (%d,%d,%d)\n",

			ht->ht_PositionNr,
			ht->ht_TrackLength,
			ht->ht_InstrumentNr );
		free( ht );
		return NULL;
	}

	bptr = &buf[16];

	// Subsongs
	for ( i=0; i<ht->ht_SubsongNr; i++ )
	{
		ht->ht_Subsongs[i] = (bptr[0]<<8)|bptr[1];
		bptr += 2;
	}

	// Position list
	for ( i=0; i<ht->ht_PositionNr; i++ )
	{
		for ( j=0; j<ht->ht_Channels; j++ )
		{
			ht->ht_Positions[i].pos_Track[j]     = *bptr++;
			ht->ht_Positions[i].pos_Transpose[j] = *(int8_t *)bptr++;
		}
	}

	// Tracks
	for ( i=0; i<=ht->ht_TrackNr; i++ )
	{
		if ( ( ( buf[6]&0x80 ) == 0x80 ) && ( i == 0 ) )
		{
			for ( j=0; j<ht->ht_TrackLength; j++ )
			{
				ht->ht_Tracks[i][j].stp_Note       = 0;
				ht->ht_Tracks[i][j].stp_Instrument = 0;
				ht->ht_Tracks[i][j].stp_FX         = 0;
				ht->ht_Tracks[i][j].stp_FXParam    = 0;
				ht->ht_Tracks[i][j].stp_FXb        = 0;
				ht->ht_Tracks[i][j].stp_FXbParam   = 0;
			}
			continue;
		}

		for ( j=0; j<ht->ht_TrackLength; j++ )
		{
			if ( bptr[0] == 0x3f )
			{
				ht->ht_Tracks[i][j].stp_Note       = 0;
				ht->ht_Tracks[i][j].stp_Instrument = 0;
				ht->ht_Tracks[i][j].stp_FX         = 0;
				ht->ht_Tracks[i][j].stp_FXParam    = 0;
				ht->ht_Tracks[i][j].stp_FXb        = 0;
				ht->ht_Tracks[i][j].stp_FXbParam   = 0;
				bptr++;
				continue;
			}

			ht->ht_Tracks[i][j].stp_Note       = bptr[0];
			ht->ht_Tracks[i][j].stp_Instrument = bptr[1];
			ht->ht_Tracks[i][j].stp_FX         = bptr[2]>>4;
			ht->ht_Tracks[i][j].stp_FXParam    = bptr[3];
			ht->ht_Tracks[i][j].stp_FXb        = bptr[2]&0xf;
			ht->ht_Tracks[i][j].stp_FXbParam   = bptr[4];
			bptr += 5;
		}
	}

	// Instruments
	for ( i=1; i<=ht->ht_InstrumentNr; i++ )
	{
		ht->ht_Instruments[i].ins_Volume      = bptr[0];
		ht->ht_Instruments[i].ins_FilterSpeed = ((bptr[1]>>3)&0x1f)|((bptr[12]>>2)&0x20);
		ht->ht_Instruments[i].ins_WaveLength  = bptr[1]&0x07;

		ht->ht_Instruments[i].ins_Envelope.aFrames = bptr[2];
		ht->ht_Instruments[i].ins_Envelope.aVolume = bptr[3];
		ht->ht_Instruments[i].ins_Envelope.dFrames = bptr[4];
		ht->ht_Instruments[i].ins_Envelope.dVolume = bptr[5];
		ht->ht_Instruments[i].ins_Envelope.sFrames = bptr[6];
		ht->ht_Instruments[i].ins_Envelope.rFrames = bptr[7];
		ht->ht_Instruments[i].ins_Envelope.rVolume = bptr[8];

		ht->ht_Instruments[i].ins_FilterLowerLimit     = bptr[12]&0x7f;
		ht->ht_Instruments[i].ins_VibratoDelay         = bptr[13];
		ht->ht_Instruments[i].ins_HardCutReleaseFrames = (bptr[14]>>4)&0x07;
		ht->ht_Instruments[i].ins_HardCutRelease       = bptr[14]&0x80?1:0;
		ht->ht_Instruments[i].ins_VibratoDepth         = bptr[14]&0x0f;
		ht->ht_Instruments[i].ins_VibratoSpeed         = bptr[15];
		ht->ht_Instruments[i].ins_SquareLowerLimit     = bptr[16];
		ht->ht_Instruments[i].ins_SquareUpperLimit     = bptr[17];
		ht->ht_Instruments[i].ins_SquareSpeed          = bptr[18];
		ht->ht_Instruments[i].ins_FilterUpperLimit     = bptr[19]&0x3f;
		ht->ht_Instruments[i].ins_PList.pls_Speed      = bptr[20];
		ht->ht_Instruments[i].ins_PList.pls_Length     = bptr[21];

		ht->ht_Instruments[i].ins_PList.pls_Entries    = ple;
		ple += bptr[21];

		bptr += 22;
		for ( j=0; j<ht->ht_Instruments[i].ins_PList.pls_Length; j++ )
		{
			ht->ht_Instruments[i].ins_PList.pls_Entries[j].ple_FX[0] = bptr[0]&0xf;
			ht->ht_Instruments[i].ins_PList.pls_Entries[j].ple_FX[1] = (bptr[1]>>3)&0xf;
			ht->ht_Instruments[i].ins_PList.pls_Entries[j].ple_Waveform = bptr[1]&7;
			ht->ht_Instruments[i].ins_PList.pls_Entries[j].ple_Fixed = (bptr[2]>>6)&1;
			ht->ht_Instruments[i].ins_PList.pls_Entries[j].ple_Note  = bptr[2]&0x3f;
			ht->ht_Instruments[i].ins_PList.pls_Entries[j].ple_FXParam[0] = bptr[3];
			ht->ht_Instruments[i].ins_PList.pls_Entries[j].ple_FXParam[1] = bptr[4];
			bptr += 5;
		}
	}

	// nptr = (chat *)&buf[((buf[4]<<8)|buf[5]); can not be trusted, since it only stores the last 16bits (legacy)
	nptr = (char *)bptr;
	tptr = nptr;
	while (1)
	{
		if ((tptr-(char *)buf) > buflen)
		{
			cpifaceSession->cpiDebug (cpifaceSession, "[HVL] strings incomplete, no title\n");
			ht->ht_Name[0] = 0;
			nptr = 0;
			break;
		}
		if (!*(tptr++))
		{
			snprintf (ht->ht_Name, sizeof (ht->ht_Name), "%s", nptr);
			nptr = tptr;
			break;
		}
	}

	for ( i=1; i<=ht->ht_InstrumentNr; i++ )
	{
		if (!nptr)
		{
			ht->ht_Instruments[i].ins_Name[0] = 0;
			continue;
		}
		tptr = nptr;
		while (1)
		{
			if ((tptr-(char *)buf) > buflen)
			{
				cpifaceSession->cpiDebug (cpifaceSession, "[HVL] strings incomplete, instruments\n");
				ht->ht_Instruments[i].ins_Name[0] = 0;
				nptr = 0;
				break;
			}
			if (!*(tptr++))
			{
				snprintf (ht->ht_Instruments[i].ins_Name, sizeof (ht->ht_Instruments[i].ins_Name), "%s", nptr);
				nptr = tptr;
				break;
			}
		}
	}

	hvl_InitSubsong( ht, 0 );
	return ht;
}

struct hvl_tune __attribute__ ((visibility ("internal"))) *hvl_LoadTune_memory (struct cpifaceSessionAPI_t *cpifaceSession, const uint8_t *buf, uint32_t buflen, uint32_t defstereo, uint32_t freq)
{
	if( ( buf[0] == 'T' ) &&
	    ( buf[1] == 'H' ) &&
	    ( buf[2] == 'X' ) &&
	    ( buf[3] < 3 ) )
	{
		return hvl_load_ahx (cpifaceSession, buf, buflen, defstereo, freq);
	}

	if( ( buf[0] == 'H' ) &&
	    ( buf[1] == 'V' ) &&
	    ( buf[2] == 'L' ) &&
	    ( buf[3] < 2 ) )
	{
		return hvl_load_hvl (cpifaceSession, buf, buflen, defstereo, freq);
	}

	cpifaceSession->cpiDebug (cpifaceSession, "[HVL] Invalid signature\n" );
	return NULL;
}

void hvl_FreeTune ( struct hvl_tune *ht )
{
	if( !ht )
	{
		return;
	}
	free( ht );
}
