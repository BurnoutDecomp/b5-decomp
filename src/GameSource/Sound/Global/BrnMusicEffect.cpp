#include "GameSource/Sound/Global/BrnMusicEffect.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameSource/Sound/Module/BrnRootSoundModuleIo.h"
#include "GameSource/Sound/Streaming/BrnStreamingStateManager.h"
#include "GameShared/GameClasses/Sound/IO/CgsMessage.h"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"
#include "GameSource/Sound/Module/LogicModule/BrnMessageData.h"
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameSource/AttribSys/Generated/classes/songlist.h"
#include "GameSource/AttribSys/Generated/classes/song.h"
#include "SDKs/EATech/include/Nicotine/DMixIO.hpp"

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
    : BrnEffectObject(), mSecondaryStream(), mEATraxStream(), mMusicStreamMenu(),
      mJunkyardStream(), mEaTraxData(),
      mbPlaylistChanged(false), mbPreviewActive(false), meEventEndResult(0),
      mbEventWon(false), mbEventEndPending(false),
      mCarUnlockName(KU_NULL_NAME), mMenuStreamName(KU_NULL_NAME),
      meMusicType(E_MUSIC_TYPE_NONE), mePrevMusicType(E_MUSIC_TYPE_NONE),
      mePendingMusicType(E_MUSIC_TYPE_NONE),
      meJunkyardAmbience(E_JUNKYARD_AMBIENCE_NONE), miPicParadiseMusic(0),
      mbHoldVolumes(false), mbMenuStreamIsVideo(false), mbMenuStreamOverCustom(false) {}

MusicEffect::~MusicEffect() {}

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
    mSecondaryStream.Prepare(lpModule, lpStreaming, "MusicFiltVoiceSpec");
    mEATraxStream.Prepare(lpModule, lpStreaming, "MusicFiltVoiceSpec");
    mMusicStreamMenu.Prepare(lpModule, lpStreaming, "MusicFiltVoiceSpec");
    mJunkyardStream.Prepare(lpModule, lpStreaming, "MusicFiltVoiceSpec");
    return mEaTraxData.Prepare(lpModule);
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

    // X360: the JumpHpf open/close arm sits here (MusicEffect::JumpHpf::Prepare /
    // Release / Update @0x82687648 / 0x826877E0, gated on the player car being in the
    // air). NOT RECONSTRUCTED -- the JumpHpf sub-object has no declaration anywhere in
    // this tree. It only filters the music while airborne; it does not start or stop it.

    // X360 0x826FE5C8 (`v7[265] = v21; v7[169] = v21; v7[457] = v21;`): the same
    // "streams paused" byte goes into mEATraxStream / mSecondaryStream / mJunkyardStream
    // (+0x59 in each, mbStreamPaused). Its source is the module's dispatch state block
    // at +0x13570 -- see SoundLogicModule::AreMusicStreamsPaused.
    const bool lbStreamsPaused = lpModule->AreMusicStreamsPaused();
    mEATraxStream.SetStreamPaused(lbStreamsPaused);
    mSecondaryStream.SetStreamPaused(lbStreamsPaused);
    mJunkyardStream.SetStreamPaused(lbStreamsPaused);

    UpdateSongs();
    // X360: UpdatePreviewTrack(this, lbCustomSoundtrack) @0x826F6FD0 runs here -- the
    // EA Trax menu's per-track preview. NOT RECONSTRUCTED this wave (front-end only).

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
        // again.
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
                if (MusicDiagEnabled())
                    MusicDiagPrintf("[music] select song %d/%d type=%d stream='%s' spec=0x%08X "
                                "-> Queue(EATrax, slot 1)\n",
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
            mEaTraxData.miPreviewSong = -1;
            mEaTraxData.miPreviousPreviewSong = -1;
            if (MusicDiagEnabled())
                MusicDiagPrintf("[music] menu stream spec=0x%08X -> Queue(Menu, slot 12)\n",
                            mMenuStreamName);
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
    mEATraxStream.SetHighPassFreq(0.0f);
    mEATraxStream.SetLowPassFreq(GetRWACMixerOutputValue(8, DMixIO::DMX_FREQ));

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
                // X360: MusicEffect::GetStreamFromVideoName @0x826BB9B0 -- a pass-through
                // unless the name is the "video" sentinel, in which case it resolves the
                // localised stream through SpeechEffect::GetSpeechMapping +
                // Attrib::Gen::languagestreamconfiguration. Neither of those exists in this
                // tree yet, so only the pass-through arm is live here.
                u32 luContentSpec = luName;
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
        // X360 opens/closes MusicEffect::JumpHpf (0x82687648 / 0x826877E0). The JumpHpf
        // sub-object is NOT in this tree (no declaration anywhere); reconstructing it is
        // its own unit of work. Reported, not silently dropped.
        if (MusicDiagEnabled())
            MusicDiagPrintf("[music]   id 14 (jump filter) has no JumpHpf in this tree "
                        "-- X360 0x82687648/0x826877E0 not reconstructed\n");
        break;

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
        // X360 stores SpeechEffect::GetLanguage(payload) for GetStreamFromVideoName's
        // language lookup. Both of those are absent from this tree; the stored value has
        // no consumer here, so there is nothing to keep. Reported, not silently dropped.
        if (MusicDiagEnabled())
            MusicDiagPrintf("[music]   id 33 (set language) needs SpeechEffect::GetLanguage "
                        "-- not reconstructed\n");
        break;

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
