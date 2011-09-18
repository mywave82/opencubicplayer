#ifndef ASM_X86_INTERNAL_H
#warning Do not include this file directly
#else

static inline void asm_fld(struct assembler_state_t *state, int index)
{
	int tag1 = read_fpu_sub_tag(state->FPUTagWord, 0);

	if (tag1 == FPU_TAG_EMPTY)
	{
		fprintf(stderr, "asm_fld: #Stack underflow occurred\n");
		write_fpu_status_exception_underflow(state->FPUStatusWord, 1);
		write_fpu_status_conditioncode1(state->FPUStatusWord, 1);
	} else {
		FPU_TYPE data = read_fpu_st(state, index);
		int tag = read_fpu_sub_tag(state->FPUTagWord, index);
		int exception = push_fpu_sub_tag(&state->FPUTagWord, tag);
		if (!exception)
		{
			write_fpu_status_top(state->FPUStatusWord, (read_fpu_status_top(state->FPUStatusWord)-1));
			write_fpu_st(state, 0, data);
		} else {
			state->FPUStatusWord |= exception;
		}
	}
}

static inline void asm_flds(struct assembler_state_t *state, float data)
{
	int tag;
	int exception;

	switch (fpclassify(data))
	{
		case FP_NAN:
		case FP_INFINITE:
		case FP_SUBNORMAL:
		default:
			tag = FPU_TAG_SPECIAL;
			break;
		case FP_ZERO:
			tag = FPU_TAG_ZERO;
			break;
		case FP_NORMAL:
			tag = FPU_TAG_VALID;
			break;
	}

	exception = push_fpu_sub_tag(&state->FPUTagWord, tag);
	if (!exception)
	{
		write_fpu_status_top(state->FPUStatusWord, (read_fpu_status_top(state->FPUStatusWord)-1));
		write_fpu_st(state, 0, data);
	} else {
		state->FPUStatusWord |= exception;
	}
}
static inline void asm_fldd(struct assembler_state_t *state, double data)
{
	int tag;
	int exception;

	switch (fpclassify(data))
	{
		case FP_NAN:
		case FP_INFINITE:
		case FP_SUBNORMAL:
		default:
			tag = FPU_TAG_SPECIAL;
			break;
		case FP_ZERO:
			tag = FPU_TAG_ZERO;
			break;
		case FP_NORMAL:
			tag = FPU_TAG_VALID;
			break;
	}

	exception = push_fpu_sub_tag(&state->FPUTagWord, tag);

	if (!exception)
	{
		write_fpu_status_top(state->FPUStatusWord, (read_fpu_status_top(state->FPUStatusWord)-1));
		write_fpu_st(state, 0, data);
	} else {
		state->FPUStatusWord |= exception;
	}
}
#define asm_fldl asm_fldd
static inline void asm_fldx(struct assembler_state_t *state, long double data)
{
	int tag;
	int exception;

	switch (fpclassify(data))
	{
		case FP_NAN:
		case FP_INFINITE:
		case FP_SUBNORMAL:
		default:
			tag = FPU_TAG_SPECIAL;
			break;
		case FP_ZERO:
			tag = FPU_TAG_ZERO;
			break;
		case FP_NORMAL:
			tag = FPU_TAG_VALID;
			break;
	}

	exception = push_fpu_sub_tag(&state->FPUTagWord, tag);

	if (!exception)
	{
		write_fpu_status_top(state->FPUStatusWord, (read_fpu_status_top(state->FPUStatusWord)-1));
		write_fpu_st(state, 0, data);
	} else {
		state->FPUStatusWord |= exception;
	}
}

#endif
