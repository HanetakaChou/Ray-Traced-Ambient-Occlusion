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
#include "ambient_occlusion_pipeline_resource_binding.sli"
#include "../thirdparty/Packed-Vector/shaders/packed_vector.sli"
#include "../thirdparty/Packed-Vector/shaders/octahedron_mapping.sli"
#include "low_discrepancy_sequence.sli"
#include "pdf_sampling.sli"
#include "offset_ray_origin.sli"

// https://docs.microsoft.com/en-us/windows/win32/direct3d11/direct3d-11-advanced-stages-compute-shader
// D3D11_CS_THREAD_GROUP_MAX_X 1024
// D3D11_CS_THREAD_GROUP_MAX_Y 1024
// D3D11_CS_THREAD_GROUP_MAX_Z 64
// D3D11_CS_THREAD_GROUP_MAX_THREADS_PER_GROUP 1024
#define THREAD_GROUP_X 128
#define THREAD_GROUP_Y 1
#define THREAD_GROUP_Z 1

#define MAX_SAMPLE_COUNT (THREAD_GROUP_X * THREAD_GROUP_Y * THREAD_GROUP_Z)

#define USE_WAVE_INTRINSICS 0

// NOTE: the actual "wave lane count" must be NO less than 32, otherwise the "GROUP_SHARED_MEMORY_COUNT" will be NOT correct.
#define MIN_WAVE_LANE_COUNT 32

// https://docs.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-variable-syntax
// "in D3D11 the maximum size is 32kb"
#if USE_WAVE_INTRINSICS
// 16 = 4 * (128 / 32)
#define GROUP_SHARED_MEMORY_COUNT (MAX_SAMPLE_COUNT / MIN_WAVE_LANE_COUNT)
#else
// 256 = 4 * (128 / 2)
#define GROUP_SHARED_MEMORY_COUNT (MAX_SAMPLE_COUNT / 2)
#endif
brx_group_shared brx_float reduction_group_shared_memory[GROUP_SHARED_MEMORY_COUNT];

brx_root_signature(ambient_occlusion_root_signature_macro, ambient_occlusion_root_signature_name)
brx_num_threads(THREAD_GROUP_X, THREAD_GROUP_Y, THREAD_GROUP_Z)
brx_compute_shader_parameter_begin(main)
brx_compute_shader_parameter_in_group_id brx_compute_shader_parameter_split
brx_compute_shader_parameter_in_group_thread_id brx_compute_shader_parameter_split
brx_compute_shader_parameter_in_group_index
brx_pixel_shader_parameter_end(main)
{
    // GBuffer: Position + Normal
    brx_uint gbuffer_depth = brx_load_2d(g_gbuffer_textures[0], brx_int3(brx_group_id.xy, 0)).x;

    brx_branch
    if (INVALID_GBUFFER_DEPTH == gbuffer_depth)
    {
        brx_store_2d(g_ambient_occlusion_texture, brx_int2(brx_group_id.xy), brx_float4(0.0, 0.0, 0.0, 1.0));
        return;
    }

    brx_uint gbuffer_normal = brx_load_2d(g_gbuffer_textures[1], brx_int3(brx_group_id.xy, 0)).x;

    brx_float3 position_world_space;
    {
        brx_float2 uv = (brx_float2(brx_group_id.xy) + brx_float2(0.5, 0.5)) / brx_float2(g_screen_width, g_screen_height);

        brx_float position_depth = brx_uint_as_float(gbuffer_depth);

        brx_float3 position_ndc_space = brx_float3(uv * brx_float2(2.0, -2.0) + brx_float2(-1.0, 1.0), position_depth);

        brx_float4 position_view_space_with_w = brx_mul(g_inverse_projection_transform, brx_float4(position_ndc_space, 1.0));

        brx_float3 position_view_space = position_view_space_with_w.xyz / position_view_space_with_w.w;

        position_world_space = brx_mul(g_inverse_view_transform, brx_float4(position_view_space, 1.0)).xyz;
    }

    brx_float3 normal_world_space = octahedron_unmap(R16G16_SNORM_to_FLOAT2(gbuffer_normal));

    brx_int reduction_index = brx_int(brx_group_index);

    brx_int num_samples = brx_min(brx_int(g_ambient_occlusion_sample_count), MAX_SAMPLE_COUNT);

    // Since the clamped cosine is isotropic, the outgoing direction V is **usually** assumed to be in the XOZ plane.
    // Actually the clamped cosine is **also** radially symmetric and the tangent direction is arbitrary.
    // UE: [GetTangentBasis](https://github.com/EpicGames/UnrealEngine/blob/4.27/Engine/Shaders/Private/MonteCarlo.ush#L12)
    // U3D: [GetLocalFrame](https://github.com/Unity-Technologies/Graphics/blob/v10.8.1/com.unity.render-pipelines.core/ShaderLibrary/CommonLighting.hlsl#L408)
    brx_float3x3 tangent_to_world_transform;
    {
        // NOTE: "local_z" should be normalized.
        brx_float3 local_z = normal_world_space;

        brx_float x = local_z.x;
        brx_float y = local_z.y;
        brx_float z = local_z.z;

        brx_float sz = z >= 0.0 ? 1.0 : -1.0;
        brx_float a = 1.0 / (sz + z);
        brx_float ya = y * a;
        brx_float b = x * ya;
        brx_float c = x * sz;

        brx_float3 local_x = brx_float3(c * x * a - 1, sz * b, c);
        brx_float3 local_y = brx_float3(b, y * ya - sz, y);

        tangent_to_world_transform = brx_float3x3_from_columns(local_x, local_y, local_z);
    }

    // PBRT-V3: [AOIntegrator::Li](https://github.com/mmp/pbrt-v3/blob/master/src/integrators/ao.cpp)
    // PBRT-V4: [AOIntegrator::Li](https://github.com/mmp/pbrt-v4/blob/ci/src/pbrt/cpu/integrators.cpp#L1414)

    brx_float reduction_thread_local;
    brx_branch
    if (reduction_index < num_samples)
    {
        brx_float2 xi = hammersley_2d(reduction_index, num_samples);
        brx_float x1 = xi.x;
        brx_float x2 = xi.y;

#if 1
        brx_float3 omega_i_tangent_sapce = normalized_clamped_cosine_sample_omega_i(brx_float2(x1, x2));
#else
        // PBR Book V3: ["13.6.1 Uniformly Sampling a Hemisphere"](https://www.pbr-book.org/3ed-2018/Monte_Carlo_Integration/2D_Sampling_with_Multidimensional_Transformations#UniformlySamplingaHemisphere)
        // PBR Book V4: ["A.5.2 Uniformly Sampling Hemispheres and Spheres"](https://www.pbr-book.org/4ed/Sampling_Algorithms/Sampling_Multidimensional_Functions#UniformlySamplingHemispheresandSpheres)
        brx_float sin_theta = sqrt(1.0 - x1 * x1);
        brx_float cos_theta = x1;
        brx_float phi = 2 * M_PI * x2;
        brx_float3 omega_i_tangent_sapce = brx_float3(brx_cos(phi) * sin_theta, brx_sin(phi) * sin_theta, cos_theta);
#endif

        brx_float3 omega_i_world_sapce = brx_mul(tangent_to_world_transform, omega_i_tangent_sapce);

        // PBR Book V3: ["13.2 The Monte Carlo Estimator"](https://www.pbr-book.org/3ed-2018/Monte_Carlo_Integration/The_Monte_Carlo_Estimator)
        // PBR Book V4: ["2.1.3 The Monte Carlo Estimator"](https://pbr-book.org/4ed/Monte_Carlo_Integration/Monte_Carlo_Basics#TheMonteCarloEstimator)
        // PDF =  clamped_cosine_theta / M_PI = LdotN / M_PI
        // (1.0 / NumSamples) * (Visibility * (LdotN / M_PI)) / PDF = (1.0 / NumSamples) * (Visibility * (LdotN / M_PI) / (LdotN / M_PI)) = (1.0 / NumSamples) * Visibility
        brx_float visibility;
        {
            brx_float3 offset_position_world_space = offset_ray_origin(position_world_space, normal_world_space, omega_i_world_sapce);

            brx_ray_query ray_query;

            brx_ray_query_trace_ray_inline(ray_query, g_top_level_acceleration_structure, BRX_RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, offset_position_world_space, 0.0, omega_i_world_sapce, g_ambient_occlusion_max_distance);

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
                visibility = 0.0;
            }
            else
            {
                visibility = 1.0;
            }
        }

        reduction_thread_local = (1.0 / num_samples) * visibility;
    }
    else
    {
        reduction_thread_local = 0.0;
    }

    // Parallel Reduction
    brx_float reduction_group_total;
    {
#if USE_WAVE_INTRINSICS
        // U3D: [PLATFORM_SUPPORTS_WAVE_INTRINSICS](https://github.com/Unity-Technologies/Graphics/blob/v10.8.1/com.unity.render-pipelines.high-definition/Runtime/Sky/AmbientProbeConvolution.compute#L77)

        brx_int lane_index = reduction_index % brx_int(brx_wave_lane_count);
        brx_int wave_index = reduction_index / brx_int(brx_wave_lane_count);
        brx_int wave_count = num_samples / brx_int(brx_wave_lane_count);

        brx_float reduction_current_wave_total = brx_wave_active_sum(reduction_thread_local);

        brx_branch
        if (0 == lane_index)
        {
            reduction_group_shared_memory[wave_index] = reduction_current_wave_total;
        }

        brx_group_memory_barrier_with_group_sync();

#if 1
        brx_branch
        if (0 == wave_index)
        {
            // NOTE: the "wave count" must be NO greater than "lane count"
            brx_float reduction_other_wave_total = (lane_index < wave_count) ? reduction_group_shared_memory[lane_index] : 0.0;

            brx_float reduction_group_wave_total = brx_wave_active_sum(reduction_other_wave_total);

            brx_branch
            if (0 == lane_index)
            {
                reduction_group_total = reduction_group_wave_total;
            }
        }
#else
        brx_branch
        if (0 == wave_index && 0 == lane_index)
        {
            reduction_group_total = reduction_current_wave_total;

            brx_unroll_x(MAX_SAMPLE_COUNT / MIN_WAVE_LANE_COUNT)
            for (brx_int i = 1; i < wave_count; ++i)
            {
                reduction_group_total += reduction_group_shared_memory[i];
            }
        }
#endif

#else
        // Half of the group shared memory can be saved by the following method:
        // Half threads store the local values into the group shared memory, and the other threads read back these values from the group shared memory and reduce them with their local values.

        brx_branch
        if (reduction_index >= GROUP_SHARED_MEMORY_COUNT && reduction_index < (GROUP_SHARED_MEMORY_COUNT * 2))
        {
            brx_int group_shared_memory_index = reduction_index - GROUP_SHARED_MEMORY_COUNT;
            reduction_group_shared_memory[group_shared_memory_index] = reduction_thread_local;
        }

        brx_group_memory_barrier_with_group_sync();

        brx_branch
        if (reduction_index < GROUP_SHARED_MEMORY_COUNT)
        {
            brx_int group_shared_memory_index = reduction_index;
            reduction_group_shared_memory[group_shared_memory_index] = reduction_thread_local + reduction_group_shared_memory[group_shared_memory_index];
        }

#if 1
        brx_unroll
        for (brx_int k = (GROUP_SHARED_MEMORY_COUNT / 2); k > 1; k /= 2)
        {
            brx_group_memory_barrier_with_group_sync();

            brx_branch
            if (reduction_index < k)
            {
                brx_int group_shared_memory_index = reduction_index;
                reduction_group_shared_memory[group_shared_memory_index] = reduction_group_shared_memory[group_shared_memory_index] + reduction_group_shared_memory[group_shared_memory_index + k];
            }
        }
#else
        brx_unroll
        for (brx_int k = brx_firstbithigh(GROUP_SHARED_MEMORY_COUNT / 2); k > 0; --k)
        {
            brx_group_memory_barrier_with_group_sync();

            brx_branch
            if (reduction_index < (1 << k))
            {
                brx_int group_shared_memory_index = reduction_index;
                reduction_group_shared_memory[group_shared_memory_index] = reduction_group_shared_memory[group_shared_memory_index] + reduction_group_shared_memory[group_shared_memory_index + (1 << k)];
            }
        }
#endif

        brx_group_memory_barrier_with_group_sync();

        brx_branch
        if (0 == reduction_index)
        {
            brx_int group_shared_memory_index = reduction_index;
            reduction_group_total = reduction_group_shared_memory[group_shared_memory_index] + reduction_group_shared_memory[group_shared_memory_index + 1];
        }
#endif
    }

    // write the final result into the global memory
    if (0 == reduction_index)
    {
        brx_float ambient_occlusion = reduction_group_total;
        brx_store_2d(g_ambient_occlusion_texture, brx_int2(brx_group_id.xy), brx_float4(ambient_occlusion, 0.0, 0.0, 0.0));
    }
}
