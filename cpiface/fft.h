/* OpenCP Module Player
 * copyright (c) '94-'05 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * FFT routines for the spectrum analysers
 *
 * revision history: (please note changes here)
 * -doj980928  Dirk Jagdmann <doj@cubic.org>
 *   -initial release
 *
 * -doj981105  Dirk Jagdmann <doj@cubic.org>
 *   - changed paramter declaration of fftanalyseall()
 *   - deletet isqrt() from this file
 */

#ifndef FFT__H
#define FFT__H

void fftanalyseall(uint16_t *ana,
                   const int16_t *samp,
                   const int inc,
                   const int bits);

#endif
