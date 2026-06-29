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
    // The global "player car is currently in showtime" latch (X360 byte_82FB7DF2). A standalone module
    // byte the gui/camera layer reads; SetPlayerCarToShowtimeMode stores the requested flag verbatim.
    // FLAG: name proposed by role; the X360 home is a bare .data byte (byte_82FB7DF2). Modelled as a
    // file-static here -- fold into its real owning TU if one is recovered.
    static bool gbPlayerCarInShowtime = false;

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
    //   [0] -> maPlayerCarStats[0]              (asm v3[43082] == class +172328)
    //   [1] -> maPlayerCarStats[1]              (asm v3[43083] == +172332); ALSO, treated as an INTEGER,
    //          sign-extended and * 0.1f -> mfShowtimePlayerCarStrength (asm v3[43080] == +172320)
    //   [2] -> maPlayerCarStats[2]              (asm v3[43084] == +172336)
    //   [3] -> maPlayerCarStats[3]              (asm v3[43085] == +172340)
    //   [4] -> mfShowtimePlayerCarDamageLimit   (asm v3[43081] == +172324)
    //   [5] -> maPlayerCarStats[4]              (asm v3[43086] == +172344); ALSO into the player car's
    //          record at in-record +5084 (asm stw r10, 0x1B1C(5216*playerIdx + this)).
    // -------------------------------------------------------------------------------------------
    void VehicleManager::ApplyPlayerStats(const f32* lpSendCarStatsAction)
    {
        CGS_ASSERT(lpSendCarStatsAction != nullptr, "lpSendCarStatsAction");

        const s32 liPlayerIndex = static_cast<s32>(mePlayerActiveRaceCarIndex);
        CGS_ASSERT(liPlayerIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT && liPlayerIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID,
                   "( mePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT ) && ( mePlayerActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID )");

        // The raw stats block (asm writes [0],[1],[2],[3],[5] into the five-float block).
        maPlayerCarStats[0] = lpSendCarStatsAction[0];
        maPlayerCarStats[1] = lpSendCarStatsAction[1];
        maPlayerCarStats[2] = lpSendCarStatsAction[2];
        maPlayerCarStats[3] = lpSendCarStatsAction[3];
        maPlayerCarStats[4] = lpSendCarStatsAction[5];

        // The player car's per-record copy of stat [5] (asm: stw into 5216*playerIdx + in-record +5084).
        maRaceCarVehicles[liPlayerIndex].mfPlayerBoostStrengthStat = lpSendCarStatsAction[5];

        // The showtime strength: stat [1] taken as a signed INTEGER, converted to float, * 0.1f
        // (asm extsw -> fcfid -> frsp -> fmuls flt_82004014). The bit pattern of lpSendCarStatsAction[1]
        // is reinterpreted as an s32 before the integer->float convert.
        s32 liStatOneAsInt;
        const f32 lfStatOne = lpSendCarStatsAction[1];
        std::memcpy(&liStatOneAsInt, &lfStatOne, sizeof(liStatOneAsInt));
        mfShowtimePlayerCarStrength = static_cast<f32>(liStatOneAsInt) * KF_STAT_STRENGTH_SCALE;

        // The showtime damage limit: stat [4] verbatim.
        mfShowtimePlayerCarDamageLimit = lpSendCarStatsAction[4];
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
        CGS_ASSERT(luGlobalIndex < KU_MAX_GLOBAL_TRAFFIC_ENTITY_INDEX,
                   "lGlobalEntityId.GetEntityIndex() < sizeof(mu8GlobalToPhysicalEntityIndexMap)");

        const unsigned char lu8PhysicalIndex = mau8GlobalToPhysicalEntityIndexMap[luGlobalIndex];
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
    // The X360 returns a raw pointer; the two branch types share no common base, so this returns void*.
    //
    // FLAG (traffic branch DEFERRED): the contained PhysicalTrafficManager subobject @ +44768 is
    // modelled as opaque padding in this layout (it cannot be embedded by its real type because the
    // takedown TU already names maRaceCarEntityIdRemap as a direct sibling at +148128, which falls
    // inside the subobject's byte range -- the two models are mutually exclusive). Reaching the
    // contained manager's GetVehiclePhysics by NAME therefore requires the full embedded-manager layout
    // pass. Until then, only the RACECAR branch (the boot-relevant DoCarCarContactGeneration path) is
    // bodied; the TRAFFIC branch asserts + falls through to the racecar-record formula, which is the
    // X360's structural shape (5216*index + this + 1856) -- it does NOT delegate to the traffic manager
    // here. This is a KNOWN GAP, not a fabricated body.
    // -------------------------------------------------------------------------------------------
    void* VehicleManager::GetVehiclePhysi(EntityId lPhysicsVehicleId)
    {
        const u32 luOwner = (lPhysicsVehicleId.muValue >> 24) & 0xFFu;   // asm srwi r30, r31, 24
        if (luOwner != KU_ENTITY_OWNER_RACECAR)
        {
            CGS_ASSERT(luOwner == KU_ENTITY_OWNER_TRAFFIC_VEHICLE,
                       "lPhysicsVehicleId.GetOwner() == BrnWorld::E_ENTITYTYPE_RACECAR || lPhysicsVehicleId.GetOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");
        }

        const u32 luIndex = (lPhysicsVehicleId.muValue >> KU_ENTITY_INDEX_SHIFT) & KU_ENTITY_INDEX_MASK;
        // RACECAR branch (asm: 5216*index + this + 1856 == &maRaceCarVehicles[index]).
        // FLAG: the TRAFFIC branch should delegate to the contained PhysicalTrafficManager
        // (PhysicalTrafficManager::GetVehiclePhysics) -- DEFERRED (see header note); we return the
        // racecar-record address for both owners as the structurally-faithful stand-in.
        return &maRaceCarVehicles[luIndex];
    }

    // -------------------------------------------------------------------------------------------
    // HasRaceCarHadRecentImpact  @0x825B4EB8
    //
    // Recency throttle: true when the per-car "last attacker / recent-impact" slot (read as a float)
    // is > 0 (asm: load *(4*(idx+42921)+this) as a float, fcmpu > 0.0). The slot is maRaceCarLastAttacker
    // @ +171684 (4*(idx+42921) == 4*idx + 171684); the X360 reinterprets that word as a float here.
    // -------------------------------------------------------------------------------------------
    bool VehicleManager::HasRaceCarHadRecentImpact(s32 liActiveRaceCarIndex)
    {
        CGS_ASSERT(liActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0, "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
        CGS_ASSERT(liActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

        // The X360 loads the maRaceCarLastAttacker word as a float and tests > 0.0.
        f32 lfRecentImpact;
        const s32 liSlotWord = maRaceCarLastAttacker[liActiveRaceCarIndex];
        std::memcpy(&lfRecentImpact, &liSlotWord, sizeof(lfRecentImpact));
        return lfRecentImpact > 0.0f;
    }

    // -------------------------------------------------------------------------------------------
    // SetNetworkRaceCarHidden  @0x825C3040
    //
    // Mark a NETWORK race car hidden for at least liFrames frames: set its bit in the
    // mHiddenNetworkRaceCars bitset and store the frame count into maHiddenForFrames[index].
    // (asm: stdx the per-index bit into the 64-bit word @ this+44704; stwx liFrames @ 4*idx+44736.)
    // -------------------------------------------------------------------------------------------
    void VehicleManager::SetNetworkRaceCarHidden(EActiveRaceCarIndex leActiveRaceCarIndex, s32 liFrames)
    {
        const s32 liIndex = static_cast<s32>(leActiveRaceCarIndex);

        // (asm: assert maRaceCarCrashState[idx] == 2 -- the "this slot is a NETWORK race car" check;
        // the same per-car word the takedown TU names maRaceCarCrashState, value 2 == network/fatal.)
        CGS_ASSERT(maRaceCarCrashState[liIndex] == 2,
                   "maeRaceCarTypes[leActiveRaceCarIndex] == BrnWorld::E_RACE_CAR_TYPE_NETWORK");

        // (asm: an optional HIDE_ONLINE debug-log block gated on CgsDev::Message::gxMessageFilterFlags
        // & 1 -- log only, not reproduced.)

        // (asm: the CgsBitArray bounds assert that idx < 8 -- here folded into the BitArray::SetBit
        // bound check.)
        CGS_ASSERT(liIndex < 8, "Index < Number of bits");

        mHiddenNetworkRaceCars.SetBit(static_cast<u32>(liIndex));   // asm: 64-bit word @ +44704, set bit idx
        maHiddenForFrames[liIndex] = liFrames;                      // asm: stwx liFrames @ 4*idx + 44736
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

        muShowtimeBehaviour = luShowtimeBehaviour;   // asm: stwx @ +172456
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
    //   f1   = *(this+172320) == mfShowtimePlayerCarStrength,
    //   f2   = *(this+172324) == mfShowtimePlayerCarDamageLimit ).
    // The Hex-Rays `a5` parameter is a SIMD-spill artefact and is not a real argument.
    // -------------------------------------------------------------------------------------------
    void VehicleManager::SetPlayerCarToShowtimeMode(bool lbInShowtime)
    {
        const s32 liPlayerIndex = static_cast<s32>(mePlayerActiveRaceCarIndex);
        CGS_ASSERT(liPlayerIndex >= E_ACTIVE_RACE_CAR_INDEX_0, "mePlayerActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
        CGS_ASSERT(liPlayerIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "mePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

        RaceCarPhysics* const lpPlayerCar =
            reinterpret_cast<RaceCarPhysics*>(&maRaceCarVehicles[liPlayerIndex]);
        lpPlayerCar->SetPlayerVehicleInShowtime(lbInShowtime,
                                                mfShowtimePlayerCarStrength,
                                                mfShowtimePlayerCarDamageLimit);

        gbPlayerCarInShowtime = lbInShowtime;   // asm: stb r29, byte_82FB7DF2
    }

    // -------------------------------------------------------------------------------------------
    // _AssertLayoutPlayerStats -- pin the wave-10 member offsets (never called).
    // -------------------------------------------------------------------------------------------
    void VehicleManager::_AssertLayoutPlayerStats()
    {
        static_assert(offsetof(RaceCarVehicleRecord, mfPlayerBoostStrengthStat) == 5084, "in-record player boost stat (asm: 5216*idx + 6940 -> in-record +5084)");
        static_assert(offsetof(VehicleManager, mHiddenNetworkRaceCars)          == 44704,  "mHiddenNetworkRaceCars (asm +44704)");
        static_assert(offsetof(VehicleManager, maHiddenForFrames)               == 44736,  "maHiddenForFrames (asm base 44736)");
        static_assert(offsetof(VehicleManager, mau8GlobalToPhysicalEntityIndexMap) == 149456, "global->physical map (asm +149456)");
        static_assert(offsetof(VehicleManager, mfShowtimePlayerCarStrength)     == 172320, "mfShowtimePlayerCarStrength (asm +172320)");
        static_assert(offsetof(VehicleManager, mfShowtimePlayerCarDamageLimit)  == 172324, "mfShowtimePlayerCarDamageLimit (asm +172324)");
        static_assert(offsetof(VehicleManager, maPlayerCarStats)                == 172328, "maPlayerCarStats (asm base 172328)");
        static_assert(offsetof(VehicleManager, muShowtimeBehaviour)             == 172456, "muShowtimeBehaviour (asm +172456)");
    }

    // -------------------------------------------------------------------------------------------
    // VehicleManager::VehicleManager (constructor)  @0x827E4D58  -- DEFERRED.
    //
    // FLAG: NOT bodied here. The constructor sets the per-car-record + manager vtable pointers
    // (off_820974E4 / off_820D0C68 / off_820D1034 / off_820CEE90 / off_820CF988 / off_820CF1D4) and
    // runs `vector constructor iterator` over the embedded collision-generator sub-arrays inside each
    // RaceCarStatusRecord / RaceCarVehicleRecord, then chains PhysicalTrafficManager's constructor on
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
}
}
