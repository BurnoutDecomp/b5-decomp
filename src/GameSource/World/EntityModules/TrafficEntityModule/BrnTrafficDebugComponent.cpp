// ============================================================================
// Bodies for the Traffic debug page, reconstructed from BURNOUT_X360_ARTIST.XEX.
// This batch restores the original 108 registrations and metadata, component and module
// lifecycle, update/render/callback paths, telemetry writers, and attested draw helpers.
// Each production body is checked against its ARTIST pseudocode/assembly; the page/storage
// and bounded reader map is in scratch/DEBUG_MENU_1011/traffic_batch/.
// Keep original declarations without an attested live dispatch inert; in particular, do
// not revive the spontaneous-cone or dump-log declarations.
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficDebugComponent.h"
#include "SharedClasses/Traffic/BrnTrafficSection.h"       // Section, LaneRung, Neighbour
#include "SharedClasses/Traffic/BrnTrafficHull.h"          // Hull, Junction and rung arrays
#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h" // TrafficData counts, vehicle and kill-zone tables
#include "SharedClasses/Traffic/BrnTrafficPvs.h"           // active-hull grid dimensions
#include "SharedClasses/Traffic/BrnTrafficKillZone.h"      // KillZone, KillZoneRegion
#include "SharedClasses/Traffic/BrnTrafficLightTrigger.h"  // LightTriggerStartData
#include "SharedClasses/Traffic/Junctions/BrnJunctionLogicBox.h" // JunctionLogicBox
#include "SharedClasses/Traffic/Junctions/BrnTrafficStopLine.h" // StopLine
#include "GameShared/GameClasses/Core/CgsID.h"           // CgsIDUnCompress
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"   // GetHull / GetHullRuntime / maaRaceCarHulls
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficConstants.h"      // traffic capacities / sentinels
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficParam.h"          // Param, transforms, history and slow data
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicle.h"        // Vehicle, Axle and VehicleAxles
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicleTypeRuntime.h" // bounds and articulation pivots
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficFuzzyLogicBehaviours.h" // fuzzy debug toggle
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficHullRuntime.h"    // IsStoplineRed / GetSectionSpanVehicleCount
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                                    // CgsCore::SPrintf
#include "GameShared/GameClasses/Development/CgsStrStream.h"                               // CgsDev::StrStream
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "rw/core/stdc/stdc.h"                                                            // ConvertI64ToA
#include "rw/math/vpu/matrix44_operation.h"                                               // full Matrix44 inverse
#include "rw/math/vpu/matrix44affine_operation.h"                                         // SLerpDetail reciprocal estimates
#include "rw/math/vpu/vector3_operation.h"                                                 // Cross / Dot / operator+-*

#include <cmath>                                     // sqrt / fabs / floor / fma
#include <cstdio>                                    // sprintf
#include <cstring>                                   // strcpy

namespace BrnTraffic
{
    // ARTIST OnActivate @0x82762530 registers these global cells directly. Their exact
    // console addresses/defaults are retained; no extra traffic consumer is invented here.
    f32 kfSideRamProportion = 0.15f; // X360 0x82F2FFEC
    f32 kfSideRamStrength   = 0.04f; // X360 0x82F2FF94
    f32 unk_8300D018        = 0.0f;  // X360 0x8300D018
    f32 unk_82F2FFFC        = 0.01f; // X360 0x82F2FFFC
    f32 unk_82F30000        = 0.05f; // X360 0x82F30000
    f32 unk_82F30004        = 0.01f; // X360 0x82F30004
    f32 unk_82F30008        = 0.05f; // X360 0x82F30008
    f32 unk_82F3000C        = 0.15f; // X360 0x82F3000C
    f32 unk_82F30010        = 0.4f;  // X360 0x82F30010
    f32 unk_82F30014        = 1.0f;  // X360 0x82F30014
    f32 unk_82F30018        = 0.04f; // X360 0x82F30018
    f32 unk_82F3001C        = 0.06f; // X360 0x82F3001C
    f32 unk_82F30020        = 0.07f; // X360 0x82F30020
    f32 unk_82F30024        = 0.09f; // X360 0x82F30024
    f32 unk_82F30028        = 0.5f;  // X360 0x82F30028
    f32 unk_82F30030        = 20.0f; // X360 0x82F30030
    f32 unk_82F30034        = 3.0f;  // X360 0x82F30034
    f32 unk_82F30038        = 30.0f; // X360 0x82F30038

    namespace
    {
        // VecFloat and Vector3 are separate portable lane types, but the original VMX
        // MultAdd accepts the same 128-bit register for its multiplier. Copy every lane
        // explicitly, then use the canonical four-lane fused operation; do not scalarize
        // or split the multiply-add.
        inline Vector3 TrafficMultAdd(Vector3 lValue, VecFloat lScale, Vector3 lAddend)
        {
            const Vector3 lVectorScale = {lScale.x, lScale.y, lScale.z, lScale.w};
            return rw::math::vpu::MultAdd(lValue, lVectorScale, lAddend);
        }

        // TrafficData's X360 StringList at 0x820C0F70 (values 0..7).
        const CgsDev::DebugUI::StringList KA_AIR_RAM_TYPE_OPTIONS[] = {
            {0, "SPIN CW FRONT"},
            {1, "SPIN CW REAR"},
            {2, "SPIN CCW FRONT"},
            {3, "SPIN CCW REAR"},
            {4, "ROLL LEFT"},
            {5, "ROLL RIGHT"},
            {6, "SPIN OUT"},
            {7, "TEST"},
            {0, 0},
        };
    }

    // @ 0x8274FC70. Run the base DebugComponent ctor (the leading "BaseCollisionGenerator::
    // Destruct" in the pseudocode is the COMDAT-folded CgsDev::DebugComponent::Construct),
    // assert + stash the owning module, default the cull distance to 200, clear every draw
    // toggle, set the action inputs to their sentinels, copy "DEFAULT" into the override-
    // vehicle name, and seed the four pool counters (and the picked-vehicle id) to -1.
    void DebugComponent::Construct(TrafficEntityModule* lpModule)
    {

        CGS_ASSERT(lpModule != 0, "lpModule != NULL");
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

        mbUseCameraPosition = false;               // +0x34
        miHullToDrawFor      = -1;                  // +0x38 (stw r7=-1,0x38)
        miAirRamTypeToFire   = 0;                   // +0x3C
        muKillZoneToFire     = 0;                   // +0x40
        muDummyVehicleName   = 0;                   // +0x44

        // The override-vehicle name defaults to "DEFAULT" (the X360 inlined byte-copy loop
        // from &"DEFAULT" into &macOverrideVehicleName @ +0x48).
        strcpy(macOverrideVehicleName, "DEFAULT");

        mbDrawVehicleSwervingDebug = false;        // +0x68
        mbDrawAvoidance = false;                   // +0x69
        mbDrawAvoidanceAllShit = false;             // +0x6A
        mbDrawPressure = false;                    // +0x6B
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

    // @0x82763E78. Preserve the original X360 dispatch order and route each toggle to its
    // recovered draw consumer.
    void DebugComponent::RenderWorld(CgsDev::Debug3DImmediateRender* lpDisplay)
    {
        CGS_ASSERT(mpModule != 0, "mpModule != NULL");

        if (mbDrawSegments)              DrawSegments(lpDisplay);
        if (mbDrawSections)              DrawSections(lpDisplay);
        if (mbDrawSectionCurves)         DrawSectionCurves(lpDisplay);
        if (mbDrawNeighbours)            DrawNeighbours(lpDisplay);
        if (mbDrawJunctions)             DrawJunctions(lpDisplay);
        if (mbDrawStopLines)             DrawStopLines(lpDisplay);
        if (mbDrawStartLines)            DrawStartLines(lpDisplay);
        if (mbDrawSplitters)             DrawSplitters(lpDisplay);
        if (mbDrawParams)                DrawParams(lpDisplay);
        if (mbDrawLerpedParams)          DrawLerpedParams(lpDisplay);
        if (mbDrawParamBehaviours)       DrawParamBehaviours(lpDisplay);
        if (mbDrawParamSlowData)         DrawParamSlowData(lpDisplay);
        if (mbDrawParamTrails)           DrawParamTrails(lpDisplay);
        if (mbDrawVehicles)              DrawVehicles(lpDisplay);
        if (mbDrawNonCollidableVehicles) DrawNonCollidableVehicles(lpDisplay);
        if (mbDrawCollidableVehicles)    DrawCollidableVehicles(lpDisplay);
        if (mbDrawPhysicalVehicleState)  DrawPhysicalVehicleState(lpDisplay);
        if (mbDrawStuckDetection)        DrawStuckDetection(lpDisplay);
        if (mbDrawVehicleManoeuvres)     DrawVehicleManoeuvres(lpDisplay);
        if (mbDrawParamVehicleLinks)     DrawParamVehicleLinks(lpDisplay);
        if (mbDrawParamParamLinks)       DrawParamParamLinks(lpDisplay);
        if (mbDrawNearestVehicleId)      DrawNearestVehicleId(lpDisplay);
        if (mbDrawVehicleIds)            DrawVehicleIds(lpDisplay);
        if (mbDrawAllKillZones)          DrawAllKillZones(lpDisplay);
        if (mbDrawRecentKillZones)       DrawRecentKillZones(lpDisplay);
        if (mbDrawFrontArticPoints)      DrawFrontArticPoints(lpDisplay);
        if (mbDrawRearArticPoints)       DrawRearArticPoints(lpDisplay);
        if (mbDrawAxles)                 DrawAxles(lpDisplay);
        if (mbDrawHorns)                 DrawHorns(lpDisplay);
        if (mbDrawAvoidance)             DrawAvoidance(lpDisplay);
        if (mpModule->mFuzzyBehaviours.mbDEBUGRenderParamScores)
            DrawFuzzyLogic(lpDisplay);
        if (mpModule->mbDEBUGShowtimeStuff)
            DrawShowtime(lpDisplay);
        if (mbDrawPressure)              DrawPressure(lpDisplay);
        if (mbDrawVehicleSwervingDebug)  DrawVehicleSwervingDebugging(lpDisplay);

        if (mbDrawJunctionFUPDetection)
        {
            // X360 @0x82764214..0x8276425C: compare mfJunctionFUP with 65.0, select the exact
            // packed colour, then DrawSolidSphere(mAveragePhysicalCentre, 60.0, colour).
            rw::RGBA lColour;
            lColour.m_rgba = (mpModule->mfJunctionFUP < 65.0f) ? 0x64640000u : 0x640000FFu;
            lpDisplay->DrawSolidSphere(mpModule->mAveragePhysicalCentre, 60.0f, lColour);
        }

        if (mpModule->mbDEBUGPickVehicleFromCamera)
            DrawPickedVehicle(lpDisplay);
    }

    // @0x82760AB0. Preserve the X360 HUD ordering and its module-owned perfmon gate.
    void DebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender* lpDisplay)
    {
        CGS_ASSERT(mpModule != 0, "mpModule != NULL");

        if (mbDrawPlayerHullId)
            DrawPlayerCarHullId(lpDisplay);
        if (mbDrawActiveHulls)
            DrawActiveHulls(lpDisplay);
        if (mpModule->mbDEBUGRenderSpecialPerfmons)
            RenderPerformanceTable(lpDisplay);
    }

    // @0x82762530. Keep all 108 original rows, their overload/tag choice, group, metadata,
    // callbacks, and the two range writes whose X360 pointer argument targets the hull selector.
    void DebugComponent::OnActivate()
    {
        RegisterVariable(&mpModule->mbDEBUGTurnTrafficOff, "", "Turn traffic off");

        RegisterVariable(&mpModule->miDEBUGOverBudgetness, "Stats", "Over budget-ness");
        SetReadOnly(&mpModule->miDEBUGOverBudgetness, true);
        RegisterVariable(&miNumActiveParams, "Stats", "Active params");
        SetReadOnly(&miNumActiveParams, true);
        RegisterVariable(&miNumFreeParams, "Stats", "Free params");
        SetReadOnly(&miNumFreeParams, true);
        RegisterVariable(&miNumPurgatoryParams, "Stats", "Purgatory params");
        SetReadOnly(&miNumPurgatoryParams, true);
        RegisterVariable(&miNumActiveHulls, "Stats", "Active hulls");
        SetReadOnly(&miNumActiveHulls, true);

        RegisterVariable(&miAirRamTypeToFire, "Air Rams", "Type");
        SetRange(&miAirRamTypeToFire, 0, 7);
        SetOptions(&miAirRamTypeToFire, KA_AIR_RAM_TYPE_OPTIONS);
        RegisterFunction(&DebugComponent::FireAirRam, this, "Air Rams", "Fire!");

        RegisterVariable(&kfSideRamProportion, "Checking", "Side ram proportion");
        RegisterVariable(&kfSideRamStrength, "Checking", "Side ram strength");
        RegisterVariable(&unk_8300D018, "Checking", "Min strength stat for traffic check");
        RegisterVariable(&unk_82F2FFFC, "Checking", "After check min first ram strength");
        RegisterVariable(&unk_82F30000, "Checking", "After check max first ram strength");
        RegisterVariable(&unk_82F30004, "Checking", "After slam min first ram strength");
        RegisterVariable(&unk_82F30008, "Checking", "After slam max first ram strength");
        RegisterVariable(&unk_82F3000C, "Checking", "After check first ram decay");
        RegisterVariable(&unk_82F30010, "Checking", "After check min swerve time (seconds)");
        RegisterVariable(&unk_82F30014, "Checking", "After check max swerve time (seconds)");
        RegisterVariable(&unk_82F30018, "Checking", "After check flipout spin min ram strength");
        RegisterVariable(&unk_82F3001C, "Checking", "After check flipout spin max ram strength");
        RegisterVariable(&unk_82F30020, "Checking", "After check flipout flip min ram strength");
        RegisterVariable(&unk_82F30024, "Checking", "After check flipout flip max ram strength");
        RegisterVariable(&unk_82F30028, "Checking", "Racecar kick strength");
        SetStep(&kfSideRamStrength, 0.01f);
        SetStep(&kfSideRamProportion, 0.05f);
        SetStep(&unk_8300D018, 0.05f);
        SetStep(&unk_82F2FFFC, 0.05f);
        SetStep(&unk_82F30000, 0.05f);
        SetStep(&unk_82F30004, 0.05f);
        SetStep(&unk_82F30008, 0.05f);
        SetStep(&unk_82F3000C, 0.05f);
        SetStep(&unk_82F30010, 0.1f);
        SetStep(&unk_82F30014, 0.1f);
        SetStep(&unk_82F30018, 0.01f);
        SetStep(&unk_82F3001C, 0.01f);
        SetStep(&unk_82F30020, 0.01f);
        SetStep(&unk_82F30024, 0.01f);

        RegisterVariable(&unk_82F30030, "Traffic Jams", "Slow driving speed");
        SetStep(&unk_82F30030, 0.5f);
        RegisterVariable(&unk_82F30034, "Traffic Jams", "Max slow driving time");
        SetStep(&unk_82F30034, 0.5f);
        RegisterVariable(&unk_82F30038, "Traffic Jams", "Max time in one hull");
        SetStep(&unk_82F30038, 0.5f);
        RegisterVariable(&mpModule->mfTimeSincePlayerWasDrivingQuickly,
                         "Traffic Jams", "Player slow driving time");
        SetReadOnly(&mpModule->mfTimeSincePlayerWasDrivingQuickly, true);
        RegisterVariable(&mpModule->mfTimeSincePlayerHullChange,
                         "Traffic Jams", "Player in same hull time");
        SetReadOnly(&mpModule->mfTimeSincePlayerHullChange, true);
        RegisterVariable(&mpModule->mbDEBUGEnablePressureSystem,
                         "Traffic Jams", "Enable pressure system");

        RegisterVariable(&mfRenderCullDistance, "DRAWING", "Cull distance");
        SetStep(&mfRenderCullDistance, 20.0f);
        RegisterVariable(&mbDrawSegments, "DRAWING", "Render Segments");
        RegisterVariable(&mbDrawSections, "DRAWING", "Render Sections");
        RegisterVariable(&mbDrawSectionCurves, "DRAWING", "Render Section Curves");
        RegisterVariable(&mbDrawSplitters, "DRAWING", "Render Splitters");
        RegisterVariable(&mbDrawNeighbours, "DRAWING", "Render Neighbours");
        RegisterVariable(&mbDrawJunctions, "DRAWING", "Render Junctions");
        RegisterVariable(&mbDrawStopLines, "DRAWING", "Render Stop Lines");
        RegisterVariable(&mbDrawStartLines, "DRAWING", "Render Start Grids");
        RegisterVariable(&mbDrawParams, "DRAWING", "Render Params");
        RegisterVariable(&mbDrawLerpedParams, "DRAWING", "Render lerped param positions");
        RegisterVariable(&mbDrawParamBehaviours, "DRAWING", "Render param behaviours");
        RegisterVariable(&mbDrawParamSlowData, "DRAWING", "Render param slow data");
        RegisterVariable(&mpModule->mFuzzyBehaviours.mbDEBUGRenderParamScores,
                         "DRAWING", "Render param fuzzy scores");
        RegisterVariable(&mbDrawParamTrails, "DRAWING", "Render param trails");
        RegisterVariable(&mbDrawVehicles, "DRAWING", "Render Vehicles");
        RegisterVariable(&mbDrawNonCollidableVehicles, "DRAWING", "Render non-collidable vehicles");
        RegisterVariable(&mbDrawCollidableVehicles, "DRAWING", "Render collidable vehicles");
        RegisterVariable(&mbDrawPhysicalVehicleState, "DRAWING", "Render phys vehicle state");
        RegisterVariable(&mbDrawVehicleManoeuvres, "DRAWING", "Render vehicle manoeuvres");
        RegisterVariable(&mpModule->mbDEBUGDontRenderMeshes, "DRAWING", "Hide Vehicle Meshes");
        RegisterVariable(&mbDrawParamVehicleLinks, "DRAWING", "Render Param-Vehicle Links");
        RegisterVariable(&mbDrawParamParamLinks, "DRAWING", "Render Param-Param Links");

        RegisterVariable(&mbDrawVehicleIds, "DRAWING 2", "Show Vehicle IDs");
        RegisterVariable(&mbDrawNearestVehicleId, "DRAWING 2", "Show Nearest Vehicle ID");
        RegisterVariable(&mbDrawAllKillZones, "DRAWING 2", "Render ALL kill zones");
        RegisterVariable(&mbDrawRecentKillZones, "DRAWING 2", "Render recent kill zones");
        RegisterVariable(&mbDrawActiveHulls, "DRAWING 2", "Draw active hulls");
        RegisterVariable(&mbDrawPlayerHullId, "DRAWING 2", "Draw player hull id");
        RegisterVariable(&miHullToDrawFor, "DRAWING 2", "Hull to draw for (-1 is all)");
        SetRange(&miHullToDrawFor, -1, mpModule->mpData->muNumHulls);
        RegisterVariable(&mbDrawFrontArticPoints, "DRAWING 2", "Draw front artic points");
        RegisterVariable(&mbDrawRearArticPoints, "DRAWING 2", "Draw rear artic points");
        RegisterVariable(&mbDrawAxles, "DRAWING 2", "Draw axles");
        RegisterVariable(&mbDrawHorns, "DRAWING 2", "Draw horns");
        RegisterVariable(&mbDrawAvoidance, "DRAWING 2", "Draw AI Avoidance");
        RegisterVariable(&mbDrawAvoidanceAllShit, "DRAWING 2", "Draw AI Avoidance BrainPain");
        RegisterVariable(&mbDrawPressure, "DRAWING 2", "Draw Pressure Stats");
        RegisterVariable(&mpModule->mbDEBUGShowtimeStuff, "DRAWING 2", "Showtime Stuff");
        RegisterVariable(&mpModule->mbDEBUGRenderContacts, "DRAWING 2", "Contact Points");
        RegisterVariable(&mbDrawStuckDetection, "DRAWING 2", "Render Stuck detection");

        RegisterVariable(&mbUseCameraPosition, "Use camera position");
        RegisterVariable(&mpModule->mbDEBUGStopTrafficMoving, "Stop traffic moving");
        RegisterVariable(&mpModule->mbDEBUGAllowAnarchy, "", "Allow ANARCHY!");
        RegisterVariable(&mpModule->mfDEBUGDistanceForAnarchy, "", "Distance for ANARCHY!");
        SetRange(&mpModule->mfDEBUGDistanceForAnarchy, 1.0f, 1000.0f);
        SetStep(&mpModule->mfDEBUGDistanceForAnarchy, 5.0f);
        RegisterVariable(&mpModule->mfSpeedMultiplier, "", "Global speed multiplier");
        SetRange(&mpModule->mfSpeedMultiplier, 0.0f, 50.0f);
        SetStep(&mpModule->mfSpeedMultiplier, 0.05f);
        RegisterVariable(&mpModule->mfDEBUGTrafficLightTimeMultiplier,
                         "", "Global traffic light time multiplier");
        SetRange(&mpModule->mfDEBUGTrafficLightTimeMultiplier, 0.0f, 50.0f);
        SetStep(&mpModule->mfDEBUGTrafficLightTimeMultiplier, 0.05f);
        RegisterVariable(&mpModule->mfGameModeDensityScale, "Game mode density scale");
        SetRange(&mpModule->mfGameModeDensityScale, 0.0f, 16.0f);
        SetStep(&mpModule->mfGameModeDensityScale, 0.05f);
        RegisterVariable(&mpModule->mfTrafficAmountScale, "FINAL density scale");
        SetReadOnly(&mpModule->mfTrafficAmountScale, true);
        RegisterVariable(&mpModule->miBigVehicleAmount, "Big vehicle amount");
        SetRange(&mpModule->miBigVehicleAmount, 0, 100);
        SetStep(&mpModule->miBigVehicleAmount, 10);
        RegisterVariable(&mpModule->miDEBUGOverrideVehicleToSpawn, "", "Vehicle to spawn");

        // X360 call 0x82763178 receives this+0x38 again, not the vehicle type selector
        // registered immediately before it. Preserve that original range binding.
        SetRange(&miHullToDrawFor, -1, mpModule->mpData->muNumVehicleTypes);
        RegisterVariable(&muDummyVehicleName, "", macOverrideVehicleName);
        SetReadOnly(&muDummyVehicleName, true);
        RegisterVariable(&mpModule->miDEBUGFlowtypeOverride, "", "Flowtype override");
        SetRange(&mpModule->miDEBUGFlowtypeOverride, -1,
                 static_cast<s32>(mpModule->mpData->muNumFlowTypes) - 1);
        RegisterVariable(&mbDrawVehicleSwervingDebug, "", "Show vehicle swerving debug");

        RegisterVariable(&mpModule->mbDEBUGEnableKillzones, "KillZones", "Enable Killzones");
        RegisterVariable(&muKillZoneToFire, "KillZones", "KillZone to fire");
        SetRange(&muKillZoneToFire, 0u, static_cast<u32>(mpModule->mpData->muNumKillZones) - 1u);
        RegisterFunction(&DebugComponent::FireKillZone, this, "KillZones", "Fire KillZone!");

        RegisterVariable(&mpModule->mbDEBUGRunningWorstCase, "Performance", "Running Worst Case");
        RegisterVariable(&mpModule->muNumUpdateVehiclesJobs, "Performance", "# UpdateVehicles jobs");
        SetRange(&mpModule->muNumUpdateVehiclesJobs, 1u, 4u);
        RegisterVariable(&mpModule->mbDEBUGRenderSpecialPerfmons,
                         "Performance", "Render special perfmons");
        RegisterVariable(&mpModule->mbDEBUGEnableAvoidance, "Enable Avoidance");
        RegisterVariable(&mpModule->mbDEBUGTestSympCrash, "Test Sympathetic Crashing");
        RegisterVariable(&mpModule->mbDEBUGFakeShowtime, "Fake Showtime");
        RegisterFunction(&DebugComponent::OnKillNearPlayer, this, "Kill traffic near player");
        RegisterFunction(&DebugComponent::ReloadBehaviourData, this, "Reload Behaviour Data");

        RegisterVariable(&mpModule->mfCrashSliderCrashScoreFactor, "Crash Control", "Overall Slider");
        SetRange(&mpModule->mfCrashSliderCrashScoreFactor, 0.0f, 50.0f);
        SetStep(&mpModule->mfCrashSliderCrashScoreFactor, 0.1f);
        RegisterVariable(&mpModule->mfCrashSliderCrashScoreDecay, "Crash Control", "Crash score Decay Slider");
        SetRange(&mpModule->mfCrashSliderCrashScoreDecay, 0.0f, 1.0f);
        SetStep(&mpModule->mfCrashSliderCrashScoreDecay, 0.1f);
        RegisterVariable(&mpModule->mfCrashSliderCrashScore, "Crash Control", "Crash score");
        SetReadOnly(&mpModule->mfCrashSliderCrashScore, true);
        RegisterVariable(&mpModule->mfCrashSliderFinalValue, "Crash Control", "Final value");
        SetReadOnly(&mpModule->mfCrashSliderFinalValue, true);
        RegisterVariable(&mpModule->mfPlayerIdleTime, "Crash Control", "Player Idle Time");
        SetReadOnly(&mpModule->mfPlayerIdleTime, true);
        RegisterVariable(&mpModule->mfShowtimeTimer, "Crash Control", "Showtime Timer");
        SetReadOnly(&mpModule->mfShowtimeTimer, true);
        RegisterVariable(&mpModule->mfShowtimeTimeNextCrashSpike,
                         "Crash Control", "Showtime Next Spike");
        SetReadOnly(&mpModule->mfShowtimeTimeNextCrashSpike, true);
        RegisterVariable(&mpModule->mfShowtimeMisBounceTimer,
                         "Crash Control", "Showtime Mis Bounce Time");
        SetReadOnly(&mpModule->mfShowtimeMisBounceTimer, true);

        RegisterVariable(&mbDrawJunctionFUPDetection, "Junction FUP Detection", "Draw Detection");
        RegisterVariable(&mpModule->mfJunctionFUP, "Junction FUP Detection", "Junction FUP");
        SetReadOnly(&mpModule->mfJunctionFUP, true);
        RegisterVariable(&mpModule->mbDEBUGOverrideJunctionFUP,
                         "Junction FUP Detection", "Fake it!");
        RegisterVariable(&mpModule->mbDEBUGPickVehicleFromCamera, "Picking", "Enable Picking");
        RegisterFunction(&DebugComponent::OnPickVehicle, this, "Picking", "Pick vehicle");
        RegisterVariable(&mpModule->mbDEBUGPick_StopVehicle, "Picking", "Stop vehicle");
        RegisterVariable(&mpModule->mbDEBUGPick_DontStopForPickedVehicle,
                         "Picking", "Others - Dont stop for vehicle");
    }

    // @0x827584F0. Refresh the four menu counters and the displayed spawn-vehicle asset name.
    void DebugComponent::UpdateStats()
    {
        miNumPurgatoryParams = static_cast<s32>(mpModule->maPurgatoryList.GetLength());
        miNumFreeParams = static_cast<s32>(mpModule->mFreeParams.GetLength());
        miNumActiveParams = static_cast<s32>(KU_MAX_PARAMS) - miNumFreeParams - miNumPurgatoryParams;
        miNumActiveHulls = static_cast<s32>(mpModule->mActiveHulls.GetLength());

        if (mpModule->miDEBUGOverrideVehicleToSpawn == -1)
        {
            strcpy(macOverrideVehicleName, "DEFAULT");
        }
        else
        {
            const u32 luVehicleType = static_cast<u32>(mpModule->miDEBUGOverrideVehicleToSpawn);
            const u32 luAsset = mpModule->mpData->mpaVehicleTypes[luVehicleType].muAssetId;
            CgsIDUnCompress(mpModule->mpData->mpaVehicleAssets[luAsset].GetVehicleId(),
                            macOverrideVehicleName);
        }
    }

    // @0x82750508. The action copies the selected component type into the module's fire latch.
    void DebugComponent::FireAirRam(void* lpUserData)
    {
        DebugComponent* const lpComponent = static_cast<DebugComponent*>(lpUserData);
        CGS_ASSERT(lpComponent != 0, "lpComponent");
        lpComponent->mpModule->meDEBUGAirRamToFire = lpComponent->miAirRamTypeToFire;
    }

    // @0x82750748. Only fire the clear when the local-player cache is valid.
    void DebugComponent::OnKillNearPlayer(void* lpUserData)
    {
        DebugComponent* const lpComponent = static_cast<DebugComponent*>(lpUserData);
        TrafficEntityModule* const lpModule = lpComponent->mpModule;
        if (lpModule->meLocalPlayerIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID)
        {
            lpModule->KillAllTrafficInCylinder(lpModule->mLocalPlayerPosition,
                                               150.0f, 1000.0f, false);
        }
    }

    // @0x8275BB40 (the entry has no independent named export). The row range is the only
    // original bounds metadata; the callback itself indexes the kill-zone id table directly.
    void DebugComponent::FireKillZone(void* lpUserData)
    {
        DebugComponent* const lpComponent = static_cast<DebugComponent*>(lpUserData);
        CGS_ASSERT(lpComponent != 0, "lpComponent");
        const u64 luKillZoneId = lpComponent->mpModule->mpData->mpaKillZoneIds[
            lpComponent->muKillZoneToFire];
        lpComponent->mpModule->FireKillZone(luKillZoneId);
    }

    // @0x82750788. Copy the component's selected vehicle id to the module's pick latch.
    void DebugComponent::OnPickVehicle(void* lpUserData)
    {
        DebugComponent* const lpComponent = static_cast<DebugComponent*>(lpUserData);
        lpComponent->mpModule->muDEBUGPickedVehicle = lpComponent->muCurrentVehicleToPick;
    }

    // @0x827614D8. Reload the module's current fuzzy behavior resource.
    void DebugComponent::ReloadBehaviourData(void* lpUserData)
    {
        DebugComponent* const lpComponent = static_cast<DebugComponent*>(lpUserData);
        CGS_ASSERT(lpComponent != 0, "lpComponent");
        lpComponent->mpModule->mFuzzyBehaviours.ReloadBehaviours();
    }

    // @0x82750000. The first-left and last-right rung endpoints are tested through
    // the now named Section/LaneRung types; a zero-rung section keeps the original
    // unsigned underflow behavior and receives no invented guard.
    bool DebugComponent::ShouldSectionBeCulled(const Section* lpSection, const Hull* lpHull,
                                               CgsDev::Debug3DImmediateRender* lpDisplay) const
    {
        CGS_ASSERT(lpSection != 0, "lpSection");
        CGS_ASSERT(lpHull != 0, "lpHull");
        CGS_ASSERT(lpDisplay != 0, "lpDisplay");
        if (ShouldObjectBeCulled(lpHull->mpaRungs[lpSection->muRungOffset].maPoints[0], lpDisplay))
            return ShouldObjectBeCulled(
                lpHull->mpaRungs[lpSection->muRungOffset + lpSection->muNumRungs - 1].maPoints[1], lpDisplay);
        return false;
    }
}

namespace BrnTraffic
{
    namespace
    {
        // Stop-line colours: rw::RGBA(255, 0, 0, 255) for a red light, (0, 255, 0, 255) for green
        // (the image words 0xFF0000FF / 0xFF00FF00).
        const rw::RGBA KCOLOUR_STOPLINE_RED(255, 0, 0, 255);
        const rw::RGBA KCOLOUR_STOPLINE_GREEN(0, 255, 0, 255);

        // Stop lines are lifted this far off the road so they do not z-fight with it.
        const f32 KF_STOPLINE_DRAW_HEIGHT = 0.05f;

        // DrawPressure lays the span table out in columns of 40 lines, 100 apart.
        const s32 KI_PRESSURE_LINES_PER_COLUMN = 40;
        const f32 KF_PRESSURE_COLUMN_WIDTH     = 100.0f;

        // The rsqrt-refined unit vector DrawOffsetArrow builds; it has no zero-length guard, so a
        // zero input yields the same non-finite lanes the console's estimate would.
        Vector3 UnitVector(Vector3 lVector)
        {
            return lVector * (1.0f / std::sqrt(rw::math::vpu::Dot(lVector, lVector)));
        }
    }

    // The occupancy of every section span in the hull the first race car is in, one line per
    // span ("span : vehicles / budget  percent%"), the percentage being the integer quotient of
    // vehicles over budget times 100 (1000 for a span with no budget).
    void DebugComponent::DrawPressure(CgsDev::Debug3DImmediateRender* /*lpDisplay*/) const
    {
        CgsDev::DebugInterface lDebugInterface;
        CgsDev::DebugRender& lrRender = lDebugInterface.Get2dRender();

        char lacText[5120];
        CgsDev::StrStream lStream(lacText, sizeof(lacText));

        const ::Array<u16, KU_MAX_ACTIVE_HULLS_PER_RACECAR>& lrRaceCarHulls = mpModule->maaRaceCarHulls[0];
        if (lrRaceCarHulls.GetLength() < 1)
        {
            return;
        }

        const u16 luHull = lrRaceCarHulls.GetItem(0);
        const Hull* lpHull = mpModule->GetHull(luHull);
        const HullRuntime* lpHullRuntime = mpModule->GetHullRuntimeSafe(luHull);
        if (lpHullRuntime == 0)
        {
            return;
        }

        lStream << "Hull : " << static_cast<s32>(luHull)
                << "\nSection Spans : " << static_cast<s32>(lpHull->muNumSectionSpans)
                << "\n--------------------------";
        f32 lfX = 50.0f;
        DrawTextWithOffsets(lrRender, &lStream, lfX, 30.0f);
        lStream.Reset();

        s32 liLinesInColumn = 0;
        for (u32 luSpan = 0; luSpan < lpHull->muNumSectionSpans; ++luSpan)
        {
            if (liLinesInColumn >= KI_PRESSURE_LINES_PER_COLUMN)
            {
                DrawTextWithOffsets(lrRender, &lStream, lfX, 75.0f);
                liLinesInColumn = 0;
                lfX += KF_PRESSURE_COLUMN_WIDTH;
            }

            const u16 luMaxVehicles = lpHull->GetSectionSpan(luSpan)->GetMaxVehicleCount();
            const u16 luVehicles    = lpHullRuntime->GetSectionSpanVehicleCount(luSpan);

            f32 lfPercentage;
            if (luMaxVehicles != 0)
            {
                lfPercentage = static_cast<f32>(luVehicles / luMaxVehicles) * 100.0f;
            }
            else
            {
                lfPercentage = 1000.0f;
            }

            char lacLine[512];
            CgsCore::SPrintf(lacLine, sizeof(lacLine), "%3d : %3d / %3d  %6d%%\n", luSpan,
                             static_cast<u32>(luVehicles), static_cast<u32>(luMaxVehicles),
                             static_cast<s32>(lfPercentage));
            lStream << lacLine;

            ++liLinesInColumn;
        }

        if (lacText[0] != '\0')
        {
            DrawTextWithOffsets(lrRender, &lStream, lfX, 75.0f);
        }
    }

    // Every stop line of every section in the hull, red or green by its light, culled by its
    // start point and drawn slightly above the road.
    void DebugComponent::DrawStopLinesInHull(s32 liHull, CgsDev::Debug3DImmediateRender* lpDisplay) const
    {
        const Hull* lpHull = mpModule->GetHull(liHull);
        const HullRuntime* lpHullRuntime = mpModule->GetHullRuntime(liHull);

        for (u32 luSection = 0; luSection < lpHull->muNumSections; ++luSection)
        {
            const Section& lrSection = lpHull->mpaSections[luSection];

            for (u32 luStopLine = 0; luStopLine < lrSection.muNumStopLines; ++luStopLine)
            {
                const rw::RGBA lColour = lpHullRuntime->IsStoplineRed(lrSection.muStopLineOffset + luStopLine)
                                             ? KCOLOUR_STOPLINE_RED
                                             : KCOLOUR_STOPLINE_GREEN;

                Vector3 lStart;
                Vector3 lEnd;
                GetStopLinePosition(static_cast<u16>(liHull), static_cast<u8>(luSection),
                                    static_cast<u8>(luStopLine), lStart, lEnd);

                if (!ShouldObjectBeCulled(lStart, lpDisplay))
                {
                    lStart.y += KF_STOPLINE_DRAW_HEIGHT;
                    lEnd.y   += KF_STOPLINE_DRAW_HEIGHT;
                    lpDisplay->DrawLine(lStart, lEnd, lColour);
                }
            }
        }
    }

    // DrawArrow shifted sideways by lfOffset along the horizontal perpendicular of the arrow
    // (world up crossed with its direction), so two opposing links draw side by side.
    void DebugComponent::DrawOffsetArrow(Vector3 lFrom, Vector3 lTo, f32 lfOffset, rw::RGBA lColour,
                                         CgsDev::Debug3DImmediateRender* lpDisplay) const
    {
        const Vector3 lUp        = { 0.0f, 1.0f, 0.0f, 0.0f };
        const Vector3 lDirection = UnitVector(lTo - lFrom);
        const Vector3 lSide      = UnitVector(rw::math::vpu::Cross(lUp, lDirection));
        const Vector3 lOffset    = lSide * lfOffset;

        DrawArrow(lFrom + lOffset, lTo + lOffset, lColour, lpDisplay);
    }
}

// Complete independent Traffic geometry family. Integration requires existing
// Traffic Hull/Section/Junction/LightTrigger types and vector3_operation.h.
// ShouldSectionBeCulled REPLACES the existing raw-offset body; do not append twice.
namespace BrnTraffic
{

// ARTIST 0x8274FF38..0x8274FFF8 (raw image recovery).
bool DebugComponent::ShouldObjectBeCulled(Vector3 lPosition,
                                         CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    CGS_ASSERT(lpDisplay != 0, "lpDisplay");
    const Vector3 lOffset = lPosition - lpDisplay->GetCameraPosition();
    f32 lfCullSquared = mfRenderCullDistance * mfRenderCullDistance;
    // The scalar square is compared in VMX mode, where subnormal operands are zero.
    if (std::fpclassify(lfCullSquared) == FP_SUBNORMAL)
        lfCullSquared = 0.0f;
    // Model vmsum3fp128 with a wide sum; scalar Dot rounds each product.
    // Reuse the existing VMX dot model, including overflow and denormal results.
    // Original negates (threshold > squared-distance): equality and NaN cull.
    return !(lfCullSquared > rw::math::vpu::SLerpDetail::Dot3(lOffset, lOffset));
}

// ARTIST 0x82758670. Unlike the following wrappers, this ignores miHullToDrawFor.
void DebugComponent::DrawSegments(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    CGS_ASSERT(mpModule != 0, "mpModule != NULL");
    const rw::RGBA lColour(0, 255, 255, 255); // 0xFFFFFF00
    for (u32 luActive = 0; luActive < mpModule->mActiveHulls.GetLength(); ++luActive)
    {
        const Hull* lpHull = mpModule->GetHull(mpModule->mActiveHulls.GetItem(luActive));
        for (u32 luSection = 0; luSection < lpHull->muNumSections; ++luSection)
        {
            const Section* lpSection = &lpHull->mpaSections[luSection];
            if (ShouldSectionBeCulled(lpSection, lpHull, lpDisplay))
                continue;
            for (u32 luSegment = 0; luSegment + 1 < lpSection->muNumRungs; ++luSegment)
            {
                const LaneRung& lrA = lpHull->mpaRungs[lpSection->muRungOffset + luSegment];
                const LaneRung& lrB = lpHull->mpaRungs[lpSection->muRungOffset + luSegment + 1];
                lpDisplay->DrawQuad(lrA.maPoints[0], lrA.maPoints[1], lrB.maPoints[1], lrB.maPoints[0], lColour);
            }
        }
    }
}

// ARTIST 0x82760B68.
void DebugComponent::DrawSections(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    CGS_ASSERT(mpModule != 0, "mpModule != NULL");
    if (miHullToDrawFor != -1)
        DrawSectionsInHull(miHullToDrawFor, lpDisplay);
    else
        for (u32 luActive = 0; luActive < mpModule->mActiveHulls.GetLength(); ++luActive)
            DrawSectionsInHull(mpModule->mActiveHulls.GetItem(luActive), lpDisplay);
}

// ARTIST 0x82758888.
void DebugComponent::DrawSectionsInHull(s32 liHull, CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    const Hull* lpHull = mpModule->GetHull(liHull);
    const Vector3 lLift = {0.0f, 0.1f, 0.0f, 0.0f};
    const rw::RGBA lColour(255, 255, 0, 255); // 0xFF00FFFF
    for (u32 luSection = 0; luSection < lpHull->muNumSections; ++luSection)
    {
        const Section* lpSection = &lpHull->mpaSections[luSection];
        if (ShouldSectionBeCulled(lpSection, lpHull, lpDisplay))
            continue;
        const LaneRung* lpaRungs = lpHull->mpaRungs + lpSection->muRungOffset;
        for (u32 luSegment = 0; luSegment + 1 < lpSection->muNumRungs; ++luSegment)
        {
            lpDisplay->DrawLine(lpaRungs[luSegment].maPoints[0] + lLift,
                                lpaRungs[luSegment + 1].maPoints[0] + lLift, lColour);
            lpDisplay->DrawLine(lpaRungs[luSegment].maPoints[1] + lLift,
                                lpaRungs[luSegment + 1].maPoints[1] + lLift, lColour);
        }
        lpDisplay->DrawLine(lpaRungs[0].maPoints[0] + lLift, lpaRungs[0].maPoints[1] + lLift, lColour);
        const LaneRung& lrLast = lpaRungs[lpSection->muNumRungs - 1];
        lpDisplay->DrawLine(lrLast.maPoints[0] + lLift, lrLast.maPoints[1] + lLift, lColour);
    }
}

// ARTIST 0x82760C98.
void DebugComponent::DrawSectionCurves(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    CGS_ASSERT(mpModule != 0, "mpModule");
    CGS_ASSERT(lpDisplay != 0, "lpDisplay");
    if (miHullToDrawFor != -1)
        DrawSectionCurvesInHull(miHullToDrawFor, lpDisplay);
    else
        for (u32 luActive = 0; luActive < mpModule->mActiveHulls.GetLength(); ++luActive)
            DrawSectionCurvesInHull(mpModule->mActiveHulls.GetItem(luActive), lpDisplay);
}

// ARTIST 0x82758A18.
void DebugComponent::DrawSectionCurvesInHull(s32 liHull, CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    const Hull* lpHull = mpModule->GetHull(liHull);
    CGS_ASSERT(lpHull != 0, "lpHull");
    const rw::RGBA lCurveColour(0, 127, 255, 255); // 0xFFFF7F00
    const rw::RGBA lDirectionColour(80, 150, 255, 255); // 0xFFFF9650
    const VecFloat lHalf = {0.5f, 0.5f, 0.5f, 0.5f};
    for (u32 luSection = 0; luSection < lpHull->muNumSections; ++luSection)
    {
        const Section* lpSection = &lpHull->mpaSections[luSection];
        if (ShouldSectionBeCulled(lpSection, lpHull, lpDisplay))
            continue;
        for (f32 lfParam = 0.0f; ; lfParam += 0.125f)
        {
            const f32 lfNext = lfParam + 0.125f;
            if (lfNext >= static_cast<f32>(lpSection->GetNumSegments()))
                break;
            const VecFloat lParam = {lfParam, lfParam, lfParam, lfParam};
            const VecFloat lNext = {lfNext, lfNext, lfNext, lfNext};
            Vector3 lA, lB, lDirection;
            const u32 luSegment = static_cast<u32>(lfParam);
            lpSection->CalcPositionAtParameter(lpHull->mpaRungs, lParam, luSegment, lA);
            lpSection->CalcPositionAtParameter(lpHull->mpaRungs, lNext, static_cast<u32>(lfNext), lB);
            lA.y += 0.2f;
            lB.y += 0.2f;
            lpDisplay->DrawLine(lA, lB, lCurveColour);
            lpSection->CalcDirectionAtParameter(lpHull->mpaRungs, lParam, luSegment, lDirection);
            lpDisplay->DrawLine(lA, TrafficMultAdd(lDirection, lHalf, lA), lDirectionColour);
        }
    }
}

// ARTIST 0x82760DF0.
void DebugComponent::DrawJunctions(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    CGS_ASSERT(mpModule != 0, "mpModule != NULL");
    if (miHullToDrawFor != -1)
        DrawJunctionsInHull(miHullToDrawFor, lpDisplay);
    else
        for (u32 luActive = 0; luActive < mpModule->mActiveHulls.GetLength(); ++luActive)
            DrawJunctionsInHull(mpModule->mActiveHulls.GetItem(luActive), lpDisplay);
}

// ARTIST 0x82759068.
void DebugComponent::DrawJunctionsInHull(s32 liHull, CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    const Hull* lpHull = mpModule->GetHull(liHull);
    const Vector3 lMin = {-1.0f, -1.0f, -1.0f, 0.0f};
    const Vector3 lMax = {1.0f, 1.0f, 1.0f, 0.0f};
    const rw::RGBA lColour(80, 80, 150, 255); // 0xFF965050
    for (u32 luJunction = 0; luJunction < lpHull->muNumJunctions; ++luJunction)
    {
        const JunctionLogicBox* lpJunction = &lpHull->mpaJunctions[luJunction];
        const Vector3 lPosition = lpJunction->GetPosition();
        if (ShouldObjectBeCulled(lPosition, lpDisplay))
            continue;
        Matrix44Affine lTransform;
        lTransform.SetIdentity();
        lTransform.Pos() = lPosition + Vector3{0.0f, 4.0f, 0.0f, 0.0f};
        lpDisplay->DrawBox(lMin, lMax, lTransform, lColour);
        char lacText[1024];
        CgsCore::SPrintf(lacText, sizeof(lacText), "Event Junction ID: %u", lpJunction->GetEventJunctionID());
        lpDisplay->DrawText(lTransform.Pos() + Vector3{0.0f, 2.0f, 0.0f, 0.0f}, lacText, 16.0f);
    }
}

// ARTIST 0x827636A8.
void DebugComponent::DrawStopLines(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    CGS_ASSERT(mpModule != 0, "mpModule != NULL");
    if (miHullToDrawFor != -1)
        DrawStopLinesInHull(miHullToDrawFor, lpDisplay);
    else
        for (u32 luActive = 0; luActive < mpModule->mActiveHulls.GetLength(); ++luActive)
            DrawStopLinesInHull(mpModule->mActiveHulls.GetItem(luActive), lpDisplay);
}

// ARTIST export hole 0x827595A0. r4=hull, r5=section, r6=local stop index.
void DebugComponent::GetStopLinePosition(u16 luHull, u8 luSection, u8 luStopLine,
                                       Vector3& lrStart, Vector3& lrEnd) const
{
    const Hull* lpHull = mpModule->GetHull(luHull);
    const Section* lpSection = &lpHull->mpaSections[luSection];
    const StopLine* lpStop = lpHull->GetStopLine(lpSection->muStopLineOffset + luStopLine);
    const f32 lfParam = StopLine::ConvertToFloat(lpStop->GetParameterAlongSection());
    const u32 luRung = static_cast<u32>(lfParam);
    CGS_ASSERT(luRung + 1 < lpSection->muNumRungs, "( luSegment + 1 ) < lpSection->muNumRungs");
    const f32 lfFraction = lfParam - std::floor(lfParam);
    const VecFloat lFraction = {lfFraction, lfFraction, lfFraction, lfFraction};
    const LaneRung& lrA = lpHull->mpaRungs[lpSection->muRungOffset + luRung];
    const LaneRung& lrB = lpHull->mpaRungs[lpSection->muRungOffset + luRung + 1];
    lrStart = TrafficMultAdd(lrB.maPoints[0] - lrA.maPoints[0], lFraction, lrA.maPoints[0]);
    lrEnd = TrafficMultAdd(lrB.maPoints[1] - lrA.maPoints[1], lFraction, lrA.maPoints[1]);
}

// ARTIST 0x827592B0. This also ignores the inspected-hull selector.
void DebugComponent::DrawStartLines(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    CGS_ASSERT(mpModule != 0, "mpModule != NULL");
    const Vector3 lLift = {0.0f, 0.1f, 0.0f, 0.0f};
    const VecFloat lHalf = {0.5f, 0.5f, 0.5f, 0.5f};
    const rw::RGBA lColour(200, 30, 200, 255); // 0xFFC81EC8
    for (u32 luActive = 0; luActive < mpModule->mActiveHulls.GetLength(); ++luActive)
    {
        const Hull* lpHull = mpModule->GetHull(mpModule->mActiveHulls.GetItem(luActive));
        // Original loop uses muNumLightTriggers, not the start-data count.
        for (u32 luStart = 0; luStart < lpHull->muNumLightTriggers; ++luStart)
        {
            const LightTriggerStartData* lpStart = &lpHull->mpaLightTriggerStartData[luStart];
            if (lpStart->GetNumStartPositions() == 0 || ShouldObjectBeCulled(lpStart->GetStartPosition(0), lpDisplay))
                continue;
            for (u32 luPosition = 0; luPosition < lpStart->GetNumStartPositions(); ++luPosition)
            {
                const Vector3 lDirection = lpStart->GetStartDirection(luPosition);
                const Vector3 lPosition = lpStart->GetStartPosition(luPosition);
                const Vector3 lEnd = TrafficMultAdd(lDirection, lHalf, lPosition);
                DrawArrow(lpStart->GetStartPosition(luPosition) + lLift, lEnd + lLift, lColour, lpDisplay);
            }
        }
    }
}

// ARTIST 0x82758CC0. Neighbours remain in the same hull; each entry's rung
// mapping selects the two lane midpoints at the start of the shared interval.
void DebugComponent::DrawNeighbours(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    CGS_ASSERT(mpModule != 0, "mpModule != NULL");
    const VecFloat lHalf = {0.5f, 0.5f, 0.5f, 0.5f};
    const rw::RGBA laColours[2] = {rw::RGBA(200, 0, 0, 255), rw::RGBA(0, 200, 0, 255)};
    for (u32 luActive = 0; luActive < mpModule->mActiveHulls.GetLength(); ++luActive)
    {
        const Hull* lpHull = mpModule->GetHull(mpModule->mActiveHulls.GetItem(luActive));
        for (u32 luSection = 0; luSection < lpHull->muNumSections; ++luSection)
        {
            const Section* lpSection = &lpHull->mpaSections[luSection];
            if (ShouldSectionBeCulled(lpSection, lpHull, lpDisplay))
                continue;
            const Neighbour* lpNeighbour = lpHull->mpaNeighbourData + lpSection->muNeighbourOffset;
            f32 lfOffset = -1.5f;
            Vector3 lLift = {0.0f, 0.2f, 0.0f, 0.0f};
            for (u32 luSide = 0; luSide < 2; ++luSide)
            {
                const u32 luCount = luSide == 0 ? lpSection->muLeftNeighbourCount : lpSection->muRightNeighbourCount;
                for (u32 luNeighbour = 0; luNeighbour < luCount; ++luNeighbour, ++lpNeighbour)
                {
                    const LaneRung& lrOurRung =
                        lpHull->mpaRungs[lpSection->muRungOffset + lpNeighbour->muOurStartRung];
                    const Section& lrTheirSection = lpHull->mpaSections[lpNeighbour->muSection];
                    const LaneRung& lrTheirRung =
                        lpHull->mpaRungs[lrTheirSection.muRungOffset + lpNeighbour->muTheirStartRung];
                    const Vector3 lOurRight = lrOurRung.GetRightVector();
                    const Vector3 lTheirRight = lrTheirRung.GetRightVector();
                    Vector3 lFrom = TrafficMultAdd(lrOurRung.maPoints[1] - lrOurRung.maPoints[0],
                                                         lHalf, lrOurRung.maPoints[0]);
                    Vector3 lTo = TrafficMultAdd(lrTheirRung.maPoints[1] - lrTheirRung.maPoints[0],
                                                       lHalf, lrTheirRung.maPoints[0]);
                    const VecFloat lOffset = {lfOffset, lfOffset, lfOffset, lfOffset};
                    const VecFloat lOtherOffset = {-lfOffset, -lfOffset, -lfOffset, -lfOffset};
                    lFrom = TrafficMultAdd(lOurRight, lOffset, lFrom);
                    lTo = TrafficMultAdd(lTheirRight, lOtherOffset, lTo);
                    DrawArrow(lFrom + lLift, lTo + lLift, laColours[luSide], lpDisplay);
                }
                lfOffset = -lfOffset;
                lLift.y += 0.05f;
            }
        }
    }
}

} // namespace BrnTraffic

namespace BrnTraffic
{

// ARTIST export hole 0x827610D0.
void DebugComponent::DrawAllKillZones(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    CGS_ASSERT(mpModule != 0, "mpModule");
    for (u32 luZone = 0; luZone < mpModule->mpData->muNumKillZones; ++luZone)
        DrawKillZone(&mpModule->mpData->mpaKillZones[luZone],
                     mpModule->mpData->mpaKillZoneIds[luZone], lpDisplay);
}

// ARTIST 0x827611A8.
void DebugComponent::DrawRecentKillZones(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    CGS_ASSERT(mpModule != 0, "mpModule");
    for (u32 luZone = 0; luZone < mpModule->mDEBUGRecentlyFiredKillZones.GetLength(); ++luZone)
    {
        const u64 luId = mpModule->mDEBUGRecentlyFiredKillZones.GetItem(luZone).mKillZoneId;
        const KillZone* lpZone = mpModule->mpData->FindKillZone(luId);
        CGS_ASSERT(lpZone != 0, "lpKillZone");
        DrawKillZone(lpZone, luId, lpDisplay);
    }
}

// ARTIST 0x8275B678. ABI: r3=this, r4=zone, r5=64-bit id, r6=renderer.
void DebugComponent::DrawKillZone(const KillZone* lpKillZone, u64 luKillZoneId,
                                 CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    rw::RGBA lColour;
    lColour.m_rgba = 0x320064FF;
    rw::RGBA lApproachColour;
    lApproachColour.m_rgba = 0x32001EFF;
    const Vector3 lLift = {0, 3.5f, 0, 0};
    const VecFloat lApproachLength = {18, 18, 18, 18};
    CGS_ASSERT(lpKillZone != 0, "lpKillZone");
    Vector3 lLabelSum = {0, 0, 0, 0};
    f32 lfLabelCount = 0;
    bool lbDrawn = false;
    for (u32 luRegion = lpKillZone->muOffset;
         luRegion < static_cast<u32>(lpKillZone->muOffset) + lpKillZone->muCount; ++luRegion)
    {
        const KillZoneRegion& lrRegion = mpModule->mpData->mpaKillZoneRegions[luRegion];
        const Hull* lpHull = mpModule->GetHull(lrRegion.muHull);
        const Section* lpSection = lpHull->GetSection(lrRegion.muSection);
        if (ShouldSectionBeCulled(lpSection, lpHull, lpDisplay))
            continue;
        lbDrawn = true;
        for (u32 luRung = lrRegion.muStartRung; luRung < lrRegion.muEndRung; ++luRung)
        {
            const LaneRung& lrA = lpHull->mpaRungs[lpSection->muRungOffset + luRung];
            const LaneRung& lrB = lpHull->mpaRungs[lpSection->muRungOffset + luRung + 1];
            lpDisplay->DrawSolidQuad(lrB.maPoints[0], lrB.maPoints[0] + lLift,
                                    lrA.maPoints[0] + lLift, lrA.maPoints[0], lColour);
            lpDisplay->DrawSolidQuad(lrA.maPoints[1], lrA.maPoints[1] + lLift,
                                    lrB.maPoints[1] + lLift, lrB.maPoints[1], lColour);
        }
        const LaneRung& lrStart = lpHull->mpaRungs[lpSection->muRungOffset + lrRegion.muStartRung];
        const LaneRung& lrNext = lpHull->mpaRungs[lpSection->muRungOffset + lrRegion.muStartRung + 1];
        const Vector3 lLeftDirection = lrNext.maPoints[0] - lrStart.maPoints[0];
        const Vector3 lRightDirection = lrNext.maPoints[1] - lrStart.maPoints[1];
        // The original has an unconditional reciprocal-square-root refinement,
        // with no zero-length selection. Preserve degenerate-input NaNs.
        const f32 lfLeftInv = 1.0f / std::sqrt(rw::math::vpu::Dot(lLeftDirection, lLeftDirection));
        const f32 lfRightInv = 1.0f / std::sqrt(rw::math::vpu::Dot(lRightDirection, lRightDirection));
        const VecFloat lLeftInv = {lfLeftInv, lfLeftInv, lfLeftInv, lfLeftInv};
        const VecFloat lRightInv = {lfRightInv, lfRightInv, lfRightInv, lfRightInv};
        const Vector3 lLeftBack = lrStart.maPoints[0] - (lLeftDirection * lLeftInv) * lApproachLength;
        const Vector3 lRightBack = lrStart.maPoints[1] - (lRightDirection * lRightInv) * lApproachLength;
        lpDisplay->DrawSolidQuad(lrStart.maPoints[0], lrStart.maPoints[0] + lLift,
                                lLeftBack + lLift, lLeftBack, lApproachColour);
        lpDisplay->DrawSolidQuad(lrStart.maPoints[1], lrStart.maPoints[1] + lLift,
                                lRightBack + lLift, lRightBack, lApproachColour);
        lpDisplay->DrawSolidQuad(lLeftBack, lLeftBack + lLift,
                                lRightBack + lLift, lRightBack, lApproachColour);
        const LaneRung& lrEnd = lpHull->mpaRungs[lpSection->muRungOffset + lrRegion.muEndRung];
        lpDisplay->DrawSolidQuad(lrEnd.maPoints[1], lrEnd.maPoints[1] + lLift,
                                lrEnd.maPoints[0] + lLift, lrEnd.maPoints[0], lColour);
        // The binary averages these four particular corners, rather than all rendered vertices.
        lLabelSum = ((lLabelSum + lLeftBack) + (lLeftBack + lLift))
                    + lrEnd.maPoints[1] + (lrEnd.maPoints[1] + lLift);
        lfLabelCount = (lfLabelCount + 2.0f) + 2.0f;
    }
    if (lbDrawn)
    {
        char lacText[128];
        rw::core::stdc::ConvertI64ToA(static_cast<s64>(luKillZoneId), lacText, 10);
        const f32 lfInverse = 1.0f / lfLabelCount;
        const VecFloat lInverse = {lfInverse, lfInverse, lfInverse, lfInverse};
        lpDisplay->DrawText(lLabelSum * lInverse, lacText, 16.0f);
    }
}

} // namespace BrnTraffic

namespace BrnTraffic
{

// ARTIST 0x8275AEC0. This overlay intentionally has no distance cull.
void DebugComponent::DrawStuckDetection(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    CGS_ASSERT(mpModule != 0, "mpModule != NULL");
    rw::RGBA lContactColour;
    lContactColour.m_rgba = 0xFF0000FF;
    rw::RGBA lBelow;
    lBelow.m_rgba = 0xFF00FF00;
    rw::RGBA lAbove;
    lAbove.m_rgba = 0xC80000FF;
    // ARTIST std r22(0) clears the two spare lanes of each Vector2.
    static const Vector2 lFrontOffset = {-0.8f, 0.4f, 0, 0};
    static const Vector2 lBackOffset = {0.8f, 0.4f, 0, 0};
    for (u32 luVehicle = 0; luVehicle < KU_MAX_TOTAL_TRAFFIC; ++luVehicle)
    {
        const Vehicle& lrVehicle = mpModule->maVehicles[luVehicle];
        if (!lrVehicle.IsAlive() || !lrVehicle.IsPhysical())
            continue;
        const TrafficPhysicsInfo* lpInfo = mpModule->GetTrafficPhysicsInfoForVehicl(luVehicle);
        CGS_ASSERT(lrVehicle.IsAlive(), "IsAlive()");
        const VehicleTypeRuntime* lpType = mpModule->GetVehicleTypeRuntime(lrVehicle.GetVehicleType());
        CGS_ASSERT(lpType != 0, "lpVehicleTypeRuntime");
        Matrix44Affine lTransform = mpModule->GetVehicleTransform(luVehicle);
        const Vector3 lPosition = lTransform.Pos();
        const Vector3 lBase = lPosition + Vector3{0, 1, 0, 0};
        const Vector3 lHalf = lpType->GetBBoxHalfSize();
        const Vector3 lMin = {-lHalf.x, -0.15f, -0.15f, 0};
        const Vector3 lMax = {lHalf.x, 0.15f, 0.15f, 0};
        const VecFloat lDistance = {lHalf.z, lHalf.z, lHalf.z, lHalf.z};
        if ((lpInfo->muContactSideFlags & TrafficPhysicsInfo::E_CONTACT_SIDE_FRONT) != 0)
        {
            lTransform.Pos() = TrafficMultAdd(lTransform.At(), lDistance, lBase);
            lpDisplay->DrawSolidBox(lMin, lMax, lTransform, lContactColour);
        }
        if ((lpInfo->muContactSideFlags & TrafficPhysicsInfo::E_CONTACT_SIDE_BACK) != 0)
        {
            lTransform.Pos() = lBase - lTransform.At() * lDistance;
            lpDisplay->DrawSolidBox(lMin, lMax, lTransform, lContactColour);
        }
        const Vector3 lMeterPosition = lPosition + Vector3{0, 2.5f, 0, 0};
        RenderVerticalMeter(lpDisplay, lMeterPosition, lFrontOffset, 0.3f, 2.0f,
                            lBelow, lAbove, 6.0f, lpInfo->mfStuckTimeFront);
        RenderVerticalMeter(lpDisplay, lMeterPosition, lBackOffset, 0.3f, 2.0f,
                            lBelow, lAbove, 6.0f, lpInfo->mfStuckTimeBack);
    }
}

// ARTIST 0x8275C298: all 40 allocated records, ignoring the live count.
void DebugComponent::DrawFuzzyLogic(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    CGS_ASSERT(mpModule != 0, "mpModule != NULL");
    const u32 laColours[6] = {0xFFFF50FF, 0xFF5050B4, 0xFFFFB400,
                              0xFF0000FF, 0xFFFF5050, 0xFF00FF00};
    rw::RGBA lBase;
    lBase.m_rgba = 0xB4FFFFFF;
    for (u32 luRecord = 0; luRecord < 40; ++luRecord)
    {
        const DEBUG_VehicleFuzzyLogic& lrRecord = mpModule->mpaDEBUGVehicleFuzzyLogic[luRecord];
        Vector2 lOffset = {-0.9000000357627869f, 0.1f, 0, 0};
        for (u32 luOutput = 0; luOutput < 6; ++luOutput)
        {
            rw::RGBA lColour;
            lColour.m_rgba = laColours[luOutput];
            RenderVerticalMeter(lpDisplay, lrRecord.mRenderPosition, lOffset, 0.15f, 1.0f,
                                lBase, lColour, 1.0f, lrRecord.mafParamOutputScores[luOutput]);
            lOffset.x += 0.3f;
        }
    }
}

// ARTIST 0x8275BE28. The shipping body only draws cached collidables; the
// diagnostic avoidance-test count is the original constant zero.
void DebugComponent::DrawAvoidance(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    CgsDev::DebugInterface lDebugInterface;
    CgsDev::DebugRender& lrRender = lDebugInterface.Get2dRender();
    char lacText[5120];
    CgsDev::StrStream lStream(lacText, sizeof(lacText));
    u32 luCachedCount = 0;
    rw::RGBA lBoxColour;
    lBoxColour.m_rgba = 0x6400FFFF;
    rw::RGBA lArrowColour;
    lArrowColour.m_rgba = 0xA000FFFF;
    const Vector3 lMin = {-0.4f, -0.4f, -0.4f, 0};
    const Vector3 lMax = {0.4f, 0.4f, 0.4f, 0};
    const VecFloat lHalf = {0.5f, 0.5f, 0.5f, 0.5f};
    for (u32 luPacket = 0; luPacket < mpModule->mCachedCollidableList.GetLength(); ++luPacket)
    {
        const CollidableVehicleInfo4& lrPacket = mpModule->mCachedCollidableList.GetItem(luPacket);
        for (u32 luLane = 0; luLane < 4; ++luLane)
        {
            // Actual structure-of-arrays lanes, not byte offsets or duplicate packet structs.
            const auto lLane = [luLane](const Vector4& lrVector) -> f32
            {
                switch (luLane)
                {
                case 0: return lrVector.x;
                case 1: return lrVector.y;
                case 2: return lrVector.z;
                default: return lrVector.w;
                }
            };
            Vector3 lPosition = {lLane(lrPacket.mPosition_X), lLane(lrPacket.mPosition_Y),
                                 lLane(lrPacket.mPosition_Z), 0};
            const Vector3 lOffset = lPosition - Vector3{mpModule->KF_MAX_FLOAT.x, mpModule->KF_MAX_FLOAT.y,
                                                        mpModule->KF_MAX_FLOAT.z, 0};
            if (!(std::fabs(lOffset.x) > 0.0000152587890625f
                  || std::fabs(lOffset.y) > 0.0000152587890625f
                  || std::fabs(lOffset.z) > 0.0000152587890625f))
                continue;
            lPosition.y += 2.0f;
            Matrix44Affine lTransform;
            lTransform.SetIdentity();
            lTransform.Pos() = lPosition;
            lpDisplay->DrawSolidBox(lMin, lMax, lTransform, lBoxColour);
            const Vector3 lVelocity = {lLane(lrPacket.mLinearVelocity_X),
                                       lLane(lrPacket.mLinearVelocity_Y),
                                       lLane(lrPacket.mLinearVelocity_Z), 0};
            lpDisplay->DrawArrow(lPosition, TrafficMultAdd(lVelocity, lHalf, lPosition), lArrowColour);
            ++luCachedCount;
        }
    }
    lStream << "Cached Vehicle Count: " << luCachedCount << "\n" << "Avoidance Test Count: " << u32(0);
    DrawTextWithOffsets(lrRender, &lStream, 50.0f, 50.0f);
}

} // namespace BrnTraffic

// Scratch only. Uses the existing public Param fields and real history API.
namespace BrnTraffic
{

// ARTIST 0x82759D68.
void DebugComponent::DrawParamBehaviours(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    CGS_ASSERT(mpModule != 0, "mpModule != NULL");
    const Vector3 lHalf = {0.3f, 0.3f, 0.3f, 0};
    const u32 laColours[7] = {0xFF00C8C8, 0xFF1E1E64, 0xFFC81EC8, 0xFFC86400,
                              0xFF0000C8, 0xFFC81E1E, 0xFF00C800};
    rw::RGBA lStopColour;
    lStopColour.m_rgba = 0xFF823232;
    for (u32 luParam = 0; luParam < KU_MAX_PARAMS; ++luParam)
    {
        const Param& lrParam = mpModule->maParams[luParam];
        if (!lrParam.IsAlive())
            continue;
        const ParamTransform* lpTransform = mpModule->GetParamTransform(luParam);
        const Vector3 lPosition = lpTransform->GetDeterministicPos() + Vector3{0, 2, 0, 0};
        if (ShouldObjectBeCulled(lPosition, lpDisplay))
            continue;
        rw::RGBA lColour;
        lColour.m_rgba = lrParam.miBehaviour >= 0 && lrParam.miBehaviour < 7
                            ? laColours[lrParam.miBehaviour] : 0xFFC8C8C8;
        lpDisplay->DrawSolidBox(lPosition - lHalf, lPosition + lHalf, lColour);
        const u16 luInFront = mpModule->maParamNeedToSlowData[luParam].muParamInFront;
        if (lrParam.miBehaviour == 5 && luInFront != KU_INVALID_PARAM)
        {
            const Vector3 lOtherPosition = mpModule->GetParamTransform(luInFront)->GetDeterministicPos();
            DrawArrow(lPosition, lOtherPosition + Vector3{0, 2, 0, 0}, lColour, lpDisplay);
        }
        if (lrParam.mfStopDist < 1000.0f)
        {
            const Vector3 lFrom = lPosition - Vector3{0, 1, 0, 0};
            const Vector3 lDirection = mpModule->GetParamTransform(luParam)->GetDirection();
            const VecFloat lDistance = {lrParam.mfStopDist, lrParam.mfStopDist,
                                       lrParam.mfStopDist, lrParam.mfStopDist};
            lpDisplay->DrawLine(lFrom, TrafficMultAdd(lDirection, lDistance, lFrom), lStopColour);
        }
    }
}

// ARTIST 0x8275A0F8.
void DebugComponent::DrawParamSlowData(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    CGS_ASSERT(mpModule != 0, "mpModule != NULL");
    rw::RGBA lLinkColour;
    lLinkColour.m_rgba = 0xFF9628B4;
    rw::RGBA lStopColour;
    lStopColour.m_rgba = 0xFF823232;
    for (u32 luParam = 0; luParam < KU_MAX_PARAMS; ++luParam)
    {
        const Param& lrParam = mpModule->maParams[luParam];
        if (!lrParam.IsAlive())
            continue;
        const Vector3 lPosition = mpModule->GetParamTransform(luParam)->GetDeterministicPos() + Vector3{0, 2, 0, 0};
        if (ShouldObjectBeCulled(lPosition, lpDisplay))
            continue;
        const u16 luInFront = mpModule->maParamNeedToSlowData[luParam].muParamInFront;
        if (luInFront != KU_INVALID_PARAM)
        {
            const Vector3 lOtherPosition = mpModule->GetParamTransform(luInFront)->GetDeterministicPos();
            DrawArrow(lPosition, lOtherPosition + Vector3{0, 2, 0, 0}, lLinkColour, lpDisplay);
        }
        if (lrParam.mfStopDist < 1000.0f)
        {
            const Vector3 lFrom = lPosition - Vector3{0, 1, 0, 0};
            const Vector3 lDirection = mpModule->GetParamTransform(luParam)->GetDirection();
            const VecFloat lDistance = {lrParam.mfStopDist, lrParam.mfStopDist,
                                       lrParam.mfStopDist, lrParam.mfStopDist};
            lpDisplay->DrawLine(lFrom, TrafficMultAdd(lDirection, lDistance, lFrom), lStopColour);
        }
    }
}

// ARTIST 0x8275A390. No validity filter is inserted into the six-entry history walk.
void DebugComponent::DrawParamTrails(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    CGS_ASSERT(mpModule != 0, "mpModule != NULL");
    rw::RGBA lColour;
    lColour.m_rgba = 0xFFFFFF00;
    for (u32 luParam = 0; luParam < KU_MAX_PARAMS; ++luParam)
    {
        const Param& lrParam = mpModule->maParams[luParam];
        if (!lrParam.IsAlive())
            continue;
        const Vector3 lPosition = mpModule->GetParamTransform(luParam)->GetDeterministicPos();
        if (ShouldObjectBeCulled(lPosition, lpDisplay))
            continue;
        for (u32 luHistory = 0; luHistory < KU_PARAM_NUM_SEGMENTS_TO_REMEMBER; ++luHistory)
        {
            u32 luSegment;
            u32 luHull;
            lrParam.GetHistoryEntry(luHistory, &luSegment, &luHull);
            const Hull* lpHull = mpModule->GetHull(luHull);
            const LaneRung& lrFrom = lpHull->mpaRungs[luSegment];
            const LaneRung& lrTo = lpHull->mpaRungs[luSegment + 1];
            lpDisplay->DrawQuad(lrFrom.maPoints[0], lrFrom.maPoints[1],
                                lrTo.maPoints[1], lrTo.maPoints[0], lColour);
        }
    }
}

} // namespace BrnTraffic

// Existing typed dependencies: Param/ParamTransform/ParamListNode, Vehicle/Axle,
// VehicleTypeRuntime, TrafficData and Module's actual pool arrays. No new owners.
namespace BrnTraffic
{

// ARTIST 0x82754328.
void DebugComponent::DrawParams(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    CGS_ASSERT(mpModule != 0, "mpModule != NULL");
    const Vector3 lMin = {-0.3f, -0.3f, -0.3f, 0.0f};
    const Vector3 lMax = {0.3f, 0.3f, 0.3f, 0.0f};
    const rw::RGBA lColour(0, 255, 0, 255); // 0xFF00FF00
    const VecFloat lTwo = {2.0f, 2.0f, 2.0f, 2.0f};
    for (u32 luParam = 0; luParam < KU_MAX_PARAMS; ++luParam)
    {
        if (!mpModule->GetParam(luParam)->IsAlive())
            continue;
        const ParamTransform* lpTransform = mpModule->GetParamTransform(luParam);
        Vector3 lPosition = lpTransform->GetDeterministicPos();
        if (ShouldObjectBeCulled(lPosition, lpDisplay))
            continue;
        lpDisplay->DrawSolidBox(lPosition + lMin, lPosition + lMax, lColour);
        lPosition.y += 0.1f;
        const Vector3 lDirection = mpModule->GetParamTransform(luParam)->GetDirection();
        DrawArrow(lPosition, TrafficMultAdd(lDirection, lTwo, lPosition), lColour, lpDisplay);
    }
}

// ARTIST 0x82754550.
void DebugComponent::DrawLerpedParams(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    CGS_ASSERT(mpModule != 0, "mpModule != NULL");
    const Vector3 lMin = {-0.25f, -0.25f, -0.25f, 0.0f};
    const Vector3 lMax = {0.25f, 0.25f, 0.25f, 0.0f};
    const rw::RGBA lColour(70, 130, 180, 255); // 0xFFB48246
    for (u32 luParam = 0; luParam < KU_MAX_PARAMS; ++luParam)
    {
        if (!mpModule->GetParam(luParam)->IsAlive())
            continue;
        const Vector3 lPosition = mpModule->GetParamTransform(luParam)->GetLerpedPos();
        if (!ShouldObjectBeCulled(lPosition, lpDisplay))
            lpDisplay->DrawSolidBox(lPosition + lMin, lPosition + lMax, lColour);
    }
}

// ARTIST 0x82754998. The link starts at the FRONT AXLE, not the vehicle transform.
void DebugComponent::DrawParamVehicleLinks(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    CGS_ASSERT(mpModule != 0, "mpModule != NULL");
    const rw::RGBA lColour(64, 0, 128, 255); // 0xFF800040
    for (u32 luParam = 0; luParam < KU_MAX_PARAMS; ++luParam)
    {
        if (!mpModule->maParams[luParam].IsAlive() || !mpModule->maVehicles[luParam].IsAlive())
            continue;
        const VehicleAxles* lpAxles = mpModule->GetVehicleAxles(luParam);
        const Vector3 lPosition = mpModule->GetParamTransform(luParam)->GetDeterministicPos();
        if (!ShouldObjectBeCulled(lPosition, lpDisplay))
            lpDisplay->DrawLine(lpAxles->mFrontAxle.mPosAndWheelRadius.GetVector3(), lPosition, lColour);
    }
}

// ARTIST 0x82754B08.
void DebugComponent::DrawParamParamLinks(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    CGS_ASSERT(mpModule != 0, "mpModule != NULL");
    const Vector3 lLift = {0.0f, 1.0f, 0.0f, 0.0f};
    const rw::RGBA lNextColour(240, 0, 64, 255); // 0xFF4000F0
    const rw::RGBA lPrevColour(64, 0, 240, 255); // 0xFFF00040
    for (u32 luParam = 0; luParam < KU_MAX_PARAMS; ++luParam)
    {
        if (!mpModule->maParams[luParam].IsAlive())
            continue;
        const Vector3 lPosition = mpModule->GetParamTransform(luParam)->GetDeterministicPos();
        if (ShouldObjectBeCulled(lPosition, lpDisplay))
            continue;
        const ParamListNode& lrNode = mpModule->maParamListNodes[luParam];
        const u16 luNext = lrNode.muNextParam;
        const u16 luPrev = lrNode.muPrevParam;
        if (luNext != 0xFFFFu)
        {
            const Vector3 lFrom = mpModule->GetParamTransform(luParam)->GetDeterministicPos() + lLift;
            const Vector3 lTo = mpModule->GetParamTransform(luNext)->GetDeterministicPos() + lLift;
            DrawOffsetArrow(lFrom, lTo, 0.5f, lNextColour, lpDisplay);
        }
        if (luPrev != 0xFFFFu)
        {
            const Vector3 lFrom = mpModule->GetParamTransform(luParam)->GetDeterministicPos() + lLift;
            const Vector3 lTo = mpModule->GetParamTransform(luPrev)->GetDeterministicPos() + lLift;
            DrawOffsetArrow(lFrom, lTo, 0.5f, lPrevColour, lpDisplay);
        }
    }
}

// ARTIST 0x8275A538.
void DebugComponent::DrawVehicles(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    CGS_ASSERT(mpModule != 0, "mpModule != NULL");
    const rw::RGBA laColours[4] = {rw::RGBA(255, 0, 0, 255), rw::RGBA(0, 255, 0, 255),
                                  rw::RGBA(0, 0, 255, 255), rw::RGBA(170, 0, 170, 255)};
    for (u32 luVehicle = 0; luVehicle < KU_MAX_TOTAL_TRAFFIC; ++luVehicle)
    {
        const Vehicle& lrVehicle = mpModule->maVehicles[luVehicle];
        if (!lrVehicle.IsAlive())
            continue;
        const Matrix44Affine lTransform = mpModule->GetVehicleTransform(luVehicle);
        if (ShouldObjectBeCulled(lTransform.Pos(), lpDisplay))
            continue;
        const u32 luType = lrVehicle.GetVehicleType();
        const u8 luClass = mpModule->mpData->mpaVehicleTypes[luType].muVehicleClass;
        const VehicleTypeRuntime& lrType = mpModule->maVehicleTypeRuntime[luType];
        const Vector3 lOffset = lrType.GetBBoxOffset();
        const Vector3 lHalf = lrType.GetBBoxHalfSize();
        if (mbDrawParams)
            lpDisplay->DrawBox(lOffset - lHalf, lOffset + lHalf, lTransform, laColours[luClass]);
        else
            lpDisplay->DrawSolidBox(lOffset - lHalf, lOffset + lHalf, lTransform, laColours[luClass]);
    }
}

// ARTIST 0x8275A6F0. Noncollidable vehicles always use a wireframe box.
void DebugComponent::DrawNonCollidableVehicles(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    CGS_ASSERT(mpModule != 0, "mpModule != NULL");
    const rw::RGBA laColours[4] = {rw::RGBA(255, 0, 0, 255), rw::RGBA(0, 255, 0, 255),
                                  rw::RGBA(0, 0, 255, 255), rw::RGBA(170, 0, 170, 255)};
    for (u32 luVehicle = 0; luVehicle < KU_MAX_TOTAL_TRAFFIC; ++luVehicle)
    {
        const Vehicle& lrVehicle = mpModule->maVehicles[luVehicle];
        if (!lrVehicle.IsAlive() || lrVehicle.IsCollidable())
            continue;
        const Matrix44Affine lTransform = mpModule->GetVehicleTransform(luVehicle);
        if (ShouldObjectBeCulled(lTransform.Pos(), lpDisplay))
            continue;
        const u32 luType = lrVehicle.GetVehicleType();
        const u8 luClass = mpModule->mpData->mpaVehicleTypes[luType].muVehicleClass;
        const VehicleTypeRuntime& lrType = mpModule->maVehicleTypeRuntime[luType];
        const Vector3 lOffset = lrType.GetBBoxOffset();
        const Vector3 lHalf = lrType.GetBBoxHalfSize();
        lpDisplay->DrawBox(lOffset - lHalf, lOffset + lHalf, lTransform, laColours[luClass]);
    }
}

// ARTIST 0x8275A870.
void DebugComponent::DrawCollidableVehicles(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    CGS_ASSERT(mpModule != 0, "mpModule != NULL");
    const rw::RGBA laColours[4] = {rw::RGBA(255, 0, 0, 255), rw::RGBA(0, 255, 0, 255),
                                  rw::RGBA(0, 0, 255, 255), rw::RGBA(170, 0, 170, 255)};
    for (u32 luVehicle = 0; luVehicle < KU_MAX_TOTAL_TRAFFIC; ++luVehicle)
    {
        const Vehicle& lrVehicle = mpModule->maVehicles[luVehicle];
        if (!lrVehicle.IsAlive() || !lrVehicle.IsCollidable())
            continue;
        const Matrix44Affine lTransform = mpModule->GetVehicleTransform(luVehicle);
        if (ShouldObjectBeCulled(lTransform.Pos(), lpDisplay))
            continue;
        const u32 luType = lrVehicle.GetVehicleType();
        const u8 luClass = mpModule->mpData->mpaVehicleTypes[luType].muVehicleClass;
        const VehicleTypeRuntime& lrType = mpModule->maVehicleTypeRuntime[luType];
        const Vector3 lOffset = lrType.GetBBoxOffset();
        const Vector3 lHalf = lrType.GetBBoxHalfSize();
        if (mbDrawParams)
            lpDisplay->DrawBox(lOffset - lHalf, lOffset + lHalf, lTransform, laColours[luClass]);
        else
            lpDisplay->DrawSolidBox(lOffset - lHalf, lOffset + lHalf, lTransform, laColours[luClass]);
    }
}

} // namespace BrnTraffic

// Scratch only. Includes <cmath>, <cstdio>, real TrafficPhysicsInfo and Vehicle;
// BrnTrafficVehicle.h friendship is approved for the named unasserted field reads.
namespace BrnTraffic
{

// ARTIST export hole 0x8274FB18; DecFIGS BrnTrafficDebugComponent.cpp:128.
// The original interpolates normalized channels, truncates to s64 and keeps each
// low byte. It does not clamp the interpolation factor or the output channels.
rw::RGBA Lerp(const rw::RGBA& lrA, const rw::RGBA& lrB, f32 lfT)
{
    rw::RGBA lResult;
    lResult.m_rgba = 0;
    for (u32 luShift = 0; luShift < 32; luShift += 8)
    {
        const f32 lfA = static_cast<f32>((lrA.m_rgba >> luShift) & 0xFF) * 0.003921568859368563f;
        const f32 lfDifference = std::fma(static_cast<f32>((lrB.m_rgba >> luShift) & 0xFF),
                                         0.003921568859368563f, -lfA);
        const f32 lfChannel = std::fma(lfDifference, lfT, lfA) * 255.0f;
        const u8 luChannel = static_cast<u8>(static_cast<s64>(lfChannel));
        lResult.m_rgba |= static_cast<u32>(luChannel) << luShift;
    }
    return lResult;
}

// ARTIST 0x82755058. The decompiler aliases renderer and vehicle registers;
// asm 82755288 reads Vehicle::mTargetPos, not the renderer's +0x70 bytes.
void DebugComponent::DrawVehicleSwervingDebugging(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    CGS_ASSERT(mpModule != 0, "mpModule != NULL");
    const Vector3 lHalf = {0.3f, 0.3f, 0.3f, 0};
    rw::RGBA lNoSwerve;
    lNoSwerve.m_rgba = 0xFFC0C0C0;
    rw::RGBA lBase;
    lBase.m_rgba = 0xFFFFC8C8;
    rw::RGBA lRecover;
    lRecover.m_rgba = 0xFF0000FF;
    rw::RGBA lSwerve;
    lSwerve.m_rgba = 0xFFFF5400;
    // Original fifth static colour 8300D4A0 = FF37B837 is initialized but unused.
    for (u32 luVehicle = 0; luVehicle < KU_MAX_TOTAL_TRAFFIC; ++luVehicle)
    {
        const Vehicle& lrVehicle = mpModule->maVehicles[luVehicle];
        if (!lrVehicle.IsAlive())
            continue;
        CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC, "luIndex < KU_MAX_TOTAL_TRAFFIC");
        if (ShouldObjectBeCulled(mpModule->maVehicleTransforms[luVehicle].Pos(), lpDisplay))
            continue;
        const Vector3 lPosition = lrVehicle.mTargetPos;
        const f32 lfAmount = lrVehicle.GetSwerveAmount().x;
        rw::RGBA lColour;
        if (!(lfAmount > 0.00000011920928955078125f) && lfAmount >= -0.00000011920928955078125f)
            lColour = lNoSwerve;
        else if (lrVehicle.mfSwerveTime < 0.0f)
            lColour = Lerp(lBase, lRecover, -lrVehicle.mfSwerveTime);
        else
        {
            rw::RGBA lTarget = lSwerve;
            if (lrVehicle.IsExtremeSwerving())
                lTarget.m_rgba = 0xFF00B4B4;
            lColour = Lerp(lBase, lTarget, lrVehicle.mfSwerveTime * 2.0f);
        }
        lpDisplay->DrawSolidBox(lPosition - lHalf, lPosition + lHalf, lColour);
    }
}

// ARTIST 0x8275AA08. The marker advances after every tested state, even when
// no box is drawn; the deformation/fatal label consumes one additional row.
void DebugComponent::DrawPhysicalVehicleState(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    CGS_ASSERT(mpModule != 0, "mpModule != NULL");
    const Vector3 lMin = {-0.15f, -0.15f, -0.15f, 0};
    const Vector3 lMax = {0.15f, 0.15f, 0.15f, 0};
    const Vector3 lRow = {0, 0.15f * 2.5f, 0, 0};
    for (u32 luVehicle = 0; luVehicle < KU_MAX_TOTAL_TRAFFIC; ++luVehicle)
    {
        const Vehicle& lrVehicle = mpModule->maVehicles[luVehicle];
        if (!lrVehicle.IsAlive() || !lrVehicle.IsPhysical())
            continue;
        Matrix44Affine lTransform = mpModule->GetVehicleTransform(luVehicle);
        if (ShouldObjectBeCulled(lTransform.Pos(), lpDisplay))
            continue;
        lTransform.Pos() = lTransform.Pos() + Vector3{0, 2, 0, 0};
        const TrafficPhysicsInfo* lpInfo = mpModule->GetTrafficPhysicsInfoForVehicl(luVehicle);
        if (lpInfo->mbIsDeforming || lpInfo->mbIsFatallyCrashing)
        {
            char lacText[128] = "    ";
            if (lpInfo->mbIsDeforming)
                lacText[0] = 'D';
            if (lpInfo->mbIsFatallyCrashing)
                lacText[2] = 'F';
            if (lpInfo->mfTimeNotDriving > 0.0f)
                std::sprintf(lacText + 4, "%.1f", static_cast<double>(lpInfo->mfTimeNotDriving));
            lpDisplay->DrawText(lTransform.Pos(), lacText, 16.0f);
            lTransform.Pos() = lTransform.Pos() + lRow;
        }
        const auto lDrawState = [&](bool lbState, u32 luColour)
        {
            if (lbState)
            {
                rw::RGBA lColour;
                lColour.m_rgba = luColour;
                lpDisplay->DrawSolidBox(lMin, lMax, lTransform, lColour);
            }
            lTransform.Pos() = lTransform.Pos() + lRow;
        };
        lDrawState(lrVehicle.IsCrashing(), 0xFF0000FF);
        lDrawState(lrVehicle.IsSympatheticallyCrashing(), 0xFF00C8C8);
        lDrawState(lrVehicle.IsExtremeSwerving(), 0xFFC800C8);
        lDrawState(lrVehicle.GetCurrentManoeuvre() == Vehicle::E_MANOEUVRE_EXTREME_SWERVE, 0xFF640064);
        lDrawState(lrVehicle.IsRecoveringFromSlam(), 0xFFC8C800);
        lDrawState(lrVehicle.IsBeingChecked(), 0xFF64C864);
        lDrawState(lrVehicle.IsNormalPhysical(), 0xFF000000);
    }
}

} // namespace BrnTraffic

namespace BrnTraffic
{

// ARTIST export hole 0x827553A8..82755844. DecFIGS 0x990D24 confirms const;
// muCurrentVehicleToPick must be mutable (same real u32 member, no new storage).
void DebugComponent::DrawPickedVehicle(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    CGS_ASSERT(mpModule != 0, "mpModule");
    const Matrix44Affine& lrCamera = mpModule->mCameraLastFrame.GetTransform();
    const Vector3 lDirection = lrCamera.At();
    const Vector3 lCameraPosition = lrCamera.Pos();
    const VecFloat lRayLength = {500, 500, 500, 500};
    const Vector3 lRayEnd = TrafficMultAdd(lDirection, lRayLength, lCameraPosition);
    f32 lfNearestSquared = 9000000.0f;
    muCurrentVehicleToPick = 0xFFFFFFFF;
    Vector3 lNearestCentre;
    f32 lfNearestRadius;
    rw::RGBA lGuideColour;
    lGuideColour.m_rgba = 0xFF00FFFF;
    rw::RGBA lMissColour;
    lMissColour.m_rgba = 0xFF006400;
    for (u32 luVehicle = 0; luVehicle < KU_MAX_TOTAL_TRAFFIC; ++luVehicle)
    {
        CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC, "luIndex < KU_MAX_TOTAL_TRAFFIC");
        const Vehicle& lrVehicle = mpModule->maVehicles[luVehicle];
        const Vector3 lCentre = mpModule->maVehicleTransforms[luVehicle].Pos();
        if (!lrVehicle.IsAlive() || ShouldObjectBeCulled(lCentre, lpDisplay))
            continue;
        // The original sphere is centered on transform.Pos(), without the bbox offset.
        const f32 lfRadius = mpModule->maVehicleTypeRuntime[lrVehicle.GetVehicleType()].GetBBoxHalfSize().z;
        const f32 lfRadiusSquared = lfRadius * lfRadius;
        const Vector3 lFromStart = lCentre - lCameraPosition;
        const Vector3 lFromEnd = lCentre - lRayEnd;
        const f32 lfStartSquared = rw::math::vpu::Dot(lFromStart, lFromStart);
        const f32 lfEndSquared = rw::math::vpu::Dot(lFromEnd, lFromEnd);
        const f32 lfAlong = rw::math::vpu::Dot(lDirection, lFromStart);
        const VecFloat lAlong = {lfAlong, lfAlong, lfAlong, lfAlong};
        const Vector3 lPerpendicular = lFromStart - lDirection * lAlong;
        const f32 lfPerpendicularSquared = rw::math::vpu::Dot(lPerpendicular, lPerpendicular);
        // Literal comparison polarity preserves the VMX not-greater branches at NaN.
        const bool lbIntersects = !(lfStartSquared > lfRadiusSquared)
            || !(lfEndSquared > lfRadiusSquared)
            || (lfAlong >= 0.0f && !(lfAlong > 500.0f)
                && !(lfPerpendicularSquared > lfRadiusSquared));
        if (!lbIntersects)
            lpDisplay->DrawSphere(lCentre, lfRadius, lMissColour);
        else if (lfNearestSquared > lfStartSquared)
        {
            lfNearestSquared = lfStartSquared;
            muCurrentVehicleToPick = luVehicle;
            lNearestCentre = lCentre;
            lfNearestRadius = lfRadius;
        }
    }
    if (muCurrentVehicleToPick != 0xFFFFFFFF)
    {
        lGuideColour.m_rgba = 0xFF0000FF;
        lpDisplay->DrawSphere(lNearestCentre, lfNearestRadius, lGuideColour);
    }
    if (mpModule->muDEBUGPickedVehicle != 0xFFFFFFFF)
    {
        const Vehicle* lpVehicle = mpModule->GetVehicle(mpModule->muDEBUGPickedVehicle);
        CGS_ASSERT(mpModule->muDEBUGPickedVehicle < KU_MAX_TOTAL_TRAFFIC, "luIndex < KU_MAX_TOTAL_TRAFFIC");
        const Matrix44Affine lTransform = mpModule->maVehicleTransforms[mpModule->muDEBUGPickedVehicle];
        const VehicleTypeRuntime& lrType = mpModule->maVehicleTypeRuntime[lpVehicle->GetVehicleType()];
        const Vector3 lOffset = lrType.GetBBoxOffset();
        const Vector3 lHalf = lrType.GetBBoxHalfSize();
        lpDisplay->DrawBox(lOffset - lHalf, lOffset + lHalf, lTransform, rw::RGBA(255, 255, 255, 255));
    }
    Matrix44Affine lGuide;
    lGuide.SetIdentity();
    const VecFloat lGuideDistance = {10, 10, 10, 10};
    lGuide.Pos() = TrafficMultAdd(lDirection, lGuideDistance, lCameraPosition);
    lpDisplay->DrawBox(Vector3{-0.08f, -0.08f, -0.08f, 0},
                       Vector3{0.08f, 0.08f, 0.08f, 0}, lGuide, lGuideColour);
}

} // namespace BrnTraffic

namespace BrnTraffic
{

// ARTIST 0x8275CA58. There is no cull or live vehicle check on the showtime-info list.
void DebugComponent::DrawShowtime(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    rw::RGBA lLandingColour;
    lLandingColour.m_rgba = 0xFF0000FF;
    rw::RGBA lGroundColour;
    lGroundColour.m_rgba = 0xFF00FFFF;
    rw::RGBA lCandidateColour;
    lCandidateColour.m_rgba = 0xFF00FF00;
    rw::RGBA lTargetColour;
    lTargetColour.m_rgba = 0x64FFFF00;
    rw::RGBA lCrashColour;
    lCrashColour.m_rgba = 0xFF00C8C8;
    lpDisplay->DrawSolidSphere(mpModule->mShowtimePlayerLandingPos2D, 0.2f, lLandingColour);
    lpDisplay->DrawSolidSphere(mpModule->mShowtimePlayerGroundPos, 0.2f, lGroundColour);
    for (u32 luInfo = 0; luInfo < mpModule->muShowtimeVehicleInfoCount; ++luInfo)
    {
        const ShowtimeVehicleInfo* lpInfo = &mpModule->maShowtimeVehicleInfoList[luInfo];
        CGS_ASSERT(lpInfo != 0, "lpShowtimeInfo");
        CGS_ASSERT(lpInfo->muVehicleIndex < KU_MAX_TOTAL_TRAFFIC, "luIndex < KU_MAX_TOTAL_TRAFFIC");
        const Vector3 lPosition = mpModule->maVehicleTransforms[lpInfo->muVehicleIndex].Pos();
        const Vehicle* lpVehicle = mpModule->GetVehicle(lpInfo->muVehicleIndex);
        CGS_ASSERT(lpVehicle != 0, "lpVehicle");
        if ((lpInfo->muFlags & 2) != 0)
            lpDisplay->DrawSolidSphere(lPosition + Vector3{0, 3.6f, 0, 0}, 0.2f, lCandidateColour);
        if (lpVehicle->IsAlive())
            lpDisplay->DrawSolidSphere(lpVehicle->mTargetPos, 0.2f, lTargetColour);
    }
    const Vector3 lMin = {-0.15f, -0.15f, -0.15f, 0};
    const Vector3 lMax = {0.15f, 0.15f, 0.15f, 0};
    for (u32 luVehicle = 0; luVehicle < KU_MAX_TOTAL_TRAFFIC; ++luVehicle)
    {
        const Vehicle& lrVehicle = mpModule->maVehicles[luVehicle];
        if (!lrVehicle.IsAlive() || !lrVehicle.IsPhysical() || !lrVehicle.IsSympatheticallyCrashing())
            continue;
        CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC, "luIndex < KU_MAX_TOTAL_TRAFFIC");
        Matrix44Affine lTransform = mpModule->maVehicleTransforms[luVehicle];
        lTransform.Pos() = lTransform.Pos() + Vector3{0, 3, 0, 0};
        lpDisplay->DrawSolidBox(lMin, lMax, lTransform, lCrashColour);
        const EntityId lTarget = lrVehicle.GetSympatheticCrashTarget();
        if ((lTarget.muValue >> 24) == 2)
        {
            Matrix44Affine lTargetTransform = mpModule->GetVehicleTransform((lTarget.muValue >> 10) & 0x3FFF);
            lTargetTransform.Pos() = lTargetTransform.Pos() + Vector3{0, 3, 0, 0};
            lpDisplay->DrawLine(lTransform.Pos(), lTargetTransform.Pos(), lCrashColour);
            lpDisplay->DrawSolidBox(lMin, lMax, lTargetTransform, lLandingColour);
        }
    }
}

} // namespace BrnTraffic

namespace BrnTraffic
{

// ARTIST 0x82759738. The side-exit gate and x/y/z lift are original.
void DebugComponent::DrawSplitters(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    CGS_ASSERT(mpModule != 0, "mpModule != NULL");
    const VecFloat lHalf = {0.5f, 0.5f, 0.5f, 0.5f};
    const Vector3 lLift = {0, 0.3f, 0.2f, 0};
    rw::RGBA lCrossColour;
    lCrossColour.m_rgba = 0xFFFF0000; // 82F300D4
    const u32 laExitOrder[3] = {1, 0, 2};
    const u32 laColours[3] = {0xFF0000FF, 0xFFFFFFFF, 0xFF00FF00};
    for (u32 luActive = 0; luActive < mpModule->mActiveHulls.GetLength(); ++luActive)
    {
        const Hull* lpHull = mpModule->GetHull(mpModule->mActiveHulls.GetItem(luActive));
        for (u32 luSection = 0; luSection < lpHull->muNumSections; ++luSection)
        {
            const Section* lpSection = &lpHull->mpaSections[luSection];
            if (ShouldSectionBeCulled(lpSection, lpHull, lpDisplay)
                || (lpSection->mauForwardSections[1] == 0xFF && lpSection->mauForwardSections[2] == 0xFF))
                continue;
            const LaneRung& lrLast = lpHull->mpaRungs[lpSection->muRungOffset + lpSection->muNumRungs - 1];
            const LaneRung& lrPrevious = lpHull->mpaRungs[lpSection->muRungOffset + lpSection->muNumRungs - 2];
            lpDisplay->DrawLine(lrLast.maPoints[0], lrLast.maPoints[1], lCrossColour);
            const Vector3 lLastCentre = TrafficMultAdd(lrLast.maPoints[1] - lrLast.maPoints[0], lHalf, lrLast.maPoints[0]);
            const Vector3 lPreviousCentre = TrafficMultAdd(lrPrevious.maPoints[1] - lrPrevious.maPoints[0], lHalf, lrPrevious.maPoints[0]);
            const Vector3 lFrom = (lLastCentre + lPreviousCentre) * lHalf;
            for (u32 luExitIndex = 0; luExitIndex < 3; ++luExitIndex)
            {
                const u32 luExit = laExitOrder[luExitIndex];
                if (lpSection->mauForwardSections[luExit] == 0xFF)
                    continue;
                const Hull* lpOtherHull = mpModule->GetHull(lpSection->mauForwardHulls[luExit]);
                const Section* lpOtherSection = lpOtherHull->GetSection(lpSection->mauForwardSections[luExit]);
                const LaneRung& lrFirst = lpOtherHull->mpaRungs[lpOtherSection->muRungOffset];
                const LaneRung& lrSecond = lpOtherHull->mpaRungs[lpOtherSection->muRungOffset + 1];
                const Vector3 lFirstCentre = TrafficMultAdd(lrFirst.maPoints[1] - lrFirst.maPoints[0], lHalf, lrFirst.maPoints[0]);
                const Vector3 lSecondCentre = TrafficMultAdd(lrSecond.maPoints[1] - lrSecond.maPoints[0], lHalf, lrSecond.maPoints[0]);
                rw::RGBA lColour;
                lColour.m_rgba = laColours[luExitIndex];
                lpDisplay->DrawArrow(lFrom + lLift,
                    TrafficMultAdd(lFirstCentre + lSecondCentre, lHalf, lLift), lColour);
            }
        }
    }
}

// ARTIST 0x82750110: nearest live vehicle to the renderer camera, independent of cull range.
void DebugComponent::DrawNearestVehicleId(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    CGS_ASSERT(mpModule != 0, "mpModule != NULL");
    const Vector3 lCamera = lpDisplay->GetCameraPosition();
    f32 lfNearestSquared = 3.402823466e38f; // 8204F664 = FLT_MAX
    u32 luNearest = KU_INVALID_VEHICLE;
    Vector3 lNearestPosition = {0, 0, 0, 0};
    for (u32 luVehicle = 0; luVehicle < KU_MAX_TOTAL_TRAFFIC; ++luVehicle)
    {
        const Vehicle* lpVehicle = mpModule->GetVehicle(luVehicle);
        if (!lpVehicle->IsAlive())
            continue;
        CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC, "luIndex < KU_MAX_TOTAL_TRAFFIC");
        const Vector3 lPosition = mpModule->maVehicleTransforms[luVehicle].Pos();
        const Vector3 lOffset = lCamera - lPosition;
        const f32 lfSquared = rw::math::vpu::Dot(lOffset, lOffset);
        if (lfSquared < lfNearestSquared)
        {
            luNearest = luVehicle;
            lfNearestSquared = lfSquared;
            lNearestPosition = lPosition;
        }
    }
    if (luNearest != KU_INVALID_VEHICLE)
    {
        char lacText[128];
        CgsCore::SPrintf(lacText, sizeof(lacText), "ID: %u", luNearest);
        lpDisplay->DrawText(lNearestPosition + Vector3{0, 3, 0, 0}, lacText, 16.0f);
    }
}

// ARTIST 0x827502F8: fixed inclusive 30-unit radius, not mfRenderCullDistance.
void DebugComponent::DrawVehicleIds(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    CGS_ASSERT(mpModule != 0, "mpModule != NULL");
    const Vector3 lCamera = lpDisplay->GetCameraPosition();
    for (u32 luVehicle = 0; luVehicle < KU_MAX_TOTAL_TRAFFIC; ++luVehicle)
    {
        const Vehicle* lpVehicle = mpModule->GetVehicle(luVehicle);
        if (!lpVehicle->IsAlive())
            continue;
        CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC, "luIndex < KU_MAX_TOTAL_TRAFFIC");
        const Vector3 lPosition = mpModule->maVehicleTransforms[luVehicle].Pos();
        const Vector3 lOffset = lCamera - lPosition;
        if (900.0f >= rw::math::vpu::Dot(lOffset, lOffset))
        {
            char lacText[128];
            CgsCore::SPrintf(lacText, sizeof(lacText), "ID: %u", luVehicle);
            lpDisplay->DrawText(lPosition + Vector3{0, 3, 0, 0}, lacText, 16.0f);
        }
    }
}

} // namespace BrnTraffic

// Scratch only: additional complete vehicle overlays. Vehicle::muSpecies needs
// DebugComponent friendship for DrawAxles' original high-bit diagnostic read.
namespace BrnTraffic
{

// ARTIST 0x82754700. The stream receives signed phases and a truncated float timer.
void DebugComponent::DrawVehicleManoeuvres(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    CGS_ASSERT(mpModule != 0, "mpModule != NULL");
    char lacText[4096];
    CgsDev::StrStream lStream(lacText, sizeof(lacText));
    rw::RGBA lColour;
    lColour.m_rgba = 0xFFAA6464;
    for (u32 luVehicle = 0; luVehicle < KU_MAX_TOTAL_TRAFFIC; ++luVehicle)
    {
        const Vehicle& lrVehicle = mpModule->maVehicles[luVehicle];
        if (!lrVehicle.IsAlive() || !lrVehicle.IsPhysical())
            continue;
        const Matrix44Affine lTransform = mpModule->GetVehicleTransform(luVehicle);
        if (ShouldObjectBeCulled(lTransform.Pos(), lpDisplay))
            continue;
        switch (lrVehicle.GetCurrentManoeuvre())
        {
        case Vehicle::E_MANOEUVRE_EXTREME_SWERVE:
            lStream << "Extreme Swerve";
            break;
        case Vehicle::E_MANOEUVRE_3_POINT_TURN:
            lStream << "3-Point Turn / " << static_cast<s32>(static_cast<s8>(lrVehicle.GetCurrentManoeuvrePhase()));
            break;
        case Vehicle::E_MANOEUVRE_GIVE_UP:
            lStream << "Given Up / " << static_cast<s32>(static_cast<s8>(lrVehicle.GetCurrentManoeuvrePhase()));
            break;
        case Vehicle::E_MANOEUVRE_STUCK_REVERSE:
            lStream << "Stuck - Reverse [ " << static_cast<s32>(static_cast<s8>(lrVehicle.GetCurrentManoeuvrePhase())) << " ]";
            break;
        default:
            continue;
        }
        lStream << " T: " << static_cast<s32>(lrVehicle.GetManoeuvreTime());
        lpDisplay->DrawText(lTransform.Pos() + Vector3{0, 2, 0, 0}, lacText, 16.0f, lColour);
        lStream.Reset();
    }
}

// ARTIST 0x82754DA8: only the 400 standard-pool slots, and the unasserted other-half read.
void DebugComponent::DrawRearArticPoints(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    CGS_ASSERT(lpDisplay != 0, "lpDisplay");
    for (u32 luVehicle = 0; luVehicle < KU_MAX_STANDARD_TRAFFIC; ++luVehicle)
    {
        const Vehicle& lrVehicle = mpModule->maVehicles[luVehicle];
        if (!lrVehicle.IsAlive() || lrVehicle.GetOtherHalfIndex() == KU_INVALID_VEHICLE)
            continue;
        Matrix44Affine lTransform = mpModule->GetVehicleTransform(luVehicle);
        if (ShouldObjectBeCulled(lTransform.Pos(), lpDisplay))
            continue;
        const f32 lfPivot = mpModule->maVehicleTypeRuntime[lrVehicle.GetVehicleType()].GetCabPivotDistance();
        const VecFloat lPivot = {lfPivot, lfPivot, lfPivot, lfPivot};
        lTransform.Pos() = TrafficMultAdd(lTransform.At(), lPivot, lTransform.Pos()) + Vector3{0, 0.2f, 0, 0};
        lpDisplay->DrawAxis(lTransform);
    }
}

// ARTIST 0x82754F18. Original mixes trailer slot zero's type with transform pool slot zero.
void DebugComponent::DrawFrontArticPoints(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    CGS_ASSERT(lpDisplay != 0, "lpDisplay");
    const Vehicle* lpVehicle = mpModule->GetTrailerVehicle(0);
    if (!lpVehicle->IsAlive())
        return;
    Matrix44Affine lTransform = mpModule->maVehicleTransforms[0];
    if (ShouldObjectBeCulled(lTransform.Pos(), lpDisplay))
        return;
    const f32 lfPivot = mpModule->maVehicleTypeRuntime[lpVehicle->GetVehicleType()].GetTrailerPivotDistance();
    const VecFloat lPivot = {lfPivot, lfPivot, lfPivot, lfPivot};
    lTransform.Pos() = TrafficMultAdd(lTransform.At(), lPivot, lTransform.Pos()) + Vector3{0, 0.2f, 0, 0};
    lpDisplay->DrawAxis(lTransform);
}

// ARTIST 0x82750570. Effects byte bit zero is the horn; no extra horn accessor assert.
void DebugComponent::DrawHorns(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    CGS_ASSERT(lpDisplay != 0, "lpDisplay");
    const Vector3 lMin = {-0.5f, -0.5f, -0.5f, 0};
    const Vector3 lMax = {0.5f, 0.5f, 0.5f, 0};
    rw::RGBA lColour;
    lColour.m_rgba = 0xFF0000FF;
    for (u32 luVehicle = 0; luVehicle < KU_MAX_TOTAL_TRAFFIC; ++luVehicle)
    {
        const Vehicle* lpVehicle = mpModule->GetVehicle(luVehicle);
        if (!lpVehicle->IsAlive() || (lpVehicle->GetEffectState() & 1) == 0)
            continue;
        Matrix44Affine lTransform = mpModule->GetVehicleTransform(luVehicle);
        if (ShouldObjectBeCulled(lTransform.Pos(), lpDisplay))
            continue;
        lTransform.Pos() = lTransform.Pos() + Vector3{0, 2.5f, 0, 0};
        lpDisplay->DrawBox(lMin, lMax, lTransform, lColour);
    }
}

// ARTIST 0x8275BBC0. The original validates xyz by self-equality (NaN only).
void DebugComponent::DrawAxle(const Axle* lpAxle, CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    CGS_ASSERT(lpAxle != 0, "lpAxle");
    const Vector3 lPosition = lpAxle->mPosAndWheelRadius.GetVector3();
    if (!(lPosition.x == lPosition.x && lPosition.y == lPosition.y && lPosition.z == lPosition.z))
        return;
    rw::RGBA lSphereColour;
    lSphereColour.m_rgba = 0xFF323280;
    lpDisplay->DrawSolidSphere(lPosition, 0.1f, lSphereColour);
    const Vector3 lUp = lpAxle->mUpAndDebug.GetVector3();
    if (!(lUp.x == lUp.x && lUp.y == lUp.y && lUp.z == lUp.z))
        return;
    const f32 lfDebug = lpAxle->mUpAndDebug.GetPlus();
    rw::RGBA lColour;
    // 820C0B90 = 1/65536, independently corroborated by the image's IEEE bits 0x37800000.
    if (!(std::fabs(lfDebug - 2.0f) > 0.0000152587890625f))
        lColour.m_rgba = 0xFF00FFFF;
    else if (!(std::fabs(lfDebug - 1.0f) > 0.0000152587890625f))
        lColour.m_rgba = 0xFF00FF00;
    else
        lColour.m_rgba = 0xFF0000FF;
    const VecFloat lScale = {2.5f, 2.5f, 2.5f, 2.5f};
    lpDisplay->DrawLine(lPosition, TrafficMultAdd(lUp, lScale, lPosition), lColour);
}

// ARTIST 0x827612C8. maVehicleTransforms is read directly, not GetVehicleTransform.
void DebugComponent::DrawAxles(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    CGS_ASSERT(lpDisplay != 0, "lpDisplay");
    for (u32 luVehicle = 0; luVehicle < KU_MAX_TOTAL_TRAFFIC; ++luVehicle)
    {
        const Vehicle* lpVehicle = mpModule->GetVehicle(luVehicle);
        if (!lpVehicle->IsAlive())
            continue;
        const VehicleAxles* lpAxles = mpModule->GetVehicleAxles(luVehicle);
        // The original inlines the transform accessor's pool bound assert.
        CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC, "luIndex < KU_MAX_TOTAL_TRAFFIC");
        const Vector3 lPosition = mpModule->maVehicleTransforms[luVehicle].Pos();
        if (ShouldObjectBeCulled(lPosition, lpDisplay))
            continue;
        if (lpVehicle->IsOfStandardSpecies())
            DrawAxle(&lpAxles->mFrontAxle, lpDisplay);
        if (lpVehicle->IsOfStandardSpecies() || lpVehicle->IsOfTrailerSpecies())
            DrawAxle(&lpAxles->mBackAxle, lpDisplay);
        if ((lpVehicle->muSpecies & 0x80) != 0)
            lpDisplay->DrawText(lPosition + Vector3{0, 2, 0, 0}, "P", 25.0f);
    }
}

} // namespace BrnTraffic

namespace BrnTraffic
{

// ARTIST 0x827507A0. ABI: v1=from, v2=to, r4=packed rw::RGBA, r5=renderer.
void DebugComponent::DrawArrow(Vector3 lFrom, Vector3 lTo, rw::RGBA lColour,
                               CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    namespace Vmx = rw::math::vpu::SLerpDetail;
    const auto lPermute63 = [](Vector3 lValue) -> Vector3
    {
        // vpermwi128 0x63 = word selectors 01,10,00,11 = Y,Z,X,W.
        return {lValue.y, lValue.z, lValue.x, lValue.w};
    };
    const Vector3 lDelta = lTo - lFrom;
    const f32 lfSquaredLength = Vmx::Dot3(lDelta, lDelta);
    const f32 lfInverseLength = Vmx::RefinedReciprocalSqrt(lfSquaredLength, 2);
    const Vector3 lDirection = Vmx::Scale(lDelta, lfInverseLength);
    // Only the scalar magnitude is zero-selected. Direction normalization is unguarded.
    const f32 lfLength = lfSquaredLength == 0.0f ? 0.0f : lfSquaredLength * lfInverseLength;

    const Vector3 lUp = {0, 1, 0, 0};
    const Vector3 lPermutedUp = lPermute63(lUp);
    const Vector3 lPermutedDirection = lPermute63(lDirection);
    const Vector3 lMaskedDirection = {
        lUp.x * lPermutedDirection.x, lUp.y * lPermutedDirection.y,
        lUp.z * lPermutedDirection.z, lUp.w * lPermutedDirection.w};
    // 82750884 raw 141EF6F3 is vmr128 v0,v126. Preserve that copy; do not
    // insert a guessed multiply. Classic vnmsubfp at 827508A0 is B - A*C.
    const Vector3 lCopiedDirection = lDirection;
    const Vector3 lDifference = {
        Vmx::NegativeMultiplySubtract(lPermutedUp.x, lCopiedDirection.x, lMaskedDirection.x),
        Vmx::NegativeMultiplySubtract(lPermutedUp.y, lCopiedDirection.y, lMaskedDirection.y),
        Vmx::NegativeMultiplySubtract(lPermutedUp.z, lCopiedDirection.z, lMaskedDirection.z),
        Vmx::NegativeMultiplySubtract(lPermutedUp.w, lCopiedDirection.w, lMaskedDirection.w)};
    const Vector3 lSideUnnormalised = lPermute63(lDifference);
    const f32 lfSideSquared = Vmx::Dot3(lSideUnnormalised, lSideUnnormalised);
    const f32 lfInverseSide = Vmx::RefinedReciprocalSqrt(lfSideSquared, 2);
    const Vector3 lSide = Vmx::Scale(lSideUnnormalised, lfInverseSide);

    const f32 lfProportionalHead = lfLength * 0.05000000074505806f; // 820C0F50
    const f32 lfSelector = lfProportionalHead - 0.5f;
    // Original fsel: >=0 chooses .5; an unordered selector keeps the proportional value.
    const f32 lfHead = lfSelector >= 0.0f ? 0.5f : lfProportionalHead;
    lpDisplay->DrawLine(lFrom, lTo, lColour);
    const Vector3 lBack = lTo - Vmx::Scale(lDirection, lfHead);
    lpDisplay->DrawLine(lTo, lBack - Vmx::Scale(lSide, lfHead), lColour);
    const Vector3 lOtherHead = {
        std::fmaf(lSide.x, lfHead, lBack.x), std::fmaf(lSide.y, lfHead, lBack.y),
        std::fmaf(lSide.z, lfHead, lBack.z), std::fmaf(lSide.w, lfHead, lBack.w)};
    lpDisplay->DrawLine(lTo, lOtherHead, lColour);
}

} // namespace BrnTraffic

namespace BrnTraffic
{

// ARTIST 0x82755848. DecFIGS .h:3924 confirms types/order; f1/f2 are half-width/height,
// f3/f4 are full-scale/value, and r7/r8 are outline/fill colors. v1=position, v2=offset.
void DebugComponent::RenderVerticalMeter(CgsDev::Debug3DImmediateRender* lpDisplay,
                                         Vector3 lPosition, Vector2 lOffset,
                                         f32 lfHalfWidth, f32 lfHeight,
                                         rw::RGBA lOutlineColour, rw::RGBA lFillColour,
                                         f32 lfFullScale, f32 lfValue) const
{
    CGS_ASSERT(lpDisplay != 0, "lpDisplay");
    const auto lRow = [](const Vector4& lrValue) -> Vector3
    {
        return {lrValue.x, lrValue.y, lrValue.z, lrValue.w};
    };
    const auto lMultiplyAdd = [](Vector3 lA, f32 lfB, Vector3 lC) -> Vector3
    {
        return {std::fmaf(lA.x, lfB, lC.x), std::fmaf(lA.y, lfB, lC.y),
                std::fmaf(lA.z, lfB, lC.z), std::fmaf(lA.w, lfB, lC.w)};
    };
    const Matrix44 lViewProjection = lpDisplay->GetViewProjectionMatrix();
    Vector3 lClip = lMultiplyAdd(lRow(lViewProjection.xAxis), lPosition.x,
                                 lRow(lViewProjection.wAxis));
    lClip = lMultiplyAdd(lRow(lViewProjection.yAxis), lPosition.y, lClip);
    lClip = lMultiplyAdd(lRow(lViewProjection.zAxis), lPosition.z, lClip);
    // Original is 0.001 > clip.z, with no W division and no unordered early-out.
    if (0.0010000000474974513f > lClip.z)
        return;

    VecFloat lDeterminant;
    const Matrix44 lInverse = rw::math::vpu::Inverse(lViewProjection, lDeterminant);
    const Vector3 lUp = lRow(lInverse.yAxis);
    const Vector2 lScreen = lpDisplay->GetVirtualScreenSize();
    const f32 lfInverseWidth = rw::math::vpu::SLerpDetail::RefinedReciprocal(lScreen.x, 2);
    const f32 lfAspect = lfInverseWidth * lScreen.y;
    const Vector3 lHorizontal = rw::math::vpu::SLerpDetail::Scale(lRow(lInverse.xAxis), lfAspect);
    lPosition = lMultiplyAdd(lHorizontal, lOffset.x, lPosition);
    lPosition = lMultiplyAdd(lUp, lOffset.y, lPosition);
    const Vector3 lHalfWidth = rw::math::vpu::SLerpDetail::Scale(lHorizontal, lfHalfWidth);
    const Vector3 lTop = lMultiplyAdd(lUp, lfHeight, lPosition);
    const Vector3 lMiddle = lMultiplyAdd(lUp, lfHeight * 0.5f, lPosition);

    lpDisplay->DrawLine(lPosition, lTop, lOutlineColour);
    const Vector3 lBaseRight = lPosition + lHalfWidth;
    const Vector3 lBaseLeft = lPosition - lHalfWidth;
    lpDisplay->DrawLine(lBaseLeft, lBaseRight, lOutlineColour);
    lpDisplay->DrawLine(lTop - lHalfWidth, lTop + lHalfWidth, lOutlineColour);
    lpDisplay->DrawLine(lMiddle - lHalfWidth, lMiddle + lHalfWidth, lOutlineColour);

    const f32 lfRatio = lfValue / lfFullScale;
    // These two fsel predicates are deliberate. For NaN ratio the second chooses 1;
    // substituting std::clamp, max/min, or a divide-by-zero guard changes the original.
    f32 lfFill = -lfRatio >= 0.0f ? 0.0f : lfRatio;
    lfFill = 1.0f - lfFill >= 0.0f ? lfFill : 1.0f;
    const Vector3 lFill = rw::math::vpu::SLerpDetail::Scale(lUp, lfFill * lfHeight);
    lpDisplay->DrawSolidQuad(lBaseLeft, lBaseRight, lBaseRight + lFill, lBaseLeft + lFill,
                             lFillColour);
}

} // namespace BrnTraffic

int MaybeDrawText(CgsDev::Debug2DImmediateRender*, const char*, f32, f32, f32,
                  CgsDev::RGBA, bool);

namespace BrnTraffic
{

// ARTIST 0x8274FDD0. The last float is the font scale, not a column width.
void DebugComponent::RenderPerfmon(CgsDev::Debug2DImmediateRender* lpDisplay,
                                    const CgsDev::PerfMonCpuMonitorData* lpData,
                                    f32 lfX, f32 lfY, f32 lfScale)
{
    const CgsDev::RGBA lColour = 0xFFFFFFFFu;
    MaybeDrawText(lpDisplay, lpData->mpcName, lfX, lfY, lfScale, lColour, false);
    char lacText[32];
    CgsCore::SPrintf(lacText, sizeof(lacText), "%.03f", lpData->mfCurrentValue);
    MaybeDrawText(lpDisplay, lacText, lfX + 200.0f, lfY, lfScale, lColour, false);
    CgsCore::SPrintf(lacText, sizeof(lacText), "%.03f", lpData->mfAverageValue);
    MaybeDrawText(lpDisplay, lacText, lfX + 270.0f, lfY, lfScale, lColour, false);
    CgsCore::SPrintf(lacText, sizeof(lacText), "%.03f", lpData->mfMinMaxValue);
    MaybeDrawText(lpDisplay, lacText, lfX + 340.0f, lfY, lfScale, lColour, false);
    CgsCore::SPrintf(lacText, sizeof(lacText), "%d", lpData->miNumCalls);
    MaybeDrawText(lpDisplay, lacText, lfX + 410.0f, lfY, lfScale, lColour, false);
}

// ARTIST 0x82754038. Assembly multiplies screen width by 0.5 before adding 50;
// Hex-Rays loses the vector result and prints the misleading expression 0.5+50.
void DebugComponent::RenderPerformanceTable(CgsDev::Debug2DImmediateRender* lpDisplay)
{
    const f32 lfX = lpDisplay->GetVirtualScreenSize().x * 0.5f + 50.0f;
    const CgsDev::RGBA lColour = 0xFFFFFFFFu;
    MaybeDrawText(lpDisplay, "Traffic [special]", lfX, 76.0f, 24.0f, lColour, false);
    MaybeDrawText(lpDisplay, "Current", lfX + 200.0f, 84.0f, 16.0f, lColour, false);
    MaybeDrawText(lpDisplay, "Average", lfX + 270.0f, 84.0f, 16.0f, lColour, false);
    MaybeDrawText(lpDisplay, "Min/Max", lfX + 340.0f, 84.0f, 16.0f, lColour, false);
    MaybeDrawText(lpDisplay, "Calls", lfX + 410.0f, 84.0f, 16.0f, lColour, false);
    MaybeDrawText(lpDisplay, "DECISION", lfX, 100.0f, 16.0f, lColour, false);
    RenderPerfmon(lpDisplay, &mpModule->mPostPhysPerfmonData_Decision, lfX, 116.0f, 16.0f);
    RenderPerfmon(lpDisplay, &mpModule->mParamPerfmonData_Decision, lfX, 132.0f, 16.0f);
    RenderPerfmon(lpDisplay, &mpModule->mVehiclePerfmonData_Decision, lfX, 148.0f, 16.0f);
    MaybeDrawText(lpDisplay, "NON-DECISION", lfX, 180.0f, 16.0f, lColour, false);
    RenderPerfmon(lpDisplay, &mpModule->mPostPhysPerfmonData_NonDecision, lfX, 196.0f, 16.0f);
    RenderPerfmon(lpDisplay, &mpModule->mParamPerfmonData_NonDecision, lfX, 212.0f, 16.0f);
    RenderPerfmon(lpDisplay, &mpModule->mVehiclePerfmonData_NonDecision, lfX, 228.0f, 16.0f);
}

// ARTIST 0x8275B2E8. The grid uses numCells/numCellsX, not numCellsZ. The
// original divides by the stored dimensions without substituting fallback sizes.
void DebugComponent::DrawActiveHulls(CgsDev::Debug2DImmediateRender* lpDisplay) const
{
    CGS_ASSERT(mpModule != 0, "mpModule != NULL");
    const Pvs* lpPvs = mpModule->mpData->mpPvs;
    const u32 luColumns = lpPvs->muNumCells_X;
    const u32 luRows = lpPvs->muNumCells / luColumns;
    const f32 lfStepX = 170.0f / static_cast<f32>(luColumns);
    const f32 lfStepY = 170.0f / static_cast<f32>(luRows);
    const Vector2 lSize = {lfStepX, lfStepY, 0.0f, 0.0f};

    for (u32 luActive = 0; luActive < mpModule->mActiveHulls.GetLength(); ++luActive)
    {
        const u16 luHull = mpModule->mActiveHulls.GetItem(luActive);
        const u32 luHullColumns = mpModule->mpData->mpPvs->muNumCells_X;
        const Vector2 lPosition = {
            std::fmaf(static_cast<f32>(luHull % luHullColumns), lfStepX, 80.0f),
            std::fmaf(static_cast<f32>(luHull / luHullColumns), lfStepY, 80.0f),
            0.0f, 0.0f};
        // Vector DrawBox takes position+size; scalar DrawBox takes min/max.
        lpDisplay->DrawBox(lPosition, lSize, 0x88FFFF00u);
    }

    Vector2 lStart = {80.0f, 80.0f, 0.0f, 0.0f};
    Vector2 lEnd = {80.0f, 250.0f, 0.0f, 0.0f};
    u32 luLines = luColumns + 1;
    do
    {
        lpDisplay->DrawLine(lStart, lEnd, 0xFF000000u);
        --luLines;
        lStart.x += lfStepX;
        lEnd.x += lfStepX;
    } while (luLines);

    lStart = {80.0f, 80.0f, 0.0f, 0.0f};
    lEnd = {250.0f, 80.0f, 0.0f, 0.0f};
    luLines = luRows + 1;
    do
    {
        lpDisplay->DrawLine(lStart, lEnd, 0xFF000000u);
        --luLines;
        lStart.y += lfStepY;
        lEnd.y += lfStepY;
    } while (luLines);

}

// ARTIST 0x8275C938. Even with another local player, this reads car zero and
// emits the original warning. Array::GetItem retains the empty-array assertion.
void DebugComponent::DrawPlayerCarHullId(CgsDev::Debug2DImmediateRender* lpDisplay) const
{
    CGS_ASSERT(mpModule != 0, "mpModule != NULL");
    CGS_ASSERT(lpDisplay != 0, "lpDisplay != NULL");
    if (mpModule->meLocalPlayerIndex != 0)
        MaybeDrawText(lpDisplay, "Warning: Hull ID is for car zero, not player",
                      40.0f, 100.0f, 16.0f, 0xFF0000FFu, false);
    char lacText[256];
    CgsCore::SPrintf(lacText, sizeof(lacText), "Traf Hull: %u",
                    static_cast<u32>(mpModule->maaRaceCarHulls[0].GetItem(0)));
    MaybeDrawText(lpDisplay, lacText, 60.0f, 100.0f, 16.0f, 0xFFFFFFFFu, false);
}

// ARTIST 0x82750AE8. stwu allocates 0x10C0 and the stream's capacity is 0x1000:
// the local buffer is 4096 bytes despite Hex-Rays' 96-byte array declaration.
void DebugComponent::DrawTextWithOffsets(CgsDev::DebugRender& lrDisplay,
                                          CgsDev::StrStream* lpStream,
                                          f32 lfX, f32 lfY) const
{
    char lacLine[4096];
    CgsDev::StrStream lLine(lacLine, sizeof(lacLine));
    s32 liInputIndex = 0;
    s32 liLineCount = 0;
    s32 liCharacter = static_cast<s8>(lpStream->GetBuffer()[liInputIndex]);
    lLine.AppendFormat("%c", liCharacter);
    while (liCharacter != 0)
    {
        if (liCharacter == '\n' || (liLineCount > 128 && liCharacter == ' '))
        {
            lrDisplay.Draw2DText(lLine.GetBuffer(), lfX, lfY, 12.0f, 0xFFFFFFFFu);
            lLine.Reset();
            lfY += 12.0f;
            liLineCount = 0;
        }
        ++liInputIndex;
        ++liLineCount;
        liCharacter = static_cast<s8>(lpStream->GetBuffer()[liInputIndex]);
        lLine.AppendFormat("%c", liCharacter);
    }
    lrDisplay.Draw2DText(lLine.GetBuffer(), lfX, lfY, 12.0f, 0xFFFFFFFFu);
    lLine.Reset();
}

} // namespace BrnTraffic
