#ifndef ASM_X86_INTERNAL_H
#warning Do not include this file directly
#else

static inline void asm_addl(struct assembler_state_t *state, const uint32_t src, uint32_t *dst)
{
	uint32_t tmp = *dst + src;

	write_cf(state->eflags, tmp < *dst); /* unsigned overflow */
	if ((*dst & 0x80000000) == (src & 0x80000000))
		write_of(state->eflags, (src & 0x80000000) != (tmp & 0x80000000)); /* signed overflow */
	else
		write_of(state->eflags, 0);

	write_sf(state->eflags, tmp & 0x80000000);
	write_zf(state->eflags, !tmp);
#ifdef X86_PF
	asm_update_pf(state->eflags, tmp);
#endif
#ifdef X86_AF
	asm_updae_af(state->eflags, tmp, *dst);
#endif

	*dst = tmp;
}
static inline void asm_addw(struct assembler_state_t *state, const uint16_t src, uint16_t *dst)
{
	uint16_t tmp = *dst + src;

	write_cf(state->eflags, tmp < *dst); /* unsigned overflow */
	if ((*dst & 0x8000) == (src & 0x8000))
		write_of(state->eflags, (src & 0x8000) != (tmp & 0x8000)); /* signed overflow */
	else
		write_of(state->eflags, 0);

	write_sf(state->eflags, tmp & 0x8000);
	write_zf(state->eflags, !tmp);
#ifdef X86_PF
	asm_update_pf(state->eflags, tmp);
#endif
#ifdef X86_AF
	asm_update_af(state->eflags, tmp, *dst);
#endif

	*dst = tmp;
}
static inline void asm_addb(struct assembler_state_t *state, const uint8_t src, uint8_t *dst)
{
	uint8_t tmp = *dst + src;

	write_cf(state->eflags, tmp < *dst); /* unsigned overflow */
	if ((*dst & 0x80) == (src & 0x80))
		write_of(state->eflags, (src & 0x80) != (tmp & 0x80)); /* signed overflow */
	else
		write_of(state->eflags, 0);

	write_sf(state->eflags, tmp & 0x80);
	write_zf(state->eflags, !tmp);
#ifdef X86_PF
	asm_update_pf(state->eflags, tmp);
#endif
#ifdef X86_AF
	asm_update_af(state->eflags, tmp, *dst);
#endif

	*dst = tmp;
}

#endif
