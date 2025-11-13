#pragma once

#include "xxh3.h"

// From https://github.com/Cyan4973/xxHash/issues/453:
// > In order to extract 32-bit from a good 64-bit hash result,
// > there are many possible choices, which are all valid.
// > I would typically grab the lower 32-bit and call it a day.
// Let's hope it works fine :-)
template <constexpr_xxh3::BytesType Bytes>
consteval uint32_t pq_hash_const(const Bytes& input) noexcept {
    return constexpr_xxh3::XXH3_64bits_const(std::data(input), constexpr_xxh3::bytes_size(input));
}

static inline uint32_t pq_hash(char const * input, size_t len)
{
    return constexpr_xxh3::XXH3_64bits(input, len);
}