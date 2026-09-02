#ifndef BRN_ACTIVE_RACE_CAR_DATA_H
#define BRN_ACTIVE_RACE_CAR_DATA_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                              // CgsID
#include "GameSource/Effects/Wheel/WheelStateMachine.h"                  // BrnEffects::WheelStateMachine (by value x4)
#include "GameSource/Effects/Boost/BoostStateMachine.h"                  // BrnEffects::BoostStateMachine (by value)
#include "GameSource/Effects/Jump/JumpStateMachine.h"                    // BrnEffects::JumpStateMachine (by value)
#include "GameSource/Effects/Particles/Native/BrnTrailSystem.h"          // BrnParticle::Native::TrailEmitterData (by value x4)

namespace CgsNumeric { class Random; }
namespace CgsResource { template <class Type> struct ResourcePtr; }
namespace BrnPhysics { namespace Deformation { struct StreamedDeformationSpec; } }
namespace BrnPhysics { namespace Vehicle { struct RaceCarState; } }

// =============================================================================
// ActiveRaceCarData.h -- BrnEffects::BurstAccumulator + BrnEffects::ActiveRaceCarData
//
// ActiveRaceCarData is the effects module's per-active-race-car state (one of
// EffectsModule::maActiveRaceCarData[8]): the four wheel skid-smoke machines, the
// four tyre-mark emitter handles, the jump and boost machines, the crash flags and
// the world-grinding burst accumulator.
//
// DWARF AUTHORITY (DecFIGS ActiveRaceCarData.h:130-240) for the member SET, order,
// names and the method shapes. Every console offset below is pinned by the ARTIST asm
// of the five bodies in ActiveRaceCarData.cpp (Construct @0x82287E08, Reset
// @0x8229B618, Initialise @0x8229D7C8, ExtractTags @0x8229B6C0, Tick @0x82287ED8)
// and by their callers (EffectsModule::UpdateActiveRaceCars @0x8229DB30 strides the
// array by 384; HandleWheels @0x82296C80 reads mTrailEmitters at +0x80 and the wheel
// machines' mvPreviousPosition at +0x10 + 0x20*i; WheelStateMachine::Update reads
// mFlags at +0x130). Console sizeof == 384 (0x180).
//
//   +0x000  WheelStateMachine  mWheelStateMachine[4]   (32 each)
//   +0x080  TrailEmitterData   mTrailEmitters[4]       (32 each on the console)
//   +0x100  JumpStateMachine   mJumpMachine            (0x14)
//   +0x114  u32                mJumpEffectHandle
//   +0x118  u32                maJumpLandingWheelEffectHandles[4]
//   +0x128  CgsID              mID
//   +0x130  u16                mFlags
//   +0x134  f32                mfTimeCrashStarted
//   +0x138  f32                mfGroundPositionY
//   +0x13C  BoostStateMachine  mBoostMachine           (0x24)
//   +0x160  BurstAccumulator   mBurstAccumulatorWorldGrinding (0x18)
//
// Host widths: TrailEmitterData carries a pointer, the two machines carry a vptr, so
// the host object is wider than 384; nothing addresses it by byte offset -- the
// module holds it by name as an array element.
// =============================================================================

namespace BrnEffects
{
    struct CarState;
    class  ParticleEffectHelper;
    class  RaceCarParticleEffectHelper;

    // -------------------------------------------------------------------------
    // BurstAccumulator drives the spark/debris burst timing for an active race car.
    // It accumulates contact "energy" over time and, once a randomised threshold is
    // crossed, emits a whole-number burst count and carries the fractional remainder
    // forward. The randomised next threshold is shaped (rand^2) so small bursts are
    // more common than large ones.
    //
    // Layout proven by Construct @ 0x82278530 and Update @ 0x8227EC90 (six f32, no
    // padding -> sizeof 0x18). Construct's asserts cite
    //   "..\\..\\..\\GameSource\\Effects/ActiveRaceCarData.h" lines 51 ("lfMaxBurstSize
    //   > lfMinBurstSize") and 52 ("lfBurstTimeout > 0.0f").
    // Members accessed by name only.
    // -------------------------------------------------------------------------
    class BurstAccumulator
    {
    public:
        // Construct @ 0x82278530. Installs the configured min/max burst sizes and
        // the burst timeout, primes the next-threshold to lfMinBurstSize and the
        // accumulator to lfMinBurstSize, and zeroes the next-burst time.
        void Construct(f32 lfMinBurstSize, f32 lfMaxBurstSize, f32 lfBurstTimeout);

        // ActiveRaceCarData.h:64 (DWARF) -- inlined into ActiveRaceCarData::Reset
        // @0x8229B618 and ::Initialise @0x8229D7C8 as the three trailing stores
        // (+0x0C = mfMinBurstSize, +0x10 = lfTime, +0x14 = mfMinBurstSize): re-prime the
        // threshold and the accumulator to the minimum and schedule the next burst at
        // lfTime. Both callers pass 0.0f.
        void Reset(f32 lfTime)
        {
            mfNextThreshold = mfMinBurstSize;
            mfNextBurstTime = lfTime;
            mfAccumulator   = mfMinBurstSize;
        }

        // Update @ 0x8227EC90. Adds lfDelta to the accumulator; if lfTime has
        // reached the next-burst time the accumulator is reset to mfMaxBurstSize.
        // While the accumulator is below the current threshold returns 0; once the
        // threshold is crossed it draws a fresh randomised threshold, schedules the
        // next-burst time at (mfBurstTimeout + lfTime), and returns the whole-number
        // burst count (truncated accumulator), carrying the remainder forward.
        // The X360 leaves the integer arguments after lfTime unused.
        s32 Update(f32 lfDelta, f32 lfTime, s32 liArg3, s32 liArg4, CgsNumeric::Random* lpRandom);

    private:
        f32 mfMinBurstSize;       // +0x00
        f32 mfMaxBurstSize;       // +0x04
        f32 mfBurstTimeout;       // +0x08
        f32 mfNextThreshold;      // +0x0C  (randomised burst threshold)
        f32 mfNextBurstTime;      // +0x10  (next allowed burst time)
        f32 mfAccumulator;        // +0x14
    };

    static_assert(sizeof(BurstAccumulator) == 0x18, "BurstAccumulator layout drift");

    // -------------------------------------------------------------------------
    // ActiveRaceCarData (DWARF ActiveRaceCarData.h:130).
    // -------------------------------------------------------------------------
    class ActiveRaceCarData
    {
    public:
        // ActiveRaceCarData.h:219/220 (DWARF) -- the mFlags bits.
        static const u16 eARDFlagWasCrashing = 1;
        static const u16 eARDFlagIsCrashing  = 2;

        // The four tag types ExtractTags harvests: E_TAGPOINT_FXBOOSTPOINT1..4 (41..44).
        static const u32 KU_NUM_WHEELS = 4;

        // :133 -- @0x82287E08 (called by EffectsModule::Construct for each of the 8 slots).
        void Construct();

        // :141 -- @0x82287ED8 (called per car by EffectsModule::UpdateActiveRaceCars).
        void Tick(CarState& lCarState, const BrnPhysics::Vehicle::RaceCarState& lRaceCarState,
                  RaceCarParticleEffectHelper& lHelper, bool lbEventIntroActive, bool lbIsPlayer);

        // :156 -- @0x8229D7C8. Bind the slot to a (new) car: id, boost reset, tag harvest,
        // wheel-machine / trail-emitter / burst re-prime.
        void Initialise(CgsID lID,
                        const CgsResource::ResourcePtr<BrnPhysics::Deformation::StreamedDeformationSpec>& lrPhysicsResource,
                        ParticleEffectHelper& lHelper);

        // :159 -- @0x8229B618. Clear the slot (car gone / model changed).
        void Reset(ParticleEffectHelper& lHelper);

        // :174 / :180 / :186 / :192 / :198 / :204 / :210 -- DWARF inlines.
        f32  GetGroundPositionY() const     { return mfGroundPositionY; }
        bool WasCrashing() const            { return (mFlags & eARDFlagWasCrashing) != 0; }
        bool IsCrashing() const             { return (mFlags & eARDFlagIsCrashing) != 0; }
        bool JustStartedCrashing() const    { return IsCrashing() && !WasCrashing(); }
        bool JustFinishedCrashing() const   { return !IsCrashing() && WasCrashing(); }
        f32  GetCrashStartTime() const      { return mfTimeCrashStarted; }
        BurstAccumulator& GetBurstAccumulatorWorldGrinding() { return mBurstAccumulatorWorldGrinding; }
        u16  GetFlags() const               { return mFlags; }
        CgsID GetID() const                 { return mID; }

        // :166 / :167 / :169 / :170 / :171 -- public data (DWARF: these five precede the
        // private section).
        WheelStateMachine                       mWheelStateMachine[KU_NUM_WHEELS];        // +0x000
        BrnParticle::Native::TrailEmitterData   mTrailEmitters[KU_NUM_WHEELS];            // +0x080
        JumpStateMachine                        mJumpMachine;                             // +0x100
        u32                                     mJumpEffectHandle;                        // +0x114
        u32                                     maJumpLandingWheelEffectHandles[KU_NUM_WHEELS]; // +0x118

    private:
        // :227 -- @0x8229B6C0. Harvest the car's FXBOOSTPOINT tags out of its streamed
        // deformation spec into the boost machine's tag list.
        void ExtractTags(ParticleEffectHelper& lHelper,
                         const CgsResource::ResourcePtr<BrnPhysics::Deformation::StreamedDeformationSpec>& lrPhysicsResource);

        CgsID               mID;                              // +0x128  (:229)
        u16                 mFlags;                           // +0x130  (:230)
        f32                 mfTimeCrashStarted;               // +0x134  (:232)
        f32                 mfGroundPositionY;                // +0x138  (:235)

    public:
        // The boost machine is reached by EffectsModule::Update's camera-changed arm
        // (BoostStateMachine::SetWorldIndex on the player's slot) -- kept reachable by name.
        BoostStateMachine   mBoostMachine;                    // +0x13C  (:237)

    private:
        BurstAccumulator    mBurstAccumulatorWorldGrinding;   // +0x160  (:240)
    };
}

#endif
