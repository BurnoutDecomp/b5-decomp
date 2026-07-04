#pragma once

#include "types.hpp"
#include "ppmalloc/EAGeneralAllocator.h"          // EA::Allocator::GeneralAllocator (the embedded heap engine)
#include "coreallocator/icoreallocator_interface.h" // EA::Allocator::ICoreAllocator (the abstract base)

// ============================================================================
// GameShared/GameClasses/Memory/CgsHeapMalloc.h
//
// CgsMemory::HeapMalloc -- the general-purpose heap allocator used across the
// engine (the sibling of the bump-pointer CgsMemory::LinearMalloc homed in
// CgsLinearMalloc.h). It owns a buffer (mpBuffer / mnBufferSize) and a
// GeneralAllocator free-list over it; Malloc/Free/ReAlloc service allocations
// from that buffer.
//
// Layout/method set are from the DecFIGS DWARF (CgsHeapMalloc.h:53):
//   private:  void* mpBuffer;  int32_t mnBufferSize;  GeneralAllocator mAllocator;
// mAllocator is now the REAL EA::Allocator::GeneralAllocator (the engine landed in
// vendor/PPMalloc) -- HeapMalloc is a thin wrapper that adopts the caller's buffer as the
// allocator's core and delegates Malloc/Free/ReAlloc/CAlloc to it. (On X360 sizeof(HeapMalloc)
// was 0x520, fixing ICE::ICEMemory's embedded mEditHeap at +0x520; we use compiler layout +
// NAMED members everywhere -- ICEMemory references mEditHeap by name, not raw +0x520 -- so the
// x64 size differs and that is fine.)
//
// All methods are NON-STATIC members (verified against the X360 asm: the call sites pass the
// heap object as `this` in r3).
// ============================================================================

namespace CgsMemory
{
    // Tag is `class` to match the committed forward declaration in
    // BrnScoringSystem.h (`namespace CgsMemory { class HeapMalloc; }`) and
    // CgsHardwareInit.h -- avoids an elaborated-type tag mismatch (MSVC C4099).
    class HeapMalloc
    {
    public:
        static const s32 KI_DEFAULT_ALIGNMENT = 4;   // CgsHeapMalloc.h:37

        // Construct the heap over a caller-supplied [lpBuffer, lpBuffer+lnBufferSize)
        // block (initialises the GeneralAllocator free-list over it). X360 0x82866680.
        void Construct(void* lpBuffer, s32 lnBufferSize);
        void Destruct();
        bool Prepare();

        // Allocate lnSize bytes, lnAlignment-aligned, from this heap (null on
        // exhaustion). X360 0x82866780.
        void* Malloc(s32 lnSize, s32 lnAlignment);
        // Free a block previously returned by this heap's Malloc. X360 member call
        // `Free(this, block)` (the heap is the `this` in r3). 0x82866xxx.
        void  Free(void* lpBlock);
        void* ReAlloc(void* lpBlock, s32 lnSize);
        void* CAlloc(s32 lnCount, s32 lnSize);

        // Dump the live allocation list (diagnostics, used on the out-of-memory
        // path). X360 0x82866848.
        void  PrintAllocations();
        u32   GetLargestFreeBlock(bool lbContiguous);

        // Accessor onto the embedded GeneralAllocator engine. The X360 build inlines
        // `mpNetworkHeapMalloc->GetAllocator()->ValidateHeap(...)` at the heap-validation
        // assert sites (e.g. CarData::Prepare 0x82327158), so a by-name accessor is needed
        // rather than reaching the private member through an offset cast.
        EA::Allocator::GeneralAllocator*       GetAllocator()       { return &mAllocator; }
        const EA::Allocator::GeneralAllocator* GetAllocator() const { return &mAllocator; }

    private:
        void* mpBuffer;        // CgsHeapMalloc.h:117  +0x0
        s32   mnBufferSize;    // CgsHeapMalloc.h:118  +0x4
        EA::Allocator::GeneralAllocator mAllocator;   // CgsHeapMalloc.h:123 the heap engine
    };

    // ========================================================================
    // CgsMemory::HeapMallocCoreAllocator (CgsHeapMalloc.h:137)
    //
    // An EA::Allocator::ICoreAllocator front-end onto a HeapMalloc. The X360
    // Construct (0x82866870) is `HeapMalloc::Construct(this + 1dword, ...)`: with
    // the vtable pointer at +0x0 the embedded HeapMalloc sits at +0x4, so this is
    // a thin ICoreAllocator wrapper that OWNS a HeapMalloc and forwards the
    // interface's Alloc/Free onto HeapMalloc::Malloc/Free. (On X360 the `this + 1`
    // was a raw +4 dword skip past the vtable pointer; we reconstruct it as a
    // named embedded member so the x64 layout falls out naturally.)
    //
    // Used by CgsSystem::HardwareInit::InitializeHardware to expose the engine's
    // general heap to the EASTL / EA-middleware code that allocates through an
    // ICoreAllocator*.
    // ========================================================================
    class HeapMallocCoreAllocator : public EA::Allocator::ICoreAllocator
    {
    public:
        // CgsHeapMalloc.cpp:221 - forward the buffer to the embedded HeapMalloc.
        void Construct(void* lpBuffer, s32 lnBufferSize);
        // CgsHeapMalloc.cpp:234 - tear the embedded HeapMalloc down.
        void Destruct();

        // @ 0x827EF3F0 -- out-of-line virtual dtor; overrides ICoreAllocator's slot-0 virtual
        // dtor and anchors the vtable (off_8200F5B4). The compiler-synthesised vector deleting
        // destructor runs the implicit ~HeapMalloc -> ~GeneralAllocator (the asm's a1+0xC call)
        // member chain, restores the vtable, then conditionally operator-delete's `this`.
        virtual ~HeapMallocCoreAllocator();

        // EA::Allocator::ICoreAllocator overrides (CgsHeapMalloc.cpp:247/263/280).
        // Alloc tags are debug-only and ignored by the underlying engine.
        virtual void* Alloc(size_t lnSize, const char* lpcName, unsigned int luFlags);
        virtual void* Alloc(size_t lnSize, const char* lpcName, unsigned int luFlags,
                            unsigned int luAlignment, unsigned int luAlignmentOffset = 0);
        virtual void  Free(void* lpBlock, size_t lnSize = 0);

    private:
        HeapMalloc mAllocator;   // CgsHeapMalloc.h:162 the wrapped general heap (sits past the vtable ptr)
    };
}
