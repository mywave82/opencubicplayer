/* Miscellaneous definitions for xz80, copyright (C) 1994 Ian Collier.
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

#define Z80_quit  1
#define Z80_NMI   2
#define Z80_reset 3
#define Z80_load  4
#define Z80_save  5
#define Z80_log   6

extern void __attribute__ ((visibility ("internal"))) ay_z80_init(unsigned char *data,unsigned char *stacketc);
extern void __attribute__ ((visibility ("internal"))) ay_z80loop(void);

#define fetch(x) (ay_mem[x])
#define fetch2(x) ((fetch(((x)+1)&0xffff)<<8)|fetch(x))

#define store(x,y) do {\
		ay_mem[x]=(y); \
		} while(0)

#define store2b(x,hi,lo) do {\
		ay_mem[x]=lo; \
		ay_mem[(x+1)&0xffff]=hi; \
		} while(0)

#define store2(x,y) store2b(x,(y)>>8,(y)&255)

#ifdef __GNUC__
static inline void storefunc(uint16_t ad, uint8_t b){
   store(ad,b);
}
#undef store
#define store(x,y) storefunc(x,y)

static inline void store2func(uint16_t ad,unsigned char b1,uint8_t b2){
   store2b(ad,b1,b2);
}
#undef store2b
#define store2b(x,hi,lo) store2func(x,hi,lo)
#endif

#define bc ((b<<8)|c)
#define de ((d<<8)|e)
#define hl ((h<<8)|l)
