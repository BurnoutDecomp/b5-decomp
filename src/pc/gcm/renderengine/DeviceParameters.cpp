#include "pc/gcm/renderengine/DeviceParameters.h"

#include <cstddef>   // offsetof

// X360 reconstruction of renderengine::Device::Parameters. The bodies mirror the X360 image
// store-for-store: the parameter dwords are written by name in the order the asm writes them, the
// packed format/version constants reproduce the lis/ori immediates exactly, and the FixD3DDefaults
// bit math matches the PPC extlwi (extract 22 bits at bit 1 -> (2*v) & 0xFFFFFC00).

namespace renderengine
{
    // The named members must land at the X360 store offsets the image uses.
    static_assert(offsetof(DeviceParameters, muBackBufferWidth)   == 0x00, "DeviceParameters layout");
    static_assert(offsetof(DeviceParameters, muBackBufferHeight)  == 0x04, "DeviceParameters layout");
    static_assert(offsetof(DeviceParameters, muFormatA)           == 0x08, "DeviceParameters layout");
    static_assert(offsetof(DeviceParameters, muFrontBufferWidth)  == 0x0C, "DeviceParameters layout");
    static_assert(offsetof(DeviceParameters, muFrontBufferHeight) == 0x10, "DeviceParameters layout");
    static_assert(offsetof(DeviceParameters, muFormatB)           == 0x14, "DeviceParameters layout");
    static_assert(offsetof(DeviceParameters, muFlag18)            == 0x18, "DeviceParameters layout");
    static_assert(offsetof(DeviceParameters, muFlag1C)            == 0x1C, "DeviceParameters layout");
    static_assert(offsetof(DeviceParameters, muFlag20)            == 0x20, "DeviceParameters layout");
    static_assert(offsetof(DeviceParameters, muFormatC)           == 0x24, "DeviceParameters layout");
    static_assert(offsetof(DeviceParameters, muFlag28)            == 0x28, "DeviceParameters layout");
    static_assert(offsetof(DeviceParameters, muFlag2C)            == 0x2C, "DeviceParameters layout");
    static_assert(offsetof(DeviceParameters, muRefreshRate)       == 0x30, "DeviceParameters layout");
    static_assert(offsetof(DeviceParameters, muFlag34)            == 0x34, "DeviceParameters layout");
    static_assert(offsetof(DeviceParameters, muFlag38)            == 0x38, "DeviceParameters layout");
    static_assert(offsetof(DeviceParameters, muFlag3C)            == 0x3C, "DeviceParameters layout");
    static_assert(offsetof(DeviceParameters, muFlag40)            == 0x40, "DeviceParameters layout");
    static_assert(offsetof(DeviceParameters, muFlag44)            == 0x44, "DeviceParameters layout");
    static_assert(offsetof(DeviceParameters, muFlag60)            == 0x60, "DeviceParameters layout");

    // 0x82B61540 -- seed the device parameter block from a video-mode index.
    void DeviceParametersOps::Initialize(DeviceParameters* lpThis, u32 luVideoMode)
    {
        // The constant register seed (HIDWORD(v3) = 757072278 = 0x2D200196) is the source for the
        // +0x24 store below; reproduced as the immediate the lis/ori build.
        lpThis->muFormatA = 0x18280186u;   // stw 0x18280186, 8(r31)
        lpThis->muFormatB = 0x28280106u;   // stw 0x28280106, 0x14(r31)
        lpThis->muFlag1C  = 0u;            // stw 0, 0x1C(r31)
        lpThis->muFlag18  = 1u;            // stw 1, 0x18(r31)  (low byte = 1; pad bytes follow)
        lpThis->mauPad19[0] = 0u;
        lpThis->mauPad19[1] = 0u;
        lpThis->mauPad19[2] = 0u;
        lpThis->muFlag20  = 1u;            // stw 1, 0x20(r31)
        lpThis->muFormatC = 0x2D200196u;   // stw 0x2D200196, 0x24(r31)
        lpThis->muFlag28  = 1u;            // stb 1, 0x28(r31)
        lpThis->mauPad29[0] = 0u;
        lpThis->mauPad29[1] = 0u;
        lpThis->mauPad29[2] = 0u;
        lpThis->muFlag34  = 1u;            // stw 1, 0x34(r31)
        lpThis->muFlag2C  = 0u;            // stw 0, 0x2C(r31)
        lpThis->muFlag38  = 0u;            // stw 0, 0x38(r31)
        lpThis->muFlag3C  = 0u;            // stw 0, 0x3C(r31)
        lpThis->muFlag40  = 0u;            // stw 0, 0x40(r31)
        lpThis->muFlag44  = 0u;            // stw 0, 0x44(r31)

        if (luVideoMode == 0u)
        {
            // LABEL_16: default SD 640x480 (the < 3 fall-through), refresh untouched here.
            lpThis->muBackBufferWidth   = 640u;   // r11 = 0x280
            lpThis->muBackBufferHeight  = 480u;   // r10 = 0x1E0
            lpThis->muFrontBufferWidth  = 640u;
            lpThis->muFrontBufferHeight = 480u;
        }
        else if (luVideoMode == 1u)
        {
            // PAL 640x576 @ 50 Hz.
            lpThis->muBackBufferWidth   = 640u;   // 0x280
            lpThis->muBackBufferHeight  = 576u;   // 0x240
            lpThis->muFrontBufferWidth  = 640u;
            lpThis->muFrontBufferHeight = 576u;
            lpThis->muRefreshRate       = 50u;    // 0x32
        }
        else if (luVideoMode < 3u)
        {
            // mode 2: same SD 640x480 as the goto LABEL_16 path.
            lpThis->muBackBufferWidth   = 640u;
            lpThis->muBackBufferHeight  = 480u;
            lpThis->muFrontBufferWidth  = 640u;
            lpThis->muFrontBufferHeight = 480u;
        }
        else if (luVideoMode == 3u)
        {
            // 720x480 @ 60 Hz.
            lpThis->muBackBufferWidth   = 720u;   // r11 = 0x2D0 (width), then v7 = 480
            lpThis->muBackBufferHeight  = 480u;   // 0x1E0
            lpThis->muFrontBufferWidth  = 720u;
            lpThis->muFrontBufferHeight = 480u;
            lpThis->muRefreshRate       = 60u;    // 0x3C
        }
        else if (luVideoMode < 5u)
        {
            // mode 4: 1280x720 @ 60 Hz.
            lpThis->muBackBufferWidth   = 1280u;  // 0x500
            lpThis->muBackBufferHeight  = 720u;   // 0x2D0
            lpThis->muFrontBufferWidth  = 1280u;
            lpThis->muFrontBufferHeight = 720u;
            lpThis->muRefreshRate       = 60u;    // 0x3C
        }
        else if (luVideoMode == 5u)
        {
            // 1920x1080 @ 60 Hz.
            lpThis->muBackBufferWidth   = 1920u;  // 0x780
            lpThis->muBackBufferHeight  = 1080u;  // 0x438
            lpThis->muFrontBufferWidth  = 1920u;
            lpThis->muFrontBufferHeight = 1080u;
            lpThis->muRefreshRate       = 60u;    // 0x3C
        }
        else if (luVideoMode < 7u)
        {
            // mode 6: query the current video mode and use its width/height; refresh untouched.
            XVideoModeBlock lVideoMode;
            for (int li = 0; li < 14; ++li)
            {
                lVideoMode.mauDwords[li] = 0u;   // the frame zero-fills 6 qwords before the query
            }
            XGetVideoMode(&lVideoMode);
            const u32 luWidth  = lVideoMode.mauDwords[0];  // v6 = v11[0]
            const u32 luHeight = lVideoMode.mauDwords[1];  // v7 = v11[1]
            lpThis->muBackBufferWidth   = luWidth;
            lpThis->muBackBufferHeight  = luHeight;
            lpThis->muFrontBufferWidth  = luWidth;
            lpThis->muFrontBufferHeight = luHeight;
        }
        // mode 7+ : LABEL_21 -- only the trailing zero-fill below runs (resolution untouched).

        lpThis->muFlag48 = 0u;   // stw 0, 0x48(r31)
        lpThis->muFlag4C = 0u;   // stw 0, 0x4C(r31)
        lpThis->muFlag50 = 0u;   // stw 0, 0x50(r31)
        lpThis->muFlag54 = 0u;   // stw 0, 0x54(r31)
        lpThis->muFlag58 = 0u;   // stw 0, 0x58(r31)
        lpThis->muFlag5C = 0u;   // stw 0, 0x5C(r31)
        lpThis->muFlag60 = 0u;   // stw 0, 0x60(r31)
    }

    // 0x82B617F0 -- fill the three memory-budget dwords with defaults when zero.
    DeviceParameters* DeviceParametersOps::FixD3DDefaults(DeviceParameters* lpResult)
    {
        if (lpResult->muFlag3C == 0u)          // result[15] (+0x3C)
        {
            lpResult->muFlag3C = 0x8000u;
        }
        if (lpResult->muFlag40 == 0u)          // result[16] (+0x40)
        {
            lpResult->muFlag40 = 0x200000u;
        }
        if (lpResult->muFlag44 == 0u)          // result[17] (+0x44)
        {
            const u32 luBudgetB = lpResult->muFlag40;   // v1 = result[16]
            lpResult->muFlag44 = 32u;
            if (luBudgetB > 0x1000000u)
            {
                // extlwi r11, r11, 22, 1 == (v << 1) keeping 22 bits from bit 1 == (2*v) & 0xFFFFFC00
                lpResult->muFlag44 = (2u * luBudgetB) & 0xFFFFFC00u;
            }
        }
        return lpResult;
    }
}
