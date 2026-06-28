// ===========================================================================
// EATech Apt -- AptScriptFunction1 out-of-line bodies.
//
// Reconstructed from the X360 ARTIST.XEX (the authoritative spine):
//     AptScriptFunction1::AptScriptFunction1   @ 0x82AF1308
//     AptScriptFunction1::operator new         @ 0x82AE6178
//     AptScriptFunction1::operator delete      @ 0x82AF13A0
//     AptScriptFunction1::GetName              @ 0x82AF1450
//     AptScriptFunction1::GetNumArguments      @ 0x82AF1460
//     AptScriptFunction1::GetByteCodeBase      @ 0x82AF1360
//     AptScriptFunction1::GetByteCodeSize      @ 0x82AF1370
//     AptScriptFunction1::GetConstantPool      @ 0x82AF1380
//     AptScriptFunction1::SetArgument          @ 0x82AF5338
//     AptScriptFunction1::Duplicate            @ 0x82B024A8
// plus the two copy constructors the Duplicate path inlines through:
//     AptScriptFunction1   copy ctor           @ 0x82B01000  (sub_82B01000)
//     AptScriptFunctionBase copy ctor          @ 0x82B00E90  (sub_82B00E90; its
//                                              body lands with class:AptScriptFunctionBase)
// The `vector deleting destructor' is a compiler thunk (restore the base vtable ->
// ~AptObject -> conditional operator delete) and is dropped, not hand-written; the
// implicit ~AptScriptFunction1 in the header covers it.
//
// See AptScriptFunction1.h for the layout / base derivation and the compiled-record
// (AptScriptFunction1ByteCode) format.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptScriptFunction1.h"
#include "SDKs/EATech/include/Apt/AptFrameStack.h"               // spFrameStack->Set (the call-local hash)
#include "SDKs/EATech/include/Apt/AptDefine.h"                   // gpGCPoolManager
#include "SDKs/EATech/Apt/AptValueGCPoolManager.h"               // AptValueGC_PoolManager + gAptValueGCSizeOffset
#include "SDKs/EATech/Apt/AptValueGCAllocator.h"                 // AptValueGC_MemItem (alloc bookkeeping)

#include <new>   // placement new (Duplicate)

// ---------------------------------------------------------------------------
// operator new @ 0x82AE6178
//
// Allocate from the garbage-collected value pool (off_8324D834 == gpGCPoolManager)
// and mark the AptValueGC_MemItem header allocated (byte_8324D804 ==
// gAptValueGCSizeOffset selects the size-word offset). No null guard in the asm:
// this is only ever reached after the Apt runtime has wired the GC pool.
//
// The cast-to-AptValueGC_MemItem is allocator bookkeeping on the raw pool block
// (the high-bit "allocated" flag the GC walk reads), not a member poke into a live
// C++ object -- the same external-allocator pattern as
// AptValueGC_PoolManager::DeallocateAptValueGC / AptNativeFunction::operator new.
// ---------------------------------------------------------------------------
void* AptScriptFunction1::operator new(size_t size)
{
    void* lpMem = gpGCPoolManager->Allocate(size);
    reinterpret_cast<AptValueGC_MemItem*>(lpMem)->SetIsAllocated(gAptValueGCSizeOffset, true);
    return lpMem;
}

// ---------------------------------------------------------------------------
// operator delete @ 0x82AF13A0
//
// Return the block to the GC pool; on a successful free, clear the
// AptValueGC_MemItem allocated flag. (The X360 `clrlwi. r11, r3, 24` tests the
// bool result -- Deallocate returns true on success.)
// ---------------------------------------------------------------------------
void AptScriptFunction1::operator delete(void* p, size_t size)
{
    if (gpGCPoolManager->Deallocate(p, size))
        reinterpret_cast<AptValueGC_MemItem*>(p)->SetIsAllocated(gAptValueGCSizeOffset, false);
}

// ---------------------------------------------------------------------------
// ctor @ 0x82AF1308
//
// X360: AptScriptFunctionBase::AptScriptFunctionBase(this, 34, pCallContext, pCIH,
// /*bMakePrototype*/1); store the compiled record at +0x30; install the
// AptScriptFunction1 vtable (off_82145CA8, emitted automatically by the compiler).
// ---------------------------------------------------------------------------
AptScriptFunction1::AptScriptFunction1(AptValue* pCallContext,
                                       AptScriptFunction1ByteCode* pByteCode,
                                       AptValue* pCIH)
    : AptScriptFunctionBase(AptVFT_ScriptFunction1, pCallContext, pCIH, true)
    , mpByteCode(pByteCode)
{
}

// ---------------------------------------------------------------------------
// copy ctor @ 0x82B01000 (sub_82B01000)
//
// X360: AptScriptFunctionBase copy ctor (this, 34, rOther, pCIH); install the
// AptScriptFunction1 vtable; copy the compiled-record pointer (this[12] =
// rOther[12], i.e. mpByteCode = rOther.mpByteCode).
// ---------------------------------------------------------------------------
AptScriptFunction1::AptScriptFunction1(const AptScriptFunction1& rOther, AptValue* pCIH)
    : AptScriptFunctionBase(AptVFT_ScriptFunction1, rOther, pCIH)
    , mpByteCode(rOther.mpByteCode)
{
}

// ---------------------------------------------------------------------------
// Duplicate @ 0x82B024A8
//
// Pool-allocate a fresh AptScriptFunction1 and copy-construct it from this one,
// re-binding it to pCIH. The pool operator new returns null on exhaustion (the asm
// `cmplwi r3, 0 / beq` guards the construct), so this returns null in that case.
// ---------------------------------------------------------------------------
AptScriptFunction1* AptScriptFunction1::Duplicate(AptValue* pCIH) const
{
    void* pMem = AptScriptFunction1::operator new(sizeof(AptScriptFunction1));
    if (pMem)
        return ::new (pMem) AptScriptFunction1(*this, pCIH);   // global placement new (class has its own operator new)
    return nullptr;
}

// ---------------------------------------------------------------------------
// GetName @ 0x82AF1450 -- the function's name string (movie data).
// ---------------------------------------------------------------------------
const char* AptScriptFunction1::GetName() const
{
    return mpByteCode->mpName;
}

// ---------------------------------------------------------------------------
// GetNumArguments @ 0x82AF1460 -- declared parameter count.
// ---------------------------------------------------------------------------
int32_t AptScriptFunction1::GetNumArguments() const
{
    return mpByteCode->mnNumArguments;
}

// ---------------------------------------------------------------------------
// GetByteCodeBase @ 0x82AF1360 -- the start of the action bytecode, which follows
// the compiled-record header inline (+0x18 == &maByteCode[0]).
// ---------------------------------------------------------------------------
void* AptScriptFunction1::GetByteCodeBase() const
{
    return mpByteCode->maByteCode;
}

// ---------------------------------------------------------------------------
// GetByteCodeSize @ 0x82AF1370 -- length in bytes of the action bytecode.
// ---------------------------------------------------------------------------
int32_t AptScriptFunction1::GetByteCodeSize() const
{
    return mpByteCode->mnByteCodeSize;
}

// ---------------------------------------------------------------------------
// GetConstantPool @ 0x82AF1380 -- the constant-pool string table + its entry count.
// (The X360 returns the {entries, count} pair through a hidden result pointer.)
// ---------------------------------------------------------------------------
AptConstantPool AptScriptFunction1::GetConstantPool() const
{
    AptConstantPool pool;
    pool.mppEntries = mpByteCode->mppConstantPool;
    pool.mnCount    = mpByteCode->mnConstantPoolCount;
    return pool;
}

// ---------------------------------------------------------------------------
// SetArgument @ 0x82AF5338
//
// Bind the nArgIndex'th parameter name to pValue in the current call's local frame.
// The frame stack is created lazily on first use, then the parameter name (a movie
// string) is wrapped in a temporary EAStringC key and inserted into the frame's
// local-variable hash (AptValueWithHash::Set on the AptFrameStack). The temporary
// key releases its string reference when it goes out of scope (the X360's trailing
// EAStringC::DecreaseInternalRefCount).
// ---------------------------------------------------------------------------
void AptScriptFunction1::SetArgument(int32_t nArgIndex, AptValue* pValue)
{
    if (!AptScriptFunctionBase::spFrameStack)
        CreateFrameStack();

    EAStringC key(mpByteCode->mppArgumentNames[nArgIndex]);
    AptScriptFunctionBase::spFrameStack->Set(key, pValue);
}
