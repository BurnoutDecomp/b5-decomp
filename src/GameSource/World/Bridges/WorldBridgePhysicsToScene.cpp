#include "GameSource/World/Bridges/WorldBridgePhysicsToScene.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT


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
}
