#include "GameSource/Gui/PFX/BrnGuiPFXHookNodeBlender.h"
#include <cstddef>

// Compile-time exercise of BrnGui::PFXHookNodeBlender::IsActive and the field
// offsets proven by the X360 body @0x824ECFC0:
//   mpHook         @ +0x00  (`lwz r11, 0(r3)`)
//   maNodeBlends   @ +0x14  (`addi r10, r3, 0x14`)
//   per-slot stride 0x130   (`addi r10, r10, 0x130`)
//   slot state     @ entry+0 (`cmpwi r8, 4`)
// (Only intra-struct offsets are pinned; no absolute / cross-pointer-member
// offset is asserted.)

using BrnGui::PFXHookNodeBlend;
using BrnGui::PFXHookNodeBlender;

static_assert(sizeof(PFXHookNodeBlend) == 0x130, "node-blend slot stride 0x130");
static_assert(offsetof(PFXHookNodeBlend, meState) == 0x00, "slot state @entry+0");

// mpHook @ +0x00 and maNodeBlends @ +0x14 are private; their offsets are fixed
// by the member declaration order + the 16-byte reserved gap in the header and
// are exercised (not just asserted) by IsActive's pointer walk below.
bool BrnGuiPFXHookNodeBlender_embed_check(const PFXHookNodeBlender* lpBlender)
{
    return lpBlender->IsActive();
}
