// Harness for run_fxdeformlat_render_sensors.py (crash parity G17-D2, FX-DEFORM-LAT).
//
// The shipped DeformableObject::RenderSensors (BrnDeformableObject_Update.cpp), GetTransform
// (BrnDeformableObject_Accessors.cpp) and StreamedDeformationSpec::GetDeformationSensorSpec are
// pasted in through methods.inc and run on zero-filled storage of the REAL DeformableObject /
// VehiclePhysics / StreamedDeformationSpec types; the real rw::RGBA constructor is linked. The two
// Debug3DImmediateRender draws are recorder fixtures.
//
// ARTIST RenderSensors @0x825E08C0 (see the body's banner): per sensor, the selected one draws a
// white line to each linked sensor's world sphere (next - mu8NumVehicleBodies) and a white box;
// the others draw a box coloured 0xFF000000 | (u8)(s64)(clamp(d, 0, 0.2) * 5 * 255), d =
// |local centre - spec initial offset|; then each wheel draws a green box at its world sphere and a
// green line to its mapped sensor. Boxes are (-r,-r,-r,0)..(r,r,r,0) in a frame whose rows are the
// vehicle transform and whose translation is the sensor centre in world space.
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h"
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnStreamedDeformationSpec.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.h"
#include "SharedClasses/Physics/Deformation/BrnSensorSpec.h"
#include "rw/rwcore_structs.h"
#include "rw/math/vpu/vector3_operation.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

static int giAsserts = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcExpr, const char*, int) { ++giAsserts; std::printf("  assert: %s\n", lpcExpr); return 0; }
void* EndAssert() { return nullptr; }
} }

using namespace BrnPhysics;
using namespace BrnPhysics::Deformation;

struct DrawRecord { bool mbBox; Vector3 mA, mB; Matrix44Affine mT; u32 muRgba; };
static std::vector<DrawRecord> gDraws;
namespace CgsDev {
void Debug3DImmediateRender::DrawLine(rw::math::vpu::Vector3 lFrom, rw::math::vpu::Vector3 lTo, rw::RGBA lColour)
{
    DrawRecord r{}; r.mbBox = false; r.mA = lFrom; r.mB = lTo; r.muRgba = lColour.m_rgba; gDraws.push_back(r);
}
void Debug3DImmediateRender::DrawBox(rw::math::vpu::Vector3 lMin, rw::math::vpu::Vector3 lMax,
                                     rw::math::vpu::Matrix44Affine lTransform, rw::RGBA lColour)
{
    DrawRecord r{}; r.mbBox = true; r.mA = lMin; r.mB = lMax; r.mT = lTransform; r.muRgba = lColour.m_rgba; gDraws.push_back(r);
}
}

#include "methods.inc"

alignas(16) static unsigned char gObjectStorage[sizeof(DeformableObject)];
alignas(16) static unsigned char gVehicleStorage[sizeof(Vehicle::VehiclePhysics)];
alignas(16) static unsigned char gSpecStorage[sizeof(StreamedDeformationSpec)];
alignas(16) static unsigned char gRenderStorage[sizeof(CgsDev::Debug3DImmediateRender)];

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName, int liIndex = -1)
{
    ++giChecks;
    if (!lbPass) { ++giFailures; std::printf("FAIL: %s (%d)\n", lpcName, liIndex); }
}
static bool Near(const Vector3& a, float x, float y, float z) { return std::fabs(a.x - x) < 1e-5f && std::fabs(a.y - y) < 1e-5f && std::fabs(a.z - z) < 1e-5f; }
static u32 Rgba(u8 r, u8 g, u8 b, u8 a) { return rw::RGBA(r, g, b, a).m_rgba; }

int main()
{
    DeformableObject&        lrObject  = *reinterpret_cast<DeformableObject*>(gObjectStorage);
    Vehicle::VehiclePhysics& lrVehicle = *reinterpret_cast<Vehicle::VehiclePhysics*>(gVehicleStorage);
    StreamedDeformationSpec& lrSpec    = *reinterpret_cast<StreamedDeformationSpec*>(gSpecStorage);
    CgsDev::Debug3DImmediateRender* lpRender = reinterpret_cast<CgsDev::Debug3DImmediateRender*>(gRenderStorage);

    // Vehicle frame: a 90-degree yaw (x -> -z, z -> x) at (10, 20, 30).
    Matrix44Affine lCar;
    lCar.xAxis = Vector3{ 0.0f, 0.0f, -1.0f, 0.0f };
    lCar.yAxis = Vector3{ 0.0f, 1.0f,  0.0f, 0.0f };
    lCar.zAxis = Vector3{ 1.0f, 0.0f,  0.0f, 0.0f };
    lCar.wAxis = Vector3{ 10.0f, 20.0f, 30.0f, 1.0f };
    lrVehicle.mTransform = lCar;
    lrObject.mVehicleBody.mpAttachedVehicle = &lrVehicle;
    lrObject.mpDeformationSpec = &lrSpec;

    // Three sensors; body count 1, so a streamed "next sensor" n names sensor n - 1.
    lrSpec.mu8NumDeformationSensors = 3;
    lrSpec.mu8NumVehicleBodies = 1;
    static CgsGeometric::Sphere saLocal[3], saWorld[3];
    const float lafDisp[3] = { 0.1f, 0.0f, 0.5f };   // |local - initial|: ramp 127, (selected), 255
    for (int i = 0; i < 3; ++i)
    {
        saLocal[i].mPositionRadius = Vector4{ 1.0f + i, 2.0f, 3.0f, 0.25f * (i + 1) };
        saWorld[i].mPositionRadius = Vector4{ 100.0f + i, 200.0f + i, 300.0f + i, 0.5f };
        lrSpec.maDeformationSensorSpecs[i].mInitialOffset = Vector3{ 1.0f + i - lafDisp[i], 2.0f, 3.0f, 0.0f };
        lrObject.maDeformationSensors[i].mpSpec = &lrSpec.maDeformationSensorSpecs[i];
        lrObject.maDeformationSensors[i].mpLocalSpaceSphere = &saLocal[i];
        lrObject.maDeformationSensors[i].mpWorldSpaceSphere = &saWorld[i];
    }
    const u8 lau8Next[6] = { 2, 0, 3, 0, 1, 0 };     // sensor 1 links to sensors 1, 2, 0
    std::memcpy(lrSpec.maDeformationSensorSpecs[1].maNextSensor, lau8Next, 6);
    for (int j = 0; j < 4; ++j)
    {
        lrObject.maWorldSensorSpheres[3 + j].mPositionRadius = Vector4{ -5.0f - j, 1.0f, 7.0f, 0.4f };
    }
    const u8 lau8Map[4] = { 2, 1, 0, 2 };
    std::memcpy(lrObject.mau8WheelToSensorMap, lau8Map, 4);

    lrObject.RenderSensors(lpRender, 1);

    if (gDraws.size() != 14u)
    {
        std::printf("  draws: %zu\n", gDraws.size());
        for (const DrawRecord& r : gDraws)
            std::printf("   %s rgba %08X a(%g %g %g) b(%g %g %g)\n", r.mbBox ? "box " : "line", r.muRgba, r.mA.x, r.mA.y, r.mA.z, r.mB.x, r.mB.y, r.mB.z);
    }
    Check(gDraws.size() == 14u, "14 draws: box, 3 links + box, box, then 4 x (box, line)");
    if (gDraws.size() < 14u) gDraws.resize(14u);   // a missing draw fails every check that reads it
    {
        const DrawRecord& s0 = gDraws[0];   // sensor 0: local (1,2,3) -> world (10+3, 20+2, 30-1)
        Check(s0.mbBox && Near(s0.mT.wAxis, 13.0f, 22.0f, 29.0f), "sensor box at the world centre", 0);
        Check(Near(s0.mA, -0.25f, -0.25f, -0.25f) && Near(s0.mB, 0.25f, 0.25f, 0.25f) && s0.mA.w == 0.0f && s0.mB.w == 0.0f,
              "box extent (-r..r, w 0)", 0);
        Check(Near(s0.mT.xAxis, 0.0f, 0.0f, -1.0f) && Near(s0.mT.zAxis, 1.0f, 0.0f, 0.0f), "box rows = vehicle rows", 0);
        Check(s0.muRgba == Rgba(0, 0, 127, 0xFF), "ramp clamp(0.1,0,0.2)*5*255 -> 127 (blue)", 0);
        const int laLinkTo[3] = { 1, 2, 0 };
        for (int k = 0; k < 3; ++k)
        {
            const DrawRecord& l = gDraws[1 + k];
            Check(!l.mbBox && Near(l.mA, 13.0f, 22.0f, 28.0f), "selected-sensor link from its world centre (local (2,2,3))", k);
            Check(Near(l.mB, 100.0f + laLinkTo[k], 200.0f + laLinkTo[k], 300.0f + laLinkTo[k]),
                  "link to sensor (next - mu8NumVehicleBodies)'s world sphere", k);
            Check(l.muRgba == 0xFFFFFFFFu, "link white", k);
        }
        Check(gDraws[4].mbBox && gDraws[4].muRgba == 0xFFFFFFFFu && Near(gDraws[4].mB, 0.5f, 0.5f, 0.5f),
              "selected sensor box white, r = 0.5", 1);
        Check(gDraws[5].mbBox && gDraws[5].muRgba == Rgba(0, 0, 255, 0xFF), "ramp clamps at 0.2 -> 255", 2);
        for (int j = 0; j < 4; ++j)
        {
            const DrawRecord& b = gDraws[6 + 2 * j];
            const DrawRecord& l = gDraws[7 + 2 * j];
            Check(b.mbBox && b.muRgba == Rgba(0, 0xFF, 0, 0xFF) && Near(b.mT.wAxis, -5.0f - j, 1.0f, 7.0f)
                  && b.mT.wAxis.w == 0.4f && Near(b.mB, 0.4f, 0.4f, 0.4f), "wheel box green at the wheel sphere", j);
            Check(!l.mbBox && l.muRgba == Rgba(0, 0xFF, 0, 0xFF) && Near(l.mA, -5.0f - j, 1.0f, 7.0f)
                  && Near(l.mB, 100.0f + lau8Map[j], 200.0f + lau8Map[j], 300.0f + lau8Map[j]),
                  "wheel line green to its mapped sensor", j);
        }
    }
    Check(giAsserts == 0, "valid fixture fires no tripwire");
    std::printf("FxDeformLatRenderSensors: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
