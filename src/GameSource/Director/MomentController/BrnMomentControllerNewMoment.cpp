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
//
// =========================================================================================
// STILL UNMOUNTED as of 2026-08-23, and the reason is now MEASURED rather than guessed.
// Each `AllocateVoid<T>` placement-constructs a T, which emits T's vftable, which needs
// EVERY one of T's virtuals defined at link -- so this one file drags all twelve moment
// subclasses in whole. Measured against the object list of the current shipping build
// (build/game/obj, 1,524 objects, archived and symbol-dumped):
//     * this TU + BrnMoment.cpp + all 14 Moments/*.cpp  ->  142 NON-CRT unresolved externals
//       (per TU: NewMoment 33 | PlayerJumping 33 | HardStop 26 | TakedownLookback 18 |
//        PlayerStunt 13 | Tumbling 10 | BystanderSeesAction 9 | StationaryCrash 9 |
//        PassengerSeesAction 5 | NewCarJoined 5 | HitTraffic 2 | StaticCamImpact 1)
//     * this TU trimmed to the two cutaway types only    ->   ~50 NON-CRT unresolved externals
// The 2026-08-01 GROUP F note that said "+9 unresolved" is STALE by an order of magnitude.
// The dominant families are (a) ~44 `detail::MomentSharedInfo_*` reach shims -- the
// MomentSharedInfo record has NO home in this tree, and homing it is the real keystone;
// (b) the four `BehaviourCollection<>` instantiations MomentPlayerJumping holds (24 symbols
// from SIX template methods -- write the template bodies once and all four resolve);
// (c) ~38 declaration-only per-class virtuals (Destruct / GetInstanceType / SetParameters /
// Prepare / Release), most of them one-liners recoverable straight from the asm.
//
// AND MOUNTING THIS ALONE WOULD STILL NOT MAKE A CUTAWAY PLAY -- see the GROUP F banner in
// DirectorLinkStubs.cpp: nothing in this tree ticks a moment. MomentController::
// UpdateAllMoments @0x82239DE8 has no body anywhere, its only caller MainDirector::
// UpdateMoments @0x82250268 is declaration-only (BrnMainDirector.h:161), and that call is
// itself commented out in MainDirector::Update (BrnMainDirector.cpp:1617, `GATE:
// UpdateMoments( lpIO, liPlayerCarIndex );`). A moment that is never Updated never leaves
// E_STATE_INVALID_INACTIVE, so it is never IsValid(), so it is never counted or selected.
// =========================================================================================

#include "GameSource/Director/MomentController/BrnMomentController.h"
// ---- the twelve concrete moment types NewMoment allocates ---------------------------------
// ⭐ 2026-08-23: this umbrella lives HERE, in the only TU that needs all twelve, and NOT in a
// shared header. BrnMomentParameterBank.h -- which is on BrnMainDirector.cpp's include path --
// used to carry it via BrnMomentSubclasses.h, and these headers are far too heavy (and, for
// BrnMomentHardStop.h, outright incompatible with BrnDirectorModuleIO.h) to sit there.
// MomentBystanderSeesAction (case 5) is homed in BrnMoment.h; MomentTumbling (case 2) arrives
// via BrnMomentController.h -> BrnMomentParameterBank.h; MomentHardStop (case 0) is STILL the
// layout-stubbed slice in BrnMomentSubclasses.h -- read that file's DELETE-WHEN.
#include "GameSource/Director/MomentController/BrnMomentSubclasses.h"                 // case 0  (SLICE -- see its banner)
#include "GameSource/Director/MomentController/Moments/BrnMomentHitTraffic.h"         // case 1
#include "GameSource/Director/MomentController/Moments/BrnMomentTakedownLookback.h"   // case 3
#include "GameSource/Director/MomentController/Moments/BrnMomentPassengerSeesAction.h"// case 4
#include "GameSource/Director/MomentController/Moments/BrnMomentFailsafe.h"           // case 6
#include "GameSource/Director/MomentController/Moments/BrnMomentPlayerJumping.h"      // case 7
#include "GameSource/Director/MomentController/Moments/BrnMomentPlayerStunt.h"        // case 8
#include "GameSource/Director/MomentController/Moments/BrnMomentStaticCamImpact.h"    // case 9
#include "GameSource/Director/MomentController/Moments/BrnMomentNewCarJoined.h"       // case 10
#include "GameSource/Director/MomentController/Moments/BrnMomentStationaryCrash.h"    // case 11
#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"              // [DIAG] one-shot jump-ladder rung only

namespace BrnDirector
{

// ---------------------------------------------------------------------------------------
// ⭐ THE BUCKET-FITS RATCHET (2026-08-23, jump/stunt cutaway wave).
//
// This is the ONE TU in the program that sees all twelve concrete moment types, so it is the
// only place the pool's slot size can be checked against them. AbstractPool::AllocateVoid<T>
// carries a runtime CGS_ASSERT for this -- but that assert is NON-FATAL: it fires and then
// the placement-new proceeds, writing sizeof(T) bytes into a slot that is too small and
// silently corrupting the neighbouring moment. Catch it at COMPILE time instead.
//
// This fired for real: with the console's literal 70-unit (1120 B) bucket,
// MomentPlayerJumping is 1296 B on this x64 host. See the HOST BUCKET WIDENING banner in
// BrnMomentController.h.
// ---------------------------------------------------------------------------------------
namespace
{
    typedef MomentController::MomentPool::Bucket MomentBucket;
    #define BRN_MOMENT_FITS(T)         static_assert(sizeof(MomentBucket) >= sizeof(T),                       "moment pool bucket too small for " #T " -- raise MomentController::KU_MOMENT_POOL_UNITS")
    BRN_MOMENT_FITS(MomentHardStop);
    BRN_MOMENT_FITS(MomentHitTraffic);
    BRN_MOMENT_FITS(MomentTumbling);
    BRN_MOMENT_FITS(MomentTakedownLookback);
    BRN_MOMENT_FITS(MomentPassengerSeesAction);
    BRN_MOMENT_FITS(MomentBystanderSeesAction);
    BRN_MOMENT_FITS(MomentFailSafe);
    BRN_MOMENT_FITS(MomentPlayerJumping);
    BRN_MOMENT_FITS(MomentPlayerStunt);
    BRN_MOMENT_FITS(MomentStaticCamImpact);
    BRN_MOMENT_FITS(MomentNewCarJoined);
    BRN_MOMENT_FITS(MomentStationaryCrash);
    #undef BRN_MOMENT_FITS
}

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
            // DO NOT "FIX" THE UNINITIALISED lVoidHandle ON THIS ARM -- the console has it too.
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

    // [DIAG] NOT IN THE X360 BINARY. Rung 5b of the `[jump-ladder]`: a moment was really
    // ALLOCATED. Until 2026-08-23 this whole function was a GROUP-F stub in
    // DirectorLinkStubs.cpp that returned true while allocating nothing, so every
    // MomentHandle stayed !IsAllocated() and no cutaway could ever exist. First-N (one
    // line per moment type at most; MomentSelector::Prepare calls this three times for
    // the roaming state and then never again).
    {
        static bool sbaLoggedType[Moment::E_MOMENT_COUNT] = { false };
        if (leMomentType >= 0 && leMomentType < Moment::E_MOMENT_COUNT &&
            !sbaLoggedType[leMomentType] && CgsDev::Log::gpDebugPrint != 0)
        {
            sbaLoggedType[leMomentType] = true;
            *CgsDev::Log::gpDebugPrint
                << "[FLAG PC bring-up] [jump-ladder] MomentController::NewMoment allocated type="
                << static_cast<s32>(leMomentType)
                << " (7=PLAYER_JUMPING 8=PLAYER_STUNT 10=NEW_CAR_JOINED)"
                << " paramID=" << static_cast<s32>(leMomentParamID)
                << " allocated=" << (lrMomentHandleInOut.IsAllocated() ? 1 : 0)
                << "\n";
        }
    }

    return true;
}

} // namespace BrnDirector
