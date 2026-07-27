#include "GameSource/Resource/SharedIO/BrnAssetIds.h"
#include "GameShared/GameClasses/Core/CgsID.h"            // CgsIDCompress
#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::SPrintf
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT

// BrnResource asset-id factories. Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   MakeVehicleId        @ 0x8229FB50
//   MakeTrafficVehicleId @ 0x8274F130
//   MakeTrackUnitId      @ 0x8229FC30
//
// MakeVehicleId / MakeTrafficVehicleId: strlen-bound the asset name, SPrintf the
// prefixed name into a CgsID-sized (13-byte) buffer and compress it. The X360
// streamed-assert message machinery (StrStream into gpcMessageBuffer) is folded
// into the project CGS_ASSERT; the over-length message text is preserved.

namespace BrnResource
{

namespace
{
    // Length of a NUL-terminated string, X360-faithful: count up to (but not
    // including) the terminator. The X360 walked the bytes inline; the bound is on
    // the character count, hence the asset name must be at most 8 chars to leave
    // room for the 4-char prefix in the 13-byte (KI_CGSID_STRING_LEN) buffer.
    u32 NameLength(const char* lpcName)
    {
        const char* lpcCursor = lpcName;
        while (*lpcCursor)
        {
            ++lpcCursor;
        }
        return static_cast<u32>(lpcCursor - lpcName);
    }
}

CgsID MakeVehicleId(const char* lpcName)
{
    CGS_ASSERT(NameLength(lpcName) <= 8, "Vehicle name too long\n");

    char lacBuffer[KI_CGSID_STRING_LEN];
    CgsCore::SPrintf(lacBuffer, KI_CGSID_STRING_LEN, "VEH_%s", lpcName);
    return CgsIDCompress(lacBuffer);
}

CgsID MakeTrafficVehicleId(const char* lpcName)
{
    // The X360 over-length assert streams "Vehicle name too long: <name>" (or
    // "<NULLSTRING>"); the project CGS_ASSERT carries the static lead text.
    CGS_ASSERT(NameLength(lpcName) <= 8, "Vehicle name too long: ");

    char lacBuffer[KI_CGSID_STRING_LEN];
    CgsCore::SPrintf(lacBuffer, KI_CGSID_STRING_LEN, "TVEH%s", lpcName);
    return CgsIDCompress(lacBuffer);
}

CgsID MakeTrackUnitId(u32 luUnitIndex)
{
    // X360: liUnitIndex is read as a signed compare for the lower bound, so the
    // 0x80000000 bit (negative) trips the first assert; the upper bound is 999.
    CGS_ASSERT((luUnitIndex & 0x80000000u) == 0, "liUnitIndex >= 0");
    CGS_ASSERT(luUnitIndex <= 999, "liUnitIndex <= 999");

    // The id is the fixed track-unit base id with the (zero-padded, base-40 decimal)
    // digits of the index folded in. The X360 computes the digit fold in 32-bit
    // signed arithmetic (mullw/mulli + extsw), then adds the 64-bit base CgsID.
    //
    // *** CORRECTION (PVS wave 2026-07-27) *** the base was transcribed with its two
    // halves SWAPPED (0x89568000BEB7A6A5). The asm builds it as
    //   r9 = 0x89568000 ; r8 = 0xBEB7A6A5 ; insrdi r9, r8, 32, 0
    // and `insrdi rA,rS,32,0` writes rS's low word into rA's bits 0..31 -- i.e. the
    // HIGH half -- giving 0xBEB7A6A589568000. That is exactly CgsIDCompress("TRK_UNIT")
    // (verified by running the committed base-40 codec over the string), whereas the
    // swapped form uncompresses to the nonsense "KKS300L32D12" and made every world
    // streaming request fall through GameDataModule::ProcessLoadGameDataEvent's name
    // dispatch into `CGS_ASSERT(false, "Invalid data id: ")`.
    const s64 lnBase = static_cast<s64>(0xBEB7A6A589568000ULL);

    if (luUnitIndex < 10)
    {
        const s32 liFold = static_cast<s32>(64000 * (luUnitIndex + 3));
        return static_cast<CgsID>(static_cast<s64>(liFold) + lnBase);
    }

    if (luUnitIndex >= 100)
    {
        const s32 liFold = static_cast<s32>(
            40
          * (40 * (40 * ((luUnitIndex - (luUnitIndex - luUnitIndex % 10) / 10 % 10 - luUnitIndex % 10) / 100 % 10)
               + (luUnitIndex - luUnitIndex % 10) / 10 % 10)
          + luUnitIndex % 10
          + 4923));
        return static_cast<CgsID>(static_cast<s64>(liFold) + lnBase);
    }

    const s32 liFold = static_cast<s32>(
        1600 * (40 * ((luUnitIndex - luUnitIndex % 10) / 10 % 10) + luUnitIndex % 10 + 123));
    return static_cast<CgsID>(static_cast<s64>(liFold) + lnBase);
}

} // namespace BrnResource
