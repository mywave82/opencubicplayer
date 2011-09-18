#ifndef ASM_X86_INTERNAL_H
#warning Do not include this file directly
#else

static inline void asm_shll(struct assembler_state_t *state, uint8_t count, uint32_t *dst)
{
	count = count & 0x0000001f;

	if (count)
	{
		write_cf(state->eflags, *dst & (1<<(32-count))); /* the last bit to be shifted out */
		*dst<<=count;
	}

	if (count==1)
		write_of(state->eflags, read_cf(state->eflags) ^ (!!(*dst & 0x80000000)));

	write_sf(state->eflags, *dst & 0x80000000);
	write_zf(state->eflags, !*dst);
#ifdef X86_PF
	asm_update_pf(state->eflags, *dst);
#endif
}
static inline void asm_shlw(struct assembler_state_t *state, uint8_t count, uint16_t *dst)
{
	count = count & 0x0000001f;

	if (count)
	{
		write_cf(state->eflags, *dst & (1<<(16-count))); /* the last bit to be shifted out */
		*dst<<=count;
	}

	if (count==1)
		write_of(state->eflags, read_cf(state->eflags) ^ (!!(*dst & 0x8000)));

	write_sf(state->eflags, *dst & 0x80000000);
	write_zf(state->eflags, !*dst);
#ifdef X86_PF
	asm_update_pf(state->eflags, *dst);
#endif
}
static inline void asm_shlb(struct assembler_state_t *state, uint8_t count, uint8_t *dst)
{
	count = count & 0x0000001f;

	if (count)
	{
		write_cf(state->eflags, *dst & (1<<(8-count))); /* the last bit to be shifted out */
		*dst<<=count;
	}

	if (count==1)
		write_of(state->eflags, read_cf(state->eflags) ^ (!!(*dst & 0x80)));

	write_sf(state->eflags, *dst & 0x80);
	write_zf(state->eflags, !*dst);
#ifdef X86_PF
	asm_update_pf(state->eflags, *dst);
#endif
}
#define asm_sall(count,dst) asm_shll(count,dst)
#define asm_salw(count,dst) asm_shlw(count,dst)
#define asm_salb(count,dst) asm_shlb(count,dst)

#endif
