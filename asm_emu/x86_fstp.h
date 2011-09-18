#ifndef ASM_X86_INTERNAL_H
#warning Do not include this file directly
#else

static inline void asm_fstp_st(struct assembler_state_t *state, int index)
{
	if (read_fpu_sub_tag(state->FPUTagWord, 0) == FPU_TAG_EMPTY)
	{
		fprintf(stderr, "asm_fstps: #Stack underflow occurred\n");
		write_fpu_status_exception_underflow(state->FPUStatusWord, 1);
		write_fpu_status_conditioncode1(state->FPUStatusWord, 1);
	} else if (read_fpu_sub_tag(state->FPUTagWord, index) == FPU_TAG_EMPTY)
	{
		fprintf(stderr, "asm_fstps: #Stack underflow occurred\n");
		write_fpu_status_exception_underflow(state->FPUStatusWord, 1);
		write_fpu_status_conditioncode1(state->FPUStatusWord, 1);
	} else {
		write_fpu_st(state, index, read_fpu_st(state, 0));
		write_fpu_sub_tag(&state->FPUTagWord, index, read_fpu_sub_tag(state->FPUTagWord, 0));
		pop_fpu_sub_tag(&state->FPUTagWord);
		write_fpu_status_top(state->FPUStatusWord, (read_fpu_status_top(state->FPUStatusWord)+1));
	}
}
static inline void asm_fstps(struct assembler_state_t *state, float *data)
{
	if (read_fpu_sub_tag(state->FPUTagWord, 0) == FPU_TAG_EMPTY)
	{
		fprintf(stderr, "asm_fstps: #Stack underflow occurred\n");
		write_fpu_status_exception_underflow(state->FPUStatusWord, 1);
		write_fpu_status_conditioncode1(state->FPUStatusWord, 1);
	} else {
		*data = read_fpu_st(state, 0);
		pop_fpu_sub_tag(&state->FPUTagWord);
		write_fpu_status_top(state->FPUStatusWord, (read_fpu_status_top(state->FPUStatusWord)+1));
	}
}
static inline void asm_fstpd(struct assembler_state_t *state, double *data)
{
	if (read_fpu_sub_tag(state->FPUTagWord, 0) == FPU_TAG_EMPTY)
	{
		fprintf(stderr, "asm_fstpd: #Stack underflow occurred\n");
		write_fpu_status_exception_underflow(state->FPUStatusWord, 1);
		write_fpu_status_conditioncode1(state->FPUStatusWord, 1);
	} else {
		*data = read_fpu_st(state, 0);
		pop_fpu_sub_tag(&state->FPUTagWord);
		write_fpu_status_top(state->FPUStatusWord, (read_fpu_status_top(state->FPUStatusWord)+1));
	}
}
#define asm_fstdpl asm_fstdpx
static inline void asm_fstpx(struct assembler_state_t *state, long double *data)
{
	if (read_fpu_sub_tag(state->FPUTagWord, 0) == FPU_TAG_EMPTY)
	{
		fprintf(stderr, "asm_fstpx: #Stack underflow occurred\n");
		write_fpu_status_exception_underflow(state->FPUStatusWord, 1);
		write_fpu_status_conditioncode1(state->FPUStatusWord, 1);
	} else {
		*data = read_fpu_st(state, 0);
		pop_fpu_sub_tag(&state->FPUTagWord);
		write_fpu_status_top(state->FPUStatusWord, (read_fpu_status_top(state->FPUStatusWord)+1));
	}
}

#endif
