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
    // It holds the moment object pool (AbstractPool<70,20,Vector4>) and the parameter bank
    // by value, and hands moments out through MomentHandle. NewMoment (@0x82255850) is the
    // factory: release the in/out handle, allocate the requested moment type from the pool,
    // Prepare the handle around the new slot, then SetParameters from the bank.
    class MomentController
    {
    public:
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
        AbstractPool<70u, 20u, rw::math::vpu::Vector4> mMomentPool;       // :82
        MomentParameterBank                            mMomentParameterBank; // :83
    };

} // namespace BrnDirector
