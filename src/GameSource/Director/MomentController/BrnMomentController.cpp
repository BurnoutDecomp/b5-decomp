// Out-of-line bodies for the BrnDirector::MomentController nested helpers.
// Reconstructed from the console executable, semantic-parity.
//
// Bodied here:
//   BrnDirector::MomentController::MomentHandle::GetMoment
//   BrnDirector::MomentController::MomentHandle::Release
//   BrnDirector::MomentController::MomentHandle::Prepare   @0x821F7298 (DWARF BrnMomentController.cpp:235)
//   BrnDirector::MomentController::NewMoment               @0x82255850 (DWARF BrnMomentController.cpp:118)
//   BrnDirector::MomentController::Construct / Prepare / Destruct / UpdateAllMoments
//
// ⭐ 2026-09-24 (FX-DIRECTOR, the moment factory): NewMoment and MomentHandle::Prepare MOVED HERE from
// the project-invented split TU BrnMomentControllerNewMoment.cpp (never mounted; deleted). The DWARF
// homes both in this file, and it is mounted -- which retires DirectorLinkStubs.cpp's GROUP F stub,
// the NewMoment that allocated nothing.
//
// (Release moved here earlier, on 2026-08-01, for the same reason: the DWARF homes it in this file
// and BrnMomentSelector.cpp, which walks every handle, is mounted.)
//
// MomentDescription is a plain POD (no out-of-line member needs a body here); it is homed
// purely by the header and instantiated through Array<MomentDescription,10> in its own TU.

#include "GameSource/Director/MomentController/BrnMomentController.h"
#include "GameSource/Director/MomentController/BrnMomentSharedInfo.h"            // MomentSharedInfo (UpdateAllMoments)
#include "GameSource/Director/DirectorModule/BrnDirectorModuleDebugPrinter.h"    // DebugPrinter (the moment name lines)
#include "GameShared/GameClasses/Development/CgsStrStream.h"                     // CgsDev::StrStream (lacMessage)
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (mbIsAllocated guard)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // [DIAG] the NewMoment allocation rung
#include <cstdlib>                                           // [DIAG] getenv (BRN_CRASHCAM_DIAG)
// ---- the twelve concrete moment types NewMoment allocates ----------------------------------------
// This umbrella lives HERE, in the only TU that needs all twelve, and NOT in a shared header: these
// headers are far too heavy for BrnMainDirector.cpp's include path. MomentBystanderSeesAction (case 5)
// is homed in BrnMoment.h; MomentTumbling (case 2) and MomentHardStop (case 0) arrive through
// BrnMomentController.h -> BrnMomentParameterBank.h, which holds both records by value.
#include "GameSource/Director/MomentController/Moments/BrnMomentHitTraffic.h"         // case 1
#include "GameSource/Director/MomentController/Moments/BrnMomentTakedownLookback.h"   // case 3 (AllocateVoid)
#include "GameSource/Director/MomentController/Moments/BrnMomentPassengerSeesAction.h"// case 4
#include "GameSource/Director/MomentController/Moments/BrnMomentFailsafe.h"           // case 6
#include "GameSource/Director/MomentController/Moments/BrnMomentPlayerJumping.h"      // case 7 (AllocateVoid)
#include "GameSource/Director/MomentController/Moments/BrnMomentPlayerStunt.h"        // case 8
#include "GameSource/Director/MomentController/Moments/BrnMomentStaticCamImpact.h"    // case 9
#include "GameSource/Director/MomentController/Moments/BrnMomentNewCarJoined.h"       // case 10
#include "GameSource/Director/MomentController/Moments/BrnMomentStationaryCrash.h"    // case 11

namespace BrnDirector
{

// ---- the controller's lifecycle ------------------------------------------------------------
// DWARF BrnMomentController.cpp:24 / :33 / :105. No out-of-line console symbol for any of the
// three: MainDirector inlines them, and each is the pool's own lifecycle call (the DWARF call
// hints name AbstractPool<70,20,Vector4>::Construct / Prepare / Destruct), Construct followed by
// the parameter bank's.
//   Construct  MainDirector::Construct 0x8225B540..0x8225B554: r11 = this + 0x172D0 (the
//              controller); `std r31(=0), 0x57E8(r11)` -- the pool's occupancy word -- then
//              `addi r3, r11, 0x57F0 ; bl MomentParameterBank::Construct`.
//   Prepare    MainDirector::Prepare stage 5: the pool's free queue refilled 19..0, count 20,
//              occupancy cleared (== ObjectPool::Clear).
//   Destruct   MainDirector::Destruct @0x8224FCC0: the occupancy word cleared.
void MomentController::Construct()
{
    mMomentPool.Construct();
    mMomentParameterBank.Construct();
}

bool MomentController::Prepare()
{
    return mMomentPool.Prepare();
}

void MomentController::Destruct()
{
    mMomentPool.Destruct();
}

// ---- UpdateAllMoments @0x82239DE8 ----------------------------------------------------------
//     f31 = lrMomentSharedInfo.mfTimestep                       lfs f31, 0x51C(r29)
//     a 64-byte StrStream on the stack                           0x82239E08..0x82239E4C
//     for (liLoop = 0; liLoop < 20; ++liLoop)                    cmpwi r25, 0x14
//         if (!mMomentPool.IsObjectAllocated(liLoop)) continue;  (this + 0x10 == the ObjectPool)
//         lpMoment = mMomentPool[liLoop]                         sub_82200B30
//         lStrStream.Reset(); lStrStream << GetName()            vtable +0x18; NULL -> "<NULLSTRING>"
//         CanSwitchToMeNow (+0x178)              -> printer.PrintActive(text)      (+0x20 ? +0x14)
//         else ConditionsAreMet && IsInhibited   -> printer.Print(text, 0xFF00FFFF) (no +0x20 test)
//         else                                    -> printer.PrintInactive(text)    (+0x20 ? +0x18)
//         camera validity account (+0x148).Print(printer)
//         lpMoment->PreUpdate()                                  0x82239F48..0x82239F94
//         lpMoment->Update(f31, lrBehaviourManager, lrMomentSharedInfo)   vtable +0x08
// The printer is lrMomentSharedInfo.mpDebugPrinter (+0x500), the director's moment printer.
void MomentController::UpdateAllMoments(Camera::BehaviourManager& lrBehaviourManager,
                                        const MomentSharedInfo& lrMomentSharedInfo)
{
    const f32 lfTimeStep = lrMomentSharedInfo.mfTimestep;

    char lacMessage[64];
    CgsDev::StrStream lStrStream(lacMessage, static_cast<s32>(sizeof(lacMessage)));

    for (s32 liLoop = 0; liLoop < static_cast<s32>(KU_MOMENT_POOL_BUCKETS); ++liLoop)
    {
        if (!mMomentPool.IsObjectAllocated(liLoop))
            continue;

        Moment* lpMoment = static_cast<Moment*>(mMomentPool[liLoop]);

        lStrStream.Reset();
        const char* lpcName = lpMoment->GetName();
        lStrStream << (lpcName != 0 ? lpcName : "<NULLSTRING>");

        DebugPrinter& lrDebugPrinter = *lrMomentSharedInfo.mpDebugPrinter;
        if (lpMoment->CanSwitchToMeNow())
        {
            lrDebugPrinter.PrintActive(lacMessage);
        }
        else if (lpMoment->ConditionsAreMet() && lpMoment->IsInhibited())
        {
            lrDebugPrinter.Print(lacMessage, 0xFF00FFFFu);   // lis r5, -0x100 ; ori r5, r5, 0xFFFF
        }
        else
        {
            lrDebugPrinter.PrintInactive(lacMessage);
        }
        lpMoment->GetCamera().GetValidityAccount().Print(lrDebugPrinter);

        lpMoment->PreUpdate();
        lpMoment->Update(lfTimeStep, &lrBehaviourManager, &lrMomentSharedInfo);
    }
}

// Asserts mbIsAllocated (+0x00), then returns the held moment pointer read from +0x04.
// With the declared layout
// that word is mMomentPoolHandle.mpObject -- the moment object the
// pool handed out -- so GetMoment returns the pool handle's stored object.
Moment* MomentController::MomentHandle::GetMoment() const
{
    CGS_ASSERT(mbIsAllocated, "mbIsAllocated");
    // The const handle's Get() yields const void*; the console build GetMoment returns the stored
    // moment pointer as a mutable Moment* (it only reads the slot's object-pointer word).
    return static_cast<Moment*>(const_cast<void*>(mMomentPoolHandle.Get()));
}

// Hand the held slot back to the owning pool
// and clear the allocated flag; a no-op when nothing is held. Returns TRUE unconditionally
// (both the taken and the not-taken branch reach the same `return true` tail).
// Console walk: read mbIsAllocated -- if clear, straight to the tail; otherwise
//   * the moment's own vtable slot 4 (+0x10) Release(), inside the tripwire
//               "GetMoment().Release()" (non-gating -- the console proceeds either way);
//   * the pool handle at +0x08 / its owner at +0x0C, then vtable slot 0 on the handle --
//     the pool handle's own release-through-the-owner, i.e. AbstractPoolVoidHandle::Release;
//   * store 0 into +0x00 -> mbIsAllocated = false.
bool MomentController::MomentHandle::Release()
{
    if (mbIsAllocated)
    {
        CGS_ASSERT(GetMoment()->Release(), "GetMoment().Release()");   //  (non-gating)
        mMomentPoolHandle.Release();
        mbIsAllocated = false;
    }
    return true;
}

// ⭐ THE BUCKET-FITS RATCHET (2026-08-23, jump/stunt cutaway wave; moved here with NewMoment).
// This is the ONE TU that sees all twelve concrete moment types, so it is the only place the pool's
// slot size can be checked against them. AbstractPool::AllocateVoid<T>'s own "object is too large"
// CGS_ASSERT is NON-FATAL (it fires and the placement-new proceeds into a slot too small), so it is
// caught at COMPILE time instead. With the console's literal 70-unit (1120 B) bucket,
// MomentPlayerJumping is 1296 B on this x64 host -- see the HOST BUCKET WIDENING banner in
// BrnMomentController.h.
namespace
{
    typedef MomentController::MomentPool::Bucket MomentBucket;
    #define BRN_MOMENT_FITS(T) static_assert(sizeof(MomentBucket) >= sizeof(T), "moment pool bucket too small for " #T " -- raise MomentController::KU_MOMENT_POOL_UNITS")
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

// ---- MomentHandle::Prepare @0x821F7298 ----------------------------------------------------------
// Take ownership of a freshly placed pool slot and bring the moment up. The console, in order:
//     stw r6, 0x14(r31)                   mpParentMomentController = &lrParentMomentController
//     stb 1, 0(r31)                       mbIsAllocated = true
//     four stw to +0x04..+0x10            mMomentPoolHandle = lVoidHandle
//     lwz r11,0(obj); lwz r11,0(r11); bctrl    the moment's vtable slot 0: Construct()
//     assert mbIsAllocated (the inlined GetMoment, :141)
//     GetMoment().Prepare(lBehaviourManager)    vtable slot 1, asserted ("..Prepare(lBehaviourManager)", :242)
//     return true
// ⚠️ CORRECTED 2026-09-24: the split TU's body only stored the three members (and asserted a
// "!mbIsAllocated" the console does not have). Without the Construct call a pooled moment's meState /
// meType / inhibit flag / camera would be pool garbage the moment NewMoment asserted GetType().
bool MomentController::MomentHandle::Prepare(AbstractPoolVoidHandle lVoidHandle,
                                             MomentController& lrParentMomentController,
                                             Camera::BehaviourManager& lrBehaviourManager)
{
    mpParentMomentController = &lrParentMomentController;
    mbIsAllocated            = true;
    mMomentPoolHandle        = lVoidHandle;

    static_cast<Moment*>(const_cast<void*>(mMomentPoolHandle.Get()))->Construct();

    CGS_ASSERT(GetMoment()->Prepare(&lrBehaviourManager), "GetMoment().Prepare(lBehaviourManager)");
    return true;
}

// ---- NewMoment @0x82255850 ------------------------------------------------------------------------
// The controller's moment factory. The console spine:
//   1. Release the in/out handle (asserted).
//   2. switch (leMomentType) -> mMomentPool.AllocateVoid<MomentXxx>() for the twelve types (jump table
//      jpt_822558C4: 0 HardStop / 1 HitTraffic / 2 Tumbling / 3 TakedownLookback / 4 PassengerSeesAction
//      / 5 BystanderSeesAction / 6 FailSafe / 7 PlayerJumping / 8 PlayerStunt / 9 StaticCamImpact /
//      10 NewCarJoined / 11 StationaryCrash; default asserts "Unhandled moment type").
//   3. lrMomentHandleInOut.Prepare(lVoidHandle, *this, lrBehaviourManager) -- ONE call site, after the
//      switch (0x82255B98).
//   4. assert mbIsAllocated, then GetMoment().GetType() == leMomentType.
//   5. lpParameters = mMomentParameterBank.GetParameters(leMomentParamID).
//   6. assert mbIsAllocated, then GetMoment()->SetParameters(lpParameters) (vtable +0xC).
//
// [FX-DIRECTOR2 2026-09-25] case 7 is UN-GATED. MomentPlayerJumping is allocated as ArbStateRoaming::Construct
//   @0x82259C00 registers it ({7, 0}) and ticked every roaming frame, like the console's. Until now the arm
//   returned TRUE with the handle unallocated, so Roaming's selector held two moments where the console holds
//   three. It is INERT ON RETAIL: the moment's SEARCHING arm is gated on mbAllowJumpMoment, which
//   MainDirector::Construct seeds false (0x8225B97C) and nothing in the image writes again. Its retail path is
//   bodied; the camera side behind that gate is a LOUD trap (BrnMomentPlayerJumping.cpp).
// [FX-DIRECTOR2 2026-09-25] case 3 is UN-GATED too: MomentTakedownLookback's closure (BehaviourRig, CameraRig::
//   Construct, the presets, the look-back's own Update) is bodied and mounted. NOTE no retail selector ever asks
//   for type 3: MomentSelector::AddMoment @0x82209F80 has three callers -- ArbStateRoaming::Construct {7, 8, 10},
//   ArbStateCrashing::Construct {2, 2, 0, 5}, ArbStateTakedown::Construct {5, 2, 2} -- so this arm, like the
//   console's, is reachable only if a descriptor names it.
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
            // DO NOT "FIX" THE UNINITIALISED lVoidHandle ON THIS ARM -- the console has it too
            // (MSVC C4701 under /w14701 is a TRUE report of UB in the shipped binary): the default arm
            // writes nothing to the handle's stack slot and falls into the single shared Prepare.
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

    // [DIAG] NOT IN THE console BINARY. One line per moment TYPE the factory really allocates (the
    // selectors' Prepares call NewMoment a handful of times and then never again), so a log shows
    // which moments exist. Behind BRN_CRASHCAM_DIAG (the crash-camera gate, default off) since
    // 2026-09-24 (crash-parity diag hygiene): it used to print whenever a log existed.
    {
        static const bool sbMomentDiag = (getenv("BRN_CRASHCAM_DIAG") != 0);
        static bool sbaLoggedType[Moment::E_MOMENT_COUNT] = { false };
        if (sbMomentDiag && leMomentType >= 0 && leMomentType < Moment::E_MOMENT_COUNT &&
            !sbaLoggedType[leMomentType] && CgsDev::Log::gpDebugPrint != 0)
        {
            sbaLoggedType[leMomentType] = true;
            *CgsDev::Log::gpDebugPrint
                << "[FLAG PC bring-up] [jump-ladder] MomentController::NewMoment allocated type="
                << static_cast<s32>(leMomentType)
                << " paramID=" << static_cast<s32>(leMomentParamID)
                << " allocated=" << (lrMomentHandleInOut.IsAllocated() ? 1 : 0)
                << "\n";
        }
    }

    return true;
}

} // namespace BrnDirector
