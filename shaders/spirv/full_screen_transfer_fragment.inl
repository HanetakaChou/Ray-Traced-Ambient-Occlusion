constexpr uint32_t const full_screen_transfer_fragment_shader_module_code[] = {
#ifndef NDEBUG
#include "debug/_internal_full_screen_transfer_fragment.inl"
#else
#include "release/_internal_full_screen_transfer_fragment.inl"
#endif
};
