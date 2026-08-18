#pragma once

// Canonical home for BrnPhysics::ContactSpy::ContactSpyInterface (DWARF home
// GameSource/Physics/ContactSpies/BrnContactSpyInterface.h:48).
//
// The by-value contact-spy HANDLE embedded in several IO buffers (PhysicsModuleIO::
// OutputBuffer +998192, WorldModuleIO UpdateOutputBuffer, AIModuleIO / PropEntityModuleIO /
// RaceCarEntityModuleIO InputBuffer_PostPhysics): one pointer at a published ContactSpyData
// aggregate, plus pass-through accessors.
//
// ⭐ PROMOTED 2026-08-06 (bridge de-facade wave): the leading slot is now the DWARF's own
//    `ContactSpyData* mpData` (BrnContactSpyInterface.h:130) instead of the provisional
//    `u32 muField0`. The old spelling was a 4-byte word "to match the committed Construct body
//    verbatim" -- but on this 64-bit host it TRUNCATED the pointer GetRaceCarContactRunList
//    round-tripped through it (reinterpret_cast<const u8*>(static_cast<uintptr_t>(muField0))),
//    a latent defect its own header banner scheduled for exactly this retype ("rename to the
//    typed pointer when ContactSpyData lands"). ContactSpyData has landed; the pointer widens
//    4 -> 8 like every other IO-buffer pointer member (IO buffers are runtime-only, never
//    serialized -- the serialized-slots-stay-32-bit rule does not apply).
//    Embedder ripple, paid explicitly: alignment 4 -> 8 moves the PropEntityModuleIO
//    InputBuffer_PostPhysics member from +0x04 to +0x08; that TU's pin is retyped to the
//    adjacency form (see its banner).
//
// SetData / IsEmpty are DWARF methods (:56 / :71) newly ATTESTED by
// PhysicsModule::BridgeSimulationToOutput @0x825B0448, which inlines both:
//   * IsEmpty  @0x825B04EC..0x825B0514: lwz mpData; NULL -> empty; else bl ContactSpyData::IsEmpty.
//   * SetData  @0x825B05F0..0x825B062C: the "lpData != NULL" FireAssert cites THIS header,
//     line 264 (the console's inline body line), then stores the pointer.
// Both were header-inline on the console (no out-of-line emission in the image) and are
// header-inline here.

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT (SetData :264 tripwire)
#include "GameSource/Physics/ContactSpies/BrnContactSpyData.h"     // ContactSpyData (pointee + IsEmpty)

namespace BrnPhysics
{
namespace ContactSpy
{
    // DWARF: BrnContactSpyInterface.h:48 (struct BrnPhysics::ContactSpy::ContactSpyInterface).
    struct ContactSpyInterface
    {
        ContactSpyInterface* Construct();

        // DWARF :56. Inline on console -- BridgeSimulationToOutput @0x825B062C carries the
        // body's own "lpData != NULL" tripwire (FireAssert file/line = this header :264).
        void SetData(ContactSpyData* lpData)
        {
            CGS_ASSERT(lpData != nullptr, "lpData != NULL");
            mpData = lpData;
        }

        // DWARF :71. Inline on console (BridgeSimulationToOutput @0x825B04EC..0x825B0514):
        // an unbound interface reads as empty; a bound one defers to the aggregate.
        bool IsEmpty() const
        {
            return mpData == nullptr || mpData->IsEmpty();
        }

        // X360 0x82355BF0 (DWARF BrnContactSpyInterface.h:110): asserts mpData != NULL then
        // returns &mpData->mRaceCarContactRunList (console byte offset 0x198D0 into
        // ContactSpyData). RETYPED with the promotion (was `const void*` over the u32 slot).
        const ContactSpyData::RaceCarContactRunList* GetRaceCarContactRunList() const;

        // ⭐ NEW 2026-08-18 (wave Q round 2). DWARF BrnContactSpyInterface.h:42
        //     `const ContactSpyData::PropContactQueue * GetPropContacts() const;`
        // INLINE (attested): the X360 emits no out-of-line symbol; it folds the accessor into
        // BrnWorld::PropEntityModule::ProcessContacts @0x822FA970..0x822FA9A8 --
        //     lwz     r11, 0(r31)          ; mpData
        //     cmplwi  r11, 0 ; bne         ; the guard
        //     FireAssert("mpData != NULL", "..\..\..\GameSource\Physics/ContactSpies/…", 0xD3)
        //     lwz     r11, 0(r31)          ; re-read mpData
        //     addis   r3, r11, 1 ; addi r3, r3, 0x67E0     ; mpData + 0x167E0
        // -- i.e. the assert this header's own line 211 bakes (0xD3 == 211), then the
        // pass-through to ContactSpyData::GetPropContacts(), whose +0x167E0 IS
        // ContactSpyData::mPropContactQueue in that class's member table. Unblocks
        // ProcessContacts, the only consumer: mpData is private and the interface previously
        // exposed only GetRaceCarContactRunList(), so there was no route to the prop contacts
        // at all.
        const ContactSpyData::PropContactQueue* GetPropContacts() const
        {
            CGS_ASSERT(mpData != nullptr, "mpData != NULL");
            return mpData->GetPropContacts();
        }

    private:
        // DWARF BrnContactSpyInterface.h:130. The single published-aggregate pointer
        // (the console's 32-bit slot; widens to 8 on this host -- see the banner).
        ContactSpyData* mpData;
    };
}
}
