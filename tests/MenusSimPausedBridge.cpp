// OWNERLIST 2026-09-27, lane L5 MENUS: the director's sim-paused flag -- the PRODUCTION staging block of
// BrnGame::BrnGameModule::DoUpdate_Director (src/GameSource/Game/BrnGameModule.cpp), extracted by
// run_menus_simpaused_bridge.py and compiled against recording stand-ins that carry the production names.
//
// Checked against the ARTIST asm, DoUpdate_Director @0x823E8DE0, right after BridgeGameStateToDirector:
//   0x823E8EF8  rlwinm r11, updateSet, 0,23,23 ; bne 0x823E8F28   the 0x100 (replay-playback) bit -> store 0
//   0x823E8F0C  lbzx   r11, gm, 0x9A0635       ; bne 0x823E8F28   mbOnline                         -> store 0
//   0x823E8F20  lbzx   r11, gm, 0x9A0621                           mbSimPaused                      -> store it
//   0x823E8F2C  stb    r11, 0x7AC8(input)                          DirectorIO::InputBuffer::SetSimPaused
// The console stages the byte once per DoUpdate_Director (the pre-GUI pass); nothing else in the image writes it
// besides InputBuffer::Construct. The PC never staged it, so the director always saw an unpaused sim.
#include <cstdio>
#include "types.hpp"

static unsigned gChecks = 0, gFailures = 0;

// Recording stand-in for BrnDirector::DirectorIO::InputBuffer.
struct DirectorInputStandIn
{
    bool mbSimPaused      = false;
    int  miWriteLockDepth = 0;
    int  miStores         = 0;
    int  miLockAtStore    = -1;
    void LockForWrite()   { ++miWriteLockDepth; }
    void UnlockForWrite() { --miWriteLockDepth; }
    void SetSimPaused(bool lbSimPaused)
    {
        mbSimPaused   = lbSimPaused;
        miLockAtStore = miWriteLockDepth;
        ++miStores;
    }
};

// Stand-in for BrnGame::BrnGameModule with the three members the store reads.
struct GameModuleStandIn
{
    bool mbOnline;
    bool mbSimPaused;
    u32  muUpdateSet;
    u32 ConstructUpdateSetFromFsm() const { return muUpdateSet; }

    // Stage(...) { <the production staging block> }
    void Stage(bool lbPostGui, DirectorInputStandIn* lpDirectorInput);
};

#include "simpaused_bridge.inc"

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

static bool Staged(bool lbOnline, bool lbSimPaused, u32 luUpdateSet, bool lbPostGui, DirectorInputStandIn& lrInput)
{
    GameModuleStandIn lModule = { lbOnline, lbSimPaused, luUpdateSet };
    lModule.Stage(lbPostGui, &lrInput);
    return lrInput.mbSimPaused;
}

int main()
{
    // The in-game fsm sets (ConstructUpdateSetFromFsm @0x823BD420): 0x88, | 0x1 while the sim is paused.
    const u32 KU_IN_GAME        = 0x88u;
    const u32 KU_IN_GAME_PAUSED = 0x89u;

    {
        DirectorInputStandIn lInput;
        Check(Staged(false, true, KU_IN_GAME_PAUSED, false, lInput),
              "the pause (offline, no replay): the director is told the sim is paused (0x823E8F20 arm)");
        Check(lInput.miStores == 1, "one store per pre-GUI pass (0x823E8F2C)");
        Check(lInput.miLockAtStore == 1, "the store runs with the director input write-locked");
        Check(lInput.miWriteLockDepth == 0, "the write lock is released after the store");
    }
    {
        DirectorInputStandIn lInput;
        lInput.mbSimPaused = true;   // last frame's value must not survive: the byte is re-staged every pass
        Check(!Staged(false, false, KU_IN_GAME, false, lInput), "not paused: the director is told so (stores 0)");
    }
    {
        DirectorInputStandIn lInput;
        Check(!Staged(true, true, KU_IN_GAME_PAUSED, false, lInput),
              "online: never paused for the director (lbzx gm+0x9A0635 ; bne 0x823E8F28)");
    }
    {
        DirectorInputStandIn lInput;
        Check(!Staged(false, true, KU_IN_GAME_PAUSED | 0x100u, false, lInput),
              "replay playback (update-set 0x100): never paused for the director (rlwinm 0,23,23 ; bne 0x823E8F28)");
    }
    {
        DirectorInputStandIn lInput;
        Check(!Staged(true, true, KU_IN_GAME_PAUSED | 0x100u, false, lInput), "both gates set: 0");
    }
    {
        DirectorInputStandIn lInput;
        Staged(false, true, KU_IN_GAME_PAUSED, true, lInput);
        Check(lInput.miStores == 0, "the post-GUI pass does not stage it (the console's only store is DoUpdate_Director's)");
    }

    std::printf("MenusSimPausedBridge: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
