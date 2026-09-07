#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Menu/CgsMenuItem.h"

#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"

// CgsDev::DebugUI::MenuItem - base ctor + size accessors. The update/render/size virtual protocol is
// the menu-render follow-on; stubbed here so the class vtable links (this code is dead in the
// loading-screen build - no menu is ticked or drawn during loading). GetDisplayName/GetItemString
// write an empty string so a stub call leaves the caller buffer valid.

namespace CgsDev
{
    namespace DebugUI
    {
        MenuItem::MenuItem()
            : mfWidth(0.0f)
            , mfHeight(0.0f)
            , mpDebugLinkedListNext(nullptr)
        {
        }

        // Prepare resets the layout extents + intrusive list link. The X360 inlines these three
        // base stores into every derived ::Prepare (e.g. MenuItemVariableLineGraph::Prepare @0x828167C0
        // does stfs 0->+4/+8, stw 0->+0xC); mpDebugLinkedListNext is private to MenuItem, so the base
        // owns the reset. Not dead here - derived Prepare bodies are reconstructed and depend on it.
        void MenuItem::Prepare()
        {
            mfWidth = 0.0f;
            mfHeight = 0.0f;
            mpDebugLinkedListNext = nullptr;
        }

        f32  MenuItem::GetWidth() const  { return mfWidth; }
        f32  MenuItem::GetHeight() const { return mfHeight; }
        void MenuItem::SetWidth(f32 lfWidth)   { mfWidth = lfWidth; }
        void MenuItem::SetHeight(f32 lfHeight) { mfHeight = lfHeight; }

        // --- render/size virtuals: menu-render follow-on (stubbed for link) ---
        void    MenuItem::Update(f32, InputEvent) {}
        void    MenuItem::Render(Debug2DImmediateRender*, f32, f32, bool, f32) {}
        void    MenuItem::ComputeSize() {}
        bool    MenuItem::IsUseful() const  { return true; }
        bool    MenuItem::IsVisible() const { return true; }
        void    MenuItem::GetDisplayName(char* lpcBuffer, s32 liBufferLen) const { if (liBufferLen > 0) lpcBuffer[0] = '\0'; }
        void    MenuItem::GetItemString(char* lpcBuffer, s32 liBufferLen) const  { if (liBufferLen > 0) lpcBuffer[0] = '\0'; }
        Window* MenuItem::OpenAsWindow() { return nullptr; }

        // X360 0x828294C0. Selected rows get a one-border highlight spanning the window's full
        // content width; text then uses the selected or ordinary palette colour.
        void MenuItem::RenderMenuItemText(Debug2DImmediateRender* lpRender, const char* lpcText,
                                          f32 lfX, f32 lfY, f32 /*lfWidth*/, f32 lfHeight,
                                          bool lbSelected, f32 lfItemWidth)
        {
            const Metrics& lrMetrics = GetMetrics();
            const Palette& lrPalette = GetPalette();
            RGBA lTextColour = lrPalette.mColourText;

            if (lbSelected)
            {
                const f32 lfBorder = lrMetrics.mfWindowBorderSize;
                lpRender->DrawBox(lfX - lfBorder, lfY - lfBorder,
                                  lfItemWidth + lfBorder * 2.0f,
                                  lfHeight + lfBorder * 2.0f,
                                  lrPalette.mColourHighlight);
                lTextColour = lrPalette.mColourHighlightText;
            }

            lpRender->DrawText(lpcText, lfX, lfY, lrMetrics.mfTextSize, lTextColour);
        }

        void MenuItem::RenderMenuItemBackground(Debug2DImmediateRender* lpRender,
                                                f32 lfX, f32 lfY, f32 /*lfWidth*/, f32 lfHeight,
                                                bool lbSelected, f32 lfItemWidth)
        {
            if (!lbSelected)
                return;

            const f32 lfBorder = GetMetrics().mfWindowBorderSize;
            lpRender->DrawBox(lfX - lfBorder, lfY - lfBorder,
                              lfItemWidth + lfBorder * 2.0f,
                              lfHeight + lfBorder * 2.0f,
                              GetPalette().mColourHighlight);
        }

        void MenuItem::ComputeSizeFromText(const char* lpcText)
        {
            const Metrics& lrMetrics = GetMetrics();
            mfWidth = Get2DRenderer()->CalcTextWidth(lpcText, lrMetrics.mfTextSize);
            mfHeight = lrMetrics.mfTextSize;
        }

        const Palette& MenuItem::GetPalette() const
        {
            return DebugManager::GetInstance()->GetUI().GetPalette();
        }

        const Metrics& MenuItem::GetMetrics() const
        {
            return DebugManager::GetInstance()->GetUI().GetMetrics();
        }

        Debug2DImmediateRender* MenuItem::Get2DRenderer() const
        {
            return DebugManager::GetInstance()->GetUI().Get2DRenderer();
        }
    }
}
