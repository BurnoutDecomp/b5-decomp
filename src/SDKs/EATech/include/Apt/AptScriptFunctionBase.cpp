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
// FLAG -- the CIH / GC leaf couplings (the value-object lifecycle, not the frame
// machinery the interpreter needs):
//   * AptApt_PrepareCallContextScope -- the ctor calls pCallContext's prepare
//     virtual before capturing the live frame as the closure scope (the call
//     context type is not pinned, so the call is encapsulated).
//   * AptApt_DeriveFunctionAnimation -- derive the timeline animation a function is
//     defined on from its CIH (the X360 walks the CIH/character chain; uses the
//     reconstructed AptGetAnimationAtLevel at the leaf).
//   * AptApt_AnimationAddCharacterRef / ReleaseCharacterRef -- the +0x0C character
//     ref-counter twiddle on the owning animation (+ AptUpdateZombieVector on the
//     drop to zero); CIH internals.
//   * gpAptFunctionPrototypeRoot (dword_8324E4EC) -- the Function.prototype object a
//     freshly-made function prototype's __proto__ links to (wired at AptInit).
//   * AptValue::sReferenceRegistrationCb -- the GC mark callback (null until AptInit).
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstdlib>   // abort (the _purecall pure-virtual terminator @0x82C08F60)

#include "SDKs/EATech/include/Apt/AptScriptFunctionBase.h"
#include "SDKs/EATech/include/Apt/AptFrameStack.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/include/Apt/AptNativeHash.h"
#include "SDKs/EATech/include/Apt/AptPrototype.h"
#include "SDKs/EATech/include/Apt/AptCharacterHelper.h"      // AptGetAnimationAtLevel
#include "SDKs/EATech/Apt/DogmaAllocator.h"                  // DOGMA_PoolManager
#include "SDKs/EATech/include/Apt/AptValue/AptValueVector.h" // gpAptOperandStackPool

// FLAG (CIH / GC leaf couplings -- see header above; wired with the AptCIH + AptInit TUs).
extern void      AptApt_PrepareCallContextScope(AptValue* pCallContext);          // ctor: vtbl prepare
extern AptValue* AptApt_DeriveFunctionAnimation(AptValue* pCIH);                  // ctor/derive timeline
extern void      AptApt_AnimationAddCharacterRef(AptValue* pAnimation);           // ctor: +0x0C ref++
extern void      AptApt_AnimationReleaseCharacterRef(AptValue* pAnimation);       // dtor: +0x0C ref-- (+zombie)
extern AptValue* gpAptFunctionPrototypeRoot;                                      // dword_8324E4EC
extern AptValue* gpUndefinedValue;                                               // register-array fill (AptInit)

// ---- process-wide execution state (X360 .data globals) --------------------
AptFrameStack* AptScriptFunctionBase::spFrameStack     = 0;   // off_8324E3DC
AptValue**     AptScriptFunctionBase::spRegisters      = 0;   // off_8324E3D0
int32_t        AptScriptFunctionBase::snRegisterCount  = 0;   // dword_8324E3D4 (live high-water)
int32_t        AptScriptFunctionBase::snRegisterCapacity = 0; // dword_8324E3D8 (allocated slots)

// ===========================================================================
// Construction / destruction
// ===========================================================================

// ctor @0x82AF1030 -- bind to pCIH, capture the enclosing call frame as the closure
// scope (when defined inside a call), derive the owning timeline animation, and
// optionally build a fresh prototype object.
AptScriptFunctionBase::AptScriptFunctionBase(AptVirtualFunctionTable_Indices eType,
                                             AptValue* pCallContext,
                                             AptValue* pCIH,
                                             bool      bMakePrototype)
    : AptObject(eType, 8)   // -> AptValueWithHash(eType, 8); clears the class-flags word
    , mpCIH(pCIH)
    , mpParentAnim(0)
    , mpParentScope(0)
    , mnCreatingNestedFunction(0)
{
    // Defined inside a live call -> capture the current frame stack as the closure scope.
    if (pCallContext)
    {
        AptApt_PrepareCallContextScope(pCallContext);   // FLAG: console pCallContext->vtbl prepare
        mpParentScope = reinterpret_cast<AptValue*>(spFrameStack);
        if (spFrameStack)
            spFrameStack->AddRef();
    }

    // The timeline animation this function is defined on: walk from the CIH, else the
    // level-0 root.
    const AptVirtualFunctionTable_Indices eCIH = pCIH->getVtblIndex();
    if ((eCIH == AptVFT_CharacterInstHandle && pCIH->getIsDefined())
        || eCIH == static_cast<AptVirtualFunctionTable_Indices>(37))
        mpParentAnim = AptApt_DeriveFunctionAnimation(pCIH);   // FLAG: CIH-chain walk (-> AptGetAnimationAtLevel)
    else
        mpParentAnim = reinterpret_cast<AptValue*>(AptGetAnimationAtLevel(0));

    mpCIH->AddRef();
    mpParentAnim->AddRef();
    AptApt_AnimationAddCharacterRef(mpParentAnim);   // FLAG: +0x0C character ref-counter ++

    if (bMakePrototype)
    {
        AptPrototype* pProto = new AptPrototype();
        GetNativeHashVirtual()->SetPrototype(pProto);
        pProto->GetNativeHashVirtual()->Set__Proto__(gpAptFunctionPrototypeRoot);   // FLAG: Function.prototype root
    }
}

// copy ctor @0x82B00E90 (sub_82B00E90; PS3 DecFIGS
// _ZN21AptScriptFunctionBaseC2E31AptVirtualFunctionTable_IndicesPS_P6AptCIH @0xF5A94C)
// -- duplicate rOther's scope/prototype state while re-binding to pCIH. Used by the
// subclass Duplicate paths (AptScriptFunction1::Duplicate -> its own copy ctor ->
// this base copy ctor). Unlike the primary ctor it takes rOther.mpParentScope
// directly (no PrepareCallContextScope -- the enclosing frame is inherited verbatim
// from the original) and copies the prototype / __proto__ from rOther's embedded
// hash instead of optionally building a fresh prototype.
AptScriptFunctionBase::AptScriptFunctionBase(AptVirtualFunctionTable_Indices eType,
                                             const AptScriptFunctionBase& rOther,
                                             AptValue* pCIH)
    : AptObject(eType, 8)   // -> AptValueWithHash(eType, 8); clears the class-flags word
    , mpCIH(pCIH)
    , mpParentAnim(0)
    , mpParentScope(rOther.mpParentScope)   // a3[10]: inherit the enclosing frame verbatim
    , mnCreatingNestedFunction(0)
{
    // Derive the owning timeline animation from the (re-bound) CIH -- identical walk to
    // the primary ctor (the X360 starts from a4 == pCIH, which is what we store at +0x20).
    const AptVirtualFunctionTable_Indices eCIH = pCIH->getVtblIndex();
    if ((eCIH == AptVFT_CharacterInstHandle && pCIH->getIsDefined())
        || eCIH == static_cast<AptVirtualFunctionTable_Indices>(37))
        mpParentAnim = AptApt_DeriveFunctionAnimation(pCIH);   // FLAG: CIH-chain walk (-> AptGetAnimationAtLevel)
    else
        mpParentAnim = reinterpret_cast<AptValue*>(AptGetAnimationAtLevel(0));

    // AddRef the inherited scope (when present) + the CIH + the owning animation, then
    // bump the animation's +0x0C character ref-counter (the same twiddle the primary
    // ctor performs via AptApt_AnimationAddCharacterRef).
    if (mpParentScope)
        mpParentScope->AddRef();
    mpCIH->AddRef();
    mpParentAnim->AddRef();
    AptApt_AnimationAddCharacterRef(mpParentAnim);   // FLAG: +0x0C character ref-counter ++

    // Copy the prototype + __proto__ links from the original (a3[5] / a3[4]).
    // GetNativeHashVirtual is non-const (the SDK accessor is not const-qualified), so
    // read rOther's hash through a const_cast -- the read is non-mutating in practice.
    AptNativeHash* pOtherHash =
        const_cast<AptScriptFunctionBase&>(rOther).GetNativeHashVirtual();
    GetNativeHashVirtual()->SetPrototype(pOtherHash->GetPrototype());
    GetNativeHashVirtual()->Set__Proto__(pOtherHash->Get__Proto__());
}

// ~AptScriptFunctionBase (@0x82AF5B08 scalar-deleting-destructor thunk) -- the GC
// pointers are released by DestroyGCPointers; the dtor chains to ~AptObject.
AptScriptFunctionBase::~AptScriptFunctionBase()
{
}

// ===========================================================================
// Pure-virtual script-function interface (base slots are _purecall @0x82C08F60)
// ===========================================================================
//
// The compiled-body accessors (GetName / GetNumArguments / GetByteCodeBase /
// GetByteCodeSize / GetConstantPool / SetArgument) and Duplicate are PURE VIRTUAL on
// AptScriptFunctionBase: in the X360 ARTIST.XEX every one of these slots in the base
// vtable (off_82145C40 slots 15-20, 22) holds _purecall (0x82C08F60), and each
// concrete subclass (AptScriptFunction1 / AptScriptFunction2 /
// AptScriptFunctionByteCodeBlock) installs its own override. The base declares them
// non-pure (so the three subclass vtables share the base layout and the interpreter
// can name them on an AptScriptFunctionBase*); these out-of-line definitions
// reproduce the _purecall slot faithfully -- reaching one means a pure base method
// was dispatched (a subclass forgot to override), which the X360 _purecall handles by
// aborting. They return a type-appropriate default after the abort so the (never
// taken) fallthrough type-checks.
//
// FLAG (reconstruction): _purecall (@0x82C08F60) is a CRT abort handler
// (_NMSG_WRITE(0x19) -> _set_abort_behavior -> abort, after an optional installed
// gpPureCallHandler at dword_832BADDC). Modelled here with the PC CRT abort() (the
// faithful "pure virtual call" terminator); the dword_832BADDC user-handler hook is
// not wired on PC.

const char* AptScriptFunctionBase::GetName() const
{
    abort();           // _purecall @0x82C08F60 -- pure virtual; subclass must override
    return 0;
}

int32_t AptScriptFunctionBase::GetNumArguments() const
{
    abort();           // _purecall
    return 0;
}

void* AptScriptFunctionBase::GetByteCodeBase() const
{
    abort();           // _purecall
    return 0;
}

int32_t AptScriptFunctionBase::GetByteCodeSize() const
{
    abort();           // _purecall
    return 0;
}

AptConstantPool AptScriptFunctionBase::GetConstantPool() const
{
    abort();           // _purecall
    AptConstantPool pool = { 0, 0 };
    return pool;
}

void AptScriptFunctionBase::SetArgument(int32_t /*nArgIndex*/, AptValue* /*pValue*/)
{
    abort();           // _purecall
}

AptScriptFunctionBase* AptScriptFunctionBase::Duplicate(AptValue* /*pCIH*/) const
{
    abort();           // _purecall
    return 0;
}

// ===========================================================================
// Per-call frame stack + register window (the interpreter's call machinery)
// ===========================================================================

// CreateFrameStack @0x82AF1260 -- install a fresh local-variable frame chained to
// this function's captured scope, as the process-wide current frame.
void AptScriptFunctionBase::CreateFrameStack()
{
    AptFrameStack* pEnclosing = reinterpret_cast<AptFrameStack*>(mpParentScope);
    AptFrameStack* pFrame = new AptFrameStack(pEnclosing);
    spFrameStack = pFrame;
    pFrame->AddRef();
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
        // FLAG: the console also stashes the low halfword of the frame's hash header
        // into mnCreatingNestedFunction here -- an opaque side-effect of this
        // degenerate base (the real teardown is the AptScriptFunction2 override);
        // the essential action is releasing the frame.
        spFrameStack->Release();
    }
    spFrameStack = pSaved->mpSavedFrameStack;
}

// SetRegisterValue @0x82AD6690 -- store pValue into flat register slot nRegister,
// growing the live high-water mark (AddRef new, Release the displaced value).
void AptScriptFunctionBase::SetRegisterValue(int32_t nRegister, AptValue* pValue)
{
    if (nRegister + 1 > snRegisterCount)
        snRegisterCount = nRegister + 1;
    AptValue* pOld = spRegisters[nRegister];
    spRegisters[nRegister] = pValue;
    pValue->AddRef();
    pOld->Release();
}

// ===========================================================================
// Lexical scope chain (variable resolution / local definition)
// ===========================================================================

// CreatingNestedFunction @0x82AF12F0 -- ensure a live frame exists (used while a
// nested DefineFunction is being built so its captured scope is materialised).
bool AptScriptFunctionBase::CreatingNestedFunction() const
{
    if (!spFrameStack)
        const_cast<AptScriptFunctionBase*>(this)->CreateFrameStack();
    return mnCreatingNestedFunction != 0;
}

// GetInScopeChain @0x82AE1990 -- resolve key by walking the live frame (or this
// function's captured scope) outward.
AptValue* AptScriptFunctionBase::GetInScopeChain(const EAStringC& key)
{
    AptFrameStack* pFrame = spFrameStack
        ? spFrameStack
        : reinterpret_cast<AptFrameStack*>(mpParentScope);
    return pFrame ? pFrame->GetInScopeChain(key) : 0;
}

// SetWhereExistsInScopeChain @0x82AF5308 -- store key where it already exists up the
// chain; returns false if it is nowhere in scope.
bool AptScriptFunctionBase::SetWhereExistsInScopeChain(const EAStringC& key, AptValue* pValue)
{
    AptFrameStack* pFrame = spFrameStack
        ? spFrameStack
        : reinterpret_cast<AptFrameStack*>(mpParentScope);
    return pFrame ? pFrame->SetWhereExistsInScopeChain(key, pValue) : false;
}

// ExistsInLocalScope @0x82AE1938 -- is key a local of the innermost frame?
bool AptScriptFunctionBase::ExistsInLocalScope(const EAStringC& key)
{
    AptFrameStack* pFrame = spFrameStack
        ? spFrameStack
        : reinterpret_cast<AptFrameStack*>(mpParentScope);
    return pFrame && pFrame->GetNativeHashVirtual()->Lookup(key) != 0;
}

// SetInLocalScope @0x82AF52C0 -- define key in the innermost frame (creating one if
// none is live).
void AptScriptFunctionBase::SetInLocalScope(const EAStringC& key, AptValue* pValue)
{
    if (!spFrameStack)
        CreateFrameStack();
    spFrameStack->GetNativeHashVirtual()->Set(key, pValue);
}

// ===========================================================================
// Static execution-state lifetime + GC
// ===========================================================================

// InitializeStaticData @0x82AE26C0 -- allocate the flat register array (nRegisterCount
// slots) from the operand-stack pool and fill it with `undefined`.
void AptScriptFunctionBase::InitializeStaticData(int32_t nRegisterCount)
{
    snRegisterCapacity = nRegisterCount;
    spRegisters = reinterpret_cast<AptValue**>(
        gpAptOperandStackPool->Allocate(4 * nRegisterCount));
    for (int32_t i = 0; i < nRegisterCount; ++i)
        spRegisters[i] = gpUndefinedValue;
    snRegisterCount = 0;
}

// ShutdownStaticData @0x82AE2758 -- return the register array to the pool.
void AptScriptFunctionBase::ShutdownStaticData()
{
    gpAptOperandStackPool->Deallocate(spRegisters, 4 * snRegisterCapacity);
    spRegisters = 0;
}

// RegisterReferences @0x82AE27B0 -- GC-mark the embedded hash + the scope pointers.
void AptScriptFunctionBase::RegisterReferences()
{
    GetNativeHashVirtual()->RegisterReferences(this);
    if (mpParentScope)
        AptValue::sReferenceRegistrationCb(this, &mpParentScope, "ParentScope", 0);
    if (mpCIH)
        AptValue::sReferenceRegistrationCb(this, &mpCIH, "CIH", 0);
    if (mpParentAnim)
        AptValue::sReferenceRegistrationCb(this, &mpParentAnim, "ParentAnim", 0);
}

// DestroyGCPointers @0x82AF1508 -- release the scope pointers (dropping the owning
// animation's character reference) and tear down the hash.
void AptScriptFunctionBase::DestroyGCPointers()
{
    if (mpParentScope)
        mpParentScope->Release();
    mpParentScope = 0;

    mpCIH->Release();
    mpCIH = 0;

    AptApt_AnimationReleaseCharacterRef(mpParentAnim);   // FLAG: +0x0C ref-- (+zombie vector)
    mpParentAnim->Release();
    mpParentAnim = 0;

    GetNativeHashVirtual()->DestroyGCPointers();
}
