// BrnNetworkScoreboard_wS34_00.cpp -- ScoreboardColumn's type and width accessors
// (BrnNetworkScoreboard.cpp family). The console inlines both (the leaderboard table reads
// the type word at +0 and the width halfword at +4 directly).

#include "GameSource/Network/Managers/BrnNetworkScoreboard.h"

namespace BrnNetwork
{
    ScoreboardColumn::EDataType ScoreboardColumn::GetType() const
    {
        return meDataType;
    }

    s32 ScoreboardColumn::GetWidth() const
    {
        return miWidth;
    }
}
