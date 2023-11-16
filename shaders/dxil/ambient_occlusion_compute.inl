#define BYTE uint8_t
#ifndef NDEBUG
#include "debug/_internal_ambient_occlusion_compute.inl"
#else
#include "release/_internal_ambient_occlusion_compute.inl"
#endif
#undef BYTE