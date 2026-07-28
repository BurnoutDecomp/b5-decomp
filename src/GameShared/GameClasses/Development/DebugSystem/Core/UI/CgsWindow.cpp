#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsWindow.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                            // CGS_ASSERT
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"              // DebugManager::GetInstance()
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"                // the DebugUI accessor surface
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"  // CalcTextWidth

// CgsDev::DebugUI::Window -- the debug-window base: caption/flags/transparency, the screen rect,
// the intrusive list link, and the update/render virtual protocol the DebugUI window stack drives.
//
// HOW THIS TU REACHES THE DEBUG UI. Every accessor below that needs the UI singleton is compiled on
// the X360 as the same two loads:
//     lis  r11, mpInstance@ha ; lwz r11, mpInstance@l(r11) ; lwz r11, 0x140(r11)
// i.e. the DebugManager global, then the UI it owns. `this` is never read by any of them.
// DebugInternal::GetUI() (X360 0x82815F08) is exactly those two loads, but the DecFIGS DWARF
// declares it NON-const, so the const-qualified accessors here (GetMetrics/GetPalette/
// Get2DRenderer/IsActiveWindow) take the static singleton read instead -- which is literally the
// instruction sequence the binary emits. 0x140 / 0x5C / 0x1C are CONSOLE member offsets and are
// never reproduced: the named accessor chain is the host-layout-correct form.
//
// Still growing in (X360 addresses noted so the next pass can pick them up):
//   Render      0x82828F20 -- needs Debug2DImmediateRender::DrawBox/DrawText (vector arguments)
//                             and ScaleColour, whose X360 RGBA is a by-sret struct rather than the
//                             u32 typedef this branch currently has.
//   ScaleColour 0x828160A8 -- same RGBA blocker; deliberately absent from CgsWindow.h.
//   Update / OnGetFocus / OnLostFocus -- the base bodies are empty in the X360 (no out-of-line
//                             address; every override supplies the behaviour).
//   SetCaption / ApplyMovement -- declared by the DWARF (CgsWindow.h:224 / CgsWindow.cpp:311) but
//                             folded away in the X360 image; no call site in the export pins their
//                             bodies, so they are left unwritten rather than guessed.

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

        // X360 0x8282E4F0. Place the window (cascade unless KX_FLAGNOCASCADE), store the caption /
        // flags / size, clear the transparency and the list link, then widen the window to its
        // caption if the caption is wider than the requested width.
        //
        // The cascade block at 0x8282E524-0x82BE554 is DebugUI::GetCascadePosition inlined: it reads
        // the UI's mfCascadeX/mfCascadeY (DebugUI +0x08/+0x0C) and, when the window HAS a caption,
        // adds mfWindowBorderSize + mfTextSize (metrics +0x04/+0x00) to the Y. Note it tests
        // `8(r31)` -- the window's CURRENT mxFlags, read before the new lxFlags is stored at
        // 0x8282E56C -- which is exactly what passing `this` to GetCascadePosition produces.
        //
        // The caption assert is "mpcCaption" @ CgsWindow.cpp:87 (0x8282E588-0x8282E5B4).
        void Window::Prepare(f32 lfWidth, f32 lfHeight, const char* lpcCaption, s32 lxFlags)
        {
            f32 lfX = 0.0f;
            f32 lfY = 0.0f;
            if ((lxFlags & KX_FLAGNOCASCADE) == 0)
                GetUI().GetCascadePosition(this, lfX, lfY);

            mfX            = lfX;
            mfY            = lfY;
            mpcCaption     = lpcCaption;
            mfWidth        = lfWidth;
            mxFlags        = lxFlags;
            mfHeight       = lfHeight;
            mfTransparency = 0.0f;

            mpDebugLinkedListNext = 0;

            if ((mxFlags & KX_FLAGNOCAPTION) == 0)
            {
                CGS_ASSERT(mpcCaption, "mpcCaption");

                const f32 lfCaptionWidth = CalcCaptionWidth();
                if (mfWidth < lfCaptionWidth)
                    mfWidth = lfCaptionWidth;
            }
        }

        // Geometry accessors (X360 CgsWindow.h:164-179, always inlined).
        f32  Window::GetX() const      { return mfX; }
        f32  Window::GetY() const      { return mfY; }
        f32  Window::GetWidth() const  { return mfWidth; }
        f32  Window::GetHeight() const { return mfHeight; }

        // Caption / flag accessors (X360 CgsWindow.h:184-214, always inlined).
        //
        // GetCaption is attested by CalcCaptionWidth 0x82829224/0x82829264: the value asserted under
        // the expression string "GetCaption()" is the window's +0x04 word == mpcCaption.
        const char* Window::GetCaption() const { return mpcCaption; }
        s32         Window::GetFlags() const   { return mxFlags; }

        // Pin state (X360 CgsWindow.h:194 -- always inlined, so there is no out-of-line address; the
        // test is attested by the two inlined sites: `extrwi r11,r10,1,26` (bit 26 big-endian == the
        // 0x20 mask == KX_FLAGPINNED) inside ScriptCommand_Window @0x82831A64 and SaveState @0x82832DA0.
        bool Window::IsPinned() const { return (mxFlags & KX_FLAGPINNED) != 0; }

        bool Window::IsHidden() const  { return (mxFlags & KX_FLAGHIDDEN) != 0; }
        bool Window::IsEnabled() const { return (mxFlags & KX_FLAGDISABLED) == 0; }

        // KX_FLAGMODAL (0x80) -- attested by DebugUI::SetActiveWindow @0x82822834-0x82822844, which
        // refuses to change the active window while the current one is modal: `lwz r11,8(r4)` then
        // `extrwi r11,r11,1,24` (bit 24 big-endian == 0x80).
        bool Window::IsModal() const { return (mxFlags & KX_FLAGMODAL) != 0; }

        // The window is the UI's active one. X360 Window::Render @0x82828F90-0x82828FA4 inlines this:
        // `lwz r10,0x140(r10)` (the UI) -> `lwz r10,0(r10)` (mpActiveWindow, the UI's first member)
        // -> `cmplw r10,r31` against `this`, choosing the active-caption colour when they match.
        bool Window::IsActiveWindow() const
        {
            return DebugManager::GetInstance()->GetUI().GetActiveWindow() == this;
        }

        // A window can take focus unless it is disabled, hidden, or explicitly refuses focus.
        // The mask is attested twice: DebugUI::SetActiveWindow @0x828227E8 asserts
        // "lpWindow->CanActivate()" (CgsDebugUI.cpp:672) on `andi. r11,r11,0x51`, and
        // DebugUI::GetNextActiveWindow @0x8281AEA0 walks the list skipping windows with those bits.
        // 0x51 == KX_FLAGDISABLED | KX_FLAGHIDDEN | KX_FLAGNOFOCUS.
        bool Window::CanActivate() const
        {
            return (mxFlags & (KX_FLAGDISABLED | KX_FLAGHIDDEN | KX_FLAGNOFOCUS)) == 0;
        }

        // Flip the pin bit (X360 CgsWindow.h:219, inlined). Attested by `xori r11,r10,0x20` followed by
        // `stw r11,8(r3)` (mxFlags) inside ScriptCommand_Window @0x82831A70-0x82831A74 -- an XOR, not a
        // set: the *WINDOW handler guards it with `if (lbPinned && !IsPinned())`.
        void Window::TogglePin() { mxFlags ^= KX_FLAGPINNED; }

        // X360 0x8282E600. Grow the requested width to the caption width, store both, clamp each to
        // the screen extent plus a fixed 100.0f slack, then re-clamp the position to the screen.
        //
        // The two clamps are `fsel` pairs (fsel frD,frA,frB,frC picks frB when frA >= 0):
        //   0x8282E664/0x8282E668  fsubs f11,f12,f13 ; fsel f13,f11,f13,f12  -> min(mfWidth,  limit)
        //   0x8282E67C/0x8282E680  fsubs f12,f13,f0  ; fsel f0,f12,f0,f13    -> min(mfHeight, limit)
        // 100.0f is flt_820049E0 (the same constant LogWindow::Construct @0x8281A1F8 loads).
        void Window::SetSize(f32 lfWidth, f32 lfHeight)
        {
            const Metrics& lrMetrics = GetMetrics();

            const f32 lfCaptionWidth = CalcCaptionWidth();
            if (lfWidth < lfCaptionWidth)
                lfWidth = lfCaptionWidth;

            mfWidth  = lfWidth;
            mfHeight = lfHeight;

            const f32 lfMaxWidth  = lrMetrics.mfScreenWidth + 100.0f;
            const f32 lfMaxHeight = lrMetrics.mfScreenHeight + 100.0f;
            mfWidth  = (mfWidth < lfMaxWidth) ? mfWidth : lfMaxWidth;
            mfHeight = (mfHeight < lfMaxHeight) ? mfHeight : lfMaxHeight;

            ClampToScreen();
        }

        void Window::SetPosition(f32 lfX, f32 lfY) { mfX = lfX; mfY = lfY; }

        // The on-screen footprint: the client rect plus the border on both sides (unless the window
        // has no border) and, for the height, the caption strip on top. Attested by DebugUI::DockWindow
        // @0x82822A70-0x82822ACC, which inlines all three of these for the window it is docking
        // against: `lfs f12,0x18(r3)` + `if (!(flags & 4)) fmadds f12, mfWindowBorderSize, 2.0, f12`
        // for the width, the same for the height at +0x1C, then `fadds f13, <caption height>, f13`.
        f32 Window::CalcScreenWidth()
        {
            f32 lfWidth = mfWidth;
            if ((mxFlags & KX_FLAGNOBORDER) == 0)
                lfWidth += GetMetrics().mfWindowBorderSize * 2.0f;
            return lfWidth;
        }

        f32 Window::CalcScreenHeight()
        {
            f32 lfHeight = mfHeight;
            if ((mxFlags & KX_FLAGNOBORDER) == 0)
                lfHeight += GetMetrics().mfWindowBorderSize * 2.0f;
            return lfHeight + CalcCaptionHeight();
        }

        // X360 0x828291F0. A caption-less window measures zero; otherwise the caption text is measured
        // through the debug 2D renderer at the metrics text size (`lfs f1,0x5C(r31)` == the UI's
        // Metrics sub-object +0x00 == mfTextSize). The assert is "GetCaption()" @ CgsWindow.cpp:483.
        f32 Window::CalcCaptionWidth()
        {
            if ((mxFlags & KX_FLAGNOCAPTION) != 0)
                return 0.0f;

            CGS_ASSERT(GetCaption(), "GetCaption()");

            return Get2DRenderer()->CalcTextWidth(GetCaption(), GetMetrics().mfTextSize);
        }

        // The caption strip: the text line plus a border above and below, or nothing at all when the
        // window is caption-less. Attested by DockWindow @0x82822AA8-0x82822ACC -- `rlwinm r11,r10,0,30,30`
        // (the 0x02 KX_FLAGNOCAPTION bit) selecting between 0.0f and
        // `fmadds f0, mfWindowBorderSize, 2.0, mfTextSize`.
        f32 Window::CalcCaptionHeight()
        {
            if ((mxFlags & KX_FLAGNOCAPTION) != 0)
                return 0.0f;

            const Metrics& lrMetrics = GetMetrics();
            return lrMetrics.mfTextSize + (lrMetrics.mfWindowBorderSize * 2.0f);
        }

        // Keep the window inside the debug-UI screen rect (X360 0x82817B90, verified instruction-for-
        // instruction against the two sites where ScriptCommand_Window inlines it, 0x828319E4-0x82831A54
        // and 0x82831ACC-0x82831B3C). KX_FLAGNOCLAMPTOSCREEN windows (the Console) opt out entirely --
        // `rlwinm r11,r11,0,22,22` masks bit 22 big-endian == 0x200, then `bnelr`.
        //
        // The clamp pairs are the four fsel instructions; fsel frD,frA,frB,frC picks frB when frA >= 0:
        //   0x82817BD0  fsel f0,f10,f0,f12  -> lower bound: pick (borderLeft - width) when it is >= mfX
        //   0x82817BD8  fsel f0,f12,f0,f13  -> upper bound: keep the value while (screenW - borderR) >= it
        // and the same pair at 0x82817BFC/0x82817C04 for Y with borderTop/mfHeight/screenH/borderBottom.
        void Window::ClampToScreen()
        {
            if ((mxFlags & KX_FLAGNOCLAMPTOSCREEN) != 0)
                return;

            const Metrics& lrMetrics = GetMetrics();

            const f32 lfMinX = lrMetrics.mfScreenBorderLeft - mfWidth;
            const f32 lfMaxX = lrMetrics.mfScreenWidth - lrMetrics.mfScreenBorderRight;
            const f32 lfX    = (mfX > lfMinX) ? mfX : lfMinX;
            mfX = (lfX < lfMaxX) ? lfX : lfMaxX;

            const f32 lfMinY = lrMetrics.mfScreenBorderTop - mfHeight;
            const f32 lfMaxY = lrMetrics.mfScreenHeight - lrMetrics.mfScreenBorderBottom;
            const f32 lfY    = (mfY > lfMinY) ? mfY : lfMinY;
            mfY = (lfY < lfMaxY) ? lfY : lfMaxY;
        }

        // The palette has no out-of-line address -- the X360 inlines it at every use. Window::Render
        // @0x82828F58-0x82828FB0 reads the colour slots straight out of the UI singleton
        // (`mpInstance -> +0x140 -> +0x1C/+0x20/+0x24/+0x28/+0x2C/+0x38/+0x3C/+0x40/+0x44`), i.e. the
        // palette is a sub-object of the DebugUI exactly like the metrics are, reached by the same
        // singleton read GetMetrics uses.
        const Palette& Window::GetPalette() const
        {
            return DebugManager::GetInstance()->GetUI().GetPalette();
        }

        // X360 0x82816140 -- three instructions: `mpInstance` -> `+0x140` (the UI) -> `addi r3,r11,0x5C`
        // (the address of its Metrics sub-object).
        const Metrics& Window::GetMetrics() const
        {
            return DebugManager::GetInstance()->GetUI().GetMetrics();
        }

        // Attested by CalcCaptionWidth @0x82829260-0x82829274: the same singleton read, then a call to
        // DebugUI::Get2DRenderer (0x828221A8), which asserts "mp2dRender != NULL" and returns it.
        Debug2DImmediateRender* Window::Get2DRenderer() const
        {
            return DebugManager::GetInstance()->GetUI().Get2DRenderer();
        }

        // The base virtuals. Update/OnGetFocus/OnLostFocus are empty in the X360 image (no out-of-line
        // address survives; every derived window supplies the behaviour). Render 0x82828F20 has a real
        // body -- see the file header for why it is not written yet.
        void Window::Update(f32 /*lfTimeStep*/, InputEvent /*leEvent*/) {}
        void Window::Render(Debug2DImmediateRender* /*lpRender*/) {}
        void Window::OnGetFocus() {}
        void Window::OnLostFocus() {}

        // X360 0x82816158 -- the whole function is `li r11,0 ; stb r11,0(r4) ; blr`: the base window
        // has no menu path, so it returns the empty string and ignores the buffer length. There is no
        // null/length guard in the binary. The real paths come from the overrides
        // (MenuWindow::GetMenuPath 0x828164F0, CustomWindow::GetMenuPath 0x8281A0F8).
        void Window::GetMenuPath(char* lpcBuffer, s32 /*liBufferLen*/)
        {
            lpcBuffer[0] = 0;
        }

        // No separate X360 address: the body is byte-identical to GetMenuPath above (write the
        // terminator, return), so the linker's identical-code folding collapsed the two and IDA kept
        // only the GetMenuPath name at 0x82816158. MenuWindow::GetSelectedItemString (0x82816560)
        // is the override that produces a real string.
        void Window::GetSelectedItemString(char* lpcBuffer, s32 /*liBufferLen*/) const
        {
            lpcBuffer[0] = 0;
        }
    }
}
