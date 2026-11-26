#include "GameSource/Gui/BrnGuiKeyboard.h"
#include "GameShared/GameClasses/Gui/CgsGuideIntegration.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
namespace BrnGui
{
    // ARTIST824F5B00. Keep this wrapper unmounted until XShowKeyboardUI has its
    // real platform provider; its true result does not prove a dialog opened.
    bool BrnGuiKeyboard::Show(const CgsGui::CgsUtf16* lpDefaultText,
        const CgsGui::CgsUtf16* lpTitleText, const CgsGui::CgsUtf16* lpDescriptionText,
        CgsGui::GuiKeyboardListener& lrListener)
    {
        // The shown arm returns inside the if; the shared `return false` is ARTIST's
        // out-of-line tail (0x824F5B28 bne to 0x824F5B84).
        if (!mKeyboard.IsShown())
        {
            if (mpSystemUserProfile == 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("mpSystemUserProfile", CGS_ASSERT_LOCATION("..\\..\\..\\GameSource\\Gui/BrnGuiKeyboard.cpp", 47));
                CgsDev::Assert::EndAssert();
            }
            mKeyboard.Show(mpSystemUserProfile->GetUserIndex(), lpDefaultText,
                lpTitleText, lpDescriptionText, lrListener);
            return true;
        }
        return false;
    }
}

