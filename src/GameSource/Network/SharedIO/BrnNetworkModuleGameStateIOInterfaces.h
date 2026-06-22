// ===================================================================================
// BrnNetwork::BrnNetworkModuleIO::NetworkPlayerMappingData / GameStateToNetworkInterface
//   -- owning header
//   b5-decomp/src/GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.h
//
// SHAPE authoritative from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.h:58/74),
// gated against the X360 binary (member byte offsets / copy strides). The GameStateToNetworkInterface
// is the GameState->Network IO half: it carries the per-frame dirty-trick event queue, the
// network-player <-> active-race-car-index mapping table, the per-car "in a free-burn challenge"
// flags, and the current game-mode / online / car-select state booleans.
//
// LAYOUT of GameStateToNetworkInterface (X360-AUTHORITATIVE byte offsets; verified by the asm below):
//   +0    DirtyTrickQueue        mDirtyTrickQueue  (EventQueue<DirtyTrickEvent,28>: base 12B
//                                                   {mpEvents,miMaxLength,miLength} + 28*16B inline
//                                                   maEvents == 460B; ends at +0x1CC)
//   +460  NetworkPlayerMappingData maMapping[8]    (8 * 8B == 64B; each {NetworkPlayerID +0,
//                                                   EActiveRaceCarIndex +4}); ends at +0x20C
//   +524  bool                   mabPlayersInFreeburnChallenge[8]   (+0x20C..+0x214)
//   +532  EGameModeType          meCurrentGameMode                  (+0x214; cleared to -1)
//   +536  bool                   mbIsInOnlineGameMode               (+0x218)
//   +537  bool                   mbIsInCarSelect                    (+0x219)
//
// X360 method addresses (all bodied in this TU's .cpp):
//   Clear                       @ 0x82362528
//   Append                      @ 0x823C9360
//   GetActiveRaceCarIndex       @ 0x82542190  (dossier "GetActiveRa")
//   GetNetworkPlayerID          @ 0x82542228
//   GetPlayerInFreeburnChallenge@ 0x825422D0
//   SetActiveRaceCarIndex       @ 0x823558A0
//
// The maMapping table is searched linearly (8 entries == KI_MAX_ACTIVE_RACE_CARS): GetActiveRaceCarIndex
// matches on mNetworkPlayerID and returns the paired meActiveRaceCarIndex; GetNetworkPlayerID matches on
// meActiveRaceCarIndex and returns the paired mNetworkPlayerID; SetActiveRaceCarIndex either updates the
// entry already holding the given ARCI or claims the first free (mNetworkPlayerID == -1) slot.
#pragma once

#include "types.hpp"
#include "GameSource/BurnoutConstants.h"                            // ::EActiveRaceCarIndex enumerators (E_ACTIVE_RACE_CAR_INDEX_COUNT == 8) -- the count the asserts spell
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"         // BrnNetwork::NetworkPlayerID, EActiveRaceCarIndex, DirtyTrickEvent
#include "GameShared/GameClasses/Module/CgsEventQueue.h"            // CgsModule::EventQueue<T,N>
#include "GameSource/GameState/BrnGameStateSharedIO.h"              // BrnGameState::GameStateModuleIO::EGameModeType

namespace BrnNetwork
{
    namespace BrnNetworkModuleIO
    {
        // Max simultaneously-tracked active race cars (the player + rivals). The X360 asserts
        // spell this BrnWorld::KI_MAX_ACTIVE_RACE_CARS (== 8) and the BurnoutConstants
        // E_ACTIVE_RACE_CAR_INDEX_COUNT is the same value; no single committed shared home exists,
        // so it is modelled locally (matching the BrnGameActions / BrnGameModeParams TUs).
        static const s32 KI_MAX_ACTIVE_RACE_CARS = 8;

        // ===================================================================
        // NetworkPlayerMappingData (DWARF BrnNetworkModuleGameStateIOInterfaces.h:58)
        //   One row of the network-player <-> active-race-car-index mapping table (8B).
        // ===================================================================
        struct NetworkPlayerMappingData
        {
            NetworkPlayerID     mNetworkPlayerID;     // +0
            EActiveRaceCarIndex meActiveRaceCarIndex; // +4
        };

        // ===================================================================
        // GameStateToNetworkInterface (DWARF BrnNetworkModuleGameStateIOInterfaces.h:74)
        // ===================================================================
        struct GameStateToNetworkInterface
        {
            // BrnNetworkSharedIO.h:509 -- the per-frame queue of dirty-trick (payback) events.
            typedef CgsModule::EventQueue<DirtyTrickEvent, 28> DirtyTrickQueue;

            // ---- bodied in this TU's .cpp ----
            void                Clear();                                            // @ 0x82362528
            void                Append(const GameStateToNetworkInterface* lpOther); // @ 0x823C9360
            const EActiveRaceCarIndex GetActiveRaceCarIndex(NetworkPlayerID lNetworkPlayerID) const; // @ 0x82542190
            const NetworkPlayerID     GetNetworkPlayerID(EActiveRaceCarIndex leActiveRaceCarIndex) const; // @ 0x82542228
            const bool          GetPlayerInFreeburnChallenge(EActiveRaceCarIndex leActiveRaceCarIndex) const; // @ 0x825422D0
            void                SetActiveRaceCarIndex(NetworkPlayerID lNetworkPlayerID,
                                                      EActiveRaceCarIndex leActiveRaceCarIndex);   // @ 0x823558A0

            // ---- declared-only API (bodies are separate TUs) ----
            void                Construct();
            void                AddDirtyTrickEvent(EActiveRaceCarIndex leAggressor, EActiveRaceCarIndex leVictim,
                                                   u8 luType, u8 luStatus);
            DirtyTrickQueue*        GetDirtyTrickQueue();
            const DirtyTrickQueue*  GetDirtyTrickQueue() const;
            bool                GetIsInOnlineGameMode() const;
            void                SetIsInOnlineGameMode(bool lbIsOnline);
            bool                GetIsInCarSelect() const;
            void                SetIsCarSelect(bool lbIsInCarSelect);
            void                SetCurrentGameMode(BrnGameState::GameStateModuleIO::EGameModeType leGameMode);
            const BrnGameState::GameStateModuleIO::EGameModeType GetCurrentGameMode() const;
            void                SetPlayerInFreeburnChallenge(EActiveRaceCarIndex leActiveRaceCarIndex, bool lbInChallenge);

        private:
            DirtyTrickQueue          mDirtyTrickQueue;                               // +0    (460B)
            NetworkPlayerMappingData maMapping[KI_MAX_ACTIVE_RACE_CARS];             // +460  (64B)
            bool                     mabPlayersInFreeburnChallenge[KI_MAX_ACTIVE_RACE_CARS]; // +524 (8B)
            BrnGameState::GameStateModuleIO::EGameModeType meCurrentGameMode;        // +532
            bool                     mbIsInOnlineGameMode;                           // +536
            bool                     mbIsInCarSelect;                                // +537
        };
    } // namespace BrnNetworkModuleIO
} // namespace BrnNetwork
