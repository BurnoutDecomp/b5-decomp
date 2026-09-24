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
    // It holds the moment object pool (the declaration AbstractPool<70,20,Vector4>; host-widened, see
    // the KU_MOMENT_POOL_UNITS banner) and the parameter bank
    // by value, and hands moments out through MomentHandle. NewMoment is the
    // factory: release the in/out handle, allocate the requested moment type from the pool,
    // Prepare the handle around the new slot, then SetParameters from the bank.
    class MomentController
    {
    public:
    // ⛔ HOST BUCKET WIDENING -- an console SIZE CONSTANT THAT DOES NOT SURVIVE THE x64 PORT.
    //
    // The the declaration spells the moment pool
    // `AbstractPool<70u, 20u, rw::math::vpu::Vector4>`: 20 slots, each 70 Vector4 units ==
    // 1120 bytes, sized on the console to hold the LARGEST director moment. The console's
    // largest is MomentPlayerJumping (0x3A0 == 928 bytes, its last member meType at +0x39C),
    // i.e. the console reserved ~20% headroom over the biggest moment.
    //
    // ⚠️ MEASURED 2026-08-23 on THIS host build (sizeof probe over all twelve concrete
    // moments): MomentPlayerJumping is **1296 bytes** -- 176 bytes MORE than the console
    // bucket. Its four BehaviourCollection<> members are handle-and-pointer dense, and every
    // one of those widened 4 -> 8 bytes. With the console's literal 70, NewMoment's
    // `AllocateVoid<MomentPlayerJumping>()` would placement-new a 1296-byte object into a
    // 1120-byte slot: AbstractPool's own "object is too large" CGS_ASSERT would fire and
    // then the construction would proceed anyway, scribbling 176 bytes over the NEXT pool
    // slot -- i.e. silent heap corruption of a neighbouring live moment.
    //
    // So the console's UNIT COUNT is platform data, not behaviour. What is faithful is the
    // RULE ("one slot holds the largest moment, with ~20% headroom"); the host number is
    // re-derived from the host's own largest moment:
    //     1120 / 928  ==  1.207  ->  1296 * 1.207  ==  1564  ->  ceil(1564 / 16)  ==  98
    // The pool grows from 20*1120 == 22,400 to 20*1568 == 31,360 bytes.
    //
    // ⭐ THE NUMBER CANNOT SILENTLY ROT: BrnMomentController.cpp -- the one TU that sees all
    // twelve concrete moment types, because NewMoment lives there -- carries a static_assert per
    // type against sizeof(AbstractPool<...>::Bucket). Grow a moment past the bucket and that TU
    // stops compiling instead of the heap stopping working.
    //
    // ✅ THE SECOND SIZE LANDMINE IS GONE (2026-09-24, FX-DIRECTOR). MainDirector used to model this
    //    object as the console's 22,416-byte span (`u8 maMomentController[0x1CA60 - 0x172D0]`) plus
    //    the pool's free queue / count / occupancy a second time, and reinterpret_cast the span
    //    for ArbStateSharedInfo::mpMomentController -- which the host object (widened bucket, 8-byte
    //    vptr) does not fit. It now holds `MomentController mMomentController;` BY VALUE and reaches
    //    it by name, so NewMoment's AllocateVoid writes into memory the controller owns.
    public:
        static const u32 KU_MOMENT_POOL_UNITS_CONSOLE = 70u;   // the console's own bucket
        static const u32 KU_MOMENT_POOL_UNITS      = 98u;   // this host's re-derivation (see above)
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
        MomentPool                                     mMomentPool;       //  (host-widened bucket -- see the banner above)
        MomentParameterBank                            mMomentParameterBank;
    };

} // namespace BrnDirector
