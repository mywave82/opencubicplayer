#ifndef ASM_X86_INTERNAL_H
#warning Do not include this file directly
#else

static inline void asm_shrl(struct assembler_state_t *state, uint8_t count, uint32_t *dst)
{
	count = count & 0x0000001f;
	int of = *dst&0x80000000;

	if (count)
	{
		write_cf(state->eflags, *dst & 1<<(count-1));
		*dst>>=count;
	}

	if (count==1)
		write_of(state->eflags, of);

	write_sf(state->eflags, *dst & 0x80000000);
	write_zf(state->eflags, !*dst);
#ifdef X86_PF
	asm_update_pf(state->eflags, *dst);
#endif
}
static inline void asm_shrw(struct assembler_state_t *state, uint8_t count, uint16_t *dst)
{
	count = count & 0x0000001f;
	int of = *dst&0x8000;

	if (count)
	{
		write_cf(state->eflags, *dst & 1<<(count-1));
		*dst>>=count;
	}

	if (count==1)
		write_of(state->eflags, of);

	write_sf(state->eflags, *dst & 0x80000000);
	write_zf(state->eflags, !*dst);
#ifdef X86_PF
	asm_update_pf(state->eflags, *dst);
#endif
}
static inline void asm_shrb(struct assembler_state_t *state, uint8_t count, uint8_t *dst)
{
	count = count & 0x0000001f;
	int of = *dst&0x80;

	if (count)
	{
		write_cf(state->eflags, *dst & 1<<(count-1));
		*dst>>=count;
	}

	if (count==1)
		write_of(state->eflags, of);

	write_cf(state->eflags, 0);
	write_sf(state->eflags, *dst & 0x80);
	write_zf(state->eflags, !*dst);
#ifdef X86_PF
	asm_update_pf(state->eflags, *dst);
#endif
}

#endif
