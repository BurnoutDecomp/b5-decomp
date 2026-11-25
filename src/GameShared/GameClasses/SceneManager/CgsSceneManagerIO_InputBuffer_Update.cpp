#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO.h"

#include "GameShared/GameClasses/Core/CgsAssertStreamMessageAt.h"

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    namespace
    {
        const char KAC_SCENE_INPUT_READ_ARTIST_FILE[] =
            "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\scenemanager\\CgsSceneManagerModuleIO.h";
    }

    // X360 0x825BD8C0: write-lock (bit 3) handle to the embedded scene-update input aggregate (this+16).
    InSceneUpdateInterface* InputBuffer_Update::GetInSceneUpdateInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), CGS_ASSERT_STREAM_MESSAGE_AT(
            "Not locked for writing\n",
            "..\\..\\..\\GameShared\\GameClasses\\SceneManager/CgsSceneManagerModuleIO.h", 463));
        return &mInSceneUpdateInterface;
    }

    // X360 0x828AF1C8: the CONST twin -- READ-lock (bit 4) handle to the same aggregate.
    // Same `return this + 16`, different tripwire; see the header for why the pair exists and
    // which caller takes which. StartUpdateTriangleCache @0x828C73D8 is the read-lock caller.
    const InSceneUpdateInterface* InputBuffer_Update::GetInSceneUpdateInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), CGS_ASSERT_STREAM_MESSAGE_AT(
            "Not locked for reading\n", KAC_SCENE_INPUT_READ_ARTIST_FILE, 462));
        return &mInSceneUpdateInterface;
    }
}
}
