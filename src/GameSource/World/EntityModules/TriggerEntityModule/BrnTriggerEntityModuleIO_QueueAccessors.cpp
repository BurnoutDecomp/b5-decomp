#include "GameSource/World/EntityModules/TriggerEntityModule/BrnTriggerEntityModuleIO.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

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
    // X360 0x822BCE90 (R, :64) -- const trigger-query input queue accessor; read-lock guarded,
    // returns &mQueryInputInterface (this + 4). Read by PreSceneUpdate.
    const TriggerQueryQueue* InputBuffer_PostScene::GetQueryInputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mQueryInputInterface;
    }

    // X360 0x827A3120 (W, :65) -- mutable trigger-query input queue accessor; write-lock guarded,
    // returns &mQueryInputInterface (this + 4). Filled by the input->trigger bridges.
    TriggerQueryQueue* InputBuffer_PostScene::GetQueryInputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mQueryInputInterface;
    }

    // X360 0x822BD130 (R, :172) -- const scene-result queue accessor; read-lock guarded, returns
    // &mSceneResultQueue (this + 4). Read by ProcessSceneQueryResults.
    const SceneResultQueue* InputBuffer_PrePhysics::GetSceneResultQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mSceneResultQueue;
    }

    // X360 0x827A3468 (R, :198) -- const trigger-overlap output queue accessor; read-lock guarded,
    // returns &mOutputInterface (this + 4). Read by BridgeEntityModulesToOutput_PrePhysics.
    const TriggerEntityModuleOutputInterface* OutputBuffer_PrePhysics::GetOutputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mOutputInterface;
    }

    // X360 0x822BD1D8 (W, :199) -- mutable trigger-overlap output queue accessor; write-lock
    // guarded, returns &mOutputInterface (this + 4). Filled by ProcessLineTestFineResult.
    TriggerEntityModuleOutputInterface* OutputBuffer_PrePhysics::GetOutputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mOutputInterface;
    }

    // X360 0x822BCFE0 (R, :118) -- const pre-scene input-interface accessor; read-lock guarded,
    // returns &mInputInterface (this + 4). Read by ProcessTriggerQueryEvents. Const overload of
    // GetInputInterface (the write overload @0x827A3270 :119 is committed in
    // BrnTriggerEntityModuleIO_Accessors.cpp).
    const InputBuffer_PreScene::InputInterface*
    InputBuffer_PreScene::GetInputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mInputInterface;
    }
}
}
