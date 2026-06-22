// Embed check: instantiate BrnReplays::ReplayIO::StatusInterface by value and
// exercise GetReel / SetReel / operator=. Compile-only sanity for the recovered
// home + bodies.
#include "GameSource/Replays/BrnReplayStatusInterface.h"

#include <cstring>

using BrnReplays::Reel;
using BrnReplays::ReplayIO::StatusInterface;

int BrnReplayStatusInterface_embed_check()
{
    StatusInterface lStatus;
    std::memset(&lStatus, 0, sizeof(lStatus));

    Reel lReel;
    std::memset(&lReel, 0, sizeof(lReel));
    lReel.macName[0] = 'A';
    lReel.mbUsed = true;

    lStatus.SetReel(2, &lReel);

    const Reel* lpGot = lStatus.GetReel(2);

    StatusInterface lCopy;
    lCopy = lStatus;

    const Reel* lpCopied = lCopy.GetReel(2);

    return (lpGot->macName[0] == 'A' && lpCopied->mbUsed) ? 0 : 1;
}
