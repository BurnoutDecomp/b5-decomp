// ============================================================================
// BrnTrafficEntityModule_wT2_05.cpp
//
//   TrafficEntityModule::UpdateParams_DoTimeSlicedLogic @0x82743FE8  PARTIAL (EXPORT HOLE)
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficParam.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <cstdlib>   // getenv

namespace BrnTraffic
{
namespace
{
    // Same shape as the sibling partfiles'. [DIAG] NOT IN THE X360 BINARY. DELETE-WHEN-STABLE.
    inline void LogMissingLeg(bool& lrbAlreadyLogged, const char* lpcLegNameAndReason)
    {
        if (lrbAlreadyLogged)
        {
            return;
        }
        lrbAlreadyLogged = true;

        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[T2-traffic-leg] TrafficEntityModule leg NOT RECONSTRUCTED, skipped: "
                << lpcLegNameAndReason << " [FLAG PC partial gate]\n";
        }
    }

    bool TrafficDiagEnabled()
    {
        static const bool sbEnabled = (getenv("BRN_TRAFFIC_DIAG") != 0);
        return sbEnabled;
    }

    CgsDev::Log::DebugPrint* TrafficDiagStream()
    {
        if (!TrafficDiagEnabled() || CgsDev::Log::gpDebugPrint == 0)
        {
            return 0;
        }
        return CgsDev::Log::gpDebugPrint;
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::UpdateParams_DoTimeSlicedLogic  @ 0x82743FE8   PARTIAL
//   ARTIST EXPORT HOLE (no per-function JSON). Shape from DWARF :1635 /
//   BrnTrafficUnity.cpp:18355 (.cpp 9770..9880) plus the call site
//   UpdateNonDecisionFrame @0x8274C1A8, which passes [cursor, cursor+100) and the
//   post-physics active-race-car interface.
//
// The slot is cleared through maParamNeedToSlowData directly, not through
// GetParamNeedToSlowData: that accessor asserts muLastParamCalculated >= KU_MAX_PARAMS, which
// is false by construction inside the slicer, and its consumer
// UpdateParam_CheckIfNeedToSlow @0x82738468 likewise indexes inline (asm `16*(luParam+13528)`).
//
// FLAG: the muLastParamCalculated advance is REASONED, not attested (the function has no
// export). It is the only possible advancer: UpdateNonDecisionFrame guards on
// `cursor < KU_MAX_PARAMS` and passes [cursor, cursor+100), and UpdateDecisionFrame's tail is
// the only other writer (it stores 0). DELETE-WHEN the export hole is filled.
// ----------------------------------------------------------------------------
void TrafficEntityModule::UpdateParams_DoTimeSlicedLogic(
    u32 luBeginParam,
    u32 luEndParam,
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*
        lpActiveRaceCarInterface)
{
    CGS_ASSERT(lpActiveRaceCarInterface != 0, "lpActiveRaceCarInterface");
    CGS_ASSERT(luEndParam <= KU_MAX_PARAMS, "luEndParam <= KU_MAX_PARAMS");

    for (u32 luParam = luBeginParam; luParam < luEndParam && luParam < KU_MAX_PARAMS; ++luParam)
    {
        maParamNeedToSlowData[luParam].Clear();
    }

    {
        // GATE: the per-param UpdateParam_CheckIfNeedToSlow @0x82738468 leg.
        // BLOCKER: its four blockers are listed at the gate itself in _wT2_03.cpp -- the
        // unk_82CDA3C0 / unk_82CDA400 / unk_8327F140 vperm control vectors,
        // CalcRaceCarOnStartGridFuzzyScores @0x82716F10, IsPointWithinSquishedCone
        // (declared-only) and DoesParamNeedToStopForStopline @0x827249F8.
        // DELETE-WHEN they land. COST: miBehaviour stays at Clear's -1, so every param runs
        // Param::Initialise's KI_BEHAVIOUR_NORMAL -- no queueing, no stopping at stoplines.
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "UpdateParams_DoTimeSlicedLogic CheckIfNeedToSlow @0x82738468 leg -- blocked on the "
            "unk_82CDA3C0 / unk_82CDA400 / unk_8327F140 vperm control vectors, "
            "CalcRaceCarOnStartGridFuzzyScores @0x82716F10 and IsPointWithinSquishedCone "
            "(declared-only). COST: no queueing, no stoplines");
    }

    muLastParamCalculated = luEndParam;

    if (CgsDev::Log::DebugPrint* lpDiag = TrafficDiagStream())
    {
        // [T2-param] one-shot. DELETE-WHEN-STABLE.
        static bool sbFirst = true;
        if (sbFirst)
        {
            sbFirst = false;
            *lpDiag << "[T2-param] FIRST DoTimeSlicedLogic begin="
                    << static_cast<s32>(luBeginParam)
                    << " end=" << static_cast<s32>(luEndParam) << "\n";
        }
    }
}

}
