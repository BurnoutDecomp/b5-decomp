#pragma once

#include "types.hpp"

namespace CgsMemory
{
class HeapMalloc;
}

namespace CgsAttribSys
{
// CgsAttribSys::AttribSysPackageAllocator -- the EA AttribSys "package" allocator.
//
// A thin, accounting wrapper around a CgsMemory::HeapMalloc that the AttribSys
// vendor library hands to each of its memory "packages" (AttribSys itself,
// GameTalk, EASTL). The package allocator does not own any memory: Prepare()
// adopts a caller-supplied HeapMalloc plus an alignment, and Malloc/Free forward
// straight onto it while tracking running alloc/free byte totals. The user-package
// id is purely for the diagnostic name reported on an out-of-memory assert.
//
// Layout and method set are recovered from the DecFIGS DWARF
// (GameShared/GameClasses/System/AttribSys/CgsAttribSysPackageAllocator.h); the
// member offsets are confirmed against the X360 asm:
//   mpHeapAllocator  +0x00   (lwz r3,0(this) feeds HeapMalloc::Malloc/Free)
//   mbHasAllocator   +0x04   (lbz r11,4(this) guards Malloc/Free/Prepare)
//   meUserPackage    +0x08   (lwz r11,8(this) selects the package name)
//   miAllocTotal     +0x0C   (running total bumped by Malloc by lnSize)
//   miFreeTotal      +0x10   (cleared by Prepare; the free-side counterpart)
//   miAlignment      +0x14   (lwz r5,0x14(this) -> HeapMalloc::Malloc alignment)
//
// X360-ledger attested member bodies (defined in this TU's .cpp):
//   Free(void*, size_t)            @ 0x828020C8
//   GetUserPackageName()           @ 0x821F0200
//   Malloc(size_t, int)            @ 0x821F0358
//   Prepare(HeapMalloc*, int32_t)  @ 0x828041B0
// The remaining DWARF-declared members (Construct/Destruct/Release/the other
// Malloc/Free/MallocAligned overloads) are declared so callers compile; their
// bodies live in their own TUs and are not part of this group.
class AttribSysPackageAllocator
{
public:
    // The default alignment AttribSys asks the package allocator for
    // (CgsAttribSysPackageAllocator.h:36).
    static const s32 KI_DEFAULT_ALIGNMENT = 16;

    // Which AttribSys consumer this allocator serves; selects the diagnostic name
    // (CgsAttribSysPackageAllocator.h:68).
    enum EAttribSysUserPackage
    {
        E_PACKAGE_ATTRIBSYS = 0,
        E_PACKAGE_GAMETALK  = 1,
        E_PACKAGE_EASTL     = 2,
    };

    void Construct(EAttribSysUserPackage leUserPackage);

    // Adopt the supplied heap + alignment and reset the running totals. Asserts
    // the allocator has not already been prepared, that the heap is non-NULL, and
    // that the package id maps to a known name. X360 0x828041B0.
    bool Prepare(CgsMemory::HeapMalloc* lpHeapAllocator, s32 liAlignment);

    void Destruct();
    bool Release();

    // Allocate liSize bytes from the adopted heap at miAlignment, accounting the
    // bytes against miAllocTotal. Asserts the allocator is prepared, and on heap
    // exhaustion fires a "<package> memory overload" assert. X360 0x821F0358.
    void* Malloc(size_t lnSize, int liFlags);

    void* Malloc(s32 liSize, const char* lpcTag);
    void* MallocAligned(size_t lnSize, size_t lnAlignment, size_t lnOffset, int liFlags);

    void  Free(void* lpBlock);

    // Free a block previously returned by this allocator. The byte count is
    // accepted for interface symmetry but unused (the heap tracks block sizes
    // itself); only mbHasAllocator is asserted before forwarding. X360 0x828020C8.
    void  Free(void* lpBlock, size_t lnSize);

    void  Free(void* lpBlock, s32 liSize, const char* lpcTag);

    // The size-accounting free -- the miFreeTotal counterpart of Malloc(size, flags)'s
    // miAllocTotal += size. Asserts this allocator is live (mbHasAllocator), forwards
    // to HeapMalloc::Free(mpHeapAllocator, block), then adds lnSize to miFreeTotal.
    // Distinct from Free(void*, size_t) @ 0x828020C8 (which does NOT account) and from
    // deallocate() (which accounts a fixed +12); this is the variable-size free that the
    // X360 inlines at the AttribSys edit teardown site (Attrib::EditRecord::~EditRecord
    // @ 0x8280DE58: the sbHasLinearAllocator assert is done by the GetAttribSysAllocator()
    // that hands back this instance, so only mbHasAllocator is asserted here). The exact
    // vendor symbol name is not independently attested; FreeSized names the reconstructed
    // inline. FLAG.
    void  FreeSized(void* lpBlock, size_t lnSize);

    // @ 0x82803F18 -- EASTL container allocator adapter deallocate(void*, size_t): the hook
    // the EASTL list<Attrib::Collection*, AttribSysPackageAllocator> caller invokes as
    // mAllocator.deallocate(p, n). Asserts the AttribSys memory manager has been Prepare'd
    // (via HasMemoryBuffer(), the getter for the private sbHasLinearAllocator flag) and that
    // this allocator is live (mbHasAllocator), frees the block, and accounts +12 (one
    // list-node's worth) against miFreeTotal. Name MUST be 'deallocate' (the EASTL interface).
    void  deallocate(void* lpBlock, size_t lnSize);

private:
    // Diagnostic name for meUserPackage; NULL (with an assert) for an unknown id.
    // X360 0x821F0200.
    const char* GetUserPackageName();

    CgsMemory::HeapMalloc* mpHeapAllocator;   // +0x00
    bool                   mbHasAllocator;    // +0x04
    EAttribSysUserPackage  meUserPackage;     // +0x08
    s32                    miAllocTotal;      // +0x0C
    s32                    miFreeTotal;       // +0x10
    s32                    miAlignment;       // +0x14
};
}
