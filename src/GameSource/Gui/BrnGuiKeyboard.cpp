// ===================================================================================
// BrnGui::BrnGuiKeyboard  -- implementation
//   class:BrnGui::BrnGuiKeyboard
//
// Prepare @ 0x824EACC0
//   The GUI module hands the keyboard the system user-profile it will edit. The X360
//   asserts the argument is non-NULL (BrnGuiKeyboard.h line 139) and that the slot is
//   still empty (line 140), then latches it and returns 1.
//   Reconstructed store-for-store from the X360 pseudocode/asm.
// ===================================================================================
#include "GameSource/Gui/BrnGuiKeyboard.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnGui
{
    // @ 0x824EACC0
    s32 BrnGuiKeyboard::Prepare(SystemUserProfile* lpSystemUserProfile)
    {
        CGS_ASSERT(lpSystemUserProfile != 0, "lpSystemUserProfile");            // @0x824EACE0 (cmplwi r30,0)
        CGS_ASSERT(mpSystemUserProfile == 0, "mpSystemUserProfile == NULL");    // @0x824EAD08 (lwz 0x420(r29))

        mpSystemUserProfile = lpSystemUserProfile;                              // stw r30, 0x420(r29)
        return 1;                                                              // li r3, 1
    }
}
