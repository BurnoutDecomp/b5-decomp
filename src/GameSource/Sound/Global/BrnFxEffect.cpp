#include "GameSource/Sound/Global/BrnFxEffect.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"              // CGS_ASSERT
#include "GameShared/GameClasses/Sound/IO/CgsMessage.h"         // CgsSound::Io::Message / MessageHeader
#include "GameShared/GameClasses/Sound/Logic/CgsState.h"        // State -> StateManager walk
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"    // Name::MakeHash
#include "GameShared/GameClasses/Sound/Playback/CgsVoice.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameSource/Sound/Global/BrnHudSoundDiag.h"            // [DIAG] NOT IN THE X360 BINARY

// =============================================================================
// BrnSound::Logic::FxEffect - out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnFxEffect.h for the dual-base
// layout rationale, the embedded VoiceWrapper[4] array member, and the X360-32-bit
// -vs-host-64-bit offset note + the un-homed-leaf-member FLAG.
//
// This TU's recon'd function set is exactly two entries:
//   FxEffect()                                     @ 0x826C9288 (leaf constructor)
//   `vector deleting destructor'`adjustor{4}'      @ 0x826C9328 (a thin `this-4`
//        forwarder to FxEffect::`scalar deleting destructor', a SEPARATE un-homed
//        slice; compiler-synthesised, no source body -- see below).
//
// NOTE: class is BrnSound::Logic::FxEffect (X360 mangling
// `BrnSound::Logic::FxEffect::FxEffect`) -- there is NO `Fx` sub-namespace. The
// `Global/` directory is only a filesystem location, not a namespace level.
// =============================================================================

namespace BrnSound
{
namespace Logic
{

CgsSound::Logic::EffectObject* FxEffect::CreateObject(u32)
{
    return new FxEffect();
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* FxEffect::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject> sTypeInfo(
        0x30, "FxEffect", CgsSound::Logic::EffectObject::GetStaticTypeInfo(),
        &FxEffect::CreateObject);
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* FxEffect::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

const char* FxEffect::GetTypeName() const
{
    return "FxEffect";
}

static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* const gpFxEffectReg =
    CgsSound::Logic::EffectObject::AddToClassTypeInfoArray(FxEffect::GetStaticTypeInfo());

// ---------------------------------------------------------------------------
// FxEffect::FxEffect  @ 0x826C9288
//
//   stfs 0.0f, 0x20 ; stfs 0.0f, 0x1C           ; leaf f32 = 0.0f (FLAG: un-homed)
//   sth 0,0x10 ; stw 0,0xC ; stw 0,0x34 ; stw 0,8 ; stb 0,0x30 ; sth 0,0x12
//   stw 0,0x28 ; stw 0,0x24                      ; base meDetach/meAttach region = 0
//   stw off_820AE954,4 ; stw off_820B3800,0 ; stw off_820B37CC,4   ; dual-base vptrs
//   do { CgsSound::Logic::VoiceWrapper::VoiceWrapper(this + 0x38 + 0x50*i); } while(--i>=0)
//     ; r29 = 3 -> 4 iterations; 4 sub-objects constructed at +0x38/+0x88/+0xD8/+0x128.
//   stw 0,0x18C ; stb 0,0x190 ; stb 0,0x191      ; leaf words/bytes (FLAG: un-homed)
//   return this
//
// Same shape as the committed sibling ExplosionEffect::ExplosionEffect @ 0x826D5480:
// MSVC's INLINED full-object constructor installs the BrnEffectObject dual-vptr pair
// directly (primary @ this+0, IResourceRequester sub-object @ this+4, the transient
// off_820AE954 @ +4 overwritten by the final off_820B37CC) and inlines the base
// member zero-inits (meAttachState @ +0x24, meDetachState @ +0x28). In reconstructed
// C++ the vptr installs + base zero-inits are produced implicitly by the
// BrnEffectObject base sub-object's own default constructor (reused BY NAME). The raw
// vptr addresses NATURALLY DIFFER from ExplosionEffect's because each leaf class owns
// its own vtable -- what is shared is the dual-base STRUCTURE, not the addresses.
//
// FxEffect's own leaf tail differs from ExplosionEffect's: instead of one embedded
// VoiceWrapper it embeds an ARRAY of 4, constructed in the do/while loop (stride
// 0x50 = 80 bytes, base +0x38), reused BY NAME from the minimal VoiceWrapper home.
//
// FLAG (un-homed leaf members): the additional inlined leaf scalar zero-inits target
// FxEffect's OWN leaf members. Names/types are un-homed (no DWARF, no Feb-2007
// source). They are DECLARATION-ONLY (see header FLAG) and are NOT fabricated as
// named fields and NOT raw-offset-hacked here.
// ---------------------------------------------------------------------------
FxEffect::FxEffect()
    : BrnEffectObject()   // installs the base vptrs + zero-inits the base members (BY NAME)
    , mVoiceWrappers()    // tail do/while `bl CgsSound::Logic::VoiceWrapper::VoiceWrapper`
                          // x4 at this+0x38, +0x88, +0xD8, +0x128 (stride 0x50)
{
    // The remaining inlined leaf scalar zero-inits target un-homed leaf members
    // (DECLARATION-ONLY; see header FLAG). NOT fabricated here.
}

// ---------------------------------------------------------------------------
// ~FxEffect  (the leaf destructor the deleting-destructor thunks forward to).
// Its full member-teardown slice is DEFERRED; this out-of-line anchor forwards to
// the inherited BrnEffectObject base destructor chain (which tears down the embedded
// mVoiceWrappers[] array + the base members BY NAME). No fabricated teardown added.
// It exists so the class has a defined key function (the vtable emission point).
// ---------------------------------------------------------------------------
FxEffect::~FxEffect()
{
}

// ---------------------------------------------------------------------------
// `vector deleting destructor'`adjustor{4}'  @ 0x826C9328
//
//   0x826C9328  addi  r3, r3, -4
//   0x826C932C  b     BrnSound::Logic::FxEffect::`scalar deleting destructor'
//
// The compiler-synthesised multiple-inheritance adjustor thunk. A delete through an
// IResourceRequester* enters here on the IResourceRequester sub-object (which lives
// at this+4), recovers the primary FxEffect `this` (this - 4), then tail-calls the
// real (scalar) deleting destructor. The MI layout comes from the DWARF-confirmed
// base graph (FxEffect : BrnEffectObject; BrnEffectObject : EffectObject,
// IResourceRequester -> IResourceRequester sub-object vptr @ this+4). Per project
// convention this pure `this-4` MI-adjustor-then-tailcall is a compiler-synthesised
// artifact -- the host toolchain regenerates the equivalent thunk automatically from
// `virtual ~FxEffect()` plus the inheritance graph. No source body is emitted.
// (Same precedent as the committed BrnEffectObject.cpp adjustor{4} @ 0x826967E0.)
// ---------------------------------------------------------------------------


// =============================================================================
// THE FX-STING LANE -- Attach / Detach / ProcessUpdate / Notify.
// FxEffect is registered with ObjectID 0x30, so it resolves as Global state /
// effect id ((0x30 >> 4) & 0x7F) == 3 -- exactly where SoundLogicModule's
// ProcessGameActionQueue posts sound message 4 (E_SOUNDMESSAGE_FXMESSAGE) for
// game actions 40 (quit event -> type 7), 56 (stunt jump -> type 4) and 58
// (stunt smash -> type 2 / stunt -> type 3). Notify @0x826F7248 had NO BODY, so
// every one of those landed on the base EffectObject::Notify no-op: the stunt,
// stunt-jump, smash and quit-event stings were silent.
// =============================================================================

namespace
{
    // [DIAG] NOT IN THE X360 BINARY -- witness budget.
    u32 guFxDiagMessages = 0;

    // The X360 keeps ONE round-robin counter per FxType in .data
    // (dword_8300C784 .. dword_8300C7A4, post-incremented at each use) and hands
    // it to GetSampleTag as the selection value. Same lifetime (module globals),
    // same post-increment.
    u32 guFxSelectWindowSmash     = 0;  // dword_8300C7A4
    u32 guFxSelectCameraCut       = 0;  // dword_8300C794
    u32 guFxSelectStuntSmash      = 0;  // dword_8300C7A0
    u32 guFxSelectStuntStunt      = 0;  // dword_8300C798
    u32 guFxSelectResetOnTrack    = 0;  // dword_8300C788
    u32 guFxSelectCameraPhoto     = 0;  // dword_8300C784
    u32 guFxSelectQuitEvent       = 0;  // dword_8300C790
    u32 guFxSelectCrashInWater    = 0;  // dword_8300C79C
    u32 guFxSelectOnlineRivalSweep = 0; // dword_8300C78C
}

// ---------------------------------------------------------------------------
// FxEffect::Attach()  @ 0x8269E158
//
//   *(a1 + 14) += 1 ; *(a1 + 36) = 0            ; base attach (inlined)
//   for ( i = 0; i < 4; ++i ) {
//       *(a1 + 388 + i) = 0;                    ; mau8MixerOutputs[i] = 0
//       *(f32*)(a1 + 372 + 4*i) = 1.0;          ; mafVolumes[i] = 1.0
//   }
//   mpGlobalStateManager = *(*(a1 + 8) + 36);   ; mpState->mpStateManager
//   assert( mpGlobalStateManager );                              ; cpp:213
//   *(a1 + 400) = 0;                            ; the UpdateParams cooldown word
//   return 1;
// ---------------------------------------------------------------------------
bool FxEffect::Attach()
{
    if (!CgsSound::Logic::EffectBase::Attach())
        return false;

    for (s32 liVoice = 0; liVoice < KI_NUM_VOICES; ++liVoice)
    {
        mau8MixerOutputs[liVoice] = 0;
        mafVolumes[liVoice]       = 1.0f;
    }

    mpGlobalStateManager = static_cast<GlobalStateManager*>(
        GetStateBase() ? GetStateBase()->GetStateManager() : 0);
    CGS_ASSERT(mpGlobalStateManager != 0, "mpGlobalStateManager");

    miCooldown = 0;
    return true;
}

// ---------------------------------------------------------------------------
// FxEffect::Detach()  @ 0x826F71D8
//
//   if ( !BrnSound::Logic::BrnEffectObject::Detach(a1) ) return 0;
//   4x VoiceWrapper::Release(a1 + 52 + 80*i);     ; mVoiceWrappers
//   return 1;
// ---------------------------------------------------------------------------
bool FxEffect::Detach()
{
    if (!BrnSound::Logic::BrnEffectObject::Detach())
        return false;

    for (s32 liVoice = 0; liVoice < KI_NUM_VOICES; ++liVoice)
        mVoiceWrappers[liVoice].Release();
    return true;
}

// ---------------------------------------------------------------------------
// FxEffect::ProcessUpdate()  @ 0x826E74F8
//
//   for ( i = 0; i < 4; ++i ) {
//       mVoiceWrappers[i].Update();
//       if ( state == FINISHED || state == IDLE ) continue;
//       gain = mpDynamicMixIo ? GetDMixOutput(mau8MixerOutputs[i], 0) / 32767 : 0.0;
//       if ( live voice ) {
//           Voice::SetGain(0, mafVolumes[i] * gain, &<send name>);
//           Voice::SetParameter(0, 1.0, &<~SplicerPlayerVoice::Pitch~>);
//       }
//   }
// ---------------------------------------------------------------------------
void FxEffect::ProcessUpdate()
{
    const u32 luSendName = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("Send01"));
    const u32 luSplicerPitchName = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("~SplicerPlayerVoice::Pitch~"));

    for (s32 liVoice = 0; liVoice < KI_NUM_VOICES; ++liVoice)
    {
        CgsSound::Logic::VoiceWrapper& lrVoice = mVoiceWrappers[liVoice];
        lrVoice.Update();

        const s32 liState = lrVoice.GetState();
        if (liState == CgsSound::Logic::VoiceWrapper::E_UPDATE_STAGE_FINISHED ||
            liState == CgsSound::Logic::VoiceWrapper::E_UPDATE_STAGE_IDLE)
            continue;

        const f32 lfGain = GetMixerOutputValue(mau8MixerOutputs[liVoice],
                                               Nicotine::DMixIO::DMX_VOL) / 32767.0f;
        if (lrVoice.HasLiveVoice())
        {
            lrVoice.SetGain(0, mafVolumes[liVoice] * lfGain, &luSendName);
            lrVoice.SetParameter(0, 1.0f, &luSplicerPitchName);
        }
    }
}

// ---------------------------------------------------------------------------
// FxEffect::FindFreeVoice()  -- the four-slot scan the X360 inlines at the head
// of Notify @0x826F7248 (`v6[31] / v6[51] / v6[71] / v6[91]`, i.e. each
// wrapper's meUpdateStage). Like HUDEffect::PlaySound it keeps the LAST free
// slot rather than breaking out.
// ---------------------------------------------------------------------------
s32 FxEffect::FindFreeVoice() const
{
    s32 liSlot = -1;
    for (s32 liVoice = 0; liVoice < KI_NUM_VOICES; ++liVoice)
    {
        const s32 liState = mVoiceWrappers[liVoice].GetState();
        if (liState == CgsSound::Logic::VoiceWrapper::E_UPDATE_STAGE_FINISHED ||
            liState == CgsSound::Logic::VoiceWrapper::E_UPDATE_STAGE_IDLE)
            liSlot = liVoice;
    }
    return liSlot;
}

// ---------------------------------------------------------------------------
// FxEffect::Notify(const MessageHeader*)  @ 0x826F7248
//
//   assert( lpMessageHeader );                                       ; cpp:268
//   assert( GetEventId() == E_SOUNDMESSAGE_FXMESSAGE );               ; cpp:269
//   liSlot = <the four-slot free scan>;  if ( liSlot == -1 ) return;
//   lTag = { 0.0f, 0 };
//   switch ( message.meType ) {
//     case 0 E_WINDOW_SMASH:      tag 2, index (see FLAG),   bank = FX
//     case 1 E_CAMERA_CUT:        GetSampleTag(4,  2, sel),  bank = PRESENTATION
//     case 2 E_STUNT_SMASH:       GetSampleTag(4,  0, sel),  bank = PRESENTATION
//     case 3 E_STUNT_STUNT:       GetSampleTag(4,  1, sel),  bank = PRESENTATION
//     case 5 E_RESET_ON_TRACK:    GetSampleTag(2,  3, sel),  bank = FX
//     case 6 E_CAMERA_PHOTO:      GetSampleTag(2,  6, sel),  bank = FX
//     case 7 E_QUIT_EVENT:        GetSampleTag(4,  8, sel),  bank = PRESENTATION
//     case 8 E_CRASH_IN_WATER:    GetSampleTag(1,  5, sel),  bank = the COLLISION
//                                 splice bank (module +10600 -> +33320)
//     case 9 E_ONLINE_RIVAL_SWEEP:GetSampleTag(4, 23, sel),  bank = PRESENTATION
//     default (4 E_STUNT_JUMP):   result = 0 -> play nothing
//   }
//   every handled case also stamps mau8MixerOutputs[liSlot] = 3.
//   if ( resolved ) {
//       mVoiceWrappers[liSlot].Create({ module, ~SplicerFactory::SK_NAME~,
//            SplicerVoiceSpec, <bank>, 0, ~SplicerPlayerVoice::Slot~, Send01,
//            submix 1, sendIndex 0 });
//       mVoiceWrappers[liSlot].Play(lTag.miSampleIndex);   ; inlined Play
//       mafVolumes[liSlot] = lTag.mfVolume;
//   }
//
// ⚠ NOTE ON case 4 (E_STUNT_JUMP): the X360 switch has NO case 4 arm -- the jump
// table's "default" covers it and sets result = 0, i.e. the stunt-jump FX message
// game action 56 posts plays NOTHING through this effect. That is the console's
// own behaviour, reproduced, not an omission here.
//
// FLAG (case 0, E_WINDOW_SMASH): the console selects the sample index from a word
// it reads at `*(lpMessageHeader + 0xC8)` -- 200 bytes into the message record,
// far past the 20-byte Message<FxMessage> the queue actually stores (the X360
// AddEvent<Message<FxMessage_*>> record size is 0x14). No PC producer posts type
// 0 at all (ProcessGameActionQueue only posts 7 / 4 / 2 / 3), so rather than
// reproduce an out-of-record read this takes the console's OWN "neither 1 nor 2"
// arm: no GetSampleTag call, sample index 0, gain 0.0, FX splice bank. Restore the
// read if that field is ever identified.
// ---------------------------------------------------------------------------
void FxEffect::Notify(const CgsSound::Io::MessageHeader* apMessageHeader)
{
    CGS_ASSERT(apMessageHeader != 0, "lpMessageHeader");
    if (!apMessageHeader)
        return;
    CGS_ASSERT(apMessageHeader->GetEventId() == 4,
               "lpMessageHeader->GetEventId() == E_SOUNDMESSAGE_FXMESSAGE");
    if (apMessageHeader->GetEventId() != 4)
        return;

    const s32 liSlot = FindFreeVoice();
    if (liSlot == -1)
        return;

    const CgsSound::Io::Message<s32>* lpMessage =
        static_cast<const CgsSound::Io::Message<s32>*>(apMessageHeader);
    const s32 leType = lpMessage->mData;

    BrnEffectObject::SampleTag lTag;
    lTag.mfVolume      = 0.0f;
    lTag.miSampleIndex = 0;

    const CgsSound::Logic::Content* lpBank = 0;
    bool lbResolved = true;

    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(GetLogicModule());

    switch (leType)
    {
    case FxMessage::E_WINDOW_SMASH:
        // See the FLAG above: the console's index selector is unreachable data.
        lpBank = mpGlobalStateManager ? &mpGlobalStateManager->GetFxSpliceBank() : 0;
        mau8MixerOutputs[liSlot] = 3;
        break;

    case FxMessage::E_CAMERA_CUT:
        lbResolved = GetSampleTag(4, 2, guFxSelectCameraCut++, lTag);
        lpBank = mpGlobalStateManager ? &mpGlobalStateManager->GetPresentationSpliceBank() : 0;
        mau8MixerOutputs[liSlot] = 3;
        break;

    case FxMessage::E_STUNT_SMASH:
        lbResolved = GetSampleTag(4, 0, guFxSelectStuntSmash++, lTag);
        lpBank = mpGlobalStateManager ? &mpGlobalStateManager->GetPresentationSpliceBank() : 0;
        mau8MixerOutputs[liSlot] = 3;
        break;

    case FxMessage::E_STUNT_STUNT:
        lbResolved = GetSampleTag(4, 1, guFxSelectStuntStunt++, lTag);
        lpBank = mpGlobalStateManager ? &mpGlobalStateManager->GetPresentationSpliceBank() : 0;
        mau8MixerOutputs[liSlot] = 3;
        break;

    case FxMessage::E_RESET_ON_TRACK:
        lbResolved = GetSampleTag(2, 3, guFxSelectResetOnTrack++, lTag);
        lpBank = mpGlobalStateManager ? &mpGlobalStateManager->GetFxSpliceBank() : 0;
        mau8MixerOutputs[liSlot] = 3;
        break;

    case FxMessage::E_CAMERA_PHOTO:
        lbResolved = GetSampleTag(2, 6, guFxSelectCameraPhoto++, lTag);
        lpBank = mpGlobalStateManager ? &mpGlobalStateManager->GetFxSpliceBank() : 0;
        mau8MixerOutputs[liSlot] = 3;
        break;

    case FxMessage::E_QUIT_EVENT:
        lbResolved = GetSampleTag(4, 8, guFxSelectQuitEvent++, lTag);
        lpBank = mpGlobalStateManager ? &mpGlobalStateManager->GetPresentationSpliceBank() : 0;
        mau8MixerOutputs[liSlot] = 3;
        break;

    case FxMessage::E_CRASH_IN_WATER:
        lbResolved = GetSampleTag(1, 5, guFxSelectCrashInWater++, lTag);
        // The X360 walks module+10600 -> +33320 for this one: the COLLISION state
        // manager's own splice bank, not a Global one.
        // FLAG: the collision manager is not reachable by name from this effect in
        // this tree, so the bank stays null for type 8 and the voice plays from the
        // registry default. No PC producer posts type 8 today.
        lpBank = 0;
        mau8MixerOutputs[liSlot] = 3;
        break;

    case FxMessage::E_ONLINE_RIVAL_SWEEP:
        lbResolved = GetSampleTag(4, 23, guFxSelectOnlineRivalSweep++, lTag);
        lpBank = mpGlobalStateManager ? &mpGlobalStateManager->GetPresentationSpliceBank() : 0;
        mau8MixerOutputs[liSlot] = 3;
        break;

    default:
        // Includes E_STUNT_JUMP (4): the console's jump table has no arm for it.
        lbResolved = false;
        break;
    }

    if (HudSoundDiagBudget(guFxDiagMessages))
    {
        HudSoundDiagPrintf(
            "[fx-sound-msg] type=%d slot=%d -> %s sample=%d vol=%.3f bank=%d\n",
            leType, liSlot, lbResolved ? "PLAY" : "IGNORED",
            static_cast<s32>(lTag.miSampleIndex),
            static_cast<double>(lTag.mfVolume), lpBank ? 1 : 0);
    }

    if (!lbResolved)
        return;

    CgsSound::Logic::VoiceWrapper::CreateParams lParams;
    lParams.Clear();
    lParams.mpLogicModule  = lpModule;
    lParams.mFactoryName   = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("~SplicerFactory::SK_NAME~"));
    lParams.mVoiceSpecName = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("SplicerVoiceSpec"));
    lParams.mpContent      = lpBank;
    lParams.mContentSpecName = 0;
    lParams.mSlotName = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("~SplicerPlayerVoice::Slot~"));
    lParams.mSendName = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("Send01"));
    lParams.mSubMixVoiceID = 1;
    lParams.miSendIndex    = 0;

    mVoiceWrappers[liSlot].Create(lParams);
    mVoiceWrappers[liSlot].Play(static_cast<u32>(lTag.miSampleIndex));
    mafVolumes[liSlot] = lTag.mfVolume;
}

} // namespace Logic
} // namespace BrnSound
