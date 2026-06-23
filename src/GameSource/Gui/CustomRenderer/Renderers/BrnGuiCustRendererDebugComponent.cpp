#include "BrnBoostBarRenderer.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnGui::GuiCustRendererDebugComponent::UpdateBoostColours @ 0x824F7C48
//
// The debug-menu boost-colour component re-reads the boost-type-indexed inner and outer colours
// from the live BoostBarRenderer and caches them into its own fields so the menu can show them.
// (UpdateBoostColours is driven by the SetBoostColour* callbacks / OnActivate.)
//
// The X360 reaches the renderer's BoostBarRenderer subobject through `*(this+0xC) + 0xE520`; on the
// 64-bit host the offset is not load-bearing, so the resolved BoostBarRenderer pointer is held
// directly and accessed BY NAME (see the header FLAG). The two getters return a Vector3 by value
// (the guest's sret out-pointer + lvx128/stvx128 lane copy is just a 16-byte by-value return). The
// asm then scatters the components: inner.x -> +0x24, inner.y -> +0x28, inner.z -> +0x2C and
// outer.x -> +0x18, outer.y -> +0x1C, outer.z -> +0x20. The return value is the outer-getter's
// result (r3), modelled here as 0 (the guest returns the sret pointer; nothing reads it).

namespace BrnGui
{
int GuiCustRendererDebugComponent::UpdateBoostColours()
{
    // inner colour first (asm: GetInnerBoostBarColour into the v13 stack triple).
    const Vector3 lv3Inner = mpBoostBarRenderer->GetInnerBoostBarColour();

    // outer colour second (asm: GetOuterBoostBarColour into the v16 stack triple).
    const Vector3 lv3Outer = mpBoostBarRenderer->GetOuterBoostBarColour();

    // Tail stores, in the asm's interleaved order: inner.x is stored before the outer block is
    // re-loaded over the shared stack buffer, then inner.y/.z and the outer triple follow.
    mv3InnerColour.x = lv3Inner.x;   // guest +0x24
    mv3InnerColour.y = lv3Inner.y;   // guest +0x28
    mv3InnerColour.z = lv3Inner.z;   // guest +0x2C

    mv3OuterColour.x = lv3Outer.x;   // guest +0x18
    mv3OuterColour.y = lv3Outer.y;   // guest +0x1C
    mv3OuterColour.z = lv3Outer.z;   // guest +0x20

    return 0;
}
}
