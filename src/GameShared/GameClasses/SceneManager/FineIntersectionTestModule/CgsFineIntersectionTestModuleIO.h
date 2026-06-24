#pragma once

// CgsSceneManager::FineIntersectionTestIO::OutputBuffer — the per-frame output payload the
// FineIntersectionTestModule fills with its fine intersection-test results. Like the other
// CgsModule IO buffers it derives from CgsModule::IOBuffer (the 1-byte FlagSet lock-state
// base) and a consumer must hold the appropriate lock before touching the result buffer.
//
// DWARF home: CgsFineIntersectionTestModuleIO.h (the assert in the recovered getter cites
// "...fineintersectiontestmodule/CgsFineIntersectionTestModuleIO.h" line 192). The
// OutputBuffer's Construct/Destruct are attested in DecFIGS as living in
// CgsFineIntersectionTestModuleIO.cpp with the result store homed in CgsArray.h, i.e. the
// result store is a CgsContainers::Array<...> at the buffer's tail.
//
// RECOVERED ACCESSOR (this group bodies one function):
//   GetResults() @ 0x828B0A88
//     X360: tests ((*this >> 3) & 1) == eStatusLockedForWrite; asserts "Not locked for
//     writing" (CgsFineIntersectionTestModuleIO.h:192) on violation; returns this + 0x4020
//     (16416) — the address of the result store. The name is truncated to "OutputBuffe" in
//     the symbol table; reconstructed as GetResults() (a write-locked handle to the result
//     store, matching the CgsGuiModuleIO::OutputBuffer::GetGuiResourceRequestQueue() shape).

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"  // CgsModule::IOBuffer

namespace CgsSceneManager
{
namespace FineIntersectionTestIO
{
    struct OutputBuffer : public CgsModule::IOBuffer
    {
        // FLAG (opaque, element-internal layout not attested): the fine-intersection result
        // store the module writes into. The recovered getter returns its address at the
        // X360 member offset +0x4020 (16416). The store is a CgsContainers::Array<...> (per
        // the OutputBuffer::Construct DWARF home CgsArray.h) whose element type/capacity are
        // not pinned by the recovered getter; modelled as correctly-placed, 16-byte-aligned
        // opaque storage so the +0x4020 return offset is exact. When the result element type
        // lands this should adopt the named CgsContainers::Array<T,N> additively.
        //
        // The IOBuffer base occupies the leading bytes (1-byte FlagSet + pad); the leading
        // pad span forces the result store to the asm-attested +0x4020 offset. 0x4020 less
        // the 4-byte IOBuffer base subobject = 0x401C bytes of preceding lock-state/header
        // span the module populates elsewhere.
        struct ResultStoreStorage
        {
            // Sized only to the asm-attested top offset (+0x4020 return); the byte after the
            // returned address is the result store body, kept opaque (no internal layout is
            // attested). One 16-byte lane stands in for the (alignment-class-faithful) store
            // so the type is non-empty without fabricating its real extent.
            alignas(16) u8 maStore[16];
        };

        // GetResults() @ 0x828B0A88 — write-locked handle to the result store.
        // Returns &maResults as a raw byte pointer (the X360 `return this + 0x4020;`).
        u8* GetResults();

        // Byte-offset pin (member has access to the private result store).
        static void _AssertLayout();

    private:
        // Leading span that pushes maResults to the asm-attested +0x4020 offset (less the
        // 4-byte IOBuffer base subobject already at offset 0).
        u8                 maHeaderReserved[0x4020 - sizeof(CgsModule::IOBuffer)];
        ResultStoreStorage maResults;  // X360 +0x4020 (the getter return target)
    };
}
}
