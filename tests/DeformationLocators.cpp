// Exercise the production locator bodies with real data types and hand-computed frames.
#define _ALLOW_KEYWORD_MACROS 1
#define private public
#define protected public
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h"
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDetachedPartManager.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehicleAttribs.h"
#undef protected
#undef private
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <new>

namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* message, const char*, int)
{ std::fprintf(stderr, "Assertion: %s\n", message); std::abort(); }
void* EndAssert() { return nullptr; }
} }

// Only the locator path runs. Unrelated virtual entries fail loudly if reached.
namespace BrnPhysics { namespace Deformation {
DeformableObject::DeformableObject() {}
DeformationSensor::DeformationSensor() {}
void DeformationSensor::ApplyLocalImpulse(ImpulseParams*) { std::abort(); }
void DeformationSensor::RecievePassedOnImpulse(const ImpulseParams*, VecFloat) { std::abort(); }
void VehicleRigidBody::ApplyLocalImpulse(ImpulseParams*) { std::abort(); }
void VehicleRigidBody::RecievePassedOnImpulse(const ImpulseParams*, VecFloat) { std::abort(); }
} namespace Vehicle {
VecFloat SimpleVehiclePhysics::GetSteeringAngle() const { std::abort(); }
void SimpleVehiclePhysics::ClearCrashing() { std::abort(); }
void SimpleVehiclePhysics::SetCrashing() { std::abort(); }
void VehiclePhysics::Update(VecFloat, VecFloat, const Matrix44Affine*, const BrnPlayerDriverControls*,
    bool, bool, bool, CgsNumeric::Random&) { std::abort(); }
void VehiclePhysics::UpdateSuspension(VecFloat) { std::abort(); }
void VehiclePhysics::SetCrashing() { std::abort(); }
void VehiclePhysics::ClearCrashing() { std::abort(); }
VecFloat VehiclePhysics::GetSteeringAngle() const { std::abort(); }
} }

using namespace BrnPhysics::Deformation;
using namespace BrnPhysics::Vehicle;

static void Check(bool condition, const char* message)
{ if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::abort(); } }

static void CheckVector(Vector3 actual, Vector3 expected, const char* message)
{
    if (std::fabs(actual.x - expected.x) > 0.0001f ||
        std::fabs(actual.y - expected.y) > 0.0001f ||
        std::fabs(actual.z - expected.z) > 0.0001f)
    {
        std::fprintf(stderr, "FAIL: %s: got (%g,%g,%g), expected (%g,%g,%g)\n",
            message, actual.x, actual.y, actual.z, expected.x, expected.y, expected.z);
        std::abort();
    }
}

int main()
{
    DeformableObject model;
    DetachedPartManager parts;
    VehiclePhysics vehicle;
    VehicleAttribs attributes{};
    StreamedDeformationSpec spec{};
    IKBodyPartSpec partSpec{};
    model.mpDeformationSpec = &spec;
    model.mVehicleBody.mpAttachedVehicle = &vehicle;
    vehicle.mpAttribs = &attributes;
    spec.miNumberOfIKParts = 1;
    spec.mMeshOffset = {0.5f, 1.5f, 2.5f, 0};
    attributes.mBaseAttribs.mCOMOffset = {2, 3, 4, 0};
    vehicle.mSimpleAttribs.mCOMOffset = attributes.mBaseAttribs.mCOMOffset;
    vehicle.mSimpleAttribs.mbIsValid = true;

    // Vehicle is rotated 90 degrees about Z. Its graphics origin is (100,200,300)
    // after subtracting the rotated COM offset from the physics position.
    Matrix44Affine car;
    car.SetIdentity();
    car.xAxis = {0, 1, 0, 0};
    car.yAxis = {-1, 0, 0, 0};
    car.wAxis = {97, 202, 304, 0};
    vehicle.SetTransform(car);

    // A part rotated 90 degrees about Y, with distinct local graphics/COM offsets.
    partSpec.mGraphicsTransform.SetIdentity();
    partSpec.mGraphicsTransform.wAxis = {5, 6, 7, 0};
    model.maIKParts[0].mpSpec = &partSpec;
    model.maIKParts[0].SetPartPoolIndex(7);
    model.mau8PhysicalBodyPartPoolIndex[0] = 2; // Must use the IK part's slot, as ARTIST does.
    parts.mPartPool.mUsedParts.UnSetAll();
    parts.mPartPool.mUsedParts.SetBit(7);
    PhysicalBodyPart& part = parts.mPartPool.maParts[7];
    Matrix44Affine partWorld;
    partWorld.SetIdentity();
    partWorld.xAxis = {0, 0, -1, 0};
    partWorld.zAxis = {1, 0, 0, 0};
    partWorld.wAxis = {110, 210, 310, 0};
    part.mRwBody.SetTransform(partWorld);
    part.mLocalGraphicsPositionPlusJointVelocity = {8, 9, 10, 0};
    part.mLocalInitialComPositionPlusMaxJointAngle = {1, 2, 3, 0};

    LocatorPointSpec locator{};
    locator.mLocatorMatrix.SetIdentity();
    locator.mLocatorMatrix.wAxis = {10, 20, 30, 0};
    locator.mu8SkinPoint = 4;
    locator.miIkPartIndex = -1;
    model.maVerletOffsets_Scratch[4] = {1, 2, 3, 0};
    ETagPointType type = E_TAGPOINT_FXENGINE;
    Matrix44Affine output, unusedInverse;
    unusedInverse.SetIdentity();
    model.UpdateLocator(output, type, &locator, unusedInverse, &parts);
    CheckVector(output.wAxis, {11, 22, 33, 0}, "unparented locator deforms in graphics space");
    Check(type == E_TAGPOINT_FXENGINE, "unparented locator retains type");
    locator.miIkPartIndex = 0;
    model.maPartStates[0] = DeformableObject::E_PART_STATE_ATTACHED_IK;
    model.UpdateLocator(output, type, &locator, unusedInverse, &parts);
    CheckVector(output.wAxis, {11, 22, 33, 0}, "attached locator uses skin displacement only");
    model.maPartStates[0] = DeformableObject::E_PART_STATE_DETATCHED;
    model.UpdateLocator(output, type, &locator, unusedInverse, &parts);
    Check(type == E_TAGPOINT_COUNT, "detached locator becomes unavailable");

    // The streamed lists use real 32-bit pointer slots, like the game's low-memory arena.
    LocatorPointSpec* records = nullptr;
    for (uintptr_t address = 0x10000000; !records && address < 0xF0000000; address += 0x10000)
        records = static_cast<LocatorPointSpec*>(VirtualAlloc(reinterpret_cast<void*>(address),
            65536, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Check(records != nullptr, "allocate streamed locator fixture below 4GB");
    for (int i = 0; i < 3; ++i) new (&records[i]) LocatorPointSpec(locator);
    records[1].miIkPartIndex = records[2].miIkPartIndex = -1;
    records[1].mLocatorMatrix.wAxis = {20, 30, 40, 0};
    records[2].mLocatorMatrix.wAxis = {30, 40, 50, 0};
    spec.mGenericTags = {1, {static_cast<u32>(reinterpret_cast<uintptr_t>(&records[0]))}};
    spec.mLightTags = {1, {static_cast<u32>(reinterpret_cast<uintptr_t>(&records[1]))}};
    spec.mCameraTags = {1, {static_cast<u32>(reinterpret_cast<uintptr_t>(&records[2]))}};
    model.mLocatorData.miNumGenericLocators = 0;
    model.mLocatorData.miNumLightLocators = 0;
    model.mLocatorData.miNumCameraLocators = 0; // The console reads counts from the spec.
    model.mLocatorData.maGenericLocatorTypes[0] = E_TAGPOINT_FXENGINE;
    model.maPartStates[0] = DeformableObject::E_PART_STATE_HINGED;
    model.UpdateLocators(&parts);

    // Hand-computed chain: local point (4.5,14.5,24.5), part render origin
    // (117,217,303), world point (141.5,231.5,298.5), then inverse vehicle frame.
    const Matrix44Affine& hinged = model.mLocatorData.maGenericLocators[0];
    CheckVector(hinged.wAxis, {31.5f, -41.5f, -1.5f, 0}, "hinged locator position");
    CheckVector(hinged.xAxis, {0, 0, -1, 0}, "hinged locator X axis");
    CheckVector(hinged.yAxis, {1, 0, 0, 0}, "hinged locator Y axis");
    CheckVector(hinged.zAxis, {0, -1, 0, 0}, "hinged locator Z axis");
    CheckVector(model.mLocatorData.maLightLocators[0].wAxis, {21, 32, 43, 0}, "light spec routing");
    CheckVector(model.mLocatorData.maCameraLocators[0].wAxis, {31, 42, 53, 0}, "camera spec routing");
    Check(model.mLocatorData.maGenericLocatorTypes[0] == E_TAGPOINT_FXENGINE,
        "hinged locator retains its type");
    VirtualFree(records, 0, MEM_RELEASE);
    std::puts("PASS: deformation locator states, pool index, three spec lists and rotated COM/part frames");
}
