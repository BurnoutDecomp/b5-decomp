// ===========================================================================
// EATech Apt -- AptScriptFunctionBase: the shared base of the script-function
// value types (the per-call frame/register execution machinery + the function
// value's scope state).   DECOMPILED from the X360 ARTIST. This is the keystone
// the interpreter's call subsystem + scope-chain resolution build on:
//   ctor @0x82AF1030 / CreateFrameStack @0x82AF1260 / GetInScopeChain @0x82AE1990 /
//   SetInLocalScope @0x82AF52C0 / SetWhereExistsInScopeChain @0x82AF5308 /
//   ExistsInLocalScope @0x82AE1938 / SetRegisterValue @0x82AD6690 /
//   CreatingNestedFunction @0x82AF12F0 / SetupBeforeExecution @0x82AD6618 /
//   CleanupAfterExecution @0x82AD6630 / InitializeStaticData @0x82AE26C0 /
//   ShutdownStaticData @0x82AE2758 / RegisterReferences @0x82AE27B0 /
//   DestroyGCPointers @0x82AF1508 / ~AptScriptFunctionBase (@0x82AF5B08 thunk).
//
// The execution state (current frame stack + flat register window) is PROCESS-WIDE
// (X360 .data globals off_8324E3DC / off_8324E3D0 / dword_8324E3D4), modelled as the
// class statics. The scope methods delegate to the (landed) AptFrameStack chain.
//
// The CIH / GC leaf couplings (the value-object lifecycle, not the frame
// machinery the interpreter needs) -- all homed:
//   * The ctor's pCallContext vtbl+0x60 dispatch is CreateFrameStack (the slot
//     after CleanupAfterExecution): lazily build the caller's activation frame
//     before capturing it as the closure scope. Called directly as the member.
//   * AptApt_DeriveFunctionAnimation -- derive the timeline animation a function is
//     defined on from its CIH (the X360 walks the CIH/character chain; uses the
//     reconstructed AptGetAnimationAtLevel at the leaf).
//   * The owning animation's "+0x0C character ref" is the CIH ZOMBIE-COUNT field
//     (bits 7-22, step 0x80): Inc/DecZombieCount are called directly (the drop-
//     to-zero reap lives inside DecZombieCount).
//   * gpAptFunctionPrototypeRoot (dword_8324E4EC) -- the Function.prototype object a
//     freshly-made function prototype's __proto__ links to (wired at AptInit).
//   * AptValue::sReferenceRegistrationCb -- the GC mark callback (null until AptInit).
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

// XDK size optimization matches ARTIST at 0x82AF5B08 and 0x82AF0128.
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif

#include <cstdlib>   // abort (the _purecall pure-virtual terminator @0x82C08F60)

#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"
#include "SDKs/EATech/include/Apt/AptScriptFunctionBase.h"
#include "SDKs/EATech/include/Apt/AptFrameStack.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/include/Apt/AptNativeHash.h"
#include "SDKs/EATech/include/Apt/AptPrototype.h"
#include "SDKs/EATech/include/Apt/AptCharacterHelper.h"      // AptGetAnimationAtLevel
#include "SDKs/EATech/include/Apt/AptCIH.h"                   // GetRootAnimation (DeriveFunctionAnimation)
#include "SDKs/EATech/Apt/DogmaAllocator.h"                  // DOGMA_PoolManager
#include "SDKs/EATech/include/Apt/AptValue/AptValueVector.h" // gpAptOperandStackPool

// GC leaf couplings (see header above) -- defined in AptGlobals.cpp, wired by
// AptValueInitialize (AptInit.cpp).
extern AptValue* gpAptFunctionPrototypeRoot;                                      // dword_8324E4EC
extern AptValue* gpUndefinedValue;                                               // register-array fill (AptInit)

// ---- process-wide execution state (X360 .data globals) --------------------
AptFrameStack* AptScriptFunctionBase::spFrameStack     = 0;   // off_8324E3DC
AptValue**     AptScriptFunctionBase::spRegBlockBase             = 0;   // dword_8324E3CC (heap anchor)
AptValue**     AptScriptFunctionBase::spRegBlockCurrentFrameBase = 0;   // off_8324E3D0 (moving window base)
int32_t        AptScriptFunctionBase::snRegBlockCurrentFrameCount = 0;  // dword_8324E3D4 (live window count)
int32_t        AptScriptFunctionBase::snRegisterBlockSize        = 0;   // dword_8324E3D8 (allocated slots)

// ===========================================================================
// Construction / destruction
// ===========================================================================

// ctor @0x82AF1030 -- bind to pCIH, capture the enclosing call frame as the closure
// scope (when defined inside a call), derive the owning timeline animation, and
// optionally build a fresh prototype object.
// AptApt_DeriveFunctionAnimation inlined at its call sites (AptCIH::GetRootAnimation).

AptScriptFunctionBase::AptScriptFunctionBase(AptVirtualFunctionTable_Indices eType,
                                             AptValue* pCallContext,
                                             AptValue* pCIH,
                                             bool      bMakePrototype)
    : AptObject(eType, 8)   // -> AptValueWithHash(eType, 8); clears the class-flags word
    , mpCIH(pCIH)
    , mpParentAnim(0)
    , mpCreatorScope(0)
    , mnFrameStackReserve(0)
{
    // Defined inside a live call -> capture the current frame stack as the closure scope.
    if (pCallContext)
    {
        // ARTIST dispatches CreatingNestedFunction at virtual slot +0x60.
        static_cast<AptScriptFunctionBase*>(pCallContext)->CreatingNestedFunction();
        mpCreatorScope = spFrameStack;
        if (spFrameStack)
            spFrameStack->AddRef();
    }

    // The timeline animation this function is defined on: walk from the CIH, else the
    // level-0 root.
    const AptVirtualFunctionTable_Indices eCIH = pCIH->getVtblIndex();
    if ((eCIH == AptVFT_CharacterInstHandle && pCIH->getIsDefined())
        || eCIH == static_cast<AptVirtualFunctionTable_Indices>(37))
        mpParentAnim = static_cast<AptValue*>(reinterpret_cast<AptCIH*>(pCIH)->GetRootAnimation());   // CIH-chain walk (-> AptGetAnimationAtLevel)
    else
        mpParentAnim = reinterpret_cast<AptValue*>(AptGetAnimationAtLevel(0));

    mpCIH->AddRef();
    mpParentAnim->AddRef();
    // The +0x0C "character ref" the console bumps IS the CIH zombie-count field
    // (bits 7-22, step 0x80 -- the ctor asm's extlwi/addi 0x80/rlwimi on anim+0xC).
    static_cast<AptCIH*>(mpParentAnim)->IncZombieCount();

    if (bMakePrototype)
    {
        AptPrototype* pProto = new AptPrototype();
        GetNativeHashVirtual()->SetPrototype(pProto);
        pProto->GetNativeHashVirtual()->Set__Proto__(gpAptFunctionPrototypeRoot);   // dword_8324E4EC -- the builtin prototype root, seeded by AptValueInitialize (AptInit.cpp)
    }
}

// copy ctor @0x82B00E90 (sub_82B00E90; PS3 DecFIGS
// _ZN21AptScriptFunctionBaseC2E31AptVirtualFunctionTable_IndicesPS_P6AptCIH @0xF5A94C)
// -- duplicate rOther's scope/prototype state while re-binding to pCIH. Used by the
// subclass Duplicate paths (AptScriptFunction1::Duplicate -> its own copy ctor ->
// this base copy ctor). Unlike the primary ctor it takes rOther.mpCreatorScope
// directly (no PrepareCallContextScope -- the enclosing frame is inherited verbatim
// from the original) and copies the prototype / __proto__ from rOther's embedded
// hash instead of optionally building a fresh prototype.
AptScriptFunctionBase::AptScriptFunctionBase(AptVirtualFunctionTable_Indices eType,
                                             const AptScriptFunctionBase& rOther,
                                             AptValue* pCIH)
    : AptObject(eType, 8)   // -> AptValueWithHash(eType, 8); clears the class-flags word
    , mpCIH(pCIH)
    , mpParentAnim(0)
    , mpCreatorScope(rOther.mpCreatorScope)   // a3[10]: inherit the enclosing frame verbatim
    , mnFrameStackReserve(0)
{
    // Derive the owning timeline animation from the (re-bound) CIH -- identical walk to
    // the primary ctor (the X360 starts from a4 == pCIH, which is what we store at +0x20).
    const AptVirtualFunctionTable_Indices eCIH = pCIH->getVtblIndex();
    if ((eCIH == AptVFT_CharacterInstHandle && pCIH->getIsDefined())
        || eCIH == static_cast<AptVirtualFunctionTable_Indices>(37))
        mpParentAnim = static_cast<AptValue*>(reinterpret_cast<AptCIH*>(pCIH)->GetRootAnimation());   // CIH-chain walk (-> AptGetAnimationAtLevel)
    else
        mpParentAnim = reinterpret_cast<AptValue*>(AptGetAnimationAtLevel(0));

    // AddRef the inherited scope (when present) + the CIH + the owning animation, then
    // bump the animation's zombie-count field (the same IncZombieCount the primary
    // ctor performs).
    if (mpCreatorScope)
        mpCreatorScope->AddRef();
    mpCIH->AddRef();
    mpParentAnim->AddRef();
    // The +0x0C "character ref" the console bumps IS the CIH zombie-count field
    // (bits 7-22, step 0x80 -- the ctor asm's extlwi/addi 0x80/rlwimi on anim+0xC).
    static_cast<AptCIH*>(mpParentAnim)->IncZombieCount();

    // Copy the prototype + __proto__ links from the original (a3[5] / a3[4]).
    // GetNativeHashVirtual is non-const (the SDK accessor is not const-qualified), so
    // read rOther's hash through a const_cast -- the read is non-mutating in practice.
    AptNativeHash* pOtherHash =
        const_cast<AptScriptFunctionBase&>(rOther).GetNativeHashVirtual();
    GetNativeHashVirtual()->SetPrototype(pOtherHash->GetPrototype());
    GetNativeHashVirtual()->Set__Proto__(pOtherHash->Get__Proto__());
}

// ~AptScriptFunctionBase is header-inline (empty; the GC pointers are released by
// DestroyGCPointers and the destructor chains to ~AptObject).

// ===========================================================================
// Per-call frame stack + register window (the interpreter's call machinery)
// ===========================================================================

// CreateFrameStack @0x82AF1260 -- install a fresh local-variable frame chained to
// this function's captured scope, as the process-wide current frame.
void AptScriptFunctionBase::CreateFrameStack()
{
    if (mnFrameStackReserve != 0)
        spFrameStack = new AptFrameStack(mpCreatorScope, mnFrameStackReserve);
    else
        spFrameStack = new AptFrameStack(mpCreatorScope);
    spFrameStack->AddRef();
}

// SetupBeforeExecution @0x82AD6618 (base) -- snapshot the current frame stack into
// pSaved and clear it (the AptScriptFunction2 override additionally swaps the
// register window + pre-binds this/super/arguments/_root/_parent/_global).
void AptScriptFunctionBase::SetupBeforeExecution(SavedExecutionState* pSaved,
                                                 AptValue* /*pArgScope*/,
                                                 AptValue* /*pPreloadThis*/,
                                                 AptValue* /*pPreloadArgs*/)
{
    pSaved->mpSavedFrameStack = spFrameStack;
    spFrameStack = 0;
}

// CleanupAfterExecution @0x82AD6630 (base) -- release the current frame stack and
// restore the saved one (the AptScriptFunction2 override also restores registers).
void AptScriptFunctionBase::CleanupAfterExecution(SavedExecutionState* pSaved)
{
    if (spFrameStack)
    {
        mnFrameStackReserve = static_cast<uint16_t>(spFrameStack->GetNativeHashVirtual()->GetHashSize());
        spFrameStack->Release();
    }
    spFrameStack = pSaved->mpSavedFrameStack;
}

// SetRegisterValue @0x82AD6690 -- store pValue into flat register slot nRegister,
// growing the live high-water mark (AddRef new, Release the displaced value).
void AptScriptFunctionBase::SetRegisterValue(int32_t nRegister, AptValue* pValue)
{
    if (nRegister + 1 > snRegBlockCurrentFrameCount)
        snRegBlockCurrentFrameCount = nRegister + 1;
    AptValue* pOld = spRegBlockCurrentFrameBase[nRegister];
    spRegBlockCurrentFrameBase[nRegister] = pValue;
    pValue->AddRef();
    // InitializeStaticData fills the slots with gpUndefinedValue before this opcode runs.
    pOld->Release();
}

// GetRegisterValue -- the symmetric read: return the live value in register slot
// nRegister of the current window. The console inlines this trivial indexed accessor
// at its call sites (e.g. stackPushIndirect); recovered here as the named member.
AptValue* AptScriptFunctionBase::GetRegisterValue(int32_t nRegister)
{
    return spRegBlockCurrentFrameBase[nRegister];
}

// PopStaticData (PS3 @0x7E0AB8) -- pop the current register-block frame back to
// pSavedBase. DECOMPILED from the PS3 asm: r3 (the console method's `this`) IS the
// saved base (the arg is unused); each slot's value is Released (vtable slot 1) and
// reset to gpUndefinedValue; then the base moves to pSavedBase and the count spans
// [pSavedBase, oldBase). pSavedBase is the saved arg-heap pointer the interpreter
// captured before the run (spRegBlockCurrentFrameBase tracks gAptParseArgHeapPtr).
// The console counts 4-byte slots ((old-new)>>2); typed AptValue** subtraction gives
// the element count directly (x64-correct: 8-byte slots).
void AptScriptFunctionBase::PopStaticData(void* pSavedBase)
{
    AptValue** const pOldBase = spRegBlockCurrentFrameBase;
    for (int32_t i = 0; i < snRegBlockCurrentFrameCount; ++i)
    {
        AptValue* const pValue = pOldBase[i];
        pOldBase[i] = gpUndefinedValue;
        pValue->Release();
    }
    spRegBlockCurrentFrameBase  = static_cast<AptValue**>(pSavedBase);
    snRegBlockCurrentFrameCount = static_cast<int32_t>(pOldBase - static_cast<AptValue**>(pSavedBase));
}

// PushStaticData (PS3 @0x7E0A90) -- start a new empty register-block frame: return the
// current base (the caller's restore point), advance the base past the current frame,
// and zero the count. DECOMPILED from PS3 pseudocode (base += 4*count on the console;
// typed AptValue** arithmetic gives the x64-correct element stride). The inverse of
// PopStaticData.
AptValue** AptScriptFunctionBase::PushStaticData()
{
    AptValue** const pOldBase = spRegBlockCurrentFrameBase;
    spRegBlockCurrentFrameBase = spRegBlockCurrentFrameBase + snRegBlockCurrentFrameCount;
    snRegBlockCurrentFrameCount = 0;
    return pOldBase;
}

// ===========================================================================
// Lexical scope chain (variable resolution / local definition)
// ===========================================================================

// CreatingNestedFunction @0x82AF12F0 -- ensure a live frame exists (used while a
// nested DefineFunction is being built so its captured scope is materialised).
void AptScriptFunctionBase::CreatingNestedFunction()
{
    if (!spFrameStack)
        CreateFrameStack();
}

// GetInScopeChain @0x82AE1990 -- resolve key by walking the live frame (or this
// function's captured scope) outward.
AptValue* AptScriptFunctionBase::GetInScopeChain(const EAStringC& key)
{
    if (spFrameStack == 0)
    {
        if (mpCreatorScope != 0)
            return mpCreatorScope->GetInScopeChain(key);
        return 0;
    }
    return spFrameStack->GetInScopeChain(key);
}

// SetWhereExistsInScopeChain @0x82AF5308 -- store key where it already exists up the
// chain; returns false if it is nowhere in scope.
bool AptScriptFunctionBase::SetWhereExistsInScopeChain(const EAStringC& key, AptValue* pValue)
{
    if (spFrameStack == 0)
    {
        if (mpCreatorScope != 0)
            return mpCreatorScope->SetWhereExistsInScopeChain(key, pValue);
        return false;
    }
    return spFrameStack->SetWhereExistsInScopeChain(key, pValue);
}

// ExistsInLocalScope @0x82AE1938 -- is key a local of the innermost frame?
bool AptScriptFunctionBase::ExistsInLocalScope(const EAStringC& key)
{
    AptFrameStack* pFrame = spFrameStack;
    if (pFrame == 0)
    {
        pFrame = mpCreatorScope;
        if (pFrame == 0)
            return false;
    }
    return pFrame->Lookup(key) != 0;
}

// SetInLocalScope @0x82AF52C0 -- define key in the innermost frame (creating one if
// none is live).
void AptScriptFunctionBase::SetInLocalScope(const EAStringC& key, AptValue* pValue)
{
    if (!spFrameStack)
        CreateFrameStack();
    spFrameStack->Set(key, pValue);
}

// ===========================================================================
// Static execution-state lifetime + GC
// ===========================================================================

// InitializeStaticData @0x82AE26C0 -- allocate the flat register array (nRegisterCount
// slots) from the operand-stack pool and fill it with `undefined`.
void AptScriptFunctionBase::InitializeStaticData(const AptInitParmsT& rParms)
{
    snRegisterBlockSize = rParms.iRegisterCount;
    spRegBlockBase = static_cast<AptValue**>(
        gpAptOperandStackPool->Allocate(sizeof(AptValue*) * snRegisterBlockSize));
    spRegBlockCurrentFrameBase = spRegBlockBase;
    for (int32_t i = 0; i < snRegisterBlockSize; ++i)
        spRegBlockBase[i] = gpUndefinedValue;
    snRegBlockCurrentFrameCount = 0;
}

// ShutdownStaticData @0x82AE2758 -- return the register array to the pool.
void AptScriptFunctionBase::ShutdownStaticData()
{
    // Free the one flat block through the fixed anchor (dword_8324E3CC) -- the moving
    // window base may have advanced past it. x64: 8-byte slots (console 4*).
    gpAptOperandStackPool->Deallocate(spRegBlockBase, sizeof(AptValue*) * snRegisterBlockSize);
    spRegBlockBase             = 0;
    spRegBlockCurrentFrameBase = 0;
}

// RegisterReferences @0x82AE27B0 -- GC-mark the embedded hash + the scope pointers.
void AptScriptFunctionBase::RegisterReferences()
{
    GetNativeHashVirtual()->RegisterReferences(this);
    if (mpCreatorScope)
        AptValue::sReferenceRegistrationCb(this, reinterpret_cast<AptValue**>(&mpCreatorScope), "ParentScope", 0);
    if (mpCIH)
        AptValue::sReferenceRegistrationCb(this, &mpCIH, "CIH", 0);
    if (mpParentAnim)
        AptValue::sReferenceRegistrationCb(this, &mpParentAnim, "ParentAnim", 0);
}

// DestroyGCPointers @0x82AF1508 -- release the scope pointers (dropping the owning
// animation's character reference) and tear down the hash.
void AptScriptFunctionBase::DestroyGCPointers()
{
    if (mpCreatorScope)
        mpCreatorScope->Release();
    mpCreatorScope = 0;

    mpCIH->Release();
    mpCIH = 0;

    // The inverse of the ctor's IncZombieCount (the zombie-vector reap arm lives
    // inside DecZombieCount itself, firing when the count reaches zero).
    static_cast<AptCIH*>(mpParentAnim)->DecZombieCount();
    mpParentAnim->Release();
    mpParentAnim = 0;

    GetNativeHashVirtual()->DestroyGCPointers();
}
