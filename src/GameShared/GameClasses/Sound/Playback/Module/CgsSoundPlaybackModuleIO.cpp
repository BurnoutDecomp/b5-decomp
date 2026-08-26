// =============================================================================
// CgsSoundPlaybackModuleIO.cpp -- the playback Module's IO buffer pair bodies.
//
// The X360 image has no standalone symbols for these (each is inlined at its
// IOBufferStack carve site / the Module::Update drain), so every body here is
// reconstructed from those attested inline sites:
//   InputBuffer::Construct   -- CreateIOBuffer<InputBuffer>  @ 0x826C5B68 (`*v8 = 1`
//                               == the base IOBuffer::Construct status store)
//   OutputBuffer::Construct  -- CreateIOBuffer<OutputBuffer> @ 0x826C5C40 (base
//                               status, queue Construct @+20, array count 0 @+16,
//                               queue Clear, array count 0 again)
//   OutputBuffer::Clear      -- Module::Update @ 0x826E9700 (queue Clear @+20 +
//                               the +16 count zero)
//   Prepare/Release/Destruct -- the DWARF signatures (h:108/:112/:116) over the
//                               owned queue's own lifecycle (the only member with
//                               lifecycle state; the DWARF cpp dumps show no
//                               further calls).
// =============================================================================

#include "GameShared/GameClasses/Sound/Playback/Module/CgsSoundPlaybackModuleIO.h"

namespace CgsSound
{
namespace Playback
{
namespace Module
{
namespace Io
{

// ---------------------------------------------------------------------------
// InputBuffer  (DWARF cpp:52 / :69 / :87)
// ---------------------------------------------------------------------------
void InputBuffer::Construct()
{
    CgsModule::IOBuffer::Construct();
}

void InputBuffer::Destruct()
{
}

void InputBuffer::Clear()
{
}

// ---------------------------------------------------------------------------
// OutputBuffer  (DWARF cpp:~40 / :124 / :141 / :158 / :173)
// ---------------------------------------------------------------------------
void OutputBuffer::Construct()
{
    CgsModule::IOBuffer::Construct();
    mRequestQueue.Construct();
    mStreamBuffersFreed.Construct();
    mRequestQueue.Clear();
    mStreamBuffersFreed.Clear();
}

bool OutputBuffer::Prepare()
{
    return mRequestQueue.Prepare();
}

bool OutputBuffer::Release()
{
    return mRequestQueue.Release();
}

void OutputBuffer::Destruct()
{
    mRequestQueue.Destruct();
}

void OutputBuffer::Clear()
{
    mRequestQueue.Clear();
    mStreamBuffersFreed.Clear();
}

} // namespace Io
} // namespace Module
} // namespace Playback
} // namespace CgsSound
