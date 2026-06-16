#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Menu/CgsMenuItem.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugCollections.h"  // DebugLinkedList<MenuItem>

// CgsDev::DebugUI::Menu - a sub-menu row that is itself a list of MenuItems (so menus nest). Holds
// its caption, parent link, current selection, and the child-item list. Recovered from the DecFIGS
// DWARF (Development/DebugSystem/Core/UI/Menu/CgsMenu.h). MenuManager pools these.

namespace CgsDev
{
    class Debug2DImmediateRender;

    namespace DebugUI
    {
        struct Menu : public MenuItem
        {
            static const s32 KI_MAXMENUNAME = 25;

            Menu();

            void Prepare(const char* lpcCaption, Menu* lpParent);

            virtual void    Update(f32 lfTimeStep, InputEvent leEvent);
            virtual void    Render(Debug2DImmediateRender* lpRender, f32 lfX, f32 lfY, bool lbSelected, f32 lfAlpha);
            virtual void    ComputeSize();
            virtual void    GetDisplayName(char* lpcBuffer, s32 liBufferLen) const;
            virtual Window* OpenAsWindow();

            const char* GetCaption() const;
            Menu*       GetParent() const;
            void        GetPath(char* lpcBuffer, s32 liBufferLen);

            void      AddMenuItem(MenuItem* lpMenuItem);
            void      RemoveMenuItem(MenuItem* lpMenuItem);
            bool      IsMenuItemAdded(const MenuItem* lpMenuItem) const;
            bool      IsEmpty() const;
            void      ReplaceMenuItem(MenuItem* lpOld, MenuItem* lpNew);
            void      AddMenuItemAfter(MenuItem* lpExisting, MenuItem* lpMenuItem);
            MenuItem* SelectNextMenuItem();
            MenuItem* SelectPreviousMenuItem();
            bool      IsMenuUseful() const;
            MenuItem* FindMenuItemByName(const char* lpcName);
            void      GetSelectedItemString(char* lpcBuffer, s32 liBufferLen) const;

        protected:
            char                              macCaption[KI_MAXMENUNAME];
            Menu*                             mpParent;
            MenuItem*                         mpCurrentMenuItem;
            Internal::DebugLinkedList<MenuItem> mMenuItems;
        };
    }
}
