// ChallengeListEntry.h
// Header-only data-list slice. Two POD resource structs live in this TU:
// BrnResource::ChallengeListEntryAction (the per-action record, 0x50 bytes) and
// BrnResource::ChallengeListEntry (the challenge record whose first member is
// maAction[KI_MAX_ACTIONS_PER_CHALLENGE]). A growing set of trivial field accessors is
// reconstructed inline at the bottom of this header (GetAction const/non-const,
// GetActionType, GetChallengeStyle, GetNumActions, GetNumTargets, GetTargetDataType,
// GetCombineAction); every other method is still declared-only. Member layout
// (names + offsets) is taken from the DECFIGS DWARF.
//
// IMPORTANT layout note: GetAction @ 0x8230F0F8 computes `this + 80 * index`, which
// PROVES sizeof(ChallengeListEntryAction) == 80 (0x50) and maAction @ offset 0 in
// ChallengeListEntry.

#pragma once

#include "types.hpp"                                   // u8/s8/u32/s32/f32 widths
#include "BrnCommonTypes.h"                             // CgsID (typedef u64)
#include "GameShared/GameClasses/Core/CgsAssert.h"      // CGS_ASSERT

namespace BrnResource
{

// --- ChallengeListEntryAction ------------------------------------------------
// Per-action descriptor. sizeof == 0x50 (80), align 8 (CgsID is u64). Declared in
// full so ChallengeListEntry::maAction has the correct 80-byte element stride.
struct ChallengeListEntryAction
{
    // -- ChallengeListEntry.h:47/48 --
    static const int32_t KI_MAX_TARGETS_PER_CHALLENGE_ACTION = 2;
    static const uint8_t KU_MAX_LOCATIONS_PER_ACTION         = 4;

    // -- modifier bit flags (ChallengeListEntry.h:127-132) --
    static const uint8_t KX_MODIFIER_NONE             = 0;
    static const uint8_t KX_MODIFIER_WITHOUT_CRASHING = 1;
    static const uint8_t KX_MODIFIER_PRISTINE         = 2;
    static const uint8_t KX_MODIFIER_HEAD_ON          = 4;
    static const uint8_t KX_MODIFIER_IN_AIR           = 8;
    static const uint8_t KX_MODIFIER_BANK_FOR_SUCCESS = 16;

    // -- ChallengeListEntry.h:50 --
    enum ELocationType
    {
        E_LOCATION_TYPE_ANYWHERE       = 0,
        E_LOCATION_TYPE_DISTRICT       = 1,
        E_LOCATION_TYPE_TRIGGER        = 2,
        E_LOCATION_TYPE_ROAD           = 3,
        E_LOCATION_TYPE_ROAD_NO_MARKER = 4,
        E_LOCATION_TYPE_COUNT          = 5,
    };

    // -- ChallengeListEntry.h:61 --
    enum EChallengeActionType
    {
        E_CHALLENGE_ACTION_MINIMUM_SPEED         = 0,
        E_CHALLENGE_ACTION_IN_AIR                = 1,
        E_CHALLENGE_ACTION_AIR_DISTANCE          = 2,
        E_CHALLENGE_ACTION_LEAP_CARS             = 3,
        E_CHALLENGE_ACTION_DRIFT                 = 4,
        E_CHALLENGE_ACTION_NEAR_MISS             = 5,
        E_CHALLENGE_ACTION_BARREL_ROLLS          = 6,
        E_CHALLENGE_ACTION_ONCOMING              = 7,
        E_CHALLENGE_ACTION_FLATSPIN              = 8,
        E_CHALLENGE_ACTION_LAND_SUCCESSFUL       = 9,
        E_CHALLENGE_ACTION_ROAD_RULE_TIME        = 10,
        E_CHALLENGE_ACTION_ROAD_RULE_CRASH       = 11,
        E_CHALLENGE_ACTION_PLAYER_POWER_PARKING  = 12,
        E_CHALLENGE_ACTION_TRAFFIC_POWER_PARKING = 13,
        E_CHALLENGE_ACTION_CRASH_INTO_PLAYER     = 14,
        E_CHALLENGE_ACTION_BURNOUTS              = 15,
        E_CHALLENGE_ACTION_MEET_UP               = 16,
        E_CHALLENGE_ACTION_BILLBOARD             = 17,
        E_CHALLENGE_ACTION_BOOST_TIME            = 18,
        E_CHALLENGE_ACTION_BARREL_ROLLS_REVERSE  = 19,
        E_CHALLENGE_ACTION_FLATSPIN_REVERSE      = 20,
        E_CHALLENGE_ACTION_LAND_SUCCESSFUL_REVERSE = 21,
        E_ACTION_COUNT                           = 22,
    };

    // -- ChallengeListEntry.h:90 --
    enum EChallengeDataType
    {
        E_CHALLENGE_DATA_TYPE_CRASHES             = 0,
        E_CHALLENGE_DATA_TYPE_NEAR_MISS           = 1,
        E_CHALLENGE_DATA_TYPE_ONCOMING            = 2,
        E_CHALLENGE_DATA_TYPE_DRIFT               = 3,
        E_CHALLENGE_DATA_TYPE_AIR                 = 4,
        E_CHALLENGE_DATA_TYPE_AIR_DISTANCE        = 5,
        E_CHALLENGE_DATA_TYPE_BARREL_ROLLS        = 6,
        E_CHALLENGE_DATA_TYPE_FLAT_SPINS          = 7,
        E_CHALLENGE_DATA_TYPE_CARS_LEAPT          = 8,
        E_CHALLENGE_DATA_TYPE_SPEED_ROAD_RULE     = 9,
        E_CHALLENGE_DATA_TYPE_CRASH_ROAD_RULE     = 10,
        E_CHALLENGE_DATA_TYPE_SUCCESSFUL_LANDINGS = 11,
        E_CHALLENGE_DATA_TYPE_BURNOUTS            = 12,
        E_CHALLENGE_DATA_TYPE_POWER_PARKS         = 13,
        E_CHALLENGE_DATA_TYPE_PERCENTAGE          = 14,
        E_CHALLENGE_DATA_TYPE_MEET_UP             = 15,
        E_CHALLENGE_DATA_TYPE_BILLBOARDS          = 16,
        E_CHALLENGE_DATA_TYPE_BOOST_TIME          = 17,
        E_CHALLENGE_DATA_TYPE_COUNT               = 18,
    };

    // -- ChallengeListEntry.h:114 --
    enum EChallengeCoopType
    {
        E_CHALLENGE_COOP_TYPE_ONCE                    = 0,
        E_CHALLENGE_COOP_TYPE_INDIVIDUAL              = 1,
        E_CHALLENGE_COOP_TYPE_INDIVIDUAL_ACCUMULATION = 2,
        E_CHALLENGE_COOP_TYPE_SIMULTANEOUS            = 3,
        E_CHALLENGE_COOP_TYPE_CUMULATIVE              = 4,
        E_CHALLENGE_COOP_TYPE_AVERAGE                 = 5,
        E_CHALLENGE_COOP_TYPE_COUNT                   = 6,
    };

    // -- ChallengeListEntry.h:134 --
    enum ECombineActionType
    {
        E_COMBINE_ACTION_CHAIN                   = 0,
        E_COMBINE_ACTION_FAILURE_RESETS_CHAIN    = 1,
        E_COMBINE_ACTION_FAILURE_RESETS_EVERYONE = 2,
        E_COMBINE_ACTION_SIMULTANEOUS            = 3,
        E_COMBINE_ACTION_INDEPENDENT             = 4,
        E_COMBINE_ACTION_COUNT                   = 5,
    };

    // -- ChallengeListEntry.h:146: per-location payload (8 bytes, union over the id) --
    union LocationData
    {
        int32_t meDistrict;     // real type BrnWorld::EDistrict; modeled as s32 to avoid
                                // forking an uncommitted enum (matches BrnCheckpointData.h)
        CgsID   mTriggerID;     // ChallengeListEntry.h:148
        CgsID   mRoadID;        // ChallengeListEntry.h:149
    };

    // ---- methods (declared-only unless marked RECONSTRUCTED; the reconstructed ones are
    //      defined inline at the bottom of this header) ----
    void                  Construct();                            // :153
    void                  FixUp(void* lpBase);                    // :156
    void                  FixDown(void* lpBase);                  // :159
    EChallengeActionType  GetActionType() const;                  // :162  RECONSTRUCTED (inline below)
    bool                  HasTimeLimit() const;                   // :165
    f32                   GetTimeLimit() const;                   // :169
    bool                  HasConvoyTime() const;                  // :172
    f32                   GetConvoyTime() const;                  // :175
    int32_t               GetTargetValue(int32_t liIndex) const;  // :179
    EChallengeDataType    GetTargetDataType(int32_t liIndex) const; // :183  RECONSTRUCTED (inline below)
    CgsID                 GetCgsIDTarget(int32_t liIndex) const;  // :187
    int32_t               GetNumTargets() const;                  // :190  RECONSTRUCTED (inline below)
    ECombineActionType    GetCombineAction() const;               // :193  RECONSTRUCTED (inline below)
    EChallengeCoopType    GetCoopType() const;                    // :196
    uint8_t               GetModifier() const;                    // :199
    uint8_t               GetNumLocations() const;                // :202
    ELocationType         GetLocationType(uint8_t lu8Index) const; // :206
    int32_t               GetDistrict(uint8_t lu8Index) const;    // :210 (real ret BrnWorld::EDistrict)
    CgsID                 GetTriggerID(uint8_t lu8Index) const;   // :214  RECONSTRUCTED (inline below)
    CgsID                 GetRoadID(uint8_t lu8Index) const;      // :218
    void                  SetActionType(EChallengeActionType leType);          // :223
    void                  SetTimeLimit(f32 lfTime);                            // :226
    void                  SetConvoyTime(f32 lfTime);                           // :230
    void                  SetNumTargets(int32_t liNum);                        // :234
    void                  SetTargetValue(int32_t liIndex, int32_t liValue);    // :239
    void                  SetTargetDataType(int32_t liIndex, EChallengeDataType leType); // :244
    void                  SetCgsIDTarget(int32_t liIndex, CgsID lID);          // :249
    void                  SetCombineAction(ECombineActionType leType);         // :253
    void                  SetCoopType(EChallengeCoopType leType);              // :257
    void                  SetModifier(uint8_t lu8Modifier);                    // :261
    void                  SetNumLocations(uint8_t lu8Num);                     // :270
    void                  SetLocationType(uint8_t lu8Index, ELocationType leType); // :275
    void                  SetDistrict(uint8_t lu8Index, int32_t leDistrict);   // :280 (real BrnWorld::EDistrict)
    void                  SetTriggerID(uint8_t lu8Index, CgsID lID);           // :285
    void                  SetRoadID(uint8_t lu8Index, CgsID lID, bool lbAddMarker); // :291

private:
    // ---- on-disk layout (DWARF offsets); total sizeof == 0x50 (80) ----
    uint8_t      muActionType;          // :294  @0x00
    uint8_t      muCoopType;            // :295  @0x01
    uint8_t      mxModifier;            // :296  @0x02
    uint8_t      muCombineActionType;   // :297  @0x03
    uint8_t      muNumLocations;        // :299  @0x04
    uint8_t      mauLocationType[4];    // :300  @0x05..0x08
    // (7 bytes pad to align LocationData[] @0x10 -- CgsID/u64 forces 8-byte align)
    LocationData maLocationData[4];     // :301  @0x10..0x2F
    uint8_t      muNumTargets;          // :303  @0x30
    // (3 bytes pad to align maiTargetValue @0x34)
    int32_t      maiTargetValue[2];     // :304  @0x34..0x3B
    uint8_t      mau8TargetDataType[2]; // :305  @0x3C..0x3D
    // (2 bytes pad to align mfTimeLimit @0x40)
    f32          mfTimeLimit;           // :307  @0x40
    f32          mfConvoyTime;          // :308  @0x44
    uint32_t     muPropType;            // :310  @0x48
    // (4 bytes tail pad to round sizeof to 0x50 / 8-byte align)
};

// --- ChallengeListEntry ------------------------------------------------------
// The challenge record. maAction is the FIRST member (offset 0): GetAction returns
// `this + 80*index`, confirming maAction @ +0 with 80-byte stride.
struct ChallengeListEntry
{
    static const int32_t KI_CHALLENGE_VERSION              = 1;   // :327
    static const int32_t KI_MAX_ACTIONS_PER_CHALLENGE      = 2;   // :328
    static const int32_t KI_MAX_CHALLENGE_STRING_ID_LENGTH = 16;  // :329
    // [challenge-list wave 2026-08-27] ADDITIVE GROW (FLAG -- name from the baked assert
    // text, value from the binary). SetNumPlayers' second guard, inlined into
    // ChallengeList::AddListResource @0x8267B598, fires "liNumPlayers <= KI_MAX_PLAYERS"
    // (ChallengeListEntry.h:876) when the count exceeds EIGHT (`cmpwi r31, 8 ; ble`). The
    // constant is not in the PS3 DWARF for this class, so it is named here from the assert
    // string and sized from that compare; 8 is also the engine-wide player cap
    // (BrnGameActions.h KI_MAX_PLAYERS == BrnWorld::KI_MAX_ACTIVE_RACE_CARS).
    static const int32_t KI_MAX_PLAYERS                    = 8;

    // -- ChallengeListEntry.h:331 --
    enum ECarRestrictionType
    {
        E_CAR_TYPE_NONE       = 0,
        E_CAR_TYPE_DANGER     = 1,
        E_CAR_TYPE_AGGRESSION = 2,
        E_CAR_TYPE_STUNT      = 3,
        E_CAR_TYPE_COUNT      = 4,
    };

    // -- ChallengeListEntry.h:341 --
    enum EFreeburnChallengeStyle
    {
        E_FREEBURN_STYLE_NONE             = 0,
        E_FREEBURN_STYLE_NORMAL           = 1,
        E_FREEBURN_STYLE_ROAD_RULES_TIME  = 2,
        E_FREEBURN_STYLE_ROAD_RULES_CRASH = 3,
        E_FREEBURN_STYLE_COUNT            = 4,
    };

    // -- ChallengeListEntry.h:351 --
    enum EChallengeDifficulty
    {
        E_CHALLENGE_DIFFICULTY_EASY      = 0,
        E_CHALLENGE_DIFFICULTY_MEDIUM    = 1,
        E_CHALLENGE_DIFFICULTY_HARD      = 2,
        E_CHALLENGE_DIFFICULTY_VERY_HARD = 3,
        E_CHALLENGE_DIFFICULTY_COUNT     = 4,
    };

    // ---- methods ----
    void                          Construct();                       // :362  declared-only
    void                          FixUp(void* lpBase);               // :365  declared-only
    void                          FixDown(void* lpBase);             // :368  declared-only
    CgsID                         GetChallengeID() const;            // :372  RECONSTRUCTED (inline below)
    // NOTE (do NOT re-add `GetChall`): the ledger/IDB identity
    // `BrnResource::ChallengeListEntry::GetChall` is the IDA-TRUNCATED form of
    // GetChallengeStyle -- the IDB clips qualified names at 41 characters
    // ("BrnResource::ChallengeListEntry::GetChall" is exactly 41, and the same clip produces
    // "BrnGameState::ChallengeManager::GetChalle" / "BrnGui::ChallengeListComponent::GetChalle").
    // It is ONE binary function at 0x823542A0, reconstructed inline at the bottom of this header
    // as GetChallengeStyle; the DecFIGS DWARF for this class has no `GetChall` at any line, and
    // the DWARF's own BrnFriendsList.cpp call sites name GetChallengeStyle. A separate
    // `s32 GetChall() const` declaration therefore can never gain a definition (unresolved
    // external that `cl /c` cannot see) and would be an ODR hazard. Callers wanting the 32-bit
    // event word use `static_cast<s32>(x->GetChallengeStyle())`.
    int32_t                       GetNumPlayers() const;             // :375  declared-only
    int32_t                       GetOriginalNumPlayers() const;     // :378  declared-only
    int32_t                       GetNumActions() const;             // :381  RECONSTRUCTED (inline below)
    const ChallengeListEntryAction* GetAction(int32_t liActionIndex) const; // :384  RECONSTRUCTED (inline below, const twin)
    EChallengeDifficulty          GetDifficulty() const;             // :387  declared-only
    const char*                   GetDescriptionStringID() const;    // :390  declared-only
    const char*                   GetTitleStringID() const;          // :393  RECONSTRUCTED (inline below)
    // X360 0x823542A0. The IDB/ledger identity for this same address is the 41-char-truncated
    // name `BrnResource::ChallengeListEntry::GetChall` -- see the note above; there is no
    // separate `GetChall` method.
    EFreeburnChallengeStyle       GetChallengeStyle() const;         // :396  RECONSTRUCTED (inline below)
    ECarRestrictionType           GetCarType() const;                // :399  declared-only
    CgsID                         GetCarID() const;                  // :402  declared-only
    void                          SetChallengeID(CgsID lID);         // :409  declared-only
    void                          SetNumPlayers(int32_t liNum);      // :412  declared-only
    void                          SetNewNumPlayers(int32_t liNum);   // :415  declared-only
    void                          ResetNumPlayers();                 // :418  declared-only
    void                          SetNumActions(int32_t liNum);      // :421  declared-only
    ChallengeListEntryAction*     GetAction(int32_t liActionIndex);  // :424  RECONSTRUCTED (inline below)
    void                          SetDifficulty(EChallengeDifficulty leDifficulty); // :427 declared-only
    void                          SetDescriptionStringID(const char* lpcID);        // :431 declared-only
    void                          SetTitleStringID(const char* lpcID);              // :435 declared-only
    void                          SetCarType(ECarRestrictionType leType);           // :438 declared-only
    void                          SetCarID(CgsID lID);                              // :441 declared-only
    void                          SetCarColours(int32_t liColour, int32_t liPalette); // :444 declared-only
    int32_t                       GetCarColourIndex();               // :447  declared-only
    int32_t                       GetCarColourPaletteIndex();        // :450  declared-only

private:
    // ---- on-disk layout (DWARF offsets) ----
    ChallengeListEntryAction maAction[KI_MAX_ACTIONS_PER_CHALLENGE]; // :453  @0x00 (2 * 0x50 = 0xA0)
    char                     macDescriptionStringID[KI_MAX_CHALLENGE_STRING_ID_LENGTH]; // :455 @0xA0
    char                     macTitleStringID[KI_MAX_CHALLENGE_STRING_ID_LENGTH];       // :456 @0xB0
    CgsID                    mChallengeID;       // :458  @0xC0
    CgsID                    mCarID;             // :459  @0xC8
    uint8_t                  muCarType;          // :460  @0xD0
    int8_t                   miCarColourIndex;   // :461  @0xD1
    int8_t                   miCarColourPaletteIndex; // :462 @0xD2
    uint8_t                  muNumPlayers;       // :463  @0xD3
    uint8_t                  muNumActions;       // :464  @0xD4
    uint8_t                  muDifficulty;       // :465  @0xD5

    // ADDITIVE GROW (FLAG -- X360-attested, DWARF-gap): the PS3 DecFIGS DWARF ends the
    // record at muDifficulty (@0xD5); the X360 ARTIST binary reads a byte at @0xD6 in
    // ChallengeList::IsChallengeContentBought @0x8267BC08 (`lbz 0xD6`) and branches on
    // values 1 / 2 to select which downloadable-content entitlement gates the challenge
    // (0 / other == always available). It occupies tail space already inside the
    // 0xD8 sizeof (216, proven by GetChallengeData's 216-byte stride), so adding it does
    // not change the record size. No prior TU read @0xD6, so the grow is safe.
public:
    enum EContentBoughtType
    {
        E_CONTENT_BOUGHT_NONE  = 0,   // no DLC requirement (always available)
        E_CONTENT_BOUGHT_DLC_A = 1,   // gated by the first content entitlement
        E_CONTENT_BOUGHT_DLC_B = 2,   // gated by the second content entitlement
    };

    // The X360 reads this byte raw; the accessor exposes it for ChallengeList.
    uint8_t GetContentBoughtType() const { return muContentBoughtType; }

private:
    uint8_t                  muContentBoughtType; // X360 @0xD6 (DWARF-gap; see note above)
};

// ChallengeListEntryAction::GetActionType @ 0x8230EDC8
// (DWARF: EChallengeActionType GetActionType() const, ChallengeListEntry.h:162; baked
// assert at ChallengeListEntry.h:660.) X360 reads muActionType (the byte at offset 0),
// range-guards it via CGS_ASSERT and returns it cast to EChallengeActionType. The guard
// is non-fatal (the binary returns the raw byte even when it fails).
//
// X360/DWARF DRIFT (binary authoritative): the X360 fires the assert when the type byte
// is >= 0x29 (41), whereas the committed/PS3-DWARF EChallengeActionType has
// E_ACTION_COUNT == 22. We do NOT redefine the committed enum (same call recorded for
// GetChallengeStyle below); the condition is written against the named constant for
// source fidelity (the byte-exact X360 threshold is 0x29).
inline ChallengeListEntryAction::EChallengeActionType
ChallengeListEntryAction::GetActionType() const
{
    CGS_ASSERT(
        muActionType < E_ACTION_COUNT,
        "(ChallengeListEntryAction::EChallengeActionType)muActionType < ChallengeListEntryAction::E_ACTION_COUNT" );

    return static_cast<EChallengeActionType>( muActionType );
}

// ChallengeListEntry::GetAction @ 0x8230F0F8
// X360 pseudocode: `return 80 * a2 + a1;` with a1==this, a2==liActionIndex. Since
// sizeof(ChallengeListEntryAction)==0x50 and maAction is at offset 0, this is simply
// `&maAction[liActionIndex]`. Two range asserts guard the index via the project
// CGS_ASSERT macro (the X360-baked file/line are discarded per project convention).
// The asserts are non-fatal (binary continues and returns the pointer even on an
// out-of-range index).
inline ChallengeListEntryAction* ChallengeListEntry::GetAction( int32_t liActionIndex )
{
    CGS_ASSERT( liActionIndex >= 0, "liActionIndex >= 0" );
    CGS_ASSERT( liActionIndex < KI_MAX_ACTIONS_PER_CHALLENGE, "liActionIndex < KI_MAX_ACTIONS_PER_CHALLENGE" );

    return &maAction[ liActionIndex ];
}

// ChallengeListEntry::GetChallengeStyle @ 0x823542A0
// (DWARF: EFreeburnChallengeStyle GetChallengeStyle() const, ChallengeListEntry.h:396)
// LEDGER IDENTITY: this address is recorded as `BrnResource::ChallengeListEntry::GetChall`
// (IDA truncates qualified names at 41 chars). THIS inline IS that ledger function's
// reconstruction -- do not body `GetChall` separately.
// X360 reads the byte at offset 0 of `this` (== maAction[0].muActionType, the first action's
// type) and maps ROAD_RULE_TIME/ROAD_RULE_CRASH to the matching freeburn styles. Reconstructed
// via the named accessor maAction[0].GetActionType() rather than a raw byte read. The range
// guard is the project CGS_ASSERT (non-fatal; binary continues even on out-of-range type).
//
// X360/DWARF DRIFT (binary authoritative): the X360 fires the assert when the type byte is
// >= 0x29 (41), whereas the committed/PS3-DWARF EChallengeActionType has E_ACTION_COUNT == 22.
// We do NOT redefine the committed enum; the condition is written against the named constant
// for source fidelity (the byte-exact X360 threshold is 0x29).
inline ChallengeListEntry::EFreeburnChallengeStyle
ChallengeListEntry::GetChallengeStyle() const
{
    const ChallengeListEntryAction::EChallengeActionType leActionType =
        maAction[ 0 ].GetActionType();

    CGS_ASSERT(
        leActionType < ChallengeListEntryAction::E_ACTION_COUNT,
        "(ChallengeListEntryAction::EChallengeActionType)muActionType < ChallengeListEntryAction::E_ACTION_COUNT" );

    if ( leActionType == ChallengeListEntryAction::E_CHALLENGE_ACTION_ROAD_RULE_TIME )
    {
        return E_FREEBURN_STYLE_ROAD_RULES_TIME;
    }
    if ( leActionType == ChallengeListEntryAction::E_CHALLENGE_ACTION_ROAD_RULE_CRASH )
    {
        return E_FREEBURN_STYLE_ROAD_RULES_CRASH;
    }
    return E_FREEBURN_STYLE_NORMAL;
}

// -----------------------------------------------------------------------------
// Trivial field accessors the X360 ALWAYS inlines into its callers (so they have
// no standalone address of their own). Reconstructed here as the named calls the
// inlined reads correspond to, recovered from the FreeburnChallengeManager
// StartChallenge @0x82509D60 / TriggerChallenge @0x8250A160 call sites where they
// appear as raw byte loads at the proven member offsets:
//   GetNumActions     -> lbz this+0xD4 (muNumActions)               -- loop bound
//   GetAction (const) -> this + 80*index (== &maAction[index])      -- guards 941/942
//   GetNumTargets     -> lbz action+0x30 (muNumTargets)             -- used as count
//   GetTargetDataType -> lbz action+0x3C (mau8TargetDataType[idx])  -- target list build
//   GetCombineAction  -> lbz action+0x03 (muCombineActionType)      -- branch selector
// These are plain getters; only GetAction carries guards (its two index asserts are
// attested verbatim in the StartChallenge/TriggerChallenge asm, ChallengeListEntry.h
// lines 941/942, and mirror the already-reconstructed non-const GetAction twin).
// -----------------------------------------------------------------------------

inline int32_t ChallengeListEntry::GetNumActions() const
{
    return muNumActions;
}

// [gateui] ChallengeListEntry::GetChallengeID -- X360-inlined, no ledger row. Recovered
// from ChallengeList::GetChallengeIndex @0x82326168, whose scan body is
//     0x82326194  bl   BrnResource__ChallengeList__GetChallengeData
//     0x82326198  ld   r11, 0xC0(r3)        <-- the whole body: the 8-byte CgsID at +0xC0
//     0x8232619C  cmpld cr6, r11, r29
// +0xC0 is mChallengeID (DWARF ChallengeListEntry.h:458). An 8-byte `ld`, so this is the
// CgsID member itself, not a truncated word.
inline CgsID ChallengeListEntry::GetChallengeID() const
{
    return mChallengeID;
}

// [gateui] ChallengeListEntry::GetTitleStringID -- another accessor the X360 ALWAYS
// inlines (it has no ledger row of its own). Recovered from the ONE call site the HUD
// path uses, BrnGui::HudMessageAnalyzer::TriggerChallengeTriggeredMessage @0x8251F970:
//     0x8251FA5C  bl   BrnGui__FreeburnChallengeManager__GetCurrentChallenge
//     0x8251FA60  mr   r11, r3
//     0x8251FA70  addi r6, r11, 0xB0            <-- the whole body: `entry + 0xB0`
//     0x8251FA74  bl   BrnGui__GuiHudMessage__AddParam   (type 6 == STRINGID, string 1)
// +0xB0 is macTitleStringID (DWARF ChallengeListEntry.h:456), the 16-byte char array
// right after macDescriptionStringID -- so the accessor returns the embedded array, not a
// stored pointer, and there is nothing to null-check.
inline const char* ChallengeListEntry::GetTitleStringID() const
{
    return macTitleStringID;
}

// [gateui] the twin, identical shape over macDescriptionStringID (+0xA0). A caller arrived
// 2026-08-27: RaceMainHudState::StartFreeburnChallengeTicker @0x8247ABC0 inlines it as
// `addi r21, r20, 0xA0`.
inline const char* ChallengeListEntry::GetDescriptionStringID() const
{
    return macDescriptionStringID;
}

// [gateui] the per-challenge player count. X360 always inlines it: StartFreeburnChallengeTicker
// @0x8247AAC0/0x8247AAC4 does `lbz r11, 0xD3(r20) ; clrlwi r6, r11, 28` == muNumPlayers & 0xF,
// the same read BrnChallengeManager_wB_03.cpp:118 and _wC_01.cpp:259 record. Replaces the
// BrnFriendsListLinkGates.cpp return-0 gate (deleted in the same change -- its "unreachable
// today" premise died when the ticker mounted).
inline int32_t ChallengeListEntry::GetNumPlayers() const
{
    return muNumPlayers & 0xF;
}

// [challenge-list wave 2026-08-27] The other half of the muNumPlayers byte. X360-inlined;
// recovered from ChallengeList::AddListResource @0x8267B598, whose post-load pass reads the
// LOW nibble, range-checks it and stores `17 * n` back -- i.e. the byte is a PAIR of nibbles
// {original << 4 | current}, and 17*n writes the same value into both. MEASURED against the
// shipped build/game/ONLINECHALLENGES.BNDL: every one of its 458 records already carries a
// matched pair (0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 -- nothing else), so the store is
// idempotent on retail data, exactly as a "publish the authored count into both nibbles"
// pass should be.
inline int32_t ChallengeListEntry::GetOriginalNumPlayers() const
{
    return ( muNumPlayers >> 4 ) & 0xF;
}

// ChallengeListEntry::SetNumPlayers (DWARF :412) -- X360-inlined into
// ChallengeList::AddListResource @0x8267B598, verbatim:
//     v23 = *(entry + 0xD3) & 0xF;
//     if (v23 == 0)  FireAssert("liNumPlayers >= 1",            ChallengeListEntry.h, 874);
//     if (v23 > 8)   FireAssert("liNumPlayers <= KI_MAX_PLAYERS", ChallengeListEntry.h, 876);
//     ...
//     *(entry + 0xD3) = 17 * v23;
// Both guards are the console's own, in the console's order, and both are non-fatal
// (log-and-continue) exactly as CGS_ASSERT is. The 17 is `(n << 4) | n` -- see
// GetOriginalNumPlayers above.
inline void ChallengeListEntry::SetNumPlayers( int32_t liNum )
{
    CGS_ASSERT( liNum >= 1, "liNumPlayers >= 1" );
    CGS_ASSERT( liNum <= KI_MAX_PLAYERS, "liNumPlayers <= KI_MAX_PLAYERS" );

    muNumPlayers = static_cast<uint8_t>( 17 * liNum );
}

// ChallengeListEntryAction::SetCombineAction (DWARF :253) -- X360-inlined into
// ChallengeList::AddListResource's post-load pass, which is the ONLY writer of the byte in
// the whole image (`*(80 * actionIndex + entry + 3) = <value>`). +0x03 is
// muCombineActionType; the write carries no guard of its own on the console (the guards at
// the call site belong to GetAction's index range).
inline void ChallengeListEntryAction::SetCombineAction( ECombineActionType leType )
{
    muCombineActionType = static_cast<uint8_t>( leType );
}

inline const ChallengeListEntryAction* ChallengeListEntry::GetAction( int32_t liActionIndex ) const
{
    CGS_ASSERT( liActionIndex >= 0, "liActionIndex >= 0" );
    CGS_ASSERT( liActionIndex < KI_MAX_ACTIONS_PER_CHALLENGE, "liActionIndex < KI_MAX_ACTIONS_PER_CHALLENGE" );

    return &maAction[ liActionIndex ];
}

inline int32_t ChallengeListEntryAction::GetNumTargets() const
{
    return muNumTargets;
}

inline ChallengeListEntryAction::EChallengeDataType
ChallengeListEntryAction::GetTargetDataType( int32_t liIndex ) const
{
    return static_cast<EChallengeDataType>( mau8TargetDataType[ liIndex ] );
}

inline ChallengeListEntryAction::ECombineActionType
ChallengeListEntryAction::GetCombineAction() const
{
    return static_cast<ECombineActionType>( muCombineActionType );
}

// ChallengeListEntryAction::GetTriggerID @ 0x8230EFF8
// (DWARF: CgsID GetTriggerID(uint8_t lu8Index) const, ChallengeListEntry.h:214.)
// X360 reads maLocationData[lu8Index].mTriggerID -- the 8-byte CgsID (ldx). Two non-fatal
// guards: index range, and that the location's type byte (mauLocationType[lu8Index]) is
// E_LOCATION_TYPE_TRIGGER. Assert file/line (h:793/794) discarded per project convention.
// Sibling of GetDistrict (type==DISTRICT, 4-byte) and GetRoadID (type==ROAD, 8-byte).
inline CgsID ChallengeListEntryAction::GetTriggerID( uint8_t lu8Index ) const
{
    CGS_ASSERT( lu8Index < KU_MAX_LOCATIONS_PER_ACTION,
                "luLocationIndex < KU_MAX_LOCATIONS_PER_ACTION" );
    CGS_ASSERT( (ELocationType) mauLocationType[ lu8Index ] == E_LOCATION_TYPE_TRIGGER,
                "(ELocationType) mauLocationType[luLocationIndex] == E_LOCATION_TYPE_TRIGGER" );

    return maLocationData[ lu8Index ].mTriggerID;
}

} // namespace BrnResource
