#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/Internal/CgsDebugInternal.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"  // InputEvent

// CgsDev::DebugUI::Window - a floating debug window (a docked menu, the console, an error box, ...).
// Holds its caption, flags, transparency, screen rect, and the intrusive list link, and defines the
// update/render virtual protocol the DebugUI window stack drives. Recovered from the DecFIGS DWARF
// (Development/DebugSystem/Core/UI/CgsWindow.h). Render context + Palette/Metrics reached by
// pointer/reference (forward-declared).
//
// ScaleColour(RGBA)->RGBA is omitted here: it is a render-only helper that takes RGBA by value, and
// RGBA + the colour pipeline come with the renderer (Debug2DImmediateRender) reconstruction.

namespace CgsDev
{
    struct Debug2DImmediateRender;

    namespace Internal { template <class T> struct DebugLinkedList; }

    namespace DebugUI
    {
        struct Palette;
        struct Metrics;
        struct ScriptInterface;

        struct Window : public Internal::DebugInternal
        {
            // The DebugUI window stack threads Windows through mpDebugLinkedListNext.
            friend struct CgsDev::Internal::DebugLinkedList<Window>;
            // The script runner drives the protected surface from outside the window family:
            // ScriptCommand_Window (X360 0x828318A8) and SaveState (0x82832660) call the protected
            // GetMenuPath virtual (vtable slot +0x10) and ClampToScreen on arbitrary list windows.
            // dwarfdump does not surface friend declarations; attested by that asm.
            friend struct ScriptInterface;

            // Window flag bits (X360 CgsWindow.h:62-72).
            static const s32 KX_FLAGNORMAL        = 0;
            static const s32 KX_FLAGDISABLED      = 1;
            static const s32 KX_FLAGNOCAPTION     = 2;
            static const s32 KX_FLAGNOBORDER      = 4;
            static const s32 KX_FLAGNOBACKGROUND  = 8;
            static const s32 KX_FLAGHIDDEN        = 16;
            static const s32 KX_FLAGPINNED        = 32;
            static const s32 KX_FLAGNOFOCUS       = 64;
            static const s32 KX_FLAGMODAL         = 128;
            static const s32 KX_FLAGNOCASCADE     = 256;
            static const s32 KX_FLAGNOCLAMPTOSCREEN = 512;

            Window();

            void Prepare(f32 lfWidth, f32 lfHeight, const char* lpcCaption, s32 lxFlags);

            virtual void Update(f32 lfTimeStep, InputEvent leEvent);
            virtual void Render(Debug2DImmediateRender* lpRender);
            virtual void OnGetFocus();
            virtual void OnLostFocus();

            f32         GetX() const;
            f32         GetY() const;
            f32         GetWidth() const;
            f32         GetHeight() const;
            const char* GetCaption() const;
            s32         GetFlags() const;
            bool        IsPinned() const;
            bool        IsHidden() const;
            bool        IsEnabled() const;
            bool        IsModal() const;
            bool        IsActiveWindow() const;
            bool        CanActivate() const;
            void        TogglePin();
            void        ApplyMovement(f32 lfDeltaX, f32 lfDeltaY);
            void        SetSize(f32 lfWidth, f32 lfHeight);
            void        SetPosition(f32 lfX, f32 lfY);
            void        SetCaption(const char* lpcCaption);

            f32 CalcScreenWidth();
            f32 CalcScreenHeight();
            f32 CalcCaptionWidth();
            f32 CalcCaptionHeight();

        protected:
            virtual void GetMenuPath(char* lpcBuffer, s32 liBufferLen);
            virtual void GetSelectedItemString(char* lpcBuffer, s32 liBufferLen) const;

            void                    ClampToScreen();
            const Palette&          GetPalette() const;
            const Metrics&          GetMetrics() const;
            Debug2DImmediateRender* Get2DRenderer() const;

            const char* mpcCaption;
            s32         mxFlags;
            f32         mfTransparency;

        private:
            f32     mfX;
            f32     mfY;
            f32     mfWidth;
            f32     mfHeight;
            Window* mpDebugLinkedListNext;
        };
    }
}
