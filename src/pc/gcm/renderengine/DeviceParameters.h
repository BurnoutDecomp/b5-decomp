#pragma once

#include "types.hpp"

// renderengine::Device::Parameters -- the device-creation parameter block the X360 renderengine
// seeds before bringing the GPU device up. It holds the back-buffer resolution / front-buffer
// resolution, the refresh rate, a set of packed format+version constants, and the memory-budget
// dwords the device allocator reads. The reconstruction preserves the X360 object layout and the
// exact field-store order so the device bring-up marshals the same bytes; member accesses are by
// name (named dwords land at the observed X360 offsets).
//
//   Initialize       0x82B61540  -- seed the block from a video-mode index (0..6; 6 = query mode)
//   FixD3DDefaults   0x82B617F0  -- fill in the three memory-budget dwords with defaults if zero
//
// This is a distinct concern from the PC/D3D9 Device bring-up in device.cpp (modelled on TUB at a
// different address space): this is the X360-image Parameters value object, homed in its own .cpp.

namespace renderengine
{
    // The 0x64-byte device parameter block. Field names follow the dwords the X360 image reads /
    // writes; the offsets are the observed X360 store offsets (Initialize touches +0x00..+0x60,
    // FixD3DDefaults the budget dwords +0x3C/+0x40/+0x44). field_28 is written as a byte.
    struct DeviceParameters
    {
        u32 muBackBufferWidth;    // +0x00  back-buffer width
        u32 muBackBufferHeight;   // +0x04  back-buffer height
        u32 muFormatA;            // +0x08  packed format/version constant (0x18280186)
        u32 muFrontBufferWidth;   // +0x0C  front-buffer width
        u32 muFrontBufferHeight;  // +0x10  front-buffer height
        u32 muFormatB;            // +0x14  packed format/version constant (0x28280106)
        u8  muFlag18;             // +0x18  (=1)
        u8  mauPad19[3];          // +0x19  (padding to the next dword store)
        u32 muFlag1C;             // +0x1C  (=0)
        u32 muFlag20;             // +0x20  (=1)
        u32 muFormatC;            // +0x24  packed format/version constant (0x2D200196)
        u8  muFlag28;             // +0x28  (=1, stored as a byte)
        u8  mauPad29[3];          // +0x29  (padding to the next dword store)
        u32 muFlag2C;             // +0x2C  (=0)
        u32 muRefreshRate;        // +0x30  refresh rate (Hz): 50 (PAL), 60 (NTSC/HD), or untouched
        u32 muFlag34;             // +0x34  (=1)
        u32 muFlag38;             // +0x38  (=0)
        u32 muFlag3C;             // +0x3C  memory budget A (FixD3DDefaults default 0x8000)
        u32 muFlag40;             // +0x40  memory budget B (FixD3DDefaults default 0x200000)
        u32 muFlag44;             // +0x44  memory budget C (FixD3DDefaults default 32, or scaled)
        u32 muFlag48;             // +0x48  (=0)
        u32 muFlag4C;             // +0x4C  (=0)
        u32 muFlag50;             // +0x50  (=0)
        u32 muFlag54;             // +0x54  (=0)
        u32 muFlag58;             // +0x58  (=0)
        u32 muFlag5C;             // +0x5C  (=0)
        u32 muFlag60;             // +0x60  (=0)
    };

    // The video-mode query block XGetVideoMode fills (width @ [0], height @ [1], plus the rest of
    // the XVIDEO_MODE struct the X360 frame reserves and zero-fills). Modelled as the dword span the
    // image reads back so the call site marshals the same stack bytes.
    struct XVideoModeBlock
    {
        u32 mauDwords[14];   // the X360 frame reserves 0xA0..var_50 and zero-fills 6 qwords (12 dw)
    };

    class DeviceParametersOps
    {
    public:
        // 0x82B61540 -- seed the parameter block from a video-mode index. Index meaning (from the
        // X360 cr6 compares): 0/2 -> 640x480@?; 1 -> 640x576@50 (PAL); 3 -> 720x480@60; 4 -> 1280x720@60;
        // 5 -> 1920x1080@60; 6+ -> query the current video mode via XGetVideoMode.
        static void Initialize(DeviceParameters* lpThis, u32 luVideoMode);

        // 0x82B617F0 -- fill the three memory-budget dwords (+0x3C/+0x40/+0x44) with defaults when
        // they are zero; the third default scales with the second when the second exceeds 0x1000000.
        static DeviceParameters* FixD3DDefaults(DeviceParameters* lpResult);
    };

    // XGRAPHICS: query the current video mode into an XVIDEO_MODE block (XGetVideoMode). Platform
    // external -- only the signature the call site needs is declared; not reconstructed here.
    void XGetVideoMode(XVideoModeBlock* lpVideoModeOut);
}
