#ifndef BRN_SOUND_BRN_MIXER_DATA_H
#define BRN_SOUND_BRN_MIXER_DATA_H

// =============================================================================
// BrnSound mixer-data enumerations.
//   GameSource/Sound/BrnMixerData.h (DWARF home)
//
// Reproduced VERBATIM from references/DecFIGS/dwarfdump/GameSource/Sound/BrnMixerData.h
// -- names, values and order are the console's, not ours. Until now none of these four
// enums had a home in this tree and every consumer spelled its cases as bare integers.
//
// The DWARF line numbers below are the ORIGINAL BrnMixerData.h's, kept so the next reader
// can diff against the dump.
// =============================================================================

namespace BrnSound
{

// BrnMixerData.h:491. The music-stream selector MusicEffect drives.
enum EMusicType
{
    E_MUSIC_TYPE_NONE              = 0,
    E_MUSIC_TYPE_EA_TRAX           = 1,
    E_MUSIC_TYPE_RACE_INTRO        = 2,
    E_MUSIC_TYPE_RACE_FINISH       = 3,
    E_MUSIC_TYPE_MEDAL_AWARDED     = 4,
    E_MUSIC_TYPE_CAR_UNLOCK        = 5,
    E_MUSIC_TYPE_PICTURE_PARADISE  = 6,
    E_MUSIC_TYPE_HIGH_PASS_FREQ    = 7,
    E_MUSIC_TYPE_LOW_PASS_FREQ     = 8,
    E_MIXER_VIDEO_VOLUME           = 9,    // (sic -- the console's own odd name in this enum)
    E_MUSIC_TYPE_MENU              = 10,
    E_MUSIC_TYPE_SHOWTIME_INTRO    = 11,
    E_MUSIC_TYPE_START_SCREEN      = 12,
    E_MUSIC_TYPE_JUNKYARD          = 13,
    E_MUSIC_TYPE_EA_TRAX_FREEBURN  = 14,
};

// BrnMixerData.h:525.
enum ECrashStreamOutputParameters
{
    E_MIXER_CRASH_STREAM_IMPACT_VOLUME    = 0,
    E_MIXER_CRASH_STREAM_REALTIME_VOLUME  = 1,
    E_MIXER_CRASH_STREAM_CUTOFF           = 2,
    E_MIXER_SHOWTIME_STREAM_IMPACT_VOLUME = 3,
    E_MIXER_SHOWTIME_STREAM_REALTIME_VOLUME = 4,
};

// BrnMixerData.h:780. What kind of camera the player is looking through; the dynamic
// mixer's camera state. Corroborated in the ARTIST asm by CameraControl::GetCameraMode
// @0x8269C6E0, whose returns pair 1:1 with CameraState::EFlag -- bit 7 JUMP_CAMERA -> JUMP,
// bit 10 CRASH_CAMERA -> CRASH, bit 14 IS_PICTURE_PARADISE -> PICTURE_PARADISE, bit 11
// TAKEDOWN_CAMERA -> TAKE_DOWN, bits 12/4 (INTERNAL_CAR/BUMPER) -> INTERNAL.
enum ECameraModes
{
    E_CAMERA_MODE_DRIVING          = 0,
    E_CAMERA_MODE_INTERNAL         = 1,
    E_CAMERA_MODE_PICTURE_PARADISE = 2,
    E_CAMERA_MODE_JUMP             = 3,
    E_CAMERA_MODE_CRASH            = 4,
    E_CAMERA_MODE_TAKE_DOWN        = 5,
    E_CAMERA_MODE_SHOWTIME         = 6,
};

// BrnMixerData.h:805. The Nicotine dynamic-mixer snapshot ids. Corroborated by
// CameraControl::UpdateParams @0x826F6540, which maps camera flags onto ids 1/2/3/9 with
// matching names, and by CameraControl::GetEventSnapshot @0x82686C80, which maps
// E_MODE_ROAD_RAGE -> ROAD_RAGE, E_MODE_STUNT_ATTACK -> STUNT and so on.
enum ESnapshotTypes
{
    E_SNAPSHOT_TYPE_BASE             = 0,
    E_SNAPSHOT_TYPE_CRASH            = 1,
    E_SNAPSHOT_TYPE_IN_AIR           = 2,
    E_SNAPSHOT_TYPE_TAKEDOWN         = 3,
    E_SNAPSHOT_TYPE_EVENT_START      = 4,
    E_SNAPSHOT_TYPE_EVENT_END        = 5,
    E_SNAPSHOT_TYPE_CAR_SELECT       = 6,
    E_SNAPSHOT_TYPE_RIVAL_UNLOCK     = 7,
    E_SNAPSHOT_TYPE_ONLINE_JOIN      = 8,
    E_SNAPSHOT_TYPE_PICTURE_PARADISE = 9,
    E_SNAPSHOT_TYPE_SHOW_TIME        = 10,
    E_SNAPSHOT_TYPE_IN_RACE          = 11,
    E_SNAPSHOT_TYPE_ROAD_RAGE        = 12,
    E_SNAPSHOT_TYPE_STUNT            = 13,
    E_SNAPSHOT_TYPE_MARKED_MAN       = 14,
    E_SNAPSHOT_TYPE_SHORTCUT         = 15,
};

} // namespace BrnSound

#endif // BRN_SOUND_BRN_MIXER_DATA_H
