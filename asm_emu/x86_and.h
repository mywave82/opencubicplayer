#ifndef ASM_X86_INTERNAL_H
#warning Do not include this file directly
#else

static inline void asm_andl(struct assembler_state_t *state, const uint32_t src, uint32_t *dst)
{
	*dst = *dst & src;

	write_cf(state->eflags, 0);
	write_of(state->eflags, 0);

	write_sf(state->eflags, *dst & 0x80000000);
	write_zf(state->eflags, !*dst);
#ifdef X86_PF
	asm_update_pf(state->eflags, *dst);
#endif
}
static inline void asm_andw(struct assembler_state_t *state, const uint16_t src, uint16_t *dst)
{
	*dst = *dst & src;

	write_cf(state->eflags, 0);
	write_of(state->eflags, 0);

	write_sf(state->eflags, *dst & 0x8000);
	write_zf(state->eflags, !*dst);
#ifdef X86_PF
	asm_update_pf(state->eflags, *dst);
#endif
}
static inline void asm_andb(struct assembler_state_t *state, const uint8_t src, uint8_t *dst)
{
	*dst = *dst & src;

	write_cf(state->eflags, 0);
	write_of(state->eflags, 0);

	write_sf(state->eflags, *dst & 0x80);
	write_zf(state->eflags, !*dst);
#ifdef X86_PF
	asm_update_pf(state->eflags, *dst);
#endif
}

#endif
