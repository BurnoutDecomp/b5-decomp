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
}
}
