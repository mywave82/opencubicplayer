#ifndef ASM_X86_INTERNAL_H
#warning Do not include this file directly
#else

static inline void asm_pushl(struct assembler_state_t *state, const uint32_t src)
{
	state->esp -= 4;
	if (state->esp & 0x00000003)
	{
		fprintf(stderr, "#AC(0) exception occured here\n");
		return;
	}
	x86_write_memory(state, state->ss, state->esp, 4, src);
}
static inline void asm_pushw(struct assembler_state_t *state, const uint16_t src)
{
	asm_pushl(state, src);
}
static inline void asm_pushb(struct assembler_state_t *state, const uint8_t src)
{
	asm_pushl(state, src);
}

#endif
