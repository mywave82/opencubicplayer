#ifndef ASM_X86_INTERNAL_H
#warning Do not include this file directly
#else

static inline void asm_divl(struct assembler_state_t *state, const uint32_t src)
{
	if (src <= state->edx)
	{
		fprintf(stderr, "#DE exception occured here (0x%08x%08x / 0x%08x )\n", state->edx, state->eax, src);
		state->edx = 0xffffffff;
		state->eax = 0;

	} else {
		uint64_t temp1 = ((uint64_t)state->edx << 32) | (state->eax);
		uint32_t temp2 = temp1 / src;
		state->edx = temp1 % src;
		state->eax = temp2;
	}
}
static inline void asm_divw(struct assembler_state_t *state, const uint16_t src)
{
	if (src <= state->dx)
	{
		fprintf(stderr, "#DE exception occured here\n");
		state->edx = 0xffff;
		state->eax = 0;

	} else {
		uint32_t temp1 = ((uint32_t)state->dx << 16) | (state->ax);
		uint16_t temp2 = temp1 / src;
		state->dx = temp1 % src;
		state->ax = temp2;
	}
}
static inline void asm_divb(struct assembler_state_t *state, const uint8_t src)
{
	if (src <= state->ah)
	{
		fprintf(stderr, "#DE exception occured here\n");
		state->edx = 0xff;
		state->eax = 0;

	} else {
		uint32_t temp2 = state->ax / src;
		state->ah = state->ax % src;
		state->al = temp2;
	}
}

#endif
