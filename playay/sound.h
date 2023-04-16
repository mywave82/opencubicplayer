/* aylet 0.2, a .AY music file player.
 * Copyright (C) 2001 Russell Marks and Ian Collier. See main.c for licence.
 *
 * sound.h
 */

#define sound_freq playay_sound_freq

struct ay_driver_frame_state_t
{
	uint32_t clockrate;

	uint16_t channel_a_period; /* 0x0001 -> 0x0fff */
	uint16_t channel_b_period; /* 0x0001 -> 0x0fff */
	uint16_t channel_c_period; /* 0x0001 -> 0x0fff */

	uint8_t noise_period; /* 0x01 -> 0x1f  */
	uint8_t mixer; /* __RRRTTT R=!random enable, T=!note enable.. R is mixed into T */

	uint8_t amplitude_a; /* ___Evvvv E = use envelope, else vvvv=fixed value */
	uint8_t amplitude_b; /* ___Evvvv E = use envelope, else vvvv=fixed value */
	uint8_t amplitude_c; /* ___Evvvv E = use envelope, else vvvv=fixed value */

	uint16_t envelope_period;
	uint8_t envelope_shape; /* ___ssss */
};

OCP_INTERNAL int sound_init (void);
OCP_INTERNAL void sound_end (void);
OCP_INTERNAL int sound_frame (struct ay_driver_frame_state_t *states);
OCP_INTERNAL void sound_frame_blank (void);
OCP_INTERNAL void sound_start_fade (int fadetime_in_sec);
OCP_INTERNAL void sound_ay_write (int reg, int val, unsigned long tstates);
OCP_INTERNAL void sound_ay_reset (void);
OCP_INTERNAL void sound_ay_reset_cpc (void);
OCP_INTERNAL void sound_beeper (int on, unsigned long tstates);
extern OCP_INTERNAL unsigned int sound_freq;
