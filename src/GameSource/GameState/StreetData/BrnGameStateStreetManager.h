#pragma once

// ---------------------------------------------------------------------------
// GameSource/GameState/StreetData/BrnGameStateStreetManager.h
//   (canonical home for BrnGameState::StreetManager -- FULL layout, frozen)
//
// KEYSTONE FREEZE (StreetManager wave B). This replaces the former opaque
// minimal-coherent slice: the complete member layout is now committed, named
// per the DecFIGS DWARF (GameSource/GameState/StreetData/BrnGameStateStreetManager.h
// :199..:618) and pinned against the BURNOUT_X360_ARTIST.XEX asm (offsets below
// are the X360 32-bit guest offsets, proven by StreetManager::Construct
// @ 0x82335978 / Destruct @ 0x82335D40 / the 36-func handler family).
//
// X360 layout summary (guest offsets, sizeof == 0x1E08):
//   +0x0000 maNetworkChallengeData[64]   (ChallengeHighScoreEntry, 56B stride)
//   +0x0E00 maChallengeData[64]          (ChallengePlayerScoreEntry, 40B stride)
//   +0x1800 maaParRivalIds[64][2]        (CgsID)
//   +0x1C00 mNewHighScoreBuffer          (Array<BufferedNewHighScore,5>: 5x32B + count @+0x1CA0)
//   +0x1CA8 maRoadRulesChangedBitArrays[2] (BitArray<64> == u64 each)
//   +0x1CB8 mfUnbufferTimer  +0x1CBC miNumScoresLost  +0x1CC0 meActiveRoadRuleType
//   +0x1CC4 mbTooManyHighScoresToBuffer  +0x1CC5 mbFirstDownload
//   +0x1CC8 mpStreetData (ResourcePtr, 0x20 stride on X360)  +0x1CE8 mpAISectionData
//   +0x1D08 mDistrictMapResourceHandle   +0x1D10/14/18 mpProgressionManager/GameStateModule/RoadRulesManager
//   +0x1D1C..+0x1D3C nine RoadIndex      +0x1D40..+0x1D6F four 12-byte Vector3
//   +0x1D70..+0x1D8F node indices + timers + left/right road + wrong-way/lock timers
//   +0x1D90 miNextFreeSectionSlot        +0x1D94 mauWalkedSections[25] (u16)
//   +0x1DC6..+0x1DCF ten bools           +0x1DD0/D4/D8 the three load stages
//   +0x1DDC..+0x1DEC five perf-mon ids   +0x1DF0 mStreetManagerDebugComponent (0x18)
//
// The x64 gate cannot pin absolute offsets past the first pointer member
// (mpStreetData, +0x1CC8): _AssertLayout() pins the pointer-invariant prefix
// absolutely and uses relative anchors inside the pointer-free tail run.
// ---------------------------------------------------------------------------

#include "types.hpp"
#include "BrnCommonTypes.h"                                                // ::CgsID (u64)
#include "GameShared/GameClasses/RenderWare/Math/RwMathVectorTemplates.h"  // rw::math::fpu::Vector3Template<f32> (12-byte x/y/z -- the DWARF's Road::/SmoothStep::Vector3)
#include "SharedClasses/StreetData/BrnChallengeData.h"                     // BrnStreetData::ScoreType / ChallengeData / ChallengePlayerScoreEntry
#include "SharedClasses/StreetData/BrnStreetData.h"                        // BrnStreetData::StreetData / Road / RoadIndex / ChallengeIndex / ChallengeParScoresEntry
#include "GameSource/GameState/StreetData/BrnChallengeHighScoreEntry.h"    // BrnStreetData::ChallengeHighScoreEntry (56B)
#include "GameSource/GameState/StreetData/BrnStreetManagerDebugComponent.h"// BrnGameState::StreetManagerDebugComponent (embedded by value)
#include "GameSource/GameState/BrnCgsPlayerName.h"                         // CgsNetwork::PlayerName (16B)
#include "GameShared/GameClasses/Containers/CgsArray.h"                    // CgsContainers::Array<T,N> (elements then count)
#include "GameShared/GameClasses/Containers/CgsBitArray.h"                 // CgsContainers::BitArray<64u> (u64)
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"         // CgsResource::ResourcePtr<T> / ResourceHandle

#include <cstddef>   // offsetof

// ---- pointer-only / declaration-only dependencies -------------------------
namespace BrnProgression { class ProgressionManager; }
namespace BrnAI
{
    struct AISectionsData;   // SharedClasses/AI/AISectionsResourceType.h (full def for the ResourcePtr<T> template param; fwd ok -- only named, never sized here)
    struct AISection;
    struct Route;            // GameSource/World/AI/Route/BrnRoute.h
}
namespace CgsModule
{
    template <s32, s32> class EventReceiverQueue;   // <3072,16> used by pointer only
}
namespace BrnWorld
{
    namespace RaceCarEntityModuleIO { class RCEntityActiveRaceCarOutputInterface; }
}

namespace BrnGameState
{
    class  GameStateModule;
    class  RoadRulesManager;
    class  TriggerQueryManager;

    namespace GameStateModuleIO
    {
        struct OutputBuffer;
        // Road-rules game events (full layouts: GameSource/GameState/BrnGameEvents.h)
        struct OnlineRoadRulesPersonalBestRecvEvent;
        struct OnlineRoadRulesUploadedEvent;
        struct OnlineRoadRulesDownloadedEvent;
        struct OnlineRoadRulesConnectInfoEvent;
        struct RoadRulesScoreRequestEvent;
        struct BuddyRemovedEvent;
        // Road-rules batch query action (full layout: GameSource/GameState/BrnGameActions.h)
        struct RoadRulesBatchQueryAction;
    }
}

// The AI-car output interface param type. The DWARF spells the param the bare
// `AICarOutputInterface` (BrnAI::AIModuleIO family; the committed full type is
// embedded in GameStateModuleIO::PostWorldInputBuffer). Only ever passed by
// pointer here.
namespace BrnAI { namespace AIModuleIO { class AICarOutputInterface; } }

namespace BrnGameState
{
    // DWARF BrnGameStateStreetManager.h:102/103/106
    const int32_t KI_MAX_CHALLENGES           = 64;
    const int32_t KI_MAX_BUFFERED_HIGH_SCORES = 5;
    const int32_t KI_MAX_SECTIONS_TO_WALK     = 25;

    // DWARF BrnGameStateStreetManager.h:112
    enum ERoadRuleCompletionStatus
    {
        E_ROAD_RULE_COMPLETION_STATUS_NO_DATA    = 0,
        E_ROAD_RULE_COMPLETION_STATUS_BEATEN     = 1,
        E_ROAD_RULE_COMPLETION_STATUS_NOT_BEATEN = 2,
        E_ROAD_RULE_COMPLETION_STATUS_COUNT      = 3,
    };

    // DWARF BrnGameStateStreetManager.h:122
    enum ESectionEntryDirection
    {
        E_SECTION_ENTRY_DIRECTION_LEFT              = 0,
        E_SECTION_ENTRY_DIRECTION_SIGNIFICANT_LEFT  = 1,
        E_SECTION_ENTRY_DIRECTION_FORWARDS          = 2,
        E_SECTION_ENTRY_DIRECTION_RIGHT             = 3,
        E_SECTION_ENTRY_DIRECTION_SIGNIFICANT_RIGHT = 4,
        E_SECTION_ENTRY_DIRECTION_COUNT             = 5,
    };

    // DWARF BrnGameStateStreetManager.h:161. 32-byte stride on both X360 and the
    // x64 gate (16 + 8 + 4 + 1 + 1 + 2 pad; align 8 from the CgsID).
    struct BufferedNewHighScore
    {
        CgsNetwork::PlayerName   mPlayerName;                    // +0  (16B)
        ::CgsID                  mRoadID;                        // +16
        BrnStreetData::ScoreType meScoreType;                    // +24
        bool                     mbIsRoadWhollyOwnedByOnePlayer; // +28
        bool                     mbWasRoadRuledByPlayerBefore;   // +29
    };

    // NOTE: the 12-byte scalar vector members below are spelled
    // rw::math::fpu::Vector3Template<f32> in full (the DWARF's Road::Vector3 /
    // SmoothStep::Vector3 are namespace-merge artifacts of that template).

    // DWARF BrnGameStateStreetManager.h:179
    struct NewHighScoreBuffer : public ::Array<BufferedNewHighScore, 5u>
    {
        // DWARF :186. Removes every buffered entry matching (road id, score type).
        // Body lands with the StreetManager TU (X360 inlines it into its callers).
        void EraseMatchingEntries( ::CgsID lRoadID, BrnStreetData::ScoreType leScoreType );
    };

    // DWARF BrnGameStateStreetManager.h:142. The per-portal walk record WalkAISection
    // builds on the stack (Array of up to KI_MAX_SECTIONS_TO_WALK) and qsorts with
    // BrnGameState::CompareSectionWalkData.
    struct SectionWalkData
    {
        const BrnAI::AISection* mpLinkSection;   // :144
        const void*             mpLinkPortal;    // :145 (BrnAI::Portal*; fwd-typed, only carried)
        rw::math::fpu::Vector3Template<f32> mEntryPos;   // :146
        rw::math::fpu::Vector3Template<f32> mEntryDir;   // :147
        f32                     mfEntryAngle;    // :148
        u16                     muSectionIndex;  // :149
    };

    // DWARF BrnGameStateStreetManager.cpp:881. qsort comparator over SectionWalkData.
    // Defined once, in the WalkAISection partfile.
    int32_t CompareSectionWalkData( const void* lpA, const void* lpB );

    // ---- TU-scope constants (DWARF BrnGameStateStreetManager.cpp:39..:73) ----
    // Values recovered from the X360 image (scratchpad/waveB/streetmgr_rodata_dump.txt
    // + the visible float immediates in the pseudocode). const at namespace scope has
    // internal linkage, so partfiles share these definitions safely.
    const char  KAC_LOCAL_PLAYER_NAME_TEXT[12]         = "LCL_PLAY_NM";  // :39 (string @ 0x82021404)
    const f32   KF_TIME_BETWEEN_HIGH_SCORE_UNBUFFERS   = 4.0f;           // :40 (flt_820211D4)
    const s32   KI_MAX_SECTION_WALK_DEPTH              = 11;             // :43
    const s32   KI_MAX_PORTAL_WALK_DIRECTION_PERSISTANCE = 5;            // :44 (sic -- DWARF spelling)
    const f32   KF_UPCOMING_ROAD_SIDEWAYS_ANGLE        = 0.12217305f;    // :45 (flt_82CDB88C == 7 degrees in radians)
    const f32   KF_UPCOMING_ROAD_SIGNIFICANT_ANGLE     = 0.95993108f;    // :46 (flt_82CDB890 == 55 degrees in radians)
    const f32   KF_MAX_ROAD_LOOKAHEAD_DIST             = 250.0f;         // :47 (flt_82021390)
    const f32   KF_UPCOMING_ROADS_STICKY_TIME          = 0.1f;           // :48 (flt_82020F70)
    const f32   KF_UPCOMING_ROADS_STICKY_TIME_RACE     = 0.2f;           // :49 (flt_820213B8)
    const s32   KI_MIN_NODES_FOR_DECISION              = 2;              // :50
    const s32   KI_ROUTE_ROAD_PREDICTION_LOOKAHEAD_NODES = 13;           // :51
    const f32   KF_TIME_IN_JUNCTION_TO_KILL_UPCOMING   = 2.0f;           // :53 (flt_820211CC)
    // :52 KF_MIN_DISTANCE_FOR_DECSION / :54 KI_MIN_TIME_GOING_WRONG_WAY /
    // :56 KF_MIN_TIME_BEFORE_WE_SHOW_SIGN / :57 KF_MIN_DISTANCE_BEFORE_WE_SHOW_SIGN /
    // :59 KF_ROUTE_GUIDANCE_LOCK_TIME back the (not-in-this-wave) UpdateWrongWayMessages /
    // sign-lock path; recover their literals from that TU's asm when it lands.

    // :61..:64 -- the four interstate section road indices.
    const BrnStreetData::RoadIndex KI_INTERSTATE_SECTION_1 = 52;
    const BrnStreetData::RoadIndex KI_INTERSTATE_SECTION_2 = 54;
    const BrnStreetData::RoadIndex KI_INTERSTATE_SECTION_3 = 48;
    const BrnStreetData::RoadIndex KI_INTERSTATE_SECTION_4 = 49;

    const s32 KI_NUM_END_OF_ROUTE_IGNORE_NODES = 2;   // :67
    const s32 KI_NUM_ROADS_IN_A_PAIR           = 2;   // :70
    const s32 KI_NUM_PAIRED_ROADS              = 6;   // :71

    // :73 -- follow-on road pairs (X360 rodata @ 0x820213D4, dumped 2026-07-16).
    // AreRoadsAFollowOnPair scans these unordered pairs.
    const BrnStreetData::RoadIndex KAA_FOLLOW_ON_ROAD_PAIRS[KI_NUM_PAIRED_ROADS][KI_NUM_ROADS_IN_A_PAIR] =
    {
        { 46, 50 },
        { 29, 26 },
        { 21, 26 },
        { 62, 63 },
        { 52, 54 },
        { 48, 54 },
    };

    // Save-game challenge slot -> road CgsID order (X360 rodata @ 0x82029FA0, 64
    // entries, dumped 2026-07-16). GetSaveGameChallengeIndexByRoadID and the
    // by-road-ID handler paths scan this table for the road's persisted challenge
    // index. FLAG: the identifier is ours (the DWARF does not surface a name for
    // this table); the VALUES are byte-exact from the image.
    const ::CgsID KAA_SAVE_GAME_CHALLENGE_ROAD_IDS[KI_MAX_CHALLENGES] =
    {
        0x000000000005DA6BULL, 0x000000000005E2C9ULL, 0x000000000005E344ULL, 0x000000000005E4A7ULL,
        0x000000000005E6AEULL, 0x000000000005E806ULL, 0x000000000005E851ULL, 0x000000000005E87EULL,
        0x000000000005F643ULL, 0x000000000005FC83ULL, 0x000000000005FD54ULL, 0x000000000005FD8DULL,
        0x000000000005FDECULL, 0x000000000005FDF0ULL, 0x000000000005FF25ULL, 0x000000000005FF66ULL,
        0x000000000005FFA0ULL, 0x000000000005FFCEULL, 0x000000000005FFEEULL, 0x000000000006021CULL,
        0x0000000000060220ULL, 0x0000000000060222ULL, 0x0000000000060284ULL, 0x00000000000602ACULL,
        0x00000000000602B7ULL, 0x0000000000060421ULL, 0x0000000000060442ULL, 0x00000000000604EAULL,
        0x00000000000604F7ULL, 0x0000000000060665ULL, 0x00000000000606D5ULL, 0x00000000000607BDULL,
        0x00000000000608E2ULL, 0x0000000000060950ULL, 0x0000000000060988ULL, 0x0000000000060A8DULL,
        0x0000000000060AB0ULL, 0x0000000000060B52ULL, 0x0000000000060B65ULL, 0x0000000000060B9CULL,
        0x0000000000060BC4ULL, 0x0000000000060CA8ULL, 0x0000000000060CACULL, 0x0000000000060DA2ULL,
        0x0000000000060DAEULL, 0x0000000000060DC6ULL, 0x0000000000060EBCULL, 0x0000000000060F4EULL,
        0x0000000000060F6DULL, 0x0000000000060F91ULL, 0x0000000000061024ULL, 0x0000000000061061ULL,
        0x000000000006108FULL, 0x0000000000061121ULL, 0x0000000000061186ULL, 0x000000000007B5E0ULL,
        0x000000000007BA1AULL, 0x000000000007BA97ULL, 0x0000000000082A9BULL, 0x0000000000082A9CULL,
        0x0000000000088FE1ULL, 0x0000000000089105ULL, 0x0000000000089107ULL, 0x0000000000089108ULL,
    };

    // ========================================================================
    // BrnGameState::StreetManager  (DWARF :199; X360 sizeof == 0x1E08)
    //
    // `struct` to agree with the DWARF tag and the fwd decls in committed
    // consumers (BrnRoadRulesManager.h etc.).
    // ========================================================================
    struct StreetManager
    {
        // DWARF :573 / :583 / :593 -- the three streamed-load state machines.
        enum LoadStage
        {
            E_LOAD_NOT_STARTED    = 0,
            E_LOAD_REQUESTED      = 1,
            E_ACQUIRE_NOT_STARTED = 2,
            E_ACQUIRE_REQUESTED   = 3,
            E_LOAD_COMPLETE       = 4,
        };
        enum AILoadStage
        {
            E_AI_DATA_LOAD_NOT_STARTED    = 0,
            E_AI_DATA_LOAD_REQUESTED      = 1,
            E_AI_DATA_ACQUIRE_NOT_STARTED = 2,
            E_AI_DATA_ACQUIRE_REQUESTED   = 3,
            E_AI_DATA_LOAD_COMPLETE       = 4,
        };
        enum DistrictMapLoadStage
        {
            E_DISTRICT_MAP_LOAD_REQUEST     = 0,
            E_DISTRICT_MAP_LOAD_RESPONSE    = 1,
            E_DISTRICT_MAP_ACQUIRE_REQUEST  = 2,
            E_DISTRICT_MAP_ACQUIRE_RESPONSE = 3,
            E_DISTRICT_MAP_DONE             = 4,
        };

    public:
        // ---- lifecycle --------------------------------------------------------
        // @ 0x82335978. Wires the three owner pointers (asserted non-null), zeroes
        // the score tables/walk state, constructs the embedded debug component and
        // registers the five perf monitors ("UpcomingRoads", "UpcomingRoadsSendMessage",
        // "UpcomingRoadsQSort", "SetRoadRuleChallengeData", "SetRoadRuleNetworkHighScores").
        void Construct( GameStateModule* lpGameStateModule,
                        BrnProgression::ProgressionManager* lpProgression,
                        RoadRulesManager* lpRoadRulesManager );

        // @ 0x82350900 -- the streamed-prepare stage the GameStateModule pumps until
        // true: LoadAIData && LoadDistrictMap, then (re)constructs + registers the
        // debug component. (X360-era split; the PS3 DWARF lists only the 3-param
        // Prepare, which the X360 binary carries as the separate "Prepare2" below.)
        bool Prepare( GameStateModuleIO::OutputBuffer* lpOutput,
                      CgsModule::EventReceiverQueue<3072,16>* lpReceiverQueue );

        // @ 0x823509D8 (committed body). Street-data stage: LoadStreetData then
        // SetupParRivals. DWARF :222 spells this overload `Prepare`.
        bool Prepare2( GameStateModuleIO::OutputBuffer* lpOutput,
                       CgsModule::EventReceiverQueue<3072,16>* lpReceiverQueue,
                       const TriggerQueryManager* lpTriggerQueryManager );

        // DWARF :226 (not in this wave's ledger; body elsewhere).
        bool Release();

        // @ 0x82335D40. Clears the score tables/buffers, drops the owner pointers,
        // destructs the embedded debug component.
        void Destruct();

        // DWARF :244. Per-frame pump (owns UpdateUpcomingStreets / UpdateFriendHighScores /
        // UpdateUserScoresFromServerRecords / UpdateBufferedHighScores). Not in this
        // wave's ledger; declared for the freeze.
        void Update( bool lbPaused, f32 lfSimTimeStep,
                     const void* lpPreWorldInput,               // GameStateModuleIO::PreWorldInputBuffer (not yet committed)
                     GameStateModuleIO::OutputBuffer* lpOutput,
                     BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface,
                     BrnAI::AIModuleIO::AICarOutputInterface* lpLastAICarOutputInterface,
                     bool lbA, bool lbB, f32 lfC );             // FLAG: trailing arg shapes owned by the Update TU

        // ---- event / score handlers (this wave) --------------------------------
        // @ 0x823496C8. Applies a freshly-scored local challenge entry to the road's
        // slot: developer-challenge notify, profile best-crash update (+612), compare
        // against par/friend/current scores, buffer new-high-score GUI actions.
        void ProcessNewRoadScore( GameStateModuleIO::OutputBuffer* lpOutput,
                                  BrnStreetData::ChallengePlayerScoreEntry lNewScoreEntry,
                                  BrnStreetData::ScoreType leScoreType,
                                  BrnStreetData::ChallengeIndex liChallengeIndex,
                                  bool lbCheckFriendScore );

        // @ 0x82349E20. Copies the profile's persisted road-rules tables into the
        // live tables and posts the road-rules score-summary game action (type 232).
        void OnProfileLoaded( GameStateModuleIO::OutputBuffer* lpOutput );

        // @ 0x82349F10. Applies a downloaded personal-best record to the friend
        // high-score table; posts the road-score action (type 280) when it affects
        // the road the player is on.
        void ProcessNetworkHighScoreEvent( GameStateModuleIO::OutputBuffer* lpOutput,
                                           const GameStateModuleIO::OnlineRoadRulesPersonalBestRecvEvent* lpPersonalBestEvent );

        // @ 0x8233F3E8. Marks the uploaded score range clean (SetClean per type).
        void ProcessUploadEvent( const GameStateModuleIO::OnlineRoadRulesUploadedEvent* lpUploadedEvent );

        // @ 0x82324B40. Null-asserts the event; the X360 retail body is otherwise
        // empty (the timestamp store compiled away).
        void ProcessDownloadEvent( const GameStateModuleIO::OnlineRoadRulesDownloadedEvent* lpDownloadedEvent );

        // @ 0x8234A148. On (re)connect: if the server's road-rules reset time differs
        // from the profile's, clear all challenge data, post the score summary, and
        // stamp the new reset time.
        void ProcessConnectedOnlineEvent( GameStateModuleIO::OutputBuffer* lpOutput,
                                          const GameStateModuleIO::OnlineRoadRulesConnectInfoEvent* lpConnectInfoEvent );

        // @ 0x8234A240. Answers a GUI road-score request with formatted user/par/
        // friend scores (posts the response action).
        void ProcessScoreRequestEvent( GameStateModuleIO::OutputBuffer* lpOutput,
                                       const GameStateModuleIO::RoadRulesScoreRequestEvent* lpScoreRequestEvent );

        // @ 0x82324B88. Online game launched: clear the new-high-score buffer, the
        // changed-bit arrays and the lost-score count.
        void ProcessOnlineGameLaunchedEvent();

        // DWARF :297 (not in this wave's ledger).
        void ProcessActiveRoadRuleChange( s32 leNewActiveRoadRuleType );   // BrnGameState::EActiveRoadRule

        // @ 0x8234A5A8. Buddy removed: scrub their scores; if that changed the
        // current road's records, re-post them (action 280) and always post the
        // buddy-scrubbed flag action (type 55).
        void ProcessBuddyRemoved( GameStateModuleIO::OutputBuffer* lpOutput,
                                  const GameStateModuleIO::BuddyRemovedEvent* lpBuddyRemovedEvent );

        // ---- streamed loads ----------------------------------------------------
        // DWARF :310 / @ 0x8234F630 (committed body, BrnGameStateStreetManager_wB_01.cpp).
        // meLoadStage machine: LoadBundle("STREETDATA.DAT", pool 5) -> AcquireResource
        // ("StreetData", pool 5) -> bind mpStreetData from the acquire response handle.
        // True once E_LOAD_COMPLETE.
        bool LoadStreetData( GameStateModuleIO::OutputBuffer* lpOutput,
                             CgsModule::EventReceiverQueue<3072,16>* lpReceiverQueue );

        // @ 0x8234FA70. meAILoadStage machine: request the AI-lanes game data
        // (RequestInterface<3072>::GetAILanes), then bind mpAISectionData from the
        // response handle. True once E_AI_DATA_LOAD_COMPLETE.
        bool LoadAIData( GameStateModuleIO::OutputBuffer* lpOutput,
                         CgsModule::EventReceiverQueue<3072,16>* lpReceiverQueue );

        // @ 0x8234FB98. meDistrictMapLoadStage machine: request the "Districts"
        // resource (HashString | 0x5_00000000 type tag), then store the returned
        // ResourceHandle. True once E_DISTRICT_MAP_DONE.
        bool LoadDistrictMap( GameStateModuleIO::OutputBuffer* lpOutput,
                              CgsModule::EventReceiverQueue<3072,16>* lpReceiverQueue );

        // ---- street-data accessors ---------------------------------------------
        // DWARF :326 (committed body). Returns mpStreetData.operator->().
        const BrnStreetData::StreetData* GetStreetData();

        s32 GetRoadCount() const;                                        // :334
        void OnEnterNewRoad();                                           // :338
        const BrnStreetData::Road* GetRoad( BrnStreetData::RoadIndex liIndex ) const;   // :343
        // :351..:385 -- span/junction/street lookups (bodies land with their own slices).
        const void* GetJunction( s32 liSpanIndex );                      // Road::SpanIndex; Junction not yet committed
        bool  IsJunction( s32 liSpanIndex );
        const void* GetStreet( s32 liSpanIndex );                        // Street not yet committed
        bool  IsStreet( s32 liSpanIndex );
        const void* GetSpan( s32 liSpanIndex );                          // SpanBase not yet committed
        s32   GetSpanIndexFromAISectionIndex( u16 luSectionIndex );
        BrnStreetData::RoadIndex GetRoadIndexFromAISectionIndex( u16 luSectionIndex );

        // ---- challenge-score table access ---------------------------------------
        // @ 0x82335E08. Copy the local player's entry for a challenge slot into
        // lpData (constructed empty when absent). lbByRoadIndex==true converts the
        // road index through KAA_SAVE_GAME_CHALLENGE_ROAD_IDS first.
        void GetChallengeUserScore( BrnStreetData::ChallengeIndex liIndex,
                                    BrnStreetData::ChallengePlayerScoreEntry* lpData,
                                    bool lbByRoadIndex ) const;

        // @ 0x82335F30. Friend-table twin of GetChallengeUserScore.
        void GetChallengeFriendHighScore( BrnStreetData::ChallengeIndex liIndex,
                                          BrnStreetData::ChallengeHighScoreEntry* lpData,
                                          bool lbByRoadIndex ) const;

        // @ 0x82336168. Copies the par-score record out of the street data.
        void GetChallengeParScore( BrnStreetData::ChallengeIndex liIndex,
                                   BrnStreetData::ChallengeParScoresEntry* lpData ) const;

        // DWARF :417/:422/:427 (not in this wave's ledger; heavily called by it).
        ERoadRuleCompletionStatus HasPlayerBeatenParScore( BrnStreetData::ChallengeIndex liIndex, BrnStreetData::ScoreType leScoreType );
        ERoadRuleCompletionStatus HasPlayerBeatenFriendScore( BrnStreetData::ChallengeIndex liIndex, BrnStreetData::ScoreType leScoreType );
        ERoadRuleCompletionStatus HasPlayerBeatenParAndFriendScore( BrnStreetData::ChallengeIndex liIndex, BrnStreetData::ScoreType leScoreType );

        // DWARF :437 (not in this wave's ledger).
        void CopyFriendHighScoreChallengeData( BrnStreetData::ChallengeHighScoreEntry* lpDest, s32 liSizeInBytes );

        // @ 0x82317510. memcpy the whole 64-entry player table into lpDest
        // (asserts liSizeInBytes >= sizeof(maChallengeData)).
        void CopyUserChallengeData( BrnStreetData::ChallengePlayerScoreEntry* lpDest, s32 liSizeInBytes );

        // DWARF :452 (not in this wave's ledger).
        ::CgsID GetParRivalId( s32 liChallengeIndex, BrnStreetData::ScoreType leScoreType );

        BrnStreetData::RoadIndex GetCurrentPlayerRoadIndex();            // :456
        ::CgsID GetForwardPlayerRoadId();                                // :459
        ::CgsID GetLeftPlayerRoadId();                                   // :462
        ::CgsID GetRightPlayerRoadId();                                  // :465
        const CgsResource::ResourceHandle* GetDistrictMapResourceHandle() const;   // :468

        // @ 0x8233F350 / 0x8233F230 / 0x8233F2C0. Tally the player's ruled roads and
        // mirror the result into the ProgressionManager counters (+133456/+133460/+133464).
        s32 GetNumberOfCompleteRoadsRuledByLocalPlayer();
        s32 GetNumberOfParShowTimeRoadsRuledByLocalPlayer();
        s32 GetNumberOfParTimeTrialRoadsRuledByLocalPlayer();

        void ClearUpcomingRoadIds();                                     // :484

        // @ 0x823365A8. Fills the batch query action: per-road id + beaten-par /
        // beaten-friend booleans per score type + the owns-all-roads-offline flag.
        void FillInRoadRulesQuery( GameStateModuleIO::RoadRulesBatchQueryAction* lpRulesQueryAction );

        // ---- retained slice accessors (pre-freeze consumers) -------------------
        // Debug-HUD row accessors (RoadRulesDebugComponent::RenderHUD). Now trivial
        // inline reads of the real tables (the ChallengeData upcast is the row's base).
        const BrnStreetData::ChallengeData* GetPlayerChallengeData( s32 liRoad )
        {
            return &maChallengeData[liRoad];
        }
        const BrnStreetData::ChallengeHighScoreEntry* GetNetChallengeData( s32 liRoad )
        {
            return &maNetworkChallengeData[liRoad];
        }

        // De-inlined member accessor kept for the StreetManagerDebugComponent TU
        // (X360 reads mpProgressionManager directly).
        BrnProgression::ProgressionManager* GetProgressionManager()
        {
            return mpProgressionManager;
        }

        // @ 0x82336070. Store a player challenge score entry for a slot; when
        // lbUpdateHighScore is set the road-rules-changed bit for each dirty score
        // type is raised.
        void SetChallengeUserScore( BrnStreetData::ChallengeIndex liRoadIndex,
                                    const BrnStreetData::ChallengePlayerScoreEntry* lpEntry,
                                    bool lbUpdateHighScore );

        // @ 0x82341780. Road-rule trophy/achievement fan-out for one score type
        // (TIME -> trophies 6/24, CRASH -> 7/25, complete-all -> 8, always 23 +
        // CheckForSpecialCarUnlocks). DWARF :705.
        void UpdateTrophyUnlockOnRoadRuleWin( BrnStreetData::ScoreType leScoreType );

        // Committed static factories (X360 COMDAT names; see the committed .cpp).
        static BrnStreetData::ChallengeHighScoreEntry* CreateHighScoreEntryFromDown(
            BrnStreetData::ChallengeHighScoreEntry* lpEntry,
            int                                     liUnused,
            const CgsNetwork::PlayerName*           lpPlayerNames,
            const int32_t*                          lpScores );
        static BrnStreetData::ChallengePlayerScoreEntry* CreateUserChallengeScoreFr(
            BrnStreetData::ChallengePlayerScoreEntry* lpEntry,
            int                                       liUnused,
            const int32_t*                            lpScores );

    private:
        // ---- upcoming-streets pipeline (this wave) ------------------------------
        // @ 0x82350A88. Per-frame: resolve the player's AI section -> road index,
        // junction-timeout bookkeeping, shortcut-flag GUI action (type 287), then
        // FindUpcomingStreetsByRecursiveWalking (+ FromRoute when lbUseRoute).
        // NOTE the X360 arity: liReserved is carried but unreferenced by the body.
        void UpdateUpcomingStreets( BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface,
                                    BrnAI::AIModuleIO::AICarOutputInterface* lpLastAICarOutputInterface,
                                    GameStateModuleIO::OutputBuffer* lpOutput,
                                    f32 lfSimTimeStep,
                                    s32 liReserved,
                                    bool lbUseRoute );

        // DWARF :636 (not in this wave's ledger).
        void UpdateWrongWayMessages( f32 lfTimeGoingTheWrongWay, f32 lfSimTimeStep );

        // @ 0x8234E8C8. Seed the walk from the player's section: pick the most
        // forward portal, derive the section exit-right vector, WalkAISection, then
        // hopeful-road stickiness + SendUpcomingRoadMessage.
        void FindUpcomingStreetsByRecursiveWalking( BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface,
                                                    GameStateModuleIO::OutputBuffer* lpOutput,
                                                    BrnAI::AIModuleIO::AICarOutputInterface* lpLastAICarOutputInterface,
                                                    f32 lfSimTimeStep,
                                                    u16 luSectionIndex,
                                                    bool lbInRace );

        // @ 0x8233EAE8. Recursive breadth-limited portal walk collecting left/right/
        // forward road candidates (SectionWalkData + CompareSectionWalkData qsort).
        void WalkAISection( const rw::math::fpu::Vector3Template<f32>& lrEntryPos,
                            const rw::math::fpu::Vector3Template<f32>& lrEntryDir,
                            const BrnAI::AISection* lpSection, u16 luSectionIndex,
                            const rw::math::fpu::Vector3Template<f32>& lrPortalPos,
                            const rw::math::fpu::Vector3Template<f32>& lrExitRight,
                            ESectionEntryDirection leEntryDirection,
                            s32 liDepth, s32 liDirectionPersistance, bool lbFirst );

        // @ 0x8234EF90. Route-guided variant: walk the player's route nodes ahead,
        // classify the upcoming turn (interstate special cases), set hopeful roads.
        void FindUpcomingStreetsFromRoute( BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface,
                                           BrnAI::AIModuleIO::AICarOutputInterface* lpLastAICarOutputInterface,
                                           GameStateModuleIO::OutputBuffer* lpOutput,
                                           f32 lfSimTimeStep );

        // @ 0x82348798. Packs the upcoming-road GUI message (left/right road ids +
        // score summaries) and posts it; perf-monitored.
        void SendUpcomingRoadMessage( GameStateModuleIO::OutputBuffer* lpOutput,
                                      BrnAI::AIModuleIO::AICarOutputInterface* lpLastAICarOutputInterface,
                                      bool lbForce,
                                      BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface );

        // DWARF :683/:689/:695/:700 (not in this wave's ledger).
        f32  GetOverallAngleForRoadTurning( BrnAI::AIModuleIO::AICarOutputInterface* lpLastAICarOutputInterface,
                                            BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface );
        void DrawCurrentNodeRoute( BrnAI::AIModuleIO::AICarOutputInterface* lpLastAICarOutputInterface,
                                   BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface );
        void DrawPlayerRoute( BrnAI::AIModuleIO::AICarOutputInterface* lpLastAICarOutputInterface,
                              BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface );
        bool IsRoadOnTheInterState( BrnStreetData::RoadIndex liRoadIndex );

        // DWARF :711 (not in this wave's ledger). Param is the BrnNetworkSharedIO
        // RoadRulesDownloadedQueue (EventQueue<RoadRulesDownloadEvent,40>).
        // UpdateFriendHighScores IS in this wave -- @ 0x82348C80.
        void UpdateFriendHighScores( const void* lpDownloadedQueue,
                                     GameStateModuleIO::OutputBuffer* lpOutput );

        // @ 0x823172F0. Distance from the player to the current route's turn node
        // (2D, y dropped); KF_MAX_ROAD_LOOKAHEAD_DIST on the no-route early-outs.
        f32 CalculateDistanceToTurning( BrnAI::AIModuleIO::AICarOutputInterface* lpLastAICarOutputInterface,
                                        BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface );

        // @ 0x82348FC0. Param is the BrnNetworkSharedIO LocalRoadRulesDownloadedQueue
        // (EventQueue<RoadRulesMessageData,40>).
        void UpdateUserScoresFromServerRecords( const void* lpLocalDownloadedQueue,
                                                GameStateModuleIO::OutputBuffer* lpOutput );

        // DWARF :730/:734 (not in this wave's ledger).
        void UpdateBufferedHighScores( f32 lfSimTimeStep, bool lbOnline, GameStateModuleIO::OutputBuffer* lpOutput );
        void UpdateRoadRulesProfileScores();

        // DWARF :740/:746/:751 (not in this wave's ledger; the RoadRulesMessageData
        // param type lives in BrnNetworkSharedIO.h).
        // ChallengeHighScoreEntry CreateHighScoreEntryFromRoadRulesData(const PlayerName*, const RoadRulesMessageData*);
        // ChallengeHighScoreEntry CreateHighScoreEntryFromDownloadData(const PlayerName*, const RoadRulesMessageData*);
        // ChallengePlayerScoreEntry CreateUserChallengeScoreFromRoadRulesData(const RoadRulesMessageData*);

        s32  GetNumberOfRoadsRulesByLocalPlayer();                       // :755
        bool CheckForNewHighScore( s32 liChallengeIndex, BrnStreetData::ChallengeHighScoreEntry* lpEntry );   // :761
        BrnStreetData::ScoreType GetActiveRoadRuleType( s32 leNewActiveRoadRuleType );   // :766 (EActiveRoadRule)
        void GetHighScoreEntry( BrnStreetData::ChallengeIndex liIndex, BrnStreetData::ChallengeHighScoreEntry* lpEntry, bool lbByRoadIndex );   // :773
        void SetChallengeFriendHighScore( BrnStreetData::ChallengeIndex liIndex, const BrnStreetData::ChallengeHighScoreEntry* lpEntry );        // :786
        void SetChallengeFriendDownloadTimestamp( u32 luTimestamp );     // :791
        void ProcessLocalHighScoreEvent( GameStateModuleIO::OutputBuffer* lpOutput, BrnStreetData::ChallengeIndex liIndex, BrnStreetData::ScoreType leScoreType );   // :798

        // @ 0x8233F560 (this wave). Places the two per-district par rivals for every
        // road: district from the WorldMap2D "Districts" map at the road's mid node,
        // rivals from the progression data (FindRivalsByDistrict).
        void SetupParRivals( const TriggerQueryManager* lpTriggerQueryManager );

        // @ 0x82336360 (this wave). Collects up to liMaxRivals rival CgsIDs whose
        // district matches leDistrict from the progression data's rival table.
        s32 FindRivalsByDistrict( s32 leDistrict, ::CgsID* lpaRivalIds, s32 liMaxRivals );   // BrnWorld::EDistrict

        void ClearSectionWalkingData();                                  // :811
        void ClearUpcomingRoadStickiness();                              // :814

        // @ 0x82324CA8 (this wave). Re-Constructs every net + player table entry
        // (blank player names via PlayerName::Construct("")).
        void ClearAllChallengeData();

        bool ClearAllChallengeDataForBuddy( const CgsNetwork::PlayerName* lpBuddyName );   // :823
        void PushSectionIndex( u16 luSectionIndex );                     // :827
        bool HasSectionAlreadyBeenWalked( u16 luSectionIndex );          // :831
        bool CanContinueWalking();                                       // :834
        s32  GetSaveGameChallengeIndexByRoadID( ::CgsID lRoadID ) const; // :839

        // @ 0x82317290 (this wave). Unordered scan of KAA_FOLLOW_ON_ROAD_PAIRS.
        bool AreRoadsAFollowOnPair( BrnStreetData::RoadIndex liRoadIndex1, BrnStreetData::RoadIndex liRoadIndex2 );

    private:
        // ===== member layout (DWARF :499..:618 order; X360 offsets in comments) =====
        BrnStreetData::ChallengeHighScoreEntry   maNetworkChallengeData[KI_MAX_CHALLENGES];   // +0x0000
        BrnStreetData::ChallengePlayerScoreEntry maChallengeData[KI_MAX_CHALLENGES];          // +0x0E00
        ::CgsID                                  maaParRivalIds[KI_MAX_CHALLENGES][BrnStreetData::E_SCORE_TYPE_COUNT];   // +0x1800
        NewHighScoreBuffer                       mNewHighScoreBuffer;                          // +0x1C00 (count word @ +0x1CA0)
        CgsContainers::BitArray<64u>             maRoadRulesChangedBitArrays[BrnStreetData::E_SCORE_TYPE_COUNT];   // +0x1CA8
        f32                                      mfUnbufferTimer;                              // +0x1CB8
        s32                                      miNumScoresLost;                              // +0x1CBC
        BrnStreetData::ScoreType                 meActiveRoadRuleType;                         // +0x1CC0 (Construct inits E_SCORE_TYPE_COUNT)
        bool                                     mbTooManyHighScoresToBuffer;                  // +0x1CC4
        bool                                     mbFirstDownload;                              // +0x1CC5 (Construct inits true)

        CgsResource::ResourcePtr<BrnStreetData::StreetData> mpStreetData;                      // +0x1CC8 (0x20 X360 stride)
        CgsResource::ResourcePtr<BrnAI::AISectionsData>     mpAISectionData;                   // +0x1CE8
        CgsResource::ResourceHandle              mDistrictMapResourceHandle;                   // +0x1D08

        BrnProgression::ProgressionManager*      mpProgressionManager;                         // +0x1D10
        GameStateModule*                         mpGameStateModule;                            // +0x1D14
        RoadRulesManager*                        mpRoadRulesManager;                           // +0x1D18

        BrnStreetData::RoadIndex                 miCurrentPlayerRoadIndex;                     // +0x1D1C (-1 init)
        BrnStreetData::RoadIndex                 miLastPlayerRoadIndex;                        // +0x1D20 (-1)
        BrnStreetData::RoadIndex                 miHopefulForwardPlayerRoadIndex;              // +0x1D24 (-1)
        BrnStreetData::RoadIndex                 miHopefulLeftPlayerRoadIndex;                 // +0x1D28 (-1)
        BrnStreetData::RoadIndex                 miHopefulRightPlayerRoadIndex;                // +0x1D2C (-1)
        BrnStreetData::RoadIndex                 miNewestForwardPlayerRoadIndex;               // +0x1D30 (-1)
        BrnStreetData::RoadIndex                 miNewestLeftPlayerRoadIndex;                  // +0x1D34 (-1)
        BrnStreetData::RoadIndex                 miNewestRightPlayerRoadIndex;                 // +0x1D38 (-1)
        BrnStreetData::RoadIndex                 miOriginalInterStateRoadIndex;                // +0x1D3C (-1)

        rw::math::fpu::Vector3Template<f32>     mCurrentPlayerSectionExitRight;               // +0x1D40 (12B)
        rw::math::fpu::Vector3Template<f32>     mFirstJunctionPosition;                       // +0x1D4C
        rw::math::fpu::Vector3Template<f32>     mLastNode;                                    // +0x1D58
        rw::math::fpu::Vector3Template<f32>     mSecondLastNode;                              // +0x1D64

        s32                                      miNewRoadNodeIndex;                           // +0x1D70
        s32                                      miLastRoadNodeIndex;                          // +0x1D74
        f32                                      mfHopefulRoadTimer;                           // +0x1D78
        f32                                      mfTimeInJunction;                             // +0x1D7C
        BrnStreetData::RoadIndex                 mLeftRoadIndex;                               // +0x1D80 (-1)
        BrnStreetData::RoadIndex                 mRightRoadIndex;                              // +0x1D84 (-1)
        f32                                      mfTotalTimeGoinTheWrongWay;                   // +0x1D88 (sic -- DWARF spelling)
        f32                                      mfGuidanceLockTime;                           // +0x1D8C

        s32                                      miNextFreeSectionSlot;                        // +0x1D90
        u16                                      mauWalkedSections[KI_MAX_SECTIONS_TO_WALK];   // +0x1D94 (50B)

        bool                                     mbNeedToUpdateUpcomingRoads;                  // +0x1DC6 (Construct inits true)
        bool                                     mbRightRoadHighlighted;                       // +0x1DC7
        bool                                     mbLeftRoadHighlighted;                        // +0x1DC8
        bool                                     mbNewRoadIsInterStateExit;                    // +0x1DC9
        bool                                     mbPlayerOnInterstateExit;                     // +0x1DCA
        bool                                     mbJunctionPosSet;                             // +0x1DCB
        bool                                     mbWrongWay;                                   // +0x1DCC
        bool                                     mbLockRightSign;                              // +0x1DCD
        bool                                     mbLockLeftSign;                               // +0x1DCE
        bool                                     mbPlayerIsInAShortcut;                        // +0x1DCF

        // ⚠️ [FLAG PC bring-up] the three stage words carry in-class initialisers holding the
        // EXACT values StreetManager::Construct @0x82335978 writes into them. Reason, stated
        // rather than hidden: GameStateModule now embeds this manager BY VALUE (DWARF :425) but
        // does NOT yet call StreetManager::Construct -- that Construct's first statement is
        // mStreetManagerDebugComponent.Construct(this), which emits the debug component's vtable
        // and therefore hard-references its virtual Update/OnActivate/RenderHUD and the ~15
        // still-unhomed StreetManager/ScoringSystem/ProgressionManager accessors those call
        // (MEASURED with dumpbin /SYMBOLS over the mount candidate set). Without the stage words
        // seeded, LoadStreetData would switch on an INDETERMINATE value on the very first tick.
        // These initialisers do not change the layout (the _AssertLayout offsets below still
        // hold) and they are not invented values -- they are Construct's own three stores.
        // Same doctrine as BrnGameStateModule.h's `mpOutputBuffer = 0`.
        // DELETE-WHEN GameStateModule::Construct calls StreetManager::Construct for real.
        LoadStage                                meLoadStage            = E_LOAD_NOT_STARTED;            // +0x1DD0
        AILoadStage                              meAILoadStage          = E_AI_DATA_LOAD_NOT_STARTED;    // +0x1DD4
        DistrictMapLoadStage                     meDistrictMapLoadStage = E_DISTRICT_MAP_LOAD_REQUEST;   // +0x1DD8

        s32                                      miUpcomingRoadsPM;                            // +0x1DDC ("UpcomingRoads")
        s32                                      miUpcomingRoadsSentMessagePM;                 // +0x1DE0 ("UpcomingRoadsSendMessage")
        s32                                      miUpcomingRoadsQSortPM;                       // +0x1DE4 ("UpcomingRoadsQSort")
        s32                                      miSetRoadRuleChallengeDataPM;                 // +0x1DE8 ("SetRoadRuleChallengeData")
        s32                                      miSetRoadRuleNetworkHighScoresPM;             // +0x1DEC ("SetRoadRuleNetworkHighScores")

        StreetManagerDebugComponent              mStreetManagerDebugComponent;                 // +0x1DF0 (0x18 on X360)

        // ---- layout pins (never called) ----------------------------------------
        // Absolute pins only over the pointer-free prefix (byte-identical on the
        // LLP64 gate); relative anchors inside the pointer-free tail run.
        static void _AssertLayout()
        {
            // prefix -- absolute (no pointers anywhere before mpStreetData)
            static_assert( sizeof(BrnStreetData::ChallengeHighScoreEntry) == 56,  "net challenge row stride 0x38" );
            static_assert( sizeof(BrnStreetData::ChallengePlayerScoreEntry) == 40, "player challenge row stride 0x28" );
            static_assert( sizeof(BufferedNewHighScore) == 32,                    "buffered high score stride 0x20" );
            static_assert( offsetof(StreetManager, maChallengeData)             == 0x0E00, "player table @ +0xE00" );
            static_assert( offsetof(StreetManager, maaParRivalIds)              == 0x1800, "par rival ids @ +0x1800" );
            static_assert( offsetof(StreetManager, mNewHighScoreBuffer)         == 0x1C00, "high score buffer @ +0x1C00" );
            static_assert( offsetof(StreetManager, maRoadRulesChangedBitArrays) == 0x1CA8, "changed bits @ +0x1CA8" );
            static_assert( offsetof(StreetManager, mfUnbufferTimer)             == 0x1CB8, "unbuffer timer @ +0x1CB8" );
            static_assert( offsetof(StreetManager, miNumScoresLost)             == 0x1CBC, "scores lost @ +0x1CBC" );
            static_assert( offsetof(StreetManager, meActiveRoadRuleType)        == 0x1CC0, "active rule type @ +0x1CC0" );
            static_assert( offsetof(StreetManager, mbTooManyHighScoresToBuffer) == 0x1CC4, "too-many flag @ +0x1CC4" );
            static_assert( offsetof(StreetManager, mbFirstDownload)             == 0x1CC5, "first download @ +0x1CC5" );
            static_assert( offsetof(StreetManager, mpStreetData)                == 0x1CC8, "street data ptr @ +0x1CC8" );

            // pointer-free tail run -- relative anchors (base: miCurrentPlayerRoadIndex,
            // X360 +0x1D1C). Deltas hold on any LP/LLP model.
            static_assert( offsetof(StreetManager, miOriginalInterStateRoadIndex) - offsetof(StreetManager, miCurrentPlayerRoadIndex) == 0x20, "nine RoadIndex run" );
            static_assert( offsetof(StreetManager, mCurrentPlayerSectionExitRight) - offsetof(StreetManager, miCurrentPlayerRoadIndex) == 0x24, "vectors follow the road indices" );
            static_assert( offsetof(StreetManager, mLastNode)              - offsetof(StreetManager, miCurrentPlayerRoadIndex) == 0x3C, "12-byte Vector3 stride" );
            static_assert( offsetof(StreetManager, miNewRoadNodeIndex)     - offsetof(StreetManager, miCurrentPlayerRoadIndex) == 0x54, "post-vector scalars" );
            static_assert( offsetof(StreetManager, miNextFreeSectionSlot)  - offsetof(StreetManager, miCurrentPlayerRoadIndex) == 0x74, "walk slot @ X360 +0x1D90" );
            static_assert( offsetof(StreetManager, mauWalkedSections)      - offsetof(StreetManager, miCurrentPlayerRoadIndex) == 0x78, "walked sections @ X360 +0x1D94" );
            static_assert( offsetof(StreetManager, mbNeedToUpdateUpcomingRoads) - offsetof(StreetManager, miCurrentPlayerRoadIndex) == 0xAA, "bool block @ X360 +0x1DC6" );
            static_assert( offsetof(StreetManager, meLoadStage)            - offsetof(StreetManager, miCurrentPlayerRoadIndex) == 0xB4, "load stages @ X360 +0x1DD0" );
            static_assert( offsetof(StreetManager, miUpcomingRoadsPM)      - offsetof(StreetManager, miCurrentPlayerRoadIndex) == 0xC0, "perfmon ids @ X360 +0x1DDC" );
        }
    };
}
