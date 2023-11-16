#include <stddef.h>
#include <stdint.h>

static constexpr uint8_t const the_white_room_bin[] = {
#include "bin2h/_internal_the-white-room.bin.inl"
};

extern uint8_t const *const the_white_room_bin_range_base = the_white_room_bin;

extern size_t const the_white_room_bin_range_size = sizeof(the_white_room_bin);
