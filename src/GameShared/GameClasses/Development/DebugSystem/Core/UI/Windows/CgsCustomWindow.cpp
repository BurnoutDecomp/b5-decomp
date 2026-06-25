#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Windows/CgsCustomWindow.h"

// CgsDev::DebugUI::CustomWindow + CustomWindowMenuItem -- a debug window that also exposes itself as a
// menu row. The DecFIGS DWARF declares these methods out of line; the X360 bodies are not yet
// reconstructed.
//
// FLAG: MINIMAL STUBS FOR LINK (not decompiled). Present so LogWindow (embedded by value in
// BrnSound::Debug::DebugComponent) links. Same grow-in pattern as CgsWindow.cpp -- the real
// menu-registration + render/layout bodies land with the DebugUI window stack reconstruction.

namespace CgsDev
{
    namespace DebugUI
    {
        // --- CustomWindowMenuItem ---
        void CustomWindowMenuItem::Prepare(Window* lpWindow) { mpWindow = lpWindow; }

        void CustomWindowMenuItem::Update(f32 /*lfTimeStep*/, InputEvent /*leEvent*/) {}
        void CustomWindowMenuItem::Render(Debug2DImmediateRender* /*lpRender*/, f32 /*lfX*/, f32 /*lfY*/, bool /*lbSelected*/, f32 /*lfAlpha*/) {}
        void CustomWindowMenuItem::ComputeSize() {}
        bool CustomWindowMenuItem::IsUseful() const { return mpWindow != 0; }
        void CustomWindowMenuItem::GetDisplayName(char* lpcBuffer, s32 liBufferLen) const { if (lpcBuffer && liBufferLen > 0) lpcBuffer[0] = 0; }
        Window* CustomWindowMenuItem::OpenAsWindow() { return mpWindow; }

        // --- CustomWindow ---
        void CustomWindow::Prepare(f32 lfWidth, f32 lfHeight, const char* lpcCaption, const char* /*lpcMenuPath*/, s32 lxFlags)
        {
            // Bring up the underlying window, then point the menu row back at this window.
            Window::Prepare(lfWidth, lfHeight, lpcCaption, lxFlags);
            mMenuItem.Prepare(this);
        }

        void CustomWindow::Update(f32 /*lfTimeStep*/, InputEvent /*leEvent*/) {}
        void CustomWindow::Register(const char* /*lpcMenuPath*/) {}
        void CustomWindow::Unregister() {}
        void CustomWindow::GetMenuPath(char* lpcBuffer, s32 liBufferLen) { if (lpcBuffer && liBufferLen > 0) lpcBuffer[0] = 0; }
    }
}
