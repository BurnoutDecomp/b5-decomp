#include "GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX. The GameStateToNetworkInterface is the
// GameState->Network IO half (network-player <-> active-race-car-index mapping table, per-car
// free-burn-challenge flags, dirty-trick event queue, mode/online/car-select state).
//
//   BrnNetwork::BrnNetworkModuleIO::GameStateToNetworkInterface::Clear                        @ 0x82362528
//   BrnNetwork::BrnNetworkModuleIO::GameStateToNetworkInterface::Append                       @ 0x823C9360
//   BrnNetwork::BrnNetworkModuleIO::GameStateToNetworkInterface::GetActiveRaceCarIndex        @ 0x82542190
//   BrnNetwork::BrnNetworkModuleIO::GameStateToNetworkInterface::GetNetworkPlayerID           @ 0x82542228
//   BrnNetwork::BrnNetworkModuleIO::GameStateToNetworkInterface::GetPlayerInFreeburnChallenge @ 0x825422D0
//   BrnNetwork::BrnNetworkModuleIO::GameStateToNetworkInterface::SetActiveRaceCarIndex        @ 0x823558A0
//
// The class layout (member byte offsets, queue stride) is in the owning header.

namespace BrnNetwork
{
    namespace BrnNetworkModuleIO
    {
        // @ 0x82362528 -- reset the interface to empty. Drops every queued dirty-trick event
        // (mDirtyTrickQueue.miLength = 0; the asm writes the queue's +8 length word directly),
        // clears the mode/online/car-select state, and sets every mapping row to "none" (-1/-1)
        // and every free-burn-challenge flag to false. The trailing per-flag loop carries a
        // non-gating tripwire assert (leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT).
        void GameStateToNetworkInterface::Clear()
        {
            // The X360 stores 0 to the queue's miLength word at +8 directly (it does not call the
            // queue's Construct -- the backing pointer/maxlen are left intact). Model as the
            // additive BaseEventQueue<T>::Clear (pure miLength reset).
            mDirtyTrickQueue.Clear();

            mbIsInOnlineGameMode = false;                                          // +536
            mbIsInCarSelect      = false;                                          // +537
            meCurrentGameMode    = BrnGameState::GameStateModuleIO::E_MODE_NONE;   // +532 (-1)

            for (s32 li = 0; li < KI_MAX_ACTIVE_RACE_CARS; ++li)
            {
                maMapping[li].mNetworkPlayerID     = -1;
                maMapping[li].meActiveRaceCarIndex = static_cast<EActiveRaceCarIndex>(-1);
            }

            for (s32 li = 0; li < KI_MAX_ACTIVE_RACE_CARS; ++li)
            {
                mabPlayersInFreeburnChallenge[li] = false;
                CGS_ASSERT(li + 1 <= E_ACTIVE_RACE_CAR_INDEX_COUNT,
                           "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT");
            }
        }

        // @ 0x823C9360 -- merge another interface's state onto this one. First appends the other
        // queue's live dirty-trick events onto this queue's tail (BaseEventQueue<T>::Append), then
        // overwrites the scalar state (online/car-select/mode), the whole 8-entry mapping table,
        // and the 8 free-burn-challenge flags with the other's values. The per-flag loop carries
        // the same non-gating tripwire assert as Clear.
        void GameStateToNetworkInterface::Append(const GameStateToNetworkInterface* lpOther)
        {
            mDirtyTrickQueue.Append(lpOther->mDirtyTrickQueue);

            mbIsInOnlineGameMode = lpOther->mbIsInOnlineGameMode;                  // +536
            mbIsInCarSelect      = lpOther->mbIsInCarSelect;                       // +537
            meCurrentGameMode    = lpOther->meCurrentGameMode;                     // +532

            for (s32 li = 0; li < KI_MAX_ACTIVE_RACE_CARS; ++li)                   // maMapping[8] (+460)
            {
                maMapping[li] = lpOther->maMapping[li];
            }

            for (s32 li = 0; li < KI_MAX_ACTIVE_RACE_CARS; ++li)                   // flags (+524)
            {
                mabPlayersInFreeburnChallenge[li] = lpOther->mabPlayersInFreeburnChallenge[li];
                CGS_ASSERT(li + 1 <= E_ACTIVE_RACE_CAR_INDEX_COUNT,
                           "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT");
            }
        }

        // @ 0x82542190 -- map a network-player id to its active-race-car-index slot. Asserts the id
        // is not the invalid sentinel (non-gating), then linearly searches the mapping table for the
        // row whose mNetworkPlayerID matches and returns its paired meActiveRaceCarIndex; returns
        // INVALID (-1) when no row matches.
        const EActiveRaceCarIndex GameStateToNetworkInterface::GetActiveRaceCarIndex(
                NetworkPlayerID lNetworkPlayerID) const
        {
            if (lNetworkPlayerID == -1)
            {
                CGS_ASSERT(lNetworkPlayerID != -1,
                           "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");
            }

            for (s32 li = 0; li < KI_MAX_ACTIVE_RACE_CARS; ++li)
            {
                if (maMapping[li].mNetworkPlayerID == lNetworkPlayerID)
                {
                    return maMapping[li].meActiveRaceCarIndex;
                }
            }
            return static_cast<EActiveRaceCarIndex>(-1);
        }

        // @ 0x82542228 -- inverse of GetActiveRaceCarIndex: map an active-race-car-index back to its
        // network-player id. Asserts the index is in [0, COUNT) (both non-gating tripwires), then
        // linearly searches the mapping table for the row whose meActiveRaceCarIndex matches and
        // returns its paired mNetworkPlayerID; returns INVALID (-1) when no row matches.
        const NetworkPlayerID GameStateToNetworkInterface::GetNetworkPlayerID(
                EActiveRaceCarIndex leActiveRaceCarIndex) const
        {
            if (leActiveRaceCarIndex < 0)
            {
                CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                           "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
            }
            if (leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_COUNT)
            {
                CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                           "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
            }

            for (s32 li = 0; li < KI_MAX_ACTIVE_RACE_CARS; ++li)
            {
                if (maMapping[li].meActiveRaceCarIndex == leActiveRaceCarIndex)
                {
                    return maMapping[li].mNetworkPlayerID;
                }
            }
            return -1;
        }

        // @ 0x825422D0 -- is the car in the given active-race-car slot currently in a free-burn
        // challenge? Asserts the index is in [0, COUNT) (both non-gating tripwires), then returns
        // the indexed mabPlayersInFreeburnChallenge flag.
        const bool GameStateToNetworkInterface::GetPlayerInFreeburnChallenge(
                EActiveRaceCarIndex leActiveRaceCarIndex) const
        {
            if (leActiveRaceCarIndex < 0)
            {
                CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                           "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
            }
            if (leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_COUNT)
            {
                CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                           "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
            }

            return mabPlayersInFreeburnChallenge[leActiveRaceCarIndex];
        }

        // @ 0x823558A0 -- bind a network-player id to an active-race-car-index in the mapping table.
        // Asserts the ARCI is neither the invalid sentinel nor COUNT (non-gating). Then linearly
        // scans the table: if a row already holds this ARCI, its mNetworkPlayerID is updated in
        // place (and the X360 build streams a debug "Changed entry ..." trace line via
        // CgsDev::Message -- a developer log with no state effect, omitted here). Otherwise the first
        // free row (mNetworkPlayerID == -1, tracked as liFreeEntry) is claimed: its mNetworkPlayerID
        // and meActiveRaceCarIndex are set. The "liFreeEntry >= 0" assert is a non-gating tripwire
        // for a full table.
        void GameStateToNetworkInterface::SetActiveRaceCarIndex(
                NetworkPlayerID lNetworkPlayerID, EActiveRaceCarIndex leActiveRaceCarIndex)
        {
            if (leActiveRaceCarIndex == static_cast<EActiveRaceCarIndex>(-1) ||
                leActiveRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_COUNT)
            {
                CGS_ASSERT(leActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID &&
                               leActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_COUNT,
                           "( leActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID ) && "
                           "( leActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_COUNT )");
            }

            s32 liFreeEntry = -1;
            for (s32 li = 0; li < KI_MAX_ACTIVE_RACE_CARS; ++li)
            {
                if (maMapping[li].meActiveRaceCarIndex == leActiveRaceCarIndex)
                {
                    // Row already owns this ARCI: update its player id in place. (The X360 also
                    // emits a CgsDev::Message debug trace line here -- "Changed entry at <NPID> to
                    // NPID <free> and ARCI <arci>" -- a developer-only log with no state effect.)
                    maMapping[li].mNetworkPlayerID = lNetworkPlayerID;
                    return;
                }
                if (maMapping[li].mNetworkPlayerID == -1)
                {
                    liFreeEntry = li;
                }
            }

            CGS_ASSERT(liFreeEntry >= 0, "liFreeEntry >= 0");
            CGS_ASSERT(liFreeEntry < KI_MAX_ACTIVE_RACE_CARS,
                       "liFreeEntry < BrnWorld::KI_MAX_ACTIVE_RACE_CARS");

            maMapping[liFreeEntry].mNetworkPlayerID     = lNetworkPlayerID;
            maMapping[liFreeEntry].meActiveRaceCarIndex = leActiveRaceCarIndex;
        }
    } // namespace BrnNetworkModuleIO
} // namespace BrnNetwork
