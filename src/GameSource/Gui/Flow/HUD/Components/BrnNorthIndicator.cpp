// ============================================================================
// b5-decomp/src/GameSource/Gui/Flow/HUD/Components/BrnNorthIndicator.cpp
//
// BrnGui::NorthIndicatorComponent -- the HUD compass component. Two methods:
//   Update       (X360 0x824113B8) -- publish the needle rotation as the "_rotation" view state
//   SetEventType (X360 0x82411430) -- map the game mode to a compass style "_style" view state
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX. The X360 inlines the
// default-case Begin/Fire/EndAssert assert triple in SetEventType; it is reproduced as a
// house CGS_ASSERT (the baked file/line are discarded per project convention).
// ============================================================================

#include "GameSource/Gui/Flow/HUD/Components/BrnNorthIndicator.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::SPrintf

namespace BrnGui
{

// X360 rodata: radians -> degrees scale (flt_82011268 == 57.295776) and the +180 offset
// (flt_820025FC == 180.0) the X360 fmadds applies to the heading.
static const f32 KF_RAD_TO_DEG     = 57.295776f;
static const f32 KF_ROTATION_OFFSET = 180.0f;

// The "_style" apt view-state strings, indexed by the style id SetEventType resolves
// (off_82F24890[v3]). off_82F24890[0] == "Race" is asm-attested; the remaining ids carry
// the DWARF-recovered E_STYLES names (BrnNorthIndicator.h:47). Only ids 0..3 are reachable
// from SetEventType's game-mode mapping.
static const char* const mpacStyleAptStrings[NorthIndicatorComponent::E_STYLE_COUNT] =
{
    "Race",          // E_STYLE_RACE
    "RoadRage",      // E_STYLE_ROADRAGE
    "TrafficAttack", // E_STYLE_TRAFFICATTACK
    "Pursuit",       // E_STYLE_PURSUIT
    "Eliminator",    // E_STYLE_ELIMINATOR
    "BurningRoute",  // E_STYLE_BURNINGROUTE
    "ShowTime",      // E_STYLE_SHOWTIME
};

// @ 0x824113B8
//   v8 = (double)(a2 * 57.295776 + 180.0)        ; fmadds f1,f1,scale,offset ; stfd -> ld as the %f arg
//   SPrintf(buf16, 7, "%f", v8); buf[7] = 0;     ; format the rotation into a 16-byte temp
//   AddOutputAptViewState(this, "_rotation", buf, true)
void NorthIndicatorComponent::Update(f32 lfHeadingRadians)
{
    // The X360 stores the fmadds result as a double and passes it to SPrintf via the "%f"
    // varargs slot (a default-promoted double).
    const double ldRotation = static_cast<double>(lfHeadingRadians * KF_RAD_TO_DEG + KF_ROTATION_OFFSET);

    char lacRotation[16];
    CgsCore::SPrintf(lacRotation, 7, "%f", ldRotation);
    lacRotation[7] = 0;   // X360 stb 0, buf+7

    AddOutputAptViewState("_rotation", lacRotation, true);
}

// @ 0x82411430
//   switch (leGameMode) -> style id (default 0):
//     {0,1,6,9,10,12,13,14,15,17} -> 0 (Race)
//     {2,16}                      -> 2 (TrafficAttack)
//     {3,5,7,8,11}                -> 1 (RoadRage)
//     {4}                         -> 3 (Pursuit)
//     default                     -> assert + 0 (Race)
//   AddOutputAptViewState(this, "_style", styleString[id], false)
void NorthIndicatorComponent::SetEventType(BrnGameState::GameStateModuleIO::EGameModeType leGameMode)
{
    s32 liStyle = E_STYLE_RACE;
    switch (leGameMode)
    {
        case BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_RACE:            // 0
        case BrnGameState::GameStateModuleIO::E_MODE_FACE_OFF:                // 1
        case BrnGameState::GameStateModuleIO::E_MODE_ELIMINATOR:             // 6
        case BrnGameState::GameStateModuleIO::E_MODE_TRAFFIC_ATTACK:         // 9
        case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_RACE:            // 10 (== E_MODE_ONLINE_MODE_START)
        case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FUGITIVE:        // 12
        case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_BURNING_HOME_RUN: // 13
        case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN:       // 14
        case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY: // 15
        case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_MODE_END:        // 17 (== E_MODE_COUNT)
            liStyle = E_STYLE_RACE;
            break;

        case BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME:       // 2
        case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_SHOWTIME:        // 16
            liStyle = E_STYLE_TRAFFICATTACK;
            break;

        case BrnGameState::GameStateModuleIO::E_MODE_ROAD_RAGE:              // 3
        case BrnGameState::GameStateModuleIO::E_MODE_BURNING_ROUTE:          // 5
        case BrnGameState::GameStateModuleIO::E_MODE_STUNT_ATTACK:           // 7
        case BrnGameState::GameStateModuleIO::E_MODE_MARKED_MAN:             // 8
        case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE:       // 11
            liStyle = E_STYLE_ROADRAGE;
            break;

        case BrnGameState::GameStateModuleIO::E_MODE_PURSUIT:                // 4
            liStyle = E_STYLE_PURSUIT;
            break;

        default:
            CGS_ASSERT(false, "Unknown enum value passed to NorthIndicatorComponent::SetEventType\n");
            liStyle = E_STYLE_RACE;
            break;
    }

    AddOutputAptViewState("_style", mpacStyleAptStrings[liStyle], false);
}

} // namespace BrnGui
