#include "GameSource/Game/BrnGameModuleIO.h"

namespace BrnGame
{
namespace BrnGameModuleIO
{
    // The real GameModule vtable820D14A8 names the template lifetime entries:
    // CreateInput827E3090 and CreateOutput827E30C8 call the empty8284CB38
    // fold; DestroyInput827E3100 and DestroyOutput827E3128 do the same.
    // Prepare/ReleaseDataStructures827E3150/31C0 invoke the true-return fold
    // for each structure. These empty/true bodies are attested native code.
    void InputBuffer::Construct() {}
    bool InputBuffer::Prepare() { return true; }
    bool InputBuffer::Release() { return true; }
    void InputBuffer::Destruct() {}

    // ARTIST827E5F64: the implicit queue constructor clears only its leading
    // constructed flag at GameModule+22C. The other queue bytes are untouched.
    OutputBuffer::OutputBuffer()
    {
        mResourceRequestInterface.mRequestQueue.MarkUnconstructed();
    }
    void OutputBuffer::Construct() {}
    bool OutputBuffer::Prepare() { return true; }
    bool OutputBuffer::Release() { return true; }
    void OutputBuffer::Destruct() {}
}
}
