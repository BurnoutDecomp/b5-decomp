#include "GameSource/Replays/BrnReplayStatusInterface.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"

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
    // Member-wise assignment. The X360 body inlines the per-reel name copy as a raw
    // NUL-terminated byte copy guarded by an over-length assert (CgsStringUtils.h:65):
    // it strlen()s the source name, and if that length is >= sizeof(macName) (256) it
    // fires "String <name> is too long. Buffer size = 256, string length = <len>\n"
    // through the CgsDev::Assert machinery (built via a stack StrStream, matching the
    // CgsNetwork::PlayerName::Construct precedent) -- it does NOT silently truncate
    // like CgsCore::StrCpy. The copy itself is then an unbounded strcpy either way.
    // The mbUsed flag and the scalar tail (record/playback reel + debug alpha) are
    // copied verbatim.
    StatusInterface& StatusInterface::operator=(const StatusInterface& rOther)
    {
        mxStatusFlags = rOther.mxStatusFlags;

        for (s32 liReel = 0; liReel < KI_NUMREELS; ++liReel)
        {
            const char* lpcSrcName = rOther.maReels[liReel].macName;

            if (std::strlen(lpcSrcName) >= sizeof(maReels[liReel].macName))
            {
                CgsDev::Assert::BeginAssert();
                char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStrStream << "String " << (lpcSrcName ? lpcSrcName : "<NULLSTRING>")
                           << " is too long. Buffer size = " << static_cast<s32>(sizeof(maReels[liReel].macName))
                           << ", string length = " << static_cast<s32>(std::strlen(lpcSrcName))
                           << "\n";
                CgsDev::Assert::FireAssert(
                    lacMessageBuffer,
                    "..\\..\\..\\GameShared\\GameClasses\\Core/CgsStringUtils.h",
                    65);
                CgsDev::Assert::EndAssert();
            }

            std::strcpy(maReels[liReel].macName, lpcSrcName);
            maReels[liReel].mbUsed = rOther.maReels[liReel].mbUsed;
        }

        miCurrentRecordReel   = rOther.miCurrentRecordReel;
        miCurrentPlaybackReel = rOther.miCurrentPlaybackReel;
        mfDebugHudAlpha       = rOther.mfDebugHudAlpha;

        return *this;
    }
}
}
