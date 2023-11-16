#include <stddef.h>
#include <stdint.h>

static constexpr uint8_t const the_white_room_gltf[] = {
#include "bin2h/_internal_the-white-room.gltf.inl"
};

extern uint8_t const *const the_white_room_gltf_range_base = the_white_room_gltf;

extern size_t const the_white_room_gltf_range_size = sizeof(the_white_room_gltf);
