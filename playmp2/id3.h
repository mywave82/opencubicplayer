#ifndef _PLAYMP2_ID3_H
#define _PLAYMP2_ID3_H

struct ID3v1data_t /* no need to free */
{
	uint8_t title[61];   /* goes to the file-list as-is */
	uint8_t artist[61];  /* will replace composer in the file-list */
	uint8_t album[61];   /* will replace style in the file-list */
	uint8_t comment[46]; /* goes to the file-list as-is */
	uint8_t genre;
	uint8_t subgenre[21];
	uint8_t year[5];     /* goes to the file-list as-is */
	int16_t track;
};

struct ID3_pic_t
{
	int is_jpeg;
	int is_png;
	int size;
	uint8_t *data;
};

struct ID3_t
{
	int serial;

	uint8_t *TIT1; /* Content Group */
	uint8_t *TIT2; /* Track Title   */
	uint8_t *TIT3; /* Subtitle      */
	uint8_t *TPE1; /* Lead Artist   */
	uint8_t *TPE2; /* Band          */
	uint8_t *TPE3; /* Conductor     */
	uint8_t *TPE4; /* Interpretor   */
	uint8_t *TALB; /* Album         */
	uint8_t *TCOM; /* Composer      */
	uint8_t *TEXT; /* Lyrics        */
	uint8_t *TRCK; /* Track number  */
	uint8_t *TCON; /* Content Type  */ /* Needs further rendering */
	uint8_t *TDRC; // Recorded YYYY-MM-ddHH:mm:ss/next one or space (strip precision from the right   / is duration, space is list*/  
	uint8_t *TDRL; // Released YYYY-MM-ddHH:mm:ss/next one or space (strip precision from the right   / is duration, space is list */
	uint8_t *TYER; // YEAR YYYY (recording)
	uint8_t *TDAT; // DATE DDMM (recording)
	uint8_t *TIME; // TIME HHMM (recording)
	uint8_t *COMM; /* Comment       */
	// TODO APIC
	struct ID3_pic_t APIC[0x15];
	// TODO tag_is_an_update
	int tag_is_an_update;
};

extern const char *ID3_APIC_Titles[0x15]; 

/* length should be 128 bytes
 * source can point to single ID3v1.0 or ID3v1.1 block
 *
 * returns non-zero if invalid
 */

int parse_ID3v1x(struct ID3v1data_t *data, const unsigned char *source, unsigned int length);

/* length should be 128 bytes
 * source should point to single ID3v1.2 block
 *
 * dest should be prefilled by parse_ID3v1x()
 *
 * returns non-zero if invalid
 */
int parse_ID3v12(struct ID3v1data_t *data, const unsigned char *source, unsigned int length);

/* destination is memset to zero initially
 *
 * returns -1 if something goes wrong, 0 if OK
 */
int finalize_ID3v1(struct ID3_t *destination, struct ID3v1data_t *data);

/* source data will likely be modified during unescaping sync sequences, but will touch data outside the original bounds
 *
 * returns non-zero if invalid
 */
int parse_ID3v2x(struct ID3_t *destination, unsigned char *source, uint32_t length);

/* calls free() on all members, and sets them back to zero */
void ID3_clear(struct ID3_t *destination);

#endif
