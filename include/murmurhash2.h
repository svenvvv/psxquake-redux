#ifndef MURMURHASH2_H
#define MURMURHASH2_H

#include <stdint.h>

uint32_t MurmurHash2 (const void * key, int len);
_Static_assert(sizeof(int) == 4 && "the included murmurhash2 implementation assumes 4-byte ints, fixme");

#endif //MURMURHASH2_H
