// ============================================================================
// Bodies for the traffic-entity-module debug overlay, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This slice bodies ONLY the asm-recoverable methods whose
// effect is entirely over `this`'s own homed members / the homed DebugComponent base:
//   Construct @ 0x8274FC70   GetName @ 0x8274FF28   Update @ 0x82760AA8
//
// The remaining 48 functions of this 51-function TU are declaration-only (see
// BrnTrafficDebugComponent.h banner): every Draw* / Render* is a multi-stage VMX traffic-
// graph draw pipeline reaching the still-un-homed BrnTraffic Vehicle / Param / Section /
// Hull / Junction interiors (raw module-offset reads such as *(mpModule + 464960 / 468928
// / 469108 / ...)) and/or dispatches to sibling Draw* defined in OTHER TUs and/or reads
// un-dumped .rdata tuning globals. Per the project rules a multi-stage VMX pipeline is
// NEVER paraphrased to scalar and a raw offset into an un-homed aggregate is not a named
// accessor, so those bodies belong to their own follow-on slices.
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficDebugComponent.h"
#include "SharedClasses/Traffic/BrnTrafficHull.h"      // BrnTraffic::Hull (mpaRungs) + Section 48-byte placeholder
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstring>                                    // strcpy

namespace BrnTraffic
{
    // @ 0x8274FC70. Run the base DebugComponent ctor (the leading "BaseCollisionGenerator::
    // Destruct" in the pseudocode is the COMDAT-folded CgsDev::DebugComponent::Construct),
    // assert + stash the owning module, default the cull distance to 200, clear every draw
    // toggle, set the action inputs to their sentinels, copy "DEFAULT" into the override-
    // vehicle name, and seed the four pool counters (and the picked-vehicle id) to -1.
    void DebugComponent::Construct(TrafficEntityModule* lpModule)
    {
        CgsDev::DebugComponent::Construct();

        CGS_ASSERT(lpModule != nullptr, "lpModule != NULL");
        mpModule = lpModule;                       // +0x0C

        mfRenderCullDistance = 200.0f;             // +0x10

        mbDrawSegments             = false;        // +0x14
        mbDrawSections             = false;        // +0x15
        mbDrawSectionCurves        = false;        // +0x16
        mbDrawNeighbours           = false;        // +0x17
        mbDrawJunctions            = false;        // +0x18
        mbDrawStopLines            = false;        // +0x19
        mbDrawStartLines           = false;        // +0x1A
        mbDrawSplitters            = false;        // +0x1B
        mbDrawParams               = false;        // +0x1C
        mbDrawLerpedParams         = false;        // +0x1D
        mbDrawParamBehaviours      = false;        // +0x1E
        mbDrawParamSlowData        = false;        // +0x1F
        mbDrawParamTrails          = false;        // +0x20
        mbDrawVehicles             = false;        // +0x21
        mbDrawNonCollidableVehicles= false;        // +0x22
        mbDrawCollidableVehicles   = false;        // +0x23
        mbDrawPhysicalVehicleState = false;        // +0x24
        mbDrawStuckDetection       = false;        // +0x25
        mbDrawVehicleManoeuvres    = false;        // +0x26
        mbDrawParamVehicleLinks    = false;        // +0x27
        mbDrawParamParamLinks      = false;        // +0x28
        mbDrawNearestVehicleId     = false;        // +0x29
        mbDrawVehicleIds           = false;        // +0x2A
        mbDrawActiveHulls          = false;        // +0x2B
        mbDrawPlayerHullId         = false;        // +0x2C
        mbDrawAllKillZones         = false;        // +0x2D
        mbDrawRecentKillZones      = false;        // +0x2E
        mbDrawSpontaneousCones     = false;        // +0x2F
        mbDrawRearArticPoints      = false;        // +0x30
        mbDrawFrontArticPoints     = false;        // +0x31
        mbDrawAxles                = false;        // +0x32
        mbDrawHorns                = false;        // +0x33

        miHullToDrawFor      = -1;                  // +0x38 (stw r7=-1,0x38)
        miAirRamTypeToFire   = 0;                   // +0x3C
        muKillZoneToFire     = 0;                   // +0x44
        muDummyVehicleName   = 0;                   // +0x34

        // The override-vehicle name defaults to "DEFAULT" (the X360 inlined byte-copy loop
        // from &"DEFAULT" into &macOverrideVehicleName @ +0x48).
        strcpy(macOverrideVehicleName, "DEFAULT");

        mbDrawVehicleSwervingDebug = false;        // +0x68
        mbDrawJunctionFUPDetection = false;        // +0x6C

        miNumActiveParams    = -1;                 // +0x58 (stw r7=-1)
        miNumPurgatoryParams = -1;                 // +0x5C
        miNumFreeParams      = -1;                 // +0x60
        miNumActiveHulls     = -1;                 // +0x64

        muCurrentVehicleToPick = static_cast<u32>(-1); // +0x70 (stw r10=-1,0x70)
    }

    // @ 0x8274FF28. The debug-menu page name.
    const char* DebugComponent::GetName() const
    {
        return "Traffic";
    }

    // @ 0x82760AA8. Per-frame update -- the X360 is a single `b UpdateStats` tail call.
    void DebugComponent::Update()
    {
        UpdateStats();
    }

    // @ 0x82750000. A traffic-graph section is culled iff BOTH endpoints of its
    // rung span are individually culled: the START point (maPoints[0]) of the FIRST
    // rung and the END point (maPoints[1]) of the LAST rung. The rungs live in
    // lpHull->mpaRungs (element = LaneRung, Vector3[2], stride 32); the section
    // occupies rungs [muRungOffset .. muRungOffset+muNumRungs-1].
    //
    // The committed BrnTraffic::Section is still the guarded 48-byte placeholder
    // (BrnTrafficHull.h), so its two fields are read by raw byte offset exactly as
    // the X360 asm does (lwz 0(section) / lbz 4(section)), matching the committed
    // Hull::GetSection opaque-Section idiom. DWARF (BrnTrafficSection.h:108/109):
    //   muRungOffset u32 @+0     muNumRungs u8 @+4
    // LaneRung is Vector3[2] (stride 32); maPoints[0] @+0, maPoints[1] @+16.
    bool DebugComponent::ShouldSectionBeCulled(const Section* lpSection, const Hull* lpHull,
                                               CgsDev::Debug3DImmediateRender* lpDisplay) const
    {
        CGS_ASSERT(lpSection != nullptr, "lpSection");
        CGS_ASSERT(lpHull    != nullptr, "lpHull");
        CGS_ASSERT(lpDisplay != nullptr, "lpDisplay");

        static const u32 KU_LANE_RUNG_STRIDE    = 32;  // sizeof(LaneRung) (Vector3[2])
        static const u32 KU_LANE_RUNG_END_POINT = 16;  // byte offset of maPoints[1]

        const u8* lpcSection = reinterpret_cast<const u8*>(lpSection);
        const u32 luRungOffset = *reinterpret_cast<const u32*>(lpcSection + 0);   // muRungOffset
        const u8  lu8NumRungs  = *reinterpret_cast<const u8*>(lpcSection + 4);    // muNumRungs

        const u8* lpcRungs = reinterpret_cast<const u8*>(lpHull->mpaRungs);

        // First rung's start point (maPoints[0]).
        const Vector3& lStartPoint =
            *reinterpret_cast<const Vector3*>(lpcRungs + KU_LANE_RUNG_STRIDE * luRungOffset);

        if (ShouldObjectBeCulled(lStartPoint, lpDisplay))
        {
            // Last rung's end point (maPoints[1] of rung muRungOffset+muNumRungs-1).
            const Vector3& lEndPoint =
                *reinterpret_cast<const Vector3*>(
                    lpcRungs
                    + KU_LANE_RUNG_STRIDE * (luRungOffset + lu8NumRungs - 1)
                    + KU_LANE_RUNG_END_POINT);
            return ShouldObjectBeCulled(lEndPoint, lpDisplay);
        }
        return false;
    }
}
