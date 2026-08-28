#ifndef GAMESOURCE_DIRECTOR_UTILS_BRN_DIRECTOR_VEHICLE_TRACKER_H
#define GAMESOURCE_DIRECTOR_UTILS_BRN_DIRECTOR_VEHICLE_TRACKER_H

#include "types.hpp"
#include "rw/math/vpu/types.h"                                // rw::math::vpu::Vector3 (journal entry type)
#include "rw/math/vpu/vector3_operation.h"                    // operator- / operator/ (GetImplicitVelocity)
#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT
#include "GameSource/BurnoutConstants.h"                      // EActiveRaceCarIndex
#include "GameSource/Director/Utils/BrnDirectorDataJournal.h" // DataJournal<T,N>
#include "GameSource/Director/DirectorModule/BrnDirectorGameState.h" // BrnDirector::GameState (real home; de-forked)

// ============================================================================
// GameSource/Director/Utils/BrnDirectorVehicleTracker.h
//
// BrnDirector::VehicleTracker -- the director's per-car motion historian. Each frame Update
// samples one tracked race car's position, linear velocity, MPH and (the world) timestep and
// pushes them into four fixed-capacity history journals, then classifies a crash by energy
// and ramps a "race end" cinematic-effect amount up as the car nears the finish line. The
// camera behaviours read the journals back (GetImplicitVelocity = the journal-derived speed)
// to drive lag/lookahead. HOME for the VehicleTracker class.
//
// This header is reconstructed against the X360 binary layout (the 0x2A8-byte tracker that
// embeds CarScoreData and the crash/race-end state), NOT the older/smaller Feb leak shape:
// the binary's Update/GetImplicitVelocity touch members at the offsets pinned below.
//
// ----------------------------------------------------------------------------
// LAYOUT (pinned from BrnDirector::VehicleTracker::Update @0x8223B1A8 and
//   ::GetImplicitVelocity @0x82205F70):
//
//   +0x000  Vector3Journal mPositionJournal        (data +0x000, cursor +0x080, size +0x084)
//   +0x090  Vector3Journal mLinearVelocityJournal  (data +0x090, cursor +0x110, size +0x114)
//   +0x120  FloatJournal   mMphJournal             (data +0x120, cursor +0x140, size +0x144)
//   +0x148  FloatJournal   mTimestepJournal        (data +0x148, cursor +0x168, size +0x16C)
//   +0x170  CarScoreData   mScoreData              (CarScoreData::op copies into it; +0x18 = distance-to-finish)
//   +0x298  s32            meCrashType             (E_CRASH_*; the energy classification)
//   +0x29C  f32            mfRaceEndEffectAmount    (the ramped finish-line effect amount)
//   +0x2A0  s32            miVehicleIndex           (-1 == unbound; the tracked car slot)
//   +0x2A4  bool           mbFirstFrame             (first-frame-of-tracking gate: SetAll vs SetCurrent)
//   +0x2A5  bool           mbIsFirstFrameOfCrash    (set the frame a crash is first detected)
// ----------------------------------------------------------------------------

namespace BrnDirector
{

// ----------------------------------------------------------------------------
// BrnDirector::CrashAnalysis -- the 8-byte per-crash analysis record the tracker
// publishes and the hard-stop moment snapshots (Camera.h forward-declares it;
// Camera::mpCrashAnalysis points at a moment's snapshot). MINIMAL SLICE grown by
// the MomentHardStop TU (@0x82271438's reads): the flag word's attested bits and
// the byte-5 side pick; the remaining bytes are named neutrally. GROW in place.
// ----------------------------------------------------------------------------
struct CrashAnalysis
{
    // +0x00 -- crash direction/type bits (the shot-selector's event-flag word):
    //   bit 1 Left, bit 2 Right, bit 6 Front, bit 7 Rear (the "Hardstop:" log
    //   line), bit 3 <the ultra-slo-mo eligibility>, bit 5 <the hard-stop
    //   eligibility gate>. Roles of the rest not yet recovered.
    u32 mxFlags;

    u8  mau4;            // +0x04 (not consumed by the reconstructed bodies)
    u8  mbUseLeftSide;   // +0x05 (the "Use L"/"Use R" preferred-side pick)
    u8  mau6;            // +0x06
    u8  mau7;            // +0x07
};


// ----------------------------------------------------------------------------
// FLAG: minimal slice of the per-car score block the tracker embeds (CarScoreData has a real
//   reconstructed home at GameSource/GameState/BrnGameStateSharedIO.h, but pulling that whole
//   GameState IO tree into this per-TU `cl /c` gate would drag an unrelated dependency cone;
//   the tracker only reads ONE field of it -- the distance-to-finish at +0x18 -- and copies
//   the whole block via op=). Modelled here as a fixed-size opaque span with exactly that one
//   named field at its pinned offset, copied wholesale by op=. Replace with the real
//   CarScoreData when this TU links against the GameState IO home; the field NAME and the
//   block size are stable.
// ----------------------------------------------------------------------------
class CarScoreData
{
public:
    // Distance from the tracked car to the finish line (CarScoreData +0x18). The race-end
    // effect ramp reads only this field.
    f32 GetDistanceToFinish() const { return mfDistanceToFinish; }

    u8  maHead[0x18];          // +0x00  opaque score fields ahead of the distance
    f32 mfDistanceToFinish;    // +0x18  distance to the finish line
    u8  maTail[0x128 - 0x1C];  // +0x1C  remaining score block (whole block is 0x128 bytes)
};

// ----------------------------------------------------------------------------
// FLAG: minimal slices of the two director-IO inputs Update reads. The runtime InputBuffer /
//   GameState have real homes under GameSource/Director and GameSource/GameState, but Update
//   reaches only the handful of accessors below; modelling those keeps the per-TU gate from
//   pulling the whole director-IO cone. Each accessor's NAME/role is pinned from the body.
//   Replace with the real homes when this TU links; the accessor NAMES are stable.
// ----------------------------------------------------------------------------

// The per-frame world-state snapshot Update reads (event-state journal head + countdown flag)
// now comes from its real home (BrnDirectorGameState.h, #included above) -- de-forked from the
// minimal slice that used to live here. GetCurrentEventState()/IsInCountdown() and the
// E_EVENT_STATE_RACING alias are provided there.

// ⛔⛔ THE PRIVATE `namespace DirectorIO { VehicleInfo; TimerStatusInterface; InputBuffer; }`
// FORK THAT STOOD HERE MOVED INTO THE .cpp (2026-08-29, crash-camera wave), UNCHANGED.
//
// It was three declaration-only slices of types whose REAL homes are
// DirectorModule/BrnDirectorModuleIO.h -- i.e. an ODR fork of BrnDirector::DirectorIO::
// InputBuffer / VehicleInfo, exactly the species this project's rules forbid. It survived only
// because no HEADER had ever pulled this file into a TU that also reaches the real ones.
//
// ⚠️ THE MOMENT ONE DID, IT WAS SIX BROKEN TUs. ArbStateCrashing embeds
// VehicleTracker::ECrashType by value, so BrnArbStateCrashing.h includes this file, and the
// arbitrator state CONTAINER includes that -- which put this fork in front of
// BrnDirectorModuleIO.h in BrnMainDirector.cpp, BrnDirectorModule.cpp, BrnDirectorArbitrator.cpp,
// BrnGameModule.cpp and both GameBridge TUs. Every one of them died on
//   BrnDirectorModuleIO.h(145): error C2011: 'BrnDirector::DirectorIO::InputBuffer': 'class'
//   type redefinition
// and then on ~30 follow-on C2027s. Moving the block to the .cpp -- its only consumer -- makes
// this header safe to include from anywhere, which is what a header carrying a type other
// classes embed by value has to be.
//
// DELETE-WHEN (the real fix, still owed): VehicleTracker::Update is re-fitted to the REAL
// InputBuffer API (GetUsedRaceCars / GetRaceCarInfo / GetTimerStatusInterface) and the .cpp's
// copy goes too. The accessors this TU wants -- IsRaceCarUsed, GetVehicleInfo(idx),
// GetWorldSimTimeStep, GetPlayerScoreData, GetPlayerSpeedMph, IsForcedFastTopDownCrashArmed --
// are all declaration-only today, so nothing is emitted for them and the move costs nothing.
// ⚠️ CLASS-KEY: `struct`, matching the real home (BrnDirectorModuleIO.h:145) and
// BrnCameraFinaliser.h's forward declaration. MSVC mangles the class-key into the symbol, so
// declaring it `class` here -- which the moved fork did -- made every TU that saw THIS header
// FIRST emit `VInputBuffer` while BrnCameraFinaliser.cpp emitted `UInputBuffer`, and
// CameraFinaliser::Update came up unresolved out of BrnMainDirector.obj. Measured, this build.
namespace DirectorIO { struct InputBuffer; }

class VehicleTracker
{
public:

    enum { KI_JOURNAL_SIZE = 8 };

    typedef DataJournal<rw::math::vpu::Vector3, KI_JOURNAL_SIZE> Vector3Journal;
    typedef DataJournal<f32, KI_JOURNAL_SIZE>                    FloatJournal;

    // The crash energy band the tracker classifies the tracked car into.
    enum ECrashType
    {
        E_CRASH_NOT_CRASHING = 0,
        E_CRASH_LOW_ENERGY   = 1,
        E_CRASH_NORMAL       = 2,
        E_CRASH_HIGH_ENERGY  = 3,
    };

    // Reset the journals and the crash/race-end state to empty.
    void Construct();

    // Sample the tracked car this frame: push position / linear-velocity / MPH / timestep
    // into the journals (SetAll on the first frame, SetCurrent thereafter), classify the
    // crash energy when this is the player car, copy the player's score block, and ramp the
    // race-end effect amount toward its target. @0x8223B1A8.
    void Update(
        const GameState* lpGameState,
        const DirectorIO::InputBuffer* lpInput,
        EActiveRaceCarIndex lePlayerCarIndex,
        bool lbForceNextWorldCrashToBeFastTopDown);

    // The implicit velocity = (newest position - previous position) / newest timestep, or
    // zero when there is not yet a previous sample. @0x82205F70.
    rw::math::vpu::Vector3 GetImplicitVelocity() const;

    // The implicit acceleration = (newest linear velocity - previous linear velocity) /
    // newest timestep, or zero when there is not yet a previous linear-velocity sample.
    // @0x82206178 (called by BrnDirector::ArbStateCarSelect::Update). The velocity-journal
    // analogue of GetImplicitVelocity; the console body does the divide as a VMX reciprocal
    // broadcast over the lanes (vrefp + Newton refinement), folded here to the scalar lane
    // divide (operator/(Vector3, f32)) per the rw vpu reconstruction precedent.
    rw::math::vpu::Vector3 GetImplicitAcceleration() const;

    // Read-only journal access for the camera behaviours.
    const Vector3Journal& GetPositionJournal() const       { return mPositionJournal; }
    const Vector3Journal& GetLinearVelocityJournal() const { return mLinearVelocityJournal; }
    const FloatJournal&   GetMphJournal() const             { return mMphJournal; }

    f32                   GetMPHLastFrame() const            { return mMphJournal.GetPrevious(); }
    // @0x82218CC0 (called by BrnDirector::MainDirector::Update). The asm guards via the
    // indexed GetPrevious overload (asserts miSize > 0 at DataJournal.h:115 and
    // lIndex < miSize-1 at :116 -- the liIndex >= 0 check at :114 folds away for the constant
    // 0), then returns the journal's [1] entry: the linear velocity sampled last frame.
    rw::math::vpu::Vector3 GetLinearVelocityLastFrame() const { return mLinearVelocityJournal.GetPrevious(0); }

    void SetVehicleIndex(s32 liVehicleIndex) { miVehicleIndex = liVehicleIndex; }

    ECrashType GetCrashType() const          { return meCrashType; }
    bool       IsFirstFrameOfCrash() const   { return mbIsFirstFrameOfCrash; }
    f32        GetRaceEndEffectAmount() const { return mfRaceEndEffectAmount; }

private:

    Vector3Journal mPositionJournal;        // +0x000
    Vector3Journal mLinearVelocityJournal;  // +0x090
    FloatJournal   mMphJournal;             // +0x120
    FloatJournal   mTimestepJournal;        // +0x148

    CarScoreData   mScoreData;              // +0x170

    ECrashType     meCrashType;             // +0x298
    f32            mfRaceEndEffectAmount;   // +0x29C
    s32            miVehicleIndex;          // +0x2A0
    bool           mbFirstFrame;            // +0x2A4
    bool           mbIsFirstFrameOfCrash;   // +0x2A5
};



// ----------------------------------------------------------------------------
// BrnDirector::VehicleTracker::GetImplicitVelocity @0x82205F70
//   When fewer than two position samples exist, returns zero. Otherwise asserts the newest
//   timestep is non-zero and returns (position[0] - position[1]) / timestep[0]. The console
//   body does the divide as a VMX reciprocal broadcast over the lanes; the reconstructed
//   rw math home folds that to the scalar lane divide (operator/(Vector3, f32)).
// ----------------------------------------------------------------------------
inline rw::math::vpu::Vector3
VehicleTracker::GetImplicitVelocity() const
{
    if (mPositionJournal.GetSize() > 1)
    {
        CGS_ASSERT(!rw::math::vpu::IsZero(rw::math::vpu::Vector3{ mTimestepJournal[0], 0.0f, 0.0f, 0.0f }),
                   "!rw::math::fpu::IsZero(mTimestepJournal[0])");
        return (mPositionJournal[0] - mPositionJournal[1]) / mTimestepJournal[0];
    }
    else
    {
        return rw::math::vpu::Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
    }
}

// ----------------------------------------------------------------------------
// BrnDirector::VehicleTracker::GetImplicitAcceleration @0x82206178
//   When fewer than two linear-velocity samples exist (mLinearVelocityJournal.miSize <= 1),
//   returns zero. Otherwise asserts the newest timestep is non-zero and returns
//   (linearVelocity[0] - linearVelocity[1]) / timestep[0]. The console body does the divide
//   as a VMX reciprocal broadcast over the lanes (vrefp estimate + one Newton refinement step
//   then vmulfp128); the reconstructed rw math home folds that to the scalar lane divide
//   (operator/(Vector3, f32)). The newest-vs-previous element pair is read out of the journal
//   ring as [0]/[1] (the asm's (writeIndex)%8 and (writeIndex+1)%8 slots).
// ----------------------------------------------------------------------------
inline rw::math::vpu::Vector3
VehicleTracker::GetImplicitAcceleration() const
{
    if (mLinearVelocityJournal.GetSize() > 1)
    {
        CGS_ASSERT(!rw::math::vpu::IsZero(rw::math::vpu::Vector3{ mTimestepJournal[0], 0.0f, 0.0f, 0.0f }),
                   "!rw::math::fpu::IsZero(mTimestepJournal[0])");
        return (mLinearVelocityJournal[0] - mLinearVelocityJournal[1]) / mTimestepJournal[0];
    }
    else
    {
        return rw::math::vpu::Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
    }
}

} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_UTILS_BRN_DIRECTOR_VEHICLE_TRACKER_H
