// WARNING: You are looking for the a different types.h - this one is just for the exe2minixfs tool!
#ifndef _MINIXFSTYPES_H_
#define _MINIXFSTYPES_H_

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <sys/types.h>
#include "kprintf.h"

#define ustl std

#include "../../common/include/console/debug.h"

typedef int8_t int8;
typedef uint8_t uint8;

typedef int16_t int16;
typedef uint16_t uint16;

typedef int32_t int32;
typedef uint32_t uint32;

typedef uint64_t uint64;
typedef int64_t int64;

typedef void* pointer;

typedef loff_t l_off_t;

class ArchCommon
{
  public:
    static void bzero(void* s, size_t n) { ::bzero(s,n); };
};

#endif
