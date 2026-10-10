// BrnWorld::RaceCarEntityModuleDebugComponent -- the race-car entity module's debug page.
// See the header banner for the layout and the declaration gate.
//
// Every body below is the console's own, read from its asm:
//   Construct, GetName, AirRamAddNew, ResetAverageSpeedCallback, OnActivate, Update,
//   RenderWorld, RenderTailgateCones, RenderSingleDebugCone, DrawResetOnWaterHeight,
//   RenderHUD, RenderEngineState, RenderTrafficRelated, DrawTextWithOffsets.
//
// Colours: the console words are 0xAARRGGBB. The 3D immediate renderer takes rw::RGBA, spelled
// rw::RGBA(r, g, b, a); the 2D immediate renderer takes the packed word (CgsDev::RGBA).
//
// The console's "draw text at x, y" helper used by these HUDs is the scalar
// Debug2DImmediateRender::DrawText(text, x, y, scale, colour) overload (it builds the Vector2 and
// forwards with lbCentred false), so the bodies call that overload directly.

#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleDebugComponent.h"

#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h"   // module, ActiveRaceCar, RaceCarStreamer
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnActiveRaceCar.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCar.h"               // RaceCar::GetPersistentDamage
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarStreamer.h"
#include "GameSource/BurnoutConstants.h"                                                  // EActiveRaceCarIndex + range-guarded operator++
#include "SharedClasses/World/BrnCollisionTag.h"                                          // CollisionTag::GetTrafficInfo
#include "GameShared/GameClasses/Core/CgsAssert.h"                                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"                                            // CgsIDUnCompress
#include "GameShared/GameClasses/Development/CgsStrStream.h"                              // StrStream / SimpleStrStream
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"            // DebugUI::GetVariableManager
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Variables/CgsVariableManager.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.h"
#include "rw/math/vpu/vector3_operation.h"                                                // Magnitude, operator+/-/*

#include <cmath>                                                                          // atan2, sinf, cosf, tanf

// "Graphics/Vehicles.../Wheels to Render" -- the shared wheel-count switch RenderRaceCar and
// TrafficEntityModule::RenderTrafficCar read (defined beside RenderRaceCar). The console keeps it
// as a u32 and registers it through the u32 variable path.
extern s32 giWheelsToRender;

namespace BrnWorld
{
    // The vehicle LOD policy tables and switches ("Graphics/Vehicles.../LODs..."), defined beside
    // WorldModule::CalculateVehicleLODs, their reader.
    extern f32  KA_VEHICLE_QUALITY_LOD_DISTANCE[5];
    extern f32  KA_VEHICLE_AGGRESSIVE_LOD_DISTANCE[5];
    extern bool sbUseDynamicLods;
    extern bool sbUseFixedLods;
    extern bool sbUseAggressiveLods;
    extern s32  siFixedVehicleLod;

    // The module's tailgating tunables (defined in BrnRaceCarEntityModule_wBT_01.cpp).
    extern f32 KF_Z_OFFSET_FROM_CAR_CENTRE_TO_CONE_APEX;
    extern f32 KF_TAILGATING_CONE_HALF_ANGLE_RADS;
    extern f32 KF_TAILGATING_CONE_DEPTH;
    extern f32 KF_MIN_TAILGATE_DURATION;
    extern f32 KF_TAILGATING_MAX_RELATIVE_VELOCITY_MAGNITUDE;
    extern f32 KF_MIN_TAILGATEE_SPEED;

namespace
{
    namespace vpu = rw::math::vpu;

    // ---- rodata, read out of the image at the addresses the bodies load ----------------------
    const f32 KF_TWO_PI               = 6.28318548f;   // 0x40C90FDB
    const f32 KF_RADIANS_TO_DEGREES   = 57.2957802f;   // 0x42652EE1
    const f32 KF_RADIANS_TO_QUADRANTS = 1.27323949f;   // 0x3FA2F983 (4 / pi)

    // The per-car HUD tables (damage / streaming): one line per active-race-car slot.
    const f32 KF_CAR_TABLE_X          = 100.0f;
    const f32 KF_CAR_TABLE_Y          = 100.0f;
    const f32 KF_CAR_TABLE_LINE_SPACE = 20.0f;
    const f32 KF_CAR_TABLE_TEXT_SIZE  = 16.0f;

    // The average-speed readout.
    const f32 KF_AVERAGE_SPEED_X         = 32.0f;
    const f32 KF_AVERAGE_SPEED_Y         = 32.0f;
    const f32 KF_AVERAGE_SPEED_TEXT_SIZE = 20.0f;

    // The engine-state readout.
    const f32 KF_ENGINE_STATE_X         = 140.0f;
    const f32 KF_ENGINE_STATE_Y         = 120.0f;
    const f32 KF_ENGINE_STATE_TEXT_SIZE = 30.0f;

    // DrawTextWithOffsets: the size it draws at and the line advance (one value), and the line
    // length after which a space breaks the line.
    const f32 KF_WRAPPED_TEXT_SIZE         = 12.0f;
    const s32 KI_WRAPPED_TEXT_BREAK_LENGTH = 128;
    const s32 KI_WRAPPED_TEXT_BUFFER_SIZE  = 4096;

    // RenderTrafficRelated's text block origin and stream size.
    const f32 KF_TRAFFIC_TEXT_X           = 50.0f;
    const f32 KF_TRAFFIC_TEXT_Y           = 50.0f;
    const s32 KI_TRAFFIC_TEXT_BUFFER_SIZE = 5120;

    // RenderHUD's per-line stream size.
    const s32 KI_HUD_LINE_BUFFER_SIZE = 256;

    // RenderWorld / DrawResetOnWaterHeight sphere radii.
    const f32 KF_RESET_SPHERE_RADIUS       = 2.0f;
    const f32 KF_WATER_HEIGHT_SPHERE_RADIUS = 0.1f;

    // RenderTailgateCones: segment count for each cone.
    const s32 KI_TAILGATE_CONE_SEGMENTS = 32;

    // OnActivate ranges and steps.
    const f32 KF_MAGNITUDE_MAX              = 100.0f;
    const f32 KF_CONE_HALF_ANGLE_STEP       = 0.0314159282f;   // 0x3D00ADFD (pi / 100)
    const f32 KF_CONE_DEPTH_STEP            = 2.0f;
    const f32 KF_MIN_DURATION_STEP          = 0.1f;
    const f32 KF_COLOUR_STEP                = 0.05f;
    const f32 KF_ILLUMINATION_STEP          = 0.1f;
    const s32 KI_MAX_PALETTE_INDEX          = 3;
    const s32 KI_MAX_COLOUR_INDEX           = 50;
    const u32 KU_MAX_WHEELS_TO_RENDER       = 4;
    const s32 KI_MAX_FIXED_VEHICLE_LOD      = 4;
    const f32 KF_MAX_LOD_DISTANCE           = 300.0f;
    const f32 KF_MAX_RESET_ON_WATER_HEIGHT  = 0.5f;
    const f32 KF_RESET_ON_WATER_HEIGHT_STEP = 0.01f;

    // CgsCore's speed conversions. KF_MPS_TO_MPH reads zero in the image: a CRT dynamic
    // initialiser stores 1.0f / KF_MPH_TO_MPS into it, which is exactly this expression.
    const f32 KF_MPH_TO_MPS = 0.44704f;
    const f32 KF_MPS_TO_MPH = 1.0f / KF_MPH_TO_MPS;

    const CgsDev::RGBA KRGBA_TEXT_WHITE = 0xFFFFFFFFu;
}

// ================================================================================================
// Lifecycle
// ================================================================================================

// The base Construct, the module pointer (asserted), then the members the console zeroes. The
// average-speed toggle, mbDrawDamageHUD, mbRenderCurrentDistrict, mbAirRam, mMag and the
// intersection array are left as they are, exactly as the console leaves them.
void RaceCarEntityModuleDebugComponent::Construct(RaceCarEntityModule* lpRaceCarEntityModule)
{
    DebugComponent::Construct();

    CGS_ASSERT(lpRaceCarEntityModule != nullptr, "lpRaceCarEntityModule != NULL");

    mpRaceCarEntityModule = lpRaceCarEntityModule;

    mbRenderCarDamageState       = false;
    mbRenderCarSreamingState     = false;
    mbRenderLastResetInfo        = false;
    mbRenderPlayerResetPositions = false;
    mbForceOnlineFreeburnSpawnPosition = false;

    miLastDriftDistance    = 0;
    miLastInAirDistance    = 0;
    miLastOncomingDistance = 0;

    // Identity rotation, zero translation, zero w lanes -- the four rows the console builds on
    // the stack and copies in.
    mLastResetTransform.SetIdentity();

    mbRenderTailgateCones    = false;
    mbShowEngineState        = false;
    mbDrawResetOnWaterHeight = false;
    mfAverageSpeed           = 0.0f;
    miAverageSpeedFrameCount = 0;
    mbTrafficRelatedData     = false;
}

const char* RaceCarEntityModuleDebugComponent::GetName() const
{
    return "Race Car Entity";
}

// "Apply Ram" menu action.
void RaceCarEntityModuleDebugComponent::AirRamAddNew(void* lpData)
{
    static_cast<RaceCarEntityModuleDebugComponent*>(lpData)->mbAirRam = true;
}

// "Reset average speed" menu action.
void RaceCarEntityModuleDebugComponent::ResetAverageSpeedCallback(void* lpData)
{
    RaceCarEntityModuleDebugComponent* lpComponent = static_cast<RaceCarEntityModuleDebugComponent*>(lpData);
    lpComponent->mfAverageSpeed           = 0.0f;
    lpComponent->miAverageSpeedFrameCount = 0;
}

// ================================================================================================
// Debug menu
// ================================================================================================

void RaceCarEntityModuleDebugComponent::OnActivate()
{
    RaceCarEntityModule* lpModule = mpRaceCarEntityModule;

    RegisterVariable(&mbRenderCarDamageState,       "Render car damage state");
    RegisterVariable(&mbRenderCarSreamingState,     "Render car streaming state");
    RegisterVariable(&mbRenderLastResetInfo,        "Show place on track info");
    RegisterVariable(&mbRenderPlayerResetPositions, "Show player reset positions");
    RegisterVariable(&mbShowAverageSpeed,           "Show average speed");
    RegisterFunction(ResetAverageSpeedCallback, this, "Reset average speed");
    RegisterVariable(&lpModule->mbSixaxisSteeringEnabled, "SIXAXIS steering");
    RegisterVariable(&mbShowEngineState,            "Show player engine state");
    RegisterVariable(&mbTrafficRelatedData,         "Traffic Related");
    RegisterFunction(AirRamAddNew, this, "Apply Ram");

    RegisterVariable(&mMag, "Magnitude");
    SetRange(&mMag, 0.0f, KF_MAGNITUDE_MAX);

    RegisterVariable(&lpModule->mbDisplayPlayerCarPosition, "Show Player Car Position");
    RegisterVariable(&mbRenderCurrentDistrict,              "Render current district");

    // ---- tailgating ----
    const char* lpcTailgate = "Tailgate...";
    RegisterVariable(&lpModule->mfCurrentTailgateDuration, lpcTailgate, "Duration");
    RegisterVariable(&mbRenderTailgateCones,               lpcTailgate, "Render Cones");
    RegisterVariable(&KF_TAILGATING_CONE_HALF_ANGLE_RADS,  lpcTailgate, "Cone HalfAngle (Rads)");
    SetStep(&KF_TAILGATING_CONE_HALF_ANGLE_RADS, KF_CONE_HALF_ANGLE_STEP);
    RegisterVariable(&KF_TAILGATING_CONE_DEPTH,            lpcTailgate, "Cone Depth");
    SetStep(&KF_TAILGATING_CONE_DEPTH, KF_CONE_DEPTH_STEP);
    RegisterVariable(&KF_MIN_TAILGATE_DURATION,            lpcTailgate, "Min Duration");
    SetStep(&KF_MIN_TAILGATE_DURATION, KF_MIN_DURATION_STEP);
    RegisterVariable(&KF_TAILGATING_MAX_RELATIVE_VELOCITY_MAGNITUDE, lpcTailgate, "Max Relative Speed");
    RegisterVariable(&KF_MIN_TAILGATEE_SPEED,              lpcTailgate, "Min Tailgate Speed");

    // ---- race-car rendering ----
    const char* lpcDeformation = "Physics.../Deformation...";
    RegisterVariable(&lpModule->mbRenderCarsDuringCrash, lpcDeformation, "Render Race Cars");
    RegisterVariable(&lpModule->mbRenderWheels,          lpcDeformation, "Render Wheels   ");

    // ---- colour overrides ----
    const char* lpcColour = "Graphics/Vehicles.../Colour...";
    RegisterVariable(&lpModule->DEBUG_mbOverrideCarColor,    lpcColour, "Override Colour");
    RegisterVariable(&lpModule->DEBUG_mfOverridePaintColorR, lpcColour, "Paint R");
    RegisterVariable(&lpModule->DEBUG_mfOverridePaintColorG, lpcColour, "Paint G");
    RegisterVariable(&lpModule->DEBUG_mfOverridePaintColorB, lpcColour, "Paint B");
    RegisterVariable(&lpModule->DEBUG_mfOverridePearlColorR, lpcColour, "Pearl R");
    RegisterVariable(&lpModule->DEBUG_mfOverridePearlColorG, lpcColour, "Pearl G");
    RegisterVariable(&lpModule->DEBUG_mfOverridePearlColorB, lpcColour, "Pearl B");
    RegisterVariable(&lpModule->DEBUG_mbOverrideCarPalette,  lpcColour, "Override Palette");
    RegisterVariable(&lpModule->DEBUG_miPaletteIndex,        lpcColour, "Palette Index");
    RegisterVariable(&lpModule->DEBUG_miColourIndex,         lpcColour, "Colour Index");
    SetRange(&lpModule->DEBUG_mfOverridePaintColorR, 0.0f, 1.0f);
    SetRange(&lpModule->DEBUG_mfOverridePaintColorG, 0.0f, 1.0f);
    SetRange(&lpModule->DEBUG_mfOverridePaintColorB, 0.0f, 1.0f);
    SetRange(&lpModule->DEBUG_mfOverridePearlColorR, 0.0f, 1.0f);
    SetRange(&lpModule->DEBUG_mfOverridePearlColorG, 0.0f, 1.0f);
    SetRange(&lpModule->DEBUG_mfOverridePearlColorB, 0.0f, 1.0f);
    SetStep(&lpModule->DEBUG_mfOverridePaintColorR, KF_COLOUR_STEP);
    SetStep(&lpModule->DEBUG_mfOverridePaintColorG, KF_COLOUR_STEP);
    SetStep(&lpModule->DEBUG_mfOverridePaintColorB, KF_COLOUR_STEP);
    SetStep(&lpModule->DEBUG_mfOverridePearlColorR, KF_COLOUR_STEP);
    SetStep(&lpModule->DEBUG_mfOverridePearlColorG, KF_COLOUR_STEP);
    SetStep(&lpModule->DEBUG_mfOverridePearlColorB, KF_COLOUR_STEP);
    SetRange(&lpModule->DEBUG_miPaletteIndex, 0, KI_MAX_PALETTE_INDEX);
    SetRange(&lpModule->DEBUG_miColourIndex,  0, KI_MAX_COLOUR_INDEX);

    // ---- damage overrides ----
    const char* lpcDamage = "Graphics/Vehicles.../Damage...";
    RegisterVariable(&lpModule->DEBUG_mbOverrideDamage,       lpcDamage, "Override Damage");
    RegisterVariable(&lpModule->DEBUG_mfVehicleScratchAmount, lpcDamage, "Scratch Amount");
    RegisterVariable(&lpModule->DEBUG_mfVehicleDustAmount,    lpcDamage, "Dust Amount");
    RegisterVariable(&lpModule->DEBUG_mfVehicleCrumpleAmount, lpcDamage, "Crumple Amount");
    SetRange(&lpModule->DEBUG_mfVehicleScratchAmount, 0.0f, 1.0f);
    SetRange(&lpModule->DEBUG_mfVehicleDustAmount,    0.0f, 1.0f);
    SetRange(&lpModule->DEBUG_mfVehicleCrumpleAmount, 0.0f, 1.0f);
    SetStep(&lpModule->DEBUG_mfVehicleScratchAmount, KF_COLOUR_STEP);
    SetStep(&lpModule->DEBUG_mfVehicleDustAmount,    KF_COLOUR_STEP);
    SetStep(&lpModule->DEBUG_mfVehicleCrumpleAmount, KF_COLOUR_STEP);

    // ---- self-illumination overrides ----
    const char* lpcIllumination = "Graphics/Vehicles.../Illumination...";
    RegisterVariable(&lpModule->DEBUG_mfSelfIlluminationR, lpcIllumination, "Brake / High Beam");
    RegisterVariable(&lpModule->DEBUG_mfSelfIlluminationG, lpcIllumination, "Driving");
    RegisterVariable(&lpModule->DEBUG_mfSelfIlluminationB, lpcIllumination, "Reversing");
    SetRange(&lpModule->DEBUG_mfSelfIlluminationR, 0.0f, 1.0f);
    SetRange(&lpModule->DEBUG_mfSelfIlluminationG, 0.0f, 1.0f);
    SetRange(&lpModule->DEBUG_mfSelfIlluminationB, 0.0f, 1.0f);
    SetStep(&lpModule->DEBUG_mfSelfIlluminationR, KF_ILLUMINATION_STEP);
    SetStep(&lpModule->DEBUG_mfSelfIlluminationG, KF_ILLUMINATION_STEP);
    SetStep(&lpModule->DEBUG_mfSelfIlluminationB, KF_ILLUMINATION_STEP);

    // ---- wheel count: registered under its absolute path, straight into the variable manager ----
    u32* lpuWheelsToRender = reinterpret_cast<u32*>(&giWheelsToRender);   // a u32 on the console
    GetUI().GetVariableManager().RegisterVariable(lpuWheelsToRender, "Graphics/Vehicles...", "Wheels to Render");
    SetRange(lpuWheelsToRender, 0u, KU_MAX_WHEELS_TO_RENDER);

    // ---- vehicle LODs ----
    const char* lpcLods = "Graphics/Vehicles.../LODs...";
    RegisterVariable(&sbUseDynamicLods,    lpcLods, "Use Dynamic LODs");
    RegisterVariable(&sbUseFixedLods,      lpcLods, "Use Fixed LODs");
    RegisterVariable(&sbUseAggressiveLods, lpcLods, "Use Aggressive LODs");
    RegisterVariable(&siFixedVehicleLod,   lpcLods, "Vehicle LOD");
    RegisterVariable(&KA_VEHICLE_QUALITY_LOD_DISTANCE[0],    lpcLods, "Quality LOD 0");
    RegisterVariable(&KA_VEHICLE_QUALITY_LOD_DISTANCE[1],    lpcLods, "Quality LOD 1");
    RegisterVariable(&KA_VEHICLE_QUALITY_LOD_DISTANCE[2],    lpcLods, "Quality LOD 2");
    RegisterVariable(&KA_VEHICLE_QUALITY_LOD_DISTANCE[3],    lpcLods, "Quality LOD 3");
    RegisterVariable(&KA_VEHICLE_QUALITY_LOD_DISTANCE[4],    lpcLods, "Quality LOD 4");
    RegisterVariable(&KA_VEHICLE_AGGRESSIVE_LOD_DISTANCE[0], lpcLods, "Aggressive LOD 0");
    RegisterVariable(&KA_VEHICLE_AGGRESSIVE_LOD_DISTANCE[1], lpcLods, "Aggressive LOD 1");
    RegisterVariable(&KA_VEHICLE_AGGRESSIVE_LOD_DISTANCE[2], lpcLods, "Aggressive LOD 2");
    RegisterVariable(&KA_VEHICLE_AGGRESSIVE_LOD_DISTANCE[3], lpcLods, "Aggressive LOD 3");
    RegisterVariable(&KA_VEHICLE_AGGRESSIVE_LOD_DISTANCE[4], lpcLods, "Aggressive LOD 4");
    SetRange(&siFixedVehicleLod, 0, KI_MAX_FIXED_VEHICLE_LOD);
    for (s32 liLod = 0; liLod < 5; ++liLod)
    {
        SetRange(&KA_VEHICLE_QUALITY_LOD_DISTANCE[liLod], 0.0f, KF_MAX_LOD_DISTANCE);
    }
    for (s32 liLod = 0; liLod < 5; ++liLod)
    {
        SetStep(&KA_VEHICLE_QUALITY_LOD_DISTANCE[liLod], 1.0f);
    }
    for (s32 liLod = 0; liLod < 5; ++liLod)
    {
        SetRange(&KA_VEHICLE_AGGRESSIVE_LOD_DISTANCE[liLod], 0.0f, KF_MAX_LOD_DISTANCE);
    }
    for (s32 liLod = 0; liLod < 5; ++liLod)
    {
        SetStep(&KA_VEHICLE_AGGRESSIVE_LOD_DISTANCE[liLod], 1.0f);
    }

    // ---- reset on water ----
    RegisterVariable(&mbDrawResetOnWaterHeight, "Draw reset on water height");
    RegisterVariable(&lpModule->mfResetOnWaterHeight, "Reset on water height");
    SetRange(&lpModule->mfResetOnWaterHeight, 0.0f, KF_MAX_RESET_ON_WATER_HEIGHT);
    SetStep(&lpModule->mfResetOnWaterHeight, KF_RESET_ON_WATER_HEIGHT_STEP);

    RegisterVariable(&mbForceOnlineFreeburnSpawnPosition, "Force Online Freeburn Spawn Position");
}

// ================================================================================================
// Per-frame
// ================================================================================================

// The player's running mean speed (m/s): new mean = (speed + mean * n) / (n + 1).
void RaceCarEntityModuleDebugComponent::Update()
{
    ActiveRaceCar* lpPlayerCar =
        mpRaceCarEntityModule->GetActiveRaceCar(mpRaceCarEntityModule->GetPlayerActiveRaceCarIndex());

    if (lpPlayerCar != nullptr)
    {
        const f32 lfSpeed = vpu::Magnitude(lpPlayerCar->GetVelocity());

        const s32 liFrameCount = miAverageSpeedFrameCount;
        miAverageSpeedFrameCount = liFrameCount + 1;

        mfAverageSpeed = (static_cast<f32>(liFrameCount) * mfAverageSpeed + lfSpeed)
                       / static_cast<f32>(liFrameCount + 1);
    }
}

// ================================================================================================
// World overlays
// ================================================================================================

void RaceCarEntityModuleDebugComponent::RenderWorld(CgsDev::Debug3DImmediateRender* lpRender)
{
    const rw::RGBA lWhite(0xFF, 0xFF, 0xFF, 0xFF);

    if (mbRenderTailgateCones)
    {
        RenderTailgateCones(lpRender);
    }

    // The last place-on-track: its transform, its line test and every intersection it found.
    if (mbRenderLastResetInfo)
    {
        lpRender->DrawAxis(mLastResetTransform);
        lpRender->DrawLine(mLastResetLineTestStart, mLastResetLineTestEnd, lWhite);

        for (u32 luIntersection = 0; luIntersection < mLastResetLineTestIntersections.GetLength(); ++luIntersection)
        {
            lpRender->DrawSphere(mLastResetLineTestIntersections.GetItem(luIntersection), KF_RESET_SPHERE_RADIUS, lWhite);
        }
    }

    // The player's reset ring: one sphere at the position row of every recorded transform.
    if (mbRenderPlayerResetPositions)
    {
        ActiveRaceCar* lpPlayerCar =
            mpRaceCarEntityModule->GetActiveRaceCar(mpRaceCarEntityModule->GetPlayerActiveRaceCarIndex());

        for (s32 liTransform = 0; liTransform < lpPlayerCar->mPrevTransforms.GetLength(); ++liTransform)
        {
            lpRender->DrawSphere(lpPlayerCar->mPrevTransforms[static_cast<u32>(liTransform)].wAxis,
                                 KF_RESET_SPHERE_RADIUS, lWhite);
        }
    }

    if (mbDrawResetOnWaterHeight)
    {
        DrawResetOnWaterHeight(lpRender);
    }
}

// One translucent cone per other active car, apex KF_Z_OFFSET_FROM_CAR_CENTRE_TO_CONE_APEX ahead
// of its centre along its direction and opening KF_TAILGATING_CONE_DEPTH back along its z axis.
void RaceCarEntityModuleDebugComponent::RenderTailgateCones(CgsDev::Debug3DImmediateRender* lpRender)
{
    const EActiveRaceCarIndex lePlayerIndex = mpRaceCarEntityModule->GetPlayerActiveRaceCarIndex();
    ActiveRaceCar* lpPlayerCar = &mpRaceCarEntityModule->maActiveRaceCars[lePlayerIndex];

    // The console fetches the player's position and physics state here and reads neither; the
    // physics-state accessor carries the IsAttached() guard.
    const Vector3 lPlayerPosition = lpPlayerCar->GetPosition();
    CGS_ASSERT(lpPlayerCar->IsAttached(), "IsAttached()");
    const BrnPhysics::Vehicle::RaceCarState* lpPlayerState = lpPlayerCar->GetPhysicsState();
    (void)lPlayerPosition;
    (void)lpPlayerState;

    const rw::RGBA lConeColour(0x32, 0xB4, 0x00, 0x50);

    for (EActiveRaceCarIndex leIndex = E_ACTIVE_RACE_CAR_INDEX_0; leIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT; leIndex++)
    {
        ActiveRaceCar* lpCar = &mpRaceCarEntityModule->maActiveRaceCars[leIndex];

        if (leIndex == lePlayerIndex || !lpCar->IsActive())
        {
            continue;
        }

        Matrix44Affine lConeTransform = lpCar->GetTransform();
        const Vector3  lDirection     = lpCar->GetDirection();

        lConeTransform.wAxis = lDirection * KF_Z_OFFSET_FROM_CAR_CENTRE_TO_CONE_APEX + lConeTransform.wAxis;
        lConeTransform.zAxis = lConeTransform.zAxis * -KF_TAILGATING_CONE_DEPTH;

        const VecFloat lvfHalfAngle = { KF_TAILGATING_CONE_HALF_ANGLE_RADS, KF_TAILGATING_CONE_HALF_ANGLE_RADS,
                                        KF_TAILGATING_CONE_HALF_ANGLE_RADS, KF_TAILGATING_CONE_HALF_ANGLE_RADS };

        RenderSingleDebugCone(lpRender, lConeTransform, lvfHalfAngle, lConeColour, KI_TAILGATE_CONE_SEGMENTS);
    }
}

// A solid cone with its apex at the transform's position row and its base circle at the end of
// the z axis, radius |z| * tan(half angle), built from liNumSegments double-sided triangles (each
// drawn as a degenerate quad in both windings).
void RaceCarEntityModuleDebugComponent::RenderSingleDebugCone(CgsDev::Debug3DImmediateRender* lpRender,
                                                              Matrix44Affine lTransform, VecFloat lvfHalfAngle,
                                                              rw::RGBA lColour, s32 liNumSegments) const
{
    const f32 lfRadius = vpu::Magnitude(lTransform.zAxis) * tanf(lvfHalfAngle.x);
    const f32 lfStep   = KF_TWO_PI / static_cast<f32>(liNumSegments);

    for (s32 liSegment = 0; liSegment < liNumSegments; ++liSegment)
    {
        const f32 lfAngle     = lfStep * static_cast<f32>(liSegment);
        const f32 lfNextAngle = lfAngle + lfStep;

        Vector3 lEdge = lTransform.yAxis * sinf(lfAngle);
        lEdge = lTransform.xAxis * cosf(lfAngle) + lEdge;
        lEdge = lEdge * lfRadius + lTransform.zAxis;

        Vector3 lNextEdge = lTransform.yAxis * sinf(lfNextAngle);
        lNextEdge = lTransform.xAxis * cosf(lfNextAngle) + lNextEdge;
        lNextEdge = lNextEdge * lfRadius + lTransform.zAxis;

        const Vector3 lApex = lTransform.wAxis;
        lpRender->DrawSolidQuad(lApex, lApex, lApex + lEdge, lApex + lNextEdge, lColour);
        lpRender->DrawSolidQuad(lApex, lApex, lApex + lNextEdge, lApex + lEdge, lColour);
    }
}

// An arrow from the player's lowest point down by the module's reset-on-water height, and a
// small sphere at the lowest point.
void RaceCarEntityModuleDebugComponent::DrawResetOnWaterHeight(CgsDev::Debug3DImmediateRender* lpRender)
{
    const rw::RGBA lWhite(0xFF, 0xFF, 0xFF, 0xFF);

    ActiveRaceCar* lpPlayerCar =
        mpRaceCarEntityModule->GetActiveRaceCar(mpRaceCarEntityModule->GetPlayerActiveRaceCarIndex());

    Vector3 lLowestPoint = lpPlayerCar->GetPosition();
    Vector3 lWaterLine   = lLowestPoint;
    lLowestPoint.y = lpPlayerCar->mvfLowestPointWorldSpace.x;
    lWaterLine.y   = lLowestPoint.y - mpRaceCarEntityModule->mfResetOnWaterHeight;

    lpRender->DrawArrow(lLowestPoint, lWaterLine, lWhite);
    lpRender->DrawSphere(lLowestPoint, KF_WATER_HEIGHT_SPHERE_RADIUS, lWhite);
}

// ================================================================================================
// HUD
// ================================================================================================

void RaceCarEntityModuleDebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender* lpRender)
{
    // One line per active-race-car slot: "<slot>:" then, for an active car,
    // "Damaged: <bool>, <persistent damage>".
    if (mbRenderCarDamageState)
    {
        for (EActiveRaceCarIndex leIndex = E_ACTIVE_RACE_CAR_INDEX_0; leIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT; leIndex++)
        {
            CgsDev::SimpleStrStream lLine;
            ActiveRaceCar* lpCar = mpRaceCarEntityModule->GetActiveRaceCar(leIndex);

            lLine << static_cast<s32>(leIndex) << ":";

            if (lpCar->IsActive())
            {
                const bool lbDamaged = lpCar->GetRenderParams()->IsDamaged();
                lLine << "Damaged: " << (lbDamaged ? "true" : "false") << ", ";
                lLine << lpCar->GetGlobalRaceCar()->GetPersistentDamage();
            }

            lpRender->DrawText(lLine.GetBuffer(), KF_CAR_TABLE_X,
                               static_cast<f32>(leIndex) * KF_CAR_TABLE_LINE_SPACE + KF_CAR_TABLE_Y,
                               KF_CAR_TABLE_TEXT_SIZE, KRGBA_TEXT_WHITE);
        }
    }

    // One line per streamer slot: the loaded model, its Unused / Loaded state and the desired
    // model with its priority, or "Inactive".
    if (mbRenderCarSreamingState)
    {
        RaceCarStreamer& lrStreamer = mpRaceCarEntityModule->mRaceCarStreamer;

        for (s32 liActiveRaceCar = 0; liActiveRaceCar < RaceCarStreamer::KI_MAX_ACTIVE_RACE_CARS; ++liActiveRaceCar)
        {
            char lacLine[KI_HUD_LINE_BUFFER_SIZE];
            CgsDev::StrStream lLine(lacLine, KI_HUD_LINE_BUFFER_SIZE);

            lLine << liActiveRaceCar << ": ";

            if (lrStreamer.IsRaceCarActive(liActiveRaceCar))
            {
                char lacId[KI_CGSID_STRING_LEN];
                CgsIDUnCompress(lrStreamer.GetCarModelId(liActiveRaceCar), lacId);
                lLine << lacId;

                if (lrStreamer.maxLoadFlags[liActiveRaceCar] & RaceCarStreamer::E_LOADFLAG_UNUSED)
                {
                    lLine << ", Unused";
                }

                if (lrStreamer.IsRaceCarLoaded(liActiveRaceCar))
                {
                    lLine << ", Loaded";
                }

                if (lrStreamer.maDesiredCarIds[liActiveRaceCar] != 0)
                {
                    CgsIDUnCompress(lrStreamer.maDesiredCarIds[liActiveRaceCar], lacId);
                    lLine << ", (Desired: " << lacId << ", Priority: ";
                    lLine << lrStreamer.maDesiredPriorities[liActiveRaceCar] << ")";
                }
            }
            else
            {
                lLine << "Inactive";
            }

            lpRender->DrawText(lLine.GetBuffer(), KF_CAR_TABLE_X,
                               static_cast<f32>(liActiveRaceCar) * KF_CAR_TABLE_LINE_SPACE + KF_CAR_TABLE_Y,
                               KF_CAR_TABLE_TEXT_SIZE, KRGBA_TEXT_WHITE);
        }
    }

    if (mbShowAverageSpeed)
    {
        char lacLine[KI_HUD_LINE_BUFFER_SIZE];
        CgsDev::StrStream lLine(lacLine, KI_HUD_LINE_BUFFER_SIZE);

        lLine << "Average speed: " << static_cast<s32>(mfAverageSpeed * KF_MPS_TO_MPH) << " mph";

        lpRender->DrawText(lLine.GetBuffer(), KF_AVERAGE_SPEED_X, KF_AVERAGE_SPEED_Y,
                           KF_AVERAGE_SPEED_TEXT_SIZE, KRGBA_TEXT_WHITE);
    }

    if (mbShowEngineState)
    {
        RenderEngineState(lpRender);
    }

    if (mbTrafficRelatedData)
    {
        RenderTrafficRelated(lpRender);
    }
}

void RaceCarEntityModuleDebugComponent::RenderEngineState(CgsDev::Debug2DImmediateRender* lpRender)
{
    ActiveRaceCar* lpPlayerCar =
        mpRaceCarEntityModule->GetActiveRaceCar(mpRaceCarEntityModule->GetPlayerActiveRaceCarIndex());

    // A function-local static: constructed on first use, as the console guards it.
    static const rw::RGBA skEngineStateColour(0x14, 0xB4, 0x14, 0xFF);

    const Vector2 lPosition = { KF_ENGINE_STATE_X, KF_ENGINE_STATE_Y, 0.0f, 0.0f };

    switch (lpPlayerCar->GetEngineState())
    {
        case RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_OFF:
            lpRender->DrawText("ENGINE OFF", lPosition, KF_ENGINE_STATE_TEXT_SIZE, skEngineStateColour.m_rgba, false);
            break;

        case RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_STARTING:
            lpRender->DrawText("ENGINE STARTING", lPosition, KF_ENGINE_STATE_TEXT_SIZE, skEngineStateColour.m_rgba, false);
            break;

        case RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_RUNNING:
            lpRender->DrawText("ENGINE RUNNING", lPosition, KF_ENGINE_STATE_TEXT_SIZE, skEngineStateColour.m_rgba, false);
            break;

        case RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_STOPPING:
            lpRender->DrawText("ENGINE STOPPING", lPosition, KF_ENGINE_STATE_TEXT_SIZE, skEngineStateColour.m_rgba, false);
            break;

        default:
            break;
    }
}

// The player's heading (degrees, [0, 360)) and the traffic direction of the surface under the
// car, with its quadrant.
void RaceCarEntityModuleDebugComponent::RenderTrafficRelated(CgsDev::Debug2DImmediateRender* lpRender)
{
    char lacText[KI_TRAFFIC_TEXT_BUFFER_SIZE];
    CgsDev::StrStream lText(lacText, KI_TRAFFIC_TEXT_BUFFER_SIZE);

    ActiveRaceCar* lpPlayerCar =
        mpRaceCarEntityModule->GetActiveRaceCar(mpRaceCarEntityModule->GetPlayerActiveRaceCarIndex());
    CGS_ASSERT(lpPlayerCar, "lpPlayerCar");

    CGS_ASSERT(lpPlayerCar->IsAttached(), "IsAttached()");   // the physics-state accessor's guard
    const BrnPhysics::Vehicle::RaceCarState* lpRaceCarState = lpPlayerCar->GetPhysicsState();
    CGS_ASSERT(lpRaceCarState, "lpRaceCarState");

    if (!lpRaceCarState->mAboveGroundTestResult.mbValid)
    {
        return;
    }

    // The tag under the car, copied out of the physics state (the state carries it as the packed
    // group/material word; same unpacking as the module's own reader of this field).
    const u32 luPackedTag = lpRaceCarState->mAboveGroundTestResult.mCollisionTag.muValue;
    CollisionTag lCollisionTag;
    lCollisionTag.Construct(static_cast<u16>(luPackedTag >> 16), static_cast<u16>(luPackedTag));

    f32 lfPlayerAngle = static_cast<f32>(atan2(static_cast<f64>(lpRaceCarState->mTransform.zAxis.z),
                                               static_cast<f64>(lpRaceCarState->mTransform.zAxis.x)));
    if (lfPlayerAngle < 0.0f)
    {
        lfPlayerAngle += KF_TWO_PI;
    }

    lText << "Player Direction: " << lfPlayerAngle * KF_RADIANS_TO_DEGREES << "\n";

    f32 lfTrafficAngle = 0.0f;
    const TrafficDirection leTrafficDirection = lCollisionTag.GetTrafficInfo(&lfTrafficAngle);
    const f32 lfQuadrant = lfTrafficAngle * KF_RADIANS_TO_QUADRANTS;

    if (leTrafficDirection == E_TRAFFIC_DIRECTION_VALID)
    {
        lText << "Traffic Direction: " << lfTrafficAngle * KF_RADIANS_TO_DEGREES
              << " [Quadrant: " << lfQuadrant << " ]\n";
    }
    else if (leTrafficDirection == E_TRAFFIC_DIRECTION_NO_LANES)
    {
        lText << "Traffic Direction: No Lanes\n";
    }
    else
    {
        lText << "Traffic Direction: Invalid\n";
    }

    DrawTextWithOffsets(lpRender, &lText, KF_TRAFFIC_TEXT_X, KF_TRAFFIC_TEXT_Y);
}

// Draw a stream's text one line at a time: a line ends at a newline, or at the first space once it
// is longer than KI_WRAPPED_TEXT_BREAK_LENGTH characters, and each line drops KF_WRAPPED_TEXT_SIZE.
// The breaking character stays at the end of the line it ends.
void RaceCarEntityModuleDebugComponent::DrawTextWithOffsets(CgsDev::Debug2DImmediateRender* lpRender,
                                                            CgsDev::StrStream* lpStream, f32 lfX, f32 lfY)
{
    char lacLine[KI_WRAPPED_TEXT_BUFFER_SIZE];
    CgsDev::StrStream lLine(lacLine, KI_WRAPPED_TEXT_BUFFER_SIZE);

    const char* lpcText = lpStream->GetBuffer();
    s32 liIndex      = 0;
    s32 liLineLength = 0;

    char lcChar = lpcText[liIndex];
    lLine.AppendFormat("%c", lcChar);

    while (lcChar != '\0')
    {
        if (lcChar == '\n' || (liLineLength > KI_WRAPPED_TEXT_BREAK_LENGTH && lcChar == ' '))
        {
            lpRender->DrawText(lLine.GetBuffer(), lfX, lfY, KF_WRAPPED_TEXT_SIZE, KRGBA_TEXT_WHITE);
            lLine.Reset();
            lfY += KF_WRAPPED_TEXT_SIZE;
            liLineLength = 0;
        }

        ++liIndex;
        ++liLineLength;
        lcChar = lpcText[liIndex];
        lLine.AppendFormat("%c", lcChar);
    }

    lpRender->DrawText(lLine.GetBuffer(), lfX, lfY, KF_WRAPPED_TEXT_SIZE, KRGBA_TEXT_WHITE);
    lLine.Reset();
}

} // namespace BrnWorld
