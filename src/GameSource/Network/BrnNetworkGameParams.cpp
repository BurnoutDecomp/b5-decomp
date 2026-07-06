#include "GameSource/Network/BrnNetworkGameParams.h"
#include "GameSource/Network/Parameters/BrnNetworkParameterData.h"   // AmendGameModeContexts / MatchmakingContext
#include "GameShared/GameClasses/Core/CgsAssert.h"                   // CGS_ASSERT (range/round-trip tripwires)

#include <cstring>   // std::memcpy

// =============================================================================
// BrnNetwork::GameParams -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnNetworkGameParams.h for the
// corrected base (CgsNetwork::ServerInterfaceGameParamsX360) + the opaque leaf
// layout (maBlock150 @ +0x150, seven 0xA0-stride maPlayerSlots @ +0x170).
//
// This TU SHIPS:
//   ~GameParams  (the `scalar deleting destructor' @ 0x82567338 forwards to it)
//   operator=    @ 0x82566A00
// =============================================================================

namespace BrnNetwork
{

// X360 @ 0x82567338 (`scalar deleting destructor'). As the object tears down,
// the Xenon codegen reinstalls the shared ServerInterfaceStructureInterface base
// vtable (off_8207C88C) into each of the seven embedded 0xA0-stride sub-objects
// (this+0x170 .. this+0x530) and then the primary vtable at this+0, before the
// delete-expression's conditional operator delete. That whole vtable walk + the
// conditional free are compiler-synthesised from this trivial out-of-line virtual
// destructor; only the empty body is hand-written, matching the committed
// convention (BrnNetwork::GameResults::~GameResults @ 0x827DFB60). Defining it
// out-of-line here also anchors this class's vtable to this TU.
GameParams::~GameParams()
{
}

// X360 @ 0x82566A00. Member-wise copy: chain to the X360 base operator=
// (0x82558DB0), then copy the game leaf's own storage in the exact order and
// byte ranges the Xenon codegen emits -- the 0x20-byte block at +0x150 followed
// by the seven 0xA0-stride sub-object records at +0x170, each copied over its
// full live span [+4 .. +0xA0) (only the +0 vtable slot is left untouched, as an
// operator= must not copy vtables). The leaf field layout is not attested, so
// (as in BrnNetwork::GameResults) the records are reached by raw byte offset
// from `this`/`other` rather than by fabricated member names.
GameParams& GameParams::operator=(const GameParams& lrOther)
{
    CgsNetwork::ServerInterfaceGameParamsX360::operator=(lrOther);

    u8* lpDst = reinterpret_cast<u8*>(this);
    const u8* lpSrc = reinterpret_cast<const u8*>(&lrOther);

    // +0x150: 8-word (32-byte) block, copied word-wise (mtctr 8).
    std::memcpy(lpDst + 0x150, lpSrc + 0x150, 8 * sizeof(u32));

    // +0x170: seven 0xA0-byte sub-object records. Each copies its full live span
    // [+4 .. +0xA0): a 0x40-byte byte run, seven words, a 0x24-byte run, then
    // seven more words -- contiguous, leaving only the leading +0 vtable slot.
    for (s32 liSlot = 0; liSlot < KI_GAMEPARAMS_PLAYER_SLOTS; ++liSlot)
    {
        const s32 liBase = 0x170 + liSlot * KI_GAMEPARAMS_PLAYER_SLOT_STRIDE;
        std::memcpy(lpDst + liBase + 0x04, lpSrc + liBase + 0x04, 0x40);            // [+0x04,+0x44)
        std::memcpy(lpDst + liBase + 0x44, lpSrc + liBase + 0x44, 7 * sizeof(u32)); // [+0x44,+0x60)
        std::memcpy(lpDst + liBase + 0x60, lpSrc + liBase + 0x60, 0x24);            // [+0x60,+0x84)
        std::memcpy(lpDst + liBase + 0x84, lpSrc + liBase + 0x84, 7 * sizeof(u32)); // [+0x84,+0xA0)
    }

    return *this;
}

// =============================================================================
// Game-surface packed-flag accessors (wave58). The two bit-packed dwords have no
// attested inner field layout, so they are reached by raw byte offset off `this`
// (search-data word @ +0xE8 == inherited muCustomFlags; leaf game-params word @
// +0x15C == maBlock150+0xC), matching operator='s / GameResults' convention. Each
// base-bit/num-bit placement is X360-attested from the extrwi/insrwi asm operands.
// =============================================================================

// X360 @ 0x825840C8. Boost-type discriminant, bits 24..26 of the game-search word
// at +0xE8. The Xenon codegen reads only the TOP byte (lbz @+0xE8) and masks the
// low 3 bits (clrlwi ,29), reproduced verbatim as lpBytes[0xE8] & 7.
s32 GameParams::BoostType() const
{
    const u8* lpBytes = reinterpret_cast<const u8*>(this);
    return lpBytes[0xE8] & 7;   // KU_BRN_GAMESEARCHDATA_BOOST_TYPE (base bit 24, 3 bits)
}

// X360 @ 0x82584098. Game-mode discriminant: bits 8..12 of the game-search word,
// biased by +10 (the E_GAME_MODE_* enum base).
s32 GameParams::GameMode() const
{
    const u8* lpBase = reinterpret_cast<const u8*>(this);
    u32 luFlags;
    std::memcpy(&luFlags, lpBase + 0xE8, sizeof(luFlags));
    return static_cast<s32>((luFlags >> 8) & 0x1F) + 10;
}

// X360 @ 0x82583DB0. Replicated-payload accessor: hands back the 0x20-byte leaf data
// block at +0x150 (== maBlock150). The base declares GetData() and GetData() const as
// SEPARATE pure-virtual slots, so both must be defined; the const overload shares this
// trivial body.
void* GameParams::GetData()
{
    return maBlock150;
}

const void* GameParams::GetData() const
{
    return maBlock150;
}

// X360 @ 0x82583DA8. Size of the replicated payload GetData() returns.
u32 GameParams::GetDataSize() const
{
    return sizeof(maBlock150);   // 0x20
}

// X360 @ 0x82584D28. This leaf's serialisation pattern string: "l*" == one long field.
const char* GameParams::GetPattern() const
{
    return "l*";
}

// X360 @ 0x82584058. lwz r11,0x15C(r3); extrwi r3,r11,1,24 -> (word >> 7) & 1.
bool GameParams::InfiniteBoost() const
{
    const u32 luGameParamsFlags =
        *reinterpret_cast<const u32*>(reinterpret_cast<const u8*>(this) + 0x15C);
    return (luGameParamsFlags >> 7) & 1;
}

// X360 @ 0x825840B8. lwz r11,0xE8(r3); extrwi r3,r11,1,8 -> (word >> 23) & 1.
bool GameParams::IsTrafficCheckingOn() const
{
    const u32 luGameSearchFlags =
        *reinterpret_cast<const u32*>(reinterpret_cast<const u8*>(this) + 0xE8);
    return (luGameSearchFlags >> 23) & 1;
}

// X360 @ 0x825840A8. lwz r11,0xE8(r3); extrwi r3,r11,1,14 -> (word >> 17) & 1.
bool GameParams::IsTrafficOn() const
{
    const u32 luGameSearchFlags =
        *reinterpret_cast<const u32*>(reinterpret_cast<const u8*>(this) + 0xE8);
    return (luGameSearchFlags >> 17) & 1;
}

// X360 @ 0x825840E8. Runner-crash count packed into muCustomFlags bits 27..29.
//   lwz r11,0xE8(r3); extrwi r3,r11,3,2  ->  (muCustomFlags >> 27) & 7
s32 GameParams::NumRunnerCrashes() const
{
    return static_cast<s32>((muCustomFlags >> KU_BRN_GAMESEARCHDATA_RUNNER_CRASHES_BASE_BIT)
                            & ((1u << KU_BRN_GAMESEARCHDATA_RUNNER_CRASHES_NUM_BITS) - 1u));
}

// X360 @ 0x82584078. Rounds count packed into the leaf game-params word @ +0x15C, bits 11..14.
s32 GameParams::NumberRounds() const
{
    const u32 luFlags = *reinterpret_cast<const u32*>(
        reinterpret_cast<const u8*>(this) + KI_GAMEPARAMS_LEAF_FLAGS_OFFSET);
    return static_cast<s32>((luFlags >> KI_GAME_PARAMS_ROUNDS_BASE_BIT)
                            & ((1u << KI_GAME_PARAMS_ROUNDS_NUM_BITS) - 1u));
}

// X360 @ 0x82584088. Security level = low 2 bits of muCustomFlags.
//   lwz r11,0xE8(r3); clrlwi r3,r11,30  ->  muCustomFlags & 3
s32 GameParams::Security() const
{
    return static_cast<s32>(muCustomFlags
                            & ((1u << KU_BRN_GAMESEARCHDATA_SECURITY_NUM_BITS) - 1u));
}

// X360 @ 0x82584018. Pack the 3-bit boost-type field (base bit 24) into the game-search
// word at +0xE8 (insrwi r11,r4,3,5 -> mask 0x07000000).
void GameParams::SetBoostType(s32 liBoostType)
{
    u32* lpuWord = reinterpret_cast<u32*>(reinterpret_cast<u8*>(this) + 0xE8);
    *lpuWord = ((static_cast<u32>(liBoostType) << 24) & 0x07000000u) | (*lpuWord & 0xF8FFFFFFu);
}

// X360 @ 0x8258A790. Pack the 5-bit game-mode field (base bit 8, stored biased by -10)
// into the game-search word at +0xE8, then (re)publish the game-mode LIVE context via
// AmendGameModeContexts over the base's context array (maRankedContexts @ +0xF8, count
// @ +0x148). AmendGameModeContexts is handed the RAW game-mode value as lpParams. Then
// assert the field round-trips.
void GameParams::SetGameMode(s32 liGameMode)
{
    u32* lpuWord = reinterpret_cast<u32*>(reinterpret_cast<u8*>(this) + 0xE8);
    *lpuWord = ((static_cast<u32>(liGameMode - 10) << 8) & 0x1F00u) | (*lpuWord & 0xFFFFE0FFu);

    AmendGameModeContexts(reinterpret_cast<void*>(static_cast<intptr_t>(liGameMode)),
                          &miContextCount,
                          reinterpret_cast<MatchmakingContext*>(maRankedContexts));

    CGS_ASSERT(GameMode() == liGameMode, "GameMode() == leGameMode");
}

// X360 @ 0x82583EA8. Pack the 1-bit infinite-boost flag (base bit 7) into the leaf
// game-params word at +0x15C (byte +0xC of maBlock150).
void GameParams::SetInfiniteBoost(bool lbInfiniteBoost)
{
    u32* lpuWord = reinterpret_cast<u32*>(reinterpret_cast<u8*>(this) + 0x15C);
    *lpuWord = ((static_cast<u32>(lbInfiniteBoost) << 7) & 0x80u) | (*lpuWord & 0xFFFFFF7Fu);
}

// X360 @ 0x82584038. Pack the runner-crash count into muCustomFlags bits 27..29
// (insrwi r11,r4,3,2 -> mask 0x38000000).
void GameParams::SetNumRunnerCrashes(s32 liNumRunnerCrashes)
{
    muCustomFlags = ((static_cast<u32>(liNumRunnerCrashes) << 27) & 0x38000000u)
                  | (muCustomFlags & 0xC7FFFFFFu);
}

// X360 @ 0x82583F48. Pack the round count into the leaf game-params word bits 11..14
// (insrwi r11,r30,4,17 -> mask 0x7800). Two range asserts.
void GameParams::SetNumberRounds(s32 liNumRounds)
{
    CGS_ASSERT(liNumRounds > 0, "liNumRounds > 0");
    CGS_ASSERT(liNumRounds < 16, "liNumRounds < (1<<KI_GAME_PARAMS_ROUNDS_NUM_BITS)");

    u32& lruGameParamFlags = GameParamFlags();
    lruGameParamFlags = ((static_cast<u32>(liNumRounds) << 11) & 0x7800u)
                      | (lruGameParamFlags & 0xFFFF87FFu);
}

// X360 @ 0x82583FD0. Pack (game-mode - 10) into muCustomFlags bits 18..22
// (insrwi r11,(r4-10),5,9).
void GameParams::SetPreviousGameMode(s32 liGameMode)
{
    muCustomFlags = ((static_cast<u32>(liGameMode - 10) << 18) & 0x7C0000u)
                  | (muCustomFlags & 0xFF83FFFFu);
}

// X360 @ 0x82583E30. Pack the vehicle level limit into the leaf game-params word bits 3..6
// (insrwi r11,r30,4,25 == (8 * liLevelLimit) & 0x78). Two range asserts.
void GameParams::SetRagerVehicleLevelLimit(s32 liLevelLimit)
{
    CGS_ASSERT(liLevelLimit >= 0, "liLevelLimit >= 0");
    CGS_ASSERT(liLevelLimit < 16, "liLevelLimit < (1<<KI_GAME_PARAMS_VEHICLE_LEVEL_NUM_BITS)");

    u32& lruGameParamFlags = GameParamFlags();
    lruGameParamFlags = ((static_cast<u32>(liLevelLimit) << 3) & 0x78u)
                      | (lruGameParamFlags & 0xFFFFFF87u);
}

// X360 @ 0x82584028. Store bit 30 (vehicle-choice) of the game-search word at +0xE8.
void GameParams::SetVehicleChoice(s32 liVehicleChoice)
{
    const u32 luMask = ((1u << KU_BRN_GAMESEARCHDATA_VEHICLE_CHOICE_NUM_BITS) - 1)
                       << KU_BRN_GAMESEARCHDATA_VEHICLE_CHOICE_BASE_BIT;
    u32& lruWord = SearchDataWord();
    lruWord = ((static_cast<u32>(liVehicleChoice) << KU_BRN_GAMESEARCHDATA_VEHICLE_CHOICE_BASE_BIT) & luMask)
              | (lruWord & ~luMask);
}

// X360 @ 0x82583DB8. Range-check the vehicle-level limit, then store it in the 4-bit
// VEC_LEVEL field (bits 13..16) of the game-search word at +0xE8.
void GameParams::SetVehicleLevelLimit(s32 liLevelLimit)
{
    CGS_ASSERT(liLevelLimit >= 0, "liLevelLimit >= 0");
    CGS_ASSERT(liLevelLimit < (1 << KU_BRN_GAMESEARCHDATA_VEC_LEVEL_NUM_BITS),
               "liLevelLimit < (1<<KU_BRN_GAMESEARCHDATA_VEC_LEVEL_NUM_BITS)");

    const u32 luMask = ((1u << KU_BRN_GAMESEARCHDATA_VEC_LEVEL_NUM_BITS) - 1)
                       << KU_BRN_GAMESEARCHDATA_VEC_LEVEL_BASE_BIT;
    u32& lruWord = SearchDataWord();
    lruWord = ((static_cast<u32>(liLevelLimit) << KU_BRN_GAMESEARCHDATA_VEC_LEVEL_BASE_BIT) & luMask)
              | (lruWord & ~luMask);
}

// X360 @ 0x82584068. Read the 5-bit TIME_LIMIT field (bits 15..19) of the leaf
// game-params word at +0x15C.
s32 GameParams::TimeLimit() const
{
    const u32 luMask = (1u << KI_GAME_PARAMS_TIME_LIMIT_NUM_BITS) - 1;
    return static_cast<s32>((GameParamsWord() >> KI_GAME_PARAMS_TIME_LIMIT_BASE_BIT) & luMask);
}

// X360 @ 0x825840D8. Vehicle-choice bit (bit 30) of muCustomFlags (+0xE8).
s32 GameParams::VehicleChoice() const
{
    return (muCustomFlags >> 30) & 0x1;
}

// X360 @ 0x82584048. 4-bit vehicle-level-limit field (bits 13..16) of muCustomFlags (+0xE8).
s32 GameParams::VehicleLevelLimit() const
{
    return (muCustomFlags >> 13) & 0xF;
}

} // namespace BrnNetwork
