// Out-of-line bodies for the BrnDirector::MomentController moment-factory path.
// Reconstructed from BURNOUT_X360_ARTIST.XEX, semantic-parity.
//
// Bodied here:
//   BrnDirector::MomentController::NewMoment              @0x82255850
//   BrnDirector::MomentController::MomentHandle::Prepare  (DWARF BrnMomentController.cpp:235)
//   BrnDirector::MomentController::MomentHandle::Release  (DWARF BrnMomentController.cpp:277)
//
// NewMoment is the controller's moment factory. The X360 spine (@0x82255850):
//   1. Release the in/out handle (assert it returned true).
//   2. switch(leMomentType) -> mMomentPool.AllocateVoid<MomentXxx>() for the 12 types,
//      each returning the four-word AbstractPoolVoidHandle for the freshly-constructed slot.
//      The jump table maps case 0..11 to HardStop/HitTraffic/Tumbling/TakedownLookback/
//      PassengerSeesAction/BystanderSeesAction/FailSafe/PlayerJumping/PlayerStunt/
//      StaticCamImpact/NewCarJoined/StationaryCrash (the default arm fires "Unhandled
//      moment type").
//   3. lrMomentHandleInOut.Prepare(lVoidHandle, *this, lrBehaviourManager).
//   4. assert mbIsAllocated, then assert GetMoment().GetType() == leMomentType.
//   5. lpParameters = mMomentParameterBank.GetParameters(leMomentParamID).
//   6. assert mbIsAllocated, then GetMoment()->SetParameters(lpParameters)
//      (vtable +0xC == SetParameters, Moment slot 3).
// The X360-baked assert file/line are dropped per project convention; the stringized
// conditions match the X360 message text.

#include "GameSource/Director/MomentController/BrnMomentController.h"
#include "GameSource/Director/MomentController/BrnMomentSubclasses.h"   // the 11 stub moment types
#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT

namespace BrnDirector
{

// DWARF BrnMomentController.cpp:235. Take ownership of a freshly-allocated pool slot.
// X360 stores the handle/parent and marks the slot allocated; the two GetMoment() reads the
// DWARF attests are the moment-type tag/back-reference wiring once the slot is owned.
bool MomentController::MomentHandle::Prepare(AbstractPoolVoidHandle lVoidHandle,
                                             MomentController& lrParentMomentController,
                                             Camera::BehaviourManager& /*lrBehaviourManager*/)
{
    CGS_ASSERT(!mbIsAllocated, "!mbIsAllocated");

    mMomentPoolHandle        = lVoidHandle;
    mpParentMomentController = &lrParentMomentController;
    mbIsAllocated            = true;
    return true;
}

// MomentHandle::Release MOVED OUT of this TU 2026-08-01 -> BrnMomentController.cpp, which is
// the DWARF's own home for it (BrnMomentController.cpp:277) AND is on the exe source list.
// This TU is not mounted (NewMoment's twelve AllocateVoid<MomentXxx>() arms drag the moment
// subclass family), and BrnMomentSelector.cpp -- which IS mounted -- calls Release on every
// handle, so leaving the only copy here meant the link could not see it.
// ⚠️ WHEN THIS TU IS FINALLY MOUNTED, do NOT re-add Release here: it would be an LNK2005.

// @0x82255850.
bool MomentController::NewMoment(Moment::EType leMomentType,
                                 MomentParameterBank::EMomentParamID leMomentParamID,
                                 MomentHandle& lrMomentHandleInOut,
                                 Camera::BehaviourManager& lrBehaviourManager)
{
    CGS_ASSERT(lrMomentHandleInOut.Release(), "lrMomentHandleInOut.Release()");

    AbstractPoolVoidHandle lVoidHandle;
    switch (leMomentType)
    {
        case Moment::E_MOMENT_HARD_STOP:
            lVoidHandle = mMomentPool.AllocateVoid<MomentHardStop>();
            break;
        case Moment::E_MOMENT_HIT_TRAFFIC:
            lVoidHandle = mMomentPool.AllocateVoid<MomentHitTraffic>();
            break;
        case Moment::E_MOMENT_TUMBLING:
            lVoidHandle = mMomentPool.AllocateVoid<MomentTumbling>();
            break;
        case Moment::E_MOMENT_TAKEDOWN_LOOKBACK:
            lVoidHandle = mMomentPool.AllocateVoid<MomentTakedownLookback>();
            break;
        case Moment::E_MOMENT_PASSENGER_SEES_ACTION:
            lVoidHandle = mMomentPool.AllocateVoid<MomentPassengerSeesAction>();
            break;
        case Moment::E_MOMENT_BYSTANDER_SEES_ACTION:
            lVoidHandle = mMomentPool.AllocateVoid<MomentBystanderSeesAction>();
            break;
        case Moment::E_MOMENT_FAILSAFE:
            lVoidHandle = mMomentPool.AllocateVoid<MomentFailSafe>();
            break;
        case Moment::E_MOMENT_PLAYER_JUMPING:
            lVoidHandle = mMomentPool.AllocateVoid<MomentPlayerJumping>();
            break;
        case Moment::E_MOMENT_PLAYER_STUNT:
            lVoidHandle = mMomentPool.AllocateVoid<MomentPlayerStunt>();
            break;
        case Moment::E_MOMENT_STATIC_CAM_IMPACT:
            lVoidHandle = mMomentPool.AllocateVoid<MomentStaticCamImpact>();
            break;
        case Moment::E_MOMENT_NEW_CAR_JOINED:
            lVoidHandle = mMomentPool.AllocateVoid<MomentNewCarJoined>();
            break;
        case Moment::E_MOMENT_STATIONARY_CRASH:
            lVoidHandle = mMomentPool.AllocateVoid<MomentStationaryCrash>();
            break;
        default:
            // ⚠️ DO NOT "FIX" THE UNINITIALISED lVoidHandle ON THIS ARM -- the console has it too.
            // MSVC reports C4701 here (measured 2026-08-16 under /w14701). It is a TRUE report of
            // UB that is in the SHIPPED X360 BINARY, not a transcription defect:
            //   0x82255B68 default: BeginAssert/FireAssert("Unhandled moment type")/EndAssert
            //   0x82255B84 <- every case ALSO branches here
            //   0x82255B88 ld r4, var_110(r1) ; 0x82255B90 ld r5, var_108(r1)   <- the handle slot
            //   0x82255B98 bl MomentHandle::Prepare
            // The default arm writes NOTHING to var_110/var_108 and falls straight into the one
            // shared Prepare, i.e. the original source hoisted Prepare out of the switch exactly
            // as it is written below. (Hex-Rays shows Prepare duplicated into all 13 arms; the ASM
            // has a single call site. Rung 1 is the asm.) Seeding lVoidHandle would be behaviour
            // the binary does not have.
            CGS_ASSERT(false, "Unhandled moment type");
            break;
    }

    lrMomentHandleInOut.Prepare(lVoidHandle, *this, lrBehaviourManager);

    CGS_ASSERT(lrMomentHandleInOut.IsAllocated(), "mbIsAllocated");
    CGS_ASSERT(lrMomentHandleInOut.GetMoment()->GetType() == leMomentType,
               "lrMomentHandleInOut.GetMoment().GetType() == leMomentType");

    Moment::Parameters* lpParameters = mMomentParameterBank.GetParameters(leMomentParamID);

    CGS_ASSERT(lrMomentHandleInOut.IsAllocated(), "mbIsAllocated");
    lrMomentHandleInOut.GetMoment()->SetParameters(lpParameters);

    return true;
}

} // namespace BrnDirector
