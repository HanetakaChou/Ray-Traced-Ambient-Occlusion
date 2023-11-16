constexpr uint32_t const ambient_occlusion_compute_shader_module_code[] = {
#ifndef NDEBUG
#include "debug/_internal_ambient_occlusion_compute.inl"
#else
#include "release/_internal_ambient_occlusion_compute.inl"
#endif
};