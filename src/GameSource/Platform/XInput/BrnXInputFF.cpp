#include "types.hpp"
#include <cstring>   // memset

// X360 force-feedback (rumble motor) wrapper layer. The X360 ARTIST image provides a
// thin anonymous-namespace helper GetFFDeviceContext that:
//   (a) queries XInputGetCapabilities to confirm the controller supports FF (bit 0 of
//       XINPUT_CAPABILITIES::Flags == XINPUT_CAPS_FFB_SUPPORTED), and
//   (b) calls through a global function pointer (the DirectInput creation hook) to
//       acquire the underlying FF device context.
// Called by all XInputFF* entry points before issuing motor commands.
//
// XInput platform types -- forward-declared to avoid pulling in Windows SDK headers
// that require /D_AMD64_ or /D_X86_ architecture defines not set by the compile gate.
// The minimal layout below matches XINPUT_CAPABILITIES as defined in XInput.h (first
// three bytes: Type, SubType, Flags[WORD] at offset 2). Only the Flags word is read.
namespace
{
    static const u32 KU_XINPUT_FLAG_GAMEPAD        = 0x00000001u;
    static const u16 KU_XINPUT_CAPS_FFB_SUPPORTED  = 0x0001u;

    struct XInputCapabilities
    {
        u8  muType;
        u8  muSubType;
        u16 muFlags;
        // Remaining XINPUT_CAPABILITIES fields (Gamepad, Vibration) not accessed here.
        u8  maPad[20u - 4u];
    };

    extern "C" u32 XInputGetCapabilities(u32 dwUserIndex, u32 dwFlags, XInputCapabilities* lpCapabilities);
}

// Global function pointer set up by the DirectInput / XAudio FF init path.
// X360 image: dword_8327FDD0 (data-section thunk). Non-null only after the
// force-feedback subsystem has been initialised; callers treat null as "not available".
static int (*s_pfnGetFFDeviceContext)(u32 dwUserIndex, u32 dwFlags, void** ppContextOut) = nullptr;

namespace
{
    // Acquire the XInput force-feedback device context for controller dwUserIndex,
    // writing it to *ppContextOut. Returns 0 on success, or:
    //   22   -- controller does not report FF capability (XINPUT_CAPS_FFB_SUPPORTED clear)
    //    1   -- FF subsystem not initialised (s_pfnGetFFDeviceContext is null)
    // 1167   -- s_pfnGetFFDeviceContext returned a failure code
    // Reconstructed from X360 ARTIST @ 0x82BDF178.
    static int GetFFDeviceContext(u32 dwUserIndex, void** ppContextOut)
    {
        XInputCapabilities caps;
        memset(&caps, 0, 20);
        int liResult = static_cast<int>(XInputGetCapabilities(dwUserIndex, KU_XINPUT_FLAG_GAMEPAD, &caps));
        if (!liResult)
        {
            if (caps.muFlags & KU_XINPUT_CAPS_FFB_SUPPORTED)
            {
                if (s_pfnGetFFDeviceContext)
                {
                    if (s_pfnGetFFDeviceContext(dwUserIndex, 0u, ppContextOut) < 0)
                        liResult = 1167;
                }
                else
                {
                    liResult = 1;
                }
            }
            else
            {
                liResult = 22;
            }
        }
        return liResult;
    }
}
