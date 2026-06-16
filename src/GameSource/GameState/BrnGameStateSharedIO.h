#pragma once

#include "BrnCommonTypes.h"
#include "GameSource/GameState/BrnGameStateTypes.h"   // BrnGameState::LandmarkIndex

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
    }
}
