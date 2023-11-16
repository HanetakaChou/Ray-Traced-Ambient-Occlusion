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

#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

#include "renderer.h"
#include "renderer_config.h"
#include "scene_renderer.h"
#include "../thirdparty/McRT-Malloc/include/mcrt_vector.h"
#include "../thirdparty/Brioche/include/brx_device.h"
#include "../thirdparty/Brioche-ImGui/backends/imgui_impl_brx.h"

// The unified facade for both the UI renderer and the scene renderer
class facade_renderer final : public renderer
{
	brx_device *m_device;

	scene_renderer m_scene_renderer;

	brx_graphics_queue *m_graphics_queue;
	brx_graphics_command_buffer *m_graphics_command_buffers[FRAME_THROTTLING_COUNT];
	brx_fence *m_fences[FRAME_THROTTLING_COUNT];

	brx_descriptor_set_layout *m_full_screen_transfer_pipeline_none_update_descriptor_set_layout;
	brx_descriptor_set *m_full_screen_transfer_pipeline_none_update_descriptor_set;
	brx_pipeline_layout *m_full_screen_transfer_pipeline_layout;

	BRX_COLOR_ATTACHMENT_IMAGE_FORMAT m_swap_chain_image_format;

	brx_render_pass *m_swap_chain_render_pass;
	brx_graphics_pipeline *m_full_screen_transfer_pipeline;

	brx_surface *m_surface;
	brx_swap_chain *m_swap_chain;
	uint32_t m_swap_chain_image_width;
	uint32_t m_swap_chain_image_height;
	mcrt_vector<brx_frame_buffer *> m_swap_chain_frame_buffers;

	uint32_t m_frame_throttling_index;

	void attach_swap_chain();

	void detach_swap_chain();

	void create_swap_chain_compatible_render_pass_and_pipeline();

	void destroy_swap_chain_compatible_render_pass_and_pipeline();

	void attach_window(void *wsi_window) override;

	void on_window_resize() override;

	void detach_window() override;

	void draw(float interval_time, ui_model_t const *ui_model, user_camera_model_t const *user_camera_model, void const *ui_draw_data) override;

public:
	facade_renderer();

	~facade_renderer();

	void init(void *wsi_connection, ui_model_t *out_ui_model, user_camera_model_t *out_user_camera_model);

	void uninit();
};

extern renderer *renderer_init(void *wsi_connection, ui_model_t *out_ui_model, user_camera_model_t *out_user_camera_model)
{
	void *new_unwrapped_renderer_base = malloc(sizeof(facade_renderer));
	assert(NULL != new_unwrapped_renderer_base);

	facade_renderer *new_unwrapped_renderer = new (new_unwrapped_renderer_base) facade_renderer{};
	new_unwrapped_renderer->init(wsi_connection, out_ui_model, out_user_camera_model);
	return new_unwrapped_renderer;
}

extern void renderer_destroy(renderer *wrapped_renderer)
{
	assert(NULL != wrapped_renderer);
	facade_renderer *delete_unwrapped_renderer = static_cast<facade_renderer *>(wrapped_renderer);

	delete_unwrapped_renderer->uninit();

	delete_unwrapped_renderer->~facade_renderer();
	free(delete_unwrapped_renderer);
}

facade_renderer::facade_renderer()
	: m_device(NULL),
	  m_graphics_queue(NULL),
	  m_graphics_command_buffers{NULL, NULL, NULL},
	  m_fences{NULL, NULL, NULL},
	  m_full_screen_transfer_pipeline_none_update_descriptor_set_layout(NULL),
	  m_full_screen_transfer_pipeline_none_update_descriptor_set(NULL),
	  m_full_screen_transfer_pipeline_layout(NULL),
	  m_swap_chain_render_pass(NULL),
	  m_full_screen_transfer_pipeline(NULL),
	  m_surface(NULL),
	  m_swap_chain(NULL),
	  m_swap_chain_image_width(0U),
	  m_swap_chain_image_height(0U),
	  m_frame_throttling_index(0)
{
}

facade_renderer::~facade_renderer()
{
	assert(NULL == this->m_device);

	assert(NULL == this->m_graphics_queue);
	for (uint32_t frame_throttling_index = 0U; frame_throttling_index < FRAME_THROTTLING_COUNT; ++frame_throttling_index)
	{
		assert(NULL == this->m_graphics_command_buffers[frame_throttling_index]);
		assert(NULL == this->m_fences[frame_throttling_index]);
	}

	assert(NULL == this->m_full_screen_transfer_pipeline_none_update_descriptor_set_layout);
	assert(NULL == this->m_full_screen_transfer_pipeline_none_update_descriptor_set);
	assert(NULL == this->m_full_screen_transfer_pipeline_layout);

	assert(NULL == this->m_swap_chain_render_pass);
	assert(NULL == this->m_full_screen_transfer_pipeline);

	assert(NULL == this->m_surface);
	assert(NULL == this->m_swap_chain);
	assert(0U == this->m_swap_chain_image_width);
	assert(0U == this->m_swap_chain_image_height);
}

void facade_renderer::init(void *wsi_connection, ui_model_t *out_ui_model, user_camera_model_t *out_user_camera_model)
{
	assert(NULL == this->m_device);
	this->m_device = brx_init_device(wsi_connection, true);

	// Scene Renderer Init
	this->m_scene_renderer.init(this->m_device, FRAME_THROTTLING_COUNT, out_ui_model, out_user_camera_model);

	// UI Renderer Init
	ImGui_ImplBrx_Init(this->m_device, FRAME_THROTTLING_COUNT);

	assert(NULL == this->m_graphics_queue);
	this->m_graphics_queue = this->m_device->create_graphics_queue();

	for (uint32_t frame_throttling_index = 0U; frame_throttling_index < FRAME_THROTTLING_COUNT; ++frame_throttling_index)
	{
		assert(NULL == this->m_graphics_command_buffers[frame_throttling_index]);
		this->m_graphics_command_buffers[frame_throttling_index] = this->m_device->create_graphics_command_buffer();

		assert(NULL == this->m_fences[frame_throttling_index]);
		this->m_fences[frame_throttling_index] = this->m_device->create_fence(true);
	}

	// Descriptor Set Layout
	{
		assert(NULL == this->m_full_screen_transfer_pipeline_none_update_descriptor_set_layout);

		BRX_DESCRIPTOR_SET_LAYOUT_BINDING const full_screen_transfer_none_update_descriptor_set_layout_bindings[] = {
			// texture and sampler
			{0U, BRX_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1U}};
		this->m_full_screen_transfer_pipeline_none_update_descriptor_set_layout = this->m_device->create_descriptor_set_layout(sizeof(full_screen_transfer_none_update_descriptor_set_layout_bindings) / sizeof(full_screen_transfer_none_update_descriptor_set_layout_bindings[0]), full_screen_transfer_none_update_descriptor_set_layout_bindings);
	}

	// Descriptor Set
	{
		assert(NULL == this->m_full_screen_transfer_pipeline_none_update_descriptor_set);
		this->m_full_screen_transfer_pipeline_none_update_descriptor_set = this->m_device->create_descriptor_set(this->m_full_screen_transfer_pipeline_none_update_descriptor_set_layout, 0U);
	}

	// Pipeline Layout
	{
		assert(NULL == this->m_full_screen_transfer_pipeline_layout);
		brx_descriptor_set_layout *const full_screen_transfer_descriptor_set_layouts[] = {this->m_full_screen_transfer_pipeline_none_update_descriptor_set_layout};
		this->m_full_screen_transfer_pipeline_layout = this->m_device->create_pipeline_layout(sizeof(full_screen_transfer_descriptor_set_layouts) / sizeof(full_screen_transfer_descriptor_set_layouts[0]), full_screen_transfer_descriptor_set_layouts);
	}

	// Pipeline
	{
		this->m_swap_chain_image_format = BRX_COLOR_ATTACHMENT_FORMAT_B8G8R8A8_UNORM;
		this->create_swap_chain_compatible_render_pass_and_pipeline();
	}

	// Frame Throttling
	assert(0U == this->m_frame_throttling_index);
}

void facade_renderer::uninit()
{
	for (uint32_t frame_throttling_index = 0U; frame_throttling_index < FRAME_THROTTLING_COUNT; ++frame_throttling_index)
	{
		this->m_device->wait_for_fence(this->m_fences[frame_throttling_index]);
	}

	this->destroy_swap_chain_compatible_render_pass_and_pipeline();

	// Pipeline Layout
	{
		assert(NULL != this->m_full_screen_transfer_pipeline_layout);
		this->m_device->destroy_pipeline_layout(this->m_full_screen_transfer_pipeline_layout);
		this->m_full_screen_transfer_pipeline_layout = NULL;
	}

	// Descriptor Set
	{
		assert(NULL != this->m_full_screen_transfer_pipeline_none_update_descriptor_set);
		this->m_device->destroy_descriptor_set(this->m_full_screen_transfer_pipeline_none_update_descriptor_set);
		this->m_full_screen_transfer_pipeline_none_update_descriptor_set = NULL;
	}

	// Descriptor Set Layout
	{
		assert(NULL != this->m_full_screen_transfer_pipeline_none_update_descriptor_set_layout);
		this->m_device->destroy_descriptor_set_layout(this->m_full_screen_transfer_pipeline_none_update_descriptor_set_layout);
		this->m_full_screen_transfer_pipeline_none_update_descriptor_set_layout = NULL;
	}

	for (uint32_t frame_throttling_index = 0U; frame_throttling_index < FRAME_THROTTLING_COUNT; ++frame_throttling_index)
	{
		assert(NULL != this->m_graphics_command_buffers[frame_throttling_index]);
		this->m_device->destroy_graphics_command_buffer(this->m_graphics_command_buffers[frame_throttling_index]);
		this->m_graphics_command_buffers[frame_throttling_index] = NULL;

		assert(NULL != this->m_fences[frame_throttling_index]);
		this->m_device->destroy_fence(this->m_fences[frame_throttling_index]);
		this->m_fences[frame_throttling_index] = NULL;
	}

	assert(NULL != this->m_graphics_queue);
	this->m_device->destroy_graphics_queue(this->m_graphics_queue);
	this->m_graphics_queue = NULL;

	// UI Renderer
	ImGui_ImplBrx_Shutdown(this->m_device);

	// Scene Renderer
	this->m_scene_renderer.destroy(this->m_device);

	brx_destroy_device(this->m_device);
	this->m_device = NULL;
}

void facade_renderer::attach_window(void *wsi_window)
{
	assert(NULL == this->m_surface);
	this->m_surface = this->m_device->create_surface(wsi_window);

	this->attach_swap_chain();
}

void facade_renderer::on_window_resize()
{
	for (uint32_t frame_throttling_index = 0U; frame_throttling_index < FRAME_THROTTLING_COUNT; ++frame_throttling_index)
	{
		this->m_device->wait_for_fence(this->m_fences[frame_throttling_index]);
	}

	this->detach_swap_chain();
	this->attach_swap_chain();
}

void facade_renderer::detach_window()
{
	for (uint32_t frame_throttling_index = 0U; frame_throttling_index < FRAME_THROTTLING_COUNT; ++frame_throttling_index)
	{
		this->m_device->wait_for_fence(this->m_fences[frame_throttling_index]);
	}

	this->detach_swap_chain();

	assert(NULL != this->m_surface);
	this->m_device->destroy_surface(this->m_surface);
	this->m_surface = NULL;
}

void facade_renderer::attach_swap_chain()
{
	assert(NULL == this->m_swap_chain);
	this->m_swap_chain = this->m_device->create_swap_chain(this->m_surface);

	assert(0U == this->m_swap_chain_image_width);
	assert(0U == this->m_swap_chain_image_height);
	this->m_swap_chain_image_width = this->m_swap_chain->get_image_width();
	this->m_swap_chain_image_height = this->m_swap_chain->get_image_height();

	if (this->m_swap_chain_image_format != this->m_swap_chain->get_image_format())
	{
		this->m_swap_chain_image_format = this->m_swap_chain->get_image_format();
		this->destroy_swap_chain_compatible_render_pass_and_pipeline();
		this->create_swap_chain_compatible_render_pass_and_pipeline();
	}

	uint32_t const swap_chain_image_count = this->m_swap_chain->get_image_count();
	assert(0U == this->m_swap_chain_frame_buffers.size());
	this->m_swap_chain_frame_buffers.resize(swap_chain_image_count);

	for (uint32_t swap_chain_image_index = 0U; swap_chain_image_index < swap_chain_image_count; ++swap_chain_image_index)
	{
		brx_color_attachment_image const *const swap_chain_color_attachment_image = this->m_swap_chain->get_image(swap_chain_image_index);

		this->m_swap_chain_frame_buffers[swap_chain_image_index] = this->m_device->create_frame_buffer(this->m_swap_chain_render_pass, this->m_swap_chain_image_width, this->m_swap_chain_image_height, 1U, &swap_chain_color_attachment_image, NULL);
	}

	this->m_scene_renderer.on_swap_chain_attach(this->m_device, this->m_swap_chain_image_width, this->m_swap_chain_image_height);

	// Descriptor
	{
		// Full Screen Transfer Pipeline
		{
			// The VkDescriptorSetLayout should still be valid when perform write update on VkDescriptorSet
			assert(NULL != this->m_full_screen_transfer_pipeline_none_update_descriptor_set_layout);
			{
				brx_sampled_image const *const sampled_images[] = {
					this->m_scene_renderer.get_scene_color_image()};
				this->m_device->write_descriptor_set(this->m_full_screen_transfer_pipeline_none_update_descriptor_set, 0U, BRX_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 0U, sizeof(sampled_images) / sizeof(sampled_images[0]), NULL, NULL, NULL, NULL, sampled_images, NULL, NULL, NULL);
			}
		}
	}
}

void facade_renderer::detach_swap_chain()
{
	assert(NULL != this->m_swap_chain);

	this->m_scene_renderer.on_swap_chain_detach(this->m_device);

	uint32_t const swap_chain_image_count = static_cast<uint32_t>(this->m_swap_chain_frame_buffers.size());
	assert(this->m_swap_chain->get_image_count() == swap_chain_image_count);
	for (uint32_t swap_chain_image_index = 0U; swap_chain_image_index < swap_chain_image_count; ++swap_chain_image_index)
	{
		this->m_device->destroy_frame_buffer(this->m_swap_chain_frame_buffers[swap_chain_image_index]);
	}
	this->m_swap_chain_frame_buffers.clear();

	assert(this->m_swap_chain->get_image_width() == this->m_swap_chain_image_width);
	this->m_swap_chain_image_width = 0U;

	assert(this->m_swap_chain->get_image_height() == this->m_swap_chain_image_height);
	this->m_swap_chain_image_height = 0U;

	this->m_device->destroy_swap_chain(this->m_swap_chain);
	this->m_swap_chain = NULL;
}

void facade_renderer::create_swap_chain_compatible_render_pass_and_pipeline()
{
	// Render Pass
	{
		assert(NULL == this->m_swap_chain_render_pass);
		BRX_RENDER_PASS_COLOR_ATTACHMENT color_attachments[1] = {
			{this->m_swap_chain_image_format,
			 BRX_RENDER_PASS_COLOR_ATTACHMENT_LOAD_OPERATION_CLEAR,
			 BRX_RENDER_PASS_COLOR_ATTACHMENT_STORE_OPERATION_FLUSH_FOR_PRESENT}};

		this->m_swap_chain_render_pass = this->m_device->create_render_pass(sizeof(color_attachments) / sizeof(color_attachments[0]), color_attachments, NULL);
	}

	// Pipeline
	{

		assert(NULL == this->m_full_screen_transfer_pipeline);
		switch (this->m_device->get_backend_name())
		{
		case BRX_BACKEND_NAME_D3D12:
		{
#include "../shaders/dxil/full_screen_transfer_vertex.inl"
#include "../shaders/dxil/full_screen_transfer_fragment.inl"
			this->m_full_screen_transfer_pipeline = this->m_device->create_graphics_pipeline(this->m_swap_chain_render_pass, this->m_full_screen_transfer_pipeline_layout, sizeof(full_screen_transfer_vertex_shader_module_code), full_screen_transfer_vertex_shader_module_code, sizeof(full_screen_transfer_fragment_shader_module_code), full_screen_transfer_fragment_shader_module_code, false, true, false, BRX_GRAPHICS_PIPELINE_DEPTH_COMPARE_OPERATION_UNKNOWN, false, BRX_GRAPHICS_PIPELINE_BLEND_OPERATION_UNKNOWN);
		}
		break;
		case BRX_BACKEND_NAME_VK:
		{
#include "../shaders/spirv/full_screen_transfer_vertex.inl"
#include "../shaders/spirv/full_screen_transfer_fragment.inl"
			this->m_full_screen_transfer_pipeline = this->m_device->create_graphics_pipeline(this->m_swap_chain_render_pass, this->m_full_screen_transfer_pipeline_layout, sizeof(full_screen_transfer_vertex_shader_module_code), full_screen_transfer_vertex_shader_module_code, sizeof(full_screen_transfer_fragment_shader_module_code), full_screen_transfer_fragment_shader_module_code, false, true, false, BRX_GRAPHICS_PIPELINE_DEPTH_COMPARE_OPERATION_UNKNOWN, false, BRX_GRAPHICS_PIPELINE_BLEND_OPERATION_UNKNOWN);
		}
		break;
		default:
		{
			assert(false);
		}
		}

		ImGui_ImplBrx_Init_Pipeline(this->m_device, this->m_swap_chain_render_pass);
	}
}

void facade_renderer::destroy_swap_chain_compatible_render_pass_and_pipeline()
{
	// Pipeline
	{
		ImGui_ImplBrx_Shutdown_Pipeline(this->m_device);

		{
			assert(NULL != this->m_full_screen_transfer_pipeline);
			this->m_device->destroy_graphics_pipeline(this->m_full_screen_transfer_pipeline);
			this->m_full_screen_transfer_pipeline = NULL;
		}
	}

	// Render Pass
	{
		assert(NULL != this->m_swap_chain_render_pass);
		this->m_device->destroy_render_pass(this->m_swap_chain_render_pass);
		this->m_swap_chain_render_pass = NULL;
	}
}

void facade_renderer::draw(float interval_time, ui_model_t const *ui_model, user_camera_model_t const *user_camera_model, void const *ui_draw_data)
{
	if (NULL == this->m_surface)
	{
		// skip this frame
		return;
	}

	assert(NULL != this->m_swap_chain);

	this->m_device->wait_for_fence(this->m_fences[this->m_frame_throttling_index]);

	this->m_device->reset_graphics_command_buffer(this->m_graphics_command_buffers[this->m_frame_throttling_index]);

	this->m_graphics_command_buffers[this->m_frame_throttling_index]->begin();

	// Scene Color Pass
	{
		this->m_scene_renderer.draw(this->m_graphics_command_buffers[this->m_frame_throttling_index], this->m_frame_throttling_index, interval_time, ui_model, user_camera_model);
	}

	uint32_t swap_chain_image_index = -1;
	bool acquire_next_image_not_out_of_date = this->m_device->acquire_next_image(this->m_graphics_command_buffers[this->m_frame_throttling_index], this->m_swap_chain, &swap_chain_image_index);
	if (!acquire_next_image_not_out_of_date)
	{
		// NOTE: we should end the command buffer before we destroy the bound image
		this->m_graphics_command_buffers[this->m_frame_throttling_index]->end();

		for (uint32_t frame_throttling_index = 0U; frame_throttling_index < FRAME_THROTTLING_COUNT; ++frame_throttling_index)
		{
			this->m_device->wait_for_fence(this->m_fences[frame_throttling_index]);
		}

		this->detach_swap_chain();
		this->attach_swap_chain();

		// skip this frame
		return;
	}

	// Swap Chain Pass
	{
		this->m_graphics_command_buffers[this->m_frame_throttling_index]->begin_debug_utils_label("Swap Chain Pass");

		float color_clear_values[4] = {0.0F, 0.0F, 0.0F, 0.0F};
		this->m_graphics_command_buffers[this->m_frame_throttling_index]->begin_render_pass(this->m_swap_chain_render_pass, this->m_swap_chain_frame_buffers[swap_chain_image_index], this->m_swap_chain_image_width, this->m_swap_chain_image_height, 1U, &color_clear_values, NULL, NULL);

		// Full Screen Transfer Pass
		{
			this->m_graphics_command_buffers[this->m_frame_throttling_index]->begin_debug_utils_label("Full Screen Transfer Pass");

			this->m_graphics_command_buffers[this->m_frame_throttling_index]->set_view_port(this->m_swap_chain_image_width, this->m_swap_chain_image_height);

			this->m_graphics_command_buffers[this->m_frame_throttling_index]->set_scissor(0, 0, this->m_swap_chain_image_width, this->m_swap_chain_image_height);

			this->m_graphics_command_buffers[this->m_frame_throttling_index]->bind_graphics_pipeline(this->m_full_screen_transfer_pipeline);

			brx_descriptor_set *const descritor_sets[1] = {this->m_full_screen_transfer_pipeline_none_update_descriptor_set};
			this->m_graphics_command_buffers[this->m_frame_throttling_index]->bind_graphics_descriptor_sets(this->m_full_screen_transfer_pipeline_layout, sizeof(descritor_sets) / sizeof(descritor_sets[0]), descritor_sets, 0U, NULL);
			this->m_graphics_command_buffers[this->m_frame_throttling_index]->draw(3U, 1U, 0U, 0U);

			this->m_graphics_command_buffers[this->m_frame_throttling_index]->end_debug_utils_label();
		}

		// ImGui Pass
		{
			this->m_graphics_command_buffers[this->m_frame_throttling_index]->begin_debug_utils_label("ImGui Pass");

			ImGui_ImplBrx_RenderDrawData(ui_draw_data, this->m_graphics_command_buffers[this->m_frame_throttling_index], this->m_frame_throttling_index);

			this->m_graphics_command_buffers[this->m_frame_throttling_index]->end_debug_utils_label();
		}

		this->m_graphics_command_buffers[this->m_frame_throttling_index]->end_render_pass();

		this->m_graphics_command_buffers[this->m_frame_throttling_index]->end_debug_utils_label();
	}

	this->m_graphics_command_buffers[this->m_frame_throttling_index]->end();

	this->m_device->reset_fence(this->m_fences[this->m_frame_throttling_index]);

	bool present_not_out_of_date = this->m_graphics_queue->submit_and_present(this->m_graphics_command_buffers[this->m_frame_throttling_index], this->m_swap_chain, swap_chain_image_index, this->m_fences[this->m_frame_throttling_index]);
	if (!present_not_out_of_date)
	{
		for (uint32_t frame_throttling_index = 0U; frame_throttling_index < FRAME_THROTTLING_COUNT; ++frame_throttling_index)
		{
			this->m_device->wait_for_fence(this->m_fences[frame_throttling_index]);
		}

		this->detach_swap_chain();
		this->attach_swap_chain();

		// continue this frame
	}

	++this->m_frame_throttling_index;
	this->m_frame_throttling_index %= FRAME_THROTTLING_COUNT;
}
