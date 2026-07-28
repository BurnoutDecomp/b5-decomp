#include "GameSource/Director/Camera/BrnBehaviourManager.h"
#include "GameSource/Director/Camera/Behaviours/BehaviourRig.h"   // BehaviourRig (the instantiation's T)

// ============================================================================
// GameSource/Director/Camera/BrnBehaviourHandleRig.cpp
//
// Compilation home for the BehaviourHandle<BehaviourRig> instantiation (the IDA
// symbols truncate it to "BrnDirector::Camera::BehaviourRig>"). The TU is the
// three X360 member instantiations:
//   BehaviourHandle<BehaviourRig>::AttachTweaker @0x82212608 (h:623)
//   BehaviourHandle<BehaviourRig>::DetachTweaker @0x82212668 (h:633)
//   BehaviourHandle<BehaviourRig>::Prepare       @0x8222F518 (h:589)
// Each body is the template member in BrnBehaviourManager.h (Prepare is the
// committed generic @0x8224AFF0 shape, re-emitted per T for its
// GetBehaviourSlotFromHandle<T> tail; the tweaker pair verified against the asm
// -- see the template-member comments). The anchor below forces exactly those
// three to be emitted. Callers in the export set: ArbStateTestbed::Update
// (AttachTweaker), ArbStateTestbed::Release (DetachTweaker),
// BehaviourManager::NewBehaviour<BehaviourRig> (Prepare).
// ============================================================================

namespace BrnDirector
{
namespace Camera
{

// Out-of-line anchor: exercises exactly the three exported members.
// (Prepare's second argument is now the owning manager's HELPER POOL -- the +0x08 word was
// identified as a pool pointer, not an index; see BrnBehaviourManager.h.)
void BehaviourRigHandle_Anchor(BehaviourHandle<BehaviourRig>& lrHandle,
                               BehaviourHelperIndex lHelperIndex,
                               BehaviourManager::HelperPool* lpHelperPool,
                               BehaviourManager* lpManager)
{
    lrHandle.Prepare(lHelperIndex, lpHelperPool, lpManager);
    lrHandle.AttachTweaker();
    lrHandle.DetachTweaker();
}

}
}
