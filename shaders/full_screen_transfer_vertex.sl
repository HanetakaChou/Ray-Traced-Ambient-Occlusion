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

#include "full_screen_transfer_pipeline_layout.sli"

brx_root_signature(full_screen_transfer_root_signature_macro, full_screen_transfer_root_signature_name)
brx_vertex_shader_parameter_begin(main)
brx_vertex_shader_parameter_in_vertex_index brx_vertex_shader_parameter_split
brx_vertex_shader_parameter_out_position
brx_vertex_shader_parameter_end(main)
{
    const brx_float2 full_screen_triangle_positions[3] = brx_array_constructor_begin(brx_float2, 3) 
		brx_float2(-1.0, -1.0) brx_array_constructor_split
		brx_float2(3.0, -1.0) brx_array_constructor_split
		brx_float2(-1.0, 3.0)
        brx_array_constructor_end;

	brx_position = brx_float4(full_screen_triangle_positions[brx_vertex_index], 0.5, 1.0);
}