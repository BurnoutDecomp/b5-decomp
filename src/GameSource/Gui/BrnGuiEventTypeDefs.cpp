// BrnGuiEventTypeDefs.cpp
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The four range-checked accessors on
// BrnGui::GuiEventUpdateSatNav::SatNavIconInfo. Each reads a single byte off `this`,
// runs two non-fatal CGS_ASSERT range guards (the X360 binary returns the raw byte
// even when a guard fails), and returns the value cast to its enum type.
//
//   GetCounty             @ 0x823A6A20  byte @0x24 (zero-extended)  guards: >=0, < E_COUNTY_COUNT(6)
//   GetDistrict           @ 0x823A6AA8  byte @0x25 (zero-extended)  guards: >=0, < E_DISTRICT_COUNT(0x13)
//   GetActiveRaceCarIndex @ 0x824B2EF8  byte @0x26 (sign-extended)  guards: >= INVALID(-1), < COUNT(8)
//   GetPlayerTeam         @ 0x824EB190  byte @0x27 (sign-extended)  guards: >= START(0), < COUNT(9)
//
// The X360-baked assert file/line are discarded per project convention; the stringized
// condition matches the X360 assert message text.

#include "GameSource/Gui/BrnGuiEventTypeDefs.h"

#include <cstring>  // std::memcpy -- the DoWorstCase compaction is a 0x30-byte block move.

namespace BrnGui
{

// File-scope sat-nav "worst case" animation phase (X360 flt_82FB508C). A persistent
// scalar advanced each DoWorstCase call: clamped-ramp up by 1.5 while <= 100, else reset
// to 0. Used to wobble the synthesised icon-ring X coordinate. The X360 stored this in a
// module global; modelled here as the equivalent translation-unit-local scalar.
static f32 sfWorstCasePhase = 0.0f;

// X360 immediates baked into DoWorstCase (named for clarity; values from the asm float
// pool): the phase ramp limit, the per-call phase step, and the icon-ring spacing/offset.
static const f32 KF_WORST_CASE_PHASE_LIMIT = 100.0f; // flt_820049E0
static const f32 KF_WORST_CASE_PHASE_STEP  = 1.5f;   // flt_82004D04
static const f32 KF_WORST_CASE_PHASE_RESET = 0.0f;   // flt_82001CC0
static const f32 KF_WORST_CASE_RING_STEP   = 50.0f;  // flt_820138DC (ring Z spacing & X offset)

// @ 0x823A6A20 — zero-extended byte; guards leCounty >= 0 (vacuous for an unsigned
// byte but the X360 still emits it) and leCounty < BrnWorld::E_COUNTY_COUNT.
BrnWorld::ECounty GuiEventUpdateSatNav::SatNavIconInfo::GetCounty() const
{
    const u8 luCounty = mu8County;
    CGS_ASSERT( luCounty >= 0, "leCounty >= 0" );
    CGS_ASSERT( luCounty < BrnWorld::E_COUNTY_COUNT, "leCounty < BrnWorld::E_COUNTY_COUNT" );
    return static_cast<BrnWorld::ECounty>( luCounty );
}

// @ 0x823A6AA8 — zero-extended byte; guards leDistrict >= 0 and
// leDistrict < BrnWorld::E_DISTRICT_COUNT (0x13).
BrnWorld::EDistrict GuiEventUpdateSatNav::SatNavIconInfo::GetDistrict() const
{
    const u8 luDistrict = mu8District;
    CGS_ASSERT( luDistrict >= 0, "leDistrict >= 0" );
    CGS_ASSERT( luDistrict < BrnWorld::E_DISTRICT_COUNT, "leDistrict < BrnWorld::E_DISTRICT_COUNT" );
    return static_cast<BrnWorld::EDistrict>( luDistrict );
}

// @ 0x824B2EF8 — sign-extended byte; guards leActiveRaceCarIndex >= INVALID(-1) and
// < E_ACTIVE_RACE_CAR_INDEX_COUNT(8).
EActiveRaceCarIndex GuiEventUpdateSatNav::SatNavIconInfo::GetActiveRaceCarIndex() const
{
    const s8 liActiveRaceCarIndex = mi8ActiveRaceCarIndex;
    CGS_ASSERT( liActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_INVALID,
                "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_INVALID" );
    CGS_ASSERT( liActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT" );
    return static_cast<EActiveRaceCarIndex>( liActiveRaceCarIndex );
}

// @ 0x824EB190 (IDA "GetPlay") — sign-extended byte; guards lePlayerTeam >= START(0)
// and < GsmIO::E_PLAYER_TEAM_COUNT(9).
EPlayerTeam GuiEventUpdateSatNav::SatNavIconInfo::GetPlayerTeam() const
{
    const s8 liPlayerTeam = mi8PlayerTeam;
    CGS_ASSERT( liPlayerTeam >= E_PLAYER_TEAM_START, "lePlayerTeam >= GsmIO::E_PLAYER_TEAM_START" );
    CGS_ASSERT( liPlayerTeam < E_PLAYER_TEAM_COUNT, "lePlayerTeam < GsmIO::E_PLAYER_TEAM_COUNT" );
    return static_cast<EPlayerTeam>( liPlayerTeam );
}

// ---------------------------------------------------------------------------
// GuiEventUpdateSatNav::SatNavI @ 0x823A6B30
//
//   lbz r11,0x28(r3) ; extsb r31,r11           -> v = (s8)maIconInfo[0].mi8IconType
//   assert v >= 0      ("leIconType >= 0",      BrnGuiEventTypeDefs.h:1911)
//   assert v < 14      ("leIconType < E_SATNAVICON_MAX", :1912)
//   return v
//
// Sign-extends and range-checks the leading icon's icon-type byte (offset +0x28),
// returning it. The X360 guards are non-fatal (the raw byte is returned regardless).
// ---------------------------------------------------------------------------
s32 GuiEventUpdateSatNav::SatNavI(const GuiEventUpdateSatNav* lpThis)
{
    const s32 liIconType = lpThis->maIconInfo[0].mi8IconType; // lbz;extsb -> sign-extended
    CGS_ASSERT( liIconType >= 0, "leIconType >= 0" );
    CGS_ASSERT( liIconType < SatNavIconInfo::E_SATNAVICON_MAX, "leIconType < E_SATNAVICON_MAX" );
    return liIconType;
}

// ---------------------------------------------------------------------------
// GuiEventUpdateSatNav::DoWorstCase @ 0x823B1980
//
// Three phases (store order / offsets per the X360 asm; see BrnGuiEventTypeDefs.h
// outer-struct grow banner):
//
//   1. Scan the active icons for the player-car entry (icon-type 0). For each scanned
//      icon assert leIconType in [0, E_SATNAVICON_MAX); stop at the first type-0 icon;
//      if the scan reaches the last active icon without finding it, fire the X360
//      "FAILED to find player pos to do worst case!" assert. If the player icon was
//      found at index liPlayerIdx > 0, compact it to the front (a 0x30-byte block move).
//
//   2. Advance the module animation phase (sfWorstCasePhase): ramp +1.5 while <= 100,
//      else reset to 0.
//
//   3. Synthesise a 7-icon "worst case" ring (icons[1..7]) around the player icon's
//      position, all tagged E_SATNAVICON_NETWORKRIVAL (== 2) with the player icon's
//      county/district, an incrementing per-icon index, and a ring position offset by
//      the phase/step. Finally set the icon count to 8.
//
// The X360 used VMX (lvx128/stvx128) to move the 16-byte position lanes; reproduced
// here as scalar Vector4 lane assignments (semantic parity, endian-independent).
// ---------------------------------------------------------------------------
GuiEventUpdateSatNav* GuiEventUpdateSatNav::DoWorstCase()
{
    // ---- phase 1: locate the player-car icon (type 0) and compact it to the front ----
    s32 liPlayerIdx = 0;
    if ( miNumIcons > 0 )
    {
        do
        {
            const s32 liIconType = maIconInfo[liPlayerIdx].mi8IconType; // lbz;extsb
            CGS_ASSERT( liIconType >= 0, "leIconType >= 0" );
            CGS_ASSERT( liIconType < SatNavIconInfo::E_SATNAVICON_MAX, "leIconType < E_SATNAVICON_MAX" );
            if ( liIconType == 0 )
                break;
            CGS_ASSERT( liPlayerIdx != miNumIcons - 1,
                        "FAILED to find player pos to do worst case!" );
            ++liPlayerIdx;
        }
        while ( liPlayerIdx < miNumIcons );

        if ( liPlayerIdx != 0 )
        {
            std::memcpy( &maIconInfo[0], &maIconInfo[liPlayerIdx], sizeof(SatNavIconInfo) );
        }
    }

    // ---- phase 2: advance the animation phase ----
    if ( sfWorstCasePhase <= KF_WORST_CASE_PHASE_LIMIT )
        sfWorstCasePhase = sfWorstCasePhase + KF_WORST_CASE_PHASE_STEP;
    else
        sfWorstCasePhase = KF_WORST_CASE_PHASE_RESET;

    // ---- phase 3: synthesise the 7-icon ring (icons[1..7]) ----
    const SatNavIconInfo& lPlayer = maIconInfo[0];
    f32 lfRingZ = KF_WORST_CASE_PHASE_LIMIT; // v17 starts at 100.0, += 50 each entry
    for ( s32 liIndex = 1; liIndex < 8; ++liIndex )
    {
        SatNavIconInfo& lIcon = maIconInfo[liIndex];

        const u8 lu8County = lPlayer.mu8County;
        CGS_ASSERT( lu8County < BrnWorld::E_COUNTY_COUNT, "leCounty < BrnWorld::E_COUNTY_COUNT" );
        lIcon.mu8County = lu8County;

        const u8 lu8District = lPlayer.mu8District;
        CGS_ASSERT( lu8District < BrnWorld::E_DISTRICT_COUNT, "leDistrict < BrnWorld::E_DISTRICT_COUNT" );
        lIcon.mu8District = lu8District;

        lIcon.mi8IconType = SatNavIconInfo::E_SATNAVICON_NETWORKRIVAL; // _R31[3] = 2
        lfRingZ = lfRingZ + KF_WORST_CASE_RING_STEP;                   // v17 += 50
        lIcon.mi8ActiveRaceCarIndex = static_cast<s8>( liIndex );      // _R31[1] = v12++

        // position = { player.x + 50 - phase, player.y, ringZ, 0 }
        lIcon.mv4Position.x = ( lPlayer.mv4Position.x + KF_WORST_CASE_RING_STEP ) - sfWorstCasePhase;
        lIcon.mv4Position.y = lPlayer.mv4Position.y;
        lIcon.mv4Position.z = lfRingZ;
        lIcon.mv4Position.w = 0.0f;
    }

    miNumIcons = 8;
    return this;
}

} // namespace BrnGui
