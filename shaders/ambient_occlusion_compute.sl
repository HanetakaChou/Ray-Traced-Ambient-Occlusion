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

#include "ambient_occlusion_pipeline_layout.sli"
#include "octahedron_mapping.sli"
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

#define USE_WAVE_INTRINSICS 1

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
pal_group_shared pal_float reduction_group_shared_memory[GROUP_SHARED_MEMORY_COUNT];

// NOTE: user can customize value
#define INVALID_GEOMETRIC_BUFFER_DEPTH 0.0
#define MAX_RAY_QUERY_PROCEED_COUNT 7

pal_root_signature(ambient_occlusion_root_signature_macro, ambient_occlusion_root_signature_name)
pal_num_threads(THREAD_GROUP_X, THREAD_GROUP_Y, THREAD_GROUP_Z)
pal_compute_shader_parameter_begin(main)
pal_compute_shader_parameter_in_group_id pal_compute_shader_parameter_split
pal_compute_shader_parameter_in_group_thread_id pal_compute_shader_parameter_split
pal_compute_shader_parameter_in_group_index
pal_pixel_shader_parameter_end(main)
{
    // Geometric Buffer: Position + Normal
    pal_float geometric_buffer_depth = pal_load_2d(g_geometric_buffer_textures[1], g_geometric_buffer_sampler[0], pal_int3(pal_group_id.xy, 0)).x;

    pal_branch
    if (INVALID_GEOMETRIC_BUFFER_DEPTH == geometric_buffer_depth)
    {
        pal_store_2d(g_ambient_occlusion_texture[0], pal_int2(pal_group_id.xy), pal_float4(0.0, 0.0, 0.0, 1.0));
        return;
    }

    pal_float2 geometric_buffer_normal = pal_load_2d(g_geometric_buffer_textures[0], g_geometric_buffer_sampler[0], pal_int3(pal_group_id.xy, 0)).xy;

    pal_float3 position_world_space;
    {
        pal_float2 uv = (pal_float2(pal_group_id.xy) + pal_float2(0.5, 0.5)) / pal_float2(g_screen_width, g_screen_height);

        pal_float3 position_ndc_space = pal_float3(uv * pal_float2(2.0, -2.0) + pal_float2(-1.0, 1.0), geometric_buffer_depth);

        pal_float4 position_view_space_with_w = pal_mul(g_inverse_projection_transform, pal_float4(position_ndc_space, 1.0));

        pal_float3 position_view_space = position_view_space_with_w.xyz / position_view_space_with_w.w;

        position_world_space = pal_mul(g_inverse_view_transform, pal_float4(position_view_space, 1.0)).xyz;
    }

    pal_float3 normal_world_space = octahedron_unmap(geometric_buffer_normal * 2.0 - pal_float2(1.0, 1.0));

    pal_int reduction_index = pal_int(pal_group_index);

    pal_int num_samples = pal_min(pal_int(g_sample_count), MAX_SAMPLE_COUNT);

    // Since the clamped cosine is isotropic, the outgoing direction V is **usually** assumed to be in the XOZ plane.
    // Actually the clamped cosine is **also** radially symmetric and the tangent direction is arbitrary.
    // UE: [GetTangentBasis](https://github.com/EpicGames/UnrealEngine/blob/4.27/Engine/Shaders/Private/MonteCarlo.ush#L12)
    // U3D: [GetLocalFrame](https://github.com/Unity-Technologies/Graphics/blob/v10.8.1/com.unity.render-pipelines.core/ShaderLibrary/CommonLighting.hlsl#L408)
    pal_float3x3 tangent_to_world_transform;
    {
        // NOTE: "local_z" should be normalized.
        pal_float3 local_z = normal_world_space;

        pal_float x = local_z.x;
        pal_float y = local_z.y;
        pal_float z = local_z.z;

        pal_float sz = z >= 0.0 ? 1.0 : -1.0;
        pal_float a = 1.0 / (sz + z);
        pal_float ya = y * a;
        pal_float b = x * ya;
        pal_float c = x * sz;

        pal_float3 local_x = pal_float3(c * x * a - 1, sz * b, c);
        pal_float3 local_y = pal_float3(b, y * ya - sz, y);

        tangent_to_world_transform = pal_float3x3_from_columns(local_x, local_y, local_z);
    }

    // PBRT-V3: [AOIntegrator::Li](https://github.com/mmp/pbrt-v3/blob/master/src/integrators/ao.cpp)
    // PBRT-V4: [AOIntegrator::Li](https://github.com/mmp/pbrt-v4/blob/ci/src/pbrt/cpu/integrators.cpp#L1414)

    pal_float reduction_thread_local = 0.0;
    pal_branch
    if (reduction_index < num_samples)
    {
        pal_float2 xi = hammersley_2d(reduction_index, num_samples);
        pal_float x1 = xi.x;
        pal_float x2 = xi.y;

#if 1
        pal_float3 omega_i_tangent_sapce = normalized_clamped_cosine_sample_omega_i(pal_float2(x1, x2));
#else
        // PBR Book V3: ["13.6.1 Uniformly Sampling a Hemisphere"](https://www.pbr-book.org/3ed-2018/Monte_Carlo_Integration/2D_Sampling_with_Multidimensional_Transformations#UniformlySamplingaHemisphere)
        // PBR Book V4: ["A.5.2 Uniformly Sampling Hemispheres and Spheres"](https://www.pbr-book.org/4ed/Sampling_Algorithms/Sampling_Multidimensional_Functions#UniformlySamplingHemispheresandSpheres)
        pal_float sin_theta = sqrt(1.0 - x1 * x1);
        pal_float cos_theta = x1;
        pal_float phi = 2 * M_PI * x2;
        pal_float3 omega_i_tangent_sapce = pal_float3(pal_cos(phi) * sin_theta, pal_sin(phi) * sin_theta, cos_theta);
#endif

        pal_float3 omega_i_world_sapce = pal_mul(tangent_to_world_transform, omega_i_tangent_sapce);

        // PBR Book V3: ["13.2 The Monte Carlo Estimator"](https://www.pbr-book.org/3ed-2018/Monte_Carlo_Integration/The_Monte_Carlo_Estimator)
        // PBR Book V4: ["2.1.3 The Monte Carlo Estimator"](https://pbr-book.org/4ed/Monte_Carlo_Integration/Monte_Carlo_Basics#TheMonteCarloEstimator)
        // PDF =  clamped_cosine_theta / M_PI = LdotN / M_PI
        // (1.0 / NumSamples) * (Visibility * (LdotN / M_PI)) / PDF = (1.0 / NumSamples) * (Visibility * (LdotN / M_PI) / (LdotN / M_PI)) = (1.0 / NumSamples) * Visibility
        pal_float visibility;
        {
            pal_ray_query ray_query;

            pal_float3 offset_position_world_space = offset_ray_origin(position_world_space, normal_world_space, omega_i_world_sapce);

            pal_ray_query_trace_ray_inline(ray_query, g_scene_top_level_acceleration_structures[0], PAL_RAY_FLAG_NONE, 0xFF, offset_position_world_space, 0.0, omega_i_world_sapce, g_max_distance);

            pal_unroll
            for (pal_int i = 0; i < MAX_RAY_QUERY_PROCEED_COUNT; ++i)
            {
                pal_branch
                if (!pal_ray_query_proceed(ray_query))
                {
                    break;
                }
            }

            pal_branch
            if (PAL_COMMITTED_NOTHING != pal_ray_query_committed_status(ray_query))
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
    

    // Parallel Reduction
    pal_float reduction_group_total;
    {
#if USE_WAVE_INTRINSICS
        // U3D: [PLATFORM_SUPPORTS_WAVE_INTRINSICS](https://github.com/Unity-Technologies/Graphics/blob/v10.8.1/com.unity.render-pipelines.high-definition/Runtime/Sky/AmbientProbeConvolution.compute#L77)

        pal_int lane_index = reduction_index % pal_int(pal_wave_lane_count);
        pal_int wave_index = reduction_index / pal_int(pal_wave_lane_count);
        pal_int wave_count = num_samples / pal_int(pal_wave_lane_count);

        pal_float reduction_current_wave_total = pal_wave_active_sum(reduction_thread_local);

        pal_branch
        if (0 == lane_index)
        {
            reduction_group_shared_memory[wave_index] = reduction_current_wave_total;
        }

        pal_group_memory_barrier_with_group_sync();

#if 1
        pal_branch
        if (0 == wave_index)
        {
            // NOTE: the "wave count" must be NO greater than "lane count"
            pal_float reduction_other_wave_total = (lane_index < wave_count) ? reduction_group_shared_memory[lane_index] : 0.0;

            pal_float reduction_group_wave_total = pal_wave_active_sum(reduction_other_wave_total);

            pal_branch
            if (0 == lane_index)
            {
                reduction_group_total = reduction_group_wave_total;
            }
        }
#else
        pal_branch
        if (0 == wave_index && 0 == lane_index)
        {
            reduction_group_total = reduction_current_wave_total;

            pal_unroll_x(MAX_SAMPLE_COUNT / MIN_WAVE_LANE_COUNT)
            for (pal_int i = 1; i < wave_count; ++i)
            {
                reduction_group_total += reduction_group_shared_memory[i];
            }
        }
#endif

#else
        // Half of the group shared memory can be saved by the following method:
        // Half threads store the local values into the group shared memory, and the other threads read back these values from the group shared memory and reduce them with their local values.

        pal_branch
        if (reduction_index >= GROUP_SHARED_MEMORY_COUNT && reduction_index < (GROUP_SHARED_MEMORY_COUNT * 2))
        {
            pal_int group_shared_memory_index = reduction_index - GROUP_SHARED_MEMORY_COUNT;
            reduction_group_shared_memory[group_shared_memory_index] = reduction_thread_local;
        }

        pal_group_memory_barrier_with_group_sync();

        pal_branch
        if (reduction_index < GROUP_SHARED_MEMORY_COUNT)
        {
            pal_int group_shared_memory_index = reduction_index;
            reduction_group_shared_memory[group_shared_memory_index] = reduction_thread_local + reduction_group_shared_memory[group_shared_memory_index];
        }

#if 0
        pal_unroll
        for (pal_int k = (GROUP_SHARED_MEMORY_COUNT / 2); k > 1; k /= 2)
        {
            pal_group_memory_barrier_with_group_sync();

            pal_branch
            if (reduction_index < k)
            {
                pal_int group_shared_memory_index = reduction_index;
                reduction_group_shared_memory[group_shared_memory_index] = reduction_group_shared_memory[group_shared_memory_index] + reduction_group_shared_memory[group_shared_memory_index + k];
            }
        }
#else
        pal_unroll
        for (pal_int k = pal_firstbithigh(GROUP_SHARED_MEMORY_COUNT / 2); k > 0; --k)
        {
            pal_group_memory_barrier_with_group_sync();

            pal_branch
            if (reduction_index < (1 << k))
            {
                pal_int group_shared_memory_index = reduction_index;
                reduction_group_shared_memory[group_shared_memory_index] = reduction_group_shared_memory[group_shared_memory_index] + reduction_group_shared_memory[group_shared_memory_index + (1 << k)];
            }
        }
#endif

        pal_group_memory_barrier_with_group_sync();

        pal_branch
        if (0 == reduction_index)
        {
            pal_int group_shared_memory_index = reduction_index;
            reduction_group_total = reduction_group_shared_memory[group_shared_memory_index] + reduction_group_shared_memory[group_shared_memory_index + 1];
        }
#endif
    }

    // write the final result into the global memory
    if (0 == reduction_index)
    {
        pal_float ambient_occlusion = reduction_group_total;
        pal_store_2d(g_ambient_occlusion_texture[0], pal_int2(pal_group_id.xy), pal_float4(ambient_occlusion, ambient_occlusion, ambient_occlusion, 1.0));
    }
}