#include "GameSource/Sound/Vehicles/Engines/BrnEngineControl.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// =============================================================================
// BrnSound::Vehicles::Engines::EngineControl -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnEngineControl.h for the
// MINIMAL home note and the tri-base (BrnEffectControl + IShiftingActivator
// sub-object) shape rationale.
//
// This TU's recon'd function set is exactly two entries:
//   EngineControl::GetStartRPM                  @ 0x82698FC8
//   EngineControl::`vector deleting destructor'  @ 0x826B2BA0  (-> ~EngineControl anchor)
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

f32 EngineControl::GetStartRPM() const
{
    return mfStartRPM; // lfs f1, 0x18(this)
}

// ---------------------------------------------------------------------------
// ~EngineControl  @ 0x826B2BA0  (the X360 `vector deleting destructor')
//
//   stw  off_820AF228, 0x38(r31)   ; IShiftingActivator sub-object vptr settle (structural;
//                                    see header FLAG -- not a hand-declared base here)
//   stw  off_820AEA6C, 0(r31)      ; primary vptr (EffectControl path)
//   stw  off_820AEA38, 4(r31)      ; (transient) base-class IResourceRequester vptr
//   li   r6, 3 ; stw r6, 0x28(r31) ; meDetachState = E_DETACH_STATE_FINISHED
//   stw  off_820AA820, 4(r31)      ; final IResourceRequester sub-object vptr
//   stb  0, 0x31(r31)              ; mbResourcesReady = false
//   stw  0, 0x24(r31)              ; meAttachState = E_ATTACH_STATE_NONE
//   if (a2 & 1) { ... deallocate via off_82FFB954 (the MemBase allocator) }
//   return this
//
// Byte-identical to the committed BrnEffectControl vector-deleting-destructor @
// 0x826AEF68 (same off_820AEA6C / off_820AEA38 -> off_820AA820 progressive vptr
// settle and the same +0x24/+0x28/+0x31 member teardown) PLUS the one extra
// IShiftingActivator sub-object vptr store at +0x38 -- the same tri-base pattern
// already reconstructed for the sibling ClutchControl / WheelControl homes. All of
// the torn-down members (meAttachState/meDetachState/mbResourcesReady) are owned by
// the inherited BrnEffectControl base; EngineControl itself adds nothing to the
// teardown, so the leaf destructor body is empty (same treatment as WheelControl).
// FLAG: the (a2 & 1) tail invokes the global sound allocator (off_82FFB954) to free
// the object; that allocator is not homed here, so operator-delete dispatch is left
// to the host toolchain (the `delete' half of the X360 vector deleting destructor).
// FLAG: the +0x38 IShiftingActivator sub-object vptr settle is not reproduced as a
// hand-declared base (see header FLAG) to avoid re-forking ShiftControl's own header
// home; it is documented here for asm-parity bookkeeping only.
// ---------------------------------------------------------------------------
EngineControl::~EngineControl()
{
}

s32 EngineControl::GetController(s32 aiSlot)
{
    static const s32 kaiControllers[] = { 0, 3, 2, 7, 1 };
    return (aiSlot >= 0 && aiSlot < static_cast<s32>(sizeof(kaiControllers) / sizeof(kaiControllers[0])))
        ? kaiControllers[aiSlot] : -1;
}

void EngineControl::AttachController(CgsSound::Logic::EffectBase* apController)
{
    switch (apController->GetEffectID())
    {
    case 0:
    case 1:
    case 2:
    case 3:
    case 7:
        return;
    default:
        CGS_ASSERT(false, "false");
        return;
    }
}

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound
