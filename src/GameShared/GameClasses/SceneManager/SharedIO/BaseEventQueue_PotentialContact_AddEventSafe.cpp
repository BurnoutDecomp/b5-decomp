#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                  // BaseEventQueue<T>::AddEventSafe (inline generic)
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h"  // CgsSceneManager::SceneManagerIO::PotentialContact (80-byte element)

// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::PotentialContact>::AddEventSafe
//   @ X360 0x825E49F8 (dossier id "class:CgsSceneManager::SceneManagerIO::PotentialContact>").
//
// The generic BaseEventQueue<T>::AddEventSafe body is already inline in CgsBaseEventQueue.h;
// this is the thin explicit instantiation. The X360 body matches the generic store-for-store:
//   * asserts mpEvents != NULL (CgsBaseEventQueue.h:331 "mpEvents != NULL" tripwire,
//     `lwz r11,0(r31)`; bne skips the assert);
//   * bounds-gated full check: `lwz r11,8(r31)` (miLength) vs `lwz r10,4(r31)` (miMaxLength),
//     `bge` => return 0 WITHOUT appending when miLength >= miMaxLength;
//   * otherwise copies the 80-byte element to mpEvents[miLength] at an 80-byte stride
//     (`slwi r9,r11,2; add r11,r11,r9; slwi r11,r11,4` == miLength*5*16 == miLength*80;
//     ctr=10 std/ld 64-bit block moves == 80 bytes), bumps miLength (`stw r11,8(r31)`)
//     and returns 1.
// The 80-byte stride matches sizeof(CgsSceneManager::SceneManagerIO::PotentialContact) == 80
// (3x 16-byte Vector3 @0/16/32 + 2x 8-byte VolumeInstanceId @48/56 + 2x 4-byte poly tag @64/68
// + 2x 2-byte primitive index @72/74 = 76, alignas(16) padded to 80; see CgsPotentialContact.h,
// already committed). Called by
// BrnPhysics::PhysicsModuleIO::PotentialContactInterface::AddEvent,
// BrnPhysics::Vehicle::VehicleManager::DoRaceCarWorldContactValidation,
// BrnPhysics::Vehicle::PhysicalTrafficManager::AddArticulatedJointContacts, and sub_825E73D0.
template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::PotentialContact>::AddEventSafe(
    const CgsSceneManager::SceneManagerIO::PotentialContact&);