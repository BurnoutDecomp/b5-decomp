#include "GameSource/Sound/Vehicles/Engines/BrnHybridEngineControl.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Sound/Vehicles/Engines/BrnPhysicsControl.h"
#include "GameSource/Sound/Vehicles/Engines/BrnEngineControl.h"
#include "GameSource/Sound/Vehicles/Engines/BrnShiftControl.h"
#include "GameSource/Sound/Vehicles/Engines/BrnClutchControl.h"
#include "GameSource/AttribSys/Generated/attrib_findcollection.h"

// ARTIST compiled this file through the GameSource unity file; its asserts carry this path.
#define BRN_HYBRIDENGINECONTROL_CPP_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Sound/Vehicles/Engines/BrnHybridEngineControl.cpp"

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
    // DataPoint::Update (previous = current, current = the paired exhaust's sample): the argument
    // is read before either store, as ARTIST loads both values first (0x82699B9C / 0x82699BA0).
    mGinsuRpm.Update(mpHybridExhaustControl->GetGinsuRPM());
}

// ---------------------------------------------------------------------------
// HybridEngineControl::CreateObject(u32)  @ 0x826CC738   (the RTTI factory hook)
//
// The X360 allocates a 320-byte (0x140) block through CgsSound::MemBase::operator
// new(size, tag, flavour) tagged "HybridEngineControl" and inline-constructs a
// HybridEngineControl, returning it upcast to CgsSound::Logic::EffectControl*. `a1`
// only selects the operator-new flavour (0/1); both arms use the same size + ctor.
//
// Tagged allocation now dispatches through the shared sound MemBase allocator.
// ---------------------------------------------------------------------------
CgsSound::Logic::EffectControl* HybridEngineControl::CreateObject(u32 luType)
{
    if (luType == 0)
        return new (sTypeInfo.typeName, false) HybridEngineControl();
    else
        return new (sTypeInfo.typeName, true) HybridEngineControl();
}

// ARTIST 0x82F2F5B8: {0x10060, "HybridEngineControl", HybridExhaustControl, CreateObject}, registered by
// its startup thunk through ClassTypeInfo<EffectControl>::AddToClassTypeInfoArray.
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl> HybridEngineControl::sTypeInfo(
    0x10060, "HybridEngineControl", &BrnSound::Vehicles::Engines::HybridExhaustControl::sTypeInfo, &HybridEngineControl::CreateObject);

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* HybridEngineControl::GetStaticTypeInfo()
{
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* HybridEngineControl::GetTypeInfo() const
{
    return &sTypeInfo;
}

const char* HybridEngineControl::GetTypeName() const
{
    return sTypeInfo.typeName;
}

bool HybridEngineControl::Attach()
{
    if (!HybridExhaustControl::Attach())
        return false;

    // ARTIST 0x82699BEC reads mpHybridExhaustControl with no assert or null test.
    const u64 luMasterCollection =
        mpHybridExhaustControl->GetVehicleEngineAttributes().CollectionKey();
    Attrib::Collection* lpMasterCollection = Attrib::FindCollectionWithDefault(
        0x7F161D94482CB3BFull, luMasterCollection);
    mMasterVehicleEngineComponentAttributes.Change(lpMasterCollection);
    return true;
}

s32 HybridEngineControl::GetController(s32 aiSlot)
{
    // ARTIST 0x82684AB0: a switch over the controller slot (unsigned range test).
    switch (aiSlot)
    {
    case 0:
        return 0;
    case 1:
        return 4;
    case 2:
        return 2;
    case 3:
        return 3;
    case 4:
        return 5;
    case 5:
        return 13;
    default:
        return -1;
    }
}

void HybridEngineControl::AttachController(CgsSound::Logic::EffectBase* apController)
{
    // ARTIST 0x82684B20: one switch over ids 0..13 (no call to the base AttachController): the
    // base's four controllers, the hybrid exhaust control (5), 13 ignored, any other id asserts.
    switch (apController->GetEffectID())
    {
    case 0:
        mpPhysicsControl = static_cast<PhysicsControl*>(apController);
        break;
    case 4:
        mpEngineControl = static_cast<EngineControl*>(apController);
        break;
    case 2:
        mpShiftControl = static_cast<ShiftControl*>(apController);
        break;
    case 3:
        mpClutchControl = static_cast<ClutchControl*>(apController);
        break;
    case 5:
        mpHybridExhaustControl = static_cast<HybridExhaustControl*>(apController);
        break;
    case 13:
        break;
    default:
        CGS_FAIL_AT("I don't know how to handle attaching this type of EffectBase.",
                      BRN_HYBRIDENGINECONTROL_CPP_FILE, 1005);
        break;
    }
}

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound
