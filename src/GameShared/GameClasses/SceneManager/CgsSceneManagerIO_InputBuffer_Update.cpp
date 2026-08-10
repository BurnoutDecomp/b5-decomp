#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// CgsSceneManager::SceneManagerIO::InputBuffer_Update member function, reconstructed from
// BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
//
//   GetInSceneUpdateInterface() @ 0x825BD8C0 -> write-lock (bit 3), &mInSceneUpdateInterface (this+16)
//
// X360 store-for-store: reads the 1-byte IOBuffer status (lbz 0(this)), tests write-lock bit 3
// (`((*a1 >> 3) & 1) == 0` -> "Not locked for writing\n" tripwire), then returns `this + 16`
// (`addi r3, this, 0x10` -- &mInSceneUpdateInterface; the 1-byte IOBuffer status + 15 pad bytes
// place the aggregate at +16, per CgsSceneManagerIO.h). The X360-baked d:\p4 CgsSceneManagerModuleIO.h
// file/line (463) is discarded per project policy; CGS_ASSERT supplies the condition + __FILE__/__LINE__.
// Called by the 14 physics triangle-cache producers (PhysicalTrafficManager / VehicleManager /
// PropManager / DeformationManager Prepare/UpdateTriangleCache) and the world scene bridges, which
// then Append onto the returned InSceneUpdateInterface.

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // X360 0x825BD8C0: write-lock (bit 3) handle to the embedded scene-update input aggregate (this+16).
    InSceneUpdateInterface* InputBuffer_Update::GetInSceneUpdateInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mInSceneUpdateInterface;
    }

    // X360 0x828AF1C8: the CONST twin -- READ-lock (bit 4) handle to the same aggregate.
    // Same `return this + 16`, different tripwire; see the header for why the pair exists and
    // which caller takes which. StartUpdateTriangleCache @0x828C73D8 is the read-lock caller.
    const InSceneUpdateInterface* InputBuffer_Update::GetInSceneUpdateInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mInSceneUpdateInterface;
    }
}
}
