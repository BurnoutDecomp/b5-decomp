#include "types.hpp"
#include "GameSource/Effects/ActiveRaceCarData.h"
#include "GameSource/Effects/EffectsModule.h"                                       // BrnEffects::CarState
#include "GameSource/Effects/ParticleEffectHelper.h"                                // ParticleEffectHelper / RaceCarParticleEffectHelper
#include "GameSource/Effects/Particles/ParticleModule.h"                            // BrnParticle::ParticleModule / LionEffect
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"            // BrnPhysics::Vehicle::RaceCarState / WheelLite
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnStreamedDeformationSpec.h" // StreamedDeformationSpec / LocatorPointSpecList
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"                  // CgsResource::ResourcePtr / NULLResourcePtr
#include "GameShared/GameClasses/Core/CgsAssert.h"                                  // CGS_ASSERT
#include "GameShared/GameClasses/Numeric/CgsRandom.h"                               // CgsNumeric::Random
#include <cfloat>                                                                   // FLT_MAX

// =============================================================================
// BrnEffects::BurstAccumulator / BrnEffects::ActiveRaceCarData (X360 ARTIST)
//   BurstAccumulator::Construct @0x82278530   ::Update @0x8227EC90
//   ActiveRaceCarData::Construct @0x82287E08  ::Reset @0x8229B618
//   ActiveRaceCarData::Initialise @0x8229D7C8 ::ExtractTags @0x8229B6C0
//   ActiveRaceCarData::Tick @0x82287ED8
// =============================================================================

namespace BrnEffects
{

// ---- Construct @ 0x82278530 ----------------------------------------------
// Asserts (then stores) per the X360 body: lfMaxBurstSize > lfMinBurstSize and
// lfBurstTimeout > 0.0f. Both fields fall through to the stores regardless of the
// assert outcome (the assert is advisory). Stores: min, max, timeout, then the
// next-threshold and accumulator both primed to lfMinBurstSize and the
// next-burst time to 0.0f.
void BurstAccumulator::Construct(f32 lfMinBurstSize, f32 lfMaxBurstSize, f32 lfBurstTimeout)
{
    CGS_ASSERT(lfMaxBurstSize > lfMinBurstSize, "lfMaxBurstSize > lfMinBurstSize");
    CGS_ASSERT(lfBurstTimeout > 0.0f, "lfBurstTimeout > 0.0f");

    mfMinBurstSize  = lfMinBurstSize;
    mfMaxBurstSize  = lfMaxBurstSize;
    mfBurstTimeout  = lfBurstTimeout;
    mfNextThreshold = lfMinBurstSize;
    mfNextBurstTime = 0.0f;
    mfAccumulator   = lfMinBurstSize;
}

// ---- Update @ 0x8227EC90 -------------------------------------------------
// Accumulate lfDelta; reset the accumulator to mfMaxBurstSize once lfTime reaches
// the scheduled next-burst time. Below the current threshold -> emit nothing.
// Otherwise draw a fresh rand-in-[0,1) (the X360 inlines CgsNumeric::Random's
// per-call draw: read the oldest ring slot, refill it from the advanced LCG seed,
// and return slot-1.0f), shape the next threshold as
//   mfNextThreshold = (mfMaxBurstSize - mfMinBurstSize) * r * r + mfMinBurstSize,
// schedule the next-burst time at (mfBurstTimeout + lfTime), emit the truncated
// accumulator as the burst count and carry the fractional remainder forward.
// liArg3 / liArg4 are unreferenced by the X360 body.
s32 BurstAccumulator::Update(f32 lfDelta, f32 lfTime, s32 liArg3, s32 liArg4, CgsNumeric::Random* lpRandom)
{
    (void)liArg3;
    (void)liArg4;

    mfAccumulator = lfDelta + mfAccumulator;
    if (lfTime >= mfNextBurstTime)
    {
        mfAccumulator = mfMaxBurstSize;
    }

    if (mfAccumulator < mfNextThreshold)
    {
        return 0;
    }

    const f32 lfRand01 = lpRandom->RandomFloat();
    const f32 lfRange  = mfMaxBurstSize - mfMinBurstSize;

    mfNextBurstTime = mfBurstTimeout + lfTime;

    const s32 liBurstCount = static_cast<s32>(mfAccumulator);
    mfNextThreshold = lfRange * lfRand01 * lfRand01 + mfMinBurstSize;
    mfAccumulator   = mfAccumulator - static_cast<f32>(liBurstCount);

    return liBurstCount;
}

// =============================================================================
// ActiveRaceCarData
// =============================================================================

namespace
{
    // BurstAccumulator::Construct(+0x160, 8.0, 150.0, 1.0) -- the world-grinding burst
    // shape, three immediates in ActiveRaceCarData::Construct @0x82287E08.
    const f32 KF_WORLD_GRINDING_MIN_BURST_SIZE = 8.0f;
    const f32 KF_WORLD_GRINDING_MAX_BURST_SIZE = 150.0f;
    const f32 KF_WORLD_GRINDING_BURST_TIMEOUT  = 1.0f;

    // The FXBOOSTPOINT tag range ExtractTags harvests (E_TAGPOINT_FXBOOSTPOINT1..4).
    inline bool IsBoostPointTag(BrnPhysics::Deformation::ETagPointType leType)
    {
        return leType == BrnPhysics::Deformation::E_TAGPOINT_FXBOOSTPOINT1
            || leType == BrnPhysics::Deformation::E_TAGPOINT_FXBOOSTPOINT2
            || leType == BrnPhysics::Deformation::E_TAGPOINT_FXBOOSTPOINT3
            || leType == BrnPhysics::Deformation::E_TAGPOINT_FXBOOSTPOINT4;
    }
}

// ---- Construct @ 0x82287E08 (called by EffectsModule::Construct for each slot) -----
//   +0x128 mID = 0 (a 64-bit store)      +0x130 mFlags = 0
//   mBoostMachine.OnConstruct()          (the vtable slot-3 call on +0x13C)
//   for each wheel: WheelStateMachine::Construct(i)  (index, three zero accumulators,
//                   zero previous position -- the 32-byte stride loop)
//   maJumpLandingWheelEffectHandles[i] = -1
//   mJumpMachine: mTime = 0.0 (+0x108), mState = 0 (+0x104)   (Construct inlined)
//   +0x138 mfGroundPositionY = 0.0      +0x114 mJumpEffectHandle = -1
//   BurstAccumulator::Construct(+0x160, 8.0, 150.0, 1.0)
// NOTE: the console does NOT touch mTrailEmitters here (they are primed by Initialise).
void ActiveRaceCarData::Construct()
{
    mID    = 0;
    mFlags = 0;

    mBoostMachine.OnConstruct();

    for (u32 luWheel = 0; luWheel < KU_NUM_WHEELS; ++luWheel)
    {
        mWheelStateMachine[luWheel].Construct(luWheel);
        maJumpLandingWheelEffectHandles[luWheel] = BrnParticle::LionEffect::KU_HANDLE_INVALID;
    }

    // JumpStateMachine's Construct, inlined: +0x108 mTime = 0.0 then +0x104 mState = 0.
    mJumpMachine.mTime  = 0.0f;
    mJumpMachine.mState = EffectsStateAllOff;
    mfGroundPositionY = 0.0f;
    mJumpEffectHandle = BrnParticle::LionEffect::KU_HANDLE_INVALID;

    mBurstAccumulatorWorldGrinding.Construct(KF_WORLD_GRINDING_MIN_BURST_SIZE,
                                             KF_WORLD_GRINDING_MAX_BURST_SIZE,
                                             KF_WORLD_GRINDING_BURST_TIMEOUT);
}

// ---- Reset @ 0x8229B618 ----------------------------------------------------
//   mID = 0; mFlags = 0; BoostStateMachine::Reset(mBoostMachine, lHelper);
//   for each wheel: WheelStateMachine::Reset() (the three accumulators + the previous
//                   position zeroed; the index kept -- +4..+12 and the vec at +16);
//   BurstAccumulator::Reset(0.0f) on the world-grinding accumulator (+0x16C = min,
//   +0x170 = 0.0, +0x174 = min).
void ActiveRaceCarData::Reset(ParticleEffectHelper& lHelper)
{
    mID    = 0;
    mFlags = 0;

    mBoostMachine.Reset(lHelper);

    for (u32 luWheel = 0; luWheel < KU_NUM_WHEELS; ++luWheel)
    {
        mWheelStateMachine[luWheel].Reset();
    }

    mBurstAccumulatorWorldGrinding.Reset(0.0f);
}

// ---- Initialise @ 0x8229D7C8 -------------------------------------------------
//   mID = lID (std); mFlags = 0; BoostStateMachine::Reset(mBoostMachine, lHelper);
//   ExtractTags(lHelper, lrPhysicsResource);
//   for each wheel: WheelStateMachine::Reset() AND TrailEmitterData::Prepare()
//                   (+0x90/+0x94 = 0 / -1.0, stride 32 -- the emitter handle cleared,
//                   "no trail this frame");
//   BurstAccumulator::Reset(0.0f).
void ActiveRaceCarData::Initialise(CgsID lID,
                                   const CgsResource::ResourcePtr<BrnPhysics::Deformation::StreamedDeformationSpec>& lrPhysicsResource,
                                   ParticleEffectHelper& lHelper)
{
    mID    = lID;
    mFlags = 0;

    mBoostMachine.Reset(lHelper);
    ExtractTags(lHelper, lrPhysicsResource);

    for (u32 luWheel = 0; luWheel < KU_NUM_WHEELS; ++luWheel)
    {
        mWheelStateMachine[luWheel].Reset();
        mTrailEmitters[luWheel].Prepare();
    }

    mBurstAccumulatorWorldGrinding.Reset(0.0f);
}

// ---- ExtractTags @ 0x8229B6C0 --------------------------------------------------
// Skipped entirely when the resource pointer is the NULL sentinel
// (CgsResource::BaseResourcePtr::IsEqual(&NULLResourcePtr, &lrPhysicsResource)).
// Otherwise walk the spec's generic locator list (spec +36 == mGenericTags): every
// FXBOOSTPOINT1..4 tag claims the next boost-machine tag slot -- if the particle module's
// playing-effect slot for that slot's stored handle still holds the handle, the effect is
// stopped first (the inlined GetLionEffect: `maPlayingEffects[h & 0x7F].muHandle == h`),
// the slot's handle is reset to KU_HANDLE_INVALID and the tag count advances. Then the
// boost machine's state/timer (+0x140/+0x144) and the jump machine's (+0x104/+0x108) are
// zeroed.
//
// FLAG (faithful oddity, not a bug of ours): the tag loop never stores the tag's locator
// into the slot -- on the console the slot only ever records "a tag exists" through the
// count, and the boost machine's OnChangeState re-resolves locators by tag type at start.
void ActiveRaceCarData::ExtractTags(ParticleEffectHelper& lHelper,
                                    const CgsResource::ResourcePtr<BrnPhysics::Deformation::StreamedDeformationSpec>& lrPhysicsResource)
{
    if (CgsResource::NULLResourcePtr.IsEqual(&lrPhysicsResource))
    {
        return;
    }

    // ResourcePtr<StreamedDeformationSpec>::operator-> (X360 sub_82285DB8, the baked
    // "resource pointer NULL" assert) -- the const overload of the committed template.
    const BrnPhysics::Deformation::StreamedDeformationSpec* lpSpec =
        const_cast<CgsResource::ResourcePtr<BrnPhysics::Deformation::StreamedDeformationSpec>&>(lrPhysicsResource).operator->();

    const BrnPhysics::Deformation::LocatorPointSpecList& lrGenericTags = lpSpec->mGenericTags;
    const u32 luNumLocators = lrGenericTags.GetNumLocatorPoints();

    BrnParticle::ParticleModule& lrParticleModule = lHelper.ParticleModule();

    for (u32 luTag = 0; luTag < luNumLocators; ++luTag)
    {
        const BrnPhysics::Deformation::LocatorPointSpec lLocator = lrGenericTags.CreateLo(luTag);
        if (!IsBoostPointTag(lLocator.meTagPointType))
        {
            continue;
        }

        u32& lruHandle = mBoostMachine.mEffects[mBoostMachine.muNumBoostTags].muHandle;

        // GetLionEffect inlined: the slot still holds this handle -> the effect is live.
        BrnParticle::LionEffect& lrSlot =
            lrParticleModule.maPlayingEffects[lruHandle & BrnParticle::LionEffect::KU_HANDLE_INDEX_MASK];
        if (lrSlot.muHandle == lruHandle)
        {
            lrParticleModule.StopLionEffect(&lrSlot);
        }

        lruHandle = BrnParticle::LionEffect::KU_HANDLE_INVALID;
        ++mBoostMachine.muNumBoostTags;
    }

    mBoostMachine.mState = EffectsStateAllOff;
    mBoostMachine.mTime  = 0.0f;
    mJumpMachine.mState  = EffectsStateAllOff;
    mJumpMachine.mTime   = 0.0f;
}

// ---- Tick @ 0x82287ED8 ---------------------------------------------------------
//   WasCrashing <- IsCrashing (bit 1 -> bit 0);  IsCrashing <- lRaceCarState.mbCrashing (+1098)
//   mfGroundPositionY = the LOWEST road-contact y among the wheels that are attached
//                       (+0x60) and whose line test is valid (+0x2B), seeded at FLT_MAX
//                       (the four fsel(FLT_MAX - y) mins, one per 112-byte WheelLite).
//   if (!lbEventIntroActive || lbIsPlayer) mBoostMachine.Tick(lCarState, lHelper)
//   if (JustStartedCrashing()) mfTimeCrashStarted = lCarState.mfCurrentTime (+0x14)
void ActiveRaceCarData::Tick(CarState& lCarState, const BrnPhysics::Vehicle::RaceCarState& lRaceCarState,
                             RaceCarParticleEffectHelper& lHelper, bool lbEventIntroActive, bool lbIsPlayer)
{
    if ((mFlags & eARDFlagIsCrashing) != 0)
        mFlags = static_cast<u16>((mFlags & ~eARDFlagWasCrashing) | eARDFlagWasCrashing);
    else
        mFlags = static_cast<u16>(mFlags & ~eARDFlagWasCrashing);

    if (lRaceCarState.mbCrashing)
        mFlags = static_cast<u16>(mFlags | eARDFlagIsCrashing);
    else
        mFlags = static_cast<u16>(mFlags & ~eARDFlagIsCrashing);

    // The console keeps the running minimum in f0 and only STORES it inside each arm, so
    // a car with no valid wheel keeps its previous mfGroundPositionY -- reproduced.
    f32 lfLowest = FLT_MAX;
    for (u32 luWheel = 0; luWheel < KU_NUM_WHEELS; ++luWheel)
    {
        const BrnPhysics::Vehicle::WheelLite& lrWheel = lRaceCarState.maWheels[luWheel];
        if (lrWheel.mbAttached && lrWheel.mRoadContact.mbLineTestIsValid)
        {
            const f32 lfY = lrWheel.mRoadContact.mPosition.y;
            lfLowest = ((lfLowest - lfY) >= 0.0f) ? lfY : lfLowest;   // fsel(FLT_MAX - y, y, running)
            mfGroundPositionY = lfLowest;
        }
    }

    if (!lbEventIntroActive || lbIsPlayer)
    {
        mBoostMachine.Tick(lCarState, lHelper);
    }

    if (JustStartedCrashing())
    {
        // asm 0x82288048/4C: `lfs f0, 0x14(r30)` / `stfs f0, 0x134(r31)` -- the CarState's
        // EffectsModuleParams::mTime (+0x14), NOT a `mfCurrentTime` member (CarState has none;
        // the old spelling never compiled -- this TU had never been through the gate).
        mfTimeCrashStarted = lCarState.GetTime();
    }
}

} // namespace BrnEffects
