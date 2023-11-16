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
#include <vector>

#include "renderer.h"
#include "frame_throttling.h"
#include "tick_count.h"
#include "../../thirdparty/Brioche/include/brx_device.h"
#include "../demo.h"

class renderer
{
	brx_device *m_device;

	brx_graphics_queue *m_graphics_queue;

	uint32_t m_frame_throtting_index;

	brx_graphics_command_buffer *m_command_buffers[FRAME_THROTTLING_COUNT];
	brx_fence *m_fences[FRAME_THROTTLING_COUNT];

	brx_surface *m_surface;
	brx_swap_chain *m_swap_chain;

	Demo m_demo;

	brx_sampler *m_linear_sampler;

	brx_pipeline_layout *m_swap_chain_pipeline_layout;

	brx_render_pass *m_swap_chain_render_pass;
	brx_graphics_pipeline *m_swap_chain_pipeline;

	brx_descriptor_set *m_swap_chain_global_descriptor_set;

	BRX_COLOR_ATTACHMENT_IMAGE_FORMAT m_swap_chain_image_format;
	uint32_t m_swap_chain_image_width;
	uint32_t m_swap_chain_image_height;
	std::vector<brx_frame_buffer *> m_swap_chain_frame_buffers;

	double m_tick_count_resolution;
	uint64_t m_tick_count_previous_frame;

	void attach_swap_chain();

	void dettach_swap_chain();

	void create_swap_chain_render_pass_and_pipeline();

	void destroy_swap_chain_render_pass_and_pipeline();

public:
	renderer();

	~renderer();

	void init();

	void destroy();

	void attach_window(void *window);

	void dettach_window();

	void draw();
};

renderer::renderer() : m_surface(NULL), m_swap_chain(NULL)
{
}

renderer::~renderer()
{
	for (uint32_t frame_throtting_index = 0U; frame_throtting_index < FRAME_THROTTLING_COUNT; ++frame_throtting_index)
	{
		assert(NULL == this->m_command_buffers[frame_throtting_index]);
		assert(NULL == this->m_fences[frame_throtting_index]);
	}

	assert(NULL == this->m_surface);
	assert(NULL == this->m_swap_chain);

	assert(NULL == this->m_device);
}

extern renderer *renderer_init()
{
	renderer *new_renderer = new (malloc(sizeof(renderer))) renderer{};
	new_renderer->init();
	return new_renderer;
}

extern void renderer_destroy(renderer *renderer)
{
	renderer->destroy();
	renderer->~renderer();
	free(renderer);
}

extern void renderer_attach_window(renderer *renderer, void *window)
{
	renderer->attach_window(window);
}

extern void renderer_dettach_window(renderer *renderer)
{
	renderer->dettach_window();
}

extern void renderer_draw(renderer *renderer)
{
	renderer->draw();
}

void renderer::init()
{
#if defined(USE_D3D12) && USE_D3D12
	this->m_device = brx_init_d3d12_device(true);
#elif defined(USE_VK) && USE_VK
	this->m_device = brx_init_vk_device(true);
#else
#error Unknown Backend
#endif

	this->m_graphics_queue = this->m_device->create_graphics_queue();

	this->m_frame_throtting_index = 0U;

	for (uint32_t frame_throtting_index = 0U; frame_throtting_index < FRAME_THROTTLING_COUNT; ++frame_throtting_index)
	{
		this->m_command_buffers[frame_throtting_index] = this->m_device->create_graphics_command_buffer();
		this->m_fences[frame_throtting_index] = this->m_device->create_fence(true);
	}

	// Demo Init
	this->m_demo.init(this->m_device);

	// Sampler
	this->m_linear_sampler = this->m_device->create_sampler(BRX_SAMPLER_FILTER_LINEAR);

	// Descriptor Layout - Create
	brx_descriptor_set_layout *swap_chain_global_descriptor_set_layout = NULL;
	{

		BRX_DESCRIPTOR_SET_LAYOUT_BINDING swap_chain_global_descriptor_set_layout_bindings[2] = {
			// texture and sampler
			{0U, BRX_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1U},
			{1U, BRX_DESCRIPTOR_TYPE_SAMPLER, 1U}};
		swap_chain_global_descriptor_set_layout = this->m_device->create_descriptor_set_layout(sizeof(swap_chain_global_descriptor_set_layout_bindings) / sizeof(swap_chain_global_descriptor_set_layout_bindings[0]), swap_chain_global_descriptor_set_layout_bindings);

		brx_descriptor_set_layout *const swap_chain_descriptor_set_layouts[1] = {swap_chain_global_descriptor_set_layout};
		this->m_swap_chain_pipeline_layout = this->m_device->create_pipeline_layout(sizeof(swap_chain_descriptor_set_layouts) / sizeof(swap_chain_descriptor_set_layouts[0]), swap_chain_descriptor_set_layouts);
	}

	// Pipeline
	{
		this->m_swap_chain_image_format = BRX_COLOR_ATTACHMENT_FORMAT_B8G8R8A8_UNORM;
		this->m_swap_chain_render_pass = NULL;
		this->m_swap_chain_pipeline = NULL;
		this->create_swap_chain_render_pass_and_pipeline();
	}

	// Descriptor - Global
	{
		this->m_swap_chain_global_descriptor_set = this->m_device->create_descriptor_set(swap_chain_global_descriptor_set_layout);

		brx_sampled_image const *const source_sampled_image = this->m_demo.get_sampled_image_for_present();
		this->m_device->write_descriptor_set(this->m_swap_chain_global_descriptor_set, BRX_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 0U, 0U, 1U, NULL, NULL, NULL, &source_sampled_image, NULL, NULL, NULL);

		this->m_device->write_descriptor_set(this->m_swap_chain_global_descriptor_set, BRX_DESCRIPTOR_TYPE_SAMPLER, 1U, 0U, 1U, NULL, NULL, NULL, NULL, &this->m_linear_sampler, NULL, NULL);
	}

	// Descriptor Layout - Destroy
	this->m_device->destroy_descriptor_set_layout(swap_chain_global_descriptor_set_layout);
	swap_chain_global_descriptor_set_layout = NULL;

	// Init SwapChain Related
	this->m_swap_chain_image_width = 0U;
	this->m_swap_chain_image_height = 0U;
	assert(0U == this->m_swap_chain_frame_buffers.size());

	// Tick Count
	this->m_tick_count_resolution = (1.0 / static_cast<double>(tick_count_per_second()));
	this->m_tick_count_previous_frame = tick_count_now();
}

void renderer::destroy()
{
	for (uint32_t frame_throtting_index = 0U; frame_throtting_index < FRAME_THROTTLING_COUNT; ++frame_throtting_index)
	{
		this->m_device->wait_for_fence(this->m_fences[frame_throtting_index]);
	}

	this->m_demo.destroy(this->m_device);

	this->m_device->destroy_descriptor_set(this->m_swap_chain_global_descriptor_set);

	this->destroy_swap_chain_render_pass_and_pipeline();

	this->m_device->destroy_pipeline_layout(this->m_swap_chain_pipeline_layout);

	this->m_device->destroy_sampler(this->m_linear_sampler);

	for (uint32_t frame_throtting_index = 0U; frame_throtting_index < FRAME_THROTTLING_COUNT; ++frame_throtting_index)
	{
		this->m_device->destroy_graphics_command_buffer(this->m_command_buffers[frame_throtting_index]);
		this->m_command_buffers[frame_throtting_index] = NULL;
		this->m_device->destroy_fence(this->m_fences[frame_throtting_index]);
		this->m_fences[frame_throtting_index] = NULL;
	}

#if defined(USE_D3D12) && USE_D3D12
	brx_destroy_d3d12_device(this->m_device);
#elif defined(USE_VK) && USE_VK
	brx_destroy_vk_device(this->m_device);
#else
#error Unknown Backend
#endif
	this->m_device = NULL;
}

void renderer::attach_window(void *window)
{
	assert(NULL == this->m_surface);

	this->m_surface = this->m_device->create_surface(window);
	this->attach_swap_chain();
}

void renderer::dettach_window()
{
	for (uint32_t frame_throtting_index = 0U; frame_throtting_index < FRAME_THROTTLING_COUNT; ++frame_throtting_index)
	{
		this->m_device->wait_for_fence(this->m_fences[frame_throtting_index]);
	}

	this->dettach_swap_chain();
	this->m_device->destroy_surface(this->m_surface);

	this->m_surface = NULL;
	this->m_swap_chain = NULL;
}

void renderer::attach_swap_chain()
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
		this->destroy_swap_chain_render_pass_and_pipeline();
		this->create_swap_chain_render_pass_and_pipeline();
	}

	uint32_t const swap_chain_image_count = this->m_swap_chain->get_image_count();
	assert(0U == this->m_swap_chain_frame_buffers.size());
	this->m_swap_chain_frame_buffers.resize(swap_chain_image_count);

	for (uint32_t swap_chain_image_index = 0U; swap_chain_image_index < swap_chain_image_count; ++swap_chain_image_index)
	{
		brx_color_attachment_image const *const swap_chain_color_attachment_image = this->m_swap_chain->get_image(swap_chain_image_index);

		this->m_swap_chain_frame_buffers[swap_chain_image_index] = this->m_device->create_frame_buffer(this->m_swap_chain_render_pass, this->m_swap_chain_image_width, this->m_swap_chain_image_height, 1U, &swap_chain_color_attachment_image, NULL);
	}
}

void renderer::dettach_swap_chain()
{
	uint32_t const swap_chain_image_count = static_cast<uint32_t>(this->m_swap_chain_frame_buffers.size());
	assert(this->m_swap_chain->get_image_count() == swap_chain_image_count);

	for (uint32_t swap_chain_image_index = 0U; swap_chain_image_index < swap_chain_image_count; ++swap_chain_image_index)
	{
		this->m_device->destroy_frame_buffer(this->m_swap_chain_frame_buffers[swap_chain_image_index]);
	}

	this->m_swap_chain_frame_buffers.clear();

	this->m_swap_chain_image_width = 0U;
	this->m_swap_chain_image_height = 0U;

	this->m_device->destroy_swap_chain(this->m_swap_chain);
	this->m_swap_chain = NULL;
}

void renderer::create_swap_chain_render_pass_and_pipeline()
{
	assert(NULL == this->m_swap_chain_render_pass);
	assert(NULL == this->m_swap_chain_pipeline);

	// Render Pass
	{
		BRX_RENDER_PASS_COLOR_ATTACHMENT color_attachments[1] = {
			{this->m_swap_chain_image_format,
			 BRX_RENDER_PASS_COLOR_ATTACHMENT_LOAD_OPERATION_CLEAR,
			 BRX_RENDER_PASS_COLOR_ATTACHMENT_STORE_OPERATION_FLUSH_FOR_PRESENT}};

		this->m_swap_chain_render_pass = this->m_device->create_render_pass(sizeof(color_attachments) / sizeof(color_attachments[0]), color_attachments, NULL);
	}

	// Pipeline
	{
#if defined(USE_D3D12) && USE_D3D12
		typedef uint8_t BYTE;
#include "../../dxbc/swap_chain_vertex.inl"
#include "../../dxbc/swap_chain_fragment.inl"
#elif defined(USE_VK) && USE_VK
		constexpr uint32_t const swap_chain_vertex_shader_module_code[] = {
#include "../../spirv/swap_chain_vertex.inl"
		};

		constexpr uint32_t const swap_chain_fragment_shader_module_code[] = {
#include "../../spirv/swap_chain_fragment.inl"
		};
#else
#error Unknown Backend
#endif

		this->m_swap_chain_pipeline = this->m_device->create_graphics_pipeline(this->m_swap_chain_render_pass, this->m_swap_chain_pipeline_layout, sizeof(swap_chain_vertex_shader_module_code), swap_chain_vertex_shader_module_code, sizeof(swap_chain_fragment_shader_module_code), swap_chain_fragment_shader_module_code, 0U, NULL, 0U, NULL, false, BRX_GRAPHICS_PIPELINE_COMPARE_OPERATION_ALWAYS);
	}
}

void renderer::destroy_swap_chain_render_pass_and_pipeline()
{
	this->m_device->destroy_render_pass(this->m_swap_chain_render_pass);
	this->m_device->destroy_graphics_pipeline(this->m_swap_chain_pipeline);

	this->m_swap_chain_render_pass = NULL;
	this->m_swap_chain_pipeline = NULL;
}

void renderer::draw()
{
	if (NULL == this->m_surface)
	{
		// skip this frame
		return;
	}

	assert(NULL != this->m_swap_chain);

	this->m_device->wait_for_fence(this->m_fences[this->m_frame_throtting_index]);

	this->m_device->reset_graphics_command_buffer(this->m_command_buffers[this->m_frame_throtting_index]);

	this->m_command_buffers[this->m_frame_throtting_index]->begin();

	{
		uint64_t const tick_count_current_frame = tick_count_now();
		float const interval_time = static_cast<float>(static_cast<double>(tick_count_current_frame - this->m_tick_count_previous_frame) * this->m_tick_count_resolution);
		this->m_tick_count_previous_frame = tick_count_current_frame;

		this->m_demo.draw(this->m_command_buffers[this->m_frame_throtting_index], interval_time);
	}

	uint32_t swap_chain_image_index = -1;
	bool acquire_next_image_not_out_of_date = this->m_device->acquire_next_image(this->m_command_buffers[this->m_frame_throtting_index], this->m_swap_chain, &swap_chain_image_index);
	if (!acquire_next_image_not_out_of_date)
	{
		for (uint32_t frame_throtting_index = 0U; frame_throtting_index < FRAME_THROTTLING_COUNT; ++frame_throtting_index)
		{
			this->m_device->wait_for_fence(this->m_fences[frame_throtting_index]);
		}

		this->dettach_swap_chain();
		this->attach_swap_chain();

		// skip this frame
		this->m_command_buffers[this->m_frame_throtting_index]->end();
		return;
	}

	// draw full screen triangle
	{
		this->m_command_buffers[this->m_frame_throtting_index]->begin_debug_utils_label("Swap Chain Pass");

		float color_clear_values[4] = {0.0F, 0.0F, 0.0F, 0.0F};
		this->m_command_buffers[this->m_frame_throtting_index]->begin_render_pass(this->m_swap_chain_render_pass, this->m_swap_chain_frame_buffers[swap_chain_image_index], this->m_swap_chain_image_width, this->m_swap_chain_image_height, 1U, &color_clear_values, NULL, NULL);

		this->m_command_buffers[this->m_frame_throtting_index]->bind_graphics_pipeline(this->m_swap_chain_pipeline);

		this->m_command_buffers[this->m_frame_throtting_index]->set_view_port(this->m_swap_chain_image_width, this->m_swap_chain_image_height);

		this->m_command_buffers[this->m_frame_throtting_index]->set_scissor(this->m_swap_chain_image_width, this->m_swap_chain_image_height);

		brx_descriptor_set *const descritor_sets[1] = {this->m_swap_chain_global_descriptor_set};
		this->m_command_buffers[this->m_frame_throtting_index]->bind_graphics_descriptor_sets(this->m_swap_chain_pipeline_layout, sizeof(descritor_sets) / sizeof(descritor_sets[0]), descritor_sets, 0U, NULL);
		this->m_command_buffers[this->m_frame_throtting_index]->draw(3U, 1U);

		this->m_command_buffers[this->m_frame_throtting_index]->end_render_pass();

		this->m_command_buffers[this->m_frame_throtting_index]->end_debug_utils_label();
	}

	this->m_command_buffers[this->m_frame_throtting_index]->end();

	this->m_device->reset_fence(this->m_fences[this->m_frame_throtting_index]);

	bool present_not_out_of_date = this->m_graphics_queue->submit_and_present(this->m_command_buffers[this->m_frame_throtting_index], this->m_swap_chain, swap_chain_image_index, this->m_fences[this->m_frame_throtting_index]);
	if (!present_not_out_of_date)
	{
		for (uint32_t frame_throtting_index = 0U; frame_throtting_index < FRAME_THROTTLING_COUNT; ++frame_throtting_index)
		{
			this->m_device->wait_for_fence(this->m_fences[frame_throtting_index]);
		}

		this->dettach_swap_chain();
		this->attach_swap_chain();

		// continue this frame
	}

	++this->m_frame_throtting_index;
	this->m_frame_throtting_index %= FRAME_THROTTLING_COUNT;
}
