#include "assets.h"

extern uint8_t const *const the_white_room_gltf_range_base;

extern size_t const the_white_room_gltf_range_size;

extern uint8_t const *const the_white_room_bin_range_base;

extern size_t const the_white_room_bin_range_size;

extern uint8_t const *const the_white_room_1_dds_range_base;

extern size_t const the_white_room_1_dds_range_size;

extern uint8_t const *const the_white_room_1_s_dds_range_base;

extern size_t const the_white_room_1_s_dds_range_size;

extern uint8_t const *const the_white_room_2_dds_range_base;

extern size_t const the_white_room_2_dds_range_size;

extern uint8_t const *const the_white_room_2_n_dds_range_base;

extern size_t const the_white_room_2_n_dds_range_size;

extern uint8_t const *const the_white_room_3_dds_range_base;

extern size_t const the_white_room_3_dds_range_size;

extern uint8_t const *const the_white_room_4_dds_range_base;

extern size_t const the_white_room_4_dds_range_size;

extern uint8_t const *const keqing_lolita_love_you_gltf_range_base;

extern size_t const keqing_lolita_love_you_gltf_range_size;

extern uint8_t const *const keqing_lolita_love_you_bin_range_base;

extern size_t const keqing_lolita_love_you_bin_range_size;

extern uint8_t const *const keqing_lolita_dds_range_base;

extern size_t const keqing_lolita_dds_range_size;

extern import_asset_input_stream_factory *import_asset_init_memory_input_stream_factory()
{
    constexpr size_t const input_stream_count = 11U;

    char const *const input_stream_file_names[input_stream_count] = {
        "the-white-room/the-white-room.gltf",
        "the-white-room/the-white-room.bin",
        "the-white-room/the-white-room-1.dds",
        "the-white-room/the-white-room-1_s.dds",
        "the-white-room/the-white-room-2.dds",
        "the-white-room/the-white-room-2_n.dds",
        "the-white-room/the-white-room-3.dds",
        "the-white-room/the-white-room-4.dds",
        "keqing-lolita/keqing-lolita-love-you.gltf",
        "keqing-lolita/keqing-lolita-love-you.bin",
        "keqing-lolita/keqing-lolita.dds"};

    void const *const input_stream_memory_range_bases[input_stream_count] = {
        the_white_room_gltf_range_base,
        the_white_room_bin_range_base,
        the_white_room_1_dds_range_base,
        the_white_room_1_s_dds_range_base,
        the_white_room_2_dds_range_base,
        the_white_room_2_n_dds_range_base,
        the_white_room_3_dds_range_base,
        the_white_room_4_dds_range_base,
        keqing_lolita_love_you_gltf_range_base,
        keqing_lolita_love_you_bin_range_base,
        keqing_lolita_dds_range_base,};

    size_t const input_stream_memory_range_sizes[input_stream_count] = {
        the_white_room_gltf_range_size,
        the_white_room_bin_range_size,
        the_white_room_1_dds_range_size,
        the_white_room_1_s_dds_range_size,
        the_white_room_2_dds_range_size,
        the_white_room_2_n_dds_range_size,
        the_white_room_3_dds_range_size,
        the_white_room_4_dds_range_size,
        keqing_lolita_love_you_gltf_range_size,
        keqing_lolita_love_you_bin_range_size,
        keqing_lolita_dds_range_size};

    return import_asset_init_memory_input_stream_factory(input_stream_count, input_stream_file_names, input_stream_memory_range_bases, input_stream_memory_range_sizes);
}