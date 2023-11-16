#include <stddef.h>
#include <stdint.h>

static constexpr uint8_t const keqing_lolita_dds[] = {
#include "bin2h/_internal_keqing-lolita.dds.inl"
};

extern uint8_t const *const keqing_lolita_dds_range_base = keqing_lolita_dds;

extern size_t const keqing_lolita_dds_range_size = sizeof(keqing_lolita_dds);
