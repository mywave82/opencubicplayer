#ifndef ASM_X86_INTERNAL_H
#warning Do not include this file directly
#else

static inline void asm_testl(struct assembler_state_t *state, uint32_t src1, uint32_t src2)
{
	uint32_t tmp = src1 & src2;

	write_zf(state->eflags, !tmp);
	write_sf(state->eflags, tmp & 0x80000000);

	write_of(state->eflags, 0);
	write_cf(state->eflags, 0);
#ifdef X86_PF
	asm_update_pf(state->eflags, tmp);
#endif
	/* LEAVE AF alone */
}
static inline void asm_testw(struct assembler_state_t *state, uint16_t src1, uint16_t src2)
{
	uint16_t tmp = src1 & src2;

	write_zf(state->eflags, !tmp);
	write_sf(state->eflags, tmp & 0x8000);

	write_of(state->eflags, 0);
	write_cf(state->eflags, 0);
#ifdef X86_PF
	asm_update_pf(state->eflags, tmp);
#endif
	/* LEAVE AF alone */
}
static inline void asm_testb(struct assembler_state_t *state, uint8_t src1, uint8_t src2)
{
	uint8_t tmp = src1 & src2;

	write_zf(state->eflags, !tmp);
	write_sf(state->eflags, tmp & 0x80);

	write_of(state->eflags, 0);
	write_cf(state->eflags, 0);
#ifdef X86_PF
	asm_update_pf(state->eflags, tmp);
#endif
	/* LEAVE AF alone */
}

#endif
