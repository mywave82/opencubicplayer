#ifndef ASM_X86_INTERNAL_H
#warning Do not include this file directly
#else

static inline void asm_stosb(struct assembler_state_t *state)
{
	x86_write_memory(state, state->es, state->edi, 1, state->al);
	if (read_df(state->eflags))
		state->edi--;
	else
		state->edi++;
}
static inline void asm_stosw(struct assembler_state_t *state)
{
	x86_write_memory(state, state->es, state->edi, 2, state->ax);
	if (read_df(state->eflags))
		state->edi-=2;
	else
		state->edi+=2;
}
static inline void asm_stosl(struct assembler_state_t *state)
{
	x86_write_memory(state, state->es, state->edi, 4, state->eax);
	if (read_df(state->eflags))
		state->edi-=4;
	else
		state->edi+=4;
}

static inline void asm_rep_stosb(struct assembler_state_t *state)
{
	while (state->ecx)
	{
		x86_write_memory(state, state->es, state->edi, 1, state->al);
		if (read_df(state->eflags))
			state->edi--;
		else
			state->edi++;

		state->ecx--;
	}
}
static inline void asm_rep_stosw(struct assembler_state_t *state)
{
	while (state->ecx)
	{
		x86_write_memory(state, state->es, state->edi, 2, state->ax);
		if (read_df(state->eflags))
			state->edi-=2;
		else
			state->edi+=2;

		state->ecx--;
	}
}
static inline void asm_rep_stosl(struct assembler_state_t *state)
{
	while (state->ecx)
	{
		x86_write_memory(state, state->es, state->edi, 4, state->eax);
		if (read_df(state->eflags))
			state->edi-=2;
		else
			state->edi+=2;

		state->ecx--;
	}
}

#endif
