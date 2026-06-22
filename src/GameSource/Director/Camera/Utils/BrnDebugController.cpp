// BrnDirector::Camera::Utils::DebugController -- debug-controller input snapshot used
// by the in-game camera debug paths (orbit/fly behaviours, ICE editor wrapper).
// Reconstructed from BURNOUT_X360_ARTIST.XEX, semantic-parity (not byte-matching).
//
// Bodied here (5 ledger functions):
//   DebugController::Clear                         @0x8220BFD8
//   DebugController::DebugControllerInfo::Clear    @0x821F8738
//   DebugController::GetIsPressed                  @0x821F8708
//   DebugController::GetJustPressed                @0x821F8718
//   DebugController::GetJustReleased               @0x821F8728

#include "GameSource/Director/Camera/Utils/BrnDebugController.h"

namespace BrnDirector
{
    namespace Camera
    {
        namespace Utils
        {
            // @0x821F8738. Zero the four parallel per-control arrays in a single 22-pass
            // loop, then clear the four analogue stick axes. The X360 walks a byte cursor
            // (state/justPressed/justReleased) alongside the float value cursor.
            void DebugController::DebugControllerInfo::Clear()
            {
                for (u32 luLoop = 0; luLoop < E_CONTROL_COUNT; ++luLoop)
                {
                    mafControlValue[luLoop]        = 0.0f;
                    mabControlState[luLoop]        = false;
                    mabControlJustPressed[luLoop]  = false;
                    mabControlJustReleased[luLoop] = false;
                }

                mfLeftStickXAxis  = 0.0f;
                mfLeftStickYAxis  = 0.0f;
                mfRightStickXAxis = 0.0f;
                mfRightStickYAxis = 0.0f;
            }

            // @0x8220BFD8. Forwards to the embedded info block's Clear (tail-call).
            void DebugController::Clear()
            {
                mDebugControllerInfo.Clear();
            }

            // @0x821F8708. mabControlState[control] (this + control + 0x58).
            bool DebugController::GetIsPressed(EControl leControl) const
            {
                return mDebugControllerInfo.mabControlState[leControl];
            }

            // @0x821F8718. mabControlJustPressed[control] (this + control + 0x6E).
            bool DebugController::GetJustPressed(EControl leControl) const
            {
                return mDebugControllerInfo.mabControlJustPressed[leControl];
            }

            // @0x821F8728. mabControlJustReleased[control] (this + control + 0x84).
            bool DebugController::GetJustReleased(EControl leControl) const
            {
                return mDebugControllerInfo.mabControlJustReleased[leControl];
            }
        }
    }
}
