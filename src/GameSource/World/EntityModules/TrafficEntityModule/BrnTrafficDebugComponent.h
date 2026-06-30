#pragma once

// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficDebugComponent.h
//
// BrnTraffic::DebugComponent -- the in-game dev-tools / debug-menu surface over the
// traffic entity module (the "Traffic" page of the debug menu). It owns the per-render-
// pass draw toggles for the traffic graph (segments / sections / section curves /
// splitters / junction boxes / stop lines / params / param links / vehicles / kill
// zones / artic points / axles / spontaneous cones / ...), a clutch of read-only pool
// counters refreshed every frame (active / free / purgatory params, active hulls), the
// air-ram / kill-zone / vehicle-pick action inputs, and the cull distance. RenderWorld
// (3D) and RenderHUD (2D) fan out to one private Draw* per toggle; OnActivate registers
// every toggle / counter / action with the debug menu; UpdateStats refreshes the
// counters from the owning module's param/hull pools.
//
// TYPE HOME for BrnTraffic::DebugComponent. Reconstructed from:
//   - BURNOUT_X360_ARTIST.XEX asm (authoritative for behaviour + the X360 member byte
//     offsets). Construct @ 0x8274FC70 pins the full member layout (see below);
//     RenderWorld @ 0x82763E78 / RenderHUD @ 0x82760AB0 pin the toggle byte offsets.
//   - DecFIGS DWARF (BrnTrafficDebugComponent.h) -> member names/types/order + the
//     class' virtual / method declaration shapes.
//   - references/Feb-2007/.../BrnTrafficDebugComponent.cpp -> style / idiom only (NOTE:
//     the Feb-2007 member set has drifted from this X360 build; the X360 asm + DWARF win).
//
// LAYOUT (X360-console byte offsets are provenance, NOT host asserts -- the PC x64
// compile widens the base region + the mpModule pointer, so we model the PC layout in
// DWARF order and do NOT byte-match; cf. the sibling PropEntityDebugComponent). The X360
// Construct @ 0x8274FC70 stores pin every offset used below:
//   mpModule              @ +0x0C  TrafficEntityModule*   (stw r30,0xC; = lpModule)
//   mfRenderCullDistance  @ +0x10  f32 = 200.0            (stfs flt_8201A1F0,0x10)
//   mbDrawSegments..mbDrawHorns + the 3 trailing bools    bytes +0x14..+0x33, +0x69..+0x6B
//   muDummyVehicleName    @ +0x34  (= 0)
//   miHullToDrawFor       @ +0x38  s32 = -1               (stw r7=-1,0x38)
//   miAirRamTypeToFire    @ +0x3C  s32 = 0                (stw 0,0x3C; OnActivate regs +0x3C)
//   muKillZoneToFire      @ +0x44  u32 = 0
//   macOverrideVehicleName@ +0x48  char[13] = "DEFAULT"   (the strcpy loop from +0x48)
//   miNumActiveParams     @ +0x58  s32 = -1   miNumPurgatoryParams @ +0x5C
//   miNumFreeParams       @ +0x60  s32 = -1   miNumActiveHulls     @ +0x64 = -1
//   muCurrentVehicleToPick@ +0x70  u32 = -1
//
// BODIED (asm-recoverable, effect entirely over `this`'s own homed members / base):
//   Construct 0x8274FC70, GetName 0x8274FF28, Update 0x82760AA8.
// DECLARATION-ONLY + FLAG (the rest of the 51-function TU): every Draw* / Render* is a
// multi-stage VMX debug-draw pipeline that reaches the still-un-homed BrnTraffic
// Vehicle / Param / Section / Hull / Junction interiors (raw module-offset reads such as
// *(mpModule + 464960/468928/...)) and/or dispatches to sibling Draw* in OTHER TUs and/or
// reads un-dumped .rdata tuning globals. Per the project rules a multi-stage VMX pipeline
// is NEVER paraphrased to scalar, and a raw offset read into an un-homed aggregate is not
// a named accessor -- so those bodies live in their own follow-on slices. They are
// declared here (this class owns them; DWARF declaration shape) but not defined in this
// slice. RenderWorld / RenderHUD / OnActivate / UpdateStats / FireAirRam / OnPickVehicle /
// ReloadBehaviourData are decl-only here for the same reason (they reach the un-homed
// module interior at raw byte offsets).
// ============================================================================

#include "types.hpp"        // u8/u32/s32/f32
#include "BrnCommonTypes.h" // Vector2, Vector3, Matrix44Affine, VecFloat
#include "rw/rwcore_structs.h" // rw::RGBA (by-value Draw* params)
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h" // CgsDev::DebugComponent

// Forward declarations -- the un-homed traffic value types the (decl-only) private draw
// methods reach only by pointer / reference. Forward-declaring keeps this header free of
// the not-yet-reconstructed traffic-graph interiors (their owning slices grow them).
namespace CgsDev
{
    struct Debug2DImmediateRender;
    struct Debug3DImmediateRender;
    struct PerfMonCpuMonitorData;
    struct StrStream;
    class  DebugRender;
}

namespace BrnTraffic
{
    class TrafficEntityModule;   // homed: BrnTrafficEntityModule.h (reuse BY NAME)
    struct Section;              // un-homed traffic-graph section record
    struct Hull;                 // un-homed traffic hull record
    struct KillZone;             // un-homed kill-zone record
    struct Axle;                 // un-homed per-vehicle axle record

    // ========================================================================
    // BrnTraffic::DebugComponent : public CgsDev::DebugComponent
    // ========================================================================
    class DebugComponent : public CgsDev::DebugComponent
    {
    public:
        // 0x8274FC70 -- run the base DebugComponent ctor, stash the owning module, default
        // the cull distance to 200, clear every draw toggle, set the action inputs to their
        // sentinels (-1 hull / 0 air-ram type / 0 kill-zone) and the override-vehicle name
        // to "DEFAULT", and seed the four pool counters to -1 (BODIED).
        void Construct(TrafficEntityModule* lpModule);

        // 0x82760AA8 -- per-frame update; refreshes the pool stat counters (BODIED: tail-
        // calls UpdateStats, matching the X360 `b UpdateStats`).
        virtual void Update();

        // -- decl-only (reach the un-homed module interior) --
        void Destruct();   // (not in this build's postmortem set; declared for shape)
        void Activate();   // (not in this build's postmortem set; declared for shape)

        // RenderWorld 0x82763E78 -- the 3D debug pass: fan out to one Draw* per draw toggle.
        // DECL-ONLY: dispatches to ~39 Draw* (several in OTHER TUs), reads module-interior
        // DEBUG flags at raw byte offsets, and inlines a VMX DrawSolidSphere block.
        virtual void RenderWorld(CgsDev::Debug3DImmediateRender* lpDisplay);
        // RenderHUD 0x82760AB0 -- the 2D debug pass (player hull id / active hulls / perf
        // table). DECL-ONLY: third gate reads a module-interior DEBUG flag at a raw offset.
        virtual void RenderHUD(CgsDev::Debug2DImmediateRender* lpDisplay);

        // 0x...: UseCameraPosition (DWARF :531) -- declared for shape; body is its own TU.
        bool UseCameraPosition() const;

    protected:
        // 0x8274FF28 -- "Traffic" (BODIED).
        virtual const char* GetName() const;
        // OnActivate 0x82762530 -- register every toggle / counter / action / tuning global
        // with the debug menu. DECL-ONLY: registers module-interior DEBUG members at raw byte
        // offsets and many un-dumped .rdata tuning globals.
        virtual void OnActivate();

    private:
        // ---- the rest of the 51-function TU: all DECL-ONLY + FLAGGED (see header banner).
        // Multi-stage VMX traffic-graph draw pipelines reaching un-homed Vehicle / Param /
        // Section / Hull / Junction interiors; bodies are their own follow-on slices. ----
        void UpdateStats();                                                    // 0x827584F0
        void RenderPerformanceTable(CgsDev::Debug2DImmediateRender* lpDisplay); // 0x82754038
        void RenderPerfmon(CgsDev::Debug2DImmediateRender* lpDisplay,
                           const CgsDev::PerfMonCpuMonitorData* lpData,
                           f32 lfX, f32 lfY, f32 lfWidth);                      // 0x8274FDD0
        bool ShouldObjectBeCulled(Vector3 lPosition,
                                  CgsDev::Debug3DImmediateRender* lpDisplay) const;
        bool ShouldSectionBeCulled(const Section* lpSection, const Hull* lpHull,
                                   CgsDev::Debug3DImmediateRender* lpDisplay) const;

        void DrawSegments(CgsDev::Debug3DImmediateRender* lpDisplay) const;            // 0x82758670
        void DrawSections(CgsDev::Debug3DImmediateRender* lpDisplay) const;            // 0x82760B68
        void DrawNeighbours(CgsDev::Debug3DImmediateRender* lpDisplay) const;          // 0x82758CC0
        void DrawSectionCurves(CgsDev::Debug3DImmediateRender* lpDisplay) const;       // 0x82760C98
        void DrawJunctions(CgsDev::Debug3DImmediateRender* lpDisplay) const;           // 0x82760DF0
        void DrawStopLines(CgsDev::Debug3DImmediateRender* lpDisplay) const;           // 0x827636A8
        void DrawStartLines(CgsDev::Debug3DImmediateRender* lpDisplay) const;
        void DrawSectionsInHull(s32 liHull, CgsDev::Debug3DImmediateRender* lpDisplay) const;       // 0x82758888
        void DrawSectionCurvesInHull(s32 liHull, CgsDev::Debug3DImmediateRender* lpDisplay) const;  // 0x82758A18
        void DrawJunctionsInHull(s32 liHull, CgsDev::Debug3DImmediateRender* lpDisplay) const;       // 0x82759068
        u32  GetSectionIdWithStopLine(u8 lu8Junction, u16 lu16Stop) const;
        void DrawStopLinesInHull(s32 liHull, CgsDev::Debug3DImmediateRender* lpDisplay) const;
        void GetStopLinePosition(u16 lu16Section, u8 lu8A, u8 lu8B,
                                 Vector3& lrStart, Vector3& lrEnd) const;
        void DrawSplitters(CgsDev::Debug3DImmediateRender* lpDisplay) const;           // 0x82759738
        void DrawParams(CgsDev::Debug3DImmediateRender* lpDisplay) const;              // 0x82754328
        void DrawLerpedParams(CgsDev::Debug3DImmediateRender* lpDisplay) const;        // 0x82754550
        void DrawParamBehaviours(CgsDev::Debug3DImmediateRender* lpDisplay) const;     // 0x82759D68
        void DrawParamSlowData(CgsDev::Debug3DImmediateRender* lpDisplay) const;       // 0x8275A0F8
        void DrawParamTrails(CgsDev::Debug3DImmediateRender* lpDisplay) const;         // 0x8275A390
        void DrawVehicles(CgsDev::Debug3DImmediateRender* lpDisplay) const;            // 0x8275A538
        void DrawNonCollidableVehicles(CgsDev::Debug3DImmediateRender* lpDisplay) const; // 0x8275A6F0
        void DrawCollidableVehicles(CgsDev::Debug3DImmediateRender* lpDisplay) const;  // 0x8275A870
        void DrawPhysicalVehicleState(CgsDev::Debug3DImmediateRender* lpDisplay) const;
        void DrawStuckDetection(CgsDev::Debug3DImmediateRender* lpDisplay) const;      // 0x8275AEC0
        void DrawVehicleManoeuvres(CgsDev::Debug3DImmediateRender* lpDisplay) const;
        void DrawParamVehicleLinks(CgsDev::Debug3DImmediateRender* lpDisplay) const;   // 0x82754998
        void DrawParamParamLinks(CgsDev::Debug3DImmediateRender* lpDisplay) const;     // 0x82754B08
        void DrawNearestVehicleId(CgsDev::Debug3DImmediateRender* lpDisplay) const;    // 0x82750110
        void DrawVehicleIds(CgsDev::Debug3DImmediateRender* lpDisplay) const;          // 0x827502F8
        void DrawActiveHulls(CgsDev::Debug2DImmediateRender* lpDisplay) const;         // 0x8275B2E8
        void DrawAllKillZones(CgsDev::Debug3DImmediateRender* lpDisplay) const;
        void DrawRecentKillZones(CgsDev::Debug3DImmediateRender* lpDisplay) const;     // 0x827611A8
        // mKillZoneId is TrafficData::KillZoneId == u64 (BrnTrafficData.h:40); named here as
        // the proven underlying width to avoid forcing the TrafficData resource type complete.
        void DrawKillZone(const KillZone* lpKillZone, u64 luKillZoneId,
                          CgsDev::Debug3DImmediateRender* lpDisplay) const;            // 0x8275B678
        static void FireKillZone(void* lpUserData);
        static void FireAirRam(void* lpUserData);                                      // 0x82750508
        void DrawRearArticPoints(CgsDev::Debug3DImmediateRender* lpDisplay) const;     // 0x82754DA8
        void DrawFrontArticPoints(CgsDev::Debug3DImmediateRender* lpDisplay) const;    // 0x82754F18
        void DrawAxles(CgsDev::Debug3DImmediateRender* lpDisplay) const;               // 0x827612C8
        void DrawAxle(const Axle* lpAxle, CgsDev::Debug3DImmediateRender* lpDisplay) const; // 0x8275BBC0
        void DrawHorns(CgsDev::Debug3DImmediateRender* lpDisplay) const;               // 0x82750570
        void DrawAvoidance(CgsDev::Debug3DImmediateRender* lpDisplay) const;           // 0x8275BE28
        void DrawPressure(CgsDev::Debug3DImmediateRender* lpDisplay) const;
        void DrawVehicleSwervingDebugging(CgsDev::Debug3DImmediateRender* lpDisplay) const; // 0x82755058
        void DrawPlayerCarHullId(CgsDev::Debug2DImmediateRender* lpDisplay) const;     // 0x8275C938
        void DrawFuzzyLogic(CgsDev::Debug3DImmediateRender* lpDisplay) const;          // 0x8275C298
        void DrawShowtime(CgsDev::Debug3DImmediateRender* lpDisplay) const;            // 0x8275CA58
        void DrawJunctionFUPDetection(CgsDev::Debug3DImmediateRender* lpDisplay) const;
        static void OnKillNearPlayer(void* lpUserData);
        void DrawPickedVehicle(CgsDev::Debug3DImmediateRender* lpDisplay) const;
        static void OnPickVehicle(void* lpUserData);                                   // 0x82750788
        static void ReloadBehaviourData(void* lpUserData);                             // 0x827614D8
        void DrawArrow(Vector3 lFrom, Vector3 lTo, rw::RGBA lColour,
                       CgsDev::Debug3DImmediateRender* lpDisplay) const;               // 0x827507A0
        void DrawOffsetArrow(Vector3 lFrom, Vector3 lTo, f32 lfOffset, rw::RGBA lColour,
                             CgsDev::Debug3DImmediateRender* lpDisplay) const;
        void RenderSpontaneousTargetCones(CgsDev::Debug3DImmediateRender* lpDisplay) const;
        void RenderSingleDebugCone(CgsDev::Debug3DImmediateRender* lpDisplay, Matrix44Affine lTransform,
                                   VecFloat lExtent, rw::RGBA lColour, s32 liSegments) const;
        void DrawTextWithOffsets(CgsDev::DebugRender& lrDisplay, CgsDev::StrStream* lpStream,
                                 f32 lfX, f32 lfY) const;                              // 0x82750AE8
        void RenderVerticalMeter(CgsDev::Debug3DImmediateRender* lpDisplay, Vector3 lPosition,
                                 Vector2 lSize, f32 lfMin, f32 lfMax, rw::RGBA lColourA,
                                 rw::RGBA lColourB, f32 lfValueA, f32 lfValueB) const; // 0x82755848
        static void DumpLoggedData(void* lpUserData);

    private:
        // ---- DWARF-faithful member layout (X360 byte offsets in the banner above; modelled
        //      in DWARF order, PC layout, NOT byte-matched). ----
        TrafficEntityModule* mpModule;                 // +0x0C owning module
        f32                  mfRenderCullDistance;      // +0x10 (default 200.0)

        bool                 mbDrawSegments;            // +0x14
        bool                 mbDrawSections;            // +0x15
        bool                 mbDrawSectionCurves;       // +0x16
        bool                 mbDrawNeighbours;          // +0x17
        bool                 mbDrawJunctions;           // +0x18
        bool                 mbDrawStopLines;           // +0x19
        bool                 mbDrawStartLines;          // +0x1A
        bool                 mbDrawSplitters;           // +0x1B
        bool                 mbDrawParams;              // +0x1C
        bool                 mbDrawLerpedParams;        // +0x1D
        bool                 mbDrawParamBehaviours;     // +0x1E
        bool                 mbDrawParamSlowData;       // +0x1F
        bool                 mbDrawParamTrails;         // +0x20
        bool                 mbDrawVehicles;            // +0x21
        bool                 mbDrawNonCollidableVehicles;// +0x22
        bool                 mbDrawCollidableVehicles;  // +0x23
        bool                 mbDrawPhysicalVehicleState;// +0x24
        bool                 mbDrawStuckDetection;      // +0x25
        bool                 mbDrawVehicleManoeuvres;   // +0x26
        bool                 mbDrawParamVehicleLinks;   // +0x27
        bool                 mbDrawParamParamLinks;     // +0x28
        bool                 mbDrawNearestVehicleId;    // +0x29
        bool                 mbDrawVehicleIds;          // +0x2A
        bool                 mbDrawActiveHulls;         // +0x2B
        bool                 mbDrawPlayerHullId;        // +0x2C
        bool                 mbDrawAllKillZones;        // +0x2D
        bool                 mbDrawRecentKillZones;     // +0x2E
        bool                 mbDrawSpontaneousCones;    // +0x2F
        bool                 mbDrawRearArticPoints;     // +0x30
        bool                 mbDrawFrontArticPoints;    // +0x31
        bool                 mbDrawAxles;               // +0x32
        bool                 mbDrawHorns;               // +0x33
        bool                 mbUseCameraPosition;       // (DWARF :128)

        s32                  miHullToDrawFor;           // +0x38 (default -1)
        s32                  miAirRamTypeToFire;         // +0x3C (default 0)
        u32                  muKillZoneToFire;          // +0x44 (default 0)
        u32                  muDummyVehicleName;        // +0x34 (default 0)
        char                 macOverrideVehicleName[13];// +0x48 (default "DEFAULT")

        s32                  miNumActiveParams;         // +0x58 (default -1)
        s32                  miNumPurgatoryParams;      // +0x5C (default -1)
        s32                  miNumFreeParams;           // +0x60 (default -1)
        s32                  miNumActiveHulls;          // +0x64 (default -1)

        bool                 mbDrawVehicleSwervingDebug;// (DWARF :149) +0x68
        bool                 mbDrawAvoidance;           // (DWARF :151) +0x69
        bool                 mbDrawAvoidanceAllShit;    // (DWARF :152) +0x6A
        bool                 mbDrawPressure;            // (DWARF :153) +0x6B
        bool                 mbDrawJunctionFUPDetection;// (DWARF :154) +0x6C

        u32                  muCurrentVehicleToPick;    // +0x70 (default -1)
    };
}
