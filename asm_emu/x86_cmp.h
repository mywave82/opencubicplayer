#ifndef ASM_X86_INTERNAL_H
#warning Do not include this file directly
#else

static inline void asm_subl(struct assembler_state_t *state, const uint32_t src, uint32_t *dst);
static inline void asm_subw(struct assembler_state_t *state, const uint16_t src, uint16_t *dst);
static inline void asm_subb(struct assembler_state_t *state, const uint8_t src, uint8_t *dst);
static inline void asm_cmpl(struct assembler_state_t *state, uint32_t src, uint32_t dst)
{
	uint32_t tmp = dst;
	asm_subl(state, src, &tmp);
}
static inline void asm_cmpw(struct assembler_state_t *state, uint16_t src, uint16_t dst)
{
	uint16_t tmp = dst;
	asm_subw(state, src, &tmp);
}
static inline void asm_cmpb(struct assembler_state_t *state, uint8_t src, uint8_t dst)
{
	uint8_t tmp = dst;
	asm_subb(state, src, &tmp);
}

#endif
