#pragma once

#include "types.hpp"

namespace BrnResource { namespace GameDataIO { class AllocatorList; } }
namespace CgsMemory { class HeapMalloc; }
namespace MassiveAdClient3 { class CMassiveClientCore; }

// Reconstructed from BURNOUT_X360_ARTIST.XEX (BrnSystemHWX360.cpp / .h).
//
// BrnHW::System360HW is the Xbox 360 hardware-abstraction object owned by
// BrnGameModule. It owns the platform launch-data block, the command line, the
// embedded Massive (memory) sub-system, and two single-step prepare/release state
// machines.
//
// Layout is recovered store-for-store from the X360 asm of Prepare/Release/
// HasGameBeenRebootedDueToInvite:
//   +0x00  user index          (XUserGetSigninState / XUserGetName dwUserIndex)
//   +0x04  signed-in user name (16 bytes; strnicmp'd against the live profile name)
//   +0x14  byte flag gating HasGameBeenRebootedDueToInvite
//   +0x94  byte flag gating HasGameBeenRebootedDueToInvite
//   +0x98  prepare-stage enum (Prepare advances it via operator++)
//   +0x9C  release-stage enum (Release advances it via operator++)
//   +0xA0  embedded Massive sub-system (System360HWMassive)
//   +0xA4  9 unknown bytes, then the command-line buffer at +0xAD
// The first 0x98 bytes are the buffer XGetLaunchData fills (dwBufferSize = 0x98).

namespace BrnMassive
{
    // The embedded Xbox-360 Massive (in-game advertising) sub-system, embedded in
    // System360HW at +0xA0. Defined out-of-line in its own platform TU
    // (BrnSystemHWX360Massive.cpp). It drives two independent state machines (prepare
    // and release) and owns the MassiveAdClient3 client-core pointer.
    //
    // Layout recovered store-for-store from the X360 asm of Prepare @ 0x823AB258 and
    // Release @ 0x823AB4B8 (this-relative loads/stores at +0x00 / +0x04 / +0x08):
    //   +0x00  mePrepareStage  (Prepare switches on it; advanced by operator++)
    //   +0x04  meReleaseStage  (Release switches on it; advanced by operator++)
    //   +0x08  mpClientCore    (CMassiveClientCore*, set by Initialize, cleared by Release)
    // The X360 pointer width is 4 bytes; on the 64-bit host mpClientCore widens to 8,
    // so the absolute struct size differs -- members are accessed by NAME, never offset.
    struct System360HWMassive
    {
        // Multi-step Massive prepare state machine (0..4). operator++ advances the stage
        // by one; the asm asserts the value never runs past E_PREPARESTAGE_DONE (the
        // `result > 4` overflow test in the prepare-stage operator++ @ 0x823A8958).
        // Concrete intermediate stage names are not attested; only the START/DONE bounds
        // (0 and 4) are recovered from the asm.
        enum EPrepareStage
        {
            E_PREPARESTAGE_START = 0,
            E_PREPARESTAGE_DONE  = 4
        };

        // Release state machine -- a DISTINCT enum from EPrepareStage. The X360 Release
        // switch has four cases (0..3) and advances the stage through a SEPARATE postfix
        // operator++ overload (sub_823A89B8 @ 0x823A89B8, distinct from the prepare-stage
        // operator++ @ 0x823A8958), which pins DONE at 3. Intermediate stages unnamed.
        enum EReleaseStage
        {
            E_RELEASESTAGE_START = 0,
            E_RELEASESTAGE_DONE  = 3
        };

        EPrepareStage                          mePrepareStage;  // +0x00
        EReleaseStage                          meReleaseStage;  // +0x04
        MassiveAdClient3::CMassiveClientCore*  mpClientCore;    // +0x08

        // @ 0x823AB258. Drive the Massive prepare state machine: once a user is signed in
        // to LIVE, install the custom heap hooks and initialise the MassiveAd client core
        // (EnterZone "TEST1"). Returns 0 while it cannot yet proceed, 1 once initialised.
        int Prepare();

        // @ 0x823AB4B8. Drive the Massive release state machine: exit the ad zone, flush
        // impressions and shut the client core down. Always returns 1.
        int Release();

        // MassiveAd custom heap hooks registered via CMassiveClientCore::SetCustomMemory-
        // Functions (their addresses are taken as C callbacks). They route MassiveAd
        // allocations through the shared Massive heap (gpMassiveHeapMalloc).
        //   @ 0x823AB0F8  BurnoutMassiveMalloc(size)  -> block, or null on failure
        //   @ 0x823AB1E8  BurnoutMassiveFree(block)   -> free the block
        static void* BurnoutMassiveMalloc(unsigned int luSize);
        static void  BurnoutMassiveFree(void* lpMemory);
    };

    // Postfix increment for the Massive prepare-stage state machine
    // (BrnSystemHWX360Massive.cpp @ 0x823A8958).
    System360HWMassive::EPrepareStage operator++(System360HWMassive::EPrepareStage& reStage, int);

    // Postfix increment for the Massive release-stage state machine (sub_823A89B8 @
    // 0x823A89B8). Its body -- and the verbatim rodata of its overflow assert -- are not
    // in this TU's dossier, so it is left to its own ledger TU; declared here so Release()
    // can advance the stage through the real call rather than a plain integer bump.
    System360HWMassive::EReleaseStage operator++(System360HWMassive::EReleaseStage& reStage, int);

    // The Massive heap allocator: a single X360 .data global (spHeapMalloc) shared between
    // BrnHW::System360HW::PrepareMassiveMemory (which fills it from the game-data bank
    // registry) and the Massive custom heap hooks above (which allocate from it). Defined
    // once in BrnSystemHWX360Massive.cpp.
    extern CgsMemory::HeapMalloc* gpMassiveHeapMalloc;
}

namespace BrnHW
{
    struct System360HW
    {
        // Single-step prepare/release state machine. START is the initial state and
        // operator++ advances to DONE; the asm asserts the value never runs past
        // DONE (the `v > 1` overflow test in Prepare/Release).
        enum EPrepareStage
        {
            E_PREPARESTAGE_START = 0,
            E_PREPARESTAGE_DONE  = 1
        };

        // Launch-data sized window the platform fills (XGetLaunchData dwBufferSize = 0x98).
        u32  mdwUserIndex;          // +0x00
        char macUserName[16];       // +0x04  signed-in profile name snapshot
        u8   mPad14;                // +0x14  flag gating HasGameBeenRebootedDueToInvite
        u8   mPad15[0x7F];          // +0x15..+0x93
        u8   mPad94;                // +0x94  flag gating HasGameBeenRebootedDueToInvite
        u8   mPad95[3];             // +0x95..+0x97 (pad to the 0x98 launch-data extent)

        EPrepareStage mePrepareStage;   // +0x98
        EPrepareStage meReleaseStage;   // +0x9C

        BrnMassive::System360HWMassive mMassive;  // +0xA0

        // Command-line storage. The launch command line is copied to +0xAD (index 9
        // of the +0xA4 byte region); CgsStringUtils caps the copy at 64 bytes.
        u8   mPadA4[9];                 // +0xA4..+0xAC
        char macCommandLine[64];        // +0xAD  launch command line text

        bool Prepare();
        int  Release();
        // The first argument is ignored by the X360 body (r3 is overwritten by the
        // AllocatorList passed in r4); kept in the signature to match the call site.
        static int PrepareMassiveMemory(int liUnused,
                                        BrnResource::GameDataIO::AllocatorList* lpAllocatorList);
        bool HasGameBeenRebootedDueToInvite();
    };

    // Postfix increment for the prepare/release state machine (BrnSystemHWX360.cpp).
    System360HW::EPrepareStage operator++(System360HW::EPrepareStage& reStage, int);
}
