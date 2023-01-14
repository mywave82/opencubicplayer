/* OpenCP Module Player
 * copyright (c) 2005-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Channel viewer for AY file support.
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
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "dev/mcp.h"
#include "ayplay.h"
#include "sound.h"
#include "stuff/poutput.h"
#include "cpiface/cpiface.h"

/*
         111111111122222222223333333
123456789012345678901234567890123456
Chan 1     440Hz vol:f
Chan 2     rndHz vol:0
Chan 3    5440Hz vol:0 <noise> <env>
Buzzer      80Hz                    
Noise:           period:00
Envelope    10Hz shape: /-----------


         11111111112222222222333333333344444
12345678901234567890123456789012345678901234
Chan 1     440Hz volume:  f    |          | 
Chan 2     123Hz volume:  0    |          | 
Chan 3    5440Hz volume:  0 <noise>    <env>
Buzzer      80Hz               |          | 
Noise            period: 00    +          | 
Envelope    10Hz shape: /---------------  + 


         11111111112222222222333333333344444444445555555555666
12345678901234567890123456789012345678901234567890123456789012
Channel 1          440Hz volume:  f    |             |        
Channel 2          123Hz volume:  0    |             |        
Channel 3         5440Hz volume:  0 <noise>      <envelope>   
Buzzer              80Hz               |             |        
Noise                    period: 00    +             |        
Global Envelope     10Hz  shape: /---------------    +        


         1111111111222222222233333333334444444444555555555566666666667777777
1234567890123456789012345678901234567890123456789012345678901234567890123456
Channel 1          440Hz  volume: f                 |                  |    
Channel 2          123Hz  volume: 0                 |                  |    
Channel 3         5440Hz  volume: 0              <noise>          <envelope>
Buzzer              80Hz                            |                  |    
Noise                                    period: 00 +                  |    
Global Envelope     10Hz                       shape: /--------------- +    

                                                                                                   11111111111111111111111111111
         11111111112222222222333333333344444444445555555555666666666677777777778888888888999999999900000000001111111111222222222
12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678
Channel 1          440 Hz  volume: f                  |                                   |                                     
Channel 2          123 Hz  volume: 0                  |                                   |                                     
Channel 3         5440 Hz  volume: 0               <noise>                           <envelope>                                 
Buzzer              80 Hz                             |                                   |                                     
Noise                                     period: 00  +                                   |                                     
Global Envelope     10 Hz                                       shape: /---------------   +                                     

*/

static void _write_envelope (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, const int offset, unsigned char color, int shape, const int length)
{
	switch (shape)
	{
		case 0:
		case 1:
		case 2:
		case 3:
		case 9:
			cpifaceSession->console->WriteString (buf, offset, color, "\\_______________", length);
			break;
		case 4:
		case 5:
		case 6:
		case 7:
		case 15:
			cpifaceSession->console->WriteString (buf, offset, color, "/_______________", length);
			break;
		case 8:
			cpifaceSession->console->WriteString (buf, offset, color, "\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\", length);
			break;
		case 10:
			cpifaceSession->console->WriteString (buf, offset, color, "\\/\\/\\/\\/\\/\\/\\/\\/", length);
			break;
		case 11:
			cpifaceSession->console->WriteString (buf, offset, color, "\\\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"", length);
			break;
		case 12:
			cpifaceSession->console->WriteString (buf, offset, color, "////////////////", length);
			break;
		case 13:
			cpifaceSession->console->WriteString (buf, offset, color, "/\"\"\"\"\"\"\"\"\"\"", length);
			break;
		case 14:
			cpifaceSession->console->WriteString (buf, offset, color, "\\/\\/\\/\\/\\/\\/\\/\\/", length);
			break;
	}
}

static void _drawchannel36 (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, const int i, struct ay_driver_frame_state_t *ci, const uint16_t channel_period, uint8_t amplitude)
{
	unsigned char tcol=0x0F;
	unsigned char tcold=0x07;
	unsigned char tcolr=0x0B;
	unsigned char mute = cpifaceSession->MuteChannel[i];

	cpifaceSession->console->WriteString (buf, 0, tcol, "Chan        - Hz vol:               ", 36);
	cpifaceSession->console->WriteNum (buf, 5, tcol, i + 1, 10, 1, 0);
	if (!(ci->mixer & (0x01 << i)))
	{
		cpifaceSession->console->WriteNum (buf, 7, mute?tcold:tcolr, ci->clockrate / (channel_period * 16), 10, 7, 1);
		/*                                                                   16 = clock divider */
	}
	if (!(ci->mixer & (0x08 << i)))
	{
		cpifaceSession->console->WriteString  (buf, 23, mute?tcold:tcolr, "<noise>", 7);
	}
	cpifaceSession->console->WriteNum (buf, 21, mute?tcold:tcolr, amplitude & 0xf, 16, 1, 0);
	if (amplitude & 0x10)
	{
		cpifaceSession->console->WriteString  (buf, 31, mute?tcold:tcolr, "<env>", 5);
	}
}

static void drawchannel36 (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int i)
{
	struct ay_driver_frame_state_t ci;

	unsigned char tcol=0x0F;
	// unsigned char tcold=0x07;
	unsigned char tcolr=0x0B;
	// unsigned char mute = cpifaceSession->MuteChannel[i];

	ayGetChans(&ci);

	switch (i)
	{
		case 0:
			_drawchannel36 (cpifaceSession, buf, 0, &ci, ci.channel_a_period, ci.amplitude_a);
			break;
		case 1:
			_drawchannel36 (cpifaceSession, buf, 1, &ci, ci.channel_b_period, ci.amplitude_b);
			break;
		case 2:
			_drawchannel36 (cpifaceSession, buf, 2, &ci, ci.channel_c_period, ci.amplitude_c);
			break;
		case 3:
			cpifaceSession->console->WriteString (buf, 0, tcol, "Buzzer        Hz                    ", 36);
			#warning TODO, we need to store, and print Buzzer rate
			break;
		case 4:
			cpifaceSession->console->WriteString (buf, 0, tcol, "Noise            period:            ", 36);
			cpifaceSession->console->WriteNum (buf, 24, tcolr, ci.noise_period, 16, 2, 1);
			break;
		case 5:
			cpifaceSession->console->WriteString (buf, 0, tcol, "Envelope      Hz shape:             ", 36);
			cpifaceSession->console->WriteNum (buf, 9, tcolr, ci.clockrate / (ci.envelope_period * 16 * 16), 10, 5, 1);
			/*                                                           16 = clock divider          */
			/*                                                                16 = substeps per step */
			_write_envelope (cpifaceSession, buf, 24, tcolr, ci.envelope_shape & 0x0f, 11);
			break;
	}
}

static void _drawchannel44 (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, const int i, struct ay_driver_frame_state_t *ci, const uint16_t channel_period, uint8_t amplitude)
{
	unsigned char tcol=0x0F;
	unsigned char tcold=0x07;
	unsigned char tcolr=0x0B;
	unsigned char mute = cpifaceSession->MuteChannel[i];

	cpifaceSession->console->WriteString (buf, 0, tcol, "Chan          Hz volume:       |          | ", 44);
	cpifaceSession->console->WriteNum (buf, 5, tcol, i + 1, 10, 1, 0);
	if (!(ci->mixer & (0x01 << i)))
	{
		cpifaceSession->console->WriteNum (buf, 6, mute?tcold:tcolr, ci->clockrate / (channel_period * 16), 10, 8, 1);
		/*                                                                   16 = clock divider */
	}
	if (!(ci->mixer & (0x08 << i)))
	{
		cpifaceSession->console->WriteString  (buf, 28, mute?tcold:tcolr, "<noise>", 7);
	}
	cpifaceSession->console->WriteNum (buf, 26, mute?tcold:tcolr, amplitude & 0xf, 16, 1, 0);
	if (amplitude & 0x10)
	{
		cpifaceSession->console->WriteString  (buf, 39, mute?tcold:tcolr, "<env>", 5);
	}
}

static void drawchannel44 (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int i)
{
	struct ay_driver_frame_state_t ci;

	unsigned char tcol=0x0F;
	// unsigned char tcold=0x07;
	unsigned char tcolr=0x0B;
	// unsigned char mute = cpifaceSession->MuteChannel[i];

	ayGetChans(&ci);

	switch (i)
	{
		case 0:
			_drawchannel44 (cpifaceSession, buf, 0, &ci, ci.channel_a_period, ci.amplitude_a);
			break;
		case 1:
			_drawchannel44 (cpifaceSession, buf, 1, &ci, ci.channel_b_period, ci.amplitude_b);
			break;
		case 2:
			_drawchannel44 (cpifaceSession, buf, 2, &ci, ci.channel_c_period, ci.amplitude_c);
			break;
		case 3:
			cpifaceSession->console->WriteString (buf, 0, tcol, "Buzzer        Hz               |          | ", 44);
			#warning TODO, we need to store, and print Buzzer rate
			break;
		case 4:
			cpifaceSession->console->WriteString (buf, 0, tcol, "Noise            period:       +          | ", 44);
			cpifaceSession->console->WriteNum (buf, 25, tcolr, ci.noise_period, 16, 2, 1);
			break;
		case 5:
			cpifaceSession->console->WriteString (buf, 0, tcol, "Envelope      Hz shape:                   + ", 44);
			cpifaceSession->console->WriteNum (buf, 8, tcolr, ci.clockrate / (ci.envelope_period * 16 * 16), 10, 6, 1);
			/*                                                           16 = clock divider          */
			/*                                                                16 = substeps per step */
			_write_envelope (cpifaceSession, buf, 24, tcolr, ci.envelope_shape & 0x0f, 16);
			break;
	}
}

static void _drawchannel62 (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, const int i, struct ay_driver_frame_state_t *ci, const uint16_t channel_period, uint8_t amplitude)
{
	unsigned char tcol=0x0F;
	unsigned char tcold=0x07;
	unsigned char tcolr=0x0B;
	unsigned char mute = cpifaceSession->MuteChannel[i];

	cpifaceSession->console->WriteString (buf, 0, tcol, "Channel               Hz volume:       |             |        ", 62);
	cpifaceSession->console->WriteNum (buf, 8, tcol, i + 1, 10, 1, 0);
	if (!(ci->mixer & (0x01 << i)))
	{
		cpifaceSession->console->WriteNum (buf, 12, mute?tcold:tcolr, ci->clockrate / (channel_period * 16), 10, 10, 1);
		/*                                                                    16 = clock divider */
	}
	if (!(ci->mixer & (0x08 << i)))
	{
		cpifaceSession->console->WriteString  (buf, 36, mute?tcold:tcolr, "<noise>", 7);
	}
	cpifaceSession->console->WriteNum (buf, 34, mute?tcold:tcolr, amplitude & 0xf, 16, 1, 0);
	if (amplitude & 0x10)
	{
		cpifaceSession->console->WriteString  (buf, 49, mute?tcold:tcolr, "<envelope>", 10);
	}
}

static void drawchannel62 (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int i)
{
	struct ay_driver_frame_state_t ci;

	unsigned char tcol=0x0F;
	// unsigned char tcold=0x07;
	unsigned char tcolr=0x0B;
	// unsigned char mute = cpifaceSession->MuteChannel[i];

	ayGetChans(&ci);

	switch (i)
	{
		case 0:
			_drawchannel62 (cpifaceSession, buf, 0, &ci, ci.channel_a_period, ci.amplitude_a);
			break;
		case 1:
			_drawchannel62 (cpifaceSession, buf, 1, &ci, ci.channel_b_period, ci.amplitude_b);
			break;
		case 2:
			_drawchannel62 (cpifaceSession, buf, 2, &ci, ci.channel_c_period, ci.amplitude_c);
			break;
		case 3:
			cpifaceSession->console->WriteString (buf, 0, tcol, "Buzzer                Hz               |             |        ", 62);
			#warning TODO, we need to store, and print Buzzer rate
			break;
		case 4:
			cpifaceSession->console->WriteString (buf, 0, tcol, "Noise                    period:       +             |        ", 62);
			cpifaceSession->console->WriteNum (buf, 33, tcolr, ci.noise_period, 16, 2, 1);
			break;
		case 5:
			cpifaceSession->console->WriteString (buf, 0, tcol, "Global Envelope       Hz  shape:                     +        ", 62);
			cpifaceSession->console->WriteNum (buf, 15, tcolr, ci.clockrate / (ci.envelope_period * 16 * 16), 10, 7, 1);
			/*                                                            16 = clock divider          */
			/*                                                                 16 = substeps per step */
			_write_envelope (cpifaceSession, buf, 33, tcolr, ci.envelope_shape & 0x0f, 16);
			break;
	}
}

static void _drawchannel76 (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, const int i, struct ay_driver_frame_state_t *ci, const uint16_t channel_period, uint8_t amplitude)
{
	unsigned char tcol=0x0F;
	unsigned char tcold=0x07;
	unsigned char tcolr=0x0B;
	unsigned char mute = cpifaceSession->MuteChannel[i];

	cpifaceSession->console->WriteString (buf, 0, tcol, "Channel               Hz  volume:                   |                  |    ", 76);
	cpifaceSession->console->WriteNum (buf, 8, tcol, i + 1, 10, 1, 0);
	if (!(ci->mixer & (0x01 << i)))
	{
		cpifaceSession->console->WriteNum (buf, 12, mute?tcold:tcolr, ci->clockrate / (channel_period * 16), 10, 10, 1);
		/*                                                                    16 = clock divider */
	}
	if (!(ci->mixer & (0x08 << i)))
	{
		cpifaceSession->console->WriteString  (buf, 49, mute?tcold:tcolr, "<noise>", 7);
	}
	cpifaceSession->console->WriteNum (buf, 34, mute?tcold:tcolr, amplitude & 0xf, 16, 1, 0);
	if (amplitude & 0x10)
	{
		cpifaceSession->console->WriteString  (buf, 66, mute?tcold:tcolr, "<envelope>", 10);
	}
}

static void drawchannel76 (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int i)
{
	struct ay_driver_frame_state_t ci;

	unsigned char tcol=0x0F;
	// unsigned char tcold=0x07;
	unsigned char tcolr=0x0B;
	// unsigned char mute = cpifaceSession->MuteChannel[i];

	ayGetChans(&ci);

	switch (i)
	{
		case 0:
			_drawchannel76 (cpifaceSession, buf, 0, &ci, ci.channel_a_period, ci.amplitude_a);
			break;
		case 1:
			_drawchannel76 (cpifaceSession, buf, 1, &ci, ci.channel_b_period, ci.amplitude_b);
			break;
		case 2:
			_drawchannel76 (cpifaceSession, buf, 2, &ci, ci.channel_c_period, ci.amplitude_c);
			break;
		case 3:
			cpifaceSession->console->WriteString (buf, 0, tcol, "Buzzer                Hz                            |                  |    ", 76);
			#warning TODO, we need to store, and print Buzzer rate
			break;
		case 4:
			cpifaceSession->console->WriteString (buf, 0, tcol, "Noise                                    period:    +                  |    ", 76);
			cpifaceSession->console->WriteNum (buf, 49, tcolr, ci.noise_period, 16, 2, 1);
			break;
		case 5:
			cpifaceSession->console->WriteString (buf, 0, tcol, "Global Envelope       Hz                       shape:                  +    ", 76);
			cpifaceSession->console->WriteNum (buf, 15, tcolr, ci.clockrate / (ci.envelope_period * 16 * 16), 10, 7, 1);
			/*                                                            16 = clock divider          */
			/*                                                                 16 = substeps per step */
			_write_envelope (cpifaceSession, buf, 54, tcolr, ci.envelope_shape & 0x0f, 16);
			break;
	}
}

static void _drawchannel128 (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, const int i, struct ay_driver_frame_state_t *ci, const uint16_t channel_period, uint8_t amplitude)
{
	unsigned char tcol=0x0F;
	unsigned char tcold=0x07;
	unsigned char tcolr=0x0B;
	unsigned char mute = cpifaceSession->MuteChannel[i];

	cpifaceSession->console->WriteString (buf, 0, tcol, "Channel                Hz  volume:                    |                                   |                                     ", 128);
	cpifaceSession->console->WriteNum (buf, 8, tcol, i + 1, 10, 1, 0);
	if (!(ci->mixer & (0x01 << i)))
	{
		cpifaceSession->console->WriteNum (buf, 12, mute?tcold:tcolr, ci->clockrate / (channel_period * 16), 10, 10, 1);
		/*                                                                    16 = clock divider */
	}
	if (!(ci->mixer & (0x08 << i)))
	{
		cpifaceSession->console->WriteString  (buf, 51, mute?tcold:tcolr, "<noise>", 7);
	}
	cpifaceSession->console->WriteNum (buf, 35, mute?tcold:tcolr, amplitude & 0xf, 16, 1, 0);
	if (amplitude & 0x10)
	{
		cpifaceSession->console->WriteString  (buf, 85, mute?tcold:tcolr, "<envelope>", 10);
	}
}

static void drawchannel128 (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int i)
{
	struct ay_driver_frame_state_t ci;
	unsigned char tcol=0x0F;
	// unsigned char tcold=0x07;
	unsigned char tcolr=0x0B;
	// unsigned char mute = cpifaceSession->MuteChannel[i];

	ayGetChans(&ci);

	switch (i)
	{
		case 0:
			_drawchannel128 (cpifaceSession, buf, 0, &ci, ci.channel_a_period, ci.amplitude_a);
			break;
		case 1:
			_drawchannel128 (cpifaceSession, buf, 1, &ci, ci.channel_b_period, ci.amplitude_b);
			break;
		case 2:
			_drawchannel128 (cpifaceSession, buf, 2, &ci, ci.channel_c_period, ci.amplitude_c);
			break;
		case 3:
			cpifaceSession->console->WriteString (buf, 0, tcol, "Buzzer                 Hz                             |                                   |                                     ", 128);
			#warning TODO, we need to store, and print Buzzer rate
			break;
		case 4:
			cpifaceSession->console->WriteString (buf, 0, tcol, "Noise                                     period:     +                                   |                                     ", 128);
			cpifaceSession->console->WriteNum (buf, 50, tcolr, ci.noise_period, 16, 2, 1);
			break;
		case 5:
			cpifaceSession->console->WriteString (buf, 0, tcol, "Global Envelope        Hz                                       shape:                    +                                     ", 128);
			cpifaceSession->console->WriteNum (buf, 15, tcolr, ci.clockrate / (ci.envelope_period * 16 * 16), 10, 7, 1);
			/*                                                            16 = clock divider          */
			/*                                                                 16 = substeps per step */
			_write_envelope (cpifaceSession, buf, 71, tcolr, ci.envelope_shape & 0x0f, 16);
	}
}

static void drawchannel (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, enum cpiChanWidth width, int i, int compoMode)
{
	switch (width)
	{
		case cpiChanWidth_36:
			drawchannel36 (cpifaceSession, buf, i);
			break;
		case cpiChanWidth_44:
			drawchannel44 (cpifaceSession, buf, i);
			break;
		case cpiChanWidth_62:
			drawchannel62 (cpifaceSession, buf, i);
			break;
		case cpiChanWidth_76:
			drawchannel76 (cpifaceSession, buf, i);
			break;
		case cpiChanWidth_128:
			drawchannel128 (cpifaceSession, buf, i);
			break;
	}
}

void __attribute__ ((visibility ("internal"))) ayChanSetup (struct cpifaceSessionAPI_t *cpifaceSession)
{
	cpifaceSession->UseChannels (cpifaceSession, drawchannel);
}
