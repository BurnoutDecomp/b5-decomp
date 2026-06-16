#ifndef BRN_OPPONENT_DATA_H
#define BRN_OPPONENT_DATA_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // CgsID

namespace BrnGameState
{
// Per-rival event record stored by value in GameModeParams::maOpponentData
// (Array<OpponentData,7>). Reconstructed from BURNOUT_X360_ARTIST.XEX: the element stride is
// 112 bytes (Array<OpponentData,7>::Append @ 0x82317D90 / ::GetI @ 0x822AE310 both index with
// 112*i, and the Array's trailing count word lands at owner+784 == 7*112).
//
// DWARF (BrnGameModeParams.h:112) names four members in source order:
//   CgsID                 mCarModelId
//   EventStartGridSlot    mStartGridSlot
//   OpponentBalanceData   mRaceBalanceData
//   EventRacerPersonality mPersonality
// Only mCarModelId's type is fully recoverable (CgsID == u64). The three trailing sub-structs'
// internal layouts are not reconstructable from the DWARF here (forward-decl'd; only
// EventRacerPersonality's 4 floats are attested), and Append/GetItem never touch a sub-member,
// so they are folded into one explicit pad sized to fill the 112-byte stride. Single owner --
// grow in place (replace the pad with the real EventStartGridSlot / OpponentBalanceData /
// EventRacerPersonality types when those TUs land), do not fork.
class OpponentData
{
public:
    CgsID mCarModelId;       // 0x00 (8 bytes) -- DWARF BrnGameModeParams.h:134
private:
    u8    maPad[104];        // 0x08..0x6F: mStartGridSlot + mRaceBalanceData + mPersonality (stride pad to 112)
};
}

#endif
