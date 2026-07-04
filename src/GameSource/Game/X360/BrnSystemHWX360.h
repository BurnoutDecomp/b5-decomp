#pragma once

#include "types.hpp"

namespace BrnResource { namespace GameDataIO { class AllocatorList; } }

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
    // The embedded Xbox-360 Massive memory sub-system. Defined out-of-line in its
    // own platform TU (BrnSystemHWX360Massive.cpp); declared here so System360HW can
    // embed it at +0xA0 and name Release(). It occupies the 0xA0..0xA3 window of the
    // owner (the next named owner member sits at +0xA4), so it is reserved as 4
    // bytes here; its concrete field layout is reconstructed separately.
    struct System360HWMassive
    {
        // Multi-step Massive prepare state machine. operator++ advances the stage by
        // one; the asm asserts the value never runs past E_PREPARESTAGE_DONE (the
        // `result > 4` overflow test in operator++ @ 0x823A8958). Concrete intermediate
        // stage names are not attested; only the START/DONE bounds (0 and 4) are
        // recovered from the asm.
        enum EPrepareStage
        {
            E_PREPARESTAGE_START = 0,
            E_PREPARESTAGE_DONE  = 4
        };

        u8 mPad00[4];
        int Release();
    };

    // Postfix increment for the Massive prepare-stage state machine
    // (BrnSystemHWX360Massive.cpp @ 0x823A8958).
    System360HWMassive::EPrepareStage operator++(System360HWMassive::EPrepareStage& reStage, int);
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
