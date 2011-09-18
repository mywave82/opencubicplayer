#ifndef ASM_X86_INTERNAL_H
#warning Do not include this file directly
#else

#define asm_fistpq asm_fistpll
static inline void asm_fistpll(struct assembler_state_t *state, uint64_t *data)
{
	if (read_fpu_sub_tag(state->FPUTagWord, 0) == FPU_TAG_EMPTY)
	{
		fprintf(stderr, "asm_fistpl: #Stack underflow occurred\n");
		write_fpu_status_exception_underflow(state->FPUStatusWord, 1);
		write_fpu_status_conditioncode1(state->FPUStatusWord, 1);
	} else {
		*data = (int32_t)read_fpu_st(state, 0);
		pop_fpu_sub_tag(&state->FPUTagWord);
		write_fpu_status_top(state->FPUStatusWord, (read_fpu_status_top(state->FPUStatusWord)+1));
	}
}
static inline void asm_fistpl(struct assembler_state_t *state, uint32_t *data)
{
	if (read_fpu_sub_tag(state->FPUTagWord, 0) == FPU_TAG_EMPTY)
	{
		fprintf(stderr, "asm_fistpl: #Stack underflow occurred\n");
		write_fpu_status_exception_underflow(state->FPUStatusWord, 1);
		write_fpu_status_conditioncode1(state->FPUStatusWord, 1);
	} else {
		*data = (int32_t)read_fpu_st(state, 0);
		pop_fpu_sub_tag(&state->FPUTagWord);
		write_fpu_status_top(state->FPUStatusWord, (read_fpu_status_top(state->FPUStatusWord)+1));
	}
}
#define asm_fistp asm_fistps
static inline void asm_fistps(struct assembler_state_t *state, uint16_t *data)
{
	if (read_fpu_sub_tag(state->FPUTagWord, 0) == FPU_TAG_EMPTY)
	{
		fprintf(stderr, "asm_fistps: #Stack underflow occurred\n");
		write_fpu_status_exception_underflow(state->FPUStatusWord, 1);
		write_fpu_status_conditioncode1(state->FPUStatusWord, 1);
	} else {
		*data = (int16_t)read_fpu_st(state, 0);
		pop_fpu_sub_tag(&state->FPUTagWord);
		write_fpu_status_top(state->FPUStatusWord, (read_fpu_status_top(state->FPUStatusWord)+1));
	}
}

#endif
