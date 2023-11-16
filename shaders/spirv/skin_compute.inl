constexpr uint32_t const skin_compute_shader_module_code[] = {
#ifndef NDEBUG
#include "debug/_internal_skin_compute.inl"
#else
#include "release/_internal_skin_compute.inl"
#endif
};