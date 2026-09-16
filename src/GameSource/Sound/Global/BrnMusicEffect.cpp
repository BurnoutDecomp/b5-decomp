#include "GameSource/Sound/Global/BrnMusicEffect.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameSource/Sound/Module/BrnRootSoundModuleIo.h"
#include "GameSource/Sound/Streaming/BrnStreamingStateManager.h"
#include "GameSource/Sound/Global/BrnMixerControl.h"   // mpMixerControl (the +0x230 controller)
#include "GameShared/GameClasses/Sound/IO/CgsMessage.h"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"
#include "GameSource/Sound/Module/LogicModule/BrnMessageData.h"
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameSource/AttribSys/Generated/classes/songlist.h"
#include "GameSource/AttribSys/Generated/classes/song.h"
#include "GameSource/AttribSys/Generated/classes/streammappings.h"               // GetStreamFromVideoName
#include "GameSource/AttribSys/Generated/classes/languagestreamconfiguration.h"  // GetStreamFromVideoName
#include "GameSource/Sound/Global/BrnSpeechEffect.h"                              // SpeechEffect::GetLanguage (message 33)
#include "SDKs/EATech/include/Nicotine/DMixIO.hpp"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"  // the JumpHpf crash gate
#include "GameSource/Gui/Events/BrnGuiEventAudioTrax.h"   // BrnGui::GuiEATraxNewTrackEvent (GUI event 502)
#include <cmath>   // powf (JumpHpf::Update's exponential sweep, sub_82C09970)

#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>

namespace BrnSound
{
namespace Logic
{

namespace
{
    // K_NULL_NAME is 0 (CgsCommon.h: "not a recon'd standalone ctor TU -- K_NULL_NAME
    // semantics are 0"). The X360 reads it out of dword_830082A8.
    const u32 KU_NULL_NAME = 0u;

    // DWARF BrnMusicEffect.cpp:33/36/39 -- the jump high-pass constants, read out of
    // the ARTIST image (tools/re/x360rd.py): flt_82F2CE70 / flt_82F2CE74 / flt_82F2CE78.
    // Notify(14) opens towards 2 kHz over 3 s; Notify(14, closed) / a crash closes
    // back to the 230 Hz rest (flt_820AA558) over 1 s.
    const f32 KF_JUMP_HPF_OPEN_FREQUENCY = 2000.0f;
    const f32 KF_JUMP_HPF_OPEN_TIME      = 3.0f;
    const f32 KF_JUMP_HPF_CLOSE_TIME     = 1.0f;
    const f32 KF_JUMP_HPF_REST_FREQUENCY = 230.0f;   // flt_820AA558

    // RwMathFPU::IsZero as the console inlines it in JumpHpf::Prepare @0x82687530 /
    // ::Release @0x82687690: |x| <= FLT_EPSILON (flt_820AA114 / flt_82002514).
    inline bool JumpHpfIsZero(f32 afValue)
    {
        return afValue <= 1.1920929e-7f && afValue >= -1.1920929e-7f;
    }

    // X360 MusicEffect.cpp KI_NUM_PICTURE_PARADISE_NAMES == 22, asserted at
    // UpdateParams @0x826FE5C8 ("miPicParadiseMusic < KI_NUM_PICTURE_PARADISE_NAMES").
    const s32 KI_NUM_PICTURE_PARADISE_NAMES = 22;

    // X360 KA_CLASSICAL_MUSIC_DATA @0x820AA560, 22 records of
    // { const char* mpcStream; const char* mpcComposerTextId; const char* mpcWorkTextId; }
    // read out of the ARTIST image (tools/re/x360rd.py 820AA560, stride 12).
    struct ClassicalMusicData
    {
        const char* mpcStream;
        const char* mpcComposerTextId;
        const char* mpcWorkTextId;
    };
    const ClassicalMusicData KA_CLASSICAL_MUSIC_DATA[KI_NUM_PICTURE_PARADISE_NAMES] =
    {
        { "PicParadise01", "COMP_BACH",        "WORK_BACH_AIR" },
        { "PicParadise15", "COMP_BOCCHERINI",  "WORK_BOCCHERINI_MINUET" },
        { "PicParadise08", "COMP_MOZART",      "WORK_MOZART_EINEKLEINENACHTMUSIC" },
        { "PicParadise09", "COMP_DELIBES",     "WORK_DELIBES_FLOWERDUET" },
        { "PicParadise02", "COMP_GOUNOD",      "WORK_GOUNOD_AVEMARIA" },
        { "PicParadise21", "COMP_TCHAIKOVSKY", "WORK_TCHAIKOVSKY_NUTCRACKER" },
        { "PicParadise03", "COMP_SAINTSAENS",  "WORK_SAINTSAENS_AQUARIUM" },
        { "PicParadise12", "COMP_MOZART",      "WORK_MOZART_HORNCONCERTO4" },
        { "PicParadise18", "COMP_TCHAIKOVSKY", "WORK_TCHAIKOVSKY_SLEEPINGBEAUTY" },
        { "PicParadise11", "COMP_VERDI",       "WORK_VERDI_HEBREW" },
        { "PicParadise19", "COMP_DVORAK",      "WORK_DVORAK_SYMPHONY9" },
        { "PicParadise05", "COMP_DEBUSSY",     "WORK_DEBUSSY_CLAIRDELUNE" },
        { "PicParadise16", "COMP_BEETHOVEN",   "WORK_BEETHOVEN_MOONLIGHT" },
        { "PicParadise17", "COMP_MOZART",      "WORK_MOZART_PIANOCONCERTO" },
        { "PicParadise24", "COMP_HANDEL",      "WORK_HANDEL_WATERMUSIC1" },
        { "PicParadise04", "COMP_SAINTSAENS",  "WORK_SAINTSAENS_SWAN" },
        { "PicParadise10", "COMP_BIZET",       "WORK_BIZET_CARMEN" },
        { "PicParadise13", "COMP_MOZART",      "WORK_MOZART_HORNCONCERTO3" },
        { "PicParadise23", "COMP_VERDI",       "WORK_VERDI_AIDA" },
        { "PicParadise25", "COMP_HANDEL",      "WORK_HANDEL_WATERMUSIC2" },
        { "PicParadise06", "COMP_VIVALDI",     "WORK_VIVALDI_SPRING" },
        { "PicParadise14", "COMP_BRAHMS",      "WORK_BRAHMS_HUNGARIANDANCE" },
    };

    // X360 off_82F2CE7C, indexed 1..4 by the event-end result rank (index 0 is the
    // "INVALID_ASSET" guard entry the console never selects: UpdateParams takes this
    // table only when `result > 0 && result <= 4`).
    const char* const KA_EVENT_END_RANK_NAMES[6] =
    {
        "INVALID_ASSET",
        "00Race_End_RANK_01", "00Race_End_RANK_02",
        "00Race_End_RANK_03", "00Race_End_RANK_04", "00Race_End_RANK_05"
    };

    // [DIAG] NOT IN THE X360 BINARY -- opt-in music witness (BRN_MUSIC_DIAG=1).
    // Names exactly what it prints: the music messages this effect receives, the music
    // type it computes each frame (only when it CHANGES), the song it selects, and the
    // stream Queue it posts. Rate limited by construction (type changes + selections are
    // rare); the per-message line is capped.
    bool MusicDiagEnabled()
    {
        static int siEnabled = -1;
        if (siEnabled < 0)
        {
            const char* lpcEnv = std::getenv("BRN_MUSIC_DIAG");
            siEnabled = (lpcEnv && lpcEnv[0] && lpcEnv[0] != '0') ? 1 : 0;
        }
        return siEnabled != 0;
    }
    u32 guMusicDiagMessages = 0;
    const u32 KU_MUSIC_DIAG_MESSAGE_CAP = 400;

    // [DIAG] NOT IN THE X360 BINARY -- POSITIVE CONTROL for the music chain
    // (BRN_MUSIC_FORCE_TYPE=<n>, default OFF/-1). It overrides ONLY the value
    // GetMusicType computes from the game-mode interface, so the rest of the chain
    // (playlist masks -> SelectSong -> songlist/song attrib -> MusicStream::Queue ->
    // StreamingStateManager) runs for real. It exists because the game side never
    // publishes the GameModeOutputInterface (GameStateModuleIO::OutputBuffer's
    // mGameModeOutputInterfaceStorage @ console +176344 has NO writer in this tree --
    // ModeManager::PreWorldUpdate's publish is PARKED at
    // BrnModeManager_WorldTick.cpp:719), so the interface reads mode 0 / state 0 for
    // the whole run and the music type can never leave 2. Named for what it does: it
    // forces the TYPE, and nothing else.
    s32 MusicDiagForcedType()
    {
        static s32 siForced = -2;
        if (siForced == -2)
        {
            const char* lpcEnv = std::getenv("BRN_MUSIC_FORCE_TYPE");
            siForced = (lpcEnv && lpcEnv[0]) ? std::atoi(lpcEnv) : -1;
        }
        return siForced;
    }

    // [DIAG] NOT IN THE X360 BINARY. std::printf does NOT reach BrnGame.log on this
    // build; CgsDev::Log::WriteToLog is the sink the harness reads (the same one the
    // committed [lionfx] witness uses).
    void MusicDiagPrintf(const char* lpcFormat, ...)
    {
        char lacMsg[512];
        va_list lArgs;
        va_start(lArgs, lpcFormat);
        std::vsnprintf(lacMsg, sizeof(lacMsg), lpcFormat, lArgs);
        va_end(lArgs);
        CgsDev::Log::WriteToLog(lacMsg);
    }
}

// ---------------------------------------------------------------------------------
// EaTraxData
// ---------------------------------------------------------------------------------

MusicEffect::EaTraxData::EaTraxData()
    : miPreviewSong(-1), miPreviousPreviewSong(-1), miCurrentSong(-1),
      mpLogicModule(0), mePlayOrder(E_PLAY_ORDER_SEQUENTIAL)
{
    mEnabledSongs.Construct();
    mEnabledSongsNoGameMode.Construct();
    mRemainingSongs.Construct();
}

// X360 0x82697538.
bool MusicEffect::EaTraxData::Prepare(Module::SoundLogicModule* apLogicModule)
{
    CGS_ASSERT(apLogicModule != 0, "lpLogicModule");
    mEnabledSongs.Construct();
    mEnabledSongsNoGameMode.Construct();
    mRemainingSongs.Construct();
    miPreviewSong = -1;
    miPreviousPreviewSong = -1;
    miCurrentSong = -1;
    mpLogicModule = apLogicModule;
    mePlayOrder = E_PLAY_ORDER_SEQUENTIAL;
    return true;
}

// X360 0x8269D5E8. Two passes over [0, aiNumSongs): the first collects every song that
// is still remaining AND enabled for this music type; with 0 candidates the caller gets
// -1 (its cue to refill mRemainingSongs and ask again) and with 1 it gets that one.
// Otherwise the play order decides: shuffle re-collects excluding the current song and
// draws `RandomUInt() % count`; sequential walks forward from the current index.
s32 MusicEffect::EaTraxData::SelectSong(s32 aiNumSongs, s32 aeMusicType)
{
    const CgsContainers::FastBitArray<128>& lrEnabled =
        (aeMusicType == E_MUSIC_TYPE_EATRAX_FREEBURN) ? mEnabledSongsNoGameMode
                                                      : mEnabledSongs;

    u8 laCandidates[288];
    s32 liCount = 0;
    for (s32 liSong = 0; liSong < aiNumSongs; ++liSong)
    {
        if (mRemainingSongs.IsBitSet(static_cast<u32>(liSong)) &&
            lrEnabled.IsBitSet(static_cast<u32>(liSong)))
        {
            laCandidates[liCount++] = static_cast<u8>(liSong);
        }
    }
    if (liCount == 0)
        return -1;
    if (liCount == 1)
        return laCandidates[0];

    if (mePlayOrder == E_PLAY_ORDER_SHUFFLE)
    {
        s32 liShuffleCount = 0;
        for (s32 liSong = 0; liSong < aiNumSongs; ++liSong)
        {
            if (mRemainingSongs.IsBitSet(static_cast<u32>(liSong)) &&
                lrEnabled.IsBitSet(static_cast<u32>(liSong)) &&
                (miCurrentSong == -1 || liSong != miCurrentSong))
            {
                laCandidates[liShuffleCount++] = static_cast<u8>(liSong);
            }
        }
        CGS_ASSERT(liShuffleCount > 0, "liNumSongsToChooseFrom > 0");
        if (liShuffleCount <= 0)
            return -1;
        // X360: the module's own CgsNumeric::Random (module + 0x135D0), one LCG step,
        // the drawn value being the OLD seed's high word -- that is RandomUInt().
        CGS_ASSERT(mpLogicModule != 0, "mpLogicModule");
        const u32 luDraw = mpLogicModule->GetRandomGenerator().RandomUInt();
        return laCandidates[luDraw % static_cast<u32>(liShuffleCount)];
    }

    CGS_ASSERT(mePlayOrder == E_PLAY_ORDER_SEQUENTIAL, "Unhandled play order");
    s32 liSong = miCurrentSong;
    bool lbFound = false;
    for (s32 liTry = 0; liTry < aiNumSongs && !lbFound; ++liTry)
    {
        liSong = (liSong + 1) % aiNumSongs;
        if (mRemainingSongs.IsBitSet(static_cast<u32>(liSong)) &&
            lrEnabled.IsBitSet(static_cast<u32>(liSong)))
        {
            lbFound = true;
        }
    }
    return liSong;
}

// X360 0x8269DF30.
void MusicEffect::EaTraxData::SetCurrentSong(s32 aiSong)
{
    CGS_ASSERT(mRemainingSongs.IsBitSet(static_cast<u32>(aiSong)),
               "mRemainingSongs.IsBitSet( liSong )");
    mRemainingSongs.UnSetBit(static_cast<u32>(aiSong));
    miCurrentSong = aiSong;
}

// ---------------------------------------------------------------------------------
// MusicEffect
// ---------------------------------------------------------------------------------

MusicEffect::MusicEffect()
    : BrnEffectObject(), mpMixerControl(0), mfLastPublishedGuiVolume(0.0f),
      mJumpHpf(),   // @0x826C8DB4 `stw 0, 0x34(r3)` -- DESTRUCTED until Attach
      mSecondaryStream(), mEATraxStream(), mMusicStreamMenu(),
      mJunkyardStream(), mEaTraxData(),
      mbPlaylistChanged(false), mbPreviewActive(false), meEventEndResult(0),
      mbEventWon(false), mbEventEndPending(false),
      mCarUnlockName(KU_NULL_NAME), mMenuStreamName(KU_NULL_NAME),
      meMusicType(E_MUSIC_TYPE_NONE), mePrevMusicType(E_MUSIC_TYPE_NONE),
      mePendingMusicType(E_MUSIC_TYPE_NONE),
      meJunkyardAmbience(E_JUNKYARD_AMBIENCE_NONE), miPicParadiseMusic(0),
      mbHoldVolumes(false), mbMenuStreamIsVideo(false), mbMenuStreamOverCustom(false),
      meLanguage(0) {}

MusicEffect::~MusicEffect() {}

// ---------------------------------------------------------------------------
// MusicEffect::JumpHpf -- the exponential high-pass sweep (24 bytes @ MusicEffect+0x34).
// ---------------------------------------------------------------------------

// Inlined in MusicEffect::Attach @0x8269CE10..0x8269CE50:
//   lfs f12, flt_820AA558 (230.0) ; li r9, 1 ; lfs f0, flt_82001CC0 (0.0)
//   stfs f12, 0x38(r30) ; stfs f0, 0x40(r30) ; stw r9, 0x34(r30)
void MusicEffect::JumpHpf::Construct()
{
    mfStartFrequency   = KF_JUMP_HPF_REST_FREQUENCY;
    mfCurrentFrequency = 0.0f;
    meState            = E_JUMPHPFSTATE_CONSTRUCTED;
}

// @0x82687480 (export hole; ppcdis). assert(lfTargetFrequency > 0.0f) :0x79D, then
//   CONSTRUCTED / IDLE : mfCurrentFrequency = 230.0 (flt_820AA558)      ; fall into
//   CLOSING            : mfStartFrequency = mfCurrentFrequency;
//                        assert(!RwMathFPU::IsZero(mfStartFrequency)) :0x7AB;
//                        mfFrequencyGain = lfTargetFrequency / mfStartFrequency;
//                        mfTime = afTime; mfTimeThrough = 0; meState = OPENING
//   OPENING / OPEN     : nothing
//   default            : assert("Unhandled state ") :0x7BF
// Every arm returns true.
bool MusicEffect::JumpHpf::Prepare(f32 afTargetFrequency, f32 afTime)
{
    CGS_ASSERT(afTargetFrequency > 0.0f, "lfTargetFrequency > 0.0f");
    switch (meState)
    {
    case E_JUMPHPFSTATE_CONSTRUCTED:
    case E_JUMPHPFSTATE_IDLE:
        mfCurrentFrequency = KF_JUMP_HPF_REST_FREQUENCY;
        // fall through
    case E_JUMPHPFSTATE_CLOSING:
        mfStartFrequency = mfCurrentFrequency;
        CGS_ASSERT(!JumpHpfIsZero(mfStartFrequency), "!RwMathFPU::IsZero(mfStartFrequency)");
        mfFrequencyGain = afTargetFrequency / mfStartFrequency;
        mfTime          = afTime;
        mfTimeThrough   = 0.0f;
        meState         = E_JUMPHPFSTATE_OPENING;
        break;
    case E_JUMPHPFSTATE_OPENING:
    case E_JUMPHPFSTATE_OPEN:
        break;
    default:
        CGS_ASSERT(false, "Unhandled state ");
        break;
    }
    return true;
}

// @0x82687648.
//   CONSTRUCTED / IDLE / CLOSING : nothing
//   OPENING / OPEN               : mfStartFrequency = mfCurrentFrequency;
//                                  assert(!RwMathFPU::IsZero(mfStartFrequency)) :2021;
//                                  mfFrequencyGain = 230.0 / mfStartFrequency;
//                                  mfTime = afTime; mfTimeThrough = 0; meState = CLOSING
//   default                      : assert("Unhandled state ") :2034
// Every arm returns true.
bool MusicEffect::JumpHpf::Release(f32 afTime)
{
    switch (meState)
    {
    case E_JUMPHPFSTATE_CONSTRUCTED:
    case E_JUMPHPFSTATE_IDLE:
    case E_JUMPHPFSTATE_CLOSING:
        break;
    case E_JUMPHPFSTATE_OPENING:
    case E_JUMPHPFSTATE_OPEN:
        mfStartFrequency = mfCurrentFrequency;
        CGS_ASSERT(!JumpHpfIsZero(mfStartFrequency), "!RwMathFPU::IsZero(mfStartFrequency)");
        mfFrequencyGain = KF_JUMP_HPF_REST_FREQUENCY / mfStartFrequency;
        mfTime          = afTime;
        mfTimeThrough   = 0.0f;
        meState         = E_JUMPHPFSTATE_CLOSING;
        break;
    default:
        CGS_ASSERT(false, "Unhandled state ");
        break;
    }
    return true;
}

// @0x826877E0.
//   CONSTRUCTED / IDLE / OPEN : nothing
//   OPENING / CLOSING         : mfTimeThrough += dt;
//       if (mfTimeThrough >= mfTime) { mfCurrentFrequency = mfFrequencyGain * mfStartFrequency;
//                                      meState = (OPENING ? OPEN : IDLE); }
//       else mfCurrentFrequency = mfStartFrequency * powf(mfFrequencyGain, mfTimeThrough / mfTime)
//   default                   : assert("Unhandled state ") :2097
void MusicEffect::JumpHpf::Update(f32 afDeltaTime)
{
    switch (meState)
    {
    case E_JUMPHPFSTATE_CONSTRUCTED:
    case E_JUMPHPFSTATE_IDLE:
    case E_JUMPHPFSTATE_OPEN:
        break;
    case E_JUMPHPFSTATE_OPENING:
    case E_JUMPHPFSTATE_CLOSING:
    {
        const EJumpFilterState lePrev = meState;
        mfTimeThrough += afDeltaTime;
        if (mfTimeThrough >= mfTime)
        {
            mfCurrentFrequency = mfFrequencyGain * mfStartFrequency;
            meState = (lePrev == E_JUMPHPFSTATE_OPENING) ? E_JUMPHPFSTATE_OPEN
                                                         : E_JUMPHPFSTATE_IDLE;
        }
        else
        {
            mfCurrentFrequency =
                mfStartFrequency * powf(mfFrequencyGain, mfTimeThrough / mfTime);
        }
        break;
    }
    default:
        CGS_ASSERT(false, "Unhandled state ");
        break;
    }
}

CgsSound::Logic::EffectObject* MusicEffect::CreateObject(u32)
{
    return new MusicEffect();
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* MusicEffect::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject> sTypeInfo(
        0x20, "MusicEffect", CgsSound::Logic::EffectObject::GetStaticTypeInfo(),
        &MusicEffect::CreateObject);
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* MusicEffect::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

const char* MusicEffect::GetTypeName() const { return "MusicEffect"; }

static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* const gpMusicEffectReg =
    CgsSound::Logic::EffectObject::AddToClassTypeInfoArray(MusicEffect::GetStaticTypeInfo());

// X360 0x82687408. The PC build has no XMP (dashboard music player) session, so the
// console's `XMPTitleHasPlaybackControl(&lb); return lb == 0;` is constant false here.
bool MusicEffect::IsCustomSoundtrackActive()
{
    return false;
}

// X360 0x82685D38: `return (a2 == 0) - 1;`
s32 MusicEffect::GetController(s32 aiIndex)
{
    return aiIndex == 0 ? 0 : -1;
}

// X360 0x826873A0:
//   if ( (*(a2 + 20) & 0x7F0) != 0 ) assert("Unexpected control.", BrnMusicEffect.cpp:483);
//   else                             result[140] = a2 - 4;      // this+0x230
// `(*(ctrl + 20) & 0x7F0) != 0` is GetEffectID() != 0 (the id field is bits [10:4] of
// the packed effect id -- the same extraction DMixIO::GetSFX_ID uses), so the console
// accepts exactly the effect-control whose id is 0: MixerControl (ClassTypeInfo
// ObjectID 0). The `- 4` is the primary-base adjustment of the EffectBase sub-object
// pointer, expressed here as the ordinary derived cast.
void MusicEffect::AttachController(CgsSound::Logic::EffectBase* apController)
{
    CGS_ASSERT(apController != 0, "lpController");
    if (!apController)
        return;
    if (apController->GetEffectID() == 0)
        mpMixerControl = static_cast<MixerControl*>(apController);
    else
        CGS_ASSERT(false, "Unexpected control.");
}

bool MusicEffect::Attach()
{
    if (!CgsSound::Logic::EffectBase::Attach())
        return false;
    Module::SoundLogicModule* lpModule = static_cast<Module::SoundLogicModule*>(mpLogicModule);
    Streaming::StreamingStateManager* lpStreaming =
        static_cast<Streaming::StreamingStateManager*>(
            lpModule->GetEnvironment().GetStateManager(6));
    CGS_ASSERT(lpStreaming != 0, "lpStreamingStateManager");
    if (!lpStreaming)
        return false;
    // X360 Attach @0x8269CC60 asserts BOTH: cpp:514 "mpStreamingStateManager" and
    // cpp:515 "mpMixerControl" -- the controller must already have been attached by
    // State::CreateSFXCtrls before the effect attaches.
    CGS_ASSERT(mpMixerControl != 0, "mpMixerControl");
    // X360 @0x8269CC60: `stw 0, +4/+8` on each of the four streams (the voice-stage
    // pair) right before the inlined MusicStream::Prepare of each.
    mSecondaryStream.ResetVoiceStages();
    mEATraxStream.ResetVoiceStages();
    mMusicStreamMenu.ResetVoiceStages();
    mJunkyardStream.ResetVoiceStages();
    // Request priorities as the console's inlined Prepares seed them (@0x8269CC60:
    // `stw 5,0xA0 / stw 5,0x100 / stw 5,0x160 / stw 0,0x1C0`): 5, 5, 5, junkyard 0.
    mSecondaryStream.Prepare(lpModule, lpStreaming, "MusicFiltVoiceSpec", 5);
    mEATraxStream.Prepare(lpModule, lpStreaming, "MusicFiltVoiceSpec", 5);
    mMusicStreamMenu.Prepare(lpModule, lpStreaming, "MusicFiltVoiceSpec", 5);
    mJunkyardStream.Prepare(lpModule, lpStreaming, "MusicFiltVoiceSpec", 0);
    // X360 0x8269CE48..0x8269CE50 (inlined JumpHpf::Construct): state CONSTRUCTED,
    // start frequency flt_820AA558 = 230 Hz, current frequency flt_82001CC0 = 0.
    mJumpHpf.Construct();
    return mEaTraxData.Prepare(lpModule);
}

// X360 0x826BB9B0.
//     if (dword_830080AC != name) return name;                       // not the "intro" sentinel
//     if (!SpeechEffect::GetSpeechMapping(module, name, &spec)) return name;
//     languagestreamconfiguration ls(spec);  assert(IsValid)  assert(Num_ContentSpecs == 6)
//     cs = ls.ContentSpecs[meLanguage];  return cs ? cs : name;
// The GetSpeechMapping the console calls is `streammappings(GlobalData.StreamMappings())
// .Find(name, spec)` (SpeechEffect::GetSpeechMapping @0x8269E918); it is inlined here by
// value because the tree keeps that helper as a SpeechEffect member.
u32 MusicEffect::GetStreamFromVideoName(u32 auName) const
{
    static const u32 KU_INTRO_VIDEO_NAME =
        static_cast<u32>(CgsSound::Playback::Name::MakeHash("intro"));   // dword_830080AC
    if (auName != KU_INTRO_VIDEO_NAME)
        return auName;
    const Module::SoundLogicModule* lpModule =
        static_cast<const Module::SoundLogicModule*>(mpLogicModule);
    CGS_ASSERT(lpModule != 0, "lpLogicModule");
    if (!lpModule)
        return auName;
    Attrib::RefSpec lConfiguration;
    Attrib::Gen::streammappings lMappings(lpModule->GetGlobalData().StreamMappings());
    if (!lMappings.Find(auName, lConfiguration))
        return auName;
    Attrib::Gen::languagestreamconfiguration lLanguageStream(lConfiguration);
    CGS_ASSERT(lLanguageStream.IsValid(), "lLanguageStream.IsValid()");
    // (The console's second assert, Num_ContentSpecs() == eLanguage::Count (6), is the
    // bound ContentSpec() applies itself: an out-of-range index reads the zeroed default.)
    const u32 luContentSpec = lLanguageStream.ContentSpec(static_cast<u32>(meLanguage));
    if (MusicDiagEnabled())
        MusicDiagPrintf("[music]   video name 0x%08X -> language %d stream 0x%08X\n",
                        auName, meLanguage, luContentSpec);
    if (!luContentSpec)
        return auName;
    return luContentSpec;
}

// X360 0x8269CE70. The picture-paradise camera wins over everything; then any pending
// one-shot type Notify pushed (menu / showtime / car unlocked); then the junkyard
// (which plays its own ambience stream, so the music type is NONE); otherwise the
// running game mode and its phase decide.
s32 MusicEffect::GetMusicType(const void* apGameModeInterface) const
{
    // X360 first test: `if (CameraControl::GetCameraModeFromLogicModule(mpLogicModule)
    // == 2) return 6;` -- the picture-paradise (photo mode) camera. CameraControl in
    // this tree is the MINIMAL boot-trace home (BrnCameraControl.h: "GetCameraMode /
    // GetCameraModeFromLogicModule / GetEventSnapshot ... DEFERRED"), so that test has
    // nothing to call yet. Consequence, stated rather than hidden: music type 6 is
    // unreachable here, so the classical picture-paradise arm below never runs.
    if (mePendingMusicType)
        return mePendingMusicType;
    if (meJunkyardAmbience)
        return E_MUSIC_TYPE_NONE;

    // GameModeOutputInterface +0x08 == the game mode, +0x0C == its phase. Read by the
    // same memcpy-off-mData idiom CollisionStateManager::MapGameModesToBinFlags uses.
    s32 liGameMode = -1;
    s32 liPhase = 0;
    if (apGameModeInterface)
    {
        const u8* lpuBytes = static_cast<const u8*>(apGameModeInterface);
        std::memcpy(&liGameMode, lpuBytes + 8, sizeof(liGameMode));
        std::memcpy(&liPhase, lpuBytes + 12, sizeof(liPhase));
    }

    switch (liGameMode)
    {
    case -1:
    case 15:
        return E_MUSIC_TYPE_EATRAX_FREEBURN;
    case 0:
    case 4:
    case 9:
        if (liPhase == 0)
            return E_MUSIC_TYPE_EVENT_START;
        break;
    case 3:
    case 5:
    case 7:
    case 8:
        if (liPhase == 0 || liPhase == 1)
            return E_MUSIC_TYPE_EVENT_START;
        break;
    case 10:
        break;
    case 2:
        return (liPhase == 3 || liPhase == 4) ? E_MUSIC_TYPE_EVENT_END
                                              : E_MUSIC_TYPE_NONE;
    default:
        return E_MUSIC_TYPE_NONE;
    }
    if (liPhase == 2)
        return E_MUSIC_TYPE_EATRAX_IN_GAME;
    return (liPhase == 3 || liPhase == 4) ? E_MUSIC_TYPE_EVENT_END : E_MUSIC_TYPE_NONE;
}

// X360 0x8269D260.
void MusicEffect::UpdateSongs()
{
    if (!mbPlaylistChanged || mEaTraxData.miCurrentSong == -1)
    {
        mbPlaylistChanged = false;
        return;
    }
    const u32 luCurrent = static_cast<u32>(mEaTraxData.miCurrentSong);
    if (meMusicType == E_MUSIC_TYPE_EATRAX_IN_GAME ||
        meMusicType == E_MUSIC_TYPE_EATRAX_FREEBURN)
    {
        const CgsContainers::FastBitArray<128>& lrEnabled =
            (meMusicType == E_MUSIC_TYPE_EATRAX_FREEBURN)
                ? mEaTraxData.mEnabledSongsNoGameMode : mEaTraxData.mEnabledSongs;
        if (!mEaTraxData.mRemainingSongs.IsBitSet(luCurrent) ||
            !lrEnabled.IsBitSet(luCurrent))
        {
            mEATraxStream.StopAndUnqueue(2.0f);
            if (MusicDiagEnabled())
                MusicDiagPrintf("[music] playlist dropped current song %d -> stop EATrax\n",
                            mEaTraxData.miCurrentSong);
        }
    }
    mEaTraxData.mRemainingSongs.UnSetBit(luCurrent);
    mbPlaylistChanged = false;
}

// ---- UpdatePreviewTrack @0x826F6FD0 ---------------------------------------------
// ⭐ THE EA TRAX MENU'S AUDITION, AND IT WAS A SILENT DROP. The whole chain into this
// function already existed and was bodied -- CrashNavTrax posts GUI event 460,
// SoundLogicModule::ProcessGuiEvents case 460 turns it into sound message 9, and Notify's
// case 9 stores miPreviewSong / miPreviousPreviewSong / mbPreviewActive. This is the body
// that turns that stored state into a queued stream, and it had no definition anywhere in
// the tree, so the state was captured every time and then dropped. Pressing RB on the EA
// Trax tab did nothing.
//
// The function is a no-op on every frame except the one where the previewed track CHANGES
// (miPreviousPreviewSong != miPreviewSong), which is why UpdateParams can call it
// unconditionally.
void MusicEffect::UpdatePreviewTrack(bool abCustomSoundtrack)
{
    // ⚠️ THE AUDITION PLAYS ON THE *MENU* STREAM, NOT THE EA TRAX STREAM, and that is
    // asm-pinned rather than inferred. Every stream this body touches is r31+0x110, and
    // MusicEffect::UpdateParams -- which calls this with `mr r3, r30` @0x826FE7EC, i.e. the
    // SAME base -- writes the three mbStreamPaused bytes (+0x59) at r30+0xA9 / 0x109 /
    // 0x1C9, which the committed note at the head of this file already attributes to
    // mSecondaryStream / mEATraxStream / mJunkyardStream. Those put the four stream bases at
    // r30+0x50 / 0xB0 / 0x110 / 0x170, so r31+0x110 is the THIRD one: mMusicStreamMenu.
    // It has to be: previewing on mEATraxStream would fight the in-game playlist for the
    // same slot, and the preview is a front-end audition.
    //
    // A custom soundtrack owns the menu stream unless it is allowed over it: kill any
    // audition and leave. (X360 `if (!a2 || *(r31 + 0x24A)) goto <main>`; r31 is the -4 base
    // this file's other bodies use, so that byte is this+0x246 == mbMenuStreamOverCustom.)
    if (abCustomSoundtrack && !mbMenuStreamOverCustom)
    {
        if (mMusicStreamMenu.IsPlayingOrQueued())
        {
            mMusicStreamMenu.StopAndUnqueue(0.0f);
            return;
        }
    }

    if (mEaTraxData.miPreviousPreviewSong == mEaTraxData.miPreviewSong)
    {
        return;
    }

    // The previewed track changed: fade the previous one out over a second.
    mMusicStreamMenu.StopAndUnqueue(1.0f);

    const s32 liPreviewSong = mEaTraxData.miPreviewSong;

    // -1 is the menu's "stop previewing" value (CrashNavTrax::PreviewTrack posts it on
    // ACCEPT and on the cancel flow), and a custom soundtrack never auditions.
    if (liPreviewSong != -1 && !abCustomSoundtrack)
    {
        Module::SoundLogicModule* lpModule =
            static_cast<Module::SoundLogicModule*>(mpLogicModule);
        CGS_ASSERT(lpModule, "lpLogicModule");   // BrnMusicEffect.cpp:1755

        // The console hands the RefSpec straight to each generated ctor (its Instance base
        // takes one); this tree's generated ctors take the resolved Collection, so it is
        // resolved here -- the committed UpdateParams idiom a few hundred lines above.
        Attrib::Gen::songlist lSongList(
            const_cast<Attrib::Collection*>(
                const_cast<Attrib::RefSpec&>(
                    lpModule->GetGlobalData().SongList()).GetCollection()), 0);
        Attrib::RefSpec* lpSongSpec = reinterpret_cast<Attrib::RefSpec*>(
            lSongList.Songs(static_cast<u32>(liPreviewSong)));
        Attrib::Gen::song lSong(
            lpSongSpec ? const_cast<Attrib::Collection*>(lpSongSpec->GetCollection()) : 0, 0);

        const char* lpcStream = lSong.Stream();
        const u32 luContentSpec = lpcStream
            ? static_cast<u32>(CgsSound::Playback::Name::MakeHash(lpcStream)) : 0u;

        // ⚠️ THE OUTPUT SLOT IS THE PREVIEW FLAG, INVERTED. X360 0x826F7150..0x826F7170
        // reads mbPreviewActive, turns it into 0 or -1, clears bit 3 (`rlwinm r11,r11,0,
        // 29,27`) and adds 10 -- i.e. slot 1 when the preview is active and slot 10 when it
        // is not. Slot 1 is the menu-music output the preview is meant to be heard on.
        mMusicStreamMenu.Queue(luContentSpec,
                            static_cast<u8>(mbPreviewActive ? 1 : 10));

        // [DIAG] NOT IN THE X360 BINARY -- opt-in witness (BRN_MUSIC_DIAG=1) that the EA
        // Trax menu's audition reached the stream, which is the whole point of this body.
        if (MusicDiagEnabled())
        {
            MusicDiagPrintf("[music] PREVIEW track %d stream='%s' spec=0x%08X -> Queue(MenuStream, "
                            "slot %d)\n", liPreviewSong, lpcStream ? lpcStream : "<null>",
                            luContentSpec, mbPreviewActive ? 1 : 10);
        }

        // Tell the GUI what is auditioning: 24 bytes on the module's GuiOut queue as event
        // 502 -- the "now playing" chyron BrnGuiAlwaysAvailableComponentsManager's case 502
        // drives. The payload is the playlist's remaining-songs mask followed by the track
        // index and a set flag (X360 copies this+0x1EC/0x1F4 then stores the index at +0x10
        // and 1 at +0x14).
        // The record type moved to its DWARF-named home (Gui/Events/BrnGuiEventAudioTrax.h)
        // when case 502 was wired up, so producer and consumer share ONE definition. The
        // +0x14 byte was locally called `mbPlaying` here; the DWARF calls it mbPreview, and
        // the consumer settles it -- a set byte means "audition, do not persist to profile".
        // This body only posts from the preview path, so 1 is correct either way.
        BrnGui::GuiEATraxNewTrackEvent lRecord;
        lRecord.mRemainingSongs = mEaTraxData.mRemainingSongs;
        lRecord.miSongIndex     = liPreviewSong;
        lRecord.mbPreview       = 1;

        CgsModule::VariableEventQueue<256, 16>* lpGuiOut =
            reinterpret_cast<CgsModule::VariableEventQueue<256, 16>*>(
                lpModule->GetPreUpdateOutput().maGuiOutEventQueueStorage);
        lpGuiOut->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRecord), 502, 24);
    }

    // Latch, so the change is handled exactly once. The console spells it as a swap
    // (`lwz r11,0x200 ; stw r30,0x200 ; stw r11,0x204`) whose net effect is
    // miPreviousPreviewSong = miPreviewSong, because r30 IS miPreviewSong.
    const s32 liOld = mEaTraxData.miPreviewSong;
    mEaTraxData.miPreviewSong         = liPreviewSong;
    mEaTraxData.miPreviousPreviewSong = liOld;
}


// X360 0x8269CFC0.
u32 MusicEffect::GetEventStartContentSpec(const void* apGameModeInterface)
{
    CGS_ASSERT(apGameModeInterface != 0, "lpGameModeInterface");
    s32 liGameMode = -1;
    if (apGameModeInterface)
        std::memcpy(&liGameMode, static_cast<const u8*>(apGameModeInterface) + 8,
                    sizeof(liGameMode));
    const char* lpcName = "RaceStart0";
    switch (liGameMode)
    {
    case 0:
    case 4:  lpcName = "RaceStartRolling"; break;
    case 3:  lpcName = "RoadRageStart";    break;
    case 5:  lpcName = "car_chal_start";   break;
    case 7:  lpcName = "StuntStart";       break;
    case 8:  lpcName = "marked_man";       break;
    case 10:
        CGS_ASSERT(false, "online race is a special case");
        lpcName = "RaceStartOnline";
        break;
    default: break;
    }
    return static_cast<u32>(CgsSound::Playback::Name::MakeHash(lpcName));
}

// X360 0x8269D0F0. mbEventWon picks the win/lose family; the mode picks the variant.
u32 MusicEffect::GetEventEndContentSpec(const void* apGameModeInterface) const
{
    CGS_ASSERT(apGameModeInterface != 0, "lpGameModeInterface");
    s32 liGameMode = -1;
    if (apGameModeInterface)
        std::memcpy(&liGameMode, static_cast<const u8*>(apGameModeInterface) + 8,
                    sizeof(liGameMode));
    const char* lpcName;
    if (mbEventWon)
    {
        lpcName = (liGameMode == 3) ? "RR_win"
                : (liGameMode == 5) ? "car_chal_win" : "Race_Win";
    }
    else
    {
        lpcName = (liGameMode == 3) ? "RR_lose" : "Race_Lose";
    }
    return static_cast<u32>(CgsSound::Playback::Name::MakeHash(lpcName));
}

// X360 0x826FE5C8. The music state machine: compute the type, let the playlist settle,
// then drive the four streams. Was four bare MusicStream::Update() calls before this
// wave, which is why EA Trax never played (no song was ever selected or queued).
void MusicEffect::UpdateParams(f32 afDeltaTime)
{
    const bool lbCustomSoundtrack = IsCustomSoundtrackActive();

    Module::SoundLogicModule* lpModule =
        static_cast<Module::SoundLogicModule*>(mpLogicModule);
    CGS_ASSERT(lpModule != 0, "lpLogicModule");
    if (!lpModule)
        return;
    Module::Io::LogicInputBuffer* lpInput = lpModule->GetBrnInputStructure();
    CGS_ASSERT(lpInput != 0, "mpBrnLogicInputBuffer");
    if (!lpInput)
        return;
    const void* lpGameMode = lpInput->GetGameModeInterface();
    CGS_ASSERT(lpGameMode != 0, "lpGameModeInterface");

    mePrevMusicType = meMusicType;
    meMusicType = GetMusicType(lpGameMode);
    if (MusicDiagForcedType() >= 0)   // [DIAG] NOT IN THE X360 BINARY -- positive control
        meMusicType = MusicDiagForcedType();

    static bool sbFirstUpdateParams = true;
    if (MusicDiagEnabled() && (sbFirstUpdateParams || meMusicType != mePrevMusicType))
    {
        sbFirstUpdateParams = false;
        s32 liGameMode = -1, liPhase = 0;
        if (lpGameMode)
        {
            std::memcpy(&liGameMode, static_cast<const u8*>(lpGameMode) + 8, 4);
            std::memcpy(&liPhase, static_cast<const u8*>(lpGameMode) + 12, 4);
        }
        MusicDiagPrintf("[music] type %d -> %d (gamemode=%d phase=%d pending=%d junkyard=%d)\n",
                    mePrevMusicType, meMusicType, liGameMode, liPhase,
                    mePendingMusicType, meJunkyardAmbience);
    }

    // X360 0x826FE6D8..0x826FE778: a crash closes the jump filter, then it ticks.
    //   idx = iface+0x2858 (mePlayerActiveRaceCarIndex)
    //   if (idx != -1 && car[idx]+0x77B /*mbIsFatalyCrashing*/) -> Release(CLOSE_TIME)
    //   else if (idx != -1 && car[idx]+0x77A /*mbCrashing*/)     -> Release(CLOSE_TIME)
    //   JumpHpf::Update(dt)
    {
        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpcIface =
            lpInput->GetVehicleInterface();
        const bool lbPlayerCrashing =
            lpcIface && (lpcIface->IsPlayerCarFatalyCrashing() || lpcIface->IsPlayerCarCrashing());
        if (lbPlayerCrashing)
            mJumpHpf.Release(KF_JUMP_HPF_CLOSE_TIME);
        mJumpHpf.Update(afDeltaTime);
    }

    // X360 0x826FE5C8 (`v7[265] = v21; v7[169] = v21; v7[457] = v21;`): the same
    // "streams paused" byte goes into mEATraxStream / mSecondaryStream / mJunkyardStream
    // (+0x59 in each, mbStreamPaused). Its source is the module's dispatch state block
    // at +0x13570 -- see SoundLogicModule::AreMusicStreamsPaused.
    const bool lbStreamsPaused = lpModule->AreMusicStreamsPaused();
    mEATraxStream.SetStreamPaused(lbStreamsPaused);
    mSecondaryStream.SetStreamPaused(lbStreamsPaused);
    mJunkyardStream.SetStreamPaused(lbStreamsPaused);

    UpdateSongs();
    UpdatePreviewTrack(lbCustomSoundtrack);   // X360 `bl 0x826F6FD0` @0x826FE7F0

    switch (meMusicType)
    {
    case E_MUSIC_TYPE_NONE:
        if (mePrevMusicType == E_MUSIC_TYPE_EATRAX_IN_GAME ||
            mePrevMusicType == E_MUSIC_TYPE_EATRAX_FREEBURN)
        {
            mEATraxStream.PauseWithFade(2.0f);
        }
        break;

    case E_MUSIC_TYPE_EATRAX_IN_GAME:
    case E_MUSIC_TYPE_EATRAX_FREEBURN:
        if (lbCustomSoundtrack)
        {
            if (mEATraxStream.IsPlayingOrQueued())
                mEATraxStream.StopAndUnqueue(0.0f);
            break;
        }
        if (mePrevMusicType != meMusicType)
        {
            // Entering EA Trax: if the song we were on is no longer enabled for the new
            // type, drop it; and coming out of the event-end sting, stop that too.
            const CgsContainers::FastBitArray<128>& lrEnabled =
                (meMusicType == E_MUSIC_TYPE_EATRAX_FREEBURN)
                    ? mEaTraxData.mEnabledSongsNoGameMode : mEaTraxData.mEnabledSongs;
            if (mEaTraxData.miCurrentSong < 0 ||
                !lrEnabled.IsBitSet(static_cast<u32>(mEaTraxData.miCurrentSong)))
            {
                mEATraxStream.StopAndUnqueue(2.0f);
            }
            if (mePrevMusicType == E_MUSIC_TYPE_PICTURE_PARADISE)
                mSecondaryStream.StopAndUnqueue(0.1f);
        }
        // ⭐ X360 UpdateParams @0x826FE5C8, case 1/14 (pseudocode lines 340-344):
        //     v48 = *(a1 + 136);                      // mSecondaryStream.meState
        //     v49 = v48 == 1 || v48 == 4 || *(a1 + 166);  // Secondary.IsPlayingOrQueued()
        //     if ( !v49 ) *(a1 + 260) = 0;            // mEATraxStream.mbInternalPause = false
        // +260 is the EA TRAX stream's mbInternalPause (its base +88); +261 is
        // mbStreamPaused, which the prologue above already writes for all three streams
        // every frame. This arm used to call SetStreamPaused(false) -- the WRONG BYTE, and
        // the only writer of mbInternalPause anywhere is MusicStream::Update's
        // "new song from E_STOPPED" arm. So once any sting ducked EA Trax with
        // PauseWithFade(0.1f) (event start/end, car unlocked, picture paradise, showtime)
        // the EA Trax voice's PauseControl stayed at 1 FOR THE REST OF THE SESSION: the
        // stream still streamed and still had send gain, and was simply never unpaused
        // again. Measured: run 2 t=18..160 s sat at RMS ~300 with
        // `[sndstream] gain slot=1 ... state=1 pause=1`, against RMS ~1800 in the
        // no-event run 1.
        if (!mSecondaryStream.IsPlayingOrQueued())
            mEATraxStream.SetInternalPaused(false);
        // X360 line 348-353: the select-song gate is `!EATrax.IsPlayingOrQueued() &&
        // !Secondary.IsPlayingOrQueued() && !v22`, where v22 is the same "streams paused"
        // flag the prologue publishes -- a paused mix must not start a new song.
        if (!mEATraxStream.IsPlayingOrQueued() && !mSecondaryStream.IsPlayingOrQueued() &&
            !lbStreamsPaused)
        {
            // The console passes the RefSpec straight in (its Instance base ctor
            // resolves it); this tree's generated ctors take the resolved Collection,
            // so the RefSpec is resolved here -- the committed idiom from
            // BrnVehicleState.cpp:151. An unresolved spec yields a null collection and
            // songlist's own DefaultDataArea, i.e. Num_Songs() == 0, which is exactly
            // what the console does before BurnoutGlobalData.bin is bound.
            Attrib::Gen::songlist lSongList(
                const_cast<Attrib::Collection*>(
                    const_cast<Attrib::RefSpec&>(
                        lpModule->GetGlobalData().SongList()).GetCollection()), 0);
            const s32 liNumSongs = lSongList.Num_Songs();
            s32 liTrack = mEaTraxData.SelectSong(liNumSongs, meMusicType);
            if (liTrack == -1)
            {
                // Playlist exhausted: refill mRemainingSongs and ask once more (the
                // X360 sets both 64-bit fields to all-ones inline here).
                mEaTraxData.mRemainingSongs.SetAll();
                liTrack = mEaTraxData.SelectSong(liNumSongs, meMusicType);
            }
            if (liTrack != -1)
            {
                mEaTraxData.SetCurrentSong(liTrack);
                // songlist::Songs returns a pointer INTO the attribute data: one
                // 0x18-byte Song record, which is an Attrib::RefSpec (same 24-byte
                // shape; songlist.cpp's own out-of-range fallback is
                // DefaultDataArea(0x18)). The console hands it straight to song::song
                // because its Instance base ctor takes a RefSpec (sub_8280A248 @
                // 0x8280A248 -> RefSpec::GetCollection); this tree's generated ctors
                // take the resolved Collection, so it is resolved here.
                Attrib::RefSpec* lpSongSpec = reinterpret_cast<Attrib::RefSpec*>(
                    lSongList.Songs(static_cast<u32>(liTrack)));
                Attrib::Gen::song lSong(
                    lpSongSpec ? const_cast<Attrib::Collection*>(
                                     lpSongSpec->GetCollection()) : 0, 0);
                const char* lpcStream = lSong.Stream();
                const u32 luContentSpec = lpcStream
                    ? static_cast<u32>(CgsSound::Playback::Name::MakeHash(lpcStream)) : 0u;
                mEATraxStream.Queue(luContentSpec, 1);

                // ⭐ THE IN-GAME "NOW PLAYING" POST (X360 @0x826FECCC..0x826FECF4). This is
                // the arm that actually puts the chyron on screen during play; the preview
                // post further down only fires while auditioning in the EA Trax menu, which
                // is why the overlay never appeared in game. Immediately after the Queue:
                //   826FECC4  addi r10, r18, 0x1EC  ; &mEaTraxData.mRemainingSongs
                //   826FECCC  li   r6, 0x18         ; 24-byte record
                //   826FECD0  li   r5, 0x1F6        ; GUI event 502
                //   826FECE4/E8  std/std            ; the 128-bit mask copied in two halves
                //   826FECEC  stw  r31, 0xE0(r1)    ; +0x10 miSongIndex == liTrack
                //   826FECF0  stb  r16, 0xE4(r1)    ; +0x14 mbPreview == 0 (li r16,0 @0x826FE6D4)
                // mbPreview is ZERO here -- that is what lets the consumer's tail persist the
                // track to the profile, which an audition must not do.
                {
                    BrnGui::GuiEATraxNewTrackEvent lNewTrack;
                    lNewTrack.mRemainingSongs = mEaTraxData.mRemainingSongs;
                    lNewTrack.miSongIndex     = liTrack;
                    lNewTrack.mbPreview       = 0;

                    CgsModule::VariableEventQueue<256, 16>* lpGuiOut =
                        reinterpret_cast<CgsModule::VariableEventQueue<256, 16>*>(
                            lpModule->GetPreUpdateOutput().maGuiOutEventQueueStorage);
                    lpGuiOut->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lNewTrack), 502, 24);
                }

                if (MusicDiagEnabled())
                    MusicDiagPrintf("[music] select song %d/%d type=%d stream='%s' spec=0x%08X "
                                "-> Queue(EATrax, slot 1) + GUI 502 now-playing\n",
                                liTrack, liNumSongs, meMusicType,
                                lpcStream ? lpcStream : "<null>", luContentSpec);
            }
            else if (MusicDiagEnabled())
            {
                MusicDiagPrintf("[music] NO SONG SELECTABLE (numsongs=%d type=%d) -- the "
                            "playlist masks are empty\n", liNumSongs, meMusicType);
            }
        }
        break;

    case E_MUSIC_TYPE_EVENT_START:
        if (mePrevMusicType != meMusicType)
        {
            mSecondaryStream.StopAndUnqueue(0.0f);
            mEATraxStream.PauseWithFade(0.1f);
            const u32 luSpec = GetEventStartContentSpec(lpGameMode);
            mSecondaryStream.Queue(luSpec, 2);
            if (MusicDiagEnabled())
                MusicDiagPrintf("[music] event start sting spec=0x%08X -> Queue(Secondary, slot 2)\n",
                            luSpec);
        }
        break;

    case E_MUSIC_TYPE_EVENT_END:
        if (mbEventEndPending)
        {
            mSecondaryStream.StopAndUnqueue(0.0f);
            mEATraxStream.PauseWithFade(0.1f);
            u32 luSpec;
            if (meEventEndResult > 0 && meEventEndResult <= 4)
                luSpec = static_cast<u32>(CgsSound::Playback::Name::MakeHash(
                    KA_EVENT_END_RANK_NAMES[meEventEndResult]));
            else
                luSpec = GetEventEndContentSpec(lpGameMode);
            mSecondaryStream.Queue(luSpec, 3);
            mbEventEndPending = false;
            if (MusicDiagEnabled())
                MusicDiagPrintf("[music] event end sting rank=%d won=%d spec=0x%08X "
                            "-> Queue(Secondary, slot 3)\n",
                            meEventEndResult, mbEventWon ? 1 : 0, luSpec);
        }
        break;

    case E_MUSIC_TYPE_JUNKYARD:
        break;

    case E_MUSIC_TYPE_CAR_UNLOCKED:
        if (mePrevMusicType != meMusicType)
        {
            mSecondaryStream.StopAndUnqueue(0.1f);
            mEATraxStream.PauseWithFade(0.1f);
            CGS_ASSERT(mCarUnlockName != KU_NULL_NAME,
                       "mCarUnlockName != CgsSound::Playback::K_NULL_NAME");
            mSecondaryStream.Queue(mCarUnlockName, 5);
            if (MusicDiagEnabled())
                MusicDiagPrintf("[music] car-unlocked sting spec=0x%08X -> Queue(Secondary, slot 5)\n",
                            mCarUnlockName);
            mCarUnlockName = KU_NULL_NAME;
        }
        mePendingMusicType = E_MUSIC_TYPE_NONE;
        break;

    case E_MUSIC_TYPE_PICTURE_PARADISE:
        if (lbCustomSoundtrack)
        {
            if (mSecondaryStream.IsPlayingOrQueued())
                mSecondaryStream.StopAndUnqueue(0.0f);
            break;
        }
        if (mePrevMusicType != meMusicType)
        {
            mSecondaryStream.StopAndUnqueue(0.0f);
            mEATraxStream.PauseWithFade(0.1f);
        }
        if (!mSecondaryStream.IsPlayingOrQueued())
        {
            CGS_ASSERT(miPicParadiseMusic < KI_NUM_PICTURE_PARADISE_NAMES,
                       "miPicParadiseMusic < KI_NUM_PICTURE_PARADISE_NAMES");
            const ClassicalMusicData& lrTrack =
                KA_CLASSICAL_MUSIC_DATA[miPicParadiseMusic % KI_NUM_PICTURE_PARADISE_NAMES];
            CGS_ASSERT(lrTrack.mpcStream != 0,
                       "KA_CLASSICAL_MUSIC_DATA[ miPicParadiseMusic ].mpcStream");
            mSecondaryStream.Queue(
                static_cast<u32>(CgsSound::Playback::Name::MakeHash(lrTrack.mpcStream)), 6);
            // X360: a 100-byte GUI-out event 503 carrying { index, composer text id,
            // work text id } goes out here so the photo HUD can caption the piece.
            // Its producer side is the GuiOut queue this effect does not yet reach.
            if (MusicDiagEnabled())
                MusicDiagPrintf("[music] picture paradise '%s' (%s / %s) -> Queue(Secondary, slot 6)\n",
                            lrTrack.mpcStream, lrTrack.mpcComposerTextId,
                            lrTrack.mpcWorkTextId);
            miPicParadiseMusic = (miPicParadiseMusic + 1) % KI_NUM_PICTURE_PARADISE_NAMES;
        }
        break;

    case E_MUSIC_TYPE_SHOWTIME:
        if (mePrevMusicType != meMusicType)
        {
            mSecondaryStream.StopAndUnqueue(0.0f);
            mEATraxStream.PauseWithFade(0.1f);
            mSecondaryStream.Queue(
                static_cast<u32>(CgsSound::Playback::Name::MakeHash("ShowtimeStart")), 11);
            if (MusicDiagEnabled())
                MusicDiagPrintf("[music] showtime start -> Queue(Secondary, slot 11)\n");
        }
        mePendingMusicType = E_MUSIC_TYPE_NONE;
        break;

    case E_MUSIC_TYPE_MENU:
        if (!mMusicStreamMenu.IsPlayingOrQueued() &&
            (!lbCustomSoundtrack || mbMenuStreamOverCustom))
        {
            mMusicStreamMenu.StopAndUnqueue(0.1f);
            CGS_ASSERT(mMenuStreamName != KU_NULL_NAME,
                       "mMenuStreamName != CgsSound::Playback::K_NULL_NAME");
            mMusicStreamMenu.Queue(mMenuStreamName, 12);
            // X360 @0x826FF68C..690: `stw r16(=0), 0x1FC(r18); stw r16, 0x200(r18)` -- the two
            // preview cursors are ZEROED here (Notify's clear arm is the one that writes -1).
            mEaTraxData.miPreviewSong = 0;
            mEaTraxData.miPreviousPreviewSong = 0;
            if (MusicDiagEnabled())
                MusicDiagPrintf("[music] menu stream spec=0x%08X -> Queue(Menu, slot 12)\n",
                            mMenuStreamName);
        }
        // X360 @0x826FF694..6D0:
        //     if (menu.prevVoiceStage == menu.voiceStage || menu.voiceStage == 6)
        //         AddEvent(module + 0x4EB0 /*PreUpdateOutput GuiOut*/, &byte, 504, 1);
        // GuiOut 504 is the MovieManager's "audio ready" (RecvEvent @0x824F9688: 6 -> 7);
        // the GuiModule feeds it from the sound module's GuiOut queue every frame.
        if (mMusicStreamMenu.IsVoiceStageSettledOrPlaying())
        {
            u8 lu8AudioReady = 0;
            CgsModule::VariableEventQueue<256, 16>* lpGuiOut =
                reinterpret_cast<CgsModule::VariableEventQueue<256, 16>*>(
                    lpModule->GetPreUpdateOutput().maGuiOutEventQueueStorage);
            lpGuiOut->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lu8AudioReady),
                               504, 1);
        }
        break;

    default:
        CGS_ASSERT(false, "UpdateParams : Unexpected type");
        break;
    }

    mEATraxStream.Update(afDeltaTime);
    mSecondaryStream.Update(afDeltaTime);
    mMusicStreamMenu.Update(afDeltaTime);
    mJunkyardStream.Update(afDeltaTime);
}

void MusicEffect::ProcessUpdate()
{
    using Nicotine::DMixIO;

    // X360 0x826F6D70 opens with `if (mbHoldVolumes) return;` -- message 15 freezes the
    // mixer sends the effect publishes (used over transitions that must not re-duck).
    if (mbHoldVolumes)
        return;

    mEATraxStream.SetVolume(
        GetRWACMixerOutputValue(mEATraxStream.GetOutputSlot(), DMixIO::DMX_VOL));
    // X360 0x826F6E18: `lfs f13, 0x40(r31)` -> `*(a1+248) = *(a1+64)` -- the EA Trax
    // high-pass follows the JumpHpf sub-object's current frequency (this+0x34+0xC).
    mEATraxStream.SetHighPassFreq(mJumpHpf.GetFrequency());
    mEATraxStream.SetLowPassFreq(GetRWACMixerOutputValue(8, DMixIO::DMX_FREQ));

    // ⭐ X360 ProcessUpdate @0x826F6D70, immediately after the EA Trax volume stores:
    //     v7 = *(a1 + 232);                                   // mEATraxStream.meState
    //     if ( v7 == 1 || v7 == 4 || *(a1 + 262) ) v8 = 1;    // ... || mbSongQueued
    //     if ( !v8 ) goto LABEL_19;                           // -> v10 = 0
    //     if ( *(a1 + 261) || *(a1 + 260) ) goto LABEL_19;    // mbStreamPaused | mbInternalPause
    //     v10 = 0x7FFF * *(*(a1 + 560) + 72) / 11;            // mpMixerControl's music volume
    //     CgsSound::Logic::EffectBase::SetMixerInputValue(a1, 0, v10);
    // This effect's own dynamic-mixer INPUT 0 -- "EA Trax is audible right now, at this
    // music-volume setting". It had NO producer in this tree, so the mix map's music
    // input read 0 for the whole session and every mixer output derived from it stayed
    // at its silent-music rest value.
    s32 liMusicInput = 0;
    if (mEATraxStream.IsPlayingOrQueued() && !mEATraxStream.IsPaused())
    {
        CGS_ASSERT(mpMixerControl != 0, "mpMixerControl");
        if (mpMixerControl)
            liMusicInput = 0x7FFF * mpMixerControl->GetMusicVolumeForMixer() / 11;
    }
    SetMixerInputValue(0, liMusicInput);

    mSecondaryStream.SetVolume(
        GetRWACMixerOutputValue(mSecondaryStream.GetOutputSlot(), DMixIO::DMX_VOL));
    mSecondaryStream.SetHighPassFreq(0.0f);
    mSecondaryStream.SetLowPassFreq(96000.0f);

    mMusicStreamMenu.SetVolume(
        GetRWACMixerOutputValue(mMusicStreamMenu.GetOutputSlot(), DMixIO::DMX_VOL));
    mMusicStreamMenu.SetHighPassFreq(0.0f);
    mMusicStreamMenu.SetLowPassFreq(96000.0f);

    mJunkyardStream.SetVolume(
        GetRWACMixerOutputValue(mJunkyardStream.GetOutputSlot(), DMixIO::DMX_VOL));
    mJunkyardStream.SetHighPassFreq(0.0f);
    mJunkyardStream.SetLowPassFreq(96000.0f);

    // ⭐ X360 ProcessUpdate @0x826F6D70 tail:
    //     v16 = GetRWACMixerOutputValue(a1, 9, 0);
    //     if ( v16 != *(a1 + 572) ) {
    //         AddEvent(*(a1 + 40) + 20144, &v16, 513, 4);   // the module's PreUpdateOutput
    //         *(a1 + 572) = v16;                            // GuiOut queue, event 513
    //     }
    // Mixer output slot 9 is the music level the FRONT END shows; the effect republishes
    // it as GuiOut event 513 only when it CHANGES. `*(a1 + 40) + 20144` is
    // mpLogicModule + 0x4EB0 == SoundLogicModule::mPreUpdateOutput, whose first member is
    // the GuiOut VariableEventQueue<256,16>. It had no producer in this tree.
    const f32 lfGuiVolume = GetRWACMixerOutputValue(9, DMixIO::DMX_VOL);
    if (lfGuiVolume != mfLastPublishedGuiVolume)
    {
        Module::SoundLogicModule* lpGuiModule =
            static_cast<Module::SoundLogicModule*>(mpLogicModule);
        CgsModule::VariableEventQueue<256, 16>* lpGuiOut =
            reinterpret_cast<CgsModule::VariableEventQueue<256, 16>*>(
                lpGuiModule->GetPreUpdateOutput().maGuiOutEventQueueStorage);
        lpGuiOut->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lfGuiVolume),
                           513, static_cast<s32>(sizeof(f32)));
        mfLastPublishedGuiVolume = lfGuiVolume;
    }
}

// X360 0x826BBAF8. Thirteen message ids (jump table 0x826BBB64, cases 0..33 == ids
// 7..40); every other id asserts "Notify : Unexpected event".
void MusicEffect::Notify(const CgsSound::Io::MessageHeader* apMessage)
{
    CGS_ASSERT(apMessage != 0, "lpMessageHeader");
    if (!apMessage)
        return;

    const s16 li16Event = apMessage->GetEventId();
    const u8* lpuPayload =
        reinterpret_cast<const u8*>(apMessage) + sizeof(CgsSound::Io::MessageHeader);

    if (MusicDiagEnabled() && guMusicDiagMessages < KU_MUSIC_DIAG_MESSAGE_CAP)
    {
        ++guMusicDiagMessages;
        MusicDiagPrintf("[music] msg id=%d (type=%d prev=%d pending=%d)\n",
                    li16Event, meMusicType, mePrevMusicType, mePendingMusicType);
    }

    switch (li16Event)
    {
    case 7:   // E_SOUNDMESSAGE_EATRAX -- the playlist masks (GUI 458).
    {
        // X360 0x826BBBEC: payload[0x10..0x1F] -> mEnabledSongs (+0x00),
        // payload[0x00..0x0F] -> mEnabledSongsNoGameMode (+0x10), mRemainingSongs = -1.
        std::memcpy(&mEaTraxData.mEnabledSongs, lpuPayload + 16,
                    sizeof(mEaTraxData.mEnabledSongs));
        std::memcpy(&mEaTraxData.mEnabledSongsNoGameMode, lpuPayload,
                    sizeof(mEaTraxData.mEnabledSongsNoGameMode));
        mEaTraxData.mRemainingSongs.SetAll();
        mbPlaylistChanged = true;
        if (MusicDiagEnabled())
        {
            s32 liInGame = 0, liFreeBurn = 0;
            for (u32 luBit = 0; luBit < 128; ++luBit)
            {
                if (mEaTraxData.mEnabledSongs.IsBitSet(luBit)) ++liInGame;
                if (mEaTraxData.mEnabledSongsNoGameMode.IsBitSet(luBit)) ++liFreeBurn;
            }
            MusicDiagPrintf("[music]   playlist masks updated (%d songs enabled in game, "
                        "%d enabled free burn)\n", liInGame, liFreeBurn);
        }
        break;
    }
    case 8:   // E_SOUNDMESSAGE_EATRAX_LAST_PLAYED_INDEXES (GUI 459) -- restore state.
        // X360 0x826BBCD4: payload[0x00..0x0F] -> mRemainingSongs, payload+0x14 ->
        // miPicParadiseMusic (the saved classical-track cursor).
        std::memcpy(&mEaTraxData.mRemainingSongs, lpuPayload,
                    sizeof(mEaTraxData.mRemainingSongs));
        std::memcpy(&miPicParadiseMusic, lpuPayload + 20, sizeof(miPicParadiseMusic));
        break;

    case 9:   // E_SOUNDMESSAGE_EATRAX_PREVIEW (GUI 460).
        // X360 0x826BBCFC.
        mEaTraxData.miPreviousPreviewSong = mEaTraxData.miPreviewSong;
        std::memcpy(&mEaTraxData.miPreviewSong, lpuPayload,
                    sizeof(mEaTraxData.miPreviewSong));
        mbPreviewActive = (lpuPayload[4] != 0);
        break;

    case 10:  // E_SOUNDMESSAGE_EATRAX_ADVANCE_TRACK (GUI 461) -- "skip".
        if (mEATraxStream.IsPlayingOrQueued())
            mEATraxStream.StopAndUnqueue(1.0f);
        break;

    case 11:  // E_SOUNDMESSAGE_EATRAX_PLAY_ORDER (GUI 462).
        // X360 0x826BBD1C.
        std::memcpy(&mEaTraxData.mePlayOrder, lpuPayload,
                    sizeof(mEaTraxData.mePlayOrder));
        break;

    case 13:  // E_SOUNDMESSAGE_PLAY_MUSIC_ON_MENU_STREAM (GUI 23 / 469).
    {
        // X360 0x826BBEF4 reads the message at +0x10 (u32 name), +0x14 and +0x15.
        const BrnSound::MusicOnMenuStreamData& lrData =
            *reinterpret_cast<const BrnSound::MusicOnMenuStreamData*>(lpuPayload);
        const u32 luName = lrData.muStreamNameHash;
        const bool lbIsVideo = (lrData.mbFromVideo != 0);
        const bool lbOverCustom = (lrData.mbPlayOverCustomSoundtrack != 0);
        if (MusicDiagEnabled())
            MusicDiagPrintf("[music]   id 13 name=0x%08X isVideo=%d overCustom=%d "
                            "(mbMenuStreamIsVideo=%d) -> %s\n",
                            luName, lbIsVideo ? 1 : 0, lbOverCustom ? 1 : 0,
                            mbMenuStreamIsVideo ? 1 : 0,
                            (!mbMenuStreamIsVideo || lbIsVideo) ? "accepted" : "IGNORED");
        if (!mbMenuStreamIsVideo || lbIsVideo)
        {
            if (luName == KU_NULL_NAME)
            {
                mMenuStreamName = KU_NULL_NAME;
                mMusicStreamMenu.StopAndUnqueue(0.1f);
                mEaTraxData.miPreviewSong = -1;
                mEaTraxData.miPreviousPreviewSong = -1;
                mePendingMusicType = E_MUSIC_TYPE_NONE;
                mbMenuStreamIsVideo = false;
            }
            else
            {
                mePendingMusicType = E_MUSIC_TYPE_MENU;
                // X360: `mMenuStreamName = GetStreamFromVideoName(name)` @0x826BB9B0 -- a
                // pass-through unless the name is the "intro" video sentinel, which resolves
                // the localised INTRO stream through the stream mappings +
                // languagestreamconfiguration[meLanguage].
                u32 luContentSpec = GetStreamFromVideoName(luName);
                // FLAG (pre-existing, kept): the shipped title screen asks for
                // "GunsAndRoses" while the streams registry holds "Guns_And_Roses".
                const u32 luGunsAndRoses =
                    static_cast<u32>(CgsSound::Playback::Name::MakeHash("GunsAndRoses"));
                if (luName == luGunsAndRoses)
                    luContentSpec = static_cast<u32>(
                        CgsSound::Playback::Name::MakeHash("Guns_And_Roses"));
                mMenuStreamName = luContentSpec;
                mbMenuStreamIsVideo = lbIsVideo;
                mbMenuStreamOverCustom = lbOverCustom;
                if (lbIsVideo && mEATraxStream.IsPlayingOrQueued())
                    mEATraxStream.StopAndUnqueue(1.0f);
            }
        }
        break;
    }

    case 14:  // E_SOUNDMESSAGE_MUSIC_JUMP_FILTER.
    {
        // X360 0x826BBDC8..0x826BBEC8: payload byte 0 (`lbz 0x10(msg)`) is "open".
        //   open : if the player car is neither fatally crashing (car+0x77B) nor
        //          crashing (car+0x77A) -> assert(mJumpHpf.Prepare(OPEN_FREQUENCY, OPEN_TIME))
        //   close: assert(mJumpHpf.Release(CLOSE_TIME))
        if (lpuPayload[0] != 0)
        {
            Module::SoundLogicModule* lpModule =
                static_cast<Module::SoundLogicModule*>(mpLogicModule);
            const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpcIface =
                lpModule->GetBrnInputStructure()->GetVehicleInterface();
            const bool lbPlayerCrashing =
                lpcIface && (lpcIface->IsPlayerCarFatalyCrashing() || lpcIface->IsPlayerCarCrashing());
            if (!lbPlayerCrashing)
            {
                const bool lbPrepared =
                    mJumpHpf.Prepare(KF_JUMP_HPF_OPEN_FREQUENCY, KF_JUMP_HPF_OPEN_TIME);
                CGS_ASSERT(lbPrepared, "mJumpHpf.Prepare(KF_JUMP_HPF_OPEN_FREQUENCY, KF_JUMP_HPF_OPEN_TIME)");
                (void)lbPrepared;
            }
        }
        else
        {
            const bool lbReleased = mJumpHpf.Release(KF_JUMP_HPF_CLOSE_TIME);
            CGS_ASSERT(lbReleased, "mJumpHpf.Release(KF_JUMP_HPF_CLOSE_TIME)");
            (void)lbReleased;
        }
        if (MusicDiagEnabled())
            MusicDiagPrintf("[music]   id 14 (jump filter) open=%d -> hpf %.1f Hz\n",
                        lpuPayload[0] != 0, mJumpHpf.GetFrequency());
        break;
    }

    case 15:  // E_SOUNDMESSAGE_HOLD_VOLUMES.
        mbHoldVolumes = (lpuPayload[0] != 0);
        break;

    case 23:  // E_SOUND_MESSAGE_EVENT_RESULTS (game action 37's ShowModeResultsAction).
    {
        // X360 0x826BBD2C, reading the 232-byte ShowModeResultsAction payload:
        //   won = payload[0xE0] && (!payload[0xE1] || mode == 3 || mode == 7)
        //   if (payload[0xDC]) meEventEndResult = payload[0x5C]; else 0
        s32 liMode = 0;
        std::memcpy(&liMode, lpuPayload, sizeof(liMode));
        bool lbWon = false;
        if (lpuPayload[0xE0])
        {
            if (!lpuPayload[0xE1] || liMode == 3 || liMode == 7)
                lbWon = true;
        }
        if (lpuPayload[0xDC])
            std::memcpy(&meEventEndResult, lpuPayload + 0x5C, sizeof(meEventEndResult));
        else
            meEventEndResult = 0;
        mbEventWon = lbWon;
        mbEventEndPending = true;
        break;
    }

    case 24:  // E_SOUND_MESSAGE_SHOWTIME_INTRO.
        mePendingMusicType = E_MUSIC_TYPE_SHOWTIME;
        break;

    case 28:  // E_SOUND_MESSAGE_PLAY_SEQUENCE (the car-unlock sting).
    {
        const CgsSound::Io::Message<CgsSound::Playback::Name>* lpMessage =
            static_cast<const CgsSound::Io::Message<CgsSound::Playback::Name>*>(apMessage);
        mCarUnlockName = static_cast<u32>(lpMessage->mData.GetValue());
        mePendingMusicType = E_MUSIC_TYPE_CAR_UNLOCKED;
        break;
    }

    case 33:  // E_SOUNDMESSAGE_SPEECH_SET_LANGUAGE.
    {
        // X360 @0x826BBAF8 case 0x21: `meLanguage = SpeechEffect::GetLanguage(*(msg + 16))`
        // -- the CgsLanguage payload word mapped onto AttribSys eLanguage, read back by
        // GetStreamFromVideoName.
        s32 liCgsLanguage = 0;
        std::memcpy(&liCgsLanguage, lpuPayload, sizeof(liCgsLanguage));
        meLanguage = SpeechEffect::GetLanguage(liCgsLanguage);
        if (MusicDiagEnabled())
            MusicDiagPrintf("[music]   id 33 set language cgs=%d -> %d\n", liCgsLanguage, meLanguage);
        break;
    }

    case 40:  // E_SOUNDMESSAGE_IN_JUNKYARD.
    {
        s32 leAmbience = E_JUNKYARD_AMBIENCE_NONE;
        std::memcpy(&leAmbience, lpuPayload, sizeof(leAmbience));
        meJunkyardAmbience = leAmbience;
        if (leAmbience == E_JUNKYARD_AMBIENCE_NONE)
        {
            if (mJunkyardStream.IsPlayingOrQueued())
                mJunkyardStream.StopAndUnqueue(0.5f);
        }
        else if (leAmbience == E_JUNKYARD_AMBIENCE_NEW_PROFILE)
        {
            if (IsCustomSoundtrackActive())
            {
                meJunkyardAmbience = E_JUNKYARD_AMBIENCE_NONE;
            }
            else
            {
                mJunkyardStream.StopAndUnqueue(0.1f);
                mJunkyardStream.Queue(static_cast<u32>(
                    CgsSound::Playback::Name::MakeHash("JunkyardWithMusic")), 13);
            }
        }
        else if (leAmbience < E_JUNKYARD_AMBIENCE_COUNT)
        {
            mJunkyardStream.StopAndUnqueue(0.1f);
            mJunkyardStream.Queue(static_cast<u32>(
                CgsSound::Playback::Name::MakeHash("Junkyard")), 13);
        }
        else
        {
            CGS_ASSERT(false, "Unhandled junkyard ambience type");
        }
        break;
    }

    default:
        CGS_ASSERT(false, "Notify : Unexpected event ");
        break;
    }
}

} // namespace Logic
} // namespace BrnSound
