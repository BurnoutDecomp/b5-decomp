// ============================================================================
// CgsState.cpp
//
// Definition home for the CgsSound::Logic::State base, reconstructed from
// BURNOUT_X360_ARTIST.XEX:
//   State::IsAttachedToThis                                 @ 0x826916D8
//
// The State default ctor reproduces the base-member zero/seed sequence the
// derived state ctors inline (offsets +4..+80). Member access is BY NAME (no
// raw-offset writes); the X360 absolute offsets in CgsState.h are documentation
// only. (2026-08-25, audio-faithfulness wave 5: the three concrete Brn state
// leaves this TU used to body -- PassbyState @0x826BF5E0, StreamingState
// @0x826B0CB0 (+ ~ @0x826C9B28), VehicleState @0x826C9E70 -- moved to their
// DWARF homes under GameSource/Sound/{Passby,Streaming,Vehicles}/.)
//
// Cited by X360 address only -- no leaked-source provenance.
// ============================================================================

#include "GameShared/GameClasses/Sound/Logic/CgsState.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/Logic/CgsEffectBase.h"
#include "GameShared/GameClasses/Sound/Logic/CgsStateManager.h"

namespace CgsSound
{
namespace Logic
{

static ClassTypeInfo<State>* gapStateTypeInfo[State::KU_SIZEOF_CLASS_ARRAY] = { 0 };

// ---------------------------------------------------------------------------
// State::AddToClassTypeInfoArray  @ 0x8268DF08  (static array dword_82FFBC18)
//
// Identical RTTI-registration routine to the EffectBase family
// (CgsEffectBase.cpp), here templated on CgsSound::Logic::State. The X360 scans
// the per-class static array for the first NULL slot, capped at the class-array
// size (loop bound 0x10 == KU_SIZEOF_CLASS_ARRAY for State); on finding a NULL
// slot it stores the descriptor there. If no slot is free within the cap it falls
// through WITHOUT storing -- the "Too Many Class registations" assert
// (CgsState.h:363) only fires once the (16-bit) slot counter reaches 4*KU (0x40),
// which the 0x10-bounded loop never reaches. Reproduced generically by array NAME;
// no raw-offset cast.
// ---------------------------------------------------------------------------
ClassTypeInfo<State>* State::AddToClassTypeInfoArray(ClassTypeInfo<State>* apTypeInfo)
{
    // Scan for the first empty slot, capped at the class-array size (0x10).
    u32 lu32Index = 0;
    for (lu32Index = 0; lu32Index < KU_SIZEOF_CLASS_ARRAY; ++lu32Index)
    {
        if (gapStateTypeInfo[lu32Index] == 0)
        {
            gapStateTypeInfo[lu32Index] = apTypeInfo;
            return apTypeInfo;
        }
    }

    // No empty slot within the cap. The X360 only asserts once the (16-bit) counter
    // reaches 4*KU (0x40); the 0x10-bounded loop above stops at KU, so the counter is
    // < 0x40 here and the assert does not fire. Modelled with the same predicate.
    CGS_ASSERT(lu32Index < (4u * KU_SIZEOF_CLASS_ARRAY),
               "Too Many Class registations. Increase KU_SIZEOF_CLASS_ARRAY");
    return apTypeInfo;
}

ClassTypeInfo<State>* State::GetRegisteredTypeInfo(u32 auIndex)
{
    return auIndex < KU_SIZEOF_CLASS_ARRAY ? gapStateTypeInfo[auIndex] : 0;
}

ClassTypeInfo<State>* State::GetTypeInfo() const
{
    static ClassTypeInfo<State> sTypeInfo(0, "State", 0, 0);
    return &sTypeInfo;
}

const char* State::GetTypeName() const
{
    return "State";
}

// CgsState.h:370. Zero/seed the State base members. This mirrors the inline base
// init the derived ctors emit (X360 stores 0 across +4..+80, with mfCurTime /
// mfDeltaTime seeded to 0.0). MemBase's vtable is installed by its own ctor.
State::State()
    : miInstNum(0)
    , meMapState(0)
    , miStateInstType(0)
    , mpvAttachment(0)
    , mpPrevState(0)
    , mpNextState(0)
    , mpHeadEffectControl(0)
    , mpHeadEffectObject(0)
    , mpStateManager(0)
    , mpLogicModule(0)
    , miSFXFlags(0)
    , miNumLoadedEffectObjects(0)
    , miNumLoadedEffectControls(0)
    , mePrepareState(E_PREPARE_STATE_CREATE_OBJECTS)
    , mpCurrentEffect(0)
    , mbIsAttached(false)
    , mfCurTime(0.0f)
    , mfDeltaTime(0.0f)
{
    mauUpdateState[0] = 0u;
    mauUpdateState[1] = 0u;
}

// 0x826916D8. The X360 computes (mpvAttachment - apv == 0) as a boolean: true when
// the supplied pointer is this state's attachment.
bool State::IsAttachedToThis(void* apvAttachment)
{
    return mpvAttachment == apvAttachment;
}

void State::CreateSFXObjs()
{
    for (s32 liEffect = 0; liEffect < MAX_NUM_SFXOBJS_PER_STATE; ++liEffect)
    {
        if ((miSFXFlags & (1 << liEffect)) != 0)
            NewSFXObj(liEffect);
    }
}

void State::ForceCreateEffectControls(s32 liMask)
{
    for (s32 liEffect = 0; liEffect < MAX_NUM_SFXOBJS_PER_STATE; ++liEffect)
    {
        if ((liMask & (1 << liEffect)) != 0)
            NewSFXCtrl(liEffect);
    }
}

EffectBase* State::HasCtrlBeenAdded(s32 liEffectId)
{
    for (EffectBase* lpEffect = mpHeadEffectControl; lpEffect; lpEffect = lpEffect->mpNextEffectBase)
    {
        if (lpEffect->GetEffectID() == liEffectId)
            return lpEffect;
    }
    return 0;
}

void State::CreateSFXCtrls()
{
    for (EffectBase* lpEffect = mpHeadEffectObject; lpEffect; lpEffect = lpEffect->mpNextEffectBase)
    {
        for (s32 liController = 0; ; ++liController)
        {
            const s32 liEffectId = lpEffect->GetController(liController);
            if (liEffectId == -1)
                break;
            EffectBase* lpControl = HasCtrlBeenAdded(liEffectId);
            if (!lpControl)
                lpControl = NewSFXCtrl(liEffectId);
            lpEffect->AttachController(lpControl);
        }
    }

    for (EffectBase* lpEffect = mpHeadEffectControl; lpEffect; lpEffect = lpEffect->mpNextEffectBase)
    {
        for (s32 liController = 0; ; ++liController)
        {
            const s32 liEffectId = lpEffect->GetController(liController);
            if (liEffectId == -1)
                break;
            EffectBase* lpControl = HasCtrlBeenAdded(liEffectId);
            if (!lpControl)
                lpControl = NewSFXCtrl(liEffectId);
            lpEffect->AttachController(lpControl);
        }
    }
    SortSFXCtl();
}

void State::NewSFXObj(s32 liEffectId)
{
    EffectBase* lpEffect = mpStateManager->CreateEffectObject(miInstNum, liEffectId);
    if (!lpEffect)
        return;
    ++miNumLoadedEffectObjects;
    if (!mpHeadEffectObject)
    {
        mpHeadEffectObject = lpEffect;
        return;
    }
    EffectBase* lpTail = mpHeadEffectObject;
    while (lpTail->mpNextEffectBase)
        lpTail = lpTail->mpNextEffectBase;
    lpTail->mpNextEffectBase = lpEffect;
}

EffectBase* State::NewSFXCtrl(s32 liEffectId)
{
    if (liEffectId == -1)
        return 0;
    EffectBase* lpEffect = mpStateManager->CreateEffectControl(miInstNum, liEffectId);
    CGS_ASSERT(lpEffect != 0, "SND_ERROR: Could not find EffectControl");
    if (!lpEffect)
        return 0;
    ++miNumLoadedEffectControls;
    if (!mpHeadEffectControl)
        mpHeadEffectControl = lpEffect;
    else
    {
        EffectBase* lpTail = mpHeadEffectControl;
        while (lpTail->mpNextEffectBase)
            lpTail = lpTail->mpNextEffectBase;
        lpTail->mpNextEffectBase = lpEffect;
    }
    return lpEffect;
}

void State::SortSFXCtl()
{
    EffectBase* lapEffects[EffectBase::KI_MAX_SFX_CTLS] = { 0 };
    s32 liCount = 0;
    for (EffectBase* lpEffect = mpHeadEffectControl;
         lpEffect && liCount < EffectBase::KI_MAX_SFX_CTLS;
         lpEffect = lpEffect->mpNextEffectBase)
        lapEffects[liCount++] = lpEffect;

    mpHeadEffectControl = 0;
    EffectBase* lpTail = 0;
    for (;;)
    {
        s32 liBest = -1;
        s32 liBestId = EffectBase::KI_MAX_SFX_CTLS;
        for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
        {
            if (lapEffects[liIndex] && lapEffects[liIndex]->GetEffectID() < liBestId)
            {
                liBestId = lapEffects[liIndex]->GetEffectID();
                liBest = liIndex;
            }
        }
        if (liBest < 0)
            break;
        EffectBase* lpEffect = lapEffects[liBest];
        lapEffects[liBest] = 0;
        lpEffect->mpNextEffectBase = 0;
        if (!mpHeadEffectControl)
            mpHeadEffectControl = lpEffect;
        else
            lpTail->mpNextEffectBase = lpEffect;
        lpTail = lpEffect;
    }
}

bool State::Prepare(s32 liSfxFlags, StateManager* apStateManager)
{
    if (mePrepareState == E_PREPARE_STATE_CREATE_OBJECTS)
    {
        miSFXFlags = liSfxFlags;
        mpStateManager = apStateManager;
        mpLogicModule = apStateManager->GetLogicModule();
        mpvAttachment = 0;
        mbIsAttached = false;
        CreateSFXObjs();
        CreateSFXCtrls();
        mpCurrentEffect = mpHeadEffectObject;
        mePrepareState = E_PREPARE_STATE_OBJECTS;
    }

    if (mePrepareState == E_PREPARE_STATE_OBJECTS)
    {
        while (mpCurrentEffect && mpCurrentEffect->Prepare(this))
            mpCurrentEffect = mpCurrentEffect->mpNextEffectBase;
        if (mpCurrentEffect)
            return false;
        mpCurrentEffect = mpHeadEffectControl;
        mePrepareState = E_PREPARE_STATE_CONTROLS;
    }

    if (mePrepareState == E_PREPARE_STATE_CONTROLS)
    {
        while (mpCurrentEffect && mpCurrentEffect->Prepare(this))
            mpCurrentEffect = mpCurrentEffect->mpNextEffectBase;
        if (mpCurrentEffect)
            return false;
        mePrepareState = E_PREPARE_STATE_DONE;
    }
    return true;
}

void State::Attach(void* apvAttachment)
{
    CGS_ASSERT(mauUpdateState[0] == E_UPDATE_UNATTACHED ||
               mauUpdateState[0] == E_UPDATE_DETATCHING,
               "meUpdateState == E_UPDATE_UNATTACHED || meUpdateState == E_UPDATE_DETATCHING");
    const u32 luPreviousState = mauUpdateState[0];
    mpvAttachment = apvAttachment;
    mbIsAttached = true;
    mpCurrentEffect = 0;
    if (luPreviousState == E_UPDATE_UNATTACHED)
    {
        mauUpdateState[0] = E_INITIALIZE_CONTROLS;
        mauUpdateState[1] = luPreviousState;
    }
}

bool State::AttachEffect()
{
    CGS_ASSERT(mePrepareState == E_PREPARE_STATE_DONE, "E_PREPARE_STATE_DONE == mePrepareState");
    if (!mpCurrentEffect)
        return true;
    switch (mpCurrentEffect->GetAttachState())
    {
    case EffectBase::E_ATTACH_STATE_NONE:
        mpCurrentEffect->SetAttachState(EffectBase::E_ATTACH_STATE_WAITING_FOR_DATA);
        // X360 0x8268D6A8/0x8268D6AC: a fresh attachment invalidates the
        // previous data-ready latch before SetupLoadData is dispatched.
        mpCurrentEffect->ClearLoadedData();
        mpCurrentEffect->SetupLoadData();
        CGS_ASSERT(mpCurrentEffect->GetAttachState() ==
                       EffectBase::E_ATTACH_STATE_WAITING_FOR_DATA ||
                   mpCurrentEffect->GetAttachState() ==
                       EffectBase::E_ATTACH_STATE_PREPARING,
                   "mpCurrentEffect->GetAttachState() == EffectBase::E_ATTACH_STATE_WAITING_FOR_DATA ||mpCurrentEffect->GetAttachState() == EffectBase::E_ATTACH_STATE_PREPARING");
        if (mpCurrentEffect->GetAttachState() ==
            EffectBase::E_ATTACH_STATE_WAITING_FOR_DATA)
            return false;
        // SetupLoadData may synchronously advance a no-data effect to
        // PREPARING. ARTIST falls directly into the Attach call in that case.
        // fall through
    case EffectBase::E_ATTACH_STATE_WAITING_FOR_DATA:
        if (mpCurrentEffect->GetAttachState() ==
            EffectBase::E_ATTACH_STATE_WAITING_FOR_DATA)
            return false;
        // fall through
    case EffectBase::E_ATTACH_STATE_PREPARING:
        mpCurrentEffect->UpdateTime(mfCurTime, mfDeltaTime);
        if (!mpCurrentEffect->Attach())
            return false;
        break;
    case EffectBase::E_ATTACH_STATE_FINISHED:
        break;
    default:
        CGS_ASSERT(false, "Invalid State");
        break;
    }
    mpCurrentEffect->SetAttachState(EffectBase::E_ATTACH_STATE_FINISHED);
    mpCurrentEffect = mpCurrentEffect->mpNextEffectBase;
    return true;
}

void State::UpdateParams(f32 af32DeltaTime)
{
    mfDeltaTime = af32DeltaTime;
    switch (mauUpdateState[0])
    {
    case E_INITIALIZE_CONTROLS:
        mpCurrentEffect = mpHeadEffectControl;
        mauUpdateState[0] = E_INITIALIZE_CONTROLS_UPDATE;
        // fall through
    case E_INITIALIZE_CONTROLS_UPDATE:
        while (mpCurrentEffect && AttachEffect()) {}
        if (mpCurrentEffect)
            return;
        mpCurrentEffect = mpHeadEffectObject;
        mauUpdateState[0] = E_INITIALIZE_EFFECTS;
        // fall through
    case E_INITIALIZE_EFFECTS:
    case E_INITIALIZE_EFFECTS_UPDATE:
        mauUpdateState[0] = E_INITIALIZE_EFFECTS_UPDATE;
        while (mpCurrentEffect && AttachEffect()) {}
        if (mpCurrentEffect)
            return;
        mauUpdateState[0] = E_UPDATE_ATTACHED;
        // fall through
    case E_UPDATE_ATTACHED:
        for (EffectBase* lpEffect = mpHeadEffectControl; lpEffect; lpEffect = lpEffect->mpNextEffectBase)
        {
            lpEffect->UpdateTime(mfCurTime, af32DeltaTime);
            lpEffect->UpdateParams(af32DeltaTime);
        }
        for (EffectBase* lpEffect = mpHeadEffectObject; lpEffect; lpEffect = lpEffect->mpNextEffectBase)
        {
            lpEffect->UpdateTime(mfCurTime, af32DeltaTime);
            lpEffect->UpdateParams(af32DeltaTime);
        }
        break;
    case E_UPDATE_DETATCHING:
    {
        bool lbStillDetaching = false;
        for (EffectBase* lpEffect = mpHeadEffectObject; lpEffect;
             lpEffect = lpEffect->mpNextEffectBase)
        {
            lpEffect->UpdateTime(mfCurTime, af32DeltaTime);
            if (!lpEffect->Detach())
                lbStillDetaching = true;
        }
        for (EffectBase* lpEffect = mpHeadEffectControl; lpEffect;
             lpEffect = lpEffect->mpNextEffectBase)
        {
            lpEffect->UpdateTime(mfCurTime, af32DeltaTime);
            if (!lpEffect->Detach())
                lbStillDetaching = true;
        }
        if (!lbStillDetaching)
        {
            const u32 luPreviousState = mauUpdateState[0];
            if (mbIsAttached)
            {
                mauUpdateState[0] = E_INITIALIZE_CONTROLS;
                mauUpdateState[1] = luPreviousState;
                UpdateParams(0.0f);
            }
            else
            {
                mauUpdateState[0] = E_UPDATE_UNATTACHED;
                mauUpdateState[1] = luPreviousState;
            }
        }
        break;
    }
    default:
        break;
    }
}

void State::ProcessUpdate()
{
    if (mauUpdateState[0] != E_UPDATE_ATTACHED)
        return;
    for (EffectBase* lpEffect = mpHeadEffectObject; lpEffect; lpEffect = lpEffect->mpNextEffectBase)
    {
        if (lpEffect->GetAttachState() == EffectBase::E_ATTACH_STATE_FINISHED)
            lpEffect->ProcessUpdate();
    }
}

bool State::Detach()
{
    mauUpdateState[0] = E_UPDATE_DETATCHING;
    mbIsAttached = false;
    return true;
}

void State::DestroyEffects()
{
    while (mpHeadEffectControl)
    {
        EffectBase* lpNext = mpHeadEffectControl->mpNextEffectBase;
        mpHeadEffectControl->Destroy();
        mpHeadEffectControl = lpNext;
    }
    while (mpHeadEffectObject)
    {
        EffectBase* lpNext = mpHeadEffectObject->mpNextEffectBase;
        mpHeadEffectObject->Destroy();
        mpHeadEffectObject = lpNext;
    }
    miNumLoadedEffectControls = 0;
    miNumLoadedEffectObjects = 0;
}

// ---------------------------------------------------------------------------
// State::~State  @ 0x826ABCD8  (scalar deleting destructor)
//
//   stw  off_820AE1F4, 0(this)               ; install State's own vtable
//   bl   CgsSound::Logic::State::DestroyEffects  ; (this in r3) tear down effects
//   stw  off_820AA820, 0(this)               ; re-install the MemBase base vtable
//   if (flags & 1)                           ; deleting flavour
//       <sound allocator>.Free(this)         ; via off_82FFB954, vtable slot +0x14
//   return this
//
// The base-class analogue of the committed StreamingState (@0x826C9B28) /
// GlobalState (@0x826D2250) scalar deleting destructors. The single observable
// source-level side effect is the DestroyEffects() call on the State base (reused BY
// NAME). The two vtable installs and the conditional allocator-routed free
// (off_82FFB954, vtable slot +0x14) are MSVC's compiler-synthesised deleting-
// destructor thunk, re-emitted from this virtual destructor + the class's operator
// delete; the sound allocator is not homed in this group, so the host `delete`
// stands in for the custom dispatch.
//
// FLAG: State::DestroyEffects() is declaration-only in CgsState.h (its body is a
// separate un-homed sound-logic recon slice). It is called BY NAME here to match the
// X360 `bl` exactly; no body is fabricated for it.
// ---------------------------------------------------------------------------
State::~State()
{
    DestroyEffects();
}

// ---------------------------------------------------------------------------
// State::G  @ 0x8268D410
//
//   lis  r11, unk_82F2FA90@ha
//   addi r3,  r11, unk_82F2FA90@l   ; r3 = &unk_82F2FA90 (rodata sentinel)
//   blr
//
// Returns a pointer to the rodata sentinel unk_82F2FA90. Per the &unk_XXXX
// convention (HARD RULE 5), a raw IDA rodata sentinel reconstructs as the empty
// string literal. FLAG (confidence low): the meaning of G() is not recoverable from
// this one instruction pair; only the observable return (the sentinel) is modelled.
// ---------------------------------------------------------------------------
void* State::G()
{
    return (void*)"";
}

} // namespace Logic
} // namespace CgsSound
