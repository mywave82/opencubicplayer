#ifndef ASM_X86_INTERNAL_H
#warning Do not include this file directly
#else

static inline void asm_finit(struct assembler_state_t *state)
{
	state->FPUControlWord = 0x037F;
	state->FPUStatusWord = 0x0;
	state->FPUTagWord = 0xFFFF;
#ifdef X86_FPU_EXCEPTIONS
	state->FPUDataPointerOffset = 0x0;
	state->FPUDataPointerSelector = 0x0;
	state->FPUInstructionPointerOffset = 0x0;
	state->FPUInstructionPointerSegment = 0x0;

	state->FPULastInstructionOpcode = 0x0;
#endif
}
#define asm_fninit asm_finit

#endif
