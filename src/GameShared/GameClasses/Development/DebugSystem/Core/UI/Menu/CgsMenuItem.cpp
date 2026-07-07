#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Menu/CgsMenuItem.h"

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

        // --- text helpers used by MenuItemVariable/MenuItemFunction render: menu-render
        // follow-on, dead on the loading-screen boot. Stubbed for link. ---
        void MenuItem::RenderMenuItemText(Debug2DImmediateRender*, const char*, f32, f32, f32, f32, bool, f32) {}
        void MenuItem::ComputeSizeFromText(const char*) {}
    }
}
