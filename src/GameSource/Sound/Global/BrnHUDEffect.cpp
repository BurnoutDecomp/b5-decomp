#include "GameSource/Sound/Global/BrnHUDEffect.h"
#include "GameSource/Sound/BrnDebugComponent.h"
#include "GameShared/GameClasses/Core/CgsID.h"
// ---- the game-mode HUD audio lane's callees (2026-09-16) -------------------
#include "GameSource/AttribSys/Generated/classes/sampletags.h"   // FirstIndices/LastIndices/Volumes
#include "GameShared/GameClasses/Numeric/CgsBranchlessOperations.h"   // CgsSound::Utils::IntClamp
#include "GameSource/Director/Camera/BrnCameraState.h"          // CameraState::E_FLAG_TAKEDOWN_CAMERA
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"              // CGS_ASSERT
#include "GameShared/GameClasses/Sound/IO/CgsMessage.h"         // CgsSound::Io::Message / MessageHeader
#include "GameShared/GameClasses/Sound/Logic/CgsState.h"        // State -> StateManager walk
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"    // Name::MakeHash / PlayerVoice slot names
#include "GameShared/GameClasses/Sound/Playback/CgsVoice.h"
#include "GameSource/Sound/Global/BrnGlobalStateManager.h"      // GetPresentationSpliceBank
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameSource/Sound/Streaming/BrnStreamingStateManager.h"  // the HUD stream's manager
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
bool KB_HUD_SPEW_MESSAGES = false; // ARTIST82FFB8D0, DecFIGS BrnHUDEffect.cpp:27

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
// so they are runtime-initialised FILE-SCOPE GLOBALS, not function-local constants).
// BOTH VALUES ARE RECOVERED FROM THEIR CRT INIT THUNKS, which sit next to each other:
//   * flt_830060C0, thunk @0x82C62DB8: XMVectorCos(flt_8209D74C == 0.346f) == 0.9407368f,
//     the cosine of the ~19.8 degree slipstream cone half-angle.
//   * flt_830083E4, thunk @0x82C62E08 (`lfs 1.0 ; lfs flt_830060C0 ; fsubs ; fdivs ; stfs`):
//     1.0f / (1.0f - flt_830060C0) == 16.87f. It rescales the alignment term, which can
//     only reach (1 - cos) on its own, back onto [0,1].
// The second initialiser reads the first, so their definition order here is load-bearing.
static f32 gfSlipstreamConeCos = 0.9407368f;                          // flt_830060C0
static f32 gfSlipstreamScale   = 1.0f / (1.0f - gfSlipstreamConeCos); // flt_830083E4

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
// unused; the three Vector3 args arrive in vector registers v1/v2/v3, loaded by the only
// caller, UpdateSlipStreaming @0x82701F0C:
//   v1 = the player's position (GetPlayerPosition),
//   v2 = the car in front's position + 2.0 * its unit velocity,
//   v3 = the negated unit velocity of the car in front (vxor128 sign-flip).
// So the result grows as the player sits straight behind that car, in its direction of
// travel.
//
// ASM (0x82686BA8-0x82686C5C), store-for-store:
//   offset    = v1 - v2                                    ; vsubfp
//   distSq    = Dot3(offset, offset)                        ; vmsum3fp128
//   behind    = Dot3(v3, offset)                            ; vmsum3fp128
//   invDist   = 1/sqrt(distSq)                              ; vrsqrtefp + 1 NR step
//   t1 = 1.0 - (behind - 25.0) * (1/15), saturate01         ; two fsel
//   unitOffset = offset * invDist                           ; normalize
//   aligned    = Dot3(unitOffset, v3)                       ; vmsum3fp128
//   t2 = saturate01(aligned - gfSlipstreamConeCos)          ; two fsel
//   return (t2 * gfSlipstreamScale) * t1
//
// FLAG (VMX->portable): vrsqrtefp + 1 NR refine collapsed to exact 1.0f/std::sqrt
//   (project convention, matches the BrnMathUtils Magnitude family).
// FLAG (fsel-faithful clamps): both saturate pairs reproduce the PPC
//   `fsel(a,b,c) == (a >= 0.0f) ? b : c` sense exactly (NOT std::clamp), which also keeps
//   the console's answer for a NaN input (a stationary car in front): the fsel takes c.
// ---------------------------------------------------------------------------
f64 HUDEffect::GetSlipstreamAmount( Vector3 lPlayerPosition,
                                    Vector3 lSlipstreamOrigin,
                                    Vector3 lNegatedDirection )
{
    const Vector3 lOffset = lPlayerPosition - lSlipstreamOrigin;                        // v0 = v1 - v2

    const f32 lfDistSq = rw::math::vpu::Dot(lOffset, lOffset);                          // vmsum3fp128 v13,v0,v0
    const f32 lfBehind = rw::math::vpu::Dot(lNegatedDirection, lOffset);                // vmsum3fp128 v10,v3,v0

    // vrsqrtefp + 1 Newton-Raphson refine -> exact reciprocal-sqrt (see FLAG).
    const f32 lfInvDist = 1.0f / std::sqrt(lfDistSq);

    // Full weight up to 25 m behind the car, fading to nothing at 40 m (two fsel).
    f32 lfDistanceFactor = 1.0f - (lfBehind - 25.0f) * 0.06666667f;
    lfDistanceFactor = ((-lfDistanceFactor) >= 0.0f) ? 0.0f : lfDistanceFactor;        // fsel: lower clamp -> 0
    lfDistanceFactor = ((1.0f - lfDistanceFactor) >= 0.0f) ? lfDistanceFactor : 1.0f;  // fsel: upper clamp -> 1

    const Vector3 lUnitOffset = lOffset * lfInvDist;                                    // vmulfp128 v0,v0,v13
    const f32 lfAligned = rw::math::vpu::Dot(lUnitOffset, lNegatedDirection);           // vmsum3fp128 v0,v0,v3

    // How far inside the cone the player sits (two fsel).
    f32 lfAlignmentFactor = lfAligned - gfSlipstreamConeCos;
    lfAlignmentFactor = ((-lfAlignmentFactor) >= 0.0f) ? 0.0f : lfAlignmentFactor;        // fsel: lower clamp -> 0
    lfAlignmentFactor = ((1.0f - lfAlignmentFactor) >= 0.0f) ? lfAlignmentFactor : 1.0f;  // fsel: upper clamp -> 1

    return static_cast<f64>((lfAlignmentFactor * gfSlipstreamScale) * lfDistanceFactor);
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
    u32 guHudDiagModes = 0;
    u32 guHudDiagCues = 0;
    s32 giHudDiagLastMode = -2;   // -2: nothing seen yet (-1 is E_MODE_NONE)
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
//   the mSlipstreamAmount Average block (a1+1020..a1+1064 == this+0x400) zeroed
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

    // The X360 writes the HUD music-stream block field by field here, priority 2 among
    // them, and fills the stream's CreateParams later, on every MusicStream::Queue
    // @0x8269CB88 (module, the shared factory and slot names, "MusicFiltVoiceSpec",
    // "Send01", the content spec), and it hands the streaming manager to every
    // MusicStream::Update as an argument (*(module + 0x296C), UpdateSlipStreaming
    // @0x82701D68).
    // Our MusicStream keeps those fixed values from Prepare on (the MusicEffect streams do too), so
    // they are stored here rather than on every Queue. The values and their use are the console's.
    mMusicStream.Prepare(lpModule,
                         static_cast<Streaming::StreamingStateManager*>(
                             lpModule->GetEnvironment().GetStateManager(6)),
                         "MusicFiltVoiceSpec", 2);
    mMusicStream.ResetVoiceStages();
    mMusicStream.SetVolume(0.0f);
    mMusicStream.SetHighPassFreq(0.0f);
    mMusicStream.SetLowPassFreq(96000.0f);
    mMusicStream.StopAndUnqueue(0.0f);

    // this+0x400 (`addi r9, r31, 0x3FC`, r31 == this+4), the slipstream mean. This used to
    // zero mGameModeData.mEventScoreDelta (+0x430), which X360 Attach does not touch.
    mSlipstreamAmount.mfAverage = 0.0f;
    for (u32 luPoint = 0; luPoint < 10u; ++luPoint)
        mSlipstreamAmount.maPoints[luPoint] = 0.0f;
    mSlipstreamAmount.muNextPoint = 0;

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
// HUDEffect::UpdateParams(f32)   @ 0x827037E0
//
// ⭐⭐⭐ CORRECTED 2026-09-16 -- THE X360 BODY *IS* IN THE IMAGE, AND IT IS THREE
// STATEMENTS, NOT ONE. The note that used to stand here said "the X360 body for
// this one function is not in the export set" and derived the accumulate from
// IsReTrigger's invariant. It is in the image; it is simply UNNAMED in the
// ledger, which is the documented "missing body == a NAME search failing" trap.
// It sits at 0x827037E0, between TrafficStateManager::UpdateParams and
// CollisionStateManager::GetRandomSampleID, and it reads:
//     827037F8  addi r30, r31, -4          ; r31 is the EffectBase sub-object,
//     82703800  lfs  f0, 0x4CC(r31)        ;   so 0x4CC(r31) == this+0x4D0 ==
//     82703804  fadds f0, f1, f0           ;   mfTimeSinceLastTrigger
//     82703808  stfs f0, 0x4CC(r31)
//     8270380C  bl   0x82701BA8            ; HUDEffect::UpdateSlipStreaming
//     82703814  lwz  r4, 0x28(r31)         ; this+0x2C == mpLogicModule
//     82703818  bl   0x82702670            ; HUDEffect::UpdateGameModeHud
//
// This is the ONLY entry point the whole HUD-audio lane has: UpdateGameModeHud
// @0x82702670 dispatches on the live game mode to UpdateShowtime @0x82701830
// (modes 2/16), UpdateRoadRage @0x82701AA0 (mode 3), UpdateEventTimeRemaining @0x826FE400
// (mode 5) and UpdateStuntRun @0x82701598 (mode 7); UpdateSlipStreaming @0x82701BA8 is the
// slipstream whoosh. All of them are bodied in this file.
//
// Settled facts the bodies rely on: mGameModeData's members line up one-for-one with the
// console's a1+1072.. cursors (mEventScoreDelta.mfAverage at +0x2C, mEventTimeRemaining at
// +0x30, mEventBoostAmount at +0x38, mfTimeSinceScoreTick at +0x40, mStuntScore at +0x44,
// mStuntResultScore at +0x54, mShowtimeScore at +0x5C, mShowtimeBoostDelta at +0x64,
// mbTimeExtended at +0x6C), and the showtime / stunt thresholds are .rdata
// 0x82F2CE48/4C/50 == 3.5 / 1.5 / 1.0 with the re-trigger gate 0x82F2CE54 == 0.379 s.
// RootInputBuffer::GuiAudioEventResults is EIGHT bytes, { s32 miStuntResultScore @+0x00;
// f32 mfShowtimeScore @+0x04 }, which is what RootInputBuffer::Construct writes (0 as a u32
// at +0x13730, flt_82001CC0 at +0x13734) and what these arms read. The mode-5 arm's
// arguments: UpdateGameModeHud @0x827027F0 gates on the scoring interface's byte at +0xAA8
// and passes (scoring[+0xA98] - scoring[+0xA90], 10.0f, tag 21, mixer output 3).
// ---------------------------------------------------------------------------
void HUDEffect::UpdateParams(f32 af32DeltaTime)
{
    mfTimeSinceLastTrigger += af32DeltaTime;
    UpdateSlipStreaming(af32DeltaTime);                                         // @0x8270380C
    UpdateGameModeHud(static_cast<Module::SoundLogicModule*>(mpLogicModule));   // @0x82703818
}

// ---------------------------------------------------------------------------
// The GAME-MODE HUD AUDIO LANE  (2026-09-16)
//
// HUDEffect::UpdateParams @0x827037E0 is three statements -- accumulate, UpdateSlipStreaming,
// UpdateGameModeHud -- and is unnamed in the ledger, so a name search for it comes back empty.
//
// The console's dispatch is UpdateGameModeHud @0x82702670: it takes the live game mode from
// the input buffer's GameModeOutputInterface, resets the per-mode block when the mode
// changes, and hands off to one updater per scoring mode. Every cue these play goes through
// PlaySound(ePresentationSampleTags, u8), i.e. the presentation splice bank.
//
// THE SCORING INTERFACE IS AN OPAQUE BLOB IN THIS TREE (u8 mData[0xAB0], the exact bytes
// SetScoringInterface XMemCpy's), so the five fields these bodies read are reached through
// the file-local cursor below, at the console's own offsets. It is a fixed-width POD block
// -- no member widens on the host -- so the offsets stay valid; this is the same
// external-record boundary the committed GUI states use over their opaque event payloads.
// ---------------------------------------------------------------------------
namespace
{
    // The five ScoringOutputInterface fields the HUD audio reads, at the offsets the X360
    // bodies load them from. Named for what their readers do with them.
    struct ScoringCursor
    {
        const u8* mpBase;

        s32 Word(u32 luOffset) const
        {
            return *reinterpret_cast<const s32*>(mpBase + luOffset);
        }
        f32 Float(u32 luOffset) const
        {
            return *reinterpret_cast<const f32*>(mpBase + luOffset);
        }

        // UpdateStuntRun @0x82701598
        s32 StuntComboMultiplier() const { return Word(0xA70u); }   // *(a2 + 2672)
        s32 StuntRunSentinel()     const { return Word(0xA7Cu); }   // *(a2 + 2684), -1 == idle
        s32 StuntScore()           const { return Word(0xA80u); }   // *(a2 + 2688)

        // The event clock. ⭐ These four are NAMED on the producer side -- the same words
        // are GameStateModuleIO::ScoringOutputInterface's mfModeTimeElapsed (+0xA90),
        // mfModeTimeRemaining (+0xA94, which BrnGameStateModule.cpp calls "THE number the
        // stunt-run HUD clock counts down"), mfCurrentTargetModeTime (+0xA98) and
        // mbTimerActive (+0xAA8), written by GameStateModule::CopyScoringDataToOutput
        // behind ScoringSystem::IsTimeLimitActive(). The SOUND side keeps the block opaque
        // on purpose, hence this cursor; the offsets are the producer's own.
        // UpdateGameModeHud's mode-5 arm takes the remaining time as (duration - elapsed)
        // @0x82702800; UpdateRoadRage and UpdateStuntRun read +0xA94 directly.
        f32 EventElapsed()   const { return Float(0xA90u); }        // v6[676]
        f32 TimeRemaining()  const { return Float(0xA94u); }        // *(a2 + 2708)
        f32 EventDuration()  const { return Float(0xA98u); }        // v6[678]
        bool IsTimedEvent()  const { return mpBase[0xAA8u] != 0; }  // *(v6 + 2728)
    };

    // .rdata: the three showtime/stunt score-rate thresholds and the re-trigger gate, plus
    // the boost-earn threshold and the event-timer window.
    const f32 KF_SCORE_RATE_FAST      = 3.5f;    // flt_82F2CE48
    const f32 KF_SCORE_RATE_MEDIUM    = 1.5f;    // flt_82F2CE4C
    const f32 KF_SCORE_RATE_SLOW      = 1.0f;    // flt_82F2CE50
    const f32 KF_SCORE_TICK_GAP       = 0.379f;  // flt_82F2CE54 -- seconds between ticks
    const f32 KF_BOOST_EARN_THRESHOLD = 0.1f;    // flt_82F2CE58
    const f32 KF_EVENT_TIMER_WINDOW   = 10.0f;   // flt_82004A20 -- the countdown window

    // The X360 compares the previous boost delta against +/-FLT_EPSILON to mean "was zero".
    const f32 KF_EPSILON = 1.1920929e-7f;

    // The HUD message CgsID UpdateStuntRun consumes for the multiplier sting. The X360
    // stages it as a 64-bit immediate (0x82701740: lis 0x5AB7 / ori 0xC340, lis 0xB939 /
    // ori 0x0CF4, rldimi), compares it against mLastReceivedHudMessage and CLEARS that
    // member on a hit so the sting fires once per message.
    const CgsID KU_HUD_MESSAGE_STUNT_MULTIPLIER = 0xB9390CF45AB7C340ull;

    // The scoring modes this lane answers, as GameStateModuleIO::EGameModeType values.
    // The sound-side GameModeOutputInterface deliberately holds the word as an s32 so that
    // header stands alone, so the four the console switches on are named here.
    const s32 KI_MODE_OFFLINE_SHOWTIME = 2;    // E_MODE_OFFLINE_SHOWTIME
    const s32 KI_MODE_ROAD_RAGE        = 3;    // E_MODE_ROAD_RAGE
    const s32 KI_MODE_BURNING_ROUTE    = 5;    // E_MODE_BURNING_ROUTE -- the timed event
    const s32 KI_MODE_STUNT_ATTACK     = 7;    // E_MODE_STUNT_ATTACK
    const s32 KI_MODE_ONLINE_SHOWTIME  = 16;   // E_MODE_ONLINE_SHOWTIME

    // The three modes the slipstream runs in. 17 is spelled E_MODE_ONLINE_MODE_END /
    // E_MODE_COUNT in the shared enum, but it is a real dispatchable mode (the enum's own
    // aliasing is explained at ModeManager::KI_GAME_MODE_SLOTS).
    const s32 KI_MODE_ONLINE_FUGITIVE  = 12;   // E_MODE_ONLINE_FUGITIVE
    const s32 KI_MODE_ONLINE_FREE_BURN = 14;   // E_MODE_ONLINE_FREE_BURN
    const s32 KI_MODE_ONLINE_MODE_END  = 17;   // E_MODE_ONLINE_MODE_END

    // X360 tests this inline, twice, as `14 || 12 || 17` (0x82701C74, 0x82701D70).
    inline bool IsSlipstreamGameMode(s32 aiMode)
    {
        return aiMode == KI_MODE_ONLINE_FREE_BURN || aiMode == KI_MODE_ONLINE_FUGITIVE ||
               aiMode == KI_MODE_ONLINE_MODE_END;
    }

    // [DIAG] NOT IN THE X360 BINARY -- what the [hud-slip] witness last printed.
    u32 guHudDiagSlip = 0;
    s32 giHudDiagSlipKey = -1;

    // The mixer outputs each lane plays on (X360 `li r5/r7, N` at the call sites).
    const u8 KU8_MIXER_OUTPUT_ROAD_RAGE = 3;
    const u8 KU8_MIXER_OUTPUT_STUNT     = 17;
    const u8 KU8_MIXER_OUTPUT_SHOWTIME  = 18;
    const u8 KU8_MIXER_OUTPUT_SLIPSTREAM = 17;   // X360 `li r6/r5, 0x11` @0x82701CB8 / 0x82701CCC
}

// ---- PlaySound(ePresentationSampleTags, u8)   @ 0x826FE540 ----------------------
// The splice-bank overload: resolve one sample out of the presentation bank (tag set 4)
// with the round-robin cursor, then hand it to the SampleTag overload.
// ⚠️ THE CURSOR ADVANCES WHETHER OR NOT THE LOOKUP SUCCEEDED -- the X360 increments
// this+0x4D4 between the call and the test (`bl GetSampleTag ; ++*(a1+1236) ; if (result)`).
void HUDEffect::PlaySound(ePresentationSampleTags aeTag, u8 au8MixerOutput)
{
    BrnEffectObject::SampleTag lTag;
    lTag.mfVolume      = 0.0f;
    lTag.miSampleIndex = 0;

    const bool lbResolved = GetSampleTag(4u, static_cast<u32>(aeTag), muRoundRobin, lTag);
    ++muRoundRobin;

    if (lbResolved)
    {
        // [DIAG] NOT IN THE X360 BINARY -- the cue witness (BRN_HUD_SOUND_DIAG=1).
        if (HudSoundDiagBudget(guHudDiagCues))
        {
            HudSoundDiagPrintf("[hud-cue] splice tag %d -> mixer out %u (sample %d)\n",
                               static_cast<s32>(aeTag), static_cast<u32>(au8MixerOutput),
                               static_cast<s32>(lTag.miSampleIndex));
        }
        PlaySound(lTag, au8MixerOutput);
    }
}

// ---- UpdateEventTimeRemaining   @ 0x826FE400 ------------------------------------
// The shared countdown cue: tick once per whole second as the clock falls through the last
// afWindow seconds. The X360 floors the time with an `fsel` pair rather than a call, and
// that floor is the whole trick -- mEventTimeRemaining holds the FLOORED value, so
// "current < previous" is true exactly once per second rather than every frame.
//
// The sample INDEX is the second itself, clamped into the tag's own first/last range, so a
// bank with N samples counts down the last N seconds and repeats the last one below that.
void HUDEffect::UpdateEventTimeRemaining(f32 afTimeRemaining, f32 afWindow,
                                         ePresentationSampleTags aeTag, u8 au8MixerOutput)
{
    mGameModeData.mEventTimeRemaining.mPreviousValue =
        mGameModeData.mEventTimeRemaining.mCurrentValue;

    // X360 0x826FE41C..0x826FE438: the branchless round-to-integer idiom (`fsel` picks the
    // round magic for a non-negative input and 0.0 otherwise, then `x - magic + magic`, then
    // a second `fsel` corrects a round-up back down). For a NON-NEGATIVE input that is a
    // floor; for a negative one the first fsel selects 0.0, so the value passes through
    // unrounded. Both halves are spelled out rather than replaced with std::floor, because
    // std::floor would turn a negative clock into the NEXT whole second down and change
    // which frame the "current < previous" edge lands on.
    const f32 lfFloored = (afTimeRemaining >= 0.0f)
        ? static_cast<f32>(static_cast<s32>(afTimeRemaining))
        : afTimeRemaining;
    mGameModeData.mEventTimeRemaining.mCurrentValue = lfFloored;

    const f32 lfPrevious = mGameModeData.mEventTimeRemaining.mPreviousValue;
    if (lfFloored >= 0.0f && lfFloored <= afWindow &&
        lfFloored < lfPrevious && (lfFloored - lfPrevious) > -2.0f)
    {
        Attrib::Gen::sampletags lTags(
            const_cast<Attrib::Collection*>(
                const_cast<Attrib::RefSpec&>(
                    static_cast<Module::SoundLogicModule*>(mpLogicModule)
                        ->GetSampleTags(4u)).GetCollection()), 0);

        const u32 luTag   = static_cast<u32>(aeTag);
        const s16 li16First = *static_cast<const s16*>(lTags.FirstIndices(luTag));
        const s16 li16Last  = lTags.LastIndices(luTag);

        BrnEffectObject::SampleTag lSampleTag;
        lSampleTag.mfVolume = *static_cast<const f32*>(lTags.Volumes(luTag));
        lSampleTag.miSampleIndex = static_cast<s16>(CgsSound::Utils::IntClamp(
            static_cast<s32>(lfFloored) + li16First, li16First, li16Last));

        PlaySound(lSampleTag, au8MixerOutput);
    }
}

// ---- UpdateSlipStreaming   @ 0x82701BA8 ------------------------------------------
// The convoy slipstream: a looping "SlipStream" stream while game action 16 says the player
// is in a convoy, plus a mixer input carrying how squarely the player sits behind the car
// in front. The module's mu8FlagAt1 / mi32At4 are that action's "in convoy" one-shot and the
// active race-car index of the car in front (InProgressStuntAction::miCarInFrontIndex).
//
// The console's stack frame is 0x29F0 bytes because it COPIES the whole vehicle interface
// (RCEntityActiveRaceCarOutputInterface's copy ctor @0x826BB258, into var_2970) and queries
// the copy. Nothing writes to it and it does not outlive the function, so the buffer's own
// interface is read directly, as UpdateShowtime @0x82701830 does.
void HUDEffect::UpdateSlipStreaming(f32 afDeltaTime)
{
    Module::SoundLogicModule* lpModule = static_cast<Module::SoundLogicModule*>(mpLogicModule);
    Module::Io::RootInputBuffer* lpInputBuffer = lpModule->GetBrnInputStructure();
    CGS_ASSERT(lpInputBuffer, "mpBrnLogicInputBuffer");

    const s32 liGameMode = lpInputBuffer->GetGameModeInterface()->miCurrentGameModeType;
    const u8  lu8InConvoy = lpModule->mDispatchState.mu8FlagAt1;

    // Note the two arms are NOT an if/else on one test: in a convoy with the stream already
    // up, or outside the three modes, neither runs.
    if (lu8InConvoy == 1)
    {
        if (!mMusicStream.IsPlayingOrQueued() && IsSlipstreamGameMode(liGameMode))
        {
            const u32 luSlipStream = static_cast<u32>(
                CgsSound::Playback::Name::MakeHash("SlipStream"));
            mMusicStream.Queue(luSlipStream, KU8_MIXER_OUTPUT_SLIPSTREAM);
            PlaySound(E_SPLICE_SLIPSTREAM, KU8_MIXER_OUTPUT_SLIPSTREAM);
        }
    }
    else if (lu8InConvoy == 0 && mMusicStream.IsPlayingOrQueued())
    {
        mMusicStream.StopAndUnqueue(1.0f);   // flt_82001C98, the fold @0x82701D20..0x82701D58
    }

    mMusicStream.Update(afDeltaTime);

    // [DIAG] NOT IN THE X360 BINARY -- BRN_HUD_SOUND_DIAG=1, one line per CHANGE of what
    // the gates saw, so a whole run costs a handful of lines.
    if (HudSoundDiagEnabled())
    {
        const s32 liKey = ((liGameMode + 1) << 16) | (static_cast<s32>(lu8InConvoy) << 12) |
                          ((lpModule->mDispatchState.mi32At4 + 1) << 4) |
                          static_cast<s32>(mMusicStream.GetState());
        if (liKey != giHudDiagSlipKey && HudSoundDiagBudget(guHudDiagSlip))
        {
            giHudDiagSlipKey = liKey;
            HudSoundDiagPrintf(
                "[hud-slip] mode %d gated=%d convoy=%u carInFront=%d stream=%d out=%u mean=%.4f\n",
                liGameMode, IsSlipstreamGameMode(liGameMode) ? 1 : 0,
                static_cast<u32>(lu8InConvoy), lpModule->mDispatchState.mi32At4,
                static_cast<s32>(mMusicStream.GetState()),
                static_cast<u32>(mMusicStream.GetOutputSlot()),
                static_cast<double>(mSlipstreamAmount.GetAverage()));
        }
    }

    if (!IsSlipstreamGameMode(liGameMode))
    {
        return;
    }

    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpVehicle =
        lpModule->GetBrnInputStructure()->GetVehicleInterface();
    const s32 liCarInFront = lpModule->mDispatchState.mi32At4;
    const EActiveRaceCarIndex leCarInFront = static_cast<EActiveRaceCarIndex>(liCarInFront);

    f32 lfAmount = 0.0f;
    if (liCarInFront != -1 && lpVehicle->IsPlayerCarActive() &&
        lpVehicle->IsRaceCarActive(leCarInFront) &&
        !lpVehicle->GetPlayerRaceCarState()->mbCrashing)
    {
        CGS_ASSERT(liCarInFront < 8, "Invalid RaceCarIndex");   // cpp:888
        const BrnPhysics::Vehicle::RaceCarState* lpCarInFront =
            lpVehicle->GetRaceCarState(leCarInFront);
        CGS_ASSERT(lpCarInFront,
                   "Cannot find the RaceCarState for the Car in front of the player in the convoy");   // cpp:890

        // X360 0x82701EAC..0x82701EFC: normalise the velocity (vrsqrtefp + 1 NR step, exact
        // reciprocal-sqrt here as in GetSlipstreamAmount above), then push the car's position
        // 2.0 along it with vmaddfp128. 0x82701F04..14 flips all four sign bits for v3.
        const f32 lfSpeedSq = rw::math::vpu::Dot(lpCarInFront->mLinearVelocity,
                                                 lpCarInFront->mLinearVelocity);
        const Vector3 lDirection = lpCarInFront->mLinearVelocity * (1.0f / std::sqrt(lfSpeedSq));
        const Vector3 lOrigin    = lDirection * 2.0f + lpCarInFront->mTransform.Pos();

        lfAmount = static_cast<f32>(
            GetSlipstreamAmount(lpVehicle->GetPlayerPosition(), lOrigin, -lDirection));
    }
    mSlipstreamAmount.Record(lfAmount);

    // subfic/subfe/clrlwi 17 @0x82701F3C: any non-zero byte -> 0x7FFF.
    SetMixerInputValue(1, lpModule->mDispatchState.mu8FlagAt1 != 0 ? 0x7FFF : 0);
    SetMixerInputValue(2, static_cast<s32>(mSlipstreamAmount.GetAverage() * 32767.0f));   // flt_820AD310, fctiwz
}

// ---- UpdateShowtime   @ 0x82701830 ----------------------------------------------
// Showtime's two cues: a score TICK whose urgency comes from the averaged score rate, and a
// one-shot when the run earns a chunk of boost.
void HUDEffect::UpdateShowtime()
{
    Module::SoundLogicModule* lpModule =
        static_cast<Module::SoundLogicModule*>(mpLogicModule);
    Module::Io::RootInputBuffer* lpInputBuffer = lpModule->GetBrnInputStructure();
    CGS_ASSERT(lpInputBuffer, "lpInputBuffer");   // cpp:718

    // The player's live boost bar, or zero when there is no player car this frame.
    f32 lfBoostAmount = 0.0f;
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpVehicle =
        lpInputBuffer->GetVehicleInterface();
    if (lpVehicle->IsPlayerCarActive())
    {
        const EActiveRaceCarIndex leIndex = lpVehicle->GetPlayerActiveRaceCarIndex();
        const BrnWorld::RaceCarEntityModuleIO::BoostOutputInfo* lpBoost =
            lpVehicle->GetBoostOutputInfoN(leIndex);
        if (lpBoost)
        {
            lfBoostAmount = lpBoost->mfBoostAmount;
        }
    }

    const Module::Io::RootInputBuffer::GuiAudioEventResults* lpResults =
        lpInputBuffer->GetGuiAudioEventResults();

    const f32 lfPreviousScore = mGameModeData.mShowtimeScore.mCurrentValue;
    const f32 lfScore         = lpResults->mfShowtimeScore;
    mGameModeData.mShowtimeScore.Update(lfScore);
    mGameModeData.mEventScoreDelta.Record(lfScore - lfPreviousScore);

    mGameModeData.mEventBoostAmount.Update(lfBoostAmount);
    mGameModeData.mShowtimeBoostDelta.Update(
        lfBoostAmount - mGameModeData.mEventBoostAmount.mPreviousValue);

    const f32 lfRate = mGameModeData.mEventScoreDelta.GetAverage();
    if (mGameModeData.mfTimeSinceScoreTick > KF_SCORE_TICK_GAP)
    {
        s32 leTag = -1;
        if (lfRate >= KF_SCORE_RATE_FAST)
        {
            leTag = E_SPLICE_SHOWTIME_SCORE_FAST;
        }
        else if (lfRate >= KF_SCORE_RATE_MEDIUM)
        {
            leTag = E_SPLICE_SHOWTIME_SCORE_MEDIUM;
        }
        else if (lfRate >= KF_SCORE_RATE_SLOW)
        {
            leTag = E_SPLICE_SHOWTIME_SCORE_SLOW;
        }
        else if ((mGameModeData.mShowtimeScore.mCurrentValue -
                  mGameModeData.mShowtimeScore.mPreviousValue) > 0.0f)
        {
            // Scoring at all, however slowly, still ticks.
            leTag = E_SPLICE_SHOWTIME_SCORE_V_SLOW;
        }

        if (leTag != -1)
        {
            PlaySound(static_cast<ePresentationSampleTags>(leTag), KU8_MIXER_OUTPUT_SHOWTIME);
            mGameModeData.mfTimeSinceScoreTick = 0.0f;
        }
    }
    mGameModeData.mfTimeSinceScoreTick += mfDeltaTime;

    // The RISING EDGE of a boost chunk: the previous delta was zero and this one is not.
    const f32 lfPreviousDelta = mGameModeData.mShowtimeBoostDelta.mPreviousValue;
    const bool lbWasZero = (lfPreviousDelta <= KF_EPSILON) && (lfPreviousDelta >= -KF_EPSILON);
    if (lbWasZero &&
        mGameModeData.mShowtimeBoostDelta.mCurrentValue > KF_BOOST_EARN_THRESHOLD)
    {
        PlaySound(E_SPLICE_SHOWTIME_BOOST_EARN, KU8_MIXER_OUTPUT_SHOWTIME);
    }
}

// ---- UpdateRoadRage   @ 0x82701AA0 ----------------------------------------------
// Road Rage's clock: the shared countdown, plus the "time added" sting when a takedown
// finishes and the "time up" sting when the clock reaches zero.
void HUDEffect::UpdateRoadRage(const Module::Io::RootInputBuffer::ScoringOutputInterface* apScoring)
{
    const ScoringCursor lScoring = { reinterpret_cast<const u8*>(apScoring) };

    UpdateEventTimeRemaining(lScoring.TimeRemaining(), KF_EVENT_TIMER_WINDOW,
                             E_SPLICE_ROAD_RAGE_COUNT_DOWN, KU8_MIXER_OUTPUT_ROAD_RAGE);

    // The clock going UP is a takedown award; latch it and wait for the camera.
    if (mGameModeData.mEventTimeRemaining.mCurrentValue >
        mGameModeData.mEventTimeRemaining.mPreviousValue)
    {
        mGameModeData.mbTimeExtended = true;
    }

    // ⭐ THE STING WAITS FOR THE TAKEDOWN CAMERA TO END, not for the award. The X360 asks
    // the director camera's state whether E_FLAG_TAKEDOWN_CAMERA has just gone from set to
    // clear (`!IsFlagSet(11) && HasChanged(11)`, @0x82694478), so the cue lands as the
    // replay cuts back rather than underneath it.
    if (mGameModeData.mbTimeExtended)
    {
        Module::SoundLogicModule* lpModule =
            static_cast<Module::SoundLogicModule*>(mpLogicModule);
        Module::Io::RootInputBuffer* lpInputBuffer = lpModule->GetBrnInputStructure();
        const Module::Io::RootInputBuffer::DirectorCamera* lpCamera =
            lpInputBuffer->GetDirectorCamera();

        if (lpCamera->GetState().HasChangedToUnset(
                BrnDirector::Camera::CameraState::E_FLAG_TAKEDOWN_CAMERA))
        {
            mGameModeData.mbTimeExtended = false;
            PlaySound(E_SPLICE_ROAD_RAGE_TIME_ADD, KU8_MIXER_OUTPUT_ROAD_RAGE);
        }
    }

    if (mGameModeData.mEventTimeRemaining.mCurrentValue <= 0.0f &&
        mGameModeData.mEventTimeRemaining.mPreviousValue > 0.0f)
    {
        PlaySound(E_SPLICE_ROAD_RAGE_TIME_ENDED, KU8_MIXER_OUTPUT_ROAD_RAGE);
    }
}

// ---- UpdateStuntRun   @ 0x82701598 ----------------------------------------------
// Stunt Run's three cues: the score tick, the multiplier sting, and the shared countdown.
void HUDEffect::UpdateStuntRun(const Module::Io::RootInputBuffer::ScoringOutputInterface* apScoring)
{
    const ScoringCursor lScoring = { reinterpret_cast<const u8*>(apScoring) };

    Module::SoundLogicModule* lpModule =
        static_cast<Module::SoundLogicModule*>(mpLogicModule);
    const Module::Io::RootInputBuffer::GuiAudioEventResults* lpResults =
        lpModule->GetBrnInputStructure()->GetGuiAudioEventResults();

    // The live stunt score, or zero while the run is idle (the -1 sentinel).
    mGameModeData.mStuntScore.Update(
        lScoring.StuntRunSentinel() == -1 ? 0 : lScoring.StuntScore());
    mGameModeData.mEventScoreDelta.Record(
        static_cast<f32>(mGameModeData.mStuntScore.mCurrentValue -
                         mGameModeData.mStuntScore.mPreviousValue));

    // ⚠️ THE TICK RATE COMES FROM THE *RESULT* SCORE WHEN ONE IS LANDING, and from the
    // running average otherwise. The X360 takes the result delta only when the result score
    // went UP from a non-zero previous value -- i.e. while a landed run is counting up.
    const s32 liPreviousResult = mGameModeData.mStuntResultScore.mCurrentValue;
    mGameModeData.mStuntResultScore.Update(lpResults->miStuntResultScore);
    f32 lfRate;
    if (mGameModeData.mStuntResultScore.mCurrentValue > liPreviousResult &&
        liPreviousResult > 0)
    {
        lfRate = static_cast<f32>(mGameModeData.mStuntResultScore.mCurrentValue -
                                  mGameModeData.mStuntResultScore.mPreviousValue);
    }
    else
    {
        lfRate = mGameModeData.mEventScoreDelta.GetAverage();
    }

    if (mGameModeData.mfTimeSinceScoreTick > KF_SCORE_TICK_GAP)
    {
        s32 leTag = -1;
        if (lfRate >= KF_SCORE_RATE_FAST)
        {
            leTag = E_SPLICE_STUNT_SCORE_FAST;
        }
        else if (lfRate >= KF_SCORE_RATE_MEDIUM)
        {
            leTag = E_SPLICE_STUNT_SCORE_MEDIUM;
        }
        else if (lfRate >= KF_SCORE_RATE_SLOW)
        {
            leTag = E_SPLICE_STUNT_SCORE_SLOW;
        }
        else if ((mGameModeData.mStuntScore.mCurrentValue -
                  mGameModeData.mStuntScore.mPreviousValue) > 0)
        {
            leTag = E_SPLICE_STUNT_SCORE_V_SLOW;
        }

        if (leTag != -1)
        {
            PlaySound(static_cast<ePresentationSampleTags>(leTag), KU8_MIXER_OUTPUT_STUNT);
            mGameModeData.mfTimeSinceScoreTick = 0.0f;
        }
    }
    mGameModeData.mfTimeSinceScoreTick += mfDeltaTime;

    // The multiplier sting, gated on the HUD message rather than on the number: the sample
    // INDEX is the multiplier, clamped into the tag's range, so x2/x3/x4 are distinct cues.
    if (lScoring.StuntComboMultiplier() > 1)
    {
        if (mLastReceivedHudMessage == KU_HUD_MESSAGE_STUNT_MULTIPLIER)
        {
            mLastReceivedHudMessage = 0;
            mGameModeData.mStuntComboMultiplier.Update(lScoring.StuntComboMultiplier());

            Attrib::Gen::sampletags lTags(
                const_cast<Attrib::Collection*>(
                    const_cast<Attrib::RefSpec&>(
                        lpModule->GetSampleTags(4u)).GetCollection()), 0);

            const u32 luTag = static_cast<u32>(E_SPLICE_STUNT_MULTIPLIERS);
            const s16 li16First = *static_cast<const s16*>(lTags.FirstIndices(luTag));

            const s16 li16Last = lTags.LastIndices(luTag);

            BrnEffectObject::SampleTag lSampleTag;
            lSampleTag.mfVolume = *static_cast<const f32*>(lTags.Volumes(luTag));
            // X360 0x827017AC..0x827017D0: IntClamp(first - previous + current - 1,
            // first, last). The index walks the multiplier bank by HOW FAR the multiplier
            // moved, so x1->x2 and x3->x4 pick different samples, and the clamp keeps it
            // inside the tag's own range.
            lSampleTag.miSampleIndex = static_cast<s16>(CgsSound::Utils::IntClamp(
                li16First - mGameModeData.mStuntComboMultiplier.mPreviousValue +
                mGameModeData.mStuntComboMultiplier.mCurrentValue - 1,
                li16First, li16Last));

            PlaySound(lSampleTag, KU8_MIXER_OUTPUT_STUNT);
        }
    }
    else
    {
        mGameModeData.mStuntComboMultiplier.Update(lScoring.StuntComboMultiplier());
    }

    // ⚠️ THE TAG REALLY IS THE ROAD-RAGE ONE (X360 0x82701818 `li r6, 0x15`), on the STUNT
    // mixer output. Tag 21 is the generic last-ten-seconds countdown bank and all three
    // timed lanes reuse it -- E_SPLICE_EVENT_COUNTDOWN (14) is a different cue and is NOT
    // what any of these pass. Do not "correct" this to 14.
    UpdateEventTimeRemaining(lScoring.TimeRemaining(), KF_EVENT_TIMER_WINDOW,
                             E_SPLICE_ROAD_RAGE_COUNT_DOWN, KU8_MIXER_OUTPUT_STUNT);
}

// ---- UpdateGameModeHud   @ 0x82702670 -------------------------------------------
// The lane's dispatcher. The mode numbers are the scoring system's, taken from the input
// buffer's GameModeOutputInterface: its word at +0x08 is the CURRENT mode and the one at
// +0x00 is the mode the block was last reset for.
void HUDEffect::UpdateGameModeHud(Module::SoundLogicModule* apModule)
{
    CGS_ASSERT(apModule != 0, "lpLogicModule");                       // cpp:542
    CGS_ASSERT(apModule->GetBrnInputStructure(), "mpBrnLogicInputBuffer");

    Module::Io::RootInputBuffer* lpInputBuffer = apModule->GetBrnInputStructure();
    const Module::Io::RootInputBuffer::GameModeOutputInterface* lpGameMode =
        lpInputBuffer->GetGameModeInterface();
    const Module::Io::RootInputBuffer::ScoringOutputInterface* lpScoring =
        lpInputBuffer->GetScoringInterface();
    CGS_ASSERT(lpGameMode && lpScoring,
               "( lpGameModeInterface ) && ( lpScoringInterface )");   // cpp:547
    if (!lpGameMode || !lpScoring)
    {
        return;
    }

    const s32 liCurrentMode = lpGameMode->miCurrentGameModeType;

    // A new mode starts from a clean block -- otherwise the first frame's score delta is
    // the whole of the previous event's score.
    if (liCurrentMode != lpGameMode->miPreviousGameModeType)
    {
        mGameModeData.Reset();
    }

    // [DIAG] NOT IN THE X360 BINARY -- opt-in witness (BRN_HUD_SOUND_DIAG=1) that this
    // lane is being reached AND which mode it dispatched to. It fires on CHANGE only, so
    // a whole event costs one line.
    if (liCurrentMode != giHudDiagLastMode && HudSoundDiagBudget(guHudDiagModes))
    {
        giHudDiagLastMode = liCurrentMode;
        HudSoundDiagPrintf("[hud-mode] game mode %d (previous %d, state %d)\n",
                           liCurrentMode, lpGameMode->miPreviousGameModeType,
                           lpGameMode->miCurrentGameModeState);
    }

    switch (liCurrentMode)
    {
    case KI_MODE_OFFLINE_SHOWTIME:
    case KI_MODE_ONLINE_SHOWTIME:
        UpdateShowtime();
        break;

    case KI_MODE_ROAD_RAGE:
        UpdateRoadRage(lpScoring);
        break;

    case KI_MODE_BURNING_ROUTE:
    {
        const ScoringCursor lScoring = { reinterpret_cast<const u8*>(lpScoring) };
        if (lScoring.IsTimedEvent())
        {
            UpdateEventTimeRemaining(lScoring.EventDuration() - lScoring.EventElapsed(),
                                     KF_EVENT_TIMER_WINDOW,
                                     E_SPLICE_ROAD_RAGE_COUNT_DOWN,
                                     KU8_MIXER_OUTPUT_ROAD_RAGE);
        }
        break;
    }

    case KI_MODE_STUNT_ATTACK:
        UpdateStuntRun(lpScoring);
        break;

    default:
        break;
    }
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
//           lu8MixerOutput = 3;
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
// The original byte_82FFB8D0 gates the received/re-trigger lines through the
// Sound component's combined sink, independently of the optional PC witness.
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
    char lacMessageId[24];
    CgsIDUnCompress(lrEvent.mHudMessageId, lacMessageId);
    if (KB_HUD_SPEW_MESSAGES)
        BrnSound::Debug::DebugComponent::mLogCombined << "[HUD] Received "
            << lacMessageId << "\tat [" << mfRunningTime << "].\n";

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
        // ARTIST @0x826F60B0 tests bit 1 of the module dispatch flags. The mixer
        // slot is the constant 3 already in r10 (`stb r10` @0x826F60DC).
        // Hex-Rays incorrectly aliases that register with the module flags load.
        const BrnSound::Module::SoundLogicModule* lpModule =
            static_cast<const BrnSound::Module::SoundLogicModule*>(GetLogicModule());
        if (lpModule && (lpModule->mDispatchState.mu64Flags & 2u) == 0)
        {
            lu8ChokeGroup  = 0;
            lu8MixerOutput = 3;
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
        if (KB_HUD_SPEW_MESSAGES)
            BrnSound::Debug::DebugComponent::mLogCombined << "[HUD] Ignoring "
                << lacMessageId << "\tat [" << mfRunningTime << "] due to re-trigger.\n";
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
