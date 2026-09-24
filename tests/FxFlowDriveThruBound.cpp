// FX-FLOW (crash parity 2026-09-24, reviewer C item): the PRODUCTION drive-thru constants
// (every `static const f32 KF_*` of src/GameSource/GameState/Offences/BrnDriveThruManager.cpp, in
// file order) and the PRODUCTION re-arm statement of DriveThruManager::HandleDriveThru, extracted by
// run_fxflow_drive_thru_bound.py.
//
// Checked against the ARTIST image:
//   flt_82FADEC8 (.bss) is written once, by the TU's dynamic initialiser @0x82C4D730..0x82C4D748:
//       lfs f0, flt_82CDBD90 (0x40066666 = 2.1f) ; lfs f13, flt_82001C98 (0x3F800000 = 1.0f)
//       fsubs f0, f0, f13 ; stfs f0, flt_82FADEC8        -> 0x3F8CCCCC (1.0999999f)
//   HandleDriveThru @0x8239B690..0x8239B6BC: timer < flt_82FADEC8 && type != 0 && timer < 1.0f
//       -> timer = 1.0f (`stfs f13, 0x20(r19)`).
#include "types.hpp"
#include <cstdio>
#include <cstring>

namespace fixture
{
#include "drive_thru_constants.inc"

struct DriveThruTriggerData
{
    f32 mfTimeToActiveation;
};

// The production statement, verbatim, in a function that supplies its two names.
static void ReArmOnEntry(DriveThruTriggerData& lrEntry, s32 leType)
{
#include "drive_thru_rearm.inc"
}
}

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

static u32 Bits(f32 lfValue)
{
    u32 luBits;
    std::memcpy(&luBits, &lfValue, sizeof(luBits));
    return luBits;
}

static f32 AfterEntry(f32 lfTimer, s32 leType)
{
    fixture::DriveThruTriggerData lEntry;
    lEntry.mfTimeToActiveation = lfTimer;
    fixture::ReArmOnEntry(lEntry, leType);
    return lEntry.mfTimeToActiveation;
}

int main()
{
    Check(Bits(fixture::KF_DRIVE_THRU_JUST_ARMED_BOUND) == 0x3F8CCCCCu,
          "KF_DRIVE_THRU_JUST_ARMED_BOUND == 0x3F8CCCCC (flt_82FADEC8 = 2.1f - 1.0f, fsubs @0x82C4D744)");
    Check(Bits(fixture::KF_DRIVE_THRU_ACTIVATION_TIME) == 0x40066666u &&
          Bits(fixture::KF_DRIVE_THRU_REARM_TIME) == 0x3F800000u,
          "the initialiser's operands: flt_82CDBD90 == 0x40066666, flt_82001C98 == 0x3F800000");
    Check(AfterEntry(0.5f, 3) == 1.0f,
          "paint shop re-entered with 0.5 s left: pushed back up to 1.0 (0.5 < 1.1 && 0.5 < 1.0)");
    Check(AfterEntry(0.0f, 1) == 1.0f,
          "gas station re-entered at exactly 0.0: pushed back up to 1.0 (0.0 < 1.1)");
    Check(AfterEntry(0.99f, 2) == 1.0f,
          "body shop re-entered with 0.99 s left: pushed back up to 1.0");
    Check(AfterEntry(1.05f, 1) == 1.05f,
          "1.05 s left is not below 1.0: unchanged (the `timer < 1.0f` leg @0x8239B6B4)");
    Check(AfterEntry(0.5f, 0) == 0.5f,
          "a junkyard (type 0) is never re-armed (`cmpwi r17, 0 ; beq` @0x8239B6A4)");
    Check(AfterEntry(-0.5f, 1) == 1.0f,
          "a lapsed (negative) countdown is re-armed to 1.0 as well");

    std::printf("FxFlowDriveThruBound: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
