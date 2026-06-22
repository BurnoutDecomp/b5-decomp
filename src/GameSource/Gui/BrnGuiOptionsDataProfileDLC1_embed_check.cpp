#include "GameSource/Gui/BrnGuiOptionsDataProfileDLC1.h"

// Compile-time exercise of BrnGui::OptionsDataProfileDLC1::ValidateProfile.
// The X360 body @0x824F0C38 reads miVersion @ +0x00, writes miReserved @ +0x04
// and clears the 8 flag bytes @ +0x08..+0x0F; sizeof pins that footprint.
// (No absolute / cross-pointer-member offset is asserted.)

using BrnGui::OptionsDataProfileDLC1;

static_assert(sizeof(OptionsDataProfileDLC1) == 0x10,
              "DLC1 profile footprint: s32 version + s32 reserved + 8 flag bytes");

bool BrnGuiOptionsDataProfileDLC1_embed_check(OptionsDataProfileDLC1* lpProfile)
{
    return lpProfile->ValidateProfile();
}
