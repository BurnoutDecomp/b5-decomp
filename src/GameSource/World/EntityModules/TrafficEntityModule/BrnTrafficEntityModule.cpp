// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp
//
// KEYSTONE class TU: BrnTraffic::TrafficEntityModule -- the traffic entity module (a
// CgsModule::ModuleSingleBuffered subclass that owns + ticks the traffic-vehicle fleet
// through the scene-update interface). FOUNDATION slice.
//
// ⭐ 2026-08-21 (wave T1, cluster C1) -- THE RAW-OFFSET ACCESS IS GONE.
//   Every body below used to poke `this` at a literal X360 BYTE offset through a local
//   `FieldAt<T>(this, N)` helper, because the class was modelled as `u8 mOpaque[0x73000]`.
//   The header now carries the REAL ordered member list (see its banner for the DWARF +
//   X360 attestation), so each body reads the member it always meant. The console's
//   offset/element arithmetic is retained as the per-function attestation comment -- that
//   is what proves the member each line resolves to.
//
//   Nothing here strides or offsets by an X360 byte value any more, which was the point:
//   host pointers are 8 bytes wide and the console displacements were never valid here.
//
// ONLY the functions in this TU's postmortem dossier are bodied. No constant, body, or
// rodata is invented. The large remainder (constructor, the VMX update/spawn/avoidance
// pipelines, the methods reaching still-partial sub-aggregate interiors, and the one
// helper whose floor constant flt_82001CC0 the dossier dropped) is declaration-only +
// FLAGGED in the header.
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h" // CgsDev::Log::gpDebugPrint / CgsDev::Message::gxMessageFilterFlags

#include <cstddef>   // offsetof / size_t

namespace BrnTraffic
{
    // =========================================================================================
    // Vehicle / param pool address accessors.
    //
    // X360 SHAPE: the three vehicle "pools" are ONE array (maVehicles) addressed with three
    // different base element indices at a 128-byte stride -- 85 / 485 / 684. Those bases differ
    // by exactly 400 and 199, which is what pins KU_MAX_STANDARD_TRAFFIC / KU_MAX_STATIC_TRAFFIC
    // (BrnTrafficConstants.h carries the full derivation). Here they are the named pool slices
    // the console arithmetic describes; the element indices are host-irrelevant.
    // =========================================================================================

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

    // leak :1655 -- the per-vehicle-TYPE runtime record (bbox + axle offsets + paint colours).
    // X360-ATTESTED ARRAY: Prepare @0x8274A578 stage 3 walks this array from
    // `this + 0x76390` == &maVehicleTypeRuntime[0].mBBoxHalfSize at a 128-byte stride, bounded
    // by mpData->muNumVehicleTypes -- which is the same bound this accessor asserts.
    //
    // FLAG: the console asserts against `mpData->muNumVehicleTypes`; mpData's interior is
    // TrafficData's (SharedClasses/Traffic), and this cluster does not reach into it, so the
    // assert here is the static capacity bound only. C4/C5 tighten it to the data bound when
    // they body the LoadData ladder that fills the array.
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
    //   * 464871 == 0x717E7 == mbAllowDivergentBehaviour (:726 -- the last of the eleven
    //     consecutive bools that start at mbPlayingShowtimeMode @0x717DD, the offset wQ7_01's
    //     SetPlayingShowtime already uses).
    //   * 467836 == 0x7237C == mfCrashSliderFinalValue (:766). ANCHORED, not ordered-guessed:
    //     UpdateCrashSlider @0x82715A18 owns the whole crash-slider block and writes
    //     +467824/+467828/+467832 with 100.0 / 0.0 / 10.0 (:763/:764/:765), then makes
    //     +467836 its FINAL store -- the 0..1 normalisation of *(467824) via
    //     `1.0 - ((score - 10.0) * 0.011111111)` with two fsel clamps. The same function pins
    //     the showtime timers 308 bytes HIGHER: `*(this+468144) += *(this+463868)` (mfSimTimeStep)
    //     compared against `*(this+468148)`, with `*(this+468156) >= 3.0` -- i.e. :772/:773/:775
    //     are at 468144/468148/468156, so mfShowtimeTimer is NOT this offset.
    //     Corroboration: UpdateParams_PrecalcBehaviourParams @0x82717C48 computes
    //     `1.0 - *(this + 467836)`, which is slider arithmetic, not timer arithmetic.
    //   * 469098 == 0x7286A == mbDEBUGTestSympCrash (:858). ANCHORED: Prepare @0x8274A578
    //     stores the 2560-byte DEBUG_VehicleFuzzyLogic allocation to +469100
    //     (mpaDEBUGVehicleFuzzyLogic, :862) and 0 to +469104 (muDEBUGVehicleFuzzyLogicCount,
    //     :863), which puts :865/:866 at 469108/469109 -- and 469109 is exactly the byte
    //     NeedToTakeActionAgainstJunctionFUP @0x82707FD0 (and UpdateJunctionFUP @0x82745218)
    //     reads as mbDEBUGOverrideJunctionFUP. 469098 therefore sits ten bytes lower, in the
    //     four-bool run :855/:856/:858/:860 at 469096..469099, whose middle is pinned by
    //     CalculateAndSetSteeringUsingAvoidance @0x8273D258 reading 0x72869 == 469097 ==
    //     mbDEBUGEnableAvoidance. Semantics agree: 469098 is also read by
    //     UpdateParams_BuildListOfCrashingThings @0x82737270 (the sympathetic-crash list
    //     builder) and by UpdateParams_PrecalcBehaviourParams (the sympathetic-crash cone).
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

    // @ 0x827081D8 -- on entering replay, clear the byte at +468257 (0x72521 -- `lis r11,7;
    // ori r11,r11,0x2521; stbx r10,r31,r11`, r10 == 0). The debug-print branch is gated on
    // CgsDev::Message::gxMessageFilterFlags & 1.
    //
    // FLAG (mbInReplay is NOT that byte -- mbInReplay is :757, pinned at +465714 by Prepare
    // @0x8274A578 stage 0's `stbx r31,r26,0x71B32`, and BOTH replay hooks store ZERO here, which
    // no in/out replay flag would do): 0x72521 lands between the showtime timers (:775) and
    // mfPlayerIdleTime (:778) -- i.e. in the :776/:777 gap the DecFIGS DWARF does not emit (the
    // dwarfdump jumps straight from :775 to :778, so those two members exist in the header but
    // not in that build's type). Naming it would be fabrication, so both replay hooks are
    // reduced to their observable, name-free half (the debug print) and the store is PARKED.
    // Recover the two missing members from the X360 Construct @0x82740220 field-init walk (C4)
    // and restore the store then.
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
    // ⭐ CORRECTED 2026-08-21: +780 is 0x30C == meRunningStateToUseAfterStartup (:610), NOT
    // meTearingDownState (:611, +0x310). The previous body mislabelled it; both enum zeroes are
    // 0 so the bug was invisible on the blob. Semantically the corrected reading is the right
    // one: a restart request says "when start-up finishes, come back up NORMAL".
    //
    // ⭐ CORRECTED 2026-08-21 (C1 fix round): the two byte stores are NOT unnameable and are no
    // longer parked. The earlier note mis-converted the displacements (it wrote 0x72609/0x7260A
    // for 468457/468458, which are really 0x725E9/0x725EA -- read straight off the `ori` pair in
    // the asm above) and so landed them in EnterReplay's un-emitted-DWARF window. They are
    // mbDontCreateVehiclesNearAnyPlayers (:791) and mbDontCreateStaticVehiclesNearAnyPlayers
    // (:792), both modelled in this header. The run closes with zero slack:
    //   468448 mfJunctionFUP (:785)                  -- NeedToTakeActionAgainstJunctionFUP @0x82707FD0
    //   468452 mfJunctionFUP_TimeTillNextPhysicalKill(:786) -- UpdateJunctionFUP @0x82745218 does
    //                                                  `*(this+468452) -= *(this+463868)`
    //   468456 mbTrafficIsHidden (:789)              -- UnhideAllTraffic @0x8274A500 tests then
    //                                                  clears it; RenderTrafficCar @0x82728B08
    //                                                  and UpdateCollidableVehicles @0x827302C8 read it
    //   468457 mbDontCreateVehiclesNearAnyPlayers       (:791)
    //   468458 mbDontCreateStaticVehiclesNearAnyPlayers (:792)
    // and the consumers name themselves: UpdateVehicles_CreateNewVehicles @0x8273A308 reads
    // 468457 before spawning a moving vehicle, StaticVehicles_CreateNewVehicles @0x827229F0
    // reads 468458 before spawning a parked one. Dropping the stores would let the restart path
    // spawn traffic on top of the player, so they are restored by name.
    void TrafficEntityModule::RestartTraffic()
    {
        const EState leState = meState;
        mbDontCreateVehiclesNearAnyPlayers       = true;
        mbDontCreateStaticVehiclesNearAnyPlayers = true;
        if (leState == E_STATE_RUNNING)
            EnterTearingDownState();
        meRunningStateToUseAfterStartup = E_RUNNINGSTATE_NORMAL;
    }

    // -----------------------------------------------------------------------------------------
    // _AssertLayout -- HOST-NATIVE pins only.
    //
    // Per the wave-T1 rule these pin RELATIVE MEMBER ORDER and asm-attested ARRAY COUNTS. They
    // deliberately do NOT pin absolute byte offsets: the console displacements quoted in the
    // comments above are 32-bit-pointer values and are meaningless on this target. A static
    // member function is used so offsetof reaches the private members.
    // -----------------------------------------------------------------------------------------
    void TrafficEntityModule::_AssertLayout()
    {
        // --- the state block: ten consecutive DWARF members (:602-:613) that the X360 places on
        // ten consecutive offsets 0x2F0..0x314. Order is what the wQ7 Prepare/LoadData ladders
        // and the state machine depend on.
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

        // --- the three vehicle pools are ONE array, in the order the console's base element
        // indices (85 / 485 / 684) spell, and the axle array follows it.
        static_assert(offsetof(TrafficEntityModule, maVehicles)
                      <  offsetof(TrafficEntityModule, maVehicleAxles), "maVehicles before maVehicleAxles");
        static_assert(offsetof(TrafficEntityModule, maVehicleAxles)
                      <  offsetof(TrafficEntityModule, maVehicleTransforms), "maVehicleAxles before maVehicleTransforms");
        static_assert(offsetof(TrafficEntityModule, maVehicleTransforms)
                      <  offsetof(TrafficEntityModule, maParams), "maVehicleTransforms before maParams");
        static_assert(offsetof(TrafficEntityModule, maParams)
                      <  offsetof(TrafficEntityModule, maParamTransforms), "maParams before maParamTransforms");

        // --- the tail run the Prepare stages walk, in DWARF order (:938 -> :944).
        static_assert(offsetof(TrafficEntityModule, mStreamer)
                      <  offsetof(TrafficEntityModule, maVehicleTypeRuntime), "mStreamer before maVehicleTypeRuntime");
        static_assert(offsetof(TrafficEntityModule, maVehicleTypeRuntime)
                      <  offsetof(TrafficEntityModule, miResourceRequestCount), "maVehicleTypeRuntime before miResourceRequestCount");
        static_assert(offsetof(TrafficEntityModule, miResourceRequestCount)
                      <  offsetof(TrafficEntityModule, mpVehicleList), "miResourceRequestCount before mpVehicleList");
        static_assert(offsetof(TrafficEntityModule, mpVehicleList)
                      <  offsetof(TrafficEntityModule, maStoredAITrafficData), "mpVehicleList before maStoredAITrafficData");

        // --- asm-attested ARRAY COUNTS (pool capacities; see BrnTrafficConstants.h) ---
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

        // --- pointer-free record sizes the console's container strides pin. These are the ONLY
        // absolute sizes pinned here: each record holds no pointer, so the console byte size and
        // the host byte size are the same number for the same reason.
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
