#include "renderstates.h"

namespace renderengine
{
// X360 0x82B636F8: five {0, 1} entries, then the entry-0 qword store 0x0000006000000004
// = { muSize 0x60, muAlignment 4 } -- the full 24-word object (17 state words + 6 widened
// flag words + the initialised word), matching the serialised world MaterialState blocks.
ResourceDescriptor5* DepthStencilState::GetResourceDescriptor(ResourceDescriptor5* lpDescriptor)
{
    for (ResourceDescriptor5::Entry& lEntry : lpDescriptor->maEntries)
    {
        lEntry.muSize = 0;
        lEntry.muAlignment = 1;
    }

    lpDescriptor->maEntries[0].muSize = static_cast<u32>(sizeof(DepthStencilState));  // X360: 0x60
    lpDescriptor->maEntries[0].muAlignment = 4;
    static_assert(sizeof(DepthStencilState) == 0x60, "the X360 descriptor sizes the object at 0x60");
    return lpDescriptor;
}

// X360 the immediate-mode state-library builder passes the params alongside the out descriptor; the
// descriptor build is a fixed { size, align } block and ignores the params. Delegates to the 1-arg form.
ResourceDescriptor5* DepthStencilState::GetResourceDescriptor(
    ResourceDescriptor5* lpDescriptor, const Parameters* /*lpParameters*/)
{
    return GetResourceDescriptor(lpDescriptor);
}

// Create a depth/stencil state from the parameter block (the allocator has already carved the backing
// store the descriptor sized; *ppState is the first handle).
//
// X360 renderengine::DepthStencilState::Initialize @0x82B62890 (0x82B62890..0x82B62957; the function
// IS in the ARTIST .i64 and IS named -- it is only absent from the JSON export subset, so it was
// recovered headlessly). The body is a fully unrolled straight-line field copy, exactly the shape of
// its siblings BlendState::Initialize @0x82B627C8 and RasterizerState::Initialize @0x82B62958:
//
//     lwz r10, 0x00(r4) / stw r10, 0x00(r3)   ... through ...  lwz 0x40 / stw 0x40   (17 word copies)
//     lbz r10, 0x44(r4) / stw r10, 0x44(r3)     bytes 0x44..0x49 of Parameters widen, one per word,
//     lbz r10, 0x45(r4) / stw r10, 0x48(r3)     into the object words at 0x44/0x48/0x4C/0x50/0x54/0x58
//     lbz r10, 0x46(r4) / stw r10, 0x4C(r3)
//     lbz r10, 0x47(r4) / stw r10, 0x50(r3)
//     lbz r10, 0x48(r4) / stw r10, 0x54(r3)
//     lbz r10, 0x49(r4) / stw r10, 0x58(r3)
//     li  r11, 1        / stw r11, 0x5C(r3)     the initialised word
//
// Unlike the two siblings (whose parameter block is permuted relative to the marshalled block) the
// depth/stencil copy is POSITIONAL: Parameters word N lands in object word N, and Parameters byte
// 0x44+N lands in object word 0x44+4*N. So each destination below is named by the object member the
// dispatch applier (shadow::Device::Xbox2SetDepthStencilStateLowLevelShadowed @0x827E8150) pushes
// through the matching D3DDevice_SetRenderState_* fast-set. The source names are the (still partly
// unattested) Parameters field names; see the note on Parameters in renderstates.h.
DepthStencilState* DepthStencilState::Initialize(DepthStencilState** ppState, const Parameters* lpParameters)
{
    DepthStencilState* lpState = *ppState;

    // The 17 marshalled state words (Parameters +0x00..+0x40 -> object +0x00..+0x40).
    lpState->muZFunc                = lpParameters->muFunction;          // +0x00 -> ZFunc
    lpState->muStencilFail          = lpParameters->maState1[0];         // +0x04 -> StencilFail
    lpState->muStencilZFail         = lpParameters->maState1[1];         // +0x08 -> StencilZFail
    lpState->muStencilPass          = lpParameters->maState1[2];         // +0x0C -> StencilPass
    lpState->muStencilFunc          = lpParameters->muState4;            // +0x10 -> StencilFunc
    lpState->muCcwStencilFail       = lpParameters->maState5[0];         // +0x14 -> CCWStencilFail
    lpState->muCcwStencilZFail      = lpParameters->maState5[1];         // +0x18 -> CCWStencilZFail
    lpState->muCcwStencilPass       = lpParameters->maState5[2];         // +0x1C -> CCWStencilPass
    lpState->muCcwStencilFunc       = lpParameters->muState8;            // +0x20 -> CCWStencilFunc
    lpState->muHiStencilFunc        = lpParameters->muState9;            // +0x24 -> HiStencilFunc
    lpState->muStencilRef           = lpParameters->muState10;           // +0x28 -> StencilRef
    lpState->muStencilMask          = lpParameters->muStencilReadMask;   // +0x2C -> StencilMask
    lpState->muStencilWriteMask     = lpParameters->muStencilWriteMask;  // +0x30 -> StencilWriteMask
    lpState->muCcwStencilRef        = lpParameters->muState13;           // +0x34 -> CCWStencilRef
    lpState->muCcwStencilMask       = lpParameters->muState14;           // +0x38 -> CCWStencilMask
    lpState->muCcwStencilWriteMask  = lpParameters->muState15;           // +0x3C -> CCWStencilWriteMask
    lpState->muHiStencilRef         = lpParameters->muState16;           // +0x40 -> HiStencilRef

    // The six Parameters flag BYTES (+0x44..+0x49), each lbz->stw widened into its own word.
    lpState->muZEnable              = lpParameters->mbDepthTestEnable;   // +0x44 -> ZEnable
    lpState->muZWriteEnable         = lpParameters->mbDepthWriteEnable;  // +0x48 -> ZWriteEnable
    lpState->muStencilEnable        = lpParameters->mu8Flag2;            // +0x4C -> StencilEnable
    lpState->muTwoSidedStencilMode  = lpParameters->mu8Flag3;            // +0x50 -> TwoSidedStencilMode
    lpState->muHiStencilEnable      = lpParameters->mu8Flag4;            // +0x54 -> HiStencilEnable
    lpState->muHiStencilWriteEnable = lpParameters->mu8Flag5;            // +0x58 -> HiStencilWriteEnable

    lpState->muInitialised = 1u;                                        // +0x5C  li 1 / stw
    return lpState;
}
}
