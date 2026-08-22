// ============================================================================
// BrnTraffic::TrafficEntityModule -- the traffic entity module, a
// CgsModule::ModuleSingleBuffered subclass that owns and ticks the traffic-vehicle fleet
// through the scene-update interface.
//
// Nothing here strides or offsets by an X360 byte value; every body reads a named member.
// The console's offset/element arithmetic stays as the per-function attestation comment,
// because that is what proves which member each line resolves to.
//
// The remainder of the class (constructor, the VMX update/spawn/avoidance pipelines, the
// methods reaching still-partial sub-aggregates, and the helper whose floor constant
// flt_82001CC0 is unattested) is declaration-only and FLAGged in the header.
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h" // CgsDev::Log::gpDebugPrint / CgsDev::Message::gxMessageFilterFlags

#include <cstddef>   // offsetof / size_t

namespace BrnTraffic
{
    // Vehicle / param pool accessors. The three vehicle pools are one array (maVehicles)
    // addressed with three base element indices at a 128-byte stride: 85 / 485 / 684, which
    // differ by exactly 400 and 199. See BrnTrafficConstants.h for the derivation.

    // leak :1590 -- the flat vehicle index space [0, KU_MAX_TOTAL_TRAFFIC).
    Vehicle* TrafficEntityModule::GetVehicle(u32 luIndex)
    {
        CGS_ASSERT(luIndex < KU_MAX_TOTAL_TRAFFIC, "luIndex < KU_MAX_TOTAL_TRAFFIC");
        return &maVehicles[luIndex];
    }

    // @ 0x82707A38 -- `((luIndex + 85) << 7) + this` == &maVehicles[luIndex].
    Vehicle* TrafficEntityModule::GetStandardVehicle(u32 luIndex)
    {
        CGS_ASSERT(luIndex < KU_MAX_STANDARD_TRAFFIC, "luIndex < KU_MAX_STANDARD_TRAFFIC");
        return &maVehicles[luIndex];
    }

    // @ 0x827079D0 -- `((luIndex + 485) << 7) + this`; 485 == 85 + KU_MAX_STANDARD_TRAFFIC.
    Vehicle* TrafficEntityModule::GetStaticVehicle(u32 luIndex)
    {
        CGS_ASSERT(luIndex < KU_MAX_STATIC_TRAFFIC, "luIndex < KU_MAX_STATIC_TRAFFIC");
        return &maVehicles[KU_STATIC_TRAFFIC_OFFSET + luIndex];
    }

    // @ 0x82707AA0 -- `((liIndex + 684) << 7) + this`; 684 == 485 + KU_MAX_STATIC_TRAFFIC.
    // The asm asserts the index in range via the streamed "Out of range trailer vehicle"
    // message; with KU_MAX_TRAILER_TRAFFIC == 1 that is `liIndex == 0`.
    Vehicle* TrafficEntityModule::GetTrailerVehicle(s32 liIndex)
    {
        CGS_ASSERT(liIndex >= 0 && static_cast<u32>(liIndex) < KU_MAX_TRAILER_TRAFFIC,
                   "Out of range trailer vehicle");
        return &maVehicles[KU_TRAILER_TRAFFIC_OFFSET + static_cast<u32>(liIndex)];
    }

    // @ 0x82707C28 -- `((luIndex + 1370) << 6) + this`; 1370*64 == 684*128 + 128, i.e. the
    // axle array starts immediately after the last (trailer) vehicle slot.
    VehicleAxles* TrafficEntityModule::GetVehicleAxles(u32 luIndex)
    {
        CGS_ASSERT(luIndex < KU_MAX_TOTAL_TRAFFIC, "luIndex < KU_MAX_TOTAL_TRAFFIC");
        return &maVehicleAxles[luIndex];
    }

    // @ 0x82707858 -- `6 * (luIndex + 42384) + this`; stride 6 == sizeof(StaticTrafficParam).
    StaticTrafficParam* TrafficEntityModule::GetStaticTrafficParam(u32 luIndex)
    {
        CGS_ASSERT(luIndex < KU_MAX_STATIC_TRAFFIC, "luIndex < KU_MAX_STATIC_TRAFFIC");
        return &maStaticTrafficParams[luIndex];
    }

    // @ 0x82707950 -- `6 * (luIndex + 41984) + this`. Same array, indexed from the FULL vehicle
    // index space: 42384 - 41984 == 400 == KU_STATIC_TRAFFIC_OFFSET, so the console is spelling
    // maStaticTrafficParams[luIndex - KU_STATIC_TRAFFIC_OFFSET] (leak :1541).
    StaticTrafficParam* TrafficEntityModule::GetStaticTrafficParamFro(u32 luIndex)
    {
        CGS_ASSERT(luIndex >= KU_STATIC_TRAFFIC_OFFSET && luIndex < KU_STATIC_TRAFFIC_OFFSET + KU_MAX_STATIC_TRAFFIC,
                   "( luIndex >= KU_STATIC_TRAFFIC_OFFSET ) && ( luIndex < KU_STATIC_TRAFFIC_OFFSET + KU_MAX_STATIC_TRAFFIC )");
        return &maStaticTrafficParams[luIndex - KU_STATIC_TRAFFIC_OFFSET];
    }

    // @ 0x827078D0 -- identical arithmetic to GetStaticTrafficParamFro (the X360 emitted both
    // out-of-line copies of the same leak-:1541 inline; the two truncated ledger names are the
    // console's own symbol truncation, not two different functions).
    StaticTrafficParam* TrafficEntityModule::GetStaticTrafficParamFromFullV(u32 luIndex)
    {
        CGS_ASSERT(luIndex >= KU_STATIC_TRAFFIC_OFFSET && luIndex < KU_STATIC_TRAFFIC_OFFSET + KU_MAX_STATIC_TRAFFIC,
                   "( luIndex >= KU_STATIC_TRAFFIC_OFFSET ) && ( luIndex < KU_STATIC_TRAFFIC_OFFSET + KU_MAX_STATIC_TRAFFIC )");
        return &maStaticTrafficParams[luIndex - KU_STATIC_TRAFFIC_OFFSET];
    }

    // @ 0x82707D18 -- map a static-vehicle index into the full vehicle index space (+400).
    u32 TrafficEntityModule::GetVehicleIndexFromStaticIndex(u32 luStaticVehicle)
    {
        CGS_ASSERT(luStaticVehicle < KU_MAX_STATIC_TRAFFIC, "luStaticVehicle < KU_MAX_STATIC_TRAFFIC");
        return luStaticVehicle + KU_STATIC_TRAFFIC_OFFSET;
    }

    // leak :1457.
    Param* TrafficEntityModule::GetParam(u32 luParam)
    {
        CGS_ASSERT(luParam < KU_MAX_PARAMS, "luParam < KU_MAX_PARAMS");
        return &maParams[luParam];
    }

    // @ 0x82707D70 -- `6 * luPlan + (luParam << 7) + 165256 + this`, with the alive-flag byte
    // read at `(luParam << 7) + 165312`. Param stride 128; 165312 - 165256 == 56 and maPlans is
    // at +0x08 inside a Param while mxFlags is at +0x40 (BrnTrafficParam.h) -- 0x40 - 0x08 == 56.
    // So the console is spelling &maParams[luParam].maPlans[luPlan], guarded by IsAlive().
    ParamPlan* TrafficEntityModule::GetParamPlan(u32 luParam, u32 luPlan)
    {
        CGS_ASSERT(luParam < KU_MAX_PARAMS, "luParam < KU_MAX_PARAMS");
        CGS_ASSERT(luPlan < KU_PARAM_NUM_PLANS, "luPlan < KU_PARAM_NUM_PLANS");
        CGS_ASSERT((maParams[luParam].mxFlags & Param::E_FLAG_ALIVE) != 0,
                   "maParams[luParam].IsAlive()");
        return &maParams[luParam].maPlans[luPlan];
    }

    const ParamPlan* TrafficEntityModule::GetParamPlan(u32 luParam, u32 luPlan) const
    {
        return const_cast<TrafficEntityModule*>(this)->GetParamPlan(luParam, luPlan);
    }

    // @ 0x827077D0 -- `16 * (luParam + 13528) + this`. Both console asserts kept: the second
    // one is the pool-warm-up guard (muLastParamCalculated is seeded past KU_MAX_PARAMS).
    ParamNeedToSlowData* TrafficEntityModule::GetParamNeedToSlowData(u32 luParam)
    {
        CGS_ASSERT(luParam < KU_MAX_PARAMS, "luParam < KU_MAX_PARAMS");
        CGS_ASSERT(muLastParamCalculated >= KU_MAX_PARAMS, "muLastParamCalculated >= KU_MAX_PARAMS");
        return &maParamNeedToSlowData[luParam];
    }

    // @ 0x82707700 (EXPORT HOLE). Shape mirrors GetParam @0x82707630: bounds assert then the
    // element. Console stride 64 from base +226048 == &maParamTransforms[0].
    ParamTransform* TrafficEntityModule::GetParamTransform(u32 luParam)
    {
        CGS_ASSERT(luParam < KU_MAX_PARAMS, "luParam < KU_MAX_PARAMS");
        return &maParamTransforms[luParam];
    }

    // DWARF BrnTrafficUnity.cpp:15022. No standalone ARTIST symbol: every caller inlines the
    // bounds assert plus the `8 * (luParam + 27856) + this` index (== &maParamListNodes[luParam]).
    ParamListNode* TrafficEntityModule::GetParamListNode(u32 luParam)
    {
        CGS_ASSERT(luParam < KU_MAX_PARAMS, "luParam < KU_MAX_PARAMS");
        return &maParamListNodes[luParam];
    }

    // DWARF :1602 (BrnTrafficUnity.cpp:18527, .cpp 9146). Every X360 caller inlines it to the
    // list node's muPrevParam.
    u32 TrafficEntityModule::GetParamBehind(u32 luParam) const
    {
        CGS_ASSERT(luParam < KU_MAX_PARAMS, "luParam < KU_MAX_PARAMS");
        return maParamListNodes[luParam].muPrevParam;
    }

    // @ 0x82707C90 (EXPORT HOLE). DWARF :2558 returns Matrix44Affine BY VALUE; every call site
    // passes an sret buffer. SetVehicleTransform @0x827142B8 is the writer.
    Matrix44Affine TrafficEntityModule::GetVehicleTransform(u32 luIndex) const
    {
        CGS_ASSERT(luIndex < KU_MAX_TOTAL_TRAFFIC, "luIndex < KU_MAX_TOTAL_TRAFFIC");
        return maVehicleTransforms[luIndex];
    }

    // leak :1655 -- the per-vehicle-type runtime record (bbox, axle offsets, paint colours).
    // X360-attested: Prepare @0x8274A578 stage 3 walks the array from
    // `this + 0x76390` == &maVehicleTypeRuntime[0].mBBoxHalfSize at a 128-byte stride.
    // FLAG: the console bounds it by mpData->muNumVehicleTypes. mpData's interior belongs to
    // TrafficData (SharedClasses/Traffic) and is not reached here, so this assert is the
    // static capacity bound. Tighten it to the data bound when LoadData's ladder is bodied.
    const VehicleTypeRuntime* TrafficEntityModule::GetVehicleTypeRuntime(u32 luVehicleType) const
    {
        CGS_ASSERT(luVehicleType < KU_MAX_VEHICLE_TYPES, "Out-of-range vehicle type");
        return &maVehicleTypeRuntime[luVehicleType];
    }

    // =========================================================================================
    // State / flag book-keeping.
    // =========================================================================================

    // @ 0x82707560 -- meState @ +0x300 must be E_STATE_RUNNING; returns
    // meRunningState @ +0x308 == E_RUNNINGSTATE_PAUSED (1).
    bool TrafficEntityModule::IsPaused()
    {
        CGS_ASSERT(meState == E_STATE_RUNNING, "meState == E_STATE_RUNNING");
        return meRunningState == E_RUNNINGSTATE_PAUSED;
    }

    // @ 0x827075C8 --
    //   lfsx f13, r3, 0x7237C  ; if ( *(this + 467836) <= 0.0099999998 ) -> false
    //   lbzx r11, r3, 0x717E7  ; ...else gated on the bool at +464871
    //   lbzx r10, r3, 0x7286A  ; if ( *(this + 469098) ) return true
    //
    // The three offsets resolve as:
    //   * 0x717E7 == mbAllowDivergentBehaviour (:726), last of the eleven consecutive bools
    //     starting at mbPlayingShowtimeMode @0x717DD.
    //   * 0x7237C == mfCrashSliderFinalValue (:766), anchored by UpdateCrashSlider @0x82715A18:
    //     it writes :763/:764/:765 at +467824/+467828/+467832 (100.0 / 0.0 / 10.0), then makes
    //     +467836 its final store, the 0..1 normalisation of +467824. The showtime timers
    //     :772/:773/:775 sit 308 bytes higher at 468144/468148/468156, so this is not a timer.
    //   * 0x7286A == mbDEBUGTestSympCrash (:858), anchored by Prepare @0x8274A578 storing
    //     mpaDEBUGVehicleFuzzyLogic at +469100 and its count at +469104, which puts :865/:866 at
    //     469108/469109 (the byte NeedToTakeActionAgainstJunctionFUP @0x82707FD0 reads). That
    //     places the four-bool run :855/:856/:858/:860 at 469096..469099, with
    //     CalculateAndSetSteeringUsingAvoidance @0x8273D258 reading 0x72869 as its middle.
    bool TrafficEntityModule::ShouldBeHollywoodAction()
    {
        bool lbShowtimeActionActive = true;
        if (mfCrashSliderFinalValue <= 0.0099999998f || !mbAllowDivergentBehaviour)
            lbShowtimeActionActive = false;

        if (mbDEBUGTestSympCrash)
            return true;
        return lbShowtimeActionActive;
    }

    // @ 0x82707FD0 -- forced flag @ +0x72875 (mbDEBUGOverrideJunctionFUP, :866); else if
    // mbAllowDivergentBehaviour @ +0x717E7 is set, true when the junction-FUP score
    // @ +0x725E0 (mfJunctionFUP, :785) >= 65.0 (flt_820BA290).
    bool TrafficEntityModule::NeedToTakeActionAgainstJunctionFUP()
    {
        if (mbDEBUGOverrideJunctionFUP)
            return true;
        if (mbAllowDivergentBehaviour)
            return mfJunctionFUP >= 65.0f;
        return false;
    }

    // @ 0x827081D8 -- on entering replay, clear the byte at +0x72521 and, when
    // CgsDev::Message::gxMessageFilterFlags & 1, print.
    //
    // PARKED: +0x72521 has no name. It is not mbInReplay (:757, pinned at +465714 by Prepare
    // @0x8274A578 stage 0), and both replay hooks store ZERO there, which no in/out flag would
    // do. It lands in the :776/:777 gap the DecFIGS dwarfdump skips, so naming it would be
    // fabrication. Recover those two members from the X360 Construct @0x82740220 field-init
    // walk, then restore the store. DELETE THIS WHEN the member is named.
    void TrafficEntityModule::EnterReplay()
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            (*CgsDev::Log::gpDebugPrint) << "TRAF: Enter Replay\n";
        // PARKED STORE: `*(this + 0x72521) = 0` -- member unnamed, see the FLAG above.
    }

    // @ 0x82708248 -- symmetric to EnterReplay (same parked store).
    void TrafficEntityModule::LeaveReplay()
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            (*CgsDev::Log::gpDebugPrint) << "TRAF: Leave Replay\n";
        // PARKED STORE: `*(this + 0x72521) = 0` -- member unnamed, see EnterReplay's FLAG.
    }

    // @ 0x82708F98 --
    //   lwz   r9, 0x300(r31)          ; meState, read BEFORE the two byte stores
    //   stbx  r11, r31, 0x725E9       ; *(this + 468457) = 1
    //   stbx  r11, r31, 0x725EA       ; *(this + 468458) = 1
    //   if ( r9 == 1 ) EnterTearingDownState();
    //   stw   0, 0x30C(r31)
    //
    // +0x30C is meRunningStateToUseAfterStartup (:610), not meTearingDownState (:611, +0x310):
    // a restart request says "when start-up finishes, come back up NORMAL".
    //
    // The two byte stores land in a run that closes with zero slack:
    //   468448 mfJunctionFUP (:785)                        -- read @0x82707FD0
    //   468452 mfJunctionFUP_TimeTillNextPhysicalKill (:786) -- decremented @0x82745218
    //   468456 mbTrafficIsHidden (:789)                    -- UnhideAllTraffic @0x8274A500
    //   468457 mbDontCreateVehiclesNearAnyPlayers (:791)
    //   468458 mbDontCreateStaticVehiclesNearAnyPlayers (:792)
    // The consumers name themselves: UpdateVehicles_CreateNewVehicles @0x8273A308 reads 468457
    // before spawning a moving vehicle, StaticVehicles_CreateNewVehicles @0x827229F0 reads
    // 468458 before spawning a parked one. Dropping either store lets the restart path spawn
    // traffic on top of the player.
    void TrafficEntityModule::RestartTraffic()
    {
        const EState leState = meState;
        mbDontCreateVehiclesNearAnyPlayers       = true;
        mbDontCreateStaticVehiclesNearAnyPlayers = true;
        if (leState == E_STATE_RUNNING)
            EnterTearingDownState();
        meRunningStateToUseAfterStartup = E_RUNNINGSTATE_NORMAL;
    }

    // _AssertLayout pins relative member order and asm-attested array counts, never absolute
    // byte offsets: the console displacements quoted above are 32-bit-pointer values and mean
    // nothing on this target. It is a static member so offsetof reaches the private members.
    void TrafficEntityModule::_AssertLayout()
    {
        // The state block, ten consecutive DWARF members (:602-:613) on ten consecutive X360
        // offsets 0x2F0..0x314. The Prepare/LoadData ladders and the state machine need it.
        static_assert(offsetof(TrafficEntityModule, mePrepareStage)
                      <  offsetof(TrafficEntityModule, meReleaseStage), "mePrepareStage before meReleaseStage");
        static_assert(offsetof(TrafficEntityModule, meReleaseStage)
                      <  offsetof(TrafficEntityModule, meResourceStage), "meReleaseStage before meResourceStage");
        static_assert(offsetof(TrafficEntityModule, meResourceStage)
                      <  offsetof(TrafficEntityModule, meEmptyTrafficPoolState), "meResourceStage before meEmptyTrafficPoolState");
        static_assert(offsetof(TrafficEntityModule, meEmptyTrafficPoolState)
                      <  offsetof(TrafficEntityModule, meState), "meEmptyTrafficPoolState before meState");
        static_assert(offsetof(TrafficEntityModule, meState)
                      <  offsetof(TrafficEntityModule, meStartingUpState), "meState before meStartingUpState");
        static_assert(offsetof(TrafficEntityModule, meStartingUpState)
                      <  offsetof(TrafficEntityModule, meRunningState), "meStartingUpState before meRunningState");
        static_assert(offsetof(TrafficEntityModule, meRunningState)
                      <  offsetof(TrafficEntityModule, meRunningStateToUseAfterStartup), "meRunningState before meRunningStateToUseAfterStartup");
        static_assert(offsetof(TrafficEntityModule, meRunningStateToUseAfterStartup)
                      <  offsetof(TrafficEntityModule, meTearingDownState), "meRunningStateToUseAfterStartup before meTearingDownState");
        static_assert(offsetof(TrafficEntityModule, meTearingDownState)
                      <  offsetof(TrafficEntityModule, mReceiverQueue), "meTearingDownState before mReceiverQueue");

        // The three vehicle pools are one array, in the order the console's base element
        // indices (85 / 485 / 684) spell, with the axle array after it.
        static_assert(offsetof(TrafficEntityModule, maVehicles)
                      <  offsetof(TrafficEntityModule, maVehicleAxles), "maVehicles before maVehicleAxles");
        static_assert(offsetof(TrafficEntityModule, maVehicleAxles)
                      <  offsetof(TrafficEntityModule, maVehicleTransforms), "maVehicleAxles before maVehicleTransforms");
        static_assert(offsetof(TrafficEntityModule, maVehicleTransforms)
                      <  offsetof(TrafficEntityModule, maParams), "maVehicleTransforms before maParams");
        static_assert(offsetof(TrafficEntityModule, maParams)
                      <  offsetof(TrafficEntityModule, maParamTransforms), "maParams before maParamTransforms");

        // The tail run the Prepare stages walk, in DWARF order (:938 -> :944).
        static_assert(offsetof(TrafficEntityModule, mStreamer)
                      <  offsetof(TrafficEntityModule, maVehicleTypeRuntime), "mStreamer before maVehicleTypeRuntime");
        static_assert(offsetof(TrafficEntityModule, maVehicleTypeRuntime)
                      <  offsetof(TrafficEntityModule, miResourceRequestCount), "maVehicleTypeRuntime before miResourceRequestCount");
        static_assert(offsetof(TrafficEntityModule, miResourceRequestCount)
                      <  offsetof(TrafficEntityModule, mpVehicleList), "miResourceRequestCount before mpVehicleList");
        static_assert(offsetof(TrafficEntityModule, mpVehicleList)
                      <  offsetof(TrafficEntityModule, maStoredAITrafficData), "mpVehicleList before maStoredAITrafficData");

        // Asm-attested array counts (pool capacities; see BrnTrafficConstants.h).
        static_assert(sizeof(TrafficEntityModule::maVehicles) / sizeof(Vehicle) == 600,
                      "maVehicles holds KU_MAX_TOTAL_TRAFFIC == 600 (asm 0x258)");
        static_assert(sizeof(TrafficEntityModule::maVehicleAxles) / sizeof(VehicleAxles) == 600,
                      "maVehicleAxles is one per vehicle slot");
        static_assert(sizeof(TrafficEntityModule::maStaticTrafficParams) / sizeof(StaticTrafficParam) == 199,
                      "maStaticTrafficParams holds KU_MAX_STATIC_TRAFFIC == 199 (asm 0xC7)");
        static_assert(sizeof(TrafficEntityModule::maParams) / sizeof(Param) == 400,
                      "maParams holds KU_MAX_PARAMS == 400");
        static_assert(sizeof(TrafficEntityModule::maVehicleTypeRuntime) / sizeof(VehicleTypeRuntime) == 96,
                      "maVehicleTypeRuntime holds KU_MAX_VEHICLE_TYPES == 96");
        static_assert(sizeof(TrafficEntityModule::maStoredAITrafficData) / sizeof(StoredAITrafficData) == 8,
                      "maStoredAITrafficData is one per active race car (Prepare stage 4 walks 8)");
        static_assert(sizeof(TrafficEntityModule::maGenerators) / sizeof(GeneratorAddress) == 512,
                      "maGenerators holds KU_MAX_GENERATORS == 512");

        // Pointer-free record sizes, pinned by the console's container strides. These are the
        // only absolute sizes pinned here: no record holds a pointer, so the console and host
        // byte sizes are the same number for the same reason.
        static_assert(sizeof(PurgatoryInfo) == 4,
                      "PurgatoryInfo == 4 (Array<PurgatoryInfo,N> count word at N*4)");
        static_assert(sizeof(VehicleRenderInfo) == 12,
                      "VehicleRenderInfo == 12 (Array<VehicleRenderInfo,64> count word at 0x300)");
        static_assert(sizeof(HullChangeInfo) == 8,
                      "HullChangeInfo == 8 (Array<HullChangeInfo,400> count word at 0xC80)");
        static_assert(sizeof(TrafficCrashInfo) == 16,
                      "TrafficCrashInfo == 16 (Array<TrafficCrashInfo,160> count word at 0xA00)");
        static_assert(sizeof(FiredKillZoneInfo) == 16,
                      "FiredKillZoneInfo == 16 (Array<FiredKillZoneInfo,8> count word at 0x80)");
        static_assert(sizeof(CollidableVehicleInfo4) == 128,
                      "CollidableVehicleInfo4 == 128 (eight Vector4 lanes; operator[] shifts by 7)");
        static_assert(sizeof(CrashingThingData) == 32,
                      "CrashingThingData == 32 (Array<CrashingThingData,168> operator[] shifts by 5)");
        static_assert(sizeof(StoredAITrafficData) == 136,
                      "StoredAITrafficData == 136 (Prepare stage 4 stride)");
        static_assert(sizeof(DEBUG_VehicleFuzzyLogic) == 64,
                      "DEBUG_VehicleFuzzyLogic == 64 (Prepare stage 4 allocates 40 * 64 == 2560)");
    }
}
