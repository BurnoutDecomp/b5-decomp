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
        // accessor -- byte offset (index 6) is asm-proven; the NAME is the best-fit DWARF accessor
        // (:76 GetRaceCarWithWorldQueueValidated) FLAGGED as unproven (the accessor->index binding is
        // not recoverable from this build). Host addressing uses the typed member index, so it stays
        // layout-correct without the X360 32-bit byte offset.
        const CustomPotentialContactQueue& GetRaceCarWithWorldQueueValidated() const { return maCustomEventQueues[6]; }

    private:
        const InPotentialContactQueue* mpQueue;                    // +4  :246 (const source queue)
        CustomPotentialContactQueue    maCustomEventQueues[KI_CUSTOM_QUEUE_COUNT]; // +16 :247
    };
}
}
