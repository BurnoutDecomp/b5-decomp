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
// (3D) and RenderHUD (2D) fan out to private render helpers per toggle; OnActivate registers
// all 108 original rows with the debug menu; UpdateStats refreshes counters from the owning
// module's param/hull pools.
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
//   mbDrawSegments..mbDrawHorns                           bytes +0x14..+0x33
//   mbUseCameraPosition   @ +0x34  (= 0)
//   miHullToDrawFor       @ +0x38  s32 = -1               (stw r7=-1,0x38)
//   miAirRamTypeToFire    @ +0x3C  s32 = 0                (stw 0,0x3C; OnActivate regs +0x3C)
//   muKillZoneToFire      @ +0x40  u32 = 0
//   muDummyVehicleName    @ +0x44  u32 = 0
//   macOverrideVehicleName@ +0x48  char[13] = "DEFAULT"   (the strcpy loop from +0x48)
//   miNumActiveParams     @ +0x58  s32 = -1   miNumPurgatoryParams @ +0x5C
//   miNumFreeParams       @ +0x60  s32 = -1   miNumActiveHulls     @ +0x64 = -1
//   muCurrentVehicleToPick@ +0x70  u32 = -1
//
// Reconstructed in the Traffic batch (source evidence and reader limits are in
// scratch/DEBUG_MENU_1011/traffic_batch/):
//   Construct 0x8274FC70, GetName 0x8274FF28, Update 0x82760AA8, UpdateStats 0x827584F0,
//   OnActivate 0x82762530, RenderWorld 0x82763E78, RenderHUD 0x82760AB0, and registered
//   callbacks. OnActivate preserves all 108 ARTIST rows, including original range mis-targets;
//   see scratch/DEBUG_MENU_1011/traffic_batch/registration_map.md. The ARTIST-dispatched
//   render helpers are bodied from their original consumers; unrelated declarations without
//   an attested live dispatch remain inert.
// ============================================================================

#include "types.hpp"        // u8/u32/s32/f32
#include "BrnCommonTypes.h" // Vector2, Vector3, Matrix44Affine, VecFloat
#include "rw/rwcore_structs.h" // rw::RGBA (by-value Draw* params)
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h" // CgsDev::DebugComponent

// Forward declarations keep the component header independent of the traffic graph's heavier
// owner headers; the implementation includes the concrete types its render helpers read.
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
    struct Section;
    struct Hull;
    struct KillZone;
    struct Axle;

    // File-scope ARTIST tuning cells registered by OnActivate. The bounded full-image reader
    // review confirms no behavior consumer yet; preserve the original menu cells and defaults
    // rather than binding unrelated fields. Export-hole limits are recorded in the traffic map.
    extern f32 kfSideRamProportion; // X360 0x82F2FFEC, 0.15
    extern f32 kfSideRamStrength;   // X360 0x82F2FF94, 0.04
    extern f32 unk_8300D018;        // X360 0x8300D018, 0.0
    extern f32 unk_82F2FFFC;        // X360 0x82F2FFFC, 0.01
    extern f32 unk_82F30000;        // X360 0x82F30000, 0.05
    extern f32 unk_82F30004;        // X360 0x82F30004, 0.01
    extern f32 unk_82F30008;        // X360 0x82F30008, 0.05
    extern f32 unk_82F3000C;        // X360 0x82F3000C, 0.15
    extern f32 unk_82F30010;        // X360 0x82F30010, 0.4
    extern f32 unk_82F30014;        // X360 0x82F30014, 1.0
    extern f32 unk_82F30018;        // X360 0x82F30018, 0.04
    extern f32 unk_82F3001C;        // X360 0x82F3001C, 0.06
    extern f32 unk_82F30020;        // X360 0x82F30020, 0.07
    extern f32 unk_82F30024;        // X360 0x82F30024, 0.09
    extern f32 unk_82F30028;        // X360 0x82F30028, 0.5
    extern f32 unk_82F30030;        // X360 0x82F30030, 20.0
    extern f32 unk_82F30034;        // X360 0x82F30034, 3.0
    extern f32 unk_82F30038;        // X360 0x82F30038, 30.0

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

        // The X360 module does not call this component destructor; it remains module-owned.
        void Destruct();   // (not in this build's postmortem set; declared for shape)
        void Activate();   // (not in this build's postmortem set; declared for shape)

        // RenderWorld 0x82763E78 -- original 3D dispatch gates and the inlined FUP sphere.
        virtual void RenderWorld(CgsDev::Debug3DImmediateRender* lpDisplay);
        // RenderHUD 0x82760AB0 -- original player-hull, active-hulls, and perf-table gates.
        virtual void RenderHUD(CgsDev::Debug2DImmediateRender* lpDisplay);

        // DecFIGS :531; ARTIST UpdateRaceCarHulls @0x82721460 inlines its byte read at +0x34.
        bool UseCameraPosition() const { return mbUseCameraPosition; }

    protected:
        // 0x8274FF28 -- "Traffic" (BODIED).
        virtual const char* GetName() const;
        // OnActivate 0x82762530 -- registers all 108 original controls/actions and metadata.
        virtual void OnActivate();

    private:
        // Component lifecycle, page rendering, registered callbacks, and all ARTIST-
        // dispatched render helpers are bodied in BrnTrafficDebugComponent.cpp. Only original
        // declarations without an attested live dispatch remain intentionally unimplemented.
        void UpdateStats();                                                    // 0x827584F0
        void RenderPerformanceTable(CgsDev::Debug2DImmediateRender* lpDisplay); // 0x82754038
        void RenderPerfmon(CgsDev::Debug2DImmediateRender* lpDisplay,
                           const CgsDev::PerfMonCpuMonitorData* lpData,
                           f32 lfX, f32 lfY, f32 lfScale);                      // 0x8274FDD0
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
        static void FireKillZone(void* lpUserData);                             // 0x8275BB40 (no named export)
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
        static void OnKillNearPlayer(void* lpUserData);                         // 0x82750748
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
                                 Vector2 lOffset, f32 lfHalfWidth, f32 lfHeight,
                                 rw::RGBA lOutlineColour, rw::RGBA lFillColour,
                                 f32 lfFullScale, f32 lfValue) const; // 0x82755848
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
        bool                 mbUseCameraPosition;       // +0x34 (DWARF :128)

        s32                  miHullToDrawFor;           // +0x38 (default -1)
        s32                  miAirRamTypeToFire;         // +0x3C (default 0)
        u32                  muKillZoneToFire;          // +0x40 (default 0)
        u32                  muDummyVehicleName;        // +0x44 (default 0)
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

        mutable u32          muCurrentVehicleToPick;    // +0x70 (default -1; const DrawPickedVehicle cache)
    };
}
