#pragma once

#include "types.hpp"
#include "rw/rwcore_structs.h"

// ===================================================================================================
// The immediate-mode / render-state reconstruction models the one allocator entry point it uses as a
// local single-virtual interface:
//
//     class ResourceAllocator { public: virtual void* Create(void* out, ResourceAllocator*,
//                                                            const void* descriptor, int flags) = 0; };
//     ResourceAllocator* lpAllocatorIf = reinterpret_cast<ResourceAllocator*>(lpAllocator);
//     lpAllocatorIf->Create(&out, lpAllocatorIf, &descriptor, 0);
//
// (fifteen copies across the tree, each in its own anonymous namespace), because the X360 asm reaches
// the allocator through one indirect call, `(*(*a2 + 16))(handlesOut, a2, descriptor, 0)`.
//
// ⚠️ THAT SHIM DISPATCHED THE WRONG VTABLE SLOT, AND THE MISS WAS DESTRUCTIVE. A class whose first
// declared virtual is `Create` puts `Create` at vtable SLOT 0. The object actually behind the
// reinterpret_cast is an `rw::IResourceAllocator`, whose slot 0 is its VIRTUAL DESTRUCTOR (slot 1 is
// DoAllocate, slot 2 is Free). So every `lpAllocatorIf->Create(...)` call ran the allocator's scalar
// deleting destructor: it never allocated anything, it returned an unwritten (uninitialised)
// out-parameter, and -- because MSVC resets the vptr to the base class's vtable while destroying --
// it left the allocator permanently downgraded, so every LATER virtual call on that same object
// dispatched to the inert `rw::IResourceAllocator` defaults instead of the concrete override.
//
// That is what fired the four BrnSkyDomeManager::CreateGeometry
// CGS_ASSERT(...GetMemoryResource()) failures on every boot: BrnRendererModule's
// sWorldDispatchAllocator served the three dispatch bins correctly, then
// Im3dSkyDome::Construct's Create() call destroyed it, and the sky dome's four following
// DoAllocate calls all landed on rw::IResourceAllocator::DoAllocate, which returns an empty
// Resource. The pool was never short of memory (measured: 77440 bytes free against a 40-byte
// request).
//
// The fix is to stop guessing a slot: call the interface method by NAME. Each shim's Create is now a
// NON-VIRTUAL forwarder into the helper below, so no vtable of the fake class is ever formed and the
// real `rw::IResourceAllocator::DoAllocate` override runs.
//
// OUT-PARAMETER WIDTH. The console writes the carved lanes straight into the caller's handle array.
// Call sites hand in out-parameters of three different widths -- renderengine::ProgramResourceLayout
// (24 bytes: {mpData, muReserved4, mpPhysicalBlock}), rw::Resource (32), and `T* handles[5]` (40) --
// so the helper writes the narrowest common width, the first KU_CREATE_OUT_LANES lanes. That is
// exactly the triple ProgramResourceLayout names, and covers every lane any committed consumer reads
// (lane 0 = the object, lane 2 = its data block). Wider callers zero-initialise their arrays, so the
// lanes past the third stay null rather than becoming garbage.
// ===================================================================================================

namespace CgsGraphics
{
    // Lanes written into the caller's out-parameter: see OUT-PARAMETER WIDTH above.
    const u32 KU_CREATE_OUT_LANES = 3u;

    inline void* ResourceAllocatorCreate(void* lpAllocatorShim, void* lpOut, const void* lpDescriptor)
    {
        // The shim pointer IS the rw::IResourceAllocator the caller reinterpret_cast away.
        rw::IResourceAllocator* lpAllocator = static_cast<rw::IResourceAllocator*>(lpAllocatorShim);
        if (lpAllocator == 0 || lpOut == 0 || lpDescriptor == 0)
        {
            return lpOut;
        }

        // Every producer of these descriptors (BlendState / RasterizerState / ProgramBuffer /
        // VertexDescriptor ::GetResourceDescriptor) writes the serialised five-entry
        // {u32 m_size; u32 m_alignment;} form, whose first four entries are layout-identical to the
        // PC rw::ResourceDescriptor. The fifth entry has no PC pool and is dropped, as everywhere
        // else the <5> -> <4> narrowing happens.
        const rw::Resource lResource = lpAllocator->DoAllocate(
            *static_cast<const rw::ResourceDescriptor*>(lpDescriptor), 0);

        void** lpLanes = static_cast<void**>(lpOut);
        for (u32 luLane = 0; luLane < KU_CREATE_OUT_LANES; ++luLane)
        {
            lpLanes[luLane] = lResource.m_baseResources[luLane];
        }
        return lpOut;
    }
}
