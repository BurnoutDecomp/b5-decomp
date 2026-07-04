#pragma once

// ============================================================================
// GameSource/Gui/BrnGuiMovieAllocator.h
//
// BrnGui::MovieAllocator -- the polymorphic rw::IResourceAllocator BrnGui::MovieManager
// carves from the memory reclaimed while a boot/attract movie plays (the collision-world +
// car-pool blocks). It owns a general HeapMalloc (mMainAllocator, the main sub-heap) and a
// bump LinearMalloc (mGraphicsAllocator, the graphics sub-heap), plus the main alignment.
// MovieManager::PrepareMovieAllocator builds it; DestroyMemoryResourceAndDescriptor tears
// it down.
//
// Shape / member names verbatim from the DecFIGS DWARF (BrnGuiMovieManager.cpp:998..1149),
// gated against the X360 ARTIST binary:
//   +0x00  rw::IResourceAllocator base : vptr (off_8200F5B4) + DoAllocate/DoFree/
//                                        DoFreeDisposable/dtor vtable slots
//   +0x04  mMainAllocator     (CgsMemory::HeapMalloc)   -- the X360 scalar deleting
//                                        destructor's `~GeneralAllocator(a1+0xC)` call
//                                        resolves to mMainAllocator.mAllocator @ +0xC
//   +....  mGraphicsAllocator (CgsMemory::LinearMalloc)
//   +....  miMainAlignment    (s32)
// Members are reached BY NAME; the X360 32-bit-pointer-ABI offsets above are documentary
// (the host build widens the base pointer prefix). Mirrors the committed sibling
// BrnResource::DefaultLinearAllocator.
//
// Only the compiler-synthesised `scalar deleting destructor' (X360 @0x827DD408) is in this
// batch (reconstructed as the out-of-line defaulted virtual dtor below, which anchors the
// MovieAllocator vtable in this TU). Construct/Prepare/Release/Destruct + the virtual
// DoAllocate/DoFree/DoFreeDisposable overrides are DWARF-attested but NOT in this batch --
// declared here, bodies land with their own TUs (GROW this home then, do NOT fork the type).
// ============================================================================

#include "types.hpp"
#include "rw/rwcore_structs.h"                                   // rw::IResourceAllocator (base), rw::Resource / ResourceDescriptor
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"         // CgsMemory::HeapMalloc mMainAllocator
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"       // CgsMemory::LinearMalloc mGraphicsAllocator

namespace BrnGui
{
    class MovieAllocator : public rw::IResourceAllocator
    {
    public:
        // The out-of-line (defaulted) virtual destructor anchors the vtable in
        // BrnGuiMovieAllocator.cpp so the X360's polymorphic-delete thunk
        // (`scalar deleting destructor' @0x827DD408) is emitted there. It destructs the
        // members in reverse order (mGraphicsAllocator: trivial; then mMainAllocator, whose
        // only non-trivial sub-object is the embedded GeneralAllocator at +0xC), then the
        // rw::IResourceAllocator base -- exactly one ~GeneralAllocator call on this+0xC.
        ~MovieAllocator() override;

        // --- lifecycle (DWARF-attested; bodies land with their own TUs) -----------------
        void Construct();
        bool Prepare();
        bool Release();
        void Destruct();

        // --- rw::IResourceAllocator overrides (DWARF-attested; bodies in their own TUs) --
        rw::Resource DoAllocate(const rw::ResourceDescriptor& lrDescriptor, const char* lpcName) override;
        void         DoFree(void* lpBlock);
        void         DoFreeDisposable(void* lpBlock);

    private:
        CgsMemory::HeapMalloc   mMainAllocator;      // +0x04 the main sub-heap
        CgsMemory::LinearMalloc mGraphicsAllocator;  // the graphics sub-heap (bump)
        s32                     miMainAlignment;     // the main-heap default alignment
    };
}
