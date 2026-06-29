#pragma once

#include "types.hpp"

// ============================================================================
// GameShared/GameClasses/Development/DebugSystem/GameTalk/CgsDebugGameTalkInterface.h
//
// CgsDev::DebugGameTalkInterface -- the bridge that lets external authoring
// tools drive the in-game debug console over the EA GameTalk channel. It holds
// the CgsGameTalk::GameTalk transport it registers against, tracks a small
// prepare-stage so it registers its message handler exactly once, and routes
// inbound "ExecuteScript" messages (on the "DebugSystem" channel) into the
// debug UI's script runner.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The asm asserts identify the
// original source:
//   ..\..\..\GameShared\GameClasses\Development\DebugSystem\GameTalk\
//       CgsDebugGameTalkInterface.cpp
//     (Construct asserts line 52, Prepare asserts line 90).
//
// Owned by BrnResource::GameDataModule (GameDataModule::Construct calls
// Construct @0x828249C8; GameDataModule::Prepare calls Prepare @0x82833E20).
//
// Layout (member offsets read directly off the X360 asm):
//   +0x00  mpGameTalk   the GameTalk transport (Construct stores its argument
//                       here via `*this = lpGameTalk`; Prepare reads it back at
//                       *a1 to register the handler). Asserted non-NULL.
//   +0x04  (unused)     not touched by any of the three reconstructed methods.
//   +0x08  miStage      prepare stage: 0 == not yet registered, 1 == prepared.
//                       Prepare registers the handler only on the 0 -> 1 edge;
//                       a stage > 1 fires the "Invalid stage" assert.
//   +0x0C  miReserved   cleared to 0 by Prepare (a1[3] = 0).
// ============================================================================

namespace CgsGameTalk { class GameTalk; }
namespace EA { namespace GameTalk { class GameTalkMessage; } }

namespace CgsDev
{
    class DebugGameTalkInterface
    {
    public:
        // X360 @0x828249C8. Store the GameTalk transport this interface drives
        // (asserting it is non-NULL -- the asm's "lpGameTalk" assert at line 52).
        // The X360 ABI hands the object pointer back in r3; modelled as a void
        // member that writes mpGameTalk.
        void Construct(CgsGameTalk::GameTalk* lpGameTalk);

        // X360 @0x82833E20. On the first call (miStage == 0) register
        // GameTalkMsgHandler on the GameTalk transport's "DebugSystem" channel,
        // then mark prepared (miStage = 1, miReserved = 0). A second call while
        // already prepared (miStage == 1) just re-marks prepared without
        // re-registering. miStage > 1 fires the "Invalid stage" assert and
        // returns false. Returns true once prepared.
        bool Prepare();

    private:
        // X360 @0x82833D20. GameTalk message-received callback registered on the
        // "DebugSystem" channel. When the message's state word is 1 and it carries
        // an "ExecuteScript" key, run that script text through the debug UI's
        // script interface (acquiring the DebugManager around the call).
        static void GameTalkMsgHandler(EA::GameTalk::GameTalkMessage* lpMessage);

        CgsGameTalk::GameTalk* mpGameTalk;   // +0x00
        u8                     mPad04[4];    // +0x04 (unused by these methods)
        s32                    miStage;      // +0x08
        s32                    miReserved;   // +0x0C
    };
}
