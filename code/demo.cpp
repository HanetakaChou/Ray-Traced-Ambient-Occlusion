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

#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <vector>
#include <assert.h>
#include "demo.h"
#include "support/camera_controller.h"
#include "../shaders/octahedron_mapping.sli"
#include "../shaders/gbuffer_pipeline_resource_binding.sli"
#include "../shaders/ambient_occlusion_pipeline_resource_binding.sli"

static inline DirectX::XMMATRIX XM_CALLCONV DirectX_Math_Matrix_PerspectiveFovRH_ReversedZ(float FovAngleY, float AspectRatio, float NearZ, float FarZ);

static float g_camera_fov = -1.0F;

Demo::Demo()
{
}

void Demo::init(brx_device *device)
{
	// Descriptor Layout - Create
	brx_descriptor_set_layout *gbuffer_pipeline_none_update_descriptor_set_layout = NULL;
	brx_descriptor_set_layout *ambient_occlusion_pipeline_none_update_descriptor_set_layout = NULL;
	{
		BRX_DESCRIPTOR_SET_LAYOUT_BINDING const gbuffer_pipeline_none_update_descriptor_set_layout_bindings[] = {
			{0U, BRX_DESCRIPTOR_TYPE_DYNAMIC_UNIFORM_BUFFER, 1U},
			{1U, BRX_DESCRIPTOR_TYPE_TOP_LEVEL_ACCELERATION_STRUCTURE, 1U},
			{2U, BRX_DESCRIPTOR_TYPE_READ_ONLY_STORAGE_BUFFER, MAX_GEOMETRY_BUFFER_COUNT},
			{3U, BRX_DESCRIPTOR_TYPE_DYNAMIC_UNIFORM_BUFFER, 1U},
			{4U, BRX_DESCRIPTOR_TYPE_DYNAMIC_UNIFORM_BUFFER, 1U},
			{5U, BRX_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2U}};
		gbuffer_pipeline_none_update_descriptor_set_layout = device->create_descriptor_set_layout(sizeof(gbuffer_pipeline_none_update_descriptor_set_layout_bindings) / sizeof(gbuffer_pipeline_none_update_descriptor_set_layout_bindings[0]), gbuffer_pipeline_none_update_descriptor_set_layout_bindings);

		brx_descriptor_set_layout *const gbuffer_pipeline_descriptor_set_layouts[] = {
			gbuffer_pipeline_none_update_descriptor_set_layout};
		this->m_gbuffer_pipeline_layout = device->create_pipeline_layout(sizeof(gbuffer_pipeline_descriptor_set_layouts) / sizeof(gbuffer_pipeline_descriptor_set_layouts[0]), gbuffer_pipeline_descriptor_set_layouts);

		BRX_DESCRIPTOR_SET_LAYOUT_BINDING ambient_occlusion_pipeline_none_update_descriptor_set_layout_bindings[] = {
			{0U, BRX_DESCRIPTOR_TYPE_DYNAMIC_UNIFORM_BUFFER, 1U},
			{1U, BRX_DESCRIPTOR_TYPE_TOP_LEVEL_ACCELERATION_STRUCTURE, 1U},
			{2U, BRX_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 2U},
			{3U, BRX_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1U}};
		ambient_occlusion_pipeline_none_update_descriptor_set_layout = device->create_descriptor_set_layout(sizeof(ambient_occlusion_pipeline_none_update_descriptor_set_layout_bindings) / sizeof(ambient_occlusion_pipeline_none_update_descriptor_set_layout_bindings[0]), ambient_occlusion_pipeline_none_update_descriptor_set_layout_bindings);

		brx_descriptor_set_layout *const ambient_occlusion_pipeline_descriptor_set_layouts[] = {
			ambient_occlusion_pipeline_none_update_descriptor_set_layout};
		this->m_ambient_occlusion_pipeline_layout = device->create_pipeline_layout(sizeof(ambient_occlusion_pipeline_descriptor_set_layouts) / sizeof(ambient_occlusion_pipeline_descriptor_set_layouts[0]), ambient_occlusion_pipeline_descriptor_set_layouts);
	}

	// Render Pass and Pipeline
	{
		// Pipeline
		{
#if defined(USE_D3D12) && USE_D3D12
			typedef uint8_t BYTE;
#include "../dxbc/gbuffer_compute.inl"
#elif defined(USE_VK) && USE_VK
			constexpr uint32_t const gbuffer_compute_shader_module_code[] = {
#include "../spirv/gbuffer_compute.inl"
			};
#else
#error Unknown Backend
#endif

			this->m_gbuffer_pipeline = device->create_compute_pipeline(this->m_gbuffer_pipeline_layout, sizeof(gbuffer_compute_shader_module_code), gbuffer_compute_shader_module_code);
		}

		{
#if defined(USE_D3D12) && USE_D3D12
			typedef uint8_t BYTE;
#include "../dxbc/ambient_occlusion_compute.inl"
#elif defined(USE_VK) && USE_VK
			constexpr uint32_t const ambient_occlusion_compute_shader_module_code[] = {
#include "../spirv/ambient_occlusion_compute.inl"
			};
#else
#error Unknown Backend
#endif

			this->m_ambient_occlusion_pipeline = device->create_compute_pipeline(this->m_ambient_occlusion_pipeline_layout, sizeof(ambient_occlusion_compute_shader_module_code), ambient_occlusion_compute_shader_module_code);
		}
	}

	// Init Intermediate Images
	this->m_screen_width = 512U;
	this->m_screen_height = 512U;
	this->m_gbuffer_depth_image = device->create_storage_image(BRX_STORAGE_IMAGE_FORMAT_R32_UINT, this->m_screen_width, this->m_screen_height, true);
	this->m_gbuffer_normal_image = device->create_storage_image(BRX_STORAGE_IMAGE_FORMAT_R32_UINT, this->m_screen_width, this->m_screen_height, true);
	this->m_ambient_occlusion_image = device->create_storage_image(BRX_STORAGE_IMAGE_FORMAT_R16G16B16A16_SFLOAT, this->m_screen_width, this->m_screen_height, true);

	// Assets
	{
		brx_compacted_bottom_level_acceleration_structure_size_query_pool *const compacted_bottom_level_acceleration_structure_size_query_pool = device->create_compacted_bottom_level_acceleration_structure_size_query_pool(scene_instance_count);

		brx_upload_command_buffer *const upload_command_buffer = device->create_upload_command_buffer();

		brx_graphics_command_buffer *const graphics_command_buffer = device->create_graphics_command_buffer();

		brx_upload_queue *const upload_queue = device->create_upload_queue();

		brx_graphics_queue *const graphics_queue = device->create_graphics_queue();

		brx_fence *const fence = device->create_fence(true);

		brx_staging_non_compacted_bottom_level_acceleration_structure *scene_staging_non_compacted_bottom_level_acceleration_structures[scene_instance_count] = {
			NULL,
			NULL};

		// step 1
		// upload vertex position buffer
		// upload vertex varying buffer
		// upload index buffer
		// build non compacted bottom level acceleration structure
		{
			std::vector<brx_staging_upload_buffer *> staging_upload_buffers;

			std::vector<brx_scratch_buffer *> scratch_buffers;

			device->reset_upload_command_buffer(upload_command_buffer);

			device->reset_graphics_command_buffer(graphics_command_buffer);

			upload_command_buffer->begin();

			graphics_command_buffer->begin();

			uint32_t scene_vertex_counts[scene_instance_count];
			float const *scene_vertex_positions[scene_instance_count];
			float const *scene_vertex_normals[scene_instance_count];
			uint16_t const *scene_indices[scene_instance_count];
			{
#include "../assets/plane.h"
#include "../assets/house.h"

				scene_vertex_counts[0] = g_plane_vertex_count;
				scene_vertex_counts[1] = g_house_vertex_count;

				scene_vertex_positions[0] = g_plane_vertex_position;
				scene_vertex_positions[1] = g_house_vertex_position;

				scene_vertex_normals[0] = g_plane_vertex_normal;
				scene_vertex_normals[1] = g_house_vertex_normal;

				this->m_scene_index_counts[0] = g_plane_index_count;
				this->m_scene_index_counts[1] = g_house_index_count;

				scene_indices[0] = g_plane_index;
				scene_indices[1] = g_house_index;
			}

			// vertex position buffer
			// vertex varying buffer
			// index buffer
			// non compacted bottom level acceleration structure
			for (uint32_t instance_index = 0; instance_index < scene_instance_count; ++instance_index)
			{
				{
					uint32_t const vertex_position_buffer_size = (sizeof(float) * 3U) * scene_vertex_counts[instance_index];

					this->m_scene_vertex_position_buffers[instance_index] = device->create_asset_vertex_position_buffer(vertex_position_buffer_size);

					brx_staging_upload_buffer *const vertex_position_staging_upload_buffer = device->create_staging_upload_buffer(vertex_position_buffer_size);

					std::memcpy(vertex_position_staging_upload_buffer->get_host_memory_range_base(), scene_vertex_positions[instance_index], vertex_position_buffer_size);

					upload_command_buffer->upload_from_staging_upload_buffer_to_asset_vertex_position_buffer(this->m_scene_vertex_position_buffers[instance_index], 0U, vertex_position_staging_upload_buffer, 0U, vertex_position_buffer_size);

					staging_upload_buffers.push_back(vertex_position_staging_upload_buffer);
				}

				{
					std::vector<uint32_t> converted_vertex_normal(static_cast<size_t>(scene_vertex_counts[instance_index]));
					for (uint32_t vertex_index = 0; vertex_index < scene_vertex_counts[instance_index]; ++vertex_index)
					{
						DirectX::XMFLOAT2 const position_rectangle_space = octahedron_map(DirectX::XMFLOAT3(scene_vertex_normals[instance_index][3U * vertex_index], scene_vertex_normals[instance_index][3U * vertex_index + 1U], scene_vertex_normals[instance_index][3U * vertex_index + 2U]));

#ifndef NDEBUG
						DirectX::XMFLOAT3 const position_sphere_space = octahedron_unmap(position_rectangle_space);
#endif
						assert(std::abs(position_sphere_space.x - scene_vertex_normals[instance_index][3U * vertex_index]) < 0.00000075F);
						assert(std::abs(position_sphere_space.y - scene_vertex_normals[instance_index][3U * vertex_index + 1U]) < 0.00000075F);
						assert(std::abs(position_sphere_space.z - scene_vertex_normals[instance_index][3U * vertex_index + 2U]) < 0.00000075F);

						DirectX::PackedVector::XMSHORTN2 packed_vector;
						DirectX::PackedVector::XMStoreShortN2(&packed_vector, DirectX::XMLoadFloat2(&position_rectangle_space));

						converted_vertex_normal[vertex_index] = packed_vector.v;
					}

					uint32_t const vertex_vary_buffer_size = sizeof(uint32_t) * scene_vertex_counts[instance_index];

					this->m_scene_vertex_varying_buffers[instance_index] = device->create_asset_vertex_varying_buffer(vertex_vary_buffer_size);

					brx_staging_upload_buffer *const vertex_varying_staging_upload_buffer = device->create_staging_upload_buffer(vertex_vary_buffer_size);

					std::memcpy(vertex_varying_staging_upload_buffer->get_host_memory_range_base(), &converted_vertex_normal[0], vertex_vary_buffer_size);

					upload_command_buffer->upload_from_staging_upload_buffer_to_asset_vertex_varying_buffer(this->m_scene_vertex_varying_buffers[instance_index], 0U, vertex_varying_staging_upload_buffer, 0U, vertex_vary_buffer_size);

					staging_upload_buffers.push_back(vertex_varying_staging_upload_buffer);
				}

				{
					uint32_t const index_buffer_size = sizeof(uint16_t) * this->m_scene_index_counts[instance_index];

					this->m_scene_index_buffers[instance_index] = device->create_asset_index_buffer(index_buffer_size);

					brx_staging_upload_buffer *const index_staging_upload_buffer = device->create_staging_upload_buffer(index_buffer_size);

					std::memcpy(index_staging_upload_buffer->get_host_memory_range_base(), scene_indices[instance_index], index_buffer_size);

					upload_command_buffer->upload_from_staging_upload_buffer_to_asset_index_buffer(this->m_scene_index_buffers[instance_index], 0U, index_staging_upload_buffer, 0U, index_buffer_size);

					staging_upload_buffers.push_back(index_staging_upload_buffer);
				}

				{
					uint32_t staging_non_compacted_bottom_level_acceleration_structure_size = -1;
					uint32_t staging_non_compacted_bottom_level_acceleration_structure_build_scratch_size = -1;
					BRX_BOTTOM_LEVEL_ACCELERATION_STRUCTURE_GEOMETRY const bottom_level_acceleration_structure_geometries[1] = {
						{true,
						 BRX_GRAPHICS_PIPELINE_VERTEX_ATTRIBUTE_FORMAT_R32G32B32_SFLOAT,
						 sizeof(float) * 3U,
						 scene_vertex_counts[instance_index],
						 this->m_scene_vertex_position_buffers[instance_index]->get_vertex_position_buffer(),
						 BRX_GRAPHICS_PIPELINE_INDEX_TYPE_UINT16,
						 this->m_scene_index_counts[instance_index],
						 this->m_scene_index_buffers[instance_index]->get_index_buffer()}};
					device->get_staging_non_compacted_bottom_level_acceleration_structure_size(sizeof(bottom_level_acceleration_structure_geometries) / sizeof(bottom_level_acceleration_structure_geometries[0]), &bottom_level_acceleration_structure_geometries[0], &staging_non_compacted_bottom_level_acceleration_structure_size, &staging_non_compacted_bottom_level_acceleration_structure_build_scratch_size);

					scene_staging_non_compacted_bottom_level_acceleration_structures[instance_index] = device->create_staging_non_compacted_bottom_level_acceleration_structure(staging_non_compacted_bottom_level_acceleration_structure_size);

					brx_scratch_buffer *const staging_non_compacted_bottom_level_acceleration_structure_build_scratch_buffer = device->create_scratch_buffer(staging_non_compacted_bottom_level_acceleration_structure_build_scratch_size);

					upload_command_buffer->build_staging_non_compacted_bottom_level_acceleration_structure(scene_staging_non_compacted_bottom_level_acceleration_structures[instance_index], sizeof(bottom_level_acceleration_structure_geometries) / sizeof(bottom_level_acceleration_structure_geometries[0]), &bottom_level_acceleration_structure_geometries[0], staging_non_compacted_bottom_level_acceleration_structure_build_scratch_buffer, compacted_bottom_level_acceleration_structure_size_query_pool, instance_index);

					scratch_buffers.push_back(staging_non_compacted_bottom_level_acceleration_structure_build_scratch_buffer);
				}
			}

			// release and acquire
			// TODO: merge barrier
			for (uint32_t instance_index = 0; instance_index < scene_instance_count; ++instance_index)
			{
				upload_command_buffer->release_asset_vertex_position_buffer(this->m_scene_vertex_position_buffers[instance_index]);

				upload_command_buffer->release_asset_vertex_varying_buffer(this->m_scene_vertex_varying_buffers[instance_index]);

				upload_command_buffer->release_asset_index_buffer(this->m_scene_index_buffers[instance_index]);

				graphics_command_buffer->acquire_asset_vertex_position_buffer(this->m_scene_vertex_position_buffers[instance_index]);

				graphics_command_buffer->acquire_asset_vertex_varying_buffer(this->m_scene_vertex_varying_buffers[instance_index]);

				graphics_command_buffer->acquire_asset_index_buffer(this->m_scene_index_buffers[instance_index]);
			}

			upload_command_buffer->end();

			graphics_command_buffer->end();

			upload_queue->submit_and_signal(upload_command_buffer);

			device->reset_fence(fence);

			graphics_queue->wait_and_submit(upload_command_buffer, graphics_command_buffer, fence);

			device->wait_for_fence(fence);

			for (brx_staging_upload_buffer *const staging_upload_buffer : staging_upload_buffers)
			{
				device->destroy_staging_upload_buffer(staging_upload_buffer);
			}

			staging_upload_buffers.clear();

			for (brx_scratch_buffer *const scratch_buffer : scratch_buffers)
			{
				device->destroy_scratch_buffer(scratch_buffer);
			}

			scratch_buffers.clear();
		}

		// step 2
		// build compacted bottom level acceleration structure
		// build top level acceleration structure
		{
			brx_scratch_buffer *top_level_acceleration_structure_build_scratch_buffer = NULL;

			device->reset_upload_command_buffer(upload_command_buffer);

			device->reset_graphics_command_buffer(graphics_command_buffer);

			upload_command_buffer->begin();

			graphics_command_buffer->begin();

			// compacted bottom level acceleration structure
			for (uint32_t instance_index = 0U; instance_index < scene_instance_count; ++instance_index)
			{
				uint32_t const compacted_bottom_level_acceleration_structure_size = device->get_compacted_bottom_level_acceleration_structure_size_query_pool_result(compacted_bottom_level_acceleration_structure_size_query_pool, instance_index);

				this->m_scene_compacted_bottom_level_acceleration_structures[instance_index] = device->create_asset_compacted_bottom_level_acceleration_structure(compacted_bottom_level_acceleration_structure_size);

				upload_command_buffer->compact_bottom_level_acceleration_structure(this->m_scene_compacted_bottom_level_acceleration_structures[instance_index], scene_staging_non_compacted_bottom_level_acceleration_structures[instance_index]);
			}

			// release and acquire
			for (uint32_t instance_index = 0U; instance_index < scene_instance_count; ++instance_index)
			{
				upload_command_buffer->release_asset_compacted_bottom_level_acceleration_structure(this->m_scene_compacted_bottom_level_acceleration_structures[instance_index]);

				graphics_command_buffer->acquire_asset_compacted_bottom_level_acceleration_structure(this->m_scene_compacted_bottom_level_acceleration_structures[instance_index]);
			}

			// scene top level acceleration structure
			{
				DirectX::XMMATRIX const scene_instance_transform_matrices[scene_instance_count] = {
					DirectX::XMMatrixIdentity(),
					DirectX::XMMatrixIdentity()};

				uint32_t top_level_acceleration_structure_size = -1;
				uint32_t top_level_acceleration_structure_build_scratch_size = -1;
				uint32_t top_level_acceleration_structure_update_scratch_size = -1;
				device->get_top_level_acceleration_structure_size(scene_instance_count, &top_level_acceleration_structure_size, &top_level_acceleration_structure_build_scratch_size, &top_level_acceleration_structure_update_scratch_size);

				this->m_scene_top_level_acceleration_structure = device->create_top_level_acceleration_structure(top_level_acceleration_structure_size);

				this->m_scene_top_level_acceleration_structure_instance_upload_buffer = device->create_top_level_acceleration_structure_instance_upload_buffer(scene_instance_count);

				for (uint32_t instance_index = 0U; instance_index < scene_instance_count; ++instance_index)
				{
					BRX_TOP_LEVEL_ACCELERATION_STRUCTURE_INSTANCE top_level_acceleration_structure_instance = {
						{},
						instance_index,
						0XFFU,
						true,
						false,
						false,
						false,
						this->m_scene_compacted_bottom_level_acceleration_structures[instance_index]};

					DirectX::XMStoreFloat3x4(reinterpret_cast<DirectX::XMFLOAT3X4 *>(&top_level_acceleration_structure_instance.transform_matrix), DirectX::XMMatrixIdentity());

					this->m_scene_top_level_acceleration_structure_instance_upload_buffer->write_instance(instance_index, &top_level_acceleration_structure_instance);
				}

				top_level_acceleration_structure_build_scratch_buffer = device->create_scratch_buffer(top_level_acceleration_structure_build_scratch_size);

				graphics_command_buffer->build_top_level_acceleration_structure(this->m_scene_top_level_acceleration_structure, scene_instance_count, this->m_scene_top_level_acceleration_structure_instance_upload_buffer, top_level_acceleration_structure_build_scratch_buffer);

				this->m_scene_top_level_acceleration_structure_update_scratch_buffer = device->create_scratch_buffer(top_level_acceleration_structure_update_scratch_size);
			}

			upload_command_buffer->end();

			graphics_command_buffer->end();

			upload_queue->submit_and_signal(upload_command_buffer);

			device->reset_fence(fence);

			graphics_queue->wait_and_submit(upload_command_buffer, graphics_command_buffer, fence);

			device->wait_for_fence(fence);

			device->destroy_scratch_buffer(top_level_acceleration_structure_build_scratch_buffer);

			for (uint32_t instance_index = 0U; instance_index < scene_instance_count; ++instance_index)
			{
				device->destroy_staging_non_compacted_bottom_level_acceleration_structure(scene_staging_non_compacted_bottom_level_acceleration_structures[instance_index]);
				scene_staging_non_compacted_bottom_level_acceleration_structures[instance_index] = NULL;
			}
		}

		device->destroy_compacted_bottom_level_acceleration_structure_size_query_pool(compacted_bottom_level_acceleration_structure_size_query_pool);

		device->destroy_fence(fence);

		device->destroy_upload_command_buffer(upload_command_buffer);

		device->destroy_graphics_command_buffer(graphics_command_buffer);

		device->destroy_upload_queue(upload_queue);

		device->destroy_graphics_queue(graphics_queue);
	}

	// Uniform Buffers
	{
		this->m_common_none_update_uniform_buffer = device->create_uniform_upload_buffer(sizeof(common_none_update_set_uniform_buffer_binding));

		this->m_gbuffer_pipeline_none_update_geometry_data_uniform_buffer = device->create_uniform_upload_buffer(sizeof(gbuffer_pipeline_none_update_set_geometry_data_uniform_buffer_binding));

		this->m_gbuffer_pipeline_none_update_instance_data_uniform_buffer = device->create_uniform_upload_buffer(sizeof(gbuffer_pipeline_none_update_set_instance_data_uniform_buffer_binding));
	}

	// Descriptor
	{
		// Geometric Buffer Pipeline
		{
			this->m_gbuffer_pipeline_none_update_descriptor_set = device->create_descriptor_set(gbuffer_pipeline_none_update_descriptor_set_layout);
			{
				constexpr uint32_t const dynamic_uniform_buffers_range = sizeof(common_none_update_set_uniform_buffer_binding);
				device->write_descriptor_set(this->m_gbuffer_pipeline_none_update_descriptor_set, BRX_DESCRIPTOR_TYPE_DYNAMIC_UNIFORM_BUFFER, 0U, 0U, 1U, &this->m_common_none_update_uniform_buffer, &dynamic_uniform_buffers_range, NULL, NULL, NULL, NULL, NULL);
			}
			{
				brx_top_level_acceleration_structure const *const top_level_acceleration_structures[] = {
					this->m_scene_top_level_acceleration_structure};
				device->write_descriptor_set(this->m_gbuffer_pipeline_none_update_descriptor_set, BRX_DESCRIPTOR_TYPE_TOP_LEVEL_ACCELERATION_STRUCTURE, 1U, 0U, sizeof(top_level_acceleration_structures) / sizeof(top_level_acceleration_structures[0]), NULL, NULL, NULL, NULL, NULL, NULL, &top_level_acceleration_structures[0]);
			}
			{
				brx_storage_buffer const *storage_buffers[3 * scene_instance_count];
				for (uint32_t instance_index = 0; instance_index < scene_instance_count; ++instance_index)
				{
					storage_buffers[3 * instance_index] = this->m_scene_vertex_position_buffers[instance_index]->get_storage_buffer();
					storage_buffers[3 * instance_index + 1] = this->m_scene_vertex_varying_buffers[instance_index]->get_storage_buffer();
					storage_buffers[3 * instance_index + 2] = this->m_scene_index_buffers[instance_index]->get_storage_buffer();
				}
				device->write_descriptor_set(this->m_gbuffer_pipeline_none_update_descriptor_set, BRX_DESCRIPTOR_TYPE_READ_ONLY_STORAGE_BUFFER, 2U, 0U, sizeof(storage_buffers) / sizeof(storage_buffers[0]), NULL, NULL, storage_buffers, NULL, NULL, NULL, NULL);
			}
			{
				constexpr uint32_t const dynamic_uniform_buffers_range = sizeof(gbuffer_pipeline_none_update_set_geometry_data_uniform_buffer_binding);
				device->write_descriptor_set(this->m_gbuffer_pipeline_none_update_descriptor_set, BRX_DESCRIPTOR_TYPE_DYNAMIC_UNIFORM_BUFFER, 3U, 0U, 1U, &this->m_gbuffer_pipeline_none_update_geometry_data_uniform_buffer, &dynamic_uniform_buffers_range, NULL, NULL, NULL, NULL, NULL);
			}
			{
				constexpr uint32_t const dynamic_uniform_buffers_range = sizeof(gbuffer_pipeline_none_update_set_instance_data_uniform_buffer_binding);
				device->write_descriptor_set(this->m_gbuffer_pipeline_none_update_descriptor_set, BRX_DESCRIPTOR_TYPE_DYNAMIC_UNIFORM_BUFFER, 4U, 0U, 1U, &this->m_gbuffer_pipeline_none_update_instance_data_uniform_buffer, &dynamic_uniform_buffers_range, NULL, NULL, NULL, NULL, NULL);
			}
			{
				brx_storage_image const *const storage_images[] = {
					this->m_gbuffer_depth_image,
					this->m_gbuffer_normal_image};
				device->write_descriptor_set(this->m_gbuffer_pipeline_none_update_descriptor_set, BRX_DESCRIPTOR_TYPE_STORAGE_IMAGE, 5U, 0U, sizeof(storage_images) / sizeof(storage_images[0]), NULL, NULL, NULL, NULL, NULL, &storage_images[0], NULL);
			}
		}

		// Ambient Occlusion Pipeline
		{
			this->m_ambient_occlusion_pipeline_none_update_escriptor_set = device->create_descriptor_set(ambient_occlusion_pipeline_none_update_descriptor_set_layout);
			{
				constexpr uint32_t const dynamic_uniform_buffers_range = sizeof(common_none_update_set_uniform_buffer_binding);
				device->write_descriptor_set(this->m_ambient_occlusion_pipeline_none_update_escriptor_set, BRX_DESCRIPTOR_TYPE_DYNAMIC_UNIFORM_BUFFER, 0U, 0U, 1U, &this->m_common_none_update_uniform_buffer, &dynamic_uniform_buffers_range, NULL, NULL, NULL, NULL, NULL);
			}
			{
				brx_top_level_acceleration_structure const *const top_level_acceleration_structures[] = {
					this->m_scene_top_level_acceleration_structure};
				device->write_descriptor_set(this->m_ambient_occlusion_pipeline_none_update_escriptor_set, BRX_DESCRIPTOR_TYPE_TOP_LEVEL_ACCELERATION_STRUCTURE, 1U, 0U, sizeof(top_level_acceleration_structures) / sizeof(top_level_acceleration_structures[0]), NULL, NULL, NULL, NULL, NULL, NULL, &top_level_acceleration_structures[0]);
			}
			{
				brx_sampled_image const *const sampled_images[] = {
					this->m_gbuffer_depth_image->get_sampled_image(),
					this->m_gbuffer_normal_image->get_sampled_image()};
				device->write_descriptor_set(this->m_ambient_occlusion_pipeline_none_update_escriptor_set, BRX_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 2U, 0U, sizeof(sampled_images) / sizeof(sampled_images[0]), NULL, NULL, NULL, sampled_images, NULL, NULL, NULL);
			}
			{
				brx_storage_image const *const storage_images[] = {
					this->m_ambient_occlusion_image};
				device->write_descriptor_set(this->m_ambient_occlusion_pipeline_none_update_escriptor_set, BRX_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3U, 0U, sizeof(storage_images) / sizeof(storage_images[0]), NULL, NULL, NULL, NULL, NULL, &storage_images[0], NULL);
			}
		}
	}

	// Descriptor Layout - Destroy
	device->destroy_descriptor_set_layout(gbuffer_pipeline_none_update_descriptor_set_layout);
	device->destroy_descriptor_set_layout(ambient_occlusion_pipeline_none_update_descriptor_set_layout);
	gbuffer_pipeline_none_update_descriptor_set_layout = NULL;
	ambient_occlusion_pipeline_none_update_descriptor_set_layout = NULL;

	// Init Transform
	this->m_house_spin_angle = 0.0F;

	// Init Camera
	g_camera_controller.m_eye_position = DirectX::XMFLOAT3(5.0F, 4.0F, -4.0F);
	g_camera_controller.m_eye_direction = DirectX::XMFLOAT3(-5.0F, -3.0F, 4.0F);
	g_camera_controller.m_up_direction = DirectX::XMFLOAT3(0.0F, 1.0F, 0.0F);
	g_camera_fov = 1.047F;
}

void Demo::destroy(brx_device *device)
{
	device->destroy_descriptor_set(this->m_gbuffer_pipeline_none_update_descriptor_set);
	device->destroy_descriptor_set(this->m_ambient_occlusion_pipeline_none_update_escriptor_set);

	device->destroy_uniform_upload_buffer(this->m_common_none_update_uniform_buffer);
	device->destroy_uniform_upload_buffer(this->m_gbuffer_pipeline_none_update_geometry_data_uniform_buffer);
	device->destroy_uniform_upload_buffer(this->m_gbuffer_pipeline_none_update_instance_data_uniform_buffer);

	for (uint32_t instance_index = 0; instance_index < scene_instance_count; ++instance_index)
	{
		device->destroy_asset_vertex_position_buffer(this->m_scene_vertex_position_buffers[instance_index]);
		device->destroy_asset_vertex_varying_buffer(this->m_scene_vertex_varying_buffers[instance_index]);
		device->destroy_asset_index_buffer(this->m_scene_index_buffers[instance_index]);
		device->destroy_asset_compacted_bottom_level_acceleration_structure(this->m_scene_compacted_bottom_level_acceleration_structures[instance_index]);
	}

	device->destroy_top_level_acceleration_structure(this->m_scene_top_level_acceleration_structure);
	device->destroy_top_level_acceleration_structure_instance_upload_buffer(this->m_scene_top_level_acceleration_structure_instance_upload_buffer);
	device->destroy_scratch_buffer(this->m_scene_top_level_acceleration_structure_update_scratch_buffer);

	device->destroy_storage_image(this->m_gbuffer_depth_image);
	device->destroy_storage_image(this->m_gbuffer_normal_image);
	device->destroy_storage_image(this->m_ambient_occlusion_image);

	device->destroy_compute_pipeline(this->m_ambient_occlusion_pipeline);

	device->destroy_pipeline_layout(this->m_ambient_occlusion_pipeline_layout);

	device->destroy_compute_pipeline(this->m_gbuffer_pipeline);

	device->destroy_pipeline_layout(this->m_gbuffer_pipeline_layout);
}

brx_sampled_image const *Demo::get_sampled_image_for_present()
{
	return this->m_ambient_occlusion_image->get_sampled_image();
}

void Demo::draw(brx_graphics_command_buffer *command_buffer, float interval_time)
{
	this->m_house_spin_angle += (45.0F / 180.0F * DirectX::XM_PI) * interval_time;

	DirectX::XMMATRIX const model_transforms[scene_instance_count] = {
		DirectX::XMMatrixIdentity(),
		DirectX::XMMatrixRotationY(this->m_house_spin_angle)};

	// Update Uniform Buffer
	{
		// Common - None Update (update frequency denotes the updating of the resource bindings)
		{
			common_none_update_set_uniform_buffer_binding *const common_none_update_set_uniform_buffer_binding_destination = reinterpret_cast<common_none_update_set_uniform_buffer_binding *>(this->m_common_none_update_uniform_buffer->get_host_memory_range_base());

			DirectX::XMFLOAT3 eye_position = g_camera_controller.m_eye_position;
			DirectX::XMFLOAT3 eye_direction = g_camera_controller.m_eye_direction;
			DirectX::XMFLOAT3 up_direction = g_camera_controller.m_up_direction;
			DirectX::XMMATRIX view_transform = DirectX::XMMatrixLookToRH(DirectX::XMLoadFloat3(&eye_position), DirectX::XMLoadFloat3(&eye_direction), DirectX::XMLoadFloat3(&up_direction));
			DirectX::XMMATRIX projection_transform = DirectX_Math_Matrix_PerspectiveFovRH_ReversedZ(g_camera_fov, static_cast<float>(this->m_screen_width) / static_cast<float>(this->m_screen_height), 0.1F, 1000.0F);

			DirectX::XMStoreFloat4x4(&common_none_update_set_uniform_buffer_binding_destination->g_view_transform, view_transform);
			DirectX::XMStoreFloat4x4(&common_none_update_set_uniform_buffer_binding_destination->g_projection_transform, projection_transform);
			DirectX::XMStoreFloat4x4(&common_none_update_set_uniform_buffer_binding_destination->g_inverse_view_transform, DirectX::XMMatrixInverse(NULL, view_transform));
			DirectX::XMStoreFloat4x4(&common_none_update_set_uniform_buffer_binding_destination->g_inverse_projection_transform, DirectX::XMMatrixInverse(NULL, projection_transform));

			common_none_update_set_uniform_buffer_binding_destination->g_screen_width = static_cast<float>(this->m_screen_width);
			common_none_update_set_uniform_buffer_binding_destination->g_screen_height = static_cast<float>(this->m_screen_height);

			common_none_update_set_uniform_buffer_binding_destination->g_ambient_occlusion_max_distance = 5.0F;
			common_none_update_set_uniform_buffer_binding_destination->g_ambient_occlusion_sample_count = 64.0F;
		}
	}

	// Geometric Buffer Pipeline - None Update
	{

		gbuffer_pipeline_none_update_set_geometry_data_uniform_buffer_binding *const gbuffer_pipeline_none_update_set_geometry_data_uniform_buffer_binding_destination = reinterpret_cast<gbuffer_pipeline_none_update_set_geometry_data_uniform_buffer_binding *>(this->m_gbuffer_pipeline_none_update_geometry_data_uniform_buffer->get_host_memory_range_base());
		for (uint32_t geometry_index = 0; geometry_index < scene_instance_count; ++geometry_index)
		{
			gbuffer_pipeline_none_update_set_geometry_data_uniform_buffer_binding_destination->g_geometry_data[geometry_index].m_non_uniform_geometry_buffer_index = geometry_index;
		}

		gbuffer_pipeline_none_update_set_instance_data_uniform_buffer_binding *const gbuffer_pipeline_none_update_set_instance_data_uniform_buffer_binding_destination = reinterpret_cast<gbuffer_pipeline_none_update_set_instance_data_uniform_buffer_binding *>(this->m_gbuffer_pipeline_none_update_instance_data_uniform_buffer->get_host_memory_range_base());
		for (uint32_t instance_index = 0; instance_index < scene_instance_count; ++instance_index)
		{
			DirectX::XMStoreFloat4x4(&gbuffer_pipeline_none_update_set_instance_data_uniform_buffer_binding_destination->g_instance_data[instance_index].m_model_transform, model_transforms[instance_index]);
			gbuffer_pipeline_none_update_set_instance_data_uniform_buffer_binding_destination->g_instance_data[instance_index].m_geometry_data_index_offset = instance_index;
		}
	}

	// Build Acceleration Structure Pass
	{
		command_buffer->begin_debug_utils_label("Build Acceleration Structure Pass");

		for (uint32_t instance_index = 0U; instance_index < scene_instance_count; ++instance_index)
		{
			BRX_TOP_LEVEL_ACCELERATION_STRUCTURE_INSTANCE top_level_acceleration_structure_instance = {
				{},
				instance_index,
				0XFFU,
				true,
				false,
				false,
				false,
				this->m_scene_compacted_bottom_level_acceleration_structures[instance_index]};

			DirectX::XMStoreFloat3x4(reinterpret_cast<DirectX::XMFLOAT3X4 *>(&top_level_acceleration_structure_instance.transform_matrix), model_transforms[instance_index]);

			this->m_scene_top_level_acceleration_structure_instance_upload_buffer->write_instance(instance_index, &top_level_acceleration_structure_instance);
		}

		command_buffer->update_top_level_acceleration_structure(this->m_scene_top_level_acceleration_structure, this->m_scene_top_level_acceleration_structure_instance_upload_buffer, this->m_scene_top_level_acceleration_structure_update_scratch_buffer);

		command_buffer->acceleration_structure_pass_store_top_level(this->m_scene_top_level_acceleration_structure);

		command_buffer->end_debug_utils_label();
	}

	// GBuffer Pass
	{
		command_buffer->begin_debug_utils_label("GBuffer Pass");

		command_buffer->compute_pass_load_storage_image(this->m_gbuffer_depth_image, BRX_STORAGE_IMAGE_LOAD_OPERATION_DONT_CARE);
		command_buffer->compute_pass_load_storage_image(this->m_gbuffer_normal_image, BRX_STORAGE_IMAGE_LOAD_OPERATION_DONT_CARE);

		command_buffer->bind_compute_pipeline(this->m_gbuffer_pipeline);

		brx_descriptor_set *const descritor_sets[] = {
			this->m_gbuffer_pipeline_none_update_descriptor_set};
		uint32_t const dynamic_offsets[] = {
			0U,
			0U,
			0U,
		};
		command_buffer->bind_compute_descriptor_sets(this->m_gbuffer_pipeline_layout, sizeof(descritor_sets) / sizeof(descritor_sets[0]), descritor_sets, sizeof(dynamic_offsets) / sizeof(dynamic_offsets[0]), dynamic_offsets);

		command_buffer->dispatch(this->m_screen_width, this->m_screen_height, 1U);

		command_buffer->compute_pass_store_storage_image(this->m_gbuffer_depth_image, BRX_STORAGE_IMAGE_STORE_OPERATION_FLUSH_FOR_SAMPLED_IMAGE);
		command_buffer->compute_pass_store_storage_image(this->m_gbuffer_normal_image, BRX_STORAGE_IMAGE_STORE_OPERATION_FLUSH_FOR_SAMPLED_IMAGE);

		command_buffer->end_debug_utils_label();
	}

	// Ambient Occlusion Pass
	{
		command_buffer->begin_debug_utils_label("Ambient Occlusion Pass");

		command_buffer->compute_pass_load_storage_image(this->m_ambient_occlusion_image, BRX_STORAGE_IMAGE_LOAD_OPERATION_DONT_CARE);

		command_buffer->bind_compute_pipeline(this->m_ambient_occlusion_pipeline);

		brx_descriptor_set *const descritor_sets[] = {
			this->m_ambient_occlusion_pipeline_none_update_escriptor_set};
		uint32_t const dynamic_offsets[] = {
			0U};
		command_buffer->bind_compute_descriptor_sets(this->m_ambient_occlusion_pipeline_layout, sizeof(descritor_sets) / sizeof(descritor_sets[0]), descritor_sets, sizeof(dynamic_offsets) / sizeof(dynamic_offsets[0]), dynamic_offsets);

		command_buffer->dispatch(this->m_screen_width, this->m_screen_height, 1U);

		command_buffer->compute_pass_store_storage_image(this->m_ambient_occlusion_image, BRX_STORAGE_IMAGE_STORE_OPERATION_FLUSH_FOR_SAMPLED_IMAGE);

		command_buffer->end_debug_utils_label();
	}
}

static inline DirectX::XMMATRIX XM_CALLCONV DirectX_Math_Matrix_PerspectiveFovRH_ReversedZ(float FovAngleY, float AspectRatio, float NearZ, float FarZ)
{
	// [Reversed-Z](https://developer.nvidia.com/content/depth-precision-visualized)
	//
	// _  0  0  0
	// 0  _  0  0
	// 0  0  b -1
	// 0  0  a  0
	//
	// _  0  0  0
	// 0  _  0  0
	// 0  0 zb  -z
	// 0  0  a
	//
	// z' = -b - a/z
	//
	// Standard
	// 0 = -b + a/nearz // z=-nearz
	// 1 = -b + a/farz  // z=-farz
	// a = farz*nearz/(nearz - farz)
	// b = farz/(nearz - farz)
	//
	// Reversed-Z
	// 1 = -b + a/nearz // z=-nearz
	// 0 = -b + a/farz  // z=-farz
	// a = farz*nearz/(farz - nearz)
	// b = nearz/(farz - nearz)

	// __m128 _mm_shuffle_ps(__m128 lo,__m128 hi, _MM_SHUFFLE(hi3,hi2,lo1,lo0))
	// Interleave inputs into low 2 floats and high 2 floats of output. Basically
	// out[0]=lo[lo0];
	// out[1]=lo[lo1];
	// out[2]=hi[hi2];
	// out[3]=hi[hi3];

	// DirectX::XMMatrixPerspectiveFovRH

	float SinFov;
	float CosFov;
	DirectX::XMScalarSinCos(&SinFov, &CosFov, 0.5F * FovAngleY);

	float Height = CosFov / SinFov;
	float Width = Height / AspectRatio;
	float b = NearZ / (FarZ - NearZ);
	float a = (FarZ / (FarZ - NearZ)) * NearZ;
#if defined(_XM_SSE_INTRINSICS_)
	// Note: This is recorded on the stack
	DirectX::XMVECTOR rMem = {
		Width,
		Height,
		b,
		a};

	// Copy from memory to SSE register
	DirectX::XMVECTOR vValues = rMem;
	DirectX::XMVECTOR vTemp = _mm_setzero_ps();
	// Copy x only
	vTemp = _mm_move_ss(vTemp, vValues);
	// CosFov / SinFov,0,0,0
	DirectX::XMMATRIX M;
	M.r[0] = vTemp;
	// 0,Height / AspectRatio,0,0
	vTemp = vValues;
	vTemp = _mm_and_ps(vTemp, DirectX::g_XMMaskY);
	M.r[1] = vTemp;
	// x=b,y=a,0,-1.0f
	vTemp = _mm_setzero_ps();
	vValues = _mm_shuffle_ps(vValues, DirectX::g_XMNegIdentityR3, _MM_SHUFFLE(3, 2, 3, 2));
	// 0,0,b,-1.0f
	vTemp = _mm_shuffle_ps(vTemp, vValues, _MM_SHUFFLE(3, 0, 0, 0));
	M.r[2] = vTemp;
	// 0,0,a,0.0f
	vTemp = _mm_shuffle_ps(vTemp, vValues, _MM_SHUFFLE(2, 1, 0, 0));
	M.r[3] = vTemp;
	return M;
#elif defined(_XM_ARM_NEON_INTRINSICS_)
	const float32x4_t Zero = vdupq_n_f32(0);

	DirectX::XMMATRIX M;
	M.r[0] = vsetq_lane_f32(Width, Zero, 0);
	M.r[1] = vsetq_lane_f32(Height, Zero, 1);
	M.r[2] = vsetq_lane_f32(b, DirectX::g_XMNegIdentityR3.v, 2);
	M.r[3] = vsetq_lane_f32(a, Zero, 2);
	return M;
#endif
}
