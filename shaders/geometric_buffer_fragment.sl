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
pal_early_depth_stencil
pal_pixel_shader_parameter_begin(main)
pal_pixel_shader_parameter_in_position pal_pixel_shader_parameter_split
pal_pixel_shader_parameter_in(pal_float3, in_interpolated_normal_world_space, 0) pal_pixel_shader_parameter_split
pal_pixel_shader_parameter_out(pal_float4, out_geometric_buffer_a, 0)
pal_pixel_shader_parameter_end(main)
{
    // World Space
    pal_float3 normal_world_space = pal_normalize(in_interpolated_normal_world_space);

    out_geometric_buffer_a = pal_float4((octahedron_map(normal_world_space) + pal_float2(1.0, 1.0)) * 0.5, 0.0, 0.0);
}