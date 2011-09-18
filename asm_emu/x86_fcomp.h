#ifndef ASM_X86_INTERNAL_H
#warning Do not include this file directly
#else

static inline void asm_fcomps(struct assembler_state_t *state, float data)
{
	int tag = read_fpu_sub_tag(state->FPUTagWord, 0);
	if (tag == FPU_TAG_EMPTY)
	{
		fprintf(stderr, "asm_fcoms: #Stack underflow occurred\n");
		write_fpu_status_exception_underflow(state->FPUStatusWord, 1);
		write_fpu_status_conditioncode1(state->FPUStatusWord, 1);
		return;
	} if (tag == FPU_TAG_SPECIAL)
	{
		write_fpu_status_conditioncode0(state->FPUStatusWord, 1);
		write_fpu_status_conditioncode2(state->FPUStatusWord, 1);
		write_fpu_status_conditioncode3(state->FPUStatusWord, 1);
	} else {
		switch (fpclassify(data))
		{
			case FP_NAN:
			case FP_INFINITE:
			case FP_SUBNORMAL:
			default:
			{
				write_fpu_status_conditioncode0(state->FPUStatusWord, 1);
				write_fpu_status_conditioncode2(state->FPUStatusWord, 1);
				write_fpu_status_conditioncode3(state->FPUStatusWord, 1);
				break;
			}
			case FP_ZERO:
			case FP_NORMAL:
			{
				FPU_TYPE st0 = read_fpu_st(state, 0);
				if (st0 > data)
				{
					write_fpu_status_conditioncode0(state->FPUStatusWord, 0);
					write_fpu_status_conditioncode3(state->FPUStatusWord, 0);
				} else if (st0 < data)
				{
					write_fpu_status_conditioncode0(state->FPUStatusWord, 1);
					write_fpu_status_conditioncode3(state->FPUStatusWord, 0);
				} else {
					write_fpu_status_conditioncode0(state->FPUStatusWord, 0);
					write_fpu_status_conditioncode3(state->FPUStatusWord, 1);
				}
				write_fpu_status_conditioncode2(state->FPUStatusWord, 0);
				break;
			}
		}
	}
	pop_fpu_sub_tag(&state->FPUTagWord);
	write_fpu_status_top(state->FPUStatusWord, (read_fpu_status_top(state->FPUStatusWord)+1));
}
static inline void asm_fcompd(struct assembler_state_t *state, double data)
{
	int tag = read_fpu_sub_tag(state->FPUTagWord, 0);
	if (tag == FPU_TAG_EMPTY)
	{
		fprintf(stderr, "asm_fcoms: #Stack underflow occurred\n");
		write_fpu_status_exception_underflow(state->FPUStatusWord, 1);
		write_fpu_status_conditioncode1(state->FPUStatusWord, 1);
		return;
	} if (tag == FPU_TAG_SPECIAL)
	{
		write_fpu_status_conditioncode0(state->FPUStatusWord, 1);
		write_fpu_status_conditioncode2(state->FPUStatusWord, 1);
		write_fpu_status_conditioncode3(state->FPUStatusWord, 1);
		return;
	} else {
		switch (fpclassify(data))
		{
			case FP_NAN:
			case FP_INFINITE:
			case FP_SUBNORMAL:
			default:
			{
				write_fpu_status_conditioncode0(state->FPUStatusWord, 1);
				write_fpu_status_conditioncode2(state->FPUStatusWord, 1);
				write_fpu_status_conditioncode3(state->FPUStatusWord, 1);
				break;
			}
			case FP_ZERO:
			case FP_NORMAL:
			{
				FPU_TYPE st0 = read_fpu_st(state, 0);
				if (st0 > data)
				{
					write_fpu_status_conditioncode0(state->FPUStatusWord, 0);
					write_fpu_status_conditioncode3(state->FPUStatusWord, 0);
				} else if (st0 < data)
				{
					write_fpu_status_conditioncode0(state->FPUStatusWord, 1);
					write_fpu_status_conditioncode3(state->FPUStatusWord, 0);
				} else {
					write_fpu_status_conditioncode0(state->FPUStatusWord, 0);
					write_fpu_status_conditioncode3(state->FPUStatusWord, 1);
				}
				write_fpu_status_conditioncode2(state->FPUStatusWord, 0);
				break;
			}
		}
	}
	pop_fpu_sub_tag(&state->FPUTagWord);
	write_fpu_status_top(state->FPUStatusWord, (read_fpu_status_top(state->FPUStatusWord)+1));
}

static inline void asm_fcomp_st(struct assembler_state_t *state, int index)
{
	int tag1 = read_fpu_sub_tag(state->FPUTagWord, 0);
	int tag2 = read_fpu_sub_tag(state->FPUTagWord, index);

	if (tag1 == FPU_TAG_EMPTY)
	{
		fprintf(stderr, "asm_fcom: #Stack underflow occurred\n");
		write_fpu_status_exception_underflow(state->FPUStatusWord, 1);
		write_fpu_status_conditioncode1(state->FPUStatusWord, 1);
		return;
	} else if (tag2 == FPU_TAG_EMPTY)
	{
		fprintf(stderr, "asm_fcom: #Stack underflow occurred\n");
		write_fpu_status_exception_underflow(state->FPUStatusWord, 1);
		write_fpu_status_conditioncode1(state->FPUStatusWord, 1);
		return;
	} else {
		if ((tag1==FPU_TAG_SPECIAL)||(tag2==FPU_TAG_SPECIAL))
		{
			write_fpu_status_conditioncode0(state->FPUStatusWord, 1);
			write_fpu_status_conditioncode2(state->FPUStatusWord, 1);
			write_fpu_status_conditioncode3(state->FPUStatusWord, 1);
		} else {
			FPU_TYPE st0 = read_fpu_st(state, 0);
			FPU_TYPE data = read_fpu_st(state, index);

			if (st0 > data)
			{
				write_fpu_status_conditioncode0(state->FPUStatusWord, 0);
				write_fpu_status_conditioncode3(state->FPUStatusWord, 0);
			} else if (st0 < data)
			{
				write_fpu_status_conditioncode0(state->FPUStatusWord, 1);
				write_fpu_status_conditioncode3(state->FPUStatusWord, 0);
			} else {
				write_fpu_status_conditioncode0(state->FPUStatusWord, 0);
				write_fpu_status_conditioncode3(state->FPUStatusWord, 1);
			}
			write_fpu_status_conditioncode2(state->FPUStatusWord, 0);
		}
	}
	pop_fpu_sub_tag(&state->FPUTagWord);
	write_fpu_status_top(state->FPUStatusWord, (read_fpu_status_top(state->FPUStatusWord)+1));
}

static inline void asm_fcomp(struct assembler_state_t *state)
{
	int tag1 = read_fpu_sub_tag(state->FPUTagWord, 0);
	int tag2 = read_fpu_sub_tag(state->FPUTagWord, 1);

	if (tag1 == FPU_TAG_EMPTY)
	{
		fprintf(stderr, "asm_fcom: #Stack underflow occurred\n");
		write_fpu_status_exception_underflow(state->FPUStatusWord, 1);
		write_fpu_status_conditioncode1(state->FPUStatusWord, 1);
		return;
	} else if (tag2 == FPU_TAG_EMPTY)
	{
		fprintf(stderr, "asm_fcom: #Stack underflow occurred\n");
		write_fpu_status_exception_underflow(state->FPUStatusWord, 1);
		write_fpu_status_conditioncode1(state->FPUStatusWord, 1);
		return;
	} else {
		if ((tag1==FPU_TAG_SPECIAL)||(tag2==FPU_TAG_SPECIAL))
		{
			write_fpu_status_conditioncode0(state->FPUStatusWord, 1);
			write_fpu_status_conditioncode2(state->FPUStatusWord, 1);
			write_fpu_status_conditioncode3(state->FPUStatusWord, 1);
		} else {
			FPU_TYPE st0 = read_fpu_st(state, 0);
			FPU_TYPE data = read_fpu_st(state, 1);

			if (st0 > data)
			{
				write_fpu_status_conditioncode0(state->FPUStatusWord, 0);
				write_fpu_status_conditioncode3(state->FPUStatusWord, 0);
			} else if (st0 < data)
			{
				write_fpu_status_conditioncode0(state->FPUStatusWord, 1);
				write_fpu_status_conditioncode3(state->FPUStatusWord, 0);
			} else {
				write_fpu_status_conditioncode0(state->FPUStatusWord, 0);
				write_fpu_status_conditioncode3(state->FPUStatusWord, 1);
			}
			write_fpu_status_conditioncode2(state->FPUStatusWord, 0);
		}
	}
	pop_fpu_sub_tag(&state->FPUTagWord);
	write_fpu_status_top(state->FPUStatusWord, (read_fpu_status_top(state->FPUStatusWord)+1));
}

#endif
