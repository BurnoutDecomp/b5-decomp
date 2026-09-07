#include "GameSource/World/Bridges/WorldBridgeEntityModulesToEntityModules.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // gpDebugPrint / gxMessageFilterFlags (the DIAG witness only)
#include "GameSource/World/EntityModules/TriggerEntityModule/SharedIO/BrnTriggerEntityModuleInputInterface.h" // TriggerManagementInputInterface (real aggregate)

#include <cstdlib>   // getenv (the BRN_TRAFFIC_DIAG witness below)

// WorldModule entity-module -> entity-module bridges -- reconstructed from
// BURNOUT_X360_ARTIST.XEX @ 0x827A52B0 / 0x827A51F0 / 0x827AD788 (this TU's DWARF home is
// WorldBridgeEntityModulesToEntityModules.cpp; signatures/locals/callee names verbatim from
// the PS3 DecFIGS BrnWorldBridgesUnity.cpp dump). Modelled as namespace functions whose
// leading lpWorldModule arg is the X360 r3 (the WorldModule `this`), per the committed bridge
// precedent (WorldBridgeInputToEntityModules.cpp / WorldBridgeToEntityModules.cpp).

namespace
{
    // ---- [DIAG] NOT IN THE X360 BINARY. OFF unless BRN_TRAFFIC_DIAG is set. ----------------
    // Same opt-in switch the traffic partfiles already use
    // (BrnPhysicalTrafficManager_Remove.cpp:56). This is the runtime witness that the
    // traffic->trigger hand-off actually reaches the trigger module now that this TU is
    // mounted: the source's two queue lengths and the destination's, taken just before the
    // merge. Budgeted, because the bridge runs every frame. DELETE-WHEN-STABLE.
    bool TrafficDiagEnabled()
    {
        static const bool sbEnabled = (getenv("BRN_TRAFFIC_DIAG") != 0);
        return sbEnabled;
    }

    const s32 KI_TRIGGER_MERGE_WITNESS_BUDGET = 24;
    s32       giTriggerMergeWitnessLinesLeft  = KI_TRIGGER_MERGE_WITNESS_BUDGET;
}

namespace WorldModule
{

// ⭐⭐ THIS TU IS MOUNTED AS OF 2026-09-07 (traffic-to-trigger wave), and its two bridges are
// now the ONLY definitions of their symbols in the link -- the inert copies in
// GameSource/World/WorldLinkStubs.cpp were deleted in the same change. The three IO accessors
// that held the mount up are all bodied:
//   TriggerEntityModuleIO::InputBuffer_PreScene::GetInputInterface
//       (BrnTriggerEntityModuleIO_Accessors.cpp / _QueueAccessors.cpp)
//   BrnTrafficIO::OutputBuffer_PreScene::GetTriggerManagementInputInterface
//       (BrnTrafficEntityModuleIO.cpp, X360 0x8279FE00 / 0x82710E78)
//   BrnTrafficIO::OutputBuffer_PostScene::GetTrafficToRaceCarInterface_PostScene   <- the last one
//       (BrnTrafficEntityModuleIO.cpp, X360 0x827A00B0; landed with this change)
//
// ⛔ MOVED OUT 2026-08-01 (car-select hand-off wave): BridgeRaceCarModuleToWorldModule_PreScene
// @0x827A52B0 lives in its own TU, GameSource/World/Bridges/WorldBridgeRaceCarToWorldModule.cpp.
// Now that this TU mounts, folding it back is possible -- but do it as the WorldModule METHOD it
// now is, NOT as a namespace function, and only together with the build-list change that drops
// the other TU.
//
// ⛔ ITS X360-OFFSET CONSTANTS ARE DELETED WITH IT (2026-08-11). They read
//   KU_WORLD_MODULE_PLAYER_ACTIVE_RACE_CAR_INDEX_OFFSET = 6167272 / ..._TYPE_ARRAY = 6167280,
// which are the X360 offsets of meLocalPlayerActiveRaceCarIndex / maeCarControls. On the x64
// PC layout those members sit at 6234776 / 6234784 (compile-time offsetof probe), so the
// constants were a live corruption, not a stopgap. Do not reintroduce them here.

// @ 0x827A51F0 -- WorldBridgeEntityModulesToEntityModules.cpp:69. Latch the traffic
// module's post-scene traffic->race-car interface into the race-car module's pre-physics
// input buffer. Both null tripwires are NON-gating (the X360 falls through after firing);
// the X360 tail returns the forwarded call's result as a register artifact -- the logical
// return type is void.
//
// FLAG cross-home cast: the race-car buffer's TrafficToRaceCarInterface_PostScene resolves to
// the class-level BrnTrafficIO::TrafficToRaceCarInterface_PostScene (BrnRaceCarEntityModuleIO.h
// :301), while the traffic getter returns the OutputBuffer_PostScene-nested struct of the same
// name; both model the SAME X360 payload (the setter block-copies it), so the getter result is
// reinterpret_cast to the setter's pointer type -- the documented cross-home adapter used by the
// sibling bridges.
void BridgeTrafficToRaceCar_PrePhysics(
    void* lpWorldModule,
    BrnWorld::RaceCarEntityModuleIO::InputBuffer_PrePhysics* lpRaceCarInputBuffer_PrePhysics,
    const BrnTraffic::BrnTrafficIO::OutputBuffer_PostScene* lpTrafficOutputBuffer_PostScene)
{
    (void)lpWorldModule;

    CGS_ASSERT(lpRaceCarInputBuffer_PrePhysics != 0, "lpRaceCarInputBuffer_PrePhysics");   // :72
    CGS_ASSERT(lpTrafficOutputBuffer_PostScene != 0, "lpTrafficOutputBuffer_PostScene");   // :73

    lpRaceCarInputBuffer_PrePhysics->SetTrafficToRaceCarInterface_PostScene(
        reinterpret_cast<const BrnWorld::RaceCarEntityModuleIO::InputBuffer_PrePhysics::TrafficToRaceCarInterface_PostScene*>(
            lpTrafficOutputBuffer_PostScene->GetTrafficToRaceCarInterface_PostScene()));
}

// @ 0x827AD788 -- WorldBridgeEntityModulesToEntityModules.cpp:46. Merge the traffic
// module's pre-scene trigger-management output (add + remove trigger queues) into the
// trigger module's pre-scene input buffer. The X360 inlines the aggregate merge as a
// VariableEventQueue<131072,16>::Append (the add queue) followed by an
// InRemoveTriggerEvent-queue Append at +131088 (0x20010); both are reproduced by the
// committed TriggerManagementInputInterface::Append. The X360 tail returns the remove-queue
// Append result in r3; the logical return type is void.
void BridgeTrafficToTrigger_PreScene(
    void* lpWorldModule,
    BrnWorld::TriggerEntityModuleIO::InputBuffer_PreScene* lpTriggerInputBuffer_PreScene,
    const BrnTraffic::BrnTrafficIO::OutputBuffer_PreScene* lpTrafficOutputBuffer_PreScene)
{
    (void)lpWorldModule;

    CGS_ASSERT(lpTriggerInputBuffer_PreScene != 0, "lpTriggerInputBuffer_PreScene");       // :50
    CGS_ASSERT(lpTrafficOutputBuffer_PreScene != 0, "lpTrafficOutputBuffer_PreScene");     // :51

    // FLAG cross-home casts: both IO buffers expose their trigger-management member as opaque
    // storage; both ARE the trigger module's TriggerManagementInputInterface (whose Append
    // performs the X360's inlined add-queue + remove-queue whole-interface merge).
    BrnWorld::TriggerEntityModuleIO::TriggerManagementInputInterface* lpTriggerManagementInput =
        reinterpret_cast<BrnWorld::TriggerEntityModuleIO::TriggerManagementInputInterface*>(
            lpTriggerInputBuffer_PreScene->GetInputInterface());
    const BrnWorld::TriggerEntityModuleIO::TriggerManagementInputInterface* lpSource =
        reinterpret_cast<const BrnWorld::TriggerEntityModuleIO::TriggerManagementInputInterface*>(
            lpTrafficOutputBuffer_PreScene->GetTriggerManagementInputInterface());

    // ---- [DIAG] NOT IN THE X360 BINARY. Opt-in (BRN_TRAFFIC_DIAG), budget 24 lines. --------
    // Reads only. A "[T-trigmerge] ... merge:" line with a non-zero src count is the proof that
    // the trigger module's PRE-SCENE input received traffic trigger events; the one-shot
    // "bridge LIVE, traffic source empty" line below proves the bridge itself is running when
    // there is nothing to carry. No line at all means this TU is not in the link.
    // DELETE-WHEN-STABLE.
    const s32 liSourceAdds    = lpSource->GetAddTriggerEventQueue().GetLength();
    const s32 liSourceRemoves = lpSource->GetRemoveTriggerEventQueue().GetLength();
    if (TrafficDiagEnabled() &&
        (CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
    {
        if (liSourceAdds != 0 || liSourceRemoves != 0)
        {
            if (giTriggerMergeWitnessLinesLeft > 0)
            {
                --giTriggerMergeWitnessLinesLeft;
                *CgsDev::Log::gpDebugPrint
                    << "[T-trigmerge] traffic->trigger PreScene merge: src add=" << liSourceAdds
                    << " remove=" << liSourceRemoves
                    << " dest before add="
                    << lpTriggerManagementInput->GetAddTriggerEventQueue().GetLength()
                    << " remove="
                    << lpTriggerManagementInput->GetRemoveTriggerEventQueue().GetLength()
                    << "\n";
            }
        }
        else
        {
            // One-shot, so a run can tell "this bridge never executed" (no line at all, i.e.
            // still the deleted WorldLinkStubs gate or an unmounted TU) apart from "it executed
            // and the traffic module had nothing queued". The latter is the EXPECTED state until
            // the traffic pre-scene producer that writes BrnTrafficIO::OutputBuffer_PreScene's
            // trigger-management interface lands: the non-const writer accessor (X360 0x82710E78)
            // has no caller in this tree yet.
            static bool sbLoggedEmptySource = false;
            if (!sbLoggedEmptySource)
            {
                sbLoggedEmptySource = true;
                *CgsDev::Log::gpDebugPrint
                    << "[T-trigmerge] bridge LIVE, traffic source empty (no traffic trigger "
                       "events queued this frame)\n";
            }
        }
    }

    lpTriggerManagementInput->Append(*lpSource);
}

}
