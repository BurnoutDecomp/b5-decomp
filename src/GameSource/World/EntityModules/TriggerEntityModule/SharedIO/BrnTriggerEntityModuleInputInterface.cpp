// ===== Owning TU: BrnWorld::TriggerEntityModuleIO::TriggerManagementInputInterface::AddTriggerRegion
// X360 0x8238ECF8. The world-side add-box-trigger producer: called by
// BrnGameState::TriggerQueryManager::UpdateTriggers to arm one trigger region for the frame.
//
// Reconstructed from the ARTIST X360 pseudocode + assembly (spine). It reads the region's leading
// BoxRegion (dimensions + ComputeTransform), the TriggerRegion region-index/type bytes, and the
// GenericRegion category byte, packs them into a 96-byte InAddBoxTriggerEvent, and posts it onto
// the interface's embedded add queue (VariableEventQueue<131072,16>) as event type 2.
#include "GameSource/World/EntityModules/TriggerEntityModule/SharedIO/BrnTriggerEntityModuleInputInterface.h"

#include "SharedClasses/Trigger/BrnGenericRegion.h"  // BrnTrigger::TriggerRegion / GenericRegion / BoxRegion


namespace BrnWorld
{
namespace TriggerEntityModuleIO
{
    // X360 0x8238ECF8. Faithful de-optimisation of the asm: the vector loads/stores that build the
    // event on the stack become named field writes; the AddEvent<InAddBoxTriggerEvent> tail call
    // (event type 2) is the embedded add queue's typed AddEvent.
    void TriggerManagementInputInterface::AddTriggerRegion(
        s32 liQueryFlags, const BrnTrigger::TriggerRegion* lpRegion)
    {
        const BrnTrigger::BoxRegion* lpBox = lpRegion->GetBoxRegion();

        InAddBoxTriggerEvent lEvent;

        // event +0x50: abs(box dimensions). X360 loads [dimX,dimY,dimZ,0] then `vandc` clears each
        // lane's sign bit -> fabs per lane (the 4th lane stays 0.0).
        Vector3 lDimensions = lpBox->GetDimensions();
        const Vector3 lPackedDimensions(static_cast<f32>(lDimensions.GetX()),
                                        static_cast<f32>(lDimensions.GetY()),
                                        static_cast<f32>(lDimensions.GetZ()));
        const __vector4 lSign = __vslw(__vspltisw(-1), __vspltisw(-1));
        lEvent.mDimensions = __vandc(lPackedDimensions.mV, lSign);

        // event +0x40: (queryFlags << 24) | sign-extended region index (asm: slwi r8,r4,24 /
        // lhz+extsh r9 / or r9,r8,r9). GetRegionIndex() is the sign-extended 16-bit index.
        lEvent.mTriggerID =
            (static_cast<u32>(liQueryFlags) << 24) |
            static_cast<u32>(static_cast<s32>(lpRegion->GetRegionIndex()));

        // event +0x00: the box's world transform (BoxRegion::ComputeTransform -- own TU).
        lEvent.mTransform = lpBox->ComputeTransform();

        // event +0x44: the trigger-region kind byte (lbz 0x2A).
        lEvent.miTriggerRegionType = static_cast<u8>(lpRegion->GetType());

        // event +0x45: 0xFF when the region IS a generic region, else the GenericRegion category
        // byte at +0x36 (asm: cmplwi meType,2 -> beq set -1, else lbz 0x36). Reproduced exactly.
        if (lpRegion->GetType() == BrnTrigger::TriggerRegion::E_TYPE_GENERIC_REGION)
        {
            lEvent.miGenericRegionType = 0xFFu;
        }
        else
        {
            lEvent.miGenericRegionType =
                static_cast<u8>(static_cast<const BrnTrigger::GenericRegion*>(lpRegion)->GetType());
        }

        // X360 tail call: AddEvent<InAddBoxTriggerEvent>(this, &event, 2). mAddTriggerEventQueue is
        // the interface's first member (+0), so posting on it matches the asm's `this`-relative call.
        mAddTriggerEventQueue.AddEvent<InAddBoxTriggerEvent>(&lEvent, 2);
    }
}
}
