constexpr uint32_t const gbuffer_compute_shader_module_code[] = {
#ifndef NDEBUG
#include "debug/_internal_gbuffer_compute.inl"
#else
#include "release/_internal_gbuffer_compute.inl"
#endif
};