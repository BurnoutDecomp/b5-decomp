#ifndef GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_SIMPLE_ICE_TAKEDOWN_PLAYER_H
#define GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_SIMPLE_ICE_TAKEDOWN_PLAYER_H

#include "types.hpp"
#include <cstddef>                                              // offsetof
#include "GameSource/AttribSys/Generated/classes/iceanim.h"     // Attrib::Gen::iceanim
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorState.h"    // ArbitratorState / ArbStateSharedInfo
#include "GameSource/Director/Camera/BrnBehaviourManager.h"               // BehaviourHandle<>
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourIceAnim.h"    // Camera::BehaviourIceAnim

// ============================================================================
// GameSource/Director/Arbitrator/States/BrnSimpleIceTakedownPlayer.h
//
// BrnDirector::SimpleIceTakedownPlayer -- one of the takedown-camera players (the
// ICE-anim-driven variant). Originally landed as a MINIMAL SLICE (only the member
// SetIceAnim touches, the iceanim pointer @+0x18); GROWN here to the full player
// (TakedownPlayer interface + state machine) by the BrnArbStateTakedown.cpp TU, which
// is the ArbStateTakedown owner and needs the complete SimpleIceTakedownPlayer to embed
// by value. Reconstructed member layout / DWARF order from
// references/DecFIGS/dwarfdump/GameSource/Director/Arbitrator/States/BrnArbStateTakedown.h
// (BrnArbStateTakedown.h:210), gated on the X360 ledger (Prepare @0x8226CF38,
// Update @0x8225A1E8, Release @0x82235208, HasFinished @0x821F62B0).
//
// FLAG: TakedownPlayer (the shared polymorphic interface every takedown-style player
//       implements) is defined HERE rather than in BrnArbStateTakedown.h, because this header
//       is the first (chronologically) to need a complete base to derive from, and
//       BrnArbStateTakedown.h in turn includes this header to reuse SimpleIceTakedownPlayer --
//       defining the shared base in BOTH would fork it, and defining it only in
//       BrnArbStateTakedown.h would create an include cycle. DWARF home
//       BrnArbStateTakedown.h:40; vtable order pinned by the DWARF slot order.
//       mPad0 below is replaced by the real BehaviourHandle (its size (0x14, five words)
//       matches the original +0x18 pad exactly, so mpIceAnim's offset relative to the object
//       head is unchanged: +0x18 X360 / by-name here).
// ============================================================================

namespace BrnDirector
{
    // BrnDirector::TakedownPlayer -- the common polymorphic interface every takedown-style
    // player implements (B3ClassicTakedownPlayer, DestructionPathTakedownPlayer,
    // DriveByTakedownPlayer, ShutdownTakedownPlayer, SimpleIceTakedownPlayer). DWARF home
    // BrnArbStateTakedown.h:40.
    class TakedownPlayer
    {
    public:
        virtual bool Prepare(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo) = 0;
        virtual Camera::Camera Update(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo) = 0;
        virtual void Release(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo) = 0;
        virtual bool HasFinished() const = 0;
    };

    class SimpleIceTakedownPlayer : public TakedownPlayer
    {
    public:
        // DWARF EState (BrnArbStateTakedown.h:242).
        enum EState
        {
            E_STATE_INACTIVE  = 0,
            E_STATE_PREPARING = 1,
            E_STATE_ACTIVE    = 2,
            E_STATE_FINISHED  = 3,

            E_NUM_STATES      = 4
        };

        // Bind the iceanim that drives this takedown camera. Asserts the passed
        // object carries iceanim's class-key tag. @0x821F58C8.
        void SetIceAnim(Attrib::Gen::iceanim* lpIceAnim);

        // Prepare @0x8226CF38 -- allocate + configure the ICE-anim behaviour (once) with the
        // bound shot's takedown look/eye vehicle refs on the live player race car, then report
        // whether the freshly-allocated behaviour is ready. Declared here; body in
        // BrnArbStateTakedown.cpp (this player's owning TU).
        bool Prepare(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo) override;

        // Update @0x8225A1E8 -- drive the camera from the ICE-anim behaviour's produced camera;
        // once PREPARING succeeds advance to ACTIVE; once the anim finishes or fails advance to
        // FINISHED. Body in BrnArbStateTakedown.cpp.
        Camera::Camera Update(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo) override;

        // Release @0x82235208 -- drop the ICE-anim behaviour hold (if allocated) and reset the
        // state machine. Body in BrnArbStateTakedown.cpp.
        void Release(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo) override;

        // HasFinished @0x821F62B0: return meState == E_STATE_FINISHED.
        bool HasFinished() const override { return meState == E_STATE_FINISHED; }

    private:
        // X360 (4-byte-pointer) layout: vtable@+0x00, mIceCam (the BehaviourHandle<> the
        // original SetIceAnim slice padded around) @+0x04 (0x14 bytes), mpIceAnim @+0x18,
        // mfActiveTime @+0x1C, meState @+0x20 (Prepare/Update/Release/HasFinished asm). Parity
        // here is BY NAMED MEMBER (the x64-gate rule); mIceCam's PC BehaviourHandle<> is
        // pointer-sized-member-correct but not byte-identical to the X360 slot.
        Camera::BehaviourHandle<Camera::BehaviourIceAnim> mIceCam;   // X360 +0x04 (0x14) -- was mPad0[0x18]

        Attrib::Gen::iceanim* mpIceAnim;     // X360 +0x18  the bound ICE anim
        f32    mfActiveTime;                 // X360 +0x1C  (asm: *(a2+28))
        EState meState;                      // X360 +0x20  (asm: *(a2+32))

        // Pin the one X360 offset the ORIGINAL SetIceAnim TU proved byte-exact (mpIceAnim
        // follows the handle immediately) as a relative-ordering check: mIceCam precedes
        // mpIceAnim, which precedes mfActiveTime, which precedes meState. Absolute offsets are
        // not asserted (x64 widens BehaviourHandle's pointer members).
        static void _AssertLayout()
        {
            static_assert(offsetof(SimpleIceTakedownPlayer, mIceCam) < offsetof(SimpleIceTakedownPlayer, mpIceAnim),
                          "SimpleIceTakedownPlayer: mIceCam before mpIceAnim");
            static_assert(offsetof(SimpleIceTakedownPlayer, mpIceAnim) < offsetof(SimpleIceTakedownPlayer, mfActiveTime),
                          "SimpleIceTakedownPlayer: mpIceAnim before mfActiveTime");
            static_assert(offsetof(SimpleIceTakedownPlayer, mfActiveTime) < offsetof(SimpleIceTakedownPlayer, meState),
                          "SimpleIceTakedownPlayer: mfActiveTime before meState");
        }
    };
}

#endif // GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_SIMPLE_ICE_TAKEDOWN_PLAYER_H
