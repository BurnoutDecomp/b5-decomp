#ifndef GAMESHARED_GAMECLASSES_SYSTEM_GAMETALK_CGSGAMETALK_H
#define GAMESHARED_GAMECLASSES_SYSTEM_GAMETALK_CGSGAMETALK_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/System/GameTalk/X360/CgsGameTalkProtocolX360.h"
#include "GameShared/GameClasses/System/GameTalk/CgsGameTalkDebugComponent.h"
#include "SDKs/EA/GameTalk/GameTalk.h"

// ============================================================================
// GameShared/GameClasses/System/GameTalk/CgsGameTalk.h
//
// CgsGameTalk::GameTalk -- the game-side front door for the EA GameTalk
// in-game <-> authoring-tool channel. It owns the platform transport
// (GameTalkProtocol, the Xbox 360 listen/accept socket backend), wires the
// process-wide EA::GameTalk::GameTalkManager singleton to it, and forwards
// register/send/poll calls through. A LogWindow debug surface and a
// DebugComponent expose its state in the in-game debug UI.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The asm asserts identify the
// original headers/source:
//   ..\..\..\GameShared\GameClasses\System\GameTalk\CgsGameTalk.h
//     (RegisterMessageHandler / SendMessage are header-inline -- their asserts
//      report CgsGameTalk.h lines 143-146 and 178-179)
//   d:\p4\b5_main\burnout\main\code\gameshared\gameclasses\system\gametalk\CgsGameTalk.cpp
//     (Construct / Prepare / Update are out-of-line)
//
// Layout (member offsets read directly off the X360 asm):
//   +0x0000  mProtocol          GameTalkProtocol embedded at offset 0 -- GameTalk::Construct
//                               passes its own `this` straight to GameTalkProtocol::Construct
//                               (the protocol subobject occupies [0, 0x4024))
//   +0x4024  mDebugComponent    CgsDev::DebugComponent (Construct in GameTalk::Construct,
//                               Register in GameTalk::Prepare; 12 bytes -> ends at 0x4030)
//   +0x4030  mpGameTalkManager  the EA GameTalk singleton (Prepare stores the global)
//   +0x4034  mbPrepared         set 1 by Prepare, asserted by Register/Send/Update
// ============================================================================

namespace CgsDev { namespace DebugUI { struct LogWindow; } }

namespace CgsGameTalk
{
    // The existing ARTIST82F32FD4 owner is shared by the split native TUs.
    // Protocol status text uses this same window's public mLog stream.
    extern CgsDev::DebugUI::LogWindow sLogWindow;

    class GameTalk
    {
    public:
        // X360 @0x82837648 (the only GameTalk function that ran in the goal trace).
        // Clears mbPrepared/mpGameTalkManager, constructs the transport on the
        // GameTalk listen port (the asm passes 0x2694 == 9876 in r5), prepares the
        // debug component, and brings up the "Core/GameTalk" log window.
        void Construct();

        // X360 @0x828394B8. Bring up the GameTalk singleton, point mpGameTalkManager
        // at it, prepare the transport with the inactivity timeout / update step,
        // register the AttribSys GameTalk handler on the "AttribSys.xenon" channel,
        // register the debug component, and mark prepared.
        bool Prepare(f32 lrInactivityTimeout, f32 lrUpdateTimeStep);

        // ARTIST82664418 calls this on the GameTalk owner; target82C296C8
        // is the emitted li r3,1;blr ICF fold, with no additional cleanup.
        bool Release();

        // X360 @0x828248D0 (header-inline). Forward a handler registration to the
        // GameTalk manager once prepared.
        s32 RegisterMessageHandler(EA::GameTalk::MessageHandler lpHandlerFunc,
                                   const char* lpacFilter);

        // X360 @0x828DAEE8 (header-inline). Ferry lpMessage out to the lpTarget
        // tool endpoint via the GameTalk manager.
        s32 SendMessage(const char* lpTarget, EA::GameTalk::GameTalkMessage* lpMessage);

        // X360 @0x828376C0. Pump the GameTalk singleton's transport once per frame.
        void Update();

    private:
        GameTalkProtocol            mProtocol;          // +0x0000
        GameTalkDebugComponent     mDebugComponent;    // +0x4024
        EA::GameTalk::GameTalkManager* mpGameTalkManager; // +0x4030
        bool                        mbPrepared;         // +0x4034
    };
}

#endif // GAMESHARED_GAMECLASSES_SYSTEM_GAMETALK_CGSGAMETALK_H
