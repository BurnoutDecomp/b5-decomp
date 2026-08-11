// =================================================================================================
// GameSource/Physics/VehicleManager/BrnVehicleManager_DriverArms.cpp
//
// ⭐⭐ THE FOUR VehicleManager ARMS OF VehicleManager::UpdateDrivers @0x82642C68 -- the functions
// that move a frame's driver-control record OUT of the drained queue and INTO the per-car driver
// state the vehicle sim reads. UpdateDrivers itself landed last wave
// (BrnVehicleManager_UpdateDrivers.cpp) with all five callees as named BRN_CONDUCTOR_GATEs; this
// TU bodies four of them and BrnPhysicalTrafficManager_UpdateTrafficDriver.cpp bodies the fifth.
// All five gates in BrnPhysicsConductorGates.cpp are DELETED by this wave.
//
//   VehicleManager::UpdatePlayerDriver   @0x825E9F38  (401 insns)  DWARF BrnVehicleManager.h:875
//   VehicleManager::UpdateAIDriver       @0x825C5110  (185 insns)  DWARF BrnVehicleManager.h:881
//   VehicleManager::UpdateNetworkDriver  @0x825C4D08  (258 insns)  DWARF BrnVehicleManager.h:878
//   VehicleManager::DoHornTakedowns      @0x8263CC68  (333 insns)  DWARF BrnVehicleManager.h:1254
//
// ⭐ THE PRIOR WAVE'S "SHALLOW CLOSURE" CLAIM IS CONFIRMED, costed by address first. xrefs_from
// over all four, minus the assert/stream machinery (BeginAssert/FireAssert/EndAssert,
// BasePriorityQueue::Clear, StrStreamBase::AppendFormat, sub_821F0E50/sub_821F0EC8 = the streamed
// `operator<<(int)` helpers) and minus the __savegprlr_NN frame helpers, leaves exactly TWO real
// callees across the four:
//     memcpy                                   -- the record copies below
//     VehicleManager::InstantTakedown @0x82636108 -- ALREADY BODIED (BrnVehicleManager.cpp:499)
// There is no absent callee here. Nothing in this TU is parked and nothing is gated.
//
// ⚠️ THE INSTRUCTION COUNTS ARE 90% ASSERT MACHINERY. UpdatePlayerDriver's 401 instructions carry
// FOUR streamed asserts (each ~45 instructions of stream setup) plus the fully-inlined BitArray<8>
// iterator; its actual behaviour is ~35 instructions. Same for the other three. Per the standing
// project rule the streamed asserts are lowered to CGS_ASSERT with the static message text.
//
// ⚠️ WHAT THE CONSOLE INLINED, AND WHY IT IS *NOT* DE-INLINED HERE. All four arms end in the same
// three-to-four-store idiom against a VehicleDriver -- set meDriverType, copy the 0x48-byte
// BrnPlayerDriverControls base into mControls, clear the AI tail's mfSpeedMatchSpeed and
// mbForceComeOutOfDrift -- and BrnVehicleDriver.h declares a `VehicleDriver::Update` overload set
// that is obviously its source. It is NOT called out of line here, because the driver-type constant
// is a per-call-site IMMEDIATE (`li 0` / `li 1` / `li 2` / `li 3`) while the NETWORK arm copies only
// the 0x48 base of a 0xC0-byte BrnNetworkDriverControls -- i.e. the arms do not agree on which
// overload they would be calling, so any de-inline would have to INVENT a function boundary the
// binary does not attest. The stores are written out by name at each site instead, with this note.
// (VehicleDriver::Update has no X360 address of its own; it is inlined at every site in the image.)
//
// ⚠️ SECOND WITNESS on the two seats BrnVehicleManager_UpdateDrivers.cpp flagged:
//   * `maRaceCarDrivers` at class +64, stride 224 -- CONFIRMED INDEPENDENTLY. UpdatePlayerDriver
//     computes its element three times, each as `mulli r11, r11, 0xE0 ; add r11,r11,r17 ;
//     addi rX, r11, 0x40` (0x825EA1E8, 0x825EA270, 0x825EA2E4). Same base, same stride.
//   * `mControls.mbIsInvulnerableToVehicles` / `mbIsInvulnerableToWorld` at in-record 0x3C / 0x3D --
//     ⭐ NOW CONFIRMED, and by a ROLE-DISCRIMINATING witness rather than a coincidence of offsets.
//     This passage used to say "NEITHER CONFIRMED NOR REFUTED", because UpdatePlayerDriver never
//     addresses either byte on its own. The witness is in the sibling commit routine:
//     VehicleManager::SetRaceCarCrashing @0x82634CF0-0x82634D40 reads BOTH bytes off the victim's
//     VehicleDriver (asm +124 / +125, i.e. in-record 0x3C / 0x3D) and suppresses the crash when
//         +0x3C is set AND the AGGRESSOR entity id's owner byte is in {1, 2}, or
//         +0x3D is set AND that owner byte is in {0, 3, 5}.
//     Against BrnEntityTypes.h those two sets are exactly the vehicle owners
//     (E_ENTITYTYPE_RACECAR = 1, E_ENTITYTYPE_TRAFFIC_VEHICLE = 2) and the world/static owners
//     (E_ENTITYTYPE_WORLD = 0, E_ENTITYTYPE_PROP = 3, E_ENTITYTYPE_WORLD_GRAPHICS = 5). A byte
//     whose gate is the vehicle owner set IS mbIsInvulnerableToVehicles and a byte whose gate is
//     the world owner set IS mbIsInvulnerableToWorld -- the roles fall out of the discriminator,
//     they are not read off a guessed offset.
//     What UpdatePlayerDriver independently confirms is the *mechanism* the caller described: the
//     record copy is `memcpy(&driver.mControls, lpControls, 0x48)` -- 72 bytes, which spans both
//     bytes -- so the caller's top-of-frame `mbIsInvulnerableToWorld = true` default really is
//     overwritten by any incoming player record. The bytes this function reads by name inside the
//     record (+0x38 miVehicleIDToMerge, +0x3B mbBoost, +0x04/+0x08/+0x0C/+0x10 the four control
//     floats) all land on their committed names coherently.
//
// ⛔ WHAT STILL DOES NOT REACH A WHEEL -- see the closing note at the bottom of this file.
// =================================================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                          // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                  // gpDebugPrint / gxMessageFilterFlags
#include "GameSource/GameState/BrnTakedownType.h"                           // BrnGameState::ETakedownType
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarType.h" // BrnWorld::ERaceCarType
#include "GameSource/BurnoutConstants.h"                                    // EActiveRaceCarIndex
#include "rw/math/vpu/vector3_operation.h"                                  // Subtract / Magnitude

namespace BrnPhysics
{
namespace Vehicle
{
    // ---------------------------------------------------------------------------------------------
    // The .rdata float-pool immediates these four bodies load, resolved out of the X360 image. Each
    // symbol is already homed with the same value elsewhere in this tree (flt_82001CC0 == 0.0f and
    // flt_82001C98 == 1.0f are the pair VehicleManager::Construct uses for the mCameraMatrix
    // identity; flt_82001DA0 == 0.5f is the constant BrnVehicleManager_UpdateDrivers.cpp already
    // names as its steering-decay scale).
    // ---------------------------------------------------------------------------------------------

    // The remote driver's steering counts HALF as much as the local driver's when the two records
    // are merged (`fmadds f0, remoteSteer, 0.5, localSteer`, 0x825EA274).
    static const f32 KF_MERGED_STEERING_REMOTE_SCALE = 0.5f;    // flt_82001DA0

    // The merged steering is clamped back into the control range afterwards (two fsel clamps,
    // 0x825EA2A0..0x825EA2D4).
    static const f32 KF_MERGED_STEERING_MIN = -1.0f;            // flt_820037C8
    static const f32 KF_MERGED_STEERING_MAX =  1.0f;            // flt_82001C98

    // "No vehicle to merge" -- the sentinel BrnNetworkDriverControls::Clear @0x82581200 seeds into
    // miVehicleIDToMerge (`stb -1, 0x38(r3)`), read back here with `lbz ; extsb ; cmpwi -1`.
    static const s8 KI8_NO_VEHICLE_TO_MERGE = -1;

    // The normal-stress the horn takedown commits with (flt_820138DC, 0x8263D158). It is a pure
    // constant here: InstantTakedown consumes lfNormalStressSq for nothing (see its body's note),
    // so this value never reaches the crash.
    static const f32 KF_HORN_TAKEDOWN_NORMAL_STRESS_SQ = 50.0f; // flt_820138DC

    // The debug-log category bit DoHornTakedowns gates its one log line on
    // (`CgsDev::Message::gxMessageFilterFlags & 1`, 0x8263D01C).
    static const u64 KX_LOG_CATEGORY_DEBUG = 1;

    // EntityId packing for an active-race-car slot: `(index << 10) | 0x1000000`, the 8/14/10
    // owner|entityIndex|partIndex split with owner == BrnWorld::E_ENTITYTYPE_RACECAR (1). This is
    // the same encode BrnVehicleManager.cpp's classifiers spell inline; file-local here so this
    // slice does not depend on that TU's internal helper.
    static const u32 KU_ENTITY_INDEX_SHIFT      = 10;
    static const u32 KU_ENTITYTYPE_RACECAR_BITS = 0x1000000u;   // asm `oris rX, rY, 0x100`

    static inline EntityId MakeRaceCarEntityId(u32 luActiveRaceCarIndex)
    {
        EntityId lId;
        lId.muValue = (luActiveRaceCarIndex << KU_ENTITY_INDEX_SHIFT) | KU_ENTITYTYPE_RACECAR_BITS;
        return lId;
    }

    // ==============================================================================================
    // @0x825E9F38  VehicleManager::UpdatePlayerDriver  (401 insns)
    //
    // The PLAYER arm (E_DRIVER_TYPE_PLAYER, case 0 of UpdateDrivers' jumptable). Four things, in
    // this order:
    //   1. claim the car's bit in the caller's per-frame BitArray<8> (double-update tripwire);
    //   2. the FIRST player record of the frame contributes its steering to mfPlayerRecentSteering
    //      -- the running average UpdateDrivers then decays in fixed 0.02 s steps. `mbUpdatedPlayerDriver`
    //      is what makes it "first": UpdateDrivers cleared it at the top of the frame;
    //   3. install the record into maRaceCarDrivers[miVehicleID] -- either verbatim, or MERGED with
    //      a second driver's record when miVehicleIDToMerge names one (the co-op / dual-input path:
    //      brake is dropped, boost is OR-ed, gas and handbrake take the MAXIMUM, steering takes the
    //      local plus HALF the remote, clamped to [-1,1]);
    //   4. clear every live car's one-frame `mbJustBeenSlammed` latch.
    //
    // ⚠️ Step 4 is genuinely global and genuinely here -- it is not inside either branch and it is
    // not indexed by this record's car (asm 0x825EA314..0x825EA56C, the fully-inlined
    // BitArray<8>::GetFirstNonZeroBit/GetNextNonZeroBit walk over mUsedRaceCars, storing 0 to
    // `5216*idx + 0x1A9D` == maRaceCarVehicles[idx] + 0x135D == mbJustBeenSlammed). The 5216 is the
    // CONSOLE stride and is NOT reproduced: the host RaceCarPhysics is 5008 bytes, so the element is
    // reached by NAME through the typed array. Same rule the whole file follows.
    //
    // ⚠️ ASSERTS NOT REPRODUCED, named rather than dropped silently: the console fires the
    // CgsBitArray.h:203 ("invalid index : N < 8") and :222 ("Index: N, Number of bits: 8") bounds
    // tripwires from inside the inlined IsBitSet/SetBit at three sites. This tree's CgsBitArray.h
    // deliberately carries no assert dependency (see its banner), and every one of those three sites
    // is dominated by the KI_MAX_ACTIVE_RACE_CARS assert immediately below, which tests the same
    // index against the same bound. Reproducing them would add three identical CGS_ASSERTs.
    // ==============================================================================================
    void VehicleManager::UpdatePlayerDriver(const BrnPlayerDriverControls* lpControls,
                                            CgsContainers::BitArray<8u>& lrUpdatedCars)
    {
        const s32 liVehicleID = lpControls->miVehicleID;

        // 0x825E9F54..0x825E9F80 -- BrnVehicleManager.cpp:3687 (0xE67).
        CGS_ASSERT(liVehicleID < static_cast<s32>(E_ACTIVE_RACE_CAR_INDEX_COUNT),
                   "lpControls->miVehicleID < BrnWorld::KI_MAX_ACTIVE_RACE_CARS");

        // 0x825EA048..0x825EA0F4 -- BrnVehicleManager.cpp:3688 (0xE68). Console streams the id
        // ("Race car " << id << " has had multiple controller updates this frame").
        CGS_ASSERT(!lrUpdatedCars.IsBitSet(static_cast<u32>(liVehicleID)),
                   "Race car has had multiple controller updates this frame");

        // 0x825EA190..0x825EA1AC -- claim the bit for this frame.
        lrUpdatedCars.SetBit(static_cast<u32>(liVehicleID));

        // 0x825EA1B0..0x825EA1D4 -- first player record of the frame only.
        if (!mbUpdatedPlayerDriver)
        {
            mbUpdatedPlayerDriver   = true;
            mfPlayerRecentSteering += lpControls->mfSteering;
        }

        VehicleDriver& lrDriver = maRaceCarDrivers[liVehicleID];

        if (lpControls->miVehicleIDToMerge == KI8_NO_VEHICLE_TO_MERGE)
        {
            // ---- 0x825EA2E4..0x825EA310: the ordinary single-driver path, verbatim record copy.
            lrDriver.meDriverType = E_DRIVER_TYPE_PLAYER;                 // stw 0, 0xD0(r31)
            static_cast<BrnPlayerDriverControls&>(lrDriver.mControls) = *lpControls;  // memcpy 0x48
            lrDriver.mControls.mfSpeedMatchSpeed = 0.0f;                  // stfs 0.0f, 0x48(r31)
        }
        else
        {
            // ---- 0x825EA1E8..0x825EA2DC: TWO drivers on one car. The other driver's already
            //      installed record is the second input; the two are blended per channel.
            const VehicleDriver* const lpRemoteDriverControls =
                &maRaceCarDrivers[lpControls->miVehicleIDToMerge];

            // 0x825EA1F4..0x825EA214 -- BrnVehicleManager.cpp:3706 (0xE7A). The console really does
            // null-check the element pointer it just computed off `this`; kept as the tripwire it is.
            CGS_ASSERT(lpRemoteDriverControls != 0, "lpRemoteDriverControls");

            const BrnPlayerDriverControls& lrRemoteControls = lpRemoteDriverControls->mControls;

            // 0x825EA218 -- the working copy the merge mutates (memcpy 0x48 to the stack).
            BrnPlayerDriverControls lMergedControls = *lpControls;

            // 0x825EA238 -- the merged record NEVER brakes.
            lMergedControls.mfBrake = 0.0f;

            // 0x825EA22C..0x825EA268 -- either driver holding boost boosts the car.
            lMergedControls.mbBoost = (lrRemoteControls.mbBoost || lMergedControls.mbBoost);

            // 0x825EA258..0x825EA274 -- local steering plus HALF the remote's.
            f32 lfMergedSteering = lrRemoteControls.mfSteering * KF_MERGED_STEERING_REMOTE_SCALE
                                 + lMergedControls.mfSteering;

            lrDriver.meDriverType = E_DRIVER_TYPE_PLAYER;                 // stw 0, 0xD0(r31)

            // 0x825EA2B8 / 0x825EA2C0 -- `fsel(remote - local, remote, local)` on both pedals: the
            // car obeys whichever driver is asking for MORE.
            lMergedControls.mfHandBrake = (lrRemoteControls.mfHandBrake >= lMergedControls.mfHandBrake)
                                        ? lrRemoteControls.mfHandBrake : lMergedControls.mfHandBrake;
            lMergedControls.mfGas       = (lrRemoteControls.mfGas >= lMergedControls.mfGas)
                                        ? lrRemoteControls.mfGas : lMergedControls.mfGas;

            // 0x825EA2A0 / 0x825EA2D0 -- the two fsel clamps, low bound first (matching the asm's
            // order; the result is the same closed clamp either way).
            if (lfMergedSteering < KF_MERGED_STEERING_MIN)
                lfMergedSteering = KF_MERGED_STEERING_MIN;
            if (lfMergedSteering > KF_MERGED_STEERING_MAX)
                lfMergedSteering = KF_MERGED_STEERING_MAX;
            lMergedControls.mfSteering = lfMergedSteering;

            static_cast<BrnPlayerDriverControls&>(lrDriver.mControls) = lMergedControls; // memcpy 0x48
            lrDriver.mControls.mfSpeedMatchSpeed = 0.0f;                  // stfs 0.0f, 0x48(r31)
        }

        // ---- 0x825EA318: common tail of both branches (`stb 0, 0x4D(r31)`). A player record never
        //      carries the AI payload, so the AI tail's drift override is cleared with it.
        lrDriver.mControls.mbForceComeOutOfDrift = false;

        // ---- 0x825EA314..0x825EA56C: drop every live car's one-frame slam latch.
        for (s32 liCar = mUsedRaceCars.GetFirstNonZeroBit();
             liCar != CgsContainers::BitArray<8u>::KI_INVALID_BITINDEX;
             liCar = mUsedRaceCars.GetNextNonZeroBit(liCar))
        {
            maRaceCarVehicles[liCar].mbJustBeenSlammed = false;
        }
    }

    // ==============================================================================================
    // @0x825C5110  VehicleManager::UpdateAIDriver  (185 insns)
    //
    // The AI arm (E_DRIVER_TYPE_AI, case 1). Two independent installs, and the console really does
    // test the same condition twice rather than if/else-ing it:
    //   * if this record drives the LOCAL PLAYER'S car, it is ALSO installed into the manager's
    //     spare mPlayerAiDriver and mbPlayerAiDriverValid is raised. That is the "the AI is
    //     currently driving the player's car" seat (junkyard/attract/AI-takeover), kept separately
    //     so the real player record can still land in the per-car slot.
    //   * the per-car install is SKIPPED only when this record is for the player's car AND a player
    //     record already arrived this frame (`mbUpdatedPlayerDriver`) -- i.e. a live pad beats the
    //     AI for the same car, but the AI's copy still sits in mPlayerAiDriver.
    //
    // ⚠️ THE COPY IS 80 BYTES HERE, not 72: `memcpy(dst, src, 0x50)` at both sites installs the WHOLE
    // BrnAIDriverControls -- base plus mfSpeedMatchSpeed/mbDoSpeedMatch/mbForceComeOutOfDrift/
    // mbSlamPlayer -- which is exactly why this arm, alone of the four, does not clear the AI tail
    // afterwards. Expressed as a whole-object assignment rather than a base-slice.
    // ==============================================================================================
    void VehicleManager::UpdateAIDriver(const BrnAIDriverControls* lpControls,
                                        CgsContainers::BitArray<8u>& lrUpdatedCars)
    {
        const s32 liVehicleID = lpControls->miVehicleID;
        const bool lbIsPlayersCar = (liVehicleID == static_cast<s32>(mePlayerActiveRaceCarIndex));

        // 0x825C5140..0x825C5160 (`stw 1, this+172176` == mPlayerAiDriver.meDriverType, then the
        // 80-byte copy, then the valid flag).
        if (lbIsPlayersCar)
        {
            mPlayerAiDriver.meDriverType = E_DRIVER_TYPE_AI;
            mPlayerAiDriver.mControls    = *lpControls;
            mbPlayerAiDriverValid        = true;
        }

        if (!lbIsPlayersCar || !mbUpdatedPlayerDriver)
        {
            // BrnVehicleManager.cpp:3803 (0xEDB).
            CGS_ASSERT(liVehicleID < static_cast<s32>(E_ACTIVE_RACE_CAR_INDEX_COUNT),
                       "lpControls->miVehicleID < BrnWorld::KI_MAX_ACTIVE_RACE_CARS");
            // BrnVehicleManager.cpp:3804 (0xEDC) -- streamed with the car index.
            CGS_ASSERT(!lrUpdatedCars.IsBitSet(static_cast<u32>(liVehicleID)),
                       "Race car has had multiple controller updates this frame");

            lrUpdatedCars.SetBit(static_cast<u32>(liVehicleID));

            VehicleDriver& lrDriver = maRaceCarDrivers[liVehicleID];
            lrDriver.meDriverType = E_DRIVER_TYPE_AI;      // stw 1, 224*id + this + 272
            lrDriver.mControls    = *lpControls;           // memcpy 0x50 -- the FULL AI record
        }
    }

    // ==============================================================================================
    // @0x825C4D08  VehicleManager::UpdateNetworkDriver  (258 insns)
    //
    // The NETWORK arm (E_DRIVER_TYPE_NETWORK, case 2). It is almost entirely a gate: a network
    // record is only allowed to drive a car whose maeRaceCarTypes[] slot says the car IS a network
    // car; a record aimed at an INACTIVE slot is asserted-and-dropped, and any other type is a hard
    // tripwire.
    //
    // ⚠️ THE COPY IS 72 BYTES, not 192. BrnNetworkDriverControls is 0xC0 (it carries mTransform,
    // the two velocities, mfCatchupTime, meCrasherRaceCarIndex, mbSnap, mbCrash) but this arm's
    // `memcpy` is `li r5, 0x48` -- ONLY the BrnPlayerDriverControls base reaches the driver record.
    // The catch-up interpolation half of a network record is consumed elsewhere (the
    // VehicleDriver::StartCatchupInterpolation path), NOT here. Reproduced as a base-slice
    // assignment so the extra 120 bytes provably cannot leak into the 80-byte mControls.
    //
    // ⚠️ The console re-loads maeRaceCarTypes[id] and tests `== E_RACE_CAR_TYPE_NETWORK`
    // THREE times, not twice: 0x825C4E30, 0x825C4ECC and 0x825C50CC. (This note used to say
    // "TWICE"; corrected 2026-08-11.) Nothing between the three tests can change it, so it is
    // written as one `if` here; the reloads are a scheduling artefact, not re-reads of changed state.
    //
    // ⚠️ SILENT ENUM COUPLING at the meDriverType store: the console does NOT materialise a literal 2
    // there -- it stores the maeRaceCarTypes[id] value it just LOADED, i.e. it writes an
    // ERaceCarType into an EDriverType field and relies on E_RACE_CAR_TYPE_NETWORK == 2 ==
    // E_DRIVER_TYPE_NETWORK (BrnRaceCarType.h:12 / BrnVehicleDriverControls.h:27). Written by name
    // here because the value is identical on this build; if either enum is ever renumbered the
    // console's coupling breaks and this line is where it shows up.
    // ==============================================================================================
    void VehicleManager::UpdateNetworkDriver(const BrnNetworkDriverControls* lpControls,
                                             CgsContainers::BitArray<8u>& lrUpdatedCars)
    {
        const s32 liVehicleID = lpControls->miVehicleID;

        // BrnVehicleManager.cpp:3749 (0xEA5) / :3750 (0xEA6). Both stream "lpControls->miVehicleID="
        // followed by the value; the two bounds are separate asserts on the console.
        CGS_ASSERT(liVehicleID < static_cast<s32>(E_ACTIVE_RACE_CAR_INDEX_COUNT),
                   "lpControls->miVehicleID=");
        CGS_ASSERT(liVehicleID >= 0, "lpControls->miVehicleID=");

        const BrnWorld::ERaceCarType leRaceCarType = maeRaceCarTypes[liVehicleID];

        // BrnVehicleManager.cpp:3756 (0xEAC) -- streams "VehicleID=" << id << " maeRaceCarTypes[ID]="
        // << type. INACTIVE is tolerated (the record is simply dropped); anything else is a bug.
        CGS_ASSERT(leRaceCarType == BrnWorld::E_RACE_CAR_TYPE_INACTIVE ||
                   leRaceCarType == BrnWorld::E_RACE_CAR_TYPE_NETWORK,
                   "VehicleID= maeRaceCarTypes[ID]=");

        if (leRaceCarType == BrnWorld::E_RACE_CAR_TYPE_NETWORK)
        {
            // BrnVehicleManager.cpp:3761 (0xEB1) -- streamed with the car index.
            CGS_ASSERT(!lrUpdatedCars.IsBitSet(static_cast<u32>(liVehicleID)),
                       "Race car has had multiple controller updates this frame");
            lrUpdatedCars.SetBit(static_cast<u32>(liVehicleID));

            VehicleDriver& lrDriver = maRaceCarDrivers[liVehicleID];
            lrDriver.meDriverType = E_DRIVER_TYPE_NETWORK;                // stw 2, +0xD0
            static_cast<BrnPlayerDriverControls&>(lrDriver.mControls) = *lpControls;  // memcpy 0x48
            lrDriver.mControls.mbForceComeOutOfDrift = false;             // stb 0, +0x4D
            lrDriver.mControls.mfSpeedMatchSpeed     = 0.0f;              // stfs 0.0f, +0x48
        }
    }

    // ==============================================================================================
    // @0x8263CC68  VehicleManager::DoHornTakedowns  (333 insns)
    //
    // The cheat/debug horn takedown, run by UpdateDrivers AFTER the queue drain. Gated on the
    // (unseeded, debug-menu) DEBUG_mbHornTakedownEnabled AND on the player car's horn button as of
    // LAST frame's installed controls -- `maRaceCarVehicles[player].mPreviousControls.mbHorn`, read
    // at `5216*player + 6160` byte 2 == RaceCarPhysics +0x10D2 == mPreviousControls +0x42 == mbHorn.
    // When both hold it finds the live race car NEAREST the player (excluding the player) and
    // instantly takes it down.
    //
    // ⚠️ THE COMMITTED SIGNATURE'S PARAMETER ORDER IS (Request, Vehicle, Manager, Deformation) --
    // that is UpdateDrivers' own call order (r4..r7 @0x82642E14) -- while InstantTakedown's is
    // (Request, Manager, Vehicle, Deformation). The forward below CROSSES the middle two on purpose;
    // the asm agrees exactly (0x8263D130..0x8263D17C loads r7=arg1, r8=arg3, r9=arg2, r10=arg4).
    //
    // ⚠️ leTakedownType == E_TAKEDOWN_T_BONE (2). This is asm-literal but SEMANTICALLY ODD, so it is
    // flagged rather than "corrected": the value is `li r6, 2 ; stw r6, sp+0x7C` @0x8263D14C/D168,
    // and sp+0x7C is the same outgoing stack slot CheckForHeadToHead @0x8263D1A0 writes its
    // E_TAKEDOWN_HEAD_ON (5) into at `stw r26, 0xF0+var_74(r1)` -- two frames of different size,
    // same r1-relative slot, which is what identifies it as the 10th argument and not a local.
    // (Hex-Rays renders that `li r6, 2` as InstantTakedown's FOURTH argument. It is not: r6 is dead
    // scratch at the call, because lfNormalStressSq's float skips its GPR slot -- the same PPC
    // float-ABI hole BrnVehicleManager_UpdateDrivers.cpp documents for UpdateDrivers' own r4.)
    //
    // ⚠️ The collision normal is a CONSTANT world-up (0,1,0) built on the stack at
    // 0x8263D120..0x8263D13C and loaded into v1; the contact point is the victim's own position.
    // A horn takedown has no real contact, so the commit is handed a synthetic one.
    // ==============================================================================================
    void VehicleManager::DoHornTakedowns(
            BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
            VehicleOutputInterface* lpVehicleOutputInterface,
            VehicleManagerOutputInterface* lpManagerOutputInterface,
            BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface)
    {
        const s32 liPlayerRaceCarIndex = static_cast<s32>(mePlayerActiveRaceCarIndex);

        // 0x8263CCA8..0x8263CCE4 -- both halves of the gate, in the console's order.
        if (!DEBUG_mbHornTakedownEnabled)
            return;
        if (!maRaceCarVehicles[liPlayerRaceCarIndex].mPreviousControls.mbHorn)
            return;

        // 0x8263CD18..0x8263CD20 -- the player's world position, hoisted out of the loop.
        const Vector3 lPlayerPosition = maRaceCarVehicles[liPlayerRaceCarIndex].GetPosition();

        // 0x8263CCFC / 0x8263CD04 -- the "nothing found yet" seeds (r16 == -1, f31 == 0.0f).
        s32 liNearestRaceCarIndex = CgsContainers::BitArray<8u>::KI_INVALID_BITINDEX;
        f32 lfNearestDistance     = 0.0f;

        // 0x8263CD24..0x8263D018 -- the inlined BitArray<8> walk over mUsedRaceCars, skipping the
        // player's own slot. The VMX body is `Magnitude(other - player)`: dot3, rsqrt with two
        // Newton refinements, then `dot * rsqrt(dot)` with a vsel guarding dot == 0 -- which is
        // exactly rw::math::vpu::Magnitude's committed lowering, so it is spelled as that.
        for (s32 liCar = mUsedRaceCars.GetFirstNonZeroBit();
             liCar != CgsContainers::BitArray<8u>::KI_INVALID_BITINDEX;
             liCar = mUsedRaceCars.GetNextNonZeroBit(liCar))
        {
            if (liCar == liPlayerRaceCarIndex)
                continue;

            const f32 lfDistance = rw::math::vpu::Magnitude(
                rw::math::vpu::Subtract(maRaceCarVehicles[liCar].GetPosition(), lPlayerPosition));

            if (liNearestRaceCarIndex == CgsContainers::BitArray<8u>::KI_INVALID_BITINDEX ||
                lfDistance < lfNearestDistance)
            {
                liNearestRaceCarIndex = liCar;
                lfNearestDistance     = lfDistance;
            }
        }

        // 0x8263D01C..0x8263D0B4 -- emitted BEFORE the "did we find one" test, so the console really
        // does log "Doing Horn Takedown on -1" when the sweep finds nobody. Order preserved.
        if ((CgsDev::Message::gxMessageFilterFlags & KX_LOG_CATEGORY_DEBUG) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "Doing Horn Takedown on " << liNearestRaceCarIndex << "\n";
        }

        // 0x8263D0B8.
        if (liNearestRaceCarIndex == CgsContainers::BitArray<8u>::KI_INVALID_BITINDEX)
            return;

        // 0x8263D0C4 / 0x8263D0FC -- the EntityId ctor bound check, fired once per packed id
        // (CgsEntityId.h:116). Dead for a 0..7 slot index; emitted on console, so kept.
        CGS_ASSERT(static_cast<u32>(liNearestRaceCarIndex) < (1u << 14),
                   "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)");
        CGS_ASSERT(static_cast<u32>(liPlayerRaceCarIndex) < (1u << 14),
                   "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)");

        // ⭐ CAST RETIRED 2026-08-11 (consolidation wave). This seam used to reinterpret_cast
        // `lpVehicleOutputInterface` into a `BrnGameState::GameStateModuleIO::VehicleOutputInterface*`
        // because InstantTakedown's committed declaration named that (non-existent) fork type. The
        // DWARF settled it -- NS0_ is BrnPhysics::Vehicle in all three takedown-chain mangles --
        // and the declarations are now re-typed, so the pointer is passed straight through:
        //   InstantTakedown             ..PNS0_29VehicleManagerOutputInterfaceE PNS0_22VehicleOutputInterfaceE ..
        //   SetRaceCarCrashing          ..PNS0_29VehicleManagerOutputInterfaceE PNS0_22VehicleOutputInterfaceE ..
        //   HandleRaceCarRaceCarContact ..PNS0_22VehicleOutputInterfaceE PNS0_29VehicleManagerOutputInterfaceE ..

        // 0x8263D11C..0x8263D180. Victim == the nearest car (r4), aggressor == the player (r5).
        InstantTakedown(MakeRaceCarEntityId(static_cast<u32>(liNearestRaceCarIndex)),
                        MakeRaceCarEntityId(static_cast<u32>(liPlayerRaceCarIndex)),
                        Vector3{ 0.0f, 1.0f, 0.0f, 0.0f },                           // v1: world up
                        maRaceCarVehicles[liNearestRaceCarIndex].GetPosition(),       // v2
                        KF_HORN_TAKEDOWN_NORMAL_STRESS_SQ,                            // f1
                        lpRequestOutputInterface,
                        lpManagerOutputInterface,
                        lpVehicleOutputInterface,
                        lpDeformationInterface,
                        BrnGameState::E_TAKEDOWN_T_BONE);                              // FLAG: see banner
    }

    // ==============================================================================================
    // ⭐⭐ WHAT THIS WAVE ACTUALLY CONNECTS, and what it does NOT. Stated here because the caller's
    // banner ends with "a control still does not reach a wheel" and that sentence now needs an
    // exact successor.
    //
    // CONNECTED, end to end, by name:
    //     RaceCarEntityModule::ProcessPlayerVehicleInput  (the producer, sibling wave)
    //       -> VariableEventQueue<5040,16>::AddEvent      (0x48 bytes into the driver queue)
    //       -> VehicleManager::UpdateDrivers              (landed last wave; drains + dispatches)
    //       -> VehicleManager::UpdatePlayerDriver         (THIS FILE)
    //       -> maRaceCarDrivers[id].mControls             (a live BrnAIDriverControls record)
    // A nonzero mfGas / mfSteering / mfBrake / mbBoost pushed by the pad now genuinely LANDS in
    // `VehicleManager::maRaceCarDrivers[carIndex].mControls` and survives the frame. That was not
    // true before this wave -- it stopped at a log-once gate.
    //
    // ⭐ CONNECTED AFTER ALL -- the "next seam" this banner originally named was a mis-diagnosis,
    // corrected the same day (conductor, 2026-08-11) once the 219 instructions were actually read.
    // `VehicleDriver::UpdateVehicle(VehiclePhysics*)` @0x825D7290 is landed (BrnVehicleDriver.cpp)
    // and it NEVER TOUCHES mControls: it is the slerp-transform applier (gated on
    // mi8NumOfInterpSteps > 0; a no-op in normal driving). No controls-installer hop exists or is
    // needed, because the mounted per-car loop passes the record STRAIGHT INTO the integrator:
    //     BrnVehicleManager_UpdateVehiclePhysics.cpp:504
    //         maRaceCarVehicles[liCar].Update(..., maRaceCarDrivers[liCar].GetControls(), ...)
    // and VehiclePhysics::Update @0x826412C0 (re-verified callee-exact the same day) memcpy's
    // that parameter into mPreviousControls itself before dispatching UpdateDriving @0x82638198.
    // So the chain from this TU's install to a wheel is closed:
    //     maRaceCarDrivers[i].mControls
    //       -> UpdateVehiclePhysics per-car loop (GetControls(), by parameter)
    //       -> VehiclePhysics::Update -> UpdateDriving  (+ UpdateBoost / UpdateDriftScale / ...)
    //       -> Wheel steering + engine torque
    // DoHornTakedowns additionally closes its own loop: it reads mPreviousControls.mbHorn and
    // calls the bodied InstantTakedown, so the horn cheat is live the moment controls flow.
    // LESSON (third instance this wave): a hypothesis about unread instructions was quoted
    // forward through three banners -- always read the body before naming it a gap.
    // ==============================================================================================
}
}
