// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourLooseAttachment.cpp
//
// Compilation home for the BrnDirector::Camera::BehaviourLooseAttachment slice this TU owns:
//   - BehaviourLooseAttachment::AttachTo      @0x821F4458
//   - BehaviourLooseAttachment::Get           @0x821FAA58
//   - BehaviourLooseAttachment::SetParameters @0x821F43E8
//   - BehaviourLooseAttachment::SetTarget     @0x821F44B8
//
// The four methods are defined inline in the header; this .cpp is the translation-unit anchor that
// pulls the header into the compile gate and pins the asm-attested member offsets. Installed by the
// new-car-joined / shutdown-takedown moments and the testbed arbitrator state.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourLooseAttachment.h"
#include <cstddef>   // offsetof

namespace BrnDirector
{
namespace Camera
{

// Pin the asm-attested member offsets the four methods touch.
static_assert(offsetof(BehaviourLooseAttachment, mParamWord1)          == 0x010, "param word 1 @ +0x10");
static_assert(offsetof(BehaviourLooseAttachment, miTargetSet)          == 0x304, "target set flag @ +0x304");
static_assert(offsetof(BehaviourLooseAttachment, meTargetRaceCarIndex) == 0x308, "target index @ +0x308");
static_assert(offsetof(BehaviourLooseAttachment, miTargetField30C)     == 0x30C, "target field @ +0x30C");
static_assert(offsetof(BehaviourLooseAttachment, mbTargetField310)     == 0x310, "target byte @ +0x310");
static_assert(offsetof(BehaviourLooseAttachment, miAttachSet)          == 0x314, "attach set flag @ +0x314");
static_assert(offsetof(BehaviourLooseAttachment, meAttachRaceCarIndex) == 0x318, "attach index @ +0x318");
static_assert(offsetof(BehaviourLooseAttachment, miAttachField31C)     == 0x31C, "attach field @ +0x31C");
static_assert(offsetof(BehaviourLooseAttachment, mbAttachField320)     == 0x320, "attach byte @ +0x320");
static_assert(offsetof(BehaviourLooseAttachment, muParametersSlot)     == 0x324, "params slot @ +0x324");
static_assert(offsetof(BehaviourLooseAttachment, mbNoResult)           == 0x32C, "no-result flag @ +0x32C");

// Out-of-line anchors: force the four inline methods to be emitted in this TU.
void BehaviourLooseAttachment_AttachToAnchor(BehaviourLooseAttachment& lr, s32 li)      { lr.AttachTo(li); }
void* BehaviourLooseAttachment_GetAnchor(BehaviourLooseAttachment& lr)                  { return lr.Get(); }
void BehaviourLooseAttachment_SetParametersAnchor(BehaviourLooseAttachment& lr,
                                                   const BehaviourLooseAttachment::Parameters* lp) { lr.SetParameters(lp); }
void BehaviourLooseAttachment_SetTargetAnchor(BehaviourLooseAttachment& lr, s32 li)     { lr.SetTarget(li); }

} // namespace Camera
} // namespace BrnDirector
