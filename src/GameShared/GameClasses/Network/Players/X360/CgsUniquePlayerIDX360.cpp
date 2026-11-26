#include "GameShared/GameClasses/Network/Players/X360/CgsUniquePlayerIDX360.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (Begin/Fire/End)
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerParams.h" // ServerInterfacePlayerParamsBase
#include "dirtyaddr.h"                                 // DirtyAddrToHostAddr

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

// ARTIST's asserts name the header (lines 99/101), where the original defined this body.
#define CGS_ARTIST_UNIQUE_PLAYER_ID_X360_H "..\\..\\..\\GameShared\\GameClasses\\Network/Players/X360/CgsUniquePlayerIDX360.h"

namespace CgsNetwork
{

void UniquePlayerIDX360::Construct(const char* lpcPlayerName, u64 lqXuid)
{
    CGS_ASSERT_AT(lpcPlayerName, "lpcPlayerName", CGS_ARTIST_UNIQUE_PLAYER_ID_X360_H, 99);
    // size_t against int: the unsigned compare ARTIST makes (cmplwi 0x10 at 0x82541A78).
    CGS_ASSERT_AT(std::strlen(lpcPlayerName) < static_cast<s32>(KI_USERNAME_LENGTH),
                  "strlen( lpcPlayerName ) < static_cast<int32_t>( KI_USERNAME_LENGTH )",
                  CGS_ARTIST_UNIQUE_PLAYER_ID_X360_H, 101);

    PlayerName::Construct(lpcPlayerName);

    mqXuid = lqXuid;   // std r28, 0x10(this)
}

// The lobby player's machine address carries the XUID; decode it, then take the name.
void UniquePlayerIDX360::Construct(ServerInterfacePlayerParamsBase* lpPlayerParams)
{
    CGS_ASSERT(lpPlayerParams != 0, "lpPlayerParams");

    u64 lqXuid;
    DirtyAddrToHostAddr(&lqXuid, sizeof(lqXuid), static_cast<const DirtyAddrT*>(lpPlayerParams->GetDirtyAddrT()));
    Construct(lpPlayerParams->GetName(), lqXuid);
}

}
