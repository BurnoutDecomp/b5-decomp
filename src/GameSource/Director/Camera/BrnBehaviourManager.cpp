// ============================================================================
// GameSource/Director/Camera/BrnBehaviourManager.cpp
//
// Bodies for the BrnDirector::Camera::BehaviourManager camera-behaviour manager.
// The full layout is homed in BrnBehaviourManager.h (committed e1028d1); this TU
// bodies the manager methods against that header's REAL named members
// (mBehaviourHelperIndexArray / mBehaviourHelperPool / the four BitArray<28> sets /
// the ref-count Array tables / mpDirectorResourceManager / ...).
//
// SOURCE-OF-TRUTH: the X360 BURNOUT_X360_ARTIST.XEX pseudocode + asm for this TU is
// the spine; DecFIGS DWARF gave the declaration shape; the Feb-2007 partial was style
// only (it has no source for this TU).
//
// SCOPE OF THIS WAVE (postmortem dossier = exactly 10 functions):
//   FAITHFULLY BODIED (operate purely on the homed manager members + committed APIs):
//     * BehaviourManager::CheckNoBehavioursAreAllocatedByState  @0x822201B0
//
//   DECLARATION-ONLY + FLAGGED (each reaches an un-homed opaque interior, an un-homed
//   template family, or a multi-stage VMX pipeline -- bodying any of these would require
//   fabricating state the binary's structure does NOT attest in the homed layout):
//     * BehaviourHelper::GetDebugFullName        @0x821F8350  (reaches the Behaviour
//         interior's debug-parameters name + the debug-owner GetName virtuals through the
//         un-homed BehaviourHelper::Get()->Behaviour interior)
//     * BehaviourHelper::Prepare                 @0x82255F48  (calls a virtual through the
//         pooled object via the type-erased handle then constructs the slot camera; the
//         handle-dispatch + behaviour vtable are the un-homed Behaviour interior)
//     * BehaviourHelper::Update                  @0x82220688  (drives the Behaviour's
//         ValidityAccount + PreUpdate virtuals -- un-homed Behaviour/CameraState interior)
//     * BehaviourManager::DebugDumpToTTY         @0x82220750  (per-slot GetDebugFullName +
//         CgsDev::Log debug print -- reaches the same opaque BehaviourHelper::Get() name)
//     * BehaviourManager::GenerateSceneQueries   @0x8221F1C0  (per-slot
//         BehaviourHelper::GenerateSceneQueries -> Behaviour interior collision policy)
//     * BehaviourManager::NewBehaviour<>         @0x82267418  (the AllocateBehaviour<T>
//         template family + per-behaviour SetParameters + AttribSys instance decode --
//         un-homed template + opaque parameter-bank interior)
//     * BehaviourManager::ProcessSceneQueryResults @0x8221F438 (per-slot
//         BehaviourHelper::ProcessSceneQueryResults -> Behaviour interior)
//     * BehaviourManager::PostCollisionUpdateAllBehaviours @0x8221F870  VMX-PIPELINE
//         (vrlimi128/vperm/vcmpgtfp attitude-band math) -- NEVER scalar-paraphrased
//     * BehaviourManager::UpdateAllBehaviours    @0x82251960  VMX-PIPELINE
//         (vrlimi128/vperm/vcmpgtfp attitude bands + responder time accumulator) --
//         NEVER scalar-paraphrased
//
//   The declaration-only methods are intentionally NOT defined here; their out-of-line
//   bodies land when the Behaviour interior / the responder+rotation-controller sub-types /
//   the AllocateBehaviour<> template / the VMX attitude pipeline are homed. Leaving them
//   undefined keeps the TU honest (no fabricated bodies) while the one faithfully-recovered
//   method below links.
// ----------------------------------------------------------------------------

#include "GameSource/Director/Camera/BrnBehaviourManager.h"
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorState.h"   // ArbitratorState (owner identity + GetName)

namespace BrnDirector
{
namespace Camera
{
    // ------------------------------------------------------------------------
    // BehaviourManager::CheckNoBehavioursAreAllocatedByState  @0x822201B0
    //
    // Debug audit fired from every arbitrator state's Release(): walk the active
    // helper-index table and assert that no live behaviour is still flagged as used-by-handle
    // while it is owned by the releasing state. Pure book-keeping over the homed members --
    // the active-index Array<>, the helper ObjectPool occupancy, each slot's debug
    // arbitrator-state owner, and the used-by-handle BitArray<28>.
    //
    // Faithful to the X360 asm branch structure: NULL-state assert; iterate
    // mBehaviourHelperIndexArray [0, GetLength); for every helper whose slot is allocated and
    // whose debug arbitrator-state owner is the releasing state, assert it is NOT still set in
    // mBehaviourUsedByHandleFlags. The X360 builds a rich StrStream assert message (state name
    // + behaviour name) for the failure; that message text reaches the un-homed Behaviour
    // interior name getter, so the project CGS_ASSERT here carries a plain static message (the
    // macro supplies file/line) -- NO behaviour-interior field is fabricated.
    // ------------------------------------------------------------------------
    void BehaviourManager::CheckNoBehavioursAreAllocatedByState(ArbitratorState* lpArbitratorState)
    {
        CGS_ASSERT(lpArbitratorState != 0, "lpArbitratorState != NULL");

        const u32 luNumBehaviours = mBehaviourHelperIndexArray.GetLength();
        for (u32 luLoop = 0; luLoop < luNumBehaviours; ++luLoop)
        {
            const BehaviourHelperIndex lCurrentHelperIndex = mBehaviourHelperIndexArray[luLoop];

            if (mBehaviourHelperPool.IsObjectAllocated(lCurrentHelperIndex))
            {
                const BehaviourHelper& lrBehaviourHelper = mBehaviourHelperPool[lCurrentHelperIndex];

                if (lrBehaviourHelper.GetDebugArbitratorStateOwner() == lpArbitratorState)
                {
                    CGS_ASSERT(!mBehaviourUsedByHandleFlags.IsBitSet(static_cast<u32>(lCurrentHelperIndex)),
                               "State has a behaviour allocated at Release");
                }
            }
        }
    }

    // The const-void* owner-key overload the header declares for non-ArbitratorState call sites
    // routes straight through the attested ArbitratorState body above by owner identity only.
    void BehaviourManager::CheckNoBehavioursAreAllocatedByState(const void* lpState)
    {
        CheckNoBehavioursAreAllocatedByState(
            const_cast<ArbitratorState*>(static_cast<const ArbitratorState*>(lpState)));
    }
}
} // namespace BrnDirector
