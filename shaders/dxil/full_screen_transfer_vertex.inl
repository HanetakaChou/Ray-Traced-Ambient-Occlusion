#define BYTE uint8_t
#ifndef NDEBUG
#include "debug/_internal_full_screen_transfer_vertex.inl"
#else
#include "release/_internal_full_screen_transfer_vertex.inl"
#endif
#undef BYTE