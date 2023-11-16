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

brx_root_signature(swap_chain_root_signature_macro, swap_chain_root_signature_name)
brx_vertex_shader_parameter_begin(main)
brx_vertex_shader_parameter_in_vertex_id brx_vertex_shader_parameter_split
brx_vertex_shader_parameter_out_position brx_vertex_shader_parameter_split
brx_vertex_shader_parameter_out(brx_float2, out_uv, 0)
brx_vertex_shader_parameter_end(main)
{
    const brx_float2 full_screen_triangle_positions[3] = brx_array_constructor_begin(brx_float2, 3) 
		brx_float2(-1.0, -1.0) brx_array_constructor_split
		brx_float2(3.0, -1.0) brx_array_constructor_split
		brx_float2(-1.0, 3.0)
        brx_array_constructor_end;

	const brx_float2 full_screen_triangle_uvs[3] = brx_array_constructor_begin(brx_float2, 3) 
		brx_float2(0.0, 1.0) brx_array_constructor_split
		brx_float2(2.0, 1.0) brx_array_constructor_split
		brx_float2(0.0, -1.0)
        brx_array_constructor_end;

	brx_position = brx_float4(full_screen_triangle_positions[brx_vertex_id], 0.5, 1.0);
	out_uv = full_screen_triangle_uvs[brx_vertex_id];
}