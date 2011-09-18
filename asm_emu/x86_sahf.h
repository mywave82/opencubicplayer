#ifndef ASM_X86_INTERNAL_H
#warning Do not include this file directly
#else

static inline void asm_sahf(struct assembler_state_t *state)
{
	uint32_t temp = state->eflags;
	temp &= 0xffffff00;
	temp |= 2;
	temp |= (state->ah & 0xd5);
	state->eflags = temp;
}

#endif
