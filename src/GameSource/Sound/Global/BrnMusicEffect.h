#ifndef BRN_SOUND_LOGIC_BRN_MUSIC_EFFECT_H
#define BRN_SOUND_LOGIC_BRN_MUSIC_EFFECT_H

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsFastBitArray.h"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"
#include "GameSource/Sound/Streaming/BrnIStreamUser.h"

namespace BrnSound
{
namespace Module { struct SoundLogicModule; }
namespace Logic
{
namespace Streaming { class StreamingStateManager; }
struct MixerControl;   // BrnMixerControl.h -- the +0x230 controller (effect id 0)

class MusicStream : public Streaming::IStreamUser
{
public:
    enum EState
    {
        E_STOPPED = 0,
        E_PLAYING = 1,
        E_STOP_REQUESTED = 2,
        E_FADING_OUT = 3,
        E_PAUSING = 4
    };

    MusicStream();
    void Prepare(Module::SoundLogicModule* apModule,
                 Streaming::StreamingStateManager* apStreamingManager,
                 const char* apVoiceSpec);
    void Queue(u32 auContentSpec, u8 auOutputSlot, u32 auPriority = 6);
    void Stop(f32 afFadeOut = 0.25f);
    void Update(f32 afDeltaTime);
    void SetSongQueued(bool abSongQueued);
    u8 GetOutputSlot() const { return mu8OutputSlot; }
    void SetVolume(f32 afVolume) { mfVolume = afVolume; }
    void SetHighPassFreq(f32 afFrequency) { mfHighPassFrequency = afFrequency; }
    void SetLowPassFreq(f32 afFrequency) { mfLowPassFrequency = afFrequency; }

    // ---- the three X360-INLINED stream idioms MusicEffect uses ------------------
    // Every MusicEffect call site folds these by hand; they are named here so the
    // effect reads like the console's source instead of repeating the bit patterns.
    //
    // (a) "playing or waiting to play" -- `meState == E_PLAYING || meState == E_PAUSING
    //     || mbSongQueued`, the guard in front of most of the console's stops
    //     (e.g. MusicEffect::Notify @0x826BBAF8 case 10, UpdateParams @0x826FE5C8 case 1).
    bool IsPlayingOrQueued() const
    {
        return meState == E_PLAYING || meState == E_PAUSING || mbSongQueued;
    }
    // (b) the folded Stop: `if (meState in [E_PLAYING..E_PAUSING]) { mfFadeDuration = f;
    //     meState = E_STOP_REQUESTED; } mbSongQueued = false;`.  NOT MusicStream::Stop():
    //     the console writes mbSongQueued directly (no SetSongQueued assert) and does not
    //     reset mfFadeTime here -- Update()'s E_STOP_REQUESTED arm does that next tick.
    void StopAndUnqueue(f32 afFadeOut)
    {
        if (meState == E_PLAYING || meState == E_STOP_REQUESTED ||
            meState == E_FADING_OUT || meState == E_PAUSING)
        {
            mfFadeDuration = afFadeOut;
            meState = E_STOP_REQUESTED;
        }
        mbSongQueued = false;
    }
    // (c) the folded Pause/duck: `if (meState == E_PLAYING || meState == E_PAUSING)
    //     { mfFadeTime = 0; mfFadeDuration = f; meState = E_PAUSING; }` -- fade the voice
    //     out over f and then internally pause it (Update()'s E_PAUSING arm, X360
    //     MusicStream::Update @0x826871F8 case 4).
    void PauseWithFade(f32 afFadeOut)
    {
        if (meState == E_PLAYING || meState == E_PAUSING)
        {
            mfFadeTime = 0.0f;
            mfFadeDuration = afFadeOut;
            meState = E_PAUSING;
        }
    }
    void SetStreamPaused(bool abPaused) { mbStreamPaused = abPaused; }
    // (d) the RESUME half of (c). PauseWithFade -> Update()'s E_PAUSING arm leaves the
    //     stream in E_PLAYING with mbInternalPause SET; only this clears it again, and
    //     UpdateVoiceParams turns the pair into the voice's PauseControl. The X360
    //     writes the byte inline: UpdateParams @0x826FE5C8 case 1/14 does
    //     `if (!Secondary.IsPlayingOrQueued()) *(this + 260) = 0` -- +260 is the EA Trax
    //     stream's mbInternalPause (its base +88), NOT mbStreamPaused (+261, which the
    //     prologue writes unconditionally for all three streams).
    void SetInternalPaused(bool abPaused) { mbInternalPause = abPaused; }
    // Either pause byte holds the voice's PauseControl at 1 (UpdateVoiceParams). The
    // X360 tests them as a pair -- ProcessUpdate @0x826F6D70 `*(a1+261) || *(a1+260)`.
    bool IsPaused() const { return mbStreamPaused || mbInternalPause; }
    EState GetState() const { return meState; }

    const CgsSound::Logic::VoiceWrapper::CreateParams& GetCreateParams() const override;
    void UpdateVoiceParams(CgsSound::Logic::VoiceWrapper& arVoice,
                           f32 afGain, f32 afElapsedTime) override;
    void StreamStopped() override;

private:
    CgsSound::Logic::VoiceWrapper::CreateParams mCreateParams;
    Streaming::StreamingStateManager* mpStreamingManager;
    EState meState;
    f32 mfFadeTime;
    f32 mfFadeDuration;
    f32 mfVolume;
    f32 mfHighPassFrequency;
    f32 mfLowPassFrequency;
    u32 muPriority;
    u32 muQueuedContentSpec;
    bool mbInternalPause;
    bool mbStreamPaused;
    bool mbSongQueued;
    u8 mu8QueuedOutputSlot;
    u8 mu8OutputSlot;

    // [DIAG] NOT IN THE X360 BINARY. Edge-detector for the opt-in BRN_STREAM_DIAG
    // send-gain witness in UpdateVoiceParams (the gain quantised to 1/1000), so a
    // steady stream logs one line rather than one per frame. Never read by any
    // behavioural arm.
    int miDiagLastGainQ;
};

class MusicEffect : public BrnEffectObject
{
public:
    enum EJunkyardAmbience
    {
        E_JUNKYARD_AMBIENCE_NONE = 0,
        E_JUNKYARD_AMBIENCE_NEW_PROFILE = 1,
        E_JUNKYARD_AMBIENCE_STANDARD = 2,
        E_JUNKYARD_AMBIENCE_COUNT = 3
    };

    // The music-type discriminant GetMusicType @0x8269CE70 returns and UpdateParams
    // @0x826FE5C8 switches on. The values are the console's own switch labels; the
    // names are from the arm each one drives (every one of them is attested by the
    // stream + output slot it queues, e.g. type 2 -> GetEventStartContentSpec on
    // mSecondaryStream slot 2). Types 7/8/9/10/13 never appear in either function.
    enum EMusicType
    {
        E_MUSIC_TYPE_NONE             = 0,
        E_MUSIC_TYPE_EATRAX_IN_GAME   = 1,   // EA Trax, inside a game mode
        E_MUSIC_TYPE_EVENT_START      = 2,
        E_MUSIC_TYPE_EVENT_END        = 3,
        E_MUSIC_TYPE_JUNKYARD         = 4,   // (UpdateParams case 4: nothing to do)
        E_MUSIC_TYPE_CAR_UNLOCKED     = 5,
        E_MUSIC_TYPE_PICTURE_PARADISE = 6,
        E_MUSIC_TYPE_SHOWTIME         = 11,
        E_MUSIC_TYPE_MENU             = 12,
        E_MUSIC_TYPE_EATRAX_FREEBURN  = 14   // EA Trax, no game mode running
    };

    // EA Trax play order, set by sound message 11 (GUI event 462). SelectSong
    // @0x8269D5E8 branches on it: 1 == shuffle (random pick, never the current song),
    // 0 == sequential (walk forward from the current index). Anything else asserts.
    enum EPlayOrder
    {
        E_PLAY_ORDER_SEQUENTIAL = 0,
        E_PLAY_ORDER_SHUFFLE    = 1
    };

    // X360 MusicEffect + 0x1CC. The EA Trax playlist state.
    struct EaTraxData
    {
        EaTraxData();
        bool Prepare(Module::SoundLogicModule* apLogicModule);

        // X360 0x8269D5E8. Return the index of the next song to play out of
        // [0, aiNumSongs), or -1 when no song is both still "remaining" and enabled
        // for this music type. aeMusicType picks which enabled-song mask is used.
        s32 SelectSong(s32 aiNumSongs, s32 aeMusicType);
        // X360 0x8269DF30. Assert the song is still in mRemainingSongs, clear its bit
        // (so the playlist does not repeat it) and record it as the current song.
        void SetCurrentSong(s32 aiSong);

        // +0x00 / +0x10: the two enabled-song masks the GUI publishes with sound
        // message 7 (GUI event 458 GuiEventAudioTraxUpdate). SelectSong picks
        // mEnabledSongsNoGameMode for music type 14 and mEnabledSongs for every other
        // type (X360 0x8269D5E8: `if (aeMusicType == 14) mask = this + 0x10`).
        // Notify @0x826BBAF8 case id 7 fills them from the payload: the payload's
        // SECOND 16 bytes (+0x10) land at +0x00 and its FIRST 16 (+0x00) at +0x10.
        CgsContainers::FastBitArray<128> mEnabledSongs;            // +0x00
        CgsContainers::FastBitArray<128> mEnabledSongsNoGameMode;  // +0x10
        // +0x20: songs not yet played this cycle. Set to all-ones by message 7 and
        // restored wholesale by message 8 (GUI 459, the saved "last played" state).
        CgsContainers::FastBitArray<128> mRemainingSongs;          // +0x20
        s32 miPreviewSong;           // +0x30  message 9 (GUI 460)
        s32 miPreviousPreviewSong;   // +0x34  message 9 pushes the old value here
        s32 miCurrentSong;           // +0x38  SetCurrentSong's record
        Module::SoundLogicModule* mpLogicModule;  // +0x3C
        s32 mePlayOrder;             // +0x40  message 11 (GUI 462), EPlayOrder
    };

    MusicEffect();
    virtual ~MusicEffect();
    CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* GetTypeInfo() const override;
    const char* GetTypeName() const override;
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* GetStaticTypeInfo();
    static CgsSound::Logic::EffectObject* CreateObject(u32 auType);
    bool Attach() override;
    void UpdateParams(f32 afDeltaTime) override;
    void ProcessUpdate() override;
    void Notify(const CgsSound::Io::MessageHeader* apMessage) override;

    // @ 0x82685D38: `return (a2 == 0) - 1;` -- controller slot 0 is effect-control id 0
    // (MixerControl, whose ClassTypeInfo ObjectID is 0), every other slot ends the
    // State::CreateSFXCtrls walk.
    s32 GetController(s32 aiIndex) override;
    // @ 0x826873A0: assert the control's effect id is 0 ("Unexpected control.",
    // BrnMusicEffect.cpp:483 -- the console tests `(*(ctrl+20) & 0x7F0) != 0`, which is
    // GetEffectID() != 0), then store it at this+0x230 (`result[140] = a2 - 4`, the
    // primary-base adjustment of the EffectBase sub-object pointer).
    void AttachController(CgsSound::Logic::EffectBase* apController) override;

    // X360 0x82687408. True while the player's own soundtrack (the dashboard music
    // player) owns playback, in which case the game must not start a song of its own.
    // On the PC build there is no XMP session, so this is always false -- the console
    // body is `XMPTitleHasPlaybackControl(&lb); return lb == 0;`.
    static bool IsCustomSoundtrackActive();

private:
    // X360 0x8269CE70. Derive the music type from the sound root input's game-mode
    // interface, the pending one-shot type and the junkyard ambience.
    s32 GetMusicType(const void* apGameModeInterface) const;
    // X360 0x8269D260. Drop the current song if the playlist stopped allowing it, and
    // take it out of mRemainingSongs. Runs only when message 7/8 changed the playlist.
    void UpdateSongs();
    // X360 0x8269CFC0 / 0x8269D0F0. Map the running game mode (and, for the end
    // stinger, whether the player won) onto the sting's ContentSpec.
    static u32 GetEventStartContentSpec(const void* apGameModeInterface);
    u32 GetEventEndContentSpec(const void* apGameModeInterface) const;

    // X360 MusicEffect +0x230, stored by AttachController @0x826873A0 and asserted
    // non-null by Attach @0x8269CC60 ("mpMixerControl", BrnMusicEffect.cpp:515).
    // ProcessUpdate reads its cached music-volume setting to publish this effect's
    // dynamic-mixer INPUT 0.
    MixerControl* mpMixerControl;

    // X360 MusicEffect +0x23C. The last slot-9 mixer output this effect published as
    // GuiOut event 513; the publish is edge-triggered on it (ProcessUpdate's tail).
    f32 mfLastPublishedGuiVolume;

    MusicStream mSecondaryStream;
    MusicStream mEATraxStream;
    MusicStream mMusicStreamMenu;
    MusicStream mJunkyardStream;
    EaTraxData mEaTraxData;

    // ---- X360 MusicEffect tail (+0x210 .. +0x246) ------------------------------
    bool mbPlaylistChanged;      // +0x210  set by message 7, consumed by UpdateSongs
    bool mbPreviewActive;        // +0x211  message 9's second payload byte
    s32  meEventEndResult;       // +0x214  message 23 payload +0x5C (rank 1..4 -> a
                                 //         "00Race_End_RANK_0n" sting, else the mode sting)
    bool mbEventWon;             // +0x218  message 23: mode-dependent "won" flag
    bool mbEventEndPending;      // +0x219  message 23 arms the end sting for UpdateParams
    u32  mCarUnlockName;         // +0x21C  message 28's ContentSpec
    u32  mMenuStreamName;        // +0x220  message 13's ContentSpec
    s32  meMusicType;            // +0x224
    s32  mePrevMusicType;        // +0x228
    s32  mePendingMusicType;     // +0x234  one-shot type pushed by Notify (5/11/12)
    s32  meJunkyardAmbience;     // +0x238  message 40
    s32  miPicParadiseMusic;     // +0x240  index into KA_CLASSICAL_MUSIC_DATA
    bool mbHoldVolumes;          // +0x244  message 15 -- ProcessUpdate's early-out
    bool mbMenuStreamIsVideo;    // +0x245  message 13 payload +4
    bool mbMenuStreamOverCustom; // +0x246  message 13 payload +5
};

} // namespace Logic
} // namespace BrnSound

#endif
