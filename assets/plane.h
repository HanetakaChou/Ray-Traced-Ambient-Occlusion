#ifndef _PLANE_H_
#define _PLANE_H_ 1

#include <stdint.h>

static constexpr uint32_t const g_plane_vertex_count = 6U;

extern float const g_plane_vertex_position[3U * g_plane_vertex_count];

extern float const g_plane_vertex_normal[3U * g_plane_vertex_count];

static constexpr uint32_t const g_plane_index_count = 6U;

extern uint16_t const g_plane_index[g_plane_index_count];

#endif