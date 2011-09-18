#ifndef ASM_X86_INTERNAL_H
#warning Do not include this file directly
#else

static inline void asm_faddp(struct assembler_state_t *state)
{
	if (read_fpu_sub_tag(state->FPUTagWord, 0) == FPU_TAG_EMPTY)
	{
		fprintf(stderr, "asm_faddp: #Stack underflow occurred\n");
		write_fpu_status_exception_underflow(state->FPUStatusWord, 1);
		write_fpu_status_conditioncode1(state->FPUStatusWord, 1);
	} else if (read_fpu_sub_tag(state->FPUTagWord, 1) == FPU_TAG_EMPTY)
	{
		fprintf(stderr, "asm_faddp: #Stack underflow occurred\n");
		write_fpu_status_exception_underflow(state->FPUStatusWord, 1);
		write_fpu_status_conditioncode1(state->FPUStatusWord, 1);
	} else {
		FPU_TYPE temp;
		write_fpu_st(state, 1, temp = read_fpu_st(state, 0) + read_fpu_st(state, 1));
		if (fpclassify(temp) == FP_ZERO)
			write_fpu_sub_tag(&state->FPUTagWord, 1, FPU_TAG_ZERO);
		pop_fpu_sub_tag(&state->FPUTagWord);
		write_fpu_status_top(state->FPUStatusWord, (read_fpu_status_top(state->FPUStatusWord)+1));
	}
}

static inline void asm_faddp_stst(struct assembler_state_t *state, int index1, int index2)
{
	if ((index1 != 0) || (index2 == 0))
	{
		fprintf(stderr, "asm_faddp_2: invalid opcode\n");
		return;
	}
	if (read_fpu_sub_tag(state->FPUTagWord, index1) == FPU_TAG_EMPTY)
	{
		fprintf(stderr, "asm_faddp_2: #Stack underflow occurred\n");
		write_fpu_status_exception_underflow(state->FPUStatusWord, 1);
		write_fpu_status_conditioncode1(state->FPUStatusWord, 1);
	} else if (read_fpu_sub_tag(state->FPUTagWord, index2) == FPU_TAG_EMPTY)
	{
		fprintf(stderr, "asm_faddp_2: #Stack underflow occurred\n");
		write_fpu_status_exception_underflow(state->FPUStatusWord, 1);
		write_fpu_status_conditioncode1(state->FPUStatusWord, 1);
	} else {
		FPU_TYPE temp;
		write_fpu_st(state, index2, temp = read_fpu_st(state, index1) + read_fpu_st(state, index2));
		if (fpclassify(temp) == FP_ZERO)
			write_fpu_sub_tag(&state->FPUTagWord, index2, FPU_TAG_ZERO);
		pop_fpu_sub_tag(&state->FPUTagWord);
		write_fpu_status_top(state->FPUStatusWord, (read_fpu_status_top(state->FPUStatusWord)+1));
	}
}

#endif
