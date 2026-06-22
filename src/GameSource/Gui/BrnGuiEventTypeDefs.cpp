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

namespace BrnGui
{

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

} // namespace BrnGui
