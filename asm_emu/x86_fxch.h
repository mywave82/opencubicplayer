#ifndef ASM_X86_INTERNAL_H
#warning Do not include this file directly
#else

static inline void asm_fxch(struct assembler_state_t *state)
{
	if (read_fpu_sub_tag(state->FPUTagWord, 0) == FPU_TAG_EMPTY)
	{
		fprintf(stderr, "asm_fxch: #Stack underflow occurred\n");
		write_fpu_status_exception_underflow(state->FPUStatusWord, 1);
		write_fpu_status_conditioncode1(state->FPUStatusWord, 1);
	} else if (read_fpu_sub_tag(state->FPUTagWord, 1) == FPU_TAG_EMPTY)
	{
		fprintf(stderr, "asm_fxch: #Stack underflow occurred\n");
		write_fpu_status_exception_underflow(state->FPUStatusWord, 1);
		write_fpu_status_conditioncode1(state->FPUStatusWord, 1);
	} else {
		FPU_TYPE temp = read_fpu_st(state, 0);
		int tag = read_fpu_sub_tag(state->FPUTagWord, 0);

		write_fpu_sub_tag(&state->FPUTagWord, 0, read_fpu_sub_tag(state->FPUTagWord, 1));
		write_fpu_st(state, 0, read_fpu_st(state, 1));

		write_fpu_sub_tag(&state->FPUTagWord, 1, tag);
		write_fpu_st(state, 1, temp);
	}
}
static inline void asm_fxch_st(struct assembler_state_t *state, int index1)
{
	if (!index1)
	{
		fprintf(stderr, "asm_fxch_st: invalid upcode\n");
		return;
	}

	if (read_fpu_sub_tag(state->FPUTagWord, 0) == FPU_TAG_EMPTY)
	{
		fprintf(stderr, "asm_fxch_st: #Stack underflow occurred\n");
		write_fpu_status_exception_underflow(state->FPUStatusWord, 1);
		write_fpu_status_conditioncode1(state->FPUStatusWord, 1);
	} else if (read_fpu_sub_tag(state->FPUTagWord, index1) == FPU_TAG_EMPTY)
	{
		fprintf(stderr, "asm_fxch_st: #Stack underflow occurred\n");
		write_fpu_status_exception_underflow(state->FPUStatusWord, 1);
		write_fpu_status_conditioncode1(state->FPUStatusWord, 1);
	} else {
		FPU_TYPE temp = read_fpu_st(state, index1);
		int tag = read_fpu_sub_tag(state->FPUTagWord, index1);

		write_fpu_sub_tag(&state->FPUTagWord, index1, read_fpu_sub_tag(state->FPUTagWord, 0));
		write_fpu_st(state, index1, read_fpu_st(state, 0));

		write_fpu_sub_tag(&state->FPUTagWord, 0, tag);
		write_fpu_st(state, 0, temp);
	}
}
static inline void asm_fxch_stst(struct assembler_state_t *state, int index2, int index1)
{
	if (((!!index1)^(!!index2))!=1)
	{
		fprintf(stderr, "asm_fxch_stst: invalid upcode\n");
		return;
	}

	if (read_fpu_sub_tag(state->FPUTagWord, index2) == FPU_TAG_EMPTY)
	{
		fprintf(stderr, "asm_fxch_stst: #Stack underflow occurred\n");
		write_fpu_status_exception_underflow(state->FPUStatusWord, 1);
		write_fpu_status_conditioncode1(state->FPUStatusWord, 1);
	} else if (read_fpu_sub_tag(state->FPUTagWord, index1) == FPU_TAG_EMPTY)
	{
		fprintf(stderr, "asm_fxch_stst: #Stack underflow occurred\n");
		write_fpu_status_exception_underflow(state->FPUStatusWord, 1);
		write_fpu_status_conditioncode1(state->FPUStatusWord, 1);
	} else {
		FPU_TYPE temp = read_fpu_st(state, index1);
		int tag = read_fpu_sub_tag(state->FPUTagWord, index1);

		write_fpu_sub_tag(&state->FPUTagWord, index1, read_fpu_sub_tag(state->FPUTagWord, index2));
		write_fpu_st(state, index1, read_fpu_st(state, index2));

		write_fpu_sub_tag(&state->FPUTagWord, index2, tag);
		write_fpu_st(state, index2, temp);
	}
}
#endif
