// BrnDirector::Camera::Utils::Tweaker -- live camera-parameter debug tweaker.
// Semantic-parity reconstruction (not byte-matching).
//
// Bodied here: Construct, both AddMapping overloads, AddJustPressedMapping, GetAxisValue,
// Update and Render, plus the two static name tables Render prints through.
//
// Declaration-only (no recovered body, no caller in the tree): AddJustReleasedMapping.
// Update already drives the just-released bindings, so the feature is complete the day a
// caller and a body for that one binder land.

#include "GameSource/Director/Camera/Utils/BrnCameraTweaker.h"

#include "GameSource/Director/DirectorModule/BrnDirectorModuleDebugPrinter.h" // DebugPrinter
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                       // CgsCore::SPrintf
#include "GameShared/GameClasses/Development/AssertSystem/CgsAssertManager.h" // CGS_ASSERT

namespace BrnDirector
{
    namespace Camera
    {
        namespace Utils
        {
            // ----------------------------------------------------------------
            // The two name tables Render prints. Recovered verbatim from the shipped
            // image's own tables (the original build fills them from a pointer table at
            // start-up; a plain constant table is the same thing with less ceremony).
            // ----------------------------------------------------------------
            extern const char* const KAAC_AXIS_NAMES[Tweaker::E_AXIS_COUNT] =
            {
                "Left Stick Left/Right",   // E_AXIS_LEFT_STICK_X
                "Left Stick Up/Down",      // E_AXIS_LEFT_STICK_Y
                "Right Stick Left/Right",  // E_AXIS_RIGHT_STICK_X
                "Right Stick Up/Down",     // E_AXIS_RIGHT_STICK_Y
                "DPad Up/Down",            // E_AXIS_DPAD_Y
                "DPad Left/Right",         // E_AXIS_DPAD_X
                "Lower Triggers",          // E_AXIS_LOWER_TRIGGERS
                "Upper Triggers",          // E_AXIS_UPPER_TRIGGERS
                "Square/Circle or X/B",    // E_AXIS_BUTTONS_LEFT_RIGHT
            };

            extern const char* const KAAC_CONTROL_NAMES[DebugController::E_CONTROL_COUNT] =
            {
                "DPad Up",              // E_CONTROL_UP_DPAD
                "DPad Down",            // E_CONTROL_DOWN_DPAD
                "DPad Left",            // E_CONTROL_LEFT_DPAD
                "DPad Right",           // E_CONTROL_RIGHT_DPAD
                "Y Button",             // E_CONTROL_UP_BUTTON
                "A Button",             // E_CONTROL_DOWN_BUTTON
                "X Button",             // E_CONTROL_LEFT_BUTTON
                "B Button",             // E_CONTROL_RIGHT_BUTTON
                "Left Trigger",         // E_CONTROL_LOWER_TRIGGER_LEFT
                "Right Trigger",        // E_CONTROL_LOWER_TRIGGER_RIGHT
                "L1",                   // E_CONTROL_UPPER_TRIGGER_LEFT
                "R1",                   // E_CONTROL_UPPER_TRIGGER_RIGHT
                "Left Stick Up",        // E_CONTROL_LEFT_STICK_UP
                "Left Stick Down",      // E_CONTROL_LEFT_STICK_DOWN
                "Left Stick Left",      // E_CONTROL_LEFT_STICK_LEFT
                "Left Stick Right",     // E_CONTROL_LEFT_STICK_RIGHT
                // The shipped table repeats the four LEFT stick captions for the RIGHT
                // stick directions -- an authoring slip in the original table, carried
                // verbatim so the debug overlay reads the way it shipped.
                "Left Stick Up",        // E_CONTROL_RIGHT_STICK_UP
                "Left Stick Down",      // E_CONTROL_RIGHT_STICK_DOWN
                "Left Stick Left",      // E_CONTROL_RIGHT_STICK_LEFT
                "Left Stick Right",     // E_CONTROL_RIGHT_STICK_RIGHT
                "Left Stick Pressed",   // E_CONTROL_LEFT_STICK_BUTTON
                "Right Stick Pressed",  // E_CONTROL_RIGHT_STICK_BUTTON
            };

            // Reset the tweaker to an empty, "instructions-shown" state. Walks the mbUsed
            // flag of every binding and clears it, then clears mbHideInstructions. Nothing
            // else is touched -- AddMapping fully overwrites a slot when it is (re)used.
            void Tweaker::Construct()
            {
                for (s32 liMap = 0; liMap < E_MAP_COUNT; ++liMap)
                {
                    for (s32 liAxis = 0; liAxis < E_AXIS_COUNT; ++liAxis)
                    {
                        maAxisMapping[liMap][liAxis].mbUsed = false;
                    }

                    for (s32 liControl = 0; liControl < DebugController::E_CONTROL_COUNT; ++liControl)
                    {
                        mJustPressedMapping[liMap][liControl].mbUsed  = false;
                        mJustReleasedMapping[liMap][liControl].mbUsed = false;
                    }
                }

                mbHideInstructions = false;
            }

            // Bind a tunable float to an axis with a CONSTANT scale. Fills the [map][axis]
            // slot: the live-scale source is cleared (the constant wins), the name, the
            // tracked float and the constant scale are stored, and mbUsed is set.
            void Tweaker::AddMapping(const char* lpcName, f32* lpfVariableToTweak, f32 lfScale,
                                     EAxis leAxisToMapTo, EMap leMap)
            {
                CGS_ASSERT(lpcName != nullptr, "lpcName != NULL");
                CGS_ASSERT(lpfVariableToTweak != nullptr, "lpfVariableToTweak != NULL");
                CGS_ASSERT(leAxisToMapTo < E_AXIS_COUNT, "leAxisToMapTo < E_AXIS_COUNT");
                CGS_ASSERT(leMap < E_MAP_COUNT, "leMap < E_MAP_COUNT");

                AxisMapping& lrMapping = maAxisMapping[leMap][leAxisToMapTo];
                lrMapping.lpcName            = lpcName;
                lrMapping.mpfVariableToTweak = lpfVariableToTweak;
                lrMapping.mpfScale           = 0;
                lrMapping.mfScale            = lfScale;
                lrMapping.mbUsed             = true;
            }

            // Bind a tunable float to an axis using a LIVE scale source (a pointer the
            // caller keeps updating). Asserts all five preconditions, then fills the
            // [map][axis] slot: mfScale is forced to 0.0 (the live source wins), the three
            // pointers are stored, and mbUsed is set.
            void Tweaker::AddMapping(const char* lpcName, f32* lpfVariableToTweak, f32* lpfScale,
                                     EAxis leAxisToMapTo, EMap leMap)
            {
                CGS_ASSERT(lpcName != nullptr, "lpcName != NULL");
                CGS_ASSERT(lpfVariableToTweak != nullptr, "lpfVariableToTweak != NULL");
                CGS_ASSERT(lpfScale != nullptr, "lpfScale != NULL");
                CGS_ASSERT(leAxisToMapTo < E_AXIS_COUNT, "leAxisToMapTo < E_AXIS_COUNT");
                CGS_ASSERT(leMap < E_MAP_COUNT, "leMap < E_MAP_COUNT");

                AxisMapping& lrMapping = maAxisMapping[leMap][leAxisToMapTo];
                lrMapping.mfScale            = 0.0f;
                lrMapping.lpcName            = lpcName;
                lrMapping.mpfVariableToTweak = lpfVariableToTweak;
                lrMapping.mpfScale           = lpfScale;
                lrMapping.mbUsed             = true;
            }

            // Bind a callback to a control's just-pressed edge. Fills the [map][control]
            // slot with the caption, the callback and its user data, and sets mbUsed.
            void Tweaker::AddJustPressedMapping(const char* lpcName, void (*lpFunction)(void*),
                                                void* lpUserData, DebugController::EControl leControl,
                                                EMap leMap)
            {
                CGS_ASSERT(lpcName != nullptr, "lpcName != NULL");
                CGS_ASSERT(lpFunction != nullptr, "lpFunction != NULL");
                CGS_ASSERT(leControl < DebugController::E_CONTROL_COUNT, "leControl < E_CONTROL_COUNT");
                CGS_ASSERT(leMap < E_MAP_COUNT, "leMap < E_MAP_COUNT");

                ControlFunctionMapping& lrMapping = mJustPressedMapping[leMap][leControl];
                lrMapping.mpUserData = lpUserData;
                lrMapping.mbUsed     = true;
                lrMapping.mpcName    = lpcName;
                lrMapping.mpFunction = lpFunction;
            }

            // Drive every used binding from this frame's controller.
            //   * The A-button edge toggles mbHideInstructions.
            //   * Each used axis mapping advances its tracked float by
            //     axisValue * scale  (scale from *mpfScale when present, else mfScale).
            //   * Each used just-pressed / just-released control mapping fires its
            //     callback on the matching controller edge.
            void Tweaker::Update(const DebugController& lrDebugController)
            {
                if (lrDebugController.GetJustPressed(DebugController::E_CONTROL_DOWN_BUTTON))
                {
                    mbHideInstructions = !mbHideInstructions;
                }

                for (s32 liMap = 0; liMap < E_MAP_COUNT; ++liMap)
                {
                    for (s32 liAxis = 0; liAxis < E_AXIS_COUNT; ++liAxis)
                    {
                        AxisMapping& lrMapping = maAxisMapping[liMap][liAxis];
                        if (lrMapping.mbUsed)
                        {
                            const f32 lfAxisValue =
                                GetAxisValue(static_cast<EAxis>(liAxis), lrDebugController);
                            const f32 lfScale =
                                lrMapping.mpfScale ? *lrMapping.mpfScale : lrMapping.mfScale;
                            *lrMapping.mpfVariableToTweak += lfAxisValue * lfScale;
                        }
                    }

                    for (s32 liControl = 0; liControl < DebugController::E_CONTROL_COUNT; ++liControl)
                    {
                        const DebugController::EControl leControl =
                            static_cast<DebugController::EControl>(liControl);

                        // The callback is guarded on the mapping's mbUsed flag, not on the
                        // function pointer, before the controller edge is tested.
                        ControlFunctionMapping& lrPressed = mJustPressedMapping[liMap][liControl];
                        if (lrPressed.mbUsed && lrDebugController.GetJustPressed(leControl))
                        {
                            lrPressed.mpFunction(lrPressed.mpUserData);
                        }

                        ControlFunctionMapping& lrReleased = mJustReleasedMapping[liMap][liControl];
                        if (lrReleased.mbUsed && lrDebugController.GetJustReleased(leControl))
                        {
                            lrReleased.mpFunction(lrReleased.mpUserData);
                        }
                    }
                }
            }

            // The raw value of the requested axis from the controller's analogue stick
            // floats. Only the four stick components are read straight; the d-pad /
            // trigger / button axes are the difference of two control values, and an
            // unhandled axis asserts.
            f32 Tweaker::GetAxisValue(EAxis leAxisToGet, const DebugController& lrDebugController)
            {
                const DebugController::DebugControllerInfo& lrInfo =
                    lrDebugController.GetControllerInfo();

                switch (leAxisToGet)
                {
                    case E_AXIS_LEFT_STICK_X:
                        return lrInfo.mfLeftStickXAxis;
                    case E_AXIS_LEFT_STICK_Y:
                        return lrInfo.mfLeftStickYAxis;
                    case E_AXIS_RIGHT_STICK_X:
                        return lrInfo.mfRightStickXAxis;
                    case E_AXIS_RIGHT_STICK_Y:
                        return lrInfo.mfRightStickYAxis;

                    case E_AXIS_DPAD_Y:
                        return lrInfo.mafControlValue[DebugController::E_CONTROL_UP_DPAD]
                             - lrInfo.mafControlValue[DebugController::E_CONTROL_DOWN_DPAD];
                    case E_AXIS_DPAD_X:
                        return lrInfo.mafControlValue[DebugController::E_CONTROL_RIGHT_DPAD]
                             - lrInfo.mafControlValue[DebugController::E_CONTROL_LEFT_DPAD];
                    case E_AXIS_LOWER_TRIGGERS:
                        return lrInfo.mafControlValue[DebugController::E_CONTROL_LOWER_TRIGGER_RIGHT]
                             - lrInfo.mafControlValue[DebugController::E_CONTROL_LOWER_TRIGGER_LEFT];
                    case E_AXIS_UPPER_TRIGGERS:
                        return lrInfo.mafControlValue[DebugController::E_CONTROL_UPPER_TRIGGER_RIGHT]
                             - lrInfo.mafControlValue[DebugController::E_CONTROL_UPPER_TRIGGER_LEFT];
                    case E_AXIS_BUTTONS_LEFT_RIGHT:
                        return lrInfo.mafControlValue[DebugController::E_CONTROL_RIGHT_BUTTON]
                             - lrInfo.mafControlValue[DebugController::E_CONTROL_LEFT_BUTTON];

                    default:
                        CGS_ASSERT(false,
                                   "BrnDirector::Camera::Utils::Tweaker::GetAxisValue : unhandled axis");
                        break;
                }

                return 0.0f;
            }

            // Render the live bindings through the DebugPrinter. While instructions are
            // shown, print one "<axis>  <var>" line per used axis binding and one
            // "<control>  <action>" line per used just-pressed binding (across all three
            // maps), then the hide-instructions hint naming the control the Update toggle
            // reads.
            void Tweaker::Render(DebugPrinter& lrDebugPrinter)
            {
                const char lacFormat[10] = "%-30s  %s";

                if (mbHideInstructions)
                {
                    return;
                }

                const s32 kiMessageLength = 256;
                char      lacMessage[256];

                for (s32 liMap = 0; liMap < E_MAP_COUNT; ++liMap)
                {
                    for (s32 liAxis = 0; liAxis < E_AXIS_COUNT; ++liAxis)
                    {
                        const AxisMapping& lrMapping = maAxisMapping[liMap][liAxis];
                        if (lrMapping.mbUsed)
                        {
                            CgsCore::SPrintf(lacMessage, kiMessageLength, lacFormat,
                                             KAAC_AXIS_NAMES[liAxis], lrMapping.lpcName);
                            lrDebugPrinter.Print(lacMessage);
                        }
                    }

                    for (s32 liControl = 0; liControl < DebugController::E_CONTROL_COUNT; ++liControl)
                    {
                        const ControlFunctionMapping& lrMapping = mJustPressedMapping[liMap][liControl];
                        if (lrMapping.mbUsed)
                        {
                            CgsCore::SPrintf(lacMessage, kiMessageLength, lacFormat,
                                             KAAC_CONTROL_NAMES[liControl], lrMapping.mpcName);
                            lrDebugPrinter.Print(lacMessage);
                        }
                    }
                }

                CgsCore::SPrintf(lacMessage, kiMessageLength, "Press the %s to hide instructions",
                                 KAAC_CONTROL_NAMES[DebugController::E_CONTROL_DOWN_BUTTON]);
                lrDebugPrinter.Print("");
                lrDebugPrinter.Print(lacMessage);
            }
        } // namespace Utils
    } // namespace Camera
} // namespace BrnDirector
