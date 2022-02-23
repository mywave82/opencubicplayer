/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * auxiliary assembler routines for no sound wavetable device
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
 *
 * revision history: (please note changes here)
 *  -nb980510   Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *    -first release
 *  -ss04????   Stian Skjelstad <stian@nixia.no>
 *    -assembler rewritten to gcc
 *  -ss040908   Stian Skjelstad <stian@nixia.no>
 *    -minor changes to make gcc able to compile, even with VERY high optimize level
 *  -doj040914  Dirk Jagdmann  <doj@cubic.org>
 *    -push/pop %ebp, and don't flag is as dirty in nonePlayChannel
 */

/* Butt ugly, and no usage of the fpos ... pure 32 bit for now */

void nonePlayChannel(unsigned long len, struct channel *ch)
{
	unsigned int r;

	if (!(ch->status&MIX_PLAYING))
		return;
	if ((!ch->step)||(!len))
		return; /* if no length, or pitch is zero.. we don't need to simulate shit*/
	while(len)
	{
		if (ch->step<0)
		{
			int32_t t = -ch->step;
			unsigned int tmp=ch->fpos;
			r = t>>16;
			tmp-=t&0xffff;
			if (tmp>0xffff)
				r++;
			ch->fpos=(uint16_t)tmp;
		} else {
			unsigned int tmp=ch->fpos;
			r = ch->step>>16;
			tmp+=ch->step&0xffff;
			if (tmp>0xffff)
				r++;
			ch->fpos=(uint16_t)tmp;
		}

		while (r)
		{
			if (ch->step<0)
			{ /* ! forward */
				if (ch->pos-r<ch->loopstart)
				{
					r-=ch->pos-ch->loopstart;
					ch->pos=ch->loopstart;
					ch->step*=-1;
				} else {
					ch->pos-=r;
					r=0;
				}
			} else {
				if (ch->status&MIX_LOOPED)
				{
					if (ch->pos+r>ch->loopend)
					{
						r-=ch->loopend-ch->pos;
						if (ch->status&MIX_PINGPONGLOOP)
						{
							ch->pos=ch->loopend;
							ch->step*=-1;
						} else {
							ch->pos=ch->loopstart;
						}
					} else {
						ch->pos+=r;
						r=0;
					}
				} else {
					if (ch->pos+r>ch->length)
					{
						ch->pos=0;
						ch->fpos=0;
						ch->step=0;
						r=0;
						len=1;
					} else {
						ch->pos+=r;
						r=0;
					}
				}
			}
		}
		len--;
	}
}
