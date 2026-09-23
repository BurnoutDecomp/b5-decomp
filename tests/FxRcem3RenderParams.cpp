// FX-RCEM3 (crash-parity 2026-09-23): the production ActiveRaceCar::RenderParams::SetWheelScale
// (ARTIST 0x822CD170) and RenderParams::Reset (ARTIST 0x822E6818), extracted VERBATIM from
// BrnActiveRaceCarRenderParams.cpp by run_fxrcem3_render_params.py (together with the file's
// PIN_RP_OFFSETS layout pin) and run against the REAL RenderParams layout (BrnActiveRaceCar.h).
//
//   G62-D1  SetWheelScale builds diag(sx, sy, sz) from a stack template whose ONLY 1.0f slots are
//           sp+0x50/+0x64/+0x78 (stfs f13 @0x822CD1F0/1F8/1FC); the fourth diagonal slot sp+0x8C
//           is an integer zero (li r10,0 @0x822CD218 / stw @0x822CD258). Row 3 is never vperm'd,
//           so wAxis == (0,0,0,0); lrScale.w is never read.
//   G62-D2  Reset's complete store list (0x822E6818..0x822E6AD0) has NO store to +0x40..+0x83F
//           (maVerletOffsets): the 128 verlet rows must come out of Reset untouched. Every
//           console-stored field is checked against the asm; +0xD9C (mfDeformationSquared) is a
//           FLAGGED PC-only store kept while the three PC skips of UpdateDeformationState's store
//           remain (see the Reset banner) -- the check flips when those guards retire.
#include "types.hpp"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnActiveRaceCar.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <cstddef>
#include <cstdio>
#include <cstring>

static unsigned guAssertions = 0;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int) { ++guAssertions; std::fprintf(stderr, "ASSERT: %s\n", lpcMessage); return 0; }
void* EndAssert() { return nullptr; }
}
}

#include "fxrcem3_render_params.inc"

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName)
{
    ++giChecks;
    if (!lbPass) { ++giFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}

static u32 Bits(f32 lf) { u32 lu; std::memcpy(&lu, &lf, 4); return lu; }

// True when every byte of [lp, lp + luSize) still holds luByte.
static bool Holds(const void* lp, size_t luSize, u8 luByte)
{
    const u8* lpBytes = static_cast<const u8*>(lp);
    for (size_t i = 0; i < luSize; ++i) if (lpBytes[i] != luByte) return false;
    return true;
}

// Bitwise compare of a Matrix44Affine against 16 console lane values.
static bool MatrixIs(const Matrix44Affine& lrM, const f32 (&lafExpect)[16])
{
    const u8* lpBytes = reinterpret_cast<const u8*>(&lrM);
    for (int i = 0; i < 16; ++i)
    {
        u32 lu; std::memcpy(&lu, lpBytes + 4 * i, 4);
        if (lu != Bits(lafExpect[i])) return false;
    }
    return true;
}

// Every lane of a Matrix44Affine holds luBits.
static bool AllLanes(const Matrix44Affine& lrM, u32 luBits)
{
    const u8* lpBytes = reinterpret_cast<const u8*>(&lrM);
    for (int i = 0; i < 16; ++i)
    {
        u32 lu; std::memcpy(&lu, lpBytes + 4 * i, 4);
        if (lu != luBits) return false;
    }
    return true;
}

using BrnWorld::ActiveRaceCar;
typedef ActiveRaceCar::RenderParams RenderParams;

alignas(16) static unsigned char gaStorage[sizeof(RenderParams)];

int main()
{
    RenderParams* lp = reinterpret_cast<RenderParams*>(gaStorage);
    const f32 kIdentityZeroW[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,0 };

    // ---- G62-D1: SetWheelScale -------------------------------------------------------------
    {
        for (int w = 0; w < 6; ++w)
        {
            u32* lpLanes = reinterpret_cast<u32*>(&lp->mWheelScaleTransforms[w]);
            for (int i = 0; i < 16; ++i) lpLanes[i] = 0x7FFFFFFFu;
        }
        Vector3 lScale; lScale.x = 2.0f; lScale.y = 3.0f; lScale.z = 4.0f; lScale.w = 9.0f;
        lp->SetWheelScale(2u, lScale);

        const f32 lafExpect[16] = { 2,0,0,0, 0,3,0,0, 0,0,4,0, 0,0,0,0 };
        static const char* const kapcLane[16] = {
            "G62-D1 [2].xAxis.x == scale.x (vperm entry +0x00 @0x822CD29C)", "G62-D1 [2].xAxis.y == 0", "G62-D1 [2].xAxis.z == 0",
            "G62-D1 [2].xAxis.w == 0 (li r10,0 -> sp+0x5C)",
            "G62-D1 [2].yAxis.x == 0", "G62-D1 [2].yAxis.y == scale.y (vperm entry +0x40)", "G62-D1 [2].yAxis.z == 0",
            "G62-D1 [2].yAxis.w == 0 (sp+0x6C)",
            "G62-D1 [2].zAxis.x == 0", "G62-D1 [2].zAxis.y == 0", "G62-D1 [2].zAxis.z == scale.z (vperm entry +0x80)",
            "G62-D1 [2].zAxis.w == 0 (sp+0x7C)",
            "G62-D1 [2].wAxis.x == 0", "G62-D1 [2].wAxis.y == 0", "G62-D1 [2].wAxis.z == 0",
            "G62-D1 [2].wAxis.w == 0x00000000 (sp+0x8C: li r10,0 @0x822CD218 / stw @0x822CD258), not 1.0f; scale.w (9) not propagated" };
        const u32* lpLanes = reinterpret_cast<const u32*>(&lp->mWheelScaleTransforms[2]);
        for (int i = 0; i < 16; ++i) Check(lpLanes[i] == Bits(lafExpect[i]), kapcLane[i]);
        Check(AllLanes(lp->mWheelScaleTransforms[1], 0x7FFFFFFFu), "G62-D1 wheel [1] untouched");
        Check(AllLanes(lp->mWheelScaleTransforms[3], 0x7FFFFFFFu), "G62-D1 wheel [3] untouched");
    }

    // ---- G62-D2: Reset ---------------------------------------------------------------------
    {
        const u8 kuSentinel = 0xA5;
        std::memset(gaStorage, kuSentinel, sizeof(gaStorage));
        lp->Reset();

        // No console store in +0x40..+0x83F: all 128 verlet rows come out untouched.
        for (u32 luRow = 0; luRow < BrnWorld::KU_MAX_RACE_CAR_VERLET_POINTS; ++luRow)
        {
            char lacName[160];
            std::snprintf(lacName, sizeof(lacName),
                          "G62-D2 maVerletOffsets[%u] (+0x%X) untouched -- the console Reset makes no store in +0x40..+0x83F",
                          luRow, 0x40u + 16u * luRow);
            Check(Holds(&lp->maVerletOffsets[luRow], sizeof(lp->maVerletOffsets[luRow]), kuSentinel), lacName);
        }

        // The console's stores.
        Check(MatrixIs(lp->mBodyTransform, kIdentityZeroW),
              "G62-D2 mBodyTransform (+0x00..+0x3F) = identity with a zero wAxis (stvx128 @0x822E6A40/44/48/50)");
        for (int w = 0; w < 6; ++w)
        {
            Check(MatrixIs(lp->mWheelTransforms[w], kIdentityZeroW),
                  "G62-D2 mWheelTransforms[i] (+0x840 + 64i) = identity, zero wAxis (loop @0x822E6940..60)");
            Check(MatrixIs(lp->mWheelScaleTransforms[w], kIdentityZeroW),
                  "G62-D2 mWheelScaleTransforms[i] (+0x9C0 + 64i) = identity, zero wAxis (loop @0x822E6928..3C)");
        }
        Check(Bits(lp->mPaintColour.x) == Bits(1.0f) && Bits(lp->mPaintColour.y) == Bits(1.0f)
                  && Bits(lp->mPaintColour.z) == Bits(1.0f) && Bits(lp->mPaintColour.w) == Bits(1.0f),
              "G62-D2 mPaintColour (+0xB80) = (1,1,1,1) (stvx128 @0x822E6A24)");
        Check(Bits(lp->mPearlescentColour.x) == Bits(1.0f) && Bits(lp->mPearlescentColour.y) == Bits(1.0f)
                  && Bits(lp->mPearlescentColour.z) == Bits(1.0f) && Bits(lp->mPearlescentColour.w) == Bits(1.0f),
              "G62-D2 mPearlescentColour (+0xB90) = (1,1,1,1) (stvx128 @0x822E6A3C)");
        Check(Holds(lp->mabWheelExists, sizeof(lp->mabWheelExists), 0),
              "G62-D2 mabWheelExists[6] (+0xD80..+0xD85) = 0 (stbx @0x822E6964)");
        Check(Holds(&lp->mBodyPartVisibility, sizeof(lp->mBodyPartVisibility), 0xFF),
              "G62-D2 mBodyPartVisibility (+0xDA0/+0xDA8) = ~0 (std r7=-1 @0x822E6A54/58)");
        Check(lp->maDetachedParts.GetLength() == 0 && lp->maDetachedParts.GetMaxLength() == 20
                  && lp->maDetachedParts.GetQueueStartPointer() == lp->maDetachedParts.maEvents,
              "G62-D2 maDetachedParts (+0xDB0) = {this+0xDC0, 20, 0} (0x822E6A98..AC)");
        Check(static_cast<u32>(lp->mLOD) == 4u, "G62-D2 mLOD (+0x1400) = 4 (stw r8=4 @0x822E6AB8)");
        Check(lp->mbCrashing == false && lp->mbIsHidden == false && lp->mbBluesAndTwosActive == false
                  && lp->mu8RenderDamageFlags == 0,
              "G62-D2 +0x1405/+0x140B/+0x1415/+0x1416 = 0 (stb r31 @0x822E6A60/64/6C/AB4)");
        Check(lp->mbBluesAndTwosCanSwitchState == true, "G62-D2 +0x1414 = 1 (stb r26=1 @0x822E6A68)");
        Check(Bits(lp->mfLightOpacityFlipFlop) == 0u, "G62-D2 +0x140C = 0.0f (stfs f0 @0x822E6A5C)");
        {
            bool lbZero = true;
            for (int p = 0; p < 8; ++p) if (Bits(lp->mafCrackedGlassFractureAmount[p]) != 0u) lbZero = false;
            Check(lbZero, "G62-D2 +0x1418..+0x1437 = 0 (the 8-word stw loop @0x822E6AC0)");
        }
        // [FLAG PC] +0xD9C: NOT a console store. Kept (0.0f) while the three PC skips of
        // UpdateDeformationState's store remain; when they retire, the Reset store goes and this
        // expectation flips to "untouched" (Holds(..., kuSentinel)).
        Check(Bits(lp->mfDeformationSquared) == 0u,
              "G62-D2 [FLAG PC] mfDeformationSquared (+0xD9C) = 0.0f while the PC readback guards remain");

        // Fields the console Reset does not store keep the sentinel.
        Check(Holds(lp->maAxlePositions, sizeof(lp->maAxlePositions), kuSentinel), "G62-D2 maAxlePositions untouched");
        Check(Holds(lp->maLightLocatorPos, sizeof(lp->maLightLocatorPos), kuSentinel)
                  && Holds(lp->maLightLocatorType, sizeof(lp->maLightLocatorType), kuSentinel)
                  && Holds(&lp->miNumLightLocators, sizeof(lp->miNumLightLocators), kuSentinel),
              "G62-D2 light locators untouched");
        Check(Holds(lp->mafWheelAngularVelocities, sizeof(lp->mafWheelAngularVelocities), kuSentinel),
              "G62-D2 mafWheelAngularVelocities untouched");
        Check(Holds(&lp->mbDamaged, 1, kuSentinel) && Holds(&lp->mbIsEngineOff, 1, kuSentinel)
                  && Holds(&lp->mbIsBraking, 1, kuSentinel) && Holds(&lp->mbIsReversing, 1, kuSentinel)
                  && Holds(&lp->mbIsIndicatingLeft, 1, kuSentinel) && Holds(&lp->mbIsIndicatingRight, 1, kuSentinel),
              "G62-D2 +0x1404/+0x1406..+0x140A untouched");
        Check(Holds(&lp->mfLightSwitchTimeOut, sizeof(f32), kuSentinel), "G62-D2 +0x1410 untouched");
        Check(Holds(lp->mafCrackedGlassEqualisationFactor, sizeof(lp->mafCrackedGlassEqualisationFactor), kuSentinel)
                  && Holds(lp->mavCrackedGlassScaleFactors, sizeof(lp->mavCrackedGlassScaleFactors), kuSentinel),
              "G62-D2 glass equalisation/scale untouched");
    }

    Check(guAssertions == 0, "valid fixtures fire no assertions");
    std::printf("FxRcem3RenderParams: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
