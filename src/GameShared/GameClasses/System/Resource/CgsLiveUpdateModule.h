#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"     // base CgsModule::ModuleSingleBuffered
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"   // mReceiverQueue (EventReceiverQueue<512,16>)

// ============================================================================
// GameShared/GameClasses/System/Resource/CgsLiveUpdateModule.h
//
// CgsResource::LiveUpdateModule -- the "live update" resource module. It lets an
// external authoring tool (over GameTalk, key "ReplaceResourcesFromFile") request
// that the resources in an already-running game be replaced from a patch bundle on
// disk. The module is a process-wide singleton (spInstance, asserted NULL at
// Construct): the GameTalk handler stashes the requested patch-bundle path and arms
// the module, and the per-frame Update() walks a tiny state machine that, after a
// few frames' settle, pushes a LoadBundleRequest (live-update-replace = true) onto
// its output request queue, waits for the loader's response on its receiver queue,
// and reports "success"/"failed" back to the tool over GameTalk.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Asserts in the bodies identify the
// original source: d:\p4\b5_main\...\system\resource\CgsLiveUpdateModule.cpp.
//   Construct           @ 0x828E6548   (ran in the goal trace)
//   Destruct            @ 0x828E6708
//   Release             @ 0x828E6680
//   GameTalkMsgHandler  @ 0x828DAF60   (static GameTalk message callback)
//   Update              @ 0x82903128
//
// LAYOUT (member offsets read directly off the X360 asm; the base
// CgsModule::ModuleSingleBuffered subobject occupies [0, 0x228) and the module's own
// members follow at +0x228; widths are x64 here, so offsets are documentary, NOT
// byte-matched -- no offsetof asserts):
//   +0x228  mReceiverQueue   EventReceiverQueue<512,16> (Construct binds buffer @ +0x18,
//                            capacity 512, alignment 16; ends at +0x440)
//   +0x440  mpGameTalk       the CgsGameTalk::GameTalk this module replies through
//                            (Construct arg a2)
//   +0x444  miField273       reset to 0 by Construct and Release (purpose not attested
//                            by these 5 functions; named by its X360 word index)
//   +0x448  meReleaseStage   release-stage guard (Construct = 1; Release asserts it is
//                            0 or 1, "Invalid stage")
//   +0x44C  miUpdateState    the per-frame state machine field (0 idle / 1 armed /
//                            2 settling / 3 awaiting-response)
//   +0x450  mcUpdateCounter  s8 settle counter (incremented while settling, fires at 5)
//   +0x451  macFileName[256] the requested patch-bundle path (GameTalk handler copies
//                            it in; Update feeds it to the LoadBundleRequest)
// ============================================================================

namespace EA          { namespace GameTalk { class GameTalkMessage; } }
namespace CgsGameTalk { class GameTalk; }
namespace CgsModule   { struct IOBuffer; }

namespace CgsResource
{
    class LiveUpdateModule : public CgsModule::ModuleSingleBuffered
    {
    public:
        // X360 0x828E6548. Assert the singleton is not yet live, chain the base module
        // Construct, bind+Clear the receiver queue (capacity 512, alignment 16), publish
        // spInstance, store the GameTalk channel, and init the state fields.
        // (lpGameTalk is the Construct arg a2 -- the reply channel.)
        void Construct(CgsGameTalk::GameTalk* lpGameTalk);

        // X360 0x828E6708. Clear the receiver queue and chain the base Destruct.
        void Destruct();

        // X360 0x828E6680. Reset the receiver queue + state for a fresh run, guarding the
        // release stage ("Invalid stage"). Returns true on success.
        bool Release();

        // X360 0x82903128. Per-frame: write-lock the output buffer, advance the state
        // machine (arm -> settle 5 frames -> push the LoadBundleRequest -> await the
        // loader response -> report success/failed over GameTalk), publish the in-progress
        // status, and unlock.
        void Update(CgsModule::IOBuffer* lpOutputBuffer);

        // X360 0x828DAF60. Static GameTalk message callback (registered as an
        // EA::GameTalk::MessageHandler). When the incoming message's state is 1 and it
        // carries a non-empty "ReplaceResourcesFromFile" key, copy that path into the
        // singleton's macFileName and arm the module (miUpdateState = 1) -- unless a
        // request is already armed/running.
        static void GameTalkMsgHandler(EA::GameTalk::GameTalkMessage* lpMessage);

    private:
        // The process-wide singleton (X360 off_830EA3F0): Construct asserts it is NULL,
        // sets it to `this`, and the static GameTalk handler reaches the live module
        // through it.
        static LiveUpdateModule* spInstance;

        // ---- Layout (faithful order; x64 widths; compiler-laid-out; offsets documentary) ----
        CgsModule::EventReceiverQueue<512, 16> mReceiverQueue;  // +0x228
        CgsGameTalk::GameTalk* mpGameTalk;                      // +0x440
        s32  miField273;                                        // +0x444 (reset 0)
        s32  meReleaseStage;                                    // +0x448 (Construct = 1)
        s32  miUpdateState;                                     // +0x44C
        s8   mcUpdateCounter;                                   // +0x450
        char macFileName[256];                                  // +0x451
    };
}
