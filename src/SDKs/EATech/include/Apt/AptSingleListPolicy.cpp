// ===========================================================================
// EATech Apt -- AptSingleListPolicy<T> explicit instantiation home.
//
// The PushFront/PopFront/Erase bodies are defined in the header (template); this
// .cpp gives the X360 instance (T = void*, the loaded-item pointer AptLoader's
// forward list tracks) a definition home so all three owned methods get bodies:
//     AptSingleListPolicy<void*>::PushFront @ 0x82AEC3A0
//     AptSingleListPolicy<void*>::PopFront  @ 0x82AEC418
//     AptSingleListPolicy<void*>::Erase     @ 0x82AEC470
//
// The payload the X360 stores/compares is a single 32-bit word; void* models
// that pointer-sized loaded-item handle without committing AptLoader's (out-of-
// group, not-yet-homed) element identity. Vendor EA code in its canonical home.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptSingleListPolicy.h"

// AptLoader's forward list of loaded-item pointers.
template class AptSingleListPolicy<void*>;
