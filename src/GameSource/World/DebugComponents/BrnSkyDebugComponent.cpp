// BrnWorld::EnvironmentSettings::DebugComponent, reconstructed from the
// BURNOUT_X360_ARTIST.XEX routines in BrnSkyDebugComponent.cpp.

#include "GameSource/World/DebugComponents/BrnSkyDebugComponent.h"

#include "GameSource/World/EnvironmentManager/BrnEnvironmentManager.h"
#include "GameSource/World/EnvironmentSettings/BrnEnvironmentSettings.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"
#include "SharedClasses/World/BrnEnvironmentDictionary.h"
#include "SharedClasses/World/BrnEnvironmentUtil.h"

#include <cmath>

namespace BrnWorld
{
namespace EnvironmentSettings
{
namespace
{
    const f32 KF_SECONDS_PER_MINUTE = 60.0f;
    const f32 KF_SUN_ELEVATION_ZERO_SECONDS = 23400.0f;
    const f32 KF_RADIANS_PER_SECOND_OF_DAY = 0.000072722054f;
    const f32 KF_JUNKYARD_MAX_VERTICAL = 0.98900002f;

    f32 ClockToSeconds(u32 luHours, u32 luMinutes, u32 luSeconds)
    {
        return (static_cast<f32>(luHours) * KF_SECONDS_PER_MINUTE
              + static_cast<f32>(luMinutes)) * KF_SECONDS_PER_MINUTE
              + static_cast<f32>(luSeconds);
    }
}

// @0x827C7668. The first call is DebugComponent::Construct; IDA names the
// empty folded body after an unrelated class.
void DebugComponent::Construct(EnvironmentManager* lpEnvironmentManager)
{
    CgsDev::DebugComponent::Construct();
    CGS_ASSERT(lpEnvironmentManager != nullptr, "lpEnvironmentManager != NULL");

    mpEnvironmentManager = lpEnvironmentManager;
    HH_MM_SS(&muTimeOfDay_HH, &muTimeOfDay_MM, &muTimeOfDay_SS, 46800.0f);

    mbUpdateValuesFromTool = false;
    mbPrintDebugInfo = false;
    mbSimulateTimeOfDay = true;
    mfTimeOfDayDelta = std::fabs(54.0f);
    mfCloudDelta = std::fabs(15.0f);

    mafJunkyardKeyLightDirection[0] = 0.0f;
    mafJunkyardKeyLightDirection[1] = -0.98799998f;
    mafJunkyardKeyLightDirection[2] = 0.0f;
    mbCalculateJunkyardKeyLightDirectionFromTime = false;
    mbOverrideJunkyardKeyLightDirection = false;
    mbGetJunkyardKeyLightDirection = false;

    muSunElevTodLBoundHH = 0;
    muSunElevTodLBoundMM = 0;
    muSunElevTodLBoundSS = 0;
    muSunElevTodUBoundHH = 0;
    muSunElevTodUBoundMM = 0;
    muSunElevTodUBoundSS = 0;

    ManagerToDebug();
}

void DebugComponent::Destruct()
{
    CgsDev::DebugComponent::Destruct();
}

const char* DebugComponent::GetName() const
{
    return "Environment";
}

// DecFIGS emits the override as a distinct function returning its shared empty
// string. ARTIST folds it away, which is why there is no separate X360 export.
const char* DebugComponent::GetPath() const
{
    return "";
}

// @0x827B2408. Registration order is menu order.
void DebugComponent::OnActivate()
{
    const char* const KPC_ROOT = "";

    const auto RegisterFloat = [this](f32* lpfValue, const char* lpcGroup,
                                      const char* lpcName, f32 lfMin, f32 lfMax, f32 lfStep)
    {
        RegisterVariable(lpfValue, lpcGroup, lpcName);
        SetRange(lpfValue, lfMin, lfMax);
        SetStep(lpfValue, lfStep);
    };
    const auto RegisterClock = [this](u32* lpuValue, const char* lpcGroup,
                                      const char* lpcName, u32 luMax)
    {
        RegisterVariable(lpuValue, lpcGroup, lpcName);
        SetRange(lpuValue, 0u, luMax);
    };

    RegisterVariable(&mbUpdateValuesFromTool, KPC_ROOT, "Update values from Tool");
    RegisterVariable(&mbPrintDebugInfo, KPC_ROOT, "Print debug information");
    RegisterVariable(&mbSimulateTimeOfDay, KPC_ROOT, "Simulate time of day");
    RegisterVariable(&mpEnvironmentManager->mbUseDefaultEffects, KPC_ROOT,
                     "Override effects in time of day");

    RegisterClock(&muTimeOfDay_HH, KPC_ROOT, "Hours", 23);
    RegisterClock(&muTimeOfDay_MM, KPC_ROOT, "Minutes", 59);
    RegisterClock(&muTimeOfDay_SS, KPC_ROOT, "Seconds", 59);
    RegisterFloat(&mfTimeOfDayDelta, KPC_ROOT, "TOD seconds per frame", 0.0f, 2000.0f, 1.0f);
    RegisterFloat(&mfCloudDelta, KPC_ROOT, "Clouds seconds per frame", 0.0f, 2000.0f, 1.0f);

    RegisterFloat(&mfKeyLightColour_r, KPC_ROOT, "KeyLight R", 0.0f, 10.0f, 0.05f);
    RegisterFloat(&mfKeyLightColour_g, KPC_ROOT, "KeyLight G", 0.0f, 10.0f, 0.05f);
    RegisterFloat(&mfKeyLightColour_b, KPC_ROOT, "KeyLight B", 0.0f, 10.0f, 0.05f);
    RegisterFloat(&mfSpecularColour_r, KPC_ROOT, "Specular R", 0.0f, 10.0f, 0.05f);
    RegisterFloat(&mfSpecularColour_g, KPC_ROOT, "Specular G", 0.0f, 10.0f, 0.05f);
    RegisterFloat(&mfSpecularColour_b, KPC_ROOT, "Specular B", 0.0f, 10.0f, 0.05f);

    RegisterFloat(&mfSkyTopColour_r, "Sky colours", "Top Colour R", 0.0f, 10.0f, 0.01f);
    RegisterFloat(&mfSkyTopColour_g, "Sky colours", "Top Colour G", 0.0f, 10.0f, 0.01f);
    RegisterFloat(&mfSkyTopColour_b, "Sky colours", "Top Colour B", 0.0f, 10.0f, 0.01f);
    RegisterFloat(&mfSkyHorColour_r, "Sky colours", "Hor Colour R", 0.0f, 10.0f, 0.01f);
    RegisterFloat(&mfSkyHorColour_g, "Sky colours", "Hor Colour G", 0.0f, 10.0f, 0.01f);
    RegisterFloat(&mfSkyHorColour_b, "Sky colours", "Hor Colour B", 0.0f, 10.0f, 0.01f);
    RegisterFloat(&mfSkyHorPow, "Sky colours", "Sky Shape", 0.0f, 10.0f, 0.1f);
    RegisterFloat(&mfSkySunColour_r, "Sky colours", "Sun Colour R", 0.0f, 10.0f, 0.01f);
    RegisterFloat(&mfSkySunColour_g, "Sky colours", "Sun Colour G", 0.0f, 10.0f, 0.01f);
    RegisterFloat(&mfSkySunColour_b, "Sky colours", "Sun Colour B", 0.0f, 10.0f, 0.01f);
    RegisterFloat(&mfSkySunPow, "Sky colours", "Sun Shape", 0.0f, 50.0f, 1.0f);
    RegisterFloat(&mfSkyHorBleedPow, "Sky colours", "Hor bleeding width", 0.01f, 50.0f, 1.0f);
    RegisterFloat(&mfSkyHorBleedScl, "Sky colours", "Hor bleeding height", 0.01f, 50.0f, 1.0f);
    RegisterFloat(&mfSkySunBleedPow, "Sky colours", "Sun/Hor bleeding balance", 0.01f, 50.0f, 1.0f);
    RegisterFloat(&mfSkyDrk, "Sky colours", "Darkness", 0.0f, 1.0f, 0.05f);

    RegisterVariable(&mpEnvironmentManager->mbSetScattColsFromSky, KPC_ROOT,
                     "Set scatt colours from sky");
    RegisterFloat(&mfScattTopColour_r, "Scatt colours", "Top Colour R", 0.0f, 10.0f, 0.01f);
    RegisterFloat(&mfScattTopColour_g, "Scatt colours", "Top Colour G", 0.0f, 10.0f, 0.01f);
    RegisterFloat(&mfScattTopColour_b, "Scatt colours", "Top Colour B", 0.0f, 10.0f, 0.01f);
    RegisterFloat(&mfScattHorColour_r, "Scatt colours", "Hor Colour R", 0.0f, 10.0f, 0.01f);
    RegisterFloat(&mfScattHorColour_g, "Scatt colours", "Hor Colour G", 0.0f, 10.0f, 0.01f);
    RegisterFloat(&mfScattHorColour_b, "Scatt colours", "Hor Colour B", 0.0f, 10.0f, 0.01f);
    RegisterFloat(&mfScattHorPow, "Scatt colours", "Sky Shape", 0.0f, 10.0f, 0.1f);
    RegisterFloat(&mfScattSunColour_r, "Scatt colours", "Sun Colour R", 0.0f, 10.0f, 0.01f);
    RegisterFloat(&mfScattSunColour_g, "Scatt colours", "Sun Colour G", 0.0f, 10.0f, 0.01f);
    RegisterFloat(&mfScattSunColour_b, "Scatt colours", "Sun Colour B", 0.0f, 10.0f, 0.01f);
    RegisterFloat(&mfScattSunPow, "Scatt colours", "Sun Shape", 0.0f, 50.0f, 1.0f);
    RegisterFloat(&mfScattHorBleedPow, "Scatt colours", "Hor Bleeding Width", 0.01f, 50.0f, 1.0f);
    RegisterFloat(&mfScattHorBleedScl, "Scatt colours", "Hor Bleeding Height", 0.01f, 50.0f, 1.0f);
    RegisterFloat(&mfScattSunBleedPow, "Scatt colours", "Sun/Hor Bleeding Balance", 0.01f, 50.0f, 1.0f);
    RegisterFloat(&mfScattDrk, "Scatt colours", "Darkness", 0.0f, 1.0f, 0.05f);
    RegisterFloat(&mafScattDist[0], KPC_ROOT, "Scattering distance 0", 1.0f, 1000000.0f, 10.0f);
    RegisterFloat(&mafScattDist[1], KPC_ROOT, "Scattering distance 1", 1.0f, 1000000.0f, 50.0f);
    RegisterFloat(&mfScattPow, KPC_ROOT, "Scattering power", 0.0f, 100.0f, 0.1f);
    RegisterFloat(&mfScattCap, KPC_ROOT, "Scattering cap", 0.01f, 1.0f, 0.05f);

    RegisterFloat(&mafCloudLayerDensity[0], "Clouds", "Layer 0 density", 0.0f, 1.0f, 0.02f);
    RegisterFloat(&mafCloudLayerDensity[1], "Clouds", "Layer 1 density", 0.0f, 1.0f, 0.02f);
    RegisterFloat(&mafCloudLayerFeathering[0], "Clouds", "Layer 0 feathering", 0.0f, 1.0f, 0.02f);
    RegisterFloat(&mafCloudLayerFeathering[1], "Clouds", "Layer 1 feathering", 0.0f, 1.0f, 0.02f);
    RegisterFloat(&mafCloudLayerOpacity[0], "Clouds", "Layer 0 opacity", 0.0f, 1.0f, 0.02f);
    RegisterFloat(&mafCloudLayerOpacity[1], "Clouds", "Layer 1 opacity", 0.0f, 1.0f, 0.02f);
    RegisterFloat(&mfCloudLayerDarkColourR, "Clouds", "Layer 0 dark R", 0.0f, 1.0f, 0.05f);
    RegisterFloat(&mfCloudLayerDarkColourG, "Clouds", "Layer 0 dark G", 0.0f, 1.0f, 0.05f);
    RegisterFloat(&mfCloudLayerDarkColourB, "Clouds", "Layer 0 dark B", 0.0f, 1.0f, 0.05f);
    RegisterFloat(&mfCloudLayerLiteColourR, "Clouds", "Layer 0 lite R", 0.0f, 1.0f, 0.05f);
    RegisterFloat(&mfCloudLayerLiteColourG, "Clouds", "Layer 0 lite G", 0.0f, 1.0f, 0.05f);
    RegisterFloat(&mfCloudLayerLiteColourB, "Clouds", "Layer 0 lite B", 0.0f, 1.0f, 0.05f);
    RegisterFloat(&mpEnvironmentManager->mfCloudDistanceCurve, "Clouds", "Distance falloff", 1.0f, 10.0f, 0.1f);

    RegisterVariable(&mpEnvironmentManager->mbSetIrradianceFromSky, KPC_ROOT,
                     "Set Irradiance rig from Sky");
    RegisterFloat(&mfAmbientIrradianceScale, "Irradiance rig", "Irradiance scale", 0.0f, 5.0f, 0.01f);
    RegisterFloat(&mfKeyFillColour_r, "Irradiance rig", "Keyfill colour R", 0.0f, 10.0f, 0.1f);
    RegisterFloat(&mfKeyFillColour_g, "Irradiance rig", "Keyfill colour G", 0.0f, 10.0f, 0.1f);
    RegisterFloat(&mfKeyFillColour_b, "Irradiance rig", "Keyfill colour B", 0.0f, 10.0f, 0.1f);
    RegisterFloat(&mfShadowFillColour_r, "Irradiance rig", "Shadowfill colour R", 0.0f, 10.0f, 0.1f);
    RegisterFloat(&mfShadowFillColour_g, "Irradiance rig", "Shadowfill colour G", 0.0f, 10.0f, 0.1f);
    RegisterFloat(&mfShadowFillColour_b, "Irradiance rig", "Shadowfill colour B", 0.0f, 10.0f, 0.1f);
    RegisterFloat(&mfLeftFillColour_r, "Irradiance rig", "Leftfill colour R", 0.0f, 10.0f, 0.1f);
    RegisterFloat(&mfLeftFillColour_g, "Irradiance rig", "Leftfill colour G", 0.0f, 10.0f, 0.1f);
    RegisterFloat(&mfLeftFillColour_b, "Irradiance rig", "Leftfill colour B", 0.0f, 10.0f, 0.1f);
    RegisterFloat(&mfRightFillColour_r, "Irradiance rig", "Rightfill colour R", 0.0f, 10.0f, 0.1f);
    RegisterFloat(&mfRightFillColour_g, "Irradiance rig", "Rightfill colour G", 0.0f, 10.0f, 0.1f);
    RegisterFloat(&mfRightFillColour_b, "Irradiance rig", "Rightfill colour B", 0.0f, 10.0f, 0.1f);
    RegisterFloat(&mfUpFillColour_r, "Irradiance rig", "Upfill colour R", 0.0f, 10.0f, 0.1f);
    RegisterFloat(&mfUpFillColour_g, "Irradiance rig", "Upfill colour G", 0.0f, 10.0f, 0.1f);
    RegisterFloat(&mfUpFillColour_b, "Irradiance rig", "Upfill colour B", 0.0f, 10.0f, 0.1f);
    RegisterFloat(&mfDownFillColour_r, "Irradiance rig", "Downfill colour R", 0.0f, 10.0f, 0.1f);
    RegisterFloat(&mfDownFillColour_g, "Irradiance rig", "Downfill colour G", 0.0f, 10.0f, 0.1f);
    RegisterFloat(&mfDownFillColour_b, "Irradiance rig", "Downfill colour B", 0.0f, 10.0f, 0.1f);

    RegisterFloat(&mfSunTiltAtHorizon, KPC_ROOT,
                  "Sun path tilt from vertical at horizon (degrees)", 1.0f, 89.0f, 1.0f);
    RegisterFloat(&mfSunTiltAtMidday, KPC_ROOT,
                  "Sun path tilt from vertical at midday (degrees)", 1.0f, 89.0f, 1.0f);

    RegisterFloat(&mafJunkyardKeyLightDirection[0], "Junkyard Lighting", "LightX", -1.0f, 1.0f, 0.05f);
    RegisterFloat(&mafJunkyardKeyLightDirection[1], "Junkyard Lighting", "LightY", -1.0f, 1.0f, 0.05f);
    RegisterFloat(&mafJunkyardKeyLightDirection[2], "Junkyard Lighting", "LightZ", -1.0f, 1.0f, 0.05f);
    RegisterVariable(&mbCalculateJunkyardKeyLightDirectionFromTime,
                     "Junkyard Lighting", "Set from time of day");
    RegisterVariable(&mbOverrideJunkyardKeyLightDirection,
                     "Junkyard Lighting", "Override key light direction");
    RegisterVariable(&mbGetJunkyardKeyLightDirection,
                     "Junkyard Lighting", "Get current values");

    RegisterClock(&muSunElevTodLBoundHH, "Sun Clamp", "LBound Hours", 23);
    RegisterClock(&muSunElevTodLBoundMM, "Sun Clamp", "LBound Minutes", 59);
    RegisterClock(&muSunElevTodLBoundSS, "Sun Clamp", "LBound Seconds", 59);
    RegisterClock(&muSunElevTodUBoundHH, "Sun Clamp", "UBound Hours", 23);
    RegisterClock(&muSunElevTodUBoundMM, "Sun Clamp", "UBound Minutes", 59);
    RegisterClock(&muSunElevTodUBoundSS, "Sun Clamp", "UBound Seconds", 59);
}

// @0x827B3B28. Copy every menu scalar into the live blended environment.
void DebugComponent::DebugToManager()
{
    EnvironmentManager& lrManager = *mpEnvironmentManager;
    ScatteringData& lrScattering = lrManager.mScattering;
    LightingData& lrLighting = lrManager.mLighting;
    CloudsData& lrClouds = lrManager.mClouds;

    lrManager.mfTimeOfDay = ClockToSeconds(muTimeOfDay_HH, muTimeOfDay_MM, muTimeOfDay_SS);
    lrManager.mfTimeOfDayDelta = lrManager.mfTimeOfDayDelta <= 0.0f
                               ? -mfTimeOfDayDelta : mfTimeOfDayDelta;
    lrManager.mfCloudDelta = mfCloudDelta;

    lrScattering.mv3SkyTopColour[0] = mfSkyTopColour_r;
    lrScattering.mv3SkyTopColour[1] = mfSkyTopColour_g;
    lrScattering.mv3SkyTopColour[2] = mfSkyTopColour_b;
    lrScattering.mv3SkyHorColour[0] = mfSkyHorColour_r;
    lrScattering.mv3SkyHorColour[1] = mfSkyHorColour_g;
    lrScattering.mv3SkyHorColour[2] = mfSkyHorColour_b;
    lrScattering.mv3SkySunColour[0] = mfSkySunColour_r;
    lrScattering.mv3SkySunColour[1] = mfSkySunColour_g;
    lrScattering.mv3SkySunColour[2] = mfSkySunColour_b;
    lrScattering.mfSkyHorPow = mfSkyHorPow;
    lrScattering.mfSkySunPow = mfSkySunPow;
    lrScattering.mfSkyDrk = mfSkyDrk;
    lrScattering.mfSkyHorBleedScl = mfSkyHorBleedScl;
    lrScattering.mfSkyHorBleedPow = mfSkyHorBleedPow;
    lrScattering.mfSkySunBleedPow = mfSkySunBleedPow;

    lrScattering.mv3ScattTopColour[0] = mfScattTopColour_r;
    lrScattering.mv3ScattTopColour[1] = mfScattTopColour_g;
    lrScattering.mv3ScattTopColour[2] = mfScattTopColour_b;
    lrScattering.mv3ScattHorColour[0] = mfScattHorColour_r;
    lrScattering.mv3ScattHorColour[1] = mfScattHorColour_g;
    lrScattering.mv3ScattHorColour[2] = mfScattHorColour_b;
    lrScattering.mv3ScattSunColour[0] = mfScattSunColour_r;
    lrScattering.mv3ScattSunColour[1] = mfScattSunColour_g;
    lrScattering.mv3ScattSunColour[2] = mfScattSunColour_b;
    lrScattering.mfScattHorPow = mfScattHorPow;
    lrScattering.mfScattSunPow = mfScattSunPow;
    lrScattering.mfScattDrk = mfScattDrk;
    lrScattering.mfScattHorBleedScl = mfScattHorBleedScl;
    lrScattering.mfScattHorBleedPow = mfScattHorBleedPow;
    lrScattering.mfScattSunBleedPow = mfScattSunBleedPow;
    lrScattering.mafScattDist[0] = mafScattDist[0];
    lrScattering.mafScattDist[1] = mafScattDist[1];
    lrScattering.mfScattPow = mfScattPow;
    lrScattering.mfScattCap = mfScattCap;

    lrLighting.mv3KeyLightColour[0] = mfKeyLightColour_r;
    lrLighting.mv3KeyLightColour[1] = mfKeyLightColour_g;
    lrLighting.mv3KeyLightColour[2] = mfKeyLightColour_b;
    lrLighting.mv3SpecularColour[0] = mfSpecularColour_r;
    lrLighting.mv3SpecularColour[1] = mfSpecularColour_g;
    lrLighting.mv3SpecularColour[2] = mfSpecularColour_b;
    lrLighting.mv3KeyFillColour[0] = mfKeyFillColour_r;
    lrLighting.mv3KeyFillColour[1] = mfKeyFillColour_g;
    lrLighting.mv3KeyFillColour[2] = mfKeyFillColour_b;
    lrLighting.mv3ShadowFillColour[0] = mfShadowFillColour_r;
    lrLighting.mv3ShadowFillColour[1] = mfShadowFillColour_g;
    lrLighting.mv3ShadowFillColour[2] = mfShadowFillColour_b;
    lrLighting.mv3RightFillColour[0] = mfRightFillColour_r;
    lrLighting.mv3RightFillColour[1] = mfRightFillColour_g;
    lrLighting.mv3RightFillColour[2] = mfRightFillColour_b;
    lrLighting.mv3LeftFillColour[0] = mfLeftFillColour_r;
    lrLighting.mv3LeftFillColour[1] = mfLeftFillColour_g;
    lrLighting.mv3LeftFillColour[2] = mfLeftFillColour_b;
    lrLighting.mv3UpFillColour[0] = mfUpFillColour_r;
    lrLighting.mv3UpFillColour[1] = mfUpFillColour_g;
    lrLighting.mv3UpFillColour[2] = mfUpFillColour_b;
    lrLighting.mv3DownFillColour[0] = mfDownFillColour_r;
    lrLighting.mv3DownFillColour[1] = mfDownFillColour_g;
    lrLighting.mv3DownFillColour[2] = mfDownFillColour_b;
    lrLighting.mfAmbientIrradianceScale = mfAmbientIrradianceScale;

    lrClouds.mafLayerDensity[0] = mafCloudLayerDensity[0];
    lrClouds.mafLayerDensity[1] = mafCloudLayerDensity[1];
    lrClouds.mafLayerFeathering[0] = mafCloudLayerFeathering[0];
    lrClouds.mafLayerFeathering[1] = mafCloudLayerFeathering[1];
    lrClouds.mafLayerOpacity[0] = mafCloudLayerOpacity[0];
    lrClouds.mafLayerOpacity[1] = mafCloudLayerOpacity[1];
    lrClouds.mav3LayerDarkColour[0][0] = mfCloudLayerDarkColourR;
    lrClouds.mav3LayerDarkColour[0][1] = mfCloudLayerDarkColourG;
    lrClouds.mav3LayerDarkColour[0][2] = mfCloudLayerDarkColourB;
    lrClouds.mav3LayerLiteColour[0][0] = mfCloudLayerLiteColourR;
    lrClouds.mav3LayerLiteColour[0][1] = mfCloudLayerLiteColourG;
    lrClouds.mav3LayerLiteColour[0][2] = mfCloudLayerLiteColourB;

    lrManager.mfSunTiltAtHorizon = mfSunTiltAtHorizon;
    lrManager.mfSunTiltAtMidday = mfSunTiltAtMidday;

    // ARTIST only publishes the clamp when the lower-bound hour is non-zero.
    if (muSunElevTodLBoundHH != 0)
    {
        lrManager.mfSunElevTodLBound = ClockToSeconds(muSunElevTodLBoundHH,
                                                      muSunElevTodLBoundMM,
                                                      muSunElevTodLBoundSS);
        lrManager.mfSunElevTodUBound = ClockToSeconds(muSunElevTodUBoundHH,
                                                      muSunElevTodUBoundMM,
                                                      muSunElevTodUBoundSS);
    }
}

// @0x827BFC80. Refresh the menu mirror from the manager's current blended data.
void DebugComponent::ManagerToDebug()
{
    const EnvironmentManager& lrManager = *mpEnvironmentManager;
    const ScatteringData& lrScattering = lrManager.mScattering;
    const LightingData& lrLighting = lrManager.mLighting;
    const CloudsData& lrClouds = lrManager.mClouds;

    HH_MM_SS(&muTimeOfDay_HH, &muTimeOfDay_MM, &muTimeOfDay_SS, lrManager.mfTimeOfDay);
    mfTimeOfDayDelta = std::fabs(lrManager.mfTimeOfDayDelta);
    mfCloudDelta = lrManager.mfCloudDelta;

    mfSkyTopColour_r = lrScattering.mv3SkyTopColour[0];
    mfSkyTopColour_g = lrScattering.mv3SkyTopColour[1];
    mfSkyTopColour_b = lrScattering.mv3SkyTopColour[2];
    mfSkyHorColour_r = lrScattering.mv3SkyHorColour[0];
    mfSkyHorColour_g = lrScattering.mv3SkyHorColour[1];
    mfSkyHorColour_b = lrScattering.mv3SkyHorColour[2];
    mfSkySunColour_r = lrScattering.mv3SkySunColour[0];
    mfSkySunColour_g = lrScattering.mv3SkySunColour[1];
    mfSkySunColour_b = lrScattering.mv3SkySunColour[2];
    mfSkyHorPow = lrScattering.mfSkyHorPow;
    mfSkySunPow = lrScattering.mfSkySunPow;
    mfSkyDrk = lrScattering.mfSkyDrk;
    mfSkyHorBleedScl = lrScattering.mfSkyHorBleedScl;
    mfSkyHorBleedPow = lrScattering.mfSkyHorBleedPow;
    mfSkySunBleedPow = lrScattering.mfSkySunBleedPow;

    mfScattTopColour_r = lrScattering.mv3ScattTopColour[0];
    mfScattTopColour_g = lrScattering.mv3ScattTopColour[1];
    mfScattTopColour_b = lrScattering.mv3ScattTopColour[2];
    mfScattHorColour_r = lrScattering.mv3ScattHorColour[0];
    mfScattHorColour_g = lrScattering.mv3ScattHorColour[1];
    mfScattHorColour_b = lrScattering.mv3ScattHorColour[2];
    mfScattSunColour_r = lrScattering.mv3ScattSunColour[0];
    mfScattSunColour_g = lrScattering.mv3ScattSunColour[1];
    mfScattSunColour_b = lrScattering.mv3ScattSunColour[2];
    mfScattHorPow = lrScattering.mfScattHorPow;
    mfScattSunPow = lrScattering.mfScattSunPow;
    mfScattDrk = lrScattering.mfScattDrk;
    mfScattHorBleedScl = lrScattering.mfScattHorBleedScl;
    mfScattHorBleedPow = lrScattering.mfScattHorBleedPow;
    mfScattSunBleedPow = lrScattering.mfScattSunBleedPow;
    mafScattDist[0] = lrScattering.mafScattDist[0];
    mafScattDist[1] = lrScattering.mafScattDist[1];
    mfScattPow = lrScattering.mfScattPow;
    mfScattCap = lrScattering.mfScattCap;

    mfKeyLightColour_r = lrLighting.mv3KeyLightColour[0];
    mfKeyLightColour_g = lrLighting.mv3KeyLightColour[1];
    mfKeyLightColour_b = lrLighting.mv3KeyLightColour[2];
    mfSpecularColour_r = lrLighting.mv3SpecularColour[0];
    mfSpecularColour_g = lrLighting.mv3SpecularColour[1];
    mfSpecularColour_b = lrLighting.mv3SpecularColour[2];
    mfKeyFillColour_r = lrLighting.mv3KeyFillColour[0];
    mfKeyFillColour_g = lrLighting.mv3KeyFillColour[1];
    mfKeyFillColour_b = lrLighting.mv3KeyFillColour[2];
    mfShadowFillColour_r = lrLighting.mv3ShadowFillColour[0];
    mfShadowFillColour_g = lrLighting.mv3ShadowFillColour[1];
    mfShadowFillColour_b = lrLighting.mv3ShadowFillColour[2];
    mfRightFillColour_r = lrLighting.mv3RightFillColour[0];
    mfRightFillColour_g = lrLighting.mv3RightFillColour[1];
    mfRightFillColour_b = lrLighting.mv3RightFillColour[2];
    mfLeftFillColour_r = lrLighting.mv3LeftFillColour[0];
    mfLeftFillColour_g = lrLighting.mv3LeftFillColour[1];
    mfLeftFillColour_b = lrLighting.mv3LeftFillColour[2];
    mfUpFillColour_r = lrLighting.mv3UpFillColour[0];
    mfUpFillColour_g = lrLighting.mv3UpFillColour[1];
    mfUpFillColour_b = lrLighting.mv3UpFillColour[2];
    mfDownFillColour_r = lrLighting.mv3DownFillColour[0];
    mfDownFillColour_g = lrLighting.mv3DownFillColour[1];
    mfDownFillColour_b = lrLighting.mv3DownFillColour[2];
    mfAmbientIrradianceScale = lrLighting.mfAmbientIrradianceScale;

    mafCloudLayerDensity[0] = lrClouds.mafLayerDensity[0];
    mafCloudLayerDensity[1] = lrClouds.mafLayerDensity[1];
    mafCloudLayerFeathering[0] = lrClouds.mafLayerFeathering[0];
    mafCloudLayerFeathering[1] = lrClouds.mafLayerFeathering[1];
    mafCloudLayerOpacity[0] = lrClouds.mafLayerOpacity[0];
    mafCloudLayerOpacity[1] = lrClouds.mafLayerOpacity[1];
    mfCloudLayerDarkColourR = lrClouds.mav3LayerDarkColour[0][0];
    mfCloudLayerDarkColourG = lrClouds.mav3LayerDarkColour[0][1];
    mfCloudLayerDarkColourB = lrClouds.mav3LayerDarkColour[0][2];
    mfCloudLayerLiteColourR = lrClouds.mav3LayerLiteColour[0][0];
    mfCloudLayerLiteColourG = lrClouds.mav3LayerLiteColour[0][1];
    mfCloudLayerLiteColourB = lrClouds.mav3LayerLiteColour[0][2];

    mfSunTiltAtHorizon = lrManager.mfSunTiltAtHorizon;
    mfSunTiltAtMidday = lrManager.mfSunTiltAtMidday;

    // The zero hour is the component's one-shot initialization sentinel.
    if (muSunElevTodLBoundHH == 0)
    {
        HH_MM_SS(&muSunElevTodLBoundHH, &muSunElevTodLBoundMM, &muSunElevTodLBoundSS,
                 lrManager.mfSunElevTodLBound);
        HH_MM_SS(&muSunElevTodUBoundHH, &muSunElevTodUBoundMM, &muSunElevTodUBoundSS,
                 lrManager.mfSunElevTodUBound);
    }
}

// @0x827C7760.
void DebugComponent::Update()
{
    if (mbGetJunkyardKeyLightDirection)
    {
        mafJunkyardKeyLightDirection[0] = mpEnvironmentManager->mOverrideKeyLightDirection.x;
        mafJunkyardKeyLightDirection[1] = mpEnvironmentManager->mOverrideKeyLightDirection.y;
        mafJunkyardKeyLightDirection[2] = mpEnvironmentManager->mOverrideKeyLightDirection.z;
    }

    if (mbOverrideJunkyardKeyLightDirection)
    {
        const f32 lfX = mafJunkyardKeyLightDirection[0];
        const f32 lfY = mafJunkyardKeyLightDirection[1];
        const f32 lfZ = mafJunkyardKeyLightDirection[2];
        const f32 lfInvLength = 1.0f / std::sqrt(lfX * lfX + lfY * lfY + lfZ * lfZ);

        mpEnvironmentManager->mOverrideKeyLightDirection.x = lfX * lfInvLength;
        mpEnvironmentManager->mOverrideKeyLightDirection.y = lfY * lfInvLength;
        mpEnvironmentManager->mOverrideKeyLightDirection.z = lfZ * lfInvLength;
        if (mpEnvironmentManager->mOverrideKeyLightDirection.y > KF_JUNKYARD_MAX_VERTICAL)
            mpEnvironmentManager->mOverrideKeyLightDirection.y = KF_JUNKYARD_MAX_VERTICAL;
        if (mpEnvironmentManager->mOverrideKeyLightDirection.y < -KF_JUNKYARD_MAX_VERTICAL)
            mpEnvironmentManager->mOverrideKeyLightDirection.y = -KF_JUNKYARD_MAX_VERTICAL;
        mpEnvironmentManager->mOverrideKeyLightDirection.w = 0.0f;
    }

    if (mbCalculateJunkyardKeyLightDirectionFromTime)
    {
        const Vector3 lDirection = ComputeKeyLightDirection(
            (mpEnvironmentManager->mfTimeOfDay - KF_SUN_ELEVATION_ZERO_SECONDS)
                * KF_RADIANS_PER_SECOND_OF_DAY,
            mpEnvironmentManager->mfSunRigRotation,
            mpEnvironmentManager->mfSunTiltAtHorizon,
            mpEnvironmentManager->mfSunTiltAtMidday);
        mafJunkyardKeyLightDirection[0] = lDirection.x;
        mafJunkyardKeyLightDirection[1] = lDirection.y;
        mafJunkyardKeyLightDirection[2] = lDirection.z;
    }

    mpEnvironmentManager->UpdateFromTool(mbUpdateValuesFromTool);
    if (mbUpdateValuesFromTool)
    {
        ManagerToDebug();
        return;
    }

    mpEnvironmentManager->Pause(!mbSimulateTimeOfDay);
    if (mbSimulateTimeOfDay)
        ManagerToDebug();
    else
        DebugToManager();
}

// @0x827C79A0.
void DebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender* lpDisplay)
{
    if (!mbPrintDebugInfo)
        return;

    const char* lpcSeason = "NONE";
    const char* lpcColourCubes = "NONE";

    if (mpEnvironmentManager->mDictionaryPtr.HasMemoryResource()
        && mpEnvironmentManager->miCurrSeason >= 0)
    {
        const Dictionary* const lpDictionary =
            mpEnvironmentManager->mDictionaryPtr.GetMemoryResource();
        const s32 liSeason = mpEnvironmentManager->maiSeasons[mpEnvironmentManager->miCurrSeason];
        if (liSeason >= 0)
        {
            const Dictionary::SeasonData& lrSeason = lpDictionary->mpSeasonDatii[liSeason];
            lpcSeason = lrSeason.macBundle;
            lpcColourCubes = lrSeason.macColourCubesBundle;
        }
    }

    char acTime[32];
    char acText[1024];
    BuildTimeOfDay(acTime, mpEnvironmentManager->mfTimeOfDay);

    CgsCore::SPrintf(acText, sizeof(acText), "TOD: %s", acTime);
    lpDisplay->DrawText(acText, 360.0f, 535.0f, 15.0f, 0xFFFFFFFFu);
    CgsCore::SPrintf(acText, sizeof(acText), "Season: %s", lpcSeason);
    lpDisplay->DrawText(acText, 360.0f, 550.0f, 15.0f, 0xFFFFFFFFu);
    CgsCore::SPrintf(acText, sizeof(acText), "CCubes: %s", lpcColourCubes);
    lpDisplay->DrawText(acText, 360.0f, 565.0f, 15.0f, 0xFFFFFFFFu);
}

} // namespace EnvironmentSettings
} // namespace BrnWorld
