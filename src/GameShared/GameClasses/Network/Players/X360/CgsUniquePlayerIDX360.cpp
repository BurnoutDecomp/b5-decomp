#include "GameShared/GameClasses/Network/Players/X360/CgsUniquePlayerIDX360.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (Begin/Fire/End)

#include <cstring>   // std::strlen

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::UniquePlayerIDX360::Construct @ 0x82541A10
//       (called by ScoreboardManager::RequestXUIDForPlayerCallback / HandleEvScoreTargetEvent)
//
// Validate the player name (non-null, then walk it for its length and assert it fits in the
// 16-byte name field), copy it into the PlayerName base via PlayerName::Construct, then store
// the 64-bit XUID at +0x10. The two guards are the vacuous CGS_ASSERT front-end; the name walk
// in the asm (lbz/until-NUL, subf, addi -1, cmplwi 0x10) is a plain strlen < KI_USERNAME_LENGTH.
// The XUID store is `std r28, 0x10(this)` -- a 64-bit store of the single XUID arg register.

namespace CgsNetwork
{

UniquePlayerIDX360* UniquePlayerIDX360::Construct(const char* lpcPlayerName, u64 lqXuid)
{
    CGS_ASSERT(lpcPlayerName != 0, "lpcPlayerName");
    CGS_ASSERT(static_cast<s32>(std::strlen(lpcPlayerName)) < static_cast<s32>(KI_USERNAME_LENGTH),
               "strlen( lpcPlayerName ) < static_cast<int32_t>( KI_USERNAME_LENGTH )");

    // Copy the name into the PlayerName base (the X360 returns this call's r3 as the result).
    PlayerName::Construct(lpcPlayerName);

    mqXuid = lqXuid;   // std r28, 0x10(this)
    return this;
}

}
