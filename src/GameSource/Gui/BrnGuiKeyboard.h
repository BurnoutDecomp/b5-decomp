#pragma once

#include "GameShared/GameClasses/Gui/CgsGuiKeyboard.h"

namespace CgsGui { class SystemUserProfile; }

namespace BrnGui
{
    // DecFIGS BrnGuiKeyboard.h:87/88; ARTIST825187CC/D0 constructs these
    // two real owners. Host placement follows the actual contained keyboard.
    class BrnGuiKeyboard
    {
    public:
        void Construct();
        bool Prepare(CgsGui::SystemUserProfile* lpSystemUserProfile);
        bool Release();
        void Destruct();
        void Update();
        bool IsShown() const;

        // ARTIST824F5B00. CP58 preserves the recovered body as an unmounted
        // source proposal until the actual XShowKeyboardUI backend is available.
        bool Show(const CgsGui::CgsUtf16* lpDefaultText,
                  const CgsGui::CgsUtf16* lpTitleText,
                  const CgsGui::CgsUtf16* lpDescriptionText,
                  CgsGui::GuiKeyboardListener& lrListener);

    private:
        CgsGui::GuiKeyboard mKeyboard;
        CgsGui::SystemUserProfile* mpSystemUserProfile;
    };
}
