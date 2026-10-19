// Exact production bodies are injected by the runner. Expected values below
// derive from ARTIST stores/branches and simple exact-axis or exceptional VMX
// witnesses, not a second copy of the implementation. Typed owner spies isolate
// the calls under test; this does not instantiate the actual engine lifecycle.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <vector>
#include "GameSource/World/EnvironmentSettings/BrnEnvScatteringData.h"
#include "GameSource/World/EnvironmentSettings/BrnEnvLightingData.h"
#include "GameSource/World/EnvironmentSettings/BrnEnvCloudsData.h"
#include "GameShared/GameClasses/Graphics/Dispatch/CgsOcclusionCullManager.h"
#include "rw/math/vpu/vector3_operation.h"

static unsigned gChecks = 0, gFailures = 0;
static std::vector<unsigned> gTrace;
static void Push(unsigned event) { gTrace.push_back(event); }
static void Check(bool pass, const char* label)
{
    ++gChecks;
    if (!pass) { ++gFailures; std::printf("FAIL %s\n", label); }
}
static unsigned Bits(float value)
{
    unsigned bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}
static float Float(unsigned bits)
{
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}
static bool Trace(std::initializer_list<unsigned> expected)
{
    return gTrace == std::vector<unsigned>(expected);
}

namespace BrnWorld { namespace EnvironmentSettings {
// Selected original owner members plus call spies; intentionally no ABI claim.
class EnvironmentManager
{
public:
    ScatteringData mScattering{};
    LightingData mLighting{};
    CloudsData mClouds{};
    f32 mfTimeOfDay = 0, mfTimeOfDayDelta = 0, mfCloudDelta = 0;
    f32 mfSunTiltAtHorizon = 0, mfSunTiltAtMidday = 0, mfSunRigRotation = 0;
    f32 mfSunElevTodLBound = 0, mfSunElevTodUBound = 0;
    Vector3 mOverrideKeyLightDirection{};
    unsigned managerReads = 0, pauseCalls = 0;
    bool tool = false, paused = false;
    void UpdateFromTool(bool value) { tool = value; Push(2); }
    void Pause(bool value) { paused = value; ++pauseCalls; Push(3); }
};
static EnvironmentManager* gpManager;
static Vector3 gDirectionAtCalculate;
static float gCalculateArguments[4];
static Vector3 ComputeKeyLightDirection(float a, float b, float c, float d)
{
    Push(1);
    gDirectionAtCalculate = gpManager->mOverrideKeyLightDirection;
    gCalculateArguments[0]=a; gCalculateArguments[1]=b;
    gCalculateArguments[2]=c; gCalculateArguments[3]=d;
    return {7,8,9,0};
}
}}

namespace CgsDev { namespace DebugUI {
enum InputEvent { INPUT_TEST = 3 };
class Window
{
public:
    unsigned updates = 0;
    float time = 0;
    InputEvent event = INPUT_TEST;
    void Update(float value, InputEvent input) { ++updates; time=value; event=input; }
};
}}
namespace BrnPhysics { namespace Vehicle {
struct Wheel { struct TireGripCurve { Vector4 maGripVariables{}; }; };
class GripCurveDebugGraph
{
public:
    Wheel::TireGripCurve curve{};
    rw::math::vpu::Vector2 range{};
    const Wheel::TireGripCurve* GetGripCurve() const { return &curve; }
    void SetRange(rw::math::vpu::Vector2 value) { range=value; }
};
class GripCurveDebugWindow : public CgsDev::DebugUI::Window
{
public:
    GripCurveDebugGraph mLongGripCurveGraph, mLatGripCurveGraph;
    void Update(f32, CgsDev::DebugUI::InputEvent);
    f32 GetSlipRatioRange();
    f32 GetCoefficientRange();
};
}}

#include "debug_menu_environment_math.inc"

static void TransferWitnesses()
{
    using namespace BrnWorld::EnvironmentSettings;
    EnvironmentManager manager;
    DebugComponent debug{};
    debug.mpEnvironmentManager = &manager;
    struct Colour { float *r, *g, *b, *destination; };
#define COLOUR(menu, destination) { &debug.menu##_r, &debug.menu##_g, &debug.menu##_b, manager.destination }
    Colour colours[] = {
        COLOUR(mfSkyTopColour, mScattering.mv3SkyTopColour),
        COLOUR(mfSkyHorColour, mScattering.mv3SkyHorColour),
        COLOUR(mfSkySunColour, mScattering.mv3SkySunColour),
        COLOUR(mfScattTopColour, mScattering.mv3ScattTopColour),
        COLOUR(mfScattHorColour, mScattering.mv3ScattHorColour),
        COLOUR(mfScattSunColour, mScattering.mv3ScattSunColour),
        COLOUR(mfKeyLightColour, mLighting.mv3KeyLightColour),
        COLOUR(mfSpecularColour, mLighting.mv3SpecularColour),
        COLOUR(mfKeyFillColour, mLighting.mv3KeyFillColour),
        COLOUR(mfShadowFillColour, mLighting.mv3ShadowFillColour),
        COLOUR(mfRightFillColour, mLighting.mv3RightFillColour),
        COLOUR(mfLeftFillColour, mLighting.mv3LeftFillColour),
        COLOUR(mfUpFillColour, mLighting.mv3UpFillColour),
        COLOUR(mfDownFillColour, mLighting.mv3DownFillColour),
        {&debug.mfCloudLayerDarkColourR, &debug.mfCloudLayerDarkColourG,
         &debug.mfCloudLayerDarkColourB, manager.mClouds.mav3LayerDarkColour[0]},
        {&debug.mfCloudLayerLiteColourR, &debug.mfCloudLayerLiteColourG,
         &debug.mfCloudLayerLiteColourB, manager.mClouds.mav3LayerLiteColour[0]},
    };
#undef COLOUR
    for (unsigned i=0; i<16; ++i)
    {
        *colours[i].r = float(i*10+1); *colours[i].g = float(i*10+2); *colours[i].b = float(i*10+3);
        for (unsigned lane=0; lane<4; ++lane) colours[i].destination[lane] = -99.0f;
    }
    for (unsigned lane=0; lane<4; ++lane)
    {
        manager.mClouds.mav3LayerDarkColour[1][lane] = 81.0f;
        manager.mClouds.mav3LayerLiteColour[1][lane] = 91.0f;
    }
    manager.mfSunElevTodLBound=123; manager.mfSunElevTodUBound=456;
    debug.muTimeOfDay_HH=13; debug.muTimeOfDay_MM=14; debug.muTimeOfDay_SS=15;
    debug.DebugToManager();
    for (unsigned i=0; i<16; ++i)
    {
        const float* value=colours[i].destination;
        Check(value[0]==float(i*10+1) && value[1]==float(i*10+2)
              && value[2]==float(i*10+3) && Bits(value[3])==0, "RGB transfer and positive-zero W");
    }
    bool untouched=true;
    for (unsigned lane=0; lane<4; ++lane)
        untouched &= manager.mClouds.mav3LayerDarkColour[1][lane]==81
                  && manager.mClouds.mav3LayerLiteColour[1][lane]==91;
    Check(untouched, "layer-one colors remain outside the original transfer");
    Check(manager.mfTimeOfDay==47655, "edited clock conversion");
    Check(manager.mfSunElevTodLBound==123 && manager.mfSunElevTodUBound==456, "zero-hour clamp sentinel");
    debug.muSunElevTodLBoundHH=6; debug.muSunElevTodLBoundMM=1; debug.muSunElevTodLBoundSS=2;
    debug.muSunElevTodUBoundHH=8; debug.muSunElevTodUBoundMM=3; debug.muSunElevTodUBoundSS=4;
    debug.DebugToManager();
    Check(manager.mfSunElevTodLBound==21662 && manager.mfSunElevTodUBound==28984, "nonzero-hour clamp publication");
    debug.mfTimeOfDayDelta=3;
    const float oldValues[]={2,-2,0,-0.0f,std::numeric_limits<float>::quiet_NaN(),
                              std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()};
    const float expected[]={3,-3,-3,-3,-3,3,-3};
    for (unsigned i=0; i<7; ++i)
    {
        manager.mfTimeOfDayDelta=oldValues[i]; debug.DebugToManager();
        Check(manager.mfTimeOfDayDelta==expected[i], "ordered positive clock sign predicate");
    }
}

static void UpdateWitnesses()
{
    using namespace BrnWorld::EnvironmentSettings;
    EnvironmentManager manager;
    DebugComponent debug{};
    gpManager=&manager; debug.mpEnvironmentManager=&manager;
    debug.mbUpdateValuesFromTool=true; debug.mbOverrideJunkyardKeyLightDirection=true;
    debug.mafJunkyardKeyLightDirection[0]=2;
    gTrace.clear(); debug.Update();
    Check(manager.mOverrideKeyLightDirection.x==1 && manager.mOverrideKeyLightDirection.y==0
          && manager.mOverrideKeyLightDirection.z==0 && Bits(manager.mOverrideKeyLightDirection.w)==0,
          "exact-axis full normalized vector");
    Check(Trace({2,4}) && manager.pauseCalls==0 && manager.tool, "tool branch returns before Pause");
    const float exceptional[]={0,1e-20f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()};
    for (float value : exceptional)
    {
        debug.mafJunkyardKeyLightDirection[0]=value;
        debug.mafJunkyardKeyLightDirection[1]=0; debug.mafJunkyardKeyLightDirection[2]=0;
        debug.Update();
        const auto v=manager.mOverrideKeyLightDirection;
        Check(std::isnan(v.x) && std::isnan(v.y) && std::isnan(v.z) && std::isnan(v.w),
              "unconditional exceptional refinement preserves all four NaN lanes");
    }
    debug.mafJunkyardKeyLightDirection[0]=0; debug.mafJunkyardKeyLightDirection[1]=2;
    debug.Update(); Check(manager.mOverrideKeyLightDirection.y==0.98900002f, "ordered upper vertical clamp");
    debug.mafJunkyardKeyLightDirection[1]=-2;
    debug.Update(); Check(manager.mOverrideKeyLightDirection.y==-0.98900002f, "ordered lower vertical clamp");
    manager.mOverrideKeyLightDirection={2,0,0,7};
    manager.mfTimeOfDay=23400; manager.mfSunRigRotation=0.25f;
    manager.mfSunTiltAtHorizon=0.5f; manager.mfSunTiltAtMidday=0.75f;
    debug.mbGetJunkyardKeyLightDirection=true;
    debug.mbCalculateJunkyardKeyLightDirectionFromTime=true;
    gTrace.clear(); debug.Update();
    Check(Trace({1,2,4}) && gDirectionAtCalculate.x==1 && gDirectionAtCalculate.w==0,
          "Get then Override precedes Calculate and tool refresh");
    Check(debug.mafJunkyardKeyLightDirection[0]==7 && debug.mafJunkyardKeyLightDirection[1]==8
          && debug.mafJunkyardKeyLightDirection[2]==9, "Calculate writes next edited direction");
    Check(gCalculateArguments[0]==0 && gCalculateArguments[1]==0.25f
          && gCalculateArguments[2]==0.5f && gCalculateArguments[3]==0.75f, "Calculate receives original ordered arguments");
    debug.mbGetJunkyardKeyLightDirection=false; debug.mbOverrideJunkyardKeyLightDirection=false;
    debug.mbCalculateJunkyardKeyLightDirectionFromTime=false; debug.mbUpdateValuesFromTool=false;
    debug.mbSimulateTimeOfDay=true;
    manager.mfTimeOfDay=100; gTrace.clear(); debug.Update();
    Check(Trace({2,3,4}) && !manager.tool && !manager.paused && manager.mfTimeOfDay==100,
          "simulation reads manager after unpause");
    debug.mbSimulateTimeOfDay=false; debug.muTimeOfDay_HH=1;
    gTrace.clear(); debug.Update();
    Check(Trace({2,3}) && manager.paused && manager.mfTimeOfDay==3600,
          "paused editing publishes after Pause");
}

static void GripWitnesses()
{
    using namespace BrnPhysics::Vehicle;
    GripCurveDebugWindow window;
    auto& longitudinal=window.mLongGripCurveGraph;
    auto& lateral=window.mLatGripCurveGraph;
    longitudinal.curve.maGripVariables={1,4,2,8}; lateral.curve.maGripVariables={3,2,10,6};
    window.Update(0.25f, CgsDev::DebugUI::INPUT_TEST);
    Check(longitudinal.range.x==4.8f && longitudinal.range.y==9.6f
          && longitudinal.range.z==4.8f && longitudinal.range.w==4.8f, "live longitudinal range and padding lanes");
    // Original 1.2f is 0x3F99999A; multiplying that rounded value by 3
    // rounds to 0x40666667, one ULP above the source literal 3.6f.
    Check(Bits(lateral.range.x)==0x40666667u && lateral.range.y==12
          && Bits(lateral.range.z)==0x40666667u && Bits(lateral.range.w)==0x40666667u,
          "live lateral range is independent");
    Check(window.updates==1 && window.time==0.25f && window.event==CgsDev::DebugUI::INPUT_TEST,
          "base update forwards timestep and event");
    Check(window.GetSlipRatioRange()==5 && window.GetCoefficientRange()==12, "initial joint ranges ceil after scalar scale");
    longitudinal.curve.maGripVariables={Float(1),Float(0x007fffff),Float(2),Float(3)};
    lateral.curve.maGripVariables=longitudinal.curve.maGripVariables;
    window.Update(0,CgsDev::DebugUI::INPUT_TEST);
    Check(Bits(longitudinal.range.x)==0 && Bits(longitudinal.range.y)==0
          && Bits(longitudinal.range.z)==0 && Bits(longitudinal.range.w)==0, "live subnormal inputs flush before maxima");
    Check(Bits(lateral.range.x)==0 && Bits(lateral.range.y)==0, "both curves flush independently");
    Check(window.GetSlipRatioRange()==0 && window.GetCoefficientRange()==0, "initial subnormal maxima produce zero rather than one");
    longitudinal.curve.maGripVariables={-Float(1),-Float(2),-Float(3),-Float(4)};
    lateral.curve.maGripVariables=longitudinal.curve.maGripVariables;
    window.Update(0,CgsDev::DebugUI::INPUT_TEST);
    Check(Bits(longitudinal.range.x)==0x80000000u && Bits(longitudinal.range.y)==0x80000000u,
          "negative subnormal inputs retain negative zero");
    Check(Bits(window.GetSlipRatioRange())==0x80000000u && Bits(window.GetCoefficientRange())==0x80000000u,
          "initial scalar ceil preserves negative zero");
    longitudinal.curve.maGripVariables={-0.0f,0,-0.0f,0}; lateral.curve.maGripVariables={0,-0.0f,0,-0.0f};
    window.Update(0,CgsDev::DebugUI::INPUT_TEST);
    Check(Bits(longitudinal.range.x)==0 && Bits(lateral.range.y)==0, "mixed zero signs prefer positive zero");
    const float nan=std::numeric_limits<float>::quiet_NaN();
    longitudinal.curve.maGripVariables={nan,2,4,nan}; lateral.curve.maGripVariables={2,3,5,6};
    window.Update(0,CgsDev::DebugUI::INPUT_TEST);
    Check(std::isnan(longitudinal.range.x) && std::isnan(longitudinal.range.y), "vector maxima propagate unordered lanes in either position");
    Check(window.GetSlipRatioRange()==4 && window.GetCoefficientRange()==8, "initial unordered scalar select chooses lateral maximum");
    longitudinal.curve.maGripVariables={2,3,5,6}; lateral.curve.maGripVariables={nan,2,nan,4};
    Check(std::isnan(window.GetSlipRatioRange()) && std::isnan(window.GetCoefficientRange()),
          "unordered lateral maximum survives scalar select");
}

int main()
{
    CgsGraphics::OcclusionCullManager defaults{};
    Check(!defaults.mbImmediateMode && defaults.mbConditionalRendering, "original occlusion default modes");
    TransferWitnesses(); UpdateWitnesses(); GripWitnesses();
    std::printf("DebugMenuEnvironmentMath: %u checks, %u failures\n",gChecks,gFailures);
    return gFailures ? 1 : 0;
}
