/* OpenCP Module Player
 * copyright (c) 2019-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Disassembler used by the dumpsid utility
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

static int disassemble (unsigned char *memory, uint16_t ptr, char opcode[5], char param1[9], char param2[11], char comment[8], int *length, uint16_t *alt_ptr)
{
	switch (memory[ptr])
	{
		case 0x00: strcpy (opcode, "BRK"); *length = 1;                                                                  return 0;
		case 0x01: strcpy (opcode, "ORA"); *length = 2; sprintf (param1, "($%02x,X)", memory[ptr+1]);                    return 0;
		case 0x05: strcpy (opcode, "ORA"); *length = 2; sprintf (param1, "$%02x", memory[ptr+1]);                        return 0;
		case 0x06: strcpy (opcode, "ASL"); *length = 2; sprintf (param1, "$%02x", memory[ptr+1]);                        return 0;
		case 0x08: strcpy (opcode, "PHP"); *length = 1;                                                                  return 0;
		case 0x09: strcpy (opcode, "ORA"); *length = 2; sprintf (param1, "#$%02x", memory[ptr+1]);                       return 0;
		case 0x0A: strcpy (opcode, "ASL"); *length = 1; strcpy  (param1, "A");                                           return 0;
		case 0x0D: strcpy (opcode, "ORA"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);     return 0;
		case 0x0E: strcpy (opcode, "ASL"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);     return 0;

		case 0x10: strcpy (opcode, "BPL"); *length = 2; sprintf (param1, "&$%04x", *alt_ptr = (uint16_t)(ptr + (signed char)memory[ptr+1] + *length));
		                                                sprintf (comment, "%02x", memory[ptr+1]);                        return 1;
		case 0x11: strcpy (opcode, "ORA"); *length = 2; sprintf (param1, "($%02x)", memory[ptr+1]);
		                                                strcpy  (param2, "Y");                                           return 0; //// could merge into param1 ???
		case 0x15: strcpy (opcode, "ORA"); *length = 2; sprintf (param1, "$%02x", memory[ptr+1]);
		                                                strcpy  (param2, "X");                                           return 0; //// could merge into param1 ???
		case 0x16: strcpy (opcode, "ASL"); *length = 2; sprintf (param1, "$%02x", memory[ptr+1]);
		                                                strcpy  (param2, "X");                                           return 0; //// could merge into param1 ???
		case 0x18: strcpy (opcode, "CLC"); *length = 1;                                                                  return 0;
		case 0x19: strcpy (opcode, "ORA"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);
		                                                strcpy  (param2, "Y");                                           return 0; //// could merge into param1 ???
		case 0x1D: strcpy (opcode, "ORA"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);
		                                                strcpy  (param2, "X");                                           return 0; //// could merge into param1 ???
		case 0x1E: strcpy (opcode, "ASL"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);
		                                                strcpy  (param2, "X");                                           return 0; //// could merge into param1 ???

		case 0x20: strcpy (opcode, "JSR"); *length = 3; sprintf (param1, "&$%04x", *alt_ptr = (uint16_t)((memory[ptr+2]<<8) | memory[ptr+1])); return 1;
		case 0x21: strcpy (opcode, "AND"); *length = 2; sprintf (param1, "($%02x,X)", memory[ptr+1]);                    return 0;
		case 0x24: strcpy (opcode, "BIT"); *length = 2; sprintf (param1, "$%02x", memory[ptr+1]);                        return 0;
		case 0x25: strcpy (opcode, "AND"); *length = 2; sprintf (param1, "$%02x", memory[ptr+1]);                        return 0;
		case 0x26: strcpy (opcode, "ROL"); *length = 2; sprintf (param1, "$%02x", memory[ptr+1]);                        return 0;
		case 0x28: strcpy (opcode, "PLP"); *length = 1;                                                                  return 0;
		case 0x29: strcpy (opcode, "AND"); *length = 2; sprintf (param1, "#$%02x", memory[ptr+1]);                       return 0;
		case 0x2A: strcpy (opcode, "ROL"); *length = 1; strcpy  (param1, "A");                                           return 0;
		case 0x2D: strcpy (opcode, "AND"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);     return 0;
		case 0x2C: strcpy (opcode, "BIT"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+3], memory[ptr+1]);     return 0;
		case 0x2E: strcpy (opcode, "ROL"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);
		                                                strcpy  (param2, "X");                                           return 0; //// could merge into param1 ???

		case 0x30: strcpy (opcode, "BMI"); *length = 2; sprintf (param1, "&$%04x", *alt_ptr = (uint16_t)(ptr + (signed char)memory[ptr+1] + *length));
		                                                sprintf (comment, "%02x", memory[ptr+1]);                        return 1;
		case 0x31: strcpy (opcode, "AND"); *length = 2; sprintf (param1, "($%02x)", memory[ptr+1]);
		                                                strcpy  (param2, "Y");                                           return 0; //// could merge into param1 ???

		case 0x35: strcpy (opcode, "AND"); *length = 2; sprintf (param1, "$%02x", memory[ptr+1]);
		                                                strcpy  (param2, "X");                                           return 0; //// could merge into param1 ???
		case 0x36: strcpy (opcode, "ROL"); *length = 2; sprintf (param1, "$%02x", memory[ptr+1]);
		                                                strcpy  (param2, "X");                                           return 0; //// could merge into param1 ???
		case 0x38: strcpy (opcode, "SEC"); *length = 1;                                                                  return 0;
		case 0x39: strcpy (opcode, "AND"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);
		                                                strcpy  (param2, "Y");                                           return 0; //// could merge into param1 ???
		case 0x3D: strcpy (opcode, "AND"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);
		                                                strcpy  (param2, "X");                                           return 0; //// could merge into param1 ???
		case 0x3E: strcpy (opcode, "ROL"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);
		                                                strcpy  (param2, "X");                                           return 0; //// could merge into param1 ???

		case 0x40: strcpy (opcode, "RTI"); *length = 1;                                                                  return -1;
		case 0x41: strcpy (opcode, "EOR"); *length = 2; sprintf (param1, "($%02x)", memory[ptr+1]);                      return 0;
		case 0x45: strcpy (opcode, "EOR"); *length = 2; sprintf (param1, "$%02x", memory[ptr+1]);                        return 0;
		case 0x46: strcpy (opcode, "LSR"); *length = 2; sprintf (param1, "$%02x", memory[ptr+1]);                        return 0;
		case 0x48: strcpy (opcode, "PHA"); *length = 1;                                                                  return 0;
		case 0x49: strcpy (opcode, "EOR"); *length = 2; sprintf (param1, "#$%02x", memory[ptr+1]);                       return 0;
		case 0x4A: strcpy (opcode, "LSR"); *length = 1; strcpy  (param1, "A");                                           return 0;
		case 0x4C: strcpy (opcode, "JMP"); *length = 3; sprintf (param1, "&$%04x", *alt_ptr = (uint16_t)((memory[ptr+2] << 8) | memory[ptr+1])); return 2;
		case 0x4D: strcpy (opcode, "EOR"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);     return 0;
		case 0x4E: strcpy (opcode, "LSR"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);     return 0;

		case 0x50: strcpy (opcode, "BVC"); *length = 2; sprintf (param1, "&$%04x", *alt_ptr = (uint16_t)(ptr + (signed char)memory[ptr+1] + *length));
		                                                sprintf (comment, "%02x", memory[ptr+1]);                        return 1;
		case 0x51: strcpy (opcode, "EOR"); *length = 2; sprintf (param1, "($%02x)", memory[ptr+1]);
		                                                strcpy  (param2, "Y");                                           return 0; //// could merge into param1 ???
		case 0x55: strcpy (opcode, "EOR"); *length = 2; sprintf (param1, "$%02x", memory[ptr+1]);
		                                                strcpy  (param2, "X");                                           return 0; //// could merge into param1 ???
		case 0x56: strcpy (opcode, "LSR"); *length = 2; sprintf (param1, "$%02x", memory[ptr+1]);
		                                                strcpy  (param2, "X");                                           return 0; //// could merge into param1 ???
		case 0x58: strcpy (opcode, "CLI"); *length = 1;                                                                  return 0;
		case 0x59: strcpy (opcode, "EOR"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);
		                                                strcpy  (param2, "Y");                                           return 0; //// could merge into param1 ???
		case 0x5D: strcpy (opcode, "EOR"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);
		                                                strcpy  (param2, "X");                                           return 0; //// could merge into param1 ???
		case 0x5E: strcpy (opcode, "LSR"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);
		                                                strcpy  (param2, "X");                                           return 0; //// could merge into param1 ???

		case 0x60: strcpy (opcode, "RTS"); *length = 1;                                                                  return -1;
		case 0x61: strcpy (opcode, "ADC"); *length = 2; sprintf (param1, "($%02x,X)", memory[ptr+1]);                    return 0;
		case 0x65: strcpy (opcode, "ADC"); *length = 2; sprintf (param1, "$%02x", memory[ptr+1]);                        return 0;
		case 0x66: strcpy (opcode, "ROR"); *length = 2; sprintf (param1, "$%02x", memory[ptr+1]);                        return 0;
		case 0x68: strcpy (opcode, "PLA"); *length = 1;                                                                  return 0;
		case 0x69: strcpy (opcode, "ADC"); *length = 2; sprintf (param1, "#$%02x", memory[ptr+1]);                       return 0;
		case 0x6A: strcpy (opcode, "ROR"); *length = 1; strcpy  (param1, "A");                                           return 0;
		case 0x6C: strcpy (opcode, "JMP"); *length = 2; sprintf (param1, "&($%02x%02x)", memory[ptr+2], memory[ptr+1]);  return -1;
		case 0x6D: strcpy (opcode, "ADC"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);     return 0;
		case 0x6E: strcpy (opcode, "ROR"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);     return 0;

		case 0x70: strcpy (opcode, "BVS"); *length = 2; sprintf (param1, "&$%04x", *alt_ptr = (uint16_t)(ptr + (signed char)memory[ptr+1] + *length));
		                                                sprintf (comment, "%02x", memory[ptr+1]);                        return 1;
		case 0x71: strcpy (opcode, "ADC"); *length = 2; sprintf (param1, "($%02x)", memory[ptr+1]);
		                                                strcpy  (param2, "Y");                                           return 0; //// could merge into param1 ???
		case 0x75: strcpy (opcode, "ADC"); *length = 2; sprintf (param1, "$%02x", memory[ptr+1]);
		                                                strcpy  (param2, "X");                                           return 0; //// could merge into param1 ???
		case 0x76: strcpy (opcode, "ROR"); *length = 2; sprintf (param1, "$%02x", memory[ptr+1]);
		                                                strcpy  (param2, "X");                                           return 0; //// could merge into param1 ???
		case 0x78: strcpy (opcode, "SEI"); *length = 1;                                                                  return 0;
		case 0x79: strcpy (opcode, "ADC"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);
		                                                strcpy  (param2, "Y");                                           return 0; //// could merge into param1 ???
		case 0x7D: strcpy (opcode, "ADC"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);
		                                                strcpy  (param2, "X");                                           return 0; //// could merge into param1 ???
		case 0x7E: strcpy (opcode, "ROR"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);
		                                                strcpy  (param2, "X");                                           return 0; //// could merge into param1 ???

		case 0x81: strcpy (opcode, "STA"); *length = 2; sprintf (param1, "($%02x,X)", memory[ptr+1]);                    return 0;
		case 0x84: strcpy (opcode, "STY"); *length = 2; sprintf (param1, "$%02x", memory[ptr+1]);                        return 0;
		case 0x85: strcpy (opcode, "STA"); *length = 2; sprintf (param1, "$%02x", memory[ptr+1]);                        return 0;
		case 0x86: strcpy (opcode, "STX"); *length = 2; sprintf (param1, "$%02x", memory[ptr+1]);                        return 0;
		case 0x88: strcpy (opcode, "DEY"); *length = 1;                                                                  return 0;
		case 0x8A: strcpy (opcode, "TXA"); *length = 1;                                                                  return 0;
		case 0x8C: strcpy (opcode, "STY"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);     return 0;
		case 0x8D: strcpy (opcode, "STA"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);     return 0;
		case 0x8E: strcpy (opcode, "STX"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);     return 0;

		case 0x90: strcpy (opcode, "BCC"); *length = 2; sprintf (param1, "&$%04x", *alt_ptr = (uint16_t)(ptr + (signed char)memory[ptr+1] + *length));
		                                                sprintf (comment, "%02x", memory[ptr+1]);                        return 1;
		case 0x91: strcpy (opcode, "STA"); *length = 2; sprintf (param1, "($%02x)", memory[ptr+1]);
		                                                strcpy  (param2, "Y");                                           return 0; //// could merge into param1 ???
		case 0x94: strcpy (opcode, "STY"); *length = 2; sprintf (param1, "$%02x", memory[ptr+1]);
		                                                strcpy  (param2, "X");                                           return 0; //// could merge into param1 ???
		case 0x95: strcpy (opcode, "STA"); *length = 2; sprintf (param1, "$%02x", memory[ptr+1]);
		                                                strcpy  (param2, "X");                                           return 0; //// could merge into param1 ???
		case 0x96: strcpy (opcode, "STX"); *length = 2; sprintf (param1, "$%02x", memory[ptr+1]);
		                                                strcpy  (param2, "X");                                           return 0; //// could merge into param1 ???
		case 0x98: strcpy (opcode, "TYA"); *length = 1;                                                                  return 0;
		case 0x99: strcpy (opcode, "STA"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);
		                                                strcpy  (param2, "Y");                                           return 0; //// could merge into param1 ???
		case 0x9A: strcpy (opcode, "TXS"); *length = 1;                                                                  return 0;
		case 0x9D: strcpy (opcode, "STA"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);
		                                                strcpy  (param2, "X");                                           return 0; //// could merge into param1 ???

		case 0xA0: strcpy (opcode, "LDY"); *length = 2; sprintf (param1, "#$%02x", memory[ptr+1]);                       return 0;
		case 0xA1: strcpy (opcode, "LDA"); *length = 2; sprintf (param1, "($%02x,X)", memory[ptr+1]);                    return 0;
		case 0xA2: strcpy (opcode, "LDX"); *length = 2; sprintf (param1, "#$%02x", memory[ptr+1]);                       return 0;
		case 0xA4: strcpy (opcode, "LDY"); *length = 2; sprintf (param1, "$%02x", memory[ptr+1]);                        return 0;
		case 0xA5: strcpy (opcode, "LDA"); *length = 2; sprintf (param1, "$%02x", memory[ptr+1]);                        return 0;
		case 0xA6: strcpy (opcode, "LDX"); *length = 2; sprintf (param1, "$%02x", memory[ptr+1]);                        return 0;
		case 0xA8: strcpy (opcode, "TAY"); *length = 1;                                                                  return 0;
		case 0xA9: strcpy (opcode, "LDA"); *length = 2; sprintf (param1, "#$%02x", memory[ptr+1]);                       return 0;
		case 0xAA: strcpy (opcode, "TAX"); *length = 1;                                                                  return 0;
		case 0xAC: strcpy (opcode, "LDY"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);     return 0;
		case 0xAD: strcpy (opcode, "LDA"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);     return 0;
		case 0xAE: strcpy (opcode, "LDX"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);     return 0;

		case 0xB0: strcpy (opcode, "BCS"); *length = 2; sprintf (param1, "&$%04x", *alt_ptr = (uint16_t)(ptr + (signed char)memory[ptr+1] + *length));
		                                                sprintf (comment, "%02x", memory[ptr+1]);                        return 1;
		case 0xB1: strcpy (opcode, "LDA"); *length = 2; sprintf (param1, "($%02x)", memory[ptr+1]);
		                                                strcpy  (param2, "Y");                                           return 0; //// could merge into param1 ???
		case 0xB4: strcpy (opcode, "LDY"); *length = 2; sprintf (param1, "$%02x", memory[ptr+1]);
		                                                strcpy  (param2, "X");                                           return 0; //// could merge into param1 ???
		case 0xB5: strcpy (opcode, "LDA"); *length = 2; sprintf (param1, "$%02x", memory[ptr+1]);
		                                                strcpy  (param2, "X");                                           return 0; //// could merge into param1 ???
		case 0xB6: strcpy (opcode, "LDX"); *length = 2; sprintf (param1, "$%02x", memory[ptr+1]);
		                                                strcpy  (param2, "X");                                           return 0; //// could merge into param1 ???
		case 0xB8: strcpy (opcode, "CLV"); *length = 1;                                                                  return 0;
		case 0xB9: strcpy (opcode, "LDA"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);
		                                                strcpy  (param2, "Y");                                           return 0; //// could merge into param1 ???
		case 0xBA: strcpy (opcode, "TSX"); *length = 1;                                                                  return 0;

		case 0xBC: strcpy (opcode, "LDY"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);
		                                                strcpy  (param2, "X");                                           return 0; //// could merge into param1 ???
		case 0xBD: strcpy (opcode, "LDA"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);
		                                                strcpy  (param2, "X");                                           return 0; //// could merge into param1 ???
		case 0xBE: strcpy (opcode, "LDX"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);
		                                                strcpy  (param2, "Y");                                           return 0; //// could merge into param1 ???

		case 0xC0: strcpy (opcode, "CPY"); *length = 2; sprintf (param1, "#$%02x", memory[ptr+1]);                       return 0;
		case 0xC1: strcpy (opcode, "CMP"); *length = 2; sprintf (param1, "($%02x,X)", memory[ptr+1]);                    return 0;
		case 0xC4: strcpy (opcode, "CPY"); *length = 2; sprintf (param1, "$%02x", memory[ptr+1]);                        return 0;
		case 0xC5: strcpy (opcode, "CMP"); *length = 2; sprintf (param1, "$%02x", memory[ptr+1]);                        return 0;
		case 0xC6: strcpy (opcode, "DEC"); *length = 2; sprintf (param1, "$%02x", memory[ptr+1]);                        return 0;
		case 0xC8: strcpy (opcode, "INY"); *length = 1;                                                                  return 0;
		case 0xC9: strcpy (opcode, "CMP"); *length = 2; sprintf (param1, "#$%02x", memory[ptr+1]);                       return 0;
		case 0xCA: strcpy (opcode, "DEX"); *length = 1;                                                                  return 0;
		case 0xCC: strcpy (opcode, "CPY"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);     return 0;
		case 0xCD: strcpy (opcode, "CMP"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);     return 0;
		case 0xCE: strcpy (opcode, "DEC"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);     return 0;

		case 0xD0: strcpy (opcode, "BNE"); *length = 2; sprintf (param1, "&$%04x", *alt_ptr = (uint16_t)(ptr + (signed char)memory[ptr+1] + *length));
		                                                sprintf (comment, "%02x", memory[ptr+1]);                        return 1;
		case 0xD1: strcpy (opcode, "CMP"); *length = 2; sprintf (param1, "($%02x)", memory[ptr+1]);
		                                                strcpy  (param2, "Y");                                           return 0; //// could merge into param1 ???
		case 0xD5: strcpy (opcode, "CMP"); *length = 2; sprintf (param1, "$%02x", memory[ptr+1]);
		                                                strcpy  (param2, "X");                                           return 0; //// could merge into param1 ???
		case 0xD6: strcpy (opcode, "DEC"); *length = 2; sprintf (param1, "$%02x", memory[ptr+1]);
		                                                strcpy  (param2, "X");                                           return 0; //// could merge into param1 ???
		case 0xD8: strcpy (opcode, "CLD"); *length = 1;                                                                  return 0;
		case 0xD9: strcpy (opcode, "CMP"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);
		                                                strcpy  (param2, "Y");                                           return 0; //// could merge into param1 ???
		case 0xDD: strcpy (opcode, "CMP"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);
		                                                strcpy  (param2, "X");                                           return 0; //// could merge into param1 ???
		case 0xDE: strcpy (opcode, "DEC"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);
		                                                strcpy  (param2, "X");                                           return 0; //// could merge into param1 ???

		case 0xE0: strcpy (opcode, "CPX"); *length = 2; sprintf (param1, "#$%02x", memory[ptr+1]);                       return 0;
		case 0xE1: strcpy (opcode, "SBC"); *length = 2; sprintf (param1, "($%02x,X)", memory[ptr+1]);                    return 0;
		case 0xE4: strcpy (opcode, "CPX"); *length = 2; sprintf (param1, "$%02x", memory[ptr+1]);                        return 0;
		case 0xE5: strcpy (opcode, "SBC"); *length = 2; sprintf (param1, "$%02x", memory[ptr+1]);                        return 0;
		case 0xE6: strcpy (opcode, "INC"); *length = 2; sprintf (param1, "$%02x", memory[ptr+1]);                        return 0;
		case 0xE8: strcpy (opcode, "INX"); *length = 1;                                                                  return 0;
		case 0xE9: strcpy (opcode, "SBC"); *length = 2; sprintf (param1, "#$%02x", memory[ptr+1]);                       return 0;
		case 0xEA: strcpy (opcode, "NOP"); *length = 1;                                                                  return 0;
		case 0xEC: strcpy (opcode, "CPX"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);     return 0;
		case 0xED: strcpy (opcode, "SBC"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);     return 0;
		case 0xEE: strcpy (opcode, "INC"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);     return 0;

		case 0xF0: strcpy (opcode, "BEQ"); *length = 2; sprintf (param1, "&$%04x", *alt_ptr = (uint16_t)(ptr + (signed char)memory[ptr+1] + *length));
		                                                sprintf (comment, "%02x", memory[ptr+1]);                        return 1;
		case 0xF1: strcpy (opcode, "SBC"); *length = 2; sprintf (param1, "($%02x)", memory[ptr+1]);
		                                                strcpy  (param2, "Y");                                           return 0; //// could merge into param1 ???
		case 0xF5: strcpy (opcode, "SBC"); *length = 2; sprintf (param1, "$%02x", memory[ptr+1]);
		                                                strcpy  (param2, "X");                                           return 0; //// could merge into param1 ???
		case 0xF6: strcpy (opcode, "INC"); *length = 2; sprintf (param1, "$%02x", memory[ptr+1]);
		                                                strcpy  (param2, "X");                                           return 0; //// could merge into param1 ???
		case 0xF8: strcpy (opcode, "SED"); *length = 1;                                                                  return 0;
		case 0xF9: strcpy (opcode, "SBC"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);
		                                                strcpy  (param2, "Y");                                           return 0; //// could merge into param1 ???
		case 0xFD: strcpy (opcode, "SBC"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);
		                                                strcpy  (param2, "X");                                           return 0; //// could merge into param1 ???
		case 0xFE: strcpy (opcode, "SBC"); *length = 3; sprintf (param1, "$%02x%02x", memory[ptr+2], memory[ptr+1]);
		                                                strcpy  (param2, "X");                                           return 0; //// could merge into param1 ???
	}
	return -2;
}

