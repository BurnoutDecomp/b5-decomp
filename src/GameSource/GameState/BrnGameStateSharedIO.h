#pragma once

#include "BrnCommonTypes.h"
#include "GameSource/GameState/BrnGameStateTypes.h"             // BrnGameState::LandmarkIndex
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"     // BrnNetwork::NetworkPlayerID
#include "GameShared/GameClasses/Containers/CgsBitArray.h"      // CgsContainers::BitArray<N> (CarCheckpointData)

namespace BrnGameState
{
    namespace GameStateModuleIO
    {
        // Identifies which game mode is active. Recovered verbatim from the DecFIGS DWARF
        // (BrnGameStateSharedIO.h:51); the X360 build attests these values via the mode
        // classes and the ModeManager's current-mode-type field. The duplicate values are
        // intentional (the online block re-uses the offline-count slot as its start, and
        // E_MODE_ONLINE_RACE aliases E_MODE_ONLINE_MODE_START == 10).
        enum EGameModeType
        {
            E_MODE_NONE                    = -1,
            E_MODE_OFFLINE_RACE            = 0,
            E_MODE_FACE_OFF                = 1,
            E_MODE_OFFLINE_SHOWTIME        = 2,
            E_MODE_ROAD_RAGE               = 3,
            E_MODE_PURSUIT                 = 4,
            E_MODE_BURNING_ROUTE           = 5,
            E_MODE_ELIMINATOR              = 6,
            E_MODE_STUNT_ATTACK            = 7,
            E_MODE_MARKED_MAN              = 8,
            E_MODE_TRAFFIC_ATTACK          = 9,
            E_MODE_OFFLINE_COUNT           = 10,
            E_MODE_ONLINE_MODE_START       = 10,
            E_MODE_ONLINE_RACE             = 10,
            E_MODE_ONLINE_ROAD_RAGE        = 11,
            E_MODE_ONLINE_FUGITIVE         = 12,
            E_MODE_ONLINE_BURNING_HOME_RUN = 13,
            E_MODE_ONLINE_FREE_BURN        = 14,
            E_MODE_ONLINE_FREE_BURN_LOBBY  = 15,
            E_MODE_ONLINE_SHOWTIME         = 16,
            E_MODE_ONLINE_MODE_END         = 17,
            E_MODE_COUNT                   = 17,
        };

        // Drives the per-mode state machine (GameMode's nested GameModeState set).
        // DWARF: BrnGameStateSharedIO.h:93. Slot 1 (E_GMS_INTRO) is IntroState's state.
        enum EGameModeState
        {
            E_GMS_INVALID        = -1,
            E_GMS_COUNTDOWN      = 0,
            E_GMS_INTRO          = 1,
            E_GMS_IN_PROGRESS    = 2,
            E_GMS_OUTRO          = 3,
            E_GMS_RESULTS        = 4,
            E_GMS_QUIT           = 5,
            E_GMS_ONLINE_LOADING = 6,
            E_GMS_ONLINE_SPLASH  = 7,
            E_GMS_COUNT          = 8,
        };

        // Recovered from BrnGameStateSharedIO.h / CgsBitArray.h (DecFIGS DWARF).
        // FastBitArray<2000> stores 2000 bits in 63 32-bit words. The X360 queue
        // aligns its inline buffer to 16, so the element is 16-byte aligned.
        struct alignas(16) CompletedFburnChallengesData
        {
            s32 mNetworkPlayerID;
            u32 mCompletedFreeburnChallenges[63]; // FastBitArray<2000>
        };

        // ===== Added by the GameMode/ModeManager leaf batch (Group B IO family) =====

        // Per-player scoring slot (online). Minimal home; enumerators mirror the
        // EActiveRaceCarIndex pattern (8 players + invalid). DWARF: BrnGameStateSharedIO.h.
        enum EPlayerScoringIndex : s32
        {
            E_PLAYER_SCORING_INDEX_INVALID = -1,
            E_PLAYER_SCORING_INDEX_0       = 0,
            E_PLAYER_SCORING_INDEX_1       = 1,
            E_PLAYER_SCORING_INDEX_2       = 2,
            E_PLAYER_SCORING_INDEX_3       = 3,
            E_PLAYER_SCORING_INDEX_4       = 4,
            E_PLAYER_SCORING_INDEX_5       = 5,
            E_PLAYER_SCORING_INDEX_6       = 6,
            E_PLAYER_SCORING_INDEX_7       = 7,
            E_PLAYER_SCORING_INDEX_COUNT   = 8
        };

        // Online team selector. DWARF: BrnGameStateSharedIO.h:32.
        enum EPlayerTeam : s32
        {
            E_PLAYER_TEAM_NONE      = 0,
            E_PLAYER_TEAM_RED_TEAM  = 1,
            E_PLAYER_TEAM_BLUE_TEAM = 2,
            E_PLAYER_TEAM_COUNT     = 3
        };

        // Per-event assert ceiling on landmarks (DWARF BrnGameStateSharedIO.h:1850).
        const s32 KI_MAX_LANDMARKS_IN_MODE = 16;

        // X360-only stunt-score scratch record. No DWARF/leak shape exists; the layout is
        // recovered purely from StuntScoreInfo::Clear (0x82356EA0), which zeroes a fixed set of
        // u32 words and leaves the muReservedNN gaps intact. Field semantics are unknown -- words
        // are named storage slots at their exact offsets (rename when a populating TU reveals
        // them). Single owner: grow in place, do not fork.
        struct StuntScoreInfo
        {
            u32 muWord00, muWord01, muWord02, muWord03, muWord04, muWord05, muWord06;
            u32 muReserved07;
            u32 muWord08;
            u32 muReserved09;
            u32 muWord10;
            u32 muReserved11;
            u32 muWord12;
            u32 muReserved13;
            u32 muWord14, muWord15, muWord16, muWord17, muWord18;
            u32 muReserved19;
            u32 muWord20;
            u32 muReserved21, muReserved22, muReserved23;
            u32 muWord24;

            void Clear();   // X360 0x82356EA0
        };

        // DWARF BrnGameStateSharedIO.h:872. The nested per-event record of the game-mode event
        // interface. Minimal slice: only Event + its Construct are owned here; the parent
        // SpecificGameModeEventInterface (Array<Event,175>) lands with its own TU.
        class SpecificGameModeEventInterface
        {
        public:
            class Event
            {
            public:
                // luTrafficLightTriggerId is a LightTriggerId (== u32); spelled u32 here to avoid
                // a BrnGameModeParams.h include cycle (that header depends on this one).
                void Construct(s32 liEventID, u32 luTrafficLightTriggerId,
                               LandmarkIndex* lpaLandmarkIndices, s32 liNumLandmarks); // X360 0x82354340

            private:
                LandmarkIndex maLandmarkIndices[KI_MAX_LANDMARKS_IN_MODE]; // 0x00
                u32           mTrafficLightTriggerId;                      // 0x20 (LightTriggerId)
                s32           miNumLandmarks;                              // 0x24
                s32           miEventID;                                   // 0x28
            };
        };

        // Per-player freeburn-challenge completion bit storage == CgsContainers::FastBitArray<2000>
        // (DWARF CgsFastBitArray.h: u64 maxBits[32] == 256 bytes). The full FastBitArray type has its
        // own un-reconstructed home; only the raw word storage these bodies fill is modelled here.
        // (Distinct from the EventQueue-element CompletedFburnChallengesData above.)
        struct CompletedFburnChallenges
        {
            static const u32 KU_NUM_BIT_WORDS = 32;
            u64 maxBits[KU_NUM_BIT_WORDS];
        };

        // DWARF BrnGameStateSharedIO.h:936. The "every player" freeburn completion-status block:
        // 7 per-player slots {bit-array, player id} + the local player's bit array. Minimal slice:
        // only Construct (0x82326360) + AddCompletionStatus (0x823263C8) are owned here.
        class FburnChallengeEveryPlayerStatusData
        {
        public:
            void Construct();
            void AddCompletionStatus(const CompletedFburnChallenges* lpCompletedChallenges,
                                     BrnNetwork::NetworkPlayerID lPlayerID);

        private:
            static const s32 KI_NUM_PLAYER_SLOTS = 7;
            struct CompletedChallenges
            {
                CompletedFburnChallenges    mCompletedChallenges; // slot+0   (256 bytes)
                BrnNetwork::NetworkPlayerID mPlayerID;            // slot+256 (sentinel -1)
            };
            CompletedChallenges      maCompletedChallenges[KI_NUM_PLAYER_SLOTS];
            CompletedFburnChallenges mLocalChallengeCompletionData;
        };

        // ===== Added by the CarCheckpointData TU (checkpoint bit-set IO) =====
        //
        // X360-only per-car checkpoint tracker. Absent from the PS3 DecFIGS DWARF; the layout
        // is recovered from the X360 Hex-Rays of its four methods (0x8231BDF8 / 0x8231BF90 /
        // 0x823261D0 / 0x823C4D50). Every method's inlined bit math is byte-for-byte the
        // CgsContainers::BitArray<16> template instantiation (one u64 field, 16 valid bits). So
        // the class is a thin semantic wrapper: a "checkpoints still remaining" bit set where a
        // set bit == checkpoint not yet reached. KI_MAX_LANDMARKS_IN_MODE (==16) is the committed
        // capacity and equals BitArray<16>::GetCapacity(), so it is the NUMBITS.
        //
        // MINIMAL SLICE: the only data member the four bodies touch is the embedded BitArray<16>
        // (its single u64 sits at offset 0).
        class CarCheckpointData
        {
        public:
            // X360 0x8231BDF8. Initialise the tracker for an event with liNumCheckpoints checkpoints:
            // clear all bits, then mark checkpoints [0, liNumCheckpoints) as remaining (bit set).
            void SetupCheckpoints(s32 liNumCheckpoints);

            // X360 0x8231BF90. Mark checkpoint liCheckpointIndex as reached by clearing its remaining
            // bit. Asserts the index is in range and the bit was still set (not already hit).
            void MarkCheckpointAsHit(s32 liCheckpointIndex);

            // X360 0x823261D0. Index of the next (lowest-indexed) checkpoint still to be reached,
            // or -1 if none. Asserts at least one checkpoint is set.
            s32 GetNextCheckpointIndex() const;

            // X360 0x823C4D50. Write the indices of every remaining checkpoint, in ascending order,
            // into lpaiCheckpointIndexes; returns how many were written.
            s32 GetAllRemainingCheckpointIndexes(s32* lpaiCheckpointIndexes) const;

        private:
            // Sole data member (offset 0x00, 8 bytes). NUMBITS == KI_MAX_LANDMARKS_IN_MODE (16).
            CgsContainers::BitArray<KI_MAX_LANDMARKS_IN_MODE> mCheckpointsRemaining;
        };

        // ===== Forked slices from the GameStateModuleIO TU (FlybyData / OnlineGameResults) =====
        // TODO(conductor-review): reuse committed home / DWARF member names. The verifier notes
        // FlybyData's DWARF-attested array member is mRivalsToShow, the accessor is
        // GetCarFlybyData(int32_t) const, and a miNumberOfCars int32 header precedes the array;
        // and OnlineGameResults has a committed home (BrnGameActions.h:2960, base
        // GameAction<E_ACTION_ONLINE_GAME_RESULT> with named members). Kept here as standalone
        // slices for now -- they compile, but should be re-homed at consolidation.

        // X360 FlybyData / FlybyRivalData minimal slice. The 196-byte per-rival record is opaque
        // storage; only the bounds constant + the array are modelled (per-rival fields land with
        // the FlybyManager's own TU). FlybyData = {u8 header[4]; FlybyRivalData maFlybyRivalData[3]}.
        struct FlybyRivalData
        {
            static const s32 KI_MAX_CARS_IN_FLYBY = 3;
            u8 maOpaque[196];   // per-rival fields (un-reconstructed)
        };

        struct FlybyData
        {
            // X360 0x821F2A90. Indexed accessor for one of the (KI_MAX_CARS_IN_FLYBY == 3)
            // per-rival records. Bounds asserts at BrnGameStateSharedIO.h:1110/1111.
            FlybyRivalData* GetFlybyRivalData(s32 liRivalIndex);

        private:
            u8             mHeader[4];                                       // this+0
            FlybyRivalData maFlybyRivalData[FlybyRivalData::KI_MAX_CARS_IN_FLYBY]; // this+4, 196-byte stride
        };

        // X360 0x821F2B08. Free predicate over EGameModeType: true for exactly
        // E_MODE_ONLINE_FREE_BURN_LOBBY (15) and E_MODE_ONLINE_SHOWTIME (16).
        bool IsOnlineFreeBurnLobby(EGameModeType leGameMode);

        // X360 OnlineGameResults minimal slice (260-byte payload == 65 u32 words). The copy-assign
        // skips word index 1 (offset 0x04) faithfully. TODO(conductor-review): re-home onto the
        // committed BrnGameActions.h GameAction<E_ACTION_ONLINE_GAME_RESULT> with typed members.
        struct OnlineGameResults
        {
            static const u32 KU_NUM_WORDS = 65;
            u32 mauWords[KU_NUM_WORDS];
            OnlineGameResults& operator=(const OnlineGameResults& lOther);
        };
    }
}
