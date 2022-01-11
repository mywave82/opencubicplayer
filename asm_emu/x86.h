/* OpenCP Module Player
 * copyright (c) '04-'10 Stian Skjelstad <stian@nixia.no>
 *
 * Emulation of x86 (ia32) instructions
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
#ifndef ASM_X86_H
#define ASM_X86_H 1

#include <math.h>
#include <string.h>
#include <stdio.h>

#define ASM_X86_INTERNAL_H

/* #define X86_AF */
/* #define X86_PF */
/* #define X86_FPU_EXCEPTIONS */

/* TODO */
#define FPU_TYPE long double
#define FPU_TYPE_FORMAT "%Lf"
#define FPU_REGS 8

struct reg32_t
{
	union
	{
		uint8_t values8[4];
		uint16_t values16[2];
		uint32_t values32[1];
	};
};

#define STACK_LENGTH 1024

typedef void (*writecallback_t)(uint_fast16_t selector, uint_fast32_t addr, int size, uint_fast32_t data);
typedef uint_fast32_t (*readcallback_t)(uint_fast16_t selector, uint_fast32_t addr, int size);

struct  __attribute__((packed)) assembler_state_t
{
	struct reg32_t _eax;
	struct reg32_t _ebx;
	struct reg32_t _ecx;
	struct reg32_t _edx;

	struct reg32_t _ebp;
	struct reg32_t _esp;

	struct reg32_t _esi;
	struct reg32_t _edi;
/*
	struct reg32_t eip; */

	struct reg32_t _eflags;

	uint16_t cs;
	uint16_t ds;
	uint16_t es;
	uint16_t fs;
	uint16_t gs;
	uint16_t ss;
	unsigned char stackmemory[STACK_LENGTH];

	uint16_t FPUControlWord;
	uint16_t FPUStatusWord;
	uint16_t FPUTagWord;
#ifdef X86_FPU_EXCEPTIONS
	uint32_t FPUDataPointerOffset;
	uint16_t FPUDataPointerSelector;
	uint32_t FPUInstructionPointerOffset;
	uint16_t FPUInstructionPointerSelector;
	uint16_t FPULastInstructionOpcode;
#endif
	FPU_TYPE FPUStack[FPU_REGS];

	writecallback_t write;
	readcallback_t read;
};

static inline void x86_write_memory(struct assembler_state_t *state, uint_fast16_t selector, uint_fast32_t addr, int size, uint_fast32_t data)
{
	if (selector < 4)
	{
		fprintf(stderr, "#GP exception occurred (zero-selector written to)\n");
		return;
	}
	if (selector == 4)
	{
		if ((addr+size) > STACK_LENGTH)
		{
			fprintf(stderr, "#SS exception occurred\n");
			return;
		}
		if (size==4)
		{
			state->stackmemory[addr] = data;
			state->stackmemory[addr+1] = data >> 8;
			state->stackmemory[addr+2] = data >> 16;
			state->stackmemory[addr+3] = data >> 24;
		}
		else if (size==2)
		{
			state->stackmemory[addr] = data;
			state->stackmemory[addr+1] = data >> 8;
		}
		else if (size==1)
		{
			state->stackmemory[addr] = data;
		}
	}
	state->write(selector, addr, size, data);
}

static inline uint_fast32_t x86_read_memory(struct assembler_state_t *state, uint_fast16_t selector, uint_fast32_t addr, int size)
{
	if (selector < 4)
	{
		fprintf(stderr, "#GP exception occurred (zero-selector red from)\n");
		return 0;
	}
	if (selector == 4)
	{
		if ((addr+size) > STACK_LENGTH)
		{
			fprintf(stderr, "#SS exception occurred\n");
			return state->stackmemory[addr];
		}
		if (size==4)
			return state->stackmemory[addr] | (state->stackmemory[addr+1]<<8) | (state->stackmemory[addr+2]<<16) | (state->stackmemory[addr+3]<<24);
		else if (size==2)
			return state->stackmemory[addr] | (state->stackmemory[addr+1]<<8);
		else if (size==1)
			return state->stackmemory[addr];
	}
	return state->read(selector, addr, size);
}

#ifdef WORDS_BIGENDIAN
#define offset_0 1
#define offset_1 0
#else
#define offset_0 0
#define offset_1 1
#endif

#define eax _eax.values32[0]
#define  ax _eax.values16[offset_0]
#define  al _eax.values8[offset_0]
#define  ah _eax.values8[offset_1]

#define ebx _ebx.values32[0]
#define  bx _ebx.values16[offset_0]
#define  bl _ebx.values8[offset_0]
#define  bh _ebx.values8[offset_1]

#define ecx _ecx.values32[0]
#define  cx _ecx.values16[offset_0]
#define  cl _ecx.values8[offset_0]
#define  ch _ecx.values8[offset_1]

#define edx _edx.values32[0]
#define  dx _edx.values16[offset_0]
#define  dl _edx.values8[offset_0]
#define  dh _edx.values8[offset_1]

#define ebp _ebp.values32[0]
#define  bp _ebp.values16[offset_0]

#define esp _esp.values32[0]
#define  sp _esp.values16[offset_0]

#define edi _edi.values32[0]
#define  di _edi.values16[offset_0]

#define esi _esi.values32[0]
#define  si _esi.values16[offset_0]

#define eflags _eflags.values32[0]
/* CARRY FLAG */
#define read_cf(flags) (!!(flags & 0x00000001))
#define write_cf(flags, state) flags = (flags & ~0x00000001) | ((state)?0x00000001:0)
#ifdef X86_PF
/* PARITY FLAG */
#define read_pf(flags) (!!(flags & 0x00000004))
#define write_pf(flags, state) flags = (flags & ~0x00000004) | ((state)?0x00000004:0)
#endif
#ifdef X86_AF
/* ADJUST FLAG */
#define read_af(flags) (!!(flags & 0x00000010))
#define write_af(flags, state) flags = (flags & ~0x00000010) | ((state)?0x00000010:0)
#endif
/* ZERO FLAG */
#define read_zf(flags) (!!(flags & 0x00000040))
#define write_zf(flags, state) flags = (flags & ~0x00000040) | ((state)?0x00000040:0)
/* SIGN FLAG */
#define read_sf(flags) (!!(flags & 0x00000080))
#define write_sf(flags, state) flags = (flags & ~0x00000080) | ((state)?0x00000080:0)
/* DIRECTION FLAG */
#define read_df(flags) (!!(flags & 0x00000200))
#define write_df(flags, state) flags = (flags & ~0x00000200) | ((state)?0x00000200:0)
/* OVERFLOW FLAG */
#define read_of(flags) (!!(flags & 0x00000400))
#define write_of(flags, state) flags = (flags & ~0x00000400) | ((state)?0x00000400:0)
#ifdef X86_PF
static inline void asm_update_pf(uint32_t *_eflags, const uint32_t reg)
{
	write_pf(*_eflags,
		(!!(ref&0x80))^
		(!!(reg&0x40))^
		(!!(reg&0x20))^
		(!!(reg&0x10))^
		(!!(reg&0x08))^
		(!!(reg&0x04))^
		(!!(reg&0x02))^
		(reg&0x01)^0x01);
}
#endif
#ifdef X86_AF
static inline void asm_update_af(uint32_t *_eflags, const uint32_t newreg, const uint32_t oldreg)
{

	write_af(*_eflags, (oldreg&0x10)&&((oldref&0xfffffff0)!=(newreg&0xfffffff0)));
}
#endif
/* B */
#define read_fpu_status_busy(status) (!!(status & 0x8000))
#define write_fpu_status_busy(status, state) status = (status & ~ 0x8000) | ((state)?0x8000:0)
/* C3 */
#define read_fpu_status_conditioncode3(status) (!!(status & 0x4000))
#define write_fpu_status_conditioncode3(status, state) status = (status & ~ 0x4000) | ((state)?0x4000:0)
/* C2 */
#define read_fpu_status_conditioncode2(status) (!!(status & 0x0400))
#define write_fpu_status_conditioncode2(status, state) status = (status & ~ 0x0400) | ((state)?0x0400:0)
/* C1 */
#define read_fpu_status_conditioncode1(status) (!!(status & 0x0200))
#define write_fpu_status_conditioncode1(status, state) status = (status & ~ 0x0200) | ((state)?0x0200:0)
/* C0 */
#define read_fpu_status_conditioncode0(status) (!!(status & 0x0100))
#define write_fpu_status_conditioncode0(status, state) status = (status & ~ 0x0100) | ((state)?0x0100:0)
/* TOP */
#define read_fpu_status_top(status) ((status & 0x3800)>>11)
#define write_fpu_status_top(status, state) status = (status & ~ 0x3800) | ((state<<11)&0x3800)
/* ES */
#define read_fpu_status_error_summary(status) (!!(status & 0x0040))
#define write_fpu_status_error_summary(status, state) status = (status & ~ 0x0040) | ((state)?0x0040:0)
/* SF */
#define read_fpu_status_stack_fault(status) (!!(status & 0x0040))
#define write_fpu_status_stack_fault(status, state) status = (status & ~ 0x0040) | ((state)?0x0040:0)
/* PE */
#define read_fpu_status_exception_precision(status) (!!(status & 0x0020))
#define write_fpu_status_exception_precision(status, state) status = (status & ~ 0x0020) | ((state)?0x0020:0)
/* UE */
#define read_fpu_status_exception_underflow(status) (!!(status & 0x0010))
#define write_fpu_status_exception_underflow(status, state) status = (status & ~ 0x0010) | ((state)?0x0010:0)
/* OE */
#define read_fpu_status_exception_overflow(status) (!!(status & 0x0008))
#define write_fpu_status_exception_overflow(status, state) status = (status & ~ 0x0008) | ((state)?0x0008:0)
/* ZE */
#define read_fpu_status_exception_zero_divide(status) (!!(status & 0x0004))
#define write_fpu_status_exception_zero_divide(status, state) status = (status & ~ 0x0004) | ((state)?0x0004:0)
/* DE */
#define read_fpu_status_exception_denormalized_operand(status) (!!(status & 0x0002))
#define write_fpu_status_exception_denormalized_operand(status, state) status = (status & ~ 0x0002) | ((state)?0x0002:0)
/* IE */
#define read_fpu_status_exception_invalid_operand(status) (!!(status & 0x0001))
#define write_fpu_status_exception_invalid_operand(status, state) status = (status & ~ 0x0001) | ((state)?0x0001:0)

/* X, this is actually a NC bit */
#define read_fpu_control_infinty(status) (!!(status & 0x1000))
#define write_fpu_control_infinty(status,state) status = (status & ~0x1000) | ((state)?0x1000:0)
/* RC */
#define read_fpu_control_rounding(status) ((status&0x0c00)>>10)
#define write_fpu_control_roudning(status,state) status = (status & ~0x0c000) | ((state & 0x0003)<<10)
#define RC_ROUND_TO_NEAREST 0x00
#define RC_ROUND_DOWN       0x01
#define RC_ROUND_UP         0x02
#define RC_ROUND_ZERO       0x03
/* PC */
#define read_fpu_control_precision(status) ((status&0x0300)>>8)
#define write_fpu_control_precision(status) status = (status & ~0x0300) | ((state & 0x0003)<<8)
#define PC_SINGLE_PRECISION          0x00
#define PC_RESERVED                  0x01
#define PC_DOUBLE_PRECISION          0x02
#define PC_DOUBLE_EXTENDED_PRECISION 0x03
/* PM */
#define read_fpu_control_exception_mask_precision(status) (!!(status&0x0020))
#define write_fpu_control_exception_mask_precision(status,state) status = (status & ~0x0020) | ((state)?0x0020:0)
/* UM */
#define read_fpu_control_exception_mask_underflow(status) (!!(status&0x0010))
#define write_fpu_control_exception_mask_underflow(status,state) status = (status & ~0x0010) | ((state)?0x0010:0)
/* OM */
#define read_fpu_control_exception_mask_overflow(status) (!!(status&0x0008))
#define write_fpu_control_exception_mask_overflow(status,state) status = (status & ~0x0008) | ((state)?0x0008:0)
/* ZM */
#define read_fpu_control_exception_mask_zero_divide(status) (!!(status&0x0004))
#define write_fpu_control_exception_mask_zero_divide(status,state) status = (status & ~0x0004) | ((state)?0x0004:0)
/* DM */
#define read_fpu_control_exception_mask_denormal_operand(status) (!!(status&0x0002))
#define write_fpu_control_exception_mask_denormal_operand(status,state) status = (status & ~0x0002) | ((state)?0x0002:0)
/* IM */
#define read_fpu_control_exception_mask_invalid_operation(status) (!!(status&0x0001))
#define write_fpu_control_exception_mask_invalid_operation(status,state) status = (status & ~0x0001) | ((state)?0x0001:0)

#define FPU_TAG_VALID   0x00
#define FPU_TAG_ZERO    0x01
#define FPU_TAG_SPECIAL 0x02
#define FPU_TAG_EMPTY   0x03
static inline int read_fpu_sub_tag(uint16_t status, int tag)
{
	if ((tag<0)||(tag>7))
	{
		fprintf(stderr, "read_fpu_sub_tag: invalid tag index\n");
		return 0;
	}
	return ((status>>tag)>>tag)&3;
}
static inline void write_fpu_sub_tag(uint16_t *status, int tag, int value)
{
	if ((tag<0)||(tag>7))
	{
		fprintf(stderr, "write_fpu_sub_tag: invalid tag index\n");
		return;
	}
	*status &= ~((3<<tag)<<tag);
	value &= 3;
	*status |= ((value<<tag)<<tag);
}
static inline int pop_fpu_sub_tag(uint16_t *status)
{
	if ((*status & 0x0003) == FPU_TAG_EMPTY)
	{
		fprintf(stderr, "pop_fpu_sub_tag: underflow exception occurred\n");
		return 0x0010;
	}
	*status = (*status)>>2;
	*status &= 0x3fff;
	*status |= (FPU_TAG_EMPTY<<14);
	return 0;
}
static inline int push_fpu_sub_tag(uint16_t *status, int value)
{
	if ((*status & 0xc000) != (FPU_TAG_EMPTY<<14))
	{
		fprintf(stderr, "push_fpu_sub_tag: overflow exception occurred\n");
		return 0x0010;
	}
	if ((value < 0) || (value > 2)) /* this should never happen */
	{
		fprintf(stderr, "push_fpu_sub_tag: invalid operation occurred\n");
		return 0x0001;
	}
	*status = (*status)<<2;
	*status |= (uint16_t)value;
	return 0;
}

static inline FPU_TYPE read_fpu_st(struct assembler_state_t *state, int index)
{
	int offset = (read_fpu_status_top(state->FPUStatusWord)+index) & 7;
	return state->FPUStack[offset];
}
static inline void write_fpu_st(struct assembler_state_t *state, int index, FPU_TYPE data)
{
	int offset = (read_fpu_status_top(state->FPUStatusWord)+index) & 7;
	state->FPUStack[offset] = data;
}
static inline FPU_TYPE read_fpu_r(struct assembler_state_t *state, int index)
{
	return state->FPUStack[index&7];
}
static inline void write_fpu_r(struct assembler_state_t *state, int index, FPU_TYPE data)
{
	state->FPUStack[index&7] = data;
}

static inline void asm_finit(struct assembler_state_t *state);
static inline void init_assembler_state(struct assembler_state_t *state, writecallback_t write, readcallback_t read)
{
	memset(state, 0, sizeof(*state));

	state->cs = state->ds = state->es = state->fs = state->gs = 8;
	state->ss = 4;
	state->esp=STACK_LENGTH;
	state->ebp=STACK_LENGTH;

	state->write = write;
	state->read = read;

	asm_finit(state);
}

static inline void asm_movl(struct assembler_state_t *state, uint32_t src, uint32_t *dst) {*dst = src;};
static inline void asm_movw(struct assembler_state_t *state, uint16_t src, uint16_t *dst) {*dst = src;};
static inline void asm_movb(struct assembler_state_t *state, uint8_t src, uint8_t *dst) {*dst = src;};

static inline void asm_leal(struct assembler_state_t *state, uint32_t src, uint32_t *dst) {*dst = src;};
static inline void asm_leaw(struct assembler_state_t *state, uint16_t src, uint16_t *dst) {*dst = src;};
static inline void asm_leab(struct assembler_state_t *state, uint8_t src, uint8_t *dst) {*dst = src;};



#include "asm_emu/x86_add.h"
#include "asm_emu/x86_adc.h"
#include "asm_emu/x86_and.h"
#include "asm_emu/x86_cmp.h"
#include "asm_emu/x86_dec.h"
#include "asm_emu/x86_div.h"
#include "asm_emu/x86_fadd.h"
#include "asm_emu/x86_faddp.h"
#include "asm_emu/x86_fcom.h"
#include "asm_emu/x86_fcomp.h"
#include "asm_emu/x86_fcompp.h"
#include "asm_emu/x86_fdiv.h"
#include "asm_emu/x86_fdivp.h"
#include "asm_emu/x86_fiadd.h"
#include "asm_emu/x86_fidiv.h"
#include "asm_emu/x86_fimul.h"
#include "asm_emu/x86_finit.h"
#include "asm_emu/x86_fisub.h"
#include "asm_emu/x86_fisubr.h"
#include "asm_emu/x86_fistp.h"
#include "asm_emu/x86_fld.h"
#include "asm_emu/x86_fldz.h"
#include "asm_emu/x86_fmul.h"
#include "asm_emu/x86_fmulp.h"
#include "asm_emu/x86_fst.h"
#include "asm_emu/x86_fstp.h"
#include "asm_emu/x86_fstsw.h"
#include "asm_emu/x86_fsub.h"
#include "asm_emu/x86_fsubp.h"
#include "asm_emu/x86_fsubr.h"
#include "asm_emu/x86_fsubrp.h"
#include "asm_emu/x86_fxch.h"
#include "asm_emu/x86_imul.h"
#include "asm_emu/x86_inc.h"
#include "asm_emu/x86_neg.h"
#include "asm_emu/x86_sar.h"
#include "asm_emu/x86_sahf.h"
#include "asm_emu/x86_shl.h"
#include "asm_emu/x86_shld.h"
#include "asm_emu/x86_shr.h"
#include "asm_emu/x86_sbb.h"
#include "asm_emu/x86_stos.h"
#include "asm_emu/x86_sub.h"
#include "asm_emu/x86_test.h"
#include "asm_emu/x86_or.h"
#include "asm_emu/x86_pop.h"
#include "asm_emu/x86_push.h"
#include "asm_emu/x86_xor.h"

#define asm_jmp(state,label)                                                                                          goto label /* Jump */
#define asm_ja(state,label)   if ((!read_cf((state)->eflags)&&(!read_zf((state)->eflags))))                          goto label /* Jump if above (CF=0 and ZF=0) */
#define asm_jae(state,label)  if (!read_cf((state)->eflags))                                                         goto label /* Jump if above or equal (CF=0) */
#define asm_jb(state,label)   if (read_cf((state)->eflags))                                                          goto label /* Jump if below (CF=1) */
#define asm_jbe(state,label)  if (read_cf((state)->eflags)||read_zf((state)->eflags))                                goto label /* Jump if below or equal (CF=1 or ZF=1) */
#define asm_jc(state,label)   if (read_cf((state)->eflags))                                                          goto label /* Jump if carry (CF=1) */
/*#define asm_jcxz(regs,label) TODO Jump if CX=0, 0x66 prefix dependen */
/*#define asm_jecxz(regs,label) TODO Jump if ECX=0, 0x66 prefix dependent */
#define asm_je(state,label)   if (read_zf((state)->eflags))                                                          goto label /* Jump if equal (ZF=1) */
#define asm_jg(state,label)   if ((!read_zf((state)->eflags))&&(read_of((state)->eflags)==read_sf((state)->eflags))) goto label /* Jump if greater (ZF=0 and SF=OF) */
#define asm_jge(state,label)  if (read_of((state)->eflags)==read_sf((state)->eflags))                                goto label /* Jump if greater or equal (SF=OF) */
#define asm_jl(state,label)   if (read_of((state)->eflags)!=read_sf((state)->eflags))                                goto label /* Jump if less (SF!=OF) */
#define asm_jle(state,label)  if (read_zf((state)->eflags)||(read_of((state)->eflags)!=read_sf((state)->eflags)))    goto label /* Jump if less or equal (ZF=1 or SF!=OF) */
#define asm_jna(state,label)  if (read_cf((state)->eflags)||read_zf((state)->eflags))                                goto label /* Jump if not above (CF=1 or ZF=1) */
#define asm_jnae(state,label) if (read_cf((state)->eflags))                                                          goto label /* Jump if not above or equal (CF=1) */
#define asm_jnb(state,label)  if (!read_cf((state)->eflags))                                                         goto label /* Jump if not below (CF=0) */
#define asm_jnbe(state,label) if ((!read_cf((state)->eflags)&&(!read_zf((state)->eflags))))                          goto label /* Jump if not below or equal (CF=0 and ZF=0) */
#define asm_jnc(state,label)  if (!read_cf((state)->eflags))                                                         goto label /* Jump if not carry (CF=0) */
#define asm_jne(state,label)  if (!read_zf((state)->eflags))                                                         goto label /* Jump if not equal (ZF=0) */
#define asm_jng(state,label)  if (read_zf((state)->elfags)||(read_of((state)->eflags)!=read_sf((state)->eflags)))    goto label /* Jump if not greater (ZF=1 or SF!=OF) */
#define asm_jnge(state,label) if (read_of((state)->eflags)!=read_sf((state)->eflags))                                goto label /* Jump if not greater of equal (SF!=OF) */
#define asm_jnl(state,label)  if (read_of((state)->eflags)==read_sf((state)->eflags))                                goto label /* Jump if not less (SF=OF) */
#define asm_jnle(state,label) if ((!read_zf((state)->eflags))&&read_of((state)->eflags)==read_sf((state)->eflags))   goto label /* Jump if not less or equal (ZF=0 and SF==OF) */
#define asm_jno(state,label)  if (!read_of((state)->eflags))                                                         goto label /* Jump if not overflow (OF=0) */
#ifdef X86_PF
#define asm_jnp(state,label)  if (!read_pf((state)->eflags))                                                         goto label /* Jump if not parity (PF=0) */
#endif
#define asm_jns(state,label)  if (!read_sf((state)->eflags))                                                         goto label /* Jump if not sign (SF=0) */
#define asm_jnz(state,label)  if (!read_zf((state)->eflags))                                                         goto label /* Jump if not zero (ZF=0) */
#define asm_jo(state,label)   if (read_of((state)->eflags))                                                          goto label /* Jump if overflow (OF=1) */
#ifdef X86_PF
#define asm_jp(state,label)   if (read_pf((state)->eflags))                                                          goto label /* Jump if parity (PF=1) */
#define asm_jpe(state,label)  if (read_pf((state)->eflags))                                                          goto label /* Jump if parity even (PF=1) */
#define asm_jpo(state,label)  if (!read_pf((state)->eflags))                                                         goto label /* Jump if parity odd (PF=0) */
#endif
#define asm_js(state,label)   if (read_sf((state)->eflags))                                                          goto label /* Jump if signed (SF=1) */
#define asm_jz(state,label)   if (read_zf((state)->eflags))                                                          goto label /* Jump if zero (ZF=1) */

#undef ASM_X86_INTERNAL_H

#endif
