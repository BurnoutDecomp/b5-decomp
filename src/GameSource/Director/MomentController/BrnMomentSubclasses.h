#pragma once

// ============================================================================
// GameSource/Director/MomentController/BrnMomentSubclasses.h
//
// RETIRED FROM ELEVEN STUBS DOWN TO ONE, 2026-08-23 (jump/stunt cutaway-camera wave).
//
// WHAT THIS FILE USED TO BE. Eleven `class MomentXxx : public Moment` definitions, each with
// NO data members and a BRN_MOMENT_STUB_OVERRIDES macro that made every virtual a one-liner
// (`Prepare -> return true`, `Update -> {}`, `Release -> return true`). Its own banner called
// them "deliberately MINIMAL, layout-stubbed slices" whose real member sets "land with each
// moment's own ledger TU".
//
// THEY ALL LANDED -- every one of the eleven has a real, complete home under Moments/ with the
// DWARF member set and real bodies -- and MomentController::NewMoment @0x82255850 was still
// allocating THE STUBS, because this header is what it included. That is a straight ODR
// violation (two definitions of e.g. BrnDirector::MomentPlayerJumping in one program, with
// different sizes and different vtables), and it is also why un-stubbing NewMoment on its own
// would have changed nothing: a stubbed MomentPlayerJumping's Update() is `{}`, so its meState
// never leaves E_STATE_INVALID_INACTIVE, so it is never IsValid(), so muValidMoments stays 0.
//
// TEN OF THE ELEVEN ARE GONE. Their only consumer, BrnMomentControllerNewMoment.cpp, now
// includes the ten real homes directly. That umbrella lives in that .cpp on purpose and must
// NOT be moved back into a header -- see the next paragraph.
//
// ⛔ WHY MomentHardStop COULD NOT FOLLOW -- AND WHAT WOULD LET IT.
// This header is reached by BrnMomentParameterBank.h (which needs MomentHardStop::Parameters
// by value), which is reached by BrnMomentController.h (holds the bank by value), which is
// reached by BrnMomentSelector.h, which is reached by BrnMainDirector.cpp. The REAL
// Moments/BrnMomentHardStop.h includes GameSource/Director/Utils/BrnDirectorVehicleTracker.h
// for BrnDirector::CrashAnalysis (MomentHardStop holds one by value at h:101), and that header
// carries its own `class BrnDirector::DirectorIO::InputBuffer` slice (:126) -- a SECOND
// definition of the `struct BrnDirector::DirectorIO::InputBuffer` in
// GameSource/Director/DirectorModule/BrnDirectorModuleIO.h (:145). The two have different
// class keys AND different method sets, so any TU that sees both dies with C2011. MEASURED
// 2026-08-23: routing the bank at the real MomentHardStop breaks BrnMainDirector.cpp outright.
//
// ⇒ DELETE-WHEN (this is the last stub in this file, and it is blocked on someone else's fork):
//    de-fork BrnDirector::DirectorIO::InputBuffer -- delete the declaration-only slice from
//    BrnDirectorVehicleTracker.h:126, include BrnDirectorModuleIO.h there instead, and adapt
//    BrnDirectorVehicleTracker.cpp's seven call sites (GetUsedRaceCars / IsRaceCarUsed /
//    GetVehicleInfo / GetTimerStatusInterface / GetPlayerScoreData / GetPlayerSpeedMph /
//    IsForcedFastTopDownCrashArmed) to the real InputBuffer's API. Then replace the include
//    below with Moments/BrnMomentHardStop.h and delete this file.
//
// ⭐ THE HARM THIS ONE STILL DOES IS BOUNDED AND NAMED: the nested Parameters record below is
//    layout-identical to the real one (DWARF Moments/BrnMomentHardStop.h:111 -- two floats),
//    so the parameter BANK's layout is correct either way. The live defect is that
//    NewMoment's `AllocateVoid<MomentHardStop>()` arm would allocate this empty slice instead
//    of the real 528-byte moment. That arm is unreachable today (NewMoment itself is a GROUP F
//    stub, and no mounted arbitrator state registers E_MOMENT_HARD_STOP), but it MUST be fixed
//    before the moment closure is mounted.
// ============================================================================

#include "types.hpp"
#include "GameSource/Director/MomentController/BrnMoment.h"   // BrnDirector::Moment

namespace BrnDirector
{
    // ⛔ LAYOUT-STUBBED SLICE -- see the banner. The real home is
    // Moments/BrnMomentHardStop.h and the two CANNOT share a TU.
    class MomentHardStop : public Moment
    {
    public:
        // DWARF: Moments/BrnMomentHardStop.h:111. The hard-stop "ultra slow-mo" tuning the
        // parameter bank stores by value. Layout-identical to the real home's record, which is
        // what makes the bank's layout correct despite the enclosing slice.
        struct Parameters : public Moment::Parameters
        {
            f32 mfDuration;             // BrnMomentHardStop.h:117
            f32 mfSpeedDiffThreshold;   // BrnMomentHardStop.h:118
        };

        bool        Prepare(void* /*lrBehaviourController*/) override { return true; }
        void        Update(f32, void*, const void*) override {}
        bool        Release() override { return true; }
        const char* GetName() const override { return "E_MOMENT_HARD_STOP"; }
        EType       GetInstanceType() override { return E_MOMENT_HARD_STOP; }
    };

} // namespace BrnDirector
