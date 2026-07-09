#pragma once

#include "types.hpp"

// BrnSound sound-message payload types + the ESoundMessages discriminant enum.
// DecFIGS DWARF home: GameSource/Sound/Module/LogicModule/BrnMessageData.h
// (namespace note "BrnSoundCommonSharedIO.h:26 namespace BrnSound"). These are the
// small POD payloads the sound logic module queues as CgsSound::Io::Message<T>; each
// size is X360-attested by the matching VariableEventQueue<*,16>::AddEvent<Message<T>>
// record-size arg (payload == arg - 16-byte MessageHeader).
namespace BrnSound
{

// BrnMessageData.h:23 (DWARF). The top-level sound-message discriminant, written into
// the MessageHeader id field. 4-byte enum -> Message<ESoundMessages> == 20 (arg 0x14).
enum ESoundMessages
{
    E_SOUNDMESSAGE_DEBUG_DRAW_SPHERE          = 1,
    E_SOUNDMESSAGE_COLLISION_BIND_TO_PROPS    = 2,
    E_SOUNDMESSAGE_GAMEACTION                 = 3,
    E_SOUNDMESSAGE_FXMESSAGE                  = 4,
    E_SOUNDMESSAGE_GUIAUDIO_EVENT             = 5,
    E_SOUNDMESSAGE_GUI_AUDIO_TRIGGER          = 6,
    E_SOUNDMESSAGE_EATRAX                     = 7,
    E_SOUNDMESSAGE_EATRAX_LAST_PLAYED_INDEXES = 8,
    E_SOUNDMESSAGE_EATRAX_PREVIEW             = 9,
    E_SOUNDMESSAGE_EATRAX_ADVANCE_TRACK       = 10,
    E_SOUNDMESSAGE_EATRAX_PLAY_ORDER          = 11,
    E_SOUNDMESSAGE_SETTINGS                   = 12,
    E_SOUNDMESSAGE_PLAY_MUSIC_ON_MENU_STREAM  = 13,
    E_SOUNDMESSAGE_MUSIC_JUMP_FILTER          = 14,
    E_SOUNDMESSAGE_HOLD_VOLUMES               = 15,
    E_SOUNDMESSAGE_QUEUE_ELEMENT              = 16,
    E_SOUNDMESSAGE_CAR_LOAD_DATA              = 17,
    E_SOUNDMESSAGE_RACE_CAR_IS_ACTIVE         = 18,
    E_SOUNDMESSAGE_RACE_CAR_IMPACT            = 19,
    E_SOUNDMESSAGE_PLAYER_CAR_DAMAGE          = 20,
    E_SOUNDMESSAGE_PLAYER_CAR_REPAIRED        = 21,
    E_SOUND_MESSAGE_PLAY_BANG                 = 22,
    E_SOUND_MESSAGE_EVENT_RESULTS             = 23,
    E_SOUND_MESSAGE_SHOWTIME_INTRO            = 24,
    E_SOUND_MESSAGE_NEW_RIVAL_SEQUENCE        = 25,
    E_SOUND_MESSAGE_NEW_CAR_UNLOCKED          = 26,
    E_SOUND_MESSAGE_RANK_UP_SEQUENCE          = 27,
    E_SOUND_MESSAGE_PLAY_SEQUENCE             = 28,
    E_SOUNDMESSAGE_GAME_MODE_STARTED          = 29,
    E_SOUNDMESSAGE_TROPHY_UNLOCK              = 30,
    E_SOUNDMESSAGE_SPEECH_INTRO               = 31,
    E_SOUNDMESSAGE_SPEECH_GAMEMODE_LOST       = 32,
    E_SOUNDMESSAGE_SPEECH_SET_LANGUAGE        = 33,
    E_SOUNDMESSAGE_SPEECH_FIRST_TIME_TIP      = 34,
    E_SOUNDMESSAGE_SPEECH_CAR_WON             = 35,
    E_SOUNDMESSAGE_SPEECH_TRIGGER_VO          = 36,
    E_SOUNDMESSAGE_SPEECH_TRIGGER_ONLINE_VO   = 37,
    E_SOUNDMESSAGE_SPEECH_FADE_STOP           = 38,
    E_SOUNDMESSAGE_ENABLE_OCCLUSION           = 39,
    E_SOUNDMESSAGE_IN_JUNKYARD                = 40,
    E_SOUNDMESSAGE_RESET_CAR_IN_JUNKYARD      = 41,
    E_SOUNDMESSAGE_FX_VOLUMES                 = 42,
    E_SOUNDMESSAGE_RESTART_MIXER              = 43,
    E_SOUNDMESSAGE_100PERCENT                 = 44,
};

// BrnMessageData.h:104 (DWARF). Two floats -> 8 bytes -> Message<FxVolumes> == 24
// (arg 0x18).
struct FxVolumes
{
    f32 mfWhineVolume;
    f32 mfTurboVolume;
};

// BrnMessageData.h:112 (DWARF). meGameMode is BrnGameState::GameStateModuleIO::
// EGameModeType (a 4-byte enum) -- modelled here as a 4-byte s32 to keep this payload
// header dependency-free (the enum value width/semantics are identical; the enum's
// own home is GameSource/GameState/BrnGameStateSharedIO.h). 8 bytes ->
// Message<GameModeLostResults> == 24 (arg 0x18).
struct GameModeLostResults
{
    s32 meGameMode;              // DWARF: EGameModeType meGameMode
    s32 miNumLossesForGameMode;
};

// BrnMessageData.h:89 (DWARF): { EntityId mID; int32 mRaceCarIndex; Attribute::Key
// mAttribKey; }. Those DWARF fields sum to 12 bytes (EntityId==u32, s32, Attribute::Key
// ==u32), but the X360 ARTIST spine attests the payload is 16 bytes -- every
// VariableEventQueue<*,16>::AddEvent<CgsSound::Io::Message<RaceCarIsNowActive>> bakes
// sizeof(Message<...>) == 0x20 == 32 == 16 (MessageHeader) + 16. Rung 1 (ARTIST asm)
// outranks the PS3 DWARF, so the payload IS 16 bytes; the 4-byte delta over the three
// DWARF-named fields is a merge-window drift whose placement is not recovered here.
// Modelled as a size-attested opaque block (per the GuiEvent payload house style)
// rather than fabricating a fourth field or an offset for the drift.
struct RaceCarIsNowActive
{
    u8 maData[16];   // DWARF names mID / mRaceCarIndex / mAttribKey; +4 drift un-placed.
};

}
