#include "GameSource/Gui/BrnGuiKeyboard.h"
#include "GameShared/GameClasses/Gui/CgsGuideIntegration.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnGui
{
    void BrnGuiKeyboard::Construct()
    {
        // ARTIST825187CC/D0: profile pointer first, then keyboard listener.
        mpSystemUserProfile = 0;
        mKeyboard.Construct();
    }

    // ARTIST824EACC0: original assertions continue into the actual store.
    bool BrnGuiKeyboard::Prepare(CgsGui::SystemUserProfile* lpSystemUserProfile)
    {
        if (lpSystemUserProfile == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpSystemUserProfile", "..\\..\\..\\GameSource\\Gui/BrnGuiKeyboard.h", 139);
            CgsDev::Assert::EndAssert();
        }
        if (mpSystemUserProfile != 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("mpSystemUserProfile == NULL", "..\\..\\..\\GameSource\\Gui/BrnGuiKeyboard.h", 140);
            CgsDev::Assert::EndAssert();
        }
        mpSystemUserProfile = lpSystemUserProfile;
        return true;
    }

    // Inlined in ARTIST824F9158's keyboard release leg. It does not clear
    // the async listener or fabricate an XDK cancellation.
    bool BrnGuiKeyboard::Release()
    {
#if defined(_WIN32) && !defined(_XBOX)
        // FLAG PC-platform leaf: retire native work before its title owner is released.
        mKeyboard.AbandonPlatformOperation();
#endif
        mpSystemUserProfile = 0;
        return true;
    }

    // ARTIST825077D4: only the profile owner pointer is cleared here.
    void BrnGuiKeyboard::Destruct()
    {
#if defined(_WIN32) && !defined(_XBOX)
        // FLAG PC-platform leaf: also covers Destruct without a prior Release.
        mKeyboard.AbandonPlatformOperation();
#endif
        mpSystemUserProfile = 0;
    }

    // ARTIST8252A08C..A094 inlines the wrapper Update.
    void BrnGuiKeyboard::Update()
    {
        mKeyboard.Update();
    }

    bool BrnGuiKeyboard::IsShown() const
    {
        return mKeyboard.IsShown();
    }

    // ARTIST824F5B00. Keep this wrapper unmounted until XShowKeyboardUI has its
    // real platform provider; its true result does not prove a dialog opened.
    bool BrnGuiKeyboard::Show(const CgsGui::CgsUtf16* lpDefaultText,
        const CgsGui::CgsUtf16* lpTitleText, const CgsGui::CgsUtf16* lpDescriptionText,
        CgsGui::GuiKeyboardListener& lrListener)
    {
        if (mKeyboard.IsShown())
            return false;
        if (mpSystemUserProfile == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("mpSystemUserProfile", "..\\..\\..\\GameSource\\Gui/BrnGuiKeyboard.cpp", 47);
            CgsDev::Assert::EndAssert();
        }
        mKeyboard.Show(mpSystemUserProfile->GetUserIndex(), lpDefaultText,
            lpTitleText, lpDescriptionText, lrListener);
        return true;
    }
}
