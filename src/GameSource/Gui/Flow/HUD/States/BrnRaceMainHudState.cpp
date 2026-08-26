#include "GameSource/Gui/Flow/HUD/States/BrnRaceMainHudState.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT
#include "GameShared/GameClasses/Containers/CgsHash.h"                  // CgsContainers::CgsHash::CalculateHash

// Reconstructed from BURNOUT_X360_ARTIST.XEX -- BrnGui::RaceMainHudState, the RACE_MAIN
// slot of the BrnHudFlow 14-state pool (the in-event main HUD). Landed here:
//   RaceMainHudState::SetExpectedComponent(const char*)  @ 0x82473698
//   the .rdata resource-tuple table + its count           @ 0x82F25F88 / @ 0x82F25F84
//
// E1 WAVE 2026-08-26 -- THE ODR FORK IS RETIRED. This TU used to declare its own
// `struct RaceMainHudState { u8 maHeadReserved[0x3C]; u32 maExpectedComponents[64];
// u32 muExpectedComponentCount; u8 maBodyReserved[0x7910-0x140]; }` in namespace BrnGui
// while BrnHudFlow.cpp compiled against a header that declared no members at all -- two
// definitions of one class, and the fork's constructor zero-filled 0x7910 bytes into a
// State-sized NewPoolState<RaceMainHudState> allocation. The fork is deleted; the one
// definition is BrnRaceMainHudState.h, grown onto the DecFIGS DWARF member set, and the
// bodies below address the real members by name.
//
// The constructor @0x82508110 moved to the header as an inline body: it is the compiler's
// own sub-object-vtable chain (no RaceMainHudState POD is written on console) and
// BrnHudFlow.cpp -- which IS on the build -- placement-news the state, so its definition
// has to be visible whether or not this TU is mounted. See the header for the citation.

namespace BrnGui
{
    // =======================================================================
    //  The static .rdata resource table (values read from the XEX image at
    //  0x82F25F88, count at 0x82F25F84; the name after each id is
    //  off_82F278E0[id] from the same image -- the table the FBurnMainHudState
    //  42-entry recovery used).
    // =======================================================================
    // 21 entries: the B5RaceHud apt movie plus the aux component imports the in-event HUD
    // mounts. Type 7 == E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE, type 11 ==
    // E_GUI_RESOURCETYPE_LOCALISED_TEXT. Against the freeburn state's 42-entry list this
    // one adds B5CompassComponent (24) and B5ShowtimeComponents (88) and drops the whole
    // freeburn menu/ticker/mugshot block -- the in-event HUD's own surface.
    const CgsGui::sResourceTuple RaceMainHudState::maResourcesToLoad[] =
    {
        { 192u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5RaceHud
        {  32u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // Timer
        {  23u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5NorthIndicatorComponent
        {  24u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5CompassComponent
        {  37u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5HudMessage
        {  27u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // CountdownIcon
        { 200u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5SatNavComponent
        {  25u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // SatNavDistance
        {  26u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // SatNavStatic
        { 199u, CgsGui::E_GUI_RESOURCETYPE_LOCALISED_TEXT   },  // SatNavMap
        { 201u, CgsGui::E_GUI_RESOURCETYPE_LOCALISED_TEXT   },  // SatNavMask
        {  60u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // BoostMessage
        {  56u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5Triggers
        {  62u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5PositionIndicatorComponent
        {  64u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // DistrictIcon
        {  65u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // DistrictMarker
        {  73u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5RaceEventInfo
        {  75u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5PositionTableComponent
        {  33u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5RoadRuleComponent
        {  88u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5ShowtimeComponents
        {  90u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5VersionTextComponent
    };
    const u32 RaceMainHudState::muNumResourcesToLoad = 21;

    // =======================================================================
    //  SetExpectedComponent  @ 0x82473698
    // =======================================================================
    // Append the hash of an expected APT-component name to the table. Asserts there is
    // room (count < 0x40 -- BrnRaceMainHudState.h:634, the assert's line argument
    // r5 = 0x27A), hashes the NUL-terminated name (length excludes the NUL), stores at
    // this[15 + count] (byte +0x3C + count*4 == mauExpectedComponentIds[count]) and
    // increments the count. The hash stays in r3 at return.
    u32 RaceMainHudState::SetExpectedComponent(const char* lpcName)
    {
        CGS_ASSERT(muNumExpectedComponents < KU_MAX_INIT_COMPONENTS_NUM,
                   "No space for new expected component");

        const char* lpc = lpcName;
        while (*lpc) ++lpc;
        u32 luHash = CgsContainers::CgsHash::CalculateHash(
            const_cast<char*>(lpcName), static_cast<int>(lpc - lpcName));   // length excludes the NUL

        mauExpectedComponentIds[muNumExpectedComponents] = luHash;
        ++muNumExpectedComponents;
        return luHash;
    }
}
