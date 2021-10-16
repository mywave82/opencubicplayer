int do_debug_print = 0;

#define CACHE_LINE_SIZE 3

struct cache_ocpfilehandle_t;
static void dump_self (struct cache_ocpfilehandle_t *self);

#include "filesystem-filehandle-cache.c"


#include "dirdb.h"
#include "filesystem-dir-mem.h"
#include "filesystem-file-mem.h"

uint32_t dirdbFindAndRef (uint32_t parent, const char *name, enum dirdb_use use)
{
	return 0;
}

static const char src_data[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static const int src_len = 26;

static int dst_offset = 0;
static int dst_len = 26;
static char dst_data[256];

static void dump_self (struct cache_ocpfilehandle_t *self)
{
	int i;

	fprintf (stderr,   "SRC:    %s\n"
	                   "HEAD:   ", src_data);
	for (i=0; i < self->cache_line[0].fill; i++)
	{
		fputc (self->cache_line[0].data[i], stderr);
	}
	fprintf (stderr, "\nMIDDLE: ");
	for (i=0; i < self->cache_line[1].offset; i++)
	{
		fputc (' ', stderr);
	}
	for (i=0; i < self->cache_line[1].fill; i++)
	{
		fputc (self->cache_line[1].data[i], stderr);
	}
	fprintf (stderr, "\nTAIL:   ");
	for (i=0; i < self->cache_line[2].offset; i++)
	{
		fputc (' ', stderr);
	}
	for (i=0; i < self->cache_line[2].fill; i++)
	{
		fputc (self->cache_line[2].data[i], stderr);
	}
#if 0
	fprintf (stderr, "\nPSTTAIL:");
	for (i=0; i < self->cache_line[3].offset; i++)
	{
		fputc (' ', stderr);
	}
	for (i=0; i < self->cache_line[3].fill; i++)
	{
		fputc (self->cache_line[3].data[i], stderr);
	}
#endif
	fprintf (stderr, "\nDST:    ");
	for (i=0; i < dst_offset; i++)
	{
		fputc (' ', stderr);
	}
	for (i=0; i < dst_len; i++)
	{
		fputc (dst_data[i], stderr);
	}
	fputc ('\n', stderr);
}

uint32_t dirdbRef (uint32_t ref, enum dirdb_use use)
{
	return ref;
}

void dirdbUnref (uint32_t ref, enum dirdb_use use)
{
}

struct ocpdir_t *ocpdir_t_fill_default_readdir_dir  (struct ocpdir_t *_self, uint32_t dirdb_ref)
{
	fprintf (stderr, "Dummy symbol ocpdir_t_fill_default_readdir_dir called?\n");
	_exit(1);
}

struct ocpfile_t *ocpdir_t_fill_default_readdir_file (struct ocpdir_t *_self, uint32_t dirdb_ref)
{
	fprintf (stderr, "Dummy symbol ocpdir_t_fill_default_readdir_file called?\n");
	_exit(1);
}

const char *ocpfile_t_fill_default_filename_override (struct ocpfile_t *file)
{
	return 0;
}

int ocpfilehandle_t_fill_default_ioctl (struct ocpfilehandle_t *s, const char *cmd, void *ptr)
{
	return -1;
}

struct task_t
{
	      int   seek_set;
	      int   request_bytes;
	      int   should_return_bytes;
	const char *expected_result;
	      int   PRE_should_contain_bytes;
	const char *PRE_should_contain_data;
	      int   MOVING_should_contain_bytes;
	const char *MOVING_should_contain_data;
	      int   POST_should_contain_bytes;
	const char *POST_should_contain_data;
};

const static struct task_t chain_of_reads[] = {
{ 0, 1, 1, "A", 1, "A  ", 0, "   ", 0, "   "},
{-1, 1, 1, "B", 2, "AB ", 0, "   ", 0, "   "},
{-1, 1, 1, "C", 3, "ABC", 0, "   ", 0, "   "},
{-1, 1, 1, "D", 3, "ABC", 1, "D  ", 0, "   "},
{-1, 1, 1, "E", 3, "ABC", 2, "DE ", 0, "   "},
{-1, 1, 1, "F", 3, "ABC", 3, "DEF", 0, "   "},
{-1, 1, 1, "G", 3, "ABC", 3, "EFG", 0, "   "},
{-1, 1, 1, "H", 3, "ABC", 3, "FGH", 0, "   "},
{-1, 1, 1, "I", 3, "ABC", 3, "GHI", 0, "   "},
{-1, 1, 1, "J", 3, "ABC", 3, "HIJ", 0, "   "},
{-1, 1, 1, "K", 3, "ABC", 3, "IJK", 0, "   "},
{-1, 1, 1, "L", 3, "ABC", 3, "JKL", 0, "   "},
{-1, 1, 1, "M", 3, "ABC", 3, "KLM", 0, "   "},
{-1, 1, 1, "N", 3, "ABC", 3, "LMN", 0, "   "},
{-1, 1, 1, "O", 3, "ABC", 3, "MNO", 0, "   "},
{-1, 1, 1, "P", 3, "ABC", 3, "NOP", 0, "   "},
{-1, 1, 1, "Q", 3, "ABC", 3, "OPQ", 0, "   "},
{-1, 1, 1, "R", 3, "ABC", 3, "PQR", 0, "   "},
{-1, 1, 1, "S", 3, "ABC", 3, "QRS", 0, "   "},
{-1, 1, 1, "T", 3, "ABC", 3, "RST", 0, "   "},
{-1, 1, 1, "U", 3, "ABC", 3, "STU", 0, "   "},
{-1, 1, 1, "V", 3, "ABC", 3, "TUV", 0, "   "},
{-1, 1, 1, "W", 3, "ABC", 3, "UVW", 0, "   "},
{-1, 1, 1, "X", 3, "ABC", 3, "VWX", 0, "   "},
{-1, 1, 1, "Y", 3, "ABC", 3, "WXY", 0, "   "},
{-1, 1, 1, "Z", 3, "ABC", 0, "   ", 3, "XYZ"},

{ 0, 2, 2, "AB", 3, "ABC", 0, "   ", 3, "XYZ"},
{-1, 2, 2, "CD", 3, "ABC", 1, "D  ", 3, "XYZ"},
{-1, 2, 2, "EF", 3, "ABC", 3, "DEF", 3, "XYZ"},
{-1, 2, 2, "GH", 3, "ABC", 3, "FGH", 3, "XYZ"},
{-1, 2, 2, "IJ", 3, "ABC", 3, "HIJ", 3, "XYZ"},
{-1, 2, 2, "KL", 3, "ABC", 3, "JKL", 3, "XYZ"},
{-1, 2, 2, "MN", 3, "ABC", 3, "LMN", 3, "XYZ"},
{-1, 2, 2, "OP", 3, "ABC", 3, "NOP", 3, "XYZ"},
{-1, 2, 2, "QR", 3, "ABC", 3, "PQR", 3, "XYZ"},
{-1, 2, 2, "ST", 3, "ABC", 3, "RST", 3, "XYZ"},
{-1, 2, 2, "UV", 3, "ABC", 3, "TUV", 3, "XYZ"},
{-1, 2, 2, "WX", 3, "ABC", 3, "UVW", 3, "XYZ"},
{-1, 2, 2, "YZ", 3, "ABC", 3, "UVW", 3, "XYZ"},

{ 0, 3, 3, "ABC", 3, "ABC", 3, "UVW", 3, "XYZ"},
{-1, 3, 3, "DEF", 3, "ABC", 3, "DEF", 3, "XYZ"},
{-1, 3, 3, "GHI", 3, "ABC", 3, "GHI", 3, "XYZ"},
{-1, 3, 3, "JKL", 3, "ABC", 3, "JKL", 3, "XYZ"},
{-1, 3, 3, "MNO", 3, "ABC", 3, "MNO", 3, "XYZ"},
{-1, 3, 3, "PQR", 3, "ABC", 3, "PQR", 3, "XYZ"},
{-1, 3, 3, "STU", 3, "ABC", 3, "STU", 3, "XYZ"},
{-1, 3, 3, "VWX", 3, "ABC", 3, "UVW", 3, "XYZ"},
{-1, 3, 2, "YZ ", 3, "ABC", 3, "UVW", 3, "XYZ"},

{ 0, 4, 4, "ABCD", 3, "ABC", 1, "D  ", 3, "XYZ"},
{-1, 4, 4, "EFGH", 3, "ABC", 3, "FGH", 3, "XYZ"},
{-1, 4, 4, "IJKL", 3, "ABC", 3, "JKL", 3, "XYZ"},
{-1, 4, 4, "MNOP", 3, "ABC", 3, "NOP", 3, "XYZ"},
{-1, 4, 4, "QRST", 3, "ABC", 3, "RST", 3, "XYZ"},
{-1, 4, 4, "UVWX", 3, "ABC", 3, "UVW", 3, "XYZ"},
{-1, 4, 2, "YZ  ", 3, "ABC", 3, "UVW", 3, "XYZ"},

{25, 4, 1, "Z   ", 3, "ABC", 3, "UVW", 3, "XYZ"},
{24, 4, 2, "YZ  ", 3, "ABC", 3, "UVW", 3, "XYZ"},
{23, 4, 3, "XYZ ", 3, "ABC", 3, "UVW", 3, "XYZ"},
{22, 4, 4, "WXYZ", 3, "ABC", 3, "UVW", 3, "XYZ"},
{21, 4, 4, "VWXY", 3, "ABC", 3, "UVW", 3, "XYZ"},
{20, 4, 4, "UVWX", 3, "ABC", 3, "UVW", 3, "XYZ"},
{19, 4, 4, "TUVW", 3, "ABC", 3, "UVW", 3, "XYZ"},
{18, 4, 4, "STUV", 3, "ABC", 3, "TUV", 3, "XYZ"},
{17, 4, 4, "RSTU", 3, "ABC", 3, "STU", 3, "XYZ"},
{16, 4, 4, "QRST", 3, "ABC", 3, "RST", 3, "XYZ"},
{15, 4, 4, "PQRS", 3, "ABC", 3, "QRS", 3, "XYZ"},
{14, 4, 4, "OPQR", 3, "ABC", 3, "PQR", 3, "XYZ"},
{13, 4, 4, "NOPQ", 3, "ABC", 3, "OPQ", 3, "XYZ"},
{12, 4, 4, "MNOP", 3, "ABC", 3, "NOP", 3, "XYZ"},
{11, 4, 4, "LMNO", 3, "ABC", 3, "MNO", 3, "XYZ"},
{10, 4, 4, "KLMN", 3, "ABC", 3, "LMN", 3, "XYZ"},
{ 9, 4, 4, "JKLM", 3, "ABC", 3, "KLM", 3, "XYZ"},
{ 8, 4, 4, "IJKL", 3, "ABC", 3, "JKL", 3, "XYZ"},
{ 6, 4, 4, "GHIJ", 3, "ABC", 3, "HIJ", 3, "XYZ"},
{ 5, 4, 4, "FGHI", 3, "ABC", 3, "GHI", 3, "XYZ"},
{ 4, 4, 4, "EFGH", 3, "ABC", 3, "FGH", 3, "XYZ"},
{ 3, 4, 4, "DEFG", 3, "ABC", 3, "EFG", 3, "XYZ"},
{ 2, 4, 4, "CDEF", 3, "ABC", 3, "DEF", 3, "XYZ"},
{ 1, 4, 4, "BCDE", 3, "ABC", 3, "DEF", 3, "XYZ"},
{ 0, 4, 4, "ABCD", 3, "ABC", 3, "DEF", 3, "XYZ"},

{24, 4, 2, "YZ  ", 3, "ABC", 3, "DEF", 3, "XYZ"},
{22, 4, 4, "WXYZ", 3, "ABC", 1, "W  ", 3, "XYZ"},
{20, 4, 4, "UVWX", 3, "ABC", 3, "UVW", 3, "XYZ"},
{18, 4, 4, "STUV", 3, "ABC", 3, "TUV", 3, "XYZ"},
{16, 4, 4, "QRST", 3, "ABC", 3, "RST", 3, "XYZ"},
{14, 4, 4, "OPQR", 3, "ABC", 3, "PQR", 3, "XYZ"},
{12, 4, 4, "MNOP", 3, "ABC", 3, "NOP", 3, "XYZ"},
{10, 4, 4, "KLMN", 3, "ABC", 3, "LMN", 3, "XYZ"},
{ 8, 4, 4, "IJKL", 3, "ABC", 3, "JKL", 3, "XYZ"},
{ 6, 4, 4, "GHIJ", 3, "ABC", 3, "HIJ", 3, "XYZ"},
{ 4, 4, 4, "EFGH", 3, "ABC", 3, "FGH", 3, "XYZ"},
{ 2, 4, 4, "CDEF", 3, "ABC", 3, "DEF", 3, "XYZ"},
{ 0, 4, 4, "ABCD", 3, "ABC", 3, "DEF", 3, "XYZ"},

{24, 4, 2, "YZ  ", 3, "ABC", 3, "DEF", 3, "XYZ"},
{21, 4, 4, "VWXY", 3, "ABC", 2, "VW ", 3, "XYZ"},
{18, 4, 4, "STUV", 3, "ABC", 3, "TUV", 3, "XYZ"},
{15, 4, 4, "PQRS", 3, "ABC", 3, "QRS", 3, "XYZ"},
{12, 4, 4, "MNOP", 3, "ABC", 3, "NOP", 3, "XYZ"},
{ 9, 4, 4, "JKLM", 3, "ABC", 3, "KLM", 3, "XYZ"},
{ 6, 4, 4, "GHIJ", 3, "ABC", 3, "HIJ", 3, "XYZ"},
{ 3, 4, 4, "DEFG", 3, "ABC", 3, "EFG", 3, "XYZ"},
{ 0, 4, 4, "ABCD", 3, "ABC", 3, "DEF", 3, "XYZ"},

{24, 4, 2, "YZ  ", 3, "ABC", 3, "DEF", 3, "XYZ"},
{20, 4, 4, "UVWX", 3, "ABC", 3, "UVW", 3, "XYZ"},
{16, 4, 4, "QRST", 3, "ABC", 3, "RST", 3, "XYZ"},
{12, 4, 4, "MNOP", 3, "ABC", 3, "NOP", 3, "XYZ"},
{ 8, 4, 4, "IJKL", 3, "ABC", 3, "JKL", 3, "XYZ"},
{ 4, 4, 4, "EFGH", 3, "ABC", 3, "FGH", 3, "XYZ"},
{ 0, 4, 4, "ABCD", 3, "ABC", 3, "DEF", 3, "XYZ"}

};

#define chain_of_reads_n (sizeof (chain_of_reads) / sizeof (chain_of_reads[0]))

int results[chain_of_reads_n];

int main (int argc, char *argv[])
{
	char *mem;
	struct ocpfile_t *f;
	struct ocpfilehandle_t *fh;
	struct ocpdir_t *test_dir;

	do_debug_print = 0;

	test_dir = ocpdir_mem_getdir_t(ocpdir_mem_alloc (0, "test:"));

 	mem = malloc (src_len);
	memcpy (mem, src_data, src_len);
	f = mem_file_open (test_dir, 0, mem, src_len);

	fh = f->open (f);
	{
		struct ocpfilehandle_t *c1 = cache_filehandle_open (fh);
		int i;

		for (i=0; i < chain_of_reads_n; i++)
		{
			int readres;

			memset (dst_data, '.', dst_len);

			if (chain_of_reads[i].seek_set >= 0)
			{
				c1->seek_set (c1, chain_of_reads[i].seek_set);
			}
			readres = c1->read (c1, dst_data + chain_of_reads[i].expected_result[0] - 'A', chain_of_reads[i].request_bytes);
			if (readres != chain_of_reads[i].should_return_bytes)
			{
				results[i] |= 1; /* wrong number of returned bytes */
			} else if (chain_of_reads[i].should_return_bytes)
			{
				if (memcmp (dst_data + chain_of_reads[i].expected_result[0] - 'A', chain_of_reads[i].expected_result, chain_of_reads[i].should_return_bytes))
				{
					results[i] |= 2; /* return data missmatch */
				}
			}
			if (((struct cache_ocpfilehandle_t *)c1)->cache_line[0].fill != chain_of_reads[i].PRE_should_contain_bytes)
			{
				results[i] |= 4; /* PRE has wrong length */
			} else if (chain_of_reads[i].PRE_should_contain_bytes)
			{
				if (memcmp (((struct cache_ocpfilehandle_t *)c1)->cache_line[0].data, chain_of_reads[i].PRE_should_contain_data, chain_of_reads[i].PRE_should_contain_bytes))
				{
					results[i] |= 8; /* PRE has wrong data */
				}
			}
			if (((struct cache_ocpfilehandle_t *)c1)->cache_line[1].fill != chain_of_reads[i].MOVING_should_contain_bytes)
			{
				results[i] |= 16; /* MOVING has wrong length */
			} else if (chain_of_reads[i].MOVING_should_contain_bytes)
			{
				if (memcmp (((struct cache_ocpfilehandle_t *)c1)->cache_line[1].data, chain_of_reads[i].MOVING_should_contain_data, chain_of_reads[i].MOVING_should_contain_bytes))
				{
					results[i] |= 32; /* MOVING has wrong data */
				}
			}
			if (((struct cache_ocpfilehandle_t *)c1)->cache_line[2].fill != chain_of_reads[i].POST_should_contain_bytes)
			{
				results[i] |= 64; /* POST has wrong length */
			} else if (chain_of_reads[i].POST_should_contain_bytes)
			{
				if (memcmp (((struct cache_ocpfilehandle_t *)c1)->cache_line[2].data, chain_of_reads[i].POST_should_contain_data, chain_of_reads[i].POST_should_contain_bytes))
				{
					results[i] |= 128; /* POST has wrong data */
				}
			}

			if (results[i])
			{
				fprintf (stderr, " %d ", results[i]);
			} else {
				fprintf (stderr, ".");
			}
		}
		fprintf (stderr, "\n");
#if 0
		for (i=0; i < 26; i++)
		{
			int r;
			memset (dst_data, '.', dst_len);
			r = c1->read (c1, dst_data+i, 1);
			fprintf (stderr, "\n\ni=%d d=%c (r=%d)\n\n", i, dst_data[i], r);
		}

		c1->seek_set (c1, 0);

		for (i=0; i < (26-1); i+=2)
		{
			int r;
			memset (dst_data, '.', dst_len);
			r = c1->read (c1, dst_data+i, 2);
			fprintf (stderr, "\n\ni=%d d=%c%c (r=%d)\n\n", i, dst_data[i], dst_data[i+1], r);
		}

		c1->seek_set (c1, 0);

		for (i=0; i < (26-2); i+=3)
		{
			int r;
			memset (dst_data, '.', dst_len);
			r = c1->read (c1, dst_data+i, 3);
			fprintf (stderr, "\n\ni=%d d=%c%c%c (r=%d)\n\n", i, dst_data[i], dst_data[i+1], dst_data[i+2], r);
		}

		c1->seek_set (c1, 0);

		for (i=0; i < (26-3); i+=4)
		{
			int r;
			memset (dst_data, '.', dst_len);
			r = c1->read (c1, dst_data+i, 4);
			fprintf (stderr, "\n\ni=%d d=%c%c%c%c (r=%d)\n\n", i, dst_data[i], dst_data[i+1], dst_data[i+2], dst_data[i+3], r);
		}

		for (i=26-4; i >= 0; i--)
		{
			int r;
			fprintf (stderr, "seek=%d len=4\n", i);
			c1->seek_set (c1, i);
			memset (dst_data, '.', dst_len);
			r = c1->read (c1, dst_data+i, 4);
			fprintf (stderr, "\n\ni=%d d=%c%c%c%c (r=%d)\n\n", i, dst_data[i], dst_data[i+1], dst_data[i+2], dst_data[i+3], r);
		}

		for (i=26-4; i >= 0; i-=2)
		{
			int r;
			fprintf (stderr, "seek=%d len=4\n", i);
			c1->seek_set (c1, i);
			memset (dst_data, '.', dst_len);
			r = c1->read (c1, dst_data+i, 4);
			fprintf (stderr, "\n\ni=%d d=%c%c%c%c (r=%d)\n\n", i, dst_data[i], dst_data[i+1], dst_data[i+2], dst_data[i+3], r);
		}

		for (i=26-4; i >= 0; i-=3)
		{
			int r;
			fprintf (stderr, "seek=%d len=4\n", i);
			c1->seek_set (c1, i);
			memset (dst_data, '.', dst_len);
			r = c1->read (c1, dst_data+i, 4);
			fprintf (stderr, "\n\ni=%d d=%c%c%c%c (r=%d)\n\n", i, dst_data[i], dst_data[i+1], dst_data[i+2], dst_data[i+3], r);
		}

		for (i=26-4; i >= 0; i-=4)
		{
			int r;
			fprintf (stderr, "seek=%d len=4\n", i);
			c1->seek_set (c1, i);
			memset (dst_data, '.', dst_len);
			r = c1->read (c1, dst_data+i, 4);
			fprintf (stderr, "\n\ni=%d d=%c%c%c%c (r=%d)\n\n", i, dst_data[i], dst_data[i+1], dst_data[i+2], dst_data[i+3], r);
		}
#endif

		c1->unref (c1);
	}
	f->unref (f);
	fh->unref (fh);


 	mem = malloc (src_len);
	memcpy (mem, src_data, src_len);
	f = mem_file_open (test_dir, 0, mem, src_len);
	fh = f->open (f);
	{
		struct ocpfilehandle_t *c1 = cache_filehandle_open (fh);
		int i;

		for (i=0; i < chain_of_reads_n; i++)
		{
			/*int readres;*/

			memset (dst_data, '.', dst_len);

			if (chain_of_reads[i].seek_set >= 0)
			{
				c1->seek_set (c1, chain_of_reads[i].seek_set);
			}
			do_debug_print = results[i];
			/* readres = */ c1->read (c1, dst_data + chain_of_reads[i].expected_result[0] - 'A', chain_of_reads[i].request_bytes);
			if (do_debug_print)
			{
				int j;
				fprintf (stderr, "expected:\n");
	                   	fprintf (stderr, "HEAD:   \"");
				for (j=0; j < chain_of_reads[i].PRE_should_contain_bytes; j++)
				{
					fputc (chain_of_reads[i].PRE_should_contain_data[j], stderr);
				}
				fprintf (stderr, "\"\nMIDDLE: \"");
				for (j=0; j < chain_of_reads[i].MOVING_should_contain_bytes; j++)
				{
					fputc (chain_of_reads[i].MOVING_should_contain_data[j], stderr);
				}
				fprintf (stderr, "\"\nTAIL:   \"");
				for (j=0; j < chain_of_reads[i].POST_should_contain_bytes; j++)
				{
					fputc (chain_of_reads[i].POST_should_contain_data[j], stderr);
				}
				fprintf (stderr, "\"\nDST:    \"");
				for (j=0; j < chain_of_reads[i].should_return_bytes; j++)
				{
					fputc (chain_of_reads[i].expected_result[j], stderr);
				}
				fprintf (stderr, "\"\n\n");
			}
		}
		c1->unref (c1);
	}
	f->unref (f);
	fh->unref (fh);

	test_dir->unref (test_dir); test_dir = 0;

	return 0;
}
