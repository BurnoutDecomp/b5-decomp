// ============================================================================
// GameSource/Director/Camera/BrnBehaviourManager_NewBehaviourFromShot.cpp
//
// BrnDirector::Camera::BehaviourManager::NewBehaviour  @0x82267418 -- the ATTRIBUTE-DRIVEN
// behaviour factory. Unlike the `NewBehaviour<TBehaviour>` family (@0x822580F8 and its 15
// byte-identical siblings, bodied out-of-line in BrnBehaviourManager.h), the concrete
// behaviour type here is NOT pinned at the call site: the caller hands over a shot's
// Attrib::RefSpec and this body reads the RefSpec's class key to decide which behaviour to
// allocate. It is the ONE console symbol every authored-shot camera comes into existence
// through -- the drive-thru shop cameras, the moment rig cameras, the crash shot selector.
//
// ISOLATED TU (the same reason BrnBehaviourManager_AllocateBehaviour_IceAnim.cpp exists):
// the IceAnim arm needs the REAL Behaviours/BrnBehaviourIceAnim.h, which derives the
// Camera::Behaviour base and pulls the real BrnLooker / BrnCollisionPolicy / BrnCameraTweaker
// headers. Those mutually collide with the local re-declarations in the flat-slice behaviour
// headers BrnBehaviourManager.cpp includes, so this body cannot live in that group TU.
//
// SOURCE OF TRUTH: BURNOUT_X360_ARTIST.XEX asm @0x82267418..0x82267C3C, read directly. The
// console's own assert line numbers (BrnBehaviourManager.cpp :623/:625/:627/:628/:629/:630,
// :655/:666/:679/:686/:693/:700/:707 and BrnBehaviourManager.h :654/:665) are reproduced as
// written, and they are what identifies each statement below.
//
// ⓘ THE PROLOGUE IS THE SAME PROLOGUE as NewBehaviour<TBehaviour> in the header, with ONE
// extra assert: this body also checks !mBehaviourNeedsReleasingFlags.IsBitSet(lHelperID)
// (:628), which the templated family does not. The line numbers differ because the two
// bodies live in different files (.cpp here, .h there) -- they are NOT the same function.
// ============================================================================

#include "GameSource/Director/Camera/BrnBehaviourManager.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourIceAnim.h"
#include "GameSource/AttribSys/Generated/classes/iceanim.h"          // Attrib::Gen::iceanim::ClassKey()
#include "GameSource/AttribSys/Generated/classes/aftertouchcam.h"    // KU_AFTERTOUCHCAM_CLASS_KEY
#include "GameSource/AttribSys/Generated/classes/proceduralshot.h"   // Attrib::Gen::proceduralshot::ClassKey()
#include "GameShared/GameClasses/Core/CgsAssert.h"                   // CGS_ASSERT + CgsDev::Log::gpDebugPrint
#include <cstdlib>                                                   // getenv (the diag rung's env guard)

namespace BrnDirector
{
namespace Camera
{
    namespace
    {
        // The console's own assert file string for this TU's asserts.
        const char* const KPC_ASSERT_FILE =
            "..\\..\\..\\GameSource\\Director/Camera/BrnBehaviourManager.cpp";
    }

    // ------------------------------------------------------------------------
    // NewBehaviour @0x82267418
    //
    //   r3 = this, r4 = lpAttributeData (the shot's Attrib::RefSpec), r5 = &lrHandle,
    //   r6 = lpOwningState, r7 = lpOwner, r8 = liRefLimit.
    //
    // The class-key dispatch, verbatim from the asm (each id is built by the same
    // lis/ori/insrdi pair the generated class headers already record, so the constants are
    // taken from those headers and never re-spelled here):
    //   0x75E62FC1632388D6  aftertouchcam   -> BehaviourAftertouchCam        @0x822678C8
    //   0x4644E379A997C1EE  iceanim         -> BehaviourIceAnim              @0x82267970
    //   0x9B2E3C86E02737B0  proceduralshot  -> BehaviourGyroCam, sub-typed   @0x822679E4
    //   anything else       -> "Unsupported Behavour Type" (:707) -- the console's own
    //                          spelling, typo included, and NO behaviour is allocated.
    //
    // The read is `ld r11, 0(r15)` -- the RefSpec's LEADING QWORD is its class key
    // (BrnBehaviourIceAnim.h records the same fact where it corrects the ShotReference
    // typedef), so this is a plain 64-bit load off the attribute pointer, not an
    // Attrib::Instance::GetClass() call.
    // ------------------------------------------------------------------------
    template <typename TBehaviour, typename THandle>
    void BehaviourManager::NewBehaviour(THandle& lrHandle, const void* lpAttributeData,
                                        void* lpOwningState, const void* lpOwner, s32 liArgB)
    {
        // ---- the shared prologue: reserve a helper slot and book-keep it -----------------
        const BehaviourHelperIndex lHelperID = mBehaviourHelperPool.AllocateObject();

        CGS_ASSERT(static_cast<s32>(lHelperID) >= 0, "lHelperID >= 0");                   // :623
        CGS_ASSERT(liArgB >= 0, "liRefLimit >= 0");                                       // :625
        CGS_ASSERT(!mBehaviourNeedsPreparingFlags.IsBitSet(static_cast<u32>(lHelperID)),
                   "!mBehaviourNeedsPreparingFlags.IsBitSet(lHelperID)");                 // :627
        CGS_ASSERT(!mBehaviourNeedsReleasingFlags.IsBitSet(static_cast<u32>(lHelperID)),
                   "!mBehaviourNeedsReleasingFlags.IsBitSet(lHelperID)");                 // :628
        CGS_ASSERT(!mBehaviourUsedByHandleFlags.IsBitSet(static_cast<u32>(lHelperID)),
                   "!mBehaviourUsedByHandleFlags.IsBitSet(lHelperID)");                   // :629
        CGS_ASSERT(mBehaviourRefCounts[static_cast<u32>(lHelperID)] == 0,
                   "mBehaviourRefCounts[lHelperID] == 0");                                // :630

        mBehaviourNeedsPreparingFlags.SetBit(static_cast<u32>(lHelperID));
        mBehaviourUpdateDuringPauseFlags.UnSetBit(static_cast<u32>(lHelperID));

        mBehaviourHelperIndexArray.Append(lHelperID);

        mDebugBehaviourRefCountLimits[static_cast<u32>(lHelperID)]   = liArgB;
        mDebugBehaviourRefCountIndexLog[static_cast<u32>(lHelperID)].Clear();

        BehaviourHelper& lrHelper = mBehaviourHelperPool[lHelperID];

        // ---- the class-key dispatch -------------------------------------------------------
        const u64 luShotClassKey = *static_cast<const u64*>(lpAttributeData);

        if (luShotClassKey == static_cast<u64>(Attrib::Gen::iceanim::ClassKey()))
        {
            // @0x8226798C..0x822679E0. The authored-ICE-take camera: this is the arm every
            // drive-thru shop shot, every shotgroup take and every ICE moment shot lands on.
            const bool lbHelperPrepared =
                lrHelper.Prepare(AllocateBehaviour<BehaviourIceAnim>());
            CGS_ASSERT(lbHelperPrepared,
                       "lrHelper.Prepare(AllocateBehaviour<BehaviourIceAnim>())");        // :666
            (void)lbHelperPrepared;

            // X360 `lwz r3, 0(r31)` -- the helper's first word, i.e. the pooled object -- then
            // SetParameters(behaviour, lpAttributeData). The parameter block IS the RefSpec.
            static_cast<BehaviourIceAnim*>(lrHelper.GetPoolHandle().Get())->SetParameters(
                static_cast<BehaviourIceAnim::ShotReference*>(
                    const_cast<void*>(lpAttributeData)));
        }
        else if (luShotClassKey == Attrib::Gen::aftertouchcam::KU_AFTERTOUCHCAM_CLASS_KEY)
        {
            // ⚠️ GATE (@0x822678EC..0x8226796C). The console arm is:
            //     Attrib::Gen::aftertouchcam lShot(lpAttributeData, 0);          // sub_82206728
            //     lrHelper.Prepare(AllocateBehaviour<BehaviourAftertouchCam>()); // :655
            //     behaviour->SetParameters(mBehaviourParameterBank + 0x10);
            //     behaviour->mSourceShot = lShot;                                // +0x334
            // TWO blockers, both real and both named:
            //   (a) the parameters argument is BANK+0x10 (console manager+0x12540, bank base
            //       manager+0x12530) and BrnBehaviourParameterBank.h models everything below
            //       +0x2334 as `u8 maReservedHead[0x2334]`. Writing it would be a raw-offset
            //       poke into an un-carved reserved span -- forbidden. Carve the
            //       BehaviourAftertouchCam::Parameters block at bank+0x10 (the bank's FIRST
            //       Parameters block; the header already records that fact) exactly as the
            //       deathcam block was carved into NamedParameters.
            //   (b) BrnBehaviourAftertouchCam.h is one of the flat-slice behaviour headers that
            //       mutually collide with BrnBehaviourIceAnim.h, which this TU must include --
            //       so this arm needs its own isolated partfile, the same way
            //       BrnBehaviourManager_AllocateBehaviour_{IceAnim,RenderMetrics,Rig}.cpp do.
            // CONSEQUENCE: an AFTERTOUCH shot allocates no behaviour and the handle is left
            // holding a helper whose pool handle is empty. That is NOT a silent nothing -- the
            // assert below fires, and the caller's first GetBehaviour() would fault rather than
            // quietly produce a still camera.
            // DELETE-WHEN: (a) the bank's +0x10 block is carved AND (b) this arm moves to its
            // own partfile.
            CGS_ASSERT(false,
                       "FLAG unlanded: aftertouchcam shot -- BehaviourAftertouchCam::Parameters "
                       "at BehaviourParameterBank+0x10 is not carved");
        }
        else if (luShotClassKey == static_cast<u64>(Attrib::Gen::proceduralshot::ClassKey()))
        {
            // ⚠️ GATE (@0x82267A00..0x82267B7C). The console arm constructs
            // Attrib::Gen::proceduralshot over the RefSpec, reads its shot-type field and
            // allocates a BehaviourGyroCam for types 3 / 5 / 6, each with a DIFFERENT
            // parameter block:
            //     type 3 -> SetParameters(bank + 0x6B8)   (console manager + 0x12BE8)  :693
            //     type 5 -> SetParameters(bank + 0x388)   (console manager + 0x128B8)  :679
            //     type 6 -> SetParameters(bank + 0x454)   (console manager + 0x12984)  :686
            //     default -> "Unsupported Procedural Shot Type"                        :700
            // Blocked for the same two reasons as the aftertouch arm: all three offsets fall
            // inside BehaviourParameterBank::maReservedHead, and BrnBehaviourGyroCam.h collides
            // with BrnBehaviourIceAnim.h in one TU.
            // CONSEQUENCE: a PROCEDURAL shot allocates no behaviour. Procedural shots are the
            // crash/moment shot selector's output (ShotSelector::GetCrashShot builds this very
            // class key), NOT the authored shotgroup takes the drive-thru and the arbitrator
            // states play -- so this gate does not stand between any currently-live feature and
            // its camera. DELETE-WHEN: the three bank blocks are carved and this arm gets its
            // own partfile.
            CGS_ASSERT(false,
                       "FLAG unlanded: proceduralshot -- BehaviourGyroCam::Parameters at "
                       "BehaviourParameterBank+0x388/+0x454/+0x6B8 are not carved");
        }
        else
        {
            CGS_ASSERT(false, "Unsupported Behavour Type");                               // :707
        }

        // [DIAG] NOT IN THE X360 BINARY. Off unless BRN_DRIVETHRU_DIAG is set. The class key is
        // printed because "the shot was an unsupported type" and "the shot pointer was the
        // Attrib::DefaultDataArea fallback" are indistinguishable from the outside, and both
        // present as a camera that never moves [[diagnostics-that-lie]]. Delete with the rest
        // of the drive-thru bring-up diagnostics.
        {
            static const bool sbDriveThruDiag = (getenv("BRN_DRIVETHRU_DIAG") != 0);
            if (sbDriveThruDiag && CgsDev::Log::gpDebugPrint != 0)
            {
                const bool lbAllocated = (lrHelper.GetPoolHandle().Get() != 0);
                *CgsDev::Log::gpDebugPrint
                    << "[drivethru] NewBehaviour shot classKey=0x"
                    << static_cast<s32>(static_cast<u32>(luShotClassKey >> 32)) << ":"
                    << static_cast<s32>(static_cast<u32>(luShotClassKey))
                    << " helper=" << static_cast<s32>(lHelperID)
                    << " behaviour=" << (lbAllocated ? 1 : 0) << "\n";
            }
        }

        // ---- bind the caller's handle to the slot and record the two debug owners ---------
        // @0x82267BA0..0x82267C34: Behaviour_::Prep(handle, lHelperID, &pool, this), then the
        // helper is RE-RESOLVED from the handle for each of the two owner stores (+0x170 /
        // +0x174) -- reproduced with the same shape rather than reusing lrHelper, so the
        // dependency stays visible, exactly as the templated sibling does.
        lrHandle.Prepare(lHelperID, &mBehaviourHelperPool, this);

        CGS_ASSERT(lrHandle.IsAllocated(), "IsAllocated()");                              // .h:654
        lrHandle.GetHelper().SetDebugArbitratorStateOwner(
            static_cast<const ArbitratorState*>(lpOwningState));

        CGS_ASSERT(lrHandle.IsAllocated(), "IsAllocated()");                              // .h:665
        lrHandle.GetHelper().SetMomentOwner(static_cast<const Moment*>(lpOwner));
    }

    // ========================================================================
    // Explicit instantiation. The console has ONE body for every caller (the symbol carries no
    // template arguments); this tree reaches it through the header's generic declaration, so
    // each caller's <TBehaviour, THandle> pair is instantiated here. Add the pair, never a
    // second body.
    //   <Behaviour, BehaviourHandle<Behaviour>>  -- ArbStateDriveThru::Prepare @0x8226E938
    //     (the shop-shot camera; the shot type is resolved from the attribute block, which is
    //      precisely why TBehaviour is the generic base here and not a concrete camera).
    // ========================================================================
    template void BehaviourManager::NewBehaviour<Behaviour, BehaviourHandle<Behaviour> >(
        BehaviourHandle<Behaviour>& lrHandle, const void* lpAttributeData,
        void* lpOwningState, const void* lpOwner, s32 liArgB);
}
} // namespace BrnDirector
