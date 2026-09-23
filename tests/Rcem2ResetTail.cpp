// FX-RCEM2 (crash-parity 2026-09-23): the tail of RaceCarEntityModule::ResetActiveRaceCar's
// live-car arm (ARTIST 0x822F4880), extracted VERBATIM from BrnRaceCarEntityModule.cpp by
// run_rcem2_reset_tail.py -- from the VehicleInputInterface::ResetRaceCar call to the arm's
// `return;` -- and replayed against fixture cars. Every expectation is an ARTIST instruction:
//   0x822F4B2C  bl VehicleInputInterface::ResetRaceCar
//   0x822F4B30..0x822F4C10  G67-D4  r26 = this + 0x100E0 (mabResetThisFrame); the inlined
//               BitArray<8>::SetBit bound tripwire (`cmplwi r27, 8`, CgsBitArray.h:222) and
//               `ldx ; sld 1,(slot & 63) ; or ; stdx` -- BEFORE ResetAfterCrash
//   0x822F4C14  bl ActiveRaceCar::ResetAfterCrash, r4 = (resetDeformation == 0)
//   0x822F4C18..0x822F4C40  G67-D5  if (resetDeformation) { stb 0, 0x1BF6 (mu8RenderDamageFlags);
//               8x stw 0 from 0x1BF8 (mafCrackedGlassFractureAmount) } -- AFTER ResetAfterCrash;
//               the equalisation factors that follow are untouched
#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsBitArray.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstring>

static unsigned guAssertions = 0;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int) { ++guAssertions; std::fprintf(stderr, "ASSERT: %s\n", lpcMessage); return 0; }
void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; void WriteToLog(const char*) {} }
namespace Message { u64 gxMessageFilterFlags = 0; }
}

namespace BrnPhysics { namespace Deformation { enum DeformationResetType : s32 { E_RESET_NONE = -1 }; } }

namespace Fixture {

enum EActiveRaceCarIndex : s32 { E_ACTIVE_RACE_CAR_INDEX_0 = 0, E_ACTIVE_RACE_CAR_INDEX_COUNT = 8 };

struct Vector3 { f32 x, y, z, w; };
struct Matrix44Affine { Vector3 xAxis, yAxis, zAxis, wAxis; };

struct RaceCarEntityModule;
static RaceCarEntityModule* gpModule = nullptr;
static int giOrder = 0;

struct VehicleInputInterface {
    int  miResets = 0, miOrder = -1;
    u32  muIndex = 99;
    bool mbResetDeformation = false;
    void ResetRaceCar(u32 luIndex, const Matrix44Affine&, const Vector3&, const Vector3&, u8,
                      bool, bool lbResetDeformation, f32, bool,
                      BrnPhysics::Deformation::DeformationResetType) {
        ++miResets; muIndex = luIndex; mbResetDeformation = lbResetDeformation; miOrder = giOrder++;
    }
};

struct ActiveRaceCar {
    struct RenderParams {
        u8  mu8RenderDamageFlags = 0;
        f32 mafCrackedGlassFractureAmount[8] = {};
        f32 mafCrackedGlassEqualisationFactor[8] = {};
        void SetRenderDamageFlag(u8 lu8Flags) { mu8RenderDamageFlags = lu8Flags; }
        void SetCrackedGlassFractureAmountN(u32 n, f32 lfValue) {
            CGS_ASSERT(n < 8, "( 0 <= n ) && ( 8 > n )");
            mafCrackedGlassFractureAmount[n] = lfValue;
        }
    };
    RenderParams mRenderParams;
    int  miResetAfterCrash = 0, miOrder = -1;
    bool mbKeepVerletOffsets = false;
    bool mbBitSetAtResetAfterCrash = false;
    u8   mu8FlagsAtResetAfterCrash = 0xEE;
    s32  miSlot = 0;
    RenderParams* GetRenderParams() { return &mRenderParams; }
    void ResetAfterCrash(bool lbKeepVerletOffsets);
};

struct RaceCarEntityModule {
    CgsContainers::BitArray<8u> mabResetThisFrame;
    void Tail(EActiveRaceCarIndex leActiveRaceCarIndex, ActiveRaceCar* lpActiveRaceCar,
              VehicleInputInterface* lpVehicleInputInterface, bool lbResetTransform,
              bool lbResetDeformation);
};

void ActiveRaceCar::ResetAfterCrash(bool lbKeepVerletOffsets) {
    ++miResetAfterCrash; mbKeepVerletOffsets = lbKeepVerletOffsets; miOrder = giOrder++;
    mbBitSetAtResetAfterCrash = gpModule->mabResetThisFrame.IsBitSet(static_cast<u32>(miSlot));
    mu8FlagsAtResetAfterCrash = mRenderParams.mu8RenderDamageFlags;
}

void RaceCarEntityModule::Tail(EActiveRaceCarIndex leActiveRaceCarIndex, ActiveRaceCar* lpActiveRaceCar,
                               VehicleInputInterface* lpVehicleInputInterface, bool lbResetTransform,
                               bool lbResetDeformation) {
    const Matrix44Affine lrTransform = {};
    const Vector3 lrVelocity = { 1.0f, 2.0f, 3.0f, 0.0f };
    const Vector3 lZeroAngularVelocity = { 0.0f, 0.0f, 0.0f, 0.0f };
    const s32 liResetModelIndex = 7;
    const f32 lfHowCloseToTotalled = 0.0f;
    const bool lbResettingAfterWreck = false;
    const BrnPhysics::Deformation::DeformationResetType leDeformationResetType =
        BrnPhysics::Deformation::E_RESET_NONE;
    (void)lrVelocity;
#include "rcem2_reset_tail.inc"
}

}   // namespace Fixture

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName) {
    ++giChecks;
    if (!lbPass) { ++giFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}

static void Dirty(Fixture::ActiveRaceCar& lrCar) {
    lrCar.mRenderParams.mu8RenderDamageFlags = 0x5A;
    for (int i = 0; i < 8; ++i) {
        lrCar.mRenderParams.mafCrackedGlassFractureAmount[i] = 0.125f * (i + 1);
        lrCar.mRenderParams.mafCrackedGlassEqualisationFactor[i] = 0.75f;
    }
}

int main() {
    using namespace Fixture;
    RaceCarEntityModule lModule; lModule.mabResetThisFrame.UnSetAll(); gpModule = &lModule;

    // ---- a wreck reset (resetDeformation = 1) on slot 5 -----------------------------------------
    {
        VehicleInputInterface lVehicle; ActiveRaceCar lCar; lCar.miSlot = 5; Dirty(lCar); giOrder = 0;
        lModule.Tail(static_cast<EActiveRaceCarIndex>(5), &lCar, &lVehicle, true, true);
        Check(lVehicle.miResets == 1 && lVehicle.muIndex == 5 && lVehicle.mbResetDeformation,
              "0x822F4B2C: one ResetRaceCar for the slot with the resetDeformation flag");
        Check(lModule.mabResetThisFrame.IsBitSet(5), "G67-D4 0x822F4BE8..0x822F4C10: mabResetThisFrame bit 5 set");
        bool lbOnlyFive = true;
        for (u32 i = 0; i < 8; ++i) if (i != 5 && lModule.mabResetThisFrame.IsBitSet(i)) lbOnlyFive = false;
        Check(lbOnlyFive, "G67-D4: only the reset slot's bit is set");
        Check(lCar.mbBitSetAtResetAfterCrash, "G67-D4: the bit is set BEFORE ResetAfterCrash (0x822F4C10 < 0x822F4C14)");
        Check(lCar.miResetAfterCrash == 1 && !lCar.mbKeepVerletOffsets,
              "0x822F4C14: ResetAfterCrash(resetDeformation == 0) -> keep = false");
        Check(lVehicle.miOrder < lCar.miOrder, "ResetRaceCar precedes ResetAfterCrash");
        Check(lCar.mRenderParams.mu8RenderDamageFlags == 0, "G67-D5 0x822F4C24: stb 0 -> mu8RenderDamageFlags (+0x1BF6)");
        bool lbGlassClear = true;
        for (int i = 0; i < 8; ++i) if (lCar.mRenderParams.mafCrackedGlassFractureAmount[i] != 0.0f) lbGlassClear = false;
        Check(lbGlassClear, "G67-D5 0x822F4C2C..0x822F4C3C: 8x stw 0 -> mafCrackedGlassFractureAmount[0..7]");
        bool lbEqualisationKept = true;
        for (int i = 0; i < 8; ++i) if (lCar.mRenderParams.mafCrackedGlassEqualisationFactor[i] != 0.75f) lbEqualisationKept = false;
        Check(lbEqualisationKept, "G67-D5: only 33 bytes are cleared (equalisation factors untouched)");
        Check(lCar.mu8FlagsAtResetAfterCrash == 0x5A, "G67-D5: the glass clear follows ResetAfterCrash (0x822F4C18 after the bl)");
    }

    // ---- a transform-only reset (resetDeformation = 0) on slot 2 --------------------------------
    {
        VehicleInputInterface lVehicle; ActiveRaceCar lCar; lCar.miSlot = 2; Dirty(lCar); giOrder = 0;
        lModule.Tail(static_cast<EActiveRaceCarIndex>(2), &lCar, &lVehicle, true, false);
        Check(lModule.mabResetThisFrame.IsBitSet(2) && lModule.mabResetThisFrame.IsBitSet(5),
              "G67-D4: the bit is set on every live-car reset (unconditional), and accumulates until the scene pass clears it");
        Check(lCar.miResetAfterCrash == 1 && lCar.mbKeepVerletOffsets,
              "0x822F4C14: resetDeformation 0 -> ResetAfterCrash(keep = true)");
        Check(lCar.mRenderParams.mu8RenderDamageFlags == 0x5A,
              "G67-D5 0x822F4C18 `cmplwi r30,0 ; beq`: no deformation reset -> damage flags kept");
        Check(lCar.mRenderParams.mafCrackedGlassFractureAmount[7] == 1.0f,
              "G67-D5: no deformation reset -> fractures kept");
    }

    Check(guAssertions == 0, "valid slots fire no assertions");
    std::printf("Rcem2ResetTail: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
