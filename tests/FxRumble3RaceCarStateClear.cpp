// FX-RUMBLE3 (crash parity 2026-09-24): BrnPhysics::Vehicle::RaceCarState::Clear @0x8229FFC8 -- the PRODUCTION
// body (and the file-local constants block it reads), extracted from
// src/GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.cpp by run_fxrumble3_race_car_state_clear.py
// and compiled against the real BrnVehicleEvents.h. Every expected value is read off the ARTIST asm:
//   0x8229FFD4..0x8229FFE0  memset(this, 0, 0x460)
//   0x822A0004..0x822A0094  mTransform (+0x1F0) = {1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,0}
//   0x822A00A4..0x822A0130  the 4-pass wheel-transform loop (+0x230, stride 0x40), same four rows
//   0x822A0098..0x822A00A0  lwz dword_82CDB5A0 (image 0xFFFFFFFF) -> stw 0x3C8 (+968 == mEntityId)
//   0x822A0138..0x822A0158  AGTR at +0x1C0: 32 zero bytes, +0x1E0 f 0.0, +0x1E4 sth -1, +0x1E6 sth 0x8000, +0x1E8 0
//   0x822A015C..0x822A0170  byte 0 at +0x44C/+0x451/+0x450/+0x452/+0x453, word 0 at +0x458
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cstddef>
#include <cstdio>
#include <cstring>

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { return 0; }
    void* EndAssert() { return nullptr; }
}
}

#include "fxrumble3_race_car_state_clear.inc"

static unsigned gChecks = 0, gFailures = 0;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::printf("  FAIL %s\n", lpcName);
    }
    else
    {
        std::printf("  ok   %s\n", lpcName);
    }
}

using BrnPhysics::Vehicle::RaceCarState;

static bool RowIs(const Vector3& lrRow, float x, float y, float z, float w)
{
    return lrRow.x == x && lrRow.y == y && lrRow.z == z && lrRow.w == w;
}

static bool IsClearIdentity(const Matrix44Affine& lrMatrix)
{
    return RowIs(lrMatrix.xAxis, 1.0f, 0.0f, 0.0f, 0.0f) && RowIs(lrMatrix.yAxis, 0.0f, 1.0f, 0.0f, 0.0f)
        && RowIs(lrMatrix.zAxis, 0.0f, 0.0f, 1.0f, 0.0f) && RowIs(lrMatrix.wAxis, 0.0f, 0.0f, 0.0f, 0.0f);
}

static unsigned ReadWordAt(const void* lpBase, size_t luOffset)
{
    unsigned luWord = 0;
    std::memcpy(&luWord, static_cast<const unsigned char*>(lpBase) + luOffset, sizeof(luWord));
    return luWord;
}

int main()
{
    // Storage deliberately filled with a non-zero pattern first, so a store the body skips is visible.
    alignas(16) static unsigned char lacStorage[sizeof(RaceCarState)];
    std::memset(lacStorage, 0xCD, sizeof(lacStorage));
    RaceCarState* lpState = reinterpret_cast<RaceCarState*>(lacStorage);
    lpState->Clear();

    Check(sizeof(RaceCarState) == 0x460, "sizeof == 0x460, the memset length at 0x8229FFD4");
    Check(offsetof(RaceCarState, mEntityId) == 0x3C8, "mEntityId sits at +0x3C8, the target of stw r8,0x3C8(r31) @0x822A00A0");
    Check(offsetof(RaceCarState, mfSpeedMPH) == 0x3CC, "mfSpeedMPH sits at +0x3CC (after the 8-byte key @+0x3C0)");
    Check(lpState->mEntityId.muValue == 0xFFFFFFFFu,
          "mEntityId == dword_82CDB5A0 == 0xFFFFFFFF (K_INVALID_ENTITY_ID), not the memset's 0");
    Check(ReadWordAt(lpState, 0x3C8) == 0xFFFFFFFFu, "the four bytes at +0x3C8 are all 0xFF");
    Check(lpState->mfSpeedMPH == 0.0f && ReadWordAt(lpState, 0x3CC) == 0u,
          "mfSpeedMPH (+0x3CC) keeps the memset's 0 -- the console never stores it in Clear");
    Check(IsClearIdentity(lpState->mTransform), "mTransform (+0x1F0) = identity basis with a ZERO w row");
    bool lbWheels = true;
    for (int li = 0; li < 4; ++li)
        lbWheels = lbWheels && IsClearIdentity(lpState->maWheelTransforms[li]);
    Check(lbWheels, "all four wheel transforms (+0x230 stride 0x40) = the same identity rows");
    Check(lpState->mAboveGroundTestResult.mfVerticalDistance == 0.0f
          && lpState->mAboveGroundTestResult.mCollisionTag.muValue == 0xFFFF8000u
          && !lpState->mAboveGroundTestResult.mbValid,
          "AGTR: distance 0.0, tag {0xFFFF,0x8000} (the host word 0xFFFF8000), not valid");
    Check(!lpState->mbIsDriveable && !lpState->mbIsFrontRayOccluded && !lpState->mbIsWedgedInWorld
          && !lpState->mbIsHidden && !lpState->mbContactingWall
          && static_cast<int>(lpState->meDriverType) == 0,
          "the tail bytes +0x44C/+0x450..+0x453 and the driver type +0x458 are 0");
    Check(lpState->mLinearVelocity.x == 0.0f && lpState->miRaceCarID == 0 && lpState->mCarAssetAttribKey == 0u,
          "everything else is the memset's zero");

    std::printf("FxRumble3RaceCarStateClear: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
