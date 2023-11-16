#include <stddef.h>
#include <stdint.h>

static constexpr uint8_t const the_white_room_1_dds[] = {
#include "bin2h/_internal_the-white-room-1.dds.inl"
};

extern uint8_t const *const the_white_room_1_dds_range_base = the_white_room_1_dds;

extern size_t const the_white_room_1_dds_range_size = sizeof(the_white_room_1_dds);
