#pragma once

// Home for the BrnDirector::MomentController nested helper types that the
// "director-moment" group needs as complete types:
//   * BrnDirector::MomentController::MomentHandle  -- a 24-byte slot describing one
//     live director moment owned by the controller. Its accessor GetMoment is bodied
//     out-of-line in BrnMomentController.cpp.
//   * BrnDirector::MomentDescription               -- a 16-byte POD record describing a
//     candidate moment; held by value in the MomentSelector's Array<MomentDescription,10>.
//
// Layout authority: BURNOUT_X360_ARTIST.XEX.
//   MomentController::MomentHandle::GetMoment @0x821F5798 reads mbIsAllocated (a bool at
//     this+0x00, asserted "mbIsAllocated" against BrnMomentController.h:150) then returns
//     the moment pointer at this+0x04.
//   The 24-byte element stride is pinned by Array<MomentController::MomentHandle,10>::Append
//     @0x821FD990 (count word @+0xF0 == 10*0x18; per-element copy of six 4-byte words).
//   The 16-byte MomentDescription stride is pinned by Array<MomentDescription,10>::Append
//     @0x821FD858 (count word @+0xA0 == 10*0x10; per-element copy of four 4-byte words).

#include "types.hpp"
#include "GameSource/Director/Utils/BrnAbstractPool.h"               // AbstractPool<>, AbstractPoolVoidHandle
#include "GameSource/Director/MomentController/BrnMoment.h"           // Moment, Moment::EType
#include "GameSource/Director/MomentController/BrnMomentParameterBank.h" // MomentParameterBank
#include "rw/math/vpu/types.h"                                       // rw::math::vpu::Vector4 (pool unit_type)

namespace BrnDirector
{
    namespace Camera { class BehaviourManager; }   // threaded through NewMoment / MomentHandle::Prepare (by ref, not read)

    // NOTE: BrnDirector::MomentDescription used to be modelled here as an opaque
    // `u32 mauOpaque[4]` span. That was a HYPOTHESIS, and it was wrong: the DecFIGS DWARF
    // homes MomentDescription at BrnMomentSelector.h:39 with four NAMED fields
    // (meMomentType / meMomentParamID / mfWeighting / mbCanBeInhibited), every one of which
    // is read by an X360 instruction (see the header comment there). It now lives in
    // GameSource/Director/MomentController/BrnMomentSelector.h -- its real DWARF home --
    // with the real field set. Include that header if you need the type.

    // MomentController owns the live director moments. DWARF home BrnMomentController.h:43.
    // It holds the moment object pool (DWARF AbstractPool<70,20,Vector4>; host-widened, see
    // the KU_MOMENT_POOL_UNITS banner) and the parameter bank
    // by value, and hands moments out through MomentHandle. NewMoment (@0x82255850) is the
    // factory: release the in/out handle, allocate the requested moment type from the pool,
    // Prepare the handle around the new slot, then SetParameters from the bank.
    class MomentController
    {
    public:
    // ------------------------------------------------------------------------
    // ⛔ HOST BUCKET WIDENING -- an X360 SIZE CONSTANT THAT DOES NOT SURVIVE THE x64 PORT.
    //
    // The DWARF (BrnMomentController.h:82) spells the moment pool
    // `AbstractPool<70u, 20u, rw::math::vpu::Vector4>`: 20 slots, each 70 Vector4 units ==
    // 1120 bytes, sized on the console to hold the LARGEST director moment. The X360's
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
    // ⭐ THE NUMBER CANNOT SILENTLY ROT: BrnMomentControllerNewMoment.cpp -- the one TU that
    // sees all twelve concrete moment types -- carries a static_assert per type against
    // sizeof(AbstractPool<...>::Bucket). Grow a moment past the bucket and that TU stops
    // compiling instead of the heap stopping working.
    //
    // ⛔⛔ AND THERE IS A SECOND, BIGGER SIZE LANDMINE ON THE SAME OBJECT -- IN A FILE THIS
    //    LANE MAY NOT EDIT. BrnMainDirector.h:353 does NOT hold a MomentController; it holds
    //        u8 maMomentController[0x1CA60 - 0x172D0];        // == 22,416 bytes
    //    -- the CONSOLE's byte span -- and BrnMainDirector.cpp:573 reinterpret_casts it to
    //    MomentController* to fill ArbStateSharedInfo::mpMomentController. Those are X360
    //    numbers standing in for a host object: on x64 the host MomentController is already
    //    larger than 22,416 with the console's own 70-unit bucket (20 buckets alone are 22,400,
    //    before the vptr, the pool's free queue/count/occupancy and the 72-byte parameter
    //    bank), and the widening above adds 20 * (1568 - 1120) == 8,960 more.
    //
    //    ⭐ IT IS INERT TODAY, WHICH IS THE ONLY REASON THIS IS SAFE TO LAND: nothing ever
    //    constructs or writes through that cast, because MomentController::NewMoment is still
    //    a GROUP F stub (DirectorLinkStubs.cpp) that touches nothing, and this class is never
    //    instantiated anywhere in the mounted build -- so sizeof() is never consulted and not
    //    one byte of the shipped exe changes. The FIRST write through that pointer will be
    //    NewMoment's AllocateVoid, and it will run off the end of the span into
    //    MainDirector::maMomentBucketFreeQueue and everything after it.
    //
    //    ⇒ REQUIRED BEFORE THE MOMENT CLOSURE IS MOUNTED (NOT this lane's file):
    //      replace BrnMainDirector.h:353's opaque byte span with a real
    //      `BrnDirector::MomentController mMomentController;` member (and drop the three
    //      hand-modelled pool fields at +0x1CA60 that go with it, which are that same pool's
    //      free queue / count / occupancy modelled a second time). Until then the moment
    //      sub-system MUST stay stubbed.
    // ------------------------------------------------------------------------
    public:
        static const u32 KU_MOMENT_POOL_UNITS_X360 = 70u;   // DWARF :82 -- the console's own bucket
        static const u32 KU_MOMENT_POOL_UNITS      = 98u;   // this host's re-derivation (see above)
        static const u32 KU_MOMENT_POOL_BUCKETS    = 20u;   // DWARF :82 -- unchanged, 20 live moments

        typedef AbstractPool<KU_MOMENT_POOL_UNITS, KU_MOMENT_POOL_BUCKETS,
                             rw::math::vpu::Vector4> MomentPool;

        // ---- nested handle (DWARF BrnMomentController.h:89) -------------------------------
        // One controller slot: an allocated flag, the type-erased pool handle for the moment
        // slot, and a back-pointer to the owning controller. sizeof == 0x18 (24 bytes),
        // pinned by the Array<MomentHandle,10>::Append stride (@0x821FD990): bool(+pad) +
        // AbstractPoolVoidHandle(0x10) + MomentController*(4).
        class MomentHandle
        {
        public:
            // DWARF BrnMomentController.h:93. No standalone X360 symbol -- the console
            // inlines it. MomentSelector::AddMoment @0x82209F80 emits the whole body as a
            // single `stb r11(0), var_30(r1)` into the stack handle it is about to Append,
            // i.e. it only clears the allocated flag; the pool handle / parent are left for
            // Prepare() to fill.
            void Construct() { mbIsAllocated = false; }

            // X360 @0x821F5798 (GetMoment). Asserts mbIsAllocated (BrnMomentController.h:150)
            // then returns the held moment -- the pool handle's object pointer at this+0x04
            // (== mMomentPoolHandle.mpObject). Bodied in BrnMomentController.cpp.
            Moment* GetMoment() const;

            // X360 @0x82255B98 callee. Take ownership of a freshly-allocated pool slot:
            // stash the handle/parent, mark allocated, and tag the moment's type. Bodied in
            // BrnMomentControllerNewMoment.cpp. (DWARF: Prepare(AbstractPoolVoidHandle,
            // MomentController&, BehaviourManager&).)
            bool Prepare(AbstractPoolVoidHandle lVoidHandle,
                         MomentController& lrParentMomentController,
                         Camera::BehaviourManager& lrBehaviourManager);

            // Release the held slot back to the pool and clear the allocated flag (a no-op
            // when nothing is held). Bodied in BrnMomentControllerNewMoment.cpp.
            bool Release();

            bool IsAllocated() const { return mbIsAllocated; }

        private:
            bool                    mbIsAllocated;            // +0x00 : asserted before GetMoment
            AbstractPoolVoidHandle  mMomentPoolHandle;        // +0x04 : the pool slot handle (0x10)
            MomentController*       mpParentMomentController; // +0x14 : owning controller
        };

        // ---- controller factory (the ledger function) ------------------------------------
        // X360 @0x82255850. Allocate a moment of leMomentType from the pool, hand the slot to
        // lrMomentHandleInOut, then push the bank's parameters for leMomentParamID onto it.
        bool NewMoment(Moment::EType leMomentType,
                       MomentParameterBank::EMomentParamID leMomentParamID,
                       MomentHandle& lrMomentHandleInOut,
                       Camera::BehaviourManager& lrBehaviourManager);

        // Lifecycle (DWARF BrnMomentController.h:55-69; declared-only here -- bodies live in
        // BrnMomentController.cpp and forward to the pool/bank members).
        void Construct();
        bool Prepare();
        bool Release();
        void Destruct();

    private:
        // DWARF member list (BrnMomentController.h:82-83), held by value, pool first.
        MomentPool                                     mMomentPool;       // :82 (host-widened bucket -- see the banner above)
        MomentParameterBank                            mMomentParameterBank; // :83
    };

} // namespace BrnDirector
