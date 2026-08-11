// ===========================================================================
// GameSource/GameState/StreetData/BrnGameStateStreetManager_wC_02.cpp
//   (wave C partfile -- group 2)
//
// Faithful de-optimisation of the X360 BURNOUT_X360_ARTIST.XEX:
//   BrnGameState::StreetManager::SetupParRivals            @ 0x8233F560
//   BrnGameState::StreetManager::ProcessScoreRequestEvent  @ 0x8234A240
//
// Every store / branch / early-out / assert has an asm counterpart; members are
// the frozen-header named fields (never raw-offset casts). The two vector
// immediates SetupParRivals hands WorldMap2D::Construct are the TU's own
// static-initialiser globals unk_82FADAA0 / unk_82FADD60, recovered below.
// ===========================================================================

#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"

#include "GameSource/GameState/BrnGameEvents.h"                        // GameStateModuleIO::RoadRulesScoreRequestEvent
#include "GameSource/GameState/BrnGameStateModuleIO.h"                 // OutputBuffer (GetGameActionQueue / GetGuiOutputQueue)
#include "GameSource/GameState/Progression/BrnProgressionManager.h"    // ProgressionManager::GetProgressionData
#include "GameSource/GameState/TriggerQueryManager/BrnTriggerQueryManager.h" // TriggerQueryManager::GetTriggerData
#include "SharedClasses/Progression/BrnProgressionData.h"              // ProgressionData::GetRival / GetRivalCount
#include "SharedClasses/Progression/BrnRival.h"                        // BrnProgression::Rival::GetId
#include "SharedClasses/Trigger/BrnTriggerData.h"                      // BrnTrigger::TriggerData::GetGenericRegion(Count)
#include "SharedClasses/Trigger/BrnGenericRegion.h"                    // BrnTrigger::GenericRegion (+ TriggerRegion / BoxRegion)
#include "GameShared/GameClasses/World/CgsWorldMap2D.h"                // CgsWorld::WorldMap2D / KU_INVALID_WORLD_MAP_VALUE
#include "GameShared/GameClasses/Numeric/CgsRandom.h"                  // CgsNumeric::Random
#include "GameShared/GameClasses/System/Resource/CgsBinaryFileResource.h" // CgsResource::BinaryFileResource::GetData
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"       // CgsModule::Event / VariableEventQueue<13312,16>::AddEvent
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                // CgsCore::SPrintf
#include "GameShared/GameClasses/Development/CgsStrStream.h"           // CgsDev::StrStream (streamed dev assert)
#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT + KI_MESSAGEBUFFERSIZE

#include <string.h>

// Case-insensitive bounded compare (console strnicmp -> MSVC _strnicmp).
// Committed precedent: GameSource/Game/X360/BrnSystemHWX360.cpp:11.
#if defined(_MSC_VER)
#  define strnicmp _strnicmp
#endif

namespace
{
    // -------------------------------------------------------------------
    // SetupParRivals' two lvx128 sources (@0x8233F5C8 / 0x8233F5D4) are the
    // TU's own zero-initialised .data vectors unk_82FADD60 / unk_82FADAA0,
    // filled by its static initialisers @0x82C4C2E0 / 0x82C4C320 from the
    // rodata floats flt_8200D4EC/E8 (-4208, -3846) and flt_8200D4F4/F0
    // (8270, 6101). SetupParRivals is their only consumer: they are the
    // district-map's world rectangle handed to WorldMap2D::Construct.
    // FLAG: the identifiers are ours (no DWARF name survives); the VALUES are
    // byte-exact from the image.
    // -------------------------------------------------------------------
    const Vector2 KV2_DISTRICT_MAP_WORLD_ORIGIN = { -4208.0f, -3846.0f, 0.0f, 0.0f };   // unk_82FADAA0
    const Vector2 KV2_DISTRICT_MAP_WORLD_SIZE   = {  8270.0f,  6101.0f, 0.0f, 0.0f };   // unk_82FADD60

    // The LCG state SetupParRivals draws its rival picks from, materialised as a
    // 64-bit immediate at 0x8233F5D8..0x8233F5E8 (lis/ori 0x2EC654DA + insrdi of
    // 0xB5E330D0 into the high half). It is NOT the state CgsNumeric::Random::
    // Construct() leaves behind (that spine is DEAD here -- the ring buffer is
    // never read, only muSeed is), so the seed is installed explicitly.
    const u64 KU_PAR_RIVAL_SELECTION_SEED = 0xB5E330D02EC654DAull;

    // -------------------------------------------------------------------
    // The 48-byte road-score response ProcessScoreRequestEvent builds on the
    // stack (sp+0x70..0xA0) and hands to VariableEventQueue<13312,16>::
    // AddEvent(&response, 284, 48).
    // -------------------------------------------------------------------
    struct RoadRulesScoreResponse
    {
        ::CgsID                       mRoadId;                // +0x00
        BrnStreetData::ChallengeIndex miChallengeIndex;       // +0x08
        CgsNetwork::PlayerName        mPlayerName;            // +0x0C (16B)
        s32                           miRuleType;             // +0x1C
        s32                           miUserScore;            // +0x20
        s32                           miHighScore;            // +0x24
        u8                            mbLocalPlayerIsHolder;  // +0x28
        u8                            mbUserScoreIsPar;       // +0x29
        u8                            mbHighScoreIsPar;       // +0x2A
    };
    static_assert( sizeof(RoadRulesScoreResponse) == 48, "road-rules score response is 0x30" );
}

namespace BrnGameState
{

// ---------------------------------------------------------------------------
// @ 0x8233F560. Places the two par rivals for every road: the road's
// RoadLimitId0 selects the matching generic trigger region, that region's box
// position is sampled against the "Districts" WorldMap2D, and two rivals are
// drawn at random out of the district's rival set (falling back to rival 0 when
// the position is off-map or the district has no rivals).
// ---------------------------------------------------------------------------
void StreetManager::SetupParRivals( const TriggerQueryManager* lpTriggerQueryManager )
{
    const BrnTrigger::TriggerData* lpTriggerData = lpTriggerQueryManager->GetTriggerData();

    // The committed accessor IS the asm's null-checked ResourcePtr read
    // (mpProgressionManager + 133348).
    const BrnProgression::ProgressionData* lpProgressionData = mpProgressionManager->GetProgressionData();

    CgsNumeric::Random lRandom;
    lRandom.Construct();
    lRandom.SetSeed( KU_PAR_RIVAL_SELECTION_SEED );

    // The district-map handle slot holds a pointer to the resource-memory
    // pointer; the X360 folds BinaryFileResource::GetData() into its caller as
    // `base + *(u32*)(base + 4)` (0x8233F5EC..0x8233F5F4).
    const CgsResource::BinaryFileResource* lpBinaryFileResource =
        *reinterpret_cast<const CgsResource::BinaryFileResource* const*>( mDistrictMapResourceHandle.mpResourceMemory );

    CgsWorld::WorldMap2D lWorldMap;
    lWorldMap.Construct( lpBinaryFileResource->GetData(),
                         KV2_DISTRICT_MAP_WORLD_ORIGIN,
                         KV2_DISTRICT_MAP_WORLD_SIZE );

    CGS_ASSERT( mpStreetData->GetRoadCount() <= KI_MAX_CHALLENGES,
                "mpStreetData->GetRoadCount() <= KI_MAX_CHALLENGES" );

    for ( BrnStreetData::RoadIndex liRoadIndex = 0;
          liRoadIndex < mpStreetData->GetRoadCount();
          ++liRoadIndex )
    {
        // GetRoad carries its own "liIndex < miRoadCount && liIndex >= 0" bounds assert.
        const BrnStreetData::Road* lpRoad = mpStreetData->GetRoad( liRoadIndex );

        for ( s32 liGenericRegionIndex = 0;
              liGenericRegionIndex < lpTriggerData->GetGenericRegionCount();
              ++liGenericRegionIndex )
        {
            // GetGenericRegion carries its own "liGenericRegionIndex < miGenericRegionCount" assert.
            const BrnTrigger::GenericRegion* lpGenericRegion =
                lpTriggerData->GetGenericRegion( liGenericRegionIndex );

            if ( lpGenericRegion->GetId() == lpRoad->GetRoadLimitId0() )
            {
                // ⚠️ THE SWIZZLE IS THE CALL SITE'S, AND IT IS NOT (x, y).
                // The console loads the region's three position floats into consecutive stack
                // slots (0x8233F770/84/88 -> var_100/var_FC/var_F8), lvx128's them into v0, and
                // then does `vperm v1, v0, v0, v7` (0x8233F7A8) BEFORE the bl to
                // WorldMap2D::GetValue -- i.e. GetValue(Vector3)'s ground-plane swizzle, inlined
                // here. GetValue itself reads only lanes 0 and 1 (`vspltw v10,v1,0` /
                // `vspltw v9,v1,1` @0x82907FF8), so if the wanted lanes were already 0 and 1 the
                // compiler would have passed v0 straight through and emitted no vperm at all.
                // The permute therefore PROVES the sampled pair is not (x, y); a 2D world map
                // over a Y-up world samples (x, z), which is also what the map's own rectangle
                // says (origin (-4208, -3846), size (8270, 6101) -- Paradise City's x/z extents,
                // not its height range).
                // FLAG: the control vector itself (unk_82CDA450) is data the exports do not
                // carry, so the ORDER (x, z) rather than (z, x) is inferred from that rectangle,
                // not read out of the image. Built explicitly here rather than through
                // CgsWorldMap2D.cpp's Vector3 overload, which flattens to (.x, .y).
                Vector2 lSamplePosition;
                const Vector3 lRegionPosition = lpGenericRegion->GetBoxRegion()->GetPosition();
                lSamplePosition.x = lRegionPosition.x;
                lSamplePosition.y = lRegionPosition.z;
                lSamplePosition.z = 0.0f;
                lSamplePosition.w = 0.0f;

                const u8 luDistrict = lWorldMap.GetValue( lSamplePosition );

                ::CgsID laRivalIds[2];
                s32     liRivalsFound = 0;

                if ( luDistrict != CgsWorld::KU_INVALID_WORLD_MAP_VALUE
                  && ( liRivalsFound = FindRivalsByDistrict( luDistrict, laRivalIds, 2 ) ) != 0 )
                {
                    for ( s32 liParRival = 0; liParRival < BrnStreetData::E_SCORE_TYPE_COUNT; ++liParRival )
                    {
                        // RandomInt owns the baked "liMax >= liMin" / "luMod > 0" asserts.
                        maaParRivalIds[liRoadIndex][liParRival] =
                            laRivalIds[ lRandom.RandomInt( 0, liRivalsFound - 1 ) ];
                    }
                }
                else
                {
                    CGS_ASSERT( lpProgressionData->GetRivalCount() > 0,
                                "lpProgressionData->GetRivalCount() > 0" );

                    for ( s32 liParRival = 0; liParRival < BrnStreetData::E_SCORE_TYPE_COUNT; ++liParRival )
                    {
                        // GetRival(0) is re-evaluated per pick -- its "liIndex < miRivalCount"
                        // bounds assert fires once per stored id in the asm.
                        maaParRivalIds[liRoadIndex][liParRival] = lpProgressionData->GetRival( 0 )->GetId();
                    }
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// @ 0x8234A240. Answers a GUI road-score request: formats the road's user and
// high scores against the compiled par score (falling back to the par rival's
// numeric id as the displayed name) and posts the 48-byte response (type 284).
// EVERY score access uses meActiveRoadRuleType -- never a parameter.
// ---------------------------------------------------------------------------
void StreetManager::ProcessScoreRequestEvent(
        GameStateModuleIO::OutputBuffer* lpOutput,
        const GameStateModuleIO::RoadRulesScoreRequestEvent* lpScoreRequestEvent )
{
    if ( meActiveRoadRuleType == BrnStreetData::E_SCORE_TYPE_COUNT )
    {
        return;
    }

    CGS_ASSERT( lpOutput, "lpOutput" );
    CGS_ASSERT( lpScoreRequestEvent, "lpRequestEvent" );

    RoadRulesScoreResponse lResponse;
    lResponse.mPlayerName.Construct( "" );

    const BrnStreetData::ChallengeIndex liChallengeIndex = lpScoreRequestEvent->mRoadChallengeIndex;

    lResponse.miChallengeIndex      = liChallengeIndex;
    lResponse.mbUserScoreIsPar      = 0;
    lResponse.mbHighScoreIsPar      = 0;
    lResponse.mbLocalPlayerIsHolder = 0;
    lResponse.mRoadId               = KAA_SAVE_GAME_CHALLENGE_ROAD_IDS[liChallengeIndex];

    if ( meActiveRoadRuleType == BrnStreetData::E_SCORE_TYPE_TIME )
    {
        lResponse.miRuleType = 1;
    }
    else if ( meActiveRoadRuleType == BrnStreetData::E_SCORE_TYPE_CRASH )
    {
        lResponse.miRuleType = 3;
    }
    else
    {
        char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::Assert::BeginAssert();
        CgsDev::StrStream lStrStream( lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE );
        lStrStream << "Unhandled road rule type ";
        lStrStream << static_cast<s32>( meActiveRoadRuleType );
        lStrStream << " in StreetManager::ProcessScoreRequestEvent\n";
        // 0x8234A34C-58: r4 = the recovered path literal, r5 = 0xC54 (line 3156). Passed
        // verbatim (CgsID.cpp:80 precedent) so the recovered X360 site is not discarded.
        CgsDev::Assert::FireAssert(
            lacMessage,
            "..\\..\\..\\GameSource\\Gamestate/StreetData/BrnGameStateStreetManager.cpp",
            3156 );
        CgsDev::Assert::EndAssert();
    }

    BrnStreetData::ChallengeHighScoreEntry lHighScoreEntry;
    GetHighScoreEntry( liChallengeIndex, &lHighScoreEntry, false );

    BrnStreetData::ChallengePlayerScoreEntry lUserScoreEntry;
    GetChallengeUserScore( liChallengeIndex, &lUserScoreEntry, false );

    BrnStreetData::ChallengeParScoresEntry lParScoresEntry;
    lParScoresEntry.Copy( mpStreetData->GetChallengeParScore( liChallengeIndex ) );

    s32     liParScore   = 0;
    ::CgsID lParRivalId  = 0;
    lParScoresEntry.GetScore( meActiveRoadRuleType, &liParScore, &lParRivalId );

    // ---- user score ------------------------------------------------------
    bool lbUserScoreBeatsPar = false;
    s32  liUserScore         = 0;
    if ( lUserScoreEntry.ContainsData( meActiveRoadRuleType ) )
    {
        liUserScore = lUserScoreEntry.GetScore( meActiveRoadRuleType );
        lbUserScoreBeatsPar =
            ( lUserScoreEntry.CompareScores( meActiveRoadRuleType, liUserScore, liParScore ) < 0 );
    }

    if ( lbUserScoreBeatsPar )
    {
        lResponse.miUserScore = liUserScore;
    }
    else
    {
        char lacRivalName[13];
        CgsCore::SPrintf( lacRivalName, 12, "%llu", GetParRivalId( liChallengeIndex, meActiveRoadRuleType ) );
        lacRivalName[12] = 0;
        lResponse.mPlayerName.Construct( lacRivalName );

        lResponse.miUserScore     = liParScore;
        lResponse.mbUserScoreIsPar = 1;
    }

    // ---- high score ------------------------------------------------------
    CgsNetwork::PlayerName lHighScoreHolderName;
    s32  liHighScore          = 0;
    bool lbHighScoreBeatsPar  = false;
    if ( lHighScoreEntry.ContainsData( meActiveRoadRuleType ) )
    {
        lHighScoreEntry.GetScore( meActiveRoadRuleType, &liHighScore, &lHighScoreHolderName );
        lbHighScoreBeatsPar =
            ( lHighScoreEntry.CompareScores( meActiveRoadRuleType, liHighScore, liParScore ) < 0 );
    }

    if ( lbHighScoreBeatsPar )
    {
        if ( strnicmp( lHighScoreHolderName.macName, KAC_LOCAL_PLAYER_NAME_TEXT, 16 ) != 0 )
        {
            lResponse.mPlayerName.Construct( lHighScoreHolderName.macName );
        }
        else
        {
            lResponse.mbLocalPlayerIsHolder = 1;
        }
        lResponse.miHighScore = liHighScore;
    }
    else
    {
        char lacRivalName[13];
        CgsCore::SPrintf( lacRivalName, 12, "%llu", GetParRivalId( liChallengeIndex, meActiveRoadRuleType ) );
        lacRivalName[12] = 0;
        lResponse.mPlayerName.Construct( lacRivalName );

        lResponse.miHighScore      = liParScore;
        lResponse.mbHighScoreIsPar = 1;
    }

    CGS_ASSERT( lpOutput->GetGameActionQueue(), "lpOutput->GetGameActionQueue()" );

    CgsModule::VariableEventQueue<13312, 16>* lpQueue = lpOutput->GetGuiOutputQueue();
    lpQueue->AddEvent( reinterpret_cast<const CgsModule::Event*>( &lResponse ), 284, 48 );
}

} // namespace BrnGameState
