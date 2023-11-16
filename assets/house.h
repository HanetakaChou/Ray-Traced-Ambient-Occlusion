#ifndef _HOUSE_H_
#define _HOUSE_H_ 1

#include <stdint.h>

static constexpr uint32_t const g_house_vertex_count = 50145U;

extern float const g_house_vertex_position[3U * g_house_vertex_count];

extern float const g_house_vertex_normal[3U * g_house_vertex_count];

static constexpr uint32_t const g_house_index_count = 50145U;

extern uint16_t const g_house_index[g_house_index_count];

#endif
