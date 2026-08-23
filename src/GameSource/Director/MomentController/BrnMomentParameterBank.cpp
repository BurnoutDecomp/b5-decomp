// Out-of-line bodies for BrnDirector::MomentParameterBank -- the fixed bank of pre-authored
// per-moment tuning records MomentController::NewMoment hands to a freshly-allocated moment.
//
// Bodied here:
//   BrnDirector::MomentParameterBank::Construct      @0x82209E38
//   BrnDirector::MomentParameterBank::GetParameters  @0x821F7428
//
// ============================================================================================
// ODR RECONCILE, 2026-08-23 (jump/stunt cutaway-camera wave). THIS TU USED TO BE A FORK.
//
// It did not include BrnMomentParameterBank.h at all. Instead it re-declared, LOCALLY, its own
//   struct Moment { struct Parameters {}; };
//   struct MomentHardStop / MomentBystanderSeesAction / MomentTumbling  (each with a nested
//                                                                        Parameters)
//   class  MomentParameterBank { ... };
// -- i.e. six types that all have real homes elsewhere, one of them (MomentParameterBank) held
// BY VALUE inside MomentController. Two concrete consequences, both real:
//
//   1. THE CLASS-KEY ODR FORK the GROUP F DELETE-WHEN named. BrnMoment.h spells
//      `class Moment::Parameters`; this file spelled `struct`. MSVC mangles the class key into
//      the name, so GetParameters was emitted here as `...@@QEAAPEAUParameters@Moment@...`
//      (U == struct) while every caller compiled against the header referenced
//      `...PEAVParameters@Moment@...` (V == class). Two symbols for one function: the caller's
//      would have been UNRESOLVED the moment anything called it through the real header.
//
//   2. THE FIELD NAMES WERE SWAPPED, AND THE VALUES HAD BEEN "FIXED" TO COMPENSATE.
//      The fork spelled the bystander record {mbClose, mbTakedown, mbCrash} and the tumbling
//      record {meCameraType, mbTakedown, mbCrash}. The DWARF (and the real homes) have
//      {mbCloseCamera, mbCrashMoment, mbTakedownMoment} and {meSubType, mbCrashMoment,
//      mbTakedownMoment} -- i.e. CRASH IS THE FIRST BOOL, TAKEDOWN THE SECOND, in both.
//      Because the fork had them the other way round, two entries carried FLAG comments
//      announcing that "ARTIST disagrees with our prior value" and flipped the value to match
//      the asm byte. Those FLAGs were the SYMPTOM, not a finding: the bytes were always right,
//      the labels were wrong. Read with the correct names, @0x82209E38's final stores are
//      exactly what each parameter ID is called --
//          BYSTANDER_CLOSE_TAKEDOWN_ONLY  -> close=1  crash=0  takedown=1
//          BYSTANDER_FAR_CRASH_ONLY       -> close=0  crash=1  takedown=0
//          TUMBLING_*_CRASH_ONLY          -> crash=1  takedown=0
//          TUMBLING_*_TAKEDOWN_ONLY       -> crash=0  takedown=1
//      -- which the swapped names made read as nonsense. EVERY BYTE THIS FILE WRITES IS
//      UNCHANGED BY THIS COMMIT; only the names and the types are now the real ones.
// ============================================================================================
//
// X360 LAYOUT (@0x82209E38 store offsets -- provenance only; nothing here casts by them):
//   +0x00 f32   mParamsHardStopDefault.mfDuration            (flt_82004270)
//   +0x04 f32   mParamsHardStopDefault.mfSpeedDiffThreshold  (flt_8200426C)
//   +0x08..0x0A mParamsBystanderCloseTakedownOnly            (3 bools)
//   +0x0B..0x0D mParamsBystanderFarCrashOnly                 (3 bools)
//   +0x10 +0x18 +0x20 +0x28 +0x30 +0x38 +0x40   the seven tumbling records (8 bytes each:
//                                               s32 subtype at +0, bools at +4 / +5)
//   sizeof == 0x48 == 72. The 2-byte hole at +0x0E..+0x0F is the alignment the compiler
//   inserts between the last 3-byte bystander record and the 4-byte-aligned tumbling block;
//   the fork spelled it as an explicit `u8 mPadE[2]` member, which is not needed once the real
//   records are used.
//
// FAITHFUL OMISSION (unchanged from the fork, restated so it is not mistaken for a defect):
// the asm emits each record's `Parameters::Construct()` defaults inline and then immediately
// overwrites EVERY field of that record with the explicit value below -- verified store-for-
// store for all nine records. The defaults are therefore dead stores with no observable
// effect. Calling the real Parameters::Construct() here would add two unresolved externals
// (both are declaration-only, their own ledger functions) and change nothing, so the explicit
// assignment is written directly, as before.
//
// VERIFIED 2026-08-23 (was carried as an unchecked inherited value): the two hard-stop floats
// are READ, not guessed. The IDA export carries no rodata, so the XEX basefile image was
// decrypted/decompressed and the two .rdata slots read directly --
//     flt_82004270 = 3.0f  -> mfDuration            (+0x00)
//     flt_8200426C = 5.0f  -> mfSpeedDiffThreshold  (+0x04)
// which matches what this TU has always shipped. The NAMES are the DWARF's
// (Moments/BrnMomentHardStop.h:117/:118).

#include "types.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Director/MomentController/BrnMomentParameterBank.h"

namespace BrnDirector
{

// The X360 bank is 0x48 bytes with the records at the offsets in the banner above. On this
// host the records are the real ones and the layout is checked by SHAPE, not by displacement.
static_assert(sizeof(MomentHardStop::Parameters) == 8,
              "MomentHardStop::Parameters layout drift");
static_assert(sizeof(MomentBystanderSeesAction::Parameters) == 3,
              "MomentBystanderSeesAction::Parameters layout drift");
static_assert(sizeof(MomentTumbling::Parameters) == 8,
              "MomentTumbling::Parameters layout drift");
static_assert(sizeof(MomentParameterBank) == 72,
              "MomentParameterBank layout drift");

// @0x82209E38. Seed all nine authored records. Store order below is the console's.
void MomentParameterBank::Construct()
{
    mParamsHardStopDefault.mfDuration           = 3.0f;   // +0x00 (flt_82004270)
    mParamsHardStopDefault.mfSpeedDiffThreshold = 5.0f;   // +0x04 (flt_8200426C)

    // +0x08..0x0A -- final stores 1 / 0 / 1.
    mParamsBystanderCloseTakedownOnly.mbCloseCamera    = true;
    mParamsBystanderCloseTakedownOnly.mbCrashMoment    = false;
    mParamsBystanderCloseTakedownOnly.mbTakedownMoment = true;

    // +0x0B..0x0D -- final stores 0 / 1 / 0.
    mParamsBystanderFarCrashOnly.mbCloseCamera    = false;
    mParamsBystanderFarCrashOnly.mbCrashMoment    = true;
    mParamsBystanderFarCrashOnly.mbTakedownMoment = false;

    // +0x10 -- subtype 0, crash only.
    mParamsTumblingTruckingSideCrashOnly.meSubType =
        MomentTumbling::Parameters::E_SUBTYPE_TRUCKING_SIDE;
    mParamsTumblingTruckingSideCrashOnly.mbCrashMoment    = true;
    mParamsTumblingTruckingSideCrashOnly.mbTakedownMoment = false;

    // +0x18 -- subtype 0, takedown only.
    mParamsTumblingTruckingSideTakedownOnly.meSubType =
        MomentTumbling::Parameters::E_SUBTYPE_TRUCKING_SIDE;
    mParamsTumblingTruckingSideTakedownOnly.mbCrashMoment    = false;
    mParamsTumblingTruckingSideTakedownOnly.mbTakedownMoment = true;

    // +0x20 -- subtype 1, crash only.
    mParamsTumblingTruckingFrontCrashOnly.meSubType =
        MomentTumbling::Parameters::E_SUBTYPE_TRUCKING_FRONT;
    mParamsTumblingTruckingFrontCrashOnly.mbCrashMoment    = true;
    mParamsTumblingTruckingFrontCrashOnly.mbTakedownMoment = false;

    // +0x28 -- subtype 2, crash only.
    mParamsTumblingFollowCrashOnly.meSubType =
        MomentTumbling::Parameters::E_SUBTYPE_FOLLOW;
    mParamsTumblingFollowCrashOnly.mbCrashMoment    = true;
    mParamsTumblingFollowCrashOnly.mbTakedownMoment = false;

    // +0x30 -- subtype 3, crash only.
    mParamsTumblingLeadCrashOnly.meSubType =
        MomentTumbling::Parameters::E_SUBTYPE_LEAD;
    mParamsTumblingLeadCrashOnly.mbCrashMoment    = true;
    mParamsTumblingLeadCrashOnly.mbTakedownMoment = false;

    // +0x38 -- subtype 3, takedown only.
    mParamsTumblingLeadTakedownOnly.meSubType =
        MomentTumbling::Parameters::E_SUBTYPE_LEAD;
    mParamsTumblingLeadTakedownOnly.mbCrashMoment    = false;
    mParamsTumblingLeadTakedownOnly.mbTakedownMoment = true;

    // +0x40 -- subtype 4, crash only.
    mParamsTumblingSideCrashOnly.meSubType =
        MomentTumbling::Parameters::E_SUBTYPE_SIDE;
    mParamsTumblingSideCrashOnly.mbCrashMoment    = true;
    mParamsTumblingSideCrashOnly.mbTakedownMoment = false;
}

// @0x821F7428. Return the stored record for an id; NULL for E_PARAM_NONE (which is what every
// moment the roaming state registers asks for, so a NULL Parameters* reaching SetParameters is
// the console's own normal case, not a failure).
Moment::Parameters* MomentParameterBank::GetParameters(EMomentParamID leMomentParamID)
{
    switch (leMomentParamID)
    {
        case E_PARAM_NONE:
            return 0;
        case E_PARAM_HARD_STOP_DEFAULT:
            return &mParamsHardStopDefault;
        case E_PARAM_BYSTANDER_CLOSE_TAKEDOWN_ONLY:
            return &mParamsBystanderCloseTakedownOnly;
        case E_PARAM_BYSTANDER_FAR_CRASH_ONLY:
            return &mParamsBystanderFarCrashOnly;
        case E_PARAM_TUMBLING_TRUCKING_SIDE_CRASH_ONLY:
            return &mParamsTumblingTruckingSideCrashOnly;
        case E_PARAM_TUMBLING_TRUCKING_SIDE_TAKEDOWN_ONLY:
            return &mParamsTumblingTruckingSideTakedownOnly;
        case E_PARAM_TUMBLING_TRUCKING_FRONT_CRASH_ONLY:
            return &mParamsTumblingTruckingFrontCrashOnly;
        case E_PARAM_TUMBLING_FOLLOW_CRASH_ONLY:
            return &mParamsTumblingFollowCrashOnly;
        case E_PARAM_TUMBLING_LEAD_CRASH_ONLY:
            return &mParamsTumblingLeadCrashOnly;
        case E_PARAM_TUMBLING_LEAD_TAKEDOWN_ONLY:
            return &mParamsTumblingLeadTakedownOnly;
        case E_PARAM_TUMBLING_SIDE_CRASH_ONLY:
            return &mParamsTumblingSideCrashOnly;
        default:
            CGS_ASSERT(false, "unhandled parameter type");
            return 0;
    }
}

} // namespace BrnDirector
