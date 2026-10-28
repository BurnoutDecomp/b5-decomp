#pragma once

#include "types.hpp"
#include <cstddef>
#include "SDKs/RenderEngineClub/MAIN/components/src/states/resourcedescriptor.h"

// Native ARTIST state and construction parameters; no host renderer storage.
namespace renderengine
{
class DepthStencilState
{
public:
    // Depth-test comparison function the immediate-mode library selects per state
    // (CgsGraphics::ImRendererBase::ConstructDepthStencilState passes one of these as the
    // leading parameter word). Only the values the X360 state-library builder uses are attested;
    // E_FUNCTION_ALWAYS == 7 is the value the asm stores for the depth/stencil func words
    // (the "always pass" function), so it is named; the rest are left unenumerated until attested.
    enum Function
    {
        E_FUNCTION_ALWAYS = 7,
    };

    // The 0x4A-byte depth/stencil parameter block the immediate-mode builder fills positionally
    // (X360 asm: 17 state words at +0x00..+0x40, then 6 flag bytes at +0x44..+0x49). Only the words
    // the builder writes are named; the rest are zeroed. muFunction is the leading word (the
    // requested comparison function).
    //
    // NOTE (2026-08-12): Initialize @0x82B62890 is now known to copy this block POSITIONALLY into
    // the object (Parameters word N -> object word N; Parameters byte 0x44+N -> object word
    // 0x44+4*N), which means every field below has a known destination and therefore a known
    // meaning -- see the per-field destination comments. The placeholder names (muState4,
    // muState8, ...) are kept ONLY because three consumers already spell them that way
    // (CgsDepthStencilStateFactory.cpp, CgsImRenderer.cpp, ImmediateMode.cpp); renaming
    // them is a follow-up that has to land together with those files.
    struct Parameters
    {
        u32 muFunction;        // +0x00 -> muZFunc                 (the Function arg)
        u32 maState1[3];       // +0x04 -> muStencilFail / muStencilZFail / muStencilPass (zeroed)
        u32 muState4;          // +0x10 -> muStencilFunc           == 7 (E_FUNCTION_ALWAYS)
        u32 maState5[3];       // +0x14 -> muCcwStencilFail / ZFail / Pass (zeroed)
        u32 muState8;          // +0x20 -> muCcwStencilFunc        == 7 (E_FUNCTION_ALWAYS)
        u32 muState9;          // +0x24 -> muHiStencilFunc         (zeroed)
        u32 muState10;         // +0x28 -> muStencilRef            (zeroed)
        u32 muStencilReadMask; // +0x2C -> muStencilMask           == -1
        u32 muStencilWriteMask;// +0x30 -> muStencilWriteMask      == -1
        u32 muState13;         // +0x34 -> muCcwStencilRef         (zeroed)
        u32 muState14;         // +0x38 -> muCcwStencilMask        == -1
        u32 muState15;         // +0x3C -> muCcwStencilWriteMask   == -1
        u32 muState16;         // +0x40 -> muHiStencilRef          (zeroed)
        u8  mbDepthTestEnable; // +0x44 -> muZEnable               (the first bool arg)
        u8  mbDepthWriteEnable;// +0x45 -> muZWriteEnable          (the second bool arg)
        u8  mu8Flag2;          // +0x46 -> muStencilEnable         (zeroed)
        u8  mu8Flag3;          // +0x47 -> muTwoSidedStencilMode   (zeroed)
        u8  mu8Flag4;          // +0x48 -> muHiStencilEnable       (zeroed)
        u8  mu8Flag5;          // +0x49 -> muHiStencilWriteEnable  (zeroed)
    };

    // The builder passes parameters alongside the descriptor; this fixed-size sizer ignores them.
    static ResourceDescriptor5* GetResourceDescriptor(ResourceDescriptor5* lpDescriptor, const Parameters* lpParameters);
    static DepthStencilState* Initialize(DepthStencilState** ppState, const Parameters* lpParameters);

    // The 0x60-byte object the X360 GetResourceDescriptor (0x82B636F8) sizes ({0x60, 4} entry-0
    // qword) and the serialised world MaterialState blob embeds: the 17 marshalled state words,
    // then the six Parameters flag bytes stw-widened to words, then the initialised word
    // Initialize sets to 1. All three groups are DIRECTLY attested by
    // renderengine::DepthStencilState::Initialize @0x82B62890 (17x lwz/stw, 6x lbz->stw at
    // 0x44/0x48/0x4C/0x50/0x54/0x58, li 1 -> stw 0x5C) -- recovered from the ARTIST .i64, which
    // names the function; it is merely absent from the JSON export subset.
    //
    // Every word's meaning is attested by the X360 depth/stencil half of the dispatch state
    // triple (@0x827E8150), which pushes each one through its own D3DDevice_SetRenderState_*
    // fast-set. Note that the comparison words carry the XENOS D3DCMPFUNC encoding, which is the
    // GPU's own 3-bit function field and is ZERO-based (NEVER=0 .. ALWAYS=7) -- attested by
    // D3DDevice_SetRenderState_ZFunc @0x82939870 packing `(16*Value) & 0x70` into RB_DEPTHCONTROL
    // and D3DDevice_SetRenderState_AlphaFunc @0x82939328 packing `Value & 7`.
    u32 muZFunc;                   // +0x00 -> ZFunc
    u32 muStencilFail;             // +0x04 -> StencilFail
    u32 muStencilZFail;            // +0x08 -> StencilZFail
    u32 muStencilPass;             // +0x0C -> StencilPass
    u32 muStencilFunc;             // +0x10 -> StencilFunc
    u32 muCcwStencilFail;          // +0x14 -> CCWStencilFail
    u32 muCcwStencilZFail;         // +0x18 -> CCWStencilZFail
    u32 muCcwStencilPass;          // +0x1C -> CCWStencilPass
    u32 muCcwStencilFunc;          // +0x20 -> CCWStencilFunc
    u32 muHiStencilFunc;           // +0x24 -> HiStencilFunc
    u32 muStencilRef;              // +0x28 -> StencilRef
    u32 muStencilMask;             // +0x2C -> StencilMask
    u32 muStencilWriteMask;        // +0x30 -> StencilWriteMask
    u32 muCcwStencilRef;           // +0x34 -> CCWStencilRef
    u32 muCcwStencilMask;          // +0x38 -> CCWStencilMask
    u32 muCcwStencilWriteMask;     // +0x3C -> CCWStencilWriteMask
    u32 muHiStencilRef;            // +0x40 -> HiStencilRef
    u32 muZEnable;                 // +0x44 -> ZEnable          (Parameters::mbDepthTestEnable)
    u32 muZWriteEnable;            // +0x48 -> ZWriteEnable      (Parameters::mbDepthWriteEnable)
    u32 muStencilEnable;           // +0x4C -> StencilEnable
    u32 muTwoSidedStencilMode;     // +0x50 -> TwoSidedStencilMode
    u32 muHiStencilEnable;         // +0x54 -> HiStencilEnable
    u32 muHiStencilWriteEnable;    // +0x58 -> HiStencilWriteEnable
    u32 muInitialised;             // +0x5C 1 once Initialize has run

    // Never called; exists only to pin the recovered layout. A member-function BODY is a
    // complete-class context (a bare static_assert in the class body would not be), so offsetof
    // on the members is legal here.
    //
    // Each offset is attested by both the native initializer and the state applier:
    //   * the word INDEX each member is read at by the dispatch applier
    //     shadow::Device::Xbox2SetDepthStencilStateLowLevelShadowed @0x827E8150 (a1[0]..a1[22]);
    //   * the byte OFFSET each member is stored at by renderengine::DepthStencilState::Initialize
    //     @0x82B62890 (stw ..., 0x00(r3) .. 0x5C(r3)).
    static void _AssertLayout()
    {
        CGS_STATIC_ASSERT(sizeof(DepthStencilState) == 0x60, "X360 GetResourceDescriptor @0x82B636F8 sizes the object at 0x60");

        CGS_STATIC_ASSERT(offsetof(DepthStencilState, muZFunc)                == 0x00, "a1[0]  -> ZFunc");
        CGS_STATIC_ASSERT(offsetof(DepthStencilState, muStencilFail)          == 0x04, "a1[1]  -> StencilFail");
        CGS_STATIC_ASSERT(offsetof(DepthStencilState, muStencilZFail)         == 0x08, "a1[2]  -> StencilZFail");
        CGS_STATIC_ASSERT(offsetof(DepthStencilState, muStencilPass)          == 0x0C, "a1[3]  -> StencilPass");
        CGS_STATIC_ASSERT(offsetof(DepthStencilState, muStencilFunc)          == 0x10, "a1[4]  -> StencilFunc");
        CGS_STATIC_ASSERT(offsetof(DepthStencilState, muCcwStencilFail)       == 0x14, "a1[5]  -> CCWStencilFail");
        CGS_STATIC_ASSERT(offsetof(DepthStencilState, muCcwStencilZFail)      == 0x18, "a1[6]  -> CCWStencilZFail");
        CGS_STATIC_ASSERT(offsetof(DepthStencilState, muCcwStencilPass)       == 0x1C, "a1[7]  -> CCWStencilPass");
        CGS_STATIC_ASSERT(offsetof(DepthStencilState, muCcwStencilFunc)       == 0x20, "a1[8]  -> CCWStencilFunc");
        CGS_STATIC_ASSERT(offsetof(DepthStencilState, muHiStencilFunc)        == 0x24, "a1[9]  -> HiStencilFunc");
        CGS_STATIC_ASSERT(offsetof(DepthStencilState, muStencilRef)           == 0x28, "a1[10] -> StencilRef");
        CGS_STATIC_ASSERT(offsetof(DepthStencilState, muStencilMask)          == 0x2C, "a1[11] -> StencilMask");
        CGS_STATIC_ASSERT(offsetof(DepthStencilState, muStencilWriteMask)     == 0x30, "a1[12] -> StencilWriteMask");
        CGS_STATIC_ASSERT(offsetof(DepthStencilState, muCcwStencilRef)        == 0x34, "a1[13] -> CCWStencilRef");
        CGS_STATIC_ASSERT(offsetof(DepthStencilState, muCcwStencilMask)       == 0x38, "a1[14] -> CCWStencilMask");
        CGS_STATIC_ASSERT(offsetof(DepthStencilState, muCcwStencilWriteMask)  == 0x3C, "a1[15] -> CCWStencilWriteMask");
        CGS_STATIC_ASSERT(offsetof(DepthStencilState, muHiStencilRef)         == 0x40, "a1[16] -> HiStencilRef");
        CGS_STATIC_ASSERT(offsetof(DepthStencilState, muZEnable)              == 0x44, "a1[17] -> ZEnable");
        CGS_STATIC_ASSERT(offsetof(DepthStencilState, muZWriteEnable)         == 0x48, "a1[18] -> ZWriteEnable");
        CGS_STATIC_ASSERT(offsetof(DepthStencilState, muStencilEnable)        == 0x4C, "a1[19] -> StencilEnable");
        CGS_STATIC_ASSERT(offsetof(DepthStencilState, muTwoSidedStencilMode)  == 0x50, "a1[20] -> TwoSidedStencilMode");
        CGS_STATIC_ASSERT(offsetof(DepthStencilState, muHiStencilEnable)      == 0x54, "a1[21] -> HiStencilEnable");
        CGS_STATIC_ASSERT(offsetof(DepthStencilState, muHiStencilWriteEnable) == 0x58, "a1[22] -> HiStencilWriteEnable");
        CGS_STATIC_ASSERT(offsetof(DepthStencilState, muInitialised)          == 0x5C, "Initialize's li 1 / stw 0x5C");

        // Parameters: every offset the Initialize @0x82B62890 copy reads. Also pointer-invariant
        // (17 u32 + 6 u8). The six flag BYTES are what makes the block 0x4A, not 0x5C -- getting
        // this wrong would shear the whole widened tail.
        CGS_STATIC_ASSERT(offsetof(Parameters, muFunction)         == 0x00, "lwz 0x00(r4)");
        CGS_STATIC_ASSERT(offsetof(Parameters, maState1)           == 0x04, "lwz 0x04/0x08/0x0C(r4)");
        CGS_STATIC_ASSERT(offsetof(Parameters, muState4)           == 0x10, "lwz 0x10(r4)");
        CGS_STATIC_ASSERT(offsetof(Parameters, maState5)           == 0x14, "lwz 0x14/0x18/0x1C(r4)");
        CGS_STATIC_ASSERT(offsetof(Parameters, muState8)           == 0x20, "lwz 0x20(r4)");
        CGS_STATIC_ASSERT(offsetof(Parameters, muStencilReadMask)  == 0x2C, "lwz 0x2C(r4)");
        CGS_STATIC_ASSERT(offsetof(Parameters, muStencilWriteMask) == 0x30, "lwz 0x30(r4)");
        CGS_STATIC_ASSERT(offsetof(Parameters, muState16)          == 0x40, "lwz 0x40(r4) -- last word copy");
        CGS_STATIC_ASSERT(offsetof(Parameters, mbDepthTestEnable)  == 0x44, "lbz 0x44(r4)");
        CGS_STATIC_ASSERT(offsetof(Parameters, mbDepthWriteEnable) == 0x45, "lbz 0x45(r4)");
        CGS_STATIC_ASSERT(offsetof(Parameters, mu8Flag2)           == 0x46, "lbz 0x46(r4)");
        CGS_STATIC_ASSERT(offsetof(Parameters, mu8Flag3)           == 0x47, "lbz 0x47(r4)");
        CGS_STATIC_ASSERT(offsetof(Parameters, mu8Flag4)           == 0x48, "lbz 0x48(r4)");
        CGS_STATIC_ASSERT(offsetof(Parameters, mu8Flag5)           == 0x49, "lbz 0x49(r4)");
    }
};
}
