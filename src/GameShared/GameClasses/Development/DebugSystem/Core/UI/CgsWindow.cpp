#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsWindow.h"

#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"  // GetUI().GetMetrics()

// CgsDev::DebugUI::Window -- the debug-window base. The DecFIGS DWARF declares its ctor + the
// update/render virtual protocol out of line; the X360 bodies have not been reconstructed yet.
//
// FLAG: MINIMAL STUBS FOR LINK (not decompiled). These exist so the DebugUI window hierarchy
// (CustomWindow -> LogWindow), embedded by value in BrnSound::Debug::DebugComponent, links. The
// project already follows this "debug-UI metadata/render virtuals stubbed for link" pattern (see the
// debug-ui-subsystem memory); the real bodies (caption layout, the render protocol) are grown when
// the DebugUI window stack + Debug2DImmediateRender colour pipeline are reconstructed.
// The ctor zero-inits the members by name so a constructed Window is in a defined state.
//
// DECOMPILED (not stubs): IsPinned / TogglePin / ClampToScreen -- see the per-function comments.

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

        // Pin state (X360 CgsWindow.h:194 -- always inlined, so there is no out-of-line address; the
        // test is attested by the two inlined sites: `extrwi r11,r10,1,26` (bit 26 big-endian == the
        // 0x20 mask == KX_FLAGPINNED) inside ScriptCommand_Window @0x82831A64 and SaveState @0x82832DA0.
        bool Window::IsPinned() const { return (mxFlags & KX_FLAGPINNED) != 0; }

        // Flip the pin bit (X360 CgsWindow.h:219, inlined). Attested by `xori r11,r10,0x20` followed by
        // `stw r11,8(r3)` (mxFlags) inside ScriptCommand_Window @0x82831A70-0x82831A74 -- an XOR, not a
        // set: the *WINDOW handler guards it with `if (lbPinned && !IsPinned())`.
        void Window::TogglePin() { mxFlags ^= KX_FLAGPINNED; }

        // Keep the window inside the debug-UI screen rect (X360 0x82817B90, verified instruction-for-
        // instruction against the two sites where ScriptCommand_Window inlines it, 0x828319E4-0x82831A54
        // and 0x82831ACC-0x82831B3C). KX_FLAGNOCLAMPTOSCREEN windows (the Console) opt out entirely --
        // `rlwinm r11,r11,0,22,22` masks bit 22 big-endian == 0x200, then `bnelr`.
        //
        // The clamp pairs are the four fsel instructions; fsel frD,frA,frB,frC picks frB when frA >= 0:
        //   0x82817BD0  fsel f0,f10,f0,f12  -> lower bound: pick (borderLeft - width) when it is >= mfX
        //   0x82817BD8  fsel f0,f12,f0,f13  -> upper bound: keep the value while (screenW - borderR) >= it
        // and the same pair at 0x82817BFC/0x82817C04 for Y with borderTop/mfHeight/screenH/borderBottom.
        //
        // The X360 reaches the metrics by the inlined GetMetrics() singleton walk
        // (`mpInstance` -> +0x140 mpUI -> +0x5C mMetrics; the standalone Window::GetMetrics @0x82816140
        // is literally `return *(mpInstance + 0x140) + 0x5C`). Those are X360 struct offsets, so they are
        // NOT reproduced -- the named GetUI().GetMetrics() accessor is the host-layout-correct form, and
        // the fields below are named members of Metrics rather than the +0x68/0x6C/0x70/0x74/0x78/0x7C
        // words the binary encoded (metrics base 0x5C + 0xC/0x10/0x14/0x18/0x1C/0x20).
        void Window::ClampToScreen()
        {
            if ((mxFlags & KX_FLAGNOCLAMPTOSCREEN) != 0)
                return;

            const Metrics& lrMetrics = GetUI().GetMetrics();

            const f32 lfMinX = lrMetrics.mfScreenBorderLeft - mfWidth;
            const f32 lfMaxX = lrMetrics.mfScreenWidth - lrMetrics.mfScreenBorderRight;
            const f32 lfX    = (mfX > lfMinX) ? mfX : lfMinX;
            mfX = (lfX < lfMaxX) ? lfX : lfMaxX;

            const f32 lfMinY = lrMetrics.mfScreenBorderTop - mfHeight;
            const f32 lfMaxY = lrMetrics.mfScreenHeight - lrMetrics.mfScreenBorderBottom;
            const f32 lfY    = (mfY > lfMinY) ? mfY : lfMinY;
            mfY = (lfY < lfMaxY) ? lfY : lfMaxY;
        }

        // Virtual protocol -- stubbed for link (grow-in).
        void Window::Update(f32 /*lfTimeStep*/, InputEvent /*leEvent*/) {}
        void Window::Render(Debug2DImmediateRender* /*lpRender*/) {}
        void Window::OnGetFocus() {}
        void Window::OnLostFocus() {}
        void Window::GetMenuPath(char* lpcBuffer, s32 liBufferLen) { if (lpcBuffer && liBufferLen > 0) lpcBuffer[0] = 0; }
        void Window::GetSelectedItemString(char* lpcBuffer, s32 liBufferLen) const { if (lpcBuffer && liBufferLen > 0) lpcBuffer[0] = 0; }
    }
}
