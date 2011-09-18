#ifndef ASM_X86_INTERNAL_H
#warning Do not include this file directly
#else

static inline void asm_fldz(struct assembler_state_t *state)
{
	int exception = push_fpu_sub_tag(&state->FPUTagWord, FPU_TAG_ZERO);

	if (!exception)
	{
		write_fpu_status_top(state->FPUStatusWord, (read_fpu_status_top(state->FPUStatusWord)-1));
		write_fpu_st(state, 0, 0.0);
	} else {
		state->FPUStatusWord |= exception;
	}
}

#endif
