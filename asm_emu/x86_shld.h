#ifndef ASM_X86_INTERNAL_H
#warning Do not include this file directly
#else

static inline void asm_shldl(struct assembler_state_t *state, uint8_t count, const uint32_t src, uint32_t *dst)
{
	count = count & 0x0000001f;
	int of = *dst&0x80000000;

	if (count)
	{
		int i;
		write_cf(state->eflags, *dst & (1<<(32-count))); /* the last bit to be shifted out */
		for (i=0;i<count;i++)
		{
			*dst<<=1;
			*dst|=!!(src & (1<<(31-i)));
		}
	}

	if (count==1)
		write_of(state->eflags, (of & ~*dst) & 0x80000000);
	else
		write_of(state->eflags, 0);

	write_sf(state->eflags, *dst & 0x80000000);
	write_zf(state->eflags, !*dst);
#ifdef X86_PF
	asm_update_pf(state->eflags, *dst);
#endif
}

#endif
