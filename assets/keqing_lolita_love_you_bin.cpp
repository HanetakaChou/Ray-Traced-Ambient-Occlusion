#include <stddef.h>
#include <stdint.h>

static constexpr uint8_t const keqing_lolita_love_you_bin[] = {
#include "bin2h/_internal_keqing-lolita-love-you.bin.inl"
};

extern uint8_t const *const keqing_lolita_love_you_bin_range_base = keqing_lolita_love_you_bin;

extern size_t const keqing_lolita_love_you_bin_range_size = sizeof(keqing_lolita_love_you_bin);
