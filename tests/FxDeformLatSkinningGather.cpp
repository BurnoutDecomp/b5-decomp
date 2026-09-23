// Harness for run_fxdeformlat_skinning_gather.py (crash parity G17-D3, FX-DEFORM-LAT).
//
// The shipped DeformableObject::UpdateSkinningOffsets (BrnDeformableObject_Update.cpp) and its
// three file-scope constants are pasted in through methods.inc and run on zero-filled storage of
// the REAL DeformableObject / VehiclePhysics / TagPointSpec / IKBodyPartSpec types. The two
// IKBodyPart skinning bodies are fixtures that record which scratch ROW the part walk hands them.
//
// ARTIST UpdateSkinningOffsets @0x825DFA90, gather loop 0x825DFB14..0x825DFB88:
//   r9  = &maTagPoints[0].mpSpec (0x825DFB1C), tested `lbz 0x41` (0x825DFB24/28), advanced
//         `addi r9,r9,0x20` ONLY in the skinned arm (0x825DFB44)          -> the flag of the RUNNING ROW
//   r10 = &maTagPoints[0] (0x825DFB18), advanced every iteration (0x825DFB80) -> the DATA of tag li
// PS3 0x6D7178: flag *(32*v11+15120+this+0x10)+0x41 with v11 the running row. So the console
// gathers only the LEADING run of skinned tags, and the part walk starts at that count.
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"
#include "SharedClasses/Physics/Deformation/BrnTagPointSpec.h"
#include "SharedClasses/Physics/Deformation/BrnIKBodyPartSpec.h"
#include <cstdio>
#include <cstring>

static int giAsserts = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcExpr, const char*, int) { ++giAsserts; std::printf("  assert: %s\n", lpcExpr); return 0; }
void* EndAssert() { return nullptr; }
} }

using namespace BrnPhysics;
using namespace BrnPhysics::Deformation;

static const Vector3Plus* gpSkinRowSeen = nullptr;
static int giSkinCalls = 0;
namespace BrnPhysics { namespace Deformation {
void IKBodyPart::UpdateSkinningOffsets(Vector3Plus* lpaSkinningOffsets) { gpSkinRowSeen = lpaSkinningOffsets; ++giSkinCalls; }
void IKBodyPart::UpdateSkinningOffsetsWithinBox(Vector3Plus* lpaSkinningOffsets, const CgsGeometric::AxisAlignedBox*)
{ gpSkinRowSeen = lpaSkinningOffsets; ++giSkinCalls; }
} }

#include "methods.inc"

alignas(16) static unsigned char gObjectStorage[sizeof(DeformableObject)];
alignas(16) static unsigned char gVehicleStorage[sizeof(Vehicle::VehiclePhysics)];
alignas(16) static unsigned char gTagSpecStorage[8 * sizeof(TagPointSpec)];
alignas(16) static unsigned char gPartSpecStorage[sizeof(IKBodyPartSpec)];

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcCase, const char* lpcName)
{
    ++giChecks;
    if (!lbPass) { ++giFailures; std::printf("FAIL [%s]: %s\n", lpcCase, lpcName); }
}

// One case: `lpcFlags` gives each tag's mbSkinnedPoint ('S' skinned, 'u' unskinned); the console
// gathers the leading run of 'S' (liExpectRows rows, from tags 0..liExpectRows-1) and the part
// walk starts at that row.
static void RunCase(const char* lpcFlags, int liExpectRows)
{
    std::memset(gObjectStorage, 0, sizeof(gObjectStorage));
    std::memset(gTagSpecStorage, 0, sizeof(gTagSpecStorage));
    std::memset(gPartSpecStorage, 0, sizeof(gPartSpecStorage));
    DeformableObject& lrObject = *reinterpret_cast<DeformableObject*>(gObjectStorage);
    TagPointSpec*     laSpecs  = reinterpret_cast<TagPointSpec*>(gTagSpecStorage);
    IKBodyPartSpec&   lrPart   = *reinterpret_cast<IKBodyPartSpec*>(gPartSpecStorage);

    lrObject.mVehicleBody.mpAttachedVehicle = reinterpret_cast<Vehicle::VehiclePhysics*>(gVehicleStorage);
    const int liNumTags = static_cast<int>(std::strlen(lpcFlags));
    lrObject.miNumTagPoints = liNumTags;
    for (int li = 0; li < liNumTags; ++li)
    {
        laSpecs[li].mbSkinnedPoint = (lpcFlags[li] == 'S');
        laSpecs[li].mInitialPositionAndDetachThreshold = Vector3Plus{ 0.5f * li, 0.0f, 0.0f, 0.0f };
        lrObject.maTagPoints[li].mpSpec = &laSpecs[li];
        lrObject.maTagPoints[li].SetPosition(Vector3{ 0.5f * li + 1.0f + li, 2.0f * li, -1.0f * li, 0.0f });
        lrObject.maTagPoints[li].SetScratchAmount(0.25f * (li + 1));
    }
    for (int lr = 0; lr < 16; ++lr)   // sentinel rows: a row the gather must not touch stays 77
    {
        lrObject.maVerletOffsets_Scratch[lr] = Vector3Plus{ 77.0f, 77.0f, 77.0f, 77.0f };
    }
    lrObject.miNumIKBodyParts = 1;
    lrObject.maPartStates[0] = 0;                 // attached
    lrPart.miNumberOfDrivenPoints = 2;
    lrObject.maIKParts[0].mpSpec = &lrPart;
    gpSkinRowSeen = nullptr;
    giSkinCalls = 0;

    lrObject.UpdateSkinningOffsets();

    for (int lr = 0; lr < liExpectRows; ++lr)     // row r = tag r's (mPos - initial, scratch)
    {
        const Vector3Plus& lrRow = lrObject.maVerletOffsets_Scratch[lr];
        Check(lrRow.x == 1.0f + lr && lrRow.y == 2.0f * lr && lrRow.z == -1.0f * lr && lrRow.w == 0.25f * (lr + 1),
              lpcFlags, "row r holds tag r's offset and scratch (leading run only)");
    }
    Check(lrObject.maVerletOffsets_Scratch[liExpectRows].x == 77.0f, lpcFlags,
          "the row after the leading run is untouched");
    Check(giSkinCalls == 1 && gpSkinRowSeen == &lrObject.maVerletOffsets_Scratch[liExpectRows], lpcFlags,
          "the part walk starts at the leading-run count");
}

int main()
{
    RunCase("SSSS", 4);    // control: all skinned (every retail spec's leading run)
    RunCase("SSSuu", 3);   // control: unskinned tags LAST (PUSMC01's shape) -- both rules agree
    RunCase("SuS", 1);     // flag of the RUNNING row: tag 1's 'u' is re-tested for index 2
    RunCase("uSS", 0);     // an unskinned tag first: nothing is gathered
    RunCase("SSuSS", 2);
    Check(giAsserts == 0, "all", "valid fixtures fire no tripwire");
    std::printf("FxDeformLatSkinningGather: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
