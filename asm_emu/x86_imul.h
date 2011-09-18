#ifndef ASM_X86_INTERNAL_H
#warning Do not include this file directly
#else

static inline void asm_imul_1_l(struct assembler_state_t *state, uint32_t src)
{
	uint64_t result = (uint64_t)((int64_t)((int32_t)src) * (int32_t)state->eax);
	uint32_t low_result = result;
	uint32_t high_result = result>>32;
/*
	uint32_t low_result = 0;
	uint32_t high_result = 0;
	uint32_t low_src;
	uint32_t high_src;
	uint32_t tmp;
	uint_fast32_t mask=1;
	int i;

	int invert_result = (state->eax ^ src) & 0x80000000;

	if (src & 0x80000000)
		src = ~src;
	if (state->eax & 0x80000000)
		state->eax = ~state->eax;

	low_src = state->eax;
	high_src = 0;

	for (i=0;i<32;i++)
	{
		if (src & mask)
		{
			tmp = low_result + low_src;
			if (tmp < low_result)
				high_result++;
			low_result = tmp;

			high_result += high_src;
		}

		mask <<= 1;
		high_src <<= 1;
		if (low_src & 0x80000000)
			high_src|=1;
		low_src <<= 1;
	}

	if (invert_result)
	{
		low_result = ~low_result;
		high_result = ~high_result;
	}*/
	if ((high_result == 0xffffffff) || (high_result == 0x00000000))
	{
		write_of(state->eflags, 0);
		write_cf(state->eflags, 0);
	} else {
		write_of(state->eflags, 1);
		write_cf(state->eflags, 1);
	}
	state->edx = high_result;
	state->eax = low_result;
}

static inline void asm_imul_1_w(struct assembler_state_t *state, const uint16_t src)
{
	uint32_t result = (uint32_t)((int32_t)((int16_t)src) * (int16_t)state->ax);

	state->dx = (result >> 16);
	if ((state->dx == 0xffff) || (state->dx == 0x0000))
	{
		write_of(state->eflags, 0);
		write_cf(state->eflags, 0);
	} else {
		write_of(state->eflags, 1);
		write_cf(state->eflags, 1);
	}
	state->ax = result & 0xffff;
}

static inline void asm_imul_1_b(struct assembler_state_t *state, const uint8_t src)
{
	state->ax = (uint16_t)((int16_t)((int8_t)src) * (int8_t)state->ax);

	if ((state->ah == 0xff) || (state->ah == 0x00))
	{
		write_of(state->eflags, 0);
		write_cf(state->eflags, 0);
	} else {
		write_of(state->eflags, 1);
		write_cf(state->eflags, 1);
	}
}

static inline void asm_imul_2_l(struct assembler_state_t *state, uint32_t src, uint32_t *dst)
{
	uint64_t result = (uint64_t)((int64_t)((int32_t)src) * (int32_t)*dst);
	uint32_t low_result = result;
	uint32_t high_result = result>>32;

/*
	uint32_t low_result = 0;
	uint32_t high_result = 0;
	uint32_t low_src;
	uint32_t high_src;
	uint32_t tmp;
	uint_fast32_t mask=1;
	int i;

	int invert_result = (*dst ^ src) & 0x80000000;

	if (src & 0x80000000)
		src = ~src;
	if (*dst & 0x80000000)
		*dst = ~*dst;

	low_src = *dst;
	high_src = 0;

	for (i=0;i<32;i++)
	{
		if (src & mask)
		{
			tmp = low_result + low_src;
			if (tmp < low_result)
				high_result++;
			low_result = tmp;

			high_result += high_src;
		}

		mask <<= 1;
		high_src <<= 1;
		if (low_src & 0x80000000)
			high_src|=1;
		low_src <<= 1;
	}

	if (invert_result)
	{
		low_result = ~low_result;
		high_result = ~high_result;
	}*/
	if (!high_result) /* x86 is dirty as hell here! */
	{
		write_of(state->eflags, 0);
		write_cf(state->eflags, 0);
	} else {
		write_of(state->eflags, 1);
		write_cf(state->eflags, 1);
	}
	*dst = low_result;
}

static inline void asm_imul_2_w(struct assembler_state_t *state, const uint16_t src, uint16_t *dst)
{
	uint32_t result = (uint32_t)((int32_t)((int16_t)src) * (int16_t)*dst);

	if (!(result & 0xffff0000)) /* x86 is dirty as hell here! */
	{
		write_of(state->eflags, 0);
		write_cf(state->eflags, 0);
	} else {
		write_of(state->eflags, 1);
		write_cf(state->eflags, 1);
	}
	*dst = result & 0xffff;
}
static inline void asm_imul_2_b(struct assembler_state_t *state, const uint8_t src, uint8_t *dst)
{
	uint16_t result = (uint16_t)((int16_t)((int8_t)src) * (int8_t)*dst);

	if (!(result & 0xff00)) /* x86 is dirty as hell here! */
	{
		write_of(state->eflags, 0);
		write_cf(state->eflags, 0);
	} else {
		write_of(state->eflags, 1);
		write_cf(state->eflags, 1);
	}
	*dst = result & 0xff;
}
#endif
