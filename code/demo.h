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

#include "../thirdparty/PAL/include/pal_device.h"

class Demo
{
	pal_pipeline_layout *m_geometric_buffer_pipeline_layout;

	pal_render_pass *m_geometric_buffer_render_pass;
	pal_graphics_pipeline *m_geometric_buffer_pipeline;

	pal_pipeline_layout *m_ambient_occlusion_pipeline_layout;

	pal_compute_pipeline *m_ambient_occlusion_pipeline;

	pal_sampler *m_linear_sampler;

	uint32_t m_screen_width;
	uint32_t m_screen_height;
	pal_color_attachment_image *m_geometric_buffer_a_image;
	pal_depth_stencil_attachment_image *m_geometric_buffer_depth_image;
	pal_storage_image *m_ambient_occlusion_image;

	pal_frame_buffer *m_geometric_buffer_frame_buffer;

	pal_descriptor_set *m_geometric_buffer_global_descriptor_set;
	pal_descriptor_set *m_ambient_occlusion_global_descriptor_set;

	pal_asset_vertex_position_buffer *m_plane_vertex_position_buffer;
	pal_asset_vertex_varying_buffer *m_plane_vertex_varying_buffer;
	pal_asset_index_buffer *m_plane_index_buffer;
	pal_asset_compacted_bottom_level_acceleration_structure *m_plane_compacted_bottom_level_acceleration_structure;

	pal_asset_vertex_position_buffer *m_house_vertex_position_buffer;
	pal_asset_vertex_varying_buffer *m_house_vertex_varying_buffer;
	pal_asset_index_buffer *m_house_index_buffer;
	pal_asset_compacted_bottom_level_acceleration_structure *m_house_compacted_bottom_level_acceleration_structure;

	uint32_t m_scene_top_level_acceleration_structure_update_scratch_size;
	pal_top_level_acceleration_structure *m_scene_top_level_acceleration_structure;
	pal_descriptor_set *m_ambient_occlusion_scene_descriptor_set;

public:
	Demo();

	void init(pal_device *device, pal_upload_ring_buffer const *global_upload_ring_buffer);

	void destroy(pal_device *device);

	pal_sampled_image const *get_sampled_image_for_present();

	void draw(pal_graphics_command_buffer *command_buffer, void *upload_ring_buffer_base, uint32_t upload_ring_buffer_current, uint32_t upload_ring_buffer_end, uint32_t upload_ring_buffer_offset_alignment);
};

#endif