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
// Cited by X360 address only -- no reference-source provenance.
// ============================================================================

#include "GameShared/GameClasses/Sound/Logic/CgsState.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Sound/Logic/CgsEffectBase.h"
#include "GameShared/GameClasses/Sound/Logic/CgsStateManager.h"

// ARTIST 0x820ADD50: the assert file (__FILE__) of this unit, a literal.
#define KAC_ARTIST_ASSERT_FILE "..\\..\\..\\GameShared\\GameClasses\\Sound/Logic/CgsState.cpp"

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

// ARTIST 0x82F2FA90.
ClassTypeInfo<State> State::sTypeInfo(-1, "State", 0, 0);

ClassTypeInfo<State>* State::GetStaticTypeInfo()
{
    return &sTypeInfo;
}

ClassTypeInfo<State>* State::GetTypeInfo() const
{
    return &sTypeInfo; // ARTIST 0x8268D410
}

const char* State::GetTypeName() const
{
    return sTypeInfo.typeName; // ARTIST 0x8268D420
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
    , meUpdateState(E_UPDATE_UNATTACHED)
    , mePrepareState(E_PREPARE_STATE_CREATE_OBJECTS)
    , mpCurrentEffect(0)
    , mbIsAttached(false)
    , mfCurTime(0.0f)
    , mfDeltaTime(0.0f)
{
    // ARTIST 0x826C47F8 and 0x826C4C50..0x826C4C54 access these two words.
    CGS_STATIC_ASSERT(sizeof(meUpdateState) == 8, "State update history is two words");
    CGS_STATIC_ASSERT(offsetof(State, meUpdateState) == 0x38, "ARTIST update state offset");
    CGS_STATIC_ASSERT(offsetof(State, mePrepareState) == 0x40, "ARTIST prepare state offset");
    CGS_STATIC_ASSERT(sizeof(State) == 0x54, "ARTIST sound state size");
}

// 0x826916D8. The X360 computes (mpvAttachment - apv == 0) as a boolean: true when
// the supplied pointer is this state's attachment.
bool State::IsAttachedToThis(void* apvAttachment)
{
    return apvAttachment == mpvAttachment;
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

// ARTIST 0x826EAB50: a switch over the prepare stage whose stages fall through. The logic
// module is not latched here: StateManager stamps State::mpLogicModule when it hands the
// state out (CgsStateManager.cpp), and ARTIST stores nothing at +0x28 in this body.
bool State::Prepare(s32 liSfxFlags, StateManager* apStateManager)
{
    switch (mePrepareState)
    {
    case E_PREPARE_STATE_CREATE_OBJECTS:
        miSFXFlags = liSfxFlags;
        mpStateManager = apStateManager;
        mpvAttachment = 0;
        mbIsAttached = false;
        CreateSFXObjs();
        CreateSFXCtrls();
        mpCurrentEffect = mpHeadEffectObject;
        mePrepareState = E_PREPARE_STATE_OBJECTS;
        // fall through
    case E_PREPARE_STATE_OBJECTS:
        while (mpCurrentEffect)
        {
            if (!mpCurrentEffect->Prepare(this))
                return false;
            mpCurrentEffect = mpCurrentEffect->mpNextEffectBase;
        }
        mpCurrentEffect = mpHeadEffectControl;
        mePrepareState = E_PREPARE_STATE_CONTROLS;
        // fall through
    case E_PREPARE_STATE_CONTROLS:
        while (mpCurrentEffect)
        {
            if (!mpCurrentEffect->Prepare(this))
                return false;
            mpCurrentEffect = mpCurrentEffect->mpNextEffectBase;
        }
        mePrepareState = E_PREPARE_STATE_DONE;
        break;
    default:
        break;
    }
    return true;
}

void State::Attach(void* apvAttachment)
{
    CGS_ASSERT(meUpdateState.GetCurrent() == E_UPDATE_UNATTACHED ||
               meUpdateState.GetCurrent() == E_UPDATE_DETATCHING,
        CGS_ASSERT_MESSAGE_AT(
            "meUpdateState == E_UPDATE_UNATTACHED || meUpdateState == E_UPDATE_DETATCHING",
            KAC_ARTIST_ASSERT_FILE, 798 /* ARTIST 0x826C4BC0 */));
    mpvAttachment = apvAttachment;
    mbIsAttached = true;
    mpCurrentEffect = 0;
    if (meUpdateState.GetCurrent() == E_UPDATE_UNATTACHED)
        meUpdateState.Update(E_INITIALIZE_CONTROLS);
}

bool State::AttachEffect()
{
    CGS_ASSERT(mePrepareState == E_PREPARE_STATE_DONE,
        CGS_ASSERT_MESSAGE_AT("E_PREPARE_STATE_DONE == mePrepareState",
            KAC_ARTIST_ASSERT_FILE, 833 /* ARTIST 0x8268D654 */));
    // No null test of mpCurrentEffect: ARTIST (0x8268D668) and the PS3 build read its
    // attach state straight away. WAITING_FOR_DATA (jump-table slot 1 -> 0x8268D708) returns
    // false; NONE falls into PREPARING once SetupLoadData has run; an invalid state asserts
    // and returns true without advancing (0x8268D75C -> 0x8268D778).
    switch (mpCurrentEffect->GetAttachState())
    {
    case EffectBase::E_ATTACH_STATE_NONE:
        mpCurrentEffect->OnAttach();
        mpCurrentEffect->SetupLoadData();
        CGS_ASSERT(mpCurrentEffect->GetAttachState() ==
                       EffectBase::E_ATTACH_STATE_WAITING_FOR_DATA ||
                   mpCurrentEffect->GetAttachState() ==
                       EffectBase::E_ATTACH_STATE_PREPARING,
            CGS_ASSERT_MESSAGE_AT(
                "mpCurrentEffect->GetAttachState() == EffectBase::E_ATTACH_STATE_WAITING_FOR_DATA ||mpCurrentEffect->GetAttachState() == EffectBase::E_ATTACH_STATE_PREPARING",
                KAC_ARTIST_ASSERT_FILE, 841 /* ARTIST 0x8268D6E4 */));
        if (mpCurrentEffect->GetAttachState() ==
            EffectBase::E_ATTACH_STATE_WAITING_FOR_DATA)
            return false;
        // SetupLoadData may synchronously advance a no-data effect to PREPARING.
        // fall through
    case EffectBase::E_ATTACH_STATE_PREPARING:
        mpCurrentEffect->UpdateTime(mfCurTime, mfDeltaTime);
        if (!mpCurrentEffect->Attach())
            return false;
        // fall through
    case EffectBase::E_ATTACH_STATE_FINISHED:
        mpCurrentEffect->SetAttachState(EffectBase::E_ATTACH_STATE_FINISHED);
        mpCurrentEffect = mpCurrentEffect->mpNextEffectBase;
        break;
    case EffectBase::E_ATTACH_STATE_WAITING_FOR_DATA:
        return false;
    default:
        CGS_ASSERT(false, CGS_ASSERT_MESSAGE_AT("Invalid State",
            KAC_ARTIST_ASSERT_FILE, 865 /* ARTIST 0x8268D764 */));
        break;
    }
    return true;
}

// ARTIST 0x826C4850. One jump-table slot per update state (1..6): each initialise stage
// records its own state and falls into the next; an attach that is still waiting returns.
// Once the effects are attached their mixers are switched on (objects, then controls), and
// switched off again while detaching. Locals follow DWARF (CgsState.cpp:576-637).
void State::UpdateParams(f32 lfTimeStep)
{
    mfDeltaTime = lfTimeStep;
    switch (meUpdateState.GetCurrent())
    {
    case E_INITIALIZE_CONTROLS:
        meUpdateState.Update(E_INITIALIZE_CONTROLS);
        mpCurrentEffect = mpHeadEffectControl;
        // fall through
    case E_INITIALIZE_CONTROLS_UPDATE:
        meUpdateState.Update(E_INITIALIZE_CONTROLS_UPDATE);
        while (mpCurrentEffect)
        {
            if (!AttachEffect())
                return;
        }
        // fall through
    case E_INITIALIZE_EFFECTS:
        meUpdateState.Update(E_INITIALIZE_EFFECTS);
        mpCurrentEffect = mpHeadEffectObject;
        // fall through
    case E_INITIALIZE_EFFECTS_UPDATE:
    {
        meUpdateState.Update(E_INITIALIZE_EFFECTS_UPDATE);
        while (mpCurrentEffect)
        {
            if (!AttachEffect())
                return;
        }
        for (EffectBase* CurSFXOBj = mpHeadEffectObject; CurSFXOBj;
             CurSFXOBj = CurSFXOBj->mpNextEffectBase)
        {
            CurSFXOBj->TurnOnMixer();
        }
        for (EffectBase* CurSfxCtl = mpHeadEffectControl; CurSfxCtl;
             CurSfxCtl = CurSfxCtl->mpNextEffectBase)
        {
            CurSfxCtl->TurnOnMixer();
        }
    }
        // fall through
    case E_UPDATE_ATTACHED:
        meUpdateState.Update(E_UPDATE_ATTACHED);
        for (EffectBase* CurSFXCtl = mpHeadEffectControl; CurSFXCtl; CurSFXCtl = CurSFXCtl->mpNextEffectBase)
        {
            CurSFXCtl->UpdateTime(mfCurTime, lfTimeStep);
            CurSFXCtl->UpdateParams(lfTimeStep);
        }
        for (EffectBase* CurSFXOBj = mpHeadEffectObject; CurSFXOBj; CurSFXOBj = CurSFXOBj->mpNextEffectBase)
        {
            CurSFXOBj->UpdateTime(mfCurTime, lfTimeStep);
            CurSFXOBj->UpdateParams(lfTimeStep);
        }
        break;
    case E_UPDATE_DETATCHING:
    {
        meUpdateState.Update(E_UPDATE_DETATCHING);
        bool lbStillDetaching = false;
        for (EffectBase* CurSFXOBj = mpHeadEffectObject; CurSFXOBj;
             CurSFXOBj = CurSFXOBj->mpNextEffectBase)
        {
            CurSFXOBj->TurnOffMixer();
            CurSFXOBj->UpdateTime(mfCurTime, lfTimeStep);
            if (!CurSFXOBj->Detach())
                lbStillDetaching = true;
        }
        for (EffectBase* CurSFXCtl = mpHeadEffectControl; CurSFXCtl;
             CurSFXCtl = CurSFXCtl->mpNextEffectBase)
        {
            CurSFXCtl->TurnOffMixer();
            CurSFXCtl->UpdateTime(mfCurTime, lfTimeStep);
            if (!CurSFXCtl->Detach())
                lbStillDetaching = true;
        }
        if (!lbStillDetaching)
        {
            if (mbIsAttached)
            {
                meUpdateState.Update(E_INITIALIZE_CONTROLS);
                UpdateParams(0.0f);
            }
            else
            {
                meUpdateState.Update(E_UPDATE_UNATTACHED);
            }
        }
        break;
    }
    default:
        meUpdateState.Update(E_UPDATE_UNATTACHED);
        break;
    }
}

void State::ProcessUpdate()
{
    if (meUpdateState.GetCurrent() != E_UPDATE_ATTACHED)
        return;
    for (EffectBase* lpEffect = mpHeadEffectObject; lpEffect; lpEffect = lpEffect->mpNextEffectBase)
    {
        if (lpEffect->GetAttachState() == EffectBase::E_ATTACH_STATE_FINISHED)
            lpEffect->ProcessUpdate();
    }
}

// ---------------------------------------------------------------------------
// State::Detach  @ 0x826C4C20  (vtable +0x18 of State off_820AE1F4 and of every state that does not
// override it -- PassbyState's DWARF cpp:118 override is identical-code-folded into this address)
//
//   lwz r10, 0x38 ; cmpwi 5 ; beq ok ; li r3, 0 ; blr      ; not E_UPDATE_ATTACHED -> false, no store
//   ok: stw 0, 0x10                                        ; mpvAttachment = 0
//       stb 0, 0x48                                        ; mbIsAttached = false
//       lwz r10, 0x38 ; stw 6, 0x38 ; stw r10, 0x3C        ; meUpdateState.Update(E_UPDATE_DETATCHING)
//       li r3, 1
// VehicleState::Detach @0x826C9FF8 inlines the same body word for word before its Clear(), and
// StreamingState::Detach @0x826C9C58 its tail. The PC detached from ANY update state (an
// initialising state too), kept the stale attachment for IsAttachedToThis / GetStateObj, and never
// wrote the history word. The manager-side callers test the result: CollisionStateManager::
// GetFreeState's lowest-priority steal and EmitterStateManager's farthest-emitter steal take a state
// only when this returns true.
// ---------------------------------------------------------------------------
bool State::Detach()
{
    if (meUpdateState.GetCurrent() != E_UPDATE_ATTACHED)
        return false;
    mpvAttachment = 0;
    mbIsAttached = false;
    meUpdateState.Update(E_UPDATE_DETATCHING);
    return true;
}

void State::DestroyEffects()
{
    for (EffectBase* lpEffect = mpHeadEffectControl; lpEffect;)
    {
        EffectBase* lpNext = lpEffect->mpNextEffectBase;
        delete lpEffect;
        lpEffect = lpNext;
    }
    for (EffectBase* lpEffect = mpHeadEffectObject; lpEffect;)
    {
        EffectBase* lpNext = lpEffect->mpNextEffectBase;
        delete lpEffect;
        lpEffect = lpNext;
    }
    mpHeadEffectControl = 0;
    mpHeadEffectObject = 0;
    miNumLoadedEffectObjects = 0;
    miNumLoadedEffectControls = 0;
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

} // namespace Logic
} // namespace CgsSound
