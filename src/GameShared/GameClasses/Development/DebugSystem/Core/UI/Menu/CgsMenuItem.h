#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/Internal/CgsDebugInternal.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"  // InputEvent

// CgsDev::DebugUI::MenuItem - the base row of the debug menu (a variable row, a function row, a
// sub-menu row, ...). Holds its computed size + the intrusive list link, and defines the
// update/render/size virtual protocol the menu drives. Recovered from the DecFIGS DWARF
// (Development/DebugSystem/Core/UI/Menu/CgsMenuItem.h). The render context (Debug2DImmediateRender)
// and the shared Palette/Metrics are reached only by pointer/reference, so they stay forward-
// declared.

namespace CgsDev
{
    class Debug2DImmediateRender;

    namespace Internal { template <class T> struct DebugLinkedList; }

    namespace DebugUI
    {
        struct Window;
        struct Palette;
        struct Metrics;

        struct MenuItem : public Internal::DebugInternal
        {
            // The menu threads MenuItems through mpDebugLinkedListNext via DebugLinkedList<MenuItem>.
            friend struct CgsDev::Internal::DebugLinkedList<MenuItem>;

            MenuItem();

            void Prepare();

            virtual void           Update(f32 lfTimeStep, InputEvent leEvent);
            virtual void           Render(Debug2DImmediateRender* lpRender, f32 lfX, f32 lfY, bool lbSelected, f32 lfAlpha);
            virtual void           ComputeSize();
            virtual bool           IsUseful() const;
            virtual bool           IsVisible() const;
            virtual void           GetDisplayName(char* lpcBuffer, s32 liBufferLen) const;
            virtual void           GetItemString(char* lpcBuffer, s32 liBufferLen) const;
            virtual Window*        OpenAsWindow();

            f32  GetWidth() const;
            f32  GetHeight() const;
            void SetWidth(f32 lfWidth);
            void SetHeight(f32 lfHeight);

        protected:
            void RenderMenuItemText(Debug2DImmediateRender* lpRender, const char* lpcText, f32 lfX, f32 lfY, f32 lfWidth, f32 lfHeight, bool lbSelected, f32 lfAlpha);
            void RenderMenuItemBackground(Debug2DImmediateRender* lpRender, f32 lfX, f32 lfY, f32 lfWidth, f32 lfHeight, bool lbSelected, f32 lfAlpha);
            void ComputeSizeFromText(const char* lpcText);

            const Palette&          GetPalette() const;
            const Metrics&          GetMetrics() const;
            Debug2DImmediateRender* Get2DRenderer() const;

            f32 mfWidth;
            f32 mfHeight;

        private:
            MenuItem* mpDebugLinkedListNext;
        };
    }
}
