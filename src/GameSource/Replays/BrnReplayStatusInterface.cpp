#include "GameSource/Replays/BrnReplayStatusInterface.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"

#include <cstring>

// Reconstructed from BURNOUT_X360_ARTIST.XEX (BrnReplays::ReplayIO::StatusInterface).
//
//   GetReel  @ 0x824EB0B8 : range-asserts the reel index, returns &maReels[index].
//   SetReel  @ 0x8264B010 : range-asserts the index, memcpy's sizeof(Reel) bytes
//                           from the source reel into maReels[index].
//   operator=@ 0x823A6488 : member-wise copy of the status word, all six reels
//                           (bounded name copy + used flag), the two current-reel
//                           indices, and the trailing debug-HUD alpha float.
//
// The X360 build uses SIX reels (the index guards fire on `index >= 6`, the copy
// loop runs six times); the leaked PS3 DWARF declares five. X360 asm is the
// authoritative layout here.

namespace BrnReplays
{
namespace ReplayIO
{
    // Number of reel slots in the X360 layout. The X360 asm guards GetReel/SetReel
    // with `index >= 6` and runs operator='s per-reel copy loop six times.
    static const s32 KI_NUMREELS = 6;

    // @ 0x824EB0B8
    // The X360 body asserts (BrnReplayStatusInterface.h:321) when the index is < 0
    // or >= 6, then returns the address of the indexed reel. The pointer arithmetic
    // `257 * index + this + 4` is exactly `&this->maReels[index]` because the reel
    // array starts at +0x004 and sizeof(Reel) == 257.
    const Reel* StatusInterface::GetReel(s32 liIndex) const
    {
        CGS_ASSERT(liIndex >= 0 && liIndex < KI_NUMREELS,
                   "Index out of range");
        return &maReels[liIndex];
    }

    // @ 0x8264B010
    // Same range guard (BrnReplayStatusInterface.h:314), then a 257-byte memcpy of
    // the source reel into the indexed slot (the X360 passes Size = 0x101 ==
    // sizeof(Reel)).
    void StatusInterface::SetReel(s32 liIndex, const Reel* pSrc)
    {
        CGS_ASSERT(liIndex >= 0 && liIndex < KI_NUMREELS,
                   "Index out of range");
        std::memcpy(&maReels[liIndex], pSrc, sizeof(Reel));
    }

    // @ 0x823A6488
    // Member-wise assignment. The X360 body inlines the per-reel name copy as the
    // bounded CgsStringUtils CopyString (the "String ... is too long" guard at
    // CgsStringUtils.h:65, buffer size 256); reconstructed here via CgsCore::StrCpy
    // which carries that same bounded-copy assert. The used flag and the scalar
    // tail (record/playback reel + debug alpha) are copied verbatim.
    StatusInterface& StatusInterface::operator=(const StatusInterface& rOther)
    {
        mxStatusFlags = rOther.mxStatusFlags;

        for (s32 liReel = 0; liReel < KI_NUMREELS; ++liReel)
        {
            CgsCore::StrCpy(maReels[liReel].macName,
                            sizeof(maReels[liReel].macName),
                            rOther.maReels[liReel].macName);
            maReels[liReel].mbUsed = rOther.maReels[liReel].mbUsed;
        }

        miCurrentRecordReel   = rOther.miCurrentRecordReel;
        miCurrentPlaybackReel = rOther.miCurrentPlaybackReel;
        mfDebugHudAlpha       = rOther.mfDebugHudAlpha;

        return *this;
    }
}
}
