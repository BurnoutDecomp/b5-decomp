#include "GameSource/World/CrashModule/BrnRaceCarCrash.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "GameSource/BurnoutConstants.h"              // E_ACTIVE_RACE_CAR_INDEX_COUNT

#include "rw/math/vpu/vector3_operation.h"           // Magnitude

#include <cmath>                                      // fabsf

#include <cstddef>   // offsetof

// BrnWorld::RaceCarCrash accessors, reconstructed from BURNOUT_X360_ARTIST.XEX
// (semantic parity, not byte match). Both live in World/CrashModule/BrnCrashModule.cpp.

namespace BrnWorld
{
    // GetOwner @ 0x827B1538
    //   ld    r11, 0(this)        ; read the 64-bit mRaceCarVolumeInstanceId
    //   srdi  r11, r11, 32        ; take the HIGH dword (embedded entity word)
    //   extrwi r11, r11, 14, 8    ; extract the 14-bit entity index (bit 8 big-endian == [10..23])
    //   cmplwi r11, 8 / blt ...   ; assert index < E_ACTIVE_RACE_CAR_INDEX_COUNT (8), non-gating
    //   ...re-read + re-extract -> r3 (returns the index even when the assert tripped)
    // (BrnCrashModule.cpp:174). The 14-bit extraction is exactly
    // VolumeInstanceId::GetEntityIDEntityIndex() (the canonical accessor the assert message names);
    // reuse it instead of open-coding the shift/mask.
    s32 RaceCarCrash::GetOwner() const
    {
        const u32 luIndex = mRaceCarVolumeInstanceId.GetEntityIDEntityIndex();
        CGS_ASSERT(luIndex < 8u,
                   "mRaceCarVolumeInstanceId.GetEntityIDEntityIndex() < E_ACTIVE_RACE_CAR_INDEX_COUNT");
        return static_cast<s32>(luIndex);
    }

    // SetSecondsBeforeCleanup @ 0x827B15A0
    //   fmr   f31, f1             ; lfSeconds
    //   lfs   f0, flt_82001CC0    ; 0.0f
    //   fcmpu f31, f0 / bgt ...   ; assert lfSeconds > 0.0f (non-gating)  (BrnCrashModule.cpp:194)
    //   stfs  f31, 0xC(this)      ; mfSecondsBeforeCleanup = lfSeconds  (a 32-bit float store)
    // The Hex-Rays `double a2 ... *(this+12) = a2` is the f32 store (`stfs`, not `stfd`) the asm
    // performs -- the value is held/compared/stored as a single-precision float.
    void RaceCarCrash::SetSecondsBeforeCleanup(f32 lfSeconds)
    {
        CGS_ASSERT(lfSeconds > 0.0f, "lfSeconds > 0.0f");
        mfSecondsBeforeCleanup = lfSeconds;   // stfs @0xC
    }


    // ============================================================================================
    // Construct @ 0x827B14A0
    //
    //   0x827B14C4  rldicl/rlwinm ...             -- the entity-TYPE byte of the id's high word
    //   0x827B14CC  cmplwi cr6, r11, 1            -- assert owner == E_ENTITYTYPE_RACECAR (1)
    //               FireAssert(..., BrnCrashModule.cpp:79) -- NON-GATING, execution continues
    //   0x827B14F8  stfs f31, 0xC(this)           -- mfSecondsBeforeCleanup = lfSeconds
    //   0x827B14FC  std  r30, 0(this)             -- mRaceCarVolumeInstanceId (one 64-bit store)
    //   0x827B1500  lfs  f0, 0x1CC0(r11 == 0x8200)-- flt_82001CC0; READ FROM THE IMAGE == 0.0f
    //   0x827B1508  stfs f0, 8(this)              -- mfTimeCrashing   = 0.0f
    //   0x827B150C  stfs f0, 0x10(this)           -- mfTimeStationary = 0.0f
    //   0x827B1510  stb  r11(0), 0x14(this)       -- mi8NumCleanupExtensions = 0
    //   0x827B1514  stb  r11(0), 0x15(this)       -- mu8Reserved15 = 0
    //
    // The store ORDER is the compiler's, not the source's; what is load-bearing is the set of
    // fields and their values. Note the countdown is stored through the raw member here, NOT via
    // SetSecondsBeforeCleanup -- the console emits a plain `stfs` with no `lfSeconds > 0.0f`
    // tripwire, so adding one would invent an assert the console does not have.
    // ============================================================================================
    void RaceCarCrash::Construct(CgsSceneManager::VolumeInstanceId lRaceCarVolumeInstanceId,
                                 f32 lfSeconds)
    {
        // 1u == BrnWorld::E_ENTITYTYPE_RACECAR (the enum has no home in this tree; every other
        // consumer spells it the same way -- see BrnVehicleManager.h:2655).
        CGS_ASSERT(lRaceCarVolumeInstanceId.GetEntityIDOwner() == 1u,
                   "lVolumeInstanceID.GetEntityIDOwner() == E_ENTITYTYPE_RACECAR");

        mfSecondsBeforeCleanup   = lfSeconds;
        mRaceCarVolumeInstanceId = lRaceCarVolumeInstanceId;
        mfTimeCrashing           = 0.0f;
        mfTimeStationary         = 0.0f;
        mi8NumCleanupExtensions  = 0;
        mu8Reserved15            = 0;
    }

    // ============================================================================================
    // Tick @ 0x827BF0B8  (152 insns)
    //
    // HEX-RAYS RENDERS THIS WITH 29 POSITIONAL PARAMETERS. It has eight. The X360 PPC ABI
    // gives each float argument BOTH an FPR and a GPR slot, so the register map has holes that
    // Hex-Rays fills with invented `int aN`s. Read from the asm prologue + the ONE call site
    // (CrashModule::TickCrashes @0x827C66C8..0x827C6714):
    //   r3           this
    //   f1  -> f31   lfTimeStep                       (the GPR slot r4 is consumed and unused)
    //   r5  -> r29   lpActiveRaceCarInterface         (asserted non-null, BrnCrashModule.cpp:112)
    //   r6           lbPlayerPressingBoostOutsideShowtime
    //   r7  -> r27   liMaxCrashExtensions             (CrashModule::miNumCrashExtensions, s8-extended)
    //   r8  -> r28   lbIsOfflineGameMode              (== !mbIsOnlineGameMode at the call site)
    //   r9  -> r24   lbIsPlayerCrash                  (playerIndex == this crash's owner)
    //   r10          lbIsInAGameMode
    //   sp+0x54      lpbNeedToSendEndingMessage       (&CrashModule::mbNeedToSendEndingMessage)
    //
    // r6 AND r10 ARE NEVER READ BY THIS BODY. Both are set at the call site and neither is
    // moved out of its argument register anywhere in the 152 instructions. They are kept as
    // parameters because the console passes them; they are explicitly voided below so the
    // omission is visible in the code rather than only in this banner.
    //
    // WHAT IT DOES
    //   1. Countdown. mfSecondsBeforeCleanup -= lfTimeStep, and latch whether THIS tick is the
    //      one that crossed 0.25f (flt_82003F40, image-read) from above -- the "the crash is
    //      about to end" edge. Both the before and after values are compared against the same
    //      constant, so the edge fires exactly once.
    //   2. Player-only book-keeping (skipped entirely when online, or for a non-player wreck):
    //      accumulate mfTimeCrashing and mfTimeStationary, then ZERO mfTimeStationary if the car
    //      is still moving -- |mfSpeedMPH| > 6.5f (flt_820CA5C0) or |linear velocity| > 1.5f
    //      (flt_820CA5C4), both read from the image.
    //   3. The EXTENSION. On the 0.25f edge, if the wreck has not used up its allowance and is
    //      still sliding (mfTimeStationary < 1.0f), grant one more second and count it.
    //      The 1.0f handed to SetSecondsBeforeCleanup is flt_82001C98, and the compiler REUSES
    //      the f1 it had just loaded for the `mfTimeStationary < 1.0f` compare (0x827BF2B0 loads
    //      f1, 0x827BF2B4 compares, 0x827BF2BC calls with f1 untouched). Reading only the call
    //      site would show "SetSecondsBeforeCleanup(<whatever was in f1>)"; the constant is the
    //      same 1.0f, and it is image-read, not guessed.
    //   4. Publish: *lpbNeedToSendEndingMessage = (edge && lbIsPlayerCrash && !granted-an-extension).
    //      i.e. "the PLAYER's crash is now really ending" -- an extension retracts it.
    // ============================================================================================
    void RaceCarCrash::Tick(f32 lfTimeStep,
                            const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*
                                lpActiveRaceCarInterface,
                            bool lbPlayerPressingBoostOutsideShowtime,
                            s32  liMaxCrashExtensions,
                            bool lbIsOfflineGameMode,
                            bool lbIsPlayerCrash,
                            bool lbIsInAGameMode,
                            bool* lpbNeedToSendEndingMessage)
    {
        // r6 / r10 -- passed by the console, read by nothing in this body. See the banner.
        (void)lbPlayerPressingBoostOutsideShowtime;
        (void)lbIsInAGameMode;

        // 0x827BF0E4 `lfs f30, 0xC(this)` -- the PRE-decrement value, kept for the edge test.
        const f32 lfSecondsBefore = mfSecondsBeforeCleanup;

        CGS_ASSERT(lpActiveRaceCarInterface != 0, "lpActiveRaceCarInterface");   // :112

        // 0x827BF118..0x827BF124
        const f32 lfSecondsAfter = mfSecondsBeforeCleanup - lfTimeStep;
        mfSecondsBeforeCleanup   = lfSecondsAfter;

        // 0x827BF128..0x827BF144: flt_82003F40 == 0.25f (image-read).
        const f32 lfCleanupEdge = 0.25f;
        const bool lbCrossedCleanupEdge =
            (lfSecondsAfter < lfCleanupEdge) && (lfSecondsBefore >= lfCleanupEdge);

        // 0x827BF144..0x827BF158 -- the console re-reads the id and asserts the slot, non-gating.
        const u32 luOwner = mRaceCarVolumeInstanceId.GetEntityIDEntityIndex();
        CGS_ASSERT(luOwner < static_cast<u32>(E_ACTIVE_RACE_CAR_INDEX_COUNT),
                   "mRaceCarVolumeInstanceId.GetEntityIDEntityIndex() < E_ACTIVE_RACE_CAR_INDEX_COUNT");

        bool lbGrantedExtension = false;

        // 0x827BF17C/0x827BF18C -- the whole block is skipped when online.
        if (lbIsOfflineGameMode)
        {
            const EActiveRaceCarIndex leOwner = static_cast<EActiveRaceCarIndex>(luOwner);

            // 0x827BF190..0x827BF1C4 -- the interface's own "Player car index hasn't been set"
            // tripwire (BrnRaceCarEntityModuleOutputInterface.h:980) then playerIndex == owner.
            if (lpActiveRaceCarInterface->GetPlayerActiveRaceCarIndex() == leOwner)
            {
                // 0x827BF1D0 / 0x827BF1E0 `lfs f30, 0x3CC(iface-element)` -- mfSpeedMPH, and
                // 0x827BF20C `lvx128 v13, r3, 0x340` -- mLinearVelocity, both out of the owner's
                // RaceCarState. Reached BY NAME here.
                const BrnPhysics::Vehicle::RaceCarState* lpState =
                    lpActiveRaceCarInterface->GetRaceCarState(leOwner);

                const f32     lfSpeedMPH      = lpState->mfSpeedMPH;
                const Vector3 lLinearVelocity = lpState->mLinearVelocity;

                // 0x827BF1F4..0x827BF228 -- both accumulate unconditionally, BEFORE the test.
                mfTimeStationary += lfTimeStep;
                mfTimeCrashing   += lfTimeStep;

                // 0x827BF238..0x827BF268 -- the vrsqrtefp/vmsum3fp sequence is a 3-component
                // LENGTH with the exact-zero lane selected back to zero by the vcmpeqfp/vsel.
                const f32 lfSpeed = rw::math::vpu::Magnitude(lLinearVelocity);

                // 0x827BF230/0x827BF270: flt_820CA5C0 == 6.5f, flt_820CA5C4 == 1.5f (image-read).
                if (fabsf(lfSpeedMPH) > 6.5f || lfSpeed > 1.5f)
                {
                    mfTimeStationary = 0.0f;   // flt_82001CC0 == 0.0f
                }

                // 0x827BF28C..0x827BF2D0 -- the extension. flt_82001C98 == 1.0f (image-read),
                // used BOTH as the compare bound and as the granted duration (see the banner).
                if (lbCrossedCleanupEdge &&
                    static_cast<s32>(mi8NumCleanupExtensions) < liMaxCrashExtensions &&
                    mfTimeStationary < 1.0f)
                {
                    SetSecondsBeforeCleanup(1.0f);
                    lbGrantedExtension = true;
                    ++mi8NumCleanupExtensions;
                }
            }
        }

        // 0x827BF2D4..0x827BF304
        *lpbNeedToSendEndingMessage =
            (!lbGrantedExtension && lbCrossedCleanupEdge && lbIsPlayerCrash);
    }

    // Layout pins (X360 accessor store offsets/widths).
    void RaceCarCrash::_AssertLayout()
    {
        static_assert(offsetof(RaceCarCrash, mRaceCarVolumeInstanceId) == 0,    "mRaceCarVolumeInstanceId @0");
        static_assert(offsetof(RaceCarCrash, mfSecondsBeforeCleanup)   == 0xC,  "mfSecondsBeforeCleanup @0xC");
        // [crash exit 2026-08-25] the four newly-named interior fields, each pinned by the
        // Construct/Tick store offsets quoted in the header.
        static_assert(offsetof(RaceCarCrash, mfTimeCrashing)           == 0x8,  "mfTimeCrashing @0x8 (Construct stfs 8)");
        static_assert(offsetof(RaceCarCrash, mfTimeStationary)         == 0x10, "mfTimeStationary @0x10 (Construct stfs 0x10)");
        static_assert(offsetof(RaceCarCrash, mi8NumCleanupExtensions)  == 0x14, "mi8NumCleanupExtensions @0x14 (Construct stb 0x14)");
        static_assert(offsetof(RaceCarCrash, mu8Reserved15)            == 0x15, "mu8Reserved15 @0x15 (Construct stb 0x15)");
        // sizeof pin: the Array<RaceCarCrash,8> stride asm (index*24, count word @ +0xC0 == 8*24)
        // proves the element is 24 bytes wide (see BrnRaceCarCrash.h SIZE banner).
        static_assert(sizeof(RaceCarCrash) == 24, "sizeof RaceCarCrash == 24 (Array<,8> stride)");
    }
}
