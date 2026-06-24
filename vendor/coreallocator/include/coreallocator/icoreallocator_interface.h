/////////////////////////////////////////////////////////////////////////////
// Copyright (c) Electronic Arts Inc. All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#ifndef EA_ALLOCATOR_ICOREALLOCATOR_INTERFACE_H
#define EA_ALLOCATOR_ICOREALLOCATOR_INTERFACE_H

#include <cstddef>   // size_t

// EA::Allocator::ICoreAllocator -- EA CoreAllocator package's pure-virtual memory
// allocation interface. This is the shared base that a number of EA games and shared
// libraries (EASTL's CoreAllocatorAdapter, EAThread, etc.) allocate through. It is
// completely unrelated to EASTL itself; the canonical include is
// <coreallocator/icoreallocator_interface.h> (matching how EASTL/core_allocator_adapter.h
// and the EASTL/EAThread tests include it).
//
// Surface recovered from the X360 DWARF
// (references/DecFIGS/dwarfdump/SDKs/EATech/include/coreallocator/icoreallocator_interface.h):
//   vtable = { ~ICoreAllocator,
//              Alloc(size_t, const char*, unsigned int),
//              Alloc(size_t, const char*, unsigned int, unsigned int, unsigned int),
//              Free(void*, size_t) }
//   + a static GetDefaultAllocator(). The AllocFlags enum / kMinAlignment values are the
//   standard EA CoreAllocator contract (the same one EASTL's CoreAllocatorAdapter documents
//   as the expected interface: kFlagTempMemory = 0, kFlagPermMemory = 1).
//
// This is EA vendor code: members/types follow EA's CoreAllocator convention (kFlag*/k*
// enumerators, PascalCase methods), NOT the Brn/Cgs K..._ project convention.
namespace EA
{
namespace Allocator
{
    /// AllocFlags
    ///
    /// Allocation classification flags passed as the `flags` argument of Alloc.
    /// The CoreAllocator contract defines the low bit as the memory lifetime hint.
    enum AllocFlags
    {
        kFlagTempMemory = 0,   ///< Transient memory (default).
        kFlagPermMemory = 1    ///< Persistent / long-lived memory.
    };

    /// Default / minimum alignment used when an Alloc request does not specify one.
    static const unsigned int kMinAlignment = 8;

    /// ICoreAllocator
    ///
    /// Abstract memory allocation interface. Concrete allocators (e.g.
    /// CgsMemory::HeapMallocCoreAllocator) derive from this and forward Alloc/Free
    /// onto their backing heap engine.
    class ICoreAllocator
    {
    public:
        // Polymorphic base destructor. Defined OUT-OF-LINE in
        // coreallocator/source/icoreallocator_interface.cpp so the interface owns a
        // real .cpp definition home -- the X360 base scalar-deleting destructor
        // @ 0x82276820 (stores the ICoreAllocator vtable off_8200F5B4 into *this,
        // then for the deleting variant conditionally operator-delete's). The body is
        // empty (no members, no base); the vtable store + delete-tail is what the
        // compiler emits.
        virtual ~ICoreAllocator();

        /// Allocate `nSize` bytes. `pName` is an optional debug tag, `nFlags` an AllocFlags value.
        virtual void* Alloc(size_t nSize, const char* pName, unsigned int nFlags) = 0;

        /// Allocate `nSize` bytes with explicit alignment / alignment offset.
        virtual void* Alloc(size_t nSize, const char* pName, unsigned int nFlags,
                            unsigned int nAlignment, unsigned int nAlignmentOffset = 0) = 0;

        /// Free a block previously returned by Alloc. `nSize` is the (optional) original size.
        virtual void  Free(void* pBlock, size_t nSize = 0) = 0;

        /// The process-wide default allocator (defined by the owning application).
        static ICoreAllocator* GetDefaultAllocator();
    };
}
}

#endif // EA_ALLOCATOR_ICOREALLOCATOR_INTERFACE_H
