#include "CgsPlayersConnectionManager.h"

namespace CgsNetwork
{
namespace
{
const u32 KU_PLAYER_RECORD_VTABLE = 0x820CF28C;
const u32 KU_PLAYER_MANAGER_VTABLE = 0x820CE35C;
}

class PlayerManager : public PlayersConnectionManager
{
public:
    PlayerManager();

private:
    struct PlayerRecord
    {
        u32 muVTable;
        u8  mPad4[36];
    };

    u8           mPadAfterConnectionManager[4312];
    PlayerRecord maPlayers[20];
    u8           mPadAfterPlayers[264];
    u32          muVTable;
};

static_assert(sizeof(PlayersConnectionManager) == 3864, "PlayersConnectionManager size must match recovered layout");

PlayerManager::PlayerManager()
    : PlayersConnectionManager()
{
    for (PlayerRecord& lPlayer : maPlayers)
        lPlayer.muVTable = KU_PLAYER_RECORD_VTABLE;

    muVTable = KU_PLAYER_MANAGER_VTABLE;
}
}
