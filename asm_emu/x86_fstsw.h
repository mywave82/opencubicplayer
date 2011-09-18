#ifndef ASM_X86_INTERNAL_H
#warning Do not include this file directly
#else

static inline void asm_fnstsw(struct assembler_state_t *state, uint16_t *dst)
{
	*dst = state->FPUStatusWord;
}
static inline void asm_fstsw(struct assembler_state_t *state, uint16_t *dst)
{
	*dst = state->FPUStatusWord;
}

#endif
