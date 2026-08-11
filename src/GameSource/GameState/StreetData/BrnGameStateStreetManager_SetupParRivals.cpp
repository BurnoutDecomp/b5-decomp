// ===========================================================================
// GameSource/GameState/StreetData/BrnGameStateStreetManager_SetupParRivals.cpp
//   BrnGameState::StreetManager::SetupParRivals  @ 0x8233F560
//
// ⭐ THE SECOND HALF OF Prepare2. StreetManager::Prepare2 @0x823509D8 is exactly
// `if (LoadStreetData(out, rq)) { SetupParRivals(tqm); return 1; } return 0;` and this is the
// half GameStateModule::Prepare2 case 2 had been parked on since the district-map wave.
//
// SPLIT OUT of the wave-C group-2 partfile BrnGameStateStreetManager_wC_02.cpp (which also
// carries ProcessScoreRequestEvent @0x8234A240) ON PURPOSE, and it is MEASURED (cl /c with the
// build's own flags + dumpbin /SYMBOLS against the defined-symbol set of build\game\obj):
// mounting the whole wC_02 partfile costs THIRTEEN unresolved externals -- ChallengeData::
// GetScore/SetScore/CompareScores, ChallengeParScoresEntry::Copy/GetScore, ChallengeHighScore
// Entry pieces, StreetManager::GetHighScoreEntry/GetChallengeUserScore/GetParRivalId,
// OutputBuffer::GetGuiOutputQueue, CgsNetwork::PlayerName::Construct, CgsCore::SPrintf and the
// CgsDev::StrStream chain (which also drags _purecall / type_info / operator delete) -- and
// EVERY ONE of them belongs to ProcessScoreRequestEvent. SetupParRivals touches none of it, so
// this split costs ZERO. Established repo pattern (BrnGameStateStreetManager_Prepare.cpp,
// BrnTriggerQueryManager_Prepare.cpp, BrnCarSelectManager_CarChange.cpp). Fold back into wC_02
// when the score-entry family lands.
//
// The body below is MOVED, not copied -- wC_02 no longer defines it.
//
// ===========================================================================
// ⭐ MEMBER AUDIT -- EVERY `this->` READ ON THIS LEG AND ITS WRITER (2026-08-11).
//
// Written after the first boot AV'd on the one member nobody had checked. StreetManager is not
// Construct()ed on PC (see WireOwnerPointers in the header), so "the member exists" does not
// imply "something wrote it". Anyone extending this leg must extend this table.
//
//   member                       read by            writer on the PC path                status
//   ---------------------------  -----------------  -----------------------------------  ------
//   mpProgressionManager         this, _FindRivals  GameStateModule::Construct ->         ⭐ WIRED
//                                                   StreetManager::WireOwnerPointers      this wave
//   mpStreetData (ResourcePtr)   this               StreetManager::LoadStreetData         ✅ gated:
//                                                   @0x8234F630 (wB_01, mounted)          Prepare2
//                                                   -- the ONLY writer in the image       returns
//                                                                                         false
//                                                                                         until it
//                                                                                         binds
//   mDistrictMapResourceHandle   this               StreetManager::LoadDistrictMap        ✅ +guard
//                                                   @0x8234FB98 (wB_01, mounted), pumped  below
//                                                   by Prepare stage 23
//   maaParRivalIds               this               THIS function (pure output)           ✅ n/a
//   mpGameStateModule            -- not read on this leg (wired anyway, it is free)
//   mpRoadRulesManager           -- not read on this leg; stays 0, see the header note
//
// Members reached THROUGH those pointers are audited at their own guards below:
// ProgressionManager::mpProgressionData (writer: ProgressionManager::LoadProgressionData, run by
// GameStateModule::Prepare2 case 0/1, which completes before case 2 falls into the street leg).
// ===========================================================================

#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"

#include "GameSource/GameState/Progression/BrnProgressionManager.h"    // ProgressionManager::GetProgressionData
#include "GameSource/GameState/TriggerQueryManager/BrnTriggerQueryManager.h" // TriggerQueryManager::GetTriggerData
#include "SharedClasses/Progression/BrnProgressionData.h"              // ProgressionData::GetRival / GetRivalCount
#include "SharedClasses/Progression/BrnRival.h"                        // BrnProgression::Rival::GetId
#include "SharedClasses/StreetData/BrnStreetData.h"                    // StreetData::GetRoadCount / GetRoad / Road::GetRoadLimitId0
#include "SharedClasses/StreetData/BrnChallengeData.h"                 // BrnStreetData::E_SCORE_TYPE_COUNT
#include "SharedClasses/Trigger/BrnTriggerData.h"                      // BrnTrigger::TriggerData::GetGenericRegion(Count)
#include "SharedClasses/Trigger/BrnGenericRegion.h"                    // BrnTrigger::GenericRegion (+ TriggerRegion / BoxRegion)
#include "GameShared/GameClasses/World/CgsWorldMap2D.h"                // CgsWorld::WorldMap2D / KU_INVALID_WORLD_MAP_VALUE
#include "GameShared/GameClasses/Numeric/CgsRandom.h"                  // CgsNumeric::Random
#include "GameShared/GameClasses/System/Resource/CgsBinaryFileResource.h" // CgsResource::BinaryFileResource::GetData
#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT

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

    // ⚠️ PC HARDENING #1 (2026-08-11) -- documented deviation, NOT a placeholder, and it is
    // THE FIX'S TRIPWIRE, not the fix. THIS IS THE LINE THAT CRASHED on the first boot after the
    // un-park: EXCEPTION_ACCESS_VIOLATION reading 0x1D9E8 inside GetProgressionData, because
    // mpProgressionManager was NULL (0x1D9E8 == 121320 == the host
    // offsetof(ProgressionManager, mpProgressionData), i.e. a member read off a null base).
    // The console cannot hit this: StreetManager::Construct @0x82335978 stores the back-pointer
    // at +0x1D10 and asserts it non-null (BrnGameStateStreetManager.cpp:151) long before any
    // Prepare2 runs. On PC that Construct is still parked, so the pointer is wired by the named
    // subset helper StreetManager::WireOwnerPointers, called from GameStateModule::Construct at
    // the console's own call position -- THAT is the fix. This guard exists so that if the wiring
    // ever regresses (or the helper is deleted before the real Construct lands) the failure is a
    // named assert instead of an AV in a callee.
    // DELETE-WHEN GameStateModule::Construct calls the real StreetManager::Construct, whose own
    // assert then covers it.
    CGS_ASSERT( mpProgressionManager != 0,
                "[PC] SetupParRivals: mpProgressionManager was never wired "
                "(StreetManager::Construct is parked -- see WireOwnerPointers)" );
    if ( mpProgressionManager == 0 )
    {
        return;
    }

    // The committed accessor IS the asm's null-checked ResourcePtr read
    // (mpProgressionManager + 133348).
    const BrnProgression::ProgressionData* lpProgressionData = mpProgressionManager->GetProgressionData();

    // ⚠️ PC HARDENING #2 -- the SECOND null on this path, guarded in the same pass rather than
    // waiting for it to crash. GetProgressionData() legitimately returns NULL while the
    // PROGRESSION.DAT acquire is outstanding, and the fallback branch below dereferences
    // lpProgressionData unconditionally (GetRivalCount / GetRival(0)) exactly as the console
    // does. The console is safe by ORDERING -- GameStateModule::Prepare2 @0x8239ED10 runs its
    // progression case 0/1 to completion before falling into the street case 2, and
    // ProgressionManager::LoadProgressionData is the only writer of mpProgressionData -- and the
    // PC path reproduces that ordering, so this should never fire. It is here because the
    // district-map handle taught the same lesson this wave: "an earlier stage guarantees it" is
    // a claim worth making testable.
    // DELETE-WHEN the bring-up diagnostics go.
    CGS_ASSERT( lpProgressionData != 0,
                "[PC] SetupParRivals: GetProgressionData() is null -- the progression leg of "
                "GameStateModule::Prepare2 did not complete before the street leg" );
    if ( lpProgressionData == 0 )
    {
        return;
    }

    CgsNumeric::Random lRandom;
    lRandom.Construct();
    lRandom.SetSeed( KU_PAR_RIVAL_SELECTION_SEED );

    // ⚠️ PC HARDENING (2026-08-11) -- documented deviation, NOT a placeholder.
    // The console dereferences mDistrictMapResourceHandle UNCONDITIONALLY here (0x8233F5BC
    // `lwz r10, 0x1D08(r29)` straight into 0x8233F5EC `lwz r11, 0(r10)`): it has no null
    // check and no assert, because on the X360 the acquire is guaranteed to have resolved
    // (StuntManager::Prepare loaded Districts.dat 19 prepare-stages before StreetManager
    // acquired it). On PC that guarantee is a chain of reconstructed stages, and a broken
    // link anywhere in it turns this into an access violation with no diagnostic -- which is
    // exactly what a tagged resource id in LoadDistrictMap produced until this same wave
    // (see the boot-log evidence in BrnGameStateStreetManager_wB_01.cpp). One early-out with
    // a loud assert is the difference between a named failure and a silent crash.
    // The guard NEVER fires on a healthy boot; if it does, the district-map acquire failed
    // (check the "[StreetManager] district map:" line in BrnGameStateStreetManager_Prepare.cpp)
    // and every road's par rivals stay at their Construct-time zero -- which is degraded, but
    // it is the same state the whole leg was parked in before this wave.
    // DELETE-WHEN: the PC bring-up diagnostics go, i.e. once the district-map acquire has a
    // regression test behind it. Precedent for keeping a PC-only guard in an otherwise
    // faithful body: BrnWorldModule.cpp's "KEEP THE GUARD" block.
    CGS_ASSERT( mDistrictMapResourceHandle.mpResourceMemory != 0,
                "[PC] SetupParRivals: the \"Districts\" acquire never resolved" );
    if ( mDistrictMapResourceHandle.mpResourceMemory == 0 )
    {
        return;
    }

    // The district-map handle slot holds a pointer to the resource-memory
    // pointer; the X360 folds BinaryFileResource::GetData() into its caller as
    // `base + *(u32*)(base + 4)` (0x8233F5EC..0x8233F5F4).
    const CgsResource::BinaryFileResource* lpBinaryFileResource =
        *reinterpret_cast<const CgsResource::BinaryFileResource* const*>( mDistrictMapResourceHandle.mpResourceMemory );

    CGS_ASSERT( lpBinaryFileResource != 0,
                "[PC] SetupParRivals: the district-map entry has no main-memory resource" );
    if ( lpBinaryFileResource == 0 )
    {
        return;
    }

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

            // 0x8233F758 `ld r10, 0x18(r23)` == Road::GetRoadLimitId0() (an 8-byte CgsID),
            // compared 64-bit-wide against the sign-extended 32-bit GenericRegion::mId.
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
                        // RandomInt owns the baked "liMax >= liMin" / "luMod > 0" asserts
                        // (CgsRandom.h:320/:323 -- the console fires them once per pick, which
                        // is why the call is inside the loop rather than hoisted).
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

} // namespace BrnGameState
