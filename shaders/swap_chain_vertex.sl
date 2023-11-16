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

#include "swap_chain_pipeline_layout.sli"

pal_root_signature(swap_chain_root_signature_macro, swap_chain_root_signature_name)
pal_vertex_shader_parameter_begin(main)
pal_vertex_shader_parameter_in_vertex_id pal_vertex_shader_parameter_split
pal_vertex_shader_parameter_out_position pal_vertex_shader_parameter_split
pal_vertex_shader_parameter_out(pal_float2, out_uv, 0)
pal_vertex_shader_parameter_end(main)
{
    const pal_float2 full_screen_triangle_positions[3] = pal_array_constructor_begin(pal_float2, 3) 
		pal_float2(-1.0, -1.0) pal_array_constructor_split
		pal_float2(3.0, -1.0) pal_array_constructor_split
		pal_float2(-1.0, 3.0)
        pal_array_constructor_end;

	const pal_float2 full_screen_triangle_uvs[3] = pal_array_constructor_begin(pal_float2, 3) 
		pal_float2(0.0, 1.0) pal_array_constructor_split
		pal_float2(2.0, 1.0) pal_array_constructor_split
		pal_float2(0.0, -1.0)
        pal_array_constructor_end;

	pal_position = pal_float4(full_screen_triangle_positions[pal_vertex_id], 0.5, 1.0);
	out_uv = full_screen_triangle_uvs[pal_vertex_id];
}