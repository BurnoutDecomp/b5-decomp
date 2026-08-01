// BrnDirector::Camera::Utils::Tweaker -- live camera-parameter debug tweaker.
// Reconstructed from BURNOUT_X360_ARTIST.XEX, semantic-parity (not byte-matching).
//
// Bodied here (4 ledger functions; the fifth, Tweaker::Construct @0x821F8588, was
// file-split VERBATIM into BrnCameraTweakerConstruct.cpp on 2026-08-01 so it could be
// mounted without this TU's debug-render leaves -- see that file's banner):
//   Tweaker::AddMapping    @0x821F85E0  (the f32* lpfScale overload)
//   Tweaker::GetAxisValue  @0x8220BC58
//   Tweaker::Render        @0x8220BE88
//   Tweaker::Update        @0x822217C8
//
// Declaration-only (no X360 body in this packet): the constant-scale AddMapping
// overload, AddJustPressedMapping, AddJustReleasedMapping.

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
            // Static name tables read by Render. The X360 build sources these
            // from rodata arrays (dword_82FAACE8 = axis names, dword_82FAAC90 =
            // control names); their string CONTENTS are rodata we cannot ground
            // from this packet, so they are declared extern and defined by their
            // own data TU (the per-TU `cl /c` gate does not link). KAAC_AXIS_NAMES
            // is the DWARF-attested symbol (BrnCameraTweaker.h:87).
            // ----------------------------------------------------------------
            extern const char* const KAAC_AXIS_NAMES[Tweaker::E_AXIS_COUNT];
            extern const char* const KAAC_CONTROL_NAMES[DebugController::E_CONTROL_COUNT];

            // @0x821F8588 Tweaker::Construct is NOT here any more (2026-08-01): it was
            // file-split VERBATIM into the sibling BrnCameraTweakerConstruct.cpp so it could
            // be mounted without this TU's five debug-render leaves. See that file's banner
            // for the measurement and the DELETE-WHEN. Do not re-add it here -- a second
            // definition is an LNK2005.

            // @0x821F85E0. Bind a tunable float to an axis using a LIVE scale source
            // (a pointer the caller keeps updating). Asserts all four preconditions,
            // then fills the [map][axis] slot: mfScale is forced to 0.0 (the live
            // source wins), the three pointers are stored, and mbUsed is set.
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

            // @0x822217C8. Drive every used binding from this frame's controller.
            //   * DOWN_BUTTON edge toggles mbHideInstructions (controller +0x73 =
            //     mabControlJustPressed[E_CONTROL_DOWN_BUTTON=5]).
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

                        // asm guards the callback on the mapping's mbUsed flag (lbz mapping+0xC),
                        // not on the function pointer, before testing the controller edge.
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

            // @0x8220BC58. The raw value of the requested axis from the controller's
            // analogue stick floats. The X360 only wires the four stick components
            // (left/right * X/Y); the d-pad / trigger / button axes fall through with
            // no value and an unhandled axis asserts.
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

                    // Composite axes = the difference of two control values (asm jumptable cases 4-8
                    // @0x8220BD8C..0x8220BDEC, fsubs of two DebugControllerInfo::mafControlValue[] entries).
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

            // @0x8220BE88. Render the live bindings through the DebugPrinter. While
            // instructions are shown, print one "<axis>  <var>" line per used axis
            // binding and one "<control>  <action>" line per used just-pressed binding
            // (across all three maps), then the hide-instructions hint keyed to the
            // right-stick button (control name index 5).
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

                // asm @0x8220BF98 names control_names[5] = E_CONTROL_DOWN_BUTTON (the same control the
                // Update toggle reads), NOT the right-stick button.
                CgsCore::SPrintf(lacMessage, kiMessageLength, "Press the %s to hide instructions",
                                 KAAC_CONTROL_NAMES[DebugController::E_CONTROL_DOWN_BUTTON]);
                lrDebugPrinter.Print("");
                lrDebugPrinter.Print(lacMessage);
            }
        } // namespace Utils
    } // namespace Camera
} // namespace BrnDirector
