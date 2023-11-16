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

#define BRX_ENABLE_RAY_TRACING 1
#include "gbuffer_pipeline_resource_binding.sli"
#include "../thirdparty/Packed-Vector/shaders/packed_vector.sli"
#include "../thirdparty/Packed-Vector/shaders/octahedron_mapping.sli"
#include "common_asset_constant.sli"

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
    uint packed_hit_shading_normal_world_space;
    {
        // [TraceRayInline example 1](https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html#tracerayinline-example-1)
        brx_ray_query ray_query;

        brx_ray_query_trace_ray_inline(ray_query, g_top_level_acceleration_structure, BRX_RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, camera_ray_origin, camera_ray_t_min, camera_ray_direction, camera_ray_t_max);

#if 1
        brx_ray_query_proceed(ray_query);
#else
        brx_branch
        if (brx_ray_query_proceed(ray_query))
        {
            brx_branch
            if (BRX_CANDIDATE_NON_OPAQUE_TRIANGLE == brx_ray_query_candidate_type(ray_query))
            {
                brx_ray_query_committed_non_opaque_triangle_hit(ray_query);
            }
        }
#endif

        brx_branch
        if (BRX_COMMITTED_TRIANGLE_HIT == brx_ray_query_committed_status(ray_query))
        {
            brx_uint non_uniform_vertex_position_buffer_index;
            brx_uint non_uniform_vertex_varying_buffer_index;
            brx_uint non_uniform_index_buffer_index;
            brx_uint non_uniform_information_buffer_index;
            brx_uint non_uniform_normal_texture_index;
            brx_uint non_uniform_emissive_texture_index;
            brx_uint non_uniform_base_color_texture_index;
            brx_uint non_uniform_metallic_roughness_texture_index;
            {
                brx_uint committed_instance_id = brx_ray_query_committed_instance_id(ray_query);

                brx_uint instance_information_index = committed_instance_id;

                instance_information_T instance_information = g_instance_information[instance_information_index];

                brx_uint committed_geometry_index = brx_ray_query_committed_geometry_index(ray_query);

                brx_uint global_geometry_index = instance_information.m_global_geometry_index_offset + committed_geometry_index;

                non_uniform_vertex_position_buffer_index = PER_MESH_SUBSET_BUFFER_COUNT * global_geometry_index;

                non_uniform_vertex_varying_buffer_index = PER_MESH_SUBSET_BUFFER_COUNT * global_geometry_index + 1u;

                non_uniform_index_buffer_index = PER_MESH_SUBSET_BUFFER_COUNT * global_geometry_index + 2u;

                non_uniform_information_buffer_index = PER_MESH_SUBSET_BUFFER_COUNT * global_geometry_index + 3u;

                non_uniform_normal_texture_index = PER_MESH_SUBSET_TEXTURE_COUNT * global_geometry_index;

                non_uniform_emissive_texture_index = PER_MESH_SUBSET_TEXTURE_COUNT * global_geometry_index + 1u;

                non_uniform_base_color_texture_index = PER_MESH_SUBSET_TEXTURE_COUNT * global_geometry_index + 2u;

                non_uniform_metallic_roughness_texture_index = PER_MESH_SUBSET_TEXTURE_COUNT * global_geometry_index + 3u;
            }

            brx_uint mesh_subset_buffer_texture_flags;
            brx_float mesh_subset_normal_texture_scale;
            {
                brx_uint2 packed_vector_information = brx_byte_address_buffer_load2(g_mesh_subset_buffers[brx_non_uniform_resource_index(non_uniform_information_buffer_index)], 0);

                mesh_subset_buffer_texture_flags = packed_vector_information.x;

                mesh_subset_normal_texture_scale = brx_uint_as_float(packed_vector_information.y);
            }

            brx_uint3 vertex_indices;
            {
                brx_uint committed_primitive_index = brx_ray_query_committed_primitive_index(ray_query);
                brx_uint triangle_index = committed_primitive_index;

                brx_branch
                if (0u != (mesh_subset_buffer_texture_flags & Buffer_Flag_Index_Type_UInt16))
                {
                    brx_uint index_buffer_offset = (0u == (((g_index_uint16_buffer_stride * 3u) * triangle_index) & 3u)) ? ((g_index_uint16_buffer_stride * 3u) * triangle_index) : (((g_index_uint16_buffer_stride * 3u) * triangle_index) - 2u);

                    brx_uint2 packed_vector_index_buffer = brx_byte_address_buffer_load2(g_mesh_subset_buffers[brx_non_uniform_resource_index(non_uniform_index_buffer_index)], index_buffer_offset);

                    brx_uint4 unpacked_vector_index_buffer = R16G16B16A16_UINT_to_UINT4(packed_vector_index_buffer);

                    vertex_indices = (0u == (((g_index_uint16_buffer_stride * 3u) * triangle_index) & 3u)) ? unpacked_vector_index_buffer.xyz : unpacked_vector_index_buffer.yzw;
                }
                else
                {
                    brx_uint index_buffer_offset = (g_index_uint32_buffer_stride * 3u) * triangle_index;

                    vertex_indices = brx_byte_address_buffer_load3(g_mesh_subset_buffers[brx_non_uniform_resource_index(non_uniform_index_buffer_index)], index_buffer_offset);
                }
            }

            brx_float3 vertex_positions_model_space[3];
            {
                brx_uint3 vertex_position_buffer_offset = g_vertex_position_buffer_stride * vertex_indices;

                brx_uint3 packed_vectors_vertex_position_binding[3];
                packed_vectors_vertex_position_binding[0] = brx_byte_address_buffer_load3(g_mesh_subset_buffers[brx_non_uniform_resource_index(non_uniform_vertex_position_buffer_index)], vertex_position_buffer_offset.x);
                packed_vectors_vertex_position_binding[1] = brx_byte_address_buffer_load3(g_mesh_subset_buffers[brx_non_uniform_resource_index(non_uniform_vertex_position_buffer_index)], vertex_position_buffer_offset.y);
                packed_vectors_vertex_position_binding[2] = brx_byte_address_buffer_load3(g_mesh_subset_buffers[brx_non_uniform_resource_index(non_uniform_vertex_position_buffer_index)], vertex_position_buffer_offset.z);

                vertex_positions_model_space[0] = brx_uint_as_float(packed_vectors_vertex_position_binding[0]);
                vertex_positions_model_space[1] = brx_uint_as_float(packed_vectors_vertex_position_binding[1]);
                vertex_positions_model_space[2] = brx_uint_as_float(packed_vectors_vertex_position_binding[2]);
            }

            brx_float3 vertex_geometry_normals_model_space[3];
            brx_float4 vertex_tangents_model_space[3];
            brx_float2 vertex_texcoords[3];
            {
                brx_uint3 vertex_varying_buffer_offset = g_vertex_varying_buffer_stride * vertex_indices;

                brx_uint3 packed_vectors_vertex_varying_binding[3];
                packed_vectors_vertex_varying_binding[0] = brx_byte_address_buffer_load3(g_mesh_subset_buffers[brx_non_uniform_resource_index(non_uniform_vertex_varying_buffer_index)], vertex_varying_buffer_offset.x);
                packed_vectors_vertex_varying_binding[1] = brx_byte_address_buffer_load3(g_mesh_subset_buffers[brx_non_uniform_resource_index(non_uniform_vertex_varying_buffer_index)], vertex_varying_buffer_offset.y);
                packed_vectors_vertex_varying_binding[2] = brx_byte_address_buffer_load3(g_mesh_subset_buffers[brx_non_uniform_resource_index(non_uniform_vertex_varying_buffer_index)], vertex_varying_buffer_offset.z);

                vertex_geometry_normals_model_space[0] = octahedron_unmap(R16G16_SNORM_to_FLOAT2(packed_vectors_vertex_varying_binding[0].x));
                vertex_geometry_normals_model_space[1] = octahedron_unmap(R16G16_SNORM_to_FLOAT2(packed_vectors_vertex_varying_binding[1].x));
                vertex_geometry_normals_model_space[2] = octahedron_unmap(R16G16_SNORM_to_FLOAT2(packed_vectors_vertex_varying_binding[2].x));

                brx_float3 vertex_mapped_tangents_model_space[3];
                vertex_mapped_tangents_model_space[0] = R15G15B2_SNORM_to_FLOAT3(packed_vectors_vertex_varying_binding[0].y);
                vertex_mapped_tangents_model_space[1] = R15G15B2_SNORM_to_FLOAT3(packed_vectors_vertex_varying_binding[1].y);
                vertex_mapped_tangents_model_space[2] = R15G15B2_SNORM_to_FLOAT3(packed_vectors_vertex_varying_binding[2].y);

                vertex_tangents_model_space[0] = brx_float4(octahedron_unmap(vertex_mapped_tangents_model_space[0].xy), vertex_mapped_tangents_model_space[0].z);
                vertex_tangents_model_space[1] = brx_float4(octahedron_unmap(vertex_mapped_tangents_model_space[1].xy), vertex_mapped_tangents_model_space[1].z);
                vertex_tangents_model_space[2] = brx_float4(octahedron_unmap(vertex_mapped_tangents_model_space[2].xy), vertex_mapped_tangents_model_space[2].z);

                vertex_texcoords[0] = R16G16_UNORM_to_FLOAT2(packed_vectors_vertex_varying_binding[0].z);
                vertex_texcoords[1] = R16G16_UNORM_to_FLOAT2(packed_vectors_vertex_varying_binding[1].z);
                vertex_texcoords[2] = R16G16_UNORM_to_FLOAT2(packed_vectors_vertex_varying_binding[2].z);
            }

            brx_float3 hit_position_model_space;
            brx_float3 hit_geometry_normal_model_space;
            brx_float4 hit_tangent_model_space;
            brx_float2 hit_texcoord;
            {
                brx_float2 committed_triangle_barycentrics = brx_ray_query_committed_triangle_barycentrics(ray_query);
                brx_float3 triangle_barycentrics = brx_float3(1.0 - committed_triangle_barycentrics.x - committed_triangle_barycentrics.y, committed_triangle_barycentrics.x, committed_triangle_barycentrics.y);

                hit_position_model_space = vertex_positions_model_space[0] * triangle_barycentrics.x + vertex_positions_model_space[1] * triangle_barycentrics.y + vertex_positions_model_space[2] * triangle_barycentrics.z;
                hit_geometry_normal_model_space = brx_normalize(vertex_geometry_normals_model_space[0] * triangle_barycentrics.x + vertex_geometry_normals_model_space[1] * triangle_barycentrics.y + vertex_geometry_normals_model_space[2] * triangle_barycentrics.z);
                // TODO: simply use w of vertex_tangents_model_space[0]?
                hit_tangent_model_space = brx_float4(brx_normalize(vertex_tangents_model_space[0].xyz * triangle_barycentrics.x + vertex_tangents_model_space[1].xyz * triangle_barycentrics.y + vertex_tangents_model_space[2].xyz * triangle_barycentrics.z), vertex_tangents_model_space[0].w * triangle_barycentrics.x + vertex_tangents_model_space[1].w * triangle_barycentrics.y + vertex_tangents_model_space[2].w * triangle_barycentrics.z);
                hit_texcoord = vertex_texcoords[0] * triangle_barycentrics.x + vertex_texcoords[1] * triangle_barycentrics.y + vertex_texcoords[2] * triangle_barycentrics.z;
            }

            brx_float3 hit_position_world_space;
            brx_float3 hit_geometry_normal_world_space;
            brx_float4 hit_tangent_world_space;
            {
                brx_float3x4 instance_model_transform = brx_ray_query_committed_object_to_world(ray_query);

                hit_position_world_space = brx_mul(instance_model_transform, brx_float4(hit_position_model_space, 1.0));
                hit_geometry_normal_world_space = brx_mul(instance_model_transform, brx_float4(hit_geometry_normal_model_space, 0.0));
                hit_tangent_world_space = brx_float4(brx_mul(instance_model_transform, brx_float4(hit_tangent_model_space.xyz, 0.0)), hit_tangent_model_space.w);
            }

            brx_float3 hit_position_view_space = brx_mul(g_view_transform, brx_float4(hit_position_world_space, 1.0)).xyz;
            brx_float4 hit_position_clip_space = brx_mul(g_projection_transform, brx_float4(hit_position_view_space, 1.0));
            brx_float hit_position_depth = hit_position_clip_space.z / hit_position_clip_space.w;

            // TODO: "A.3 Local Shading Normal Adaption" of "The Iray Light Transport Simulation and Rendering System"
            // TODO: shall we really use shading normal for Ambient Occlusion?
            brx_float3 hit_shading_normal_world_space;
            brx_branch
            if (0u != (mesh_subset_buffer_texture_flags & Texture_Flag_Enable_Normal_Texture))
            {
                // TODO: Ray Differentials
                brx_float lod = 0.0;

                brx_float3 hit_shading_normal_tangent_space = brx_normalize((brx_sample_level_2d(g_mesh_subset_textures[brx_non_uniform_resource_index(non_uniform_normal_texture_index)], g_sampler, hit_texcoord, lod).xyz * 2.0 - brx_float3(1.0, 1.0, 1.0)) * brx_float3(mesh_subset_normal_texture_scale, mesh_subset_normal_texture_scale, 1.0));
                brx_float3 hit_bitangent_world_space = brx_cross(hit_geometry_normal_model_space, hit_tangent_world_space.xyz) * hit_tangent_world_space.w;
                hit_shading_normal_world_space = brx_normalize(hit_tangent_world_space.xyz * hit_shading_normal_tangent_space.x + hit_bitangent_world_space * hit_shading_normal_tangent_space.y + hit_geometry_normal_world_space * hit_shading_normal_tangent_space.z);
            }
            else
            {
                hit_shading_normal_world_space = hit_geometry_normal_world_space;
            }

            packed_hit_position_depth = brx_float_as_uint(hit_position_depth);
            packed_hit_shading_normal_world_space = FLOAT2_to_R16G16_SNORM(octahedron_map(hit_shading_normal_world_space));
        }
        else
        {
            packed_hit_position_depth = INVALID_GBUFFER_DEPTH;
            packed_hit_shading_normal_world_space = 0;
        }
    }

    brx_store_2d(g_gbuffer_textures[0], brx_int2(brx_group_id.xy), brx_uint4(packed_hit_position_depth, 0, 0, 0));
    brx_store_2d(g_gbuffer_textures[1], brx_int2(brx_group_id.xy), brx_uint4(packed_hit_shading_normal_world_space, 0, 0, 0));
}
