// ============================================================================
// b5-decomp/src/GameSource/Physics/BrnPhysicsModuleIO_PotentialContactInterface.h
//
// BrnPhysics::PhysicsModuleIO::PotentialContactInterface -- the physics module's
// potential-contact IO sub-interface. Reconstructed from BURNOUT_X360_ARTIST.XEX
// (Construct 0x825A96C8, SetConstQueue 0x825A03C8, GetLength 0x825A0498,
// AddEvent 0x825E72F0, GetEvent 0x825A0578) with member NAMES/TYPES/ORDER from the
// DecFIGS DWARF (references/DecFIGS/dwarfdump/GameSource/Physics/BrnPhysicsModuleIO.h,
// struct @:24; members mpQueue :30 (:246), maCustomEventQueues :37 (:247)).
//
// LAYOUT (DWARF member order + X360 store offsets):
//   base   CgsModule::IOBuffer                     (1-byte status @+0)
//   +4      const InPotentialContactQueue* mpQueue                          :246
//   +16     CustomPotentialContactQueue    maCustomEventQueues[14]          :247
//
// Offset proof (X360, 32-bit): Construct sets the constructed flag at +0 and nulls mpQueue
// at +4 (stw 0,4(r3)); the first custom queue's base is at +16 (addi r31,r3,0x20 = maEvents;
// queue base = maEvents-0x10 = +16), each EventQueue<T,2048> having stride 0x28010
// (=163856 = 16-byte queue header + 2048*80). GetLength reads maCustomEventQueues[0].miLength
// at this+0x18 (queue0 base +16, miLength +8) and mpQueue->miLength at mpQueue+8. NB: no
// offsetof pins are emitted -- mpQueue is a raw pointer that widens to 8 bytes on the 64-bit
// host, so the X360 32-bit member offsets are not reproducible/assertable here.
//
// InPotentialContactQueue == OutputBuffer::OutPotentialContactQueue ==
// EventQueue<CgsSceneManager::SceneManagerIO::PotentialContact,2048> (DWARF
// BrnPhysicsModuleIO.h:26/:34). Both the const source queue and the 14 custom queues are that
// same fixed-capacity type; the element is the already-committed 80-byte
// CgsSceneManager::SceneManagerIO::PotentialContact (SharedIO/CgsPotentialContact.h). Only
// the five X360-emitted methods of this batch are bodied; the ~30 Get*Queue accessors
// (DWARF :162-241) are declared additively as they land.
//
// NOTE (latent, pre-existing): a DIFFERENT, incompatible minimal definition of this same
// fully-qualified class is homed in
// GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalBodyPartPool.h
// (nested CustomPotentialContactQueue = {mpContacts, mu32Pad, miNumContacts}, accessors
// GetHingedBodyPartWithWorldQueue/GetHingedBodyPartWithCarQueue over
// BrnPhysics::Deformation::PotentialContact). No TU includes BOTH headers today, so the build
// is clean; but they cannot coexist in one TU. Rehoming BodyPartPool onto this authoritative
// type is deferred (out of scope for landing this IO family) -- see the wave5 report FLAG.
// ============================================================================
#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"                        // CgsModule::IOBuffer
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                      // CgsModule::EventQueue
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h" // CgsSceneManager::SceneManagerIO::PotentialContact
#include "GameSource/Physics/ContactSpies/BrnContactId.h"                     // BrnPhysics::ContactId (GetEvent(ContactId) key)

namespace BrnPhysics
{
namespace PhysicsModuleIO
{
    struct PotentialContactInterface : public CgsModule::IOBuffer
    {
        // DWARF BrnPhysicsModuleIO.h:34 / :26.
        typedef CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::PotentialContact, 2048> CustomPotentialContactQueue;
        // DWARF names this OutputBuffer::OutPotentialContactQueue; it is the same fixed-cap
        // EventQueue<PotentialContact,2048> instantiation (that TU is not in this slice, so it
        // is aliased to CustomPotentialContactQueue here -- identical underlying type).
        typedef CustomPotentialContactQueue                                                    InPotentialContactQueue;

        static const s32 KI_CUSTOM_QUEUE_COUNT = 14;   // Construct loop count (r30 = 0xE)

        // ---- X360-emitted methods bodied in this slice --------------------------------
        void Construct();                                          // @0x825A96C8  :41 (:134)
        void SetConstQueue(const InPotentialContactQueue* lpQueue); // @0x825A03C8 :44 (:138) write-lock
        void AddEvent(const CgsSceneManager::SceneManagerIO::PotentialContact& lEvent); // @0x825E72F0 :47 (:142) write-lock

        // ⭐ ADDED 2026-08-14 (walls leg 3, harvest wave). DWARF :51 (:147, body hint :601): the
        // CUSTOM-QUEUE AddEvent overload -- inline in the console header (assert
        // BrnPhysicsModuleIO.h:596), which is why the X360 also emits an out-of-line local copy
        // (sub_825E73D0) that AddContactResultsToQueue / EndPartContactGeneration /
        // AddArticulatedJointContacts call. Assert the queue id against E_NUM_CUSTOM_QUEUE_TYPES,
        // warn on a full queue ("PHYSICS WARNING: Run out of space in contact queue N"), then the
        // bounds-gated AddEventSafe into maCustomEventQueues[luQueueID]. NO lock tripwire -- the
        // console body carries none (it runs under the caller's write lock).
        void AddEvent(u32 luQueueID, const CgsSceneManager::SceneManagerIO::PotentialContact& lrEvent); // @0x825E73D0 :51 (:147)
        s32  GetLength() const;                                    // @0x825A0498  :59 (:150) read-lock
        const CgsSceneManager::SceneManagerIO::PotentialContact& GetEvent(s32 liIndex) const; // @0x825A0578 :62 (:154) read-lock

        // ⭐ ADDED 2026-08-06 (bridge de-facade wave). DWARF :65 (:159): the ContactId-keyed
        // accessor @0x825A06A0 -- resolve a contact spy's muTag back to its potential-contact
        // record. Queue id 0 (E_QUEUE_TYPE_EXTERNAL_FROM_SCENE_CONTACTS) routes through
        // GetEvent(s32) (the mpQueue/custom split); any other id indexes maCustomEventQueues
        // [queue id] directly with its own bounds tripwire (BrnPhysicsModuleIO.h:674). Called by
        // PhysicsModule::ProcessContactSpy @0x825AB5A4. Bodied in the sibling .cpp.
        const CgsSceneManager::SceneManagerIO::PotentialContact& GetEvent(ContactId lContactId) const; // @0x825A06A0 :65 (:159)

        // Read access to the custom sub-queue the race-car-vs-world crash-prediction pass consumes.
        // The X360 crash-prediction driver (VehicleManager::HandleCrashPredictionForRaceCarAndWorld
        // @0x82640C28) reaches this queue via an inlined `this + 983152`, which is
        // maCustomEventQueues[6] (983152 == 16-byte base + 6 * 0x28010 stride). ADDITIVE inline
        // accessor -- byte offset (index 6) is asm-proven.
        // ⭐ NAME PROVEN 2026-08-14 (walls leg 3): the old "FLAGGED as unproven" caveat retires --
        // DoRaceCarWorldContactValidation @0x825EB6C8 asserts
        // "lpPotentialContactInterface->GetRaceCarWithWorldQueueValidated()->GetLength() == 0"
        // (BrnVehicleManagerContactGeneration.cpp:1384) against EXACTLY this+983152, binding the
        // DWARF accessor name to index 6 by the console's own assert string.
        const CustomPotentialContactQueue& GetRaceCarWithWorldQueueValidated() const { return maCustomEventQueues[6]; }

        // ⭐ ADDED 2026-08-14 (walls leg 3): the RAW (unvalidated) race-car-vs-world queue,
        // maCustomEventQueues[5]. Index binding asm-proven twice over: the harvest posts race-car
        // world contacts with UserTagA == 5 (DoRaceCarWorldContactGeneration's baked `li 5` queue
        // selector -> AddContactResultsToQueue -> AddEvent(5, ...)), and
        // DoRaceCarWorldContactValidation drains `this + 819296` == 16 + 5*0x28010 before
        // appending survivors to [6] "Validated". NAME is the DWARF accessor
        // (BrnPhysicsModuleIO.h dwarfdump :80 GetRaceCarWithWorldQueue) -- the [5]-raw / [6]-
        // validated pairing is exactly the raw/Validated name split.
        const CustomPotentialContactQueue& GetRaceCarWithWorldQueue() const { return maCustomEventQueues[5]; }

        // ⭐ ADDED 2026-08-14 (walls leg 4). DeformationManager::AddArticulatedJointContacts
        // @0x825DB190 (PS3 0x739FAC names the drain verbatim) walks `maCustomEventQueues[12]` --
        // the articulated-joint (traffic cab/trailer) contact queue; both entity ids are asserted
        // TRAFFIC_VEHICLE owners at :1033/:1034. Same additive-inline pattern as [5]/[6] above.
        const CustomPotentialContactQueue& GetArticulatedJointQueue() const { return maCustomEventQueues[12]; }

        // ⭐ ADDED 2026-08-14 (walls leg 4). The two hinged-body-part queues the pool's
        // UpdateJoinedParts drains -- index binding attested by the header's own offset notes
        // (ifc+0x118080 == 16 + 7*stride -> [7] hinged-vs-CAR; ifc+0x140090 == 16 + 8*stride ->
        // [8] hinged-vs-WORLD, owner asserts (RACECAR_DEFORMABLE_PART, ...)).
        const CustomPotentialContactQueue& GetHingedBodyPartWithCarQueue()   const { return maCustomEventQueues[7]; }
        const CustomPotentialContactQueue& GetHingedBodyPartWithWorldQueue() const { return maCustomEventQueues[8]; }

        // ⭐ ADDED 2026-08-06 (FixUpVehicleContacts wave). Three more custom-queue accessors, same
        // ADDITIVE pattern as [6] above -- byte offsets (indices) are asm-proven from
        // PhysicsModule::FixUpVehicleContacts @0x825A6010, which walks:
        //     ifc+0x140090 == 16 + 8*0x28010  ->  maCustomEventQueues[8]   (queue asserts owner
        //         pair (RACECAR, TRAFFIC_VEHICLE) @BrnPhysicsModuleUpdateFunctions.cpp:947)
        //     ifc+0x118080 == 16 + 7*0x28010  ->  maCustomEventQueues[7]   (asserts (RACECAR,
        //         RACECAR) @:975)
        //     ifc+0x10     == 16 + 0*0x28010  ->  maCustomEventQueues[0]   (FILTERED, not
        //         asserted, for (TRAFFIC, TRAFFIC) pairs)
        // NAMES: [8]/[7] carry the DWARF accessor names whose meaning the driver's own owner
        // asserts prove (GetRaceCarWithTrafficQueue :107 / GetRaceCarWithRaceCarQueue :98).
        // [0]'s name is the best-fit DWARF accessor (:113 GetSceneManagerContactQueue -- the
        // driver FILTERS owner pairs instead of asserting them, i.e. the queue holds mixed
        // scene-manager contacts) FLAGGED as unproven, exactly like [6]'s.
        const CustomPotentialContactQueue& GetRaceCarWithTrafficQueue() const { return maCustomEventQueues[8]; }
        const CustomPotentialContactQueue& GetRaceCarWithRaceCarQueue() const { return maCustomEventQueues[7]; }
        const CustomPotentialContactQueue& GetSceneManagerContactQueue() const { return maCustomEventQueues[0]; }

        // ⭐ ADDED 2026-08-06 (BridgeContactsToSimulation wave). Two more custom-queue accessors,
        // same ADDITIVE pattern -- byte offsets (indices) are asm-proven from PhysicsModule::
        // BridgeContactsToSimulation @0x825A99E8, which walks:
        //     ifc+0x168090 == 16 + 9*0x28010  ->  maCustomEventQueues[9]  (loop asserts owner pair
        //         (TRAFFIC_VEHICLE, WORLD) @BrnPhysicsModuleBridgeFunctions.cpp:334/:335)
        //     ifc+0x208110 == 16 + 13*0x28010 ->  maCustomEventQueues[13] (loop asserts owner pair
        //         (TRAFFIC_VEHICLE, TRAFFIC_VEHICLE) @:410/:411)
        // NAMES: the DWARF accessors whose meaning those owner asserts prove
        // (GetTrafficWithWorldQueue :83 / GetTrafficWithTrafficQueue :119). ⭐ Note the bridge's
        // per-queue ContactId owner byte EQUALS the queue index everywhere it walks a queue
        // ([6]->0x6000000, [7]->0x7..., [8]->0x8..., [9]->0x9..., [13]->0xD...) -- five
        // independent confirmations of the index<->meaning binding.
        const CustomPotentialContactQueue& GetTrafficWithWorldQueue() const { return maCustomEventQueues[9]; }
        const CustomPotentialContactQueue& GetTrafficWithTrafficQueue() const { return maCustomEventQueues[13]; }

        // ADDED 2026-08-22 (wave T3 C5). VehicleManager::DoTrafficWorldContactOrdering @0x825C8F18
        // REWRITES the records of two queues in place, so it needs the DWARF's non-const overloads
        // (BrnPhysicsModuleIO.h:86 GetTrafficWithWorldQueue(), :89/:92 GetSimpleTrafficWithWorldQueue()).
        // Index proof from that body: `a2 + 1474720` == 16 + 9*0x28010 -> [9]; `a2 + 1638576` ==
        // 16 + 10*0x28010 -> [10]. [10] is the SIMPLE/box traffic-world queue -- the same `li r7, 0xA`
        // user tag DoTrafficCarWorldContactGeneration's two CollidePrimitiveListAgainstTriangleList
        // arms pass. (Reference return matches this header's committed accessor style; the DWARF
        // spells them pointer-returning.)
        CustomPotentialContactQueue&       GetTrafficWithWorldQueue()       { return maCustomEventQueues[9]; }
        const CustomPotentialContactQueue& GetSimpleTrafficWithWorldQueue() const { return maCustomEventQueues[10]; }
        CustomPotentialContactQueue&       GetSimpleTrafficWithWorldQueue()       { return maCustomEventQueues[10]; }

    private:
        const InPotentialContactQueue* mpQueue;                    // +4  :246 (const source queue)
        CustomPotentialContactQueue    maCustomEventQueues[KI_CUSTOM_QUEUE_COUNT]; // +16 :247
    };
}
}
