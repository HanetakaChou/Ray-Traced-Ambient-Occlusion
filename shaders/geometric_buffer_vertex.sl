//
// Copyright (C) YuqiaoZhang(HanetakaChou)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#include "geometric_buffer_pipeline_layout.sli"
#include "octahedron_mapping.sli"

pal_root_signature(geometric_buffer_root_signature_macro, geometric_buffer_root_signature_name)
pal_vertex_shader_parameter_begin(main)
pal_vertex_shader_parameter_in(pal_float3, in_position, 0) pal_vertex_shader_parameter_split
pal_vertex_shader_parameter_in(pal_float2, in_normal_positive_rectangle_space, 1) pal_vertex_shader_parameter_split
pal_vertex_shader_parameter_out_position pal_vertex_shader_parameter_split
pal_vertex_shader_parameter_out(pal_float3, out_normal_world_space, 0)
pal_vertex_shader_parameter_end(main)
{
    // Model Space
    pal_float3 position_model_space = in_position;
    pal_float3 normal_model_space = octahedron_unmap(in_normal_positive_rectangle_space * 2.0 - pal_float2(1.0, 1.0));

    // World Space
    pal_float3 position_world_space = pal_mul(g_model_coordinate_transform, pal_float4(position_model_space, 1.0)).xyz;
    pal_float3 position_normal_space = pal_mul(g_model_normal_transform, normal_model_space).xyz;

    // View Space
    pal_float3 position_view_space = pal_mul(g_view_transform, pal_float4(position_world_space, 1.0)).xyz;

    // Clip Space
    pal_float4 position_clip_space = pal_mul(g_projection_transform, pal_float4(position_view_space, 1.0));

    pal_position = position_clip_space;
    out_normal_world_space = position_normal_space;
}

