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
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // the [boost*] witnesses
#include <cfloat>                                                                   // FLT_MAX
#include <cmath>                                                                    // std::fma (BurstAccumulator::Update's fmadds)
#include <cstdio>                                             // snprintf (witnesses)

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

    mfMinBurstSize       = lfMinBurstSize;
    mfMaxBurstSize       = lfMaxBurstSize;
    mfBurstTimeout       = lfBurstTimeout;
    mfBurstSizeThreshold = lfMinBurstSize;
    mfBurstTimeThreshold = 0.0f;
    mfCurrentBurstSize   = lfMinBurstSize;
}

// ---- Update @ 0x8227EC90 (62 instr) ----------------------------------------
// Straight from the asm (FX-CRASHVFX 2026-09-24 -- three corrections to the earlier body, each
// pinned by tests/run_fxcrashvfx_burst_accumulator.py against the function's own words):
//   * `fcmpu cr6, f2, f13 ; blt` -- ONLY an ordered "lfTime < mfBurstTimeThreshold" keeps the
//     accumulated size; a NaN time falls through to the reset. The old `if (lfTime >= next)`
//     had the NaN polarity backwards.
//   * the threshold is `fmuls f11, f11, f0` then `fmadds f0, f11, f0, f12`: ((max - min) * r)
//     rounded, then * r + min FUSED. The old spelling rounded twice.
//   * the count is `fctidz` + `stfiwx`: the LOW WORD of a 64-bit truncation. A NaN size
//     truncates to 0x8000000000000000, whose low word is 0 -- no burst -- where the old
//     `static_cast<s32>` returned 0x80000000 and every caller would have spawned 2^31 sparks.
//     The remainder subtracts that count as `lwz` left it, zero-extended into the `fcfid`.
// The random draw is CgsNumeric::Random::RandomFloat() inlined (0x8227ECC4..0x8227ED24).
u32 BurstAccumulator::Update(f32 lfBurstSize, f32 lfTime, CgsNumeric::Random& lrRandom)
{
    mfCurrentBurstSize = lfBurstSize + mfCurrentBurstSize;
    if (!(lfTime < mfBurstTimeThreshold))
    {
        mfCurrentBurstSize = mfMaxBurstSize;
    }

    if (mfCurrentBurstSize < mfBurstSizeThreshold)
    {
        return 0u;
    }

    const f32 lfRandom = lrRandom.RandomFloat();
    const f32 lfRange  = mfMaxBurstSize - mfMinBurstSize;

    mfBurstTimeThreshold = mfBurstTimeout + lfTime;

    // fctidz: truncate toward zero into a doubleword, saturating; NaN -> 0x8000000000000000.
    const f64 lfSize = static_cast<f64>(mfCurrentBurstSize);
    s64 liTruncated;
    if (lfSize != lfSize)
        liTruncated = static_cast<s64>(0x8000000000000000ULL);
    else if (lfSize >= 9223372036854775808.0)
        liTruncated = static_cast<s64>(0x7FFFFFFFFFFFFFFFULL);
    else if (lfSize < -9223372036854775808.0)
        liTruncated = static_cast<s64>(0x8000000000000000ULL);
    else
        liTruncated = static_cast<s64>(lfSize);
    const u32 luBurstCount = static_cast<u32>(static_cast<u64>(liTruncated));   // stfiwx: the low word

    mfBurstSizeThreshold = std::fma(lfRange * lfRandom, lfRandom, mfMinBurstSize);
    mfCurrentBurstSize   = mfCurrentBurstSize - static_cast<f32>(static_cast<f64>(luBurstCount));

    return luBurstCount;
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
    // ---- [boosttag] witness. NOT console behaviour: ours, bounded, log-only. --------
    // BoostStateMachine::StartEffects loops `for (lu = 0; lu < muNumBoostTags; ++lu)`, so a
    // zero tag count starts NOTHING and leaves no trace at all -- the loop body simply never
    // runs. Every line below therefore prints OUTSIDE any loop and on EVERY exit, including
    // the early one, so the log distinguishes the four outcomes that look identical
    // downstream: never called / called with the NULL resource / called with locators but no
    // FXBOOSTPOINT among them / called and counted N. DELETE-WHEN-STABLE.
    static u32 suExtractWitness = 0;
    const u32 KU_EXTRACT_WITNESS_LIMIT = 8;
    const bool lbWitness = (suExtractWitness < KU_EXTRACT_WITNESS_LIMIT);
    if (lbWitness)
        ++suExtractWitness;

    if (CgsResource::NULLResourcePtr.IsEqual(&lrPhysicsResource))
    {
        if (lbWitness)
        {
            char lacMsg[192];
            std::snprintf(lacMsg, sizeof(lacMsg),
                "[boosttag] #%u ExtractTags EARLY-OUT: the deformation spec is the NULL "
                "resource, so muNumBoostTags stays %u and no boost effect can start.\n",
                suExtractWitness, mBoostMachine.muNumBoostTags);
            CgsDev::Log::WriteToLog(lacMsg);
        }
        return;
    }

    // ResourcePtr<StreamedDeformationSpec>::operator-> (X360 sub_82285DB8, the baked
    // "resource pointer NULL" assert) -- the const overload of the committed template.
    const BrnPhysics::Deformation::StreamedDeformationSpec* lpSpec =
        const_cast<CgsResource::ResourcePtr<BrnPhysics::Deformation::StreamedDeformationSpec>&>(lrPhysicsResource).operator->();

    const BrnPhysics::Deformation::LocatorPointSpecList& lrGenericTags = lpSpec->mGenericTags;
    const u32 luNumLocators = lrGenericTags.GetNumLocatorPoints();

    BrnParticle::ParticleModule& lrParticleModule = lHelper.ParticleModule();

    // [boosttag] the tag types actually present, so "no FXBOOSTPOINT" is distinguishable
    // from "no locators at all" and from "the tag enum decodes to something else". The mask
    // is 64 bits ON PURPOSE: FXBOOSTPOINT1..4 are 41..44, and a 32-bit mask could not have
    // shown them at all.
    u64 luTagTypeMask = 0;
    u32 luHighestTag  = 0;

    for (u32 luTag = 0; luTag < luNumLocators; ++luTag)
    {
        const BrnPhysics::Deformation::LocatorPointSpec lLocator = lrGenericTags.CreateLo(luTag);
        if (lbWitness)
        {
            const u32 luTagType = static_cast<u32>(lLocator.meTagPointType);
            if (luTagType < 64)
                luTagTypeMask |= (1ull << luTagType);
            if (luTagType > luHighestTag)
                luHighestTag = luTagType;
        }
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

        // ⚠ HOST-SIDE BRING-UP CAPTURE, not console behaviour. The console's tag loop really
        // does throw the locator away (see the note above this function) because the DEFORMED
        // per-frame table positions the effects. That table is empty on this build, so the
        // BIND-POSE matrix is kept here for BoostStateMachine::OnTick's stand-in.
        // DELETE-WHEN the deformation module publishes VehicleLocatorOutput for race cars.
        if (mBoostMachine.muNumBoostTags < BrnEffects::BoostStateMachine::KU_MAX_BOOST_EFFECTS)
        {
            mBoostMachine.maBoostLocatorLocal[mBoostMachine.muNumBoostTags] =
                lLocator.mLocatorMatrix;
            mBoostMachine.mbBoostLocatorLocalValid = true;
        }

        lruHandle = BrnParticle::LionEffect::KU_HANDLE_INVALID;
        ++mBoostMachine.muNumBoostTags;
    }

    if (lbWitness)
    {
        char lacMsg[384];
        std::snprintf(lacMsg, sizeof(lacMsg),
            "[boosttag] #%u ExtractTags RAN: locators=%u boostTags=%u highestTag=%u "
            "tagTypesSeen(bit n == tag type n, n<64)=%016llX  "
            "FXBOOSTPOINT1..4 == bits 41..44 == mask 00001E0000000000.\n",
            suExtractWitness, luNumLocators, mBoostMachine.muNumBoostTags, luHighestTag,
            static_cast<unsigned long long>(luTagTypeMask));
        CgsDev::Log::WriteToLog(lacMsg);
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
