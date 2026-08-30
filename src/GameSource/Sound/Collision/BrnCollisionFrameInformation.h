#ifndef BRN_SOUND_LOGIC_BRN_COLLISION_FRAME_INFORMATION_H
#define BRN_SOUND_LOGIC_BRN_COLLISION_FRAME_INFORMATION_H

#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Containers/CgsBitArray.h"
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"

// =============================================================================
// BrnSound::Logic::FrameInformation
//   GameSource/Sound/Collision/BrnCollisionFrameInformation.h (DWARF home) +
//   GameSource/Sound/Collision/BrnCollisionFrameInformation.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. FrameInformation is the per-frame
// sound-logic snapshot the collision/passby sound logic reads: the player
// transform, the crash "fatality" state machine, the impact-time / super-slo-mo
// flags, the paused-source bit set, replay state and the hard-stop flag.
//
// DWARF (BrnCollisionFrameInformation.h:44) gives the full layout/surface:
//   struct BrnSound::Logic::FrameInformation {
//       Matrix44Affine                         mPlayerTransform;     // :73
//       DataPoint<EFatalityFlag>               meFatality;           // :74
//       DataPoint<eImpactTime>                 meImpactTime;         // :75
//       BitArray<3u>                           maPaused;             // :76
//       bool                                   mbInReplay;           // :77
//       u32                                    mu32FatalStartCount;  // :78
//       DataPoint<bool>                        mIsHardStop;          // :79
//   };
// This is the OWNING home (no committed home existed); the layout is modelled BY
// NAME, reusing the committed Matrix44Affine, CgsContainers::BitArray<3> and the
// (additively-grown) CgsSound::Utils::DataPoint<T> generic.
//
// This TU's recon'd function set is exactly TWO entries (the rest of the DWARF
// surface -- the ctor, IsCrashing/IsImpactTime/IsSuperSloMo/StopFatality -- is
// DEFERRED to its own TU(s)):
//   IsPaused           @ 0x826958E8
//   UpdateFatalityFlag @ 0x826ADD18
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the X360 asm accesses members by
// absolute byte offset (meFatality @ +64 = result[16], mu32FatalStartCount @ +96 =
// result[24], maPaused @ +80). Those offsets assume a 64-byte Matrix44Affine and
// 4-byte words; members are pinned BY NAME and SEQUENCE only and absolute offsets
// are NOT static_asserted across the (host-width-dependent) layout.
// =============================================================================

namespace BrnSound
{
namespace Logic
{

// BrnCollisionFrameInformation.h:24 (DWARF). Frames the fatality state machine
// must survive in E_FATAL_START before it commits to E_FATAL_ON.
const u32 KU32_FATAL_START_FRAME_COUNT = 5;

// BrnCollisionFrameInformation.h:26 (DWARF). Crash "fatality" (big-crash) state.
enum EFatalityFlag
{
    E_FATAL_OFF   = 0,
    E_FATAL_ON    = 1,
    E_FATAL_START = 2,
};

// BrnCollisionFrameInformation.h:44 (DWARF). Per-frame sound-logic snapshot.
struct FrameInformation
{
    FrameInformation()
        : meFatality(E_FATAL_OFF)
        , meImpactTime(0)
        , mbInReplay(false)
        , mu32FatalStartCount(0)
        , mIsHardStop(false)
    {
        maPaused.Prepare();
    }

    // BrnCollisionFrameInformation.h:55 (DWARF). Drive the fatality state machine
    // from the per-frame crash-input flag. @ 0x826ADD18.
    void UpdateFatalityFlag(bool lbCrashInput);

    // BrnCollisionFrameInformation.h:67 (DWARF). True iff the frame is paused (any
    // pause source active). @ 0x826958E8.
    bool IsPaused() const;

    // ORDER mirrors the DWARF / X360 access sequence:
    //   +0   -> mPlayerTransform   (Matrix44Affine, 64 bytes)
    //   +64  -> meFatality         (result[16]/result[17] = cur/prev)
    //   +72  -> meImpactTime
    //   +80  -> maPaused           (IsPaused walks the bit-field storage here)
    //   ...  -> mbInReplay
    //   +96  -> mu32FatalStartCount (result[24])
    //   ...  -> mIsHardStop
    Matrix44Affine                           mPlayerTransform;     // :73
    CgsSound::Utils::DataPoint<EFatalityFlag> meFatality;          // :74
    CgsSound::Utils::DataPoint<s32>          meImpactTime;         // :75 (eImpactTime enum -> s32)
    CgsContainers::BitArray<3>               maPaused;             // :76
    bool                                     mbInReplay;           // :77
    u32                                      mu32FatalStartCount;  // :78
    CgsSound::Utils::DataPoint<bool>         mIsHardStop;          // :79
};

} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_BRN_COLLISION_FRAME_INFORMATION_H
