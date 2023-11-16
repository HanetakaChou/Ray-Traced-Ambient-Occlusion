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

#include "skin_pipeline_resource_binding.sli"
#include "../thirdparty/Packed-Vector/shaders/packed_vector.sli"
#include "../thirdparty/Packed-Vector/shaders/octahedron_mapping.sli"
#include "common_asset_constant.sli"

#if defined(GL_SPIRV) || defined(VULKAN)
#define brx_dual_quaternion mat2x4
#include "../thirdparty/DLB/DLB.glsli"
#elif defined(HLSL_VERSION) || defined(__HLSL_VERSION)
#define brx_dual_quaternion float2x4
#include "../thirdparty/DLB/DLB.hlsli"
#else
#error Unknown Compiler
#endif

#define THREAD_GROUP_X 1
#define THREAD_GROUP_Y 1
#define THREAD_GROUP_Z 1

brx_root_signature(skin_root_signature_macro, skin_root_signature_name)
brx_num_threads(THREAD_GROUP_X, THREAD_GROUP_Y, THREAD_GROUP_Z)
brx_compute_shader_parameter_begin(main)
brx_compute_shader_parameter_in_group_id
brx_pixel_shader_parameter_end(main)
{
    brx_uint max_vertex_count = brx_byte_address_buffer_get_dimension(g_mesh_subset_buffers[0]) / g_vertex_position_buffer_stride;

    brx_uint vertex_index = brx_group_id.x + MAX_SKIN_COMPUTE_DISPATCH_THREAD_GROUPS_PER_DIMENSION * brx_group_id.y;

    brx_branch
    if(vertex_index >= max_vertex_count)
    {
        return;
    }

    brx_uint vertex_position_buffer_offset = g_vertex_position_buffer_stride * vertex_index;

    brx_uint vertex_varying_buffer_offset = g_vertex_varying_buffer_stride * vertex_index;

    brx_float3 vertex_position_model_space;
    {
        brx_uint3 packed_vector_vertex_position_binding = brx_byte_address_buffer_load3(g_mesh_subset_buffers[0], vertex_position_buffer_offset);
        vertex_position_model_space = brx_uint_as_float(packed_vector_vertex_position_binding);
    }

    brx_float3 vertex_normal_model_space;
    brx_float4 vertex_tangent_model_space;
    brx_uint packed_texcoord;
    {
        brx_uint3 packed_vector_vertex_varying_binding = brx_byte_address_buffer_load3(g_mesh_subset_buffers[1], vertex_varying_buffer_offset);
        vertex_normal_model_space = octahedron_unmap(R16G16_SNORM_to_FLOAT2(packed_vector_vertex_varying_binding.x));
        brx_float3 vertex_mapped_tangent_model_space = R15G15B2_SNORM_to_FLOAT3(packed_vector_vertex_varying_binding.y);
        vertex_tangent_model_space = brx_float4(octahedron_unmap(vertex_mapped_tangent_model_space.xy), vertex_mapped_tangent_model_space.z);
        packed_texcoord = packed_vector_vertex_varying_binding.z;
    }

    brx_uint4 joint_indices;
    brx_float4 joint_weights;
    {
        brx_uint vertex_joint_buffer_offset = g_vertex_joint_buffer_stride * vertex_index;
        brx_uint3 packed_vector_vertex_joint_buffer = brx_byte_address_buffer_load3(g_mesh_subset_buffers[2], vertex_joint_buffer_offset);
        joint_indices = R16G16B16A16_UINT_to_UINT4(packed_vector_vertex_joint_buffer.xy);
        joint_weights = R8G8B8A8_UNORM_to_FLOAT4(packed_vector_vertex_joint_buffer.z);
    }

    brx_dual_quaternion blend_dual_quaternion;
    {
        brx_dual_quaternion dual_quaternion_indices_x = brx_dual_quaternion(g_dual_quaternions[2u * joint_indices.x], g_dual_quaternions[2u * joint_indices.x + 1u]);  
        brx_dual_quaternion dual_quaternion_indices_y = brx_dual_quaternion(g_dual_quaternions[2u * joint_indices.y], g_dual_quaternions[2u * joint_indices.y + 1u]); 
        brx_dual_quaternion dual_quaternion_indices_z = brx_dual_quaternion(g_dual_quaternions[2u * joint_indices.z], g_dual_quaternions[2u * joint_indices.z + 1u]);
        brx_dual_quaternion dual_quaternion_indices_w = brx_dual_quaternion(g_dual_quaternions[2u * joint_indices.w], g_dual_quaternions[2u * joint_indices.w + 1u]);
        blend_dual_quaternion = dual_quaternion_linear_blending(dual_quaternion_indices_x, dual_quaternion_indices_y, dual_quaternion_indices_z, dual_quaternion_indices_w, joint_weights);
    }

    brx_float4 blend_quaternion;
    brx_float3 blend_translation;
    unit_dual_quaternion_to_rigid_transform(blend_dual_quaternion, blend_quaternion, blend_translation);

    brx_float3 skined_vertex_position_model_space = unit_quaternion_to_rotation_transform(blend_quaternion, vertex_position_model_space) + blend_translation;

    brx_float3 skined_vertex_normal_model_space = unit_quaternion_to_rotation_transform(blend_quaternion, vertex_normal_model_space);     
    brx_float4 skined_vertex_tangent_model_space = brx_float4(unit_quaternion_to_rotation_transform(blend_quaternion, vertex_tangent_model_space.xyz), vertex_tangent_model_space.w);

    brx_uint3 packed_vector_skined_vertex_position_binding = brx_float_as_uint(skined_vertex_position_model_space);
    brx_uint3 packed_vector_skined_vertex_varying_binding = brx_uint3(FLOAT2_to_R16G16_SNORM(octahedron_map(skined_vertex_normal_model_space)), FLOAT3_to_R15G15B2_SNORM(brx_float3(octahedron_map(skined_vertex_tangent_model_space.xyz), skined_vertex_tangent_model_space.w)), packed_texcoord);
    
    brx_byte_address_buffer_store3(g_mesh_skinned_subset_buffers[0], vertex_position_buffer_offset, packed_vector_skined_vertex_position_binding);
    brx_byte_address_buffer_store3(g_mesh_skinned_subset_buffers[1], vertex_varying_buffer_offset, packed_vector_skined_vertex_varying_binding);
}
