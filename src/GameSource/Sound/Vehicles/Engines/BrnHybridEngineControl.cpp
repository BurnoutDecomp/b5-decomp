#include "GameSource/Sound/Vehicles/Engines/BrnHybridEngineControl.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/AttribSys/Generated/attrib_findcollection.h"

// =============================================================================
// BrnSound::Vehicles::Engines::HybridEngineControl -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. DWARF: HybridEngineControl : public
// HybridExhaustControl. Recon'd function set:
//   UpdateGinsuRPM                 @ 0x82699B98
//   CreateObject(u32)              @ 0x826CC738
//   `scalar deleting destructor'   @ 0x826CC7F0  (-> ~HybridEngineControl anchor)
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

// ---------------------------------------------------------------------------
// HybridEngineControl::UpdateGinsuRPM  @ 0x82699B98  (single-step Ginsu RPM shift)
//   lwz  r11, 0x130(this)   ; mpHybridExhaustControl (the OWN derived member --
//                             sizeof(HybridExhaustControl) == 0x130 per its Create
//                             @0x826B34E0, so +0x130 is this leaf's first slot;
//                             DWARF BrnHybridEngineControl.h:208 names it)
//   lfs  f13, 0x8C(this)    ; mGinsuRpm.mCurrentValue (old current)
//   lfs  f0,  0x8C(r11)     ; the paired exhaust control's mGinsuRpm.mCurrentValue
//                             (== HybridExhaustControl::GetGinsuRPM(), DWARF h:177)
//   stfs f13, 0x90(this)    ; mGinsuRpm.mPreviousValue = old current
//   stfs f0,  0x8C(this)    ; mGinsuRpm.mCurrentValue  = new sample
//
// (2026-08-25, audio-faithfulness wave 6 evidence pass: an earlier revision misbound
// the +0x130 load to the BASE's mpEngineControl and byte-viewed the pointee -- the
// base ends at +0x130, so the slot is this class's own paired-exhaust back-pointer,
// and the pointee's +0x8C is the exhaust control's own mGinsuRpm current, proven by
// the HybridExhaustControl ctor @0x826AF938 DataPoint block at +0x7C/+0x84/+0x8C.)
// ---------------------------------------------------------------------------
void HybridEngineControl::UpdateGinsuRPM()
{
    mGinsuRpm.mPreviousValue = mGinsuRpm.mCurrentValue;               // previous = old current
    mGinsuRpm.mCurrentValue  = mpHybridExhaustControl->GetGinsuRPM(); // current  = the paired exhaust's sample
}

// ---------------------------------------------------------------------------
// HybridEngineControl::CreateObject(u32)  @ 0x826CC738   (the RTTI factory hook)
//
// The X360 allocates a 320-byte (0x140) block through CgsSound::MemBase::operator
// new(size, tag, flavour) tagged "HybridEngineControl" and inline-constructs a
// HybridEngineControl, returning it upcast to CgsSound::Logic::EffectControl*. `a1`
// only selects the operator-new flavour (0/1); both arms use the same size + ctor.
//
// FLAG (allocator gate): CgsSound::MemBase does NOT model operator new(size, tag,
// flavour) here, so this uses the host `new`; the observable result matches. Mirrors
// the committed CollisionControl::CreateObject. The 0x140 size is documentation only.
// ---------------------------------------------------------------------------
CgsSound::Logic::EffectControl* HybridEngineControl::CreateObject( u32 /*luType*/ )
{
    return new HybridEngineControl();
}

// ---------------------------------------------------------------------------
// ~HybridEngineControl  @ 0x826CC7F0  (anchor for the X360 `scalar deleting destructor').
// The X360 chains HybridExhaustControl::~HybridExhaustControl; HybridEngineControl adds
// no member teardown of its own, so this leaf body is empty. The (a2 & 1) allocator-free
// tail is re-synthesised by the host toolchain (off_82FFB954 not homed here).
// ---------------------------------------------------------------------------
HybridEngineControl::~HybridEngineControl()
{
}

bool HybridEngineControl::Attach()
{
    if (!HybridExhaustControl::Attach())
        return false;

    CGS_ASSERT(mpHybridExhaustControl != nullptr, "mpHybridExhaustControl");
    if (!mpHybridExhaustControl)
        return false;

    const u64 luMasterCollection =
        mpHybridExhaustControl->GetVehicleEngineAttributes().CollectionKey();
    Attrib::Collection* lpMasterCollection = Attrib::FindCollectionWithDefault(
        0x7F161D94482CB3BFull, luMasterCollection);
    mMasterVehicleEngineComponentAttributes.Change(lpMasterCollection);
    return true;
}

s32 HybridEngineControl::GetController(s32 aiSlot)
{
    static const s32 kaiControllers[] = { 0, 4, 2, 3, 5, 13 };
    return (aiSlot >= 0 && aiSlot < static_cast<s32>(sizeof(kaiControllers) / sizeof(kaiControllers[0])))
        ? kaiControllers[aiSlot] : -1;
}

void HybridEngineControl::AttachController(CgsSound::Logic::EffectBase* apController)
{
    if (apController->GetEffectID() == 5)
    {
        mpHybridExhaustControl = static_cast<HybridExhaustControl*>(apController);
        return;
    }
    if (apController->GetEffectID() == 13)
        return;
    HybridExhaustControl::AttachController(apController);
}

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound
