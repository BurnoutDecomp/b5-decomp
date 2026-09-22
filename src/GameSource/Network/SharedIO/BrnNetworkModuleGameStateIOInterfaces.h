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
            // Copy assignment. The console emits it as the implicit member-wise copy, with the
            // queue's own operator= (length reset, then Append) inlined; the committed
            // CgsModule::EventQueue has no copy-assignment yet (an implicit one would copy the
            // source's mpEvents pointer), so it is spelled out here.
            GameStateToNetworkInterface& operator=(const GameStateToNetworkInterface& lOther);
            void                Clear();                                            // @ 0x82362528
            void                Append(const GameStateToNetworkInterface* lpOther); // @ 0x823C9360
            const EActiveRaceCarIndex GetActiveRaceCarIndex(NetworkPlayerID lNetworkPlayerID) const; // @ 0x82542190
            const NetworkPlayerID     GetNetworkPlayerID(EActiveRaceCarIndex leActiveRaceCarIndex) const; // @ 0x82542228
            const bool          GetPlayerInFreeburnChallenge(EActiveRaceCarIndex leActiveRaceCarIndex) const; // @ 0x825422D0
            void                SetActiveRaceCarIndex(NetworkPlayerID lNetworkPlayerID,
                                                      EActiveRaceCarIndex leActiveRaceCarIndex);   // @ 0x823558A0

            // No out-of-line console body; the IO buffers inline it (body in this TU's .cpp).
            void                Construct();

            // ---- declared-only API (bodies are separate TUs) ----
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

        // ===================================================================
        // NetworkToGameStateInterface (DWARF BrnNetworkModuleGameStateIOInterfaces.h:177)
        // ===================================================================
        // The Network->GameState IO half: five fixed-capacity event queues and the network
        // frame counter. Console span 0x2438 (9272) bytes; it sits at OutputBuffer +163104, at
        // BrnNetworkModule +818608 (the module's own copy, ending at +827880) and at
        // GameStateModuleIO::PreWorldInputBuffer +0x7B0. The PreWorld region +0x7B0..+0x2CC8 is
        // 0x2518 bytes because it also holds the 0xE0-byte ControllerToGameStateInterface that
        // follows this object at +0x2BE8.
        //
        // Console layout (OutputBuffer::Construct runs each queue's Construct at these offsets;
        // Append and the copy-assignment walk the same five):
        //   +0x0000  RoadRulesReceivedQueue        EventQueue<RoadRulesRecvData,14>       (16B header + 14 x 264)
        //   +0x0E80  RoadRulesDownloadedQueue      EventQueue<RoadRulesDownloadEvent,40>  (16B header + 40 x 56)
        //   +0x1750  LocalRoadRulesDownloadedQueue EventQueue<RoadRulesMessageData,40>    (16B header + 40 x 24)
        //   +0x1B20  CompletedFburnChallengesQueue EventQueue<CompletedFburnChallengesData,7> (16B header + 7 x 264)
        //   +0x2268  DirtyTrickQueue               EventQueue<DirtyTrickEvent,28>         (12B header + 28 x 16)
        //   +0x2434  s32 miNetworkFrameSinceStart
        // Each queue header holds a pointer, so every member after the first sits later on the
        // host; reach them by name only.
        struct NetworkToGameStateInterface
        {
            typedef CgsModule::EventQueue<RoadRulesRecvData, 14> RoadRulesReceivedQueue;
            typedef CgsModule::EventQueue<BrnGameState::GameStateModuleIO::CompletedFburnChallengesData, 7>
                                                                  CompletedFburnChallengesQueue;
            typedef GameStateToNetworkInterface::DirtyTrickQueue DirtyTrickQueue;

            // The console inlines it into OutputBuffer::Construct as the five queue Constructs
            // plus a zero store to miNetworkFrameSinceStart. Body in this TU's .cpp.
            void Construct();

            // Out-of-line on the console. Asserts lpCopyFrom, Appends each of the other interface's five
            // queues onto this one's (dirty-trick queue first), then copies the frame counter.
            // Called by BrnGameModule::BridgeNetworkToGameState into PreWorldInputBuffer +0x7B0.
            // Body in this TU's .cpp.
            void Append(const NetworkToGameStateInterface* lpCopyFrom);

            // Out-of-line on the console (called by BrnNetworkModule::ProcessBeforeSimulation). The
            // implicit member-wise copy with each queue's operator= (length reset, then Append)
            // inlined; spelled out because the committed CgsModule::EventQueue has no
            // copy-assignment yet. Body in this TU's .cpp.
            NetworkToGameStateInterface& operator=(const NetworkToGameStateInterface& lOther);

            // Declared-only: no out-of-line console body and no caller reconstructed yet.
            void AddDirtyTrickEvent(EActiveRaceCarIndex leAggressor, EActiveRaceCarIndex leVictim,
                                    u8 luType, u8 luStatus);

            // The queue accessors are header-inline on the console (no out-of-line bodies; the
            // consumers read the queue at its offset directly).
            RoadRulesReceivedQueue*              GetRoadRulesReceivedQueue()              { return &mRoadRulesReceivedQueue; }
            const RoadRulesReceivedQueue*        GetRoadRulesReceivedQueue() const        { return &mRoadRulesReceivedQueue; }
            RoadRulesDownloadedQueue*            GetRoadRulesDownloadedQueue()            { return &mRoadRulesDownloadedQueue; }
            const RoadRulesDownloadedQueue*      GetRoadRulesDownloadedQueue() const      { return &mRoadRulesDownloadedQueue; }
            LocalRoadRulesDownloadedQueue*       GetLocalRoadRulesDownloadedQueue()       { return &mLocalRoadRulesDownloadedQueue; }
            const LocalRoadRulesDownloadedQueue* GetLocalRoadRulesDownloadedQueue() const { return &mLocalRoadRulesDownloadedQueue; }
            DirtyTrickQueue*                     GetDirtyTrickQueue()                     { return &mDirtyTrickEventQueue; }
            const DirtyTrickQueue*               GetDirtyTrickQueue() const               { return &mDirtyTrickEventQueue; }
            CompletedFburnChallengesQueue*       GetCompletedChallengesQueue()            { return &mCompletedChallengesQueue; }
            const CompletedFburnChallengesQueue* GetCompletedChallengesQueue() const      { return &mCompletedChallengesQueue; }

            void SetFramesSinceStart(s32 liFramesSinceStart)
            {
                miNetworkFrameSinceStart = liFramesSinceStart;
            }

            s32 GetFramesSinceStart()
            {
                return miNetworkFrameSinceStart;
            }

            const s32 GetFramesSinceStart() const
            {
                return miNetworkFrameSinceStart;
            }

        private:
            RoadRulesReceivedQueue        mRoadRulesReceivedQueue;          // console +0x0000
            RoadRulesDownloadedQueue      mRoadRulesDownloadedQueue;        // console +0x0E80
            LocalRoadRulesDownloadedQueue mLocalRoadRulesDownloadedQueue;   // console +0x1750
            CompletedFburnChallengesQueue mCompletedChallengesQueue;        // console +0x1B20
            DirtyTrickQueue               mDirtyTrickEventQueue;            // console +0x2268
            s32                           miNetworkFrameSinceStart;         // console +0x2434
        };
    } // namespace BrnNetworkModuleIO
} // namespace BrnNetwork
