#pragma once

// Home for the BrnDirector::MomentController nested helper types that the
// "director-moment" group needs as complete types:
//   * BrnDirector::MomentController::MomentHandle  -- a 24-byte slot describing one
//     live director moment owned by the controller. Its accessor GetMoment is bodied
//     out-of-line in BrnMomentController.cpp.
//   * BrnDirector::MomentDescription               -- a 16-byte POD record describing a
//     candidate moment; held by value in the MomentSelector's Array<MomentDescription,10>.
//
// Layout authority: the console executable.
//   MomentController::MomentHandle::GetMoment reads mbIsAllocated (a bool at
//     this+0x00, asserted "mbIsAllocated" against) then returns
//     the moment pointer at this+0x04.
//   The 24-byte element stride is pinned by Array<MomentController::MomentHandle,10>::Append
// (count word @+0xF0 == 10*0x18; per-element copy of six 4-byte words).
//   The 16-byte MomentDescription stride is pinned by Array<MomentDescription,10>::Append
// (count word @+0xA0 == 10*0x10; per-element copy of four 4-byte words).

#include "types.hpp"
#include "GameSource/Director/Utils/BrnAbstractPool.h"               // AbstractPool<>, AbstractPoolVoidHandle
#include "GameSource/Director/MomentController/BrnMoment.h"           // Moment, Moment::EType
#include "GameSource/Director/MomentController/BrnMomentParameterBank.h" // MomentParameterBank
#include "rw/math/vpu/types.h"                                       // rw::math::vpu::Vector4 (pool unit_type)

namespace BrnDirector
{
    namespace Camera { class BehaviourManager; }   // threaded through NewMoment / MomentHandle::Prepare (by ref, not read)
    struct MomentSharedInfo;                        // UpdateAllMoments' per-frame context (BrnMomentSharedInfo.h)

    // NOTE: BrnDirector::MomentDescription used to be modelled here as an opaque
    // `u32 mauOpaque[4]` span. That was a HYPOTHESIS, and it was wrong: the declarations
    // home MomentDescription with four NAMED fields
    // (meMomentType / meMomentParamID / mfWeighting / mbCanBeInhibited), every one of which
    // the console build reads (see the header comment there). It now lives in
    // GameSource/Director/MomentController/BrnMomentSelector.h -- its real declared home --
    // with the real field set. Include that header if you need the type.

    // MomentController owns the live director moments.
    // It holds the native AbstractPool<70,20,Vector4> and the parameter bank
    // by value, and hands moments out through MomentHandle. NewMoment is the
    // factory: release the in/out handle, allocate the requested moment type from the pool,
    // Prepare the handle around the new slot, then SetParameters from the bank.
    class MomentController
    {
    public:
    // DecFIGS declares 70 Vector4 units per slot. ARTIST MainDirector::Prepare
    // refills the 20-entry queue at controller+0x5790, and Construct stores the
    // occupancy at+0x57E8 before constructing the parameter bank at+0x57F0.
    // The native pool must retain that 1120-byte stride. Concrete moment fit
    // checks remain in BrnMomentController.cpp.
    public:
        static const u32 KU_MOMENT_POOL_UNITS_CONSOLE = 70u;   // the console's own bucket
        static const u32 KU_MOMENT_POOL_UNITS      = 70u;
        static const u32 KU_MOMENT_POOL_BUCKETS    = 20u;   // unchanged, 20 live moments

        typedef AbstractPool<KU_MOMENT_POOL_UNITS, KU_MOMENT_POOL_BUCKETS,
                             rw::math::vpu::Vector4> MomentPool;

        // ---- nested handle -------------------------------
        // One controller slot: an allocated flag, the type-erased pool handle for the moment
        // slot, and a back-pointer to the owning controller. sizeof == 0x18 (24 bytes),
        // pinned by the Array<MomentHandle,10>::Append stride: bool(+pad) +
        // AbstractPoolVoidHandle(0x10) + MomentController*(4).
        class MomentHandle
        {
        public:
            // No standalone console symbol -- the console
            // inlines it. MomentSelector::AddMoment emits the whole body as a
            // single zero byte written into the stack handle it is about to Append,
            // i.e. it only clears the allocated flag; the pool handle / parent are left for
            // Prepare() to fill.
            void Construct() { mbIsAllocated = false; }

            // console (GetMoment). Asserts mbIsAllocated
            // then returns the held moment -- the pool handle's object pointer at this+0x04
            // (== mMomentPoolHandle.mpObject). Bodied in BrnMomentController.cpp.
            Moment* GetMoment() const;

            // @0x821F7298 (DWARF BrnMomentController.cpp:235). Take ownership of a freshly placed
            // pool slot -- stash the parent / the allocated flag / the handle -- then bring the
            // moment up: its vtable slot 0 (Construct), then Prepare(lBehaviourManager) under the
            // :242 assert. Bodied in BrnMomentController.cpp.
            bool Prepare(AbstractPoolVoidHandle lVoidHandle,
                         MomentController& lrParentMomentController,
                         Camera::BehaviourManager& lrBehaviourManager);

            // Release the held slot back to the pool and clear the allocated flag (a no-op
            // when nothing is held). Bodied in BrnMomentController.cpp.
            bool Release();

            bool IsAllocated() const { return mbIsAllocated; }

        private:
            bool                    mbIsAllocated;            // +0x00 : asserted before GetMoment
            AbstractPoolVoidHandle  mMomentPoolHandle;        // +0x04 : the pool slot handle (0x10)
            MomentController*       mpParentMomentController; // +0x14 : owning controller
        };

        // ---- controller factory (the ledger function) ------------------------------------
        // console. Allocate a moment of leMomentType from the pool, hand the slot to
        // lrMomentHandleInOut, then push the bank's parameters for leMomentParamID onto it.
        bool NewMoment(Moment::EType leMomentType,
                       MomentParameterBank::EMomentParamID leMomentParamID,
                       MomentHandle& lrMomentHandleInOut,
                       Camera::BehaviourManager& lrBehaviourManager);

        // Lifecycle. DWARF BrnMomentController.cpp:24 / :33 / :96 / :105. The console has no
        // out-of-line symbol for any of them: MainDirector inlines Construct (0x8225B538..
        // 0x8225B554: the pool's occupancy word cleared, then MomentParameterBank::Construct at
        // controller +0x57F0), Prepare (its PREPARE stage 5: the pool's free queue refilled 19..0,
        // count 20, occupancy cleared) and Destruct (0x8224FCC0: the occupancy word cleared).
        // Construct / Prepare / Destruct are bodied in BrnMomentController.cpp; Release has no
        // caller in the image and stays declaration-only.
        void Construct();
        bool Prepare();
        bool Release();
        void Destruct();

        // @0x82239DE8 (DWARF BrnMomentController.cpp:44). Once per director frame, from
        // MainDirector::UpdateMoments: every ALLOCATED moment slot gets its debug name line, its
        // PreUpdate (the three per-frame permission bits back to true, its camera's validity
        // account masked to the latched failures) and its Update(timestep, manager, info).
        // Bodied in BrnMomentController.cpp.
        void UpdateAllMoments(Camera::BehaviourManager& lrBehaviourManager,
                              const MomentSharedInfo& lrMomentSharedInfo);

    private:
        // Member list, held by value, pool first.
        MomentPool                                     mMomentPool;
        MomentParameterBank                            mMomentParameterBank;

        static void AssertNativeLayout()
        {
            CGS_STATIC_ASSERT((sizeof(MomentPool::Bucket) == 1120), "ARTIST moment slot stride");
            CGS_STATIC_ASSERT((sizeof(MomentPool) == 0x57F0), "ARTIST moment pool");
            CGS_STATIC_ASSERT((offsetof(MomentController, mMomentPool) == 0), "ARTIST moment pool base");
            CGS_STATIC_ASSERT((offsetof(MomentController, mMomentParameterBank) == 0x57F0), "ARTIST moment parameters");
            CGS_STATIC_ASSERT((sizeof(MomentController) == 0x5840), "ARTIST moment controller extent");
        }
    };

} // namespace BrnDirector
