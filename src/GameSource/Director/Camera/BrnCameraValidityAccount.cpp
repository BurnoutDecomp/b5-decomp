#include "GameSource/Director/Camera/BrnCameraValidityAccount.h"
#include "GameSource/Director/DirectorModule/BrnDirectorModuleDebugPrinter.h"   // DebugLog::Append

#include <cstring>   // std::strcmp (SetupFailFlagMask's name-table self-check)

// BrnDirector::Camera::ValidityAccount -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (3 ledger functions, class:BrnDirector::Camera::ValidityAccount):
//   ValidityAccount::SetFlag @0x82204028
//   ValidityAccount::Print(DebugPrinter&)
//   ValidityAccount::Print(DebugLog&)

namespace BrnDirector
{
namespace Camera
{

// ----------------------------------------------------------------------------
// KAAC_FLAG_NAMES -- the reason-name table both Print overloads index by flag, RECOVERED
// 2026-09-11 and no longer the thing this file is waiting on. Print walks the raised bits of
// the account and appends KAAC_FLAG_NAMES[bit] for each; SetupFailFlagMask below opens with a
// strcmp of the CONSISTENCY_TEST slot against the literal "CONSISTENCY TEST", which is the
// console's own guard that the table has not drifted out of step with the enum.
//
// The table CROSS-CHECKS the three bands the enum already carries, which is why it can be
// trusted as recovered rather than assumed: entries 0..13 all read "Failed: ..." and the enum's
// failed band is exactly [0, 14); entries 27..30 all read "NoCutFrom: ..." and that band is the
// asm-attested [27, 31). The thirteen entries between them all read "NoCutTo: ...", so the
// no-cut-TO band is [14, 27) -- the one band this header records as unattested. Noted here as
// evidence; no assert is added on it, because that setter's own range check has no asm witness.
// ----------------------------------------------------------------------------
namespace
{
    const char* const KAAC_FLAG_NAMES[32] =
    {
        "Failed: Collision",                          //  0
        "Failed: Visibility",                         //  1
        "Failed: Started off-screen",                 //  2
        "Failed: Couldn't find ground",               //  3
        "Failed: Started in geometry",                //  4
        "Failed: Collision Policy",                   //  5
        "Failed: Couldn't find roadside",             //  6
        "Failed: Position too far from subject",      //  7
        "Failed: ICE movie complete",                 //  8
        "Failed: Started too near geometry",          //  9
        "Failed: Started too near vehicle",           // 10
        "Failed: Invalid vehicle ref",                // 11
        "Failed: Subject left frame",                 // 12
        "Failed: Subject occluded",                   // 13  (last of the failed band)
        "NoCutTo: Finding position",                  // 14
        "NoCutTo: Subject about to leave frame",      // 15
        "NoCutTo: Subject left frame",                // 16
        "NoCutTo: Subject occluded",                  // 17
        "NoCutTo: Moment conditions not met",         // 18
        "NoCutTo: Waiting for behaviour to prepare",  // 19
        "NoCutTo: Waiting to cut to behaviour",       // 20
        "NoCutTo: Opportunity window passed",         // 21
        "NoCutTo: Time limit nearly elapsed",         // 22
        "NoCutTo: Moment inhibited",                  // 23
        "NoCutTo: Behaviour NoCutTo",                 // 24
        "NoCutTo: About to hit geometry",             // 25
        "NoCutTo: About to hit vehicle",              // 26  (last of the no-cut-to band)
        "NoCutFrom: Interpolating",                   // 27
        "NoCutFrom: Money shot!",                     // 28
        "NoCutFrom: ICE movie not finished",          // 29
        "NoCutFrom: Behaviour NoCutFrom",             // 30
        "CONSISTENCY TEST"                            // 31  SetupFailFlagMask's self-check slot
    };

    // The colour every Print line is appended with (the console passes -1).
    const CgsDev::RGBA KU_PRINT_COLOUR = static_cast<CgsDev::RGBA>(-1);

    // The self-check slot's index, named so SetupFailFlagMask reads the way the console reads.
    const u32 KU_FLAG_CONSISTENCY_TEST = 31;
}

// The fail-flag mask pair CameraState::Clear/Construct consume (DWARF
// BrnCameraValidityAccount.h:169/:172; X360 byte_82FAA5EC / qword_82FAA5D0).
// Zero-initialised .bss state until SetupFailFlagMask (below) runs.
bool                         sbFailFlagMaskSet = false;
CgsContainers::BitArray<32u> sFailFlagMask     = {};

// @ 0x82221118 -- one-time mask setup: sanity-check the flag-name table's
// CONSISTENCY_TEST entry, zero the mask, raise bits [0..E_END_FAILED_FLAG), latch
// the set-up flag. The console's opening self-check is now expressible as itself,
// because the table it guards is committed above. The per-iteration bit-index bound assert
// is subsumed by BitArray::SetBit's own guard.
void ValidityAccount::SetupFailFlagMask()
{
    CGS_ASSERT(std::strcmp(KAAC_FLAG_NAMES[KU_FLAG_CONSISTENCY_TEST], "CONSISTENCY TEST") == 0,
               "!strcmp( KAAC_FLAG_NAMES[ CONSISTENCY_TEST ], \"CONSISTENCY TEST\" )");

    sFailFlagMask.UnSetAll();                       // X360 qword_82FAA5D0 = 0
    for (u32 luFlag = 0; luFlag < static_cast<u32>(E_END_FAILED_FLAG); ++luFlag)
    {
        sFailFlagMask.SetBit(luFlag);               // the 1 << flag OR loop (0..13)
    }
    sbFailFlagMaskSet = true;                       // X360 byte_82FAA5EC = 1
}

// Draw one line per raised reason through the on-screen printer. Same walk as
// the DebugLog overload below; the colour is the printer's OWN default (the asm loads r5 from
// the printer's mDebugPrinterInfo.muColour, which is exactly what the one-argument Print
// forwarder does -- BrnDirectorModuleDebugPrinter.h already lists this function as one of the
// eight sites that prove that forwarder's shape). Dev-only, as above.
void ValidityAccount::Print(DebugPrinter& lrDebugPrinter) const
{
    for (s32 liFlag = mFailedFlags.GetFirstNonZeroBit();
         liFlag != CgsContainers::BitArray<32u>::KI_INVALID_BITINDEX;
         liFlag = mFailedFlags.GetNextNonZeroBit(liFlag))
    {
        lrDebugPrinter.Print(KAAC_FLAG_NAMES[liFlag]);
    }
}

// Append one line per raised reason to the scrolling debug log. The console
// body is the BitArray "first set bit / next set bit" walk with DebugLog::ActualAppend(name, -1)
// in the loop; the streamed CgsBitArray "invalid index" assert the asm carries between iterations
// is the container's own guard, which GetNextNonZeroBit already enforces by returning
// KI_INVALID_BITINDEX instead of running off the end. Dev-only: every call site sits behind an
// IsDebugDisplayActive() test that retail never raises.
void ValidityAccount::Print(DebugLog& lrDebugLog) const
{
    for (s32 liFlag = mFailedFlags.GetFirstNonZeroBit();
         liFlag != CgsContainers::BitArray<32u>::KI_INVALID_BITINDEX;
         liFlag = mFailedFlags.GetNextNonZeroBit(liFlag))
    {
        lrDebugLog.Append(KAAC_FLAG_NAMES[liFlag], KU_PRINT_COLOUR);
    }
}

// @ 0x82204028 -- range-check the failure reason (h:219; the streamed
// CgsBitArray.h:222 index guard folded static per convention), then raise its bit
// in the u64-backed set (the X360 inlines the BitArray 64-bit-field SetBit).
void ValidityAccount::SetFlag(s32 leFlag)
{
    CGS_ASSERT(leFlag >= E_FIRST_FAILED_FLAG && leFlag < E_END_FAILED_FLAG,
               "leFlag >= E_FIRST_FAILED_FLAG && leFlag < E_END_FAILED_FLAG");   // h:219 (non-gating)
    CGS_ASSERT(static_cast<u32>(leFlag) < 32u,
               "Index < Number of bits");   // CgsBitArray.h:222 (streamed on the X360; folded static)
    mFailedFlags.SetBit(static_cast<u32>(leFlag));
}

// Inlined in BehaviourHelper::Update @0x82220688 (see the header). The console does the
// whole thing as one 64-bit AND against qword_82FAA5D0; expressed here as the named
// per-bit clear so the u64 field is never reached directly.
void ValidityAccount::MaskToFailFlags()
{
    CGS_ASSERT(sbFailFlagMaskSet, "sbFailFlagMaskSet");   // h:193

    for (u32 luFlag = 0; luFlag < 32u; ++luFlag)
    {
        if (!sFailFlagMask.IsBitSet(luFlag))
        {
            mFailedFlags.UnSetBit(luFlag);
        }
    }
}

// @ 0x82204148 -- the no-cut-FROM twin of SetFlag. Same shape: range-check the reason
// against [E_FIRST_NOCUTFROM_FLAG, E_END_NOCUTFROM_FLAG) (asm `cmpwi 0x1B` / `cmpwi 0x1F`,
// assert text at BrnCameraValidityAccount.h:245), then raise its bit in the same u64 set
// (the X360 inlines the BitArray 64-bit-field SetBit exactly as SetFlag does, with the
// streamed CgsBitArray.h index guard folded static per convention).
// Identified from its single caller, Behaviour::SetCantSwitchFromMeNow @0x82206388, which
// hands it `camera + 0x138` -- this account.
void ValidityAccount::SetNoCutFromFlag(s32 leFlag)
{
    CGS_ASSERT(leFlag >= E_FIRST_NOCUTFROM_FLAG && leFlag < E_END_NOCUTFROM_FLAG,
               "leFlag >= E_FIRST_NOCUTFROM_FLAG && leFlag < E_END_NOCUTFROM_FLAG");  // h:245
    CGS_ASSERT(static_cast<u32>(leFlag) < 32u,
               "Index < Number of bits");   // CgsBitArray.h:222 (streamed on the X360)
    mFailedFlags.SetBit(static_cast<u32>(leFlag));
}

// The no-cut-TO counterpart of the function above. It has no standalone symbol in the
// available ARTIST dumps (inlined at every call site), so what IS attested is only the shape
// its twin proves: raise the caller's reason bit in the same 32-slot set.
// FLAG (band NOT attested): the twin's own band [E_FIRST_NOCUTFROM_FLAG, E_END_NOCUTFROM_FLAG)
// comes from a `cmpwi 0x1B`/`cmpwi 0x1F` pair in @0x82204148. No such pair is available for
// this one, so its band assert is DELIBERATELY OMITTED rather than fabricated -- only the
// container's own index guard (which is universal) is kept. The single caller
// (Behaviour::SetCantSwitchToMeNow) passes the reason straight through, so the bit raised is
// exactly the one the console raises regardless of where the band boundaries sit.
// DELETE-WHEN: the no-cut-TO setter's address/band is identified -- then add its range assert.
void ValidityAccount::SetNoCutToFlag(s32 leFlag)
{
    CGS_ASSERT(static_cast<u32>(leFlag) < 32u,
               "Index < Number of bits");   // CgsBitArray.h:222 (streamed on the X360)
    mFailedFlags.SetBit(static_cast<u32>(leFlag));
}

}
}
