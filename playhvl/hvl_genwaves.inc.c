/* OpenCP Module Player
 * copyright (c) 2019-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Wave-forms generation routines. Heavily based on https://github.com/pete-gordon/hivelytracker/tree/master/hvl2wav
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

static void hvl_GenSawtooth (int8_t *buf, uint32_t len)
{
	uint32_t i;
	int32_t  val, add;

	add = 256 / (len - 1);
	val = -128;

	for ( i=0; i < len; i++, val += add )
	{
		*buf++ = (int8_t)val;
	}
}

static void hvl_GenTriangle (int8_t *buf, uint32_t len)
{
	uint32_t  i;
	int32_t   d2, d5, d1, d4;
	int32_t   val;
	int8_t   *buf2;

	d2  = len;
	d5  = len >> 2;
	d1  = 128/d5;
	d4  = -(d2 >> 1);
	val = 0;

	for ( i=0; i < d5; i++)
	{
		*buf++ = val;
		val += d1;
	}
	*buf++ = 0x7f;

	if( d5 != 1 )
	{
		val = 128;
		for ( i=0; i<d5-1; i++ )
		{
			val -= d1;
			*buf++ = val;
		}
	}

	buf2 = buf + d4;
	for ( i=0; i<d5*2; i++ )
	{
		int8_t c;

		c = *buf2++;
		if ( c == 0x7f )
		{
			c = 0x80;
		} else {
			c = -c;
		}
		*buf++ = c;
	}
}

static void hvl_GenSquare (int8_t *buf)
{
	uint32_t i, j;

	for( i=1; i<=0x20; i++ )
	{
		for ( j=0; j < (0x40-i)*2; j++ )
		{
			*buf++ = 0x80;
		}
		for ( j=0; j < i*2; j++ )
		{
			*buf++ = 0x7f;
		}
	}
}

static inline double clip (double x)
{
	if( x > 127.f )
	{
		x = 127.f;
	} else if( x < -128.f )
	{
		x = -128.f;
	}
	return x;
}

static void hvl_GenFilterWaves (int8_t *buf, int8_t *lowbuf, int8_t *highbuf)
{
	static const uint16_t lentab[45] =
	{
		0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f,
		0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f,
		0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
		0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
		0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
		0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
		0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
		0x7f, 0x7f,
		(0x280*3)-1
	};

	double freq;
	uint32_t  temp;

	for ( temp=0, freq=8.f; temp<31; temp++, freq+=3.f )
	{
		uint32_t wv;
		int8_t   *a0 = buf;

		for ( wv=0; wv<6+6+0x20+1; wv++ )
		{
			double  fre, high, mid, low;
			uint32_t i;

			mid = 0.f;
			low = 0.f;
			fre = freq * 1.25f / 100.0f;

			for ( i=0; i<=lentab[wv]; i++ )
			{
				high  = a0[i] - mid - low;
				high  = clip( high );
				mid  += high * fre;
				mid   = clip( mid );
				low  += mid * fre;
				low   = clip( low );
			}

			for ( i=0; i<=lentab[wv]; i++ )
			{
				high  = a0[i] - mid - low;
				high  = clip( high );
				mid  += high * fre;
				mid   = clip( mid );
				low  += mid * fre;
				low   = clip( low );
				*lowbuf++  = (int8_t)low;
				*highbuf++ = (int8_t)high;
			}

			a0 += lentab[wv]+1;
		}
	}
}

static void hvl_GenWhiteNoise (int8_t *buf, uint32_t len)
{
	uint32_t ays;

	ays = 0x41595321;

	do
	{
		uint16_t ax, bx;
		int8_t s;

		s = ays;

		if ( ays & 0x100 )
		{
			s = 0x80;

			if ( (int32_t)(ays & 0xffff) >= 0 )
			{
				s = 0x7f;
			}
		}

		*buf++ = s;
		len--;

		ays = (ays >> 5) | (ays << 27);
		ays = (ays & 0xffffff00) | ((ays & 0xff) ^ 0x9a);
		bx  = ays;
		ays = (ays << 2) | (ays >> 30);
		ax  = ays;
		bx  += ax;
		ax  ^= bx;
		ays  = (ays & 0xffff0000) | ax;
		ays  = (ays >> 3) | (ays << 29);
	} while ( len );
}
