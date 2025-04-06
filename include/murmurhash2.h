#ifndef MURMURHASH2_H
#define MURMURHASH2_H

#include <stdint.h>
#include <stddef.h>

uint32_t MurmurHash2 (const void * key, size_t len);
_Static_assert(sizeof(int) == 4 && "the included murmurhash2 implementation assumes 4-byte ints, fixme");

#endif //MURMURHASH2_H
