#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Menu/CgsMenu.h"

// CgsDev::DebugUI::Menu - the menu-path bodies: Prepare initialises a node (caption + parent +
// empty item list), AddMenuItem appends a row and makes it current if none is (matching the X360
// MenuManager register inline: mMenuItems.Add(item) then "if no current, set current"), and the
// parent/caption accessors back MenuManager::FindSubMenu. The display/navigation surface
// (Update/Render/Select*/GetPath/...) is the menu-render follow-on.

namespace CgsDev
{
    namespace DebugUI
    {
        Menu::Menu()
            : mpParent(nullptr)
            , mpCurrentMenuItem(nullptr)
        {
            macCaption[0] = '\0';
            mMenuItems.Clear();
        }

        void Menu::Prepare(const char* lpcCaption, Menu* lpParent)
        {
            s32 liIndex = 0;
            if (lpcCaption)
                for (; lpcCaption[liIndex] && liIndex < KI_MAXMENUNAME - 1; ++liIndex)
                    macCaption[liIndex] = lpcCaption[liIndex];
            macCaption[liIndex] = '\0';

            mpParent          = lpParent;
            mpCurrentMenuItem = nullptr;
            mMenuItems.Clear();
        }

        void Menu::AddMenuItem(MenuItem* lpMenuItem)
        {
            mMenuItems.Add(lpMenuItem);
            if (!mpCurrentMenuItem)
                mpCurrentMenuItem = lpMenuItem;
        }

        const char* Menu::GetCaption() const { return macCaption; }
        Menu*       Menu::GetParent() const  { return mpParent; }

        // --- render/size virtuals: menu-render follow-on (stubbed for link; dead in the loading build) ---
        void    Menu::Update(f32, InputEvent) {}
        void    Menu::Render(Debug2DImmediateRender*, f32, f32, bool, f32) {}
        void    Menu::ComputeSize() {}
        void    Menu::GetDisplayName(char* lpcBuffer, s32 liBufferLen) const { if (liBufferLen > 0) lpcBuffer[0] = '\0'; }
        Window* Menu::OpenAsWindow() { return nullptr; }
    }
}
