#ifndef SPLICE_MANAGER_H
#define SPLICE_MANAGER_H

#include "types.hpp"

// ============================================================================
// GameShared/GameClasses/Sound/Playback/Splicer/SpliceManager.h
//
// SpliceManager -- the sound-playback splice subsystem's memory broker. It owns
// a heap allocator and hands out raw blocks to the splice objects (Splice and
// CgsSound::Playback::SpliceBankStatistics both route their allocations through
// SpliceManager::Allocate on the global instance off_82FFB9F0).
//
// There is no Feb-2007 leak source / DecFIGS dwarfdump for SpliceManager, so the
// shape is reconstructed from the X360 ARTIST.XEX pseudocode + disassembly:
//     SpliceManager::Allocate @ 0x826AD630
// plus the two call sites (Splice::operator new @ 0x826C33A0,
// SpliceBankStatistics::SpliceBankStatistics @ 0x826D57E8) which fix the
// argument order and the touched members of the global instance.
//
// Member layout (X360-relative offsets reproduced BY NAME; the 32-bit guest
// offsets do not and need not reproduce byte-for-byte on the 64-bit host):
//   +0x184  mpfnFailureHandler  -- error sink read by Splice::operator new when
//                                  Allocate returns null ("Failed to dynamically
//                                  new splice"); a (const char*)->void hook.
//   +0x6C4  mpHeap              -- the heap object Allocate forwards through; its
//                                  vtable slot 0x10 (index 4) is the polymorphic
//                                  block-allocation entry point.
// Only mpHeap is touched by Allocate; the failure-handler slot is modelled so
// the documented call-site behaviour has a named home. All preceding/intervening
// storage is an opaque byte span pinned so the two named members land at their
// guest offsets.
// ============================================================================

namespace SpliceManagerDetail
{
// The polymorphic heap the SpliceManager forwards block allocations to. The X360
// builds a small allocation descriptor on the stack and calls through the heap
// object's vtable at +0x10 (the 5th slot). The descriptor's exact per-pool
// semantics are not recoverable from this TU, so it is modelled as a named
// AllocationRequest that reproduces the X360 stores field-for-field, and the
// heap exposes the one virtual entry point Allocate exercises.
struct AllocationRequest
{
    // var_30 / v7: the X360 packs { LODWORD = 4 (alignment), HIDWORD = size }.
    s32 miAlignment;   // LODWORD(v5) = 4
    s32 miSize;        // HIDWORD(v5) = requested size

    // var_28..var_C / v8..v15: four (size, 1) per-pool hint pairs the X360 fills
    // before the call (v8=v10=v12=v14 = size; v9=v11=v13=v15 = 1).
    s32 maiPoolSize[4];
    s32 maiPoolFlag[4];
};

struct Heap
{
    struct VTable
    {
        // Padding so Allocate lands at vtable byte offset 0x10 (index 4) -- the
        // slot the X360 loads via `lwz r11,0x10(r11); mtctr; bctrl`.
        void* mapReserved[4];
        // The polymorphic block allocator: (out-result, this, &request, tag).
        // Returns the allocated block (or null on failure) into the out result.
        void* (*mpfnAllocate)( void* lpResult, Heap* lpHeap,
                               const AllocationRequest* lprRequest,
                               const char* lpcTag );
    };

    const VTable* mpVTable;   // +0x00
};
}

struct SpliceManager
{
    // @ 0x826AD630. Allocate a `luSize`-byte splice block tagged `lpcTag` through
    // the owned heap. Builds the X360 allocation descriptor (alignment 4, four
    // (size,1) pool hints) and forwards to the heap's polymorphic Allocate slot,
    // returning the block address (0 on failure).
    void* Allocate( u32 luSize, const char* lpcTag );

    // Error sink: the X360 type is a bare (const char*)->void function pointer.
    typedef void (*FailureHandler)( const char* lpcMessage );

private:
    // Opaque storage up to the failure-handler slot at +0x184.
    u8 mPad0[0x184];

    // +0x184: invoked by Splice::operator new when Allocate fails.
    FailureHandler mpfnFailureHandler;

    // Opaque storage between +0x188 and the heap pointer at +0x6C4.
    u8 mPad188[0x6C4 - 0x188];

    // +0x6C4: the polymorphic heap Allocate forwards through.
    SpliceManagerDetail::Heap* mpHeap;
};

#endif // SPLICE_MANAGER_H
