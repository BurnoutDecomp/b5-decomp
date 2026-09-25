// FX-GLASSNAN (crash parity wave 5, 2026-09-26): THE GLASS SHATTER EFFECT SPAWNED AT NaN.
//
// BrnEffects::BrnEffectsGlassManager::FireGlassEffect @0x82295D50 caches every 'Glass_shattering' LION effect in
// its car's frame, mLocalTransform = lEffectTransform * Inverse(lVehicleTransform), and ::UpdateVehicleEffectPositions
// @0x8228D208 re-seats the effect every frame as mLocalTransform * the car's transform. The console inlines the AFFINE
// rw::math::vpu::Inverse and operator* (x, y, z lanes only). The PC body called the GENERAL 4x4 Inverse @0x825B2628
// instead (the console calls that one only from the shadow map and three debug components); a Matrix44Affine's w
// column is (0,0,0,0) -- the console's own SetIdentity (Reset @0x8228F298) and every PC producer write it so -- so the
// determinant was 0, every entry 0 * inf = NaN, and every glass shatter spawned its particles at NaN.
//
// run_fxglassnan.py extracts the PRODUCTION file (BrnEffectsGlassManager.cpp, every body: Reset, the inverse and
// product helpers, FireGlassEffect, UpdateVehicleEffectPositions) and, for the producer chain, the PC's own
// SimpleVehiclePhysics::GetGraphicsVehicleTransform @0x825BF158 (with its two file-static helpers), and compiles them
// onto this fixture: stand-ins for the particle module (StartLionEffect / GetLionEffect / StopLionEffect, the LION
// slot's SetTransform as ParticleModule.h writes it), the input buffer, the traffic queue and the race-car interface.
// The vendor Matrix44Operation.cpp is linked, so the pre-fix body runs its real 4x4 inverse.
//
// THE EXPECTED VALUES ARE THE CONSOLE'S OWN OUTPUTS (FxGlassNanData.h): FireGlassEffect and UpdateVehicleEffectPositions
// run whole on emu64 over the raw ARTIST words by FX-GLASSNAN's gen_glassnan_data.py. Compared bit for bit:
//   A. 26 FireGlassEffect cases x 4 checks: the fired slot's mLocalTransform (all 16 lanes); every slot's scalar fields
//      and the untouched slots' matrices; the LION effect's transform and flags; the cursor, starts, stops, asserts.
//      The car poses: the evidence run's traffic car 336 and the player's race car behind it, ten random cars, a car on
//      its roof, one on its side, an axis-aligned car at the origin -- in the PC producer's form (w column 0), six in the
//      textbook form (0,0,0,1), three with garbage w lanes on both matrices (NaN / inf / 1e30 in one), a pane facing
//      straight up (the console's own NaN effect frame), and a stale handle (only Reset runs).
//   B. 3 UpdateVehicleEffectPositions cases x 4 checks: every playing effect's transform and flags; the slots after;
//      the stops; the asserts. Six live slots each (traffic and race-car owners, one of them expiring in case 2).
//   C. The producer chain, end to end: the PC's GetGraphicsVehicleTransform hands a body built the PC's way a w column
//      of (0,0,0,0); FireGlassEffect on that transform leaves a FINITE mLocalTransform; the re-seat on the same car puts
//      the effect back on the pane (within 1 mm, 1e-4 per basis lane).
// A NaN the console makes from ordinary operands matches any NaN word here (the PowerPC's default NaN is 0x7FC00000,
// x86's 0xFFC00000); every other word is compared exactly.
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/BurnoutConstants.h"
#include "rw/math/vpu/vector3_operation.h"
#include "rw/math/vpu/matrix44_operation.h"
#include "rw/math/vpu/matrix44affine_operation.h"
#include "GameSource/Effects/BrnEffectsGlassManager.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "FxGlassNanData.h"

static unsigned gChecks = 0, gFailures = 0, gAsserts = 0, gHashes = 0, gLogLines = 0;

static void Check(bool lbPassed, const char* lpcLabel)
{
    ++gChecks;
    if (!lbPassed)
    {
        ++gFailures;
        std::printf("FAIL  %s\n", lpcLabel);
    }
    else
    {
        std::printf("pass  %s\n", lpcLabel);
    }
}

static u32 Bits(f32 lf) { u32 lu; std::memcpy(&lu, &lf, 4); return lu; }
static f32 Float(u32 lu) { f32 lf; std::memcpy(&lf, &lu, 4); return lf; }

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { ++gAsserts; return 0; }
    void* EndAssert() { return nullptr; }
}
namespace Log
{
    // The [glassfx] witness's sink (default off: BRN_GLASS_DIAG is not set here, so it stays silent).
    void WriteToLog(const char*) { ++gLogLines; }
}
}

// ---- the particle module ----------------------------------------------------------------------------------------
namespace BrnParticle
{
    struct ParticleDescription
    {
        static u32 HashString(const char*) { ++gHashes; return 0xD17D2FA3u; }
    };

    // The playing-effect slot as ParticleModule.h declares it, reduced to what the glass manager touches: the handle
    // it resolves against, the transform SetTransform writes and the flags it marks CHANGED (ParticleModule.h's
    // inline, X360 0x82295E24..0x82295E6C: four row stores at +0x10, muFlags |= 4).
    struct LionEffect
    {
        static const u32 KU_HANDLE_INVALID = 0xFFFFFFFFu;
        static const u16 EPPE_FLAG_CHANGED = 4;
        u32            muHandle;
        Matrix44Affine mTransform;
        u16            muFlags;

        void SetTransform(const Matrix44Affine& lrTransform)
        {
            mTransform = lrTransform;
            muFlags   |= EPPE_FLAG_CHANGED;
        }
    };

    struct ParticleModule
    {
        static const u32 KU_MAX_PLAYING_EFFECTS = 128;
        LionEffect       maPlayingEffects[KU_MAX_PLAYING_EFFECTS];
        u32              muStartHandle;
        u32              muStarts;
        std::vector<u32> mStops;

        // X360 0x82278380, as ParticleModule.cpp writes it: the low 7 bits index the slot; the slot is returned only
        // while it still holds the handle.
        LionEffect* GetLionEffect(u32 luHandle)
        {
            const u32 luArrayIndex = luHandle & 0x7Fu;
            CGS_ASSERT(luArrayIndex < KU_MAX_PLAYING_EFFECTS, "luArrayIndex < KU_MAX_PLAYING_EFFECTS");
            LionEffect* lpEffect = &maPlayingEffects[luArrayIndex];
            return (lpEffect->muHandle == luHandle) ? lpEffect : nullptr;
        }

        u32 StartLionEffect(u32, const char*, u32)
        {
            ++muStarts;
            return muStartHandle;
        }

        void StopLionEffect(LionEffect* lpEffect)
        {
            mStops.push_back(static_cast<u32>(lpEffect - maPlayingEffects));
        }
    };
}

// ---- the physics / race-car outputs the re-seat reads ------------------------------------------------------------
namespace BrnPhysics
{
namespace Vehicle
{
    struct PhysicalTrafficState
    {
        Matrix44Affine mTransform;   // @448 on the console (0x8228D35C..0x8228D374)
        EntityId       mEntityID;    // @800 (0x8228D350)
    };

    struct VehicleOutputInterface
    {
        struct PhysicalTrafficStateQueue
        {
            std::vector<PhysicalTrafficState> mEvents;
            s32 GetLength() const { return static_cast<s32>(mEvents.size()); }
            const PhysicalTrafficState& GetEvent(s32 liIndex) const { return mEvents[static_cast<size_t>(liIndex)]; }
        };
    };

    // ---- the producer: SimpleVehiclePhysics::GetGraphicsVehicleTransform, the PC's own body ----------------------
    namespace vpu = rw::math::vpu;

    struct SimpleAttribsFixture
    {
        Vector3 mCOMOffset;
        bool IsValid() const { return true; }
    };

    class SimpleVehiclePhysics
    {
    public:
        Matrix44Affine       mTransform;
        SimpleAttribsFixture mSimpleAttribs;
        const SimpleAttribsFixture* GetSimpleAttribs() const { return &mSimpleAttribs; }
        Matrix44Affine GetGraphicsVehicleTransform() const;
    };

#include "fxglassnan_producer.inc"
}
}

namespace BrnWorld
{
namespace RaceCarEntityModuleIO
{
    struct RCEntityActiveRaceCarOutputInterface
    {
        struct RaceCarState
        {
            Matrix44Affine mTransform;   // @496 (+0x1F0, 0x8228D560)
            EntityId       mEntityId;    // @968 (+0x3C8, 0x8228D558)
        };
        bool         mabActive[8];
        RaceCarState maStates[8];

        bool IsRaceCarActive(EActiveRaceCarIndex leIndex) const { return mabActive[leIndex]; }
        const RaceCarState* GetRaceCarState(EActiveRaceCarIndex leIndex) const { return &maStates[leIndex]; }
    };
}
}

namespace BrnEffects
{
namespace EffectsIO
{
    struct InputBuffer
    {
        const BrnPhysics::Vehicle::VehicleOutputInterface::PhysicalTrafficStateQueue*      mpTrafficQueue;
        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*       mpRaceCars;
        const BrnPhysics::Vehicle::VehicleOutputInterface::PhysicalTrafficStateQueue* GetVehiclePhysicalStateQueue() const
        {
            return mpTrafficQueue;
        }
        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* GetActiveRaceCarInterface() const
        {
            return mpRaceCars;
        }
    };
}
}

// ---- the production file --------------------------------------------------------------------------------------------
#include "fxglassnan_body.inc"

// ---- helpers --------------------------------------------------------------------------------------------------------
static bool SameWord(u32 luGot, u32 luWant)
{
    const f32 lfGot = Float(luGot);
    const f32 lfWant = Float(luWant);
    if (lfWant != lfWant)
        return lfGot != lfGot;          // a console NaN matches any NaN (PPC 0x7FC00000 vs x86 0xFFC00000)
    return luGot == luWant;
}

static Matrix44Affine MatrixFromWords(const u32* lpu)
{
    Matrix44Affine lMatrix;
    Vector3* const lapRows[4] = { &lMatrix.xAxis, &lMatrix.yAxis, &lMatrix.zAxis, &lMatrix.wAxis };
    for (u32 lu = 0; lu < 4u; ++lu)
    {
        lapRows[lu]->x = Float(lpu[4 * lu + 0]);
        lapRows[lu]->y = Float(lpu[4 * lu + 1]);
        lapRows[lu]->z = Float(lpu[4 * lu + 2]);
        lapRows[lu]->w = Float(lpu[4 * lu + 3]);
    }
    return lMatrix;
}

static void MatrixWords(const Matrix44Affine& lrMatrix, u32* lpu)
{
    const Vector3* const lapRows[4] = { &lrMatrix.xAxis, &lrMatrix.yAxis, &lrMatrix.zAxis, &lrMatrix.wAxis };
    for (u32 lu = 0; lu < 4u; ++lu)
    {
        lpu[4 * lu + 0] = Bits(lapRows[lu]->x);
        lpu[4 * lu + 1] = Bits(lapRows[lu]->y);
        lpu[4 * lu + 2] = Bits(lapRows[lu]->z);
        lpu[4 * lu + 3] = Bits(lapRows[lu]->w);
    }
}

static bool SameMatrix(const Matrix44Affine& lrGot, const u32* lpuWant, const char* lpcWhat, const char* lpcCase)
{
    u32 lau[16];
    MatrixWords(lrGot, lau);
    bool lbSame = true;
    for (u32 lu = 0; lu < 16u; ++lu)
    {
        if (!SameWord(lau[lu], lpuWant[lu]))
        {
            if (lbSame)
                std::printf("      %s [%s]: lane %u got %08X (%g) want %08X (%g)\n", lpcWhat, lpcCase, lu, lau[lu],
                            static_cast<double>(Float(lau[lu])), lpuWant[lu], static_cast<double>(Float(lpuWant[lu])));
            lbSame = false;
        }
    }
    return lbSame;
}

static bool MatrixFinite(const Matrix44Affine& lrMatrix)
{
    u32 lau[16];
    MatrixWords(lrMatrix, lau);
    for (u32 lu = 0; lu < 16u; ++lu)
    {
        if (!std::isfinite(Float(lau[lu])))
            return false;
    }
    return true;
}

static void LoadSlots(BrnEffects::BrnEffectsGlassManager& lrManager, const GlassSlotWords* lpSlots)
{
    for (u32 lu = 0; lu < BrnEffects::KU_MAX_GLASS_EFFECTS; ++lu)
    {
        BrnEffects::BrnGlassSmashEffect& lrSlot = lrManager.maGlassEffects[lu];
        lrSlot.mLocalTransform    = MatrixFromWords(lpSlots[lu].mauLocal);
        lrSlot.mVehicleID.muValue = lpSlots[lu].muId;
        lrSlot.mGlassEffectHandle = lpSlots[lu].muHandle;
        lrSlot.mfEffectEndTime    = Float(lpSlots[lu].muEnd);
        lrSlot.mbEffectActive     = lpSlots[lu].mu8Active != 0;
    }
}

static bool SameSlotScalars(const BrnEffects::BrnGlassSmashEffect& lrSlot, const GlassSlotWords& lrWant)
{
    return lrSlot.mVehicleID.muValue == lrWant.muId && lrSlot.mGlassEffectHandle == lrWant.muHandle
        && SameWord(Bits(lrSlot.mfEffectEndTime), lrWant.muEnd) && (lrSlot.mbEffectActive ? 1u : 0u) == lrWant.mu8Active;
}

static BrnParticle::ParticleModule* NewParticleModule()
{
    BrnParticle::ParticleModule* lpModule = new BrnParticle::ParticleModule();
    for (u32 lu = 0; lu < BrnParticle::ParticleModule::KU_MAX_PLAYING_EFFECTS; ++lu)
    {
        lpModule->maPlayingEffects[lu].muHandle = 0;
        lpModule->maPlayingEffects[lu].mTransform.SetZero();
        lpModule->maPlayingEffects[lu].muFlags = 0;
    }
    lpModule->muStartHandle = 0;
    lpModule->muStarts = 0;
    return lpModule;
}

// ---- A: FireGlassEffect ---------------------------------------------------------------------------------------------
static void RunFireCase(const GlassFireCase& lrCase)
{
    BrnParticle::ParticleModule* lpModule = NewParticleModule();
    for (u32 lu = 0; lu < lrCase.muNumPlaying; ++lu)
        lpModule->maPlayingEffects[lrCase.maPlaying[lu].muIndex].muHandle = lrCase.maPlaying[lu].muHandle;
    lpModule->muStartHandle = lrCase.muHandle;

    BrnEffects::BrnEffectsGlassManager lManager;
    lManager.mpParticleModule  = lpModule;
    lManager.muNextGlassEffect = lrCase.muCursor;
    LoadSlots(lManager, lrCase.maSlotsIn);

    const Matrix44Affine lEffect  = MatrixFromWords(lrCase.mauEffect);
    const Matrix44Affine lVehicle = MatrixFromWords(lrCase.mauVehicle);
    EntityId lId;
    lId.muValue = lrCase.muId;
    gAsserts = 0;
    gHashes = 0;
    lManager.FireGlassEffect(lEffect, lVehicle, lId, Float(lrCase.muTime));

    char lacLabel[320];
    const BrnEffects::BrnGlassSmashEffect& lrFired = lManager.maGlassEffects[lrCase.muCursor];
    std::snprintf(lacLabel, sizeof(lacLabel), "fire [%s]: the slot's mLocalTransform, all 16 lanes", lrCase.mpcName);
    Check(SameMatrix(lrFired.mLocalTransform, lrCase.maSlotsOut[lrCase.muCursor].mauLocal, "mLocalTransform",
                     lrCase.mpcName), lacLabel);

    bool lbSlots = true;
    for (u32 lu = 0; lu < BrnEffects::KU_MAX_GLASS_EFFECTS; ++lu)
    {
        lbSlots = lbSlots && SameSlotScalars(lManager.maGlassEffects[lu], lrCase.maSlotsOut[lu]);
        if (lu != lrCase.muCursor)
            lbSlots = lbSlots && SameMatrix(lManager.maGlassEffects[lu].mLocalTransform, lrCase.maSlotsOut[lu].mauLocal,
                                            "untouched slot", lrCase.mpcName);
    }
    std::snprintf(lacLabel, sizeof(lacLabel), "fire [%s]: every slot's id / handle / end time / active, the other "
                  "slots untouched", lrCase.mpcName);
    Check(lbSlots, lacLabel);

    bool lbLions = true;
    for (u32 lu = 0; lu < lrCase.muNumPlaying; ++lu)
    {
        const GlassLionWords& lrWant = lrCase.maLionsOut[lu];
        const BrnParticle::LionEffect& lrGot = lpModule->maPlayingEffects[lrWant.muIndex];
        lbLions = lbLions && SameMatrix(lrGot.mTransform, lrWant.mauTransform, "LION transform", lrCase.mpcName)
               && lrGot.muFlags == lrWant.mu16Flags;
    }
    std::snprintf(lacLabel, sizeof(lacLabel), "fire [%s]: the LION effect's transform and flags", lrCase.mpcName);
    Check(lbLions, lacLabel);

    bool lbRest = lManager.muNextGlassEffect == lrCase.muCursorOut && lpModule->muStarts == lrCase.muNumStarts
               && gHashes == lrCase.muNumStarts && lpModule->mStops.size() == lrCase.muNumStops
               && gAsserts == lrCase.muAsserts;
    for (u32 lu = 0; lbRest && lu < lrCase.muNumStops; ++lu)
        lbRest = lpModule->mStops[lu] == lrCase.maStops[lu].muIndex;
    std::snprintf(lacLabel, sizeof(lacLabel), "fire [%s]: the cursor, the start, the stops, the asserts", lrCase.mpcName);
    Check(lbRest, lacLabel);
    delete lpModule;
}

// ---- B: UpdateVehicleEffectPositions --------------------------------------------------------------------------------
static void RunUpdateCase(const GlassUpdateCase& lrCase)
{
    BrnParticle::ParticleModule* lpModule = NewParticleModule();
    for (u32 lu = 0; lu < lrCase.muNumPlaying; ++lu)
        lpModule->maPlayingEffects[lrCase.maPlaying[lu].muIndex].muHandle = lrCase.maPlaying[lu].muHandle;

    BrnEffects::BrnEffectsGlassManager lManager;
    lManager.mpParticleModule  = lpModule;
    lManager.muNextGlassEffect = 0;
    LoadSlots(lManager, lrCase.maSlotsIn);

    BrnPhysics::Vehicle::VehicleOutputInterface::PhysicalTrafficStateQueue lQueue;
    for (u32 lu = 0; lu < lrCase.muNumTraffic; ++lu)
    {
        BrnPhysics::Vehicle::PhysicalTrafficState lState;
        lState.mTransform = MatrixFromWords(lrCase.maTraffic[lu].mauTransform);
        lState.mEntityID.muValue = lrCase.maTraffic[lu].muId;
        lQueue.mEvents.push_back(lState);
    }
    BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpRaceCars =
        new BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface();
    for (u32 lu = 0; lu < 8u; ++lu)
    {
        lpRaceCars->mabActive[lu] = lrCase.maRaceCars[lu].mu8Active != 0;
        lpRaceCars->maStates[lu].mTransform = MatrixFromWords(lrCase.maRaceCars[lu].mauTransform);
        lpRaceCars->maStates[lu].mEntityId.muValue = lrCase.maRaceCars[lu].muId;
    }
    BrnEffects::EffectsIO::InputBuffer lInput;
    lInput.mpTrafficQueue = &lQueue;
    lInput.mpRaceCars = lpRaceCars;
    gAsserts = 0;
    lManager.UpdateVehicleEffectPositions(&lInput, Float(lrCase.muTime));

    char lacLabel[320];
    bool lbLions = true;
    bool lbFlags = true;
    for (u32 lu = 0; lu < lrCase.muNumPlaying; ++lu)
    {
        const GlassLionWords& lrWant = lrCase.maLionsOut[lu];
        const BrnParticle::LionEffect& lrGot = lpModule->maPlayingEffects[lrWant.muIndex];
        lbLions = SameMatrix(lrGot.mTransform, lrWant.mauTransform, "re-seated LION transform", lrCase.mpcName) && lbLions;
        lbFlags = lbFlags && lrGot.muFlags == lrWant.mu16Flags;
    }
    std::snprintf(lacLabel, sizeof(lacLabel), "update [%s]: every re-seated LION transform, all 16 lanes", lrCase.mpcName);
    Check(lbLions, lacLabel);
    std::snprintf(lacLabel, sizeof(lacLabel), "update [%s]: the LION flags (CHANGED where re-seated)", lrCase.mpcName);
    Check(lbFlags, lacLabel);

    bool lbSlots = true;
    for (u32 lu = 0; lu < BrnEffects::KU_MAX_GLASS_EFFECTS; ++lu)
        lbSlots = lbSlots && SameSlotScalars(lManager.maGlassEffects[lu], lrCase.maSlotsOut[lu])
               && SameMatrix(lManager.maGlassEffects[lu].mLocalTransform, lrCase.maSlotsOut[lu].mauLocal, "slot",
                             lrCase.mpcName);
    std::snprintf(lacLabel, sizeof(lacLabel), "update [%s]: the slots after (the expiry clears active and the handle)",
                  lrCase.mpcName);
    Check(lbSlots, lacLabel);

    bool lbRest = lpModule->mStops.size() == lrCase.muNumStops && gAsserts == lrCase.muAsserts;
    for (u32 lu = 0; lbRest && lu < lrCase.muNumStops; ++lu)
        lbRest = lpModule->mStops[lu] == lrCase.maStops[lu].muIndex;
    std::snprintf(lacLabel, sizeof(lacLabel), "update [%s]: the stops and the asserts", lrCase.mpcName);
    Check(lbRest, lacLabel);
    delete lpRaceCars;
    delete lpModule;
}

// ---- C: the producer chain, end to end ------------------------------------------------------------------------------
static void RunProducerChain()
{
    namespace vpu = rw::math::vpu;

    // The evidence run's traffic car 336: a body transform built the PC's way -- SetIdentity (w column 0), the basis
    // turned by the vendor rotation builders through the affine product, the position added as a Vector3 -- and a
    // centre-of-mass offset. Every one of those producers writes w = 0.
    BrnPhysics::Vehicle::SimpleVehiclePhysics lBody;
    lBody.mTransform.SetIdentity();
    lBody.mTransform = vpu::Mult(vpu::Mult(vpu::MakeRotationY(0.0207f), vpu::MakeRotationX(0.0213f)), lBody.mTransform);
    Vector3 lvPosition;
    lvPosition.x = 3032.100830f; lvPosition.y = -3.419249f; lvPosition.z = -1965.530518f; lvPosition.w = 0.0f;
    lBody.mTransform.wAxis = vpu::Add(lBody.mTransform.wAxis, lvPosition);
    lBody.mSimpleAttribs.mCOMOffset.x = 0.0f;
    lBody.mSimpleAttribs.mCOMOffset.y = 0.12f;
    lBody.mSimpleAttribs.mCOMOffset.z = -0.08f;
    lBody.mSimpleAttribs.mCOMOffset.w = 0.0f;
    const Matrix44Affine lVehicle = lBody.GetGraphicsVehicleTransform();

    Check(lVehicle.xAxis.w == 0.0f && lVehicle.yAxis.w == 0.0f && lVehicle.zAxis.w == 0.0f && lVehicle.wAxis.w == 0.0f,
          "producer: the PC's GetGraphicsVehicleTransform hands a body built the PC's way a w column of (0,0,0,0) -- "
          "the matrix FireGlassEffect is given");

    // The windscreen shatter on that car: z = the pane normal, x / y in its plane, w = the pane point, in world.
    Vector3 lvLocalNormal;
    lvLocalNormal.x = 0.0f; lvLocalNormal.y = 0.5524f; lvLocalNormal.z = 0.8336f; lvLocalNormal.w = 0.0f;
    Vector3 lvLocalPoint;
    lvLocalPoint.x = 0.0f; lvLocalPoint.y = 0.62f; lvLocalPoint.z = 1.05f; lvLocalPoint.w = 0.0f;
    Matrix44Affine lEffect;
    lEffect.zAxis = vpu::TransformVector(lVehicle, lvLocalNormal);
    lEffect.xAxis = vpu::Normalize(vpu::Cross(lVehicle.yAxis, lEffect.zAxis));
    lEffect.yAxis = vpu::Cross(lEffect.zAxis, lEffect.xAxis);
    lEffect.wAxis = vpu::TransformPoint(lVehicle, lvLocalPoint);

    BrnParticle::ParticleModule* lpModule = NewParticleModule();
    const u32 luHandle = 0x80u | 17u | (9u << 7);
    lpModule->maPlayingEffects[17].muHandle = luHandle;
    lpModule->muStartHandle = luHandle;
    BrnEffects::BrnEffectsGlassManager lManager;
    lManager.Construct(lpModule);
    EntityId lId;
    lId.muValue = 0x02054000u;   // traffic 336
    lManager.FireGlassEffect(lEffect, lVehicle, lId, 36.483f);

    const BrnEffects::BrnGlassSmashEffect& lrSlot = lManager.maGlassEffects[0];
    std::printf("      producer chain: mLocalTransform.wAxis = (%g, %g, %g, %g)\n",
                static_cast<double>(lrSlot.mLocalTransform.wAxis.x), static_cast<double>(lrSlot.mLocalTransform.wAxis.y),
                static_cast<double>(lrSlot.mLocalTransform.wAxis.z), static_cast<double>(lrSlot.mLocalTransform.wAxis.w));
    Check(lrSlot.mbEffectActive && MatrixFinite(lrSlot.mLocalTransform),
          "producer chain: FireGlassEffect on the producer's transform leaves a FINITE mLocalTransform (the evidence: "
          "NaN on the pre-fix body, a 4x4 determinant of 0)");

    BrnPhysics::Vehicle::VehicleOutputInterface::PhysicalTrafficStateQueue lQueue;
    BrnPhysics::Vehicle::PhysicalTrafficState lState;
    lState.mTransform = lVehicle;
    lState.mEntityID = lId;
    lQueue.mEvents.push_back(lState);
    BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpRaceCars =
        new BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface();
    for (u32 lu = 0; lu < 8u; ++lu)
    {
        lpRaceCars->mabActive[lu] = false;
        lpRaceCars->maStates[lu].mTransform.SetIdentity();
        lpRaceCars->maStates[lu].mEntityId.muValue = 0xFFFFFFFFu;
    }
    BrnEffects::EffectsIO::InputBuffer lInput;
    lInput.mpTrafficQueue = &lQueue;
    lInput.mpRaceCars = lpRaceCars;
    lManager.UpdateVehicleEffectPositions(&lInput, 36.5f);

    const Matrix44Affine& lrSeated = lpModule->maPlayingEffects[17].mTransform;
    std::printf("      producer chain: the pane point (%.3f, %.3f, %.3f), the re-seated effect at (%.3f, %.3f, %.3f)\n",
                static_cast<double>(lEffect.wAxis.x), static_cast<double>(lEffect.wAxis.y),
                static_cast<double>(lEffect.wAxis.z), static_cast<double>(lrSeated.wAxis.x),
                static_cast<double>(lrSeated.wAxis.y), static_cast<double>(lrSeated.wAxis.z));
    bool lbNear = MatrixFinite(lrSeated);
    const Vector3* const lapGot[4] = { &lrSeated.xAxis, &lrSeated.yAxis, &lrSeated.zAxis, &lrSeated.wAxis };
    const Vector3* const lapWant[4] = { &lEffect.xAxis, &lEffect.yAxis, &lEffect.zAxis, &lEffect.wAxis };
    for (u32 lu = 0; lu < 4u; ++lu)
    {
        const f32 lfTolerance = (lu == 3u) ? 1.0e-3f : 1.0e-4f;
        lbNear = lbNear && std::fabs(lapGot[lu]->x - lapWant[lu]->x) <= lfTolerance
                        && std::fabs(lapGot[lu]->y - lapWant[lu]->y) <= lfTolerance
                        && std::fabs(lapGot[lu]->z - lapWant[lu]->z) <= lfTolerance;
    }
    Check(lbNear && (lpModule->maPlayingEffects[17].muFlags & 4u) != 0,
          "producer chain: the re-seat on the same car puts the shatter back on the pane (the point within 1 mm, "
          "the basis within 1e-4) -- where its particles spawn");
    delete lpRaceCars;
    delete lpModule;
}

int main()
{
    for (const GlassFireCase& lrCase : kaGlassFireCases)
        RunFireCase(lrCase);
    for (const GlassUpdateCase& lrCase : kaGlassUpdateCases)
        RunUpdateCase(lrCase);
    RunProducerChain();
    std::printf("FxGlassNan: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
