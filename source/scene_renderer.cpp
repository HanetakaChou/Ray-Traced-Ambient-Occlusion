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

#include "scene_renderer.h"
#include <cstring>
#include <assert.h>
#include "../thirdparty/McRT-Malloc/include/mcrt_unordered_map.h"
#include "../thirdparty/Reversed-Z/include/reversed_z.h"
#include "../thirdparty/DLB/DLB.h"
#include "../thirdparty/Brioche-Asset-Import/include/import_image_asset.h"
#include "../thirdparty/Brioche-Asset-Import/include/import_asset_input_stream.h"
#include "../thirdparty/Packed-Vector/shaders/octahedron_mapping.sli"
#include "../assets/assets.h"
#include "../shaders/common_asset_constant.sli"
#include "../shaders/skin_pipeline_resource_binding.sli"
#include "../shaders/common_gbuffer_pipeline_ambient_occlusion_pipeline_resource_binding.sli"
#include "../shaders/gbuffer_pipeline_resource_binding.sli"
#include "../shaders/ambient_occlusion_pipeline_resource_binding.sli"

static inline uint32_t tbb_align_up(uint32_t value, uint32_t alignment);

// 60 FPS
static constexpr float const animation_frame_rate = 60.0F;

scene_renderer::scene_renderer()
    : m_intermediate_width(0U),
      m_intermediate_height(0U),
      m_gbuffer_depth_image(NULL),
      m_gbuffer_normal_image(NULL),
      m_ambient_occlusion_image(NULL)
{
}

scene_renderer::~scene_renderer()
{
    assert(0U == this->m_intermediate_width);
    assert(0U == this->m_intermediate_height);
    assert(NULL == this->m_gbuffer_depth_image);
    assert(NULL == this->m_gbuffer_normal_image);
    assert(NULL == this->m_ambient_occlusion_image);
}

void scene_renderer::init(brx_device *device, uint32_t frame_throttling_count, ui_model_t *out_ui_model, user_camera_model_t *out_user_camera_model)
{
    // Descriptor Layout
    brx_descriptor_set_layout *skin_pipeline_per_mesh_instance_update_descriptor_set_layout;
    brx_descriptor_set_layout *skin_pipeline_per_mesh_skinned_subset_update_descriptor_set_layout;
    brx_descriptor_set_layout *gbuffer_pipeline_none_update_bindless_buffer_descriptor_set_layout;
    brx_descriptor_set_layout *gbuffer_pipeline_none_update_bindless_texture_descriptor_set_layout;
    {
        BRX_DESCRIPTOR_SET_LAYOUT_BINDING const skin_pipeline_per_mesh_instance_update_descriptor_set_layout_bindings[] = {
            {0U, BRX_DESCRIPTOR_TYPE_DYNAMIC_UNIFORM_BUFFER, 1U}};
        skin_pipeline_per_mesh_instance_update_descriptor_set_layout = device->create_descriptor_set_layout(sizeof(skin_pipeline_per_mesh_instance_update_descriptor_set_layout_bindings) / sizeof(skin_pipeline_per_mesh_instance_update_descriptor_set_layout_bindings[0]), skin_pipeline_per_mesh_instance_update_descriptor_set_layout_bindings);

        BRX_DESCRIPTOR_SET_LAYOUT_BINDING const skin_pipeline_per_mesh_skined_subset_update_descriptor_set_layout_bindings[] = {
            {0U, BRX_DESCRIPTOR_TYPE_READ_ONLY_STORAGE_BUFFER, 3U},
            {1U, BRX_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2U}};
        skin_pipeline_per_mesh_skinned_subset_update_descriptor_set_layout = device->create_descriptor_set_layout(sizeof(skin_pipeline_per_mesh_skined_subset_update_descriptor_set_layout_bindings) / sizeof(skin_pipeline_per_mesh_skined_subset_update_descriptor_set_layout_bindings[0]), skin_pipeline_per_mesh_skined_subset_update_descriptor_set_layout_bindings);

        brx_descriptor_set_layout *const skin_pipeline_descriptor_set_layouts[] = {
            skin_pipeline_per_mesh_instance_update_descriptor_set_layout, skin_pipeline_per_mesh_skinned_subset_update_descriptor_set_layout};
        this->m_skin_pipeline_layout = device->create_pipeline_layout(sizeof(skin_pipeline_descriptor_set_layouts) / sizeof(skin_pipeline_descriptor_set_layouts[0]), skin_pipeline_descriptor_set_layouts);

        BRX_DESCRIPTOR_SET_LAYOUT_BINDING const gbuffer_pipeline_none_update_descriptor_set_layout_bindings[] = {
            {0U, BRX_DESCRIPTOR_TYPE_DYNAMIC_UNIFORM_BUFFER, 1U},
            {1U, BRX_DESCRIPTOR_TYPE_TOP_LEVEL_ACCELERATION_STRUCTURE, 1U},
            {2U, BRX_DESCRIPTOR_TYPE_DYNAMIC_UNIFORM_BUFFER, 1U},
            {3U, BRX_DESCRIPTOR_TYPE_SAMPLER, 1U},
            {4U, BRX_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2U}};
        this->m_gbuffer_pipeline_none_update_descriptor_set_layout = device->create_descriptor_set_layout(sizeof(gbuffer_pipeline_none_update_descriptor_set_layout_bindings) / sizeof(gbuffer_pipeline_none_update_descriptor_set_layout_bindings[0]), gbuffer_pipeline_none_update_descriptor_set_layout_bindings);

        BRX_DESCRIPTOR_SET_LAYOUT_BINDING const gbuffer_pipeline_none_update_bindless_buffer_descriptor_set_layout_bindings[] = {
            {0U, BRX_DESCRIPTOR_TYPE_READ_ONLY_STORAGE_BUFFER, BRX_DESCRIPTOR_SET_LAYOUT_BINDING_DESCRIPTOR_COUNT_UNBOUNDED}};
        gbuffer_pipeline_none_update_bindless_buffer_descriptor_set_layout = device->create_descriptor_set_layout(sizeof(gbuffer_pipeline_none_update_bindless_buffer_descriptor_set_layout_bindings) / sizeof(gbuffer_pipeline_none_update_bindless_buffer_descriptor_set_layout_bindings[0]), gbuffer_pipeline_none_update_bindless_buffer_descriptor_set_layout_bindings);

        BRX_DESCRIPTOR_SET_LAYOUT_BINDING const gbuffer_pipeline_none_update_bindless_texture_descriptor_set_layout_bindings[] = {
            {0U, BRX_DESCRIPTOR_TYPE_SAMPLED_IMAGE, BRX_DESCRIPTOR_SET_LAYOUT_BINDING_DESCRIPTOR_COUNT_UNBOUNDED}};
        gbuffer_pipeline_none_update_bindless_texture_descriptor_set_layout = device->create_descriptor_set_layout(sizeof(gbuffer_pipeline_none_update_bindless_texture_descriptor_set_layout_bindings) / sizeof(gbuffer_pipeline_none_update_bindless_texture_descriptor_set_layout_bindings[0]), gbuffer_pipeline_none_update_bindless_texture_descriptor_set_layout_bindings);

        brx_descriptor_set_layout *const gbuffer_pipeline_descriptor_set_layouts[] = {
            this->m_gbuffer_pipeline_none_update_descriptor_set_layout,
            gbuffer_pipeline_none_update_bindless_buffer_descriptor_set_layout,
            gbuffer_pipeline_none_update_bindless_texture_descriptor_set_layout};
        this->m_gbuffer_pipeline_layout = device->create_pipeline_layout(sizeof(gbuffer_pipeline_descriptor_set_layouts) / sizeof(gbuffer_pipeline_descriptor_set_layouts[0]), gbuffer_pipeline_descriptor_set_layouts);

        BRX_DESCRIPTOR_SET_LAYOUT_BINDING ambient_occlusion_pipeline_none_update_descriptor_set_layout_bindings[] = {
            {0U, BRX_DESCRIPTOR_TYPE_DYNAMIC_UNIFORM_BUFFER, 1U},
            {1U, BRX_DESCRIPTOR_TYPE_TOP_LEVEL_ACCELERATION_STRUCTURE, 1U},
            {2U, BRX_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 2U},
            {3U, BRX_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1U}};
        this->m_ambient_occlusion_pipeline_none_update_descriptor_set_layout = device->create_descriptor_set_layout(sizeof(ambient_occlusion_pipeline_none_update_descriptor_set_layout_bindings) / sizeof(ambient_occlusion_pipeline_none_update_descriptor_set_layout_bindings[0]), ambient_occlusion_pipeline_none_update_descriptor_set_layout_bindings);

        brx_descriptor_set_layout *const ambient_occlusion_pipeline_descriptor_set_layouts[] = {
            this->m_ambient_occlusion_pipeline_none_update_descriptor_set_layout};
        this->m_ambient_occlusion_pipeline_layout = device->create_pipeline_layout(sizeof(ambient_occlusion_pipeline_descriptor_set_layouts) / sizeof(ambient_occlusion_pipeline_descriptor_set_layouts[0]), ambient_occlusion_pipeline_descriptor_set_layouts);
    }

    // Pipeline
    {
        // Skin Pipeline
        {
            switch (device->get_backend_name())
            {
            case BRX_BACKEND_NAME_D3D12:
            {
#include "../shaders/dxil/skin_compute.inl"
                this->m_skin_pipeline = device->create_compute_pipeline(this->m_skin_pipeline_layout, sizeof(skin_compute_shader_module_code), skin_compute_shader_module_code);
            }
            break;
            case BRX_BACKEND_NAME_VK:
            {
#include "../shaders/spirv/skin_compute.inl"
                this->m_skin_pipeline = device->create_compute_pipeline(this->m_skin_pipeline_layout, sizeof(skin_compute_shader_module_code), skin_compute_shader_module_code);
            }
            break;
            default:
            {
                assert(false);
            }
            }
        }

        // GBuffer Pipeline
        {
            switch (device->get_backend_name())
            {
            case BRX_BACKEND_NAME_D3D12:
            {
#include "../shaders/dxil/gbuffer_compute.inl"
                this->m_gbuffer_pipeline = device->create_compute_pipeline(this->m_gbuffer_pipeline_layout, sizeof(gbuffer_compute_shader_module_code), gbuffer_compute_shader_module_code);
            }
            break;
            case BRX_BACKEND_NAME_VK:
            {
#include "../shaders/spirv/gbuffer_compute.inl"
                this->m_gbuffer_pipeline = device->create_compute_pipeline(this->m_gbuffer_pipeline_layout, sizeof(gbuffer_compute_shader_module_code), gbuffer_compute_shader_module_code);
            }
            break;
            default:
            {
                assert(false);
            }
            }
        }

        // Ambient Occlusion
        {
            switch (device->get_backend_name())
            {
            case BRX_BACKEND_NAME_D3D12:
            {
#include "../shaders/dxil/ambient_occlusion_compute.inl"
                this->m_ambient_occlusion_pipeline = device->create_compute_pipeline(this->m_ambient_occlusion_pipeline_layout, sizeof(ambient_occlusion_compute_shader_module_code), ambient_occlusion_compute_shader_module_code);
            }
            break;
            case BRX_BACKEND_NAME_VK:
            {
#include "../shaders/spirv/ambient_occlusion_compute.inl"
                this->m_ambient_occlusion_pipeline = device->create_compute_pipeline(this->m_ambient_occlusion_pipeline_layout, sizeof(ambient_occlusion_compute_shader_module_code), ambient_occlusion_compute_shader_module_code);
            }
            break;
            default:
            {
                assert(false);
            }
            }
        }
    }

    // Assets & Place Holder Texture
    {
        brx_upload_command_buffer *const upload_command_buffer = device->create_upload_command_buffer();

        brx_graphics_command_buffer *const graphics_command_buffer = device->create_graphics_command_buffer();

        brx_upload_queue *const upload_queue = device->create_upload_queue();

        brx_graphics_queue *const graphics_queue = device->create_graphics_queue();

        brx_fence *const fence = device->create_fence(true);

        // step 1
        // upload buffers
        // upload textures
        // build (non compacted) bottom level acceleration structure
        mcrt_vector<brx_non_compacted_bottom_level_acceleration_structure *> scene_non_compacted_bottom_level_acceleration_structures;
        brx_compacted_bottom_level_acceleration_structure_size_query_pool *compacted_bottom_level_acceleration_structure_size_query_pool = NULL;
        {
            mcrt_vector<brx_staging_upload_buffer *> staging_upload_buffers;

            mcrt_vector<brx_scratch_buffer *> scratch_buffers;

            device->reset_upload_command_buffer(upload_command_buffer);

            device->reset_graphics_command_buffer(graphics_command_buffer);

            upload_command_buffer->begin();

            graphics_command_buffer->begin();

            // Asset
            // upload vertex position buffer
            // upload vertex varying buffer
            // upload vertex joint buffer
            // upload index buffer
            {
                mcrt_vector<mcrt_string> const file_names = {"the-white-room/the-white-room.gltf", "keqing-lolita/keqing-lolita-love-you.gltf"};

                mcrt_vector<DirectX::XMFLOAT4X4> root_transforms(static_cast<size_t>(file_names.size()));
                DirectX::XMStoreFloat4x4(&root_transforms[0], DirectX::XMMatrixIdentity());
                DirectX::XMStoreFloat4x4(&root_transforms[1], DirectX::XMMatrixTranslation(0.0, 0.0, 2.0));

                import_asset_input_stream_factory *input_stream_factory = import_asset_init_memory_input_stream_factory();

                mcrt_unordered_map<mcrt_string, brx_sampled_asset_image *> mapped_textures;

                for (size_t file_name_index = 0U; file_name_index < file_names.size(); ++file_name_index)
                {
                    mcrt_string const &file_name = file_names[file_name_index];
                    DirectX::XMFLOAT4X4 const &root_transform = root_transforms[file_name_index];

                    mcrt_vector<scene_mesh_data> total_mesh_data;
                    if (import_gltf_scene_asset(total_mesh_data, animation_frame_rate, input_stream_factory, file_name.c_str()))
                    {
                        size_t mesh_index_offset = this->m_scene_meshes.size();

                        this->m_scene_meshes.resize(mesh_index_offset + total_mesh_data.size());

                        for (size_t mesh_index = 0U; mesh_index < total_mesh_data.size(); ++mesh_index)
                        {
                            scene_mesh_data const &in_mesh_data = total_mesh_data[mesh_index];

                            Demo_Mesh &out_mesh = this->m_scene_meshes[mesh_index_offset + mesh_index];

                            out_mesh.m_skinned = in_mesh_data.m_skinned;

                            out_mesh.m_subsets.resize(in_mesh_data.m_subsets.size());

                            for (size_t subset_index = 0U; subset_index < in_mesh_data.m_subsets.size(); ++subset_index)
                            {
                                scene_mesh_subset_data const &in_subset_data = in_mesh_data.m_subsets[subset_index];

                                Demo_Mesh_Subset &out_subset = out_mesh.m_subsets[subset_index];

                                // Buffer
                                {
                                    static constexpr uint32_t const DEMO_MESH_SUBSET_ASSET_BUFFER_COUNT = 5U;

                                    out_subset.m_vertex_count = static_cast<uint32_t>(in_subset_data.m_vertex_position_binding.size());

                                    out_subset.m_index_count = static_cast<uint32_t>(in_subset_data.m_indices.size());

                                    out_subset.m_index_type = (in_subset_data.m_max_index <= static_cast<uint32_t>(UINT16_MAX)) ? BRX_GRAPHICS_PIPELINE_INDEX_TYPE_UINT16 : BRX_GRAPHICS_PIPELINE_INDEX_TYPE_UINT32;

                                    assert(out_subset.m_vertex_count == in_subset_data.m_vertex_varying_binding.size());

                                    assert((!in_mesh_data.m_skinned) || (out_subset.m_vertex_count == in_subset_data.m_vertex_joint_binding.size()));

                                    mcrt_vector<uint16_t> uint16_indices;
                                    if (BRX_GRAPHICS_PIPELINE_INDEX_TYPE_UINT16 == out_subset.m_index_type)
                                    {
                                        uint16_indices.resize(out_subset.m_index_count);

                                        for (uint32_t index_index = 0U; index_index < out_subset.m_index_count; ++index_index)
                                        {
                                            uint16_indices[index_index] = static_cast<uint32_t>(in_subset_data.m_indices[index_index]);
                                        }
                                    }

                                    mesh_subset_information_storage_buffer_T mesh_subset_information_storage_buffer_T_source;
                                    {
                                        mesh_subset_information_storage_buffer_T_source.m_buffer_texture_flags = 0U;
                                        if (BRX_GRAPHICS_PIPELINE_INDEX_TYPE_UINT16 == out_subset.m_index_type)
                                        {
                                            mesh_subset_information_storage_buffer_T_source.m_buffer_texture_flags |= Buffer_Flag_Index_Type_UInt16;
                                        }
                                        if (!in_subset_data.m_normal_texture_image_uri.empty())
                                        {
                                            mesh_subset_information_storage_buffer_T_source.m_buffer_texture_flags |= Texture_Flag_Enable_Normal_Texture;
                                        }
                                        if (!in_subset_data.m_emissive_texture_image_uri.empty())
                                        {
                                            mesh_subset_information_storage_buffer_T_source.m_buffer_texture_flags |= Texture_Flag_Enable_Emissive_Texture;
                                        }
                                        if (!in_subset_data.m_base_color_texture_image_uri.empty())
                                        {
                                            mesh_subset_information_storage_buffer_T_source.m_buffer_texture_flags |= Texture_Flag_Enable_Base_Colorl_Texture;
                                        }
                                        if (!in_subset_data.m_metallic_roughness_texture_image_uri.empty())
                                        {
                                            mesh_subset_information_storage_buffer_T_source.m_buffer_texture_flags |= Texture_Flag_Enable_Metallic_Roughness_Texture;
                                        }
                                        mesh_subset_information_storage_buffer_T_source.m_normal_texture_scale = in_subset_data.m_normal_texture_scale;
                                        mesh_subset_information_storage_buffer_T_source.m_emissive_factor_x = in_subset_data.m_emissive_factor.x;
                                        mesh_subset_information_storage_buffer_T_source.m_emissive_factor_y = in_subset_data.m_emissive_factor.y;
                                        mesh_subset_information_storage_buffer_T_source.m_emissive_factor_z = in_subset_data.m_emissive_factor.z;
                                        mesh_subset_information_storage_buffer_T_source.m_base_color_factor_x = in_subset_data.m_base_color_factor.x;
                                        mesh_subset_information_storage_buffer_T_source.m_base_color_factor_y = in_subset_data.m_base_color_factor.y;
                                        mesh_subset_information_storage_buffer_T_source.m_base_color_factor_z = in_subset_data.m_base_color_factor.z;
                                        mesh_subset_information_storage_buffer_T_source.m_metallic_factor = in_subset_data.m_metallic_factor;
                                        mesh_subset_information_storage_buffer_T_source.m_roughness_factor = in_subset_data.m_roughness_factor;
                                    }

                                    size_t const source_asset_buffer_sizes[DEMO_MESH_SUBSET_ASSET_BUFFER_COUNT] = {
                                        sizeof(scene_mesh_vertex_position_binding) * out_subset.m_vertex_count,
                                        sizeof(scene_mesh_vertex_varying_binding) * out_subset.m_vertex_count,
                                        (!in_mesh_data.m_skinned) ? 0U : sizeof(scene_mesh_vertex_joint_binding) * out_subset.m_vertex_count,
                                        (BRX_GRAPHICS_PIPELINE_INDEX_TYPE_UINT16 == out_subset.m_index_type) ? (sizeof(uint16_t) * out_subset.m_index_count) : (sizeof(uint32_t) * out_subset.m_index_count),
                                        sizeof(mesh_subset_information_storage_buffer_T)};

                                    void const *const source_asset_buffers[DEMO_MESH_SUBSET_ASSET_BUFFER_COUNT] = {
                                        in_subset_data.m_vertex_position_binding.data(),
                                        in_subset_data.m_vertex_varying_binding.data(),
                                        (!in_mesh_data.m_skinned) ? NULL : in_subset_data.m_vertex_joint_binding.data(),
                                        (BRX_GRAPHICS_PIPELINE_INDEX_TYPE_UINT16 == out_subset.m_index_type) ? static_cast<void const *>(uint16_indices.data()) : static_cast<void const *>(in_subset_data.m_indices.data()),
                                        &mesh_subset_information_storage_buffer_T_source};

                                    brx_storage_asset_buffer **const destination_asset_buffers[DEMO_MESH_SUBSET_ASSET_BUFFER_COUNT] = {
                                        &out_subset.m_vertex_position_buffer,
                                        &out_subset.m_vertex_varying_buffer,
                                        &out_subset.m_vertex_joint_buffer,
                                        &out_subset.m_index_buffer,
                                        &out_subset.m_information_buffer};

                                    for (uint32_t mesh_subset_asset_buffer_index = 0U; mesh_subset_asset_buffer_index < DEMO_MESH_SUBSET_ASSET_BUFFER_COUNT; ++mesh_subset_asset_buffer_index)
                                    {
                                        brx_storage_asset_buffer *&destination_asset_buffer = (*destination_asset_buffers[mesh_subset_asset_buffer_index]);

                                        if (NULL != source_asset_buffers[mesh_subset_asset_buffer_index])
                                        {
                                            uint32_t const asset_buffer_size = static_cast<uint32_t>(source_asset_buffer_sizes[mesh_subset_asset_buffer_index]);

                                            destination_asset_buffer = device->create_storage_asset_buffer(asset_buffer_size);

                                            brx_staging_upload_buffer *const asset_buffer_staging_upload_buffer = device->create_staging_upload_buffer(asset_buffer_size);

                                            staging_upload_buffers.push_back(asset_buffer_staging_upload_buffer);

                                            std::memcpy(asset_buffer_staging_upload_buffer->get_host_memory_range_base(), source_asset_buffers[mesh_subset_asset_buffer_index], asset_buffer_size);

                                            upload_command_buffer->upload_from_staging_upload_buffer_to_storage_asset_buffer(destination_asset_buffer, 0U, asset_buffer_staging_upload_buffer, 0U, asset_buffer_size);
                                        }
                                        else
                                        {
                                            assert(0U == source_asset_buffer_sizes[mesh_subset_asset_buffer_index]);

                                            assert(NULL == source_asset_buffers[mesh_subset_asset_buffer_index]);

                                            destination_asset_buffer = NULL;
                                        }
                                    }
                                }

                                // Texture
                                {
                                    static constexpr uint32_t const DEMO_MESH_SUBSET_ASSET_TEXTURE_COUNT = 4U;

                                    uint32_t const staging_upload_buffer_offset_alignment = device->get_staging_upload_buffer_offset_alignment();
                                    uint32_t const staging_upload_buffer_row_pitch_alignment = device->get_staging_upload_buffer_row_pitch_alignment();

                                    mcrt_string const *const asset_texture_image_uris[DEMO_MESH_SUBSET_ASSET_TEXTURE_COUNT] = {
                                        &in_subset_data.m_normal_texture_image_uri,
                                        &in_subset_data.m_emissive_texture_image_uri,
                                        &in_subset_data.m_base_color_texture_image_uri,
                                        &in_subset_data.m_metallic_roughness_texture_image_uri};

                                    brx_sampled_asset_image **const destination_asset_textures[DEMO_MESH_SUBSET_ASSET_TEXTURE_COUNT] = {
                                        &out_subset.m_normal_texture,
                                        &out_subset.m_emissive_texture,
                                        &out_subset.m_base_color_texture,
                                        &out_subset.m_metallic_roughness_texture};

                                    for (uint32_t mesh_subset_asset_texture_index = 0U; mesh_subset_asset_texture_index < DEMO_MESH_SUBSET_ASSET_TEXTURE_COUNT; ++mesh_subset_asset_texture_index)
                                    {
                                        mcrt_string const &asset_texture_image_uri = (*asset_texture_image_uris[mesh_subset_asset_texture_index]);

                                        brx_sampled_asset_image *&destination_asset_texture = (*destination_asset_textures[mesh_subset_asset_texture_index]);

                                        if (!asset_texture_image_uri.empty())
                                        {
                                            mcrt_string image_asset_file_name_dds;
                                            mcrt_string image_asset_file_name_pvr;
                                            {
                                                mcrt_string image_asset_file_name;

                                                size_t dir_name_pos = file_name.find_last_of("/\\");
                                                if (mcrt_string::npos != dir_name_pos)
                                                {
                                                    image_asset_file_name = file_name.substr(0U, dir_name_pos + 1U);
                                                }
                                                else
                                                {
                                                    image_asset_file_name += "./";
                                                }

                                                size_t ext_name_pos = asset_texture_image_uri.find_last_of(".");
                                                if (mcrt_string::npos != ext_name_pos)
                                                {
                                                    image_asset_file_name += asset_texture_image_uri.substr(0U, ext_name_pos + 1U);
                                                }
                                                else
                                                {
                                                    image_asset_file_name += asset_texture_image_uri;
                                                }

                                                image_asset_file_name_dds = (image_asset_file_name + "dds");
                                                image_asset_file_name_pvr = (image_asset_file_name + "pvr");
                                            }

                                            mcrt_unordered_map<mcrt_string, brx_sampled_asset_image *>::const_iterator found;
                                            if (mapped_textures.end() != (found = mapped_textures.find(image_asset_file_name_dds)) || mapped_textures.end() != (found = mapped_textures.find(image_asset_file_name_pvr)))
                                            {
                                                assert(NULL != found->second);
                                                destination_asset_texture = found->second;
                                            }
                                            else
                                            {
                                                mcrt_string import_image_asset_file_name;
                                                import_asset_input_stream *import_image_asset_input_stream;
                                                bool (*pfn_import_image_asset_header_from_input_stream)(import_asset_input_stream *, IMPORT_ASSET_IMAGE_HEADER *, size_t *);
                                                bool (*pfn_import_image_asset_data_from_input_stream)(import_asset_input_stream *, IMPORT_ASSET_IMAGE_HEADER const *, size_t, void *, size_t, BRX_SAMPLED_ASSET_IMAGE_IMPORT_SUBRESOURCE_MEMCPY_DEST const *);
                                                if (device->is_sampled_asset_image_compression_bc_supported() && (NULL != (import_image_asset_input_stream = input_stream_factory->create_instance(image_asset_file_name_dds.c_str()))))
                                                {
                                                    import_image_asset_file_name = image_asset_file_name_dds;
                                                    pfn_import_image_asset_header_from_input_stream = import_dds_image_asset_header_from_input_stream;
                                                    pfn_import_image_asset_data_from_input_stream = import_dds_image_asset_data_from_input_stream;
                                                }
                                                else if (device->is_sampled_asset_image_compression_astc_supported() && (NULL != (import_image_asset_input_stream = input_stream_factory->create_instance(image_asset_file_name_pvr.c_str()))))
                                                {
                                                    import_image_asset_file_name = image_asset_file_name_pvr;
                                                    pfn_import_image_asset_header_from_input_stream = import_pvr_image_asset_header_from_input_stream;
                                                    pfn_import_image_asset_data_from_input_stream = import_pvr_image_asset_data_from_input_stream;
                                                }
                                                else
                                                {
                                                    // TODO: jpeg
                                                    // TODO: png
                                                    import_image_asset_input_stream = NULL;
                                                    pfn_import_image_asset_header_from_input_stream = NULL;
                                                    pfn_import_image_asset_data_from_input_stream = NULL;
                                                }

                                                if (NULL != import_image_asset_input_stream && NULL != pfn_import_image_asset_header_from_input_stream && NULL != pfn_import_image_asset_data_from_input_stream)
                                                {
                                                    IMPORT_ASSET_IMAGE_HEADER image_asset_header;
                                                    mcrt_vector<BRX_SAMPLED_ASSET_IMAGE_IMPORT_SUBRESOURCE_MEMCPY_DEST> subresource_memcpy_dests;
                                                    brx_staging_upload_buffer *image_staging_upload_buffer = NULL;
                                                    {
                                                        size_t image_asset_data_offset;
                                                        bool const res_import_image_asset_header = pfn_import_image_asset_header_from_input_stream(import_image_asset_input_stream, &image_asset_header, &image_asset_data_offset);
                                                        assert(res_import_image_asset_header);

                                                        uint32_t const subresource_count = image_asset_header.mip_levels;
                                                        subresource_memcpy_dests.resize(subresource_count);

                                                        // TODO: support more image paramters
                                                        assert(!image_asset_header.is_cube_map);
                                                        assert(IMPORT_ASSET_IMAGE_TYPE_2D == image_asset_header.type);
                                                        assert(1U == image_asset_header.depth);
                                                        assert(1U == image_asset_header.array_layers);
                                                        uint32_t const total_bytes = brx_sampled_asset_image_import_calculate_subresource_memcpy_dests(image_asset_header.format, image_asset_header.width, image_asset_header.height, 1U, image_asset_header.mip_levels, 1U, 0U, staging_upload_buffer_offset_alignment, staging_upload_buffer_row_pitch_alignment, subresource_count, &subresource_memcpy_dests[0]);
                                                        image_staging_upload_buffer = device->create_staging_upload_buffer(static_cast<uint32_t>(total_bytes));
                                                        staging_upload_buffers.push_back(image_staging_upload_buffer);

                                                        bool const res_import_image_asset_data = pfn_import_image_asset_data_from_input_stream(import_image_asset_input_stream, &image_asset_header, image_asset_data_offset, image_staging_upload_buffer->get_host_memory_range_base(), subresource_count, &subresource_memcpy_dests[0]);
                                                        assert(res_import_image_asset_data);
                                                    }
                                                    input_stream_factory->destory_instance(import_image_asset_input_stream);

                                                    destination_asset_texture = device->create_sampled_asset_image(image_asset_header.format, image_asset_header.width, image_asset_header.height, image_asset_header.mip_levels);

                                                    mapped_textures.emplace_hint(found, import_image_asset_file_name, destination_asset_texture);

                                                    for (uint32_t mip_level = 0U; mip_level < image_asset_header.mip_levels; ++mip_level)
                                                    {
                                                        upload_command_buffer->upload_from_staging_upload_buffer_to_sampled_asset_image(destination_asset_texture, image_asset_header.format, image_asset_header.width, image_asset_header.height, mip_level, image_staging_upload_buffer, subresource_memcpy_dests[mip_level].staging_upload_buffer_offset, subresource_memcpy_dests[mip_level].output_row_pitch, subresource_memcpy_dests[mip_level].output_row_count);
                                                    }
                                                }
                                                else
                                                {
                                                    destination_asset_texture = NULL;
                                                }
                                            }
                                        }
                                        else
                                        {
                                            destination_asset_texture = NULL;
                                        }
                                    }
                                }
                            }

                            out_mesh.m_instances.resize(total_mesh_data[mesh_index].m_instances.size());

                            for (size_t instance_index = 0U; instance_index < total_mesh_data[mesh_index].m_instances.size(); ++instance_index)
                            {
                                scene_mesh_instance_data &in_instance_data = total_mesh_data[mesh_index].m_instances[instance_index];

                                Demo_Mesh_Instance &out_mesh_instance = out_mesh.m_instances[instance_index];

                                DirectX::XMStoreFloat4x4(&out_mesh_instance.m_model_transform, DirectX::XMMatrixMultiply(DirectX::XMLoadFloat4x4(&in_instance_data.m_model_transform), DirectX::XMLoadFloat4x4(&root_transform)));

                                out_mesh_instance.m_animation_skeleton = std::move(in_instance_data.m_animation_skeleton);

                                if (!in_mesh_data.m_skinned)
                                {
                                    assert(out_mesh_instance.m_skinned_subsets.empty());
                                }
                                else
                                {
                                    out_mesh_instance.m_skinned_subsets.resize(in_mesh_data.m_subsets.size());

                                    for (size_t subset_index = 0U; subset_index < in_mesh_data.m_subsets.size(); ++subset_index)
                                    {
                                        scene_mesh_subset_data const &in_subset_data = in_mesh_data.m_subsets[subset_index];

                                        Demo_Mesh_Skinned_Subset &out_mesh_skinned_subset = out_mesh_instance.m_skinned_subsets[subset_index];

                                        uint32_t const vertex_count = static_cast<uint32_t>(in_subset_data.m_vertex_position_binding.size());

                                        uint32_t const vertex_position_buffer_size = sizeof(scene_mesh_vertex_position_binding) * vertex_count;

                                        out_mesh_skinned_subset.m_skinned_vertex_position_buffer = device->create_storage_intermediate_buffer(vertex_position_buffer_size);

                                        assert(vertex_count == in_subset_data.m_vertex_varying_binding.size());

                                        uint32_t const vertex_varying_buffer_size = sizeof(scene_mesh_vertex_varying_binding) * vertex_count;

                                        out_mesh_skinned_subset.m_skinned_vertex_varying_buffer = device->create_storage_intermediate_buffer(vertex_varying_buffer_size);
                                    }
                                }
                            }
                        }
                    }
                }

                import_asset_destroy_memory_input_stream_factory(input_stream_factory);
                input_stream_factory = NULL;

                this->m_scene_textures.reserve(mapped_textures.size());
                for (mcrt_unordered_map<mcrt_string, brx_sampled_asset_image *>::const_iterator material_texture_iterator = mapped_textures.begin(); material_texture_iterator != mapped_textures.end(); ++material_texture_iterator)
                {
                    brx_sampled_asset_image *const material_texture = material_texture_iterator->second;

                    assert(NULL != material_texture);

                    this->m_scene_textures.push_back(material_texture);
                }
            }

            // Place Holder Buffer
            {
                uint32_t const size = 1U;

                this->m_place_holder_buffer = device->create_storage_asset_buffer(size);

                brx_staging_upload_buffer *const place_holder_buffer_staging_upload_buffer = device->create_staging_upload_buffer(size);

                staging_upload_buffers.push_back(place_holder_buffer_staging_upload_buffer);

                std::memset(place_holder_buffer_staging_upload_buffer->get_host_memory_range_base(), 0, size);

                upload_command_buffer->upload_from_staging_upload_buffer_to_storage_asset_buffer(this->m_place_holder_buffer, 0U, place_holder_buffer_staging_upload_buffer, 0U, size);
            }

            // Place Holder Texture
            {
                uint32_t const staging_upload_buffer_offset_alignment = device->get_staging_upload_buffer_offset_alignment();
                uint32_t const staging_upload_buffer_row_pitch_alignment = device->get_staging_upload_buffer_row_pitch_alignment();

                BRX_SAMPLED_ASSET_IMAGE_FORMAT const format = BRX_SAMPLED_ASSET_IMAGE_FORMAT_R8G8B8A8_UNORM;
                uint32_t const width = 1U;
                uint32_t const height = 1U;
                uint32_t const mip_levels = 1U;
                this->m_place_holder_texture = device->create_sampled_asset_image(format, width, height, mip_levels);

                mcrt_vector<BRX_SAMPLED_ASSET_IMAGE_IMPORT_SUBRESOURCE_MEMCPY_DEST> subresource_memcpy_dests;
                uint32_t const subresource_count = mip_levels;
                subresource_memcpy_dests.resize(subresource_count);

                uint32_t const total_bytes = brx_sampled_asset_image_import_calculate_subresource_memcpy_dests(format, width, height, 1U, mip_levels, 1U, 0U, staging_upload_buffer_offset_alignment, staging_upload_buffer_row_pitch_alignment, subresource_count, &subresource_memcpy_dests[0]);
                brx_staging_upload_buffer *place_holder_image_staging_upload_buffer = device->create_staging_upload_buffer(total_bytes);

                staging_upload_buffers.push_back(place_holder_image_staging_upload_buffer);

                uint32_t const mip_level = 0U;
                uint32_t const subresource_index = brx_sampled_asset_image_import_calculate_subresource_index(mip_level, 0U, 0U, mip_levels, 1U);
                for (uint32_t output_slice_index = 0U; output_slice_index < subresource_memcpy_dests[mip_level].output_slice_count; ++output_slice_index)
                {
                    for (uint32_t output_row_index = 0U; output_row_index < subresource_memcpy_dests[mip_level].output_row_count; ++output_row_index)
                    {
                        void *destination = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(place_holder_image_staging_upload_buffer->get_host_memory_range_base()) + (subresource_memcpy_dests[subresource_index].staging_upload_buffer_offset + subresource_memcpy_dests[subresource_index].output_slice_pitch * output_slice_index + subresource_memcpy_dests[subresource_index].output_row_pitch * output_row_index));

                        std::memset(destination, 0, subresource_memcpy_dests[mip_level].output_row_size);
                    }
                }

                upload_command_buffer->upload_from_staging_upload_buffer_to_sampled_asset_image(this->m_place_holder_texture, format, width, height, mip_level, place_holder_image_staging_upload_buffer, subresource_memcpy_dests[mip_level].staging_upload_buffer_offset, subresource_memcpy_dests[mip_level].output_row_pitch, subresource_memcpy_dests[mip_level].output_row_count);
            }

            // build staging non compacted bottom level acceleration structure
            // release
            // acquire
            {
                // build staging non compacted bottom level acceleration structure
                {
                    mcrt_vector<brx_acceleration_structure_build_input_read_only_buffer const *> non_compacted_bottom_level_acceleration_structure_build_input_read_only_buffers;
                    uint32_t scene_non_compacted_bottom_level_acceleration_structure_count = 0U;
                    for (size_t mesh_index = 0U; mesh_index < this->m_scene_meshes.size(); ++mesh_index)
                    {
                        Demo_Mesh const &scene_mesh = this->m_scene_meshes[mesh_index];

                        if (!scene_mesh.m_skinned)
                        {
                            for (size_t subset_index = 0U; subset_index < scene_mesh.m_subsets.size(); ++subset_index)
                            {
                                Demo_Mesh_Subset const &scene_mesh_subset = scene_mesh.m_subsets[subset_index];

                                non_compacted_bottom_level_acceleration_structure_build_input_read_only_buffers.push_back(scene_mesh_subset.m_vertex_position_buffer->get_acceleration_structure_build_input_read_only_buffer());

                                non_compacted_bottom_level_acceleration_structure_build_input_read_only_buffers.push_back(scene_mesh_subset.m_index_buffer->get_acceleration_structure_build_input_read_only_buffer());
                            }

                            ++scene_non_compacted_bottom_level_acceleration_structure_count;
                        }
                    }

                    if (scene_non_compacted_bottom_level_acceleration_structure_count > 0U)
                    {
                        upload_command_buffer->build_non_compacted_bottom_level_acceleration_structure_pass_load(static_cast<uint32_t>(non_compacted_bottom_level_acceleration_structure_build_input_read_only_buffers.size()), non_compacted_bottom_level_acceleration_structure_build_input_read_only_buffers.data());

                        assert(NULL == compacted_bottom_level_acceleration_structure_size_query_pool);
                        compacted_bottom_level_acceleration_structure_size_query_pool = device->create_compacted_bottom_level_acceleration_structure_size_query_pool(scene_non_compacted_bottom_level_acceleration_structure_count);

                        scene_non_compacted_bottom_level_acceleration_structures.resize(scene_non_compacted_bottom_level_acceleration_structure_count);

                        uint32_t non_compacted_bottom_level_acceleration_structure_index = 0U;
                        for (size_t mesh_index = 0U; mesh_index < this->m_scene_meshes.size(); ++mesh_index)
                        {
                            Demo_Mesh const &scene_mesh = this->m_scene_meshes[mesh_index];

                            if (!scene_mesh.m_skinned)
                            {
                                mcrt_vector<BRX_BOTTOM_LEVEL_ACCELERATION_STRUCTURE_GEOMETRY> bottom_level_acceleration_structure_geometries(scene_mesh.m_subsets.size());

                                for (size_t subset_index = 0U; subset_index < scene_mesh.m_subsets.size(); ++subset_index)
                                {
                                    Demo_Mesh_Subset const &scene_mesh_subset = scene_mesh.m_subsets[subset_index];

                                    bottom_level_acceleration_structure_geometries[subset_index] = BRX_BOTTOM_LEVEL_ACCELERATION_STRUCTURE_GEOMETRY{true,
                                                                                                                                                    BRX_GRAPHICS_PIPELINE_VERTEX_ATTRIBUTE_FORMAT_R32G32B32_SFLOAT,
                                                                                                                                                    sizeof(scene_mesh_vertex_position_binding),
                                                                                                                                                    scene_mesh_subset.m_vertex_count,
                                                                                                                                                    scene_mesh_subset.m_vertex_position_buffer->get_acceleration_structure_build_input_read_only_buffer(),
                                                                                                                                                    scene_mesh_subset.m_index_type,
                                                                                                                                                    scene_mesh_subset.m_index_count,
                                                                                                                                                    scene_mesh_subset.m_index_buffer->get_acceleration_structure_build_input_read_only_buffer()};
                                }

                                uint32_t non_compacted_bottom_level_acceleration_structure_size = -1;
                                uint32_t non_compacted_bottom_level_acceleration_structure_build_scratch_size = -1;
                                device->get_non_compacted_bottom_level_acceleration_structure_size(static_cast<uint32_t>(bottom_level_acceleration_structure_geometries.size()), bottom_level_acceleration_structure_geometries.data(), &non_compacted_bottom_level_acceleration_structure_size, &non_compacted_bottom_level_acceleration_structure_build_scratch_size);

                                scene_non_compacted_bottom_level_acceleration_structures[non_compacted_bottom_level_acceleration_structure_index] = device->create_non_compacted_bottom_level_acceleration_structure(non_compacted_bottom_level_acceleration_structure_size);

                                brx_scratch_buffer *const non_compacted_bottom_level_acceleration_structure_build_scratch_buffer = device->create_scratch_buffer(non_compacted_bottom_level_acceleration_structure_build_scratch_size);
                                scratch_buffers.push_back(non_compacted_bottom_level_acceleration_structure_build_scratch_buffer);

                                upload_command_buffer->build_non_compacted_bottom_level_acceleration_structure(scene_non_compacted_bottom_level_acceleration_structures[non_compacted_bottom_level_acceleration_structure_index], static_cast<uint32_t>(bottom_level_acceleration_structure_geometries.size()), bottom_level_acceleration_structure_geometries.data(), non_compacted_bottom_level_acceleration_structure_build_scratch_buffer, compacted_bottom_level_acceleration_structure_size_query_pool, non_compacted_bottom_level_acceleration_structure_index);

                                ++non_compacted_bottom_level_acceleration_structure_index;
                            }
                        }
                        assert(scene_non_compacted_bottom_level_acceleration_structures.size() == non_compacted_bottom_level_acceleration_structure_index);

                        upload_command_buffer->build_non_compacted_bottom_level_acceleration_structure_pass_store(static_cast<uint32_t>(non_compacted_bottom_level_acceleration_structure_build_input_read_only_buffers.size()), non_compacted_bottom_level_acceleration_structure_build_input_read_only_buffers.data());
                    }
                }

                mcrt_vector<brx_storage_asset_buffer const *> uploaded_storage_asset_buffers;
                for (size_t mesh_index = 0U; mesh_index < this->m_scene_meshes.size(); ++mesh_index)
                {
                    Demo_Mesh const &scene_mesh = this->m_scene_meshes[mesh_index];

                    for (size_t subset_index = 0U; subset_index < scene_mesh.m_subsets.size(); ++subset_index)
                    {
                        Demo_Mesh_Subset const &scene_mesh_subset = scene_mesh.m_subsets[subset_index];

                        uploaded_storage_asset_buffers.push_back(scene_mesh_subset.m_vertex_position_buffer);

                        uploaded_storage_asset_buffers.push_back(scene_mesh_subset.m_vertex_varying_buffer);

                        if (!scene_mesh.m_skinned)
                        {
                            assert(NULL == scene_mesh_subset.m_vertex_joint_buffer);
                        }
                        else
                        {
                            uploaded_storage_asset_buffers.push_back(scene_mesh_subset.m_vertex_joint_buffer);
                        }

                        uploaded_storage_asset_buffers.push_back(scene_mesh_subset.m_index_buffer);

                        uploaded_storage_asset_buffers.push_back(scene_mesh_subset.m_information_buffer);
                    }
                }

                // Place Holder Texture
                {
                    uploaded_storage_asset_buffers.push_back(this->m_place_holder_buffer);
                }

                mcrt_vector<brx_sampled_asset_image const *> uploaded_sampled_asset_images;
                mcrt_vector<uint32_t> uploaded_destination_mip_levels;
                for (mcrt_vector<brx_sampled_asset_image *>::const_iterator material_texture_iterator = this->m_scene_textures.begin(); material_texture_iterator != this->m_scene_textures.end(); ++material_texture_iterator)
                {
                    brx_sampled_asset_image *const material_texture = (*material_texture_iterator);

                    assert(NULL != material_texture);

                    uint32_t const mip_level_count = material_texture->get_mip_levels();

                    for (uint32_t mip_level = 0U; mip_level < mip_level_count; ++mip_level)
                    {
                        uploaded_sampled_asset_images.push_back(material_texture);

                        uploaded_destination_mip_levels.push_back(mip_level);
                    }
                }

                // Place Holder Texture
                {
                    uploaded_sampled_asset_images.push_back(this->m_place_holder_texture);

                    uploaded_destination_mip_levels.push_back(0U);
                }

                assert(uploaded_sampled_asset_images.size() == uploaded_destination_mip_levels.size());

                // release
                upload_command_buffer->release(static_cast<uint32_t>(uploaded_storage_asset_buffers.size()), uploaded_storage_asset_buffers.data(), static_cast<uint32_t>(uploaded_sampled_asset_images.size()), &uploaded_sampled_asset_images[0], uploaded_destination_mip_levels.data(), 0U, NULL);

                // acquire
                graphics_command_buffer->acquire(static_cast<uint32_t>(uploaded_storage_asset_buffers.size()), uploaded_storage_asset_buffers.data(), static_cast<uint32_t>(uploaded_sampled_asset_images.size()), &uploaded_sampled_asset_images[0], uploaded_destination_mip_levels.data(), 0U, NULL);
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
        // compact bottom level acceleration structure
        // build top level acceleration structure
        {
            mcrt_vector<brx_scratch_buffer *> scratch_buffers;

            device->reset_upload_command_buffer(upload_command_buffer);

            device->reset_graphics_command_buffer(graphics_command_buffer);

            upload_command_buffer->begin();

            graphics_command_buffer->begin();

            // compact bottom level acceleration structure
            {
                mcrt_vector<brx_compacted_bottom_level_acceleration_structure *> uploaded_compacted_bottom_level_acceleration_structures(static_cast<size_t>(scene_non_compacted_bottom_level_acceleration_structures.size()));

                uint32_t non_compacted_bottom_level_acceleration_structure_index = 0U;
                for (size_t mesh_index = 0U; mesh_index < this->m_scene_meshes.size(); ++mesh_index)
                {
                    Demo_Mesh &scene_mesh = this->m_scene_meshes[mesh_index];

                    if (!scene_mesh.m_skinned)
                    {
                        uint32_t const compacted_bottom_level_acceleration_structure_size = device->get_compacted_bottom_level_acceleration_structure_size_query_pool_result(compacted_bottom_level_acceleration_structure_size_query_pool, non_compacted_bottom_level_acceleration_structure_index);

                        scene_mesh.m_compacted_bottom_level_acceleration_structure = device->create_compacted_bottom_level_acceleration_structure(compacted_bottom_level_acceleration_structure_size);
                        uploaded_compacted_bottom_level_acceleration_structures[non_compacted_bottom_level_acceleration_structure_index] = scene_mesh.m_compacted_bottom_level_acceleration_structure;

                        upload_command_buffer->compact_bottom_level_acceleration_structure(scene_mesh.m_compacted_bottom_level_acceleration_structure, scene_non_compacted_bottom_level_acceleration_structures[non_compacted_bottom_level_acceleration_structure_index]);

                        ++non_compacted_bottom_level_acceleration_structure_index;
                    }
                    else
                    {
                        scene_mesh.m_compacted_bottom_level_acceleration_structure = NULL;
                    }
                }
                assert(scene_non_compacted_bottom_level_acceleration_structures.size() == non_compacted_bottom_level_acceleration_structure_index);
                assert(uploaded_compacted_bottom_level_acceleration_structures.size() == non_compacted_bottom_level_acceleration_structure_index);

                // release
                upload_command_buffer->release(0U, NULL, 0U, NULL, NULL, static_cast<uint32_t>(uploaded_compacted_bottom_level_acceleration_structures.size()), uploaded_compacted_bottom_level_acceleration_structures.data());

                // acquire
                graphics_command_buffer->acquire(0U, NULL, 0U, NULL, NULL, static_cast<uint32_t>(uploaded_compacted_bottom_level_acceleration_structures.size()), uploaded_compacted_bottom_level_acceleration_structures.data());
            }

            // build intermediate bottom level acceleration structure
            {
                mcrt_vector<brx_intermediate_bottom_level_acceleration_structure *> built_intermediate_bottom_level_acceleration_structures;

                uint32_t intermediate_bottom_level_acceleration_structure_index = 0U;
                for (size_t mesh_index = 0U; mesh_index < this->m_scene_meshes.size(); ++mesh_index)
                {
                    Demo_Mesh &scene_mesh = this->m_scene_meshes[mesh_index];

                    if (!scene_mesh.m_skinned)
                    {
                        for (size_t mesh_instance_index = 0U; mesh_instance_index < scene_mesh.m_instances.size(); ++mesh_instance_index)
                        {
                            Demo_Mesh_Instance &scene_mesh_instance = scene_mesh.m_instances[mesh_instance_index];

                            assert(scene_mesh_instance.m_skinned_subsets.empty());

                            scene_mesh_instance.m_intermediate_bottom_level_acceleration_structure = NULL;

                            scene_mesh_instance.m_intermediate_bottom_level_acceleration_structure_update_scratch_buffer = NULL;
                        }
                    }
                    else
                    {
                        for (size_t mesh_instance_index = 0U; mesh_instance_index < scene_mesh.m_instances.size(); ++mesh_instance_index)
                        {
                            Demo_Mesh_Instance &scene_mesh_instance = scene_mesh.m_instances[mesh_instance_index];

                            assert(scene_mesh.m_subsets.size() == scene_mesh_instance.m_skinned_subsets.size());

                            mcrt_vector<BRX_BOTTOM_LEVEL_ACCELERATION_STRUCTURE_GEOMETRY> bottom_level_acceleration_structure_geometries(scene_mesh_instance.m_skinned_subsets.size());

                            for (size_t mesh_subset_index = 0U; mesh_subset_index < scene_mesh_instance.m_skinned_subsets.size(); ++mesh_subset_index)
                            {
                                Demo_Mesh_Subset const &scene_mesh_subset = scene_mesh.m_subsets[mesh_subset_index];

                                bottom_level_acceleration_structure_geometries[mesh_subset_index] = BRX_BOTTOM_LEVEL_ACCELERATION_STRUCTURE_GEOMETRY{true,
                                                                                                                                                     BRX_GRAPHICS_PIPELINE_VERTEX_ATTRIBUTE_FORMAT_R32G32B32_SFLOAT,
                                                                                                                                                     sizeof(scene_mesh_vertex_position_binding),
                                                                                                                                                     scene_mesh_subset.m_vertex_count,
                                                                                                                                                     scene_mesh_subset.m_vertex_position_buffer->get_acceleration_structure_build_input_read_only_buffer(),
                                                                                                                                                     scene_mesh_subset.m_index_type,
                                                                                                                                                     scene_mesh_subset.m_index_count,
                                                                                                                                                     scene_mesh_subset.m_index_buffer->get_acceleration_structure_build_input_read_only_buffer()};
                            }

                            uint32_t intermediate_bottom_level_acceleration_structure_size = -1;
                            uint32_t intermediate_bottom_level_acceleration_structure_build_scratch_size = -1;
                            uint32_t intermediate_bottom_level_acceleration_structure_update_scratch_size = -1;
                            device->get_intermediate_bottom_level_acceleration_structure_size(static_cast<uint32_t>(bottom_level_acceleration_structure_geometries.size()), bottom_level_acceleration_structure_geometries.data(), &intermediate_bottom_level_acceleration_structure_size, &intermediate_bottom_level_acceleration_structure_build_scratch_size, &intermediate_bottom_level_acceleration_structure_update_scratch_size);

                            scene_mesh_instance.m_intermediate_bottom_level_acceleration_structure = device->create_intermediate_bottom_level_acceleration_structure(intermediate_bottom_level_acceleration_structure_size);
                            built_intermediate_bottom_level_acceleration_structures.push_back(scene_mesh_instance.m_intermediate_bottom_level_acceleration_structure);

                            brx_scratch_buffer *const intermediate_bottom_level_acceleration_structure_build_scratch_buffer = device->create_scratch_buffer(intermediate_bottom_level_acceleration_structure_build_scratch_size);
                            scratch_buffers.push_back(intermediate_bottom_level_acceleration_structure_build_scratch_buffer);

                            graphics_command_buffer->build_intermediate_bottom_level_acceleration_structure(scene_mesh_instance.m_intermediate_bottom_level_acceleration_structure, static_cast<uint32_t>(bottom_level_acceleration_structure_geometries.size()), bottom_level_acceleration_structure_geometries.data(), intermediate_bottom_level_acceleration_structure_build_scratch_buffer);

                            scene_mesh_instance.m_intermediate_bottom_level_acceleration_structure_update_scratch_buffer = device->create_scratch_buffer(intermediate_bottom_level_acceleration_structure_update_scratch_size);

                            ++intermediate_bottom_level_acceleration_structure_index;
                        }
                    }
                }
                assert(built_intermediate_bottom_level_acceleration_structures.size() == intermediate_bottom_level_acceleration_structure_index);

                graphics_command_buffer->build_intermediate_bottom_level_acceleration_structure_store(static_cast<uint32_t>(built_intermediate_bottom_level_acceleration_structures.size()), built_intermediate_bottom_level_acceleration_structures.data());
            }

            // build top level acceleration structure
            {
                uint32_t scene_instance_count = 0U;
                for (size_t mesh_index = 0U; mesh_index < this->m_scene_meshes.size(); ++mesh_index)
                {
                    Demo_Mesh const &scene_mesh = this->m_scene_meshes[mesh_index];

                    uint32_t const mesh_instance_count = static_cast<uint32_t>(scene_mesh.m_instances.size());
                    scene_instance_count += mesh_instance_count;
                }
                assert(scene_instance_count <= MAX_INSTANCE_COUNT);

                for (uint32_t frame_throttling_index = 0U; frame_throttling_index < FRAME_THROTTLING_COUNT; ++frame_throttling_index)
                {
                    this->m_scene_top_level_acceleration_structure_instance_upload_buffers[frame_throttling_index] = device->create_top_level_acceleration_structure_instance_upload_buffer(scene_instance_count);
                }

                // we use the 0 index when initializing
                constexpr uint32_t const frame_throttling_index = 0U;

                uint32_t top_level_acceleration_structure_size = -1;
                uint32_t top_level_acceleration_structure_build_scratch_size = -1;
                uint32_t top_level_acceleration_structure_update_scratch_size = -1;
                device->get_top_level_acceleration_structure_size(scene_instance_count, &top_level_acceleration_structure_size, &top_level_acceleration_structure_build_scratch_size, &top_level_acceleration_structure_update_scratch_size);

                this->m_scene_top_level_acceleration_structure = device->create_top_level_acceleration_structure(top_level_acceleration_structure_size);

                uint32_t scene_instance_index = 0U;
                for (size_t mesh_index = 0U; mesh_index < this->m_scene_meshes.size(); ++mesh_index)
                {
                    Demo_Mesh const &scene_mesh = this->m_scene_meshes[mesh_index];

                    if (!scene_mesh.m_skinned)
                    {
                        for (size_t mesh_instance_index = 0U; mesh_instance_index < scene_mesh.m_instances.size(); ++mesh_instance_index)
                        {
                            Demo_Mesh_Instance const &scene_mesh_instance = scene_mesh.m_instances[mesh_instance_index];

                            assert(NULL == scene_mesh_instance.m_intermediate_bottom_level_acceleration_structure);

                            BRX_TOP_LEVEL_ACCELERATION_STRUCTURE_INSTANCE top_level_acceleration_structure_instance = {
                                {},
                                scene_instance_index,
                                0XFFU,
                                true,
                                false,
                                false,
                                false,
                                scene_mesh.m_compacted_bottom_level_acceleration_structure->get_bottom_level_acceleration_structure()};

                            DirectX::XMStoreFloat3x4(reinterpret_cast<DirectX::XMFLOAT3X4 *>(&top_level_acceleration_structure_instance.transform_matrix), DirectX::XMLoadFloat4x4(&scene_mesh_instance.m_model_transform));

                            this->m_scene_top_level_acceleration_structure_instance_upload_buffers[frame_throttling_index]->write_instance(scene_instance_index, &top_level_acceleration_structure_instance);

                            ++scene_instance_index;
                        }
                    }
                    else
                    {
                        assert(NULL == scene_mesh.m_compacted_bottom_level_acceleration_structure);

                        for (size_t mesh_instance_index = 0U; mesh_instance_index < scene_mesh.m_instances.size(); ++mesh_instance_index)
                        {
                            Demo_Mesh_Instance const &scene_mesh_instance = scene_mesh.m_instances[mesh_instance_index];

                            BRX_TOP_LEVEL_ACCELERATION_STRUCTURE_INSTANCE top_level_acceleration_structure_instance = {
                                {},
                                scene_instance_index,
                                0XFFU,
                                true,
                                false,
                                false,
                                false,
                                scene_mesh_instance.m_intermediate_bottom_level_acceleration_structure->get_bottom_level_acceleration_structure()};

                            DirectX::XMStoreFloat3x4(reinterpret_cast<DirectX::XMFLOAT3X4 *>(&top_level_acceleration_structure_instance.transform_matrix), DirectX::XMLoadFloat4x4(&scene_mesh_instance.m_model_transform));

                            this->m_scene_top_level_acceleration_structure_instance_upload_buffers[frame_throttling_index]->write_instance(scene_instance_index, &top_level_acceleration_structure_instance);

                            ++scene_instance_index;
                        }
                    }
                }
                assert(scene_instance_count == scene_instance_index);

                brx_scratch_buffer *const top_level_acceleration_structure_build_scratch_buffer = device->create_scratch_buffer(top_level_acceleration_structure_build_scratch_size);
                scratch_buffers.push_back(top_level_acceleration_structure_build_scratch_buffer);

                graphics_command_buffer->build_top_level_acceleration_structure(this->m_scene_top_level_acceleration_structure, scene_instance_count, this->m_scene_top_level_acceleration_structure_instance_upload_buffers[frame_throttling_index], top_level_acceleration_structure_build_scratch_buffer);

                graphics_command_buffer->build_top_level_acceleration_structure_store(this->m_scene_top_level_acceleration_structure);

                this->m_scene_top_level_acceleration_structure_update_scratch_buffer = device->create_scratch_buffer(top_level_acceleration_structure_update_scratch_size);
            }

            upload_command_buffer->end();

            graphics_command_buffer->end();

            upload_queue->submit_and_signal(upload_command_buffer);

            device->reset_fence(fence);

            graphics_queue->wait_and_submit(upload_command_buffer, graphics_command_buffer, fence);

            device->wait_for_fence(fence);

            if (!scene_non_compacted_bottom_level_acceleration_structures.empty())
            {
                device->destroy_compacted_bottom_level_acceleration_structure_size_query_pool(compacted_bottom_level_acceleration_structure_size_query_pool);
            }
            else
            {
                assert(NULL == compacted_bottom_level_acceleration_structure_size_query_pool);
            }

            for (uint32_t non_compacted_bottom_level_acceleration_structure_index = 0U; non_compacted_bottom_level_acceleration_structure_index < scene_non_compacted_bottom_level_acceleration_structures.size(); ++non_compacted_bottom_level_acceleration_structure_index)
            {
                device->destroy_non_compacted_bottom_level_acceleration_structure(scene_non_compacted_bottom_level_acceleration_structures[non_compacted_bottom_level_acceleration_structure_index]);
            }
            scene_non_compacted_bottom_level_acceleration_structures.clear();

            for (brx_scratch_buffer *const scratch_buffer : scratch_buffers)
            {
                device->destroy_scratch_buffer(scratch_buffer);
            }

            scratch_buffers.clear();
        }

        device->destroy_fence(fence);

        device->destroy_upload_command_buffer(upload_command_buffer);

        device->destroy_graphics_command_buffer(graphics_command_buffer);

        device->destroy_upload_queue(upload_queue);

        device->destroy_graphics_queue(graphics_queue);
    }

    // Sampler
    {
        this->m_sampler = device->create_sampler(BRX_SAMPLER_FILTER_LINEAR);
    }

    // Uniform Buffer
    {
        this->m_uniform_upload_buffer_offset_alignment = device->get_uniform_upload_buffer_offset_alignment();

        this->m_common_gbuffer_pipeline_ambient_occlusion_pipeline_none_update_uniform_buffer = device->create_uniform_upload_buffer(tbb_align_up(static_cast<uint32_t>(sizeof(common_none_update_set_uniform_buffer_binding)), this->m_uniform_upload_buffer_offset_alignment) * FRAME_THROTTLING_COUNT);

        this->m_gbuffer_pipeline_none_update_instance_information_uniform_buffer = device->create_uniform_upload_buffer(tbb_align_up(static_cast<uint32_t>(sizeof(gbuffer_pipeline_none_update_set_instance_information_uniform_buffer_binding)), this->m_uniform_upload_buffer_offset_alignment) * FRAME_THROTTLING_COUNT);

        for (size_t mesh_index = 0U; mesh_index < this->m_scene_meshes.size(); ++mesh_index)
        {
            Demo_Mesh &scene_mesh = this->m_scene_meshes[mesh_index];

            for (size_t instance_index = 0U; instance_index < scene_mesh.m_instances.size(); ++instance_index)
            {
                Demo_Mesh_Instance &scene_mesh_instance = scene_mesh.m_instances[instance_index];

                if (!scene_mesh.m_skinned)
                {
                    scene_mesh_instance.m_skin_pipeline_per_mesh_instance_update_uniform_buffer = NULL;
                }
                else
                {
                    scene_mesh_instance.m_skin_pipeline_per_mesh_instance_update_uniform_buffer = device->create_uniform_upload_buffer(tbb_align_up(static_cast<uint32_t>(sizeof(skin_pipeline_per_mesh_instance_update_set_uniform_buffer_binding)), this->m_uniform_upload_buffer_offset_alignment) * FRAME_THROTTLING_COUNT);
                }
            }
        }
    }

    // Descriptor
    {
        // Skin Pipeline
        {
            for (size_t mesh_index = 0U; mesh_index < this->m_scene_meshes.size(); ++mesh_index)
            {
                Demo_Mesh &scene_mesh = this->m_scene_meshes[mesh_index];

                if (!scene_mesh.m_skinned)
                {
                    for (size_t mesh_instance_index = 0U; mesh_instance_index < scene_mesh.m_instances.size(); ++mesh_instance_index)
                    {
                        Demo_Mesh_Instance &scene_mesh_instance = scene_mesh.m_instances[mesh_instance_index];

                        assert(scene_mesh_instance.m_skinned_subsets.empty());

                        scene_mesh_instance.m_skin_pipeline_per_mesh_instance_update_descriptor_set = NULL;
                    }
                }
                else
                {
                    for (size_t mesh_instance_index = 0U; mesh_instance_index < scene_mesh.m_instances.size(); ++mesh_instance_index)
                    {
                        Demo_Mesh_Instance &scene_mesh_instance = scene_mesh.m_instances[mesh_instance_index];

                        assert(scene_mesh.m_subsets.size() == scene_mesh_instance.m_skinned_subsets.size());

                        for (size_t mesh_subset_index = 0U; mesh_subset_index < scene_mesh_instance.m_skinned_subsets.size(); ++mesh_subset_index)
                        {
                            Demo_Mesh_Subset const &scene_mesh_subset = scene_mesh.m_subsets[mesh_subset_index];

                            Demo_Mesh_Skinned_Subset &scene_mesh_skinned_subset = scene_mesh_instance.m_skinned_subsets[mesh_subset_index];

                            {
                                scene_mesh_skinned_subset.m_skin_pipeline_per_mesh_skinned_subset_update_descriptor_set = device->create_descriptor_set(skin_pipeline_per_mesh_skinned_subset_update_descriptor_set_layout, 0U);

                                brx_read_only_storage_buffer const *const read_only_storage_buffers[] = {
                                    scene_mesh_subset.m_vertex_position_buffer->get_read_only_storage_buffer(),
                                    scene_mesh_subset.m_vertex_varying_buffer->get_read_only_storage_buffer(),
                                    scene_mesh_subset.m_vertex_joint_buffer->get_read_only_storage_buffer()};
                                device->write_descriptor_set(scene_mesh_skinned_subset.m_skin_pipeline_per_mesh_skinned_subset_update_descriptor_set, 0U, BRX_DESCRIPTOR_TYPE_READ_ONLY_STORAGE_BUFFER, 0U, sizeof(read_only_storage_buffers) / sizeof(read_only_storage_buffers[0]), NULL, NULL, read_only_storage_buffers, NULL, NULL, NULL, NULL, NULL);

                                brx_storage_buffer const *const storage_buffers[] = {
                                    scene_mesh_skinned_subset.m_skinned_vertex_position_buffer->get_storage_buffer(),
                                    scene_mesh_skinned_subset.m_skinned_vertex_varying_buffer->get_storage_buffer()};
                                device->write_descriptor_set(scene_mesh_skinned_subset.m_skin_pipeline_per_mesh_skinned_subset_update_descriptor_set, 1U, BRX_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0U, sizeof(storage_buffers) / sizeof(storage_buffers[0]), NULL, NULL, NULL, storage_buffers, NULL, NULL, NULL, NULL);
                            }
                        }

                        {
                            scene_mesh_instance.m_skin_pipeline_per_mesh_instance_update_descriptor_set = device->create_descriptor_set(skin_pipeline_per_mesh_instance_update_descriptor_set_layout, 0U);

                            constexpr uint32_t const dynamic_uniform_buffers_range = sizeof(skin_pipeline_per_mesh_instance_update_set_uniform_buffer_binding);
                            device->write_descriptor_set(scene_mesh_instance.m_skin_pipeline_per_mesh_instance_update_descriptor_set, 0U, BRX_DESCRIPTOR_TYPE_DYNAMIC_UNIFORM_BUFFER, 0U, 1U, &scene_mesh_instance.m_skin_pipeline_per_mesh_instance_update_uniform_buffer, &dynamic_uniform_buffers_range, NULL, NULL, NULL, NULL, NULL, NULL);
                        }
                    }
                }
            }
        }

        // GBuffer Pipeline
        {
            this->m_gbuffer_pipeline_none_update_descriptor_set = device->create_descriptor_set(this->m_gbuffer_pipeline_none_update_descriptor_set_layout, 0U);
            {
                constexpr uint32_t const dynamic_uniform_buffers_range = sizeof(common_none_update_set_uniform_buffer_binding);
                device->write_descriptor_set(this->m_gbuffer_pipeline_none_update_descriptor_set, 0U, BRX_DESCRIPTOR_TYPE_DYNAMIC_UNIFORM_BUFFER, 0U, 1U, &this->m_common_gbuffer_pipeline_ambient_occlusion_pipeline_none_update_uniform_buffer, &dynamic_uniform_buffers_range, NULL, NULL, NULL, NULL, NULL, NULL);
            }
            {
                brx_top_level_acceleration_structure const *const top_level_acceleration_structures[] = {
                    this->m_scene_top_level_acceleration_structure};
                device->write_descriptor_set(this->m_gbuffer_pipeline_none_update_descriptor_set, 1U, BRX_DESCRIPTOR_TYPE_TOP_LEVEL_ACCELERATION_STRUCTURE, 0U, sizeof(top_level_acceleration_structures) / sizeof(top_level_acceleration_structures[0]), NULL, NULL, NULL, NULL, NULL, NULL, NULL, &top_level_acceleration_structures[0]);
            }
            {
                constexpr uint32_t const dynamic_uniform_buffers_range = sizeof(gbuffer_pipeline_none_update_set_instance_information_uniform_buffer_binding);
                device->write_descriptor_set(this->m_gbuffer_pipeline_none_update_descriptor_set, 2U, BRX_DESCRIPTOR_TYPE_DYNAMIC_UNIFORM_BUFFER, 0U, 1U, &this->m_gbuffer_pipeline_none_update_instance_information_uniform_buffer, &dynamic_uniform_buffers_range, NULL, NULL, NULL, NULL, NULL, NULL);
            }
            {
                device->write_descriptor_set(this->m_gbuffer_pipeline_none_update_descriptor_set, 3U, BRX_DESCRIPTOR_TYPE_SAMPLER, 0U, 1U, NULL, NULL, NULL, NULL, NULL, NULL, &this->m_sampler, NULL);
            }

            {
                uint32_t scene_geometry_count = 0U;
                for (size_t mesh_index = 0U; mesh_index < this->m_scene_meshes.size(); ++mesh_index)
                {
                    Demo_Mesh const &scene_mesh = this->m_scene_meshes[mesh_index];

                    if (!scene_mesh.m_skinned)
                    {
                        uint32_t const mesh_geometry_count = static_cast<uint32_t>(scene_mesh.m_subsets.size());
                        scene_geometry_count += mesh_geometry_count;
                    }
                    else
                    {
                        uint32_t const mesh_geometry_count = static_cast<uint32_t>(scene_mesh.m_subsets.size() * scene_mesh.m_instances.size());
                        scene_geometry_count += mesh_geometry_count;
                    }
                }

                mcrt_vector<brx_read_only_storage_buffer const *> read_only_storage_buffers(static_cast<size_t>(PER_MESH_SUBSET_BUFFER_COUNT * scene_geometry_count));
                mcrt_vector<brx_sampled_image const *> sample_images(static_cast<size_t>(PER_MESH_SUBSET_TEXTURE_COUNT * scene_geometry_count));

                uint32_t scene_geometry_index = 0U;
                for (size_t mesh_index = 0U; mesh_index < this->m_scene_meshes.size(); ++mesh_index)
                {
                    Demo_Mesh const &scene_mesh = this->m_scene_meshes[mesh_index];

                    if (!scene_mesh.m_skinned)
                    {
                        for (size_t mesh_subset_index = 0U; mesh_subset_index < scene_mesh.m_subsets.size(); ++mesh_subset_index)
                        {
                            Demo_Mesh_Subset const &scene_mesh_subset = scene_mesh.m_subsets[mesh_subset_index];

                            read_only_storage_buffers[PER_MESH_SUBSET_BUFFER_COUNT * scene_geometry_index] = scene_mesh_subset.m_vertex_position_buffer->get_read_only_storage_buffer();
                            read_only_storage_buffers[PER_MESH_SUBSET_BUFFER_COUNT * scene_geometry_index + 1U] = scene_mesh_subset.m_vertex_varying_buffer->get_read_only_storage_buffer();
                            read_only_storage_buffers[PER_MESH_SUBSET_BUFFER_COUNT * scene_geometry_index + 2U] = scene_mesh_subset.m_index_buffer->get_read_only_storage_buffer();
                            read_only_storage_buffers[PER_MESH_SUBSET_BUFFER_COUNT * scene_geometry_index + 3U] = scene_mesh_subset.m_information_buffer->get_read_only_storage_buffer();

                            sample_images[PER_MESH_SUBSET_TEXTURE_COUNT * scene_geometry_index] = (NULL != scene_mesh_subset.m_normal_texture) ? scene_mesh_subset.m_normal_texture->get_sampled_image() : this->m_place_holder_texture->get_sampled_image();
                            sample_images[PER_MESH_SUBSET_TEXTURE_COUNT * scene_geometry_index + 1U] = (NULL != scene_mesh_subset.m_emissive_texture) ? scene_mesh_subset.m_emissive_texture->get_sampled_image() : this->m_place_holder_texture->get_sampled_image();
                            sample_images[PER_MESH_SUBSET_TEXTURE_COUNT * scene_geometry_index + 2U] = (NULL != scene_mesh_subset.m_base_color_texture) ? scene_mesh_subset.m_base_color_texture->get_sampled_image() : this->m_place_holder_texture->get_sampled_image();
                            sample_images[PER_MESH_SUBSET_TEXTURE_COUNT * scene_geometry_index + 3U] = (NULL != scene_mesh_subset.m_metallic_roughness_texture) ? scene_mesh_subset.m_metallic_roughness_texture->get_sampled_image() : this->m_place_holder_texture->get_sampled_image();

                            ++scene_geometry_index;
                        }
                    }
                    else
                    {
                        for (size_t mesh_instance_index = 0U; mesh_instance_index < scene_mesh.m_instances.size(); ++mesh_instance_index)
                        {
                            Demo_Mesh_Instance const &scene_mesh_instance = scene_mesh.m_instances[mesh_instance_index];

                            assert(scene_mesh.m_subsets.size() == scene_mesh_instance.m_skinned_subsets.size());

                            for (size_t mesh_subset_index = 0U; mesh_subset_index < scene_mesh_instance.m_skinned_subsets.size(); ++mesh_subset_index)
                            {
                                Demo_Mesh_Skinned_Subset const &scene_mesh_skin_subset = scene_mesh_instance.m_skinned_subsets[mesh_subset_index];

                                Demo_Mesh_Subset const &scene_mesh_subset = scene_mesh.m_subsets[mesh_subset_index];

                                read_only_storage_buffers[PER_MESH_SUBSET_BUFFER_COUNT * scene_geometry_index] = scene_mesh_skin_subset.m_skinned_vertex_position_buffer->get_read_only_storage_buffer();
                                read_only_storage_buffers[PER_MESH_SUBSET_BUFFER_COUNT * scene_geometry_index + 1U] = scene_mesh_skin_subset.m_skinned_vertex_varying_buffer->get_read_only_storage_buffer();
                                read_only_storage_buffers[PER_MESH_SUBSET_BUFFER_COUNT * scene_geometry_index + 2U] = scene_mesh_subset.m_index_buffer->get_read_only_storage_buffer();
                                read_only_storage_buffers[PER_MESH_SUBSET_BUFFER_COUNT * scene_geometry_index + 3U] = scene_mesh_subset.m_information_buffer->get_read_only_storage_buffer();

                                sample_images[PER_MESH_SUBSET_TEXTURE_COUNT * scene_geometry_index] = (NULL != scene_mesh_subset.m_normal_texture) ? scene_mesh_subset.m_normal_texture->get_sampled_image() : this->m_place_holder_texture->get_sampled_image();
                                sample_images[PER_MESH_SUBSET_TEXTURE_COUNT * scene_geometry_index + 1U] = (NULL != scene_mesh_subset.m_emissive_texture) ? scene_mesh_subset.m_emissive_texture->get_sampled_image() : this->m_place_holder_texture->get_sampled_image();
                                sample_images[PER_MESH_SUBSET_TEXTURE_COUNT * scene_geometry_index + 2U] = (NULL != scene_mesh_subset.m_base_color_texture) ? scene_mesh_subset.m_base_color_texture->get_sampled_image() : this->m_place_holder_texture->get_sampled_image();
                                sample_images[PER_MESH_SUBSET_TEXTURE_COUNT * scene_geometry_index + 3U] = (NULL != scene_mesh_subset.m_metallic_roughness_texture) ? scene_mesh_subset.m_metallic_roughness_texture->get_sampled_image() : this->m_place_holder_texture->get_sampled_image();

                                ++scene_geometry_index;
                            }
                        }
                    }
                }
                assert(scene_geometry_count == scene_geometry_index);

                this->m_gbuffer_pipeline_none_update_bindless_buffer_descriptor_set = device->create_descriptor_set(gbuffer_pipeline_none_update_bindless_buffer_descriptor_set_layout, PER_MESH_SUBSET_BUFFER_COUNT * scene_geometry_count);
                this->m_gbuffer_pipeline_none_update_bindless_texture_descriptor_set = device->create_descriptor_set(gbuffer_pipeline_none_update_bindless_texture_descriptor_set_layout, PER_MESH_SUBSET_TEXTURE_COUNT * scene_geometry_count);

                device->write_descriptor_set(this->m_gbuffer_pipeline_none_update_bindless_buffer_descriptor_set, 0U, BRX_DESCRIPTOR_TYPE_READ_ONLY_STORAGE_BUFFER, 0U, static_cast<uint32_t>(read_only_storage_buffers.size()), NULL, NULL, read_only_storage_buffers.data(), NULL, NULL, NULL, NULL, NULL);

                device->write_descriptor_set(this->m_gbuffer_pipeline_none_update_bindless_texture_descriptor_set, 0U, BRX_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 0U, static_cast<uint32_t>(sample_images.size()), NULL, NULL, NULL, NULL, sample_images.data(), NULL, NULL, NULL);
            }
        }

        // Ambient Occlusion Pipeline
        {
            this->m_ambient_occlusion_pipeline_none_update_descriptor_set = device->create_descriptor_set(this->m_ambient_occlusion_pipeline_none_update_descriptor_set_layout, 0U);
            {
                constexpr uint32_t const dynamic_uniform_buffers_range = sizeof(common_none_update_set_uniform_buffer_binding);
                device->write_descriptor_set(this->m_ambient_occlusion_pipeline_none_update_descriptor_set, 0U, BRX_DESCRIPTOR_TYPE_DYNAMIC_UNIFORM_BUFFER, 0U, 1U, &this->m_common_gbuffer_pipeline_ambient_occlusion_pipeline_none_update_uniform_buffer, &dynamic_uniform_buffers_range, NULL, NULL, NULL, NULL, NULL, NULL);
            }
            {
                brx_top_level_acceleration_structure const *const top_level_acceleration_structures[] = {
                    this->m_scene_top_level_acceleration_structure};
                device->write_descriptor_set(this->m_ambient_occlusion_pipeline_none_update_descriptor_set, 1U, BRX_DESCRIPTOR_TYPE_TOP_LEVEL_ACCELERATION_STRUCTURE, 0U, sizeof(top_level_acceleration_structures) / sizeof(top_level_acceleration_structures[0]), NULL, NULL, NULL, NULL, NULL, NULL, NULL, &top_level_acceleration_structures[0]);
            }
        }
    }

    device->destroy_descriptor_set_layout(skin_pipeline_per_mesh_instance_update_descriptor_set_layout);
    skin_pipeline_per_mesh_instance_update_descriptor_set_layout = NULL;
    device->destroy_descriptor_set_layout(skin_pipeline_per_mesh_skinned_subset_update_descriptor_set_layout);
    skin_pipeline_per_mesh_skinned_subset_update_descriptor_set_layout = NULL;
    device->destroy_descriptor_set_layout(gbuffer_pipeline_none_update_bindless_buffer_descriptor_set_layout);
    gbuffer_pipeline_none_update_bindless_buffer_descriptor_set_layout = NULL;
    device->destroy_descriptor_set_layout(gbuffer_pipeline_none_update_bindless_texture_descriptor_set_layout);
    gbuffer_pipeline_none_update_bindless_texture_descriptor_set_layout = NULL;

    // Init Animation Time
    this->m_animation_time = 0.0F;

    // Init User Input Model
    out_ui_model->m_ambient_occlusion_max_distance = 0.2F;
    out_ui_model->m_ambient_occlusion_sample_count = 32;

    out_user_camera_model->m_eye_position[0] = 0.0F;
    out_user_camera_model->m_eye_position[1] = 0.5F;
    out_user_camera_model->m_eye_position[2] = 5.0F;
    out_user_camera_model->m_eye_direction[0] = 0.0F;
    out_user_camera_model->m_eye_direction[1] = 0.0523359776F;
    out_user_camera_model->m_eye_direction[2] = -0.998629570F;
    out_user_camera_model->m_up_direction[0] = 0.0F;
    out_user_camera_model->m_up_direction[1] = 1.0F;
    out_user_camera_model->m_up_direction[2] = 0.0F;
    out_user_camera_model->m_fov = 0.7F;
    out_user_camera_model->m_near = 0.01F;
    out_user_camera_model->m_far = 1000.0F;
}

void scene_renderer::destroy(brx_device *device)
{
    // Descriptor
    {
        // Skin Pipeline
        {
            for (size_t mesh_index = 0U; mesh_index < this->m_scene_meshes.size(); ++mesh_index)
            {
                Demo_Mesh &scene_mesh = this->m_scene_meshes[mesh_index];

                if (!scene_mesh.m_skinned)
                {
                    for (size_t mesh_instance_index = 0U; mesh_instance_index < scene_mesh.m_instances.size(); ++mesh_instance_index)
                    {
                        Demo_Mesh_Instance &scene_mesh_instance = scene_mesh.m_instances[mesh_instance_index];

                        assert(scene_mesh_instance.m_skinned_subsets.empty());

                        assert(NULL == scene_mesh_instance.m_skin_pipeline_per_mesh_instance_update_descriptor_set);
                    }
                }
                else
                {
                    for (size_t mesh_instance_index = 0U; mesh_instance_index < scene_mesh.m_instances.size(); ++mesh_instance_index)
                    {
                        Demo_Mesh_Instance &scene_mesh_instance = scene_mesh.m_instances[mesh_instance_index];

                        assert(scene_mesh.m_subsets.size() == scene_mesh_instance.m_skinned_subsets.size());

                        for (size_t mesh_subset_index = 0U; mesh_subset_index < scene_mesh_instance.m_skinned_subsets.size(); ++mesh_subset_index)
                        {
                            Demo_Mesh_Skinned_Subset &scene_mesh_skinned_subset = scene_mesh_instance.m_skinned_subsets[mesh_subset_index];

                            device->destroy_descriptor_set(scene_mesh_skinned_subset.m_skin_pipeline_per_mesh_skinned_subset_update_descriptor_set);
                        }

                        device->destroy_descriptor_set(scene_mesh_instance.m_skin_pipeline_per_mesh_instance_update_descriptor_set);
                    }
                }
            }
        }

        // GBuffer Pipeline
        {
            device->destroy_descriptor_set(this->m_gbuffer_pipeline_none_update_descriptor_set);

            device->destroy_descriptor_set(this->m_gbuffer_pipeline_none_update_bindless_buffer_descriptor_set);

            device->destroy_descriptor_set(this->m_gbuffer_pipeline_none_update_bindless_texture_descriptor_set);

            device->destroy_descriptor_set_layout(this->m_gbuffer_pipeline_none_update_descriptor_set_layout);
        }

        // Ambient Occlusion Pipeline
        {
            device->destroy_descriptor_set(this->m_ambient_occlusion_pipeline_none_update_descriptor_set);

            device->destroy_descriptor_set_layout(this->m_ambient_occlusion_pipeline_none_update_descriptor_set_layout);
        }
    }

    // Uniform Buffer
    {
        device->destroy_uniform_upload_buffer(this->m_common_gbuffer_pipeline_ambient_occlusion_pipeline_none_update_uniform_buffer);

        device->destroy_uniform_upload_buffer(this->m_gbuffer_pipeline_none_update_instance_information_uniform_buffer);

        for (size_t mesh_index = 0U; mesh_index < this->m_scene_meshes.size(); ++mesh_index)
        {
            Demo_Mesh &scene_mesh = this->m_scene_meshes[mesh_index];

            for (size_t instance_index = 0U; instance_index < scene_mesh.m_instances.size(); ++instance_index)
            {
                Demo_Mesh_Instance &scene_mesh_instance = scene_mesh.m_instances[instance_index];

                if (!scene_mesh.m_skinned)
                {
                    assert(NULL == scene_mesh_instance.m_skin_pipeline_per_mesh_instance_update_uniform_buffer);
                }
                else
                {
                    device->destroy_uniform_upload_buffer(scene_mesh_instance.m_skin_pipeline_per_mesh_instance_update_uniform_buffer);
                }
            }
        }
    }

    // Sampler
    {
        device->destroy_sampler(this->m_sampler);
    }

    // Place Holder Buffer
    {
        device->destroy_storage_asset_buffer(this->m_place_holder_buffer);
    }

    // Place Holder Texture
    {
        device->destroy_sampled_asset_image(this->m_place_holder_texture);
    }

    // Asset
    {
        for (size_t scene_texture_index = 0U; scene_texture_index < this->m_scene_textures.size(); ++scene_texture_index)
        {
            brx_sampled_asset_image *const material_texture = this->m_scene_textures[scene_texture_index];

            assert(NULL != material_texture);
            device->destroy_sampled_asset_image(material_texture);
        }

        for (size_t mesh_index = 0U; mesh_index < this->m_scene_meshes.size(); ++mesh_index)
        {
            Demo_Mesh &scene_mesh = this->m_scene_meshes[mesh_index];

            for (size_t subset_index = 0U; subset_index < scene_mesh.m_subsets.size(); ++subset_index)
            {
                Demo_Mesh_Subset &scene_mesh_subset = scene_mesh.m_subsets[subset_index];

                device->destroy_storage_asset_buffer(scene_mesh_subset.m_vertex_position_buffer);

                device->destroy_storage_asset_buffer(scene_mesh_subset.m_vertex_varying_buffer);

                if (!scene_mesh.m_skinned)
                {
                    assert(NULL == scene_mesh_subset.m_vertex_joint_buffer);
                }
                else
                {
                    device->destroy_storage_asset_buffer(scene_mesh_subset.m_vertex_joint_buffer);
                }

                device->destroy_storage_asset_buffer(scene_mesh_subset.m_index_buffer);

                device->destroy_storage_asset_buffer(scene_mesh_subset.m_information_buffer);
            }

            if (!scene_mesh.m_skinned)
            {
                device->destroy_compacted_bottom_level_acceleration_structure(scene_mesh.m_compacted_bottom_level_acceleration_structure);

                for (size_t instance_index = 0U; instance_index < scene_mesh.m_instances.size(); ++instance_index)
                {
                    Demo_Mesh_Instance &scene_mesh_instance = scene_mesh.m_instances[instance_index];

                    assert(scene_mesh_instance.m_skinned_subsets.empty());

                    assert(NULL == scene_mesh_instance.m_intermediate_bottom_level_acceleration_structure);

                    assert(NULL == scene_mesh_instance.m_intermediate_bottom_level_acceleration_structure_update_scratch_buffer);
                }
            }
            else
            {
                assert(NULL == scene_mesh.m_compacted_bottom_level_acceleration_structure);

                for (size_t instance_index = 0U; instance_index < scene_mesh.m_instances.size(); ++instance_index)
                {
                    Demo_Mesh_Instance &scene_mesh_instance = scene_mesh.m_instances[instance_index];

                    for (size_t subset_index = 0U; subset_index < scene_mesh.m_subsets.size(); ++subset_index)
                    {
                        Demo_Mesh_Skinned_Subset &scene_mesh_skinned_subset = scene_mesh_instance.m_skinned_subsets[subset_index];

                        device->destroy_storage_intermediate_buffer(scene_mesh_skinned_subset.m_skinned_vertex_position_buffer);

                        device->destroy_storage_intermediate_buffer(scene_mesh_skinned_subset.m_skinned_vertex_varying_buffer);
                    }

                    device->destroy_intermediate_bottom_level_acceleration_structure(scene_mesh_instance.m_intermediate_bottom_level_acceleration_structure);

                    device->destroy_scratch_buffer(scene_mesh_instance.m_intermediate_bottom_level_acceleration_structure_update_scratch_buffer);
                }
            }
        }
    }

    // Top Level Acceleration Structure
    {
        device->destroy_top_level_acceleration_structure(this->m_scene_top_level_acceleration_structure);

        for (uint32_t frame_throttling_index = 0U; frame_throttling_index < FRAME_THROTTLING_COUNT; ++frame_throttling_index)
        {
            device->destroy_top_level_acceleration_structure_instance_upload_buffer(this->m_scene_top_level_acceleration_structure_instance_upload_buffers[frame_throttling_index]);
        }

        device->destroy_scratch_buffer(this->m_scene_top_level_acceleration_structure_update_scratch_buffer);
    }

    // Render Pass and Pipeline
    {
        device->destroy_compute_pipeline(this->m_ambient_occlusion_pipeline);

        device->destroy_pipeline_layout(this->m_ambient_occlusion_pipeline_layout);

        device->destroy_compute_pipeline(this->m_gbuffer_pipeline);

        device->destroy_pipeline_layout(this->m_gbuffer_pipeline_layout);

        device->destroy_compute_pipeline(this->m_skin_pipeline);

        device->destroy_pipeline_layout(this->m_skin_pipeline_layout);
    }
}

void scene_renderer::on_swap_chain_attach(brx_device *device, uint32_t swap_chain_image_width, uint32_t swap_chain_image_height)
{
    // Intermediate Images
    {
        assert(0U == this->m_intermediate_width);
        this->m_intermediate_width = swap_chain_image_width;

        assert(0U == this->m_intermediate_height);
        this->m_intermediate_height = swap_chain_image_height;

        assert(NULL == this->m_gbuffer_depth_image);
        this->m_gbuffer_depth_image = device->create_storage_image(BRX_STORAGE_IMAGE_FORMAT_R32_UINT, this->m_intermediate_width, this->m_intermediate_height, true);

        assert(NULL == this->m_gbuffer_normal_image);
        this->m_gbuffer_normal_image = device->create_storage_image(BRX_STORAGE_IMAGE_FORMAT_R32_UINT, this->m_intermediate_width, this->m_intermediate_height, true);

        assert(NULL == this->m_ambient_occlusion_image);
        this->m_ambient_occlusion_image = device->create_storage_image(BRX_STORAGE_IMAGE_FORMAT_R16_SFLOAT, this->m_intermediate_width, this->m_intermediate_height, true);
    }

    // Descriptor
    {
        // GBuffer Pipeline
        {
            // The VkDescriptorSetLayout should still be valid when perform write update on VkDescriptorSet
            assert(NULL != this->m_gbuffer_pipeline_none_update_descriptor_set_layout);
            {
                brx_storage_image const *const storage_images[] = {
                    this->m_gbuffer_depth_image,
                    this->m_gbuffer_normal_image};
                device->write_descriptor_set(this->m_gbuffer_pipeline_none_update_descriptor_set, 4U, BRX_DESCRIPTOR_TYPE_STORAGE_IMAGE, 0U, sizeof(storage_images) / sizeof(storage_images[0]), NULL, NULL, NULL, NULL, NULL, &storage_images[0], NULL, NULL);
            }
        }

        // Ambient Occlusion Pipeline
        {
            // The VkDescriptorSetLayout should still be valid when perform write update on VkDescriptorSet
            assert(NULL != this->m_ambient_occlusion_pipeline_none_update_descriptor_set_layout);
            {
                brx_sampled_image const *const sampled_images[] = {
                    this->m_gbuffer_depth_image->get_sampled_image(),
                    this->m_gbuffer_normal_image->get_sampled_image()};
                device->write_descriptor_set(this->m_ambient_occlusion_pipeline_none_update_descriptor_set, 2U, BRX_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 0U, sizeof(sampled_images) / sizeof(sampled_images[0]), NULL, NULL, NULL, NULL, sampled_images, NULL, NULL, NULL);
            }
            {
                brx_storage_image const *const storage_images[] = {
                    this->m_ambient_occlusion_image};
                device->write_descriptor_set(this->m_ambient_occlusion_pipeline_none_update_descriptor_set, 3U, BRX_DESCRIPTOR_TYPE_STORAGE_IMAGE, 0U, sizeof(storage_images) / sizeof(storage_images[0]), NULL, NULL, NULL, NULL, NULL, &storage_images[0], NULL, NULL);
            }
        }
    }
}

brx_sampled_image const *scene_renderer::get_scene_color_image()
{
    return this->m_ambient_occlusion_image->get_sampled_image();
}

void scene_renderer::on_swap_chain_detach(brx_device *device)
{
    // Intermediate Images
    {
        assert(NULL != this->m_gbuffer_depth_image);
        device->destroy_storage_image(this->m_gbuffer_depth_image);
        this->m_gbuffer_depth_image = NULL;

        assert(NULL != this->m_gbuffer_normal_image);
        device->destroy_storage_image(this->m_gbuffer_normal_image);
        this->m_gbuffer_normal_image = NULL;

        assert(NULL != this->m_ambient_occlusion_image);
        device->destroy_storage_image(this->m_ambient_occlusion_image);
        this->m_ambient_occlusion_image = NULL;

        assert(0U != this->m_intermediate_width);
        this->m_intermediate_width = 0U;

        assert(0U != this->m_intermediate_height);
        this->m_intermediate_height = 0U;
    }
}

void scene_renderer::draw(brx_graphics_command_buffer *command_buffer, uint32_t frame_throttling_index, float interval_time, ui_model_t const *ui_model, user_camera_model_t const *user_camera_model)
{
    // Update Uniform Buffer
    uint32_t scene_instance_count;
    uint32_t scene_geometry_count;
    {
        // Skin Pipeline - Per Mesh Instance Update
        {
            this->m_animation_time += interval_time;

            size_t animetion_frame_index = static_cast<size_t>(animation_frame_rate * this->m_animation_time);

            for (size_t mesh_index = 0U; mesh_index < this->m_scene_meshes.size(); ++mesh_index)
            {
                Demo_Mesh const &scene_mesh = this->m_scene_meshes[mesh_index];

                if (scene_mesh.m_skinned)
                {
                    for (size_t instance_index = 0U; instance_index < scene_mesh.m_instances.size(); ++instance_index)
                    {
                        Demo_Mesh_Instance const &scene_mesh_instance = scene_mesh.m_instances[instance_index];

                        skin_pipeline_per_mesh_instance_update_set_uniform_buffer_binding *const skin_pipeline_per_mesh_instance_update_set_uniform_buffer_binding_destination = reinterpret_cast<skin_pipeline_per_mesh_instance_update_set_uniform_buffer_binding *>(reinterpret_cast<uintptr_t>(scene_mesh_instance.m_skin_pipeline_per_mesh_instance_update_uniform_buffer->get_host_memory_range_base()) + tbb_align_up(static_cast<uint32_t>(sizeof(skin_pipeline_per_mesh_instance_update_set_uniform_buffer_binding)), this->m_uniform_upload_buffer_offset_alignment) * frame_throttling_index);

                        scene_animation_pose const &pose = scene_mesh_instance.m_animation_skeleton.get_pose(animetion_frame_index);

                        assert(pose.get_joint_count() <= MAX_JOINT_COUNT);

                        for (uint32_t joint_index = 0U; joint_index < pose.get_joint_count(); ++joint_index)
                        {
                            unit_dual_quaternion_from_rigid_transform(&skin_pipeline_per_mesh_instance_update_set_uniform_buffer_binding_destination->g_dual_quaternions[2 * joint_index], pose.get_quaternion(joint_index), pose.get_translation(joint_index));
                        }
                    }
                }
            }
        }

        // Common - None Update (update frequency denotes the updating of the resource bindings)
        {
            common_none_update_set_uniform_buffer_binding *const common_none_update_set_uniform_buffer_binding_destination = reinterpret_cast<common_none_update_set_uniform_buffer_binding *>(reinterpret_cast<uintptr_t>(this->m_common_gbuffer_pipeline_ambient_occlusion_pipeline_none_update_uniform_buffer->get_host_memory_range_base()) + tbb_align_up(static_cast<uint32_t>(sizeof(common_none_update_set_uniform_buffer_binding)), this->m_uniform_upload_buffer_offset_alignment) * frame_throttling_index);

            DirectX::XMFLOAT3 const eye_position(user_camera_model->m_eye_position[0], user_camera_model->m_eye_position[1], user_camera_model->m_eye_position[2]);
            DirectX::XMFLOAT3 const eye_direction(user_camera_model->m_eye_direction[0], user_camera_model->m_eye_direction[1], user_camera_model->m_eye_direction[2]);
            DirectX::XMFLOAT3 const up_direction(user_camera_model->m_up_direction[0], user_camera_model->m_up_direction[1], user_camera_model->m_up_direction[2]);
            DirectX::XMMATRIX view_transform = DirectX::XMMatrixLookToRH(DirectX::XMLoadFloat3(&eye_position), DirectX::XMLoadFloat3(&eye_direction), DirectX::XMLoadFloat3(&up_direction));

            float const aspect = static_cast<float>(this->m_intermediate_width) / static_cast<float>(this->m_intermediate_height);
            DirectX::XMMATRIX projection_transform = DirectX_Math_Matrix_PerspectiveFovRH_ReversedZ(user_camera_model->m_fov, aspect, user_camera_model->m_near, user_camera_model->m_far);

            DirectX::XMStoreFloat4x4(&common_none_update_set_uniform_buffer_binding_destination->g_view_transform, view_transform);
            DirectX::XMStoreFloat4x4(&common_none_update_set_uniform_buffer_binding_destination->g_projection_transform, projection_transform);
            DirectX::XMStoreFloat4x4(&common_none_update_set_uniform_buffer_binding_destination->g_inverse_view_transform, DirectX::XMMatrixInverse(NULL, view_transform));
            DirectX::XMStoreFloat4x4(&common_none_update_set_uniform_buffer_binding_destination->g_inverse_projection_transform, DirectX::XMMatrixInverse(NULL, projection_transform));

            common_none_update_set_uniform_buffer_binding_destination->g_screen_width = static_cast<float>(this->m_intermediate_width);
            common_none_update_set_uniform_buffer_binding_destination->g_screen_height = static_cast<float>(this->m_intermediate_height);

            common_none_update_set_uniform_buffer_binding_destination->g_ambient_occlusion_max_distance = ui_model->m_ambient_occlusion_max_distance;
            common_none_update_set_uniform_buffer_binding_destination->g_ambient_occlusion_sample_count = static_cast<float>(ui_model->m_ambient_occlusion_sample_count);
        }

        // GBuffer Pipeline - None Update
        {
            gbuffer_pipeline_none_update_set_instance_information_uniform_buffer_binding *const gbuffer_pipeline_none_update_set_instance_information_uniform_buffer_binding_destination = reinterpret_cast<gbuffer_pipeline_none_update_set_instance_information_uniform_buffer_binding *>(reinterpret_cast<uintptr_t>(this->m_gbuffer_pipeline_none_update_instance_information_uniform_buffer->get_host_memory_range_base()) + tbb_align_up(static_cast<uint32_t>(sizeof(gbuffer_pipeline_none_update_set_instance_information_uniform_buffer_binding)), this->m_uniform_upload_buffer_offset_alignment) * frame_throttling_index);

            uint32_t scene_instance_index = 0U;
            uint32_t scene_geometry_index = 0U;
            for (size_t mesh_index = 0U; mesh_index < this->m_scene_meshes.size(); ++mesh_index)
            {
                Demo_Mesh const &scene_mesh = this->m_scene_meshes[mesh_index];

                if (!scene_mesh.m_skinned)
                {
                    for (size_t mesh_instance_index = 0U; mesh_instance_index < scene_mesh.m_instances.size(); ++mesh_instance_index)
                    {
                        gbuffer_pipeline_none_update_set_instance_information_uniform_buffer_binding_destination->g_instance_information[scene_instance_index].m_global_geometry_index_offset = scene_geometry_index;

                        ++scene_instance_index;
                    }

                    uint32_t const mesh_geometry_count = static_cast<uint32_t>(scene_mesh.m_subsets.size());
                    scene_geometry_index += mesh_geometry_count;
                }
                else
                {
                    for (size_t mesh_instance_index = 0U; mesh_instance_index < scene_mesh.m_instances.size(); ++mesh_instance_index)
                    {
                        Demo_Mesh_Instance const &scene_mesh_instance = scene_mesh.m_instances[mesh_instance_index];

                        assert(scene_mesh.m_subsets.size() == scene_mesh_instance.m_skinned_subsets.size());

                        gbuffer_pipeline_none_update_set_instance_information_uniform_buffer_binding_destination->g_instance_information[scene_instance_index].m_global_geometry_index_offset = scene_geometry_index;

                        ++scene_instance_index;

                        uint32_t const mesh_geometry_count = static_cast<uint32_t>(scene_mesh_instance.m_skinned_subsets.size());
                        scene_geometry_index += mesh_geometry_count;
                    }
                }
            }
            assert(scene_instance_index <= MAX_INSTANCE_COUNT);
            scene_instance_count = scene_instance_index;
            scene_geometry_count = scene_geometry_index;
        }
    }

    // Skin Pass
    {
        mcrt_vector<brx_storage_buffer const *> skin_pipeline_buffers;
        mcrt_vector<brx_descriptor_set *> skin_pipeline_descriptor_sets;
        mcrt_vector<uint32_t> skin_pipeline_vertex_counts;

        for (size_t mesh_index = 0U; mesh_index < this->m_scene_meshes.size(); ++mesh_index)
        {
            Demo_Mesh const &scene_mesh = this->m_scene_meshes[mesh_index];

            if (scene_mesh.m_skinned)
            {
                for (size_t instance_index = 0U; instance_index < scene_mesh.m_instances.size(); ++instance_index)
                {
                    Demo_Mesh_Instance const &scene_mesh_instance = scene_mesh.m_instances[instance_index];

                    assert(scene_mesh.m_subsets.size() == scene_mesh_instance.m_skinned_subsets.size());

                    for (size_t subset_index = 0U; subset_index < scene_mesh_instance.m_skinned_subsets.size(); ++subset_index)
                    {
                        Demo_Mesh_Subset const &scene_mesh_subset = scene_mesh.m_subsets[subset_index];

                        Demo_Mesh_Skinned_Subset const &scene_mesh_skinned_subset = scene_mesh_instance.m_skinned_subsets[subset_index];

                        skin_pipeline_buffers.push_back(scene_mesh_skinned_subset.m_skinned_vertex_position_buffer->get_storage_buffer());
                        skin_pipeline_buffers.push_back(scene_mesh_skinned_subset.m_skinned_vertex_varying_buffer->get_storage_buffer());

                        skin_pipeline_descriptor_sets.push_back(scene_mesh_instance.m_skin_pipeline_per_mesh_instance_update_descriptor_set);
                        skin_pipeline_descriptor_sets.push_back(scene_mesh_skinned_subset.m_skin_pipeline_per_mesh_skinned_subset_update_descriptor_set);

                        skin_pipeline_vertex_counts.push_back(scene_mesh_subset.m_vertex_count);
                    }
                }
            }
        }
        assert(skin_pipeline_buffers.size() == 2U * skin_pipeline_vertex_counts.size());
        assert(skin_pipeline_descriptor_sets.size() == 2U * skin_pipeline_vertex_counts.size());

        size_t const mesh_skinned_subset_count = skin_pipeline_vertex_counts.size();

        if (mesh_skinned_subset_count > 0U)
        {
            command_buffer->begin_debug_utils_label("Skin Pass");

            mcrt_vector<BRX_COMPUTE_PASS_STORAGE_BUFFER_LOAD_OPERATION> skinned_buffer_load_operations(skin_pipeline_buffers.size(), BRX_COMPUTE_PASS_STORAGE_BUFFER_LOAD_OPERATION_DONT_CARE);
            command_buffer->compute_pass_load(static_cast<uint32_t>(skin_pipeline_buffers.size()), skin_pipeline_buffers.data(), skinned_buffer_load_operations.data(), 0U, NULL, NULL);

            command_buffer->bind_compute_pipeline(this->m_skin_pipeline);

            for (size_t subset_index = 0U; subset_index < mesh_skinned_subset_count; ++subset_index)
            {
                brx_descriptor_set *const descritor_sets[] = {
                    skin_pipeline_descriptor_sets[2U * subset_index],
                    skin_pipeline_descriptor_sets[2U * subset_index + 1U]};

                uint32_t const dynamic_offsets[] = {
                    tbb_align_up(static_cast<uint32_t>(sizeof(skin_pipeline_per_mesh_instance_update_set_uniform_buffer_binding)), this->m_uniform_upload_buffer_offset_alignment) * frame_throttling_index};

                command_buffer->bind_compute_descriptor_sets(this->m_skin_pipeline_layout, sizeof(descritor_sets) / sizeof(descritor_sets[0]), descritor_sets, sizeof(dynamic_offsets) / sizeof(dynamic_offsets[0]), dynamic_offsets);

                uint32_t const vertex_count = skin_pipeline_vertex_counts[subset_index];

                uint32_t const group_count_x = (vertex_count <= MAX_SKIN_COMPUTE_DISPATCH_THREAD_GROUPS_PER_DIMENSION) ? vertex_count : MAX_SKIN_COMPUTE_DISPATCH_THREAD_GROUPS_PER_DIMENSION;
                uint32_t const group_count_y = (vertex_count + MAX_SKIN_COMPUTE_DISPATCH_THREAD_GROUPS_PER_DIMENSION - 1U) / MAX_SKIN_COMPUTE_DISPATCH_THREAD_GROUPS_PER_DIMENSION;
                assert(group_count_x <= MAX_SKIN_COMPUTE_DISPATCH_THREAD_GROUPS_PER_DIMENSION);
                assert(group_count_y <= MAX_SKIN_COMPUTE_DISPATCH_THREAD_GROUPS_PER_DIMENSION);

                command_buffer->dispatch(group_count_x, group_count_y, 1U);
            }

            mcrt_vector<BRX_COMPUTE_PASS_STORAGE_BUFFER_STORE_OPERATION> skinned_buffer_store_operations(skin_pipeline_buffers.size(), BRX_COMPUTE_PASS_STORAGE_BUFFER_STORE_OPERATION_FLUSH_FOR_READ_ONLY_STORAGE_BUFFER_AND_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BUFFER);
            command_buffer->compute_pass_store(static_cast<uint32_t>(skin_pipeline_buffers.size()), skin_pipeline_buffers.data(), skinned_buffer_store_operations.data(), 0U, NULL, NULL);

            command_buffer->end_debug_utils_label();
        }
    }

    // Update Bottom Level Acceleration Structure Pass
    {
        command_buffer->begin_debug_utils_label("Update Bottom Level Acceleration Structure Pass");

        mcrt_vector<brx_intermediate_bottom_level_acceleration_structure *> update_intermediate_bottom_level_acceleration_structures;

        uint32_t intermediate_bottom_level_acceleration_structure_index = 0U;
        for (size_t mesh_index = 0U; mesh_index < this->m_scene_meshes.size(); ++mesh_index)
        {
            Demo_Mesh &scene_mesh = this->m_scene_meshes[mesh_index];

            if (!scene_mesh.m_skinned)
            {
                for (size_t mesh_instance_index = 0U; mesh_instance_index < scene_mesh.m_instances.size(); ++mesh_instance_index)
                {
                    Demo_Mesh_Instance &scene_mesh_instance = scene_mesh.m_instances[mesh_instance_index];

                    assert(scene_mesh_instance.m_skinned_subsets.empty());

                    assert(NULL == scene_mesh_instance.m_intermediate_bottom_level_acceleration_structure);

                    assert(NULL == scene_mesh_instance.m_intermediate_bottom_level_acceleration_structure_update_scratch_buffer);
                }
            }
            else
            {
                for (size_t mesh_instance_index = 0U; mesh_instance_index < scene_mesh.m_instances.size(); ++mesh_instance_index)
                {
                    Demo_Mesh_Instance &scene_mesh_instance = scene_mesh.m_instances[mesh_instance_index];

                    assert(scene_mesh.m_subsets.size() == scene_mesh_instance.m_skinned_subsets.size());

                    mcrt_vector<brx_acceleration_structure_build_input_read_only_buffer const *> bottom_level_acceleration_structure_geometry_vertex_position_buffers(scene_mesh_instance.m_skinned_subsets.size());

                    for (size_t mesh_subset_index = 0U; mesh_subset_index < scene_mesh_instance.m_skinned_subsets.size(); ++mesh_subset_index)
                    {
                        Demo_Mesh_Skinned_Subset const &scene_mesh_skin_subset = scene_mesh_instance.m_skinned_subsets[mesh_subset_index];

                        bottom_level_acceleration_structure_geometry_vertex_position_buffers[mesh_subset_index] = scene_mesh_skin_subset.m_skinned_vertex_position_buffer->get_acceleration_structure_build_input_read_only_buffer();
                    }

                    command_buffer->update_intermediate_bottom_level_acceleration_structure(scene_mesh_instance.m_intermediate_bottom_level_acceleration_structure, bottom_level_acceleration_structure_geometry_vertex_position_buffers.data(), scene_mesh_instance.m_intermediate_bottom_level_acceleration_structure_update_scratch_buffer);
                    update_intermediate_bottom_level_acceleration_structures.push_back(scene_mesh_instance.m_intermediate_bottom_level_acceleration_structure);

                    ++intermediate_bottom_level_acceleration_structure_index;
                }
            }
        }
        assert(update_intermediate_bottom_level_acceleration_structures.size() == intermediate_bottom_level_acceleration_structure_index);

        command_buffer->update_intermediate_bottom_level_acceleration_structure_store(static_cast<uint32_t>(update_intermediate_bottom_level_acceleration_structures.size()), update_intermediate_bottom_level_acceleration_structures.data());

        command_buffer->end_debug_utils_label();
    }

    // Update Top Level Acceleration Structure Pass
    {
        command_buffer->begin_debug_utils_label("Update Top Level Acceleration Structure Pass");

        uint32_t scene_instance_index = 0U;
        uint32_t scene_geometry_index = 0U;
        for (size_t mesh_index = 0U; mesh_index < this->m_scene_meshes.size(); ++mesh_index)
        {
            Demo_Mesh const &scene_mesh = this->m_scene_meshes[mesh_index];

            if (!scene_mesh.m_skinned)
            {
                for (size_t mesh_instance_index = 0U; mesh_instance_index < scene_mesh.m_instances.size(); ++mesh_instance_index)
                {
                    Demo_Mesh_Instance const &scene_mesh_instance = scene_mesh.m_instances[mesh_instance_index];

                    assert(NULL == scene_mesh_instance.m_intermediate_bottom_level_acceleration_structure);

                    BRX_TOP_LEVEL_ACCELERATION_STRUCTURE_INSTANCE top_level_acceleration_structure_instance = {
                        {},
                        scene_instance_index,
                        0XFFU,
                        true,
                        false,
                        false,
                        false,
                        scene_mesh.m_compacted_bottom_level_acceleration_structure->get_bottom_level_acceleration_structure()};

                    DirectX::XMStoreFloat3x4(reinterpret_cast<DirectX::XMFLOAT3X4 *>(&top_level_acceleration_structure_instance.transform_matrix), DirectX::XMLoadFloat4x4(&scene_mesh_instance.m_model_transform));

                    this->m_scene_top_level_acceleration_structure_instance_upload_buffers[frame_throttling_index]->write_instance(scene_instance_index, &top_level_acceleration_structure_instance);

                    ++scene_instance_index;
                }

                uint32_t const mesh_geometry_count = static_cast<uint32_t>(scene_mesh.m_subsets.size());
                scene_geometry_index += mesh_geometry_count;
            }
            else
            {
                assert(NULL == scene_mesh.m_compacted_bottom_level_acceleration_structure);

                for (size_t mesh_instance_index = 0U; mesh_instance_index < scene_mesh.m_instances.size(); ++mesh_instance_index)
                {
                    Demo_Mesh_Instance const &scene_mesh_instance = scene_mesh.m_instances[mesh_instance_index];

                    assert(scene_mesh.m_subsets.size() == scene_mesh_instance.m_skinned_subsets.size());

                    BRX_TOP_LEVEL_ACCELERATION_STRUCTURE_INSTANCE top_level_acceleration_structure_instance = {
                        {},
                        scene_instance_index,
                        0XFFU,
                        true,
                        false,
                        false,
                        false,
                        scene_mesh_instance.m_intermediate_bottom_level_acceleration_structure->get_bottom_level_acceleration_structure()};

                    DirectX::XMStoreFloat3x4(reinterpret_cast<DirectX::XMFLOAT3X4 *>(&top_level_acceleration_structure_instance.transform_matrix), DirectX::XMLoadFloat4x4(&scene_mesh_instance.m_model_transform));

                    this->m_scene_top_level_acceleration_structure_instance_upload_buffers[frame_throttling_index]->write_instance(scene_instance_index, &top_level_acceleration_structure_instance);

                    ++scene_instance_index;

                    uint32_t const mesh_geometry_count = static_cast<uint32_t>(scene_mesh_instance.m_skinned_subsets.size());
                    scene_geometry_index += mesh_geometry_count;
                }
            }
        }
        assert(scene_instance_count == scene_instance_index);
        assert(scene_geometry_count == scene_geometry_index);

        command_buffer->update_top_level_acceleration_structure(this->m_scene_top_level_acceleration_structure, this->m_scene_top_level_acceleration_structure_instance_upload_buffers[frame_throttling_index], this->m_scene_top_level_acceleration_structure_update_scratch_buffer);

        command_buffer->update_top_level_acceleration_structure_store(this->m_scene_top_level_acceleration_structure);

        command_buffer->end_debug_utils_label();
    }

    // GBuffer Pass
    {
        brx_storage_image const *storage_images[] = {
            this->m_gbuffer_depth_image,
            this->m_gbuffer_normal_image};

        BRX_COMPUTE_PASS_STORAGE_IMAGE_LOAD_OPERATION const storage_image_load_operations[] = {
            BRX_COMPUTE_PASS_STORAGE_IMAGE_LOAD_OPERATION_DONT_CARE,
            BRX_COMPUTE_PASS_STORAGE_IMAGE_LOAD_OPERATION_DONT_CARE};
        static_assert(sizeof(storage_images) / sizeof(storage_images[0]) == sizeof(storage_image_load_operations) / sizeof(storage_image_load_operations[0]), "");

        BRX_COMPUTE_PASS_STORAGE_IMAGE_STORE_OPERATION const storage_image_store_operations[] = {
            BRX_COMPUTE_PASS_STORAGE_IMAGE_STORE_OPERATION_FLUSH_FOR_SAMPLED_IMAGE,
            BRX_COMPUTE_PASS_STORAGE_IMAGE_STORE_OPERATION_FLUSH_FOR_SAMPLED_IMAGE};
        static_assert(sizeof(storage_images) / sizeof(storage_images[0]) == sizeof(storage_image_store_operations) / sizeof(storage_image_store_operations[0]), "");

        command_buffer->begin_debug_utils_label("GBuffer Pass");

        command_buffer->compute_pass_load(0U, NULL, NULL, sizeof(storage_images) / sizeof(storage_images[0]), storage_images, storage_image_load_operations);

        command_buffer->bind_compute_pipeline(this->m_gbuffer_pipeline);

        brx_descriptor_set *const descritor_sets[] = {
            this->m_gbuffer_pipeline_none_update_descriptor_set,
            this->m_gbuffer_pipeline_none_update_bindless_buffer_descriptor_set,
            this->m_gbuffer_pipeline_none_update_bindless_texture_descriptor_set};
        uint32_t const dynamic_offsets[] = {
            tbb_align_up(static_cast<uint32_t>(sizeof(common_none_update_set_uniform_buffer_binding)), this->m_uniform_upload_buffer_offset_alignment) * frame_throttling_index,
            tbb_align_up(static_cast<uint32_t>(sizeof(gbuffer_pipeline_none_update_set_instance_information_uniform_buffer_binding)), this->m_uniform_upload_buffer_offset_alignment) * frame_throttling_index};
        command_buffer->bind_compute_descriptor_sets(this->m_gbuffer_pipeline_layout, sizeof(descritor_sets) / sizeof(descritor_sets[0]), descritor_sets, sizeof(dynamic_offsets) / sizeof(dynamic_offsets[0]), dynamic_offsets);

        command_buffer->dispatch(this->m_intermediate_width, this->m_intermediate_height, 1U);

        command_buffer->compute_pass_store(0U, NULL, NULL, sizeof(storage_images) / sizeof(storage_images[0]), storage_images, storage_image_store_operations);

        command_buffer->end_debug_utils_label();
    }

    // Ambient Occlusion Pass
    {
        brx_storage_image const *storage_images[] = {
            this->m_ambient_occlusion_image};

        BRX_COMPUTE_PASS_STORAGE_IMAGE_LOAD_OPERATION const storage_image_load_operations[] = {
            BRX_COMPUTE_PASS_STORAGE_IMAGE_LOAD_OPERATION_DONT_CARE};
        static_assert(sizeof(storage_images) / sizeof(storage_images[0]) == sizeof(storage_image_load_operations) / sizeof(storage_image_load_operations[0]), "");

        BRX_COMPUTE_PASS_STORAGE_IMAGE_STORE_OPERATION const storage_image_store_operations[] = {
            BRX_COMPUTE_PASS_STORAGE_IMAGE_STORE_OPERATION_FLUSH_FOR_SAMPLED_IMAGE};
        static_assert(sizeof(storage_images) / sizeof(storage_images[0]) == sizeof(storage_image_store_operations) / sizeof(storage_image_store_operations[0]), "");

        command_buffer->begin_debug_utils_label("Ambient Occlusion Pass");

        command_buffer->compute_pass_load(0U, NULL, NULL, sizeof(storage_images) / sizeof(storage_images[0]), storage_images, storage_image_load_operations);

        command_buffer->bind_compute_pipeline(this->m_ambient_occlusion_pipeline);

        brx_descriptor_set *const descritor_sets[] = {
            this->m_ambient_occlusion_pipeline_none_update_descriptor_set};
        uint32_t const dynamic_offsets[] = {
            tbb_align_up(static_cast<uint32_t>(sizeof(common_none_update_set_uniform_buffer_binding)), this->m_uniform_upload_buffer_offset_alignment) * frame_throttling_index};
        command_buffer->bind_compute_descriptor_sets(this->m_ambient_occlusion_pipeline_layout, sizeof(descritor_sets) / sizeof(descritor_sets[0]), descritor_sets, sizeof(dynamic_offsets) / sizeof(dynamic_offsets[0]), dynamic_offsets);

        command_buffer->dispatch(this->m_intermediate_width, this->m_intermediate_height, 1U);

        command_buffer->compute_pass_store(0U, NULL, NULL, sizeof(storage_images) / sizeof(storage_images[0]), storage_images, storage_image_store_operations);

        command_buffer->end_debug_utils_label();
    }
}

static inline uint32_t tbb_align_up(uint32_t value, uint32_t alignment)
{
    //
    //  Copyright (c) 2005-2019 Intel Corporation
    //
    //  Licensed under the Apache License, Version 2.0 (the "License");
    //  you may not use this file except in compliance with the License.
    //  You may obtain a copy of the License at
    //
    //      http://www.apache.org/licenses/LICENSE-2.0
    //
    //  Unless required by applicable law or agreed to in writing, software
    //  distributed under the License is distributed on an "AS IS" BASIS,
    //  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    //  See the License for the specific language governing permissions and
    //  limitations under the License.
    //

    // [alignUp](https://github.com/oneapi-src/oneTBB/blob/tbb_2019/src/tbbmalloc/shared_utils.h#L42)

    assert(alignment != static_cast<uint32_t>(0));

    // power-of-2 alignment
    assert((alignment & (alignment - static_cast<uint32_t>(1))) == static_cast<uint32_t>(0));

    return (((value - static_cast<uint32_t>(1)) | (alignment - static_cast<uint32_t>(1))) + static_cast<uint32_t>(1));
}
