#ifndef BRN_SOUND_LOGIC_BRN_HUD_EFFECT_H
#define BRN_SOUND_LOGIC_BRN_HUD_EFFECT_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                        // Vector3 (GetSlipstreamAmount params), CgsID
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"
#include "GameShared/GameClasses/Sound/Logic/CgsVoiceWrapper.h"    // CgsSound::Logic::VoiceWrapper (maVoices / CustomHudVoice)
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"   // committed BrnEffectObject dual base (BY NAME)
#include "GameSource/Sound/Global/BrnMusicEffect.h"                // BrnSound::Logic::MusicStream (mMusicStream)
#include "GameSource/AttribSys/Generated/classes/presentationcomponent.h"

// =============================================================================
// BrnSound::Logic::HUDEffect
//   GameSource/Sound/Global/BrnHUDEffect.h (DWARF home) +
//   GameSource/Sound/Global/BrnHUDEffect.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. HUDEffect is the sound-logic EFFECT
// OBJECT that plays the HUD/GUI stings: it is registered with ObjectID 0x10, so
// CgsSound::Logic::StateManager's CreateEffectFromRegistry resolves it as
// state ((0x10 >> 16) & 0xFF) == 0 (Global) / effect id ((0x10 >> 4) & 0x7F) == 1,
// which is exactly the (manager 0, state 0, effect 1) address sound message 5
// (E_SOUNDMESSAGE_GUIAUDIO_EVENT) is posted to by
// SoundLogicModule::ProcessGuiEvents case 456.
//
// THE RECON'D FUNCTION SET OF THIS TU (each with its ARTIST address):
//   HUDEffect()                       @ 0x826E1940
//   ~HUDEffect()                      @ 0x826E1BC8
//   Attach()                          @ 0x8269C3C8
//   Detach()                          @ 0x826F5EF0
//   ProcessUpdate()                   @ 0x826E7220
//   Notify(const MessageHeader*)      @ 0x826F5F68
//   FindEventMapping(...)             @ 0x82686928
//   IsReTrigger(const GuiAudioEvent*) @ 0x82686858
//   GetFreeVoice(u8)                  @ 0x826D2530
//   PlaySound(const SampleTag&, u8)   @ 0x826F6338
//   GetSlipstreamAmount(...)          @ 0x82686BA8
//   GameModeData::GameModeData        @ 0x826AFE88
//   GameModeData::Reset               @ 0x826977D8
//
// DEFERRED (declared, NOT bodied -- they hang off UpdateGameModeHud @0x82702670,
// which reads the RootInputBuffer::ScoringOutputInterface; that interface is not
// reached by this slice): UpdateGameModeHud, UpdateStuntRun @0x82701598,
// UpdateShowtime @0x82701830, UpdateRoadRage @0x82701AA0, UpdateBurningRoute,
// UpdateEventTimeRemaining @0x826FE400, UpdateSlipStreaming @0x82701BA8,
// PlaySound(ePresentationSampleTags, u8), FindPauseEventMapping. They are the
// score-tick / countdown / stunt / showtime / road-rage lane; the message-driven
// GUI sting lane (Notify -> FindEventMapping -> maVoices) is complete here.
//
// LAYOUT (X360 byte offsets, all cross-checked between FOUR functions at once --
// the virtuals receive the EffectBase sub-object pointer, which the asm shows as
// `this + 4`, while the private helpers receive `HUDEffect*` directly, which is
// why the same field reads as a1+1180 in Attach/Notify and a1+1184 nowhere):
//     +0x038 (  56)  mau8RoundRobins[256]   (FindEventMapping `*(v21 + a1 + 56)`)
//     +0x138 ( 312)  maVoices[3]            (Detach `a1 + 308`, stride 80)
//     +0x228 ( 552)  mau8MixerOutputs[3]    (Attach `*(v6 - 3) = 3`)
//     +0x22B ( 555)  mau8ChokeGroups[3]     (Attach `*v6++ = 0`)
//     +0x230 ( 560)  maGameModeVoices[4]    (Detach `a1 + 556`, stride 92)
//     +0x3A0 ( 928)  mMusicStream           (Attach LP 96000.0 / priority 2)
//     +0x400 (1024)  mGameModeData          (Attach's 10-float loop @ a1+1020)
//     +0x4A0 (1184)  mHudMessageData        (Attach ChangeWithDefault(a1 + 1180))
//     +0x4B0 (1200)  mLastPlayedEvent       (Notify `*(a1 + 1196) = *v15`)
//     +0x4C8 (1224)  mLastReceivedHudMessage(Notify `*(a1 + 1220) = *(a2 + 32)`)
//     +0x4D0 (1232)  mfTimeSinceLastTrigger (Attach 0.5, Notify 0.0, IsReTrigger < 0.5)
//     +0x4D4 (1236)  muRoundRobin
// Members are pinned BY NAME + SEQUENCE; absolute offsets are NOT static_asserted
// on the 64-bit host (pointer members widen).
// =============================================================================

namespace BrnSound
{
namespace Logic
{

// BrnHUDEffect.h:47 (DWARF). One HUD voice with its client-side volume/pitch and
// the dynamic-mixer output slot it reads its gain from. 92 bytes on the console
// (VoiceWrapper 0x50 + two floats + one byte + pad), which is exactly the stride
// Detach @0x826F5EF0 and ProcessUpdate @0x826E7220 walk maGameModeVoices with.
struct CustomHudVoice
{
    CustomHudVoice() : mVoice(), mfClientVolume(1.0f), mfClientPitch(1.0f),
                       mu8MixerOutput(3) {}

    CgsSound::Logic::VoiceWrapper mVoice;   // BrnHUDEffect.h:68  (+0x00)
    f32                           mfClientVolume; // :70          (+0x50)
    f32                           mfClientPitch;  // :71          (+0x54)
    u8                            mu8MixerOutput; // :72          (+0x58)
};

// BrnHUDEffect.h:106 (DWARF). HUDEffect : public BrnSound::Logic::BrnEffectObject.
struct HUDEffect : public BrnSound::Logic::BrnEffectObject
{
    enum { E_NUM_VOICES = 3, E_NUM_GAME_MODE_VOICES = 4, KI_MAX_MAPPINGS = 256 };

    // BrnHUDEffect.h:124 (DWARF). The presentation splice-bank tags the game-mode
    // HUD lane plays through PlaySound(ePresentationSampleTags, u8).
    enum ePresentationSampleTags
    {
        E_SPLICE_SMASH_THROUGH         = 0,
        E_SPLICE_STUNT                 = 1,
        E_SPLICE_CAMERA_CUT            = 2,
        E_SPLICE_JUMP_CAM_LANDING      = 3,
        E_SPLICE_STUNT_NEGATIVE        = 4,
        E_SPLICE_STUNT_POSITIVE1       = 5,
        E_SPLICE_STUNT_POSITIVE2       = 6,
        E_SPLICE_STUNT_POSITIVE3       = 7,
        E_SPLICE_QUIT_EVENT            = 8,
        E_SPLICE_STUNT_SCORE_FAST      = 9,
        E_SPLICE_STUNT_SCORE_MEDIUM    = 10,
        E_SPLICE_STUNT_SCORE_SLOW      = 11,
        E_SPLICE_STUNT_SCORE_V_SLOW    = 12,
        E_SPLICE_STUNT_MULTIPLIERS     = 13,
        E_SPLICE_EVENT_COUNTDOWN       = 14,
        E_SPLICE_SHOWTIME_SCORE_FAST   = 15,
        E_SPLICE_SHOWTIME_SCORE_MEDIUM = 16,
        E_SPLICE_SHOWTIME_SCORE_SLOW   = 17,
        E_SPLICE_SHOWTIME_SCORE_V_SLOW = 18,
        E_SPLICE_SHOWTIME_BOOST_EARN   = 19,
        E_SPLICE_ROAD_RAGE_TIME_ADD    = 20,
        E_SPLICE_ROAD_RAGE_COUNT_DOWN  = 21,
        E_SPLICE_ROAD_RAGE_TIME_ENDED  = 22,
        E_SPLICE_ONLINE_RIVAL_SWEEP    = 23
    };

    // The GUI-side record sound message 5 carries. DWARF
    // GameSource/Gui/Events/BrnGuiAudioEvent.h:102 `struct BrnGui::GuiAudioEvent`
    // names the four fields; the X360 dispatch @0x826EE0C8 copies the record with
    // three `std`s (0/8/0x10), i.e. 24 bytes, and HUDEffect::Notify reads exactly
    // +0 / +4 / +8 / +16.
    // ⚠ This is the SOUND-SIDE VIEW of the payload, declared here because the
    // BrnGui wire type (BrnGuiDemangledEventTypes.h) is still `u8 maPayload[12]`
    // and belongs to the GUI group.
    struct GuiAudioEventRecord
    {
        s32   miComponentType;         // +0x00  1 = HUD message, 2 = pause component
        s32   miAction;                // +0x04
        s32   miAdditionalInformation; // +0x08
        s32   miPad;                   // +0x0C  (CgsID is 8-aligned)
        CgsID mHudMessageId;           // +0x10
    };

    // BrnHUDEffect.h:290 (DWARF). Per-game-mode HUD-audio data block.
    struct GameModeData
    {
        // BrnHUDEffect.h:298 (DWARF). Zero-initialise the block. @ 0x826AFE88.
        GameModeData();

        // BrnHUDEffect.h:305 (DWARF). Logical reset between events (combo
        // multiplier defaults to 1, time-extended flag set). @ 0x826977D8.
        void Reset();

        // ORDER mirrors the DWARF / X360 access sequence.
        CgsSound::Utils::Average<10, f32> mEventScoreDelta;     // :312
        CgsSound::Utils::DataPoint<f32>   mEventTimeRemaining;  // :313
        CgsSound::Utils::DataPoint<f32>   mEventBoostAmount;    // :314
        f32                               mfTimeSinceScoreTick; // :315
        CgsSound::Utils::DataPoint<s32>   mStuntScore;          // :318
        CgsSound::Utils::DataPoint<s32>   mStuntComboMultiplier;// :319
        CgsSound::Utils::DataPoint<s32>   mStuntResultScore;    // :320
        CgsSound::Utils::DataPoint<f32>   mShowtimeScore;       // :323
        CgsSound::Utils::DataPoint<f32>   mShowtimeBoostDelta;  // :324
        bool                              mbTimeExtended;       // :327
    };

    // @ 0x826E1940 -- the anchor ctor.
    HUDEffect();

    // @ 0x826E1E00 vector deleting destructor + @ 0x826E1BC0 adjustor{4} forward to
    // this virtual dtor (compiler-synthesised; the leaf anchor is @0x826E1BC8).
    virtual ~HUDEffect();

    CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* GetTypeInfo() const override;
    const char* GetTypeName() const override;
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* GetStaticTypeInfo();
    static CgsSound::Logic::EffectObject* CreateObject(u32 auType);

    // @ 0x8269C3C8 -- bind the HUD-message mapping table and reset every voice.
    virtual bool Attach();
    // @ 0x826F5EF0 -- release all seven voices after the base detach.
    virtual bool Detach();
    // @ 0x826E7220 -- pump the seven wrappers and re-apply their DMix gains.
    virtual void ProcessUpdate();
    // @ 0x826F5F68 -- the GUI audio event (sound message 5) entry point.
    virtual void Notify(const CgsSound::Io::MessageHeader* apMessageHeader);
    // Advance mfTimeSinceLastTrigger (the re-trigger guard IsReTrigger tests).
    virtual void UpdateParams(f32 af32DeltaTime);

    // @ 0x82686BA8. Slipstream-audio proximity/alignment weight for the car in front.
    // The three Vector3 args arrive in vector registers (v1/v2/v3); `this` (r3) is
    // unused by the body. DWARF renders the return as `double`.
    f64 GetSlipstreamAmount( Vector3 lCarInFrontVelocity,
                             Vector3 lOwnWeightedVelocity,
                             Vector3 lNegatedDirection );

private:
    // @ 0x82686928 (DWARF BrnHUDEffect.cpp:402).
    bool FindEventMapping(const Attrib::Gen::presentationcomponent& arComponent,
                          const GuiAudioEventRecord* apAudioEvent,
                          s32& ariSpliceIndex, u8& aru8MixerOutput, u8& aru8ChokeGroup);
    // @ 0x82686858 (DWARF BrnHUDEffect.cpp:351).
    bool IsReTrigger(const GuiAudioEventRecord* apAudioEvent) const;
    // @ 0x826D2530 (DWARF BrnHUDEffect.cpp:480).
    s32 GetFreeVoice(u8 au8ChokeGroup);
    // @ 0x826F6338 (DWARF BrnHUDEffect.cpp:911) -- the game-mode HUD voice lane.
    void PlaySound(const BrnEffectObject::SampleTag& arTag, u8 au8MixerOutput);
    // The twelve-store splicer CreateParams stack image Notify @0x826F61C4 and
    // PlaySound @0x826F6338 both build (identical in both; only the target
    // wrapper differs). Outlined here, inlined in the console.
    void BuildSplicerVoiceParams(CgsSound::Logic::VoiceWrapper::CreateParams& arParams);

public:
    // --- members, DWARF BrnHUDEffect.h:331-343 order -------------------------
    u8                            mau8RoundRobins[KI_MAX_MAPPINGS];   // :331
    CgsSound::Logic::VoiceWrapper maVoices[E_NUM_VOICES];             // :332
    u8                            mau8MixerOutputs[E_NUM_VOICES];     // :333
    u8                            mau8ChokeGroups[E_NUM_VOICES];      // :334
    CustomHudVoice                maGameModeVoices[E_NUM_GAME_MODE_VOICES]; // :336
    // FLAG (not in the PS3 DWARF member list, but ATTESTED by the X360): a 96-byte
    // MusicStream sits between maGameModeVoices and mGameModeData -- Attach writes
    // its low-pass 96000.0f / priority 2 / cleared queue exactly like
    // MusicStream::Prepare does, and ProcessUpdate writes its volume from the DMix
    // output slot. Materialised by name from the committed BrnMusicStream home.
    MusicStream                   mMusicStream;
    GameModeData                  mGameModeData;                      // :338
    Attrib::Gen::presentationcomponent mHudMessageData;               // :339
    GuiAudioEventRecord           mLastPlayedEvent;                   // :340
    CgsID                         mLastReceivedHudMessage;            // :341
    f32                           mfTimeSinceLastTrigger;             // :342
    u32                           muRoundRobin;                       // :343

    // [DIAG] NOT IN THE X360 BINARY -- last observed wrapper stage per GUI voice,
    // so the [hud-sound-voice] witness can fire on the TRANSITION into PLAYING
    // instead of once per frame. Costs 3 bytes and is never read by game logic.
    u8                            mauLastVoiceState[E_NUM_VOICES];
};

} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_BRN_HUD_EFFECT_H
