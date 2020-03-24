#ifndef _PFONTS_H
#define _PFONTS_H

/* cp437_to_unicode gives lookup for these - with some few exceptions TODO*/
extern unsigned char plFont44[256][2];
extern unsigned char plFont88[256][8];
extern unsigned char plFont816[256][16];

struct FontData_8x16_t
{
	uint16_t codepoint;
	uint8_t data[16];
};

extern const struct FontData_8x16_t plFont_8x16_latin1_addons[41];

extern unsigned char plFont816_007c[]; // latin1
extern unsigned char plFont816_00a4[]; // latin1
extern unsigned char plFont816_00a8[]; // latin1
extern unsigned char plFont816_00a9[]; // latin1
extern unsigned char plFont816_00ad[]; // latin1
extern unsigned char plFont816_00ae[]; // latin1
extern unsigned char plFont816_00af[]; // latin1
extern unsigned char plFont816_00b3[]; // latin1
extern unsigned char plFont816_00b4[]; // latin1
extern unsigned char plFont816_00b8[]; // latin1

extern unsigned char plFont816_00b9[]; // latin1
extern unsigned char plFont816_00be[]; // latin1
extern unsigned char plFont816_00c0[]; // latin1
extern unsigned char plFont816_00c1[]; // latin1
extern unsigned char plFont816_00c2[]; // latin1
extern unsigned char plFont816_00c3[]; // latin1
extern unsigned char plFont816_00c8[]; // latin1
extern unsigned char plFont816_00ca[]; // latin1
extern unsigned char plFont816_00cb[]; // latin1
extern unsigned char plFont816_00cc[]; // latin1

extern unsigned char plFont816_00cd[]; // latin1
extern unsigned char plFont816_00ce[]; // latin1
extern unsigned char plFont816_00cf[]; // latin1
extern unsigned char plFont816_00d0[]; // latin1
extern unsigned char plFont816_00d2[]; // latin1
extern unsigned char plFont816_00d3[]; // latin1
extern unsigned char plFont816_00d4[]; // latin1
extern unsigned char plFont816_00d5[]; // latin1
extern unsigned char plFont816_00d7[]; // latin1
extern unsigned char plFont816_00d8[]; // latin1

extern unsigned char plFont816_00d9[]; // latin1
extern unsigned char plFont816_00da[]; // latin1
extern unsigned char plFont816_00db[]; // latin1
extern unsigned char plFont816_00dd[]; // latin1
extern unsigned char plFont816_00de[]; // latin1
extern unsigned char plFont816_00e3[]; // latin1
extern unsigned char plFont816_00f0[]; // latin1
extern unsigned char plFont816_00f5[]; // latin1
extern unsigned char plFont816_00f8[]; // latin1
extern unsigned char plFont816_00fd[]; // latin1
extern unsigned char plFont816_00fe[]; // latin1

#endif
