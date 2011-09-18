#ifndef ASM_X86_INTERNAL_H
#warning Do not include this file directly
#else

static inline void asm_fisubl(struct assembler_state_t *state, uint32_t data)
{
	if (read_fpu_sub_tag(state->FPUTagWord, 0) == FPU_TAG_EMPTY)
	{
		fprintf(stderr, "asm_fisubl: #Stack underflow occurred\n");
		write_fpu_status_exception_underflow(state->FPUStatusWord, 1);
		write_fpu_status_conditioncode1(state->FPUStatusWord, 1);
	} else {
		FPU_TYPE temp;
		write_fpu_st(state, 0, temp = read_fpu_st(state, 0) - data);
		if (fpclassify(temp) == FP_ZERO)
			write_fpu_sub_tag(&state->FPUTagWord, 0, FPU_TAG_ZERO);
	}
}
static inline void asm_fisubw(struct assembler_state_t *state, uint16_t data)
{
	if (read_fpu_sub_tag(state->FPUTagWord, 0) == FPU_TAG_EMPTY)
	{
		fprintf(stderr, "asm_fisubw: #Stack underflow occurred\n");
		write_fpu_status_exception_underflow(state->FPUStatusWord, 1);
		write_fpu_status_conditioncode1(state->FPUStatusWord, 1);
	} else {
		FPU_TYPE temp;
		write_fpu_st(state, 0, temp = (read_fpu_st(state, 0) - data));
		if (fpclassify(temp) == FP_ZERO)
			write_fpu_sub_tag(&state->FPUTagWord, 0, FPU_TAG_ZERO);
	}
}

#endif
