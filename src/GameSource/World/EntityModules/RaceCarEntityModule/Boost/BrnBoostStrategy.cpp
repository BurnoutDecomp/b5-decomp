// ============================================================================
// BrnWorld::BoostStrategy -- the mode-start boost seed.
//
//   OnModeStart   X360 0x822A5FC8
//
// The base class's own bodies are otherwise all pure/derived (BoostBurnout2/3/5 carry the
// per-strategy TUs), so this is the first BrnBoostStrategy.cpp. OnModeStart is NOT virtual
// -- HandlePrepareForModeAction @0x823092F0 calls it directly on the module's current
// strategy (`*(a1 + 97504)`) -- but it DISPATCHES through slot 49 (+0xC4) AddBoost, which
// is what makes the quarter/full seeds respect each strategy's own earning rules.
//
// SOURCE: BURNOUT_X360_ARTIST.XEX raw asm 0x822A5FC8..0x822A608C (a 14-entry jump table at
// jpt_822A5FE4). The three float constants are read out of the decrypted image, not guessed:
//   flt_82003F40 = 0x3E800000 = 0.25f
//   flt_82005450 = 0x3F666666 = 0.9f   (0.89999998 as the pseudocode prints it)
//   flt_820147FC = 0x3F000000 = 0.5f
// ============================================================================
#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostStrategy.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"   // EGameModeType

namespace BrnWorld
{
    namespace
    {
        // The three seeds, read out of the decrypted ARTIST image at the addresses the asm
        // names (file_off = 0x3000 + vaddr - 0x82000000, big-endian) -- NOT inferred from the
        // pseudocode's printed decimals.
        const f32 KF_MODE_START_QUARTER_BAR    = 0.25f;   // flt_82003F40 (0x3E800000)
        const f32 KF_MODE_START_ONLINE_HIGH_BAR = 0.9f;   // flt_82005450 (0x3F666666)
        const f32 KF_MODE_START_ONLINE_HALF_BAR = 0.5f;   // flt_820147FC (0x3F000000)
    }

// ----------------------------------------------------------------------------
// OnModeStart @ 0x822A5FC8.
//
// Seed the boost bar for the mode that is starting. Four shapes, straight off the jump
// table -- and note the DEFAULT arm is "do nothing", reached two different ways: the
// `cmplwi cr6,r4,0xD / bgtlr cr6` guard at the top returns immediately for anything above
// E_MODE_ONLINE_BURNING_HOME_RUN (so E_MODE_ONLINE_FREE_BURN(14),
// E_MODE_ONLINE_FREE_BURN_LOBBY(15) and E_MODE_ONLINE_SHOWTIME(16) never touch the bar),
// and six in-range modes route to the empty table slot at 0x822A6088.
//
// ⚠️ THE TWO ARMS ARE NOT THE SAME OPERATION. Modes 0/3/7/8 and 5 go through the VIRTUAL
// AddBoost (slot 49, `lwz r11,0xC4(r10)` / `bctr`), which clamps and honours
// mbBoostEarningEnabled / mbCrashing; modes 10/11/13 STORE mfBoostAmount (+0xA0) directly
// (`stfs f0,0xA0(r3)`), bypassing all of that. Collapsing either into the other would
// change what the bar does on a crashed or earning-disabled car.
//
// lbFlag is the caller's `*(a1 + 96404) == 2` -- BoostManager's current strategy-type
// selector -- and only the online arm reads it.
// ----------------------------------------------------------------------------
void BoostStrategy::OnModeStart(BrnGameState::GameStateModuleIO::EGameModeType leGameModeType,
                                bool lbFlag)
{
    switch (leGameModeType)
    {
    // jumptable 822A5FE4 cases 0,3,7,8 -- asm 0x822A6020.
    case BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_RACE:
    case BrnGameState::GameStateModuleIO::E_MODE_ROAD_RAGE:
    case BrnGameState::GameStateModuleIO::E_MODE_STUNT_ATTACK:
    case BrnGameState::GameStateModuleIO::E_MODE_MARKED_MAN:
        AddBoost(mfMaxBoost * KF_MODE_START_QUARTER_BAR);
        break;

    // jumptable 822A5FE4 case 5 -- asm 0x822A6040.
    case BrnGameState::GameStateModuleIO::E_MODE_BURNING_ROUTE:
        AddBoost(mfMaxBoost);
        break;

    // jumptable 822A5FE4 cases 10,11,13 -- asm 0x822A6054.
    case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_RACE:
    case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE:
    case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_BURNING_HOME_RUN:
        mfBoostAmount = lbFlag ? (mfMaxBoost * KF_MODE_START_ONLINE_HIGH_BAR)
                               : (mfMaxBoost * KF_MODE_START_ONLINE_HALF_BAR);
        break;

    // jumptable 822A5FE4 cases 1,2,4,6,9,12 (the empty slot at 0x822A6088), plus every
    // mode > 13 via the `bgtlr` guard: the bar is left exactly as it was.
    default:
        break;
    }
}

} // namespace BrnWorld
