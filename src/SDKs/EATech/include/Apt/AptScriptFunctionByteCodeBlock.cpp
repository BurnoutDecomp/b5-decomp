// ===========================================================================
// EATech Apt -- AptScriptFunctionByteCodeBlock out-of-line bodies.
//
// Reconstructed from the X360 ARTIST.XEX (the authoritative spine):
//     AptScriptFunctionByteCodeBlock::AptScriptFunctionByteCodeBlock @ 0x82AF15C0
//     AptScriptFunctionByteCodeBlock::GetByteCodeBase                @ 0x82AD4FB0
//     AptScriptFunctionByteCodeBlock::GetConstantPool                @ 0x82AF1620
//     AptScriptFunctionByteCodeBlock::operator new                   @ 0x82AE6218
//     AptScriptFunctionByteCodeBlock::operator delete                @ 0x82AF1638
// The `vector deleting destructor' thunk is compiler-generated and dropped; the
// implicit ~AptScriptFunctionByteCodeBlock in the header covers it.
//
// See AptScriptFunctionByteCodeBlock.h for the layout / base-class derivation.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptScriptFunctionByteCodeBlock.h"

#include "SDKs/EATech/include/Apt/AptDefine.h"            // gpGCPoolManager
#include "SDKs/EATech/Apt/AptValueGCPoolManager.h"        // AptValueGC_PoolManager + gAptValueGCSizeOffset
#include "SDKs/EATech/Apt/AptValueGCAllocator.h"          // AptValueGC_MemItem (alloc bookkeeping)

// ---------------------------------------------------------------------------
// ctor @ 0x82AF15C0
//
// X360 store sequence: AptScriptFunctionBase::AptScriptFunctionBase(this, 36,
// pCallContext, pCIH, /*bMakePrototype*/ 0); store the byte-code base at +0x30, the
// byte-code size at +0x34, the argument/flags word at +0x38, the constant-pool
// descriptor as one 8-byte value at +0x3C (the `std`); then install the vtable
// (handled automatically by the C++ constructor mechanism).
//
// The base ctor's 2nd/3rd register args are pCallContext (a7) then pCIH (a6) -- the
// X360 swaps them into r5/r6 before the call (mr r6,r8; mr r5,r9), i.e. it calls
// base(36, a7, a6, 0).
// ---------------------------------------------------------------------------
AptScriptFunctionByteCodeBlock::AptScriptFunctionByteCodeBlock(void* pByteCode,
                                                               int32_t nByteCodeSize,
                                                               AptConstantPool constantPool,
                                                               const char* pFunctionName,
                                                               AptValue* pCIH,
                                                               AptValue* pCallContext)
    : AptScriptFunctionBase(AptVFT_ScriptFunctionByteCodeBlock, pCallContext, pCIH, /*bMakePrototype*/ false)
    , mpByteCode(pByteCode)
    , mnByteCodeSize(nByteCodeSize)
    , mpFunctionName(pFunctionName)
    , mConstantPool(constantPool)
{
}

// ---------------------------------------------------------------------------
// GetByteCodeBase @ 0x82AD4FB0   ( lwz r3, 0x30(r3); blr )
//
// The inline byte-code stream base, returned directly (this class embeds the body,
// so there is no record indirection -- contrast AptScriptFunction1::GetByteCodeBase
// which returns record+24).
// ---------------------------------------------------------------------------
void* AptScriptFunctionByteCodeBlock::GetByteCodeBase() const
{
    return mpByteCode;
}

// ---------------------------------------------------------------------------
// GetNumArguments @ 0x82B0F1B8   ( li r3, 0; blr )
//
// A byte-code block is not a parametrised function -- it declares NO arguments, so
// the console body returns 0. This override is REQUIRED: the base/Fn1 form derefs a
// compiled-record (mpByteCode->mnNumArguments), but on this subclass mpByteCode is the
// inline stream base, so that deref would read garbage / fault.
// ---------------------------------------------------------------------------
int32_t AptScriptFunctionByteCodeBlock::GetNumArguments() const
{
    return 0;
}

// ---------------------------------------------------------------------------
// GetByteCodeSize @ 0x82B96840   ( lwz r3, 0x34(r3); blr )
//
// The inline byte-code extent -- the +0x34 member, returned directly (contrast the
// Fn1/Fn2 record-indirection form).
// ---------------------------------------------------------------------------
int32_t AptScriptFunctionByteCodeBlock::GetByteCodeSize() const
{
    return mnByteCodeSize;
}

// ---------------------------------------------------------------------------
// GetConstantPool @ 0x82AF1620
//
// Return the inline constant-pool descriptor by value. The X360 reads the two
// dwords at +0x3C / +0x40 and writes them through the hidden struct-return pointer
// -- exactly the codegen of returning the 8-byte AptConstantPool member by value.
// ---------------------------------------------------------------------------
AptConstantPool AptScriptFunctionByteCodeBlock::GetConstantPool() const
{
    return mConstantPool;
}

// ---------------------------------------------------------------------------
// operator new @ 0x82AE6218
//
// Allocate from the garbage-collected value pool (off_8324D834 == gpGCPoolManager)
// and mark the AptValueGC_MemItem header allocated (byte_8324D804 ==
// gAptValueGCSizeOffset selects the size-word offset). No null guard in the asm:
// this is only ever reached after the Apt runtime has wired the GC pool. The
// cast-to-AptValueGC_MemItem is allocator bookkeeping on the raw pool block (the
// high-bit "allocated" flag the GC walk reads), not a member poke into a live C++
// object -- the same external-allocator pattern as the rest of the GC value family.
// ---------------------------------------------------------------------------
void* AptScriptFunctionByteCodeBlock::operator new(size_t size)
{
    void* lpMem = gpGCPoolManager->Allocate(size);
    reinterpret_cast<AptValueGC_MemItem*>(lpMem)->SetIsAllocated(gAptValueGCSizeOffset, true);
    return lpMem;
}

// ---------------------------------------------------------------------------
// operator delete @ 0x82AF1638
//
// Return the block to the GC pool; on a successful free, clear the
// AptValueGC_MemItem allocated flag. (The X360 `clrlwi. r11, r3, 24` tests the bool
// result in r3 -- Deallocate returns true on success.)
// ---------------------------------------------------------------------------
void AptScriptFunctionByteCodeBlock::operator delete(void* p, size_t size)
{
    if (gpGCPoolManager->Deallocate(p, size))
        reinterpret_cast<AptValueGC_MemItem*>(p)->SetIsAllocated(gAptValueGCSizeOffset, false);
}
