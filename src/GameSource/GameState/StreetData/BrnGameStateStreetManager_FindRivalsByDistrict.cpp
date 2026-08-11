// ===========================================================================
// GameSource/GameState/StreetData/BrnGameStateStreetManager_FindRivalsByDistrict.cpp
//   BrnGameState::StreetManager::FindRivalsByDistrict  @ 0x82336360
//
// ⭐ SetupParRivals' ONLY unhomed callee. Split out of the wave-C group-4 partfile
// BrnGameStateStreetManager_wC_04.cpp (which also carries FillInRoadRulesQuery @0x823365A8
// and GetNumberOfCompleteRoadsRuledByLocalPlayer @0x8233F350) ON PURPOSE, and it is MEASURED
// (cl /c with the build's own flags + dumpbin /SYMBOLS against the defined-symbol set of
// build\game\obj): mounting the whole wC_04 partfile costs FOUR unresolved externals --
// StreetManager::GetStreetData, ::HasPlayerBeatenParScore, ::HasPlayerBeatenFriendScore and
// BrnProgression::Rival::GetDistrict -- all four pulled in by those two siblings' road-score
// tallies. FindRivalsByDistrict touches only the last of them (now a header inline in
// BrnRival.h), so this split costs ZERO. Established repo pattern
// (BrnGameStateStreetManager_Prepare.cpp, BrnTriggerQueryManager_Prepare.cpp,
// BrnCarSelectManager_CarChange.cpp). Fold back into wC_04 when the three score accessors land.
//
// The body below is MOVED, not copied -- wC_04 no longer defines it (a duplicate definition
// would be LNK2005 the moment both TUs mount).
// ===========================================================================

#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"

#include "GameSource/GameState/Progression/BrnProgressionManager.h"  // ProgressionManager::GetProgressionData
#include "SharedClasses/Progression/BrnProgressionData.h"            // ProgressionData::GetRival / GetRivalCount
#include "SharedClasses/Progression/BrnRival.h"                      // Rival::GetId / GetDistrict

namespace BrnGameState
{

// ---------------------------------------------------------------------------
// @ 0x82336360. Collect up to liMaxRivals rival ids whose district matches leDistrict from the
// progression data's rival table, returning how many were written.
//
// Store for store against the asm:
//   0x8233636C-A8  the null-checked ResourcePtr read at mpProgressionManager (+0x1D10) + 133348
//                  == the committed ProgressionManager::GetProgressionData() accessor.
//   0x823363AC     `lwz r11, 0x2C(r28)` == ProgressionData::GetRivalCount(), re-materialised on
//                  EVERY loop-condition evaluation (0x82336420 reloads it) -- NOT hoisted here.
//   0x823363D0     `cmpw r27, r23 / bge` -- the found-count cut-out is tested at the TOP of the
//                  iteration, BEFORE the rival is fetched, so the last iteration never reaches
//                  GetRival's bounds assert.
//   0x823363D8-F4  the baked "liIndex < miRivalCount" assert (BrnProgressionData.h:460) belongs
//                  to ProgressionData::GetRival -- it is NOT duplicated here.
//   0x823363F8-08  `lwz 0x28 / add r31 (stride 0x38) / lbz 0x14 / extsb` == GetRival(i)->
//                  GetDistrict(), compared SIGNED against the wanted district.
//   0x82336410-1C  `ld r11, 0(r11) / std r11, 0(r30) / addi r30, r30, 8` == an 8-byte CgsID
//                  (Rival::GetId()) written to the caller's array, which is why lpaRivalIds is
//                  CgsID* and the count post-increments.
//
// ⚠️ NO NULL GUARD HERE, ON EITHER POINTER -- kept faithful, DELIBERATELY, and this is not an
// oversight left over from the 2026-08-11 null-back-pointer crash. Two reads could fault:
//   * mpProgressionManager -- the member the first post-un-park boot AV'd on. Its writer is now
//     GameStateModule::Construct -> StreetManager::WireOwnerPointers, and its tripwire assert
//     lives in the ONE caller, SetupParRivals, which early-outs before reaching this function.
//   * lpProgressionData -- the X360 itself loads GetRivalCount off a possibly-NULL pointer
//     (0x823363AC reads 0x2C(r28) with r28 == 0 on the null path). SetupParRivals guards that
//     one too, upstream.
// SetupParRivals @0x8233F560 is this function's only caller in the whole image (verified by
// grep over the tree and by the X360 xrefs), so guarding once upstream covers it without
// putting a check in a body the console does not have one in. If a SECOND caller is ever
// mounted, it must carry the same two guards -- or this function grows them and the note goes.
// ---------------------------------------------------------------------------
s32 StreetManager::FindRivalsByDistrict( s32 leDistrict, ::CgsID* lpaRivalIds, s32 liMaxRivals )
{
    const BrnProgression::ProgressionData* lpProgressionData = mpProgressionManager->GetProgressionData();

    s32 liNumberOfRivalsFound = 0;

    for ( s32 liRivalIndex = 0; liRivalIndex < lpProgressionData->GetRivalCount(); ++liRivalIndex )
    {
        if ( liNumberOfRivalsFound >= liMaxRivals )
        {
            break;
        }

        const BrnProgression::Rival* lpRival = lpProgressionData->GetRival( liRivalIndex );

        if ( static_cast<s32>( lpRival->GetDistrict() ) == leDistrict )
        {
            lpaRivalIds[liNumberOfRivalsFound] = lpRival->GetId();
            ++liNumberOfRivalsFound;
        }
    }

    return liNumberOfRivalsFound;
}

} // namespace BrnGameState
