#ifndef ASM_X86_INTERNAL_H
#warning Do not include this file directly
#else

static inline void asm_fidivl(struct assembler_state_t *state, uint32_t data)
{
	if (read_fpu_sub_tag(state->FPUTagWord, 0) == FPU_TAG_EMPTY)
	{
		fprintf(stderr, "asm_fidivl: #Stack underflow occurred\n");
		write_fpu_status_exception_underflow(state->FPUStatusWord, 1);
		write_fpu_status_conditioncode1(state->FPUStatusWord, 1);
	} else if (data==0)
	{
		fprintf(stderr, "asm_fidivl: #Zero divide exception\n");
		write_fpu_status_exception_zero_divide(state->FPUStatusWord, 1);
		write_fpu_status_conditioncode1(state->FPUStatusWord, 1);
	} else {
		FPU_TYPE temp;
		int tag;
		write_fpu_st(state, 0, temp = read_fpu_st(state, 0) / data);
		switch (fpclassify(temp))
		{
			case FP_NAN:
			case FP_INFINITE:
			case FP_SUBNORMAL:
			default:
				tag = FPU_TAG_SPECIAL;
				break;
			case FP_ZERO:
				tag = FPU_TAG_ZERO;
				break;
			case FP_NORMAL:
				tag = FPU_TAG_VALID;
				break;
		}
		write_fpu_sub_tag(&state->FPUTagWord, 0, FPU_TAG_ZERO);
	}
}
static inline void asm_fidivw(struct assembler_state_t *state, uint16_t data)
{
	if (read_fpu_sub_tag(state->FPUTagWord, 0) == FPU_TAG_EMPTY)
	{
		fprintf(stderr, "asm_fidivw: #Stack underflow occurred\n");
		write_fpu_status_exception_underflow(state->FPUStatusWord, 1);
		write_fpu_status_conditioncode1(state->FPUStatusWord, 1);
	} else if (data==0)
	{
		fprintf(stderr, "asm_fidivl: #Zero divide exception\n");
		write_fpu_status_exception_zero_divide(state->FPUStatusWord, 1);
		write_fpu_status_conditioncode1(state->FPUStatusWord, 1);
	} else {
		FPU_TYPE temp;
		int tag;
		write_fpu_st(state, 0, temp = read_fpu_st(state, 0) / data);
		switch (fpclassify(temp))
		{
			case FP_NAN:
			case FP_INFINITE:
			case FP_SUBNORMAL:
			default:
				tag = FPU_TAG_SPECIAL;
				break;
			case FP_ZERO:
				tag = FPU_TAG_ZERO;
				break;
			case FP_NORMAL:
				tag = FPU_TAG_VALID;
				break;
		}
		write_fpu_sub_tag(&state->FPUTagWord, 0, FPU_TAG_ZERO);

	}
}

#endif
