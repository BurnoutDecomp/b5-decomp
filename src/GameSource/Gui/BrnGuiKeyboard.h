#pragma once

#include "GameShared/GameClasses/Gui/CgsGuiKeyboard.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

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

    // The wrapper's small members are defined in this header after the class: Prepare's
    // asserts name BrnGuiKeyboard.h:139/140 (ARTIST 0x824EACC0, its COMDAT copy), and ARTIST
    // has no copy of Construct/Release/Destruct/Update/IsShown (inlined at every caller, e.g.
    // GuiModule::Release 0x824F9158's keyboard leg, BrnGuiKeyboard::Show 0x824F5B20).
    inline void BrnGuiKeyboard::Construct()
    {
        // ARTIST825187CC/D0: profile pointer first, then keyboard listener.
        mpSystemUserProfile = 0;
        mKeyboard.Construct();
    }

    inline bool BrnGuiKeyboard::Prepare(CgsGui::SystemUserProfile* lpSystemUserProfile)
    {
        CGS_ASSERT_AT(lpSystemUserProfile != 0, "lpSystemUserProfile",
                      "..\\..\\..\\GameSource\\Gui/BrnGuiKeyboard.h", 139);
        CGS_ASSERT_AT(mpSystemUserProfile == 0, "mpSystemUserProfile == NULL",
                      "..\\..\\..\\GameSource\\Gui/BrnGuiKeyboard.h", 140);
        mpSystemUserProfile = lpSystemUserProfile;
        return true;
    }

    // It does not clear the async listener or fabricate an XDK cancellation.
    inline bool BrnGuiKeyboard::Release()
    {
        mpSystemUserProfile = 0;
        return true;
    }

    // ARTIST825077D4: only the profile owner pointer is cleared here.
    inline void BrnGuiKeyboard::Destruct()
    {
        mpSystemUserProfile = 0;
    }

    // ARTIST8252A08C..A094 inlines the wrapper Update.
    inline void BrnGuiKeyboard::Update()
    {
        mKeyboard.Update();
    }

    inline bool BrnGuiKeyboard::IsShown() const
    {
        return mKeyboard.IsShown();
    }
}
