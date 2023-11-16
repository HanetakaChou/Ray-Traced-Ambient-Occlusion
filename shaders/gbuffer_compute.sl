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

#include "gbuffer_pipeline_resource_binding.sli"
#include "packed_vector.sli"
#include "octahedron_mapping.sli"

#define THREAD_GROUP_X 1
#define THREAD_GROUP_Y 1
#define THREAD_GROUP_Z 1

brx_root_signature(gbuffer_root_signature_macro, gbuffer_root_signature_name)
brx_num_threads(THREAD_GROUP_X, THREAD_GROUP_Y, THREAD_GROUP_Z)
brx_compute_shader_parameter_begin(main)
brx_compute_shader_parameter_in_group_id
brx_pixel_shader_parameter_end(main)
{
    brx_float3 camera_ray_origin;
    {
        camera_ray_origin = brx_mul(g_inverse_view_transform, brx_float4(0.0, 0.0, 0.0, 1.0)).xyz;
    }
    
    brx_float camera_ray_t_min;
    {
        brx_float4 position_z_min_view_space_with_w = brx_mul(g_inverse_projection_transform, brx_float4(0.0, 0.0, 1.0, 1.0));

        camera_ray_t_min = -(position_z_min_view_space_with_w.z / position_z_min_view_space_with_w.w);
    }

    brx_float camera_ray_t_max;
    {
        brx_float4 position_z_max_view_space_with_w = brx_mul(g_inverse_projection_transform, brx_float4(0.0, 0.0, 0.0, 1.0)); 

        camera_ray_t_max = -(position_z_max_view_space_with_w.z / position_z_max_view_space_with_w.w);
    }

    brx_float3 camera_ray_direction;
    {
        brx_float2 uv = (brx_float2(brx_group_id.xy) + brx_float2(0.5, 0.5)) / brx_float2(g_screen_width, g_screen_height);

        brx_float position_target_depth = 1.0 / 256.0;

        brx_float3 position_target_ndc_space = brx_float3(uv * brx_float2(2.0, -2.0) + brx_float2(-1.0, 1.0), position_target_depth);

        brx_float4 position_target_view_space_with_w = brx_mul(g_inverse_projection_transform, brx_float4(position_target_ndc_space, 1.0));

        brx_float3 position_target_view_space = position_target_view_space_with_w.xyz / position_target_view_space_with_w.w;

        brx_float3 position_target_world_space = brx_mul(g_inverse_view_transform, brx_float4(position_target_view_space, 1.0)).xyz;

        camera_ray_direction = brx_normalize(position_target_world_space - camera_ray_origin);
    }


    uint packed_hit_position_depth;
    uint packed_hit_normal_world_space;
    {
        // [TraceRayInline example 1](https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html#tracerayinline-example-1)        
        brx_ray_query ray_query;
        
        brx_ray_query_trace_ray_inline(ray_query, g_top_level_acceleration_structure[0], BRX_RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, camera_ray_origin, camera_ray_t_min, camera_ray_direction, camera_ray_t_max);

#if 1
        brx_ray_query_proceed(ray_query);
#else
        brx_branch
        if (brx_ray_query_proceed(ray_query))
        {
            brx_branch
            if(BRX_CANDIDATE_NON_OPAQUE_TRIANGLE == brx_ray_query_candidate_type(ray_query))
            {
                brx_ray_query_committed_non_opaque_triangle_hit(ray_query);
            }
        }
#endif

        brx_branch
        if (BRX_COMMITTED_TRIANGLE_HIT == brx_ray_query_committed_status(ray_query))
        {
            brx_uint committed_instance_id = brx_ray_query_committed_instance_id(ray_query);

            brx_uint instance_data_index = committed_instance_id;

            instance_data_T instance_data = g_instance_data[instance_data_index];
            
            brx_uint committed_geometry_index = brx_ray_query_committed_geometry_index(ray_query);

            brx_uint geometry_data_index = instance_data.m_geometry_data_index_offset + committed_geometry_index;

            geometry_data_T geometry_data = g_geometry_data[geometry_data_index];

            brx_uint non_uniform_vertex_position_buffer_index = 3 * geometry_data.m_non_uniform_geometry_buffer_index;
            
            brx_uint non_uniform_vertex_varying_buffer_index = 3 * geometry_data.m_non_uniform_geometry_buffer_index + 1;
            
            brx_uint non_uniform_index_buffer_index = 3 * geometry_data.m_non_uniform_geometry_buffer_index + 2;

            brx_uint committed_primitive_index = brx_ray_query_committed_primitive_index(ray_query);

            brx_uint triangle_index = committed_primitive_index;
            
            brx_uint index_buffer_offset = (0 == ((g_index_buffer_stride * triangle_index) % 4)) ? (g_index_buffer_stride * triangle_index) : ((g_index_buffer_stride * triangle_index) - 2);

            brx_uint2 packed_vector_index_buffer = brx_byte_address_buffer_load2(g_geometry_buffers[brx_non_uniform_resource_index(non_uniform_index_buffer_index)], index_buffer_offset);

            brx_uint4 unpacked_vector_index_buffer = R16G16B16A16_UINT_to_UINT4(packed_vector_index_buffer);

            brx_uint3 vertex_indices = (0 == ((g_index_buffer_stride * triangle_index) % 4)) ? unpacked_vector_index_buffer.xyz : unpacked_vector_index_buffer.yzw;

            brx_uint3 vertex_position_buffer_offset = g_vertex_position_buffer_stride * vertex_indices;

            brx_float3 vertex_positions_model_space[3];
            vertex_positions_model_space[0] = brx_uint_as_float(brx_byte_address_buffer_load3(g_geometry_buffers[brx_non_uniform_resource_index(non_uniform_vertex_position_buffer_index)], vertex_position_buffer_offset.x));
            vertex_positions_model_space[1] = brx_uint_as_float(brx_byte_address_buffer_load3(g_geometry_buffers[brx_non_uniform_resource_index(non_uniform_vertex_position_buffer_index)], vertex_position_buffer_offset.y));
            vertex_positions_model_space[2] = brx_uint_as_float(brx_byte_address_buffer_load3(g_geometry_buffers[brx_non_uniform_resource_index(non_uniform_vertex_position_buffer_index)], vertex_position_buffer_offset.z));

            brx_uint3 vertex_varying_buffer_offset = g_vertex_varying_buffer_stride * vertex_indices;

            brx_float3 vertex_normals_model_space[3];
            vertex_normals_model_space[0] = octahedron_unmap(R16G16_SNORM_to_FLOAT2(brx_byte_address_buffer_load(g_geometry_buffers[brx_non_uniform_resource_index(non_uniform_vertex_varying_buffer_index)], vertex_varying_buffer_offset.x)));
            vertex_normals_model_space[1] = octahedron_unmap(R16G16_SNORM_to_FLOAT2(brx_byte_address_buffer_load(g_geometry_buffers[brx_non_uniform_resource_index(non_uniform_vertex_varying_buffer_index)], vertex_varying_buffer_offset.y)));
            vertex_normals_model_space[2] = octahedron_unmap(R16G16_SNORM_to_FLOAT2(brx_byte_address_buffer_load(g_geometry_buffers[brx_non_uniform_resource_index(non_uniform_vertex_varying_buffer_index)], vertex_varying_buffer_offset.z)));

            brx_float2 committed_triangle_barycentrics = brx_ray_query_committed_triangle_barycentrics(ray_query);

            brx_float3 triangle_barycentrics = brx_float3(1.0 - committed_triangle_barycentrics.x - committed_triangle_barycentrics.y, committed_triangle_barycentrics.x, committed_triangle_barycentrics.y);

            brx_float3 hit_position_model_space = vertex_positions_model_space[0] * triangle_barycentrics.x + vertex_positions_model_space[1] * triangle_barycentrics.y + vertex_positions_model_space[2] * triangle_barycentrics.z;
            
            brx_float3 hit_normal_model_space = normalize(vertex_normals_model_space[0] * triangle_barycentrics.x + vertex_normals_model_space[1] * triangle_barycentrics.y + vertex_normals_model_space[2] * triangle_barycentrics.z);

            brx_float3 hit_position_world_space = brx_mul(instance_data.m_model_transform, brx_float4(hit_position_model_space, 1.0)).xyz;
            
            brx_float3 hit_position_view_space = brx_mul(g_view_transform, brx_float4(hit_position_world_space, 1.0)).xyz;

            brx_float4 hit_position_clip_space = brx_mul(g_projection_transform, brx_float4(hit_position_view_space, 1.0));

            brx_float hit_position_depth = hit_position_clip_space.z / hit_position_clip_space.w;
            
            packed_hit_position_depth = brx_float_as_uint(hit_position_depth);

            // TODO: "A.3 Local Shading Normal Adaption" of "The Iray Light Transport Simulation and Rendering System"
            brx_float3 hit_normal_world_space = brx_mul(instance_data.m_model_transform, brx_float4(hit_normal_model_space, 0.0)).xyz;

            packed_hit_normal_world_space = R16G16_FLOAT2_to_SNORM(octahedron_map(hit_normal_world_space));
        }
        else
        {
            packed_hit_position_depth = INVALID_GBUFFER_DEPTH;
            packed_hit_normal_world_space = 0;
        }
    }

    brx_store_2d(g_gbuffer_textures[0], brx_int2(brx_group_id.xy), brx_uint4(packed_hit_position_depth, packed_hit_position_depth, packed_hit_position_depth, packed_hit_position_depth));
    brx_store_2d(g_gbuffer_textures[1], brx_int2(brx_group_id.xy), brx_uint4(packed_hit_normal_world_space, packed_hit_normal_world_space, packed_hit_normal_world_space, packed_hit_normal_world_space));
}