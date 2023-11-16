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
#include "support/utils_align_up.h"
#include "support/camera_controller.h"
#include "../shaders/octahedron_mapping.sli"
#include "../shaders/geometric_buffer_pipeline_layout.sli"
#include "../shaders/ambient_occlusion_pipeline_layout.sli"
#include "../assets/plane.h"
#include "../assets/house.h"

static inline uint32_t linear_allocate(uint32_t &buffer_current, uint32_t buffer_end, uint32_t size, uint32_t alignment);

static inline DirectX::XMMATRIX XM_CALLCONV DirectX_Math_Matrix_PerspectiveFovRH_ReversedZ(float FovAngleY, float AspectRatio, float NearZ, float FarZ);

static float g_camera_fov = -1.0F;

Demo::Demo()
{

}

void Demo::init(pal_device *device, pal_upload_ring_buffer const *global_upload_ring_buffer)
{
	// Descriptor Layout - Create
	pal_descriptor_set_layout *geometric_buffer_global_descriptor_set_layout = NULL;
	pal_descriptor_set_layout *ambient_occlusion_global_descriptor_set_layout = NULL;
	pal_descriptor_set_layout *ambient_occlusion_scene_descriptor_set_layout = NULL;
	{
		PAL_DESCRIPTOR_SET_LAYOUT_BINDING geometric_buffer_global_descriptor_set_layout_bindings[2] = {
			// batch
			{0U, PAL_DESCRIPTOR_TYPE_DYNAMIC_UNIFORM_BUFFER, 1U},
			// draw
			{1U, PAL_DESCRIPTOR_TYPE_DYNAMIC_UNIFORM_BUFFER, 1U}};
		geometric_buffer_global_descriptor_set_layout = device->create_descriptor_set_layout(sizeof(geometric_buffer_global_descriptor_set_layout_bindings) / sizeof(geometric_buffer_global_descriptor_set_layout_bindings[0]), geometric_buffer_global_descriptor_set_layout_bindings);

		pal_descriptor_set_layout *const geometric_buffer_descriptor_set_layouts[1] = {geometric_buffer_global_descriptor_set_layout};
		this->m_geometric_buffer_pipeline_layout = device->create_pipeline_layout(sizeof(geometric_buffer_descriptor_set_layouts) / sizeof(geometric_buffer_descriptor_set_layouts[0]), geometric_buffer_descriptor_set_layouts);

		PAL_DESCRIPTOR_SET_LAYOUT_BINDING ambient_occlusion_global_descriptor_set_layout_bindings[4] = {
			// batch
			{0U, PAL_DESCRIPTOR_TYPE_DYNAMIC_UNIFORM_BUFFER, 1U},
			// texture and sampler
			{1U, PAL_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 2U},
			{2U, PAL_DESCRIPTOR_TYPE_SAMPLER, 1U},
			// write only texture
			{3U, PAL_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1U}};
		ambient_occlusion_global_descriptor_set_layout = device->create_descriptor_set_layout(sizeof(ambient_occlusion_global_descriptor_set_layout_bindings) / sizeof(ambient_occlusion_global_descriptor_set_layout_bindings[0]), ambient_occlusion_global_descriptor_set_layout_bindings);

		PAL_DESCRIPTOR_SET_LAYOUT_BINDING ambient_occlusion_scene_descriptor_set_layout_bindings[1] = {
			{0U, PAL_DESCRIPTOR_TYPE_TOP_LEVEL_ACCELERATION_STRUCTURE, 1U}};
		ambient_occlusion_scene_descriptor_set_layout = device->create_descriptor_set_layout(sizeof(ambient_occlusion_scene_descriptor_set_layout_bindings) / sizeof(ambient_occlusion_scene_descriptor_set_layout_bindings[0]), ambient_occlusion_scene_descriptor_set_layout_bindings);

		pal_descriptor_set_layout *const ambient_occlusion_descriptor_set_layouts[2] = {ambient_occlusion_global_descriptor_set_layout, ambient_occlusion_scene_descriptor_set_layout};
		this->m_ambient_occlusion_pipeline_layout = device->create_pipeline_layout(sizeof(ambient_occlusion_descriptor_set_layouts) / sizeof(ambient_occlusion_descriptor_set_layouts[0]), ambient_occlusion_descriptor_set_layouts);
	}

	constexpr PAL_COLOR_ATTACHMENT_IMAGE_FORMAT const geometric_buffer_a_format = PAL_COLOR_ATTACHMENT_FORMAT_R16G16_UNORM;

	// Render Pass and Pipeline
	{
		// Render Pass
		{
			PAL_RENDER_PASS_COLOR_ATTACHMENT color_attachments[1] = {
				{geometric_buffer_a_format,
				 PAL_RENDER_PASS_COLOR_ATTACHMENT_LOAD_OPERATION_CLEAR,
				 PAL_RENDER_PASS_COLOR_ATTACHMENT_STORE_OPERATION_FLUSH_FOR_SAMPLED_IMAGE}};

			PAL_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT depth_stencil_attachment = {

				device->get_depth_attachment_image_format(),
				PAL_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_LOAD_OPERATION_CLEAR,
				PAL_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_STORE_OPERATION_FLUSH_FOR_SAMPLED_IMAGE

			};

			this->m_geometric_buffer_render_pass = device->create_render_pass(sizeof(color_attachments) / sizeof(color_attachments[0]), color_attachments, &depth_stencil_attachment);
		}

		// Pipeline
		{
#if defined(USE_D3D12) && USE_D3D12
			typedef uint8_t BYTE;
#include "../dxbc/geometric_buffer_vertex.inl"
#include "../dxbc/geometric_buffer_fragment.inl"
#elif defined(USE_VK) && USE_VK
			constexpr uint32_t const geometric_buffer_vertex_shader_module_code[] = {
#include "../spirv/geometric_buffer_vertex.inl"
			};

			constexpr uint32_t const geometric_buffer_fragment_shader_module_code[] = {
#include "../spirv/geometric_buffer_fragment.inl"
			};
#else
#error Unknown Backend
#endif

			PAL_GRAPHICS_PIPELINE_VERTEX_BINDING vertex_bindings[2] = {
				// Position
				{sizeof(float) * 3U},
				// Varying
				{sizeof(uint16_t) * 2U}};

			PAL_GRAPHICS_PIPELINE_VERTEX_ATTRIBUTE vertex_attributes[2] = {
				// Position
				{
					0U,
					PAL_GRAPHICS_PIPELINE_VERTEX_ATTRIBUTE_FORMAT_R32G32B32_SFLOAT,
					0U},
				// Normal Positive Rectangle Space
				{
					1U,
					PAL_GRAPHICS_PIPELINE_VERTEX_ATTRIBUTE_FORMAT_R16G16_UNORM,
					0U}};

			this->m_geometric_buffer_pipeline = device->create_graphics_pipeline(this->m_geometric_buffer_render_pass, this->m_geometric_buffer_pipeline_layout, sizeof(geometric_buffer_vertex_shader_module_code), geometric_buffer_vertex_shader_module_code, sizeof(geometric_buffer_fragment_shader_module_code), geometric_buffer_fragment_shader_module_code, sizeof(vertex_bindings) / sizeof(vertex_bindings[0]), vertex_bindings, sizeof(vertex_attributes) / sizeof(vertex_attributes[0]), vertex_attributes, true, PAL_GRAPHICS_PIPELINE_COMPARE_OPERATION_GREATER);
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

	// Sampler
	this->m_linear_sampler = device->create_sampler(PAL_SAMPLER_FILTER_LINEAR);

	// Init Intermediate Images
	this->m_screen_width = 512U;
	this->m_screen_height = 512U;
	this->m_geometric_buffer_a_image = device->create_color_attachment_image(geometric_buffer_a_format, this->m_screen_width, this->m_screen_height, true);
	this->m_geometric_buffer_depth_image = device->create_depth_stencil_attachment_image(device->get_depth_attachment_image_format(), this->m_screen_width, this->m_screen_height, true);
	this->m_ambient_occlusion_image = device->create_storage_image(PAL_STORAGE_IMAGE_FORMAT_R16G16B16A16_SFLOAT, this->m_screen_width, this->m_screen_height, true);

	// Framebuffer
	{
		this->m_geometric_buffer_frame_buffer = device->create_frame_buffer(this->m_geometric_buffer_render_pass, this->m_screen_width, this->m_screen_height, 1U, &this->m_geometric_buffer_a_image, this->m_geometric_buffer_depth_image);
	}

	// Descriptor - Global
	{
		this->m_geometric_buffer_global_descriptor_set = device->create_descriptor_set(geometric_buffer_global_descriptor_set_layout);

		constexpr uint32_t const geometric_buffer_dynamic_uniform_buffers_range[2] = {
			sizeof(geometric_buffer_pipeline_layout_global_set_per_batch_constant_buffer_binding),
			sizeof(geometric_buffer_pipeline_layout_global_set_per_draw_constant_buffer_binding)};
		device->write_descriptor_set(this->m_geometric_buffer_global_descriptor_set, PAL_DESCRIPTOR_TYPE_DYNAMIC_UNIFORM_BUFFER, 0U, 0U, 1U, &global_upload_ring_buffer, &geometric_buffer_dynamic_uniform_buffers_range[0], NULL, NULL, NULL, NULL);
		device->write_descriptor_set(this->m_geometric_buffer_global_descriptor_set, PAL_DESCRIPTOR_TYPE_DYNAMIC_UNIFORM_BUFFER, 1U, 0U, 1U, &global_upload_ring_buffer, &geometric_buffer_dynamic_uniform_buffers_range[1], NULL, NULL, NULL, NULL);

		this->m_ambient_occlusion_global_descriptor_set = device->create_descriptor_set(ambient_occlusion_global_descriptor_set_layout);

		constexpr uint32_t const ambient_occlusion_dynamic_uniform_buffers_range[1] = {
			sizeof(ambient_occlusion_pipeline_layout_global_set_per_batch_constant_buffer_binding)};
		device->write_descriptor_set(this->m_ambient_occlusion_global_descriptor_set, PAL_DESCRIPTOR_TYPE_DYNAMIC_UNIFORM_BUFFER, 0U, 0U, 1U, &global_upload_ring_buffer, &ambient_occlusion_dynamic_uniform_buffers_range[0], NULL, NULL, NULL, NULL);

		pal_sampled_image const *const ambient_occlusion_sampled_images[2] = {
			this->m_geometric_buffer_a_image->get_sampled_image(),
			this->m_geometric_buffer_depth_image->get_sampled_image()};
		device->write_descriptor_set(this->m_ambient_occlusion_global_descriptor_set, PAL_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1U, 0U, 2U, NULL, NULL, ambient_occlusion_sampled_images, NULL, NULL, NULL);

		device->write_descriptor_set(this->m_ambient_occlusion_global_descriptor_set, PAL_DESCRIPTOR_TYPE_SAMPLER, 2U, 0U, 1U, NULL, NULL, NULL, &this->m_linear_sampler, NULL, NULL);

		pal_storage_image const *const ambient_occlusion_storage_image = this->m_ambient_occlusion_image;
		device->write_descriptor_set(this->m_ambient_occlusion_global_descriptor_set, PAL_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3U, 0U, 1U, NULL, NULL, NULL, NULL, &ambient_occlusion_storage_image, NULL);
	}

	// Assets
	{
		// NOTE: user can custum size
		uint32_t const staging_buffer_size = 320U * 1024U * 1024U;

		pal_staging_buffer *const staging_buffer = device->create_staging_buffer(staging_buffer_size);
		void *const staging_buffer_base = staging_buffer->get_memory_range_base();
		uint32_t staging_buffer_current = 7U;
		uint32_t staging_buffer_end = staging_buffer_size;

		// NOTE: user can custum size
		uint32_t const scratch_buffer_size = 64U * 1024U * 1024U;
		uint32_t const staging_non_compacted_bottom_level_acceleration_structure_buffer_size = 64U * 1024U * 1024U;
		uint32_t const compacted_bottom_level_acceleration_structure_size_query_pool_size = 320U;
		uint32_t const top_level_acceleration_structure_instance_buffer_size = 320U;

		pal_scratch_buffer *const scratch_buffer = device->create_scratch_buffer(scratch_buffer_size);
		uint32_t scratch_buffer_current = 0U;
		uint32_t scratch_buffer_end = scratch_buffer_size;
		uint32_t const scratch_buffer_offset_alignment = device->get_scratch_buffer_offset_alignment();

		pal_staging_non_compacted_bottom_level_acceleration_structure_buffer *const staging_non_compacted_bottom_level_acceleration_structure_buffer = device->create_staging_non_compacted_bottom_level_acceleration_structure_buffer(staging_non_compacted_bottom_level_acceleration_structure_buffer_size);
		uint32_t staging_non_compacted_bottom_level_acceleration_structure_buffer_current = 0U;
		uint32_t staging_non_compacted_bottom_level_acceleration_structure_buffer_end = staging_non_compacted_bottom_level_acceleration_structure_buffer_size;
		uint32_t const staging_non_compacted_bottom_level_acceleration_structure_buffer_offset_alignment = device->get_staging_non_compacted_bottom_level_acceleration_structure_buffer_offset_alignment();

		pal_compacted_bottom_level_acceleration_structure_size_query_pool *const compacted_bottom_level_acceleration_structure_size_query_pool = device->create_compacted_bottom_level_acceleration_structure_size_query_pool(compacted_bottom_level_acceleration_structure_size_query_pool_size);
		uint32_t compacted_bottom_level_acceleration_structure_size_query_pool_current = 0U;
		uint32_t compacted_bottom_level_acceleration_structure_size_query_pool_end = compacted_bottom_level_acceleration_structure_size_query_pool_size;

		pal_top_level_acceleration_structure_instance_buffer *const top_level_acceleration_structure_instance_buffer = device->create_top_level_acceleration_structure_instance_buffer(top_level_acceleration_structure_instance_buffer_size);
		uint32_t top_level_acceleration_structure_instance_buffer_current = 0U;
		uint32_t top_level_acceleration_structure_instance_buffer_end = top_level_acceleration_structure_instance_buffer_size;

		pal_upload_command_buffer *const upload_command_buffer = device->create_upload_command_buffer();

		pal_graphics_command_buffer *const graphics_command_buffer = device->create_graphics_command_buffer();

		pal_upload_queue *const upload_queue = device->create_upload_queue();

		pal_graphics_queue *const graphics_queue = device->create_graphics_queue();

		pal_fence *const fence = device->create_fence(true);

		device->reset_upload_command_buffer(upload_command_buffer);

		device->reset_graphics_command_buffer(graphics_command_buffer);

		upload_command_buffer->begin();

		graphics_command_buffer->begin();

		// plane vertex position buffer
		{
			this->m_plane_vertex_position_buffer = device->create_asset_vertex_position_buffer(sizeof(g_plane_vertex_position));

			uint32_t plane_vertex_position_staging_buffer_offset = linear_allocate(staging_buffer_current, staging_buffer_end, sizeof(g_plane_vertex_position), 1U);

			// write to staging buffer
			std::memcpy(reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(staging_buffer_base) + plane_vertex_position_staging_buffer_offset), g_plane_vertex_position, sizeof(g_plane_vertex_position));

			upload_command_buffer->upload_from_staging_buffer_to_asset_vertex_position_buffer(this->m_plane_vertex_position_buffer, 0U, staging_buffer, plane_vertex_position_staging_buffer_offset, sizeof(g_plane_vertex_position));
		}

		// plane vertex varying buffer
		{
			// convert
			uint32_t converted_vertex_normal[g_plane_vertex_count];
			for (int i = 0; i < g_plane_vertex_count; ++i)
			{
				DirectX::XMFLOAT2 const position_rectangle_space = octahedron_map(DirectX::XMFLOAT3(g_plane_vertex_normal[3U * i], g_plane_vertex_normal[3U * i + 1U], g_plane_vertex_normal[3U * i + 2U]));

#ifndef NDEBUG
				DirectX::XMFLOAT3 const position_sphere_space = octahedron_unmap(position_rectangle_space);
#endif
				assert(std::abs(position_sphere_space.x - g_plane_vertex_normal[3U * i]) < 0.00000075F);
				assert(std::abs(position_sphere_space.y - g_plane_vertex_normal[3U * i + 1U]) < 0.00000075F);
				assert(std::abs(position_sphere_space.z - g_plane_vertex_normal[3U * i + 2U]) < 0.00000075F);

				DirectX::XMFLOAT2 const position_positive_rectangle_space = DirectX::XMFLOAT2((position_rectangle_space.x + 1.0F) * 0.5F, (position_rectangle_space.y + 1.0F) * 0.5F);

				DirectX::PackedVector::XMUSHORTN2 packed_vector;
				DirectX::PackedVector::XMStoreUShortN2(&packed_vector, DirectX::XMLoadFloat2(&position_positive_rectangle_space));

				converted_vertex_normal[i] = packed_vector.v;
			}

			this->m_plane_vertex_varying_buffer = device->create_asset_vertex_varying_buffer(sizeof(converted_vertex_normal));

			uint32_t const vertex_varying_staging_buffer_offset = linear_allocate(staging_buffer_current, staging_buffer_end, sizeof(converted_vertex_normal), 1U);

			// write to staging buffer
			std::memcpy(reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(staging_buffer_base) + vertex_varying_staging_buffer_offset), converted_vertex_normal, sizeof(converted_vertex_normal));

			upload_command_buffer->upload_from_staging_buffer_to_asset_vertex_varying_buffer(this->m_plane_vertex_varying_buffer, 0U, staging_buffer, vertex_varying_staging_buffer_offset, sizeof(converted_vertex_normal));
		}

		// plane index buffer
		{
			this->m_plane_index_buffer = device->create_asset_index_buffer(sizeof(g_plane_index));

			uint32_t plane_index_staging_buffer_offset = linear_allocate(staging_buffer_current, staging_buffer_end, sizeof(g_plane_index), 1U);

			// write to staging buffer
			std::memcpy(reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(staging_buffer_base) + plane_index_staging_buffer_offset), g_plane_index, sizeof(g_plane_index));

			upload_command_buffer->upload_from_staging_buffer_to_asset_index_buffer(this->m_plane_index_buffer, 0U, staging_buffer, plane_index_staging_buffer_offset, sizeof(g_plane_index));
		}

		// plane non compacted bottom level acceleration structure
		pal_staging_non_compacted_bottom_level_acceleration_structure *plane_staging_non_compacted_bottom_level_acceleration_structure = NULL;
		uint32_t plane_compacted_bottom_level_acceleration_structure_size_query_index = -1;
		{
			uint32_t plane_staging_non_compacted_bottom_level_acceleration_structure_size = -1;
			uint32_t plane_build_scratch_size = -1;
			PAL_BOTTOM_LEVEL_ACCELERATION_STRUCTURE_GEOMETRY const plane_bottom_level_acceleration_structure_geometries[1] = {
				{true,
				 PAL_GRAPHICS_PIPELINE_VERTEX_ATTRIBUTE_FORMAT_R32G32B32_SFLOAT,
				 sizeof(float) * 3U,
				 g_plane_vertex_count,
				 this->m_plane_vertex_position_buffer,
				 PAL_GRAPHICS_PIPELINE_INDEX_TYPE_UINT16,
				 g_plane_index_count,
				 this->m_plane_index_buffer}};
			device->get_staging_non_compacted_bottom_level_acceleration_structure_size(sizeof(plane_bottom_level_acceleration_structure_geometries) / sizeof(plane_bottom_level_acceleration_structure_geometries[0]), &plane_bottom_level_acceleration_structure_geometries[0], &plane_staging_non_compacted_bottom_level_acceleration_structure_size, &plane_build_scratch_size);

			uint32_t const plane_staging_non_compacted_bottom_level_acceleration_structure_buffer_offset = linear_allocate(staging_non_compacted_bottom_level_acceleration_structure_buffer_current, staging_non_compacted_bottom_level_acceleration_structure_buffer_end, plane_staging_non_compacted_bottom_level_acceleration_structure_size, staging_non_compacted_bottom_level_acceleration_structure_buffer_offset_alignment);

			uint32_t const plane_scratch_buffer_offset = linear_allocate(scratch_buffer_current, scratch_buffer_end, plane_build_scratch_size, scratch_buffer_offset_alignment);

			plane_staging_non_compacted_bottom_level_acceleration_structure = device->create_staging_non_compacted_bottom_level_acceleration_structure(staging_non_compacted_bottom_level_acceleration_structure_buffer, plane_staging_non_compacted_bottom_level_acceleration_structure_buffer_offset, plane_staging_non_compacted_bottom_level_acceleration_structure_size);

			plane_compacted_bottom_level_acceleration_structure_size_query_index = compacted_bottom_level_acceleration_structure_size_query_pool_current;
			++compacted_bottom_level_acceleration_structure_size_query_pool_current;
			assert(compacted_bottom_level_acceleration_structure_size_query_pool_current < compacted_bottom_level_acceleration_structure_size_query_pool_end);

			upload_command_buffer->build_staging_non_compacted_bottom_level_acceleration_structure(scratch_buffer, plane_scratch_buffer_offset, plane_staging_non_compacted_bottom_level_acceleration_structure, sizeof(plane_bottom_level_acceleration_structure_geometries) / sizeof(plane_bottom_level_acceleration_structure_geometries[0]), &plane_bottom_level_acceleration_structure_geometries[0], compacted_bottom_level_acceleration_structure_size_query_pool, plane_compacted_bottom_level_acceleration_structure_size_query_index);
		}

		// house vertex position buffer
		{
			this->m_house_vertex_position_buffer = device->create_asset_vertex_position_buffer(sizeof(g_house_vertex_position));

			uint32_t house_vertex_position_staging_buffer_offset = linear_allocate(staging_buffer_current, staging_buffer_end, sizeof(g_house_vertex_position), 1U);

			// write to staging buffer
			std::memcpy(reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(staging_buffer_base) + house_vertex_position_staging_buffer_offset), g_house_vertex_position, sizeof(g_house_vertex_position));

			upload_command_buffer->upload_from_staging_buffer_to_asset_vertex_position_buffer(this->m_house_vertex_position_buffer, 0U, staging_buffer, house_vertex_position_staging_buffer_offset, sizeof(g_house_vertex_position));
		}

		// house vertex varying buffer
		{
			// convert
			uint32_t converted_vertex_normal[g_house_vertex_count];

			for (int i = 0; i < g_house_vertex_count; ++i)
			{
				DirectX::XMFLOAT2 const position_rectangle_space = octahedron_map(DirectX::XMFLOAT3(g_house_vertex_normal[3U * i], g_house_vertex_normal[3U * i + 1U], g_house_vertex_normal[3U * i + 2U]));

#ifndef NDEBUG
				DirectX::XMFLOAT3 const position_sphere_space = octahedron_unmap(position_rectangle_space);
#endif
				assert(std::abs(position_sphere_space.x - g_house_vertex_normal[3U * i]) < 0.00000075F);
				assert(std::abs(position_sphere_space.y - g_house_vertex_normal[3U * i + 1U]) < 0.00000075F);
				assert(std::abs(position_sphere_space.z - g_house_vertex_normal[3U * i + 2U]) < 0.00000075F);

				DirectX::XMFLOAT2 const position_positive_rectangle_space = DirectX::XMFLOAT2((position_rectangle_space.x + 1.0F) * 0.5F, (position_rectangle_space.y + 1.0F) * 0.5F);

				DirectX::PackedVector::XMUSHORTN2 packed_vector;
				DirectX::PackedVector::XMStoreUShortN2(&packed_vector, DirectX::XMLoadFloat2(&position_positive_rectangle_space));

				converted_vertex_normal[i] = packed_vector.v;
			}

			this->m_house_vertex_varying_buffer = device->create_asset_vertex_varying_buffer(sizeof(converted_vertex_normal));

			uint32_t vertex_varying_staging_buffer_offset = linear_allocate(staging_buffer_current, staging_buffer_end, sizeof(converted_vertex_normal), 1U);

			// write to staging buffer
			std::memcpy(reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(staging_buffer_base) + vertex_varying_staging_buffer_offset), converted_vertex_normal, sizeof(converted_vertex_normal));

			upload_command_buffer->upload_from_staging_buffer_to_asset_vertex_varying_buffer(this->m_house_vertex_varying_buffer, 0U, staging_buffer, vertex_varying_staging_buffer_offset, sizeof(converted_vertex_normal));
		}

		// house index buffer
		{
			this->m_house_index_buffer = device->create_asset_index_buffer(sizeof(g_house_index));

			uint32_t house_index_staging_buffer_offset = linear_allocate(staging_buffer_current, staging_buffer_end, sizeof(g_house_index), 1U);

			// write to staging buffer
			std::memcpy(reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(staging_buffer_base) + house_index_staging_buffer_offset), g_house_index, sizeof(g_house_index));

			upload_command_buffer->upload_from_staging_buffer_to_asset_index_buffer(this->m_house_index_buffer, 0U, staging_buffer, house_index_staging_buffer_offset, sizeof(g_house_index));
		}

		// house non compacted bottom level acceleration structure
		pal_staging_non_compacted_bottom_level_acceleration_structure *house_staging_non_compacted_bottom_level_acceleration_structure = NULL;
		uint32_t house_compacted_bottom_level_acceleration_structure_size_query_index = -1;
		{
			uint32_t house_staging_non_compacted_bottom_level_acceleration_structure_size = -1;
			uint32_t house_build_scratch_size = -1;
			PAL_BOTTOM_LEVEL_ACCELERATION_STRUCTURE_GEOMETRY const house_bottom_level_acceleration_structure_geometries[1] = {
				{true,
				 PAL_GRAPHICS_PIPELINE_VERTEX_ATTRIBUTE_FORMAT_R32G32B32_SFLOAT,
				 sizeof(float) * 3U,
				 g_house_vertex_count,
				 this->m_house_vertex_position_buffer,
				 PAL_GRAPHICS_PIPELINE_INDEX_TYPE_UINT16,
				 g_house_index_count,
				 this->m_house_index_buffer}};
			device->get_staging_non_compacted_bottom_level_acceleration_structure_size(sizeof(house_bottom_level_acceleration_structure_geometries) / sizeof(house_bottom_level_acceleration_structure_geometries[0]), &house_bottom_level_acceleration_structure_geometries[0], &house_staging_non_compacted_bottom_level_acceleration_structure_size, &house_build_scratch_size);

			uint32_t const house_staging_non_compacted_bottom_level_acceleration_structure_buffer_offset = linear_allocate(staging_non_compacted_bottom_level_acceleration_structure_buffer_current, staging_non_compacted_bottom_level_acceleration_structure_buffer_end, house_staging_non_compacted_bottom_level_acceleration_structure_size, staging_non_compacted_bottom_level_acceleration_structure_buffer_offset_alignment);

			uint32_t const house_scratch_buffer_offset = linear_allocate(scratch_buffer_current, scratch_buffer_end, house_build_scratch_size, scratch_buffer_offset_alignment);

			house_staging_non_compacted_bottom_level_acceleration_structure = device->create_staging_non_compacted_bottom_level_acceleration_structure(staging_non_compacted_bottom_level_acceleration_structure_buffer, house_staging_non_compacted_bottom_level_acceleration_structure_buffer_offset, house_staging_non_compacted_bottom_level_acceleration_structure_size);

			house_compacted_bottom_level_acceleration_structure_size_query_index = compacted_bottom_level_acceleration_structure_size_query_pool_current;
			++compacted_bottom_level_acceleration_structure_size_query_pool_current;
			assert(compacted_bottom_level_acceleration_structure_size_query_pool_current < compacted_bottom_level_acceleration_structure_size_query_pool_end);

			upload_command_buffer->build_staging_non_compacted_bottom_level_acceleration_structure(scratch_buffer, house_scratch_buffer_offset, house_staging_non_compacted_bottom_level_acceleration_structure, sizeof(house_bottom_level_acceleration_structure_geometries) / sizeof(house_bottom_level_acceleration_structure_geometries[0]), &house_bottom_level_acceleration_structure_geometries[0], compacted_bottom_level_acceleration_structure_size_query_pool, house_compacted_bottom_level_acceleration_structure_size_query_index);
		}

		// release and acquire
		{
			upload_command_buffer->release_asset_vertex_position_buffer(this->m_plane_vertex_position_buffer);

			upload_command_buffer->release_asset_vertex_varying_buffer(this->m_plane_vertex_varying_buffer);

			upload_command_buffer->release_asset_index_buffer(this->m_plane_index_buffer);

			upload_command_buffer->release_asset_vertex_position_buffer(this->m_house_vertex_position_buffer);

			upload_command_buffer->release_asset_vertex_varying_buffer(this->m_house_vertex_varying_buffer);

			upload_command_buffer->release_asset_index_buffer(this->m_house_index_buffer);

			graphics_command_buffer->acquire_asset_vertex_position_buffer(this->m_plane_vertex_position_buffer);

			graphics_command_buffer->acquire_asset_vertex_varying_buffer(this->m_plane_vertex_varying_buffer);

			graphics_command_buffer->acquire_asset_index_buffer(this->m_plane_index_buffer);

			graphics_command_buffer->acquire_asset_vertex_position_buffer(this->m_house_vertex_position_buffer);

			graphics_command_buffer->acquire_asset_vertex_varying_buffer(this->m_house_vertex_varying_buffer);

			graphics_command_buffer->acquire_asset_index_buffer(this->m_house_index_buffer);
		}

		upload_command_buffer->end();

		graphics_command_buffer->end();

		upload_queue->submit_and_signal(upload_command_buffer);

		device->reset_fence(fence);

		graphics_queue->wait_and_submit(upload_command_buffer, graphics_command_buffer, fence);

		device->wait_for_fence(fence);

		// start second submit

		device->reset_upload_command_buffer(upload_command_buffer);

		device->reset_graphics_command_buffer(graphics_command_buffer);

		upload_command_buffer->begin();

		graphics_command_buffer->begin();

		// plane compacted bottom level acceleration structure
		{
			uint32_t const plane_compacted_bottom_level_acceleration_structure_size = device->get_compacted_bottom_level_acceleration_structure_size_query_pool_result(compacted_bottom_level_acceleration_structure_size_query_pool, plane_compacted_bottom_level_acceleration_structure_size_query_index);

			this->m_plane_compacted_bottom_level_acceleration_structure = device->create_asset_compacted_bottom_level_acceleration_structure(plane_compacted_bottom_level_acceleration_structure_size);

			upload_command_buffer->compact_bottom_level_acceleration_structure(this->m_plane_compacted_bottom_level_acceleration_structure, plane_staging_non_compacted_bottom_level_acceleration_structure);
		}

		// house compacted bottom level acceleration structure
		{
			uint32_t const house_compacted_bottom_level_acceleration_structure_size = device->get_compacted_bottom_level_acceleration_structure_size_query_pool_result(compacted_bottom_level_acceleration_structure_size_query_pool, house_compacted_bottom_level_acceleration_structure_size_query_index);

			this->m_house_compacted_bottom_level_acceleration_structure = device->create_asset_compacted_bottom_level_acceleration_structure(house_compacted_bottom_level_acceleration_structure_size);

			upload_command_buffer->compact_bottom_level_acceleration_structure(this->m_house_compacted_bottom_level_acceleration_structure, house_staging_non_compacted_bottom_level_acceleration_structure);
		}

		// release and acquire
		{
			upload_command_buffer->release_asset_compacted_bottom_level_acceleration_structure(this->m_plane_compacted_bottom_level_acceleration_structure);

			upload_command_buffer->release_asset_compacted_bottom_level_acceleration_structure(this->m_house_compacted_bottom_level_acceleration_structure);

			graphics_command_buffer->acquire_asset_compacted_bottom_level_acceleration_structure(this->m_plane_compacted_bottom_level_acceleration_structure);

			graphics_command_buffer->acquire_asset_compacted_bottom_level_acceleration_structure(this->m_house_compacted_bottom_level_acceleration_structure);
		}

		// scene top level acceleration structure
		{
			uint32_t const top_level_acceleration_structure_instance_count = 2U;

			uint32_t top_level_acceleration_structure_size = -1;
			uint32_t top_level_acceleration_structure_build_scratch_size = -1;
			this->m_scene_top_level_acceleration_structure_update_scratch_size = -1;
			device->get_top_level_acceleration_structure_size(top_level_acceleration_structure_instance_count, &top_level_acceleration_structure_size, &top_level_acceleration_structure_build_scratch_size, &this->m_scene_top_level_acceleration_structure_update_scratch_size);

			uint32_t const top_level_acceleration_structure_scratch_buffer_offset = linear_allocate(scratch_buffer_current, scratch_buffer_end, top_level_acceleration_structure_build_scratch_size, scratch_buffer_offset_alignment);

			this->m_scene_top_level_acceleration_structure = device->create_top_level_acceleration_structure(top_level_acceleration_structure_size);

			uint32_t const top_level_acceleration_structure_start_instance_index = top_level_acceleration_structure_instance_buffer_current;
			top_level_acceleration_structure_instance_buffer_current += top_level_acceleration_structure_instance_count;
			assert(top_level_acceleration_structure_instance_buffer_current < top_level_acceleration_structure_instance_buffer_end);

			pal_asset_compacted_bottom_level_acceleration_structure *asset_compacted_bottom_level_acceleration_structures[top_level_acceleration_structure_instance_count] = {
				this->m_plane_compacted_bottom_level_acceleration_structure,
				this->m_house_compacted_bottom_level_acceleration_structure};

			for (uint32_t top_level_acceleration_structure_instance_index = 0U; top_level_acceleration_structure_instance_index < top_level_acceleration_structure_instance_count; ++top_level_acceleration_structure_instance_index)
			{
				DirectX::XMFLOAT3X4 transform_matrix;
				DirectX::XMStoreFloat3x4(&transform_matrix, DirectX::XMMatrixTranspose(DirectX::XMMatrixIdentity()));

				device->write_top_level_acceleration_structure_instance_buffer(top_level_acceleration_structure_instance_buffer, top_level_acceleration_structure_start_instance_index + top_level_acceleration_structure_instance_index, transform_matrix.m, false, false, true, false, asset_compacted_bottom_level_acceleration_structures[top_level_acceleration_structure_instance_index]);
			}

			graphics_command_buffer->build_top_level_acceleration_structure(scratch_buffer, top_level_acceleration_structure_scratch_buffer_offset, top_level_acceleration_structure_instance_count, top_level_acceleration_structure_instance_buffer, top_level_acceleration_structure_start_instance_index, this->m_scene_top_level_acceleration_structure);
		}

		upload_command_buffer->end();

		graphics_command_buffer->end();

		upload_queue->submit_and_signal(upload_command_buffer);

		device->reset_fence(fence);

		graphics_queue->wait_and_submit(upload_command_buffer, graphics_command_buffer, fence);

		device->wait_for_fence(fence);

		// destory non compacted bottom level acceleration structure

		device->destroy_staging_non_compacted_bottom_level_acceleration_structure(plane_staging_non_compacted_bottom_level_acceleration_structure);

		device->destroy_staging_non_compacted_bottom_level_acceleration_structure(house_staging_non_compacted_bottom_level_acceleration_structure);

		device->destroy_fence(fence);

		device->destroy_upload_command_buffer(upload_command_buffer);

		device->destroy_graphics_command_buffer(graphics_command_buffer);

		device->destroy_upload_queue(upload_queue);

		device->destroy_graphics_queue(graphics_queue);

		device->destroy_staging_buffer(staging_buffer);

		device->destroy_scratch_buffer(scratch_buffer);

		device->destroy_staging_non_compacted_bottom_level_acceleration_structure_buffer(staging_non_compacted_bottom_level_acceleration_structure_buffer);

		device->destroy_compacted_bottom_level_acceleration_structure_size_query_pool(compacted_bottom_level_acceleration_structure_size_query_pool);

		device->destroy_top_level_acceleration_structure_instance_buffer(top_level_acceleration_structure_instance_buffer);
	}

	// Descriptor - Assets
	{
		this->m_ambient_occlusion_scene_descriptor_set = device->create_descriptor_set(ambient_occlusion_scene_descriptor_set_layout);

		device->write_descriptor_set(this->m_ambient_occlusion_scene_descriptor_set, PAL_DESCRIPTOR_TYPE_TOP_LEVEL_ACCELERATION_STRUCTURE, 0U, 0U, 1U, NULL, NULL, NULL, NULL, NULL, &this->m_scene_top_level_acceleration_structure);
	}

	// Descriptor Layout - Destroy
	device->destroy_descriptor_set_layout(geometric_buffer_global_descriptor_set_layout);
	device->destroy_descriptor_set_layout(ambient_occlusion_global_descriptor_set_layout);
	device->destroy_descriptor_set_layout(ambient_occlusion_scene_descriptor_set_layout);
	geometric_buffer_global_descriptor_set_layout = NULL;
	ambient_occlusion_global_descriptor_set_layout = NULL;
	ambient_occlusion_scene_descriptor_set_layout = NULL;

	// Init Camera
	g_camera_controller.m_eye_position = DirectX::XMFLOAT3(5.0F, 4.0F, -4.0F);
	g_camera_controller.m_eye_direction = DirectX::XMFLOAT3(-5.0F, -3.0F, 4.0F);
	g_camera_controller.m_up_direction = DirectX::XMFLOAT3(0.0F, 1.0F, 0.0F);
	g_camera_fov = 1.047F;
}

void Demo::destroy(pal_device *device)
{
	device->destroy_descriptor_set(this->m_ambient_occlusion_scene_descriptor_set);

	device->destroy_asset_vertex_position_buffer(this->m_plane_vertex_position_buffer);
	device->destroy_asset_vertex_varying_buffer(this->m_plane_vertex_varying_buffer);
	device->destroy_asset_index_buffer(this->m_plane_index_buffer);
	device->destroy_asset_compacted_bottom_level_acceleration_structure(this->m_plane_compacted_bottom_level_acceleration_structure);

	device->destroy_asset_vertex_position_buffer(this->m_house_vertex_position_buffer);
	device->destroy_asset_vertex_varying_buffer(this->m_house_vertex_varying_buffer);
	device->destroy_asset_index_buffer(this->m_house_index_buffer);
	device->destroy_asset_compacted_bottom_level_acceleration_structure(this->m_house_compacted_bottom_level_acceleration_structure);

	device->destroy_top_level_acceleration_structure(this->m_scene_top_level_acceleration_structure);

	device->destroy_descriptor_set(this->m_geometric_buffer_global_descriptor_set);
	device->destroy_descriptor_set(this->m_ambient_occlusion_global_descriptor_set);

	device->destroy_frame_buffer(this->m_geometric_buffer_frame_buffer);

	device->destroy_storage_image(this->m_ambient_occlusion_image);
	device->destroy_color_attachment_image(this->m_geometric_buffer_a_image);
	device->destroy_depth_stencil_attachment_image(this->m_geometric_buffer_depth_image);

	device->destroy_sampler(this->m_linear_sampler);

	device->destroy_compute_pipeline(this->m_ambient_occlusion_pipeline);

	device->destroy_pipeline_layout(this->m_ambient_occlusion_pipeline_layout);

	device->destroy_render_pass(this->m_geometric_buffer_render_pass);
	device->destroy_graphics_pipeline(this->m_geometric_buffer_pipeline);

	device->destroy_pipeline_layout(this->m_geometric_buffer_pipeline_layout);
}

pal_sampled_image const *Demo::get_sampled_image_for_present()
{
	return this->m_ambient_occlusion_image->get_sampled_image();
}

void Demo::draw(pal_graphics_command_buffer *command_buffer, void *upload_ring_buffer_base, uint32_t upload_ring_buffer_current, uint32_t upload_ring_buffer_end, uint32_t upload_ring_buffer_offset_alignment)
{
	// Geometric Buffer Pass
	{
		command_buffer->begin_debug_utils_label("Geometric Buffer Pass");

		float color_clear_values[4] = {0.0F, 0.0F, 0.0F, 0.0F};
		float depth_clear_value = 0.0F;
		command_buffer->begin_render_pass(this->m_geometric_buffer_render_pass, this->m_geometric_buffer_frame_buffer, this->m_screen_width, this->m_screen_height, 1U, &color_clear_values, &depth_clear_value, NULL);

		command_buffer->bind_graphics_pipeline(this->m_geometric_buffer_pipeline);

		command_buffer->set_view_port(this->m_screen_width, this->m_screen_height);

		command_buffer->set_scissor(this->m_screen_width, this->m_screen_height);

		// update per batch constant buffer
		uint32_t const global_set_per_batch_binding_offset = linear_allocate(upload_ring_buffer_current, upload_ring_buffer_end, sizeof(geometric_buffer_pipeline_layout_global_set_per_batch_constant_buffer_binding), upload_ring_buffer_offset_alignment);
		{
			geometric_buffer_pipeline_layout_global_set_per_batch_constant_buffer_binding *const global_set_per_batch_binding = reinterpret_cast<geometric_buffer_pipeline_layout_global_set_per_batch_constant_buffer_binding *>(reinterpret_cast<uintptr_t>(upload_ring_buffer_base) + global_set_per_batch_binding_offset);

			DirectX::XMFLOAT3 eye_position = g_camera_controller.m_eye_position;
			DirectX::XMFLOAT3 eye_direction = g_camera_controller.m_eye_direction;
			DirectX::XMFLOAT3 up_direction = g_camera_controller.m_up_direction;
			DirectX::XMStoreFloat4x4(&global_set_per_batch_binding->g_view_transform, DirectX::XMMatrixLookToRH(DirectX::XMLoadFloat3(&eye_position), DirectX::XMLoadFloat3(&eye_direction), DirectX::XMLoadFloat3(&up_direction)));

			DirectX::XMStoreFloat4x4(&global_set_per_batch_binding->g_projection_transform, DirectX_Math_Matrix_PerspectiveFovRH_ReversedZ(g_camera_fov, static_cast<float>(this->m_screen_width) / static_cast<float>(this->m_screen_height), 0.1F, 1000.0F));
		}

		// update house draw constant buffer
		uint32_t const global_set_house_draw_binding_offset = linear_allocate(upload_ring_buffer_current, upload_ring_buffer_end, sizeof(geometric_buffer_pipeline_layout_global_set_per_draw_constant_buffer_binding), upload_ring_buffer_offset_alignment);
		{
			geometric_buffer_pipeline_layout_global_set_per_draw_constant_buffer_binding *const global_set_house_draw_binding = reinterpret_cast<geometric_buffer_pipeline_layout_global_set_per_draw_constant_buffer_binding *>(reinterpret_cast<uintptr_t>(upload_ring_buffer_base) + global_set_house_draw_binding_offset);

			DirectX::XMStoreFloat4x4(&global_set_house_draw_binding->g_model_coordinate_transform, DirectX::XMMatrixIdentity());
			DirectX::XMStoreFloat3x4(&global_set_house_draw_binding->g_model_normal_transform, DirectX::XMMatrixIdentity());
		}

		// update plane draw constant buffer
		uint32_t const global_set_plane_draw_binding_offset = linear_allocate(upload_ring_buffer_current, upload_ring_buffer_end, sizeof(geometric_buffer_pipeline_layout_global_set_per_draw_constant_buffer_binding), upload_ring_buffer_offset_alignment);
		{
			geometric_buffer_pipeline_layout_global_set_per_draw_constant_buffer_binding *const global_set_plane_draw_binding = reinterpret_cast<geometric_buffer_pipeline_layout_global_set_per_draw_constant_buffer_binding *>(reinterpret_cast<uintptr_t>(upload_ring_buffer_base) + global_set_plane_draw_binding_offset);

			DirectX::XMStoreFloat4x4(&global_set_plane_draw_binding->g_model_coordinate_transform, DirectX::XMMatrixIdentity());
			DirectX::XMStoreFloat3x4(&global_set_plane_draw_binding->g_model_normal_transform, DirectX::XMMatrixIdentity());
		}

		// draw house
		{
			pal_descriptor_set *const house_descritor_sets[1] = {this->m_geometric_buffer_global_descriptor_set};
			uint32_t const house_dynamic_offsets[2] = {global_set_per_batch_binding_offset, global_set_house_draw_binding_offset};
			command_buffer->bind_graphics_descriptor_sets(this->m_geometric_buffer_pipeline_layout, sizeof(house_descritor_sets) / sizeof(house_descritor_sets[0]), house_descritor_sets, sizeof(house_dynamic_offsets) / sizeof(house_dynamic_offsets[0]), house_dynamic_offsets);

			pal_vertex_buffer const *const vertex_buffers[2] = {this->m_house_vertex_position_buffer->get_vertex_buffer(), this->m_house_vertex_varying_buffer->get_vertex_buffer()};
			command_buffer->bind_vertex_buffers(sizeof(vertex_buffers) / sizeof(vertex_buffers[0]), vertex_buffers);

			command_buffer->draw_index(this->m_house_index_buffer, PAL_GRAPHICS_PIPELINE_INDEX_TYPE_UINT16, g_house_index_count, 1U);
		}

		// draw plane
		{
			pal_descriptor_set *const plane_descritor_sets[1] = {this->m_geometric_buffer_global_descriptor_set};
			uint32_t const plane_dynamic_offsets[2] = {global_set_per_batch_binding_offset, global_set_plane_draw_binding_offset};
			command_buffer->bind_graphics_descriptor_sets(this->m_geometric_buffer_pipeline_layout, sizeof(plane_descritor_sets) / sizeof(plane_descritor_sets[0]), plane_descritor_sets, sizeof(plane_dynamic_offsets) / sizeof(plane_dynamic_offsets[0]), plane_dynamic_offsets);

			pal_vertex_buffer const *const vertex_buffers[2] = {this->m_plane_vertex_position_buffer->get_vertex_buffer(), this->m_plane_vertex_varying_buffer->get_vertex_buffer()};
			command_buffer->bind_vertex_buffers(sizeof(vertex_buffers) / sizeof(vertex_buffers[0]), vertex_buffers);

			command_buffer->draw_index(this->m_plane_index_buffer, PAL_GRAPHICS_PIPELINE_INDEX_TYPE_UINT16, g_plane_index_count, 1U);
		}

		command_buffer->end_render_pass();

		command_buffer->end_debug_utils_label();
	}

	// Ambient Occlusion Pass
	{
		command_buffer->begin_debug_utils_label("Ambient Occlusion Pass");

		command_buffer->compute_pass_load_storage_image(this->m_ambient_occlusion_image, PAL_STORAGE_IMAGE_LOAD_OPERATION_DONT_CARE);

		command_buffer->bind_compute_pipeline(this->m_ambient_occlusion_pipeline);

		// update per batch constant buffer
		uint32_t const global_set_per_batch_binding_offset = linear_allocate(upload_ring_buffer_current, upload_ring_buffer_end, sizeof(ambient_occlusion_pipeline_layout_global_set_per_batch_constant_buffer_binding), upload_ring_buffer_offset_alignment);
		{
			ambient_occlusion_pipeline_layout_global_set_per_batch_constant_buffer_binding *const global_set_per_batch_binding = reinterpret_cast<ambient_occlusion_pipeline_layout_global_set_per_batch_constant_buffer_binding *>(reinterpret_cast<uintptr_t>(upload_ring_buffer_base) + global_set_per_batch_binding_offset);

			DirectX::XMFLOAT3 eye_position = g_camera_controller.m_eye_position;
			DirectX::XMFLOAT3 eye_direction = g_camera_controller.m_eye_direction;
			DirectX::XMFLOAT3 up_direction = g_camera_controller.m_up_direction;
			DirectX::XMStoreFloat4x4(&global_set_per_batch_binding->g_inverse_view_transform, DirectX::XMMatrixInverse(NULL, DirectX::XMMatrixLookToRH(DirectX::XMLoadFloat3(&eye_position), DirectX::XMLoadFloat3(&eye_direction), DirectX::XMLoadFloat3(&up_direction))));

			DirectX::XMStoreFloat4x4(&global_set_per_batch_binding->g_inverse_projection_transform, DirectX::XMMatrixInverse(NULL, DirectX_Math_Matrix_PerspectiveFovRH_ReversedZ(g_camera_fov, static_cast<float>(this->m_screen_width) / static_cast<float>(this->m_screen_height), 0.1F, 1000.0F)));

			global_set_per_batch_binding->g_screen_width = static_cast<float>(this->m_screen_width);
			global_set_per_batch_binding->g_screen_height = static_cast<float>(this->m_screen_height);

			global_set_per_batch_binding->g_max_distance = 5.0F;
			global_set_per_batch_binding->g_sample_count = 64.0F;
		}

		pal_descriptor_set *const ambient_occlusion_descritor_sets[2] = {this->m_ambient_occlusion_global_descriptor_set, this->m_ambient_occlusion_scene_descriptor_set};
		uint32_t const ambient_occlusion_dynamic_offsets[1] = {global_set_per_batch_binding_offset};
		command_buffer->bind_compute_descriptor_sets(this->m_ambient_occlusion_pipeline_layout, sizeof(ambient_occlusion_descritor_sets) / sizeof(ambient_occlusion_descritor_sets[0]), ambient_occlusion_descritor_sets, sizeof(ambient_occlusion_dynamic_offsets) / sizeof(ambient_occlusion_dynamic_offsets[0]), ambient_occlusion_dynamic_offsets);

		command_buffer->dispatch(this->m_screen_width, this->m_screen_height, 1U);

		command_buffer->compute_pass_store_storage_image(this->m_ambient_occlusion_image, PAL_STORAGE_IMAGE_STORE_OPERATION_FLUSH_FOR_SAMPLED_IMAGE);

		command_buffer->end_debug_utils_label();
	}
}

static inline uint32_t linear_allocate(uint32_t &buffer_current, uint32_t buffer_end, uint32_t size, uint32_t alignment)
{
	uint32_t buffer_offset = utils_align_up(buffer_current, alignment);
	buffer_current = (buffer_offset + size);
	assert(buffer_current < buffer_end);
	return buffer_offset;
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
