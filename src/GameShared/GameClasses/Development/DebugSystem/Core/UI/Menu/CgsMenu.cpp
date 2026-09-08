#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Menu/CgsMenu.h"

#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Menu/CgsMenuManager.h"

#include <string.h>

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

        void Menu::Update(f32 /*lfTimeStep*/, InputEvent leEvent)
        {
            if (leEvent != E_INPUTEVENT_SELECT)
                return;

            if (mpParent && !mpParent->IsMenuUseful())
                GetUI().GetMenuManager().Close(mpParent);
            OpenAsWindow();
        }

        void Menu::Render(Debug2DImmediateRender* lpRender, f32 lfX, f32 lfY,
                          bool lbSelected, f32 lfItemWidth)
        {
            RenderMenuItemText(lpRender, macCaption, lfX, lfY,
                               mfWidth, mfHeight, lbSelected, lfItemWidth);
        }

        void Menu::ComputeSize()
        {
            ComputeSizeFromText(macCaption);
        }

        void Menu::GetDisplayName(char* lpcBuffer, s32 liBufferLen) const
        {
            if (liBufferLen <= 1)
            {
                lpcBuffer[0] = '\0';
                return;
            }
            strncpy(lpcBuffer, macCaption, static_cast<size_t>(liBufferLen - 1));
            lpcBuffer[liBufferLen - 1] = '\0';
        }

        Window* Menu::OpenAsWindow()
        {
            return GetUI().GetMenuManager().Open(this);
        }

        void Menu::GetPath(char* lpcBuffer, s32 liBufferLen)
        {
            if (!mpParent)
            {
                lpcBuffer[0] = '\0';
                return;
            }
            mpParent->GetPath(lpcBuffer, liBufferLen);
            GetUI().SafeStringCat(lpcBuffer, "/", liBufferLen);
            GetUI().SafeStringCat(lpcBuffer, macCaption, liBufferLen);
        }

        void Menu::RemoveMenuItem(MenuItem* lpMenuItem)
        {
            if (mpCurrentMenuItem == lpMenuItem)
            {
                mpCurrentMenuItem = mMenuItems.GetNextWrap(lpMenuItem);
                if (mpCurrentMenuItem == lpMenuItem)
                    mpCurrentMenuItem = nullptr;
            }
            mMenuItems.Remove(lpMenuItem);
        }

        bool Menu::IsMenuItemAdded(const MenuItem* lpMenuItem) const { return mMenuItems.IsAdded(lpMenuItem); }
        bool Menu::IsEmpty() const { return mMenuItems.IsEmpty(); }

        void Menu::ReplaceMenuItem(MenuItem* lpOld, MenuItem* lpNew)
        {
            mMenuItems.Replace(lpOld, lpNew);
            if (mpCurrentMenuItem == lpOld)
                mpCurrentMenuItem = lpNew;
        }

        void Menu::AddMenuItemAfter(MenuItem* lpExisting, MenuItem* lpMenuItem)
        {
            mMenuItems.AddAfter(lpExisting, lpMenuItem);
        }

        MenuItem* Menu::SelectNextMenuItem()
        {
            if (!mpCurrentMenuItem)
                mpCurrentMenuItem = mMenuItems.GetFirst();
            if (!mpCurrentMenuItem)
                return nullptr;

            MenuItem* lpCandidate = mpCurrentMenuItem;
            do
            {
                lpCandidate = mMenuItems.GetNextWrap(lpCandidate);
                if (lpCandidate->IsVisible())
                    return mpCurrentMenuItem = lpCandidate;
            } while (lpCandidate != mpCurrentMenuItem);
            return mpCurrentMenuItem;
        }

        MenuItem* Menu::SelectPreviousMenuItem()
        {
            if (!mpCurrentMenuItem)
                mpCurrentMenuItem = mMenuItems.GetLast();
            if (!mpCurrentMenuItem)
                return nullptr;

            MenuItem* lpCandidate = mpCurrentMenuItem;
            do
            {
                lpCandidate = mMenuItems.GetPreviousWrap(lpCandidate);
                if (lpCandidate->IsVisible())
                    return mpCurrentMenuItem = lpCandidate;
            } while (lpCandidate != mpCurrentMenuItem);
            return mpCurrentMenuItem;
        }

        bool Menu::IsMenuUseful() const
        {
            for (MenuItem* lpItem = mMenuItems.GetFirst(); lpItem; lpItem = mMenuItems.GetNext(lpItem))
                if (lpItem->IsUseful())
                    return true;
            return false;
        }

        MenuItem* Menu::FindMenuItemByName(const char* lpcName)
        {
            char acName[256];
            for (MenuItem* lpItem = mMenuItems.GetFirst(); lpItem; lpItem = mMenuItems.GetNext(lpItem))
            {
                lpItem->GetDisplayName(acName, sizeof(acName));
                if (_stricmp(acName, lpcName) == 0)
                    return lpItem;
            }
            return nullptr;
        }

        void Menu::GetSelectedItemString(char* lpcBuffer, s32 liBufferLen) const
        {
            if (mpCurrentMenuItem)
                mpCurrentMenuItem->GetItemString(lpcBuffer, liBufferLen);
            else
                lpcBuffer[0] = '\0';
        }
    }
}
