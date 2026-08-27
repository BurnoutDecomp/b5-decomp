#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"  // RaceCarPhysics::SetPlayerVehicleInShowtime (declare-only callee)
#include "GameShared/GameClasses/Core/CgsAssert.h"                            // CGS_ASSERT
#include "GameShared/GameClasses/Containers/CgsBitArray.h"                    // CgsContainers::BitArray<N>

#include <cstddef>  // offsetof (layout asserts)
#include <cstring>  // std::memcpy (asm reinterprets a stat word's bit pattern as int / float)

// BrnPhysics::Vehicle::VehicleManager -- the player-stats / showtime / network / id-lookup surface.
// This second TU bodies nine X360 functions that are independent of the takedown classifier chain in
// BrnVehicleManager.cpp:
//   ApplyPlayerStats                              (@0x8259BF00)
//   GetTrafficPhysicsEntityIDFromGlobalEntityID_Safe (@0x825B4DE0)
//   GetVehiclePhysi                               (@0x825B4F50, racecar branch bodied; see FLAG)
//   HasRaceCarHadRecentImpact                     (@0x825B4EB8)
//   SetNetworkRaceCarHidden                       (@0x825C3040)
//   SetPlayerActiveRaceCarIndex                   (@0x8259C028)
//   SetShowtimeBehaviour                          (@0x8259C098)
//   SetPlayerCarToShowtimeMode                    (@0x8259C108)
//   VehicleManager (constructor)                  (@0x827E4D58)  -- DEFERRED, see FLAG at the bottom.
//
// All member access is BY NAME against the §7 layout in BrnVehicleManager.h (offsets there are
// asm-proven). Every literal/constant below is taken from the X360 asm (a stored immediate / cmp
// operand) -- no fabricated constants.

namespace BrnPhysics
{
namespace Vehicle
{
    // The integer-stat -> showtime-strength scale (X360 flt_82004014). The asm sign-extends
    // lpSendCarStatsAction[1] as an integer, converts to float, and multiplies by this. The
    // pseudocode renders the multiplier as the literal 0.1, which is the recovered rodata value.
    static const f32 KF_STAT_STRENGTH_SCALE = 0.1f;   // X360 flt_82004014 == 0.1f

    // The "no physical vehicle" sentinel stored in the global->physical index map (asm cmplwi 0x7F).
    static const unsigned char KU8_NO_PHYSICAL_VEHICLE = 127;   // 0x7F

    // The largest valid GLOBAL traffic entity index (asm cmplwi 0x258 -> assert idx < 600).
    static const u32 KU_MAX_GLOBAL_TRAFFIC_ENTITY_INDEX = 600;  // sizeof(map) == 0x258

    // EntityId packing: the entity index occupies bits 10..23, the owner/type occupies bits 24..31.
    // GetTrafficPhysicsEntityIDFromGlobalEntityID_Safe re-encodes (physicalIndex << 10) | 0x2000000;
    // the 0x2000000 type bits == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE (owner byte 2). Matches the
    // owner-byte tests (HIBYTE == 1 racecar, == 2 traffic) used by GetVehiclePhysi.
    static const u32 KU_ENTITYTYPE_TRAFFIC_VEHICLE_BITS = 0x2000000u;  // asm oris r11, r11, 0x200
    static const u32 KU_ENTITY_INDEX_SHIFT              = 10;
    static const u32 KU_ENTITY_INDEX_MASK               = 0x3FFFu;     // 14 bits
    static const u32 KU_ENTITY_OWNER_RACECAR            = 1;           // HIBYTE == 1
    static const u32 KU_ENTITY_OWNER_TRAFFIC_VEHICLE    = 2;           // HIBYTE == 2

    // -------------------------------------------------------------------------------------------
    // ApplyPlayerStats  @0x8259BF00
    //
    // Copies the per-frame player-car stats action (a pointer to >= 6 floats) into the manager's
    // player-stats block + the player car's record. The X360 layout of the source action:
    //   [0] -> miCarSpeed                (asm v3[43082] == class +172328)
    //   [1] -> miCarStrength             (asm v3[43083] == +172332); ALSO, treated as an INTEGER,
    //          sign-extended and * 0.1f -> mfPlayerStatStrength (asm v3[43080] == +172320)
    //   [2] -> miCarControl              (asm v3[43084] == +172336)
    //   [3] -> miCarBoost                (asm v3[43085] == +172340)
    //   [4] -> mfPlayerStatDamageLimit   (asm v3[43081] == +172324)
    //   [5] -> meCarType                 (asm v3[43086] == +172344); ALSO into the player car's
    //          record at in-record +5084 (asm stw r10, 0x1B1C(5216*playerIdx + this)).
    // The five destinations named here are int32/enum in the DWARF (:997-:1001), which is why the
    // "treated as an INTEGER" note on [1] was needed -- the whole payload is integers.
    // -------------------------------------------------------------------------------------------
    void VehicleManager::ApplyPlayerStats(const f32* lpSendCarStatsAction)
    {
        CGS_ASSERT(lpSendCarStatsAction != nullptr, "lpSendCarStatsAction");

        const s32 liPlayerIndex = static_cast<s32>(mePlayerActiveRaceCarIndex);
        CGS_ASSERT(liPlayerIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT && liPlayerIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID,
                   "( mePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT ) && ( mePlayerActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID )");

        // The raw stats block (asm writes [0],[1],[2],[3],[5] as WORDS).
        // the destination used to be modelled as `f32 maPlayerCarStats[5]`.
        // The DWARF names these `int32_t miCarSpeed/miCarStrength/miCarControl/miCarBoost` +
        // `BrnResource::ECarType meCarType`, and VehicleManager::Construct proves the types at the
        // opcode level (it seeds +172320/+172324 with `stfsx` and +172328..+172344 with `stwx`).
        // The X360 copy is a raw `lwz`/`stw` word move either way, so these stay BIT-PRESERVING
        // memcpys rather than float->int conversions, which would change the bytes.
        // FLAG: the `const f32*` parameter type is itself the thing to revisit -- the action's
        // payload is integers, which is why the strength computation below already had to
        // bit-reinterpret element [1]. Left alone here so no caller signature moves in a layout wave.
        std::memcpy(&miCarSpeed,    &lpSendCarStatsAction[0], sizeof(miCarSpeed));
        std::memcpy(&miCarStrength, &lpSendCarStatsAction[1], sizeof(miCarStrength));
        std::memcpy(&miCarControl,  &lpSendCarStatsAction[2], sizeof(miCarControl));
        std::memcpy(&miCarBoost,    &lpSendCarStatsAction[3], sizeof(miCarBoost));
        std::memcpy(&meCarType,     &lpSendCarStatsAction[5], sizeof(meCarType));

        // NAME + TYPE FIXED 2026-08-03 (VehiclePhysics own-block wave). This used to write a
        // proposed-name `f32 mfPlayerBoostStrengthStat`. The asm stores the SAME word twice --
        // `lwz r10,0x14(r30) ; stwx r10,r31,0x2A138` (this class's meCarType, three lines above)
        // and `lwz r10,0x14(r30) ; stw r10,0x1B1C(r11)` (the record) -- and in-record 5084 is
        // VehiclePhysics::meCarType, seated with zero slack by that class's own-block closure. So
        // the record copy is the per-car CAR TYPE, an int, not a float boost stat. The manager-level
        // sibling in this very function was already correctly named meCarType.
        maRaceCarVehicles[liPlayerIndex].meCarType = meCarType;   // asm: the same action[5] word

        // The showtime strength: stat [1] taken as a signed INTEGER, converted to float, * 0.1f
        // (asm extsw -> fcfid -> frsp -> fmuls flt_82004014). The bit pattern of lpSendCarStatsAction[1]
        // is reinterpreted as an s32 before the integer->float convert.
        s32 liStatOneAsInt;
        const f32 lfStatOne = lpSendCarStatsAction[1];
        std::memcpy(&liStatOneAsInt, &lfStatOne, sizeof(liStatOneAsInt));
        mfPlayerStatStrength = static_cast<f32>(liStatOneAsInt) * KF_STAT_STRENGTH_SCALE;

        // The showtime damage limit: stat [4] verbatim.
        mfPlayerStatDamageLimit = lpSendCarStatsAction[4];
    }

    // -------------------------------------------------------------------------------------------
    // GetTrafficPhysicsEntityIDFromGlobalEntityID_Safe  @0x825B4DE0
    //
    // Resolve a GLOBAL entity id to a PHYSICS traffic entity id via the global->physical index map.
    // Returns true and writes the packed physics id when the map slot is occupied; false when the
    // slot holds the 0x7F "no vehicle" sentinel.
    // -------------------------------------------------------------------------------------------
    bool VehicleManager::GetTrafficPhysicsEntityIDFromGlobalEntityID_Safe(u32 luGlobalEntityId,
                                                                          EntityId* lpOutPhysicsEntityId)
    {
        CGS_ASSERT(lpOutPhysicsEntityId != nullptr, "lpOutPhysicsEntityId != NULL");

        const u32 luGlobalIndex = (luGlobalEntityId >> KU_ENTITY_INDEX_SHIFT) & KU_ENTITY_INDEX_MASK;
        // RE-SEATED 2026-08-03: the map is the embedded traffic manager's own member
        // (VehicleManager +149456 == 44768 + 104688), not a sibling of VehicleManager. The assert
        // condition is the console's own text, and it can now be spelled against the real
        // `sizeof(map)` it names instead of against a hand-copied 600.
        CGS_ASSERT(luGlobalIndex < sizeof(mPhysicalTrafficManager.mu8GlobalToPhysicalEntityIndexMap),
                   "lGlobalEntityId.GetEntityIndex() < sizeof(mu8GlobalToPhysicalEntityIndexMap)");

        const unsigned char lu8PhysicalIndex =
            mPhysicalTrafficManager.mu8GlobalToPhysicalEntityIndexMap[luGlobalIndex];
        if (lu8PhysicalIndex == KU8_NO_PHYSICAL_VEHICLE)
            return false;

        // (asm fires a debug assert that the physical index is < 0x4000 here; debug-only, not reproduced.)
        lpOutPhysicsEntityId->muValue =
            (static_cast<u32>(lu8PhysicalIndex) << KU_ENTITY_INDEX_SHIFT) | KU_ENTITYTYPE_TRAFFIC_VEHICLE_BITS;
        return true;
    }

    // -------------------------------------------------------------------------------------------
    // GetVehiclePhysi  @0x825B4F50
    //
    // Resolve a packed physics-vehicle id to its physics body. The owner byte selects the source:
    //   RACECAR (1)        -> &maRaceCarVehicles[index]  (the RaceCarPhysics : VehiclePhysics record)
    //   TRAFFIC_VEHICLE(2) -> PhysicalTrafficManager::GetVehiclePhysics on the contained subobject.
    // The X360 returns a raw pointer.
    //
    // THE TWO BRANCH TYPES DO SHARE A BASE. `struct
    // VehiclePhysics : public SimpleVehiclePhysics` (VehiclePhysics.h), so RaceCarPhysics IS-A
    // SimpleVehiclePhysics and both branches return one. The DWARF types the console accessor
    // `SimpleVehiclePhysics* GetVehiclePhysics(EntityId)` (BrnVehicleManager.h:1299), and
    // DoCarCarContactGeneration relies on exactly that shared prefix -- it reads mbFrozen (+0x70)
    // and mLinearVelocity (+0x50) off either branch with no per-branch test.
    // The void* return is KEPT for now only because narrowing it changes this definition and its
    // declaration together. FIX-WHEN someone owns both files in one pass: narrow the return to
    // SimpleVehiclePhysics* and drop the two static_casts at the call site in
    // DoCarCarContactGeneration (BrnVehicleManagerContactGeneration.cpp).
    //
    // THE TRAFFIC BRANCH IS REAL AS OF 2026-08-03. The old note here read: "the contained
    // PhysicalTrafficManager subobject @ +44768 is modelled as opaque padding in this layout (it
    // cannot be embedded by its real type because the takedown TU already names
    // maRaceCarEntityIdRemap as a direct sibling at +148128, which falls inside the subobject's byte
    // range -- the two models are mutually exclusive)". The diagnosis was exactly right and the
    // resolution is the opposite of what it assumed: the sibling was the MISTAKE. +148128 IS a
    // member of the subobject (its maTrafficEntityIDs, 44768 + 103360), so the two models were never
    // in tension -- one of them was simply wrong. The manager is embedded by name now and this
    // branch delegates, as the X360 does.
    // -------------------------------------------------------------------------------------------
    void* VehicleManager::GetVehiclePhysi(EntityId lPhysicsVehicleId)
    {
        const u32 luOwner = (lPhysicsVehicleId.muValue >> 24) & 0xFFu;   // asm srwi r30, r31, 24
        if (luOwner != KU_ENTITY_OWNER_RACECAR)
        {
            CGS_ASSERT(luOwner == KU_ENTITY_OWNER_TRAFFIC_VEHICLE,
                       "lPhysicsVehicleId.GetOwner() == BrnWorld::E_ENTITYTYPE_RACECAR || lPhysicsVehicleId.GetOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");
        }

        // NON-RACECAR branch. Re-pulled from the asm this wave (@0x825B4F50, 32 instructions):
        //   0x825B4FB8  addis r3, r29, 1
        //   0x825B4FC0  addi  r3, r3, -0x5120        ; this + 44768 == &mPhysicalTrafficManager
        //   0x825B4FBC  mr    r4, r31                ; the packed id, verbatim (NOT the index)
        //   0x825B4FC4  bl    PhysicalTrafficManager::GetVehiclePhysics
        // Note the branch order: the assert above does not return, so an id whose owner is neither
        // 1 nor 2 falls through to HERE, not to the racecar branch. Reproduced exactly.
        if (luOwner != KU_ENTITY_OWNER_RACECAR)
        {
            return mPhysicalTrafficManager.GetVehiclePhysics(lPhysicsVehicleId);
        }

        // RACECAR branch (asm: extrwi index ; mulli 0x1460 ; add this ; addi 0x740
        //                     == 5216*index + this + 1856 == &maRaceCarVehicles[index]).
        const u32 luIndex = (lPhysicsVehicleId.muValue >> KU_ENTITY_INDEX_SHIFT) & KU_ENTITY_INDEX_MASK;
        return &maRaceCarVehicles[luIndex];
    }

    // -------------------------------------------------------------------------------------------
    // HasRaceCarHadRecentImpact  @0x825B4EB8
    //
    // Recency throttle: true while this car's post-impact cooldown is still running, i.e. while
    // mafNoImpactTimeSeconds[idx] > 0.0f (asm: load *(4*(idx+42921)+this) as a float, fcmpu > 0.0;
    // 4*(idx+42921) == 4*idx + 171684).
    // SIMPLIFIED 2026-08-03. This body used to read the slot as an s32 and memcpy it to a float,
    // because the member was modelled as `s32 maRaceCarLastAttacker[8]`. That workaround was the
    // symptom, not the fix: DWARF :925 names the array `mafNoImpactTimeSeconds` and types it
    // float32_t[8], and HandleRaceCarRaceCarContact seeds it with `lfsx`/`stfsx` from the f32
    // tuning constant mfMinSecondsBetweenImpacts. The slot was always a float; only the model was
    // wrong. Same bytes, one fewer reinterpretation.
    // -------------------------------------------------------------------------------------------
    bool VehicleManager::HasRaceCarHadRecentImpact(s32 liActiveRaceCarIndex)
    {
        CGS_ASSERT(liActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0, "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
        CGS_ASSERT(liActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

        return mafNoImpactTimeSeconds[liActiveRaceCarIndex] > 0.0f;
    }

    // -------------------------------------------------------------------------------------------
    // SetNetworkRaceCarHidden  @0x825C3040
    //
    // Mark a NETWORK race car hidden for at least liFrames frames: set its bit in the
    // mHiddenRaceCars bitset and store the frame count into mauNetworkCarHiddenFramesRemaining[index].
    // (asm: stdx the per-index bit into the 64-bit word @ this+44704; stwx liFrames @ 4*idx+44736.)
    // -------------------------------------------------------------------------------------------
    void VehicleManager::SetNetworkRaceCarHidden(EActiveRaceCarIndex leActiveRaceCarIndex, s32 liFrames)
    {
        const s32 liIndex = static_cast<s32>(leActiveRaceCarIndex);

        // (asm: assert maeRaceCarTypes[idx] == 2 -- the "this slot is a NETWORK race car" check;
        // the same per-car word the takedown TU names maeRaceCarTypes, value 2 == network/fatal.)
        CGS_ASSERT(maeRaceCarTypes[liIndex] == 2,
                   "maeRaceCarTypes[leActiveRaceCarIndex] == BrnWorld::E_RACE_CAR_TYPE_NETWORK");

        // (asm: an optional HIDE_ONLINE debug-log block gated on CgsDev::Message::gxMessageFilterFlags
        // & 1 -- log only, not reproduced.)

        // (asm: the CgsBitArray bounds assert that idx < 8 -- here folded into the BitArray::SetBit
        // bound check.)
        CGS_ASSERT(liIndex < 8, "Index < Number of bits");

        mHiddenRaceCars.SetBit(static_cast<u32>(liIndex));   // asm: 64-bit word @ +44704, set bit idx
        mauNetworkCarHiddenFramesRemaining[liIndex] = liFrames;                      // asm: stwx liFrames @ 4*idx + 44736
    }

    // -------------------------------------------------------------------------------------------
    // SetPlayerActiveRaceCarIndex  @0x8259C028
    //
    // Store the local player's active-race-car slot (gated 0..7).
    // -------------------------------------------------------------------------------------------
    void VehicleManager::SetPlayerActiveRaceCarIndex(EActiveRaceCarIndex lePlayerActiveRaceCarIndex)
    {
        const s32 liIndex = static_cast<s32>(lePlayerActiveRaceCarIndex);
        CGS_ASSERT(liIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT && liIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID,
                   "( lePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT ) && ( lePlayerActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID )");

        mePlayerActiveRaceCarIndex = lePlayerActiveRaceCarIndex;   // asm: stwx @ +172204
    }

    // -------------------------------------------------------------------------------------------
    // SetShowtimeBehaviour  @0x8259C098
    //
    // Store the current showtime behaviour mode (gated 0..2 against E_SHOWTIME_MODE_COUNT == 3).
    // -------------------------------------------------------------------------------------------
    void VehicleManager::SetShowtimeBehaviour(u32 luShowtimeBehaviour)
    {
        // asm: cmpwi >= 0 (always true for u32) && cmpwi < 3. The X360 gate is `> 2` -> assert.
        CGS_ASSERT(luShowtimeBehaviour < 3u,
                   "leShowtimeBehaviour >= 0 && leShowtimeBehaviour < BrnGameState::E_SHOWTIME_MODE_COUNT");

        meShowtimeBehaviour = luShowtimeBehaviour;   // asm: stwx @ +172456
    }

    // -------------------------------------------------------------------------------------------
    // SetPlayerCarToShowtimeMode  @0x8259C108
    //
    // Drive the player car into (or out of) showtime: forward the cached showtime strength
    // (+172320) and damage limit (+172324) to RaceCarPhysics::SetPlayerVehicleInShowtime on the player
    // car's record, then latch the global player-in-showtime byte.
    //
    // ASM ARG MAPPING: the call is __thiscall RaceCarPhysics::SetPlayerVehicleInShowtime(
    //   this = 5216*playerIdx + this + 1856 == &maRaceCarVehicles[playerIdx],
    //   r4   = lbInShowtime (the char arg a2),
    //   f1   = *(this+172320) == mfPlayerStatStrength,
    //   f2   = *(this+172324) == mfPlayerStatDamageLimit ).
    // The Hex-Rays `a5` parameter is a SIMD-spill artefact and is not a real argument.
    // -------------------------------------------------------------------------------------------
    void VehicleManager::SetPlayerCarToShowtimeMode(bool lbInShowtime)
    {
        const s32 liPlayerIndex = static_cast<s32>(mePlayerActiveRaceCarIndex);
        CGS_ASSERT(liPlayerIndex >= E_ACTIVE_RACE_CAR_INDEX_0, "mePlayerActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
        CGS_ASSERT(liPlayerIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "mePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

        RaceCarPhysics* const lpPlayerCar =
            &maRaceCarVehicles[liPlayerIndex];
        lpPlayerCar->SetPlayerVehicleInShowtime(lbInShowtime,
                                                mfPlayerStatStrength,
                                                mfPlayerStatDamageLimit);

        VehiclePhysics::msbInShowtime = lbInShowtime;   // asm: stb r29, byte_82FB7DF2
    }

    // -------------------------------------------------------------------------------------------
    // _AssertLayoutPlayerStats -- pin the wave-10 member offsets (never called).
    // -------------------------------------------------------------------------------------------
    void VehicleManager::_AssertLayoutPlayerStats()
    {
        // `offsetof(RaceCarVehicleRecord, meCarType) == 5084`
        // stood here. The record is gone -- `maRaceCarVehicles` is the real RaceCarPhysics -- and a
        // host class does not reproduce a console in-record seat, so the assert cannot be kept and
        // must not be re-based to whatever the host produces. The 0x13DC seat is asserted as console
        // arithmetic in the mounted VehiclePhysics_layout_check.cpp (KU_B_CARTYPE), which also names
        // the member in an existence check. What is still checkable HERE is that ApplyPlayerStats is
        // writing a member of the real class at all:
        static_assert(sizeof(decltype(VehicleManager::maRaceCarVehicles[0].meCarType)) == 4,
                      "VehiclePhysics::meCarType is the 4-byte `stw` seat ApplyPlayerStats writes "
                      "(asm 0x8259BFE8), not the f32 the retired record modelled");
        static_assert(offsetof(VehicleManager, mHiddenRaceCars)          == 44704 + KU_HOST_DRIFT_AFTER_MODEL_HANDLES,  "mHiddenRaceCars (asm +44704)");
        static_assert(offsetof(VehicleManager, mauNetworkCarHiddenFramesRemaining)               == 44736 + KU_HOST_DRIFT_AFTER_MODEL_HANDLES,  "mauNetworkCarHiddenFramesRemaining (asm base 44736)");
        // RE-SEATED 2026-08-03: the map is the embedded traffic manager's own member, not a
        // sibling of this class. The old assert claimed `offsetof(VehicleManager,
        // mau8GlobalToPhysicalEntityIndexMap) == 149456`; the X360 address is indeed 149456 ==
        // 44768 + 104688, but that in-manager 104688 does NOT reproduce on the host -- ResourceHandle
        // (16 vs 8, x20) and four 4->8 pointers and EventQueue<s8,50> (72 vs 64) all sit ahead of it
        // inside the manager. What still reproduces exactly is the manager's OWN seat, so that is
        // what is pinned; the map is reached BY NAME through it and needs no offset at all.
        static_assert(offsetof(VehicleManager, mPhysicalTrafficManager) == 44768 + KU_HOST_DRIFT_AFTER_MODEL_HANDLES,
                      "mPhysicalTrafficManager (asm PhysicalTrafficManager::Construct(this + 44768)); "
                      "the global->physical map lives inside it at X360 in-manager +104688");
        static_assert(sizeof(PhysicalTrafficManager::mu8GlobalToPhysicalEntityIndexMap) == 600,
                      "and it is 600 bytes -- the sizeof the console's own assert text names "
                      "(asm cmplwi 0x258)");
        static_assert(offsetof(VehicleManager, mfPlayerStatStrength)     == 172320 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "mfPlayerStatStrength (asm +172320)");
        static_assert(offsetof(VehicleManager, mfPlayerStatDamageLimit)  == 172324 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "mfPlayerStatDamageLimit (asm +172324)");
        static_assert(offsetof(VehicleManager, miCarSpeed)               == 172328 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "miCarSpeed (asm +172328)");
        static_assert(offsetof(VehicleManager, miCarStrength)            == 172332 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "miCarStrength (asm +172332)");
        static_assert(offsetof(VehicleManager, miCarControl)             == 172336 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "miCarControl (asm +172336)");
        static_assert(offsetof(VehicleManager, miCarBoost)               == 172340 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "miCarBoost (asm +172340)");
        static_assert(offsetof(VehicleManager, meCarType)                == 172344 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "meCarType (asm +172344)");
        static_assert(offsetof(VehicleManager, meShowtimeBehaviour)             == 172456 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "meShowtimeBehaviour (asm +172456)");
    }

    // -------------------------------------------------------------------------------------------
    // VehicleManager::VehicleManager (constructor)  @0x827E4D58  -- DEFERRED.
    //
    // FLAG: NOT bodied here. The constructor sets the per-car-record + manager vtable pointers
    // (off_820974E4 / off_820D0C68 / off_820D1034 / off_820CEE90 / off_820CF988 / off_820CF1D4) and
    // runs `vector constructor iterator` over the embedded collision-generator sub-arrays inside each
    // RaceCarDriverRecord / RaceCarVehicleRecord, then chains PhysicalTrafficManager's constructor on
    // the contained subobject @ +44768. The current VehicleManager layout models those records and the
    // contained manager as OPAQUE PADDING (no vtables, no real sub-objects), so a faithful constructor
    // body cannot be written against it without fabricating vtable symbols and placement-construction
    // over padding. It belongs to the full RaceCarPhysics / PhysicalTrafficManager layout pass that
    // replaces the padding stand-ins with the real polymorphic types. Bodying it now would violate the
    // "no fabricated types/symbols" rule, so it is intentionally left to that pass. (Boot-trace
    // executed only this constructor of the nine; the observable scalar flag stores it makes -- the
    // eight per-record +132 == 1 bytes, +172036 == 1, and the four +172364..+172376 zero-inits -- are
    // documented here and will be reproduced when the records become real typed members.)
    // -------------------------------------------------------------------------------------------

    // ===============================================================================================
    // ⭐ ADDED 2026-08-27 (showtime S3 wave) -- the five game-action leaves.
    //
    // These are the five VehicleManager methods PhysicsModule::HandleGameActions @0x825A72F0 calls
    // and that had no declaration anywhere in the tree. Together they are 32 X360 instructions. They
    // are grouped here rather than in a TU of their own because every one of them touches only the
    // player-index / mode / impact-time block this TU already owns.
    // ===============================================================================================

    // -------------------------------------------------------------------------------------------
    // SwitchPlayerAIDonuttingAttribs  @0x8262AEC8  (7 insns)
    //
    //   0x8262AEC8  lis  r11, 2 ; ori r11, r11, 0xA0AC   ; 0x2A0AC == +172204
    //   0x8262AED0  lwzx r11, r3, r11                    ; mePlayerActiveRaceCarIndex
    //   0x8262AED4  mulli r11, r11, 0x1460               ; sizeof(RaceCarPhysics) == 5216
    //   0x8262AED8  add  r11, r11, r3
    //   0x8262AEDC  addi r3,  r11, 0x740                 ; maRaceCarVehicles == this + 1856
    //   0x8262AEE0  b    VehiclePhysics::SwitchAIDonuttingAttribs
    //
    // The (0x1460, 0x740) pair is the SAME stride/base SetPlayerCarToShowtimeMode resolves above, so
    // the whole address computation collapses to one named subscript. RaceCarPhysics derives from
    // VehiclePhysics, so the tail call is an ordinary inherited call.
    //
    // ⚠️ NO BOUNDS CHECK AND NO ASSERT -- unlike SetPlayerCarToShowtimeMode, whose asserts ARE in
    // the console body, this function has neither. It is seven instructions and there is nowhere
    // for one to hide. Do not add one: an index of -1 here is a real producer failure and must stay
    // visible as one. [[invented-arms-and-the-c4715-ratchet]]
    // -------------------------------------------------------------------------------------------
    void VehicleManager::SwitchPlayerAIDonuttingAttribs(bool lbDonutting)
    {
        maRaceCarVehicles[static_cast<s32>(mePlayerActiveRaceCarIndex)]
            .SwitchAIDonuttingAttribs(lbDonutting);
    }

    // -------------------------------------------------------------------------------------------
    // OnGameModePrepare  @0x825B5708  (4 insns)
    //   `lis r11,2 ; ori r11,r11,0xA15C ; stwx r4,r3,r11 ; blr`   -- 0x2A15C == +172380.
    //
    // ⭐⭐ +172380 IS meCurrentGameModeType, THE SHOWTIME GATE. ProcessDeformationStates tests it
    // against 2 (E_MODE_OFFLINE_SHOWTIME) / 16 (E_MODE_ONLINE_SHOWTIME). Before this wave the member
    // was seeded to -1 by Construct and NEVER WRITTEN AGAIN by anything in the tree, so every
    // showtime-gated arm downstream of it was unreachable by construction rather than by state.
    // -------------------------------------------------------------------------------------------
    void VehicleManager::OnGameModePrepare(s32 leGameModeType)
    {
        meCurrentGameModeType = leGameModeType;   // asm: stwx @ +172380
    }

    // -------------------------------------------------------------------------------------------
    // OnGameModeStop  @0x825B5718  (5 insns)
    //   `li r10,-1 ; stwx r10,r3,0x2A15C ; blr`  -- meCurrentGameModeType = -1 (E_MODE_NONE).
    //
    // ⚠️ The call site (HandleGameActions cases 37/39) does `lwz r4, 0(r29)` immediately before the
    // branch. That is a DEAD STORE into the argument register -- this body never reads r4, and the
    // console prototype takes no second parameter. Do not add one to "match the call site".
    // -------------------------------------------------------------------------------------------
    void VehicleManager::OnGameModeStop()
    {
        meCurrentGameModeType = -1;   // asm: li r10,-1 ; stwx @ +172380
    }

    // -------------------------------------------------------------------------------------------
    // StartImpactTime  @0x825B5730  (8 insns)
    //   `stbx 1  -> +0x2A110 (172304 mbImpactTime)`
    //   `stbx r5 -> +0x2A11A (172314 mbAftertouchIsForceAdditive)`
    //
    // ⚠️⚠️ THE FLOAT PARAMETER IS REAL AND THE BODY IGNORES IT. Call site @0x825A76A0:
    //     addi r3, r31, 0x4AA0      ; the manager
    //     lbz  r5, 4(r29)           ; the bool -- IN r5, NOT r4
    //     lfs  f1, 0(r29)           ; the float
    // The bool landing in r5 is the whole proof: on this ABI a float argument consumes its
    // positional GPR slot, so a `(this, bool)` signature would have put it in r4. Hex-Rays renders
    // the call as `StartImpactTime(a1 + 19104, *_R29)` -- one argument, and typed as the float's
    // memory. [[reconstruction-gotchas]] Hex-Rays drops arguments.
    // The console accepts the duration and drops it; that is reproduced, not repaired.
    // -------------------------------------------------------------------------------------------
    void VehicleManager::StartImpactTime(f32 lfImpactTimeDuration, bool lbForceAdditiveAftertouch)
    {
        (void)lfImpactTimeDuration;   // f1 is passed and never read by the console body

        mbImpactTime                 = true;                          // asm: stbx 1  @ +172304
        mbAftertouchIsForceAdditive  = lbForceAdditiveAftertouch;      // asm: stbx r5 @ +172314
    }

    // -------------------------------------------------------------------------------------------
    // EndImpactTime  @0x825B5750  (8 insns) -- both bytes back to zero.
    // -------------------------------------------------------------------------------------------
    void VehicleManager::EndImpactTime()
    {
        mbImpactTime                = false;   // asm: stbx 0 @ +172304
        mbAftertouchIsForceAdditive = false;   // asm: stbx 0 @ +172314
    }
}
}
