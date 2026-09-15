#include "GameSource/Sound/Global/BrnHUDEffect.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"              // CGS_ASSERT
#include "GameShared/GameClasses/Sound/IO/CgsMessage.h"         // CgsSound::Io::Message / MessageHeader
#include "GameShared/GameClasses/Sound/Logic/CgsState.h"        // State -> StateManager walk
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"    // Name::MakeHash / PlayerVoice slot names
#include "GameShared/GameClasses/Sound/Playback/CgsVoice.h"
#include "GameSource/Sound/Global/BrnGlobalStateManager.h"      // GetPresentationSpliceBank
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameSource/Sound/Global/BrnHudSoundDiag.h"            // [DIAG] NOT IN THE X360 BINARY
#include "rw/math/vpu/vector3_operation.h"   // rw::math::vpu::Dot + operator-(Vec,Vec) + operator*(Vec,float)
#include <cmath>                              // std::sqrt

// =============================================================================
// BrnSound::Logic::HUDEffect::GameModeData -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnHUDEffect.h for the layout
// rationale and the X360-32-bit-vs-host-64-bit offset note.
//
// This TU's recon'd function set is exactly two entries:
//   GameModeData::GameModeData (Construct)  @ 0x826AFE88
//   GameModeData::Reset                     @ 0x826977D8
//
// dep_flags: none un-homed for THIS TU. Every member written is modelled BY NAME
// in the owning header (the Average<10,f32> / DataPoint<T> generics live in the
// committed CgsSoundUtils home).
// =============================================================================

namespace BrnSound
{
namespace Logic
{

CgsSound::Logic::EffectObject* HUDEffect::CreateObject(u32)
{
    return new HUDEffect();
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* HUDEffect::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject> sTypeInfo(
        0x10, "HUDEffect", CgsSound::Logic::EffectObject::GetStaticTypeInfo(),
        &HUDEffect::CreateObject);
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* HUDEffect::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

const char* HUDEffect::GetTypeName() const
{
    return "HUDEffect";
}

static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* const gpHUDEffectReg =
    CgsSound::Logic::EffectObject::AddToClassTypeInfoArray(HUDEffect::GetStaticTypeInfo());

// ---------------------------------------------------------------------------
// HUDEffect::GameModeData::GameModeData  @ 0x826AFE88  (the DWARF Construct)
//
//   for ( i = 0; i < 10; ++i ) *(4*i + this) = 0.0;   ; mEventScoreDelta.maPoints
//   *(this+44) = 0.0;  *(this+40) = 0;                 ; mfAverage / muNextPoint
//   *(this+48..60) = 0.0;                              ; time-remaining / boost DPs
//   *(this+68..104) = 0;                               ; stunt + showtime DPs
//
// The X360 ctor zeroes every field it touches (it does NOT write +64
// mfTimeSinceScoreTick nor +108 mbTimeExtended -- those are left as Reset's job).
// Reconstructed as explicit member zeroing in DWARF order.
// ---------------------------------------------------------------------------
HUDEffect::GameModeData::GameModeData()
{
    // mEventScoreDelta: zero the 10 sample points, the next-point index and the
    // cached average (the i<10 loop + the +40/+44 stores).
    for (u32 lu = 0; lu < 10u; ++lu)
    {
        mEventScoreDelta.maPoints[lu] = 0.0f;
    }
    mEventScoreDelta.muNextPoint = 0;
    mEventScoreDelta.mfAverage   = 0.0f;

    mEventTimeRemaining.Flush(0.0f);   // +48,+52
    mEventBoostAmount.Flush(0.0f);     // +56,+60

    mStuntScore.Flush(0);              // +68,+72
    mStuntComboMultiplier.Flush(0);    // +76,+80
    mStuntResultScore.Flush(0);        // +84,+88

    mShowtimeScore.Flush(0.0f);        // +92,+96
    mShowtimeBoostDelta.Flush(0.0f);   // +100,+104
}

// ---------------------------------------------------------------------------
// HUDEffect::GameModeData::Reset  @ 0x826977D8
//
//   *(this+44) = 0.0;                       ; mEventScoreDelta.mfAverage
//   do { *(4*v1 + this) = 0.0; } while (v1 < 10);  ; maPoints
//   *(this+40) = 0;                          ; muNextPoint
//   *(this+48..60) = 0.0;                    ; time-remaining / boost DPs
//   *(this+64) = 0.0;                        ; mfTimeSinceScoreTick
//   *(this+68) = 0; *(this+72) = 0;          ; mStuntScore
//   *(this+76) = 1; *(this+80) = 1;          ; mStuntComboMultiplier = 1 (cur+prev)
//   *(this+84) = 0; *(this+88) = 0;          ; mStuntResultScore
//   *(this+92..104) = 0.0;                   ; showtime DPs
//   *(this+108) = 1;                         ; mbTimeExtended = true
//
// Reset differs from Construct: it ALSO clears mfTimeSinceScoreTick (+64), defaults
// the stunt combo multiplier to 1 (both current and previous words), and sets
// mbTimeExtended true.
// ---------------------------------------------------------------------------
void HUDEffect::GameModeData::Reset()
{
    mEventScoreDelta.mfAverage = 0.0f;
    for (u32 lu = 0; lu < 10u; ++lu)
    {
        mEventScoreDelta.maPoints[lu] = 0.0f;
    }
    mEventScoreDelta.muNextPoint = 0;

    mEventTimeRemaining.Flush(0.0f);   // +48,+52
    mEventBoostAmount.Flush(0.0f);     // +56,+60

    mfTimeSinceScoreTick = 0.0f;       // +64

    mStuntScore.Flush(0);              // +68,+72
    mStuntComboMultiplier.Flush(1);    // +76,+80  (combo defaults to 1)
    mStuntResultScore.Flush(0);        // +84,+88

    mShowtimeScore.Flush(0.0f);        // +92,+96
    mShowtimeBoostDelta.Flush(0.0f);   // +100,+104

    mbTimeExtended = true;             // +108
}

// =============================================================================
// BrnSound::Logic::HUDEffect -- the enclosing effect leaf (grown surface).
// =============================================================================

// Runtime-initialised module globals mirroring the X360 .data slots
// HUDEffect::GetSlipstreamAmount reads (the shared 0.0/1.0 singletons are in the
// 0x8200_xxxx rodata range; these two live in the 0x8300_xxxx MUTABLE data segment,
// so they are runtime-initialised FILE-SCOPE GLOBALS, not function-local constants):
//   * flt_830060C0 is written by sub_82C62DB8 as XMVectorCos(0.346f) == cos(0.346 rad)
//     == 0.9407368f (a ~19.8deg slipstream-cone half-angle cosine gate). VALUE RECOVERED
//     from the writer.
//   * flt_830083E4 has a single read-only xref (this function) and no exported writer --
//     its runtime value is UNRECOVERABLE. Modelled as a flagged global initialised to
//     1.0f (identity multiplier) so the shape compiles; confidence LOW on this one number.
static f32 gfSlipstreamConeCos = 0.9407368f;  // flt_830060C0 == cos(0.346f)
static f32 gfSlipstreamScale   = 1.0f;        // flt_830083E4 -- UNRESOLVED runtime value (confidence=low)

// ---------------------------------------------------------------------------
// HUDEffect::HUDEffect (Construct)  @ 0x826E1940  (MINIMAL -- see FLAGs)
//
// MSVC's INLINED full-object constructor: it does NOT `bl` a base ctor; it installs the
// BrnEffectObject dual leaf-vptr pair (this+0 primary off_820B588C, this+4
// IResourceRequester off_820B5858) and zero-inits the base members directly, then
// tail-constructs the embedded sub-objects (maVoices[3], maGameModeVoices[4] each
// running its mVoice VoiceWrapper ctor, mMusicStream @ +0x3A0, mGameModeData @ +0x430,
// mHudMessageData @ +0x4A0). Here the base vptr install + base zero-init are produced BY
// NAME by BrnEffectObject(), and mGameModeData is default-constructed as an ordinary
// member (matching the X360 tail `bl GameModeData::GameModeData`).
//
// FLAG (un-homed sub-object types -> DEFERRED): the CustomHudVoice array (maGameModeVoices),
// the BrnSound::Logic::MusicStream object (mMusicStream @ +0x3A0), the
// Attrib::Gen::presentationcomponent (mHudMessageData @ +0x4A0), the maVoices[3]
// VoiceWrapper array, and the trailing scalar fixups (volumes=1.0, the 96000.0f
// sample-rate, the 10-float average @ +0x400, mLastPlayedEvent/CgsID/round-robin @
// +0x4C8..) have no committed homes and are DEFERRED here -- not fabricated (matching the
// committed StreamingEffect `Attrib::Gen::streamsettings` treatment).
// ---------------------------------------------------------------------------
HUDEffect::HUDEffect()
    : BrnEffectObject()   // dual vptr install (this+0 / this+4) + base member zero-init, BY NAME
    // mGameModeData default-constructs (GameModeData::GameModeData, already committed).
    // maVoices[3], maGameModeVoices[4], mMusicStream and mHudMessageData are UN-HOMED and
    // DEFERRED (see FLAG) -- not fabricated as named members / calls in this minimal home.
{
}

// ---------------------------------------------------------------------------
// ~HUDEffect  (the out-of-line anchor the vector deleting destructor @ 0x826E1E00 and
// its adjustor{4} @ 0x826E1BC0 forward to). Both are MSVC compiler-synthesised thunks
// over this virtual destructor: the vector deleting destructor calls this leaf dtor then
// conditionally frees via the global sound MemBase allocator (off_82FFB954, slot +0x14),
// and the adjustor{4} recovers the primary `this` from the +4 IResourceRequester
// sub-object pointer. The observable member teardown lives in the inherited BrnEffectObject
// base chain (this slice defers the un-homed leaf members), so this anchor body is empty;
// the host toolchain re-synthesises both thunks from this class's dual base + virtual
// dtor. No fabricated allocator is added.
// ---------------------------------------------------------------------------
HUDEffect::~HUDEffect()
{
}

// ---------------------------------------------------------------------------
// HUDEffect::GetSlipstreamAmount  @ 0x82686BA8
//
// Slipstream-audio proximity/alignment weight for the car in front. `this` (r3) is
// unused; the three Vector3 args arrive in vector registers v1/v2/v3:
//   v1 = car-in-front velocity, v2 = player's weighted velocity,
//   v3 = negated look direction (caller vxor128 sign-flip).
//
// ASM (0x82686BA8-0x82686C5C), store-for-store:
//   relVel  = v1 - v2                                     ; vsubfp
//   distSq  = Dot3(relVel, relVel)                         ; vmsum3fp128
//   projRaw = Dot3(v3, relVel)                             ; vmsum3fp128
//   invDist = 1/sqrt(distSq)                               ; vrsqrtefp + 1 NR step
//   t1 = 1.0 - (projRaw - 25.0) * (1/15), saturate01       ; two fsel
//   unitRelVel = relVel * invDist                          ; normalize
//   projNorm   = Dot3(unitRelVel, v3)                      ; vmsum3fp128
//   t2 = saturate01(projNorm - gfSlipstreamConeCos)        ; two fsel
//   return (t2 * gfSlipstreamScale) * t1
//
// FLAG (VMX->portable): vrsqrtefp + 1 NR refine collapsed to exact 1.0f/std::sqrt
//   (project convention, matches the BrnMathUtils Magnitude family).
// FLAG (fsel-faithful clamps): both saturate pairs reproduce the PPC
//   `fsel(a,b,c) == (a >= 0.0f) ? b : c` sense exactly (NOT std::clamp).
// FLAG (globals, NOT rodata literals): gfSlipstreamConeCos (flt_830060C0 == cos(0.346))
//   and gfSlipstreamScale (flt_830083E4, UNRESOLVED, modelled as 1.0f) are runtime-
//   initialised file-scope globals -- see the definitions above.
// ---------------------------------------------------------------------------
f64 HUDEffect::GetSlipstreamAmount( Vector3 lCarInFrontVelocity,
                                    Vector3 lOwnWeightedVelocity,
                                    Vector3 lNegatedDirection )
{
    const Vector3 lRelativeVelocity = lCarInFrontVelocity - lOwnWeightedVelocity;   // v0 = v1 - v2

    const f32 lfDistSq  = rw::math::vpu::Dot(lRelativeVelocity, lRelativeVelocity); // vmsum3fp128 v13,v0,v0
    const f32 lfProjRaw = rw::math::vpu::Dot(lNegatedDirection, lRelativeVelocity); // vmsum3fp128 v10,v3,v0

    // vrsqrtefp + 1 Newton-Raphson refine -> exact reciprocal-sqrt (see FLAG).
    const f32 lfInvDist = 1.0f / std::sqrt(lfDistSq);

    // t1 = 1.0 - (projRaw - 25.0) * (1/15), then saturate to [0,1] (two fsel).
    f32 lfSpeedFactor = 1.0f - (lfProjRaw - 25.0f) * 0.06666667f;
    lfSpeedFactor = ((-lfSpeedFactor) >= 0.0f) ? 0.0f : lfSpeedFactor;                 // fsel: lower clamp -> 0
    lfSpeedFactor = ((1.0f - lfSpeedFactor) >= 0.0f) ? lfSpeedFactor : 1.0f;           // fsel: upper clamp -> 1

    const Vector3 lUnitRelativeVelocity = lRelativeVelocity * lfInvDist;              // vmulfp128 v0,v0,v13
    const f32 lfProjNorm = rw::math::vpu::Dot(lUnitRelativeVelocity, lNegatedDirection); // vmsum3fp128 v0,v0,v3

    // t2 = saturate01(projNorm - coneCos) (two fsel).
    f32 lfDistanceFactor = lfProjNorm - gfSlipstreamConeCos;
    lfDistanceFactor = ((-lfDistanceFactor) >= 0.0f) ? 0.0f : lfDistanceFactor;        // fsel: lower clamp -> 0
    lfDistanceFactor = ((1.0f - lfDistanceFactor) >= 0.0f) ? lfDistanceFactor : 1.0f;  // fsel: upper clamp -> 1

    return static_cast<f64>((lfDistanceFactor * gfSlipstreamScale) * lfSpeedFactor);
}


// =============================================================================
// THE GUI-STING LANE -- Attach / Detach / ProcessUpdate / Notify and the three
// private helpers it drives. Every one of these had NO DEFINITION IN THE TREE
// before this commit, so sound message 5 (E_SOUNDMESSAGE_GUIAUDIO_EVENT), which
// SoundLogicModule::ProcessGuiEvents case 456 posts to (manager 0, state 0,
// effect 1) == this class, landed on the base EffectObject::Notify no-op and
// every HUD sting in the game was silent.
// =============================================================================

namespace
{
    // [DIAG] NOT IN THE X360 BINARY -- per-family line budgets.
    u32 guHudDiagAttach = 0;
    u32 guHudDiagMessages = 0;
    u32 guHudDiagPlays = 0;
    u32 guHudDiagVoices = 0;
}

// ---------------------------------------------------------------------------
// HUDEffect::Attach()  @ 0x8269C3C8
//
// X360 store-for-store (r31 == the EffectBase sub-object, i.e. `this + 4`):
//   *(a1 + 14)   += 1                      ; base attach counter (halfword)
//   *(a1 + 36)    = 0                      ; base word
//   Attrib::Instance::ChangeWithDefault(a1 + 1180, *(*(a1 + 40) + 79116) + 1280);
//                                          ; mHudMessageData <- BurnoutGlobalData
//                                          ;   +0x500 == the mHudMessages RefSpec
//   *(a1 + 1204)  = 0                      ; mLastPlayedEvent.miAdditionalInformation
//   *(a1 + 1212)  = 0 (64-bit)             ; mLastPlayedEvent.mHudMessageId
//   *(a1 + 1200)  = 7                      ; mLastPlayedEvent.miAction     = 7
//   *(a1 + 1196)  = 3                      ; mLastPlayedEvent.miComponentType = 3
//   *(a1 + 1228)  = 0.5                    ; mfTimeSinceLastTrigger = 0.5 (armed:
//                                          ;   IsReTrigger's guard is `< 0.5`, so a
//                                          ;   freshly attached effect never reports
//                                          ;   a re-trigger)
//   *(a1 + 1220)  = 0 (64-bit)             ; mLastReceivedHudMessage
//   4x over v5 = a1+640 stepping 92:       ; maGameModeVoices[4]
//       *(v5 - 4) = 1.0 ; *v5 = 1.0 ; *(v5 + 4) = 3
//                                          ; mfClientVolume / mfClientPitch /
//                                          ; mu8MixerOutput = 3
//   3x over v6 = a1+551:                   ; maVoices[3]
//       *(v6 - 3) = 3   ; *v6++ = 0        ; mau8MixerOutputs = 3, mau8ChokeGroups = 0
//   the mMusicStream block (a1+928..a1+1016) reset to "stopped, silent,
//   LP 96000 Hz, priority 2, nothing queued, output slot 0"
//   the mGameModeData Average block (a1+1020..a1+1064) zeroed
//   return 1
//
// The X360 does NOT call the base EffectBase::Attach here: it inlines the two
// base stores (the +14 counter and the +36 word). Expressed by name as the base
// call, which performs exactly those.
// ---------------------------------------------------------------------------
bool HUDEffect::Attach()
{
    if (!CgsSound::Logic::EffectBase::Attach())
        return false;

    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(GetLogicModule());
    if (lpModule)
        mHudMessageData.ChangeWithDefault(lpModule->GetGlobalData().HudMessages());

    // The "nothing has been played yet" sentinel the X360 seeds: component type 3
    // and action 7 are outside the 1/2 range Notify handles, so the first real
    // event can never match mLastPlayedEvent.
    mLastPlayedEvent.miComponentType         = 3;
    mLastPlayedEvent.miAction                = 7;
    mLastPlayedEvent.miAdditionalInformation = 0;
    mLastPlayedEvent.miPad                   = 0;
    mLastPlayedEvent.mHudMessageId           = 0;
    mLastReceivedHudMessage                  = 0;
    mfTimeSinceLastTrigger                   = 0.5f;

    for (s32 liVoice = 0; liVoice < E_NUM_GAME_MODE_VOICES; ++liVoice)
    {
        maGameModeVoices[liVoice].mfClientVolume = 1.0f;
        maGameModeVoices[liVoice].mfClientPitch  = 1.0f;
        maGameModeVoices[liVoice].mu8MixerOutput = 3;
    }
    for (s32 liVoice = 0; liVoice < E_NUM_VOICES; ++liVoice)
    {
        mau8MixerOutputs[liVoice] = 3;
        mau8ChokeGroups[liVoice]  = 0;
    }

    // The X360 writes the HUD music-stream block field by field rather than
    // calling MusicStream::Prepare (which would also stamp the CreateParams);
    // reproduced by name.
    // FLAG: the console also stamps the block's priority word to 2 here; the
    // committed MusicStream home exposes no priority setter and belongs to the
    // music group, so that ONE field is not written (nothing in this slice queues
    // the HUD stream, so no observable behaviour depends on it).
    mMusicStream.SetVolume(0.0f);
    mMusicStream.SetHighPassFreq(0.0f);
    mMusicStream.SetLowPassFreq(96000.0f);
    mMusicStream.StopAndUnqueue(0.0f);

    mGameModeData.mEventScoreDelta.mfAverage = 0.0f;
    for (u32 luPoint = 0; luPoint < 10u; ++luPoint)
        mGameModeData.mEventScoreDelta.maPoints[luPoint] = 0.0f;
    mGameModeData.mEventScoreDelta.muNextPoint = 0;

    if (HudSoundDiagBudget(guHudDiagAttach))
    {
        HudSoundDiagPrintf(
            "[hud-sound-attach] mHudMessageData bound: NumMappings=%u (0 == the "
            "presentationcomponent collection did not resolve)\n",
            mHudMessageData.HasData() ? mHudMessageData.NumMappings() : 0u);
    }
    return true;
}

// ---------------------------------------------------------------------------
// HUDEffect::Detach()  @ 0x826F5EF0
//
//   if ( BrnSound::Logic::BrnEffectObject::Detach(a1) ) {
//       3x VoiceWrapper::Release(a1 + 308 + 80*i);      ; maVoices
//       4x VoiceWrapper::Release(a1 + 556 + 92*i);      ; maGameModeVoices
//       return 1;
//   }
//   return 0;
// ---------------------------------------------------------------------------
bool HUDEffect::Detach()
{
    if (!BrnSound::Logic::BrnEffectObject::Detach())
        return false;

    for (s32 liVoice = 0; liVoice < E_NUM_VOICES; ++liVoice)
        maVoices[liVoice].Release();
    for (s32 liVoice = 0; liVoice < E_NUM_GAME_MODE_VOICES; ++liVoice)
        maGameModeVoices[liVoice].mVoice.Release();
    return true;
}

// ---------------------------------------------------------------------------
// HUDEffect::UpdateParams(f32)
//
// NOT an exported X360 symbol (the compiler folded the one-line body), but the
// DWARF declares it at BrnHUDEffect.cpp:86 and IsReTrigger @0x82686858 reads
// mfTimeSinceLastTrigger as a SECONDS accumulator with a 0.5 s guard, so the
// accumulator has to be advanced on the per-frame params pass. The base
// UpdateParams is a no-op, so this adds exactly the accumulate.
// FLAG: the X360 body for this one function is not in the export set; the
// accumulate is DERIVED from the invariant (Attach seeds 0.5, Notify clears to
// 0.0, IsReTrigger compares < 0.5). No other behaviour is added.
// ---------------------------------------------------------------------------
void HUDEffect::UpdateParams(f32 af32DeltaTime)
{
    mfTimeSinceLastTrigger += af32DeltaTime;
}

// ---------------------------------------------------------------------------
// HUDEffect::ProcessUpdate()  @ 0x826E7220
//
// Pump the three GUI voices and the four game-mode voices, re-applying the
// dynamic-mixer gain each frame:
//   for i in 0..2:
//       maVoices[i].Update();
//       if ( state != FINISHED && state != IDLE ) {
//           assert( mau8MixerOutputs[i] <= 0x12 );   ; HudOutput::KI_MAX_VALUE
//           gain = mpDynamicMixIo ? GetDMixOutput(mau8MixerOutputs[i], 0) / 32767
//                                 : 0.0;
//           if ( the wrapper has a live voice ) voice.SetGain(0, gain);
//       }
//   for i in 0..3:
//       maGameModeVoices[i].mVoice.Update();
//       if ( state != FINISHED && state != IDLE ) {
//           assert( mu8MixerOutput <= 0x12 );
//           gain = ... as above, for THIS voice's mu8MixerOutput;
//           if live: SetGain(0, mfClientVolume * gain);
//                    SetParameter(0 /* ~SplicerPlayerVoice::Pitch~ */, mfClientPitch);
//       }
//   SetMixerInputValue(0, 0);                        ; release the HUD input slot
//   mMusicStream.mfVolume = GetDMixOutput(mMusicStream output slot, 0) / 32767;
//   mMusicStream.mfHighPassFrequency = 0.0;
//   mMusicStream.mfLowPassFrequency  = 96000.0;
//
// 0.000030518509 == 1/32767 (the Q15 -> linear conversion the committed
// EffectBase::GetMixerOutputValue already performs).
// ---------------------------------------------------------------------------
void HUDEffect::ProcessUpdate()
{
    const u32 luSendName = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("Send01"));
    const u32 luSplicerPitchName = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("~SplicerPlayerVoice::Pitch~"));

    for (s32 liVoice = 0; liVoice < E_NUM_VOICES; ++liVoice)
    {
        CgsSound::Logic::VoiceWrapper& lrVoice = maVoices[liVoice];
        lrVoice.Update();

        const s32 liState = lrVoice.GetState();
        if (liState == CgsSound::Logic::VoiceWrapper::E_UPDATE_STAGE_FINISHED ||
            liState == CgsSound::Logic::VoiceWrapper::E_UPDATE_STAGE_IDLE)
            continue;

        CGS_ASSERT(mau8MixerOutputs[liVoice] <= 0x12,
                   "mau8MixerOutputs[ liVoice ] <= AttribSys::Enums::HudOutput::KI_MAX_VALUE");
        const f32 lfGain = GetMixerOutputValue(mau8MixerOutputs[liVoice],
                                               Nicotine::DMixIO::DMX_VOL) / 32767.0f;
        if (lrVoice.HasLiveVoice())
            lrVoice.SetGain(0, lfGain, &luSendName);

        // [DIAG] NOT IN THE X360 BINARY -- the POSITIVE CONTROL for the sting
        // lane: Create/Play only ARM the wrapper's stage machine, so a "-> PLAY"
        // line proves the request, not the sound. This fires once per slot per
        // transition into PLAYING, which is the state in which the playback voice
        // exists and is being mixed.
        if (liState != static_cast<s32>(mauLastVoiceState[liVoice]))
        {
            if (liState == CgsSound::Logic::VoiceWrapper::E_UPDATE_STAGE_PLAYING &&
                HudSoundDiagBudget(guHudDiagVoices))
            {
                HudSoundDiagPrintf(
                    "[hud-sound-voice] gui slot=%d now PLAYING (live=%d mixer=%u gain=%.4f)\n",
                    liVoice, lrVoice.HasLiveVoice() ? 1 : 0,
                    static_cast<u32>(mau8MixerOutputs[liVoice]),
                    static_cast<double>(lfGain));
            }
            mauLastVoiceState[liVoice] = static_cast<u8>(liState);
        }
    }

    for (s32 liVoice = 0; liVoice < E_NUM_GAME_MODE_VOICES; ++liVoice)
    {
        CustomHudVoice& lrHudVoice = maGameModeVoices[liVoice];
        lrHudVoice.mVoice.Update();

        const s32 liState = lrHudVoice.mVoice.GetState();
        if (liState == CgsSound::Logic::VoiceWrapper::E_UPDATE_STAGE_FINISHED ||
            liState == CgsSound::Logic::VoiceWrapper::E_UPDATE_STAGE_IDLE)
            continue;

        CGS_ASSERT(lrHudVoice.mu8MixerOutput <= 0x12,
                   "lHudVoice.mu8MixerOutput <= AttribSys::Enums::HudOutput::KI_MAX_VALUE");
        const f32 lfGain = GetMixerOutputValue(lrHudVoice.mu8MixerOutput,
                                               Nicotine::DMixIO::DMX_VOL) / 32767.0f;
        if (lrHudVoice.mVoice.HasLiveVoice())
        {
            lrHudVoice.mVoice.SetGain(0, lrHudVoice.mfClientVolume * lfGain, &luSendName);
            lrHudVoice.mVoice.SetParameter(0, lrHudVoice.mfClientPitch, &luSplicerPitchName);
        }
    }

    SetMixerInputValue(0, 0);

    mMusicStream.SetVolume(
        GetMixerOutputValue(mMusicStream.GetOutputSlot(),
                            Nicotine::DMixIO::DMX_VOL) / 32767.0f);
    mMusicStream.SetHighPassFreq(0.0f);
    mMusicStream.SetLowPassFreq(96000.0f);
}

// ---------------------------------------------------------------------------
// HUDEffect::IsReTrigger(const GuiAudioEvent*) const  @ 0x82686858
//
//   assert( lpAudioEvent );                                  ; cpp:371
//   lfSince = mfTimeSinceLastTrigger;
//   if ( lpAudioEvent->miComponentType == 1 )
//       return lfSince < 0.5 && lpAudioEvent->mHudMessageId.low
//                                 == mLastPlayedEvent.mHudMessageId.low;
//   return lfSince < 0.5
//       && lpAudioEvent->miComponentType         == mLastPlayedEvent.miComponentType
//       && lpAudioEvent->miAction                == mLastPlayedEvent.miAction
//       && lpAudioEvent->miAdditionalInformation == mLastPlayedEvent.miAdditionalInformation;
//
// The X360 compares only the LOW word of the CgsID in the type-1 arm
// (`a2[5] == *(a1 + 1220)`, i.e. the second word of each 8-byte id). Kept.
// ---------------------------------------------------------------------------
bool HUDEffect::IsReTrigger(const GuiAudioEventRecord* apAudioEvent) const
{
    CGS_ASSERT(apAudioEvent != 0, "lpAudioEvent");
    if (!apAudioEvent)
        return false;

    if (mfTimeSinceLastTrigger >= 0.5f)
        return false;

    if (apAudioEvent->miComponentType == 1)
    {
        return static_cast<u32>(apAudioEvent->mHudMessageId) ==
               static_cast<u32>(mLastPlayedEvent.mHudMessageId);
    }

    return apAudioEvent->miComponentType == mLastPlayedEvent.miComponentType &&
           apAudioEvent->miAction == mLastPlayedEvent.miAction &&
           apAudioEvent->miAdditionalInformation ==
               mLastPlayedEvent.miAdditionalInformation;
}

// ---------------------------------------------------------------------------
// HUDEffect::FindEventMapping(...)  @ 0x82686928  (DWARF cpp:402)
//
//   assert( lpAudioEvent );                                          ; cpp:422
//   assert( component.NumMappings() < 256, "Component reported N mappings,
//           which is too many." );                                   ; cpp:424
//   if ( lpAudioEvent->miAction != 0 ) return false;   ; only action 0 maps
//   for ( i = 0; i < component.NumMappings(); ++i )
//       if ( component.MessageIds(i).low == lpAudioEvent->mHudMessageId.low ) {
//           liFirst = component.SpliceIndices(i);
//           liLast  = component.LastSpliceIndices(i);
//           liRange = liLast - liFirst + 1;             ; twllei 0 / twlgei -1 ==
//                                                        ; the PPC divide-by-zero +
//                                                        ; overflow traps on `%`
//           ariSpliceIndex   = mau8RoundRobins[i] % liRange + liFirst;
//           ++mau8RoundRobins[i];
//           aru8MixerOutput  = component.MixerOutputs(i);
//           aru8ChokeGroup   = component.ChokeGroups(i);
//           return true;                                ; loop exits on the flag
//       }
//   return false;
//
// Like IsReTrigger, only the LOW word of each 8-byte MessageIds element is
// compared (`*(v24 + 4)`), against the GuiAudioEvent's own low word.
// ---------------------------------------------------------------------------
bool HUDEffect::FindEventMapping(const Attrib::Gen::presentationcomponent& arComponent,
                                 const GuiAudioEventRecord* apAudioEvent,
                                 s32& ariSpliceIndex, u8& aru8MixerOutput,
                                 u8& aru8ChokeGroup)
{
    CGS_ASSERT(apAudioEvent != 0, "lpAudioEvent");
    if (!apAudioEvent)
        return false;

    const u32 luNumMappings = arComponent.NumMappings();
    CGS_ASSERT(luNumMappings < static_cast<u32>(KI_MAX_MAPPINGS),
               "Component reported too many mappings.");

    if (apAudioEvent->miAction != 0)
        return false;

    const u32 luWantedId = static_cast<u32>(apAudioEvent->mHudMessageId);
    for (u32 luMapping = 0; luMapping < luNumMappings; ++luMapping)
    {
        if (static_cast<u32>(arComponent.MessageIds(luMapping)) != luWantedId)
            continue;

        const s32 liFirstSplice = static_cast<s32>(arComponent.SpliceIndices(luMapping));
        const s32 liLastSplice  = static_cast<s32>(arComponent.LastSpliceIndices(luMapping));
        const s32 liRange = liLastSplice - liFirstSplice + 1;
        if (liRange <= 0)
            return false;   // the console TRAPS here (twllei); a bad table is data-side

        ariSpliceIndex = (mau8RoundRobins[luMapping] % liRange) + liFirstSplice;
        ++mau8RoundRobins[luMapping];
        aru8MixerOutput = arComponent.MixerOutputs(luMapping);
        aru8ChokeGroup  = arComponent.ChokeGroups(luMapping);
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// HUDEffect::GetFreeVoice(u8)  @ 0x826D2530  (DWARF cpp:480)
//
//   liFree = -1;
//   for ( i = 0; i < 3; ++i ) {
//       lbBusy = ( state != FINISHED && state != IDLE );
//       if ( !lbBusy ) { liFree = i; continue; }
//       if ( au8ChokeGroup && mau8ChokeGroups[i] == au8ChokeGroup ) {
//           maVoices[i].Release();            ; CHOKE: steal the slot
//           if ( liFree == -1 ) liFree = i;
//       }
//   }
//   return liFree;
//
// The X360 flattens both arms into one
// `if (!v7 || a2 && ... && (Release(), v4 == -1))` so a free slot ALWAYS wins the
// assignment and a choked slot only claims it when nothing free was seen yet.
// ---------------------------------------------------------------------------
s32 HUDEffect::GetFreeVoice(u8 au8ChokeGroup)
{
    s32 liFreeVoice = -1;
    for (s32 liVoice = 0; liVoice < E_NUM_VOICES; ++liVoice)
    {
        const s32 liState = maVoices[liVoice].GetState();
        const bool lbBusy =
            liState != CgsSound::Logic::VoiceWrapper::E_UPDATE_STAGE_FINISHED &&
            liState != CgsSound::Logic::VoiceWrapper::E_UPDATE_STAGE_IDLE;

        if (!lbBusy)
        {
            liFreeVoice = liVoice;
            continue;
        }
        if (au8ChokeGroup != 0 && mau8ChokeGroups[liVoice] == au8ChokeGroup)
        {
            maVoices[liVoice].Release();
            if (liFreeVoice == -1)
                liFreeVoice = liVoice;
        }
    }
    return liFreeVoice;
}

// ---------------------------------------------------------------------------
// HUDEffect::Notify(const MessageHeader*)  @ 0x826F5F68  (DWARF cpp:234)
//
//   assert( lpMessageHeader );                                       ; cpp:256
//   assert( GetEventId() == E_SOUNDMESSAGE_GUIAUDIO_EVENT );          ; cpp:257
//   switch ( event.miComponentType ) {
//     case 1:   ; a HUD MESSAGE -- look the CgsID up in the mapping table
//       lbFound = FindEventMapping(mHudMessageData, &event, liSplice,
//                                  lu8MixerOutput, lu8ChokeGroup);
//       if ( event.miAction == 0 )  mLastReceivedHudMessage = event.mHudMessageId;
//       break;
//     case 2:   ; a PAUSE/GUI COMPONENT -- no table, a fixed two-splice pair
//       if ( (module dispatch slot 0 flag & 2) == 0 ) {
//           lu8ChokeGroup  = 0;
//           lu8MixerOutput = (u8)module dispatch slot 0;
//           liSplice       = event.miAction ? 2 : 1;
//           lbFound        = true;
//       }
//       break;
//   }
//   if ( !lbFound ) return;
//   if ( IsReTrigger(&event) ) return;                ; the 0.5 s guard
//   liFreeVoice = GetFreeVoice(lu8ChokeGroup);
//   if ( liFreeVoice == -1 ) return;
//   assert( liFreeVoice >= 0 );                                      ; cpp:315
//   assert( liFreeVoice < E_NUM_VOICES );                            ; cpp:316
//   mLastPlayedEvent        = event;          ; three ld/std pairs @0x826F61E8..
//   mfTimeSinceLastTrigger  = 0.0;
//   maVoices[liFreeVoice].Create({ module, 0, ~SplicerFactory::SK_NAME~,
//                                  SplicerVoiceSpec,
//                                  &stateManager->GetPresentationSpliceBank(), 0,
//                                  ~SplicerPlayerVoice::Slot~, Send01, submix 1,
//                                  0, 0, sendIndex 0 });
//   maVoices[liFreeVoice].Play(liSplice);
//   if ( live voice ) SetParameter(0 /* ~SplicerPlayerVoice::Pitch~ */, 1.0);
//   mau8MixerOutputs[liFreeVoice] = lu8MixerOutput;
//   mau8ChokeGroups[liFreeVoice]  = lu8ChokeGroup;
//   SetMixerInputValue(0, 0x7FFF);            ; claim the HUD mixer input
//
// The CreateParams stack image is @0x826F61C4..0x826F6270 and the splice bank is
// `*(mpState + 0x24) + 0xBC` == StateManager::mPresentationSpliceBank, the same
// Content the committed PresentationEffect::Play uses.
//
// The X360 also prints "[HUD] Received <id> at [t]." / "[HUD] Ignoring ... due to
// re-trigger." behind the console's own byte_82FFB8D0 debug flag; that flag's
// writer is not in this slice, so the equivalent information is carried by the
// opt-in [hud-sound-msg] witness instead (BRN_HUD_SOUND_DIAG=1).
// ---------------------------------------------------------------------------
void HUDEffect::Notify(const CgsSound::Io::MessageHeader* apMessageHeader)
{
    CGS_ASSERT(apMessageHeader != 0, "lpMessageHeader");
    if (!apMessageHeader)
        return;
    CGS_ASSERT(apMessageHeader->GetEventId() == 5,
               "lpMessageHeader->GetEventId() == E_SOUNDMESSAGE_GUIAUDIO_EVENT");
    if (apMessageHeader->GetEventId() != 5)
        return;

    const CgsSound::Io::Message<GuiAudioEventRecord>* lpMessage =
        static_cast<const CgsSound::Io::Message<GuiAudioEventRecord>*>(apMessageHeader);
    const GuiAudioEventRecord& lrEvent = lpMessage->mData;

    bool lbFound        = false;
    s32  liSpliceIndex  = 0;
    u8   lu8MixerOutput = 3;
    u8   lu8ChokeGroup  = 0;

    if (lrEvent.miComponentType == 1)
    {
        lbFound = FindEventMapping(mHudMessageData, &lrEvent, liSpliceIndex,
                                   lu8MixerOutput, lu8ChokeGroup);
        if (lrEvent.miAction == 0)
            mLastReceivedHudMessage = lrEvent.mHudMessageId;
    }
    else if (lrEvent.miComponentType == 2)
    {
        // The console reads the sound module's own dispatch-state block here
        // (module + 0x13570 / + 0x13574): bit 1 of the +4 byte gates the whole
        // arm (the loading-screen flag GUI event 33 maintains), and the first
        // word supplies the mixer output slot.
        const BrnSound::Module::SoundLogicModule* lpModule =
            static_cast<const BrnSound::Module::SoundLogicModule*>(GetLogicModule());
        if (lpModule && (lpModule->maDispatchState[0].mu8FlagAt4 & 2) == 0)
        {
            lu8ChokeGroup  = 0;
            lu8MixerOutput = static_cast<u8>(lpModule->maDispatchState[0].mu32Flags);
            liSpliceIndex  = (lrEvent.miAction != 0) ? 2 : 1;
            lbFound        = true;
        }
    }

    const bool lbDiag = HudSoundDiagBudget(guHudDiagMessages);
    if (!lbFound)
    {
        if (lbDiag)
        {
            HudSoundDiagPrintf(
                "[hud-sound-msg] type=%d action=%d info=%d id=0x%08X%08X -> NO MAPPING "
                "(NumMappings=%u)\n",
                lrEvent.miComponentType, lrEvent.miAction,
                lrEvent.miAdditionalInformation,
                static_cast<u32>(lrEvent.mHudMessageId >> 32),
                static_cast<u32>(lrEvent.mHudMessageId),
                mHudMessageData.HasData() ? mHudMessageData.NumMappings() : 0u);
        }
        return;
    }

    if (IsReTrigger(&lrEvent))
    {
        if (lbDiag)
        {
            HudSoundDiagPrintf(
                "[hud-sound-msg] type=%d action=%d info=%d id=0x%08X%08X -> RE-TRIGGER "
                "(%.3f s since last)\n",
                lrEvent.miComponentType, lrEvent.miAction,
                lrEvent.miAdditionalInformation,
                static_cast<u32>(lrEvent.mHudMessageId >> 32),
                static_cast<u32>(lrEvent.mHudMessageId),
                static_cast<double>(mfTimeSinceLastTrigger));
        }
        return;
    }

    const s32 liFreeVoice = GetFreeVoice(lu8ChokeGroup);
    if (liFreeVoice == -1)
    {
        if (lbDiag)
        {
            HudSoundDiagPrintf(
                "[hud-sound-msg] type=%d action=%d id=0x%08X%08X splice=%d -> NO FREE "
                "VOICE (choke=%u)\n",
                lrEvent.miComponentType, lrEvent.miAction,
                static_cast<u32>(lrEvent.mHudMessageId >> 32),
                static_cast<u32>(lrEvent.mHudMessageId),
                liSpliceIndex, static_cast<u32>(lu8ChokeGroup));
        }
        return;
    }
    CGS_ASSERT(liFreeVoice >= 0, "liFreeVoice >= 0");
    CGS_ASSERT(liFreeVoice < E_NUM_VOICES, "liFreeVoice < E_NUM_VOICES");

    mLastPlayedEvent       = lrEvent;
    mfTimeSinceLastTrigger = 0.0f;

    CgsSound::Logic::VoiceWrapper::CreateParams lParams;
    BuildSplicerVoiceParams(lParams);

    maVoices[liFreeVoice].Create(lParams);
    maVoices[liFreeVoice].Play(static_cast<u32>(liSpliceIndex));
    if (maVoices[liFreeVoice].HasLiveVoice())
    {
        const u32 luSplicerPitchName = static_cast<u32>(
            CgsSound::Playback::Name::MakeHash("~SplicerPlayerVoice::Pitch~"));
        maVoices[liFreeVoice].SetParameter(0, 1.0f, &luSplicerPitchName);
    }
    mau8MixerOutputs[liFreeVoice] = lu8MixerOutput;
    mau8ChokeGroups[liFreeVoice]  = lu8ChokeGroup;
    SetMixerInputValue(0, 0x7FFF);

    if (lbDiag)
    {
        HudSoundDiagPrintf(
            "[hud-sound-msg] type=%d action=%d info=%d id=0x%08X%08X -> PLAY splice=%d "
            "slot=%d mixer=%u choke=%u bank=%d\n",
            lrEvent.miComponentType, lrEvent.miAction,
            lrEvent.miAdditionalInformation,
            static_cast<u32>(lrEvent.mHudMessageId >> 32),
            static_cast<u32>(lrEvent.mHudMessageId),
            liSpliceIndex, liFreeVoice, static_cast<u32>(lu8MixerOutput),
            static_cast<u32>(lu8ChokeGroup), lParams.mpContent ? 1 : 0);
    }
}

// ---------------------------------------------------------------------------
// The splicer-voice CreateParams both voice lanes build (the X360 emits the same
// twelve stack stores in Notify @0x826F61C4 and PlaySound @0x826F6338; the
// factory / spec / slot / send names and the presentation splice bank are
// identical, only the target wrapper differs).
// ---------------------------------------------------------------------------
void HUDEffect::BuildSplicerVoiceParams(CgsSound::Logic::VoiceWrapper::CreateParams& arParams)
{
    GlobalStateManager* lpGlobal = static_cast<GlobalStateManager*>(
        GetStateBase() ? GetStateBase()->GetStateManager() : 0);

    arParams.Clear();
    arParams.mpLogicModule = GetLogicModule();
    arParams.mFactoryName  = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("~SplicerFactory::SK_NAME~"));
    arParams.mVoiceSpecName = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("SplicerVoiceSpec"));
    arParams.mpContent       = lpGlobal ? &lpGlobal->GetPresentationSpliceBank() : 0;
    arParams.mContentSpecName = 0;
    arParams.mSlotName = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("~SplicerPlayerVoice::Slot~"));
    arParams.mSendName = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("Send01"));
    arParams.mSubMixVoiceID = 1;
    arParams.miSendIndex    = 0;
}

// ---------------------------------------------------------------------------
// HUDEffect::PlaySound(const SampleTag&, u8)  @ 0x826F6338  (DWARF cpp:911)
//
// The GAME-MODE lane's voice start (score ticks, countdown, stunt/showtime
// stings). Picks a maGameModeVoices[] slot whose wrapper state is FINISHED or
// IDLE, creates the same splicer voice Notify does, plays the tag's sample
// index, and stamps the slot's client volume/pitch/mixer output:
//   liSlot = -1;
//   for i in 0..3: if ( state == FINISHED || state == IDLE ) liSlot = i;
//   if ( liSlot == -1 ) return;                ; every game-mode voice busy
//   maGameModeVoices[liSlot].mVoice.Create({...same params as Notify...});
//   maGameModeVoices[liSlot].mVoice.Play(arTag.miSampleIndex);
//   maGameModeVoices[liSlot].mfClientVolume = arTag.mfVolume;
//   maGameModeVoices[liSlot].mfClientPitch  = 1.0;
//   maGameModeVoices[liSlot].mu8MixerOutput = au8MixerOutput;
//
// The X360's slot scan keeps the LAST free slot it sees (it assigns without
// breaking out), so a run of free slots picks the highest index. Kept verbatim.
// ---------------------------------------------------------------------------
void HUDEffect::PlaySound(const BrnEffectObject::SampleTag& arTag, u8 au8MixerOutput)
{
    s32 liSlot = -1;
    for (s32 liVoice = 0; liVoice < E_NUM_GAME_MODE_VOICES; ++liVoice)
    {
        const s32 liState = maGameModeVoices[liVoice].mVoice.GetState();
        if (liState == CgsSound::Logic::VoiceWrapper::E_UPDATE_STAGE_FINISHED ||
            liState == CgsSound::Logic::VoiceWrapper::E_UPDATE_STAGE_IDLE)
            liSlot = liVoice;
    }
    if (liSlot == -1)
        return;

    CgsSound::Logic::VoiceWrapper::CreateParams lParams;
    BuildSplicerVoiceParams(lParams);

    CustomHudVoice& lrHudVoice = maGameModeVoices[liSlot];
    lrHudVoice.mVoice.Create(lParams);
    lrHudVoice.mVoice.Play(static_cast<u32>(arTag.miSampleIndex));
    lrHudVoice.mfClientVolume = arTag.mfVolume;
    lrHudVoice.mfClientPitch  = 1.0f;
    lrHudVoice.mu8MixerOutput = au8MixerOutput;

    if (HudSoundDiagBudget(guHudDiagPlays))
    {
        HudSoundDiagPrintf(
            "[hud-sound-play] game-mode voice slot=%d sample=%d vol=%.3f mixer=%u\n",
            liSlot, static_cast<s32>(arTag.miSampleIndex),
            static_cast<double>(arTag.mfVolume),
            static_cast<u32>(au8MixerOutput));
    }
}

} // namespace Logic
} // namespace BrnSound
