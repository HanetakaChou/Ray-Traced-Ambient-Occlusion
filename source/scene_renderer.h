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

#ifndef _SCENE_RENDERER_H_
#define _SCENE_RENDERER_H_ 1

#include "../thirdparty/Brioche/include/brx_device.h"
#include "../thirdparty/Brioche-Asset-Import/include/import_scene_asset.h"
#include "user_input_model.h"
#include "renderer_config.h"

// TLAS - Scene
// Instance - Mesh Instance
// BLAS - (Skinned) Mesh
// Geometry - Mesh (Skinned) Subset

struct Demo_Mesh_Skinned_Subset
{
	brx_storage_intermediate_buffer *m_skinned_vertex_position_buffer;
	brx_storage_intermediate_buffer *m_skinned_vertex_varying_buffer;
	brx_descriptor_set *m_skin_pipeline_per_mesh_skinned_subset_update_descriptor_set;
};

struct Demo_Mesh_Instance
{
	DirectX::XMFLOAT4X4 m_model_transform;
	scene_animation_skeleton m_animation_skeleton;
	mcrt_vector<Demo_Mesh_Skinned_Subset> m_skinned_subsets;
	brx_uniform_upload_buffer *m_skin_pipeline_per_mesh_instance_update_uniform_buffer;
	brx_descriptor_set *m_skin_pipeline_per_mesh_instance_update_descriptor_set;
	brx_intermediate_bottom_level_acceleration_structure *m_intermediate_bottom_level_acceleration_structure;
	brx_scratch_buffer *m_intermediate_bottom_level_acceleration_structure_update_scratch_buffer;
};

struct Demo_Mesh_Subset
{
	uint32_t m_vertex_count;
	uint32_t m_index_count;
	BRX_GRAPHICS_PIPELINE_INDEX_TYPE m_index_type;
	brx_storage_asset_buffer *m_vertex_position_buffer;
	brx_storage_asset_buffer *m_vertex_varying_buffer;
	brx_storage_asset_buffer *m_vertex_joint_buffer;
	brx_storage_asset_buffer *m_index_buffer;
	brx_storage_asset_buffer *m_information_buffer;
	brx_sampled_asset_image *m_normal_texture;
	brx_sampled_asset_image *m_emissive_texture;
	brx_sampled_asset_image *m_base_color_texture;
	brx_sampled_asset_image *m_metallic_roughness_texture;
};

struct Demo_Mesh
{
	bool m_skinned;
	mcrt_vector<Demo_Mesh_Subset> m_subsets;
	mcrt_vector<Demo_Mesh_Instance> m_instances;
	brx_compacted_bottom_level_acceleration_structure *m_compacted_bottom_level_acceleration_structure;
};

class scene_renderer
{
	brx_pipeline_layout *m_skin_pipeline_layout;
	brx_compute_pipeline *m_skin_pipeline;
	brx_pipeline_layout *m_gbuffer_pipeline_layout;
	brx_compute_pipeline *m_gbuffer_pipeline;
	brx_pipeline_layout *m_ambient_occlusion_pipeline_layout;
	brx_compute_pipeline *m_ambient_occlusion_pipeline;

	brx_sampler *m_sampler;

	brx_storage_asset_buffer *m_place_holder_buffer;
	brx_sampled_asset_image *m_place_holder_texture;

	mcrt_vector<Demo_Mesh> m_scene_meshes;
	mcrt_vector<brx_sampled_asset_image *> m_scene_textures;

	brx_top_level_acceleration_structure *m_scene_top_level_acceleration_structure;
	brx_top_level_acceleration_structure_instance_upload_buffer *m_scene_top_level_acceleration_structure_instance_upload_buffers[FRAME_THROTTLING_COUNT];
	brx_scratch_buffer *m_scene_top_level_acceleration_structure_update_scratch_buffer;

	uint32_t m_uniform_upload_buffer_offset_alignment;
	brx_uniform_upload_buffer *m_common_gbuffer_pipeline_ambient_occlusion_pipeline_none_update_uniform_buffer;
	brx_uniform_upload_buffer *m_gbuffer_pipeline_none_update_instance_information_uniform_buffer;

	brx_descriptor_set_layout *m_gbuffer_pipeline_none_update_descriptor_set_layout;
	brx_descriptor_set *m_gbuffer_pipeline_none_update_descriptor_set;
	brx_descriptor_set *m_gbuffer_pipeline_none_update_bindless_buffer_descriptor_set;
	brx_descriptor_set *m_gbuffer_pipeline_none_update_bindless_texture_descriptor_set;
	brx_descriptor_set_layout *m_ambient_occlusion_pipeline_none_update_descriptor_set_layout;
	brx_descriptor_set *m_ambient_occlusion_pipeline_none_update_descriptor_set;

	uint32_t m_intermediate_width;
	uint32_t m_intermediate_height;
	brx_storage_image *m_gbuffer_depth_image;
	brx_storage_image *m_gbuffer_normal_image;
	brx_storage_image *m_ambient_occlusion_image;

	float m_animation_time;

public:
	scene_renderer();

	~scene_renderer();

	void init(brx_device *device, uint32_t frame_throttling_count, ui_model_t *out_ui_model, user_camera_model_t *out_user_camera_model);

	void destroy(brx_device *device);

	void on_swap_chain_attach(brx_device *device, uint32_t swap_chain_image_width, uint32_t swap_chain_image_height);

	brx_sampled_image const *get_scene_color_image();

	void on_swap_chain_detach(brx_device *device);

	void draw(brx_graphics_command_buffer *command_buffer, uint32_t frame_throttling_index, float interval_time, ui_model_t const *ui_model, user_camera_model_t const *user_camera_model);
};

#endif
