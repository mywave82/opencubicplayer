/*-----------------------------------------------------------------------------

	ST-Sound ( YM files player library )

	Copyright (C) 1995-1999 Arnaud Carre ( http://leonard.oxg.free.fr )

	LZH depacking routine
	Original LZH code by Haruhiko Okumura (1991) and Kerwin F. Medina (1996)

	I changed to C++ object to remove global vars, so it should be thread safe now.

-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------

	This file is part of ST-Sound

	ST-Sound is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	ST-Sound is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with ST-Sound; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

-----------------------------------------------------------------------------*/


#ifndef LZH_H
#define LZH_H


#define BUFSIZE (1024 * 4)

#if defined(_WIN32) || defined(_WIN64)

typedef unsigned char  lzh_uchar;   /*  8 bits or more */
typedef unsigned int   lzh_uint;    /* 16 bits or more */
typedef unsigned short lzh_ushort;  /* 16 bits or more */
typedef unsigned long  lzh_ulong;   /* 32 bits or more */

#else

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

typedef uint8_t  lzh_uchar;
typedef uint16_t lzh_ushort;
typedef uint_fast32_t lzh_uint;
typedef uint_fast32_t lzh_ulong;

#endif

#ifndef CHAR_BIT
    #define CHAR_BIT            8
#endif

#ifndef UCHAR_MAX
    #define UCHAR_MAX           255
#endif

typedef	lzh_ushort		BITBUFTYPE;

#define BITBUFSIZ (CHAR_BIT * sizeof (BITBUFTYPE))
#define DICBIT    13                              /* 12(-lh4-) or 13(-lh5-) */
#define DICSIZ (1U << DICBIT)
#define MAXMATCH 256                              /* formerly F (not more than UCHAR_MAX + 1) */
#define THRESHOLD  3                              /* choose optimal value */
#define NC (UCHAR_MAX + MAXMATCH + 2 - THRESHOLD) /* alphabet = {0, 1, 2, ..., NC - 1} */
#define CBIT 9                                    /* $\lfloor \log_2 NC \rfloor + 1$ */
#define CODE_BIT  16                              /* codeword length */

#define MAX_HASH_VAL (3 * DICSIZ + (DICSIZ / 512 + 1) * UCHAR_MAX)

#define NP (DICBIT + 1)
#define NT (CODE_BIT + 3)
#define PBIT 4      /* smallest integer such that (1U << PBIT) > NP */
#define TBIT 5      /* smallest integer such that (1U << TBIT) > NT */
#if NT > NP
    #define NPT NT
#else
    #define NPT NP
#endif


class CLzhDepacker
{
public:

	bool	LzUnpack(const void *pSrc,int srcSize,void *pDst,int dstSize);

private:

	//----------------------------------------------
	// New stuff to handle memory IO
	//----------------------------------------------
	lzh_uchar	*	m_pSrc;
	int			m_srcSize;
	lzh_uchar	*	m_pDst;
	int			m_dstSize;

	int			DataIn(void *pBuffer,int nBytes);
	int			DataOut(void *pOut,int nBytes);



	//----------------------------------------------
	// Original Lzhxlib static func
	//----------------------------------------------
	void		fillbuf (int n);
	lzh_ushort		getbits (int n);
	void		init_getbits (void);
	int			make_table (int nchar, lzh_uchar *bitlen,int tablebits, lzh_ushort *table);
	void		read_pt_len (int nn, int nbit, int i_special);
	void		read_c_len (void);
	lzh_ushort		decode_c(void);
	lzh_ushort		decode_p(void);
	void		huf_decode_start (void);
	void		decode_start (void);
	void		decode (lzh_uint count, lzh_uchar buffer[]);


	//----------------------------------------------
	// Original Lzhxlib static vars
	//----------------------------------------------
	int			fillbufsize;
	lzh_uchar		buf[BUFSIZE];
	lzh_uchar		outbuf[DICSIZ];
	lzh_ushort		left [2 * NC - 1];
	lzh_ushort		right[2 * NC - 1];
	BITBUFTYPE	bitbuf;
	lzh_uint		subbitbuf;
	int			bitcount;
	int			decode_j;    /* remaining bytes to copy */
	lzh_uchar		c_len[NC];
	lzh_uchar		pt_len[NPT];
	lzh_uint		blocksize;
	lzh_ushort		c_table[4096];
	lzh_ushort		pt_table[256];
	int			with_error;

	lzh_uint		fillbuf_i;			// NOTE: these ones are not initialized at constructor time but inside the fillbuf and decode func.
	lzh_uint		decode_i;
};


#endif /* ifndef LZH_H */
