#include "GameSource/Sound/Collision/BrnCollisionEffect.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Sound/Logic/CgsSoundLogicModule.h"
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"
#include "GameSource/Sound/Collision/BrnCollisionControl.h"
#include "GameSource/Sound/Collision/BrnCollisionState.h"
#include "GameSource/Sound/Collision/BrnCollisionStateManager.h"
#include "GameSource/AttribSys/Generated/classes/crashbin.h"
#include "GameSource/AttribSys/Generated/classes/propscrashbin.h"
#include "GameSource/Sound/Global/BrnGlobalStateManager.h"

#include <algorithm>
#include <cstdlib>

// =============================================================================
// BrnSound::Logic::Collision::CollisionEffect -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnCollisionEffect.h for the
// base-reuse rationale and the X360-32-bit-vs-host-64-bit offset note.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Collision
{

// ---------------------------------------------------------------------------
// CollisionEffect  @ 0x826AFD20  (field-init ctor)
//
//   stfs f0(=0.0), 0x20(this)            ; base mfRunningTime/mfDeltaTime region
//   stfs f0,       0x1C(this)            ; base field
//   stw  off_820AE954, 4(this)           ; (transient) IResourceRequester sub-vptr
//   sth  0, 0x10 ; stw 0, 0xC ; stw 0, 0x34 ; stw 0, 8 ; stb 0, 0x30 ; stw 0,0x28
//   stw  0, 0x24 ; sth 0, 0x12           ; base + leaf zero-init region
//   stw  off_820B1390, 0(this)           ; primary vptr (CollisionEffect vtable)
//   stw  off_820B135C, 4(this)           ; IResourceRequester sub-object vptr (final)
//   stw  0, 0x38 ; stw 0, 0x3C           ; mePrepareState / mbFirstUpdate+mbUseAzimuth
//   stw  &off_820B0E20, 0x44(this)       ; mCrashVoice vtable (Voice vptr @ +0x44)
//   stw  0, 0x48 ; stw 0, 0x4C           ; mCrashVoice handle/owner words
//   stfs f0(=0.0),  0x50(this)           ; mfIntensity = 0.0f
//   stw  5,         0x54(this)           ; meNicotineVolumeSlider = 5
//   stw  5,         0x58(this)           ; meNicotinePitchSlider  = 5
//   stfs f13(=1.0), 0x5C(this)           ; mSizeSettings.mfVolume = 1.0f
//   stfs f13,       0x60(this)           ; mSizeSettings.mfPitch  = 1.0f
//
// The leading vptr stores (+0/+4) and the base-region zero stores are the
// compiler-emitted base sub-object construction (BrnEffectObject's own ctor chain),
// produced implicitly here by the base default ctor. This body sets the LEAF
// members the X360 explicitly initialises. The +0x34 slot (mpCollisionDMixIo) is
// nulled by the X360 (stw 0,0x34) -- see header FLAG on the DWARF name divergence.
// ---------------------------------------------------------------------------
CollisionEffect::CollisionEffect()
    : mpCollisionControl(nullptr)         // stw 0, 0x34
    , mePrepareState(E_PREPARE_STATE_CONSTRUCT_VOICE) // +0x38 zero-region
    , mbFirstUpdate(false)                // +0x3C zero-region
    , mbUseAzimuth(false)                 // +0x3C zero-region
    , mfIntensity(0.0f)                   // stfs 0.0, 0x50
    , meNicotineVolumeSlider(5)           // stw 5, 0x54
    , meNicotinePitchSlider(5)            // stw 5, 0x58
    // mCrashVoice: default-constructed (Voice vptr @ +0x44 + nulled handle/owner)
    // mSizeSettings: default-constructed (mfVolume/mfPitch = 1.0f -> +0x5C/+0x60)
{
}

// ---------------------------------------------------------------------------
// ~CollisionEffect  @ 0x826C90D8  (the X360 `vector deleting destructor')
//
//   bl   CollisionEffect::~CollisionEffect          ; chain to the real dtor
//   if (a2 & 1) {                                   ; the `delete' half
//       memset(&local[1], 0, 16); local[0] = this;
//       (*(*off_82FFB954 + 0x14))(off_82FFB954, local) ; free via MemBase allocator
//   }
//   return this
//
// The leaf destructor adds no teardown of its own: the X360 inner ~CollisionEffect
// is the inherited BrnEffectObject dual-base settle (both vptr stores + the
// attach/detach/resources-ready member clears), identical store-for-store to the
// committed BrnEffectObject dtor @ 0x826AF4C8 and the sibling SingleGinsuEffect
// dtor. In reconstructed C++ the dual-base settle and the deleting-destructor thunk
// are compiler-synthesised from the virtual dtor declared in the header, so the
// hand-written body is empty.
// FLAG: the (a2 & 1) tail frees the object through the global sound MemBase
// allocator (off_82FFB954); that allocator's vtable call is not homed here, so the
// `delete' half of the vector deleting destructor is left to the host toolchain
// rather than reproducing the raw allocator vtable dispatch.
// ---------------------------------------------------------------------------
CollisionEffect::~CollisionEffect()
{
}

CgsSound::Logic::EffectObject* CollisionEffect::CreateObject(u32 /*auAllocator*/)
{
    return new CollisionEffect();
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>*
CollisionEffect::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject> sTypeInfo(
        0x50000, "CollisionEffect",
        CgsSound::Logic::EffectObject::GetStaticTypeInfo(),
        &CollisionEffect::CreateObject);
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>*
CollisionEffect::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

const char* CollisionEffect::GetTypeName() const
{
    return "CollisionEffect";
}

s32 CollisionEffect::GetController(s32 aiIndex)
{
    return aiIndex == 0 ? 0 : -1;
}

void CollisionEffect::AttachController(CgsSound::Logic::EffectBase* apController)
{
    CGS_ASSERT(apController != nullptr, "lpController");
    if (!apController)
        return;
    if (apController->GetEffectID() == 0)
        mpCollisionControl = static_cast<CollisionControl*>(apController);
    else
        CGS_ASSERT(false, "Cound't attach controller");
}

bool CollisionEffect::Prepare(CgsSound::Logic::State* apState)
{
    switch (mePrepareState)
    {
    case E_PREPARE_STATE_CONSTRUCT_VOICE:
        if (!CgsSound::Logic::EffectBase::Prepare(apState))
            return false;

        mCrashVoice.Construct(
            GetLogicModule(), GetLogicModule()->GetUniqueId(),
            static_cast<u32>(CgsSound::Playback::Name::MakeHash(
                "~SplicerFactory::SK_NAME~")),
            static_cast<u32>(CgsSound::Playback::Name::MakeHash(
                "SplicerVoiceSpec")));
        mePrepareState = E_PREPARE_STATE_CONNECT_VOICE;
        // ARTIST falls through: a voice created synchronously can be connected on
        // this pass; an asynchronously created voice returns false until ready.
    case E_PREPARE_STATE_CONNECT_VOICE:
    {
        if (!mCrashVoice.IsReady())
            return false;

        BrnSound::Logic::GlobalStateManager* lpGlobalStateManager =
            static_cast<BrnSound::Logic::GlobalStateManager*>(
                GetLogicModule()->GetEnvironment().GetStateManager(0));
        CGS_ASSERT(lpGlobalStateManager != nullptr, "lpGlobalStateManager");
        if (!lpGlobalStateManager)
            return false;

        const s32 liCollisionSubmixIdent =
            lpGlobalStateManager->GetSubmixVoice(
                BrnSound::Logic::GlobalStateManager::E_SUBMIX_VOICE_COLLISION)
                .GetIdent();
        mCrashVoice.Connect(
            static_cast<u32>(CgsSound::Playback::Name::MakeHash("Send01")),
            static_cast<u32>(liCollisionSubmixIdent));
        return true;
    }
    default:
        return true;
    }
}

bool CollisionEffect::Attach()
{
    mbUseAzimuth = true;
    CgsSound::Logic::EffectBase::Attach();
    CGS_ASSERT(mpCollisionControl != nullptr, "mpCollisionControl");
    if (!mpCollisionControl)
        return false;
    if (mpCollisionControl->GetCollisionFinished())
        return true;

    CollisionState* lpState = mpCollisionControl->GetCollisionState();
    CGS_ASSERT(lpState != nullptr, "lpCollisionState");
    CGS_ASSERT(mCrashVoice.GetVoiceObject() != nullptr, "mCrashVoice.IsCreated()");
    if (!lpState || !mCrashVoice.GetVoiceObject())
        return false;

    const OutputCollision& lrCollision = lpState->GetOutputCollision();
    CollisionStateManager* lpManager =
        static_cast<CollisionStateManager*>(lpState->GetStateManager());
    CGS_ASSERT(lpManager != nullptr, "lpCollisionStateManager");
    if (!lpManager)
        return false;

    meNicotinePitchSlider = 1;
    if (lrCollision.mePipeline == InputCollision::E_REGULAR)
    {
        const Attrib::Gen::crashbin lBin(lrCollision.mBinKey, nullptr);
        CGS_ASSERT(lBin.IsValid(), "lCrashBin.IsValid()");
        meNicotineVolumeSlider = lBin.MixerSlider();
        CGS_ASSERT(meNicotineVolumeSlider > 2,
                   "meNicotineVolumeSlider > AttribSys::Enums::eCollisionMixerSliders::Pitch");
        switch (lrCollision.meSize)
        {
        case E_SIZE_LARGE:
            mSizeSettings.mfVolume = lBin.Volumes().z;
            mSizeSettings.mfPitch = lBin.Pitch().z;
            break;
        case E_SIZE_MEDIUM:
            mSizeSettings.mfVolume = lBin.Volumes().y;
            mSizeSettings.mfPitch = lBin.Pitch().y;
            break;
        case E_SIZE_SMALL:
            mSizeSettings.mfVolume = lBin.Volumes().x;
            mSizeSettings.mfPitch = lBin.Pitch().x;
            break;
        }
    }
    else
    {
        CGS_ASSERT(lrCollision.mePipeline == InputCollision::E_PROP,
                   "lCollision.mePipeline == InputCollision::E_PROP");
        const Attrib::Gen::propscrashbin lBin(lrCollision.mBinKey, nullptr);
        CGS_ASSERT(lBin.IsValid(), "lCrashBin.IsValid()");
        meNicotineVolumeSlider = lBin.MixerSlider();
        CGS_ASSERT(meNicotineVolumeSlider > 2,
                   "meNicotineVolumeSlider > AttribSys::Enums::eCollisionMixerSliders::Pitch");
        switch (lrCollision.meSize)
        {
        case E_SIZE_LARGE:
            mSizeSettings.mfVolume = lBin.Volumes().z;
            mSizeSettings.mfPitch = lBin.Pitch().z;
            break;
        case E_SIZE_MEDIUM:
            mSizeSettings.mfVolume = lBin.Volumes().y;
            mSizeSettings.mfPitch = lBin.Pitch().y;
            break;
        case E_SIZE_SMALL:
            mSizeSettings.mfVolume = lBin.Volumes().x;
            mSizeSettings.mfPitch = lBin.Pitch().x;
            break;
        }
    }

    mCrashVoice.Attach(
        static_cast<s32>(CgsSound::Playback::Name::MakeHash(
            "~SplicerPlayerVoice::Slot~")),
        lpManager->GetSplicerBank(
            static_cast<ECollisionSpliceBankType>(lrCollision.meBankType)));
    mCrashVoice.Play(lrCollision.miSampleID);

    if (std::getenv("BRN_COLLISION_AUDIO_DIAG") != nullptr &&
        CgsDev::Log::gpDebugPrint)
    {
        static u32 suPrintCount = 0;
        if (suPrintCount++ < 32u)
        {
            *CgsDev::Log::gpDebugPrint
                << "[collision-audio] voice attached sample="
                << lrCollision.miSampleID
                << " mixer=" << static_cast<s32>(meNicotineVolumeSlider)
                << " volume=" << mSizeSettings.mfVolume
                << " pitch=" << mSizeSettings.mfPitch << "\n";
        }
    }

    mfIntensity = std::max(0.0f, lrCollision.mNormalizedImpulse.x);
    SetMixerInputValue(0, 0x7FFF);
    const f32 lfDucking = std::min(32767.0f, mfIntensity * 327.67001f);
    SetMixerInputValue(1, static_cast<s32>(32767.0f - lfDucking));
    if (lrCollision.meAction == AttribSys::Enums::eAction::Detach)
        mbUseAzimuth = false;
    mbFirstUpdate = true;
    return true;
}

void CollisionEffect::UpdateParams(f32 /*afDeltaTime*/)
{
}

void CollisionEffect::ProcessUpdate()
{
    if (GetDMixIOPtr())
        GetDMixIOPtr()->SetDMixInput(0, 0);
    if (!mpCollisionControl || mpCollisionControl->GetCollisionFinished())
        return;

    const f32 lfGain = GetGain();
    const f32 lfPitch = GetPitch();
    f32 lfAzimuth = 0.0f;
    if (mbUseAzimuth && GetDMixIOPtr())
    {
        lfAzimuth = static_cast<f32>(
            GetDMixIOPtr()->GetDMixOutput(0, 3)) *
            0.0054932479f;
    }
    mbFirstUpdate = false;

    if (!mCrashVoice.IsPlaying())
    {
        mpCollisionControl->SetCollisionFinished(true);
        return;
    }

    const u32 luSend01 = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("Send01"));
    const u32 luPitch = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("~SplicerPlayerVoice::Pitch~"));
    const u32 luAzimuth = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("~SplicerPlayerVoice::Azimuth~"));
    mCrashVoice.SetGain(0, lfGain, 0, &luSend01);
    mCrashVoice.SetParameter(1, lfAzimuth, 0, &luAzimuth);
    mCrashVoice.SetParameter(0, lfPitch, 0, &luPitch);
}

bool CollisionEffect::Detach()
{
    if (mCrashVoice.GetVoiceObject())
    {
        mCrashVoice.Stop();
        mCrashVoice.Detach(static_cast<s32>(
            CgsSound::Playback::Name::MakeHash("~SplicerPlayerVoice::Slot~")));
    }
    if (GetDMixIOPtr())
        GetDMixIOPtr()->SetDMixInput(1, 0);
    return BrnSound::Logic::BrnEffectObject::Detach();
}

// ---------------------------------------------------------------------------
// GetGain  @ 0x82688138
//
//   lwz  r3, 0x34(this)       ; r3 = mpCollisionDMixIo
//   lwz  r4, 0x54(this)       ; r4 = meNicotineVolumeSlider
//   cmplwi r3, 0
//   beq  zero                 ; no dmix handle -> gain contribution 0
//     li   r5, 0              ; preset = 0  (DMX_VOL)
//     bl   Nicotine::DMixIO::GetDMixOutput(r3, r4, 0)
//     extsw r11, r3           ; SIGN-extend the s32 result
//     fcfid / frsp            ; (s64)->f64->f32
//     fmuls f0, f12, flt_820AA8F8   ; * 0.000030518509  (== 1/32768, the Q15 scale)
//     b    tail
//   zero:
//     lfs  f0, flt_82001CC0   ; f0 = 0.0
//   tail:
//   lfs  f13, 0x5C(this)      ; f13 = mSizeSettings.mfVolume
//   fmuls f1, f13, f0         ; return mfVolume * scaledMixerOutput
//   blr
//
// Reads the latched dynamic-mixer output for the volume slider, converts the signed
// Q15 mixer value to a normalised f32, and scales it by the size-specific volume.
// When no dmix handle is connected the mixer contribution is 0.0f (so the gain is 0).
// FLAG: DWARF declares GetGain() const; the +0x34 slot is the latched
// Nicotine::DMixIO (see header FLAG on the DWARF mpCollisionControl name).
// ---------------------------------------------------------------------------
f32 CollisionEffect::GetGain() const
{
    // 0.000030518509f == flt_820AA8F8 (the Q15 -> [0,1) scale, 1/32768).
    const f32 KF_Q15_TO_NORMALISED = 0.000030518509f;

    f32 lfMixerOutput;
    Nicotine::DMixIO* lpDmix =
        mpCollisionControl ? mpCollisionControl->GetDMixIOPtr() : nullptr;
    if (lpDmix != nullptr) // lwz r3,0x34; cmplwi; beq
    {
        // GetDMixOutput(slot, preset): preset 0 == DMX_VOL. extsw -> signed s32.
        const s32 liOutput =
            lpDmix->GetDMixOutput(meNicotineVolumeSlider, Nicotine::DMixIO::DMX_VOL);
        lfMixerOutput = static_cast<f32>(liOutput) * KF_Q15_TO_NORMALISED;
    }
    else
    {
        lfMixerOutput = 0.0f; // lfs f0, flt_82001CC0
    }

    return mSizeSettings.mfVolume * lfMixerOutput; // fmuls f1, f13, f0
}

f32 CollisionEffect::GetPitch() const
{
    Nicotine::DMixIO* lpDmix =
        mpCollisionControl ? mpCollisionControl->GetDMixIOPtr() : nullptr;
    f32 lfMixerOutput = 0.0f;
    if (lpDmix)
    {
        lfMixerOutput = static_cast<f32>(lpDmix->GetDMixOutput(
            meNicotinePitchSlider, Nicotine::DMixIO::DMX_PITCH)) *
            0.00024414062f;
    }
    return mSizeSettings.mfPitch * lfMixerOutput;
}

static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* const
    gpCollisionEffectReg =
        CgsSound::Logic::EffectObject::AddToClassTypeInfoArray(
            CollisionEffect::GetStaticTypeInfo());

// =============================================================================
// BrnSound::Logic::Collision::Collision3DControl -- out-of-line bodies.
// (Added to the existing CollisionEffect TU home; both classes share the DWARF
//  source BrnCollisionEffect.h. See the header for the inheritance rationale.)
//
// This slice's recon'd function set is three entries:
//   Collision3DControl()           @ 0x826E88D0
//   CreateObject(u32)              @ 0x826F80E8
//   `scalar deleting destructor'   @ 0x826F8168
// =============================================================================

// ---------------------------------------------------------------------------
// Collision3DControl::Collision3DControl  @ 0x826E88D0
//
//   bl   Brn3DEffectControl::Brn3DEffectControl   ; GRANDPARENT ctor (mEngineDataAtrib etc.)
//   li   r10, 0 ; stw r10, 0xD0(r31)              ; mpTransform            = nullptr
//   stw  &off_820B6240, 0(r31)                    ; primary vptr (implicit)
//   stvx128 v0, r31, 0xE0                          ; mPositionInUserSpace   = {0,0,0}
//   stvx128 v0, r31, 0xF0                          ; mDirectionInUserSpace  = {0,0,0}
//   stvx128 v0, r31, 0x100                         ; mGeneratedPosition     = {0,0,0}
//   stvx128 v0, r31, 0x110                         ; mGeneratedDirection    = {0,0,0}
//   return r31
//
// The X360 body INLINES the Brn3DUserSpaceEffectControl construction: it calls the
// grandparent Brn3DEffectControl() directly, then nulls mpTransform (+0xD0) and
// zero-inits the four 16-byte Vector3s (+0xE0/+0xF0/+0x100/+0x110). Delegating to
// Brn3DUserSpaceEffectControl() (which chains Brn3DEffectControl() + nulls mpTransform
// + SetZero()s the four Vector3s) reproduces that sequence BY NAME; Collision3DControl
// adds NO members of its own. The +0(vptr) store is the compiler-emitted leaf
// final-overrider vptr write (the RTTI table off_820B6240 is DEFERRED to its own TU).
// ---------------------------------------------------------------------------
Collision3DControl::Collision3DControl()
    : Brn3DUserSpaceEffectControl() // chains Brn3DEffectControl() + nulls mpTransform + SetZero()s the 4 Vector3s
{
}

// ---------------------------------------------------------------------------
// Collision3DControl::CreateObject(u32)  @ 0x826F80E8   (the factory hook)
//
//   if ( a1 ) { if ( MemBase::operator new(288, "Collision3DControl", 1) ) return new'd ctor; }
//   else      { if ( MemBase::operator new(288, "Collision3DControl", 0) ) return new'd ctor; }
//   return 0;
//
// The X360 allocates a 288-byte (0x120) block through CgsSound::MemBase::operator
// new(size, tag, flavour) tagged "Collision3DControl" and placement-constructs a
// Collision3DControl into it. Both arms call the SAME size+ctor; the `a1` argument
// only selects the operator-new flavour (0/1). DWARF (BrnCollisionEffect.h:199)
// attests `EffectControl* CreateObject(uint32_t)` (non-virtual, non-const).
//
// FLAG (allocator gate): CgsSound::MemBase does NOT model operator new(size, tag,
// flavour) (off_82FFB954 not homed in this group), so a faithful placement-new is not
// yet expressible. This uses the host `new`; the observable result matches. Mirrors
// the committed CollisionStateManager::CreateObject @ 0x82701FA8. The 0x120 size is
// documentation only and is NOT passed to the host new.
// ---------------------------------------------------------------------------
CgsSound::Logic::EffectControl* Collision3DControl::CreateObject( u32 /*luType*/ )
{
    return new Collision3DControl();
}

// ---------------------------------------------------------------------------
// ~Collision3DControl  @ 0x826F8168  (the X360 `scalar deleting destructor')
//
//   stw  &off_820B3670, 0(this)                    ; leaf vptr @ entry
//   bl   Attrib::Instance::~Instance(this + 0xB0)  ; destroy mEngineDataAtrib (base-owned)
//   li   r9, 3 ; stw r9, 0x24(this)                ; meDetachState = E_DETACH_STATE_FINISHED
//   stw  &off_820AA820, 0(this)                    ; final base vptr settle
//   stb  0, 0x2D(this)                             ; (un-homed byte flag) = 0
//   stw  0, 0x20(this)                             ; meAttachState = E_ATTACH_STATE_NONE
//   if (a2 & 1) { deallocate via off_82FFB954 (the global sound MemBase allocator) }
//   return this
//
// Every store above is produced by the compiler-generated virtual-destructor chain:
// the leaf vptr install, the intermediate Brn3DUserSpaceEffectControl teardown
// (trivial Vector3 members), and Brn3DEffectControl's destructor which tears down
// mEngineDataAtrib (@ +0xB0 via the committed Attrib::Instance::~Instance) and settles
// meDetachState/meAttachState + the final vptr. So this leaf body adds nothing of its
// own and is empty -- identical treatment to the sibling Passby3DControl dtor
// (BrnPassbyEffect.cpp @ 0x826E8ED0).
//
// FLAG: the (a2 & 1) tail invokes the global sound allocator (off_82FFB954) to free
// the object; that allocator is not homed here, so the `delete` half is left to the
// host toolchain (same treatment as the committed Brn3DEffectControl / Passby3DControl
// homes).
// ---------------------------------------------------------------------------
Collision3DControl::~Collision3DControl()
{
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>*
Collision3DControl::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl> sTypeInfo(
        0x50010, "Collision3DControl",
        CgsSound::Logic::EffectControl::GetStaticTypeInfo(),
        &Collision3DControl::CreateObject);
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>*
Collision3DControl::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

const char* Collision3DControl::GetTypeName() const
{
    return "Collision3DControl";
}

static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* const
    gpCollision3DControlReg =
        CgsSound::Logic::EffectControl::AddToClassTypeInfoArray(
            Collision3DControl::GetStaticTypeInfo());

} // namespace Collision
} // namespace Logic
} // namespace BrnSound
