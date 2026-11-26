// ============================================================================
// b5-decomp/src/GameSource/Network/SharedIO/BrnNetworkOutScoreboardHeadingList.cpp
// ============================================================================
// BrnNetwork::BrnNetworkModuleIO::NetworkOutScoreboardHeadingList::AddHeading @ 0x82541EB0.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (no reference / no DWARF). The X360 body:
//   * inline strlen(lpcHeading); assert it is < 31 (KI_MAX_CAT_VAR_INDEX_NAME_LENGTH).
//   * assert miLength >= 0 and miLength < 66 (KI_MAX_CAT_VAR_INDEX_COUNT).
//   * compute the destination slot = base + 8 + miLength*31 (asm: r9 = len<<5, r11 = r9 - len
//     = len*31, + base, + 8) and post-increment miLength (stw len+1, 0(base)).
//   * a SECOND inline strlen + the shared "String too long: <name>" CgsStringUtils.h assert
//     (the inlined CgsStringUtils copy-string-with-length-check helper).
//   * strncpy(slot, lpcHeading, 31).
// The three guards keep ARTIST's locations: they name BrnNetworkOutEventTypeDefs.h, lines
// 1742-1744, the header where DWARF declares the class (AddHeading at its line 1111). The
// length guards, the slot arithmetic (offset 8, stride 31), the post-increment and the bounded
// 31-byte copy are reproduced exactly. The Dest/Source/Count register setup at 0x82542050
// (r3=slot, r4=name, r5=0x1F) confirms the strncpy operands.
#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::StrnCpy
#include "GameSource/Network/SharedIO/BrnNetworkOutScoreboardHeadingList.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cstring>  // std::strlen / std::strncpy
#define CGS_ARTIST_TYPEDEFS_FILE "..\\..\\..\\GameSource\\Network/BrnNetworkOutEventTypeDefs.h"
namespace BrnNetwork
{
namespace BrnNetworkModuleIO
{
    void NetworkOutScoreboardHeadingList::AddHeading(const char* lpcHeading)
    {
        // strlen(lpcHeading) < KI_MAX_CAT_VAR_INDEX_NAME_LENGTH (first guard, 0x82541EE8).
        CGS_ASSERT_AT(std::strlen(lpcHeading) < static_cast<u32>(KI_MAX_CAT_VAR_INDEX_NAME_LENGTH),
                   "strlen(lpcHeading) < static_cast<uint32_t>(CgsNetwork::KI_MAX_CAT_VAR_INDEX_NAME_LENGTH)", CGS_ARTIST_TYPEDEFS_FILE, 1742);

        // 0 <= miLength (0x82541F14) and miLength < KI_MAX_CAT_VAR_INDEX_COUNT (0x82541F3C).
        CGS_ASSERT_AT(miLength >= 0, "miLength >= 0", CGS_ARTIST_TYPEDEFS_FILE, 1743);
        CGS_ASSERT_AT(miLength < KI_MAX_CAT_VAR_INDEX_COUNT, "miLength < CgsNetwork::KI_MAX_CAT_VAR_INDEX_COUNT", CGS_ARTIST_TYPEDEFS_FILE, 1744);

        // Destination slot = &maHeadings[miLength] (base + 8 + miLength*31), then post-increment
        // miLength -- exactly the X360 r27 = base + 8 + len*31 / stw len+1, 0(base) sequence.
        char* lpcSlot = maHeadings[miLength];
        ++miLength;

        // CgsCore::StrnCpy (CgsStringUtils.h:55; ARTIST 0x82542048 asserts at that line).
        CgsCore::StrnCpy(lpcSlot, lpcHeading, KI_MAX_CAT_VAR_INDEX_NAME_LENGTH);
    }

    // GetHeadingType (X360 0x823A62F8) is defined in the header.
}
}
