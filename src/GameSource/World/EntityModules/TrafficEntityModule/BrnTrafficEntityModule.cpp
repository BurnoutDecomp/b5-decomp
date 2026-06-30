// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp
//
// KEYSTONE class TU: BrnTraffic::TrafficEntityModule -- the traffic entity module (a
// CgsEntityModule subclass that owns + ticks the traffic-vehicle fleet through the
// scene-update interface). FOUNDATION slice.
//
// LAYOUT IS DWARF-OPAQUE (see BrnTrafficEntityModule.h header comment): the X360 DWARF emits no
// member-field layout for this ~470KB module object, only its nested enums. Every method reaches
// the object through raw, asm-attested BYTE offsets. The recoverable methods below are therefore
// bodied as asm-attested byte-offset arithmetic over `this` (the "opaque external field by
// attested offset" allowance) -- NOT as named members (which would require fabricating the layout).
//
// ONLY the functions in this TU's postmortem dossier are bodied. The byte offsets and the small
// number of decoded float/integer literals used below are taken verbatim from the X360 ARTIST
// pseudocode + assembly (the source-of-truth spine); no constant, body, or rodata is invented.
// The large remainder (constructor, the VMX update/spawn/avoidance pipelines, the methods reaching
// un-homed sub-aggregate / BrnTraffic::Vehicle interiors, and the one helper whose floor constant
// flt_82001CC0 the dossier dropped) is declaration-only + FLAGGED in the header.
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h" // CgsDev::Log::gpDebugPrint / CgsDev::Message::gxMessageFilterFlags

#include <cstddef>   // offsetof / size_t

namespace BrnTraffic
{
    // -----------------------------------------------------------------------------------------
    // Local helpers: address a byte offset / read a typed field at an asm-attested byte offset
    // into the opaque module object. These centralise the raw-offset access the X360 methods do;
    // the offsets are the literal byte displacements the assembly uses (documented per call).
    // -----------------------------------------------------------------------------------------
    namespace
    {
        inline void* ByteAddr(void* lpBase, size_t luByteOffset)
        {
            return reinterpret_cast<u8*>(lpBase) + luByteOffset;
        }

        template <typename T>
        inline T& FieldAt(void* lpBase, size_t luByteOffset)
        {
            return *reinterpret_cast<T*>(reinterpret_cast<u8*>(lpBase) + luByteOffset);
        }
    }

    // =========================================================================================
    // Vehicle / param pool address accessors. Each returns a byte address into the module object;
    // callers (other slices) reinterpret the result as the (un-homed) BrnTraffic::Vehicle / Param /
    // StaticParam interior. The displacement arithmetic is exactly the assembly's.
    // =========================================================================================

    // @ 0x82707A38 -- maStandard pool: element stride 0x80 (128), base element index 85.
    void* TrafficEntityModule::GetStandardVehicle(u32 luIndex)
    {
        CGS_ASSERT(luIndex < KU_MAX_STANDARD_TRAFFIC, "luIndex < KU_MAX_STANDARD_TRAFFIC");
        return ByteAddr(this, static_cast<size_t>(luIndex + 85) << 7);
    }

    // @ 0x827079D0 -- maStatic pool: element stride 0x80 (128), base element index 485.
    void* TrafficEntityModule::GetStaticVehicle(u32 luIndex)
    {
        CGS_ASSERT(luIndex < KU_MAX_STATIC_TRAFFIC, "luIndex < KU_MAX_STATIC_TRAFFIC");
        return ByteAddr(this, static_cast<size_t>(luIndex + 485) << 7);
    }

    // @ 0x82707AA0 -- trailer pool: element stride 0x80 (128), base element index 684.
    // The asm asserts liIndex == 0 (single trailer slot) via the streamed "Out of range trailer
    // vehicle" message; modelled as the in-range check.
    void* TrafficEntityModule::GetTrailerVehicle(s32 liIndex)
    {
        CGS_ASSERT(liIndex == 0, "Out of range trailer vehicle");
        return ByteAddr(this, static_cast<size_t>(liIndex + 684) << 7);
    }

    // @ 0x82707C28 -- per-vehicle axle block: element stride 0x40 (64), base element index 1370.
    void* TrafficEntityModule::GetVehicleAxles(u32 luIndex)
    {
        CGS_ASSERT(luIndex < KU_MAX_TOTAL_TRAFFIC, "luIndex < KU_MAX_TOTAL_TRAFFIC");
        return ByteAddr(this, static_cast<size_t>(luIndex + 1370) << 6);
    }

    // @ 0x82707858 -- static-traffic param: element stride 6, base element index 42384.
    void* TrafficEntityModule::GetStaticTrafficParam(u32 luIndex)
    {
        CGS_ASSERT(luIndex < KU_MAX_STATIC_TRAFFIC, "luIndex < KU_MAX_STATIC_TRAFFIC");
        return ByteAddr(this, static_cast<size_t>(6 * (luIndex + 42384)));
    }

    // @ 0x82707950 -- static-traffic param (full-vehicle index space): stride 6, base 41984.
    void* TrafficEntityModule::GetStaticTrafficParamFro(u32 luIndex)
    {
        CGS_ASSERT(luIndex >= KU_STATIC_TRAFFIC_OFFSET && luIndex < KU_STATIC_TRAFFIC_OFFSET + KU_MAX_STATIC_TRAFFIC,
                   "( luIndex >= KU_STATIC_TRAFFIC_OFFSET ) && ( luIndex < KU_STATIC_TRAFFIC_OFFSET + KU_MAX_STATIC_TRAFFIC )");
        return ByteAddr(this, static_cast<size_t>(6 * (luIndex + 41984)));
    }

    // @ 0x827078D0 -- identical arithmetic to GetStaticTrafficParamFro (the X360 emitted both).
    void* TrafficEntityModule::GetStaticTrafficParamFromFullV(u32 luIndex)
    {
        CGS_ASSERT(luIndex >= KU_STATIC_TRAFFIC_OFFSET && luIndex < KU_STATIC_TRAFFIC_OFFSET + KU_MAX_STATIC_TRAFFIC,
                   "( luIndex >= KU_STATIC_TRAFFIC_OFFSET ) && ( luIndex < KU_STATIC_TRAFFIC_OFFSET + KU_MAX_STATIC_TRAFFIC )");
        return ByteAddr(this, static_cast<size_t>(6 * (luIndex + 41984)));
    }

    // @ 0x82707D18 -- map a static-vehicle index into the full vehicle index space (+400).
    u32 TrafficEntityModule::GetVehicleIndexFromStaticIndex(u32 luStaticVehicle)
    {
        CGS_ASSERT(luStaticVehicle < KU_MAX_STATIC_TRAFFIC, "luStaticVehicle < KU_MAX_STATIC_TRAFFIC");
        return luStaticVehicle + 400;
    }

    // @ 0x827077D0 -- per-param "need to slow" record: element stride 0x10 (16), base index 13528.
    void* TrafficEntityModule::GetParamNeedToSlowData(u32 luParam)
    {
        CGS_ASSERT(luParam < KU_MAX_PARAMS, "luParam < KU_MAX_PARAMS");
        // The X360 also asserts muLastParamCalculated >= KU_MAX_PARAMS here (an internal-state
        // invariant on an un-homed field); that field is opaque in this slice, so only the
        // index-range invariant is reproduced. The address arithmetic is exact.
        return ByteAddr(this, static_cast<size_t>(16 * (luParam + 13528)));
    }

    // @ 0x82707D70 -- maParams[luParam].plan[luPlan]: param block at (luParam<<7)+165256, the
    // alive-flag at +165312 within that block (offset 56 from the plan base), plan stride 6.
    void* TrafficEntityModule::GetParamPlan(u32 luParam, u32 luPlan)
    {
        CGS_ASSERT(luParam < KU_MAX_PARAMS, "luParam < KU_MAX_PARAMS");
        CGS_ASSERT(luPlan < KU_PARAM_NUM_PLANS, "luPlan < KU_PARAM_NUM_PLANS");
        const size_t luParamByte = static_cast<size_t>(luParam) << 7;
        const u8 luAliveFlag = FieldAt<u8>(this, luParamByte + 165312);
        CGS_ASSERT((luAliveFlag & 1) != 0, "maParams[luParam].IsAlive()");
        return ByteAddr(this, static_cast<size_t>(6 * luPlan) + luParamByte + 165256);
    }

    // =========================================================================================
    // State / flag book-keeping.
    // =========================================================================================

    // @ 0x82707560 -- meState @ +0x300 (768) must be E_STATE_RUNNING; returns
    // meRunningState @ +0x308 (776) == E_RUNNINGSTATE_PAUSED (1).
    bool TrafficEntityModule::IsPaused()
    {
        CGS_ASSERT(FieldAt<s32>(this, 768) == E_STATE_RUNNING, "meState == E_STATE_RUNNING");
        return FieldAt<s32>(this, 776) == E_RUNNINGSTATE_PAUSED;
    }

    // @ 0x827075C8 -- returns true if a forced-action flag @ +0x7286A is set, or if the slow-motion
    // timer @ +0x7237C (f32) is <= 0.0099999998 AND the in-hollywood flag @ +0x717E7 is set.
    bool TrafficEntityModule::ShouldBeHollywoodAction()
    {
        // v1 := (timer @ +0x7237C > 0.0099999998) AND (in-hollywood flag @ +0x717E7 set).
        // (The X360 forms this as: if (timer <= 0.0099 || !flag) v1 = 0; else v1 = 1.)
        bool lbHollywoodTimerExpired = true;
        if (FieldAt<f32>(this, 0x7237C) <= 0.0099999998f || !FieldAt<u8>(this, 0x717E7))
            lbHollywoodTimerExpired = false;

        if (FieldAt<u8>(this, 0x7286A))
            return true;
        if (lbHollywoodTimerExpired)
            return true;
        return false;
    }

    // @ 0x82707FD0 -- forced flag @ +0x72875; else if active flag @ +0x717E7 is set, true when the
    // junction-FUP distance @ +0x725E0 (f32) >= 65.0 (flt_820BA290).
    bool TrafficEntityModule::NeedToTakeActionAgainstJunctionFUP()
    {
        if (FieldAt<u8>(this, 0x72875))
            return true;
        if (FieldAt<u8>(this, 0x717E7))
            return FieldAt<f32>(this, 0x725E0) >= 65.0f;
        return false;
    }

    // @ 0x827081D8 -- on entering replay, clear the per-frame "advanced this frame" flag @ +468257
    // (0x72561). The debug-print branch is gated on CgsDev::Message::gxMessageFilterFlags & 1.
    void TrafficEntityModule::EnterReplay()
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            (*CgsDev::Log::gpDebugPrint) << "TRAF: Enter Replay\n";
        FieldAt<u8>(this, 468257) = 0;
    }

    // @ 0x82708248 -- symmetric to EnterReplay.
    void TrafficEntityModule::LeaveReplay()
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            (*CgsDev::Log::gpDebugPrint) << "TRAF: Leave Replay\n";
        FieldAt<u8>(this, 468257) = 0;
    }

    // @ 0x82708F98 -- request a restart: set the two restart flags @ +468457/+468458, and if the
    // module is currently RUNNING (+0x300) enter the tearing-down state; clear meTearingDownState
    // @ +780.
    void TrafficEntityModule::RestartTraffic()
    {
        const s32 liState = FieldAt<s32>(this, 768);
        FieldAt<u8>(this, 468457) = 1;
        FieldAt<u8>(this, 468458) = 1;
        if (liState == E_STATE_RUNNING)
            EnterTearingDownState();
        FieldAt<s32>(this, 780) = E_TEARINGDOWNSTATE_WIPING;
    }

    // -----------------------------------------------------------------------------------------
    // _AssertLayout: pins the asm-attested STATE offsets the bodied book-keeping methods rely on.
    // (The module object has no DWARF member layout, so these are offset constants, not offsetof
    // of named members; they are checked here against the literals used in the bodies above.)
    // -----------------------------------------------------------------------------------------
    void TrafficEntityModule::_AssertLayout()
    {
        // meState @ +0x300, meRunningState @ +0x308 (IsPaused); opaque storage must span the
        // highest bodied offset (ShouldBeHollywoodAction/NeedToTakeAction read up to +0x72875).
        static_assert(sizeof(TrafficEntityModule::mOpaque) > 0x72875,
                      "opaque storage must cover the highest bodied byte offset");
    }
}
