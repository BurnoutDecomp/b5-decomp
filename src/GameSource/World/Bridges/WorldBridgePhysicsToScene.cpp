#include "GameSource/World/Bridges/WorldBridgePhysicsToScene.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"  // VehicleOutputRequestInterface::GetRequestFineLineQueue (the +0x28C0 seat)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventLineTestNearest.h"   // InEventLineTestNearest (type 6)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventVolumeTestDeepest.h" // InEventVolumeTestDeepest (type 7)


// @ 0x827ABA40 -- append the physics module's staged scene-update events (read-locked
// getter @0x8279F838, the +179424 scene sub-interface) into the scene manager's
// update input buffer (write-locked getter @0x825BD8C0).
// Both null tripwires are NON-gating (the X360 falls through after firing); the
// X360 tail returns the forwarded call's result as a register artifact -- the
// logical return type is void.
//
// ---- The console body (0x827ABA40..0x827ABAC4), instruction for instruction --------------
//   r4 = lpSceneInputBuffer_Update (dest), r5 = lpPhysicsModuleOutputBuffer (src);
//   r3 (the WorldModule `this`) is overwritten at 0x827ABAA4 and never read.
//   if (!dest) FireAssert("lpSceneInputBuffer_Update != NULL",   <this file>, 0x67 == 103)
//   if (!src)  FireAssert("lpPhysicsModuleOutputBuffer != NULL", <this file>, 0x68 == 104)
//   0x827ABAA8  bl 0x8279F838  PhysicsModuleIO::OutputBuffer::GetSceneInputInterface() const
//                              -- read-lock (bit 4), baked BrnPhysicsModuleIO.h:366, this+179424
//   0x827ABAB4  bl 0x825BD8C0  SceneManagerIO::InputBuffer_Update::GetInSceneUpdateInterface()
//                              -- write-lock (bit 3), baked CgsSceneManagerModuleIO.h:463, this+16
//   0x827ABABC  bl 0x827A9340  InSceneUpdateInterface::Append(dest, src)
//   ⚠ THE SOURCE GETTER RUNS FIRST. Both getters are lock tripwires, so their order is
//   behaviour; C++ leaves argument-evaluation order unspecified, hence the two locals below.
//
// ⭐ UNBLOCKED 2026-08-19 (wave Q5 cluster F2). This body was written long ago and never
// mounted because PhysicsModuleIO::OutputBuffer modelled its scene seat as a 1-byte opaque
// storage: the source had no type to Append from and nothing ever ran its Construct. Both
// facts are fixed in BrnPhysicsModuleIO.h / BrnPhysicsModuleIO_OutputBuffer.cpp this wave
// (the member IS SceneManagerIO::InSceneUpdateInterface -- the console's own Construct
// @0x825ABBEC names the type -- and OutputBuffer::Construct now runs its Construct), so the
// reinterpret_cast seam that used to stand here is DELETED: both sides are one type.
namespace WorldModule
{
// @ 0x827ABA40
void BridgePhysicsSceneUpdateToScene(
    void* lpWorldModule,
    CgsSceneManager::SceneManagerIO::InputBuffer_Update* lpSceneInputBuffer_Update,
    const BrnPhysics::PhysicsModuleIO::OutputBuffer* lpPhysicsModuleOutputBuffer)
{
    (void)lpWorldModule;

    CGS_ASSERT(lpSceneInputBuffer_Update != 0, "lpSceneInputBuffer_Update != NULL");           // :103
    CGS_ASSERT(lpPhysicsModuleOutputBuffer != 0, "lpPhysicsModuleOutputBuffer != NULL");       // :104

    // The console's evaluation order: source (read-locked) first, destination (write-locked)
    // second, then the whole-interface merge.
    const CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* const lpSource =
        lpPhysicsModuleOutputBuffer->GetSceneInputInterface();                     // @0x8279F838
    CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* const lpDestination =
        lpSceneInputBuffer_Update->GetInSceneUpdateInterface();                    // @0x825BD8C0

    lpDestination->Append(*lpSource);                                              // @0x827A9340
}
// =============================================================================================
// @ 0x827A8D20 -- BridgePhysicsSceneQueriesToScene (90 insns). Scene-query wave 1, 2026-09-02.
//
// Drain the physics module's staged scene-query REQUESTS -- the VariableEventQueue<13440,16>
// every VehicleManager pass posts into (GenerateAboveGroundLineTests @0x82633990 posts one type-6
// InEventLineTestNearest per live race car) -- into the scene manager's query input buffer,
// typed by the request's event id:
//   6 -> InputBuffer_Query::AddLineTestNearestQuery   (the nearest-line queue,   console +0xB060)
//   7 -> InputBuffer_Query::AddVolumeTestDeepestQuery (the deepest-volume queue, console +0xF790)
//   anything else is dropped.
//
// ---- The console body (0x827A8D20..0x827A8E84), instruction for instruction ---------------
//   r4 = lpSceneInputBuffer_Query (dest), r5 = lpPhysicsModuleOutputBuffer (src); r3 (the
//   WorldModule `this`) is never read.
//   if (!dest) FireAssert("lpSceneInputBuffer_Query != NULL",  <this file>, 0x28 == 40)
//   if (!src)  FireAssert("lpPhysicsModuleOutputBuffer != NULL", <this file>, 0x29 == 41)
//   0x827A8D88  bl 0x8279F448  PhysicsModuleIO::OutputBuffer::GetVehicleOutputRequestInterface() const
//                              -- read-lock (bit 4) tripwire, BrnPhysicsModuleIO.h:298, this+16
//   0x827A8D8C  addi r27, r3, 0x28C0    == &interface->mRequestFineLineQueue (the inlined
//                                           VehicleOutputRequestInterface::GetRequestFineLineQueue)
//   0x827A8D90  cmplwi r27, 0 ; bne     -- the :49 tripwire "Could not get physics vehicle
//               requests interface pointer" (only reachable if the getter returned -0x28C0;
//               a member-address can never be null on the host, so it is a compile-time truth)
//   0x827A8E18  GetFirstEvent(queue, &event, &size) ; loop while id >= 0:
//   0x827A8E38    id == 6 -> BaseEventQueue<InEventLineTestNearest>::AddEvent(dest + 0xB060, *event)
//   0x827A8E40    id == 7 -> BaseEventQueue<InEventVolumeTestDeepest>::AddEvent(dest + 0xF790, *event)
//   0x827A8E74    GetNextEvent(queue, event, &event, &size)
//   The tail returns GetNextEvent's -1 as a register artifact; the logical return type is void.
//
// ⭐ THE TWO EVENT IDS ARE THE CONTRACT WITH THE PRODUCER. GenerateAboveGroundLineTests passes
// `li r5, 6` to AddEvent<InEventLineTestNearest>; nothing but this switch ties 6 to the nearest
// queue. Quoted as literals here, next to the producer's own constant name.
//
// ⛔ THIS BRIDGE WAS AN INERT WorldLinkStubs GATE UNTIL THIS WAVE, which is why every race car's
// above-ground down-ray went unanswered: AboveGroundTestResult.mbValid stayed 0 on every frame
// of every session and UpdateDriftState @0x8261F94C guard 8 killed every brake+steer drift on
// the frame it was entered (134 of 134, measured last wave). The gate's comment said the
// consumer was "itself gated inert" -- false since SceneManagerModule::ProcessSceneQueries became
// real on 2026-08-11 (and its two passes land with this wave).
// =============================================================================================
void BridgePhysicsSceneQueriesToScene(
    void* lpWorldModule,
    CgsSceneManager::SceneManagerIO::InputBuffer_Query* lpSceneInputBuffer_Query,
    const BrnPhysics::PhysicsModuleIO::OutputBuffer* lpPhysicsModuleOutputBuffer)
{
    (void)lpWorldModule;

    CGS_ASSERT(lpSceneInputBuffer_Query != 0,    "lpSceneInputBuffer_Query != NULL");     // :40
    CGS_ASSERT(lpPhysicsModuleOutputBuffer != 0, "lpPhysicsModuleOutputBuffer != NULL");  // :41

    // The producer's own ids (BrnVehicleManager_UpdateVehiclePhysics.cpp,
    // KI_SCENE_QUERY_EVENT_LINE_TEST_NEAREST == 6); 7 is the deepest-volume request.
    static const s32 KI_REQUEST_LINE_TEST_NEAREST   = 6;
    static const s32 KI_REQUEST_VOLUME_TEST_DEEPEST = 7;

    const BrnPhysics::Vehicle::VehicleOutputRequestInterface::OutFineQueryQueue* lpRequests =
        lpPhysicsModuleOutputBuffer->GetVehicleOutputRequestInterface()->GetRequestFineLineQueue();  // @0x8279F448 + 0x28C0
    CGS_ASSERT(lpRequests != 0, "Could not get physics vehicle requests interface pointer");        // :49

    const CgsModule::Event* lpEvent = 0;
    s32                     liSize  = 0;
    for (s32 liId = lpRequests->GetFirstEvent(&lpEvent, &liSize);
         liId >= 0;
         liId = lpRequests->GetNextEvent(lpEvent, &lpEvent, &liSize))
    {
        if (liId == KI_REQUEST_LINE_TEST_NEAREST)
        {
            // The queue hands back the packed record bytes (a CgsModule::Event*); the record IS
            // an InEventLineTestNearest (the producer AddEvent'd one, 64 bytes) -- the sanctioned
            // external-byte-stream read every consumer of a VariableEventQueue in the tree uses.
            lpSceneInputBuffer_Query->AddLineTestNearestQuery(
                *reinterpret_cast<const CgsSceneManager::SceneManagerIO::InEventLineTestNearest*>(lpEvent));
        }
        else if (liId == KI_REQUEST_VOLUME_TEST_DEEPEST)
        {
            lpSceneInputBuffer_Query->AddVolumeTestDeepestQuery(
                *reinterpret_cast<const CgsSceneManager::SceneManagerIO::InEventVolumeTestDeepest*>(lpEvent));
        }
    }
}

}
