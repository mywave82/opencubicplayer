#ifndef _TYPES_H
#define _TYPES_H
/* include this file AFTER you have included libc files, since some libc's defines these, and get confused if predefined */

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#ifdef HAVE_BYTESWAP_H
#include <byteswap.h>
#else
#define __bswap_16(x) \
     ((((x) >> 8) & 0xff) | (((x) & 0xff) << 8))
#define __bswap_32(x) \
     ((((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >>  8) |               \
      (((x) & 0x0000ff00) <<  8) | (((x) & 0x000000ff) << 24))
#define __bswap_64(x) \
     ((((x) & 0xff00000000000000ull) >> 56)                                   \
      | (((x) & 0x00ff000000000000ull) >> 40)                                 \
      | (((x) & 0x0000ff0000000000ull) >> 24)                                 \
      | (((x) & 0x000000ff00000000ull) >> 8)                                  \
      | (((x) & 0x00000000ff000000ull) << 8)                                  \
      | (((x) & 0x0000000000ff0000ull) << 24)                                 \
      | (((x) & 0x000000000000ff00ull) << 40)                                 \
      | (((x) & 0x00000000000000ffull) << 56))
#endif

#ifdef WORDS_BIGENDIAN
#   define int32_little(x)   ((int32_t)(__bswap_32 (x)))
#   define int16_little(x)   ((int16_t)(__bswap_16 (x)))
#   define uint32_little(x)  ((uint32_t)(__bswap_32 (x)))
#   define uint16_little(x)  ((uint16_t)(__bswap_16 (x)))
#   define int32_big(x)      (x)
#   define int16_big(x)      (x)
#   define uint32_big(x)     (x)
#   define uint16_big(x)     (x)

# else
#   define int32_little(x)   (x)
#   define int16_little(x)   (x)
#   define uint32_little(x)  (x)
#   define uint16_little(x)  (x)
#   define int32_big(x)      ((int32_t)(__bswap_32 (x)))
#   define int16_big(x)      ((int16_t)(__bswap_16 (x)))
#   define uint32_big(x)     ((uint32_t)(__bswap_32 (x)))
#   define uint16_big(x)     ((uint16_t)(__bswap_16 (x)))
# endif

#endif
