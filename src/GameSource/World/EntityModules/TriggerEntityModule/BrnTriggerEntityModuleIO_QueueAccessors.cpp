#include "GameSource/World/EntityModules/TriggerEntityModule/BrnTriggerEntityModuleIO.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsAssertStreamMessageAt.h"   // the header-baked lock asserts
#define CGS_ARTIST_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\world\\entitymodules\\triggerentitymodule\\BrnTriggerEntityModuleIO.h"
// Out-of-line bodies of the BrnWorld::TriggerEntityModuleIO buffer queue accessors the X360
// ARTIST build emitted out-of-line. Each guarded accessor asserts the buffer's lock bit then
// returns the address of its embedded queue member (this + 4):
//   read-lock  (status>>4 &1) => IsBufferLockedForReading()  (const getters)
//   write-lock (status>>3 &1) => IsBufferLockedForWriting()  (mutators)
// The corresponding header getters are reduced to declarations so these are the sole (ODR)
// definitions. Assert strings follow the committed BrnTriggerEntityModuleIO_Accessors.cpp
// convention (no trailing "\n").

namespace BrnWorld
{
namespace TriggerEntityModuleIO
{
    // X360 0x822BCFE0 (R, :118) -- const trigger-query input queue accessor; read-lock guarded,
    // returns &mQueryInputInterface (this + 4). Read by ProcessTriggerQueryEvents.
    // ARTIST li r5,0x76 (DWARF BrnTriggerEntityModuleIO.h:118).
    const TriggerQueryQueue* InputBuffer_PostScene::GetQueryInputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), CGS_ASSERT_STREAM_MESSAGE_AT("Not locked for reading\n", CGS_ARTIST_FILE, 118));
        return &mQueryInputInterface;
    }

    // X360 0x827A3270 (W, :119) -- mutable trigger-query input queue accessor; write-lock guarded,
    // returns &mQueryInputInterface (this + 4). Its only caller is
    // WorldModule::BridgeInputToEntityModules' post-scene query append.
    // ARTIST li r5,0x77 (DWARF BrnTriggerEntityModuleIO.h:119).
    TriggerQueryQueue* InputBuffer_PostScene::GetQueryInputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), CGS_ASSERT_STREAM_MESSAGE_AT("Not locked for writing\n", CGS_ARTIST_FILE, 119));
        return &mQueryInputInterface;
    }

    // X360 0x822BD130 (R, :172) -- const scene-result queue accessor; read-lock guarded, returns
    // &mSceneResultQueue (this + 4). Read by ProcessSceneQueryResults.
    const SceneResultQueue* InputBuffer_PrePhysics::GetSceneResultQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), CGS_ASSERT_STREAM_MESSAGE_AT("Not locked for reading\n", CGS_ARTIST_FILE, 172));
        return &mSceneResultQueue;
    }

    // X360 0x827A3468 (R, :198) -- const trigger-overlap output queue accessor; read-lock guarded,
    // returns &mOutputInterface (this + 4). Read by BridgeEntityModulesToOutput_PrePhysics.
    const TriggerEntityModuleOutputInterface* OutputBuffer_PrePhysics::GetOutputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), CGS_ASSERT_STREAM_MESSAGE_AT("Not locked for reading\n", CGS_ARTIST_FILE, 198));
        return &mOutputInterface;
    }

    // X360 0x822BD1D8 (W, :199) -- mutable trigger-overlap output queue accessor; write-lock
    // guarded, returns &mOutputInterface (this + 4). Filled by ProcessLineTestFineResult.
    TriggerEntityModuleOutputInterface* OutputBuffer_PrePhysics::GetOutputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), CGS_ASSERT_STREAM_MESSAGE_AT("Not locked for writing\n", CGS_ARTIST_FILE, 199));
        return &mOutputInterface;
    }

    // X360 0x822BCE90 (R, :64) -- const pre-scene input-interface accessor; read-lock guarded,
    // returns &mInputInterface (this + 4). Read by PreSceneUpdate. Const overload of
    // GetInputInterface (DWARF h:64 GetManagementInputInterface() const; the write overload
    // @0x827A3120 :65 is in BrnTriggerEntityModuleIO_Accessors.cpp).
    const InputBuffer_PreScene::InputInterface*
    InputBuffer_PreScene::GetInputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), CGS_ASSERT_STREAM_MESSAGE_AT("Not locked for reading\n", CGS_ARTIST_FILE, 64));
        return &mInputInterface;
    }
}
}

#undef CGS_ARTIST_FILE
