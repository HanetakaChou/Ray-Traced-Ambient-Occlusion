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
#include "octahedron_mapping.sli"

pal_root_signature(swap_chain_root_signature_macro, swap_chain_root_signature_name)
pal_early_depth_stencil
pal_pixel_shader_parameter_begin(main)
pal_pixel_shader_parameter_in_position pal_pixel_shader_parameter_split
pal_pixel_shader_parameter_in(pal_float2, in_interpolated_uv, 0) pal_pixel_shader_parameter_split
pal_pixel_shader_parameter_out(pal_float4, out_swap_chain_image, 0)
pal_pixel_shader_parameter_end(main)
{
    out_swap_chain_image = pal_sample_2d(g_texture[0], g_sampler[0], in_interpolated_uv);
}