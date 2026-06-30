#ifndef GAMESOURCE_DIRECTOR_CAMERA_UTILS_BRN_CAMERA_TWEAKER_H
#define GAMESOURCE_DIRECTOR_CAMERA_UTILS_BRN_CAMERA_TWEAKER_H

// ============================================================================
// GameSource/Director/Camera/Utils/BrnCameraTweaker.h
//
// Home for BrnDirector::Camera::Utils::Tweaker -- the camera-parameter debug
// "tweaker" used by the in-game camera behaviours (BehaviourManager::AttachTweaker
// and the per-behaviour SetupTweaker hooks). It binds named tunable camera
// parameters to controller axes / buttons so a developer can nudge them live and
// renders the current bindings through the Director's DebugPrinter.
//
// This is the FIRST TU to home the type, so it carries the full DWARF-attested
// member layout. Offsets are pinned against the X360 ARTIST asm for the five
// bodied functions:
//   Tweaker::Construct     @0x821F8588
//   Tweaker::AddMapping    @0x821F85E0  (the float* lpfScale overload)
//   Tweaker::GetAxisValue  @0x8220BC58
//   Tweaker::Render        @0x8220BE88
//   Tweaker::Update        @0x822217C8
//
// Layout proof (byte offsets all corroborated by >=2 of the bodied functions):
//   maAxisMapping[3][9]    @ +0x000  (AxisMapping stride 0x14=20, mbUsed @ +16:
//                                     Construct zeroes this+16 stride 20;
//                                     Render reads 0x10(r28); AddMapping writes it)
//   mJustPressedMapping[3][22]  @ +0x21C (540) (ControlFunctionMapping stride 0x10=16,
//                                     mbUsed @ +12: Render reads 0xC(r31), stride 0x10)
//   mJustReleasedMapping[3][22] @ +0x63C (1596) (= justPressed +1056; Construct/Update
//                                     walk the two CF arrays 1056 bytes apart)
//   mbHideInstructions          @ +0xA5C (2652) (Construct/Render/Update all use +2652)
// ============================================================================

#include "types.hpp"
#include "GameSource/Director/Camera/Utils/BrnDebugController.h" // DebugController + EControl

namespace BrnDirector
{
    struct DebugPrinter;   // BrnDirectorModuleDebugPrinter.h -- Render takes a DebugPrinter&

    namespace Camera
    {
        namespace Utils
        {
            // DWARF: BrnCameraTweaker.h:45.
            struct Tweaker
            {
                // DWARF: BrnCameraTweaker.h:49.
                enum EMap
                {
                    E_MAP_NORMAL = 0,
                    E_MAP_BUTTON_DOWN = 1,
                    E_MAP_BUTTON_UP = 2,

                    E_MAP_COUNT = 3
                };

                // DWARF: BrnCameraTweaker.h:62.
                enum EAxis
                {
                    E_AXIS_LEFT_STICK_X = 0,
                    E_AXIS_LEFT_STICK_Y = 1,
                    E_AXIS_RIGHT_STICK_X = 2,
                    E_AXIS_RIGHT_STICK_Y = 3,
                    E_AXIS_DPAD_Y = 4,
                    E_AXIS_DPAD_X = 5,
                    E_AXIS_LOWER_TRIGGERS = 6,
                    E_AXIS_UPPER_TRIGGERS = 7,
                    E_AXIS_BUTTONS_LEFT_RIGHT = 8,

                    E_AXIS_COUNT = 9
                };

                // DWARF: BrnCameraTweaker.h:161. One axis->variable binding (20 bytes).
                struct AxisMapping
                {
                    const char* lpcName;             // +0x00 display name of the tuned var
                    f32*        mpfVariableToTweak;   // +0x04 the float being driven
                    f32*        mpfScale;             // +0x08 optional live scale source
                    f32         mfScale;              // +0x0C constant scale (when no source)
                    bool        mbUsed;               // +0x10 slot occupied
                };

                // DWARF: BrnCameraTweaker.h:170. One control->callback binding (16 bytes).
                struct ControlFunctionMapping
                {
                    const char* mpcName;             // +0x00 display name of the action
                    void      (*mpFunction)(void*);  // +0x04 callback
                    void*       mpUserData;          // +0x08 callback user data
                    bool        mbUsed;              // +0x0C slot occupied
                };

                // @0x821F8588. Reset every slot to "unused" and show instructions.
                void Construct();

                // DWARF: BrnCameraTweaker.h:124. Bind a tunable float to an axis with a
                // CONSTANT scale. No X360 body in this packet -> declaration-only.
                void AddMapping(const char* lpcName, f32* lpfVariableToTweak, f32 lfScale,
                                EAxis leAxisToMapTo, EMap leMap);

                // @0x821F85E0. Bind a tunable float to an axis with a LIVE scale source.
                void AddMapping(const char* lpcName, f32* lpfVariableToTweak, f32* lpfScale,
                                EAxis leAxisToMapTo, EMap leMap);

                // DWARF: BrnCameraTweaker.h:140 / :148. Bind a callback to a control's
                // just-pressed / just-released edge. No X360 body in this packet ->
                // declaration-only.
                void AddJustPressedMapping(const char* lpcName, void (*lpFunction)(void*),
                                           void* lpUserData, DebugController::EControl leControl,
                                           EMap leMap);
                void AddJustReleasedMapping(const char* lpcName, void (*lpFunction)(void*),
                                            void* lpUserData, DebugController::EControl leControl,
                                            EMap leMap);

                // @0x822217C8. Drive every used binding from the controller this frame.
                void Update(const DebugController& lrDebugController);

                // @0x8220BE88. Render the current bindings through the DebugPrinter.
                void Render(DebugPrinter& lrDebugPrinter);

            private:
                // @0x8220BC58. The raw axis value for the requested axis from the
                // controller's stick floats.
                f32 GetAxisValue(EAxis leAxisToGet, const DebugController& lrDebugController);

                AxisMapping            maAxisMapping[E_MAP_COUNT][E_AXIS_COUNT];                  // +0x000
                ControlFunctionMapping mJustPressedMapping[E_MAP_COUNT][DebugController::E_CONTROL_COUNT];  // +0x21C
                ControlFunctionMapping mJustReleasedMapping[E_MAP_COUNT][DebugController::E_CONTROL_COUNT]; // +0x63C
                bool                   mbHideInstructions;                                        // +0xA5C
            };
        } // namespace Utils
    } // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_UTILS_BRN_CAMERA_TWEAKER_H
