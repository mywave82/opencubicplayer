#ifndef ASM_X86_INTERNAL_H
#warning Do not include this file directly
#else

static inline void asm_fsubs(struct assembler_state_t *state, float data)
{
	if (read_fpu_sub_tag(state->FPUTagWord, 0) == FPU_TAG_EMPTY)
	{
		fprintf(stderr, "asm_fsubs: #Stack underflow occurred\n");
		write_fpu_status_exception_underflow(state->FPUStatusWord, 1);
		write_fpu_status_conditioncode1(state->FPUStatusWord, 1);
	} else {
		FPU_TYPE temp;
		write_fpu_st(state, 0, temp = read_fpu_st(state, 0) - data);
		if (fpclassify(temp) == FP_ZERO)
			write_fpu_sub_tag(&state->FPUTagWord, 0, FPU_TAG_ZERO);
	}
}
static inline void asm_fsubd(struct assembler_state_t *state, double data)
{
	if (read_fpu_sub_tag(state->FPUTagWord, 0) == FPU_TAG_EMPTY)
	{
		fprintf(stderr, "asm_fsubd: #Stack underflow occurred\n");
		write_fpu_status_exception_underflow(state->FPUStatusWord, 1);
		write_fpu_status_conditioncode1(state->FPUStatusWord, 1);
	} else {
		FPU_TYPE temp;
		write_fpu_st(state, 0, temp = (read_fpu_st(state, 0) - data));
		if (fpclassify(temp) == FP_ZERO)
			write_fpu_sub_tag(&state->FPUTagWord, 0, FPU_TAG_ZERO);
	}
}

static inline void asm_fsub(struct assembler_state_t *state, int index1, int index2)
{
	if (((!!index1)^(!!index2))!=1)
	{
		fprintf(stderr, "asm_fsub: invalid upcode\n");
		return;
	}

	if (read_fpu_sub_tag(state->FPUTagWord, index2) == FPU_TAG_EMPTY)
	{
		fprintf(stderr, "asm_fsub: #Stack underflow occurred\n");
		write_fpu_status_exception_underflow(state->FPUStatusWord, 1);
		write_fpu_status_conditioncode1(state->FPUStatusWord, 1);
	} else if (read_fpu_sub_tag(state->FPUTagWord, index1) == FPU_TAG_EMPTY)
	{
		fprintf(stderr, "asm_fsub: #Stack underflow occurred\n");
		write_fpu_status_exception_underflow(state->FPUStatusWord, 1);
		write_fpu_status_conditioncode1(state->FPUStatusWord, 1);
	} else {
		FPU_TYPE temp;
		write_fpu_st(state, index2, temp = read_fpu_st(state, index2) - read_fpu_st(state, index1));
		if (fpclassify(temp) == FP_ZERO)
			write_fpu_sub_tag(&state->FPUTagWord, index2, FPU_TAG_ZERO);

	}
}

#endif
