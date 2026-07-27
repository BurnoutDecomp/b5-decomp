#include "GameSource/World/EntityModules/TriggerEntityModule/BrnTriggerEntityModuleIO.h"

#include <cstddef>   // offsetof

// BrnWorld::TriggerEntityModuleIO::OutputBuffer_PreScene::Construct, reconstructed from
// BURNOUT_X360_ARTIST.XEX (0x822EED90).
//
// The X360 body sets the IOBuffer status byte to 1 (eStatusConstructed) -- i.e.
// CgsModule::IOBuffer::Construct() -- then tail-calls the scene input interface's Construct
// at `this + 0x10` (=&mSceneInputInterface). That matches the original two-statement body
//   IOBuffer::Construct();  mSceneInputInterface.Construct();
// (the second was emitted as the tail call; both flattened by the optimizer).
//
// FLAG: InSceneUpdateInterface::Construct() is owned by the InSceneUpdateInterface TU; it is
// declared-only in CgsSceneManagerIO_SceneUpdate.h and bodied there. This compile-only gate
// does not link it.

namespace BrnWorld
{
namespace TriggerEntityModuleIO
{
    void OutputBuffer_PreScene::_AssertLayout()
    {
        static_assert(offsetof(OutputBuffer_PreScene, mSceneInputInterface) == 16,
                      "mSceneInputInterface @16");
    }

    // X360 0x822EED90.
    void OutputBuffer_PreScene::Construct()
    {
        IOBuffer::Construct();
        mSceneInputInterface.Construct();
    }

    // X360 0x822EED48 -- the pre-scene INPUT buffer: the IOBuffer status byte then the
    // trigger-management aggregate's two embedded queues (the add queue at this+4 and the
    // remove queue at this+4+131088). Without it the aggregate's queues stay un-Constructed
    // and the first BridgeInputToEntityModules Append fires "Not Constructed".
    void InputBuffer_PreScene::Construct()
    {
        IOBuffer::Construct();
        mInputInterface.GetAddTriggerEventQueue().Construct();
        mInputInterface.GetRemoveTriggerEventQueue().Construct();
    }

    // X360 0x822DA168 -- post-scene INPUT: status byte + the trigger-query queue (this+4).
    void InputBuffer_PostScene::Construct()
    {
        IOBuffer::Construct();
        mQueryInputInterface.Construct();
    }

    // X360 0x822DA180 -- post-scene OUTPUT: status byte + the fine-line-test queue (this+4).
    void OutputBuffer_PostScene::Construct()
    {
        IOBuffer::Construct();
        mSceneFineQueryQueue.Construct();
    }

    // X360 0x822DA198 -- pre-physics INPUT: status byte + the scene-result queue (this+4).
    void InputBuffer_PrePhysics::Construct()
    {
        IOBuffer::Construct();
        mSceneResultQueue.Construct();
    }

    // X360 0x822DA1B0 -- pre-physics OUTPUT: status byte, the overlap output queue Construct
    // then its Clear (the X360 emits both calls back to back).
    void OutputBuffer_PrePhysics::Construct()
    {
        IOBuffer::Construct();
        mOutputInterface.Construct();
        mOutputInterface.Clear();
    }
}
}
