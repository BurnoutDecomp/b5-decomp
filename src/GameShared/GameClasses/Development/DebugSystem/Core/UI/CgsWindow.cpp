#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsWindow.h"

// CgsDev::DebugUI::Window -- the debug-window base. The DecFIGS DWARF declares its ctor + the
// update/render virtual protocol out of line; the X360 bodies have not been reconstructed yet.
//
// FLAG: MINIMAL STUBS FOR LINK (not decompiled). These exist so the DebugUI window hierarchy
// (CustomWindow -> LogWindow), embedded by value in BrnSound::Debug::DebugComponent, links. The
// project already follows this "debug-UI metadata/render virtuals stubbed for link" pattern (see the
// debug-ui-subsystem memory); the real bodies (caption layout, ClampToScreen, the render protocol)
// are grown when the DebugUI window stack + Debug2DImmediateRender colour pipeline are reconstructed.
// The ctor zero-inits the members by name so a constructed Window is in a defined state.

namespace CgsDev
{
    namespace DebugUI
    {
        Window::Window()
            : mpcCaption(0)
            , mxFlags(KX_FLAGNORMAL)
            , mfTransparency(0.0f)
            , mfX(0.0f)
            , mfY(0.0f)
            , mfWidth(0.0f)
            , mfHeight(0.0f)
            , mpDebugLinkedListNext(0)
        {
        }

        // Bring-up: store the caption/flags/size (the observable part of the X360 Prepare). The
        // screen placement/cascade logic is grow-in.
        void Window::Prepare(f32 lfWidth, f32 lfHeight, const char* lpcCaption, s32 lxFlags)
        {
            mpcCaption = lpcCaption;
            mxFlags    = lxFlags;
            mfWidth    = lfWidth;
            mfHeight   = lfHeight;
        }

        // Geometry accessors + setters (the Console slide/size path drives these). GetX/GetY read the
        // stored position; SetSize/SetPosition store the size/position the layout was computed at.
        f32  Window::GetX() const      { return mfX; }
        f32  Window::GetY() const      { return mfY; }
        f32  Window::GetWidth() const  { return mfWidth; }
        f32  Window::GetHeight() const { return mfHeight; }

        void Window::SetSize(f32 lfWidth, f32 lfHeight)   { mfWidth = lfWidth; mfHeight = lfHeight; }
        void Window::SetPosition(f32 lfX, f32 lfY)        { mfX = lfX; mfY = lfY; }

        // The screen-clamp keeps the window inside the screen rect unless KX_FLAGNOCLAMPTOSCREEN is
        // set. The real metric-driven clamp lands with the DebugUI window stack reconstruction; this is
        // the link stub (the Console is prepared NOCLAMPTOSCREEN, so the clamp is a no-op for it).
        void Window::ClampToScreen() {}

        // Virtual protocol -- stubbed for link (grow-in).
        void Window::Update(f32 /*lfTimeStep*/, InputEvent /*leEvent*/) {}
        void Window::Render(Debug2DImmediateRender* /*lpRender*/) {}
        void Window::OnGetFocus() {}
        void Window::OnLostFocus() {}
        void Window::GetMenuPath(char* lpcBuffer, s32 liBufferLen) { if (lpcBuffer && liBufferLen > 0) lpcBuffer[0] = 0; }
        void Window::GetSelectedItemString(char* lpcBuffer, s32 liBufferLen) const { if (lpcBuffer && liBufferLen > 0) lpcBuffer[0] = 0; }
    }
}
