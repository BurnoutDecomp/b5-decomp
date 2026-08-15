#pragma once

// ============================================================================
// GameShared/GameClasses/Memory/CgsIOStackLinearMalloc.h
//
// CgsMemory::IOStackLinearMalloc<N> -- a LinearMalloc arena pushed onto a
// CgsModule::IOBufferStack as an IO buffer. DWARF-authoritative shape
// (references/DecFIGS/dwarfdump/.../CgsIOStackLinearMalloc.h, instantiation
// <1048576>): IOBuffer base, a LinearMalloc, then the N-byte arena; methods
// Construct / Prepare / Destruct / GetMalloc.
//
// X360 ATTESTATION (conductor wave 2026-08-09): PhysicsModule::Update
// @0x825B0640 creates two of these per frame ("Vehicle prim alloc" /
// "Prop linear alloc", CreateIOBuffer @0x825A36E0 / DestroyIOBuffer
// @0x825A37B8) and then runs Prepare's body INLINE:
//     bl CgsMemory::LinearMalloc::Create      (r3 = buf+4, r4 = buf+32, r5 = N)
//     bl CgsMemory::LinearMalloc::SetAlignment (r3 = buf+4, r4 = 16)
// i.e. console offsets: mAlloc @ +4 (28 bytes, ends 32), maData @ +32. The
// HOST offsets differ (LinearMalloc carries pointer/size_t members) -- member
// access is BY NAME, so the width difference is absorbed here.
// ============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"     // CgsModule::IOBuffer
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h" // CgsMemory::LinearMalloc

namespace CgsMemory
{
    template <unsigned N>
    struct IOStackLinearMalloc : public CgsModule::IOBuffer
    {
        // DWARF :61. X360-attested by the CreateIOBuffer<IOStackLinearMalloc<1048576>>
        // instantiation @0x825A36E0, which after `Alloc(this, 0x100020, name)` does exactly
        //     addi r3, r3, 4 ; bl CgsMemory__LinearMalloc__Construct
        // and NOTHING else -- no `stb 1,0(p)`. So this deliberately does NOT raise the
        // IOBuffer status byte (nothing ever Lock*s an allocator buffer); it brings the
        // embedded LinearMalloc to its pre-Create state (mbCreated=false, default alignment).
        // This used to read `CgsModule::IOBuffer::Construct()` only -- the LinearMalloc was
        // left uninitialised and got away with it purely because the old PC CreateIOBuffer
        // value-initialised (zero-filled) the whole 1 MB block first.
        void Construct()
        {
            mAlloc.Construct();
        }

        // DWARF :73 -- the two calls PhysicsModule::Update @0x825B0640 inlines
        // (LinearMalloc::Create over the embedded arena; the caller then sets its
        // own alignment through GetMalloc()).
        void Prepare()
        {
            mAlloc.Create(maData, N);
        }

        // DWARF :67.
        void Destruct()
        {
            mAlloc.Destruct();
        }

        // DWARF :79.
        LinearMalloc* GetMalloc() { return &mAlloc; }

    private:
        LinearMalloc mAlloc;    // console +4
        u8           maData[N]; // console +32
    };
}
