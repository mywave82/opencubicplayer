/* OpenCP Module Player
 * copyright (c) '04-'10 Stian Skjelstad <stian@nixia.no>
 *
 * Unit test for asm_emu/x86*.h
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
#include "types.h"
#include "x86.h"

static struct assembler_state_t state;
static int failed = 0;

static void writecallback(uint_fast16_t selector, uint_fast32_t addr, int size, uint_fast32_t data)
{
}
static uint_fast32_t readcallback(uint_fast16_t selector, uint_fast32_t addr, int size)
{
	return 0;
}

static void adcb(void)
{
	int i;
	int j;

	fprintf(stderr, "adcb x, y:\n");
	for (i=0;i<=255;i++)
	for (j=0;j<=255;j++)
	{
		write_cf(state.eflags, 0);
		state.al=i;
		state.bl=j;
		asm_adcb(&state, state.al, &state.bl);
		if ((i+j != state.bl) ^ read_cf(state.eflags))
		{
			failed++;
			fprintf(stderr, "  unsigned adcb failed: %d+%d+0=%d CF=%d\n", j, i, state.bl, read_cf(state.eflags));
		}
		if (((signed int)((signed char)i)+(signed int)((signed char)j) != (signed char)state.bl) ^ read_of(state.eflags))
		{
			failed++;
			fprintf(stderr, "  signed adcb failed: %d+%d+0=%d OF=%d\n", (signed char)j, (signed char)i, (signed char)state.bl, read_of(state.eflags));
		}

		write_cf(state.eflags, 1);
		state.al=i;
		state.bl=j;
		asm_adcb(&state, state.al, &state.bl);
		if ((i+j+1 != state.bl) ^ read_cf(state.eflags))
		{
			failed++;
			fprintf(stderr, "  unsigned adcb failed: %d+%d+1=%d CF=%d\n", j, i, state.bl, read_cf(state.eflags));
		}
		if (((signed int)((signed char)i)+(signed int)((signed char)j)+1 != (signed char)state.bl) ^ read_of(state.eflags))
		{
			failed++;
			fprintf(stderr, "  signed adcb failed: %d+%d+1=%d OF=%d\n", (signed char)j, (signed char)i, (signed char)state.bl, read_of(state.eflags));
		}

	}
}

static void addb(void)
{
	int i;
	int j;

	fprintf(stderr, "addb x, y:\n");
	for (i=0;i<=255;i++)
	for (j=0;j<=255;j++)
	{
		state.al=i;
		state.bl=j;
		asm_addb(&state, state.al, &state.bl);
		if ((i+j != state.bl) ^ read_cf(state.eflags))
		{
			failed++;
			fprintf(stderr, "  unsigned addb failed: %d+%d=%d CF=%d\n", j, i, state.bl, read_cf(state.eflags));
		}
		if (((signed int)((signed char)i)+(signed int)((signed char)j) != (signed char)state.bl) ^ read_of(state.eflags))
		{
			failed++;
			fprintf(stderr, "  signed addb failed: %d+%d=%d OF=%d\n", (signed char)j, (signed char)i, (signed char)state.bl, read_of(state.eflags));
		}
	}
}

static void divl(void)
{
	uint64_t i; int donei=0; int oldi;
	uint_fast32_t j; int donej=0; int oldj;
	uint32_t tmp;

	fprintf(stderr, "divl y:\n");
	for (i=0;!donei;)
	{
		donej=0;
		for (j=0;!donej;)
		{
			state.edx=i>>32;
			state.eax=i;
			if (state.edx<j)
			{
				state.ebx = j;
				asm_divl(&state, state.ebx);
				tmp = i/j;
				if (state.eax != tmp)
				{
					failed++;
					fprintf(stderr, "  divl failed: %lld/%d=%d EXPECTED %d\n", i, j, state.eax, tmp);
				}
			}
			oldj = j;
			if (j & 1)
				j+=0x7fffff;
			else
				j++;
			donej = j < oldj;
		}
		oldi = i;
		if (i & 1)
			i+=0x7fffffffffffLL;
		else
			i++;
		donei = i < oldi;
	}
}

static void imul_1_b(void)
{
	int i;
	int j;
	int tmp;
	int tmp2;

	fprintf(stderr, "imul_1_b x:\n");
	for (i=0;i<=255;i++)
	for (j=0;j<=255;j++)
	{
		state.al=i;
		asm_imul_1_b(&state, j);
		tmp = (int8_t)i*(int8_t)j;
		tmp2 = (tmp<-256)||(tmp>255); /* x86 specs seems a bit wierd here at this point, since overflow/carry flag reflect the sign of the result*/
		if (((int16_t)state.ax != tmp) || (read_cf(state.eflags) != tmp2) || (read_of(state.eflags) != tmp2))
		{
			failed++;
			fprintf(stderr, "  imul_1_b failed: %d*%d=%d OF=%d CF=%d EXPECTED %d*%d=%d OF=%d CF=%d\n", (signed char)i, (signed char)j, (int16_t)state.ax, read_of(state.eflags), read_cf(state.eflags), (signed char)i, (signed char)j, tmp, tmp2, tmp2);
		}
	}
}

static void imul_1_w(void)
{
	int i;
	int j;
	int_fast32_t tmp;
	int tmp2;
	uint16_t tmp3, tmp4;

	fprintf(stderr, "imul_1_w x:\n");
	for (i=0;i<=65536;)
	{
		for (j=0;j<=65536;)
		{
			state.ax=i;
			asm_imul_1_w(&state, j);
			tmp = ((int_fast32_t)(int16_t)i)*(int16_t)j;
			tmp3 = (unsigned)tmp&0xffff;
			tmp4 = (unsigned)tmp>>16;
			tmp2 = (tmp<-65536)||(tmp>65535); /* x86 specs seems a bit wierd here at this point, since overflow/carry flag reflect the sign of the result*/
			if ((state.ax != tmp3) || (state.dx != tmp4) || (read_cf(state.eflags) != tmp2) || (read_of(state.eflags) != tmp2))
			{
				failed++;
				fprintf(stderr, "  imul_1_w failed: %d*%d=%d OF=%d CF=%d EXPECTED %d*%d=%d OF=%d CF=%d\n", (int16_t)i, (int16_t)j, (((int)state.ax<<16) | state.dx), read_of(state.eflags), read_cf(state.eflags), (int16_t)i, (int16_t)j, tmp, tmp2, tmp2);
			}

			if (j & 1)
				j+=127;
			else
				j++;

		}
		if (i & 1)
			i+=127;
		else
			i++;
	}
}

static void imul_1_l(void)
{
	int i; int donei=0; int oldi;
	int j; int donej=0; int oldj;
	int_fast64_t tmp;
	int tmp2;
	uint32_t tmp3, tmp4;

	fprintf(stderr, "imul_1_l x:\n");
	for (i=0;!donei;)
	{
		donej=0;
		for (j=0;!donej;)
		{
			state.eax=i;
			asm_imul_1_l(&state, j);
			tmp = ((int_fast64_t)(int32_t)i)*(int32_t)j;
			tmp3 = (uint_fast64_t)tmp&0xffffffff;
			tmp4 = (uint_fast64_t)tmp>>32;
			if ((tmp & 0xffffffff00000000LL) == 0x0000000000000000LL)
				tmp2 = 0;
			else if ((tmp & 0xffffffff00000000LL) == 0xffffffff00000000LL)
				tmp2 = 0;
			else
				tmp2 = 1;
			if ((state.eax != tmp3) || (state.edx != tmp4) || (read_cf(state.eflags) != tmp2) || (read_of(state.eflags) != tmp2))
			{
				failed++;
				fprintf(stderr, "  imul_1_l failed: %d*%d=%lld OF=%d CF=%d EXPECTED %d*%d=%lld OF=%d CF=%d\n", (int32_t)i, (int32_t)j, (((uint64_t)state.ax<<32) | state.edx), read_of(state.eflags), read_cf(state.eflags), (int32_t)i, (int32_t)j, tmp, tmp2, tmp2);
			}
			oldj = j;
			if (j & 1)
				j+=0x7fffff;
			else
				j++;
			donej = j < oldj;
		}
		oldi = i;
		if (i & 1)
			i+=32767;
		else
			i++;
		donei = i < oldi;
	}
}

static void imul_2_l(void)
{
	int i; int donei=0; int oldi;
	int j; int donej=0; int oldj;
	int_fast64_t tmp;
/*
	int tmp2;*/
	uint32_t tmp3, tmp4;

	fprintf(stderr, "imul_2_l x,y:\n");
	for (i=0x80000000;!donei;)
	{
		donej=0;
		for (j=0x80000000;!donej;)
		{
			state.eax=i;
			asm_imul_2_l(&state, j, &state.eax);
			tmp = ((int_fast64_t)(int32_t)i)*(int32_t)j;
			tmp3 = (uint_fast64_t)tmp&0xffffffff;
			tmp4 = (uint_fast64_t)tmp>>32;
			if ((tmp4 == 0x00000000) || (tmp4 == 0xffffffff)) /* we ignore overruns for now */
			{
/*
			if ((tmp & 0xffffffff00000000LL) == 0x0000000000000000LL)
				tmp2 = 0;
			else if ((tmp & 0xffffffff00000000LL) == 0xffffffff00000000LL)
				tmp2 = 0;
			else
				tmp2 = 1;*/
				if ((state.eax != tmp3)/* || (state.edx != tmp4) || (read_cf(state.eflags) != tmp2) || (read_of(state.eflags) != tmp2)*/)
				{
					failed++;
					fprintf(stderr, "  imul_2_l failed: %d*%d=%d EXPECTED %d*%d=%d\n", (int32_t)i, (int32_t)j, state.eax, (int32_t)i, (int32_t)j, (int32_t)tmp);
				}
			}
			oldj = j;
			if (j & 1)
				j+=0x7fffff;
			else
				j++;
			donej = j < oldj;
		}
		oldi = i;
		if (i & 1)
			i+=32767;
		else
			i++;
		donei = i < oldi;
	}
}

static void sarl(void)
{
	int i;
	fprintf(stderr, "sarl x, y:\n");
	for (i=0;i<=32;i++)
	{
		uint32_t tmp;
		state.eax = 0x10000000;
		if (i==0)
			tmp = state.eax;
		else if (i==32)
			tmp = state.eax; /* wrap... this is kind of undefined on the x86 platform */
		else
			tmp = (state.eax >> i);
		asm_sarl(&state, i, &state.eax);
		if (tmp != state.eax)
		{
			fprintf(stderr, " shll %d, 0x00000001 failed, expected 0x%08x, got 0x%08x\n", i, (int)tmp, (int)state.ebx);
			failed++;
		}
	}
}

static void sbbb(void)
{
	int i;
	int j;

	fprintf(stderr, "sbbb x, y:\n");
	for (i=0;i<=255;i++)
	for (j=0;j<=255;j++)
	{
		write_cf(state.eflags, 0);
		state.al=i;
		state.bl=j;
		asm_sbbb(&state, state.al, &state.bl);
		if ((j-i != state.bl) ^ read_cf(state.eflags))
		{
			failed++;
			fprintf(stderr, "  unsigned sbbb failed: %d-%d-0=%d CF=%d\n", j, i, state.bl, read_cf(state.eflags));
		}
		if (((signed int)((signed char)j)-(signed int)((signed char)i) != (signed char)state.bl) ^ read_of(state.eflags))
		{
			failed++;
			fprintf(stderr, "  signed sbbb failed: %d-%d-0=%d OF=%d\n", (signed char)j, (signed char)i, (signed char)state.bl, read_of(state.eflags));
		}

		write_cf(state.eflags, 1);
		state.al=i;
		state.bl=j;
		asm_sbbb(&state, state.al, &state.bl);
		if ((j-i-1 != state.bl) ^ read_cf(state.eflags))
		{
			failed++;
			fprintf(stderr, "  unsigned sbbb failed: %d-%d-1=%d CF=%d\n", j, i, state.bl, read_cf(state.eflags));
		}
		if (((signed int)((signed char)j)-(signed int)((signed char)i)-1 != (signed char)state.bl) ^ read_of(state.eflags))
		{
			failed++;
			fprintf(stderr, "  signed sbbb failed: %d-%d-1=%d OF=%d\n", (signed char)j, (signed char)i, (signed char)state.bl, read_of(state.eflags));
		}
	}
}

static void shll(void)
{
	int i;
	fprintf(stderr, "shll x, y:\n");
	for (i=0;i<=32;i++)
	{
		uint32_t tmp;
		state.eax = 0x00000001;
		if (i==0)
			tmp = state.eax;
		else if (i==32)
			tmp = state.eax; /* wrap... this is kind of undefined on the x86 platform */
		else
			tmp = (state.eax << i);
		asm_shll(&state, i, &state.eax);
		if (tmp != state.eax)
		{
			fprintf(stderr, " shll %d, 0x00000001 failed, expected 0x%08x, got 0x%08x\n", i, (int)tmp, (int)state.ebx);
			failed++;
		}
	}
}

static void shldl(void)
{
	int i;
	fprintf(stderr, "shldl x, y, z:\n");
	for (i=0;i<=32;i++)
	{
		uint32_t tmp;
		state.eax = 0x80000000;
		state.ebx = 0x00001000;
		if (i==0)
			tmp = state.ebx;
		else if (i==32)
			tmp = state.ebx; /* wrap... this is kind of undefined on the x86 platform */
		else
			tmp = (state.ebx << i)|(state.eax>>(32-i));
		asm_shldl(&state, i, state.eax, &state.ebx);
		if (tmp != state.ebx)
		{
			fprintf(stderr, " shldl %d, 0x%08x, 0x%08x failed, expected 0x%08x, got 0x%08x\n", i, 0x80000000, 0x00001000, (int)tmp, (int)state.ebx);
			failed++;
		}
	}
}

static void subb(void)
{
	int i;
	int j;

	fprintf(stderr, "subb x, y:\n");
	for (i=0;i<=255;i++)
	for (j=0;j<=255;j++)
	{
		state.al=i;
		state.bl=j;
		asm_subb(&state, state.al, &state.bl);
		if ((j-i != state.bl) ^ read_cf(state.eflags))
		{
			failed++;
			fprintf(stderr, "  unsigned subb failed: %d-%d=%d CF=%d\n", j, i, state.bl, read_cf(state.eflags));
		}
		if (((signed int)((signed char)j)-(signed int)((signed char)i) != (signed char)state.bl) ^ read_of(state.eflags))
		{
			failed++;
			fprintf(stderr, "  signed subb failed: %d-%d=%d OF=%d\n", (signed char)j, (signed char)i, (signed char)state.bl, read_of(state.eflags));
		}
	}
}

int main(int argc, char *argv[])
{
	init_assembler_state(&state, writecallback, readcallback);

	asm_flds(&state, -123.4567);
	asm_flds(&state, -223.4567);
	asm_flds(&state, -323.4567);

	fprintf(stderr, "[FPU reset state]\n");
	fprintf(stderr, "Status Busy [B] = %d\n", read_fpu_status_busy(state.FPUStatusWord));
	fprintf(stderr, "Status Conditioncode3 [C3] = %d\n", read_fpu_status_conditioncode3(state.FPUStatusWord));
	fprintf(stderr, "Status Conditioncode2 [C2] = %d\n", read_fpu_status_conditioncode2(state.FPUStatusWord));
	fprintf(stderr, "Status Conditioncode1 [C1] = %d\n", read_fpu_status_conditioncode1(state.FPUStatusWord));
	fprintf(stderr, "Status Conditioncode0 [C0] = %d\n", read_fpu_status_conditioncode0(state.FPUStatusWord));
	fprintf(stderr, "Status Top [TOP] = %d\n", read_fpu_status_top(state.FPUStatusWord));
	fprintf(stderr, "Status Error Summary [ES] = %d\n", read_fpu_status_error_summary(state.FPUStatusWord));
	fprintf(stderr, "Status Stack Fault [SF] = %d\n", read_fpu_status_stack_fault(state.FPUStatusWord));
	fprintf(stderr, "Status Exception Precision [PE] = %d\n", read_fpu_status_exception_precision(state.FPUStatusWord));
	fprintf(stderr, "Status Exception Underflow [UE] = %d\n", read_fpu_status_exception_underflow(state.FPUStatusWord));
	fprintf(stderr, "Status Exception Overflow [OE] = %d\n", read_fpu_status_exception_overflow(state.FPUStatusWord));
	fprintf(stderr, "Status Exception Zero [ZE] = %d\n", read_fpu_status_exception_zero_divide(state.FPUStatusWord));
	fprintf(stderr, "Status Exception Denormalized Operand [DE] = %d\n", read_fpu_status_exception_denormalized_operand(state.FPUStatusWord));
	fprintf(stderr, "Status Exception Invalid Operand [EI] = %d\n", read_fpu_status_exception_invalid_operand(state.FPUStatusWord));
	fprintf(stderr, "Control Infinity [X] = %d\n",  read_fpu_control_infinty(state.FPUControlWord));
	fprintf(stderr, "Control Rounding [RC] = %d ",  read_fpu_control_rounding(state.FPUControlWord));
	switch (read_fpu_control_rounding(state.FPUControlWord))
	{
		case RC_ROUND_TO_NEAREST:
			fprintf(stderr, "Round to nearest (even)\n");
			break;
		case RC_ROUND_DOWN:
			fprintf(stderr, "Round down (toward negative inifinty)\n");
			break;
		case RC_ROUND_UP:
			fprintf(stderr, "Round up (toward positive infinity)\n");
			break;
		case RC_ROUND_ZERO:
			fprintf(stderr, "Round toward zero (truncate)\n");
			break;
	}
	fprintf(stderr, "Control Precision [PC] = %d ", read_fpu_control_precision(state.FPUControlWord));
	switch (read_fpu_control_precision(state.FPUControlWord))
	{
		case PC_SINGLE_PRECISION:
			fprintf(stderr, "Single (24bits)\n");
			break;
		case PC_RESERVED:
			fprintf(stderr, "Reserved\n");
			break;
		case PC_DOUBLE_PRECISION:
			fprintf(stderr, "Double (53bits)\n");
			break;
		case PC_DOUBLE_EXTENDED_PRECISION:
			fprintf(stderr, "Double Extended (64bits)\n");
			break;
	}
	fprintf(stderr, "Control Exception Mask Precision [PM] = %d\n", read_fpu_control_exception_mask_precision(state.FPUControlWord));
	fprintf(stderr, "Control Exception Mask Underflow [UM] = %d\n", read_fpu_control_exception_mask_underflow(state.FPUControlWord));
	fprintf(stderr, "Control Exception Mask Overflow [OM] = %d\n", read_fpu_control_exception_mask_overflow(state.FPUControlWord));
	fprintf(stderr, "Control Exception Mask Zero Divide [ZM] = %d\n", read_fpu_control_exception_mask_zero_divide(state.FPUControlWord));
	fprintf(stderr, "Control Exception Mask Denormal Operand [DM] = %d\n", read_fpu_control_exception_mask_denormal_operand(state.FPUControlWord));
	fprintf(stderr, "Control Exception Mask Invalid Operation [IM] = %d\n", read_fpu_control_exception_mask_invalid_operation(state.FPUControlWord));
	fprintf(stderr, "Tag Word = 0x%04x:\n", state.FPUTagWord);
	{
		int i;
		for (i=0;i<8;i++)
		{
			int tag = read_fpu_sub_tag(state.FPUTagWord, i);
			fprintf(stderr, "TAG(%d) = %d ", i, tag);
			switch (tag)
			{
				case FPU_TAG_VALID:
					fprintf(stderr, "VALID\n");
					fprintf(stderr, "ST%d = " FPU_TYPE_FORMAT "\n", i, read_fpu_st(&state, i));
					break;
				case FPU_TAG_ZERO:
					fprintf(stderr, "ZERO\n");
					fprintf(stderr, "ST%d = " FPU_TYPE_FORMAT "\n", i, read_fpu_st(&state, i));
					break;
				case FPU_TAG_SPECIAL:
					fprintf(stderr, "SPECIAL ");
					{
						FPU_TYPE data = read_fpu_st(&state, i);
						if (signbit(data))
							fprintf(stderr, "- ");
						else
							fprintf(stderr, "+ ");
						if (isnan(data))
							fprintf(stderr, "NaN ");
						if (isfinite(data))
							fprintf(stderr, "Inf ");
						if (!isnormal(data))
							fprintf(stderr, "!Normal ");
						fprintf(stderr, "\n");
					}
					break;
				case FPU_TAG_EMPTY:
					fprintf(stderr, "EMPTY\n");
					break;
			}
		}
	}
	fprintf(stderr, "\n");
	asm_movl(&state, 0x12345678, &state.eax);
	asm_movl(&state, 0x01234567, &state.ebx);
	asm_cmpl(&state, state.eax, state.ebx);
	asm_jb(&state, jb_exit);
	fprintf(stderr, "cmpl 0x12345678, 0x01234567  does not branch jb\n");
	goto jb_done;
jb_exit:
	fprintf(stderr, "cmpl 0x12345678, 0x01234567  does branch jb\n");
jb_done:



	if (state.eax != 0x12345678)
	{
		fprintf(stderr, "movl failed... expected 0x12345678, got 0x%08x\n", (int)state.eax);
		failed++;
	}
	if (state.ax != 0x5678)
	{
		fprintf(stderr, "endian failed, expected 0x5678, got 0x%04x\n", (int)state.ax);
		failed++;
	}
	if (state.al != 0x78)
	{
		fprintf(stderr, "endian failed, expected 0x78, got 0x%02x\n", (int)state.al);
		failed++;
	}
	if (state.ah != 0x56)
	{
		fprintf(stderr, "endian failed, expected 0x56, got 0x%02x\n", (int)state.ah);
		failed++;
	}

/*
	asm_subl(&state, 0x12345679, &state.eax);

	fprintf(stderr, "eflags=0x%08x\n", state.eflags);
#ifdef X86_AF
	fprintf(stderr, "    af=%d\n", read_af(state.eflags));
#endif
	fprintf(stderr, "    cf=%d\n", read_cf(state.eflags));
	fprintf(stderr, "    of=%d\n", read_of(state.eflags));
#ifdef X86_PF
	fprintf(stderr, "    pf=%d\n", read_pf(state.eflags));
#endif
	fprintf(stderr, "    sf=%d\n", read_sf(state.eflags));
	fprintf(stderr, "    zf=%d\n", read_zf(state.eflags));
*/
	adcb();
	addb();
	divl();
	imul_1_b();
	imul_1_w();
	imul_1_l();
	imul_2_l();
	sarl();
	sbbb();
	shldl();
	shll();
	subb();

	return !!failed;

}
