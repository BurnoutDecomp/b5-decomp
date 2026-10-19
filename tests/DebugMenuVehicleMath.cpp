// Bounded ARTIST witnesses for editable wheelie, takeoff and compression controls.
// The .inc contains verbatim production helpers/blocks/callback, not a second
// implementation. Power-of-two ramps have exact independent expected answers;
// exceptional cases distinguish the original predicates/refinements from /.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <limits>

#if defined(DEBUG_MENU_USE_STD_SIN)
namespace XboxMath { inline float XMVectorSin(float value) { return std::sin(value); } }
#else
#include "SDKs/XboxMath/XMVectorSin.h"
#endif

using f32 = float;
using f64 = double;
using s32 = int;
using u32 = unsigned int;
struct Vector3 { float x, y, z, w; };
using VecFloat = Vector3;
namespace vpu
{
    inline Vector3 Splat(float value) { return {value, value, value, value}; }
    inline float Dot(Vector3 a, Vector3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
}
inline Vector3 operator*(Vector3 a, float b) { return {a.x*b,a.y*b,a.z*b,a.w*b}; }
inline Vector3 operator-(Vector3 a, Vector3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z,a.w-b.w}; }

static unsigned gChecks = 0, gFailures = 0, gAssertions = 0;
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

struct VehicleFixture
{
    struct Transform { Vector3 xAxis; } mTransform{{1,0,0,0}};
    Vector3 mAngularVelocity{-8,0,0,0};
    Vector3 mTotalTorque{-4,0,0,0};
    Vector3 mTotalAngularImpulse{-2,0,0,0};
    float gfWheelieLimitDamping = 0.25f;
    void WheelieLimit(float pitch, float angle);
};

enum ECompressPreset { E_COMPRESS_NONE=0, E_COMPRESS_MAX_DRIVETIME=1, E_COMPRESS_MAX_TOTAL=2 };
struct DeformationDebugComponent
{
    void* mpSelectedRig = this;
    int miCompressPreset = 0;
    unsigned calls = 0, driveCalls = 0;
    float values[6]{};
    void CompressSelectedRig(float a, float b, float c, float d, float e, float f)
    {
        ++calls;
        values[0]=a; values[1]=b; values[2]=c; values[3]=d; values[4]=e; values[5]=f;
    }
    void CompressSelectedRig_MaxDrivetime() { ++driveCalls; }
    static void OnCompressionPresetChange(void*, void*);
};
#define CGS_ASSERT(condition, message) do { if (!(condition)) ++gAssertions; } while (false)
#include "debug_menu_vehicle_math.inc"

int main()
{
    // Original 821F05E0..06AC and coefficient words: the 1.5-degree result is
    // one ULP below rounded mathematical sine, so strict comparison matters.
    Check(Bits(1.5f * Float(0x3C8EFA35u)) == 0x3CD67750u, "original degree conversion");
    Check(Bits(XboxMath::XMVectorSin(Float(0x3CD67750u))) == 0x3CD6710Au, "original 1.5-degree sine");
    Check(Bits(XboxMath::XMVectorSin(Float(0xBCD67750u))) == 0xBCD6710Au, "original negative sine");
    Check(Bits(XboxMath::XMVectorSin(0.0f)) == 0u, "zero sine");
    Check(std::isnan(XboxMath::XMVectorSin(std::numeric_limits<float>::infinity())), "infinite sine is unordered");

    VehicleFixture upper;
    upper.WheelieLimit(Float(0x3CD6710Bu),1.5f);
    Check(upper.mAngularVelocity.x == -6.0f, "upper-ULP pitch bleeds angular velocity");
    Check(upper.mTotalTorque.x == -3.0f, "upper-ULP pitch bleeds torque");
    Check(upper.mTotalAngularImpulse.x == -1.5f, "upper-ULP pitch bleeds pending impulse");
    VehicleFixture equal;
    equal.WheelieLimit(Float(0x3CD6710Au),1.5f);
    Check(equal.mAngularVelocity.x == -8.0f && equal.mTotalTorque.x == -4.0f &&
          equal.mTotalAngularImpulse.x == -2.0f, "strict equality preserves all rows");
    VehicleFixture below;
    below.WheelieLimit(Float(0x3CD67109u),1.5f);
    Check(below.mAngularVelocity.x == -8.0f, "below threshold preserves velocity");
    VehicleFixture positive;
    positive.mAngularVelocity.x=8; positive.mTotalTorque.x=4; positive.mTotalAngularImpulse.x=2;
    positive.WheelieLimit(Float(0x3CD6710Bu),1.5f);
    Check(positive.mAngularVelocity.x == 8 && positive.mTotalTorque.x == 4 &&
          positive.mTotalAngularImpulse.x == 2, "positive components stay unchanged");

    const float rolls[] = {0.0f,0.125f,0.25f,0.375f,0.5f,0.75f};
    const float factors[] = {0.0f,0.25f,0.5f,0.75f,1.0f,1.0f};
    for (unsigned i=0;i<6;++i)
        Check(TakeoffRamp(rolls[i],0.25f,0.5f,0.5f)==factors[i], "edited dyadic ramp");
    const float nan=std::numeric_limits<float>::quiet_NaN();
    const float inf=std::numeric_limits<float>::infinity();
    Check(std::isnan(TakeoffRamp(0,0,0.5f,0.5f)), "zero full threshold keeps unconditional refinement");
    Check(std::isnan(TakeoffRamp(0,Float(1),0.5f,0.5f)), "subnormal full threshold flushes before ramp");
    Check(std::isnan(TakeoffRamp(0,inf,inf,0.5f)), "infinite denominator keeps unconditional refinement");
    Check(TakeoffRamp(0.125f,nan,0.5f,0.5f)==1, "NaN full threshold takes final leg");
    Check(TakeoffRamp(nan,0.25f,0.5f,0.5f)==1, "NaN roll takes final leg");
    Check(TakeoffRamp(0.375f,0.25f,nan,0.5f)==1, "NaN no-damp threshold takes final leg");
    Check(TakeoffRamp(0.375f,0.5f,0.25f,0.5f)==0.375f, "inverted thresholds retain first ordered gate");
    Check(TakeoffRamp(0.625f,0.5f,0.25f,0.5f)==1, "inverted thresholds retain final leg");

    for (int preset : {0,1,2,3,-1,std::numeric_limits<int>::min()})
    {
        DeformationDebugComponent fixture;
        fixture.miCompressPreset=preset;
        const unsigned before=gAssertions;
        DeformationDebugComponent::OnCompressionPresetChange(nullptr,&fixture);
        if (preset<0 || preset>=3)
            Check(gAssertions==before+1 && fixture.calls==0 && fixture.driveCalls==0,
                  "invalid unsigned preset asserts without compression");
        else
        {
            Check(gAssertions==before && fixture.calls==1 && fixture.driveCalls==(preset==1 ? 1u:0u),
                  "valid preset dispatch");
            bool lanes=true;
            for (float value : fixture.values) lanes=lanes && value==(preset==2 ? 1.0f:0.0f);
            Check(lanes,"all six compression lanes");
        }
    }
    DeformationDebugComponent noRig;
    noRig.mpSelectedRig=nullptr; noRig.miCompressPreset=-1;
    const unsigned before=gAssertions;
    DeformationDebugComponent::OnCompressionPresetChange(nullptr,&noRig);
    Check(gAssertions==before && noRig.calls==0 && noRig.driveCalls==0,"null rig exits before preset validation");

    std::printf("DebugMenuVehicleMath: %u checks, %u failures\n",gChecks,gFailures);
    return gFailures ? 1:0;
}
