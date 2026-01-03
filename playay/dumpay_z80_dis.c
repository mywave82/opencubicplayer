/* OpenCP Module Player
 * copyright (c) 2019-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Disassembler used by dumpay utility
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
CB => Bit instructions
DD => IX instructions
DD CB => IX bit instructions
ED => extended instructions
FD => IY instructions
FD CB => IY bit instructions
*/

/*
 * return
 *        -2   undefined
 *        -1   do not progress
 *         0   normal
 *         1   branch
 *         2   jump
 */

static int disassemble_2 (unsigned char *memory, uint16_t ptr, char opcode[5], char param1[9], char param2[11], char comment[8], int *length, uint16_t *alt_ptr, char reg)
{
	char R[3] = {'I', reg, 0};

	*length = 4;

	switch (memory[ptr+1] & 0xc0)
	{
		case 0x00:
			switch (memory[ptr+1] & 0x38)
			{
				case 0x00: strcpy (opcode, "RLC"); break;
				case 0x08: strcpy (opcode, "RRC"); break;
				case 0x10: strcpy (opcode, "RL"); break;
				case 0x18: strcpy (opcode, "RR"); break;
				case 0x20: strcpy (opcode, "SLA"); break;
				case 0x28: strcpy (opcode, "SRA"); break;
				case 0x30: strcpy (opcode, "SLL"); break; /* not standard?? */
				case 0x38: strcpy (opcode, "SRL"); break;
			}
			sprintf (param1, "(%s+&%02x)", R, memory[ptr+3]);
			switch (memory[ptr+1] & 0x07)
			{
				case 0x00: strcpy (param2, "B"); break; /* non-standard */
				case 0x01: strcpy (param2, "C"); break; /* non-standard */
				case 0x02: strcpy (param2, "D"); break; /* non-standard */
				case 0x03: strcpy (param2, "E"); break; /* non-standard */
				case 0x04: strcpy (param2, "H"); break; /* non-standard */
				case 0x05: strcpy (param2, "L"); break; /* non-standard */
				case 0x06: break;
				case 0x07: strcpy (param2, "A"); break; /* non-standard */
			}
			return 0;
		case 0x40:
			strcpy (opcode, "BIT");
			break;
		case 0x80:
			strcpy (opcode, "RES");
			break;
		case 0xc0:
			strcpy (opcode, "SET");
			break;
	}
	/* the original assembler is crazy to have the bit/count parameters as parameter1 and target as parameter2 */
	param1[0] = "01234567"[(memory[ptr+1] >> 3) & 0x07];
	param1[1] = 0;
	switch (memory[ptr+1] & 0x07)
	{
		case 0x00: sprintf (param2, "(%s+&%02x),B", R, memory[ptr+3]); break; /* non-standard */
		case 0x01: sprintf (param2, "(%s+&%02x),C", R, memory[ptr+3]); break; /* non-standard */
		case 0x02: sprintf (param2, "(%s+&%02x),D", R, memory[ptr+3]); break; /* non-standard */
		case 0x03: sprintf (param2, "(%s+&%02x),E", R, memory[ptr+3]); break; /* non-standard */
		case 0x04: sprintf (param2, "(%s+&%02x),H", R, memory[ptr+3]); break; /* non-standard */
		case 0x05: sprintf (param2, "(%s+&%02x),L", R, memory[ptr+3]); break; /* non-standard */
		case 0x06: ; break;
		case 0x07: sprintf (param2, "(%s+&%02x),A", R, memory[ptr+3]); break; /* non-standard */
	}

	return 0;
}

static int disassemble_1 (unsigned char *memory, uint16_t ptr, char opcode[5], char param1[9], char param2[11], char comment[8], int *length, uint16_t *alt_ptr, char reg)
{
	char H[4] = {'I', reg, 'H', 0};
	char L[4] = {'I', reg, 'H', 0};
	char R[3] = {'I', reg, 0};

	switch (memory[ptr+1])
	{
		case 0x09: strcpy (opcode, "LD"  ); *length = 2; strcpy (param1, R); strcpy (param2, "BC"); return 0;
		case 0x19: strcpy (opcode, "ADD" ); *length = 2; strcpy (param1, R); strcpy (param2, "DE"); return 0;

		case 0x21: strcpy (opcode, "LD"  ); *length = 4; strcpy (param1, R); sprintf (param2, "&%02x%02x", memory[ptr+3], memory[ptr+2]); return 0;
		case 0x22: strcpy (opcode, "LD"  ); *length = 4; sprintf (param1, "(&%02x%02x)", memory[ptr+3], memory[ptr+2]); strcpy (param2, R); return 0;
		case 0x23: strcpy (opcode, "INC" ); *length = 2; strcpy (param1, R); return 0;
		case 0x24: strcpy (opcode, "INC" ); *length = 2; strcpy (param1, H); return 0; /* non-standard */
		case 0x25: strcpy (opcode, "DEC" ); *length = 2; strcpy (param1, H); return 0; /* non-standard */
		case 0x26: strcpy (opcode, "LD"  ); *length = 3; strcpy (param1, H); sprintf (param2, "&%02x",  memory[ptr+2]); return 0; /* non-standard */
		case 0x29: strcpy (opcode, "ADD" ); *length = 2; strcpy (param1, R); strcpy (param2, R); return 0;
		case 0x2a: strcpy (opcode, "LD"  ); *length = 4; strcpy (param1, R); sprintf (param2, "&%02x%02x", memory[ptr+3], memory[ptr+2]); return 0;
		case 0x2b: strcpy (opcode, "DEC" ); *length = 2; strcpy (param1, R); return 0;
		case 0x2c: strcpy (opcode, "INC" ); *length = 2; strcpy (param1, L); return 0; /* non-standard */
		case 0x2d: strcpy (opcode, "INC" ); *length = 2; strcpy (param1, L); return 0; /* non-standard */
		case 0x2e: strcpy (opcode, "LD"  ); *length = 3; strcpy (param1, L); sprintf (param2, "&%02x",  memory[ptr+2]); return 0; /* non-standard */

		case 0x34: strcpy (opcode, "INC" ); *length = 3; sprintf (param1, "(%s+&%02x)", R, memory[ptr+2]); return 0;
		case 0x35: strcpy (opcode, "DEC" ); *length = 3; sprintf (param1, "(%s+&%02x)", R, memory[ptr+2]); return 0;
		case 0x36: strcpy (opcode, "LD"  ); *length = 4; sprintf (param1, "(%s+&%02x)", R, memory[ptr+2]); sprintf (param2, "&%02x", memory[ptr+3]); return 0;
		case 0x39: strcpy (opcode, "ADD" ); *length = 2; strcpy (param1, R); strcpy (param2, "SP"); return 0;

		case 0x44: strcpy (opcode, "LD"  ); *length = 2; strcpy (param1, "B"); strcpy (param2, H); return 0; /* non-standard */
		case 0x45: strcpy (opcode, "LD"  ); *length = 2; strcpy (param1, "B"); strcpy (param2, L); return 0; /* non-standard */
		case 0x46: strcpy (opcode, "LD"  ); *length = 3; strcpy (param1, "B"); sprintf (param2, "(%s+&%02x)", R, memory[ptr+2]); return 0;
		case 0x4c: strcpy (opcode, "LD"  ); *length = 2; strcpy (param1, "C"); strcpy (param2, H); return 0; /* non-standard */
		case 0x4d: strcpy (opcode, "LD"  ); *length = 2; strcpy (param1, "C"); strcpy (param2, L); return 0; /* non-standard */
		case 0x4e: strcpy (opcode, "LD"  ); *length = 3; strcpy (param1, "C"); sprintf (param2, "(%s+&%02x)", R, memory[ptr+2]); return 0;

		case 0x54: strcpy (opcode, "LD"  ); *length = 2; strcpy (param1, "D"); strcpy (param2, H); return 0; /* non-standard */
		case 0x55: strcpy (opcode, "LD"  ); *length = 2; strcpy (param1, "D"); strcpy (param2, L); return 0; /* non-standard */
		case 0x56: strcpy (opcode, "LD"  ); *length = 3; strcpy (param1, "D"); sprintf (param2, "(%s+&%02x)", R, memory[ptr+2]); return 0;
		case 0x5c: strcpy (opcode, "LD"  ); *length = 2; strcpy (param1, "E"); strcpy (param2, H); return 0; /* non-standard */
		case 0x5d: strcpy (opcode, "LD"  ); *length = 2; strcpy (param1, "E"); strcpy (param2, L); return 0; /* non-standard */
		case 0x5e: strcpy (opcode, "LD"  ); *length = 3; strcpy (param1, "E"); sprintf (param2, "(%s+&%02x)", R, memory[ptr+2]); return 0;

		case 0x60: strcpy (opcode, "LD"  ); *length = 2; strcpy (param1, H); strcpy (param2, "B"); return 0; /* non-standard */
		case 0x61: strcpy (opcode, "LD"  ); *length = 2; strcpy (param1, H); strcpy (param2, "C"); return 0; /* non-standard */
		case 0x62: strcpy (opcode, "LD"  ); *length = 2; strcpy (param1, H); strcpy (param2, "D"); return 0; /* non-standard */
		case 0x63: strcpy (opcode, "LD"  ); *length = 2; strcpy (param1, H); strcpy (param2, "E"); return 0; /* non-standard */
		case 0x64: strcpy (opcode, "LD"  ); *length = 2; strcpy (param1, H); strcpy (param2, H); return 0; /* non-standard */
		case 0x65: strcpy (opcode, "LD"  ); *length = 2; strcpy (param1, H); strcpy (param2, L); return 0; /* non-standard */
		case 0x66: strcpy (opcode, "LD"  ); *length = 3; strcpy (param1, "H"); sprintf (param2, "(%s+&%02x)", R, memory[ptr+2]); return 0;
		case 0x67: strcpy (opcode, "LD"  ); *length = 2; strcpy (param1, H); strcpy (param2, "A"); return 0; /* non-standard */
		case 0x68: strcpy (opcode, "LD"  ); *length = 2; strcpy (param1, L); strcpy (param2, "B"); return 0; /* non-standard */
		case 0x69: strcpy (opcode, "LD"  ); *length = 2; strcpy (param1, L); strcpy (param2, "C"); return 0; /* non-standard */
		case 0x6a: strcpy (opcode, "LD"  ); *length = 2; strcpy (param1, L); strcpy (param2, "D"); return 0; /* non-standard */
		case 0x6b: strcpy (opcode, "LD"  ); *length = 2; strcpy (param1, L); strcpy (param2, "E"); return 0; /* non-standard */
		case 0x6c: strcpy (opcode, "LD"  ); *length = 2; strcpy (param1, L); strcpy (param2, H); return 0; /* non-standard */
		case 0x6d: strcpy (opcode, "LD"  ); *length = 2; strcpy (param1, L); strcpy (param2, L); return 0; /* non-standard */
		case 0x6e: strcpy (opcode, "LD"  ); *length = 3; strcpy (param1, "L"); sprintf (param2, "(%s+&%02x)", R, memory[ptr+2]); return 0;
		case 0x6f: strcpy (opcode, "LD"  ); *length = 2; strcpy (param1, L); strcpy (param2, "A"); return 0; /* non-standard */

		case 0x70: strcpy (opcode, "LD"  ); *length = 3; sprintf (param1, "(%s+&%02x)", R, memory[ptr+2]); strcpy (param2, "B"); return 0;
		case 0x71: strcpy (opcode, "LD"  ); *length = 3; sprintf (param1, "(%s+&%02x)", R, memory[ptr+2]); strcpy (param2, "C"); return 0;
		case 0x72: strcpy (opcode, "LD"  ); *length = 3; sprintf (param1, "(%s+&%02x)", R, memory[ptr+2]); strcpy (param2, "D"); return 0;
		case 0x73: strcpy (opcode, "LD"  ); *length = 3; sprintf (param1, "(%s+&%02x)", R, memory[ptr+2]); strcpy (param2, "E"); return 0;
		case 0x74: strcpy (opcode, "LD"  ); *length = 3; sprintf (param1, "(%s+&%02x)", R, memory[ptr+2]); strcpy (param2, "H"); return 0;
		case 0x75: strcpy (opcode, "LD"  ); *length = 3; sprintf (param1, "(%s+&%02x)", R, memory[ptr+2]); strcpy (param2, "L"); return 0;
		case 0x77: strcpy (opcode, "LD"  ); *length = 3; sprintf (param1, "(%s+&%02x)", R, memory[ptr+2]); strcpy (param2, "A"); return 0;
		case 0x7c: strcpy (opcode, "LD"  ); *length = 2; strcpy (param1, "A"); strcpy (param2, H); return 0; /* non-standard */
		case 0x7d: strcpy (opcode, "LD"  ); *length = 2; strcpy (param1, "A"); strcpy (param2, L); return 0; /* non-standard */
		case 0x7e: strcpy (opcode, "LD"  ); *length = 3; strcpy (param1, "A"); sprintf (param2, "(%s+&%02x)", R, memory[ptr+2]); return 0;

		case 0x84: strcpy (opcode, "ADD" ); *length = 2; strcpy (param1, "A"); strcpy (param2, H); return 0; /* non-standard */
		case 0x85: strcpy (opcode, "ADD" ); *length = 2; strcpy (param1, "A"); strcpy (param2, L); return 0; /* non-standard */
		case 0x86: strcpy (opcode, "ADD" ); *length = 3; strcpy (param1, "A"); sprintf (param2, "(%s+&%02x)", R, memory[ptr+2]); return 0;
		case 0x8c: strcpy (opcode, "ADC" ); *length = 2; strcpy (param1, "A"); strcpy (param2, H); return 0; /* non-standard */
		case 0x8d: strcpy (opcode, "ADC" ); *length = 2; strcpy (param1, "A"); strcpy (param2, L); return 0; /* non-standard */
		case 0x8e: strcpy (opcode, "ADC" ); *length = 3; strcpy (param1, "A"); sprintf (param2, "(%s+&%02x)", R, memory[ptr+2]); return 0;

		case 0x94: strcpy (opcode, "SUB" ); *length = 2; strcpy (param1, "A"); strcpy (param2, H); return 0; /* non-standard */
		case 0x95: strcpy (opcode, "SUB" ); *length = 2; strcpy (param1, "A"); strcpy (param2, L); return 0; /* non-standard */
		case 0x96: strcpy (opcode, "SUB" ); *length = 3; strcpy (param1, "A"); sprintf (param2, "(%s+&%02x)", R, memory[ptr+2]); return 0;
		case 0x9c: strcpy (opcode, "SBC" ); *length = 2; strcpy (param1, "A"); strcpy (param2, H); return 0; /* non-standard */
		case 0x9d: strcpy (opcode, "SBC" ); *length = 2; strcpy (param1, "A"); strcpy (param2, L); return 0; /* non-standard */
		case 0x9e: strcpy (opcode, "SBC" ); *length = 3; strcpy (param1, "A"); sprintf (param2, "(%s+&%02x)", R, memory[ptr+2]); return 0;

		case 0xa4: strcpy (opcode, "AND" ); *length = 2; strcpy (param1, "A"); strcpy (param2, H); return 0; /* non-standard */
		case 0xa5: strcpy (opcode, "AND" ); *length = 2; strcpy (param1, "A"); strcpy (param2, L); return 0; /* non-standard */
		case 0xa6: strcpy (opcode, "AND" ); *length = 3; strcpy (param1, "A"); sprintf (param2, "(%s+&%02x)", R, memory[ptr+2]); return 0;
		case 0xac: strcpy (opcode, "XOR" ); *length = 2; strcpy (param1, "A"); strcpy (param2, H); return 0; /* non-standard */
		case 0xad: strcpy (opcode, "XOR" ); *length = 2; strcpy (param1, "A"); strcpy (param2, L); return 0; /* non-standard */
		case 0xae: strcpy (opcode, "XOR" ); *length = 3; strcpy (param1, "A"); sprintf (param2, "(%s+&%02x)", R, memory[ptr+2]); return 0;

		case 0xb4: strcpy (opcode, "OR"  ); *length = 2; strcpy (param1, "A"); strcpy (param2, H); return 0; /* non-standard */
		case 0xb5: strcpy (opcode, "OR"  ); *length = 2; strcpy (param1, "A"); strcpy (param2, L); return 0; /* non-standard */
		case 0xb6: strcpy (opcode, "OR"  ); *length = 3; strcpy (param1, "A"); sprintf (param2, "(%s+&%02x)", R, memory[ptr+2]); return 0;
		case 0xbc: strcpy (opcode, "CP"  ); *length = 2; strcpy (param1, "A"); strcpy (param2, H); return 0; /* non-standard */
		case 0xbd: strcpy (opcode, "CP"  ); *length = 2; strcpy (param1, "A"); strcpy (param2, L); return 0; /* non-standard */
		case 0xbe: strcpy (opcode, "CP"  ); *length = 3; strcpy (param1, "A"); sprintf (param2, "(%s+&%02x)", R, memory[ptr+2]); return 0;

		case 0xcb: return disassemble_2 (memory, ptr, opcode, param1, param2, comment, length, alt_ptr, reg);

		case 0xe1: strcpy (opcode, "POP" ); *length = 2; strcpy (param1, R); return 0;
		case 0xe3: strcpy (opcode, "EX"  ); *length = 2; strcpy (param1, "(SP)"); strcpy (param2, R); return 0;
		case 0xe5: strcpy (opcode, "PUSH"); *length = 2; strcpy (param1, R); return 0;
		case 0xe9: strcpy (opcode, "JP"  ); *length = 2; sprintf (param1, "(%s)", R); return -1; /* we have no idea where it jumps */

		case 0xf9: strcpy (opcode, "LD"  ); *length = 2; strcpy (param1, "SP"); strcpy (param2, R); return 0;
	}
	return -2;
}

static int disassemble_CB (unsigned char *memory, uint16_t ptr, char opcode[5], char param1[9], char param2[11], char comment[8], int *length, uint16_t *alt_ptr)
{
	*length = 2;
	switch (memory[ptr+1] & 0xc0)
	{
		case 0x00:
			switch (memory[ptr+1] & 0x38)
			{
				case 0x00: strcpy (opcode, "RLC"); break;
				case 0x08: strcpy (opcode, "RRC"); break;
				case 0x10: strcpy (opcode, "RL"); break;
				case 0x18: strcpy (opcode, "RR"); break;
				case 0x20: strcpy (opcode, "SLA"); break;
				case 0x28: strcpy (opcode, "SRA"); break;
				case 0x30: strcpy (opcode, "SLL"); break; /* not standard?? */
				case 0x38: strcpy (opcode, "SRL"); break;
			}
			switch (memory[ptr+1] & 0x07)
			{
				case 0x00: strcpy (param1, "B"); break;
				case 0x01: strcpy (param1, "C"); break;
				case 0x02: strcpy (param1, "D"); break;
				case 0x03: strcpy (param1, "E"); break;
				case 0x04: strcpy (param1, "H"); break;
				case 0x05: strcpy (param1, "L"); break;
				case 0x06: strcpy (param1, "(HL)"); break;
				case 0x07: strcpy (param1, "A"); break;
			}
			return 0;
		case 0x40:
			strcpy (opcode, "BIT");
			break;
		case 0x80:
			strcpy (opcode, "RES");
			break;
		case 0xc0:
			strcpy (opcode, "SET");
			break;
	}

	/* the original assembler is crazy to have the bit/count parameters as parameter1 and target as parameter2 */
	param1[0] = "01234567"[(memory[ptr+1] >> 3) & 0x07];
	param1[1] = 0;
	switch (memory[ptr+1] & 0x07)
	{
		case 0x00: strcpy (param2, "B"); break;
		case 0x01: strcpy (param2, "C"); break;
		case 0x02: strcpy (param2, "D"); break;
		case 0x03: strcpy (param2, "E"); break;
		case 0x04: strcpy (param2, "H"); break;
		case 0x05: strcpy (param2, "L"); break;
		case 0x06: strcpy (param2, "(HL)"); break;
		case 0x07: strcpy (param2, "A"); break;
	}

	return 0;
}

static int disassemble_DD (unsigned char *memory, uint16_t ptr, char opcode[5], char param1[9], char param2[11], char comment[8], int *length, uint16_t *alt_ptr)
{
	return disassemble_1(memory, ptr, opcode, param1, param2, comment, length, alt_ptr, 'X');
}

static int disassemble_ED (unsigned char *memory, uint16_t ptr, char opcode[5], char param1[9], char param2[11], char comment[8], int *length, uint16_t *alt_ptr)
{
	*length = 2;
	switch (memory[ptr+1])
	{
/*
		case 0x00: strcpy (opcode, "[z80]"); strcpy (comment, "mos_quit - emulator"); return 0;
		case 0x01: strcpy (opcode, "[z80]"); strcpy (comment, "mos_cli - emulator"); return 0;
		case 0x02: strcpy (opcode, "[z80]"); strcpy (comment, "mos_byte - emulator"); return 0;
		case 0x03: strcpy (opcode, "[z80]"); strcpy (comment, "mos_word - emulator"); return 0;
		case 0x04: strcpy (opcode, "[z80]"); strcpy (comment, "mos_wrch - emulator"); return 0;
		case 0x05: strcpy (opcode, "[z80]"); strcpy (comment, "mos_rdch - emulator"); return 0;
		case 0x06: strcpy (opcode, "[z80]"); strcpy (comment, "mos_file - emulator"); return 0;
		case 0x07: strcpy (opcode, "[z80]"); strcpy (comment, "mos_args - emulator"); return 0;
		case 0x08: strcpy (opcode, "[z80]"); strcpy (comment, "mos_bget - emulator"); return 0;
		case 0x09: strcpy (opcode, "[z80]"); strcpy (comment, "mos_bput - emulator"); return 0;
		case 0x0a: strcpy (opcode, "[z80]"); strcpy (comment, "mos_gbpb - emulator"); return 0;
		case 0x0b: strcpy (opcode, "[z80]"); strcpy (comment, "mos_find - emulator"); return 0;
		case 0x0c: strcpy (opcode, "[z80]"); strcpy (comment, "mos_misc - emulator"); return 0;
		case 0x0d: strcpy (opcode, "[z80]"); strcpy (comment, "mos_sys - emulator"); return 0;
		case 0x0e: strcpy (opcode, "[z80]"); strcpy (comment, "mos_rdinf - emulator"); return 0;
		case 0x0f: strcpy (opcode, "[z80]"); strcpy (comment, "mos_wrinf - emulator"); return 0;
*/
		case 0x40: strcpy (opcode, "IN"  ); strcpy (param1, "B"); strcpy (param2, "(C)"); return 0;
		case 0x41: strcpy (opcode, "OUT" ); strcpy (param1, "(C)"); strcpy (param2, "B"); return 0;
		case 0x42: strcpy (opcode, "SBC" ); strcpy (param1, "HL"); strcpy (param2, "BC"); return 0;
		case 0x43: strcpy (opcode, "LD"  ); *length = 4; sprintf (param1, "&%02x%02x", memory[ptr+3], memory[ptr+2]); strcpy (param2, "BC"); return 0;
		case 0x44: strcpy (opcode, "NEG" ); strcpy (param1, "A"); return 0;
		case 0x45: strcpy (opcode, "RETN"); return -1;
		case 0x46: strcpy (opcode, "IM"  ); strcpy (param1, "0"); return 0;
		case 0x47: strcpy (opcode, "LD"  ); strcpy (param1, "I"); strcpy (param2, "A"); return 0;
		case 0x48: strcpy (opcode, "IN"  ); strcpy (param1, "C"); strcpy (param2, "(C)"); return 0;
		case 0x49: strcpy (opcode, "OUT" ); strcpy (param1, "(C)"); strcpy (param2, "C"); return 0;
		case 0x4a: strcpy (opcode, "ADC" ); strcpy (param1, "HL"); strcpy (param2, "BC"); return 0;
		case 0x4b: strcpy (opcode, "LD"  ); *length = 4; strcpy (param1, "BC"); sprintf (param2, "&%02x%02x", memory[ptr+3], memory[ptr+2]); return 0;
		case 0x4c: strcpy (opcode, "NEG" ); strcpy (param1, "A"); return 0; /* non-standard */
		case 0x4d: strcpy (opcode, "RETI"); return -1;
		/* case 0x4e: IM 0/1 */
		case 0x4f: strcpy (opcode, "LD"  ); strcpy (param1, "R"); strcpy (param2, "A"); return 0;

		case 0x50: strcpy (opcode, "IN"  ); strcpy (param1, "D"); strcpy (param2, "(C)"); return 0;
		case 0x51: strcpy (opcode, "OUT" ); strcpy (param1, "(C)"); strcpy (param2, "D"); return 0;
		case 0x52: strcpy (opcode, "SBC" ); strcpy (param1, "HL"); strcpy (param2, "DE"); return 0;
		case 0x53: strcpy (opcode, "LD"  ); *length = 4; sprintf (param1, "&%02x%02x", memory[ptr+3], memory[ptr+2]); strcpy (param2, "DE"); return 0;
		case 0x54: strcpy (opcode, "NEG" ); strcpy (param1, "A"); return 0; /* non-standard */
		case 0x55: strcpy (opcode, "RETN"); return -1;
		case 0x56: strcpy (opcode, "IM"  ); strcpy (param1, "1"); return 0;
		case 0x57: strcpy (opcode, "LD"  ); strcpy (param1, "A"); strcpy (param2, "I"); return 0;
		case 0x58: strcpy (opcode, "IN"  ); strcpy (param1, "E"); strcpy (param2, "(C)"); return 0;
		case 0x59: strcpy (opcode, "OUT" ); strcpy (param1, "(C)"); strcpy (param2, "E"); return 0;
		case 0x5a: strcpy (opcode, "ADC" ); strcpy (param1, "HL"); strcpy (param2, "DE"); return 0;
		case 0x5b: strcpy (opcode, "LD"  ); *length = 4; strcpy (param1, "DE"); sprintf (param2, "&%02x%02x", memory[ptr+3], memory[ptr+2]); return 0;
		case 0x5c: strcpy (opcode, "NEG" ); strcpy (param1, "A"); return 0; /* non-standard */
		case 0x5d: strcpy (opcode, "RETN"); return -1;
		case 0x5e: strcpy (opcode, "IM"  ); strcpy (param1, "2"); return 0;
		case 0x5f: strcpy (opcode, "LD"  ); strcpy (param1, "A"); strcpy (param2, "R"); return 0;

		case 0x60: strcpy (opcode, "IN"  ); strcpy (param1, "H"); strcpy (param2, "(C)"); return 0;
		case 0x61: strcpy (opcode, "OUT" ); strcpy (param1, "(C)"); strcpy (param2, "H"); return 0;
		case 0x62: strcpy (opcode, "SBC" ); strcpy (param1, "HL"); strcpy (param2, "HL"); return 0;
		case 0x63: strcpy (opcode, "LD"  ); *length = 4; sprintf (param1, "&%02x%02x", memory[ptr+3], memory[ptr+2]); strcpy (param2, "HL"); return 0; /* non-standard */
		case 0x64: strcpy (opcode, "NEG" ); strcpy (param1, "A"); return 0; /* non-standard */
		case 0x65: strcpy (opcode, "RETN"); return -1;
		case 0x66: strcpy (opcode, "IM"  ); strcpy (param1, "0"); return 0;
		case 0x67: strcpy (opcode, "RRD" ); return 0;
		case 0x68: strcpy (opcode, "IN"  ); strcpy (param1, "L"); strcpy (param2, "(C)"); return 0;
		case 0x69: strcpy (opcode, "OUT" ); strcpy (param1, "(C)"); strcpy (param2, "L"); return 0;
		case 0x6a: strcpy (opcode, "ADC" ); strcpy (param1, "HL"); strcpy (param2, "HL"); return 0;
		case 0x6b: strcpy (opcode, "LD"  ); *length = 4; strcpy (param1, "HL"); sprintf (param2, "&%02x%02x", memory[ptr+3], memory[ptr+2]); return 0; /* non-standard */
		case 0x6c: strcpy (opcode, "NEG" ); strcpy (param1, "A"); return 0; /* non-standard */
		case 0x6d: strcpy (opcode, "RETN"); return -1;
		/*case 0x6e: IM 0/1 */
		case 0x6f: strcpy (opcode, "RLD" ); return 0;

		case 0x70: strcpy (opcode, "IN"  ); strcpy (param1, "?"); strcpy (param2, "(C)"); strcpy (comment, "no-op"); return 0; /* non-standard, result not stored */
		case 0x71: strcpy (opcode, "OUT" ); strcpy (param1, "(C)"); strcpy (param2, "0"); return 0; /* non-standard */
		case 0x72: strcpy (opcode, "SBC" ); strcpy (param1, "HL"); strcpy (param2, "SP"); return 0;
		case 0x73: strcpy (opcode, "LD"  ); *length = 4; sprintf (param1, "&%02x%02x", memory[ptr+3], memory[ptr+2]); strcpy (param2, "SP"); return 0;
		case 0x74: strcpy (opcode, "NEG" ); strcpy (param1, "A"); return 0; /* non-standard */
		case 0x75: strcpy (opcode, "RETN"); return -1;
		case 0x76: strcpy (opcode, "IM"  ); strcpy (param1, "1"); return 0;
		/*case 0x77:*/
		case 0x78: strcpy (opcode, "IN"  ); strcpy (param1, "A"); strcpy (param2, "(C)"); return 0;
		case 0x79: strcpy (opcode, "OUT" ); strcpy (param1, "(C)"); strcpy (param2, "A"); return 0;
		case 0x7a: strcpy (opcode, "ADC" ); strcpy (param1, "HL"); strcpy (param2, "SP"); return 0;
		case 0x7b: strcpy (opcode, "LD"  ); *length = 4; strcpy (param1, "SP"); sprintf (param2, "&%02x%02x", memory[ptr+3], memory[ptr+2]); return 0;
		case 0x7c: strcpy (opcode, "NEG" ); strcpy (param1, "A"); return 0; /* non-standard */
		case 0x7d: strcpy (opcode, "RETN"); return -1;
		case 0x7e: strcpy (opcode, "IM"  ); strcpy (param1, "2"); return 0;
		/*case 0x7f:*/

		case 0xa0: strcpy (opcode, "LDI" ); return 0;
		case 0xa1: strcpy (opcode, "CPI" ); return 0;
		case 0xa2: strcpy (opcode, "INI" ); return 0;
		case 0xa3: strcpy (opcode, "OUTI"); return 0;
		/*case 0xa4:*/
		/*case 0xa5:*/
		/*case 0xa6:*/
		/*case 0xa7:*/
		case 0xa8: strcpy (opcode, "LDD" ); return 0;
		case 0xa9: strcpy (opcode, "CPD" ); return 0;
		case 0xaa: strcpy (opcode, "IND" ); return 0;
		case 0xab: strcpy (opcode, "OUTD"); return 0;
		/*case 0xac:*/
		/*case 0xad:*/
		/*case 0xae:*/
		/*case 0xaf:*/

		case 0xb0: strcpy (opcode, "LDIR"); return 0;
		case 0xb1: strcpy (opcode, "CPIR"); return 0;
		case 0xb2: strcpy (opcode, "INIR"); return 0;
		case 0xb3: strcpy (opcode, "OTIR"); return 0;
		/*case 0xb4:*/
		/*case 0xb5:*/
		/*case 0xb6:*/
		/*case 0xb7:*/
		case 0xb8: strcpy (opcode, "LDDR"); return 0;
		case 0xb9: strcpy (opcode, "CPDR"); return 0;
		case 0xba: strcpy (opcode, "INDR"); return 0;
		case 0xbb: strcpy (opcode, "OTDR"); return 0;
		/*case 0xbc:*/
		/*case 0xbd:*/
		/*case 0xbe:*/
		/*case 0xbf:*/

/*
		case 0xff: strcpy (opcode, "[z80]"); strcpy (comment, "mos_quit/z_quit - emulator"); return 0;
		case 0xfe: strcpy (opcode, "[z80]"); strcpy (comment, "mos_cli/z_wrrom - emulator"); return 0;
		case 0xfd: strcpy (opcode, "[z80]"); strcpy (comment, "mos_byte/z_serout - emulator"); return 0;
		case 0xfc: strcpy (opcode, "[z80]"); strcpy (comment, "mos_word/z_serin - emulator"); return 0;
		case 0xfb: strcpy (opcode, "[z80]"); strcpy (comment, "mos_wrch/z_wrmem - emulator"); return 0;
		case 0xfa: strcpy (opcode, "[z80]"); strcpy (comment, "mos_rdch/z_rdmem - emulator"); return 0;
		case 0xf9: strcpy (opcode, "[z80]"); strcpy (comment, "mos_file/z_copy - emulator"); return 0;
		case 0xf8: strcpy (opcode, "[z80]"); strcpy (comment, "mos_args - emulator"); return 0;
		case 0xf7: strcpy (opcode, "[z80]"); strcpy (comment, "mos_bget - emulator"); return 0;
		case 0xf6: strcpy (opcode, "[z80]"); strcpy (comment, "mos_bput - emulator"); return 0;
		case 0xf5: strcpy (opcode, "[z80]"); strcpy (comment, "mos_gbpb - emulator"); return 0;
		case 0xf4: strcpy (opcode, "[z80]"); strcpy (comment, "mos_find - emulator"); return 0;
		case 0xf3: strcpy (opcode, "[z80]"); strcpy (comment, "mos_misc - emulator"); return 0;
		case 0xf2: strcpy (opcode, "[z80]"); strcpy (comment, "mos_sys - emulator"); return 0;
		case 0xf1: strcpy (opcode, "[z80]"); strcpy (comment, "mos_rdinf - emulator"); return 0;
		case 0xf0: strcpy (opcode, "[z80]"); strcpy (comment, "mos_wrinf - emulator"); return 0;
*/
	}
	return -2;
}

static int disassemble_FD (unsigned char *memory, uint16_t ptr, char opcode[5], char param1[9], char param2[11], char comment[8], int *length, uint16_t *alt_ptr)
{
	return disassemble_1(memory, ptr, opcode, param1, param2, comment, length, alt_ptr, 'Y');
}

static int disassemble (unsigned char *memory, uint16_t ptr, char opcode[5], char param1[9], char param2[11], char comment[8], int *length, uint16_t *alt_ptr)
{
	switch (memory[ptr])
	{
		case 0x00: strcpy (opcode, "NOP" ); *length = 1; return 0;
		case 0x01: strcpy (opcode, "LD"  ); *length = 3; strcpy  (param1, "BC");
		                                                 sprintf (param2, "&%02x%02x", memory[ptr+2], memory[ptr+1]);
		                                                 return 0;
		case 0x02: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "(BC)");
		                                                 strcpy  (param2, "A");
		                                                 return 0;
		case 0x03: strcpy (opcode, "INC" ); *length = 1; strcpy  (param1, "BC");
		                                                 return 0;
		case 0x04: strcpy (opcode, "INC" ); *length = 1; strcpy  (param1, "B");
		                                                 return 0;
		case 0x05: strcpy (opcode, "DEC" ); *length = 1; strcpy  (param1, "B");
		                                                 return 0;
		case 0x06: strcpy (opcode, "LD"  ); *length = 2; strcpy  (param1, "B");
		                                                 sprintf (param2, "&%02x", memory[ptr+1]);
		                                                 return 0;
		case 0x07: strcpy (opcode, "RLCA"); *length = 1; return 0;
		case 0x08: strcpy (opcode, "EX"  ); *length = 1; strcpy  (param1, "AF");
		                                                 strcpy  (param2, "AF'");
		                                                 return 0;
		case 0x09: strcpy (opcode, "ADD" ); *length = 1; strcpy  (param1, "HL");
		                                                 strcpy  (param2, "BC");
		                                                 return 0;
		case 0x0a: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "(BC)");
		                                                 return 0;
		case 0x0b: strcpy (opcode, "DEC" ); *length = 1; strcpy  (param1, "BC");
		                                                 return 0;
		case 0x0c: strcpy (opcode, "INC" ); *length = 1; strcpy  (param1, "C");
		                                                 return 0;
		case 0x0d: strcpy (opcode, "DEC" ); *length = 1; strcpy  (param1, "C");
		                                                 return 0;
		case 0x0e: strcpy (opcode, "LD"  ); *length = 2; strcpy  (param1, "C");
		                                                 sprintf (param2, "&%02x", memory[ptr+1]);
		                                                 return 0;
		case 0x0f: strcpy (opcode, "RRCA"); *length = 1; return 0;
		case 0x10: strcpy (opcode, "DJNZ"); *length = 2; sprintf (param1, "&%04x", *alt_ptr = (uint16_t)(ptr+(signed char)memory[ptr+1])+*length);
		                                                 sprintf (comment, "%02x", memory[ptr+1]);
		                                                 return 1;
		case 0x11: strcpy (opcode, "LD"  ); *length = 3; strcpy  (param1, "E");
		                                                 sprintf (param2, "&%02x%02x", memory[ptr+2], memory[ptr+1]);
		                                                 return 0;
		case 0x12: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "(DE)");
		                                                 sprintf (param2, "A");
		                                                 return 0;
		case 0x13: strcpy (opcode, "INC" ); *length = 1; strcpy  (param1, "DE");
		                                                 return 0;
		case 0x14: strcpy (opcode, "INC" ); *length = 1; strcpy  (param1, "D");
		                                                 return 0;
		case 0x15: strcpy (opcode, "DEC" ); *length = 1; strcpy  (param1, "D");
		                                                 return 0;
		case 0x16: strcpy (opcode, "LD"  ); *length = 2; strcpy  (param1, "D");
		                                                 sprintf (param2, "&%02x", memory[ptr+1]);
		                                                 return 0;
		case 0x17: strcpy (opcode, "RLA" ); *length = 1; return 1;
		case 0x18: strcpy (opcode, "JR"  ); *length = 2; sprintf (param1, "&%04x", *alt_ptr = (uint16_t)(ptr+(signed char)memory[ptr+1]+*length));
		                                                 sprintf (comment, "%02x", memory[ptr+1]);
		                                                 return 2;
		case 0x19: strcpy (opcode, "ADD" ); *length = 1; strcpy  (param1, "HL");
		                                                 strcpy  (param2, "DE");
		                                                 return 0;
		case 0x1a: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "(DE)");
		                                                 return 0;
		case 0x1b: strcpy (opcode, "DEC" ); *length = 1; strcpy  (param1, "DE");
		                                                 return 0;
		case 0x1c: strcpy (opcode, "INC" ); *length = 1; strcpy  (param1, "E");
		                                                 return 0;
		case 0x1d: strcpy (opcode, "DEC" ); *length = 1; strcpy  (param1, "E");
		                                                 return 0;
		case 0x1e: strcpy (opcode, "LD"  ); *length = 2; strcpy  (param1, "E");
		                                                 sprintf (param2, "&%02x", memory[ptr+1]);
		                                                 return 0;
		case 0x1f: strcpy (opcode, "RRA" ); *length = 1; return 0;
		case 0x20: strcpy (opcode, "JR"  ); *length = 2; strcpy  (param1, "NZ");
		                                                 sprintf (param2, "&%04x", *alt_ptr = (uint16_t)(ptr+(signed char)memory[ptr+1]+*length));
		                                                 sprintf (comment, "%02x", memory[ptr+1]);
		                                                 return 1;
		case 0x21: strcpy (opcode, "LD"  ); *length = 3; strcpy  (param1, "HL");
		                                                 sprintf (param2, "&%02x%02x", memory[ptr+2], memory[ptr+1]);
		                                                 return 0;
		case 0x22: strcpy (opcode, "LD"  ); *length = 3; sprintf (param1, "(&%02x%02x)", memory[ptr+2], memory[ptr+1]);
		                                                 strcpy  (param2, "HL");
		                                                 return 0;
		case 0x23: strcpy (opcode, "INC" ); *length = 1; strcpy  (param1, "HL");
		                                                 return 0;
		case 0x24: strcpy (opcode, "INC" ); *length = 1; strcpy  (param1, "H");
		                                                 return 0;
		case 0x25: strcpy (opcode, "DEC" ); *length = 1; strcpy  (param1, "H");
		                                                 return 0;
		case 0x26: strcpy (opcode, "LD"  ); *length = 2; strcpy  (param1, "H");
		                                                 sprintf (param2, "&%02x", memory[ptr+1]);
		                                                 return 0;
		case 0x27: strcpy (opcode, "DAA" ); *length = 1; return 0;
		case 0x28: strcpy (opcode, "JR"  ); *length = 2; strcpy  (param1, "Z");
		                                                 sprintf (param2, "&%04x", *alt_ptr = (uint16_t)(ptr+(signed char)memory[ptr+1])+*length);
		                                                 sprintf (comment, "%02x", memory[ptr+1]);
		                                                 return 1;
		case 0x29: strcpy (opcode, "ADD" ); *length = 1; strcpy  (param1, "HL");
		                                                 strcpy  (param2, "HL");
		                                                 return 0;
		case 0x2a: strcpy (opcode, "LD"  ); *length = 3; strcpy  (param1, "HL");
		                                                 sprintf (param2, "(&%02x%02x)", memory[ptr+2], memory[ptr+1]);
		                                                 return 0;
		case 0x2b: strcpy (opcode, "DEC" ); *length = 1; strcpy  (param1, "HL");
		                                                 return 0;
		case 0x2c: strcpy (opcode, "INC" ); *length = 1; strcpy  (param1, "L");
		                                                 return 0;
		case 0x2d: strcpy (opcode, "DEC" ); *length = 1; strcpy  (param1, "L");
		                                                 return 0;
		case 0x2e: strcpy (opcode, "LD"  ); *length = 2; strcpy  (param1, "L");
		                                                 sprintf (param2, "&%02x", memory[ptr+1]);
		                                                 return 0;
		case 0x2f: strcpy (opcode, "CPL" ); *length = 1; return 0;
		case 0x30: strcpy (opcode, "JR"  ); *length = 2; strcpy  (param1, "NC");
		                                                 sprintf (param2, "&%04x", *alt_ptr = (uint16_t)(ptr+(signed char)memory[ptr+1])+*length);
		                                                 sprintf (param2, "%02x", memory[ptr+1]);
		                                                 return 1;
		case 0x31: strcpy (opcode, "LD"  ); *length = 3; strcpy  (param1, "SP");
		                                                 sprintf (param2, "&%02x%02x", memory[ptr+2], memory[ptr+1]);
		                                                 return 0;
		case 0x32: strcpy (opcode, "LD"  ); *length = 3; sprintf (param1, "(&%02x%02x)", memory[ptr+2], memory[ptr+1]);
		                                                 strcpy  (param2, "A");
		                                                 return 0;
		case 0x33: strcpy (opcode, "INC" ); *length = 1; strcpy  (param1, "SP");
		                                                 return 0;
		case 0x34: strcpy (opcode, "INC" ); *length = 1; strcpy  (param1, "(HL)");
		                                                 return 0;
		case 0x35: strcpy (opcode, "DEC" ); *length = 1; strcpy  (param1, "(HL)");
		                                                 return 0;
		case 0x36: strcpy (opcode, "LD"  ); *length = 2; strcpy  (param1, "(HL)");
		                                                 sprintf (param2, "&%02x", memory[ptr+1]);
		                                                 return 0;
		case 0x37: strcpy (opcode, "SCF" ); *length = 1; return 0;
		case 0x38: strcpy (opcode, "JR"  ); *length = 2; strcpy  (param1, "C");
		                                                 sprintf (param2, "&%04x", *alt_ptr = (uint16_t)(ptr+(signed char)memory[ptr+1]+*length));
		                                                 sprintf (comment, "%02x", memory[ptr+1]);
		                                                 return 1;
		case 0x39: strcpy (opcode, "ADD" ); *length = 1; strcpy  (param1, "HL");
		                                                 strcpy  (param2, "SP");
		                                                 return 0;
		case 0x3a: strcpy (opcode, "LD"  ); *length = 3; strcpy  (param1, "A");
		                                                 sprintf (param2, "(&%02x%02x)", memory[ptr+2], memory[ptr+1]);
		                                                 return 0;
		case 0x3b: strcpy (opcode, "DEC" ); *length = 1; strcpy  (param1, "SP");
		                                                 return 0;
		case 0x3c: strcpy (opcode, "INC" ); *length = 1; strcpy  (param1, "A");
		                                                 return 0;
		case 0x3d: strcpy (opcode, "DEC" ); *length = 1; strcpy  (param1, "A");
		                                                 return 0;
		case 0x3e: strcpy (opcode, "LD"  ); *length = 2; strcpy  (param1, "A");
		                                                 sprintf (param2, "&%02x", memory[ptr+1]);
		                                                 return 0;
		case 0x3f: strcpy (opcode, "CCF" ); *length = 1; return 0;
		case 0x40: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "B");
		                                                 strcpy  (param2, "B");
		                                                 return 0;
		case 0x41: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "B");
		                                                 strcpy  (param2, "C");
		                                                 return 0;
		case 0x42: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "B");
		                                                 strcpy  (param2, "D");
		                                                 return 0;
		case 0x43: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "B");
		                                                 strcpy  (param2, "E");
		                                                 return 0;
		case 0x44: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "B");
		                                                 strcpy  (param2, "H");
		                                                 return 0;
		case 0x45: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "B");
		                                                 strcpy  (param2, "L");
		                                                 return 0;
		case 0x46: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "B");
		                                                 strcpy  (param2,"(HL)");
		                                                 return 0;
		case 0x47: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "B");
		                                                 strcpy  (param2, "A");
		                                                 return 0;
		case 0x48: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "C");
		                                                 strcpy  (param2, "B");
		                                                 return 0;
		case 0x49: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "C");
		                                                 strcpy  (param2, "C");
		                                                 return 0;
		case 0x4A: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "C");
		                                                 strcpy  (param2, "D");
		                                                 return 0;
		case 0x4B: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "C");
		                                                 strcpy  (param2, "E");
		                                                 return 0;
		case 0x4C: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "C");
		                                                 strcpy  (param2, "H");
		                                                 return 0;
		case 0x4D: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "C");
		                                                 strcpy  (param2, "L");
		                                                 return 0;
		case 0x4E: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "C");
		                                                 strcpy  (param2, "(HL)");
		                                                 return 0;
		case 0x4F: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "C");
		                                                 strcpy  (param2, "A");
		                                                 return 0;
		case 0x50: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "D");
		                                                 strcpy  (param2, "B");
		                                                 return 0;
		case 0x51: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "D");
		                                                 strcpy  (param2, "C");
		                                                 return 0;
		case 0x52: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "D");
		                                                 strcpy  (param2, "D");
		                                                 return 0;
		case 0x53: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "D");
		                                                 strcpy  (param2, "E");
		                                                 return 0;
		case 0x54: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "D");
		                                                 strcpy  (param2, "H");
		                                                 return 0;
		case 0x55: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "D");
		                                                 strcpy  (param2, "L");
		                                                 return 0;
		case 0x56: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "D");
		                                                 strcpy  (param2, "(HL)");
		                                                 return 0;
		case 0x57: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "D");
		                                                 strcpy  (param2, "A");
		                                                 return 0;
		case 0x58: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "E");
		                                                 strcpy  (param2, "B");
		                                                 return 0;
		case 0x59: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "E");
		                                                 strcpy  (param2, "C");
		                                                 return 0;
		case 0x5A: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "E");
		                                                 strcpy  (param2, "D");
		                                                 return 0;
		case 0x5B: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "E");
		                                                 strcpy  (param2, "E");
		                                                 return 0;
		case 0x5C: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "E");
		                                                 strcpy  (param2, "H");
		                                                 return 0;
		case 0x5D: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "E");
		                                                 strcpy  (param2, "L");
		                                                 return 0;
		case 0x5E: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "E");
		                                                 strcpy  (param2, "(HL)");
		                                                 return 0;
		case 0x5F: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "E");
		                                                 strcpy  (param2, "A");
		                                                 return 0;
		case 0x60: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "H");
		                                                 strcpy  (param2, "B");
		                                                 return 0;
		case 0x61: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "H");
		                                                 strcpy  (param2, "C");
		                                                 return 0;
		case 0x62: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "H");
		                                                 strcpy  (param2, "D");
		                                                 return 0;
		case 0x63: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "H");
		                                                 strcpy  (param2, "E");
		                                                 return 0;
		case 0x64: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "H");
		                                                 strcpy  (param2, "H");
		                                                 return 0;
		case 0x65: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "H");
		                                                 strcpy  (param2, "L");
		                                                 return 0;
		case 0x66: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "H");
		                                                 strcpy  (param2, "(HL)");
		                                                 return 0;
		case 0x67: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "H");
		                                                 strcpy  (param2, "A");
		                                                 return 0;
		case 0x68: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "L");
		                                                 strcpy  (param2, "B");
		                                                 return 0;
		case 0x69: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "L");
		                                                 strcpy  (param2, "C");
		                                                 return 0;
		case 0x6A: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "L");
		                                                 strcpy  (param2, "D");
		                                                 return 0;
		case 0x6B: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "L");
		                                                 strcpy  (param2, "E");
		                                                 return 0;
		case 0x6C: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "L");
		                                                 strcpy  (param2, "H");
		                                                 return 0;
		case 0x6D: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "L");
		                                                 strcpy  (param2, "L");
		                                                 return 0;
		case 0x6E: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "L");
		                                                 strcpy  (param2, "(HL)");
		                                                 return 0;
		case 0x6F: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "L");
		                                                 strcpy  (param2, "A");
		                                                 return 0;
		case 0x70: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "(HL)");
		                                                 strcpy  (param2, "B");
		                                                 return 0;
		case 0x71: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "(HL)");
		                                                 strcpy  (param2, "C");
		                                                 return 0;
		case 0x72: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "(HL)");
		                                                 strcpy  (param2, "D");
		                                                 return 0;
		case 0x73: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "(HL)");
		                                                 strcpy  (param2, "E");
		                                                 return 0;
		case 0x74: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "(HL)");
		                                                 strcpy  (param2, "H");
		                                                 return 0;
		case 0x75: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "(HL)");
		                                                 strcpy  (param2, "L");
		                                                 return 0;
		case 0x76: strcpy (opcode, "HALT"); *length = 1; return 0;
		case 0x77: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "(HL)");
		                                                 strcpy  (param2, "A");
		                                                 return 0;
		case 0x78: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "B");
		                                                 return 0;
		case 0x79: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "C");
		                                                 return 0;
		case 0x7A: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "D");
		                                                 return 0;
		case 0x7B: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "E");
		                                                 return 0;
		case 0x7C: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "H");
		                                                 return 0;
		case 0x7D: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "L");
		                                                 return 0;
		case 0x7E: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "(HL)");
		                                                 return 0;
		case 0x7F: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "A");
		                                                 return 0;
		case 0x80: strcpy (opcode, "ADD" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "B");
		                                                 return 0;
		case 0x81: strcpy (opcode, "ADD" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "C");
		                                                 return 0;
		case 0x82: strcpy (opcode, "ADD" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "D");
		                                                 return 0;
		case 0x83: strcpy (opcode, "ADD" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "E");
		                                                 return 0;
		case 0x84: strcpy (opcode, "ADD" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "H");
		                                                 return 0;
		case 0x85: strcpy (opcode, "ADD" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "L");
		                                                 return 0;
		case 0x86: strcpy (opcode, "ADD" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "(HL)");
		                                                 return 0;
		case 0x87: strcpy (opcode, "ADD" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "A");
		                                                 return 0;
		case 0x88: strcpy (opcode, "ADC" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "B");
		                                                 return 0;
		case 0x89: strcpy (opcode, "ADC" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "C");
		                                                 return 0;
		case 0x8A: strcpy (opcode, "ADC" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "D");
		                                                 return 0;
		case 0x8B: strcpy (opcode, "ADC" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "E");
		                                                 return 0;
		case 0x8C: strcpy (opcode, "ADC" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "H");
		                                                 return 0;
		case 0x8D: strcpy (opcode, "ADC" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "L");
		                                                 return 0;
		case 0x8E: strcpy (opcode, "ADC" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "(HL)");
		                                                 return 0;
		case 0x8F: strcpy (opcode, "ADC" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "A");
		                                                 return 0;
		case 0x90: strcpy (opcode, "SUB" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "B");
		                                                 return 0;
		case 0x91: strcpy (opcode, "SUB" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "C");
		                                                 return 0;
		case 0x92: strcpy (opcode, "SUB" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "D");
		                                                 return 0;
		case 0x93: strcpy (opcode, "SUB" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "E");
		                                                 return 0;
		case 0x94: strcpy (opcode, "SUB" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "H");
		                                                 return 0;
		case 0x95: strcpy (opcode, "SUB" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "L");
		                                                 return 0;
		case 0x96: strcpy (opcode, "SUB" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "(HL)");
		                                                 return 0;
		case 0x97: strcpy (opcode, "SUB" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "A");
		                                                 return 0;
		case 0x98: strcpy (opcode, "SBC" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "B");
		                                                 return 0;
		case 0x99: strcpy (opcode, "SBC" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "C");
		                                                 return 0;
		case 0x9A: strcpy (opcode, "SBC" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "D");
		                                                 return 0;
		case 0x9B: strcpy (opcode, "SBC" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "E");
		                                                 return 0;
		case 0x9C: strcpy (opcode, "SBC" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "H");
		                                                 return 0;
		case 0x9D: strcpy (opcode, "SBC" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "L");
		                                                 return 0;
		case 0x9E: strcpy (opcode, "SBC" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "(HL)");
		                                                 return 0;
		case 0x9F: strcpy (opcode, "SBC" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "A");
		                                                 return 0;
		case 0xA0: strcpy (opcode, "AND" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "B");
		                                                 return 0;
		case 0xA1: strcpy (opcode, "AND" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "C");
		                                                 return 0;
		case 0xA2: strcpy (opcode, "AND" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "D");
		                                                 return 0;
		case 0xA3: strcpy (opcode, "AND" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "E");
		                                                 return 0;
		case 0xA4: strcpy (opcode, "AND" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "H");
		                                                 return 0;
		case 0xA5: strcpy (opcode, "AND" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "L");
		                                                 return 0;
		case 0xA6: strcpy (opcode, "AND" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "(HL)");
		                                                 return 0;
		case 0xA7: strcpy (opcode, "AND" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "A");
		                                                 return 0;
		case 0xA8: strcpy (opcode, "XOR" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "B");
		                                                 return 0;
		case 0xA9: strcpy (opcode, "XOR" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "C");
		                                                 return 0;
		case 0xAA: strcpy (opcode, "XOR" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "D");
		                                                 return 0;
		case 0xAB: strcpy (opcode, "XOR" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "E");
		                                                 return 0;
		case 0xAC: strcpy (opcode, "XOR" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "H");
		                                                 return 0;
		case 0xAD: strcpy (opcode, "XOR" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "L");
		                                                 return 0;
		case 0xAE: strcpy (opcode, "XOR" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "(HL)");
		                                                 return 0;
		case 0xAF: strcpy (opcode, "XOR" ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "A");
		                                                 return 0;
		case 0xB0: strcpy (opcode, "OR"  ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "B");
		                                                 return 0;
		case 0xB1: strcpy (opcode, "OR"  ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "C");
		                                                 return 0;
		case 0xB2: strcpy (opcode, "OR"  ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "D");
		                                                 return 0;
		case 0xB3: strcpy (opcode, "OR"  ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "E");
		                                                 return 0;
		case 0xB4: strcpy (opcode, "OR"  ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "H");
		                                                 return 0;
		case 0xB5: strcpy (opcode, "OR"  ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "L");
		                                                 return 0;
		case 0xB6: strcpy (opcode, "OR"  ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "(HL)");
		                                                 return 0;
		case 0xB7: strcpy (opcode, "OR"  ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "A");
		                                                 return 0;
		case 0xB8: strcpy (opcode, "CP"  ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "B");
		                                                 return 0;
		case 0xB9: strcpy (opcode, "CP"  ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "C");
		                                                 return 0;
		case 0xBA: strcpy (opcode, "CP"  ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "D");
		                                                 return 0;
		case 0xBB: strcpy (opcode, "CP"  ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "E");
		                                                 return 0;
		case 0xBC: strcpy (opcode, "CP"  ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "H");
		                                                 return 0;
		case 0xBD: strcpy (opcode, "CP"  ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "L");
		                                                 return 0;
		case 0xBE: strcpy (opcode, "CP"  ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "(HL)");
		                                                 return 0;
		case 0xBF: strcpy (opcode, "CP"  ); *length = 1; strcpy  (param1, "A");
		                                                 strcpy  (param2, "A");
		                                                 return 0;
		case 0xC0: strcpy (opcode, "RET" ); *length = 1; strcpy  (param1, "NZ");
		                                                 return 0;
		case 0xC1: strcpy (opcode, "POP" ); *length = 1; strcpy  (param1, "BC");
		                                                 return 0;
		case 0xC2: strcpy (opcode, "JP"  ); *length = 3; strcpy  (param1, "NZ");
		                                                 sprintf (param2, "&%04x", *alt_ptr = (memory[ptr+2] << 8) | memory[ptr+1]);
		                                                 return 1;
		case 0xC3: strcpy (opcode, "JP"  ); *length = 3; sprintf (param1, "&%04x", *alt_ptr = (memory[ptr+2] << 8) | memory[ptr+1]);
		                                                 return 2;
		case 0xC4: strcpy (opcode, "CALL"); *length = 3; strcpy  (param1, "NZ");
		                                                 sprintf (param2, "&%04x", *alt_ptr = (memory[ptr+2] << 8) | memory[ptr+1]);
		                                                 return 1;
		case 0xC5: strcpy (opcode, "PUSH"); *length = 1; strcpy  (param1, "BC");
		                                                 return 0;
		case 0xC6: strcpy (opcode, "ADD" ); *length = 2; strcpy  (param1, "A");
		                                                 sprintf (param2, "&%02x", memory[ptr+1]);
		                                                 return 0;
		case 0xC7: strcpy (opcode, "RST" ); *length = 1; strcpy  (param1, "&00"); *alt_ptr = 0x0000;
		                                                 return 2; /* depending on the RST vector, this might be a call... argh */
		case 0xC8: strcpy (opcode, "RET" ); *length = 1; strcpy  (param1, "Z");
		                                                 return 0;
		case 0xC9: strcpy (opcode, "RET" ); *length = 1; return -1;
		case 0xCA: strcpy (opcode, "JP"  ); *length = 3; strcpy  (param1, "Z");
		                                                 sprintf (param2, "&%04x", *alt_ptr = (memory[ptr+2] << 8) | memory[ptr+1]);
		                                                 return 1;
		case 0xCB: return disassemble_CB (memory, ptr, opcode, param1, param2, comment, length, alt_ptr);
		case 0xCC: strcpy (opcode, "CALL"); *length = 3; strcpy  (param1, "Z");
		                                                 sprintf (param2, "&%04x", *alt_ptr = (memory[ptr+2] << 8) | memory[ptr+1]);
		                                                 return 1;
		case 0xCD: strcpy (opcode, "CALL"); *length = 3; sprintf (param1, "&%04x", *alt_ptr = (memory[ptr+2] << 8) | memory[ptr+1]);
		                                                 return 1;
		case 0xCE: strcpy (opcode, "ADC" ); *length = 2; strcpy  (param1, "A");
		                                                 sprintf (param2, "&%02x", memory[ptr+1]);
		                                                 return 0;
		case 0xCF: strcpy (opcode, "RST" ); *length = 1; strcpy  (param1, "&08"); *alt_ptr = 0x0008;
		                                                 return 2; /* depending on the RST vector, this might be a call... argh */
		case 0xD0: strcpy (opcode, "RET" ); *length = 1; strcpy  (param1, "NC");
		                                                 return 0;
		case 0xD1: strcpy (opcode, "POP" ); *length = 1; strcpy  (param1, "DE");
		                                                 return 0;
		case 0xD2: strcpy (opcode, "JP"  ); *length = 3; strcpy  (param1, "NC");
		                                                 sprintf (param2, "&%04x", *alt_ptr = (memory[ptr+2] << 8) | memory[ptr+1]);
		                                                 return 1;
		case 0xD3: strcpy (opcode, "OUT" ); *length = 2; sprintf (param1, "(&%02x)", memory[ptr+1]);
		                                                 strcpy  (param2, "A");
		                                                 return 0;
		case 0xD4: strcpy (opcode, "CALL"); *length = 3; strcpy  (param1, "NC");
		                                                 sprintf (param2, "&%04x", *alt_ptr = (memory[ptr+2] << 8) | memory[ptr+1]);
		                                                 return 1;
		case 0xD5: strcpy (opcode, "PUSH"); *length = 1; strcpy  (param1, "DE");
		                                                 return 0;
		case 0xD6: strcpy (opcode, "SUB" ); *length = 2; strcpy  (param1, "A");
		                                                 sprintf (param2, "&%02x", memory[ptr+1]);
		                                                 return 0;
		case 0xD7: strcpy (opcode, "RST" ); *length = 1; strcpy  (param1, "&10"); *alt_ptr = 0x0010;
		                                                 return 2; /* depending on the RST vector, this might be a call... argh */
		case 0xD8: strcpy (opcode, "RET" ); *length = 1; strcpy  (param1, "C");
		                                                 return 0;
		case 0xD9: strcpy (opcode, "EXX" ); *length = 1; return 0;
		case 0xDA: strcpy (opcode, "JP"  ); *length = 3; strcpy  (param1, "C");
		                                                 sprintf (param2, "&%04x", *alt_ptr = (memory[ptr+2] << 8) | memory[ptr+1]);
		                                                 return 1;
		case 0xDB: strcpy (opcode, "IN"  ); *length = 2; strcpy  (param1, "A");
		                                                 sprintf (param2, "(&%02x)", memory[ptr+1]);
		                                                 return 0;
		case 0xDC: strcpy (opcode, "CALL"); *length = 3; strcpy  (param1, "C");
		                                                 sprintf (param2, "&%04x", *alt_ptr = (memory[ptr+2] << 8) | memory[ptr+1]);
		                                                 return 1;
		case 0xDD: return disassemble_DD (memory, ptr, opcode, param1, param2, comment, length, alt_ptr);
		case 0xDE: strcpy (opcode, "SBC" ); *length = 2; strcpy  (param1, "A");
		                                                 sprintf (param2, "&%02x", memory[ptr+1]);
		                                                 return 0;
		case 0xDF: strcpy (opcode, "RST" ); *length = 1; strcpy  (param1, "&18"); *alt_ptr = 0x0018;
		                                                 return 2; /* depending on the RST vector, this might be a call... argh */
		case 0xE0: strcpy (opcode, "RET" ); *length = 1; strcpy  (param1, "PO");
		                                                 return 0;
		case 0xE1: strcpy (opcode, "POP" ); *length = 1; strcpy  (param1, "HL");
		                                                 return 0;
		case 0xE2: strcpy (opcode, "JP"  ); *length = 3; strcpy  (param1, "PO");
		                                                 sprintf (param2, "&%04x", *alt_ptr = (memory[ptr+2] << 8) | memory[ptr+1]);
		                                                 return 1;
		case 0xE3: strcpy (opcode, "EX"  ); *length = 1; strcpy  (param1, "(SP)");
		                                                 strcpy  (param2, "HL");
		                                                 return 0;
		case 0xE4: strcpy (opcode, "CALL"); *length = 3; strcpy  (param1, "PO");
		                                                 sprintf (param2, "&%04x", *alt_ptr = (memory[ptr+2] << 8) | memory[ptr+1]);
		                                                 return 1;
		case 0xE5: strcpy (opcode, "PUSH"); *length = 1; strcpy  (param1, "HL");
		                                                 return 0;
		case 0xE6: strcpy (opcode, "AND" ); *length = 2; sprintf (param1, "&%02x", memory[ptr+1]);
		                                                 return 0;
		case 0xE7: strcpy (opcode, "RST" ); *length = 1; sprintf (param1, "&20"); *alt_ptr = 0x0020;
		                                                 return 2; /* depending on the RST vector, this might be a call... argh */
		case 0xE8: strcpy (opcode, "RET" ); *length = 1; strcpy  (param1, "PE");
		                                                 return 0;
		case 0xE9: strcpy (opcode, "JP"  ); *length = 1; strcpy  (param1, "(HL)");
		                                                 return -1; /* we have no idea where this jumps too */
		case 0xEA: strcpy (opcode, "JP"  ); *length = 3; strcpy  (param1, "PE");
		                                                 sprintf (param2, "&%04x", *alt_ptr = (memory[ptr+2] << 8) | memory[ptr+1]);
		                                                 return 1;
		case 0xEB: strcpy (opcode, "EX"  ); *length = 1; strcpy  (param1, "DE");
		                                                 strcpy  (param2, "HL");
		                                                 return 0;
		case 0xEC: strcpy (opcode, "CALL"); *length = 3; strcpy  (param1, "PE");
		                                                 sprintf (param2, "&%04x", *alt_ptr = (memory[ptr+2] << 8) | memory[ptr+1]);
		                                                 return 1;
		case 0xED: return disassemble_ED (memory, ptr, opcode, param1, param2, comment, length, alt_ptr);
		case 0xEE: strcpy (opcode, "XOR" ); *length = 2; sprintf (param1, "&%02x", memory[ptr+1]);
		                                                 return 0;
		case 0xEF: strcpy (opcode, "RST" ); *length = 1; strcpy  (param1, "&28"); *alt_ptr = 0x0028;
		                                                 return 2; /* depending on the RST vector, this might be a call... argh */
		case 0xF0: strcpy (opcode, "RET" ); *length = 1; strcpy  (param1, "P");
		                                                 return 0;
		case 0xF1: strcpy (opcode, "POP" ); *length = 1; strcpy  (param1, "AF");
		                                                 return 0;
		case 0xF2: strcpy (opcode, "JP"  ); *length = 3; strcpy  (param1, "P");
		                                                 sprintf (param2, "&%04x", *alt_ptr = (memory[ptr+2] << 8) | memory[ptr+1]);
		                                                 return 1;
		case 0xF3: strcpy (opcode, "DI"  ); *length = 1; return 0;
		case 0xF4: strcpy (opcode, "CALL"); *length = 3; strcpy  (param1, "P");
		                                                 sprintf (param2, "&%04x", *alt_ptr = (memory[ptr+2] << 8) | memory[ptr+1]);
		                                                 return 1;
		case 0xF5: strcpy (opcode, "PUSH"); *length = 1; strcpy  (param1, "AF");
		                                                 return 0;
		case 0xF6: strcpy (opcode, "OR"  ); *length = 2; strcpy  (param1, "A");
		                                                 sprintf (param2, "&%02x", memory[ptr+1]);
		                                                 return 0;
		case 0xF7: strcpy (opcode, "RST" ); *length = 1; strcpy  (param1, "&30"); *alt_ptr = 0x0030;
		                                                 return 2; /* depending on the RST vector, this might be a call... argh */
		case 0xF8: strcpy (opcode, "RET" ); *length = 1; strcpy  (param1, "M");
		                                                 return 0;
		case 0xF9: strcpy (opcode, "LD"  ); *length = 1; strcpy  (param1, "SP");
		                                                 strcpy  (param2, "HL");
		                                                 return 0;
		case 0xFA: strcpy (opcode, "JP"  ); *length = 3; strcpy  (param1, "M");
		                                                 sprintf (param2, "&%04x", *alt_ptr = (memory[ptr+2] << 8) | memory[ptr+1]);
		                                                 return 1;
		case 0xFB: strcpy (opcode, "EI"  ); *length = 1; return 0;
		case 0xFC: strcpy (opcode, "CALL"); *length = 3; strcpy  (param1, "M");
		                                                 sprintf (param2, "&%04x", *alt_ptr = (memory[ptr+2] << 8) | memory[ptr+1]);
		                                                 return 1;
		case 0xFD: return disassemble_FD (memory, ptr, opcode, param1, param2, comment, length, alt_ptr);
		case 0xFE: strcpy (opcode, "CP"  ); *length = 2; strcpy  (param1, "A");
		                                                 sprintf (param2, "&%02x", memory[ptr+1]);
		                                                 return 0;
		case 0xFF: strcpy (opcode, "RST" ); *length = 1; strcpy  (param1, "&38"); *alt_ptr = 0x0038;
		                                                 return 2; /* depending on the RST vector, this might be a call... argh */
	}
}
