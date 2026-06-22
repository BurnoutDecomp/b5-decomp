#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"  // DebugUI::Metrics / InputEvent

// CgsDev::DebugController - the debug-UI input layer. It reads the host pad + keyboard each frame and
// translates raw input into the DebugUI::InputEvent the menu/console consume (cursor moves, select,
// dock, toggles), with auto-repeat. The X360 build drives it from a DebugManagerPad the DebugManager
// hands over (DebugManager::SetGamePad @ 0x82815EF8 stores it into the controller's mpDebugManagerPad).
//
// Layout + the SetGamePad member are X360-authoritative (DecFIGS DWARF for this header; the asm uses
// the gamepad pointer, not the older port-int field). The keyboard helpers map to the host keyboard
// API on PC; only the accessors the debug UI reads are defined inline here - the heavy update/keyboard
// methods are declared-only and homed in CgsDebugController.cpp.

namespace CgsDev
{
    class DebugManager;
    struct DebugManagerPad;

    class DebugController
    {
    public:
        void Construct(const DebugUI::Metrics* lpMetrics);
        void Update(f32 lfTimeStep);
        void Destruct();

        // 0x82815EF8 path - the DebugManager hands the controller its gamepad source.
        void SetGamePad(DebugManagerPad* lpDebugManagerPad);

        DebugUI::InputEvent GetInputEvent() const { return meInputEvent; }

        static const s32 KI_MAX_BUTTONS = 8;

        enum SpecialKey
        {
            E_KEY_NONE = 0,
            E_KEY_LEFT,
            E_KEY_RIGHT,
            E_KEY_UP,
            E_KEY_DOWN,
            E_KEY_F1,
            E_KEY_F2,
            E_KEY_F3,
            E_KEY_F4,
            E_KEY_F5,
            E_KEY_F6,
            E_KEY_F7,
            E_KEY_F8,
            E_KEY_F9,
            E_KEY_F10,
            E_KEY_F11,
            E_KEY_F12,
        };

        f32  GetX() const  { return mfX; }
        f32  GetY() const  { return mfY; }
        f32  GetX2() const { return mfX2; }
        f32  GetY2() const { return mfY2; }
        bool IsButtonPressed(s32 liButton) const { return mabButtons[liButton]; }

        char       GetKeyPress() const        { return mcKeyPress; }
        SpecialKey GetSpecialKeyPress() const { return meSpecialKeyPress; }
        bool       IsCtrlPressed() const      { return mbCtrlPressed; }
        bool       IsShiftPressed() const     { return mbShiftPressed; }
        bool       IsAltPressed() const       { return mbAltPressed; }
        bool       IsKeyboardPresent() const  { return mbKeyboardPresent; }

        bool IsKeyboardLocked() const { return mbKeyboardLocked; }
        void LockKeyboard();
        void UnlockKeyboard();

    private:
        void ClearEvent();
        void UpdateEvent(f32 lfTimeStep);

        DebugUI::InputEvent GetInputEvent(f32 lfTimeStep);
        DebugUI::InputEvent TranslateControllerInputEvent() const;
        DebugUI::InputEvent GetAxisEvent() const;

        void CopyKeyboardToPad();

        void UpdatePad();
        void ClearPad();
        f32  TranslateAxis(s32 liAxisValue);

        bool InitKeyboard();
        void UpdateKeyboard(f32 lfTimeStep);
        void ClearKeyboard();
        void ReadKey(f32 lfTimeStep);
        char SimpleLocaliseKey(char lcKey);

        // --- members (DWARF CgsDebugController.h order) ----------------------
        const DebugUI::Metrics* mpMetrics;             // h:140

        DebugUI::InputEvent     meInputEvent;          // h:143
        DebugUI::InputEvent     meLastInputEvent;      // h:145
        f32                     mfEventRepeatDelay;    // h:146
        f32                     mfEventRepeatDelayMax; // h:147

        DebugManagerPad*        mpDebugManagerPad;     // h:165 - set by DebugManager::SetGamePad
        f32                     mfX;                   // h:166
        f32                     mfY;                   // h:167
        f32                     mfX2;                  // h:168
        f32                     mfY2;                  // h:169
        bool                    mabButtons[KI_MAX_BUTTONS]; // h:170

        SpecialKey              meSpecialKeyPress;     // h:178
        char                    mcKeyPress;            // h:179
        bool                    mbKeyboardPresent;     // h:180
        bool                    mbShiftPressed;        // h:181
        bool                    mbAltPressed;          // h:182
        bool                    mbCtrlPressed;         // h:183

        bool                    mbKeyboardLocked;      // h:199

        static const f32        KF_KEY_REPEAT_TIME;    // h:186
    };
}
