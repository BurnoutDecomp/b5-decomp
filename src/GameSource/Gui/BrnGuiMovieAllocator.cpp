#include "GameSource/Gui/BrnGuiMovieAllocator.h"

// ============================================================================
// BrnGui::MovieAllocator -- the polymorphic rw::IResourceAllocator BrnGui::MovieManager
// carves from the memory reclaimed while a boot/attract movie plays.
//
// Only the compiler-synthesised `scalar deleting destructor' (X360 @0x827DD408) is in this
// batch. Reconstructed as the out-of-line defaulted virtual dtor: the host C++ ABI supplies
// the vptr-rewrite (*this = off_8200F5B4) + conditional operator-delete the X360 thunk
// open-coded, and the default body destructs the members in reverse order --
// mGraphicsAllocator (LinearMalloc: trivial dtor, no-op), then mMainAllocator (HeapMalloc,
// whose only non-trivial sub-object is the embedded EA::Allocator::GeneralAllocator at
// +0xC), then the rw::IResourceAllocator base. That yields EXACTLY ONE ~GeneralAllocator
// call on the sub-object at byte +0xC -- matching the asm's single `bl ~GeneralAllocator`
// on this+0xC. The out-of-line body also anchors the MovieAllocator vtable in this TU (the
// reason the X360 emitted the polymorphic-delete thunk here). Mirrors the committed sibling
// BrnResource::DefaultLinearAllocator::~DefaultLinearAllocator.
//
// Construct/Prepare/Release/Destruct + the virtual DoAllocate/DoFree/DoFreeDisposable
// overrides are DWARF-attested (BrnGuiMovieManager.cpp:998..1149) but NOT in this batch --
// declared in the header, bodies land with their own TUs (GROW then, do NOT fork the type).
// ============================================================================
namespace BrnGui
{
    MovieAllocator::~MovieAllocator() = default;
}
