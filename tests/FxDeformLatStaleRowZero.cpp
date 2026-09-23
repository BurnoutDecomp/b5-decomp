// Harness for run_fxdeformlat_stale_row_zero.py (crash parity G24-D2, FX-DEFORM-LAT).
//
// The shipped statements that follow the DeformableObject::Prepare call in
// DeformationManager::ProcessAddDeformationModelEvents (BrnDeformationManager.cpp) are pasted into
// ManagerProbe::Run through block.inc and run on a sentinel-filled REAL DeformationManager.
//
// ARTIST 0x82644DB8..0x82644DDC (PS3 0x76AF48..0x76AF74): stvx128 v0(0) -> this + 0x60 + 0x700*model,
// unconditionally. With mStateOutput at this+0x30 that is &mStateOutput + 0x30 + 0x700*model for
// models 0..26 (DeformationState is pointer-free: the console arithmetic holds on the host), and for
// model 27 DetachedPartManager+0x170 == pool slot 0's mLocalGraphicsPositionPlusJointVelocity.
// The expected landing is derived from that ARITHMETIC (not from the production's by-name mapping),
// and every other byte of the manager must be untouched.
#include "GameSource/Physics/DeformationManager/BrnDeformationManager.h"
#include <cstdio>
#include <cstring>

static int giAsserts = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcExpr, const char*, int) { ++giAsserts; std::printf("  assert: %s\n", lpcExpr); return 0; }
void* EndAssert() { return nullptr; }
} }

namespace BrnPhysics { namespace Deformation {
struct ManagerProbe : public DeformationManager
{
    void Run(s32 liModelIndex)
    {
#include "block.inc"
    }
};
} }

using namespace BrnPhysics::Deformation;
alignas(16) static unsigned char gManager[sizeof(DeformationManager)];
alignas(16) static unsigned char gBefore[sizeof(DeformationManager)];

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName, int liModel)
{
    ++giChecks;
    if (!lbPass) { ++giFailures; std::printf("FAIL: %s (model %d)\n", lpcName, liModel); }
}

int main()
{
    ManagerProbe& lrProbe = *reinterpret_cast<ManagerProbe*>(gManager);
    const int laModels[] = { 0, 7, 19, 20, 21, 26, 27 };
    for (int liModel : laModels)
    {
        for (size_t lu = 0; lu < sizeof(gManager); ++lu) gManager[lu] = static_cast<unsigned char>(0x5A ^ (lu * 7));
        std::memcpy(gBefore, gManager, sizeof(gManager));

        unsigned char* lpExpected = (liModel < 27)
            ? reinterpret_cast<unsigned char*>(&lrProbe.mStateOutput) + 0x30 + 0x700 * liModel
            : reinterpret_cast<unsigned char*>(&lrProbe.mDetachedPartManager.mPartPool.maParts[0].mLocalGraphicsPositionPlusJointVelocity);
        const size_t luAt = static_cast<size_t>(lpExpected - gManager);

        lrProbe.Run(liModel);

        static const unsigned char kau8Zero[16] = {};
        Check(std::memcmp(gManager + luAt, kau8Zero, 16) == 0, "the 16-byte row at manager+0x60+0x700*model is zeroed", liModel);
        const bool lbRestSame = std::memcmp(gManager, gBefore, luAt) == 0
            && std::memcmp(gManager + luAt + 16, gBefore + luAt + 16, sizeof(gManager) - luAt - 16) == 0;
        Check(lbRestSame, "no other byte of the manager changes (control)", liModel);
    }
    Check(giAsserts == 0, "no tripwire (control)", -1);
    std::printf("FxDeformLatStaleRowZero: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
