// ===========================================================================
// EATech Apt -- AptScriptFunction2 out-of-line bodies.
//
// Reconstructed from the X360 ARTIST.XEX (the authoritative spine):
//     AptScriptFunction2::AptScriptFunction2   @ 0x82AF13F8
//     AptScriptFunction2::operator new         @ 0x82AE61C8
//     AptScriptFunction2::operator delete      @ 0x82AF14B0
//     AptScriptFunction2::GetByteCodeBase      @ 0x82AF1470
//     AptScriptFunction2::GetConstantPool      @ 0x82AF1490
//     AptScriptFunction2::SetArgument          @ 0x82AF53A0
//     AptScriptFunction2::SetupBeforeExecution @ 0x82B02558
//     AptScriptFunction2::CleanupAfterExecution@ 0x82AD6708
// The `scalar deleting destructor' @ 0x82AF5BC8 is a compiler thunk (restore the
// base vtable -> ~AptObject -> conditional operator delete) and is dropped, not
// hand-written; the implicit ~AptScriptFunction2 in the header covers it.
//
// See AptScriptFunction2.h for the layout / base derivation and the compiled-record
// (AptScriptFunction2ByteCode) format.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptScriptFunction2.h"
#include "SDKs/EATech/include/Apt/AptObject.h"   // AptValueWithHash (the live _global symbol type)
#include "SDKs/EATech/include/Apt/AptFrameStack.h"               // spFrameStack->Set (the call-local hash)
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"           // AptValue::findChild / Release / mnValueData
#include "SDKs/EATech/include/Apt/AptString/EAString.h"          // EAStringC temporaries
#include "SDKs/EATech/include/Apt/AptDefine.h"                   // gpGCPoolManager
#include "SDKs/EATech/Apt/AptValueGCPoolManager.h"               // AptValueGC_PoolManager + gAptValueGCSizeOffset
#include "SDKs/EATech/Apt/AptValueGCAllocator.h"                 // AptValueGC_MemItem (alloc bookkeeping)

// ---------------------------------------------------------------------------
// FLAG (homed elsewhere; declared here like the rest of the interpreter):
//   gpUndefinedValue   (off_8324D814) -- the shared AS `undefined` singleton, used
//                       as the value for an absent _parent / a preloaded super.
//   gpAptGlobalFallback(off_8324E380) -- the _global fallback scope value.
// Both are wired by the Apt runtime startup; null until then.
// ---------------------------------------------------------------------------
extern AptValue* gpUndefinedValue;
// LIVE symbol note (2026-07-05): off_8324E380's live definition is AptGlobal.cpp's
// AptValueWithHash* -- the old AptValue*-typed extern here bound a dead duplicate,
// so the _global REGISTER PRELOAD loaded null into preloaded registers.
extern AptValueWithHash* gpAptGlobalFallback;

// ---------------------------------------------------------------------------
// FLAG (homed by the Apt string-pool / globals TU, not yet built): the two magic
// child names SetupBeforeExecution resolves via findChild -- the X360 static
// EAStringC constants unk_8324E6C0 ("this") and unk_8324E6B8 ("arguments").
// Declared extern so the preload paths compile by name.
// ---------------------------------------------------------------------------
extern const EAStringC gAptKeyThis;        // unk_8324E6C0 -- "this"
extern const EAStringC gAptKeyArguments;   // unk_8324E6B8 -- "arguments"

// ---------------------------------------------------------------------------
// operator new @ 0x82AE61C8
//
// Allocate from the garbage-collected value pool (off_8324D834 == gpGCPoolManager)
// and mark the AptValueGC_MemItem header allocated (byte_8324D804 ==
// gAptValueGCSizeOffset selects the size-word offset). No null guard in the asm:
// this is only ever reached after the Apt runtime has wired the GC pool.
//
// The cast-to-AptValueGC_MemItem is allocator bookkeeping on the raw pool block
// (the high-bit "allocated" flag the GC walk reads), not a member poke into a live
// C++ object -- the same external-allocator pattern as AptScriptFunction1 /
// AptNativeFunction::operator new.
// ---------------------------------------------------------------------------
void* AptScriptFunction2::operator new(size_t size)
{
    void* lpMem = gpGCPoolManager->Allocate(size);
    reinterpret_cast<AptValueGC_MemItem*>(lpMem)->SetIsAllocated(gAptValueGCSizeOffset, true);
    return lpMem;
}

// ---------------------------------------------------------------------------
// operator delete @ 0x82AF14B0
//
// Return the block to the GC pool; on a successful free, clear the
// AptValueGC_MemItem allocated flag. (The X360 `clrlwi. r11, r3, 24` tests the
// bool result -- Deallocate returns true on success.)
// ---------------------------------------------------------------------------
void AptScriptFunction2::operator delete(void* p, size_t size)
{
    if (gpGCPoolManager->Deallocate(p, size))
        reinterpret_cast<AptValueGC_MemItem*>(p)->SetIsAllocated(gAptValueGCSizeOffset, false);
}

// ---------------------------------------------------------------------------
// ctor @ 0x82AF13F8
//
// X360: AptScriptFunctionBase::AptScriptFunctionBase(this, 35, pCallContext, pCIH,
// /*bMakePrototype*/1); store the compiled record at +0x30; install the
// AptScriptFunction2 vtable (off_82145D10, emitted automatically by the compiler).
// ---------------------------------------------------------------------------
AptScriptFunction2::AptScriptFunction2(AptValue* pCallContext,
                                       AptScriptFunction2ByteCode* pByteCode,
                                       AptValue* pCIH)
    : AptScriptFunctionBase(AptVFT_ScriptFunction2, pCallContext, pCIH, true)
    , mpByteCode(pByteCode)
{
}

// ---------------------------------------------------------------------------
// GetByteCodeBase @ 0x82AF1470 -- the start of the action bytecode, which follows
// the compiled-record header inline (+0x1C == &maByteCode[0]).
// ---------------------------------------------------------------------------
void* AptScriptFunction2::GetByteCodeBase() const
{
    return mpByteCode->maByteCode;
}

// ---------------------------------------------------------------------------
// GetConstantPool @ 0x82AF1490 -- the constant-pool string table + its entry count.
// (The X360 returns the {entries, count} pair through a hidden result pointer.)
// ---------------------------------------------------------------------------
AptConstantPool AptScriptFunction2::GetConstantPool() const
{
    AptConstantPool pool;
    pool.mppEntries = mpByteCode->mppConstantPool;
    pool.mnCount    = mpByteCode->mnConstantPoolCount;
    return pool;
}

// ---------------------------------------------------------------------------
// SetArgument @ 0x82AF53A0
//
// Bind the nArgIndex'th declared parameter to pValue. A DefineFunction2 parameter
// is either pre-bound into a numbered call register (the record's argument-table
// entry names a non-zero register) or passed by name. For the register form, store
// directly into the register file; for the by-name form, create the call-local
// frame stack on first use and insert the parameter name (a movie string, wrapped
// in a temporary EAStringC key that releases its reference on scope exit) into the
// frame's local-variable hash.
// ---------------------------------------------------------------------------
void AptScriptFunction2::SetArgument(int32_t nArgIndex, AptValue* pValue)
{
    const AptScriptFunction2Arg& arg = mpByteCode->mpArgTable[nArgIndex];

    if (arg.mnRegister)
    {
        AptScriptFunctionBase::SetRegisterValue(arg.mnRegister, pValue);
        return;
    }

    if (!AptScriptFunctionBase::spFrameStack)
        CreateFrameStack();

    EAStringC key(arg.mpName);
    AptScriptFunctionBase::spFrameStack->Set(key, pValue);
}

// ---------------------------------------------------------------------------
// SetupBeforeExecution @ 0x82B02558
//
// Enter this function's call: snapshot the interpreter's current frame stack +
// register window into pSaved, then install a fresh window (the new register base
// sits just past the caller's live registers; the live count resets to 0). Then,
// for each register the record's preload flags request, resolve the corresponding
// value and bind it into the next register:
//   THIS       -> the pre-resolved "this" (pPreloadThis), else findChild("this").
//   SUPER      -> `undefined` (this build does not bind a super object).
//   ARGUMENTS  -> the pre-resolved arguments (pPreloadArgs) or one resolved against
//                 pArgScope; if that is null / not a usable object, fall back to one
//                 resolved against the function's own scope.
//   ROOT       -> findChild("_root")  against the function's scope.
//   PARENT     -> findChild("_parent"), or `undefined` when absent.
//   GLOBAL     -> the _global fallback scope value.
// (The register bindings start at register 1; "this" always takes register 1 when
// requested, so the running index begins there.)
// ---------------------------------------------------------------------------
void AptScriptFunction2::SetupBeforeExecution(SavedExecutionState* pSaved,
                                              AptValue* pArgScope,
                                              AptValue* pPreloadThis,
                                              AptValue* pPreloadArgs)
{
    pSaved->mpSavedFrameStack = AptScriptFunctionBase::spFrameStack;
    AptScriptFunctionBase::spFrameStack = 0;

    // Push a fresh register window for this call (the inlined console sequence is
    // exactly PushStaticData: save the base, advance past the live window, zero count).
    pSaved->mpSavedRegisters = AptScriptFunctionBase::PushStaticData();

    const uint16_t uFlags = mpByteCode->muPreloadFlags;
    int32_t nReg = 1;

    if (uFlags & KU_PRELOAD_THIS)
    {
        if (!pPreloadThis)
            pPreloadThis = mpCIH->findChild(&gAptKeyThis, 0);
        AptScriptFunctionBase::SetRegisterValue(1, pPreloadThis);
        nReg = 2;
    }

    if (mpByteCode->muPreloadFlags & KU_PRELOAD_SUPER)
        AptScriptFunctionBase::SetRegisterValue(nReg++, gpUndefinedValue);

    if (mpByteCode->muPreloadFlags & KU_PRELOAD_ARGUMENTS)
    {
        AptValue* pArguments = pPreloadArgs;
        if (!pArguments)
            pArguments = pArgScope->findChild(&gAptKeyArguments, 0);
        // X360: ((pArguments->mnValueData >> 27) & 1) -- a type/validity bit on the
        // resolved value; if it (or the value) is missing, resolve "arguments"
        // against the function's own scope instead.
        if (!pArguments || ((pArguments->mnValueData >> 27) & 1) == 0)
            pArguments = mpCIH->findChild(&gAptKeyArguments, 0);
        AptScriptFunctionBase::SetRegisterValue(nReg++, pArguments);
    }

    if (mpByteCode->muPreloadFlags & KU_PRELOAD_ROOT)
    {
        EAStringC keyRoot("_root");
        AptValue* pRoot = mpCIH->findChild(&keyRoot, 0);
        AptScriptFunctionBase::SetRegisterValue(nReg++, pRoot);
    }

    if (mpByteCode->muPreloadFlags & KU_PRELOAD_PARENT)
    {
        EAStringC keyParent("_parent");
        AptValue* pParent = mpCIH->findChild(&keyParent, 0);
        if (!pParent)
            pParent = gpUndefinedValue;
        AptScriptFunctionBase::SetRegisterValue(nReg++, pParent);
    }

    if (mpByteCode->muPreloadFlags & KU_PRELOAD_GLOBAL)
        AptScriptFunctionBase::SetRegisterValue(nReg, gpAptGlobalFallback);
}

// ---------------------------------------------------------------------------
// CleanupAfterExecution @ 0x82AD6708
//
// Leave this function's call: let the base restore the previous frame stack from
// pSaved, then release every register bound during this call (resetting each slot
// to `undefined`) and restore the previous register window. The new live-register
// count is the size of the window that was in use before this call (the element
// distance from the restored base to the current base).
// ---------------------------------------------------------------------------
void AptScriptFunction2::CleanupAfterExecution(SavedExecutionState* pSaved)
{
    AptScriptFunctionBase::CleanupAfterExecution(pSaved);

    // Pop this call's register window back to the saved base (the inlined console
    // sequence -- release each bound slot + reset it to `undefined`, then restore the
    // base with the count spanning [savedBase, oldBase) -- is exactly PopStaticData).
    AptScriptFunctionBase::PopStaticData(pSaved->mpSavedRegisters);
}
