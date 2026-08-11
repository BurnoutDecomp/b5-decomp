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
        // DWARF :61. The X360 stack template raises the status byte; the arena is
        // adopted by Prepare, not here.
        void Construct()
        {
            CgsModule::IOBuffer::Construct();
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
