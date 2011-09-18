#ifndef ASM_X86_INTERNAL_H
#warning Do not include this file directly
#else

static inline void asm_negl(struct assembler_state_t *state, uint32_t *dst)
{
	uint32_t tmp = 0 - *dst;

	write_of(state->eflags, tmp > *dst);
	write_sf(state->eflags, tmp & 0x80000000);
	write_zf(state->eflags, !tmp);
	write_cf(state->eflags, !*dst);
#ifdef X86_PF
	asm_update_pf(state->eflags, tmp);
#endif
#ifdef X86_AF
	asm_update_af(state->eflags, tmp, *dst);
#endif

	*dst = tmp;
}
static inline void asm_negw(struct assembler_state_t *state, uint16_t *dst)
{
	uint16_t tmp = 0 - *dst;

	write_of(state->eflags, tmp > *dst);
	write_sf(state->eflags, tmp & 0x8000);
	write_zf(state->eflags, !tmp);
	write_cf(state->eflags, !*dst);
#ifdef X86_PF
	asm_update_pf(state->eflags, tmp);
#endif
#ifdef X86_AF
	asm_update_af(state->eflags, tmp, *dst);
#endif

	*dst = tmp;
}
static inline void asm_negb(struct assembler_state_t *state, uint8_t *dst)
{
	uint8_t tmp = 0 - *dst;

	write_of(state->eflags, tmp > *dst);
	write_sf(state->eflags, tmp & 0x80);
	write_zf(state->eflags, !tmp);
	write_cf(state->eflags, !*dst);
#ifdef X86_PF
	asm_update_pf(state->eflags, tmp);
#endif
#ifdef X86_AF
	asm_updae_af(state->eflags, tmp, *dst);
#endif

	*dst = tmp;
}

#endif
