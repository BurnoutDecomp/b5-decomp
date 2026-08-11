#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleInputInterface.h"          // VehicleInputInterface + GetTriangleCacheInterface
#include "GameSource/Physics/BrnPhysicsModuleIO_PotentialContactInterface.h"              // PotentialContactInterface + GetRaceCarWithWorldQueueValidated
#include "GameSource/Physics/PhysicsUtilities/BrnPotentialContactOrderer.h"               // PotentialContactOrderer (+ PotentialContact typedef)
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h"             // CgsSceneManager::SceneManagerIO::PotentialContact (muVolumeInstanceIdA)

// BrnPhysics::Vehicle::VehicleManager::HandleCrashPredictionForRaceCarAndWorld  @0x82640C28.
//
// The race-car-vs-world crash-prediction driver (called by DoCrashPrediction). It walks the
// physics module's already-validated race-car-vs-world potential-contact queue and processes it in
// contiguous GROUPS -- a group is a run of contacts that share the same volume-A entity word (the
// high dword of muVolumeInstanceIdA, i.e. all the world contacts against ONE race car). For each
// group it orders the contained contacts by predicted impact time (PotentialContactOrderer, which
// also prunes contacts that sit behind an earlier kept contact's plane), then dispatches every
// surviving contact to HandleRaceCarWorldPotentialContact. The DWARF types the return as void; the
// Hex-Rays `int result` return is the leftover r3 of the last call and is dropped.
//
// Store-for-store notes:
//   * v8  = lpContactInterface + 983152  -> maCustomEventQueues[6] (byte-proven), exposed as
//           GetRaceCarWithWorldQueueValidated() (accessor NAME flagged; index 6 is asm-proven).
//   * i   = high dword of the current contact's muVolumeInstanceIdA (asm `ld 0x30; srdi 32; clrlwi`).
//   * v47 = 0 (orderer.miNumEntries) at start + after each group flush -> PotentialContactOrderer::Construct().
//   * The contact is passed BY VALUE to HandleRaceCarWorldPotentialContact (asm marshals the 80-byte
//     record across r4-r10 + a stack tail via memcpy); the by-value C++ parameter reproduces that copy.
//   * a4 + 128016 = &VehicleInputInterface::mTriangleCacheInterface -> GetTriangleCacheInterface().
//   * PredictCarWorldContactTime returns a VecFloat; the driver reads its first lane (asm `lfs 0(r11)`)
//     as the impact time fed to AddContact.

namespace BrnPhysics
{
namespace Vehicle
{
    void VehicleManager::HandleCrashPredictionForRaceCarAndWorld(
        f32 lfTimestep,
        BrnPhysics::PhysicsModuleIO::PotentialContactInterface* lpContactInterface,
        const VehicleInputInterface* lpVehicleInputInterface,
        BrnGameState::GameStateModuleIO::VehicleOutputInterface* lpVehicleOutputInterface,
        BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
        VehicleManagerOutputInterface* lpManagerOutputInterface,
        BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface)
    {
        typedef CgsSceneManager::SceneManagerIO::PotentialContact PotentialContact;

        // v8 = lpContactInterface + 983152 -> the validated race-car-vs-world contact sub-queue.
        const PhysicsModuleIO::PotentialContactInterface::CustomPotentialContactQueue& lQueue =
            lpContactInterface->GetRaceCarWithWorldQueueValidated();

        // a4 + 128016 -> the triangle-cache interface forwarded to every dispatched contact.
        const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpTriangleCache =
            lpVehicleInputInterface->GetTriangleCacheInterface();

        PotentialContactOrderer lOrderer;
        lOrderer.Construct();   // v47 = 0

        if (lQueue.GetLength() > 0)
        {
            // The current group id: the volume-A entity word of the first queued contact.
            u32 luCurrentGroup =
                static_cast<u32>(lQueue.GetEvent(0).muVolumeInstanceIdA.muId >> 32);

            for (s32 liIndex = 0; liIndex < lQueue.GetLength(); ++liIndex)
            {
                const PotentialContact& lContact = lQueue.GetEvent(liIndex);
                const u32 luGroup =
                    static_cast<u32>(lContact.muVolumeInstanceIdA.muId >> 32);

                if (luGroup != luCurrentGroup)
                {
                    // Finished a group -- order it, then dispatch every surviving contact.
                    const s32 liValid = lOrderer.SortContacts();
                    for (s32 liDispatch = 0; liDispatch < liValid; ++liDispatch)
                    {
                        const PotentialContact* lpOrdered = lOrderer.GetContact(liDispatch);
                        HandleRaceCarWorldPotentialContact(
                            *lpOrdered,
                            lpRequestOutputInterface,
                            lpVehicleOutputInterface,
                            lpManagerOutputInterface,
                            lpDeformationInterface,
                            lpTriangleCache,
                            lfTimestep);
                    }

                    luCurrentGroup = luGroup;
                    lOrderer.Construct();   // v47 = 0
                }

                const VecFloat lvfImpactTime = PredictCarWorldContactTime(lContact);
                lOrderer.AddContact(&lContact, lvfImpactTime.x);
            }

            // Dispatch the final group.
            const s32 liValid = lOrderer.SortContacts();
            for (s32 liDispatch = 0; liDispatch < liValid; ++liDispatch)
            {
                const PotentialContact* lpOrdered = lOrderer.GetContact(liDispatch);
                HandleRaceCarWorldPotentialContact(
                    *lpOrdered,
                    lpRequestOutputInterface,
                    lpVehicleOutputInterface,
                    lpManagerOutputInterface,
                    lpDeformationInterface,
                    lpTriangleCache,
                    lfTimestep);
            }
        }
    }
}
}
