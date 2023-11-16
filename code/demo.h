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

#ifndef _DEMO_H_
#define _DEMO_H_ 1

#include "../thirdparty/Brioche/include/brx_device.h"

constexpr static const uint32_t scene_instance_count = 2U;

class Demo
{
	brx_pipeline_layout *m_gbuffer_pipeline_layout;

	brx_compute_pipeline *m_gbuffer_pipeline;

	brx_pipeline_layout *m_ambient_occlusion_pipeline_layout;

	brx_compute_pipeline *m_ambient_occlusion_pipeline;

	uint32_t m_screen_width;
	uint32_t m_screen_height;
	brx_storage_image *m_gbuffer_depth_image;
	brx_storage_image *m_gbuffer_normal_image;
	brx_storage_image *m_ambient_occlusion_image;

	uint32_t m_scene_index_counts[scene_instance_count];
	brx_asset_vertex_position_buffer *m_scene_vertex_position_buffers[scene_instance_count];
	brx_asset_vertex_varying_buffer *m_scene_vertex_varying_buffers[scene_instance_count];
	brx_asset_index_buffer *m_scene_index_buffers[scene_instance_count];
	brx_asset_compacted_bottom_level_acceleration_structure *m_scene_compacted_bottom_level_acceleration_structures[scene_instance_count];

	brx_top_level_acceleration_structure *m_scene_top_level_acceleration_structure;
	brx_top_level_acceleration_structure_instance_upload_buffer *m_scene_top_level_acceleration_structure_instance_upload_buffer;
	brx_scratch_buffer *m_scene_top_level_acceleration_structure_update_scratch_buffer;

	brx_uniform_upload_buffer *m_common_none_update_uniform_buffer;
	brx_uniform_upload_buffer *m_gbuffer_pipeline_none_update_geometry_data_uniform_buffer;
	brx_uniform_upload_buffer *m_gbuffer_pipeline_none_update_instance_data_uniform_buffer;

	brx_descriptor_set *m_gbuffer_pipeline_none_update_descriptor_set;
	brx_descriptor_set *m_ambient_occlusion_pipeline_none_update_escriptor_set;

	float m_house_spin_angle;

public:
	Demo();

	void init(brx_device *device);

	void destroy(brx_device *device);

	brx_sampled_image const *get_sampled_image_for_present();

	void draw(brx_graphics_command_buffer *command_buffer, float interval_time);
};

#endif