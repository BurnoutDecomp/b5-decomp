#pragma once

// Home for BrnDirector::Camera::Utils::DebugController.
// DWARF home: GameSource/Director/Camera/Utils/BrnDebugController.h:45.
//
// Minimal OWNING slice -- this is the first TU to home the type, so it carries the
// full DWARF-attested member layout. The DebugControllerInfo sub-struct's layout is
// pinned by the X360 asm for the four bodied functions:
//   DebugControllerInfo::Clear @0x821F8738  -- zeroes the four parallel control
//       arrays (value @+0, state @+88, justPressed @+110, justReleased @+132) plus
//       the four stick-axis floats (@+156..+168), so the per-element accessors
//       resolve to those exact offsets.
//   GetIsPressed     @0x821F8708  -- reads mabControlState[control]        (base +88)
//   GetJustPressed   @0x821F8718  -- reads mabControlJustPressed[control]  (base +110)
//   GetJustReleased  @0x821F8728  -- reads mabControlJustReleased[control] (base +132)

#include "types.hpp"
#include "rw/math/vpu/types.h"   // rw::math::vpu::Vector2
#include <cstddef>               // offsetof

namespace BrnDirector
{
    namespace Camera
    {
        namespace Utils
        {
            // DWARF: BrnDebugController.h:45.
            struct DebugController
            {
                // DWARF: BrnDebugController.h:48.
                enum EControl
                {
                    E_CONTROL_UP_DPAD = 0,
                    E_CONTROL_DOWN_DPAD = 1,
                    E_CONTROL_LEFT_DPAD = 2,
                    E_CONTROL_RIGHT_DPAD = 3,
                    E_CONTROL_UP_BUTTON = 4,
                    E_CONTROL_DOWN_BUTTON = 5,
                    E_CONTROL_LEFT_BUTTON = 6,
                    E_CONTROL_RIGHT_BUTTON = 7,
                    E_CONTROL_LOWER_TRIGGER_LEFT = 8,
                    E_CONTROL_LOWER_TRIGGER_RIGHT = 9,
                    E_CONTROL_UPPER_TRIGGER_LEFT = 10,
                    E_CONTROL_UPPER_TRIGGER_RIGHT = 11,
                    E_CONTROL_LEFT_STICK_UP = 12,
                    E_CONTROL_LEFT_STICK_DOWN = 13,
                    E_CONTROL_LEFT_STICK_LEFT = 14,
                    E_CONTROL_LEFT_STICK_RIGHT = 15,
                    E_CONTROL_RIGHT_STICK_UP = 16,
                    E_CONTROL_RIGHT_STICK_DOWN = 17,
                    E_CONTROL_RIGHT_STICK_LEFT = 18,
                    E_CONTROL_RIGHT_STICK_RIGHT = 19,
                    E_CONTROL_LEFT_STICK_BUTTON = 20,
                    E_CONTROL_RIGHT_STICK_BUTTON = 21,

                    E_CONTROL_COUNT = 22
                };

                // DWARF: BrnDebugController.h:113. Four parallel per-control arrays
                // followed by the analogue stick axes.
                struct DebugControllerInfo
                {
                    f32  mafControlValue[E_CONTROL_COUNT];        // +0x00
                    bool mabControlState[E_CONTROL_COUNT];        // +0x58 (88)
                    bool mabControlJustPressed[E_CONTROL_COUNT];  // +0x6E (110)
                    bool mabControlJustReleased[E_CONTROL_COUNT]; // +0x84 (132)

                    f32  mfLeftStickXAxis;   // +0x9C (156)
                    f32  mfLeftStickYAxis;   // +0xA0 (160)
                    f32  mfRightStickXAxis;  // +0xA4 (164)
                    f32  mfRightStickYAxis;  // +0xA8 (168)

                    // @0x821F8738.
                    void Clear();
                };

                // @0x8220BFD8 -- forwards to mDebugControllerInfo.Clear().
                void Clear();

                // @0x821F8708 / 0x821F8718 / 0x821F8728 -- per-control queries.
                bool GetIsPressed(EControl leControl) const;
                bool GetJustPressed(EControl leControl) const;
                bool GetJustReleased(EControl leControl) const;
                f32 GetControlValue(s32 liControl) const;

                // Remaining DWARF-declared accessors (BrnDebugController.h:145..170).
                // DECLARATION-ONLY -- bodies land with their own ledger entries; the
                // per-TU `cl /c` gate does not link.
                f32 GetLowerTriggerAxis() const;
                f32 GetUpperTriggerAxis() const;
                f32 GetDPadYAxis() const;
                f32 GetDPadXAxis() const;
                f32 GetButtonsLeftRightAxis() const;
                rw::math::vpu::Vector2 GetLeftStick() const;
                rw::math::vpu::Vector2 GetRightStick() const;
                const DebugControllerInfo& GetControllerInfo() const;
                void SetControllerInfo(const DebugControllerInfo& lrInfo);

            private:
                // DWARF: BrnDebugController.h:176.
                DebugControllerInfo mDebugControllerInfo;
            };

            // Pin the offsets the X360 asm proves for the bodied functions.
            static_assert(offsetof(DebugController::DebugControllerInfo, mafControlValue) == 0,
                          "DebugControllerInfo value array must be at +0x00");
            static_assert(offsetof(DebugController::DebugControllerInfo, mabControlState) == 88,
                          "DebugControllerInfo state array must be at +0x58 (GetIsPressed)");
            static_assert(offsetof(DebugController::DebugControllerInfo, mabControlJustPressed) == 110,
                          "DebugControllerInfo just-pressed array must be at +0x6E (GetJustPressed)");
            static_assert(offsetof(DebugController::DebugControllerInfo, mabControlJustReleased) == 132,
                          "DebugControllerInfo just-released array must be at +0x84 (GetJustReleased)");
            static_assert(offsetof(DebugController::DebugControllerInfo, mfLeftStickXAxis) == 156,
                          "DebugControllerInfo stick axes must follow at +0x9C");
            static_assert(sizeof(DebugController) == sizeof(DebugController::DebugControllerInfo),
                          "DebugController is just its info block (no base / extra members)");
        }
    }
}
