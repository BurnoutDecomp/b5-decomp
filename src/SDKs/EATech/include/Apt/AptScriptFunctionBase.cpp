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

#include <cstdlib>   // abort (the _purecall pure-virtual terminator @0x82C08F60)

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
    , mpParentScope(0)
    , mnCreatingNestedFunction(0)
{
    // Defined inside a live call -> capture the current frame stack as the closure scope.
    if (pCallContext)
    {
        // The console dispatches pCallContext's vtbl+0x60 -- the slot after
        // CleanupAfterExecution (+0x5C) -- then immediately captures spFrameStack:
        // it is CreateFrameStack (@0x82AF1260, lazily build the caller's activation
        // frame so the nested function has a live frame to close over).
        static_cast<AptScriptFunctionBase*>(pCallContext)->CreateFrameStack();
        mpParentScope = reinterpret_cast<AptValue*>(spFrameStack);
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
        mpParentAnim = static_cast<AptValue*>(reinterpret_cast<AptCIH*>(pCIH)->GetRootAnimation());   // CIH-chain walk (-> AptGetAnimationAtLevel)
    else
        mpParentAnim = reinterpret_cast<AptValue*>(AptGetAnimationAtLevel(0));

    // AddRef the inherited scope (when present) + the CIH + the owning animation, then
    // bump the animation's zombie-count field (the same IncZombieCount the primary
    // ctor performs).
    if (mpParentScope)
        mpParentScope->AddRef();
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
// FLAG PC-platform leaf: _purecall (@0x82C08F60) is a CRT abort handler
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
    if (nRegister + 1 > snRegBlockCurrentFrameCount)
        snRegBlockCurrentFrameCount = nRegister + 1;
    AptValue* pOld = spRegBlockCurrentFrameBase[nRegister];
    spRegBlockCurrentFrameBase[nRegister] = pValue;
    pValue->AddRef();
    // Null-guarded: the console pre-fills the whole register block with the
    // undefined singleton at init, so pOld is never raw null there; the x64
    // block's first-touch slots are zero -- skip the release (x64 hardening,
    // Phase-0 regime).
    if (pOld)
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
    // X360 @0x82AE26C0: cap = *(parms+0x30); block = Allocate(4*cap); anchor
    // (dword_8324E3CC) AND the moving window base (off_8324E3D0) both start at the
    // block; every slot is seeded with the `undefined` singleton (off_8324D814);
    // count = 0. x64: 8-byte AptValue* slots (the console allocates 4*cap bytes).
    snRegisterBlockSize = nRegisterCount;
    AptValue** lpBlock = reinterpret_cast<AptValue**>(
        gpAptOperandStackPool->Allocate(sizeof(AptValue*) * nRegisterCount));
    spRegBlockBase             = lpBlock;
    spRegBlockCurrentFrameBase = lpBlock;
    for (int32_t i = 0; i < nRegisterCount; ++i)
        lpBlock[i] = gpUndefinedValue;
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

    // The inverse of the ctor's IncZombieCount (the zombie-vector reap arm lives
    // inside DecZombieCount itself, firing when the count reaches zero).
    static_cast<AptCIH*>(mpParentAnim)->DecZombieCount();
    mpParentAnim->Release();
    mpParentAnim = 0;

    GetNativeHashVirtual()->DestroyGCPointers();
}
