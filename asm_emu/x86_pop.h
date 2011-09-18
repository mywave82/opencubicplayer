#ifndef ASM_X86_INTERNAL_H
#warning Do not include this file directly
#else

static inline void asm_popl(struct assembler_state_t *state, uint32_t *dst)
{
	if (state->esp & 0x00000003)
	{
		fprintf(stderr, "#AC(0) exception occured here\n");
		return;
	}
	*dst = x86_read_memory(state, state->ss, state->esp, 4);
	state->esp += 4;
}
static inline void asm_popw(struct assembler_state_t *state, uint16_t *dst)
{
	uint32_t tmp;
	asm_popl(state, &tmp);
	*dst = tmp;
}
static inline void asm_popb(struct assembler_state_t *state, uint8_t *dst)
{
	uint32_t tmp;
	asm_popl(state, &tmp);
	*dst = tmp;
}

#endif
