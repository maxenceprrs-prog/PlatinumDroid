#ifndef PAL_COMMON_H
#define PAL_COMMON_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifndef BOOL
typedef int BOOL;
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef u32
typedef uint32_t u32;
#endif

#ifndef u16
typedef uint16_t u16;
#endif

#ifndef u8
typedef uint8_t u8;
#endif

#ifndef s32
typedef int32_t s32;
#endif

#ifndef s16
typedef int16_t s16;
#endif

#ifndef s8
typedef int8_t s8;
#endif

#endif /* PAL_COMMON_H */
