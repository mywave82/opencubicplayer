/* Emulation of the Z80 CPU with hooks into the other parts of aylet.
 * Copyright (C) 1994-2010 Ian Collier. aylet changes (C) 2001-2002 Russell Marks.
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

#define NO_CURSES
#include "config.h"
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include "types.h"
#include "main.h"
#include "z80.h"

#define parity(a) (partable[a])

static uint8_t partable[256]={
      4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4,
      0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
      0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
      4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4,
      0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
      4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4,
      4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4,
      0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
      0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
      4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4,
      4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4,
      0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
      4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4,
      0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
      0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
      4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4
   };

static uint8_t a, f, b, c, d, e, h, l;
static uint8_t r, a1, f1, b1, c1, d1, e1, h1, l1, i, iff1, iff2, im;
static uint16_t pc;
static uint16_t ix, iy, sp;
static uint32_t radjust;
static uint32_t ixoriy, new_ixoriy;
static uint32_t intsample;
static uint8_t op;
static int interrupted;

void __attribute__ ((visibility ("internal"))) ay_z80_init(unsigned char *data,unsigned char *stacketc)
{
a=f=b=c=d=e=h=l=a1=f1=b1=c1=d1=e1=h1=l1=i=r=iff1=iff2=im=0;
ixoriy=new_ixoriy=0;
ix=iy=sp=pc=0;
ay_tstates=0;
radjust=0;

a=a1=b=b1=d=d1=h=h1=data[8];
f=f1=c=c1=e=e1=l=l1=data[9];
ix=iy=hl;
interrupted=0;

sp=stacketc[0]*256+stacketc[1];
}

void __attribute__ ((visibility ("internal"))) ay_z80loop(void)
{
  while (ay_tstates<ay_tsmax)
  {
  ixoriy=new_ixoriy;
  new_ixoriy=0;
  intsample=1;
  op=fetch(pc);
  pc++;
  radjust++;
  switch(op)
    {
#include "z80ops.c"
    }

  if(interrupted && intsample && iff1)
    {
    interrupted=0;
    if(fetch(pc)==0x76)pc++;
    iff1=iff2=0;
    ay_tstates+=5; /* accompanied by an input from the data bus */
    switch(im)
      {
      case 0: /* IM 0 */
      case 1: /* undocumented */
      case 2: /* IM 1 */
        /* there is little to distinguish between these cases */
        ay_tstates+=7; /* perhaps */
        push2(pc);
        pc=0x38;
        break;
      case 3: /* IM 2 */
        ay_tstates+=13; /* perhaps */
        {
        int addr=fetch2((i<<8)|0xff);
        push2(pc);
        pc=addr;
        }
      }
    }
  }
  ay_do_interrupt();
  ay_tstates-=ay_tsmax;
  interrupted=1;
}
